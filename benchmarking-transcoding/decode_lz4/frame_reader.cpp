#include "decode_lz4.h"
#include <stdlib.h>
#include <string.h>

int frame_reader_init(const char *filename, FILE **infile) {
    *infile = fopen(filename, "rb");
    if (!*infile) {
        fprintf(stderr, "Could not open %s\n", filename);
        return -1;
    }
    return 0;
}

int frame_reader_read_header(FILE *infile, FrameHeader *header) {
    size_t read_count = fread(header, sizeof(FrameHeader), 1, infile);
    if (read_count != 1) {
        if (feof(infile)) {
            return 1;  // EOF
        }
        fprintf(stderr, "Erro: Arquivo muito curto ou corrompido (falha ao ler header).\n");
        return -1;
    }
    return 0;
}

int frame_reader_read_data(FILE *infile, char *buffer, int size) {
    size_t read_count = fread(buffer, 1, (size_t)size, infile);
    if (read_count != (size_t)size) {
        fprintf(stderr, "Erro: O arquivo terminou antes do esperado.\n");
        return -1;
    }
    return 0;
}

void frame_reader_close(FILE *infile) {
    if (infile) {
        fclose(infile);
    }
}
