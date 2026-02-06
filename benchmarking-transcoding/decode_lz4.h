#ifndef DECODE_LZ4_H
#define DECODE_LZ4_H

#include <stdint.h>
#include <stdio.h>

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

#endif
