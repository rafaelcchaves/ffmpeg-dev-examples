// ============================================================================
// decode_lz4_mt.cpp - Decodificador LZ4 Multithread
// Arquitetura: Buffer Compartilhado com Semáforo
// ============================================================================

#include "decode_lz4.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <atomic>

// ============================================================================
// Definição das variáveis globais
// ============================================================================

// Buffer compartilhado (produtor <-> consumidores)
char *g_shared_buffer = NULL;
SharedFrameInfo g_shared_frame_info;
int g_shared_buffer_size = 0;

// Semáforos para sincronização do buffer compartilhado
sem_t g_sem_empty;  // Sinaliza que buffer está vazio (produtor pode escrever)
sem_t g_sem_full;   // Sinaliza que buffer tem dados (consumidor pode ler)

// Mutex para proteger acesso às info do frame compartilhado
pthread_mutex_t g_shared_info_mutex = PTHREAD_MUTEX_INITIALIZER;

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

// Métricas acumuladas (atómicas para thread-safety)
std::atomic<int64_t> g_total_read_time{0};
std::atomic<int64_t> g_total_decode_time{0};
std::atomic<int> g_read_frame_count{0};
std::atomic<int> g_decode_frame_count{0};

// ============================================================================
// Funções de Inicialização
// ============================================================================

int mt_initialize_shared_buffer(int buffer_size) {
    g_shared_buffer = (char *)malloc(buffer_size);
    if (!g_shared_buffer) {
        fprintf(stderr, "Erro: Nao foi possivel alocar buffer compartilhado de %d bytes\n", buffer_size);
        return -1;
    }
    g_shared_buffer_size = buffer_size;
    return 0;
}

int mt_initialize_synchronization() {
    // Semáforo "empty" inicializado com 1 (buffer vazio, produtor pode escrever)
    if (sem_init(&g_sem_empty, 0, 1) < 0) {
        perror("Erro ao inicializar semaforo empty");
        return -1;
    }

    // Semáforo "full" inicializado com 0 (buffer sem dados, consumidores aguardam)
    if (sem_init(&g_sem_full, 0, 0) < 0) {
        perror("Erro ao inicializar semaforo full");
        sem_destroy(&g_sem_empty);
        return -1;
    }

    return 0;
}

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

void mt_cleanup_shared_buffer() {
    if (g_shared_buffer) {
        free(g_shared_buffer);
        g_shared_buffer = NULL;
    }
    g_shared_buffer_size = 0;
}

