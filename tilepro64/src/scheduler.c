#include "scheduler.h"
#include "queue.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define CACHE_LINE_BYTES 64

scheduler_t g_scheduler;

void scheduler_init(scheduler_policy_t policy, uint16_t vicinity) {
    memset(&g_scheduler, 0, sizeof(scheduler_t));
    g_scheduler.policy = policy;
    g_scheduler.vicinity = (vicinity > 0 && vicinity <= TILEPRO64_MAX_HOPS) ? vicinity : TILEPRO64_MAX_HOPS;
    g_scheduler.llc_per_core_bytes = TILEPRO64_L2_BANK_KB * 1024; // 64 KB
}

uint64_t scheduler_calc_comm_cost(const data_dist_t* dist, uint16_t target_tile) {
    if (!dist) return 0;
    uint64_t cost = 0;
    for (uint16_t i = 0; i < TILEPRO64_NUM_TILES; i++) {
        if (dist->bytes_per_tile[i] > 0) {
            uint64_t lines = (dist->bytes_per_tile[i] + CACHE_LINE_BYTES - 1) / CACHE_LINE_BYTES;
            uint32_t lat = latency_get(target_tile, i);
            cost += lines * lat;
        }
    }
    return cost;
}

uint16_t scheduler_deal_task(task_t* task, uint16_t creator_tile) {
    if (!task) return creator_tile;
    uint16_t target_tile = creator_tile;

    if (g_scheduler.policy == SCHED_POLICY_LA) {
        // Algorithm 3 / Locality-Aware Work Dealing:
        // Gate: if footprint is small (<= LLC/C), keep on creator tile
        if (task->data_dist.total_bytes > g_scheduler.llc_per_core_bytes) {
            uint64_t min_cost = ULLONG_MAX;
            for (uint16_t i = 0; i < TILEPRO64_NUM_TILES; i++) {
                uint64_t c = scheduler_calc_comm_cost(&task->data_dist, i);
                if (c < min_cost) {
                    min_cost = c;
                    target_tile = i;
                }
            }
        }
    } else {
        // Baseline Work Stealing: place locally on creator tile
        target_tile = creator_tile;
    }

    task->assigned_tile = target_tile;
    task->state = TASK_STATE_ASSIGNED;
    queue_push(tile_get(target_tile)->queue, task);
    g_scheduler.total_tasks_dealt++;
    return target_tile;
}

task_t* scheduler_find_task(uint16_t worker_tile) {
    tile_t* w_tile = tile_get(worker_tile);
    if (!w_tile) return NULL;

    // 1. Pop from local queue first
    task_t* task = queue_pop_local(w_tile->queue);
    if (task) {
        task->is_stolen = false;
        return task;
    }

    // 2. Work Stealing
    if (g_scheduler.policy == SCHED_POLICY_LA) {
        // Locality-Aware: Search queues strictly within configured vicinity radius
        // Walk outwards in concentric Manhattan rings
        for (uint16_t d = 1; d <= g_scheduler.vicinity; d++) {
            for (uint16_t v = 0; v < TILEPRO64_NUM_TILES; v++) {
                if (v == worker_tile) continue;
                if (tile_distance(worker_tile, v) == d) {
                    task = queue_steal(tile_get(v)->queue);
                    if (task) {
                        task->is_stolen = true;
                        g_scheduler.total_tasks_stolen++;
                        return task;
                    }
                }
            }
        }
    } else {
        // Baseline Work-Stealing: Unrestricted search across all 63 tiles
        for (uint16_t v = 0; v < TILEPRO64_NUM_TILES; v++) {
            uint16_t candidate = (worker_tile + v + 1) % TILEPRO64_NUM_TILES;
            task = queue_steal(tile_get(candidate)->queue);
            if (task) {
                task->is_stolen = true;
                g_scheduler.total_tasks_stolen++;
                return task;
            }
        }
    }

    return NULL;
}

void scheduler_execute_task(uint16_t worker_tile, task_t* task) {
    if (!task) return;

    task->executed_tile = worker_tile;
    task->communication_cost = scheduler_calc_comm_cost(&task->data_dist, worker_tile);
    task->total_cycles = task->compute_cycles + task->communication_cost;
    task->state = TASK_STATE_EXECUTED;

    g_scheduler.total_comm_cost += task->communication_cost;
    g_scheduler.total_tasks_executed++;

    if (task->executed_tile == task->assigned_tile) {
        g_scheduler.local_executions++;
    } else {
        g_scheduler.remote_executions++;
    }

    // Advance worker busy time
    g_scheduler.tile_busy_cycles[worker_tile] += task->total_cycles;
    if (g_scheduler.tile_busy_cycles[worker_tile] > g_scheduler.max_tile_cycles) {
        g_scheduler.max_tile_cycles = g_scheduler.tile_busy_cycles[worker_tile];
    }
}

const char* scheduler_policy_name(scheduler_policy_t pol) {
    switch (pol) {
        case SCHED_POLICY_WS: return "ws";
        case SCHED_POLICY_LA: return "la";
        default: return "unknown";
    }
}
