extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
#include <lz4.h>
}

#include "decode_lz4.h"
#include <stdlib.h>
#include <stdio.h>

int decode_context_init(DecodeContext *ctx) {
    ctx->image_data[0] = NULL;
    ctx->compressed_frame = NULL;
    ctx->image_bufsize = 0;
    ctx->max_capacity = 0;
    return 0;
}

int decode_context_alloc_buffers(DecodeContext *ctx, const FrameHeader *header) {
    int ret = av_image_alloc(ctx->image_data, ctx->image_linesize,
                             header->width, header->height,
                             (AVPixelFormat)header->pix_fmt, 1);
    if (ret < 0) {
        fprintf(stderr, "Nao foi possivel alocar o buffer de video decodificado\n");
        return -1;
    }
    ctx->image_bufsize = ret;

    ctx->max_capacity = LZ4_compressBound(header->size_decompress);

    ctx->compressed_frame = (char*)malloc((size_t)ctx->max_capacity);
    if (!ctx->compressed_frame) {
        fprintf(stderr, "Error: Could not allocate compressed buffer\n");
        av_free(ctx->image_data[0]);
        return -1;
    }

    return 0;
}

void decode_context_free(DecodeContext *ctx) {
    if (ctx->compressed_frame) {
        free(ctx->compressed_frame);
        ctx->compressed_frame = NULL;
    }
    if (ctx->image_data[0]) {
        av_free(ctx->image_data[0]);
        ctx->image_data[0] = NULL;
    }
}

int decode_single_thread(DecodeContext *ctx, const char *compressed, int comp_size, const FrameHeader *hdr) {
    int decompressedSize = LZ4_decompress_safe(compressed, (char*)(ctx->image_data[0]),
                                               comp_size, ctx->max_capacity);
    if (decompressedSize < 0) {
        fprintf(stderr, "Error: Decompress failed\n");
        return -1;
    }
    return decompressedSize;
}
