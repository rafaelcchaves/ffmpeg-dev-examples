// ============================================================================
// decode_lz4_main.cpp - Entry point para decodificador LZ4
// Suporta modo single-thread e multi-thread
// ============================================================================

#include "decode_lz4.h"
#include "../cpu_stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Definição das variáveis globais
bool g_debug_mode = false;
bool g_enable_write = false;

// Declaração da função multithread
extern int mt_decode_main(int num_threads, const std::string &input_file,
                          const std::string &output_file, const std::string &profile);

// Função single-thread original
int st_decode_main(const std::string &input_file,
                   const std::string &output_file,
                   const std::string &profile);

void print_usage(const char *prog_name) {
    fprintf(stderr, "Uso: %s [opcoes]\n", prog_name);
    fprintf(stderr, "\nOpcoes:\n");
    fprintf(stderr, "  -i <arquivo>    Arquivo de entrada LZ4 compactado\n");
    fprintf(stderr, "  -o <arquivo>    Arquivo de saida YUV\n");
    fprintf(stderr, "  -p <profile>    Nome do profile (ex: low_latency)\n");
    fprintf(stderr, "  -D <threads>    Numero de threads decodificadoras (default: 1 = single-thread)\n");
    fprintf(stderr, "  -d              Modo debug: exibe métricas detalhadas de I/O e decodificação\n");
    fprintf(stderr, "  -w              Habilita escrita de arquivo de saida (default: desabilitado)\n");
    fprintf(stderr, "\nExemplos:\n");
    fprintf(stderr, "  %s -i input.enc -o output.yuv -p baseline\n", prog_name);
    fprintf(stderr, "  %s -i input.enc -o output.yuv -p low_latency -D 4\n", prog_name);
    fprintf(stderr, "  %s -i input.enc -o output.yuv -p low_latency -D 4 -d\n", prog_name);
}

int main(int argc, char **argv) {
    std::string input_file;
    std::string output_file;
    std::string profile;
    int num_decoder_threads = 1;
    bool enable_write = false;
    int opt;

    // Parse argumentos
    while ((opt = getopt(argc, argv, "i:o:p:D:dw")) != -1) {
        switch (opt) {
            case 'i':
                input_file = optarg;
                break;
            case 'o':
                output_file = optarg;
                break;
            case 'p':
                profile = optarg;
                break;
            case 'D':
                num_decoder_threads = atoi(optarg);
                break;
            case 'd':
                g_debug_mode = true;
                break;
            case 'w':
                enable_write = true;
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // Valida argumentos obrigatórios
    if (input_file.empty() || profile.empty()) {
        print_usage(argv[0]);
        return 1;
    }

    // Valida número de threads
    if (num_decoder_threads < 1) {
        fprintf(stderr, "Erro: Numero de threads deve ser >= 1\n");
        return 1;
    }

    // Default para arquivo de saida
    if (output_file.empty()) output_file = "output.yuv";

    // Configura modo escrita
    g_enable_write = enable_write;

    // Escolhe modo de operação
    if (num_decoder_threads == 1) {
        // Modo single-thread (implementação original)
        return st_decode_main(input_file, output_file, profile);
    } else {
        // Modo multi-thread
        return mt_decode_main(num_decoder_threads, input_file, output_file, profile);
    }

    return 0;
}

// Implementação single-thread (código original)
int st_decode_main(const std::string &input_file,
                   const std::string &output_file,
                   const std::string &profile) {
    FILE *infile = NULL;
    FILE *outfile = NULL;
    DecodeContext ctx;
    int allocated = 0;
    int frames = 0;
    CpuStats cpu_start, cpu_end;

    // Inicializa contexto
    decode_context_init(&ctx);

    // Abre arquivos
    if (frame_writer_init(output_file.c_str(), &outfile) < 0) {
        return 1;
    }

    if (frame_reader_init(input_file.c_str(), &infile) < 0) {
        frame_writer_close(outfile);
        return 1;
    }

    int64_t start_time = av_gettime();
    cpu_stats_read(&cpu_start);

    // Loop principal de decodificação
    while (1) {
        FrameHeader hdr;
        int ret;

        // Lê header do frame
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

        // Lê dados compactados
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

        // Imprime estatísticas
        stats_print_frame(profile.c_str(), 1, 0, decode_time);

        frames++;
    }

    // Imprime resumo final
    int64_t total_time = av_gettime() - start_time;
    cpu_stats_read(&cpu_end);
    double cpu_usage = cpu_stats_calculate_usage(&cpu_start, &cpu_end);

    stats_print_summary(profile.c_str(), 1, 0, frames, total_time);
    stats_print_cpu(profile.c_str(), 1, 0, cpu_usage);

    // Cleanup
    frame_reader_close(infile);
    frame_writer_close(outfile);
    decode_context_free(&ctx);

    return 0;
}
