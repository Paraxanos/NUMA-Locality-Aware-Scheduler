#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_QUEUE_CAPACITY 1024

struct task_t;

typedef struct task_queue_t {
    struct task_t* tasks[MAX_QUEUE_CAPACITY];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint16_t tile_id;
} task_queue_t;

task_queue_t* queue_create(uint16_t tile_id);
void queue_destroy(task_queue_t* q);
bool queue_push(task_queue_t* q, struct task_t* task);
struct task_t* queue_pop_local(task_queue_t* q);
struct task_t* queue_steal(task_queue_t* q);
uint32_t queue_size(const task_queue_t* q);

#endif // QUEUE_H
