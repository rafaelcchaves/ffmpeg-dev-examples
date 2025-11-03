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
/*
To run psnr:
ffmpeg -i <compressed> -i <original> -lavfi psnr -f null - 
To run ssim:
ffmpeg -i <compressed> -i <original> -lavfi ssim -f null - 
To compile and execute:
gcc transcode.c -o transcode -lavformat -lavcodec -lavutil -lm
./transcode -i <mp4_input_file> -o <output_path> -e <encoder_name>
To execute tests: 
 */
#define INBUF_SIZE 10000
#ifndef THREADS_IN
#define THREADS_IN 0
#endif
#ifndef THREADS_OUT
#define THREADS_OUT 0
#endif
int pts;
int dts;
static void transcode(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, AVPacket *inpkt, AVFrame *frame, AVPacket *outpkt,
                      FILE *outfile)
{
    int ret_dec;
    int ret_enc;
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
        if (strcmp(enc_ctx->codec->name, "mjpeg") == 0) {
            frame->quality = FF_QP2LAMBDA * 2;
        }
        frame->pts = frame->pkt_dts;
        ret_enc = avcodec_send_frame(enc_ctx, frame);
        if (ret_enc < 0) {
            fprintf(stderr, "Error sending a frame for encoding\n");
            exit(1);
        }
        while (ret_enc >= 0) {
            ret_enc = avcodec_receive_packet(enc_ctx, outpkt);
            if (ret_enc == AVERROR(EAGAIN) || ret_enc == AVERROR_EOF) {
                break;
            } else if (ret_enc < 0) {
                fprintf(stderr, "Error during encoding\n");
                exit(1);
            }
	    if(inpkt)
            	printf("%d, %d,'transcoding',%ld\n", THREADS_IN, THREADS_OUT, av_gettime() - outpkt->dts);
            fwrite(outpkt->data, 1, outpkt->size, outfile);
            av_packet_unref(outpkt);
        }
    }
    if(inpkt)
	    return;
    ret_enc = avcodec_send_frame(enc_ctx, NULL);
    while (ret_enc >= 0) {
        ret_enc = avcodec_receive_packet(enc_ctx, outpkt);
        if (ret_enc == AVERROR(EAGAIN) || ret_enc == AVERROR_EOF) {
            return;
        } else if (ret_enc < 0) {
            fprintf(stderr, "Error during encoding\n");
            exit(1);
        }
        fwrite(outpkt->data, 1, outpkt->size, outfile);
        av_packet_unref(outpkt);
    }
}
int main(int argc, char** argv){
    int ret;
    int width = 0, height = 0;
    double fps_double = 0.0; // Use double for fps calculation
    const char *infilename = NULL, *outfilename = NULL;
    FILE *output;
    AVFormatContext *fmt_ctx = NULL;
    int video_stream_index = -1;
    
    const AVCodec *incodec;
    AVCodecContext *incodec_ctx= NULL;
    AVPacket *inpkt;
    
    const AVCodec *outcodec;
    AVCodecContext *outcodec_ctx= NULL;
    AVPacket *outpkt;
    int opt;
    const char *encoder_name = NULL;
    while ((opt = getopt(argc, argv, "i:o:e:")) != -1) { // Removed s and f
        switch (opt) {
            case 'i':
                infilename = optarg;
                break;
            case 'o':
                outfilename = optarg;
                break;
            case 'e':
                encoder_name = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s -i <input file> -o <output file> -e <encoder>\n", argv[0]); // Updated usage
                exit(1);
        }
    }
    if (infilename == NULL || outfilename == NULL || encoder_name == NULL) { // Removed width, height, fps checks
        fprintf(stderr, "Usage: %s -i <input file> -o <output file> -e <encoder>\n", argv[0]); // Updated usage
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
    outpkt = av_packet_alloc();
    if (!inpkt || !outpkt)
        exit(1);
    // Demuxing part
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
    // Auto-detect width, height, and fps
    width = video_stream->codecpar->width;
    height = video_stream->codecpar->height;
    if (video_stream->avg_frame_rate.den > 0) {
        fps_double = (double)video_stream->avg_frame_rate.num / video_stream->avg_frame_rate.den;
    } else {
        fprintf(stderr, "Could not determine frame rate from input stream, defaulting to 30fps.\n");
        fps_double = 30.0; // Default to 30 fps if not found
    }
            int fps = (int)round(fps_double); // Convert to int for AVCodecContext
         
            incodec = avcodec_find_decoder(video_stream->codecpar->codec_id);
            if (!incodec) {
                fprintf(stderr, "Failed to find %s codec (%d)\n", avcodec_get_name(video_stream->codecpar->codec_id), video_stream_index);        exit(1);
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
    incodec_ctx->time_base = (AVRational){1, fps}; // Use auto-detected fps
    incodec_ctx->framerate = (AVRational){fps, 1}; // Use auto-detected fps
    incodec_ctx->thread_count = THREADS_IN;
    if (avcodec_open2(incodec_ctx, incodec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }
    
    outcodec = avcodec_find_encoder_by_name(encoder_name);
    if (!outcodec) {
        fprintf(stderr, "Codec '%s' not found\n", encoder_name);
        exit(1);
    }
    outcodec_ctx = avcodec_alloc_context3(outcodec);
    if (!outcodec_ctx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }
    if (strcmp(encoder_name, "libsvtjpegxs") == 0) {
        ret = av_opt_set(outcodec_ctx->priv_data, "bpp", "4", 0);
        if (ret < 0) {
            fprintf(stderr, "Could not set bpp option\n");
            avcodec_free_context(&outcodec_ctx);
            return 1;
        }
    } else if (strcmp(encoder_name, "mjpeg") == 0) {
        outcodec_ctx->flags |= AV_CODEC_FLAG_QSCALE;
    }
    outcodec_ctx->time_base = (AVRational){1, fps}; // Use auto-detected fps
    outcodec_ctx->framerate = (AVRational){fps, 1}; // Use auto-detected fps
    if (strcmp(encoder_name, "mjpeg") == 0) {
        outcodec_ctx->pix_fmt = AV_PIX_FMT_YUVJ420P;
    } else {
        outcodec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    }
    outcodec_ctx->width = width; // Use auto-detected width
    outcodec_ctx->height = height; // Use auto-detected height
    outcodec_ctx->thread_count = THREADS_OUT;
    ret = avcodec_open2(outcodec_ctx, outcodec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
        exit(1);
    }
    int64_t start_time;
    start_time = av_gettime();
    while (av_read_frame(fmt_ctx, inpkt) >= 0) {
        if (inpkt->stream_index == video_stream_index) {
            transcode(incodec_ctx, outcodec_ctx, inpkt, frame, outpkt, output);
        }
        av_packet_unref(inpkt);
    }
    transcode(incodec_ctx, outcodec_ctx, NULL, frame, outpkt, output);
    printf("%d, %d,'total',%ld\n", THREADS_IN, THREADS_OUT, av_gettime() - start_time);
    
    avformat_close_input(&fmt_ctx);
    avcodec_free_context(&outcodec_ctx);
    avcodec_free_context(&incodec_ctx);
    av_frame_free(&frame);
    av_packet_free(&inpkt);
    av_packet_free(&outpkt);
    fclose(output);
    return 0;
}
