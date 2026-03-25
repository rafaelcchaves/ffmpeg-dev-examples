extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cpu_stats.h"

#define INBUF_SIZE 10000

int frames;
int num_decoder_threads = 0;
const char *profile_name = NULL;
static bool g_enable_write = false;

static void decode(AVCodecContext *dec_ctx, AVPacket *inpkt, AVFrame *frame, FILE *outfile)
{
    static uint8_t *image_data[4];
    static int image_linesize[4];
    static int image_bufsize = 0;

    int ret_dec;
    if(inpkt){
        if(dec_ctx->codec_id == AV_CODEC_ID_AV1)
            inpkt->pts = av_gettime();
        inpkt->dts = av_gettime();
    }
    ret_dec = avcodec_send_packet(dec_ctx, inpkt);
    if (ret_dec < 0) {
        fprintf(stderr, "Error sending a packet for decoding\n");
        exit(1);
    }
    while (ret_dec >= 0) {
        ret_dec = avcodec_receive_frame(dec_ctx, frame);
        if (ret_dec == AVERROR(EAGAIN) || ret_dec == AVERROR_EOF) {
            break;
        } else if (ret_dec < 0) {
            fprintf(stderr, "Error during decoding\n");
            exit(1);
        }
    frames++;

        if(inpkt)
        printf("%s,%d,0,decoding,%ld\n", profile_name, num_decoder_threads, av_gettime() - frame->pkt_dts);

        if (g_enable_write && outfile) {
            if (image_bufsize == 0) {
                image_bufsize = av_image_alloc(image_data, image_linesize,
                                               frame->width, frame->height,
                                               (AVPixelFormat)frame->format, 1);
                if (image_bufsize < 0) {
                    fprintf(stderr, "Error allocating image buffer\n");
                    exit(1);
                }
            }
            av_image_copy(image_data, image_linesize,
                          (const uint8_t **)(frame->data), frame->linesize,
                          (AVPixelFormat)frame->format, frame->width, frame->height);
            fwrite(image_data[0], 1, image_bufsize, outfile);
        }

        av_frame_unref(frame);
    }
}

int main(int argc, char** argv){
    const char *infilename = NULL, *outfilename = NULL;
    FILE *output;
    AVFormatContext *fmt_ctx = NULL;
    int video_stream_index = -1;

    const AVCodec *incodec;
    AVCodecContext *incodec_ctx= NULL;
    AVPacket *inpkt = NULL;
    AVFrame *frame = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "i:o:p:D:w")) != -1) {
        switch (opt) {
            case 'i':
                infilename = optarg;
                break;
            case 'o':
                outfilename = optarg;
                break;
            case 'p':
                profile_name = optarg;
                break;
            case 'D':
                num_decoder_threads = atoi(optarg);
                break;
            case 'w':
                g_enable_write = true;
                break;
            default:
                fprintf(stderr, "Usage: %s -i <input-file> [-o <output-file>] -p <profile> [-D <decoder_threads>] [-w]\n", argv[0]);
                fprintf(stderr, "  -i  Input video file\n");
                fprintf(stderr, "  -o  Output YUV file (default: output.yuv)\n");
                fprintf(stderr, "  -p  Profile name (e.g., low_latency, balanced, high_throughput)\n");
                fprintf(stderr, "  -D  Decoder threads (default: 0 = auto)\n");
                fprintf(stderr, "  -w  Enable output file writing (default: disabled for benchmark)\n");
                exit(1);
        }
    }
    if (infilename == NULL || profile_name == NULL) {
        fprintf(stderr, "Usage: %s -i <input-file> [-o <output-file>] -p <profile> [-D <decoder_threads>] [-w]\n", argv[0]);
        fprintf(stderr, "  -i  Input video file\n");
        fprintf(stderr, "  -o  Output YUV file (default: output.yuv)\n");
        fprintf(stderr, "  -p  Profile name (e.g., low_latency, balanced, high_throughput)\n");
        fprintf(stderr, "  -D  Decoder threads (default: 0 = auto)\n");
        fprintf(stderr, "  -w  Enable output file writing (default: disabled for benchmark)\n");
        exit(1);
    }
    if (!outfilename) outfilename = "output.yuv";
    if (g_enable_write) {
        output = fopen(outfilename, "wb");
        if (!output) {
            fprintf(stderr, "Could not open %s\n", outfilename);
            exit(1);
        }
    } else {
        output = NULL;
    }

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }

    inpkt = av_packet_alloc();
    if (!inpkt)
        exit(1);

    if (avformat_open_input(&fmt_ctx, infilename, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", infilename);
        exit(1);
    }
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        exit(1);
    }
    video_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream_index < 0) {
        fprintf(stderr, "Could not find video stream in input file\n");
        exit(1);
    }
    AVStream *video_stream = fmt_ctx->streams[video_stream_index];

    incodec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!incodec) {
        fprintf(stderr, "Failed to find %s codec (%d)\n", avcodec_get_name(video_stream->codecpar->codec_id), video_stream_index);
        exit(1);
    }
    incodec_ctx = avcodec_alloc_context3(incodec);
    if (!incodec_ctx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }

    if (avcodec_parameters_to_context(incodec_ctx, video_stream->codecpar) < 0) {
        fprintf(stderr, "Failed to copy codec parameters to decoder context\n");
        exit(1);
    }
    incodec_ctx->thread_count = num_decoder_threads;
    if (avcodec_open2(incodec_ctx, incodec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }

    int64_t start_time;
    CpuStats cpu_start, cpu_end;

    start_time = av_gettime();
    cpu_stats_read(&cpu_start);

    while (av_read_frame(fmt_ctx, inpkt) >= 0) {
        if (inpkt->stream_index == video_stream_index) {
            decode(incodec_ctx, inpkt, frame, output);
        }
        av_packet_unref(inpkt);
    }
    decode(incodec_ctx, NULL, frame, output);

    cpu_stats_read(&cpu_end);
    double cpu_usage = cpu_stats_calculate_usage(&cpu_start, &cpu_end);

    printf("%s,%d,0,total,%ld\n", profile_name, num_decoder_threads, av_gettime() - start_time);
    printf("%s,%d,0,fps,%lf\n", profile_name, num_decoder_threads, (frames*1e6)/(av_gettime() - start_time));
    printf("%s,%d,0,cpu,%lf\n", profile_name, num_decoder_threads, cpu_usage);

    avformat_close_input(&fmt_ctx);
    avcodec_free_context(&incodec_ctx);
    av_frame_free(&frame);
    av_packet_free(&inpkt);
    if (output) fclose(output);
    return 0;
}
