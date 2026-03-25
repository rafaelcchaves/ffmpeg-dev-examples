/**
 * @file transcode_lz4_st.h
 * @brief Transcodificação LZ4/LZ4HC Single-thread
 */

#ifndef TRANSCODE_LZ4_ST_H
#define TRANSCODE_LZ4_ST_H

/**
 * @brief Função principal de codificação single-thread
 *
 * Decodifica frames com FFmpeg e comprime com LZ4/LZ4HC em um loop simples.
 * O decoder FFmpeg pode usar threading interno (controlado por decoder_threads).
 *
 * @param input_file Arquivo de entrada (vídeo)
 * @param output_file Arquivo de saída (LZ4 compactado)
 * @param decoder_threads Threads para decodificador FFmpeg (0 = auto)
 * @param encoder_type ENCODER_TYPE_LZ4 ou ENCODER_TYPE_LZ4HC
 * @param compression_level Nível de compressão ou aceleração
 * @param profile Nome do profile para métricas
 * @return 0 em sucesso, valor negativo em erro
 */
int st_encode_main(const char *input_file, const char *output_file,
                   int decoder_threads, int encoder_type,
                   int compression_level, const char *profile);

#endif /* TRANSCODE_LZ4_ST_H */
