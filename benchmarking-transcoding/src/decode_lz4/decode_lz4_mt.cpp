// ============================================================================
// decode_lz4_mt.cpp - Decodificador LZ4 Multithread
// Arquitetura: Fila Generica (Queue + FrameItem)
// ============================================================================

#include "decode_lz4.h"
#include "../cpu_stats.h"
#include "../fps_limiter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <atomic>

// ============================================================================
// Definição das variáveis globais
// ============================================================================

// Fila de frames (generica com FrameItem)
Queue g_frame_queue;

// Controle de escrita sequencial no arquivo de saída
size_t g_next_to_write = 0;
pthread_mutex_t g_write_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_write_cond = PTHREAD_COND_INITIALIZER;

// Controle de estado global
bool g_producer_finished = false;
size_t g_total_frames_processed = 0;
pthread_mutex_t g_state_mutex = PTHREAD_MUTEX_INITIALIZER;

// Controle de threads ativas (para shutdown correto)
int g_active_threads_count = 0;
pthread_mutex_t g_active_threads_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_active_threads_cond = PTHREAD_COND_INITIALIZER;

// Arquivos
FILE *g_input_file = NULL;
FILE *g_output_file = NULL;

// Configuração
int g_num_decoder_threads = 0;
std::string g_profile_name;
double g_target_fps = 0.0;

// Métricas acumuladas (atómicas para thread-safety)
std::atomic<int64_t> g_total_read_time{0};
std::atomic<int64_t> g_total_decode_time{0};
std::atomic<int> g_read_frame_count{0};
std::atomic<int> g_decode_frame_count{0};

// ============================================================================
// Funções de Inicialização
// ============================================================================

int mt_initialize_write_control(FILE *output_file) {
    g_next_to_write = 0;
    g_output_file = output_file;
    return 0;
}

int mt_initialize_state(FILE *input_file) {
    g_producer_finished = false;
    g_total_frames_processed = 0;
    g_input_file = input_file;
    g_active_threads_count = 0;  // Inicializa contador de threads ativas
    return 0;
}

// ============================================================================
// Funções de Limpeza
// ============================================================================

void mt_cleanup_write_control() {
    g_output_file = NULL;
}

void mt_cleanup_state() {
    g_input_file = NULL;
}

// ============================================================================
// Thread Produtora
// ============================================================================

void *mt_producer_thread(void *arg) {
    (void)arg;  // Não usado

    int sequence_number = 0;

    FpsLimiter limiter;
    fps_limiter_init(&limiter, g_target_fps);

    // Loop principal de leitura
    while (1) {
        FrameHeader header;
        int ret;

        // 1. Medir tempo de leitura (header + dados)
        int64_t read_start = av_gettime();

        // 1a. Ler header do frame
        ret = fread(&header, sizeof(FrameHeader), 1, g_input_file);
        if (ret != 1) {
            if (feof(g_input_file)) {
                break;
            }
            fprintf(stderr, "[Producer] Erro ao ler header no frame %d\n", sequence_number);
            break;
        }

        // 2. Alocar e ler dados compactados
        char *comp_data = (char *)malloc(header.size_compress);
        if (!comp_data) {
            fprintf(stderr, "[Producer] Erro: Nao foi possivel alocar buffer para frame %d\n", sequence_number);
            break;
        }

        ret = fread(comp_data, 1, header.size_compress, g_input_file);
        if (ret != header.size_compress) {
            fprintf(stderr, "[Producer] Erro: Arquivo terminou antes do esperado\n");
            free(comp_data);
            break;
        }

        int64_t read_time = av_gettime() - read_start;

        // FPS limiter (após leitura completa do frame, antes de processamento)
        fps_limiter_wait(&limiter);

        // 2a. Acumular métricas de leitura
        g_total_read_time += read_time;
        g_read_frame_count++;

        // 2b. Imprimir métrica de leitura se debug mode
        if (g_debug_mode) {
            printf("[METRICA] READ_FRAME %d %ld\n", sequence_number, read_time);
        }

        // 3. Cria AVBufferRef com os dados comprimidos
        AVBufferRef *buffer = av_buffer_create((uint8_t *)comp_data, header.size_compress,
                                               av_buffer_default_free, NULL, 0);
        if (!buffer) {
            fprintf(stderr, "[Producer] Erro: Nao foi possivel criar AVBufferRef\n");
            free(comp_data);
            break;
        }

        // 4. Cria FrameItem e push na fila
        FrameItem *fi = (FrameItem *)malloc(sizeof(FrameItem));
        if (!fi) {
            fprintf(stderr, "[Producer] Erro: Nao foi possivel alocar FrameItem %d\n", sequence_number);
            av_buffer_unref(&buffer);
            break;
        }
        fi->header = header;
        fi->sequence_number = sequence_number;
        fi->timestamp = av_gettime();  // Momento da leitura do frame
        fi->data = buffer;

        queue_push(&g_frame_queue, fi);
        // fi ownership transferido para a fila

        sequence_number++;
    }

    // Marca que produtor terminou
    pthread_mutex_lock(&g_state_mutex);
    g_producer_finished = true;
    g_total_frames_processed = sequence_number;
    pthread_mutex_unlock(&g_state_mutex);

    // Envia poison pills para cada thread decodificadora
    for (int i = 0; i < g_num_decoder_threads; i++) {
        queue_push(&g_frame_queue, NULL);
    }

    // Aguarda todas as threads decodificadoras terminarem ANTES de liberar recursos
    pthread_mutex_lock(&g_active_threads_mutex);
    while (g_active_threads_count > 0) {
        pthread_cond_wait(&g_active_threads_cond, &g_active_threads_mutex);
    }
    pthread_mutex_unlock(&g_active_threads_mutex);

    return NULL;
}

