/**
 * @file avbuffer_queue.h
 * @brief Filas thread-safe para AVBufferRef do FFmpeg
 *
 * Implementa duas estruturas:
 * 1. Fila circular com capacidade fixa (AVBufferQueue) - usando semáforos POSIX
 * 2. Fila unificada com lista encadeada (FrameQueue) - buffer + metadata juntos
 */

#ifndef AVBUFFER_QUEUE_H
#define AVBUFFER_QUEUE_H

#include <libavutil/buffer.h>
#include <semaphore.h>
#include <stdint.h>
#include <pthread.h>

#define AVBUFFER_QUEUE_CAPACITY 30

/**
 * @brief Estrutura da fila circular de buffers
 */
typedef struct {
    AVBufferRef* buffers[AVBUFFER_QUEUE_CAPACITY];  /**< Array circular de buffers */
    int head;                                        /**< Índice de inserção */
    int tail;                                        /**< Índice de remoção */
    int count;                                       /**< Número de elementos na fila */

    sem_t sem_empty;  /**< Conta slots vazios (inicia = CAPACITY) */
    sem_t sem_full;   /**< Conta slots ocupados (inicia = 0) */
    sem_t mutex;      /**< Exclusão mútua para head/tail/count */
} AVBufferQueue;

/**
 * @brief Inicializa a fila de buffers
 *
 * @param queue Ponteiro para a estrutura da fila
 * @return 0 em sucesso, valor negativo em caso de erro
 */
int avbuffer_queue_init(AVBufferQueue* queue);

/**
 * @brief Destrói a fila e libera todos os buffers
 *
 * Libera todos os buffers restantes na fila e destrói os semáforos.
 *
 * @param queue Ponteiro para a estrutura da fila
 */
void avbuffer_queue_destroy(AVBufferQueue* queue);

/**
 * @brief Insere um buffer na fila (bloqueante)
 *
 * Bloqueia até que haja espaço disponível na fila.
 * TRANSFERE OWNERSHIP: o chamador não deve mais usar o buffer após o push.
 *
 * @param queue Ponteiro para a estrutura da fila
 * @param buffer Buffer a ser inserido (ownership transferido para a fila)
 * @return 0 em sucesso, valor negativo em caso de erro
 */
int avbuffer_queue_push(AVBufferQueue* queue, AVBufferRef* buffer);

/**
 * @brief Tenta inserir um buffer na fila (não-bloqueante)
 *
 * Retorna imediatamente se a fila estiver cheia.
 * TRANSFERE OWNERSHIP em sucesso: o chamador não deve mais usar o buffer.
 * Em falha, o chamador mantém ownership e deve liberar o buffer.
 *
 * @param queue Ponteiro para a estrutura da fila
 * @param buffer Buffer a ser inserido
 * @return 0 em sucesso (ownership transferido), -1 se fila cheia, outro negativo em erro
 */
int avbuffer_queue_try_push(AVBufferQueue* queue, AVBufferRef* buffer);

/**
 * @brief Remove um buffer da fila (bloqueante)
 *
 * Bloqueia até que haja um elemento disponível.
 * RETORNA OWNERSHIP: o chamador se torna dono do buffer e deve liberá-lo.
 *
 * @param queue Ponteiro para a estrutura da fila
 * @return Ponteiro para o buffer removido (ownership transferido), ou NULL em erro
 */
AVBufferRef* avbuffer_queue_pop(AVBufferQueue* queue);

/**
 * @brief Tenta remover um buffer da fila (não-bloqueante)
 *
 * Retorna imediatamente se a fila estiver vazia.
 * RETORNA OWNERSHIP em sucesso: o chamador deve liberar o buffer com av_buffer_unref.
 *
 * @param queue Ponteiro para a estrutura da fila
 * @return Ponteiro para o buffer (ownership transferido), ou NULL se fila vazia/erro
 */
AVBufferRef* avbuffer_queue_try_pop(AVBufferQueue* queue);

/**
 * @brief Retorna o número de elementos na fila
 *
 * @param queue Ponteiro para a estrutura da fila
 * @return Número de elementos
 */
int avbuffer_queue_count(AVBufferQueue* queue);

/**
 * @brief Verifica se a fila está vazia
 *
 * @param queue Ponteiro para a estrutura da fila
 * @return 1 se vazia, 0 caso contrário
 */
int avbuffer_queue_is_empty(AVBufferQueue* queue);

/**
 * @brief Verifica se a fila está cheia
 *
 * @param queue Ponteiro para a estrutura da fila
 * @return 1 se cheia, 0 caso contrário
 */
int avbuffer_queue_is_full(AVBufferQueue* queue);

/**
 * @brief Limpa a fila, liberando todos os buffers
 *
 * @param queue Ponteiro para a estrutura da fila
 */
void avbuffer_queue_clear(AVBufferQueue* queue);

// ============================================================================
// FrameQueue - Fila Unificada com Lista Encadeada
// ============================================================================

/**
 * @brief Nó da lista encadeada contendo buffer + metadata
 *
 * Cada nó representa um frame completo com seus dados e metadados.
 */
typedef struct FrameQueueNode {
    AVBufferRef* buffer;          /**< Buffer com dados do frame (ownership transferido) */
    int sequence_number;          /**< Número sequencial do frame (para escrita ordenada) */
    int32_t size_decompress;      /**< Tamanho original dos dados */
    int32_t width;                /**< Largura do frame */
    int32_t height;               /**< Altura do frame */
    int32_t pix_fmt;              /**< Pixel format (AVPixelFormat) */
    int64_t pkt_dts;              /**< DTS original do frame (frame->pkt_dts) */
    struct FrameQueueNode* next;  /**< Próximo nó na lista */
} FrameQueueNode;