void mt_cleanup_synchronization() {
    sem_destroy(&g_sem_empty);
    sem_destroy(&g_sem_full);
    pthread_mutex_destroy(&g_shared_info_mutex);
}

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

    char *producer_buffer = NULL;
    int max_frame_size = 0;
    int sequence_number = 0;
    int first_frame = 1;

    // Aloca buffer privativo do produtor
    max_frame_size = LZ4_compressBound(1920 * 1080 * 3);  // Estimativa conservadora
    producer_buffer = (char *)malloc(max_frame_size);
    if (!producer_buffer) {
        fprintf(stderr, "[Producer] Erro: Nao foi possivel alocar buffer privativo\n");
        pthread_mutex_lock(&g_state_mutex);
        g_producer_finished = true;
        pthread_mutex_unlock(&g_state_mutex);
        // Acorda consumidores para encerrar
        for (int i = 0; i < g_num_decoder_threads; i++) {
            sem_post(&g_sem_full);
        }
        return NULL;
    }

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

        // 2. Verificar se precisa realocar buffer
        if (header.size_compress > max_frame_size) {
            free(producer_buffer);
            max_frame_size = header.size_compress + 1024;
            producer_buffer = (char *)malloc(max_frame_size);
            if (!producer_buffer) {
                fprintf(stderr, "[Producer] Erro: Nao foi possivel realocar buffer\n");
                break;
            }
        }

        // 3. Ler dados compactados para buffer privativo
        ret = fread(producer_buffer, 1, header.size_compress, g_input_file);
        if (ret != header.size_compress) {
            fprintf(stderr, "[Producer] Erro: Arquivo terminou antes do esperado\n");
            break;
        }

        int64_t read_time = av_gettime() - read_start;

        // 3a. Acumular métricas de leitura
        g_total_read_time += read_time;
        g_read_frame_count++;

        // 3b. Imprimir métrica de leitura se debug mode
        if (g_debug_mode) {
            printf("[METRICA] READ_FRAME %d %ld\n", sequence_number, read_time);
        }

        // 4. Aguarda buffer compartilhado estar vazio
        sem_wait(&g_sem_empty);

        // 5. Copia do buffer privativo para buffer compartilhado
        memcpy(g_shared_buffer, producer_buffer, header.size_compress);

        // 6. Atualiza info do frame compartilhado
        pthread_mutex_lock(&g_shared_info_mutex);
        g_shared_frame_info.sequence_number = sequence_number;
        g_shared_frame_info.header = header;
        g_shared_frame_info.data_size = header.size_compress;
        pthread_mutex_unlock(&g_shared_info_mutex);

        // 7. Sinaliza que dados estão disponíveis
        sem_post(&g_sem_full);

        sequence_number++;
    }

    // Marca que produtor terminou
    pthread_mutex_lock(&g_state_mutex);
    g_producer_finished = true;
    g_total_frames_processed = sequence_number;
    pthread_mutex_unlock(&g_state_mutex);

    // Modo de finalização: aguarda cada thread liberar o buffer e envia sinal de EOF
    int shutdown_signals_sent = 0;
    while (shutdown_signals_sent < g_num_decoder_threads) {
        sem_wait(&g_sem_empty);  // Aguarda alguma thread liberar o buffer

        pthread_mutex_lock(&g_shared_info_mutex);
        g_shared_frame_info.sequence_number = -1;  // Marcador de encerramento
        g_shared_frame_info.data_size = 0;         // Tamanho zero = sinal de EOF
        pthread_mutex_unlock(&g_shared_info_mutex);

        sem_post(&g_sem_full);  // Envia sinal para a thread

        shutdown_signals_sent++;
    }

    // Aguarda todas as threads decodificadoras terminarem ANTES de liberar recursos
    pthread_mutex_lock(&g_active_threads_mutex);
    while (g_active_threads_count > 0) {
        pthread_cond_wait(&g_active_threads_cond, &g_active_threads_mutex);
    }
    pthread_mutex_unlock(&g_active_threads_mutex);

    // Limpeza
    free(producer_buffer);

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
        SharedFrameInfo local_frame_info;

        // 1. Aguarda dados disponíveis no buffer compartilhado
        sem_wait(&g_sem_full);

        // 2. Copia info do frame (seção crítica pequena)
        pthread_mutex_lock(&g_shared_info_mutex);
        local_frame_info = g_shared_frame_info;
        pthread_mutex_unlock(&g_shared_info_mutex);

        // 3. Verifica se é sinal de encerramento (tamanho zero ou sequence -1)
        if (local_frame_info.data_size == 0 || local_frame_info.sequence_number == -1) {
            sem_post(&g_sem_empty);  // Libera o buffer para o próximo sinal
            break;
        }

        // 4. Copia do buffer compartilhado para buffer privativo
        memcpy(ctx->compressed_buffer, g_shared_buffer, local_frame_info.data_size);

        // 5. Libera buffer compartilhado para o produtor
        sem_post(&g_sem_empty);

        // 6. Decodificação (processamento paralelo, sem locks)
        int64_t decode_start = av_gettime();

        int decompressed_size = LZ4_decompress_safe(
            ctx->compressed_buffer,
            (char *)ctx->decode_ctx.image_data[0],
            local_frame_info.data_size,
            ctx->max_compressed_size
        );

        if (decompressed_size < 0) {
            fprintf(stderr, "[Decoder %d] Erro na decodificacao do frame %d\n",
                    thread_id, local_frame_info.sequence_number);
            continue;
        }

        int64_t decode_time = av_gettime() - decode_start;

        // 6a. Acumular métricas de decodificação
        g_total_decode_time += decode_time;
        g_decode_frame_count++;

        // 6b. Imprimir métrica de decodificação se debug mode
        if (g_debug_mode) {
            printf("[METRICA] DECODE_FRAME %d %ld\n", local_frame_info.sequence_number, decode_time);
        }

        // 7. Aguarda sua vez de escrever (escrita sequencial)
        pthread_mutex_lock(&g_write_mutex);

        while ((int)g_next_to_write != local_frame_info.sequence_number) {
            pthread_cond_wait(&g_write_cond, &g_write_mutex);
        }

        // 8. Escreve frame no arquivo
        // COMENTADO PARA BENCHMARK SEM I/O
        /*
        size_t written = fwrite(ctx->decode_ctx.image_data[0], 1,
                                local_frame_info.header.size_decompress, g_output_file);
        if (written != (size_t)local_frame_info.header.size_decompress) {
            fprintf(stderr, "[Decoder %d] Erro ao escrever frame %d\n",
                    thread_id, local_frame_info.sequence_number);
        }
        */
        (void)thread_id;  // Avoid unused warning

        // 9. Atualiza contador e acorda outras threads
        g_next_to_write++;
        pthread_mutex_unlock(&g_write_mutex);
        pthread_cond_broadcast(&g_write_cond);

        // 10. Imprime estatísticas (apenas se não estiver em modo debug)
        if (!g_debug_mode) {
            stats_print_frame(g_profile_name.c_str(), g_num_decoder_threads, decode_time);
        }
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
                   const std::string &output_file, const std::string &profile) {
    pthread_t producer_thread;
    pthread_t *decoder_threads = NULL;
    ThreadContext *thread_contexts = NULL;
    FrameHeader first_header;
    int ret;
    int64_t total_start_time;

    g_num_decoder_threads = num_threads;
    g_profile_name = profile;

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

    g_output_file = fopen(output_file.c_str(), "wb");
    if (!g_output_file) {
        fprintf(stderr, "Erro: Nao foi possivel abrir arquivo de saida: %s\n", output_file.c_str());
        fclose(g_input_file);
        return -1;
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

    // Inicializa buffer compartilhado
    int shared_buffer_size = LZ4_compressBound(first_header.size_decompress);
    if (mt_initialize_shared_buffer(shared_buffer_size) < 0) {
        fclose(g_input_file);
        fclose(g_output_file);
        return -1;
    }

    // Inicializa sincronização
    if (mt_initialize_synchronization() < 0) {
        mt_cleanup_shared_buffer();
        fclose(g_input_file);
        fclose(g_output_file);
        return -1;
    }

    // Inicializa controle de escrita
    if (mt_initialize_write_control(g_output_file) < 0) {
        mt_cleanup_synchronization();
        mt_cleanup_shared_buffer();
        fclose(g_input_file);
        fclose(g_output_file);
        return -1;
    }

    // Inicializa estado
    if (mt_initialize_state(g_input_file) < 0) {
        mt_cleanup_write_control();
        mt_cleanup_synchronization();
        mt_cleanup_shared_buffer();
        fclose(g_input_file);
        fclose(g_output_file);
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
        mt_cleanup_synchronization();
        mt_cleanup_shared_buffer();
        fclose(g_input_file);
        fclose(g_output_file);
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
            mt_cleanup_synchronization();
            mt_cleanup_shared_buffer();
            fclose(g_input_file);
            fclose(g_output_file);
            return -1;
        }

        // Aloca buffer comprimido privativo da thread
        thread_contexts[i].max_compressed_size = LZ4_compressBound(first_header.size_decompress);
        thread_contexts[i].compressed_buffer = (char *)malloc(thread_contexts[i].max_compressed_size);
        if (!thread_contexts[i].compressed_buffer) {
            fprintf(stderr, "Erro: Nao foi possivel alocar buffer comprimido para thread %d\n", i);
            // Cleanup parcial
            for (int j = 0; j < i; j++) {
                free(thread_contexts[j].compressed_buffer);
                decode_context_free(&thread_contexts[j].decode_ctx);
            }
            decode_context_free(&thread_contexts[i].decode_ctx);
            free(decoder_threads);
            free(thread_contexts);
            mt_cleanup_state();
            mt_cleanup_write_control();
            mt_cleanup_synchronization();
            mt_cleanup_shared_buffer();
            fclose(g_input_file);
            fclose(g_output_file);
            return -1;
        }
    }

    total_start_time = av_gettime();

    // Cria thread produtora
    if (pthread_create(&producer_thread, NULL, mt_producer_thread, NULL) != 0) {
        perror("Erro ao criar thread produtora");
        // Cleanup
        for (int i = 0; i < num_threads; i++) {
            free(thread_contexts[i].compressed_buffer);
            decode_context_free(&thread_contexts[i].decode_ctx);
        }
        free(decoder_threads);
        free(thread_contexts);
        mt_cleanup_state();
        mt_cleanup_write_control();
        mt_cleanup_synchronization();
        mt_cleanup_shared_buffer();
        fclose(g_input_file);
        fclose(g_output_file);
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
        stats_print_summary(profile.c_str(), num_threads, total_frames, total_time);
    }

    // Limpeza final
    // 1. Fecha arquivos
    fclose(g_input_file);
    fclose(g_output_file);

    // 2. Limpeza de sincronização (ANTES de liberar contextos das threads)
    mt_cleanup_state();
    mt_cleanup_write_control();
    mt_cleanup_synchronization();
    mt_cleanup_shared_buffer();

    // 3. Libera contextos das threads (DEPOIS dos cleanups)
    for (int i = 0; i < num_threads; i++) {
        free(thread_contexts[i].compressed_buffer);
        decode_context_free(&thread_contexts[i].decode_ctx);
    }
    free(decoder_threads);
    free(thread_contexts);

    return 0;
}
