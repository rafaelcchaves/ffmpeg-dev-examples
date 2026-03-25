/**
 * @file transcode_lz4_st.c
 * @brief Transcodificação LZ4/LZ4HC Single-thread
 *
 * Decodifica frames com FFmpeg e comprime cada frame com LZ4 ou LZ4HC
 * em um loop simples (sem Queue, sem threads para compressão).
 * O decoder FFmpeg pode usar threading interno via decoder_threads.
 */

#include "transcode_lz4_st.h"
#include "transcode_lz4_mt.h"
#include "../cpu_stats.h"
#include "../fps_limiter.h"

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <lz4.h>
#include <lz4hc.h>
#include <math.h>
#include <inttypes.h>

int st_encode_main(const char *input_file, const char *output_file,
                   int decoder_threads, int encoder_type,
                   int compression_level, const char *profile,
                   double target_fps) {
    int ret;
    int width = 0, height = 0;
    double fps_double = 0.0;
    int frames = 0;
    FILE *output = NULL;

    FpsLimiter limiter;
    fps_limiter_init(&limiter, target_fps);

    AVFormatContext *fmt_ctx = NULL;
    int video_stream_index = -1;

    const AVCodec *incodec;
    AVCodecContext *incodec_ctx = NULL;
    AVPacket *inpkt;
    AVFrame *frame;

    // Buffer para imagem contígua e compressão
    uint8_t *image_data[4] = {0};
    int image_linesize[4] = {0};
    int image_bufsize = 0;
    int maxCapacity = 0;
    char *compressedFrame = NULL;

    // Abre arquivo de saída se necessário
    if (g_enable_write) {
        output = fopen(output_file, "wb");
        if (!output) {
            fprintf(stderr, "Erro: Nao foi possivel abrir %s\n", output_file);
            return -1;
        }
    }

    // Aloca frame
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Erro: Nao foi possivel alocar video frame\n");
        if (output) fclose(output);
        return -1;
    }

    inpkt = av_packet_alloc();
    if (!inpkt) {
        fprintf(stderr, "Erro: Nao foi possivel alocar packet\n");
        av_frame_free(&frame);
        if (output) fclose(output);
        return -1;
    }

    // Demuxing
    if (avformat_open_input(&fmt_ctx, input_file, NULL, NULL) < 0) {
        fprintf(stderr, "Erro: Nao foi possivel abrir %s\n", input_file);
        av_packet_free(&inpkt);
        av_frame_free(&frame);
        if (output) fclose(output);
        return -1;
    }
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Erro: Nao foi possivel encontrar informacoes de stream\n");
        avformat_close_input(&fmt_ctx);
        av_packet_free(&inpkt);
        av_frame_free(&frame);
        if (output) fclose(output);
        return -1;
    }

    video_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream_index < 0) {
        fprintf(stderr, "Erro: Nao foi possivel encontrar stream de video\n");
        avformat_close_input(&fmt_ctx);
        av_packet_free(&inpkt);
        av_frame_free(&frame);
        if (output) fclose(output);
        return -1;
    }

    AVStream *video_stream = fmt_ctx->streams[video_stream_index];
    width = video_stream->codecpar->width;
    height = video_stream->codecpar->height;

    if (video_stream->avg_frame_rate.den > 0) {
        fps_double = (double)video_stream->avg_frame_rate.num / video_stream->avg_frame_rate.den;
    } else {
        fprintf(stderr, "Aviso: Nao foi possivel determinar FPS, usando 30fps\n");
        fps_double = 30.0;
    }
    int fps = (int)round(fps_double);

    // Decoder setup
    incodec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!incodec) {
        fprintf(stderr, "Erro: Decoder nao encontrado para codec %d\n", video_stream->codecpar->codec_id);
        avformat_close_input(&fmt_ctx);
        av_packet_free(&inpkt);
        av_frame_free(&frame);
        if (output) fclose(output);
        return -1;
    }

    incodec_ctx = avcodec_alloc_context3(incodec);
    if (!incodec_ctx) {
        fprintf(stderr, "Erro: Nao foi possivel alocar decoder context\n");
        avformat_close_input(&fmt_ctx);
        av_packet_free(&inpkt);
        av_frame_free(&frame);
        if (output) fclose(output);
        return -1;
    }

    if (avcodec_parameters_to_context(incodec_ctx, video_stream->codecpar) < 0) {
        fprintf(stderr, "Erro: Nao foi possivel copiar parametros do decoder\n");
        avcodec_free_context(&incodec_ctx);
        avformat_close_input(&fmt_ctx);
        av_packet_free(&inpkt);
        av_frame_free(&frame);
        if (output) fclose(output);
        return -1;
    }

    incodec_ctx->time_base = (AVRational){1, fps};
    incodec_ctx->framerate = (AVRational){fps, 1};
    incodec_ctx->thread_count = decoder_threads;

    if (avcodec_open2(incodec_ctx, incodec, NULL) < 0) {
        fprintf(stderr, "Erro: Nao foi possivel abrir decoder\n");
        avcodec_free_context(&incodec_ctx);
        avformat_close_input(&fmt_ctx);
        av_packet_free(&inpkt);
        av_frame_free(&frame);
        if (output) fclose(output);
        return -1;
    }

    // Aloca buffer de imagem contígua para compressão
    ret = av_image_alloc(image_data, image_linesize, width, height,
                         video_stream->codecpar->format, 1);
    if (ret < 0) {
        fprintf(stderr, "Erro: Nao foi possivel alocar buffer de imagem\n");
        avcodec_free_context(&incodec_ctx);
        avformat_close_input(&fmt_ctx);
        av_packet_free(&inpkt);
        av_frame_free(&frame);
        if (output) fclose(output);
        return -1;
    }
    image_bufsize = ret;

    maxCapacity = LZ4_compressBound(image_bufsize);
    compressedFrame = (char *)malloc(maxCapacity);
    if (!compressedFrame) {
        fprintf(stderr, "Erro: Nao foi possivel alocar buffer de compressao\n");
        av_free(image_data[0]);
        avcodec_free_context(&incodec_ctx);
        avformat_close_input(&fmt_ctx);
        av_packet_free(&inpkt);
        av_frame_free(&frame);
        if (output) fclose(output);
        return -1;
    }

    // Loop principal: decode + compress
    int64_t start_time = av_gettime();
    CpuStats cpu_start, cpu_end;
    cpu_stats_read(&cpu_start);

    while (av_read_frame(fmt_ctx, inpkt) >= 0) {
        if (inpkt->stream_index != video_stream_index) {
            av_packet_unref(inpkt);
            continue;
        }

        fps_limiter_wait(&limiter);

        int64_t frame_start = av_gettime();

        // Envia pacote para decodificação
        if (incodec_ctx->codec_id == AV_CODEC_ID_AV1)
            inpkt->pts = av_gettime();
        inpkt->dts = av_gettime();

        ret = avcodec_send_packet(incodec_ctx, inpkt);
        if (ret < 0) {
            fprintf(stderr, "Erro ao enviar pacote para decodificacao\n");
            break;
        }

        while (ret >= 0) {
            ret = avcodec_receive_frame(incodec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                fprintf(stderr, "Erro durante decodificacao\n");
                break;
            }

            frames++;

            // Copia frame decodificado para buffer contíguo
            av_image_copy(image_data, image_linesize,
                          (const uint8_t **)(frame->data), frame->linesize,
                          frame->format, frame->width, frame->height);

            // Comprime com LZ4 ou LZ4HC
            int compressedSize = 0;
            if (encoder_type == ENCODER_TYPE_LZ4HC) {
                compressedSize = LZ4_compress_HC((const char *)image_data[0],
                                                  compressedFrame, image_bufsize,
                                                  maxCapacity, compression_level);
            } else {
                compressedSize = LZ4_compress_fast((const char *)image_data[0],
                                                   compressedFrame, image_bufsize,
                                                   maxCapacity, compression_level);
            }

            if (compressedSize == 0) {
                fprintf(stderr, "Erro: Compressao falhou\n");
                break;
            }

            // Escreve FrameHeader + dados comprimidos
            if (g_enable_write) {
                FrameHeader hf = {image_bufsize, compressedSize,
                                  frame->width, frame->height, frame->format};
                fwrite(&hf, sizeof(FrameHeader), 1, output);
                fwrite(compressedFrame, 1, compressedSize, output);
            }

            // Imprime métrica por frame
            int64_t frame_time = av_gettime() - frame_start;
            printf("%s,%d,1,transcoding,%ld,%d\n", profile, decoder_threads,
                   frame_time, compression_level);
        }

        av_packet_unref(inpkt);
    }

    // Flush decoder
    avcodec_send_packet(incodec_ctx, NULL);
    while (avcodec_receive_frame(incodec_ctx, frame) >= 0) {
        frames++;
    }

    int64_t total_time = av_gettime() - start_time;
    cpu_stats_read(&cpu_end);
    double cpu_usage = cpu_stats_calculate_usage(&cpu_start, &cpu_end);

    // Imprime resumo
    printf("%s,%d,1,total,%ld,%d\n", profile, decoder_threads,
           total_time, compression_level);
    printf("%s,%d,1,fps,%lf,%d\n", profile, decoder_threads,
           (frames * 1000000.0) / total_time, compression_level);
    printf("%s,%d,1,cpu_usage,%.1f,%d\n", profile, decoder_threads,
           cpu_usage, compression_level);

    // Cleanup
    free(compressedFrame);
    av_free(image_data[0]);
    avcodec_free_context(&incodec_ctx);
    avformat_close_input(&fmt_ctx);
    av_frame_free(&frame);
    av_packet_free(&inpkt);
    if (output) fclose(output);

    return 0;
}
