// ============================================================================
// decode_mjpeg_mt.cpp - Decodificador MJPEG Multithread
// Arquitetura: Producer-Consumer com Queue generica (FrameItem)
//
// O decoder MJPEG do FFmpeg nao suporta threading interno
// (nao possui AV_CODEC_CAP_FRAME_THREADS nem AV_CODEC_CAP_SLICE_THREADS).
// Este modulo implementa paralelismo a nivel de aplicacao: multiplas
// instancias do decoder FFmpeg distribuem frames via producer-consumer.
// ============================================================================

#include "decode_mjpeg_mt.h"
#include "cpu_stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Variaveis globais
// ============================================================================

// Input filename (acessado pelo producer thread)
static std::string g_input_filename;

Queue g_mjpeg_frame_queue;

size_t g_mjpeg_next_to_write = 0;
pthread_mutex_t g_mjpeg_write_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_mjpeg_write_cond = PTHREAD_COND_INITIALIZER;

bool g_mjpeg_producer_finished = false;
size_t g_mjpeg_total_frames_processed = 0;
pthread_mutex_t g_mjpeg_state_mutex = PTHREAD_MUTEX_INITIALIZER;

int g_mjpeg_active_threads_count = 0;
pthread_mutex_t g_mjpeg_active_threads_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_mjpeg_active_threads_cond = PTHREAD_COND_INITIALIZER;

FILE *g_mjpeg_output_file = NULL;

int g_mjpeg_num_threads = 0;
std::string g_mjpeg_profile_name;
bool g_mjpeg_enable_write = false;

std::atomic<int64_t> g_mjpeg_total_demux_time{0};
std::atomic<int64_t> g_mjpeg_total_decode_time{0};

// ============================================================================
// Thread Produtora (Demux)
// ============================================================================

void *mt_mjpeg_producer_thread(void *arg) {
    (void)arg;

    AVFormatContext *fmt_ctx = NULL;
    int video_stream_index = -1;
    int sequence_number = 0;

    // Abre input com avformat
    if (avformat_open_input(&fmt_ctx, g_input_filename.c_str(), NULL, NULL) < 0) {
        fprintf(stderr, "[MJPEG-Producer] Erro ao abrir arquivo de entrada\n");
        return NULL;
    }
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "[MJPEG-Producer] Erro ao buscar info do stream\n");
        avformat_close_input(&fmt_ctx);
        return NULL;
    }

    video_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream_index < 0) {
        fprintf(stderr, "[MJPEG-Producer] Erro: stream de video nao encontrado\n");
        avformat_close_input(&fmt_ctx);
        return NULL;
    }

    AVStream *video_stream = fmt_ctx->streams[video_stream_index];
    int width = video_stream->codecpar->width;
    int height = video_stream->codecpar->height;
    AVPixelFormat pix_fmt = (AVPixelFormat)video_stream->codecpar->format;

    // Calcula tamanho do frame YUV para escrita
    int image_bufsize = av_image_get_buffer_size(pix_fmt, width, height, 1);
    if (image_bufsize < 0) {
        fprintf(stderr, "[MJPEG-Producer] Erro ao calcular tamanho do buffer\n");
        avformat_close_input(&fmt_ctx);
        return NULL;
    }

    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        avformat_close_input(&fmt_ctx);
        return NULL;
    }

    // Loop principal de demux
    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index != video_stream_index) {
            av_packet_unref(pkt);
            continue;
        }

        int64_t demux_start = av_gettime();

        // Deep copy do pacote com padding (FFmpeg requer AV_INPUT_BUFFER_PADDING_SIZE)
        int padded_size = pkt->size + AV_INPUT_BUFFER_PADDING_SIZE;
        uint8_t *data = (uint8_t *)malloc(padded_size);
        if (!data) {
            fprintf(stderr, "[MJPEG-Producer] Erro: malloc falhou no frame %d\n", sequence_number);
            av_packet_unref(pkt);
            break;
        }
        memcpy(data, pkt->data, pkt->size);
        memset(data + pkt->size, 0, AV_INPUT_BUFFER_PADDING_SIZE);

        int64_t demux_time = av_gettime() - demux_start;
        g_mjpeg_total_demux_time += demux_time;

        // Cria AVBufferRef (ownership do buffer transferido para o AVBufferRef)
        AVBufferRef *buffer = av_buffer_create(data, pkt->size,
                                               av_buffer_default_free, NULL, 0);
        if (!buffer) {
            free(data);
            av_packet_unref(pkt);
            break;
        }

        // Cria FrameItem
        FrameItem *fi = (FrameItem *)malloc(sizeof(FrameItem));
        if (!fi) {
            av_buffer_unref(&buffer);
            av_packet_unref(pkt);
            break;
        }
        fi->header.size_decompress = image_bufsize;
        fi->header.size_compress = pkt->size;
        fi->header.width = width;
        fi->header.height = height;
        fi->header.pix_fmt = (int32_t)pix_fmt;
        fi->sequence_number = sequence_number;
        fi->timestamp = av_gettime();
        fi->data = buffer;

        queue_push(&g_mjpeg_frame_queue, fi);
        // fi ownership transferido para a fila

        sequence_number++;
        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);
    avformat_close_input(&fmt_ctx);

    // Marca produtor como finalizado
    pthread_mutex_lock(&g_mjpeg_state_mutex);
    g_mjpeg_producer_finished = true;
    g_mjpeg_total_frames_processed = sequence_number;
    pthread_mutex_unlock(&g_mjpeg_state_mutex);

    // Envia poison pills (um por consumer thread)
    for (int i = 0; i < g_mjpeg_num_threads; i++) {
        queue_push(&g_mjpeg_frame_queue, NULL);
    }

    // Aguarda todas as threads decodificadoras terminarem
    pthread_mutex_lock(&g_mjpeg_active_threads_mutex);
    while (g_mjpeg_active_threads_count > 0) {
        pthread_cond_wait(&g_mjpeg_active_threads_cond, &g_mjpeg_active_threads_mutex);
    }
    pthread_mutex_unlock(&g_mjpeg_active_threads_mutex);

    return NULL;
}

