#ifndef DECODE_MJPEG_MT_H
#define DECODE_MJPEG_MT_H

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <pthread.h>
#include <atomic>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include "frame_types.h"
#include "queue.h"
}

// ============================================================================
// Contexto da thread decodificadora (memoria privativa)
// ============================================================================

typedef struct {
    int thread_id;
    AVCodecContext *dec_ctx;      // Cada thread tem o seu decoder
    AVFrame *frame;               // Cada thread tem o seu frame
    uint8_t *image_data[4];       // Buffer YUV contiguo para escrita
    int image_linesize[4];
    int image_bufsize;
} MJpegThreadContext;

// ============================================================================
// Variaveis Globais para Multithreading
// ============================================================================

extern Queue g_mjpeg_frame_queue;

// Controle de escrita sequencial
extern size_t g_mjpeg_next_to_write;
extern pthread_mutex_t g_mjpeg_write_mutex;
extern pthread_cond_t g_mjpeg_write_cond;

// Controle de estado global
extern bool g_mjpeg_producer_finished;
extern size_t g_mjpeg_total_frames_processed;
extern pthread_mutex_t g_mjpeg_state_mutex;

// Controle de threads ativas (shutdown)
extern int g_mjpeg_active_threads_count;
extern pthread_mutex_t g_mjpeg_active_threads_mutex;
extern pthread_cond_t g_mjpeg_active_threads_cond;

// Arquivo de saida
extern FILE *g_mjpeg_output_file;

// Configuracao
extern int g_mjpeg_num_threads;
extern std::string g_mjpeg_profile_name;
extern bool g_mjpeg_enable_write;

// Metricas atomicas
extern std::atomic<int64_t> g_mjpeg_total_demux_time;
extern std::atomic<int64_t> g_mjpeg_total_decode_time;

// ============================================================================
// Funcoes do pipeline multithread
// ============================================================================

int mjpeg_mt_decode(const char *input, const char *output,
                    const char *profile, int threads, bool enable_write);

#endif
