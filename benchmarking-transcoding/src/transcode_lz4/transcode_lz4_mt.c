/**
 * @file transcode_lz4_mt.c
 * @brief Implementação da transcodificação LZ4/LZ4HC Multithread
 */

#include "transcode_lz4_mt.h"
#include "../cpu_stats.h"
#include <unistd.h>

// ============================================================================
// Definição das Variáveis Globais
// ============================================================================

// Fila unificada de frames (buffer + metadata)
FrameQueue g_frame_queue;

// Controle de escrita sequencial
size_t g_next_to_write = 0;
pthread_mutex_t g_write_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_write_cond = PTHREAD_COND_INITIALIZER;

// Controle de estado
bool g_producer_finished = false;
size_t g_total_frames_encoded = 0;
pthread_mutex_t g_state_mutex = PTHREAD_MUTEX_INITIALIZER;

// Controle de threads ativas
int g_active_threads_count = 0;
pthread_mutex_t g_active_threads_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_active_threads_cond = PTHREAD_COND_INITIALIZER;

// Arquivo de saída
FILE *g_output_file = NULL;

// Configuração
int g_num_decode_threads = 1;
int g_num_encoder_threads = 1;
int g_encoder_type = ENCODER_TYPE_LZ4;
int g_compression_level = 1;
const char *g_profile_name = NULL;
bool g_debug_mode = false;

// Métricas
volatile int64_t g_total_decode_time = 0;
volatile int64_t g_total_compress_time = 0;
volatile int64_t g_total_write_time = 0;
volatile int g_frame_count = 0;

// ============================================================================
// Funções de Inicialização
// ============================================================================

int mt_encode_init_queue(void) {
    return frame_queue_init(&g_frame_queue);
}

int mt_encode_init_synchronization(void) {
    int ret;

    // Reinicializa controle de escrita
    g_next_to_write = 0;
    ret = pthread_mutex_init(&g_write_mutex, NULL);
    if (ret != 0) {
        return -1;
    }
    ret = pthread_cond_init(&g_write_cond, NULL);
    if (ret != 0) {
        pthread_mutex_destroy(&g_write_mutex);
        return -1;
    }

    // Reinicializa estado
    g_producer_finished = false;
    g_total_frames_encoded = 0;
    ret = pthread_mutex_init(&g_state_mutex, NULL);
    if (ret != 0) {
        pthread_mutex_destroy(&g_write_mutex);
        pthread_cond_destroy(&g_write_cond);
        return -1;
    }

    // Reinicializa controle de threads ativas
    g_active_threads_count = 0;
    ret = pthread_mutex_init(&g_active_threads_mutex, NULL);
    if (ret != 0) {
        pthread_mutex_destroy(&g_write_mutex);
        pthread_cond_destroy(&g_write_cond);
        pthread_mutex_destroy(&g_state_mutex);
        return -1;
    }
    ret = pthread_cond_init(&g_active_threads_cond, NULL);
    if (ret != 0) {
        pthread_mutex_destroy(&g_write_mutex);
        pthread_cond_destroy(&g_write_cond);
        pthread_mutex_destroy(&g_state_mutex);
        pthread_mutex_destroy(&g_active_threads_mutex);
        return -1;
    }

    // Reseta métricas
    g_total_decode_time = 0;
    g_total_compress_time = 0;
    g_total_write_time = 0;
    g_frame_count = 0;

    return 0;
}

int mt_encode_init_state(FILE *output_file) {
    g_output_file = output_file;
    return 0;
}

// ============================================================================
// Funções de Limpeza
// ============================================================================

void mt_encode_cleanup_queue(void) {
    frame_queue_destroy(&g_frame_queue);
}

void mt_encode_cleanup_synchronization(void) {
    pthread_mutex_destroy(&g_write_mutex);
    pthread_cond_destroy(&g_write_cond);
    pthread_mutex_destroy(&g_state_mutex);
    pthread_mutex_destroy(&g_active_threads_mutex);
    pthread_cond_destroy(&g_active_threads_cond);
}

void mt_encode_cleanup_state(void) {
    g_output_file = NULL;
}

// ============================================================================
// Thread Produtora (Decodificador FFmpeg)
// ============================================================================