// ============================================================================
// Thread Decodificadora (Consumer)
// ============================================================================

void *mt_mjpeg_decoder_thread(void *arg) {
    MJpegThreadContext *ctx = (MJpegThreadContext *)arg;

    // Registra thread como ativa
    pthread_mutex_lock(&g_mjpeg_active_threads_mutex);
    g_mjpeg_active_threads_count++;
    pthread_mutex_unlock(&g_mjpeg_active_threads_mutex);

    AVPacket *pkt = av_packet_alloc();

    while (1) {
        // Pop da fila (bloqueante)
        FrameItem *fi = (FrameItem *)queue_pop(&g_mjpeg_frame_queue);

        // Poison pill
        if (!fi) break;

        // Decodifica (paralelo, sem locks)
        int64_t decode_start = av_gettime();

        // Prepara pacote temporario apontando para os dados do FrameItem
        pkt->data = fi->data->data;
        pkt->size = fi->header.size_compress;

        int ret = avcodec_send_packet(ctx->dec_ctx, pkt);
        if (ret < 0) {
            fprintf(stderr, "[MJPEG-Decoder %d] Erro ao enviar pacote do frame %d\n",
                    ctx->thread_id, fi->sequence_number);
            frame_item_free(fi);
            continue;
        }

        while (ret >= 0) {
            ret = avcodec_receive_frame(ctx->dec_ctx, ctx->frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                fprintf(stderr, "[MJPEG-Decoder %d] Erro ao receber frame %d\n",
                        ctx->thread_id, fi->sequence_number);
                break;
            }

            // Copia frame decodificado para buffer contiguo privado
            av_image_copy(ctx->image_data, ctx->image_linesize,
                          (const uint8_t **)(ctx->frame->data), ctx->frame->linesize,
                          (AVPixelFormat)fi->header.pix_fmt,
                          fi->header.width, fi->header.height);

            av_frame_unref(ctx->frame);
        }

        int64_t decode_time = av_gettime() - decode_start;
        g_mjpeg_total_decode_time += decode_time;

        // Escrita sequencial (aguarda sua vez)
        pthread_mutex_lock(&g_mjpeg_write_mutex);
        while ((int)g_mjpeg_next_to_write != fi->sequence_number) {
            pthread_cond_wait(&g_mjpeg_write_cond, &g_mjpeg_write_mutex);
        }

        if (g_mjpeg_enable_write) {
            fwrite(ctx->image_data[0], 1, fi->header.size_decompress, g_mjpeg_output_file);
        }

        g_mjpeg_next_to_write++;
        pthread_mutex_unlock(&g_mjpeg_write_mutex);
        pthread_cond_broadcast(&g_mjpeg_write_cond);

        // Metrica por frame
        printf("%s,%d,0,decoding,%ld\n", g_mjpeg_profile_name.c_str(),
               g_mjpeg_num_threads, av_gettime() - fi->timestamp);

        frame_item_free(fi);
    }

    av_packet_free(&pkt);

    // Desregistra thread
    pthread_mutex_lock(&g_mjpeg_active_threads_mutex);
    g_mjpeg_active_threads_count--;
    if (g_mjpeg_active_threads_count == 0) {
        pthread_cond_signal(&g_mjpeg_active_threads_cond);
    }
    pthread_mutex_unlock(&g_mjpeg_active_threads_mutex);

    return NULL;
}

