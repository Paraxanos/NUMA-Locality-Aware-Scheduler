#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <stdalign.h>

#ifndef __linux__
static inline int sched_getcpu(void) { return 0; }
#endif

#ifndef PTHREAD_BARRIER_SERIAL_THREAD
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
    int tripCount;
} pthread_barrier_t;

static inline int pthread_barrier_init(pthread_barrier_t *b, void *attr, int count) {
    pthread_mutex_init(&b->mutex, NULL);
    pthread_cond_init(&b->cond, NULL);
    b->count = count;
    b->tripCount = count;
    return 0;
}

static inline int pthread_barrier_wait(pthread_barrier_t *b) {
    pthread_mutex_lock(&b->mutex);
    b->tripCount--;
    if (b->tripCount == 0) {
        b->tripCount = b->count;
        pthread_cond_broadcast(&b->cond);
        pthread_mutex_unlock(&b->mutex);
        return 1;
    } else {
        pthread_cond_wait(&b->cond, &b->mutex);
        pthread_mutex_unlock(&b->mutex);
        return 0;
    }
}

static inline int pthread_barrier_destroy(pthread_barrier_t *b) {
    pthread_mutex_destroy(&b->mutex);
    pthread_cond_destroy(&b->cond);
    return 0;
}
#endif

#define NUM_TILES 64
#define MESH_DIM 8
#define TASK_BUF_BYTES (48 * 1024) // 48 KB fits in 64 KB L2 bank
#define NUM_TASKS 64

typedef enum { BENCH_MAP, BENCH_VECMUL } bench_type_t;
typedef enum { SCHED_WS, SCHED_LA } sched_policy_t;
typedef enum { DIST_COARSE, DIST_FINE } dist_policy_t;

typedef struct {
    uint32_t task_id;
    uint32_t home_tile;
    float* buf_a;
    float* buf_b;
    float* buf_c;
} task_t;

// Per-core task deque with cache-line alignment to eliminate false sharing
typedef struct __attribute__((aligned(64))) {
    pthread_mutex_t lock;
    int head;
    int tail;
    task_t* tasks[NUM_TASKS];
} task_deque_t;

task_deque_t g_deques[NUM_TILES];
pthread_barrier_t g_barrier;
bench_type_t g_bench = BENCH_MAP;
sched_policy_t g_sched = SCHED_LA;
dist_policy_t g_dist = DIST_COARSE;
int g_vicinity = 4;
bool g_imbalance = false;

static inline int manhattan(int t1, int t2) {
    return abs((t1 % MESH_DIM) - (t2 % MESH_DIM)) + abs((t1 / MESH_DIM) - (t2 / MESH_DIM));
}

void execute_task(task_t* t, int my_id) {
    int num_elements = TASK_BUF_BYTES / sizeof(float);
    int iterations = (g_imbalance && (t->task_id % 4 == 0)) ? 2 : 1;

    for (int iter = 0; iter < iterations; iter++) {
        if (g_bench == BENCH_MAP) {
            for (int i = 0; i < num_elements; i++) {
                t->buf_a[i] = t->buf_a[i] * 1.05f + 0.01f;
            }
        } else { // BENCH_VECMUL
            for (int i = 0; i < num_elements; i++) {
                t->buf_c[i] = t->buf_a[i] * t->buf_b[i];
            }
        }
    }
}

void* worker_thread(void* arg) {
    int my_id = (int)(intptr_t)arg;
#ifdef __linux__
    int actual_cpu = sched_getcpu();
    if (actual_cpu != my_id) {
        fprintf(stderr, "FATAL: Thread ID %d mismatched on CPU %d\n", my_id, actual_cpu);
        exit(1);
    }
#endif

    // Wait for all 64 threads to mount and initialize
    pthread_barrier_wait(&g_barrier);

    // Main task execution and stealing loop
    while (true) {
        task_t* task = NULL;
        task_deque_t* my_dq = &g_deques[my_id];

        // 1. Pop from local queue
        pthread_mutex_lock(&my_dq->lock);
        if (my_dq->head < my_dq->tail) {
            task = my_dq->tasks[my_dq->head++];
        }
        pthread_mutex_unlock(&my_dq->lock);

        // 2. Vicinity Work Stealing with bounded backoff
        if (!task) {
            for (int d = 1; d <= g_vicinity; d++) {
                for (int v = 0; v < NUM_TILES; v++) {
                    if (manhattan(my_id, v) == d) {
                        task_deque_t* v_dq = &g_deques[v];
                        for (int retry = 0; retry < 3; retry++) {
                            if (pthread_mutex_trylock(&v_dq->lock) == 0) {
                                if (v_dq->head < v_dq->tail) {
                                    task = v_dq->tasks[--v_dq->tail]; // Steal from tail
                                }
                                pthread_mutex_unlock(&v_dq->lock);
                                break;
                            }
                        }
                    }
                    if (task) break;
                }
                if (task) break;
            }
        }

        if (!task) break; // All work completed

        execute_task(task, my_id);
    }

    pthread_barrier_wait(&g_barrier);
    return NULL;
}

