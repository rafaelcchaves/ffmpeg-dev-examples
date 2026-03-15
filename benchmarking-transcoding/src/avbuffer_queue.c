/**
 * @file avbuffer_queue.c
 * @brief Implementação da fila circular thread-safe para AVBufferRef
 */

#include "avbuffer_queue.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int avbuffer_queue_init(AVBufferQueue* queue) {
    if (!queue) {
        return -1;
    }

    // Inicializa estrutura
    memset(queue, 0, sizeof(AVBufferQueue));
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;

    // Inicializa semáforos
    // sem_empty começa com CAPACITY (todos os slots estão vazios)
    if (sem_init(&queue->sem_empty, 0, AVBUFFER_QUEUE_CAPACITY) != 0) {
        return -1;
    }

    // sem_full começa com 0 (nenhum slot ocupado)
    if (sem_init(&queue->sem_full, 0, 0) != 0) {
        sem_destroy(&queue->sem_empty);
        return -1;
    }

    // mutex começa com 1 (disponível)
    if (sem_init(&queue->mutex, 0, 1) != 0) {
        sem_destroy(&queue->sem_empty);
        sem_destroy(&queue->sem_full);
        return -1;
    }

    return 0;
}

void avbuffer_queue_destroy(AVBufferQueue* queue) {
    if (!queue) {
        return;
    }

    // Libera todos os buffers restantes
    avbuffer_queue_clear(queue);

    // Destrói semáforos
    sem_destroy(&queue->sem_empty);
    sem_destroy(&queue->sem_full);
    sem_destroy(&queue->mutex);
}

int avbuffer_queue_push(AVBufferQueue* queue, AVBufferRef* buffer) {
    // Nota: NULL é aceito como "poison pill" para sinalizar término
    if (!queue) {
        return -1;
    }

    // Espera por um slot vazio (bloqueante)
    if (sem_wait(&queue->sem_empty) != 0) {
        return -1;
    }

    // Entra na seção crítica
    if (sem_wait(&queue->mutex) != 0) {
        sem_post(&queue->sem_empty);  // Restaura o semáforo
        return -1;
    }

    // Insere no head - transferimos ownership da referência
    // O chamador não deve mais usar este buffer após o push
    queue->buffers[queue->head] = buffer;
    queue->head = (queue->head + 1) % AVBUFFER_QUEUE_CAPACITY;
    queue->count++;

    // Sai da seção crítica
    sem_post(&queue->mutex);

    // Sinaliza que há um item disponível
    sem_post(&queue->sem_full);

    return 0;
}

int avbuffer_queue_try_push(AVBufferQueue* queue, AVBufferRef* buffer) {
    // Nota: NULL é aceito como "poison pill" para sinalizar término
    if (!queue) {
        return -1;
    }

    // Tenta obter um slot vazio (não-bloqueante)
    if (sem_trywait(&queue->sem_empty) != 0) {
        if (errno == EAGAIN) {
            return -1;  // Fila cheia
        }
        return -2;  // Erro
    }

    // Entra na seção crítica
    if (sem_wait(&queue->mutex) != 0) {
        sem_post(&queue->sem_empty);
        return -2;
    }

    // Insere no head - transferimos ownership da referência
    queue->buffers[queue->head] = buffer;
    queue->head = (queue->head + 1) % AVBUFFER_QUEUE_CAPACITY;
    queue->count++;

    // Sai da seção crítica
    sem_post(&queue->mutex);

    // Sinaliza que há um item disponível
    sem_post(&queue->sem_full);

    return 0;
}

AVBufferRef* avbuffer_queue_pop(AVBufferQueue* queue) {
    if (!queue) {
        return NULL;
    }

    // Espera por um item disponível (bloqueante)
    if (sem_wait(&queue->sem_full) != 0) {
        return NULL;
    }

    // Entra na seção crítica
    if (sem_wait(&queue->mutex) != 0) {
        sem_post(&queue->sem_full);
        return NULL;
    }

    // Remove do tail
    AVBufferRef* buffer = queue->buffers[queue->tail];
    queue->buffers[queue->tail] = NULL;
    queue->tail = (queue->tail + 1) % AVBUFFER_QUEUE_CAPACITY;
    queue->count--;

    // Sai da seção crítica
    sem_post(&queue->mutex);

    // Sinaliza que há um slot vazio
    sem_post(&queue->sem_empty);

    return buffer;
}

AVBufferRef* avbuffer_queue_try_pop(AVBufferQueue* queue) {
    if (!queue) {
        return NULL;
    }

    // Tenta obter um item (não-bloqueante)
    if (sem_trywait(&queue->sem_full) != 0) {
        return NULL;  // Fila vazia ou erro
    }

    // Entra na seção crítica
    if (sem_wait(&queue->mutex) != 0) {
        sem_post(&queue->sem_full);
        return NULL;
    }

    // Remove do tail
    AVBufferRef* buffer = queue->buffers[queue->tail];
    queue->buffers[queue->tail] = NULL;
    queue->tail = (queue->tail + 1) % AVBUFFER_QUEUE_CAPACITY;
    queue->count--;

    // Sai da seção crítica
    sem_post(&queue->mutex);

    // Sinaliza que há um slot vazio
    sem_post(&queue->sem_empty);

    return buffer;
}

int avbuffer_queue_count(AVBufferQueue* queue) {
    if (!queue) {
        return 0;
    }

    int count;
    sem_wait(&queue->mutex);
    count = queue->count;
    sem_post(&queue->mutex);

    return count;
}

int avbuffer_queue_is_empty(AVBufferQueue* queue) {
    return avbuffer_queue_count(queue) == 0;
}

int avbuffer_queue_is_full(AVBufferQueue* queue) {
    return avbuffer_queue_count(queue) == AVBUFFER_QUEUE_CAPACITY;
}

