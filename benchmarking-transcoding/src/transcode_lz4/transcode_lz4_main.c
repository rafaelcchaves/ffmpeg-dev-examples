/**
 * @file transcode_lz4_main.c
 * @brief Ponto de entrada para transcodificação LZ4/LZ4HC multithread
 */

#include "transcode_lz4_mt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Threads definidas em tempo de compilação
#ifndef THREADS_IN
#define THREADS_IN 0    // 0 = FFmpeg escolhe automaticamente
#endif
#ifndef THREADS_OUT
#define THREADS_OUT 1   // Mínimo 1 thread codificadora LZ4
#endif

/**
 * @brief Exibe mensagem de uso
 */
void print_usage(const char *prog_name) {
    fprintf(stderr, "\nUso: %s [opcoes]\n\n", prog_name);
    fprintf(stderr, "Opcoes:\n");
    fprintf(stderr, "  -i <arquivo>    Arquivo de entrada (video)\n");
    fprintf(stderr, "  -o <arquivo>    Arquivo de saida (LZ4 compactado)\n");
    fprintf(stderr, "  -e <encoder>    Encoder: lz4 ou lz4hc (default: lz4)\n");
    fprintf(stderr, "  -l <nivel>      Nivel de compressao:\n");
    fprintf(stderr, "                  - LZ4: aceleracao (1=rapido, 65537=lento, default: 1)\n");
    fprintf(stderr, "                  - LZ4HC: nivel (1-12, default: 9)\n");
    fprintf(stderr, "  -p <profile>    Nome do profile para metricas\n");
    fprintf(stderr, "  -d              Modo debug: exibe metricas detalhadas\n");
    fprintf(stderr, "\nThreads (definidas em tempo de compilacao):\n");
    fprintf(stderr, "  THREADS_IN=%d   Threads decodificadoras FFmpeg\n", THREADS_IN);
    fprintf(stderr, "  THREADS_OUT=%d  Threads codificadoras LZ4\n", THREADS_OUT);
    fprintf(stderr, "\nExemplos:\n");
    fprintf(stderr, "  %s -i video.mp4 -o video.enc -p baseline\n", prog_name);
    fprintf(stderr, "  %s -i video.mp4 -o video.enc -e lz4 -l 1 -p fast\n", prog_name);
    fprintf(stderr, "  %s -i video.mp4 -o video.enc -e lz4hc -l 9 -p high_perf\n", prog_name);
    fprintf(stderr, "\n");
}

int main(int argc, char **argv) {
    const char *input_file = NULL;
    const char *output_file = NULL;
    const char *profile = NULL;
    const char *encoder_name = "lz4";
    int num_decode_threads = THREADS_IN;
    int num_encoder_threads = THREADS_OUT;
    int compression_level = 1;
    bool debug_mode = false;
    int opt;

    // Parse argumentos
    while ((opt = getopt(argc, argv, "i:o:e:l:p:d")) != -1) {
        switch (opt) {
            case 'i':
                input_file = optarg;
                break;
            case 'o':
                output_file = optarg;
                break;
            case 'e':
                encoder_name = optarg;
                break;
            case 'l':
                compression_level = atoi(optarg);
                break;
            case 'p':
                profile = optarg;
                break;
            case 'd':
                debug_mode = true;
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // Validação de argumentos obrigatórios
    if (!input_file || !output_file || !profile) {
        fprintf(stderr, "Erro: Argumentos obrigatorios faltando\n");
        print_usage(argv[0]);
        return 1;
    }

    // Determina tipo de encoder
    int encoder_type;
    if (strcmp(encoder_name, "lz4") == 0) {
        encoder_type = ENCODER_TYPE_LZ4;
        // Default para LZ4: aceleracao = 1 (mais rapido)
        if (compression_level <= 0) {
            compression_level = 1;
        }
    } else if (strcmp(encoder_name, "lz4hc") == 0) {
        encoder_type = ENCODER_TYPE_LZ4HC;
        // Default para LZ4HC: nivel = 9 (balanceado)
        if (compression_level <= 0) {
            compression_level = 9;
        }
        // Valida nível para LZ4HC (1-12)
        if (compression_level < 1 || compression_level > 12) {
            fprintf(stderr, "Erro: Nivel de compressao para LZ4HC deve estar entre 1 e 12\n");
            return 1;
        }
    } else {
        fprintf(stderr, "Erro: Encoder '%s' nao suportado. Use 'lz4' ou 'lz4hc'\n",
                encoder_name);
        return 1;
    }

    // Executa codificação
    return mt_encode_main(input_file, output_file, num_decode_threads, num_encoder_threads,
                          encoder_type, compression_level, profile, debug_mode);
}
