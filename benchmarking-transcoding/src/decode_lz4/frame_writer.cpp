#include "decode_lz4.h"
#include <stdlib.h>

// Flag de compilação para habilitar/desabilitar escrita de arquivo de saída
// Padrão: desabilitado (benchmark sem I/O de disco)
#ifndef ENABLE_OUTPUT_WRITE
#define ENABLE_OUTPUT_WRITE 0
#endif

int frame_writer_init(const char *filename, FILE **outfile) {
#if ENABLE_OUTPUT_WRITE
    *outfile = fopen(filename, "wb");
    if (!*outfile) {
        fprintf(stderr, "Could not open %s\n", filename);
        return -1;
    }
#else
    *outfile = NULL;  // No file opened when writes disabled
    (void)filename;  // Avoid unused warning
#endif
    return 0;
}

int frame_writer_write_frame(FILE *outfile, const DecodeContext *ctx) {
#if ENABLE_OUTPUT_WRITE
    size_t written = fwrite(ctx->image_data[0], 1, (size_t)ctx->image_bufsize, outfile);
    if (written != (size_t)ctx->image_bufsize) {
        fprintf(stderr, "Error writing frame to output\n");
        return -1;
    }
#else
    (void)outfile;  // Avoid unused warning
    (void)ctx;  // Avoid unused warning
#endif
    return 0;
}

void frame_writer_close(FILE *outfile) {
#if ENABLE_OUTPUT_WRITE
    if (outfile) {
        fclose(outfile);
    }
#else
    (void)outfile;  // Avoid unused warning
#endif
}
