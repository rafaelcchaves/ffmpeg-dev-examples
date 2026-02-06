extern "C" {
#include <libavutil/time.h>
}

#include "decode_lz4.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef THREADS_IN
#define THREADS_IN 0
#endif

int main(int argc, char** argv) {
    const char *infilename = NULL;
    const char *outfilename = NULL;
    const char *profile = NULL;
    FILE *infile = NULL;
    FILE *outfile = NULL;
    DecodeContext ctx;
    int allocated = 0;
    int frames = 0;
    int opt;

    // Parse argumentos
    while ((opt = getopt(argc, argv, "i:o:p:")) != -1) {
        switch (opt) {
            case 'i':
                infilename = optarg;
                break;
            case 'o':
                outfilename = optarg;
                break;
            case 'p':
                profile = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s -i <input-file> -o <output-file> -p <profile>\n", argv[0]);
                fprintf(stderr, "  -i  Input LZ compressed file\n");
                fprintf(stderr, "  -o  Output YUV file\n");
                fprintf(stderr, "  -p  Profile name (e.g., low_latency)\n");
                exit(1);
        }
    }

    if (infilename == NULL || outfilename == NULL || profile == NULL) {
        fprintf(stderr, "Usage: %s -i <input-file> -o <output-file> -p <profile>\n", argv[0]);
        fprintf(stderr, "  -i  Input LZ compressed file\n");
        fprintf(stderr, "  -o  Output YUV file\n");
        fprintf(stderr, "  -p  Profile name (e.g., low_latency)\n");
        exit(1);
    }

    // Inicializa contextos
    decode_context_init(&ctx);

    // Abre arquivos
    if (frame_writer_init(outfilename, &outfile) < 0) {
        return 1;
    }

    if (frame_reader_init(infilename, &infile) < 0) {
        frame_writer_close(outfile);
        return 1;
    }

    int64_t start_time = av_gettime();

    // Loop principal de decodificacao
    while (1) {
        FrameHeader hdr;
        int ret;

        // Le header do frame
        ret = frame_reader_read_header(infile, &hdr);
        if (ret < 0) {
            frame_reader_close(infile);
            frame_writer_close(outfile);
            decode_context_free(&ctx);
            return 1;
        }
        if (ret == 1) {
            // EOF
            break;
        }

        // Aloca buffers no primeiro frame
        if (!allocated) {
            if (decode_context_alloc_buffers(&ctx, &hdr) < 0) {
                frame_reader_close(infile);
                frame_writer_close(outfile);
                return 1;
            }
            allocated = 1;
        }

        // Le dados compactados
        if (frame_reader_read_data(infile, ctx.compressed_frame, hdr.size_compress) < 0) {
            frame_reader_close(infile);
            frame_writer_close(outfile);
            decode_context_free(&ctx);
            return 1;
        }

        // Decodifica
        int64_t st = av_gettime();
        if (decode_single_thread(&ctx, ctx.compressed_frame, hdr.size_compress, &hdr) < 0) {
            frame_reader_close(infile);
            frame_writer_close(outfile);
            decode_context_free(&ctx);
            return 1;
        }
        int64_t decode_time = av_gettime() - st;

        // Escreve frame decodificado
        if (frame_writer_write_frame(outfile, &ctx) < 0) {
            frame_reader_close(infile);
            frame_writer_close(outfile);
            decode_context_free(&ctx);
            return 1;
        }

        // Imprime estatisticas
        stats_print_frame(profile, THREADS_IN, decode_time);

        frames++;
    }

    // Imprime resumo final
    int64_t total_time = av_gettime() - start_time;
    stats_print_summary(profile, THREADS_IN, frames, total_time);

    // Cleanup
    frame_reader_close(infile);
    frame_writer_close(outfile);
    decode_context_free(&ctx);

    return 0;
}
