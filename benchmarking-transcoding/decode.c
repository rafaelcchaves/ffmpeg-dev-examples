#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavutil/time.h>
#include <math.h>
#include <inttypes.h>
#include <unistd.h> 
#include <libavformat/avformat.h>

#define INBUF_SIZE 10000
#ifndef THREADS_IN
#define THREADS_IN 0
#endif

static void decode (AVCodecContext *dec_ctx, AVPacket *inpkt, AVFrame *frame, FILE *outfile)
{
    int ret_dec;
    if(inpkt)
    	inpkt->dts = av_gettime();
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
        
        // Write YUV420p data
        fwrite(frame->data[0], 1, frame->width * frame->height, outfile);
        fwrite(frame->data[1], 1, frame->width * frame->height / 4, outfile);
        fwrite(frame->data[2], 1, frame->width * frame->height / 4, outfile);

	    if(inpkt)
            	printf("%d, 0,'decoding',%ld\n", THREADS_IN, av_gettime() - frame->pkt_dts);
        av_frame_unref(frame);
    }
}

int main(int argc, char** argv){
    int ret;
    const char *infilename = NULL, *outfilename = NULL;
    FILE *output;
    AVFormatContext *fmt_ctx = NULL;
    int video_stream_index = -1;
    
    const AVCodec *incodec;
    AVCodecContext *incodec_ctx= NULL;
    AVPacket *inpkt;
    
    int opt;
    while ((opt = getopt(argc, argv, "i:o:")) != -1) {
        switch (opt) {
            case 'i':
                infilename = optarg;
                break;
            case 'o':
                outfilename = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s -i <input file> -o <output file>\n", argv[0]);
                exit(1);
        }
    }
    if (infilename == NULL || outfilename == NULL) {
        fprintf(stderr, "Usage: %s -i <input file> -o <output file>\n", argv[0]);
        exit(1);
    }
    output = fopen(outfilename, "wb");
    if (!output) {
        fprintf(stderr, "Could not open %s\n", outfilename);
        exit(1);
    }
 
    AVFrame *frame;
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
    incodec_ctx->thread_count = THREADS_IN;
    if (avcodec_open2(incodec_ctx, incodec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }
    
    int64_t start_time;
    start_time = av_gettime();
    while (av_read_frame(fmt_ctx, inpkt) >= 0) {
        if (inpkt->stream_index == video_stream_index) {
            decode(incodec_ctx, inpkt, frame, output);
        }
        av_packet_unref(inpkt);
    }
    decode(incodec_ctx, NULL, frame, output);
    printf("%d, 0,'total',%ld\n", THREADS_IN, av_gettime() - start_time);
    
    avformat_close_input(&fmt_ctx);
    avcodec_free_context(&incodec_ctx);
    av_frame_free(&frame);
    av_packet_free(&inpkt);
    fclose(output);
    return 0;
}
