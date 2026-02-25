#include "decode_lz4.h"
#include <stdlib.h>

int frame_writer_init(const char *filename, FILE **outfile) {
    *outfile = fopen(filename, "wb");
    if (!*outfile) {
        fprintf(stderr, "Could not open %s\n", filename);
        return -1;
    }
    return 0;
}

int frame_writer_write_frame(FILE *outfile, const DecodeContext *ctx) {
    // COMENTADO PARA BENCHMARK SEM I/O
    (void)outfile;  // Avoid unused warning
    (void)ctx;  // Avoid unused warning
    /*
    size_t written = fwrite(ctx->image_data[0], 1, (size_t)ctx->image_bufsize, outfile);
    if (written != (size_t)ctx->image_bufsize) {
        fprintf(stderr, "Error writing frame to output\n");
        return -1;
    }
    */
    return 0;
}

void frame_writer_close(FILE *outfile) {
    if (outfile) {
        fclose(outfile);
    }
}
