#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sched.h>
#ifndef __linux__
static inline int sched_getcpu(void) { return 0; }
#endif
#include <pthread.h>
#if defined(__x86_64__) || defined(_M_X64)
#include <x86intrin.h>
#define FLUSH_LINE(p) do { _mm_clflush((const void*)(p)); _mm_mfence(); } while(0)
#define READ_CYCLES() __rdtsc()
#else
#define FLUSH_LINE(p) __builtin_prefetch((const void*)(p), 0, 0)
static inline uint64_t READ_CYCLES(void) {
    uint64_t val;
#if defined(__aarch64__)
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(val));
#else
    val = 0;
#endif
    return val;
}
#endif

#define NUM_TILES 64
#define MESH_DIM 8
#define BANK_STRIDE (64 * 1024) // 64 KB stride aligns with bank select bits [21:16]

static inline uint32_t manhattan(int t1, int t2) {
    return abs((t1 % MESH_DIM) - (t2 % MESH_DIM)) + abs((t1 / MESH_DIM) - (t2 / MESH_DIM));
}

int main(int argc, char* argv[]) {
    printf("=== TILEPro64 gem5 Latency Calibration Microbenchmark ===\n");
#ifdef __linux__
    int my_cpu = sched_getcpu();
    if (my_cpu != 0) {
        fprintf(stderr, "Calibration must be launched on Core 0 (got %d)\n", my_cpu);
        return 1;
    }
#endif

    // Allocate 64 KB * 64 tiles = 4 MB contiguous buffer
    void* raw_mem;
    int err = posix_memalign(&raw_mem, BANK_STRIDE, NUM_TILES * BANK_STRIDE);
    if (err != 0 || !raw_mem) {
        perror("posix_memalign failed");
        return 1;
    }
    uint8_t* base_ptr = (uint8_t*)raw_mem;

    // Initialize all banks sequentially to establish clean home-cache residency
    for (int t = 0; t < NUM_TILES; t++) {
        volatile uint64_t* line = (volatile uint64_t*)(base_ptr + t * BANK_STRIDE);
        *line = 0xA5A5A5A5ULL;
    }

    printf("Target_Tile,Hops,Measured_Cycles,Expected_Cycles\n");

    // Measure point-to-point round-trip latency from Core 0 to Bank T
    for (int t = 0; t < NUM_TILES; t++) {
        volatile uint64_t* target_line = (volatile uint64_t*)(base_ptr + t * BANK_STRIDE);
        uint32_t hops = manhattan(0, t);
        uint32_t expected = (hops == 0) ? 10 : (38 + 2 * hops);

        // Flush target line from local L1D
        FLUSH_LINE(target_line);

        uint64_t t0 = READ_CYCLES();
        volatile uint64_t val = *target_line;
        uint64_t t1 = READ_CYCLES();
        (void)val;

        uint64_t measured = (t1 > t0) ? (t1 - t0) : 0;
        printf("%d,%u,%llu,%u\n", t, hops, (unsigned long long)measured, expected);
    }

    free(raw_mem);
    return 0;
}
