#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern void *asm_memcpy(void *dest, const void *src, size_t n);

#define MIB (1024ULL * 1024ULL)
#define SIZE (64ULL * MIB)
#define RUNS 12
#define INNER 8

static volatile unsigned char sink;

static double now_seconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static double measure(void *(*copy_fn)(void *, const void *, size_t),
                      unsigned char *dst, const unsigned char *src,
                      size_t n, int inner) {
    double start = now_seconds();
    for (int i = 0; i < inner; ++i) {
        copy_fn(dst, src, n);
        sink ^= dst[(size_t)i % n]; 
    }
    return now_seconds() - start;
}

static int compare_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

int main(void) {
    unsigned char *src = NULL, *dst = NULL;
    if (posix_memalign((void **)&src, 64, SIZE) ||
        posix_memalign((void **)&dst, 64, SIZE)) {
        fprintf(stderr, "Не удалось выделить память\n");
        free(src); free(dst);
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < SIZE; ++i) src[i] = (unsigned char)(i * 31u + 7u);
    memset(dst, 0, SIZE);

    double asm_times[RUNS], libc_times[RUNS];

    asm_memcpy(dst, src, SIZE);
    memcpy(dst, src, SIZE);

    printf("Размер одного копирования: %llu MiB\n",
           (unsigned long long)(SIZE / MIB));
    printf("Повторов в одном прогоне: %d; прогонов: %d\n\n", INNER, RUNS);
    puts("Прогон | asm_memcpy, мс | libc memcpy, мс");

    for (int r = 0; r < RUNS; ++r) {    
        double a, l;
        if ((r & 1) == 0) {
            a = measure(asm_memcpy, dst, src, SIZE, INNER);
            l = measure(memcpy, dst, src, SIZE, INNER);
        } else {
            l = measure(memcpy, dst, src, SIZE, INNER);
            a = measure(asm_memcpy, dst, src, SIZE, INNER);
        }
        asm_times[r] = a * 1000.0 / INNER;
        libc_times[r] = l * 1000.0 / INNER;
        printf("%6d | %14.3f | %15.3f\n", r + 1, asm_times[r], libc_times[r]);
    }

    qsort(asm_times, RUNS, sizeof(double), compare_double);
    qsort(libc_times, RUNS, sizeof(double), compare_double);
    printf("\nМедиана времени на копирование %llu MiB:\n",
           (unsigned long long)(SIZE / MIB));
    printf("asm_memcpy: %.3f мс\n", asm_times[RUNS / 2]);
    printf("libc memcpy: %.3f мс\n", libc_times[RUNS / 2]);
    printf("sink=%u\n", (unsigned)sink);

    free(src);
    free(dst);
    return 0;
}
