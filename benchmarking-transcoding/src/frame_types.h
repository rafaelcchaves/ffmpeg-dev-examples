/**
 * @file frame_types.h
 * @brief Tipos compartilhados para filas de encode e decode
 *
 * Define FrameHeader (on-disk, 20 bytes) e FrameItem (item unificado
 * para filas de producao-consumo). Substitui as definicoes duplicadas
 * em transcode_lz4_mt.h, decode_lz4.h e transcode.c.
 */

#ifndef FRAME_TYPES_H
#define FRAME_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <libavutil/buffer.h>

/**
 * @brief Header do frame no arquivo binario (on-disk, 20 bytes)
 *
 * Escrito antes de cada frame comprimido no arquivo de saida.
 * Compativel com decode_lz4 para round-trip.
 */
typedef struct {
    int32_t size_decompress;  /**< Tamanho original dos dados (YUV) */
    int32_t size_compress;    /**< Tamanho apos compressao (LZ4) */
    int32_t width;            /**< Largura do frame */
    int32_t height;           /**< Altura do frame */
    int32_t pix_fmt;          /**< Pixel format (AVPixelFormat) */
} FrameHeader;

/**
 * @brief Item unificado para filas de encode E decode
 *
 * Encode: data = AVBufferRef* apontando para pixels YUV brutos
 * Decode: data = AVBufferRef* apontando para dados LZ4 comprimidos
 *
 * O consumidor sabe qual campo de header usar como tamanho:
 * - Encode: header.size_decompress (tamanho do YUV)
 * - Decode: header.size_compress (tamanho dos dados comprimidos)
 */
typedef struct {
    FrameHeader header;       /**< Metadata (tamanho, dimensoes, formato) */
    int sequence_number;      /**< Ordem do frame (para escrita sequencial) */
    int64_t timestamp;        /**< av_gettime() no momento da leitura/decodificacao */
    AVBufferRef *data;        /**< YUV bruto (encode) ou dados comprimidos LZ4 (decode) */
} FrameItem;

/**
 * @brief Libera um FrameItem e seu AVBufferRef
 *
 * Funcao de callback para Queue.free_func.
 */
void frame_item_free(void *item);

#ifdef __cplusplus
}
#endif

#endif /* FRAME_TYPES_H */
