#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>

#include "mir_task.h"
#include "mir_task_queue.h"
#include "mir_mem_pol.h"
#include "mir_arch_sim.h"

#include "../patches/nova/nova_types.h"
#include "../patches/nova/nova_shadow_index.h"
#include "../patches/nova/nova_coordinator.h"
#include "../patches/nova/nova_cost.h"
#include "../patches/nova/nova_dealer.h"
#include "../patches/nova/nova_stealer.h"

#define TASK_QUEUE_FULL(queue) ((queue)->size == (queue)->capacity)
#define TASK_QUEUE_EMPTY(queue) ((queue)->size == 0)

static inline void mir_lock_create(struct mir_lock_t* lock) { pthread_mutex_init(&lock->m, NULL); }
static inline void mir_lock_destroy(struct mir_lock_t* lock) { pthread_mutex_destroy(&lock->m); }
static inline void mir_lock_set(struct mir_lock_t* lock) { pthread_mutex_lock(&lock->m); }
static inline void mir_lock_unset(struct mir_lock_t* lock) { pthread_mutex_unlock(&lock->m); }

void* mir_task_queue_create(uint32_t capacity) {
    struct mir_task_queue_t* q = calloc(1, sizeof(struct mir_task_queue_t));
    q->buffer = calloc(capacity, sizeof(struct mir_task_t*));
    q->capacity = capacity;
    q->size = 0;
    q->in = 0;
    q->out = 0;
    mir_lock_create(&q->enq_lock);
    mir_lock_create(&q->deq_lock);
    return q;
}

void mir_task_queue_destroy(struct mir_task_queue_t* q) {
    if (!q) return;
    mir_lock_destroy(&q->enq_lock);
    mir_lock_destroy(&q->deq_lock);
    free(q->buffer);
    free(q);
}

int mir_task_queue_push(struct mir_task_queue_t* queue, struct mir_task_t* task) {
    mir_lock_set(&queue->enq_lock);
    if (TASK_QUEUE_FULL(queue)) {
        mir_lock_unset(&queue->enq_lock);
        return 0;
    }
    queue->buffer[queue->in] = task;
    queue->in = (queue->in + 1) % queue->capacity;
    __sync_fetch_and_add(&queue->size, 1);
    mir_lock_unset(&queue->enq_lock);
    return 1;
}

struct mir_task_t* mir_task_queue_pop(struct mir_task_queue_t* queue) {
    if (!queue) return NULL;
    mir_lock_set(&queue->deq_lock);
    struct mir_task_t* task = NULL;

    while (!TASK_QUEUE_EMPTY(queue)) {
        struct mir_task_t* cand = queue->buffer[queue->out];
        queue->buffer[queue->out] = NULL;
        queue->out = (queue->out + 1) % queue->capacity;
        __sync_fetch_and_sub(&queue->size, 1);

        if (cand != NULL) {
            if (__sync_bool_compare_and_swap(&cand->taken, 0, 1)) {
                task = cand;
                break;
            }
        }
    }

    mir_lock_unset(&queue->deq_lock);
    return task;
}

int mir_task_queue_lock_and_remove(struct mir_task_queue_t* queue, struct mir_task_t* task) {
    if (!queue || !task) return 0;

    mir_lock_set(&queue->deq_lock);
    mir_lock_set(&queue->enq_lock);

    if (TASK_QUEUE_EMPTY(queue)) {
        mir_lock_unset(&queue->enq_lock);
        mir_lock_unset(&queue->deq_lock);
        return 0;
    }

    int found = 0;
    uint32_t found_offset = 0;

    for (uint32_t i = 0; i < queue->size; i++) {
        uint32_t idx = (queue->out + i) % queue->capacity;
        if (queue->buffer[idx] == task) {
            found = 1;
            found_offset = i;
            break;
        }
    }

    if (!found) {
        mir_lock_unset(&queue->enq_lock);
        mir_lock_unset(&queue->deq_lock);
        return 0;
    }

    if (!__sync_bool_compare_and_swap(&task->taken, 0, 1)) {
        for (uint32_t j = found_offset; j < queue->size - 1; j++) {
            uint32_t dst = (queue->out + j) % queue->capacity;
            uint32_t src = (queue->out + j + 1) % queue->capacity;
            queue->buffer[dst] = queue->buffer[src];
        }
        uint32_t last = (queue->out + queue->size - 1) % queue->capacity;
        queue->buffer[last] = NULL;
        queue->in = (queue->in + queue->capacity - 1) % queue->capacity;
        __sync_fetch_and_sub(&queue->size, 1);

        mir_lock_unset(&queue->enq_lock);
        mir_lock_unset(&queue->deq_lock);
        return 0;
    }

    for (uint32_t j = found_offset; j < queue->size - 1; j++) {
        uint32_t dst = (queue->out + j) % queue->capacity;
        uint32_t src = (queue->out + j + 1) % queue->capacity;
        queue->buffer[dst] = queue->buffer[src];
    }
    uint32_t last = (queue->out + queue->size - 1) % queue->capacity;
    queue->buffer[last] = NULL;
    queue->in = (queue->in + queue->capacity - 1) % queue->capacity;
    __sync_fetch_and_sub(&queue->size, 1);

    mir_lock_unset(&queue->enq_lock);
    mir_lock_unset(&queue->deq_lock);
    return 1;
}

