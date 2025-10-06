#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavutil/time.h>
#include <math.h>
#include <inttypes.h>
#include <unistd.h> 


/*
To run psnr:
ffmpeg -i <compressed> -i <original> -lavfi psnr -f null -

To run ssim:
ffmpeg -i <compressed> -i <original> -lavfi ssim -f null -

To compile and execute:
gcc transcode.c -o transcode -lavcodec -lavutil
./transcode input.h264 output.mjpeg <width> <height> <fps>

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

	frame->quality = FF_QP2LAMBDA * 0;
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
            	printf("%d, %d,'transcoding',%"PRId64"\n", THREADS_IN, THREADS_OUT, av_gettime() - outpkt->dts);

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
    int width, height, fps;
    

    const char *infilename, *outfilename;
    FILE *input, *output;

    const AVCodec *incodec;
    AVCodecContext *incodec_ctx= NULL;
    AVCodecParserContext *parser;
    AVPacket *inpkt;
    uint8_t *data;
    size_t data_size;
    
    const AVCodec *outcodec;
    AVCodecContext *outcodec_ctx= NULL;
    AVPacket *outpkt;

    if (argc < 6) {
        fprintf(stderr, "Usage: %s <input file> <output file> <width> <height> <fps>\n", argv[0]);
        exit(0);
    }

    infilename = argv[1];
    input = fopen(infilename, "rb");
    if (!input) {
        fprintf(stderr, "Could not open %s\n",infilename);
        exit(1);
    }
    outfilename = argv[2];
    output = fopen(outfilename, "wb");
    if (!output) {
        fprintf(stderr, "Could not open %s\n", outfilename);
        exit(1);
    }
    width = atoi(argv[3]);
    if (!width) {
        fprintf(stderr, "The width value isn't valid: %s\n", argv[3]);
        exit(1);
    }
    height = atoi(argv[4]);
    if (!height) {
        fprintf(stderr, "The height value isn't valid: %s\n", argv[4]);
        exit(1);
    }
    fps = atoi(argv[5]);
    if (!fps) {
        fprintf(stderr, "The fps value isn't valid: %s\n", argv[5]);
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

    uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    incodec = avcodec_find_decoder(AV_CODEC_ID_H264);

    if (!incodec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }
    parser = av_parser_init(incodec->id);
    if (!parser) {
        fprintf(stderr, "parser not found\n");
        exit(1);
    }
    incodec_ctx = avcodec_alloc_context3(incodec);
    if (!incodec_ctx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }
    incodec_ctx->time_base = (AVRational){1, 25};
    incodec_ctx->framerate = (AVRational){25, 1};
    incodec_ctx->thread_count = THREADS_IN;
    if (avcodec_open2(incodec_ctx, incodec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }
    outcodec = avcodec_find_encoder_by_name("mjpeg");
    if (!outcodec) {
        fprintf(stderr, "Codec '%s' not found\n", "libx264");
        exit(1);
    }

    outcodec_ctx = avcodec_alloc_context3(outcodec);
    if (!outcodec_ctx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }

    outcodec_ctx->flags |= AV_CODEC_FLAG_QSCALE;
    outcodec_ctx->time_base = (AVRational){1, fps};
    outcodec_ctx->framerate = (AVRational){fps, 1};
    outcodec_ctx->pix_fmt = AV_PIX_FMT_YUVJ420P;
    outcodec_ctx->width = width;
    outcodec_ctx->height = height;
    outcodec_ctx->thread_count = THREADS_OUT;
    ret = avcodec_open2(outcodec_ctx, outcodec, NULL);

    if (ret < 0) {
        fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
        exit(1);
    }

    int64_t start_time;

    start_time = av_gettime();

    while (!feof(input)) {

        data_size = fread(inbuf, 1, INBUF_SIZE, input);
        if (!data_size)
            break;
 
        data = inbuf;
        while (data_size > 0) {
            ret = av_parser_parse2(parser, incodec_ctx, &inpkt->data, &inpkt->size,
                                   data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (ret < 0) {
                fprintf(stderr, "Error while parsing\n");
                exit(1);
            }
            data      += ret;
            data_size -= ret;
	    inpkt->dts = dts++;
            if (inpkt->size)
		    transcode(incodec_ctx, outcodec_ctx, inpkt, frame, outpkt, output);
        }
    }
    transcode(incodec_ctx, outcodec_ctx, NULL, frame, outpkt, output);
    printf("%d, %d,'total',%"PRId64"\n", THREADS_IN, THREADS_OUT, av_gettime() - start_time);
    avcodec_free_context(&outcodec_ctx);
    avcodec_free_context(&incodec_ctx);
    av_frame_free(&frame);
    av_packet_free(&inpkt);
    av_packet_free(&outpkt);
    return 0;
}
