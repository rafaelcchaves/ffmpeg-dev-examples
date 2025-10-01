/*
 * Exemplo de programa para demosntrar o uso da biblioteca libavcodec,
 * decodificando um arquivo de vídeo e salvando como um arquivo de vídeo bruto
 * 
 * Esse código é uma adptação de https://ffmpeg.org/doxygen/4.4/decode_video_8c-example.html
 *
 * Como compilar: gcc decode.c -o decode -lavcodec -lavutil
 * Como executar (exemplo): ./decode input.h264 decode.yuv 640 480 yuv420p h264
 * Como reproduzir vídeo decodificado (exemplo): ffplay -f rawvideo -video_size 640x480 -framerate 30 -pixel_format yuv420p decode.yuv
 */
 
#include <stdio.h>
#include <stdlib.h>
 
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
 
#define INBUF_SIZE 4096

const char *input_filename, *output_filename;
const char *pix_fmt_name, *codec_name;
int width, height;

FILE *input_file, *output_file;

const AVCodec *codec;
AVCodecContext *codec_ctx;
AVCodecParserContext *parser;
AVPacket *pkt;
AVFrame *frame;
enum AVPixelFormat pix_fmt;
uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
uint8_t *data, *data_parsed;
size_t data_size; int data_parsed_size;
uint8_t *image_data[4];
int image_linesize[4];
int image_bufsize;

AVBufferRef *avbufferrefs[4];
uint8_t *buf[4];
int buf_linesize[4];

int my_get_buffer2(struct AVCodecContext *s, AVFrame *frame, int flags){
    for(int i = 0; i < 3; i++){
        frame->data[i] = avbufferrefs[i]->data;
        frame->extended_data = frame->data;
        frame->buf[i] = av_buffer_ref(avbufferrefs[i]);
        frame->linesize[i] = buf_linesize[i];
    }
    return 0;
}

void decode(AVCodecContext *dec_ctx, AVFrame *output_frame, AVPacket *input_pkt)
{
    int ret;
 
    ret = avcodec_send_packet(dec_ctx, input_pkt);
    if (ret < 0) {
        printf("Erro ao enviar um pacote para decodificação\n");
        exit(1);
    }
 
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, output_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            printf("Erro durante a decodificação\n");
            exit(1);
        }

        /* copy decoded frame to destination buffer:
        * this is required since rawvideo expects non aligned data */
        av_image_copy(image_data, image_linesize,
                      (const uint8_t **)(output_frame->data), output_frame->linesize,
                      pix_fmt, width, height);
    
        /* write to rawvideo file */
        fwrite(image_data[0], 1, image_bufsize, output_file);
    }
}

void myfree(void *opaque, uint8_t *data){
    av_freep(&data);
}
 
int main(int argc, char **argv)
{
    int ret;
    
    if (argc != 7) {
        printf("Entrada inválida\n");
        return 1;
    }
    input_filename  = argv[1];
    output_filename = argv[2];
    width           = atoi(argv[3]);
    height          = atoi(argv[4]);
    pix_fmt_name    = argv[5];
    codec_name      = argv[6];

    input_file = fopen(input_filename, "rb");
    if (input_file == NULL) {
        printf("Não foi possível abrir %s\n", input_filename);
        return 1;
    }

    output_file = fopen(output_filename, "wb");
    if (output_file == NULL) {
        printf("Não foi possível abrir %s\n", output_filename);
        return 1;
    }
 
    codec = avcodec_find_decoder_by_name(codec_name);
    if (codec == NULL) {
        printf("Codec não encontrado\n");
        return 1;
    }
 
    parser = av_parser_init(codec->id);
    if (parser == NULL) {
        printf("Parser não encontrado\n");
        return 1;
    }
 
    codec_ctx = avcodec_alloc_context3(codec);
    if (codec_ctx == NULL) {
        printf("Não foi possível alocar o contexto do codec de vídeo\n");
        return 1;
    }
    codec_ctx->get_buffer2 = my_get_buffer2;
 
    /* For some codecs, such as msmpeg4 and mpeg4, width and height
       MUST be initialized there because this information is not
       available in the bitstream. */
 
    ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0) {
        printf("Não foi possível abrir o codec\n");
        return ret;
    }

    pkt = av_packet_alloc();
    if (pkt == NULL){
        printf("Não foi possível alocar pacote de vídeo\n");
        return 1;
    }
 
    frame = av_frame_alloc();
    if (frame == NULL) {
        printf("Não foi possível alocar quadro de vídeo\n");
        return 1;
    }

    pix_fmt = av_get_pix_fmt(pix_fmt_name);
    if(pix_fmt == AV_PIX_FMT_NONE){
        printf("Não foi possível encontrar formato de pixel\n");
        return 1;
    }

    /* allocate image where the decoded image will be put */
    ret = av_image_alloc(image_data, image_linesize, width, height, pix_fmt, 1);
    if (ret < 0) {
        printf("Não foi possível alocar o buffer de vídeo decodificado\n");
        return ret;
    }
    image_bufsize = ret;

    ret = av_image_alloc(buf, buf_linesize, width, height, pix_fmt, 1);
    if (ret < 0) {
        printf("Não foi possível alocar o buffer de vídeo decodificado\n");
        return ret;
    }

    int sizes[3] = {width*height, (width*height)/4, (width*height)/4}; // Tamanho em bytes de cada plano YUV
    for(int i = 0; i < 3; i++){
        avbufferrefs[i] = av_buffer_create(buf[i], sizes[i], myfree, NULL, 0);
        if(avbufferrefs == NULL){
            printf("Não foi possível alocar AVBufferRef\n");
            return 1;
        }
    }
    
    while (feof(input_file) == 0) {
        /* read raw data from the input file */
        data_size = fread(inbuf, 1, INBUF_SIZE, input_file);
        if (data_size == 0)
            break;
 
        /* use the parser to split the data into frames */
        data = inbuf;
        while (data_size > 0) {
            ret = av_parser_parse2(parser, codec_ctx, &data_parsed, &data_parsed_size,
                                   data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (ret < 0) {
                printf("Erro durante o parsing\n");
                return ret;
            }
            data      += ret;
            data_size -= ret;
 
            if (data_parsed_size != 0){
                pkt->data = data_parsed;
                pkt->size = data_parsed_size;

                decode(codec_ctx, frame, pkt);
            }
        }
    }
 
    /* flush the decoder */
    decode(codec_ctx, frame, NULL);
 
    fclose(input_file);
    fclose(output_file);
 
    av_parser_close(parser);
    avcodec_free_context(&codec_ctx);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    av_free(image_data[0]);
 
    return 0;
}
