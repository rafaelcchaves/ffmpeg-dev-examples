#include <stdio.h>
#include <libavcodec/avcodec.h>

//gcc -o codec_capabilities codec_capabilities.c -lavcodec -lavutil

void print_codec_capabilities(const AVCodec *codec) {
    if (codec) {
        printf("Codec: %s\n", codec->name);
        printf("Long name: %s\n", codec->long_name);
	printf("Encoder: %d\n", av_codec_is_encoder(codec));
	printf("Decoder: %d\n", av_codec_is_decoder(codec));
        printf("Capabilities:\n");

        if (codec->capabilities & AV_CODEC_CAP_DRAW_HORIZ_BAND) {
            printf(" - AV_CODEC_CAP_DRAW_HORIZ_BAND\n");
        }
        if (codec->capabilities & AV_CODEC_CAP_DR1) {
            printf(" - AV_CODEC_CAP_DR1\n");
        }
        if (codec->capabilities & AV_CODEC_CAP_TRUNCATED) {
            printf(" - AV_CODEC_CAP_TRUNCATED\n");
        }
        if (codec->capabilities & AV_CODEC_CAP_DELAY) {
            printf(" - AV_CODEC_CAP_DELAY\n");
        }
        if (codec->capabilities & AV_CODEC_CAP_SMALL_LAST_FRAME) {
            printf(" - AV_CODEC_CAP_SMALL_LAST_FRAME\n");
        }
        if (codec->capabilities & AV_CODEC_CAP_SUBFRAMES) {
            printf(" - AV_CODEC_CAP_SUBFRAMES\n");
        }
        if (codec->capabilities & AV_CODEC_CAP_EXPERIMENTAL) {
            printf(" - AV_CODEC_CAP_EXPERIMENTAL\n");
        }
        if (codec->capabilities & AV_CODEC_CAP_CHANNEL_CONF) {
            printf(" - AV_CODEC_CAP_CHANNEL_CONF\n");
        }
        if (codec->capabilities & AV_CODEC_CAP_FRAME_THREADS) {
            printf(" - AV_CODEC_CAP_FRAME_THREADS\n");
        }
        if (codec->capabilities & AV_CODEC_CAP_SLICE_THREADS) {
            printf(" - AV_CODEC_CAP_SLICE_THREADS\n");
        }
        if (codec->capabilities & AV_CODEC_CAP_PARAM_CHANGE) {
            printf(" - AV_CODEC_CAP_PARAM_CHANGE\n");
        }
        if (codec->capabilities & AV_CODEC_CAP_AUTO_THREADS) {
            printf(" - AV_CODEC_CAP_AUTO_THREADS\n");
        }
        if (codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) {
            printf(" - AV_CODEC_CAP_VARIABLE_FRAME_SIZE\n");
        }
        if (codec->capabilities & AV_CODEC_CAP_AVOID_PROBING) {
            printf(" - AV_CODEC_CAP_AVOID_PROBING\n");
        }
        if (codec->capabilities & AV_CODEC_CAP_INTRA_ONLY) {
            printf(" - AV_CODEC_CAP_INTRA_ONLY\n");
        }
        if (codec->capabilities & AV_CODEC_CAP_LOSSLESS) {
            printf(" - AV_CODEC_CAP_LOSSLESS\n");
        }

    } else {
        printf("Codec not found.\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <codec_name>\n", argv[0]);
        return 1;
    }

    const char *codec_name = argv[1];
    const AVCodec *codec = NULL;

    codec = avcodec_find_encoder_by_name(codec_name);
    // Find the encoder
    if (codec) {
        print_codec_capabilities(codec);
    } else {
        // If not found as an encoder, try to find it as a decoder
	codec = avcodec_find_decoder_by_name(codec_name);
        if (codec) {
            print_codec_capabilities(codec);
        } else {
            fprintf(stderr, "Codec '%s' not found.\n", codec_name);
            return 1;
        }
    }

    return 0;
}
