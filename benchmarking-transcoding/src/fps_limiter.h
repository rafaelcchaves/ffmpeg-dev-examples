#ifndef FPS_LIMITER_H
#define FPS_LIMITER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Limitador de FPS para leitura de frames de entrada.
 *
 * Controla o ritmo de leitura simulando uma fonte de dados com taxa fixa.
 * Usa av_gettime() para alta resolucao e prevencao de drift.
 */
typedef struct {
    int64_t frame_interval_us;  /**< Intervalo entre frames em microsegundos (1000000 / fps) */
    int64_t next_frame_time_us; /**< Horario absoluto do proximo frame */
    int     enabled;            /**< 0 = desabilitado (zero overhead) */
} FpsLimiter;

/**
 * Inicializa o limitador de FPS
 *
 * @param limiter   Ponteiro para o limitador
 * @param target_fps FPS desejado (ex: 30.0). Valores <= 0 desabilitam o limitador.
 */
void fps_limiter_init(FpsLimiter *limiter, double target_fps);

/**
 * Bloqueia ate que seja hora de ler o proximo frame
 *
 * No primeiro frame retorna imediatamente. Nos frames subsequentes,
 * dorme o tempo restante do intervalo. Se o processamento ja ultrapassou
 * o horario agendado, retorna sem dormir (sem burst de compensacao).
 *
 * @param limiter Ponteiro para o limitador (deve estar inicializado)
 */
void fps_limiter_wait(FpsLimiter *limiter);

#ifdef __cplusplus
}
#endif

#endif /* FPS_LIMITER_H */
