#include "decode_lz4.h"
#include <stdio.h>

void stats_print_frame(const char *profile, int threads, int64_t decode_time_us) {
    printf("%s,%d,0,decoding,%ld\n", profile, threads, decode_time_us);
}

void stats_print_summary(const char *profile, int threads, int frames, int64_t total_time_us) {
    printf("%s,%d,0,total,%ld\n", profile, threads, total_time_us);
    double fps = (frames * 1e6) / (double)total_time_us;
    printf("%s,%d,0,fps,%lf\n", profile, threads, fps);
}