/**
 * @brief Item retornado ao consumidor (sem ponteiro next)
 *
 * Estrutura de retorno que contém todos os dados do frame,
 * mas sem o ponteiro next (que é interno à fila).
 */
typedef struct {
    AVBufferRef* buffer;          /**< Buffer com dados do frame (ownership transferido) */
    int sequence_number;          /**< Número sequencial do frame */
    int32_t size_decompress;      /**< Tamanho original dos dados */
    int32_t width;                /**< Largura do frame */
    int32_t height;               /**< Altura do frame */
    int32_t pix_fmt;              /**< Pixel format (AVPixelFormat) */
    int64_t pkt_dts;              /**< DTS original do frame (frame->pkt_dts) */
} FrameQueueItem;

/**
 * @brief Fila thread-safe com lista encadeada
 *
 * Fila unificada que armazena buffer e metadata juntos,
 * eliminando a necessidade de filas paralelas.
 */
typedef struct {
    FrameQueueNode* head;         /**< Primeiro nó (para remoção) */
    FrameQueueNode* tail;         /**< Último nó (para inserção) */
    int count;                    /**< Número de elementos */
    pthread_mutex_t mutex;        /**< Exclusão mútua */
    pthread_cond_t not_empty;     /**< Sinaliza quando há itens */
} FrameQueue;

// ============================================================================
// Funções da FrameQueue
// ============================================================================

/**
 * @brief Inicializa a fila de frames
 *
 * @param queue Ponteiro para a estrutura da fila
 * @return 0 em sucesso, valor negativo em caso de erro
 */
int frame_queue_init(FrameQueue* queue);

/**
 * @brief Destrói a fila e libera todos os nós e buffers
 *
 * Libera todos os buffers restantes na fila, todos os nós
 * e destrói os primitivos de sincronização.
 *
 * @param queue Ponteiro para a estrutura da fila
 */
void frame_queue_destroy(FrameQueue* queue);

/**
 * @brief Insere um frame na fila (bloqueante)
 *
 * Aloca um novo nó internamente e insere no final da fila.
 * TRANSFERE OWNERSHIP: o chamador não deve mais usar o buffer após o push.
 *
 * @param queue Ponteiro para a estrutura da fila
 * @param buffer Buffer a ser inserido (ownership transferido para a fila)
 * @param seq_num Número sequencial do frame
 * @param size Tamanho dos dados descomprimidos
 * @param w Largura do frame
 * @param h Altura do frame
 * @param fmt Pixel format
 * @param dts DTS original do frame (frame->pkt_dts)
 * @return 0 em sucesso, valor negativo em caso de erro
 */
int frame_queue_push(FrameQueue* queue, AVBufferRef* buffer,
                     int seq_num, int32_t size, int32_t w, int32_t h, int32_t fmt, int64_t dts);

/**
 * @brief Tenta inserir um frame na fila (não-bloqueante)
 *
 * Aloca um novo nó internamente e insere no final da fila.
 * TRANSFERE OWNERSHIP em sucesso: o chamador não deve mais usar o buffer.
 * Em falha, o chamador mantém ownership e deve liberar o buffer.
 *
 * @param queue Ponteiro para a estrutura da fila
 * @param buffer Buffer a ser inserido
 * @param seq_num Número sequencial do frame
 * @param size Tamanho dos dados descomprimidos
 * @param w Largura do frame
 * @param h Altura do frame
 * @param fmt Pixel format
 * @param dts DTS original do frame (frame->pkt_dts)
 * @return 0 em sucesso (ownership transferido), -1 se fila cheia, outro negativo em erro
 */
int frame_queue_try_push(FrameQueue* queue, AVBufferRef* buffer,
                         int seq_num, int32_t size, int32_t w, int32_t h, int32_t fmt, int64_t dts);

/**
 * @brief Remove um frame da fila (bloqueante)
 *
 * Bloqueia até que haja um elemento disponível.
 * RETORNA OWNERSHIP: o chamador se torna dono do buffer e deve liberá-lo.
 *
 * @param queue Ponteiro para a estrutura da fila
 * @return FrameQueueItem com dados do frame (buffer=NULL indica erro ou poison pill)
 */
FrameQueueItem frame_queue_pop(FrameQueue* queue);

/**
 * @brief Tenta remover um frame da fila (não-bloqueante)
 *
 * Retorna imediatamente se a fila estiver vazia.
 * RETORNA OWNERSHIP em sucesso: o chamador deve liberar o buffer com av_buffer_unref.
 *
 * @param queue Ponteiro para a estrutura da fila
 * @return FrameQueueItem com dados do frame (buffer=NULL se vazia/erro)
 */
FrameQueueItem frame_queue_try_pop(FrameQueue* queue);

/**
 * @brief Retorna o número de elementos na fila
 *
 * @param queue Ponteiro para a estrutura da fila
 * @return Número de elementos
 */
int frame_queue_count(FrameQueue* queue);

/**
 * @brief Verifica se a fila está vazia
 *
 * @param queue Ponteiro para a estrutura da fila
 * @return 1 se vazia, 0 caso contrário
 */
int frame_queue_is_empty(FrameQueue* queue);

/**
 * @brief Limpa a fila, liberando todos os nós e buffers
 *
 * @param queue Ponteiro para a estrutura da fila
 */
void frame_queue_clear(FrameQueue* queue);

#endif /* AVBUFFER_QUEUE_H */