void *mt_producer_thread(void *arg) {
    ProducerArgs *args = (ProducerArgs *)arg;
    const char *input_filename = args->input_filename;
    int decode_thread_count = args->decode_thread_count;
    int encoder_thread_count = args->encoder_thread_count;

    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *dec_ctx = NULL;
    const AVCodec *dec = NULL;
    AVPacket *pkt = NULL;
    AVFrame *frame = NULL;
    int video_stream_index = -1;
    int ret;
    int sequence_number = 0;
    uint8_t *frame_data = NULL;
    int max_frame_size = 0;

    // Abre arquivo de entrada
    if (avformat_open_input(&fmt_ctx, input_filename, NULL, NULL) < 0) {
        fprintf(stderr, "[Producer] Erro: Não foi possível abrir arquivo %s\n", input_filename);
        goto producer_error;
    }

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "[Producer] Erro: Não foi possível encontrar informação de streams\n");
        goto producer_error;
    }

    // Encontra stream de vídeo
    video_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
    if (video_stream_index < 0) {
        fprintf(stderr, "[Producer] Erro: Não foi possível encontrar stream de vídeo\n");
        goto producer_error;
    }

    // Cria contexto do decoder
    dec_ctx = avcodec_alloc_context3(dec);
    if (!dec_ctx) {
        fprintf(stderr, "[Producer] Erro: Não foi possível alocar contexto do decoder\n");
        goto producer_error;
    }

    // Copia parâmetros do stream
    if (avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[video_stream_index]->codecpar) < 0) {
        fprintf(stderr, "[Producer] Erro: Não foi possível copiar parâmetros do codec\n");
        goto producer_error;
    }

    // Configura threading do decodificador ANTES de abrir
    if (decode_thread_count > 1) {
        dec_ctx->thread_count = decode_thread_count;
        dec_ctx->thread_type = FF_THREAD_FRAME;
    }

    // Abre decoder
    if (avcodec_open2(dec_ctx, dec, NULL) < 0) {
        fprintf(stderr, "[Producer] Erro: Não foi possível abrir decoder\n");
        goto producer_error;
    }

    // Aloca packet e frame
    pkt = av_packet_alloc();
    frame = av_frame_alloc();
    if (!pkt || !frame) {
        fprintf(stderr, "[Producer] Erro: Não foi possível alocar packet/frame\n");
        goto producer_error;
    }

    // Buffer temporário para copiar dados do frame
    max_frame_size = av_image_get_buffer_size(dec_ctx->pix_fmt, dec_ctx->width, dec_ctx->height, 1);
    frame_data = (uint8_t *)malloc(max_frame_size);
    if (!frame_data) {
        fprintf(stderr, "[Producer] Erro: Não foi possível alocar buffer temporário\n");
        goto producer_error;
    }

    if (g_debug_mode) {
        printf("[Producer] Iniciando decodificação: %dx%d, pix_fmt=%d\n",
               dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt);
    }

    // Loop principal de decodificação
    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index != video_stream_index) {
            av_packet_unref(pkt);
            continue;
        }

        // Sobrescreve DTS com timestamp atual (compatível com transcode.c)
        pkt->dts = av_gettime();

        int64_t decode_start = av_gettime();

        // Envia packet para decoder
        ret = avcodec_send_packet(dec_ctx, pkt);
        if (ret < 0) {
            fprintf(stderr, "[Producer] Erro ao enviar packet para decoder\n");
            av_packet_unref(pkt);
            continue;
        }

        // Recebe frames decodificados
        while (ret >= 0) {
            ret = avcodec_receive_frame(dec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                fprintf(stderr, "[Producer] Erro durante decodificação\n");
                break;
            }

            int64_t decode_time = av_gettime() - decode_start;
            g_total_decode_time += decode_time;

            // Calcula tamanho do frame
            int frame_size = av_image_get_buffer_size(frame->format, frame->width, frame->height, 1);

            // Realoca buffer se necessário
            if (frame_size > max_frame_size) {
                free(frame_data);
                max_frame_size = frame_size;
                frame_data = (uint8_t *)malloc(max_frame_size);
                if (!frame_data) {
                    fprintf(stderr, "[Producer] Erro: Não foi possível realocar buffer\n");
                    av_frame_unref(frame);
                    continue;
                }
            }

            // Copia dados do frame para buffer contíguo
            uint8_t *dst[4] = {0};
            int dst_linesize[4] = {0};
            // Preenche os ponteiros e linesizes corretamente para o formato do frame
            av_image_fill_arrays(dst, dst_linesize, frame_data,
                                 frame->format, frame->width, frame->height, 1);
            av_image_copy(dst, dst_linesize,
                          (const uint8_t **)frame->data, frame->linesize,
                          frame->format, frame->width, frame->height);

            // Duplica o buffer para transferir ownership
            uint8_t *data_copy = (uint8_t *)malloc(frame_size);
            if (!data_copy) {
                fprintf(stderr, "[Producer] Erro: Não foi possível alocar buffer para frame %d\n", sequence_number);
                av_frame_unref(frame);
                continue;
            }
            memcpy(data_copy, frame_data, frame_size);

            // Cria AVBufferRef com os dados
            AVBufferRef *buffer = av_buffer_create(data_copy, frame_size, NULL, NULL, 0);
            if (!buffer) {
                fprintf(stderr, "[Producer] Erro: Não foi possível criar AVBufferRef\n");
                free(data_copy);
                av_frame_unref(frame);
                continue;
            }

            // Push único e atômico na fila unificada (buffer + metadata)
            frame_queue_push(&g_frame_queue, buffer,
                             sequence_number, frame_size,
                             frame->width, frame->height, frame->format,
                             frame->pkt_dts);
            // buffer ownership transferido para a fila

            if (g_debug_mode) {
                printf("[METRICA] DECODE_FRAME %d %ld\n", sequence_number, decode_time);
            }

            sequence_number++;
            g_frame_count++;

            av_frame_unref(frame);
            decode_start = av_gettime();  // Reseta para próximo frame
        }

        av_packet_unref(pkt);
    }

    // Flush do decoder
    avcodec_send_packet(dec_ctx, NULL);
    while (avcodec_receive_frame(dec_ctx, frame) >= 0) {
        int64_t decode_start = av_gettime();

        int frame_size = av_image_get_buffer_size(frame->format, frame->width, frame->height, 1);

        // Realoca buffer se necessário
        if (frame_size > max_frame_size) {
            free(frame_data);
            max_frame_size = frame_size;
            frame_data = (uint8_t *)malloc(max_frame_size);
            if (!frame_data) {
                av_frame_unref(frame);
                continue;
            }
        }

        uint8_t *dst[4] = {0};
        int dst_linesize[4] = {0};
        av_image_fill_arrays(dst, dst_linesize, frame_data,
                             frame->format, frame->width, frame->height, 1);
        av_image_copy(dst, dst_linesize,
                      (const uint8_t **)frame->data, frame->linesize,
                      frame->format, frame->width, frame->height);

        uint8_t *data_copy = (uint8_t *)malloc(frame_size);
        if (!data_copy) {
            av_frame_unref(frame);
            continue;
        }
        memcpy(data_copy, frame_data, frame_size);

        AVBufferRef *buffer = av_buffer_create(data_copy, frame_size, NULL, NULL, 0);
        if (!buffer) {
            free(data_copy);
            av_frame_unref(frame);
            continue;
        }

        // Push único e atômico na fila unificada (buffer + metadata)
        // Durante flush, pkt_dts = 0 indica que não deve logar latência
        frame_queue_push(&g_frame_queue, buffer,
                         sequence_number, frame_size,
                         frame->width, frame->height, frame->format,
                         0);

        int64_t decode_time = av_gettime() - decode_start;
        g_total_decode_time += decode_time;

        if (g_debug_mode) {
            printf("[METRICA] DECODE_FRAME %d %ld\n", sequence_number, decode_time);
        }

        sequence_number++;
        g_frame_count++;
        av_frame_unref(frame);
    }

    // Marca produtor como finalizado
    pthread_mutex_lock(&g_state_mutex);
    g_producer_finished = true;
    g_total_frames_encoded = sequence_number;
    pthread_mutex_unlock(&g_state_mutex);

    if (g_debug_mode) {
        printf("[Producer] Finalizado. Total de frames: %d\n", sequence_number);
    }

    // Envia sinais de término para threads codificadoras (NULL buffers = poison pill)
    for (int i = 0; i < encoder_thread_count; i++) {
        frame_queue_push(&g_frame_queue, NULL, -1, 0, 0, 0, 0, 0);
    }

    // Aguarda todas as threads codificadoras terminarem
    pthread_mutex_lock(&g_active_threads_mutex);
    while (g_active_threads_count > 0) {
        pthread_cond_wait(&g_active_threads_cond, &g_active_threads_mutex);
    }
    pthread_mutex_unlock(&g_active_threads_mutex);

    // Cleanup
    free(frame_data);
