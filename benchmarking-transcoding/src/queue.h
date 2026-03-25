/**
 * @file queue.h
 * @brief Fila generica thread-safe com gerenciamento de ownership
 *
 * Fila unbounded baseada em lista encadeada com mutex + condition variable.
 * Suporta callback de liberacao para gerenciamento automatico de memoria.
 * NULL e aceito como "poison pill" para sinalizar termino.
 */

#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback para liberar itens da fila
 */
typedef void (*ItemFreeFunc)(void *item);

/**
 * @brief No da lista encadeada
 */
typedef struct ListItem {
    void *item;
    struct ListItem *next;
} ListItem;

/**
 * @brief Fila generica thread-safe
 */
typedef struct {
    ListItem *head;            /**< Primeiro no (para remocao) */
    ListItem *tail;            /**< Ultimo no (para insercao) */
    int count;                 /**< Numero de elementos */
    pthread_mutex_t mutex;     /**< Exclusao mutua */
    pthread_cond_t not_empty;  /**< Sinaliza quando ha itens */
    ItemFreeFunc free_func;    /**< Callback para liberar itens (pode ser NULL) */
} Queue;

/**
 * @brief Inicializa a fila
 *
 * @param q Ponteiro para a fila
 * @param free_func Callback para liberar itens (pode ser NULL)
 * @return 0 em sucesso, -1 em erro
 */
int queue_init(Queue *q, ItemFreeFunc free_func);

/**
 * @brief Destroi a fila, liberando todos os itens restantes
 *
 * Chama free_func para cada item restante (exceto NULL/poison pills).
 *
 * @param q Ponteiro para a fila
 */
void queue_destroy(Queue *q);

/**
 * @brief Insere um item na fila (bloqueante)
 *
 * Ownership do item e transferido para a fila.
 * NULL e aceito como poison pill (free_func nao e chamado).
 *
 * @param q Ponteiro para a fila
 * @param item Item a ser inserido (ownership transferido)
 * @return 0 em sucesso, -1 em erro
 */
int queue_push(Queue *q, void *item);

/**
 * @brief Remove um item da fila (bloqueante)
 *
 * Bloqueia ate que haja um item disponivel.
 * Ownership do item retornado e transferido para o chamador.
 *
 * @param q Ponteiro para a fila
 * @return Item removido (ownership transferido), ou NULL para poison pill
 */
void *queue_pop(Queue *q);

/**
 * @brief Limpa a fila, liberando todos os itens
 *
 * @param q Ponteiro para a fila
 */
void queue_clear(Queue *q);

/**
 * @brief Retorna o numero de elementos na fila
 *
 * @param q Ponteiro para a fila
 * @return Numero de elementos
 */
int queue_count(Queue *q);

/**
 * @brief Verifica se a fila esta vazia
 *
 * @param q Ponteiro para a fila
 * @return 1 se vazia, 0 caso contrario
 */
int queue_is_empty(Queue *q);

#ifdef __cplusplus
}
#endif

#endif /* QUEUE_H */
