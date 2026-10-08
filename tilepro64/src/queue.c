#include "queue.h"
#include <stdlib.h>
#include <string.h>

task_queue_t* queue_create(uint16_t tile_id) {
    task_queue_t* q = (task_queue_t*)malloc(sizeof(task_queue_t));
    if (!q) return NULL;
    memset(q, 0, sizeof(task_queue_t));
    q->tile_id = tile_id;
    return q;
}

void queue_destroy(task_queue_t* q) {
    if (q) {
        free(q);
    }
}

bool queue_push(task_queue_t* q, struct task_t* task) {
    if (!q || !task || q->count >= MAX_QUEUE_CAPACITY) return false;
    q->tasks[q->tail] = task;
    q->tail = (q->tail + 1) % MAX_QUEUE_CAPACITY;
    q->count++;
    return true;
}

struct task_t* queue_pop_local(task_queue_t* q) {
    if (!q || q->count == 0) return NULL;
    struct task_t* task = q->tasks[q->head];
    q->head = (q->head + 1) % MAX_QUEUE_CAPACITY;
    q->count--;
    return task;
}

struct task_t* queue_steal(task_queue_t* q) {
    if (!q || q->count == 0) return NULL;
    // Steal from tail end (work-stealing deque convention)
    q->tail = (q->tail + MAX_QUEUE_CAPACITY - 1) % MAX_QUEUE_CAPACITY;
    struct task_t* task = q->tasks[q->tail];
    q->count--;
    return task;
}

uint32_t queue_size(const task_queue_t* q) {
    return q ? q->count : 0;
}