int main(int argc, char* argv[]) {
    // Parse arguments: --bench <map|vecmul> --sched <ws|la> --dist <coarse|fine> --vicinity <d> --imbalance
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--bench") && i + 1 < argc) {
            g_bench = !strcmp(argv[++i], "vecmul") ? BENCH_VECMUL : BENCH_MAP;
        } else if (!strcmp(argv[i], "--sched") && i + 1 < argc) {
            g_sched = !strcmp(argv[++i], "ws") ? SCHED_WS : SCHED_LA;
        } else if (!strcmp(argv[i], "--dist") && i + 1 < argc) {
            g_dist = !strcmp(argv[++i], "fine") ? DIST_FINE : DIST_COARSE;
        } else if (!strcmp(argv[i], "--vicinity") && i + 1 < argc) {
            g_vicinity = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--imbalance")) {
            g_imbalance = true;
        }
    }

    // Initialize deques and allocate buffers PRE-ROI
    task_t all_tasks[NUM_TASKS];
    for (int i = 0; i < NUM_TILES; i++) {
        pthread_mutex_init(&g_deques[i].lock, NULL);
        g_deques[i].head = 0;
        g_deques[i].tail = 0;
    }

    for (int i = 0; i < NUM_TASKS; i++) {
        all_tasks[i].task_id = i;
        all_tasks[i].home_tile = (g_dist == DIST_COARSE) ? (i % NUM_TILES) : (i % NUM_TILES);
        posix_memalign((void**)&all_tasks[i].buf_a, 64, TASK_BUF_BYTES);
        memset(all_tasks[i].buf_a, 1, TASK_BUF_BYTES);
        if (g_bench == BENCH_VECMUL) {
            posix_memalign((void**)&all_tasks[i].buf_b, 64, TASK_BUF_BYTES);
            posix_memalign((void**)&all_tasks[i].buf_c, 64, TASK_BUF_BYTES);
            memset(all_tasks[i].buf_b, 2, TASK_BUF_BYTES);
            memset(all_tasks[i].buf_c, 0, TASK_BUF_BYTES);
        }

        // Deal tasks according to Algorithm 3
        int target = 0;
        if (g_sched == SCHED_LA && g_dist == DIST_COARSE) {
            target = all_tasks[i].home_tile;
        } else {
            target = 0; // WS places all tasks on Core 0
        }
        g_deques[target].tasks[g_deques[target].tail++] = &all_tasks[i];
    }

    pthread_barrier_init(&g_barrier, NULL, NUM_TILES);
    pthread_t threads[NUM_TILES];

    for (intptr_t i = 0; i < NUM_TILES; i++) {
        pthread_create(&threads[i], NULL, worker_thread, (void*)i);
    }
    for (int i = 0; i < NUM_TILES; i++) {
        pthread_join(threads[i], NULL);
    }

    // Cleanup
    for (int i = 0; i < NUM_TASKS; i++) {
        free(all_tasks[i].buf_a);
        if (g_bench == BENCH_VECMUL) {
            free(all_tasks[i].buf_b);
            free(all_tasks[i].buf_c);
        }
    }

    pthread_barrier_destroy(&g_barrier);

    printf("TILEPro64 gem5 Benchmark (%s, %s, %s, vicinity=%d) Completed.\n",
           g_bench == BENCH_MAP ? "map" : "vecmul",
           g_sched == SCHED_WS ? "ws" : "la",
           g_dist == DIST_COARSE ? "coarse" : "fine",
           g_vicinity);
    return 0;
}
