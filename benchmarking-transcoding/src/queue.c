/**
 * @file queue.c
 * @brief Implementacao da fila generica thread-safe
 *
 * Adaptada da FrameQueue em avbuffer_queue.c.
 */

#include "queue.h"
#include "frame_types.h"
#include <stdlib.h>
#include <string.h>

int queue_init(Queue *q, ItemFreeFunc free_func) {
    if (!q) {
        return -1;
    }

    memset(q, 0, sizeof(Queue));
    q->head = NULL;
    q->tail = NULL;
    q->count = 0;
    q->free_func = free_func;

    if (pthread_mutex_init(&q->mutex, NULL) != 0) {
        return -1;
    }

    if (pthread_cond_init(&q->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&q->mutex);
        return -1;
    }

    return 0;
}

void queue_destroy(Queue *q) {
    if (!q) {
        return;
    }

    queue_clear(q);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
}

int queue_push(Queue *q, void *item) {
    if (!q) {
        return -1;
    }

    // Aloca novo no
    ListItem *node = (ListItem *)malloc(sizeof(ListItem));
    if (!node) {
        return -2;
    }

    node->item = item;  // Ownership transferido (NULL = poison pill)
    node->next = NULL;

    pthread_mutex_lock(&q->mutex);

    if (q->tail == NULL) {
        q->head = node;
        q->tail = node;
    } else {
        q->tail->next = node;
        q->tail = node;
    }
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);

    return 0;
}

void *queue_pop(Queue *q) {
    if (!q) {
        return NULL;
    }

    pthread_mutex_lock(&q->mutex);

    // Espera ate haver item
    while (q->head == NULL) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    // Remove do inicio
    ListItem *node = q->head;
    q->head = node->next;
    if (q->head == NULL) {
        q->tail = NULL;
    }
    q->count--;

    void *item = node->item;

    // Libera o no (mas nao o item — ownership transferido para o chamador)
    free(node);

    pthread_mutex_unlock(&q->mutex);

    return item;
}

void queue_clear(Queue *q) {
    if (!q) {
        return;
    }

    pthread_mutex_lock(&q->mutex);

    ListItem *node = q->head;
    while (node != NULL) {
        ListItem *next = node->next;
        // NULL = poison pill, nao chama free_func
        if (node->item && q->free_func) {
            q->free_func(node->item);
        }
        free(node);
        node = next;
    }

    q->head = NULL;
    q->tail = NULL;
    q->count = 0;

    pthread_mutex_unlock(&q->mutex);
}

int queue_count(Queue *q) {
    if (!q) {
        return 0;
    }

    int count;
    pthread_mutex_lock(&q->mutex);
    count = q->count;
    pthread_mutex_unlock(&q->mutex);

    return count;
}

int queue_is_empty(Queue *q) {
    return queue_count(q) == 0;
}

// ============================================================================
// frame_item_free — definida aqui pois depende de queue.h e libavutil
// ============================================================================

void frame_item_free(void *item) {
    FrameItem *fi = (FrameItem *)item;
    if (fi->data) {
        av_buffer_unref(&fi->data);
    }
    free(fi);
}
