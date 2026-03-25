#include "decode_lz4.h"
#include "../cpu_stats.h"
#include <stdio.h>

void stats_print_frame(const char *profile, int decoder_threads, int encoder_threads, int64_t decode_time_us) {
    printf("%s,%d,%d,decoding,%ld\n", profile, decoder_threads, encoder_threads, decode_time_us);
}

void stats_print_summary(const char *profile, int decoder_threads, int encoder_threads, int frames, int64_t total_time_us) {
    printf("%s,%d,%d,total,%ld\n", profile, decoder_threads, encoder_threads, total_time_us);
    double fps = (frames * 1e6) / (double)total_time_us;
    printf("%s,%d,%d,fps,%lf\n", profile, decoder_threads, encoder_threads, fps);
}

void stats_print_cpu(const char *profile, int decoder_threads, int encoder_threads, double cpu_usage) {
    printf("%s,%d,%d,cpu,%lf\n", profile, decoder_threads, encoder_threads, cpu_usage);
}
