#ifndef TASK_H
#define TASK_H

#include "home_cache.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX_TASK_DEPS 16

typedef enum {
    TASK_STATE_CREATED,
    TASK_STATE_READY,
    TASK_STATE_ASSIGNED,
    TASK_STATE_EXECUTED
} task_state_t;

typedef struct task_t {
    uint32_t id;
    uint64_t compute_cycles;       // Simulated pure computation time
    data_dist_t data_dist;         // D[64] distribution of data across home caches
    task_state_t state;
    
    // Dependencies
    uint32_t num_dependencies;
    struct task_t* dependencies[MAX_TASK_DEPS];
    uint32_t unresolved_deps;

    // Execution metadata
    uint16_t assigned_tile;
    uint16_t executed_tile;
    bool is_stolen;
    uint64_t communication_cost;   // Computed access cost on executed tile
    uint64_t total_cycles;         // compute_cycles + communication_cost
} task_t;

task_t* task_create(uint32_t id, uint64_t compute_cycles);
void task_destroy(task_t* t);
void task_add_dependency(task_t* task, task_t* dep);
bool task_is_ready(const task_t* t);
void task_resolve_dep(task_t* task);

#endif // TASK_H