// ============================================================================
// Thread Decodificadora
// ============================================================================

void *mt_decoder_thread(void *arg) {
    ThreadContext *ctx = (ThreadContext *)arg;
    int thread_id = ctx->thread_id;

    // Incrementa contador de threads ativas
    pthread_mutex_lock(&g_active_threads_mutex);
    g_active_threads_count++;
    pthread_mutex_unlock(&g_active_threads_mutex);

    // Loop principal
    while (1) {
        // 1. Pop da fila (bloqueante)
        FrameItem *fi = (FrameItem *)queue_pop(&g_frame_queue);

        // 2. Verifica poison pill (NULL)
        if (!fi) {
            break;
        }

        // 3. Decodificação (processamento paralelo, sem locks)
        int64_t decode_start = av_gettime();

        int decompressed_size = LZ4_decompress_safe(
            (const char *)fi->data->data,
            (char *)ctx->decode_ctx.image_data[0],
            fi->header.size_compress,
            ctx->decode_ctx.max_capacity
        );

        if (decompressed_size < 0) {
            fprintf(stderr, "[Decoder %d] Erro na decodificacao do frame %d\n",
                    thread_id, fi->sequence_number);
            frame_item_free(fi);
            continue;
        }

        int64_t decode_time = av_gettime() - decode_start;

        // 3a. Acumular métricas de decodificação
        g_total_decode_time += decode_time;
        g_decode_frame_count++;

        // 3b. Imprimir métrica de decodificação se debug mode
        if (g_debug_mode) {
            printf("[METRICA] DECODE_FRAME %d %ld\n", fi->sequence_number, decode_time);
        }

        // 4. Aguarda sua vez de escrever (escrita sequencial)
        pthread_mutex_lock(&g_write_mutex);

        while ((int)g_next_to_write != fi->sequence_number) {
            pthread_cond_wait(&g_write_cond, &g_write_mutex);
        }

        // 5. Escreve frame no arquivo
        if (g_enable_write) {
            size_t written = fwrite(ctx->decode_ctx.image_data[0], 1,
                                    fi->header.size_decompress, g_output_file);
            if (written != (size_t)fi->header.size_decompress) {
                fprintf(stderr, "[Decoder %d] Erro ao escrever frame %d\n",
                        thread_id, fi->sequence_number);
            }
        }

        // 6. Atualiza contador e acorda outras threads
        g_next_to_write++;
        pthread_mutex_unlock(&g_write_mutex);
        pthread_cond_broadcast(&g_write_cond);

        // 7. Imprime estatísticas (apenas se não estiver em modo debug)
        if (!g_debug_mode) {
            stats_print_frame(g_profile_name.c_str(), g_num_decoder_threads, 0, decode_time);
        }

        // 8. Libera FrameItem
        frame_item_free(fi);
    }

    // Decrementa contador de threads ativas e acorda produtor se necessário
    pthread_mutex_lock(&g_active_threads_mutex);
    g_active_threads_count--;
    if (g_active_threads_count == 0) {
        pthread_cond_signal(&g_active_threads_cond);
    }
    pthread_mutex_unlock(&g_active_threads_mutex);

    return NULL;
}