void avbuffer_queue_clear(AVBufferQueue* queue) {
    if (!queue) {
        return;
    }

    // Drena todos os itens da fila
    while (!avbuffer_queue_is_empty(queue)) {
        AVBufferRef* buffer = avbuffer_queue_try_pop(queue);
        if (buffer) {
            av_buffer_unref(&buffer);
        }
    }
}

// ============================================================================
// FrameQueue - Implementação da Fila Unificada com Lista Encadeada
// ============================================================================

/**
 * @brief Cria um FrameQueueItem vazio (para retornos de erro)
 */
static FrameQueueItem frame_queue_empty_item(void) {
    FrameQueueItem empty = {0};
    empty.buffer = NULL;
    empty.sequence_number = -1;
    empty.size_decompress = 0;
    empty.width = 0;
    empty.height = 0;
    empty.pix_fmt = 0;
    empty.pkt_dts = 0;
    return empty;
}

int frame_queue_init(FrameQueue* queue) {
    if (!queue) {
        return -1;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;

    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        return -1;
    }

    if (pthread_cond_init(&queue->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&queue->mutex);
        return -1;
    }

    return 0;
}

void frame_queue_destroy(FrameQueue* queue) {
    if (!queue) {
        return;
    }

    // Limpa todos os nós e buffers
    frame_queue_clear(queue);

    // Destrói primitivos de sincronização
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->not_empty);
}

int frame_queue_push(FrameQueue* queue, AVBufferRef* buffer,
                     int seq_num, int32_t size, int32_t w, int32_t h, int32_t fmt, int64_t dts) {
    if (!queue) {
        return -1;
    }

    // Aloca novo nó
    FrameQueueNode* node = (FrameQueueNode*)malloc(sizeof(FrameQueueNode));
    if (!node) {
        return -2;  // Erro de alocação
    }

    // Preenche o nó
    node->buffer = buffer;  // Ownership transferido
    node->sequence_number = seq_num;
    node->size_decompress = size;
    node->width = w;
    node->height = h;
    node->pix_fmt = fmt;
    node->pkt_dts = dts;
    node->next = NULL;

    pthread_mutex_lock(&queue->mutex);

    if (queue->tail == NULL) {
        // Fila vazia
        queue->head = node;
        queue->tail = node;
    } else {
        // Adiciona ao final
        queue->tail->next = node;
        queue->tail = node;
    }
    queue->count++;

    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);

    return 0;
}

int frame_queue_try_push(FrameQueue* queue, AVBufferRef* buffer,
                         int seq_num, int32_t size, int32_t w, int32_t h, int32_t fmt, int64_t dts) {
    // Para lista encadeada, try_push é equivalente a push (sempre há espaço)
    // exceto que não bloqueia. Como não há limite de capacidade, sempre succeeds.
    return frame_queue_push(queue, buffer, seq_num, size, w, h, fmt, dts);
}

FrameQueueItem frame_queue_pop(FrameQueue* queue) {
    if (!queue) {
        return frame_queue_empty_item();
    }

    pthread_mutex_lock(&queue->mutex);

    // Espera até haver item
    while (queue->head == NULL) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }

    // Remove do início
    FrameQueueNode* node = queue->head;
    queue->head = node->next;
    if (queue->head == NULL) {
        queue->tail = NULL;  // Fila ficou vazia
    }
    queue->count--;

    // Copia dados para retorno (sem o ponteiro next)
    FrameQueueItem item = {
        .buffer = node->buffer,
        .sequence_number = node->sequence_number,
        .size_decompress = node->size_decompress,
        .width = node->width,
        .height = node->height,
        .pix_fmt = node->pix_fmt,
        .pkt_dts = node->pkt_dts
    };

    // Libera o nó (mas não o buffer - ownership transferido para chamador)
    free(node);

    pthread_mutex_unlock(&queue->mutex);

    return item;
}

FrameQueueItem frame_queue_try_pop(FrameQueue* queue) {
    if (!queue) {
        return frame_queue_empty_item();
    }

    pthread_mutex_lock(&queue->mutex);

    if (queue->head == NULL) {
        pthread_mutex_unlock(&queue->mutex);
        return frame_queue_empty_item();  // Fila vazia
    }

    // Remove do início
    FrameQueueNode* node = queue->head;
    queue->head = node->next;
    if (queue->head == NULL) {
        queue->tail = NULL;  // Fila ficou vazia
    }
    queue->count--;

    // Copia dados para retorno
    FrameQueueItem item = {
        .buffer = node->buffer,
        .sequence_number = node->sequence_number,
        .size_decompress = node->size_decompress,
        .width = node->width,
        .height = node->height,
        .pix_fmt = node->pix_fmt,
        .pkt_dts = node->pkt_dts
    };

    // Libera o nó
    free(node);

    pthread_mutex_unlock(&queue->mutex);

    return item;
}

int frame_queue_count(FrameQueue* queue) {
    if (!queue) {
        return 0;
    }

    int count;
    pthread_mutex_lock(&queue->mutex);
    count = queue->count;
    pthread_mutex_unlock(&queue->mutex);

    return count;
}

int frame_queue_is_empty(FrameQueue* queue) {
    return frame_queue_count(queue) == 0;
}

void frame_queue_clear(FrameQueue* queue) {
    if (!queue) {
        return;
    }

    pthread_mutex_lock(&queue->mutex);

    FrameQueueNode* node = queue->head;
    while (node != NULL) {
        FrameQueueNode* next = node->next;
        if (node->buffer) {
            av_buffer_unref(&node->buffer);
        }
        free(node);
        node = next;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;

    pthread_mutex_unlock(&queue->mutex);
}
