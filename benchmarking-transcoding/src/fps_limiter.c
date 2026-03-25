#include "fps_limiter.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <libavutil/time.h>
#ifdef __cplusplus
}
#endif

#include <unistd.h>

void fps_limiter_init(FpsLimiter *limiter, double target_fps)
{
    limiter->enabled = 0;
    limiter->frame_interval_us = 0;
    limiter->next_frame_time_us = 0;

    if (target_fps > 0.0) {
        limiter->enabled = 1;
        limiter->frame_interval_us = (int64_t)(1000000.0 / target_fps);
    }
}

void fps_limiter_wait(FpsLimiter *limiter)
{
    if (!limiter->enabled)
        return;

    int64_t now = av_gettime();

    if (limiter->next_frame_time_us == 0) {
        /* Primeiro frame: retorna imediatamente e agenda o proximo */
        limiter->next_frame_time_us = now + limiter->frame_interval_us;
        return;
    }

    int64_t remaining = limiter->next_frame_time_us - now;

    if (remaining > 0) {
        usleep((useconds_t)remaining);
    }

    /* Prevencao de burst: reset se atraso > 1 intervalo */
    now = av_gettime();
    if (limiter->next_frame_time_us < now - limiter->frame_interval_us) {
        limiter->next_frame_time_us = now + limiter->frame_interval_us;
    } else {
        limiter->next_frame_time_us += limiter->frame_interval_us;
    }
}