struct mir_task_t* mir_task_queue_pop_end(struct mir_task_queue_t* queue) {
    if (!queue) return NULL;

    mir_lock_set(&queue->deq_lock);
    mir_lock_set(&queue->enq_lock);

    struct mir_task_t* task = NULL;

    while (queue->size > 0) {
        uint32_t last_idx = (queue->in + queue->capacity - 1) % queue->capacity;
        struct mir_task_t* cand = queue->buffer[last_idx];
        queue->buffer[last_idx] = NULL;
        queue->in = last_idx;
        __sync_fetch_and_sub(&queue->size, 1);

        if (cand != NULL) {
            if (__sync_bool_compare_and_swap(&cand->taken, 0, 1)) {
                task = cand;
                break;
            }
        }
    }

    mir_lock_unset(&queue->enq_lock);
    mir_lock_unset(&queue->deq_lock);
    return task;
}

uint32_t mir_task_queue_size(const struct mir_task_queue_t* queue) {
    return queue ? queue->size : 0;
}

// Global simulated test state
#define NUM_TEST_NODES 8
#define TOTAL_TASKS 40000

static struct mir_task_queue_t* g_test_queues[NUM_TEST_NODES];
static struct alloc_coordinator_t g_test_coord;
static struct shadow_index_t g_test_shadow[NUM_TEST_NODES];

struct mir_task_queue_t* mir_get_node_task_queue(uint16_t node_id) {
    if (node_id >= NUM_TEST_NODES) return NULL;
    return g_test_queues[node_id];
}

uint32_t mir_get_node_task_queue_size(uint16_t node_id) {
    return mir_task_queue_size(mir_get_node_task_queue(node_id));
}

// Execution verification bitmap
static _Atomic uint32_t g_task_exec_count[TOTAL_TASKS];
static _Atomic uint32_t g_tasks_remaining;
static _Atomic uint32_t g_dealers_done;

typedef struct {
    int dealer_id;
    int start_id;
    int end_id;
} dealer_arg_t;

static void* dealer_thread(void* ptr) {
    dealer_arg_t* arg = (dealer_arg_t*)ptr;
    for (int i = arg->start_id; i < arg->end_id; i++) {
        struct mir_task_t* t = malloc(sizeof(struct mir_task_t));
        t->id = i;
        t->taken = 0;
        t->done = 0;
        t->num_data_footprints = 0;
        t->dist_by_access_type[0] = NULL;
        t->dist_by_access_type[1] = NULL;

        uint16_t target_node = nova_deal(&g_test_coord, g_test_shadow, t, arg->dealer_id, 20*1024*1024);
        while (!mir_task_queue_push(g_test_queues[target_node], t)) {
            usleep(10);
        }
    }
    atomic_fetch_add(&g_dealers_done, 1);
    return NULL;
}

typedef struct {
    int worker_id;
    int local_node;
    uint32_t owned_count;
    uint32_t stolen_count;
} worker_arg_t;

static void* worker_thread(void* ptr) {
    worker_arg_t* arg = (worker_arg_t*)ptr;
    while (atomic_load(&g_tasks_remaining) > 0) {
        // 1. Pop from local queue
        struct mir_task_t* task = mir_task_queue_pop(g_test_queues[arg->local_node]);
        if (task) {
            arg->owned_count++;
            uint32_t execs = atomic_fetch_add(&g_task_exec_count[task->id], 1);
            assert(execs == 0 && "CRITICAL: Task executed more than once!");
            atomic_fetch_sub(&g_tasks_remaining, 1);
            free(task);
            continue;
        }

        // 2. Steal if local queue is empty
        task = nova_steal(&g_test_coord, g_test_shadow, arg->worker_id);
        if (task) {
            arg->stolen_count++;
            uint32_t execs = atomic_fetch_add(&g_task_exec_count[task->id], 1);
            assert(execs == 0 && "CRITICAL: Stolen task executed more than once!");
            atomic_fetch_sub(&g_tasks_remaining, 1);
            free(task);
            continue;
        }

        usleep(50);
    }
    return NULL;
}

