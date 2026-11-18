#ifndef MOCK_MIR_TASK_QUEUE_H
#define MOCK_MIR_TASK_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "mir_task.h"

struct mir_lock_t {
    pthread_mutex_t m;
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

void* mir_task_queue_create(uint32_t capacity);
void mir_task_queue_destroy(struct mir_task_queue_t* queue);
int mir_task_queue_push(struct mir_task_queue_t* queue, struct mir_task_t* task);
struct mir_task_t* mir_task_queue_pop(struct mir_task_queue_t* queue);
uint32_t mir_task_queue_size(const struct mir_task_queue_t* queue);
int mir_task_queue_lock_and_remove(struct mir_task_queue_t* queue, struct mir_task_t* task);
struct mir_task_t* mir_task_queue_pop_end(struct mir_task_queue_t* queue);

struct mir_task_queue_t* mir_get_node_task_queue(uint16_t node_id);
uint32_t mir_get_node_task_queue_size(uint16_t node_id);

#endif
