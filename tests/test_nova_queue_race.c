#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>

// Portable barrier implementation (since macOS libc lacks pthread_barrier_t)
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int threshold;
    int count;
    int cycle;
} test_barrier_t;

static void barrier_init(test_barrier_t* b, int count) {
    pthread_mutex_init(&b->lock, NULL);
    pthread_cond_init(&b->cond, NULL);
    b->threshold = count;
    b->count = count;
    b->cycle = 0;
}

static void barrier_destroy(test_barrier_t* b) {
    pthread_mutex_destroy(&b->lock);
    pthread_cond_destroy(&b->cond);
}

static void barrier_wait(test_barrier_t* b) {
    pthread_mutex_lock(&b->lock);
    int cycle = b->cycle;
    if (--b->count == 0) {
        b->cycle++;
        b->count = b->threshold;
        pthread_cond_broadcast(&b->cond);
        pthread_mutex_unlock(&b->lock);
    } else {
        while (cycle == b->cycle) {
            pthread_cond_wait(&b->cond, &b->lock);
        }
        pthread_mutex_unlock(&b->lock);
    }
}

// Minimal mock MIR types for testing the queue operations directly
struct mir_lock_t {
    pthread_mutex_t m;
};

static inline void mir_lock_create(struct mir_lock_t* lock) {
    pthread_mutex_init(&lock->m, NULL);
}

static inline void mir_lock_destroy(struct mir_lock_t* lock) {
    pthread_mutex_destroy(&lock->m);
}

static inline void mir_lock_set(struct mir_lock_t* lock) {
    pthread_mutex_lock(&lock->m);
}

static inline void mir_lock_unset(struct mir_lock_t* lock) {
    pthread_mutex_unlock(&lock->m);
}

struct mir_task_t {
    uint32_t id;
    uint32_t taken;
    uint32_t done;
};

struct mir_task_queue_t {
    struct mir_task_t** buffer;
    uint32_t capacity;
    uint32_t size;
    uint32_t in;
    uint32_t out;
    struct mir_lock_t enq_lock;
    struct mir_lock_t deq_lock;
};

#define TASK_QUEUE_FULL(queue) ((queue)->size == (queue)->capacity)
#define TASK_QUEUE_EMPTY(queue) ((queue)->size == 0)

