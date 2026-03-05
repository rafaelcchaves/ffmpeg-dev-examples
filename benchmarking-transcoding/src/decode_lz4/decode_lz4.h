#ifndef DECODE_LZ4_H
#define DECODE_LZ4_H

#include <stdint.h>
#include <stdio.h>
#include <queue>
#include <vector>
#include <string>
#include <pthread.h>
#include <semaphore.h>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
#include <libavutil/time.h>
#include <lz4.h>
}

// Flag global de debug para métricas detalhadas
extern bool g_debug_mode;

// Estrutura do header do arquivo compactado
typedef struct {
    int32_t size_decompress;
    int32_t size_compress;
    int32_t width, height;
    int32_t pix_fmt;
} FrameHeader;

// Contexto de decodificacao
typedef struct {
    uint8_t *image_data[4];
    int image_linesize[4];
    int image_bufsize;
    char *compressed_frame;
    int max_capacity;
} DecodeContext;

// Configuracao do decodificador
typedef enum {
    DECODE_MODE_SINGLE_THREAD,  // modo atual
    DECODE_MODE_MULTI_THREAD,   // futuro
} DecodeMode;

typedef struct {
    DecodeMode mode;
    int threads;
    const char *profile_name;
} DecodeConfig;

// Interfaces do modulo frame_reader
int frame_reader_init(const char *filename, FILE **infile);
int frame_reader_read_header(FILE *infile, FrameHeader *header);
int frame_reader_read_data(FILE *infile, char *buffer, int size);
void frame_reader_close(FILE *infile);

// Interfaces do modulo frame_decoder
int decode_context_init(DecodeContext *ctx);
int decode_context_alloc_buffers(DecodeContext *ctx, const FrameHeader *header);
void decode_context_free(DecodeContext *ctx);
int decode_single_thread(DecodeContext *ctx, const char *compressed, int comp_size, const FrameHeader *hdr);

// Interfaces do modulo frame_writer
int frame_writer_init(const char *filename, FILE **outfile);
int frame_writer_write_frame(FILE *outfile, const DecodeContext *ctx);
void frame_writer_close(FILE *outfile);

// Interfaces do modulo stats
void stats_print_frame(const char *profile, int threads, int64_t decode_time_us);
void stats_print_summary(const char *profile, int threads, int frames, int64_t total_time_us);
void stats_print_cpu(const char *profile, int threads, double cpu_usage);

// ============================================================================
// Estruturas para Decodificação Multithread (Buffer Compartilhado Simples)
// ============================================================================

// Frame disponível no buffer compartilhado
typedef struct {
    int sequence_number;
    FrameHeader header;
    int data_size;           // Tamanho dos dados codificados
} SharedFrameInfo;

// Contexto da thread decodificadora (memória privativa)
typedef struct {
    int thread_id;
    DecodeContext decode_ctx;           // Buffers privados de decodificação
    char *compressed_buffer;            // Buffer privativo para dados comprimidos
    int max_compressed_size;
} ThreadContext;

// ============================================================================
// Variáveis Globais para Multithreading
// ============================================================================

// Buffer compartilhado (produtor <-> consumidores)
extern char *g_shared_buffer;
extern SharedFrameInfo g_shared_frame_info;
extern int g_shared_buffer_size;

// Semáforos para sincronização do buffer compartilhado
extern sem_t g_sem_empty;  // Sinaliza que buffer está vazio (produtor pode escrever)
extern sem_t g_sem_full;   // Sinaliza que buffer tem dados (consumidor pode ler)

// Mutex para proteger acesso às info do frame compartilhado
extern pthread_mutex_t g_shared_info_mutex;

// Controle de escrita sequencial no arquivo de saída
extern size_t g_next_to_write;
extern pthread_mutex_t g_write_mutex;
extern pthread_cond_t g_write_cond;

// Controle de estado global
extern bool g_producer_finished;
extern size_t g_total_frames_processed;
extern pthread_mutex_t g_state_mutex;

// Controle de threads ativas (para shutdown correto)
extern int g_active_threads_count;
extern pthread_mutex_t g_active_threads_mutex;
extern pthread_cond_t g_active_threads_cond;

// Arquivos
extern FILE *g_input_file;
extern FILE *g_output_file;

// Configuração
extern int g_num_decoder_threads;
extern std::string g_profile_name;

// ============================================================================
// Funções do sistema multithread
// ============================================================================

// Inicialização
int mt_initialize_shared_buffer(int buffer_size);
int mt_initialize_synchronization();
int mt_initialize_write_control(FILE *output_file);
int mt_initialize_state(FILE *input_file);

// Limpeza
void mt_cleanup_shared_buffer();
void mt_cleanup_synchronization();
void mt_cleanup_write_control();
void mt_cleanup_state();

// Thread produtora
void *mt_producer_thread(void *arg);

// Thread decodificadora
void *mt_decoder_thread(void *arg);

// Função principal multithread
int mt_decode_main(int num_threads, const std::string &input_file,
                   const std::string &output_file, const std::string &profile);

#endif
