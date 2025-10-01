#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <libavutil/buffer.h>

// How to build: gcc main.c -lavutil -DNUM_ELEM=100000 -DMSG_SIZE=1472  -DNUM_RUNS=10 && ./a.out

#ifndef NUM_RUNS
#define NUM_RUNS 5
#endif

#ifndef DATA_SIZE
#define DATA_SIZE 1472
#endif

#ifndef MSG_SIZE 
#define MSG_SIZE DATA_SIZE
#endif

#ifndef NUM_ELEM
#define NUM_ELEM 100000
#endif

#ifndef POOL_SIZE
#define POOL_SIZE NUM_ELEM
#endif

AVBufferRef* list[NUM_ELEM];
AVBufferPool* pool; 
char message[MSG_SIZE];

void function_to_measure() {
	for(int i = 0; i < NUM_ELEM; i++){
		if(!pool){
			list[i] = av_buffer_alloc(DATA_SIZE);
		} else {
			list[i] = av_buffer_pool_get(pool);	
		}
		memcpy(list[i]->data, message, MSG_SIZE);
	}
	for(int i = 0; i < NUM_ELEM; i++){
		av_buffer_unref(list + i);
	}
}

long long timespec_diff_ns(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
}

int main() {
    for (int i = 0; i < MSG_SIZE; i++)
    	message[i] = i;

#ifdef USE_POOL
    pool = av_buffer_pool_init(DATA_SIZE, NULL);
#endif

    printf("%d\n", INT_MAX);

    struct timespec start_time, end_time;
    long long elapsed_ns[NUM_RUNS];
    long long min_ns = LLONG_MAX;
    long long max_ns = 0;
    long long total_ns = 0;

    printf("Measuring execution time of 'function_to_measure' over %d runs...\n\n", NUM_RUNS);

    for (int i = 0; i < NUM_RUNS; ++i) {
        if (clock_gettime(CLOCK_MONOTONIC, &start_time) == -1) {
            perror("clock_gettime");
            return EXIT_FAILURE;
        }

        function_to_measure();

        if (clock_gettime(CLOCK_MONOTONIC, &end_time) == -1) {
            perror("clock_gettime");
            return EXIT_FAILURE;
        }

        elapsed_ns[i] = timespec_diff_ns(start_time, end_time);
        printf("Run %d: %.3f ms\n", i + 1, elapsed_ns[i] / 1000000.0);

        total_ns += elapsed_ns[i];

        if (elapsed_ns[i] < min_ns) {
            min_ns = elapsed_ns[i];
        }

        if (elapsed_ns[i] > max_ns) {
            max_ns = elapsed_ns[i];
        }
    }

#ifdef USE_POOL
    av_buffer_pool_uninit(&pool);
#endif

    double mean_ns = (double)total_ns / NUM_RUNS;

    printf("\n--- Measurement Summary ---\n");
    printf("Min time:  %.3f ms\n", min_ns / 1000000.0);
    printf("Max time:  %.3f ms\n", max_ns / 1000000.0);
    printf("Mean time: %.3f ms\n", mean_ns / 1000000.0);
    printf("---------------------------\n");

    return EXIT_SUCCESS;
}