static void run_stress_test(bool disable_shadow_index) {
    printf("--- Running Concurrency Stress Test (%d tasks, %s) ---\n",
           TOTAL_TASKS, disable_shadow_index ? "SHADOW INDEX DISABLED (Ablation)" : "SHADOW INDEX ENABLED");

    nova_shadow_set_disabled(disable_shadow_index);
    mir_arch_sim_init(NUM_TEST_NODES);
    nova_coordinator_init(&g_test_coord);

    for (int i = 0; i < NUM_TEST_NODES; i++) {
        g_test_queues[i] = mir_task_queue_create(1024);
        nova_shadow_init(&g_test_shadow[i]);
    }

    memset((void*)g_task_exec_count, 0, sizeof(g_task_exec_count));
    atomic_init(&g_tasks_remaining, TOTAL_TASKS);
    atomic_init(&g_dealers_done, 0);

    #define NUM_DEALERS 4
    #define NUM_WORKERS 8

    pthread_t dealers[NUM_DEALERS];
    dealer_arg_t dealer_args[NUM_DEALERS];
    int per_dealer = TOTAL_TASKS / NUM_DEALERS;

    pthread_t workers[NUM_WORKERS];
    worker_arg_t worker_args[NUM_WORKERS];

    for (int w = 0; w < NUM_WORKERS; w++) {
        worker_args[w].worker_id = w;
        worker_args[w].local_node = w % NUM_TEST_NODES;
        worker_args[w].owned_count = 0;
        worker_args[w].stolen_count = 0;
        pthread_create(&workers[w], NULL, worker_thread, &worker_args[w]);
    }

    for (int d = 0; d < NUM_DEALERS; d++) {
        dealer_args[d].dealer_id = d;
        dealer_args[d].start_id = d * per_dealer;
        dealer_args[d].end_id = (d + 1) * per_dealer;
        pthread_create(&dealers[d], NULL, dealer_thread, &dealer_args[d]);
    }

    for (int d = 0; d < NUM_DEALERS; d++) {
        pthread_join(dealers[d], NULL);
    }
    for (int w = 0; w < NUM_WORKERS; w++) {
        pthread_join(workers[w], NULL);
    }

    // Verification
    uint32_t total_executed = 0;
    uint32_t duplicates = 0;
    uint32_t lost = 0;

    for (int i = 0; i < TOTAL_TASKS; i++) {
        uint32_t cnt = atomic_load(&g_task_exec_count[i]);
        if (cnt == 1) total_executed++;
        else if (cnt > 1) duplicates++;
        else lost++;
    }

    uint32_t total_owned = 0, total_stolen = 0;
    for (int w = 0; w < NUM_WORKERS; w++) {
        total_owned += worker_args[w].owned_count;
        total_stolen += worker_args[w].stolen_count;
    }

    printf("  Results:\n");
    printf("    Tasks Executed: %u / %d\n", total_executed, TOTAL_TASKS);
    printf("    Local Owned:    %u (%.1f%%)\n", total_owned, (100.0 * total_owned) / TOTAL_TASKS);
    printf("    Stolen:         %u (%.1f%%)\n", total_stolen, (100.0 * total_stolen) / TOTAL_TASKS);
    printf("    Duplicates:     %u\n", duplicates);
    printf("    Lost:           %u\n", lost);

    assert(total_executed == TOTAL_TASKS && "FATAL: Total executed count mismatch!");
    assert(duplicates == 0 && "FATAL: Duplicate task executions detected!");
    assert(lost == 0 && "FATAL: Tasks lost during scheduling/stealing!");
    printf("  --> PASSED: 100%% exact-once execution verified.\n\n");

    for (int i = 0; i < NUM_TEST_NODES; i++) {
        mir_task_queue_destroy(g_test_queues[i]);
        nova_shadow_destroy(&g_test_shadow[i]);
    }
    nova_coordinator_destroy(&g_test_coord);
}

int main(void) {
    printf("=================================================================\n");
    printf("NOVA Full Concurrency & Scheduling Stress Test\n");
    printf("=================================================================\n\n");

    run_stress_test(false); // With Shadow Index
    run_stress_test(true);  // With Shadow Index disabled (Ablation test)

    printf("ALL CONCURRENCY STRESS TESTS COMPLETED SUCCESSFULLY!\n");
    return 0;
}
