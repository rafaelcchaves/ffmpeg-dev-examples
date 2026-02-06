extern "C" {
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
#include <lz4hc.h>
#include <lz4.h>
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef THREADS_IN
#define THREADS_IN 0
#endif

int frames;
const char *profile_name = NULL;
char *compressedFrame = NULL;
int maxCapacity, decompressedSize;

uint8_t *image_data[4];
int image_linesize[4];
int image_bufsize;

typedef struct {
    int32_t size_decompress;
    int32_t size_compress;
    int32_t width, height;
    int32_t pix_fmt;
} headerFile;

int main(int argc, char** argv){
    const char *infilename = NULL, *outfilename = NULL;
    FILE *output;
    int opt;

    while ((opt = getopt(argc, argv, "i:o:p:")) != -1) {
        switch (opt) {
            case 'i':
                infilename = optarg;
                break;
            case 'o':
                outfilename = optarg;
                break;
            case 'p':
                profile_name = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s -i <input-file> -o <output-file> -p <profile>\n", argv[0]);
                fprintf(stderr, "  -i  Input LZ compressed file\n");
                fprintf(stderr, "  -o  Output YUV file\n");
                fprintf(stderr, "  -p  Profile name (e.g., low_latency)\n");
                exit(1);
        }
    }
    if (infilename == NULL || outfilename == NULL || profile_name == NULL) {
        fprintf(stderr, "Usage: %s -i <input-file> -o <output-file> -p <profile>\n", argv[0]);
        fprintf(stderr, "  -i  Input LZ compressed file\n");
        fprintf(stderr, "  -o  Output YUV file\n");
        fprintf(stderr, "  -p  Profile name (e.g., low_latency)\n");
        exit(1);
    }
    output = fopen(outfilename, "wb");
    if (!output) {
        fprintf(stderr, "Could not open %s\n", outfilename);
        exit(1);
    }

    int allocated = 0;
    FILE *infile = fopen(infilename, "rb");
    if (!infile) {
        fprintf(stderr, "Could not open %s\n", infilename);
        exit(1);
    }

    int64_t start_time = av_gettime();

    while(1){
        headerFile hf;
        size_t read_count = fread(&hf, sizeof(headerFile), 1, infile);
        if (read_count != 1) {
            if(feof(infile)){
                fclose(infile);
                break;
            }
            fprintf(stderr, "Erro: Arquivo muito curto ou corrompido (falha ao ler header).\n");
            fclose(infile);
            return 1;
        }

        if(!allocated){

            int ret = av_image_alloc(image_data, image_linesize, hf.width, hf.height, (AVPixelFormat)hf.pix_fmt, 1);
            if (ret < 0) {
                fprintf(stderr, "Nao foi possivel alocar o buffer de video decodificado\n");
                return 1;
            }
            image_bufsize = ret;

            maxCapacity = LZ4_compressBound(hf.size_decompress);

            if(!(compressedFrame = (char*)malloc(maxCapacity))){
                fprintf(stderr, "Error: Could not allocate compressed buffer\n");
                return 1;
            }
            allocated = 1;
        }

        read_count = fread(compressedFrame, 1, hf.size_compress, infile);

        if (read_count != (size_t)hf.size_compress) {
            fprintf(stderr, "Erro: O arquivo terminou antes do esperado.\n");
            return 1;
        }

        int64_t st = av_gettime();
        decompressedSize = LZ4_decompress_safe((const char*)compressedFrame, (char*)(image_data[0]), hf.size_compress, maxCapacity);
        printf("%s,%d,0,decoding,%ld\n", profile_name, THREADS_IN, av_gettime() - st);
        if(decompressedSize < 0){
            fprintf(stderr, "Error: Decompress failed\n");
            return 1;
        }

        frames++;

        /* write to rawvideo file */
        fwrite(image_data[0], 1, image_bufsize, output);
    }

    printf("%s,%d,0,total,%ld\n", profile_name, THREADS_IN, av_gettime() - start_time);
    printf("%s,%d,0,fps,%lf\n", profile_name, THREADS_IN, (frames*1e6)/(av_gettime() - start_time));

    fclose(output);
    free(compressedFrame);
    av_free(image_data[0]);
    return 0;
}