// ============================================================================
// Funcoes Auxiliares
// ============================================================================

static void cleanup_thread_contexts(MJpegThreadContext *ctxs, int count) {
    if (!ctxs) return;
    for (int i = 0; i < count; i++) {
        if (ctxs[i].dec_ctx) avcodec_free_context(&ctxs[i].dec_ctx);
        if (ctxs[i].frame) av_frame_free(&ctxs[i].frame);
        if (ctxs[i].image_data[0]) av_freep(&ctxs[i].image_data[0]);
    }
}

// Inicializa N decoder contexts a partir do stream de video.
// Retorna 0 em sucesso, -1 em erro (ja faz cleanup parcial).
static int init_decoder_threads(MJpegThreadContext *ctxs, int count,
                                AVStream *video_stream, const AVCodec *codec) {
    for (int i = 0; i < count; i++) {
        ctxs[i].thread_id = i;

        ctxs[i].dec_ctx = avcodec_alloc_context3(codec);
        if (!ctxs[i].dec_ctx) {
            fprintf(stderr, "Erro: avcodec_alloc_context3 falhou para thread %d\n", i);
            cleanup_thread_contexts(ctxs, i);
            return -1;
        }

        int ret = avcodec_parameters_to_context(ctxs[i].dec_ctx,
                                                video_stream->codecpar);
        if (ret < 0) {
            fprintf(stderr, "Erro: avcodec_parameters_to_context falhou para thread %d\n", i);
            cleanup_thread_contexts(ctxs, i + 1);
            return -1;
        }

        ctxs[i].dec_ctx->thread_count = 1;

        ret = avcodec_open2(ctxs[i].dec_ctx, codec, NULL);
        if (ret < 0) {
            fprintf(stderr, "Erro: avcodec_open2 falhou para thread %d\n", i);
            cleanup_thread_contexts(ctxs, i + 1);
            return -1;
        }

        ctxs[i].frame = av_frame_alloc();
        if (!ctxs[i].frame) {
            fprintf(stderr, "Erro: av_frame_alloc falhou para thread %d\n", i);
            cleanup_thread_contexts(ctxs, i + 1);
            return -1;
        }

        ctxs[i].image_bufsize = av_image_alloc(
            ctxs[i].image_data, ctxs[i].image_linesize,
            ctxs[i].dec_ctx->width, ctxs[i].dec_ctx->height,
            ctxs[i].dec_ctx->pix_fmt, 1);
        if (ctxs[i].image_bufsize < 0) {
            fprintf(stderr, "Erro: av_image_alloc falhou para thread %d\n", i);
            cleanup_thread_contexts(ctxs, i + 1);
            return -1;
        }
    }
    return 0;
}

// ============================================================================
// Funcao Principal - Entry Point do Pipeline MT
// ============================================================================