static struct mir_task_queue_t* test_queue_create(uint32_t capacity) {
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

static void test_queue_destroy(struct mir_task_queue_t* q) {
    mir_lock_destroy(&q->enq_lock);
    mir_lock_destroy(&q->deq_lock);
    free(q->buffer);
    free(q);
}

static int test_queue_push(struct mir_task_queue_t* queue, struct mir_task_t* task) {
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

static struct mir_task_t* test_queue_pop(struct mir_task_queue_t* queue) {
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

static int test_queue_lock_and_remove(struct mir_task_queue_t* queue, struct mir_task_t* task) {
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
        // Stale pointer in queue
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

    // Successfully claimed! Shift elements
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

static struct mir_task_t* test_queue_pop_end(struct mir_task_queue_t* queue) {
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

// Test harness state
#define NUM_TRIALS 2000

typedef struct {
    struct mir_task_queue_t* q;
    struct mir_task_t* task;
    test_barrier_t* start_bar;
    test_barrier_t* end_bar;
    int thief_id;
    int claimed;
    int iterations;
} thief_arg_t;

static void* thief_func(void* ptr) {
    thief_arg_t* arg = (thief_arg_t*)ptr;
    for (int it = 0; it < arg->iterations; it++) {
        barrier_wait(arg->start_bar);
        int success = test_queue_lock_and_remove(arg->q, arg->task);
        if (success) {
            arg->claimed++;
        }
        barrier_wait(arg->end_bar);
    }
    return NULL;
}

static void test_two_thieves_race(void) {
    printf("[TEST 1] Two thieves racing for 1 task in queue (%d iterations)...\n", NUM_TRIALS);

    struct mir_task_queue_t* q = test_queue_create(64);
    struct mir_task_t task;

    test_barrier_t start_bar, end_bar;
    barrier_init(&start_bar, 3);
    barrier_init(&end_bar, 3);

    thief_arg_t arg1 = { .q = q, .task = &task, .start_bar = &start_bar, .end_bar = &end_bar, .thief_id = 1, .claimed = 0, .iterations = NUM_TRIALS };
    thief_arg_t arg2 = { .q = q, .task = &task, .start_bar = &start_bar, .end_bar = &end_bar, .thief_id = 2, .claimed = 0, .iterations = NUM_TRIALS };

    pthread_t t1, t2;
    pthread_create(&t1, NULL, thief_func, &arg1);
    pthread_create(&t2, NULL, thief_func, &arg2);

    int total_claimed = 0;
    int total_zero = 0;
    int total_double = 0;

    for (int it = 0; it < NUM_TRIALS; it++) {
        task.id = it;
        task.taken = 0;
        task.done = 0;
        test_queue_push(q, &task);
        assert(q->size == 1);

        int c1_before = arg1.claimed;
        int c2_before = arg2.claimed;

        // Release thieves
        barrier_wait(&start_bar);
        // Wait for thieves to complete attempt
        barrier_wait(&end_bar);

        int round_claims = (arg1.claimed - c1_before) + (arg2.claimed - c2_before);
        if (round_claims == 1) {
            total_claimed++;
        } else if (round_claims == 0) {
            total_zero++;
        } else {
            total_double++;
        }

        assert(q->size == 0);
        assert(task.taken == 1);
    }

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("  Results: Thief 1 claimed %d, Thief 2 claimed %d\n", arg1.claimed, arg2.claimed);
    printf("  Total single claims: %d / %d\n", total_claimed, NUM_TRIALS);
    printf("  Zero claims: %d, Double claims: %d\n", total_zero, total_double);

    assert(total_double == 0 && "FATAL: Duplicate task claim detected!");
    assert(total_zero == 0 && "FATAL: Task lost during concurrent claim!");
    assert(total_claimed == NUM_TRIALS && "FATAL: Incomplete claims!");
    printf("  --> PASSED: Mutual exclusion verified across %d races.\n\n", NUM_TRIALS);

    barrier_destroy(&start_bar);
    barrier_destroy(&end_bar);
    test_queue_destroy(q);
}

// Test 2: Local worker (pop) vs Remote thief (lock_and_remove)
typedef struct {
    struct mir_task_queue_t* q;
    struct mir_task_t* task;
    test_barrier_t* start_bar;
    test_barrier_t* end_bar;
    int worker_claimed;
    int thief_claimed;
    int iterations;
} pop_vs_steal_arg_t;

static void* worker_pop_func(void* ptr) {
    pop_vs_steal_arg_t* arg = (pop_vs_steal_arg_t*)ptr;
    for (int it = 0; it < arg->iterations; it++) {
        barrier_wait(arg->start_bar);
        struct mir_task_t* t = test_queue_pop(arg->q);
        if (t != NULL) {
            arg->worker_claimed++;
        }
        barrier_wait(arg->end_bar);
    }
    return NULL;
}

static void* thief_steal_func(void* ptr) {
    pop_vs_steal_arg_t* arg = (pop_vs_steal_arg_t*)ptr;
    for (int it = 0; it < arg->iterations; it++) {
        barrier_wait(arg->start_bar);
        int success = test_queue_lock_and_remove(arg->q, arg->task);
        if (success) {
            arg->thief_claimed++;
        }
        barrier_wait(arg->end_bar);
    }
    return NULL;
}

static void test_pop_vs_steal_race(void) {
    printf("[TEST 2] Local worker pop() vs Remote thief lock_and_remove() (%d iterations)...\n", NUM_TRIALS);

    struct mir_task_queue_t* q = test_queue_create(64);
    struct mir_task_t task;

    test_barrier_t start_bar, end_bar;
    barrier_init(&start_bar, 3);
    barrier_init(&end_bar, 3);

    pop_vs_steal_arg_t arg = {
        .q = q,
        .task = &task,
        .start_bar = &start_bar,
        .end_bar = &end_bar,
        .worker_claimed = 0,
        .thief_claimed = 0,
        .iterations = NUM_TRIALS
    };

    pthread_t tw, tt;
    pthread_create(&tw, NULL, worker_pop_func, &arg);
    pthread_create(&tt, NULL, thief_steal_func, &arg);

    int total_single = 0, total_double = 0, total_zero = 0;

    for (int it = 0; it < NUM_TRIALS; it++) {
        task.id = it;
        task.taken = 0;
        task.done = 0;
        test_queue_push(q, &task);

        int w_before = arg.worker_claimed;
        int t_before = arg.thief_claimed;

        barrier_wait(&start_bar);
        barrier_wait(&end_bar);

        int round_claims = (arg.worker_claimed - w_before) + (arg.thief_claimed - t_before);
        if (round_claims == 1) total_single++;
        else if (round_claims == 0) total_zero++;
        else total_double++;

        assert(q->size == 0);
        assert(task.taken == 1);
    }

    pthread_join(tw, NULL);
    pthread_join(tt, NULL);

    printf("  Results: Worker popped %d, Thief stole %d\n", arg.worker_claimed, arg.thief_claimed);
    printf("  Single: %d, Zero: %d, Double: %d\n", total_single, total_zero, total_double);

    assert(total_double == 0 && "FATAL: Duplicate task pop+steal detected!");
    assert(total_zero == 0 && "FATAL: Task lost during pop/steal race!");
    assert(total_single == NUM_TRIALS);
    printf("  --> PASSED: Zero duplicates, zero leaks across %d pop/steal races.\n\n", NUM_TRIALS);

    barrier_destroy(&start_bar);
    barrier_destroy(&end_bar);
    test_queue_destroy(q);
}

// Test 3: Fallback pop_end() race
static void* thief_pop_end_func(void* ptr) {
    thief_arg_t* arg = (thief_arg_t*)ptr;
    for (int it = 0; it < arg->iterations; it++) {
        barrier_wait(arg->start_bar);
        struct mir_task_t* t = test_queue_pop_end(arg->q);
        if (t != NULL) {
            arg->claimed++;
        }
        barrier_wait(arg->end_bar);
    }
    return NULL;
}

static void test_fallback_pop_end_race(void) {
    printf("[TEST 3] Two thieves racing for pop_end() fallback (%d iterations)...\n", NUM_TRIALS);

    struct mir_task_queue_t* q = test_queue_create(64);
    struct mir_task_t task;

    test_barrier_t start_bar, end_bar;
    barrier_init(&start_bar, 3);
    barrier_init(&end_bar, 3);

    thief_arg_t arg1 = { .q = q, .task = &task, .start_bar = &start_bar, .end_bar = &end_bar, .thief_id = 1, .claimed = 0, .iterations = NUM_TRIALS };
    thief_arg_t arg2 = { .q = q, .task = &task, .start_bar = &start_bar, .end_bar = &end_bar, .thief_id = 2, .claimed = 0, .iterations = NUM_TRIALS };

    pthread_t t1, t2;
    pthread_create(&t1, NULL, thief_pop_end_func, &arg1);
    pthread_create(&t2, NULL, thief_pop_end_func, &arg2);

    int total_single = 0, total_double = 0, total_zero = 0;

    for (int it = 0; it < NUM_TRIALS; it++) {
        task.id = it;
        task.taken = 0;
        task.done = 0;
        test_queue_push(q, &task);

        int c1_before = arg1.claimed;
        int c2_before = arg2.claimed;

        barrier_wait(&start_bar);
        barrier_wait(&end_bar);

        int round_claims = (arg1.claimed - c1_before) + (arg2.claimed - c2_before);
        if (round_claims == 1) total_single++;
        else if (round_claims == 0) total_zero++;
        else total_double++;

        assert(q->size == 0);
        assert(task.taken == 1);
    }

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("  Results: Thief 1 got %d, Thief 2 got %d\n", arg1.claimed, arg2.claimed);
    printf("  Single: %d, Zero: %d, Double: %d\n", total_single, total_zero, total_double);

    assert(total_double == 0 && "FATAL: Duplicate task pop_end detected!");
    assert(total_zero == 0 && "FATAL: Task lost during pop_end race!");
    assert(total_single == NUM_TRIALS);
    printf("  --> PASSED: Zero duplicates, zero leaks across %d pop_end races.\n\n", NUM_TRIALS);

    barrier_destroy(&start_bar);
    barrier_destroy(&end_bar);
    test_queue_destroy(q);
}

int main(void) {
    printf("=================================================================\n");
    printf("NOVA Queue Deterministic Race & Single-Ownership Invariant Suite\n");
    printf("=================================================================\n\n");

    test_two_thieves_race();
    test_pop_vs_steal_race();
    test_fallback_pop_end_race();

    printf("ALL 3 DETERMINISTIC QUEUE RACE SUITES PASSED CLEANLY!\n");
    return 0;
}