producer_error:
    if (frame) av_frame_free(&frame);
    if (pkt) av_packet_free(&pkt);
    if (dec_ctx) avcodec_free_context(&dec_ctx);
    if (fmt_ctx) avformat_close_input(&fmt_ctx);

    return NULL;
}

// ============================================================================
// Thread Codificadora (LZ4/LZ4HC)
// ============================================================================

void *mt_encoder_thread(void *arg) {
    EncoderThreadContext *ctx = (EncoderThreadContext *)arg;
    int thread_id = ctx->thread_id;

    // Incrementa contador de threads ativas
    pthread_mutex_lock(&g_active_threads_mutex);
    g_active_threads_count++;
    pthread_mutex_unlock(&g_active_threads_mutex);

    if (g_debug_mode) {
        printf("[Encoder %d] Iniciando (tipo=%d, nível=%d)\n",
               thread_id, ctx->encoder_type, ctx->compression_level);
    }

    // Loop principal
    while (1) {
        // Pop único e atômico da fila unificada (buffer + metadata)
        FrameQueueItem item = frame_queue_pop(&g_frame_queue);

        // Verifica sinal de término (poison pill = buffer NULL)
        if (item.buffer == NULL) {
            if (g_debug_mode) {
                printf("[Encoder %d] Recebeu sinal de término\n", thread_id);
            }
            break;
        }

        // Comprime o frame
        int64_t compress_start = av_gettime();

        int compressed_size;
        if (ctx->encoder_type == ENCODER_TYPE_LZ4HC) {
            compressed_size = LZ4_compress_HC(
                (const char *)item.buffer->data,
                ctx->compressed_buffer,
                item.size_decompress,
                ctx->max_compressed_size,
                ctx->compression_level
            );
        } else {
            compressed_size = LZ4_compress_fast(
                (const char *)item.buffer->data,
                ctx->compressed_buffer,
                item.size_decompress,
                ctx->max_compressed_size,
                ctx->compression_level
            );
        }

        int64_t compress_time = av_gettime() - compress_start;
        g_total_compress_time += compress_time;

        if (compressed_size <= 0) {
            fprintf(stderr, "[Encoder %d] Erro na compressão do frame %d\n",
                    thread_id, item.sequence_number);
            av_buffer_unref(&item.buffer);
            continue;
        }

        // Prepara header para escrita
        FrameHeader header = {
            .size_decompress = item.size_decompress,
            .size_compress = compressed_size,
            .width = item.width,
            .height = item.height,
            .pix_fmt = item.pix_fmt
        };

        if (g_debug_mode) {
            printf("[METRICA] COMPRESS_FRAME %d %ld %d->%d (%.1f%%)\n",
                   item.sequence_number, compress_time,
                   header.size_decompress, compressed_size,
                   100.0 * compressed_size / header.size_decompress);
        }

        // Aguarda sua vez de escrever (escrita sequencial)
        pthread_mutex_lock(&g_write_mutex);
        while ((int)g_next_to_write != item.sequence_number) {
            pthread_cond_wait(&g_write_cond, &g_write_mutex);
        }

        // Escreve frame no arquivo
        int64_t write_start = av_gettime();

        size_t written1 = fwrite(&header, sizeof(FrameHeader), 1, g_output_file);
        size_t written2 = fwrite(ctx->compressed_buffer, 1, compressed_size, g_output_file);

        int64_t write_time = av_gettime() - write_start;
        g_total_write_time += write_time;

        if (written1 != 1 || written2 != (size_t)compressed_size) {
            fprintf(stderr, "[Encoder %d] Erro ao escrever frame %d\n",
                    thread_id, item.sequence_number);
        }

        if (g_debug_mode) {
            printf("[METRICA] WRITE_FRAME %d %ld\n", item.sequence_number, write_time);
        }

        // Log de latência por frame (formato compatível com transcode.c)
        // Só loga se pkt_dts != 0 (frames do flush têm pkt_dts = 0)
        if (!g_debug_mode && item.pkt_dts != 0) {
            int64_t latency = av_gettime() - item.pkt_dts;
            printf("%s,%d,%d,transcoding,%ld,%d\n",
                   g_profile_name, g_num_decode_threads, g_num_encoder_threads,
                   latency, g_compression_level);
        }

        // Atualiza contador e acorda outras threads
        g_next_to_write++;
        pthread_mutex_unlock(&g_write_mutex);
        pthread_cond_broadcast(&g_write_cond);


        // Libera buffer (ownership transferido do pop)
        av_buffer_unref(&item.buffer);
    }

    // Decrementa contador de threads ativas
    pthread_mutex_lock(&g_active_threads_mutex);
    g_active_threads_count--;
    if (g_active_threads_count == 0) {
        pthread_cond_signal(&g_active_threads_cond);
    }
    pthread_mutex_unlock(&g_active_threads_mutex);

    if (g_debug_mode) {
        printf("[Encoder %d] Finalizado\n", thread_id);
    }

    return NULL;
}