// ============================================================================
// Função Principal Multithread
// ============================================================================

int mt_decode_main(int num_threads, const std::string &input_file,
                   const std::string &output_file, const std::string &profile,
                   double target_fps) {
    pthread_t producer_thread;
    pthread_t *decoder_threads = NULL;
    ThreadContext *thread_contexts = NULL;
    FrameHeader first_header;
    int ret;
    int64_t total_start_time;
    CpuStats cpu_start, cpu_end;

    g_num_decoder_threads = num_threads;
    g_profile_name = profile;
    g_target_fps = target_fps;

    // Imprime cabeçalho de debug se modo debug ativado
    if (g_debug_mode) {
        printf("========== MODO DEBUG ==========\n");
        printf("Profile: %s | Threads: %d\n\n", profile.c_str(), num_threads);
    }

    // Abre arquivos
    g_input_file = fopen(input_file.c_str(), "rb");
    if (!g_input_file) {
        fprintf(stderr, "Erro: Nao foi possivel abrir arquivo de entrada: %s\n", input_file.c_str());
        return -1;
    }

    if (g_enable_write) {
        g_output_file = fopen(output_file.c_str(), "wb");
        if (!g_output_file) {
            fprintf(stderr, "Erro: Nao foi possivel abrir arquivo de saida: %s\n", output_file.c_str());
            fclose(g_input_file);
            return -1;
        }
    } else {
        g_output_file = NULL;
    }

    // Lê primeiro header para obter dimensões
    ret = fread(&first_header, sizeof(FrameHeader), 1, g_input_file);
    if (ret != 1) {
        fprintf(stderr, "Erro: Nao foi possivel ler primeiro header\n");
        fclose(g_input_file);
        fclose(g_output_file);
        return -1;
    }

    // Volta para o início do arquivo
    rewind(g_input_file);

    // Inicializa fila generica com frame_item_free como callback
    if (queue_init(&g_frame_queue, frame_item_free) < 0) {
        fclose(g_input_file);
        if (g_enable_write && g_output_file) fclose(g_output_file);
        return -1;
    }

    // Inicializa controle de escrita
    if (mt_initialize_write_control(g_output_file) < 0) {
        queue_destroy(&g_frame_queue);
        fclose(g_input_file);
        if (g_enable_write && g_output_file) fclose(g_output_file);
        return -1;
    }

    // Inicializa estado
    if (mt_initialize_state(g_input_file) < 0) {
        mt_cleanup_write_control();
        queue_destroy(&g_frame_queue);
        fclose(g_input_file);
        if (g_enable_write && g_output_file) fclose(g_output_file);
        return -1;
    }

    // Aloca contextos das threads
    decoder_threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);
    thread_contexts = (ThreadContext *)malloc(sizeof(ThreadContext) * num_threads);

    if (!decoder_threads || !thread_contexts) {
        fprintf(stderr, "Erro: Nao foi possivel alocar memoria para threads\n");
        free(decoder_threads);
        free(thread_contexts);
        mt_cleanup_state();
        mt_cleanup_write_control();
        queue_destroy(&g_frame_queue);
        fclose(g_input_file);
        if (g_enable_write && g_output_file) fclose(g_output_file);
        return -1;
    }

    // Inicializa contextos das threads decodificadoras
    for (int i = 0; i < num_threads; i++) {
        thread_contexts[i].thread_id = i;
        decode_context_init(&thread_contexts[i].decode_ctx);

        // Aloca buffers de imagem
        if (decode_context_alloc_buffers(&thread_contexts[i].decode_ctx, &first_header) < 0) {
            fprintf(stderr, "Erro: Nao foi possivel alocar buffers para thread %d\n", i);
            // Cleanup parcial
            for (int j = 0; j < i; j++) {
                decode_context_free(&thread_contexts[j].decode_ctx);
            }
            free(decoder_threads);
            free(thread_contexts);
            mt_cleanup_state();
            mt_cleanup_write_control();
            queue_destroy(&g_frame_queue);
            fclose(g_input_file);
            if (g_enable_write && g_output_file) fclose(g_output_file);
            return -1;
        }
    }

    total_start_time = av_gettime();
    cpu_stats_read(&cpu_start);

    // Cria thread produtora
    if (pthread_create(&producer_thread, NULL, mt_producer_thread, NULL) != 0) {
        perror("Erro ao criar thread produtora");
        // Cleanup
        for (int i = 0; i < num_threads; i++) {
            decode_context_free(&thread_contexts[i].decode_ctx);
        }
        free(decoder_threads);
        free(thread_contexts);
        mt_cleanup_state();
        mt_cleanup_write_control();
        queue_destroy(&g_frame_queue);
        fclose(g_input_file);
        if (g_enable_write && g_output_file) fclose(g_output_file);
        return -1;
    }

    // Cria threads decodificadoras
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&decoder_threads[i], NULL, mt_decoder_thread, &thread_contexts[i]) != 0) {
            perror("Erro ao criar thread decodificadora");
            // Nota: cleanup complexo aqui, threads já criadas continuam rodando
            // Em produção, seria necessário um mecanismo de shutdown limpo
            break;
        }
    }

    // Aguarda thread produtora
    pthread_join(producer_thread, NULL);

    // Aguarda threads decodificadoras
    for (int i = 0; i < num_threads; i++) {
        pthread_join(decoder_threads[i], NULL);
    }

    int64_t total_time = av_gettime() - total_start_time;
    cpu_stats_read(&cpu_end);
    double cpu_usage = cpu_stats_calculate_usage(&cpu_start, &cpu_end);

    // Imprime resumo
    pthread_mutex_lock(&g_state_mutex);
    size_t total_frames = g_total_frames_processed;
    pthread_mutex_unlock(&g_state_mutex);

    // Modo debug: imprime resumo detalhado com métricas
    if (g_debug_mode) {
        printf("\n========== RESUMO ==========\n");
        printf("Profile: %s | Threads: %d\n", profile.c_str(), num_threads);
        printf("Total frames: %zu\n", total_frames);
        printf("Tempo total: %ld us\n", total_time);
        printf("FPS: %.1f\n", (total_frames * 1e6) / total_time);
        printf("[RESUMO] CPU_USAGE %.1f\n", cpu_usage);

        // Métricas de I/O (leitura)
        int64_t read_total = g_total_read_time.load();
        int read_count = g_read_frame_count.load();
        if (read_count > 0) {
            printf("[RESUMO] READ_TIME_TOTAL %ld\n", read_total);
            printf("[RESUMO] READ_TIME_AVG %ld\n", read_total / read_count);
        }

        // Métricas de decodificação
        int64_t decode_total = g_total_decode_time.load();
        int decode_count = g_decode_frame_count.load();
        if (decode_count > 0) {
            printf("[RESUMO] DECODE_TIME_TOTAL %ld\n", decode_total);
            printf("[RESUMO] DECODE_TIME_AVG %ld\n", decode_total / decode_count);
        }
    } else {
        // Modo normal: usa formato padrão
        stats_print_summary(profile.c_str(), num_threads, 0, total_frames, total_time);
        stats_print_cpu(profile.c_str(), num_threads, 0, cpu_usage);
    }

    // Limpeza final
    // 1. Fecha arquivos
    fclose(g_input_file);
    if (g_enable_write && g_output_file) fclose(g_output_file);

    // 2. Limpeza de sincronização (ANTES de liberar contextos das threads)
    mt_cleanup_state();
    mt_cleanup_write_control();
    queue_destroy(&g_frame_queue);

    // 3. Libera contextos das threads (DEPOIS dos cleanups)
    for (int i = 0; i < num_threads; i++) {
        decode_context_free(&thread_contexts[i].decode_ctx);
    }
    free(decoder_threads);
    free(thread_contexts);

    return 0;
}