int mjpeg_mt_decode(const char *input, const char *output,
                    const char *profile, int threads, bool enable_write) {
    pthread_t producer_thread;
    pthread_t *decoder_threads = NULL;
    MJpegThreadContext *thread_contexts = NULL;
    int ret;

    // Configuracao global
    g_input_filename = input;
    g_mjpeg_num_threads = threads;
    g_mjpeg_profile_name = profile;
    g_mjpeg_enable_write = enable_write;

    // -----------------------------------------------------------------------
    // Probe: abre avformat para obter stream info e criar decoder contexts
    // -----------------------------------------------------------------------
    AVFormatContext *probe_ctx = NULL;
    if (avformat_open_input(&probe_ctx, input, NULL, NULL) < 0) {
        fprintf(stderr, "Erro: Nao foi possivel abrir %s\n", input);
        return -1;
    }
    if (avformat_find_stream_info(probe_ctx, NULL) < 0) {
        fprintf(stderr, "Erro: Nao foi possivel obter info do stream\n");
        avformat_close_input(&probe_ctx);
        return -1;
    }

    int vsi = av_find_best_stream(probe_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (vsi < 0) {
        fprintf(stderr, "Erro: Stream de video nao encontrado\n");
        avformat_close_input(&probe_ctx);
        return -1;
    }

    AVStream *video_stream = probe_ctx->streams[vsi];
    const AVCodec *codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!codec) {
        fprintf(stderr, "Erro: Codec nao encontrado\n");
        avformat_close_input(&probe_ctx);
        return -1;
    }

    // -----------------------------------------------------------------------
    // Cria N AVCodecContext (um por consumer thread)
    // -----------------------------------------------------------------------
    decoder_threads = (pthread_t *)malloc(sizeof(pthread_t) * threads);
    thread_contexts = (MJpegThreadContext *)calloc(threads, sizeof(MJpegThreadContext));
    if (!decoder_threads || !thread_contexts) {
        fprintf(stderr, "Erro: malloc falhou para threads\n");
        free(decoder_threads);
        free(thread_contexts);
        avformat_close_input(&probe_ctx);
        return -1;
    }

    ret = init_decoder_threads(thread_contexts, threads, video_stream, codec);
    avformat_close_input(&probe_ctx);
    if (ret < 0) {
        free(decoder_threads);
        free(thread_contexts);
        return -1;
    }

    // -----------------------------------------------------------------------
    // Abre arquivo de saida
    // -----------------------------------------------------------------------
    if (g_mjpeg_enable_write) {
        g_mjpeg_output_file = fopen(output, "wb");
        if (!g_mjpeg_output_file) {
            fprintf(stderr, "Erro: Nao foi possivel abrir %s para escrita\n", output);
            cleanup_thread_contexts(thread_contexts, threads);
            free(decoder_threads);
            free(thread_contexts);
            return -1;
        }
    } else {
        g_mjpeg_output_file = NULL;
    }

    // -----------------------------------------------------------------------
    // Inicializa sincronizacao
    // -----------------------------------------------------------------------
    g_mjpeg_next_to_write = 0;
    g_mjpeg_producer_finished = false;
    g_mjpeg_total_frames_processed = 0;
    g_mjpeg_active_threads_count = 0;
    g_mjpeg_total_demux_time = 0;
    g_mjpeg_total_decode_time = 0;

    queue_init(&g_mjpeg_frame_queue, frame_item_free);

    // -----------------------------------------------------------------------
    // Inicia threads e mede tempo
    // -----------------------------------------------------------------------
    int64_t total_start_time = av_gettime();
    CpuStats cpu_start, cpu_end;
    cpu_stats_read(&cpu_start);

    ret = pthread_create(&producer_thread, NULL, mt_mjpeg_producer_thread, NULL);
    if (ret != 0) {
        perror("Erro ao criar thread produtora");
        queue_destroy(&g_mjpeg_frame_queue);
        if (g_mjpeg_enable_write && g_mjpeg_output_file) fclose(g_mjpeg_output_file);
        cleanup_thread_contexts(thread_contexts, threads);
        free(decoder_threads);
        free(thread_contexts);
        return -1;
    }

    for (int i = 0; i < threads; i++) {
        ret = pthread_create(&decoder_threads[i], NULL, mt_mjpeg_decoder_thread,
                             &thread_contexts[i]);
        if (ret != 0) {
            perror("Erro ao criar thread decodificadora");
            break;
        }
    }

    // -----------------------------------------------------------------------
    // Aguarda conclusao
    // -----------------------------------------------------------------------
    pthread_join(producer_thread, NULL);
    for (int i = 0; i < threads; i++) {
        pthread_join(decoder_threads[i], NULL);
    }

    int64_t total_time = av_gettime() - total_start_time;
    cpu_stats_read(&cpu_end);
    double cpu_usage = cpu_stats_calculate_usage(&cpu_start, &cpu_end);

    pthread_mutex_lock(&g_mjpeg_state_mutex);
    size_t total_frames = g_mjpeg_total_frames_processed;
    pthread_mutex_unlock(&g_mjpeg_state_mutex);

    // Imprime resumo (mesmo formato CSV do decode.cpp)
    printf("%s,%d,0,total,%ld\n", profile, threads, total_time);
    printf("%s,%d,0,fps,%lf\n", profile, threads, (total_frames * 1e6) / total_time);
    printf("%s,%d,0,cpu,%lf\n", profile, threads, cpu_usage);

    // -----------------------------------------------------------------------
    // Cleanup em ordem
    // -----------------------------------------------------------------------
    if (g_mjpeg_enable_write && g_mjpeg_output_file) fclose(g_mjpeg_output_file);
    g_mjpeg_output_file = NULL;

    queue_destroy(&g_mjpeg_frame_queue);

    cleanup_thread_contexts(thread_contexts, threads);
    free(decoder_threads);
    free(thread_contexts);

    return 0;
}
