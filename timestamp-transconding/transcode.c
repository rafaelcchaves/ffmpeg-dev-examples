#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define INBUF_SIZE 100000

int pts;
int dts;

static void transcode(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, AVPacket *inpkt, AVFrame *frame, AVPacket *outpkt, 
                   FILE *outfile)
{
    int ret_dec;
    int ret_enc;
 
    ret_dec = avcodec_send_packet(dec_ctx, inpkt);
    if (ret_dec < 0) {
        fprintf(stderr, "Error sending a packet for decoding\n");
        exit(1);
    }
 
    while (ret_dec >= 0) {
        ret_dec = avcodec_receive_frame(dec_ctx, frame);
 
        if (ret_dec == AVERROR(EAGAIN) || ret_dec == AVERROR_EOF)
            break;
        else if (ret_dec < 0) {
            fprintf(stderr, "Error during decoding\n");
            exit(1);
        }
	printf("Frame: %ld %ld\n",frame->pts, frame->pkt_dts);
        frame->pts = frame->pkt_dts;


	ret_enc = avcodec_send_frame(enc_ctx, frame);
	if (ret_enc < 0) {
	    fprintf(stderr, "Error sending a frame for encoding\n");
	    exit(1);
	}

	while (ret_enc >= 0) {
	    ret_enc = avcodec_receive_packet(enc_ctx, outpkt);
	    if (ret_enc == AVERROR(EAGAIN) || ret_enc == AVERROR_EOF)
	        break;
	    else if (ret_enc < 0) {
	        fprintf(stderr, "Error during encoding\n");
	        exit(1);
	    }
		
	    fwrite(outpkt->data, 1, outpkt->size, outfile);
	    av_packet_unref(outpkt);
	}
    }
}


int main(int argc, char** argv){
    int ret;

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

    if (argc <= 2) {
        fprintf(stderr, "Usage: %s <input file> <output file>\n", argv[0]);
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
    incodec_ctx->time_base = (AVRational){1, 30};
    incodec_ctx->framerate = (AVRational){30, 1};
    if (avcodec_open2(incodec_ctx, incodec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }
   outcodec = avcodec_find_encoder_by_name("libx264");
    if (!outcodec) {
        fprintf(stderr, "Codec '%s' not found\n", "libx264");
        exit(1);
    }

    outcodec_ctx = avcodec_alloc_context3(outcodec);
    if (!outcodec_ctx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }

    outcodec_ctx->bit_rate = 5000000;
    outcodec_ctx->time_base = (AVRational){1, 30};
    outcodec_ctx->framerate = (AVRational){30, 1};
    outcodec_ctx->gop_size = 10;
    outcodec_ctx->max_b_frames = 1;
    outcodec_ctx->width = 1280;
    outcodec_ctx->height = 720;
    outcodec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    av_opt_set(outcodec_ctx->priv_data, "preset", "slow", 0);
    ret = avcodec_open2(outcodec_ctx, outcodec, NULL);

    if (ret < 0) {
        fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
        exit(1);
    }

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

    uint8_t endcode[] = { 0, 0, 1, 0xb7 };
    fwrite(endcode, 1, sizeof(endcode), output);
    avcodec_free_context(&outcodec_ctx);
    avcodec_free_context(&incodec_ctx);
    av_frame_free(&frame);
    av_packet_free(&inpkt);
    av_packet_free(&outpkt);
    return 0;
 

}