// ============================================================================
// Função Principal Multithread
// ============================================================================

int mt_encode_main(const char *input_file, const char *output_file,
                   int num_decode_threads, int num_encoder_threads,
                   int encoder_type, int compression_level,
                   const char *profile, bool debug_mode) {
    pthread_t producer_thread;
    pthread_t *encoder_threads = NULL;
    EncoderThreadContext *thread_contexts = NULL;
    ProducerArgs producer_args;
    FILE *outfile = NULL;
    int ret;
    int64_t total_start_time;
    CpuStats cpu_start, cpu_end;

    // Configuração global
    g_num_decode_threads = num_decode_threads;
    g_num_encoder_threads = num_encoder_threads;
    g_encoder_type = encoder_type;
    g_compression_level = compression_level;
    g_profile_name = profile;
    g_debug_mode = debug_mode;

    if (g_debug_mode) {
        printf("========== MODO DEBUG ==========\n");
        printf("Profile: %s | Decode Threads: %d | Encoder Threads: %d | Encoder: %s | Level: %d\n\n",
               profile, num_decode_threads, num_encoder_threads,
               encoder_type == ENCODER_TYPE_LZ4HC ? "lz4hc" : "lz4",
               compression_level);
    }

    // Abre arquivo de saída
    outfile = fopen(output_file, "wb");
    if (!outfile) {
        fprintf(stderr, "Erro: Não foi possível abrir arquivo de saída: %s\n", output_file);
        return -1;
    }

    // Inicializa fila
    if (mt_encode_init_queue() < 0) {
        fprintf(stderr, "Erro: Falha ao inicializar fila\n");
        fclose(outfile);
        return -1;
    }

    // Inicializa sincronização
    if (mt_encode_init_synchronization() < 0) {
        fprintf(stderr, "Erro: Falha ao inicializar sincronização\n");
        mt_encode_cleanup_queue();
        fclose(outfile);
        return -1;
    }

    // Inicializa estado
    if (mt_encode_init_state(outfile) < 0) {
        fprintf(stderr, "Erro: Falha ao inicializar estado\n");
        mt_encode_cleanup_synchronization();
        mt_encode_cleanup_queue();
        fclose(outfile);
        return -1;
    }

    // Aloca threads e contextos
    encoder_threads = (pthread_t *)malloc(sizeof(pthread_t) * num_encoder_threads);
    thread_contexts = (EncoderThreadContext *)malloc(sizeof(EncoderThreadContext) * num_encoder_threads);

    if (!encoder_threads || !thread_contexts) {
        fprintf(stderr, "Erro: Não foi possível alocar memória para threads\n");
        free(encoder_threads);
        free(thread_contexts);
        mt_encode_cleanup_state();
        mt_encode_cleanup_synchronization();
        mt_encode_cleanup_queue();
        fclose(outfile);
        return -1;
    }

    // Estimativa do tamanho máximo comprimido (8K YUV420P = ~37MB)
    // Suporta resoluções de até 7680x4320
    int max_frame_size = 7680 * 4320 * 3 / 2;  // YUV420P 8K
    int max_compressed_size = LZ4_compressBound(max_frame_size);

    // Inicializa contextos das threads codificadoras
    for (int i = 0; i < num_encoder_threads; i++) {
        thread_contexts[i].thread_id = i;
        thread_contexts[i].encoder_type = encoder_type;
        thread_contexts[i].compression_level = compression_level;
        thread_contexts[i].max_compressed_size = max_compressed_size;
        thread_contexts[i].compressed_buffer = (char *)malloc(max_compressed_size);

        if (!thread_contexts[i].compressed_buffer) {
            fprintf(stderr, "Erro: Não foi possível alocar buffer para thread %d\n", i);
            // Cleanup parcial
            for (int j = 0; j < i; j++) {
                free(thread_contexts[j].compressed_buffer);
            }
            free(encoder_threads);
            free(thread_contexts);
            mt_encode_cleanup_state();
            mt_encode_cleanup_synchronization();
            mt_encode_cleanup_queue();
            fclose(outfile);
            return -1;
        }
    }

    total_start_time = av_gettime();
    cpu_stats_read(&cpu_start);

    // Prepara argumentos do produtor
    producer_args.input_filename = input_file;
    producer_args.decode_thread_count = num_decode_threads;
    producer_args.encoder_thread_count = num_encoder_threads;

    // Cria thread produtora
    ret = pthread_create(&producer_thread, NULL, mt_producer_thread, &producer_args);
    if (ret != 0) {
        perror("Erro ao criar thread produtora");
        for (int i = 0; i < num_encoder_threads; i++) {
            free(thread_contexts[i].compressed_buffer);
        }
        free(encoder_threads);
        free(thread_contexts);
        mt_encode_cleanup_state();
        mt_encode_cleanup_synchronization();
        mt_encode_cleanup_queue();
        fclose(outfile);
        return -1;
    }

    // Cria threads codificadoras
    for (int i = 0; i < num_encoder_threads; i++) {
        ret = pthread_create(&encoder_threads[i], NULL, mt_encoder_thread, &thread_contexts[i]);
        if (ret != 0) {
            perror("Erro ao criar thread codificadora");
            break;
        }
    }

    // Aguarda thread produtora
    pthread_join(producer_thread, NULL);

    // Aguarda threads codificadoras
    for (int i = 0; i < num_encoder_threads; i++) {
        pthread_join(encoder_threads[i], NULL);
    }

    int64_t total_time = av_gettime() - total_start_time;
    cpu_stats_read(&cpu_end);
    double cpu_usage = cpu_stats_calculate_usage(&cpu_start, &cpu_end);

    // Imprime resumo
    int total_frames = g_frame_count;

    if (g_debug_mode) {
        printf("\n========== RESUMO ==========\n");
        printf("Profile: %s | Decode Threads: %d | Encoder Threads: %d | Encoder: %s\n",
               profile, num_decode_threads, num_encoder_threads,
               encoder_type == ENCODER_TYPE_LZ4HC ? "lz4hc" : "lz4");
        printf("Total frames: %d\n", total_frames);
        printf("Tempo total: %ld us (%.2f s)\n", total_time, total_time / 1e6);
        printf("FPS: %.1f\n", (total_frames * 1e6) / total_time);
        printf("[RESUMO] CPU_USAGE %.1f\n", cpu_usage);

        if (total_frames > 0) {
            printf("[RESUMO] DECODE_TIME_TOTAL %ld\n", g_total_decode_time);
            printf("[RESUMO] DECODE_TIME_AVG %ld\n", g_total_decode_time / total_frames);
            printf("[RESUMO] COMPRESS_TIME_TOTAL %ld\n", g_total_compress_time);
            printf("[RESUMO] COMPRESS_TIME_AVG %ld\n", g_total_compress_time / total_frames);
            printf("[RESUMO] WRITE_TIME_TOTAL %ld\n", g_total_write_time);
            printf("[RESUMO] WRITE_TIME_AVG %ld\n", g_total_write_time / total_frames);
        }
    } else {
        // Formato padrão (compatível com transcode.c)
        printf("%s,%d,%d,total,%ld,%d\n", profile, num_decode_threads, num_encoder_threads, total_time, compression_level);
        printf("%s,%d,%d,fps,%.1f,%d\n", profile, num_decode_threads, num_encoder_threads,
               (total_frames * 1e6) / total_time, compression_level);
    }

    // Limpeza final
    fclose(outfile);

    for (int i = 0; i < num_encoder_threads; i++) {
        free(thread_contexts[i].compressed_buffer);
    }
    free(encoder_threads);
    free(thread_contexts);

    mt_encode_cleanup_state();
    mt_encode_cleanup_synchronization();
    mt_encode_cleanup_queue();

    return 0;
}
