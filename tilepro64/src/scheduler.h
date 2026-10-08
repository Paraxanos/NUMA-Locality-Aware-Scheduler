#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "tilepro64.h"
#include "task.h"
#include "latency.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SCHED_POLICY_WS, // Baseline Work-Stealing
    SCHED_POLICY_LA  // Locality-Aware Work Dealing & Stealing
} scheduler_policy_t;

typedef struct {
    scheduler_policy_t policy;
    uint16_t vicinity;            // Fixed vicinity search radius (e.g. 1..14)
    uint64_t llc_per_core_bytes;  // Significance threshold (64 KB)
    
    // Statistics counters
    uint64_t total_tasks_dealt;
    uint64_t total_tasks_executed;
    uint64_t total_tasks_stolen;
    uint64_t local_executions;
    uint64_t remote_executions;
    uint64_t total_comm_cost;
    uint64_t max_tile_cycles;     // Critical path makespan (simulated execution time)
    uint64_t tile_busy_cycles[TILEPRO64_NUM_TILES];
} scheduler_t;

extern scheduler_t g_scheduler;

void scheduler_init(scheduler_policy_t policy, uint16_t vicinity);
uint64_t scheduler_calc_comm_cost(const data_dist_t* dist, uint16_t target_tile);
uint16_t scheduler_deal_task(task_t* task, uint16_t creator_tile);
task_t* scheduler_find_task(uint16_t worker_tile);
void scheduler_execute_task(uint16_t worker_tile, task_t* task);

const char* scheduler_policy_name(scheduler_policy_t pol);

#endif // SCHEDULER_H
