/**
 * @file transcode_lz4_mt.h
 * @brief Transcodificação LZ4/LZ4HC Multithread
 *
 * Implementa transcodificação multithread usando arquitetura produtor-consumidor.
 * A thread principal decodifica frames com FFmpeg e os adiciona na fila.
 * Múltiplas threads codificadoras retiram os frames, comprimem com LZ4/LZ4HC
 * e escrevem no arquivo de saída em ordem sequencial.
 */

#ifndef TRANSCODE_LZ4_MT_H
#define TRANSCODE_LZ4_MT_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libavutil/buffer.h>
#include <libavutil/pixdesc.h>
#include <lz4.h>
#include <lz4hc.h>

#include "../avbuffer_queue.h"

// ============================================================================
// Constantes
// ============================================================================

#define ENCODER_TYPE_LZ4   0
#define ENCODER_TYPE_LZ4HC 1

// ============================================================================
// Estruturas de Dados
// ============================================================================

/**
 * @brief Header do frame no arquivo compactado
 *
 * Compatível com decode_lz4 para round-trip.
 */
typedef struct {
    int32_t size_decompress;  /**< Tamanho original dos dados */
    int32_t size_compress;    /**< Tamanho após compressão */
    int32_t width;            /**< Largura do frame */
    int32_t height;           /**< Altura do frame */
    int32_t pix_fmt;          /**< Pixel format (AVPixelFormat) */
} FrameHeader;

/**
 * @brief Informações do frame para codificação
 *
 * Armazenado em fila paralela à AVBufferQueue para manter
 * metadata junto com o buffer.
 */
typedef struct {
    int sequence_number;      /**< Número sequencial do frame (para escrita ordenada) */
    FrameHeader header;       /**< Metadata do frame */
    AVBufferRef* buffer;      /**< Buffer com dados descomprimidos (ownership) */
} EncodeFrameInfo;

/**
 * @brief Contexto da thread codificadora
 *
 * Cada thread possui seus próprios buffers para evitar contenção.
 */
typedef struct {
    int thread_id;                         /**< ID da thread */
    char *compressed_buffer;               /**< Buffer privativo para dados comprimidos */
    int max_compressed_size;               /**< Capacidade máxima do buffer */
    int compression_level;                 /**< Nível (LZ4HC) ou aceleração (LZ4) */
    int encoder_type;                      /**< ENCODER_TYPE_LZ4 ou ENCODER_TYPE_LZ4HC */
} EncoderThreadContext;

/**
 * @brief Argumentos para a thread produtora
 */
typedef struct {
    const char *input_filename;   /**< Arquivo de entrada (vídeo) */
    int decode_thread_count;      /**< Número de threads para decodificador FFmpeg */
    int encoder_thread_count;     /**< Número de threads codificadoras */
} ProducerArgs;

// ============================================================================
// Variáveis Globais
// ============================================================================

/** Fila unificada de frames (buffer + metadata) */
extern FrameQueue g_frame_queue;

/** Controle de escrita sequencial */
extern size_t g_next_to_write;
extern pthread_mutex_t g_write_mutex;
extern pthread_cond_t g_write_cond;

/** Controle de estado */
extern bool g_producer_finished;
extern size_t g_total_frames_encoded;
extern pthread_mutex_t g_state_mutex;

/** Controle de threads ativas */
extern int g_active_threads_count;
extern pthread_mutex_t g_active_threads_mutex;
extern pthread_cond_t g_active_threads_cond;

/** Arquivo de saída */
extern FILE *g_output_file;

/** Configuração */
extern int g_num_decode_threads;
extern int g_num_encoder_threads;
extern int g_encoder_type;
extern int g_compression_level;
extern const char *g_profile_name;
extern bool g_debug_mode;

/** Métricas (atómicas) */
extern volatile int64_t g_total_decode_time;
extern volatile int64_t g_total_compress_time;
extern volatile int64_t g_total_write_time;
extern volatile int g_frame_count;

// ============================================================================
// Funções de Inicialização
// ============================================================================

/**
 * @brief Inicializa a fila de buffers
 * @return 0 em sucesso, valor negativo em erro
 */
int mt_encode_init_queue(void);

/**
 * @brief Inicializa mutexes e condition variables
 * @return 0 em sucesso, valor negativo em erro
 */
int mt_encode_init_synchronization(void);

/**
 * @brief Inicializa estado global
 * @param output_file Arquivo de saída aberto para escrita
 * @return 0 em sucesso, valor negativo em erro
 */
int mt_encode_init_state(FILE *output_file);

// ============================================================================
// Funções de Limpeza
// ============================================================================

/**
 * @brief Destrói a fila de buffers
 */
void mt_encode_cleanup_queue(void);

/**
 * @brief Destrói mutexes e condition variables
 */
void mt_encode_cleanup_synchronization(void);

/**
 * @brief Limpa estado global
 */
void mt_encode_cleanup_state(void);

// ============================================================================
// Threads
// ============================================================================

/**
 * @brief Thread produtora (decodificador FFmpeg)
 *
 * Decodifica frames do arquivo de entrada e os coloca na fila
 * para as threads codificadoras.
 *
 * @param arg Ponteiro para ProducerArgs
 * @return NULL
 */
void *mt_producer_thread(void *arg);

/**
 * @brief Thread codificadora (LZ4/LZ4HC)
 *
 * Retira frames da fila, comprime e escreve no arquivo de saída
 * em ordem sequencial.
 *
 * @param arg Ponteiro para EncoderThreadContext
 * @return NULL
 */
void *mt_encoder_thread(void *arg);

// ============================================================================
// Função Principal
// ============================================================================

/**
 * @brief Função principal de codificação multithread
 *
 * Orquestra todo o processo de codificação.
 *
 * @param input_file Arquivo de entrada (vídeo)
 * @param output_file Arquivo de saída (LZ4 compactado)
 * @param num_decode_threads Número de threads para decodificador FFmpeg
 * @param num_encoder_threads Número de threads codificadoras
 * @param encoder_type ENCODER_TYPE_LZ4 ou ENCODER_TYPE_LZ4HC
 * @param compression_level Nível de compressão ou aceleração
 * @param profile Nome do profile para métricas
 * @param debug_mode Modo debug para métricas detalhadas
 * @return 0 em sucesso, valor negativo em erro
 */
int mt_encode_main(const char *input_file, const char *output_file,
                   int num_decode_threads, int num_encoder_threads,
                   int encoder_type, int compression_level,
                   const char *profile, bool debug_mode);

#endif /* TRANSCODE_LZ4_MT_H */
