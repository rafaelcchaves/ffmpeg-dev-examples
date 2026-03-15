/**
 * @file test_avbuffer_queue.c
 * @brief Testes unitários para a fila circular thread-safe de AVBufferRef
 *
 * Compilar com:
 *   gcc -O3 -Wall src/avbuffer_queue.c src/test_avbuffer_queue.c \
 *       -o test_queue -lavutil -lpthread
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include "avbuffer_queue.h"

// Contadores de testes
static int tests_passed = 0;
static int tests_failed = 0;

// Macros de teste
#define TEST_START(name) printf("\n[RUNNING] %s\n", name)
#define TEST_PASS() do { tests_passed++; printf("[PASS]\n"); } while(0)
#define TEST_FAIL(msg) do { tests_failed++; printf("[FAIL] %s\n", msg); } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { TEST_FAIL(msg); return; } } while(0)

// ============================================
// Teste 1: Inserção e Remoção Simples
// ============================================
void test_push_pop_simple(void) {
    TEST_START("Push/Pop Simples");

    AVBufferQueue queue;
    ASSERT(avbuffer_queue_init(&queue) == 0, "Falha ao inicializar fila");

    // Cria um buffer de teste
    uint8_t data[] = {1, 2, 3, 4, 5};
    AVBufferRef* buf = av_buffer_alloc(sizeof(data));
    ASSERT(buf != NULL, "Falha ao alocar buffer");
    memcpy(buf->data, data, sizeof(data));

    // Verifica fila vazia inicial
    ASSERT(avbuffer_queue_count(&queue) == 0, "Fila deveria estar vazia");
    ASSERT(avbuffer_queue_is_empty(&queue) == 1, "is_empty deveria retornar 1");

    // Insere buffer - push TRANSFERE ownership, não devemos mais usar buf
    int result = avbuffer_queue_push(&queue, buf);
    ASSERT(result == 0, "Falha ao inserir buffer");
    ASSERT(avbuffer_queue_count(&queue) == 1, "Count deveria ser 1");

    // Remove buffer - pop RETORNA ownership
    AVBufferRef* retrieved = avbuffer_queue_pop(&queue);
    ASSERT(retrieved != NULL, "Falha ao remover buffer");
    ASSERT(avbuffer_queue_count(&queue) == 0, "Fila deveria estar vazia após pop");

    // Verifica dados
    ASSERT(memcmp(retrieved->data, data, sizeof(data)) == 0, "Dados incorretos");

    // Limpeza - apenas retrieved precisa ser liberado (ownership transferido)
    av_buffer_unref(&retrieved);
    avbuffer_queue_destroy(&queue);

    TEST_PASS();
}

// ============================================
// Teste 2: Fila Cheia
// ============================================
void test_queue_full(void) {
    TEST_START("Fila Cheia");

    AVBufferQueue queue;
    ASSERT(avbuffer_queue_init(&queue) == 0, "Falha ao inicializar fila");

    // Enche a fila - push transfere ownership
    for (int i = 0; i < AVBUFFER_QUEUE_CAPACITY; i++) {
        AVBufferRef* buf = av_buffer_alloc(16);
        ASSERT(buf != NULL, "Falha ao alocar buffer");
        int result = avbuffer_queue_push(&queue, buf);
        ASSERT(result == 0, "Falha ao inserir buffer");
        // Não fazemos unref - ownership foi transferido para a fila
    }

    // Verifica fila cheia
    ASSERT(avbuffer_queue_count(&queue) == AVBUFFER_QUEUE_CAPACITY, "Count incorreto");
    ASSERT(avbuffer_queue_is_full(&queue) == 1, "is_full deveria retornar 1");

    // Tenta inserir com try_push (deve falhar)
    AVBufferRef* extra_buf = av_buffer_alloc(16);
    int result = avbuffer_queue_try_push(&queue, extra_buf);
    ASSERT(result == -1, "try_push deveria falhar com fila cheia");

    // Como o push falhou, ainda somos owners do extra_buf
    av_buffer_unref(&extra_buf);
    avbuffer_queue_destroy(&queue);

    TEST_PASS();
}

// ============================================
// Teste 3: Fila Vazia
// ============================================
void test_queue_empty(void) {
    TEST_START("Fila Vazia");

    AVBufferQueue queue;
    ASSERT(avbuffer_queue_init(&queue) == 0, "Falha ao inicializar fila");

    // Verifica estado inicial
    ASSERT(avbuffer_queue_count(&queue) == 0, "Count deveria ser 0");
    ASSERT(avbuffer_queue_is_empty(&queue) == 1, "is_empty deveria retornar 1");
    ASSERT(avbuffer_queue_is_full(&queue) == 0, "is_full deveria retornar 0");

    // Tenta remover com try_pop (deve falhar)
    AVBufferRef* buf = avbuffer_queue_try_pop(&queue);
    ASSERT(buf == NULL, "try_pop deveria retornar NULL com fila vazia");

    avbuffer_queue_destroy(&queue);

    TEST_PASS();
}

// ============================================
// Teste 4: Circularidade (wrap-around)
// ============================================
void test_circularity(void) {
    TEST_START("Circularidade (Wrap-around)");

    AVBufferQueue queue;
    ASSERT(avbuffer_queue_init(&queue) == 0, "Falha ao inicializar fila");

    // Enche a fila - push transfere ownership
    for (int i = 0; i < AVBUFFER_QUEUE_CAPACITY; i++) {
        AVBufferRef* buf = av_buffer_alloc(16);
        buf->data[0] = (uint8_t)i;  // Marca com índice
        avbuffer_queue_push(&queue, buf);
        // Não fazemos unref - ownership foi transferido
    }

    // Remove todos - pop retorna ownership
    for (int i = 0; i < AVBUFFER_QUEUE_CAPACITY; i++) {
        AVBufferRef* buf = avbuffer_queue_pop(&queue);
        ASSERT(buf != NULL, "Falha ao remover buffer");
        ASSERT(buf->data[0] == (uint8_t)i, "Ordem incorreta");
        av_buffer_unref(&buf);  // Agora somos owners, liberamos
    }

    // Enche novamente (testa wrap-around)
    for (int i = 0; i < AVBUFFER_QUEUE_CAPACITY; i++) {
        AVBufferRef* buf = av_buffer_alloc(16);
        buf->data[0] = (uint8_t)(i + 100);  // Marca com índice diferente
        avbuffer_queue_push(&queue, buf);
    }

    // Remove novamente e verifica ordem
    for (int i = 0; i < AVBUFFER_QUEUE_CAPACITY; i++) {
        AVBufferRef* buf = avbuffer_queue_pop(&queue);
        ASSERT(buf != NULL, "Falha ao remover buffer no segundo ciclo");
        ASSERT(buf->data[0] == (uint8_t)(i + 100), "Ordem incorreta no segundo ciclo");
        av_buffer_unref(&buf);
    }

    avbuffer_queue_destroy(&queue);

    TEST_PASS();
}

// ============================================
// Teste 5: Multi-thread Produtor-Consumidor
// ============================================

#define PRODUCER_COUNT 3
#define CONSUMER_COUNT 3
#define ITEMS_PER_PRODUCER 100

typedef struct {
    AVBufferQueue* queue;
    int producer_id;
    int items_produced;
} ProducerArgs;

typedef struct {
    AVBufferQueue* queue;
    int consumer_id;
    int items_consumed;
} ConsumerArgs;

static volatile int producers_done = 0;
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

void* producer_thread(void* arg) {
    ProducerArgs* args = (ProducerArgs*)arg;

    for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
        AVBufferRef* buf = av_buffer_alloc(16);
        if (buf) {
            // Marca o buffer com ID do produtor e sequência
            buf->data[0] = (uint8_t)args->producer_id;
            buf->data[1] = (uint8_t)(i % 256);

            if (avbuffer_queue_push(args->queue, buf) == 0) {
                args->items_produced++;
                // Push transfere ownership - não fazemos unref
            } else {
                // Falha no push, ainda somos owners
                av_buffer_unref(&buf);
            }
        }
        // Pequeno delay para simular trabalho real
        usleep(100);
    }

    pthread_mutex_lock(&stats_mutex);
    producers_done++;
    pthread_mutex_unlock(&stats_mutex);

    return NULL;
}

void* consumer_thread(void* arg) {
    ConsumerArgs* args = (ConsumerArgs*)arg;

    while (1) {
        // Verifica se todos produtores terminaram e fila está vazia
        pthread_mutex_lock(&stats_mutex);
        int done = producers_done;
        pthread_mutex_unlock(&stats_mutex);

        AVBufferRef* buf = avbuffer_queue_try_pop(args->queue);
        if (buf) {
            args->items_consumed++;
            av_buffer_unref(&buf);
        } else if (done >= PRODUCER_COUNT) {
            // Produtores terminaram e fila vazia
            break;
        } else {
            // Fila vazia mas produtores ainda ativos, espera um pouco
            usleep(1000);
        }
    }

    return NULL;
}

void test_multithread_producer_consumer(void) {
    TEST_START("Multi-thread Produtor-Consumidor");

    AVBufferQueue queue;
    ASSERT(avbuffer_queue_init(&queue) == 0, "Falha ao inicializar fila");

    pthread_t producers[PRODUCER_COUNT];
    pthread_t consumers[CONSUMER_COUNT];
    ProducerArgs prod_args[PRODUCER_COUNT];
    ConsumerArgs cons_args[CONSUMER_COUNT];

    producers_done = 0;

    // Inicia consumidores primeiro
    for (int i = 0; i < CONSUMER_COUNT; i++) {
        cons_args[i].queue = &queue;
        cons_args[i].consumer_id = i;
        cons_args[i].items_consumed = 0;
        pthread_create(&consumers[i], NULL, consumer_thread, &cons_args[i]);
    }

    // Inicia produtores
    for (int i = 0; i < PRODUCER_COUNT; i++) {
        prod_args[i].queue = &queue;
        prod_args[i].producer_id = i;
        prod_args[i].items_produced = 0;
        pthread_create(&producers[i], NULL, producer_thread, &prod_args[i]);
    }

    // Aguarda produtores
    for (int i = 0; i < PRODUCER_COUNT; i++) {
        pthread_join(producers[i], NULL);
    }

    // Aguarda consumidores
    for (int i = 0; i < CONSUMER_COUNT; i++) {
        pthread_join(consumers[i], NULL);
    }

    // Verifica totais
    int total_produced = 0;
    int total_consumed = 0;

    for (int i = 0; i < PRODUCER_COUNT; i++) {
        total_produced += prod_args[i].items_produced;
    }
    for (int i = 0; i < CONSUMER_COUNT; i++) {
        total_consumed += cons_args[i].items_consumed;
    }

    printf("  Total produzido: %d\n", total_produced);
    printf("  Total consumido: %d\n", total_consumed);

    ASSERT(total_produced == PRODUCER_COUNT * ITEMS_PER_PRODUCER,
           "Nem todos os itens foram produzidos");
    ASSERT(total_consumed == total_produced,
           "Total consumido diferente do produzido");
    ASSERT(avbuffer_queue_is_empty(&queue), "Fila deveria estar vazia no final");

    avbuffer_queue_destroy(&queue);

    TEST_PASS();
}

// ============================================
// Teste 6: Ownership Transfer (sem verificação direta de refcount)
// ============================================
void test_reference_counting(void) {
    TEST_START("Ownership Transfer");

    AVBufferQueue queue;
    ASSERT(avbuffer_queue_init(&queue) == 0, "Falha ao inicializar fila");

    // Cria buffer
    AVBufferRef* buf = av_buffer_alloc(16);
    ASSERT(buf != NULL, "Falha ao alocar buffer");
    memcpy(buf->data, "testdata", 9);

    // Insere na fila - push TRANSFERE ownership
    ASSERT(avbuffer_queue_push(&queue, buf) == 0, "Falha ao inserir buffer");
    // Não fazemos unref - ownership foi transferido para a fila

    // Remove da fila - pop RETORNA ownership
    AVBufferRef* retrieved = avbuffer_queue_pop(&queue);
    ASSERT(retrieved != NULL, "Falha ao remover buffer");

    // Verifica que os dados ainda estão intactos
    ASSERT(memcmp(retrieved->data, "testdata", 9) == 0, "Dados do buffer corrompidos");

    // Limpeza - agora somos owners de retrieved
    av_buffer_unref(&retrieved);
    avbuffer_queue_destroy(&queue);

    TEST_PASS();
}

// ============================================
// Teste 7: Operações try_push e try_pop
// ============================================
void test_try_operations(void) {
    TEST_START("Operações try_push e try_pop");

    AVBufferQueue queue;
    ASSERT(avbuffer_queue_init(&queue) == 0, "Falha ao inicializar fila");

    // Teste try_pop em fila vazia
    AVBufferRef* buf = avbuffer_queue_try_pop(&queue);
    ASSERT(buf == NULL, "try_pop em fila vazia deveria retornar NULL");

    // Insere alguns buffers com try_push - transfere ownership
    for (int i = 0; i < 5; i++) {
        AVBufferRef* b = av_buffer_alloc(16);
        int result = avbuffer_queue_try_push(&queue, b);
        ASSERT(result == 0, "try_push deveria succeed");
        // Não fazemos unref - ownership transferido
    }

    ASSERT(avbuffer_queue_count(&queue) == 5, "Count deveria ser 5");

    // Remove com try_pop - retorna ownership
    for (int i = 0; i < 5; i++) {
        AVBufferRef* b = avbuffer_queue_try_pop(&queue);
        ASSERT(b != NULL, "try_pop deveria retornar buffer");
        av_buffer_unref(&b);  // Agora somos owners
    }

    ASSERT(avbuffer_queue_is_empty(&queue), "Fila deveria estar vazia");

    avbuffer_queue_destroy(&queue);

    TEST_PASS();
}

// ============================================
// Teste 8: Função clear
// ============================================
void test_clear(void) {
    TEST_START("Função clear");

    AVBufferQueue queue;
    ASSERT(avbuffer_queue_init(&queue) == 0, "Falha ao inicializar fila");

    // Insere vários buffers - push transfere ownership
    for (int i = 0; i < 10; i++) {
        AVBufferRef* buf = av_buffer_alloc(16);
        avbuffer_queue_push(&queue, buf);
        // Não fazemos unref - ownership transferido
    }

    ASSERT(avbuffer_queue_count(&queue) == 10, "Count deveria ser 10");

    // Limpa a fila - deve liberar todos os buffers
    avbuffer_queue_clear(&queue);

    ASSERT(avbuffer_queue_count(&queue) == 0, "Count deveria ser 0 após clear");
    ASSERT(avbuffer_queue_is_empty(&queue), "Fila deveria estar vazia após clear");

    // Verifica se pode inserir novamente
    AVBufferRef* buf = av_buffer_alloc(16);
    int result = avbuffer_queue_push(&queue, buf);
    ASSERT(result == 0, "Deveria conseguir inserir após clear");
    // Não fazemos unref - ownership transferido

    avbuffer_queue_destroy(&queue);

    TEST_PASS();
}

// ============================================
// Teste 9: Stress Test BRUTAL
// ============================================

#define BRUTAL_PRODUCER_COUNT 5
#define BRUTAL_CONSUMER_COUNT 5
#define BRUTAL_ITEMS_PER_PRODUCER 1000
#define BRUTAL_BUFFER_SIZE 1920 * 1080 * 3  // Full HD frame RGB

typedef struct {
    AVBufferQueue* queue;
    int producer_id;
    long items_produced;
    long bytes_produced;
} BrutalProducerArgs;

typedef struct {
    AVBufferQueue* queue;
    int consumer_id;
    long items_consumed;
    long bytes_consumed;
} BrutalConsumerArgs;

static volatile int brutal_producers_done = 0;
static volatile long brutal_total_in_queue = 0;

void* brutal_producer_thread(void* arg) {
    BrutalProducerArgs* args = (BrutalProducerArgs*)arg;

    for (int i = 0; i < BRUTAL_ITEMS_PER_PRODUCER; i++) {
        // Aloca buffer grande (simula frame de vídeo)
        AVBufferRef* buf = av_buffer_alloc(BRUTAL_BUFFER_SIZE);
        if (buf) {
            // Preenche com dados (simula processamento)
            memset(buf->data, (uint8_t)(args->producer_id + i), BRUTAL_BUFFER_SIZE);

            if (avbuffer_queue_push(args->queue, buf) == 0) {
                args->items_produced++;
                args->bytes_produced += BRUTAL_BUFFER_SIZE;

                __sync_fetch_and_add(&brutal_total_in_queue, 1);
            } else {
                av_buffer_unref(&buf);
            }
        }
        // Sem delay - máximo de stress!
    }

    __sync_fetch_and_add(&brutal_producers_done, 1);
    return NULL;
}

void* brutal_consumer_thread(void* arg) {
    BrutalConsumerArgs* args = (BrutalConsumerArgs*)arg;

    while (1) {
        int done = __sync_fetch_and_add(&brutal_producers_done, 0);
        long in_queue = __sync_fetch_and_add(&brutal_total_in_queue, 0);

        AVBufferRef* buf = avbuffer_queue_try_pop(args->queue);
        if (buf) {
            args->items_consumed++;
            args->bytes_consumed += buf->size;

            __sync_fetch_and_sub(&brutal_total_in_queue, 1);

            // Simula processamento do frame
            volatile uint8_t checksum = 0;
            for (int j = 0; j < 100; j++) {
                checksum ^= buf->data[j];
            }
            (void)checksum;

            av_buffer_unref(&buf);
        } else if (done >= BRUTAL_PRODUCER_COUNT && in_queue == 0) {
            break;
        }
        // Sem delay - máximo de stress!
    }

    return NULL;
}

void test_brutal_stress(void) {
    TEST_START("STRESS TEST BRUTAL (5x5 threads, 5000 frames Full HD)");

    AVBufferQueue queue;
    ASSERT(avbuffer_queue_init(&queue) == 0, "Falha ao inicializar fila");

    pthread_t producers[BRUTAL_PRODUCER_COUNT];
    pthread_t consumers[BRUTAL_CONSUMER_COUNT];
    BrutalProducerArgs prod_args[BRUTAL_PRODUCER_COUNT];
    BrutalConsumerArgs cons_args[BRUTAL_CONSUMER_COUNT];

    brutal_producers_done = 0;
    brutal_total_in_queue = 0;

    printf("  Buffer size: %d bytes (%.2f MB)\n",
           BRUTAL_BUFFER_SIZE, BRUTAL_BUFFER_SIZE / (1024.0 * 1024.0));
    printf("  Total de frames: %d\n", BRUTAL_PRODUCER_COUNT * BRUTAL_ITEMS_PER_PRODUCER);
    printf("  Memória total: %.2f GB\n",
           (double)BRUTAL_PRODUCER_COUNT * BRUTAL_ITEMS_PER_PRODUCER * BRUTAL_BUFFER_SIZE / (1024.0 * 1024.0 * 1024.0));

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Inicia consumidores primeiro
    for (int i = 0; i < BRUTAL_CONSUMER_COUNT; i++) {
        cons_args[i].queue = &queue;
        cons_args[i].consumer_id = i;
        cons_args[i].items_consumed = 0;
        cons_args[i].bytes_consumed = 0;
        pthread_create(&consumers[i], NULL, brutal_consumer_thread, &cons_args[i]);
    }

    // Inicia produtores
    for (int i = 0; i < BRUTAL_PRODUCER_COUNT; i++) {
        prod_args[i].queue = &queue;
        prod_args[i].producer_id = i;
        prod_args[i].items_produced = 0;
        prod_args[i].bytes_produced = 0;
        pthread_create(&producers[i], NULL, brutal_producer_thread, &prod_args[i]);
    }

    // Aguarda produtores
    for (int i = 0; i < BRUTAL_PRODUCER_COUNT; i++) {
        pthread_join(producers[i], NULL);
    }

    // Aguarda consumidores
    for (int i = 0; i < BRUTAL_CONSUMER_COUNT; i++) {
        pthread_join(consumers[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    // Calcula estatísticas
    long total_produced = 0;
    long total_consumed = 0;
    long bytes_produced = 0;
    long bytes_consumed = 0;

    for (int i = 0; i < BRUTAL_PRODUCER_COUNT; i++) {
        total_produced += prod_args[i].items_produced;
        bytes_produced += prod_args[i].bytes_produced;
    }
    for (int i = 0; i < BRUTAL_CONSUMER_COUNT; i++) {
        total_consumed += cons_args[i].items_consumed;
        bytes_consumed += cons_args[i].bytes_consumed;
    }

    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double throughput_items = total_consumed / elapsed;
    double throughput_bytes = bytes_consumed / elapsed / (1024.0 * 1024.0);  // MB/s

    printf("\n  === RESULTADOS ===\n");
    printf("  Tempo total: %.3f segundos\n", elapsed);
    printf("  Frames produzidos: %ld\n", total_produced);
    printf("  Frames consumidos: %ld\n", total_consumed);
    printf("  Bytes produzidos: %.2f MB\n", bytes_produced / (1024.0 * 1024.0));
    printf("  Bytes consumidos: %.2f MB\n", bytes_consumed / (1024.0 * 1024.0));
    printf("  Throughput: %.0f frames/segundo\n", throughput_items);
    printf("  Throughput: %.2f MB/segundo\n", throughput_bytes);
    printf("  Fila vazia no final: %s\n", avbuffer_queue_is_empty(&queue) ? "SIM" : "NÃO");

    ASSERT(total_produced == BRUTAL_PRODUCER_COUNT * BRUTAL_ITEMS_PER_PRODUCER,
           "Nem todos os itens foram produzidos");
    ASSERT(total_consumed == total_produced,
           "Total consumido diferente do produzido");
    ASSERT(avbuffer_queue_is_empty(&queue), "Fila deveria estar vazia no final");

    avbuffer_queue_destroy(&queue);

    TEST_PASS();
}

// ============================================
// Função principal
// ============================================
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("============================================\n");
    printf("  Testes da Fila Circular AVBuffer\n");
    printf("============================================\n");

    // Executa todos os testes
    test_push_pop_simple();
    test_queue_full();
    test_queue_empty();
    test_circularity();
    test_multithread_producer_consumer();
    test_reference_counting();
    test_try_operations();
    test_clear();
    test_brutal_stress();

    // Resumo
    printf("\n============================================\n");
    printf("  RESUMO\n");
    printf("============================================\n");
    printf("  Testes passaram: %d\n", tests_passed);
    printf("  Testes falharam: %d\n", tests_failed);
    printf("============================================\n");

    return tests_failed > 0 ? 1 : 0;
}
