#include "scheduler.h"
#include "queue.h"
#include <limits.h>
#include <string.h>

#define CACHE_LINE_BYTES 64
#define MIGRATION_BYTES_PER_CYCLE 16

scheduler_t g_scheduler;

void scheduler_init(scheduler_policy_t policy, uint16_t vicinity) {
    memset(&g_scheduler, 0, sizeof(scheduler_t));
    g_scheduler.policy = policy;
    g_scheduler.vicinity = (vicinity > 0 && vicinity <= TILEPRO64_MAX_HOPS) ? vicinity : TILEPRO64_MAX_HOPS;
    g_scheduler.llc_per_core_bytes = TILEPRO64_L2_BANK_KB * 1024;
}

void scheduler_reset_costs(void) {
    g_scheduler.total_comm_cost = 0;
    g_scheduler.total_migration_cost = 0;
    g_scheduler.total_migrated_bytes = 0;
    g_scheduler.total_cycles_including_migration = 0;
    g_scheduler.max_tile_cycles = 0;
    memset(g_scheduler.tile_busy_cycles, 0, sizeof(g_scheduler.tile_busy_cycles));
}

uint64_t scheduler_calc_comm_cost(const data_dist_t* dist, uint16_t target_tile) {
    if (!dist || target_tile >= TILEPRO64_NUM_TILES) return 0;
    uint64_t cost = 0;
    for (uint16_t home = 0; home < TILEPRO64_NUM_TILES; home++) {
        uint64_t bytes = dist->bytes_per_tile[home];
        if (bytes) cost += ((bytes + CACHE_LINE_BYTES - 1) / CACHE_LINE_BYTES) * latency_get(target_tile, home);
    }
    return cost;
}

/* Evaluate and optionally migrate one whole shared data object. */
uint64_t scheduler_adapt_data_placement(data_dist_t* dist, uint16_t access_tile,
                                        uint32_t accesses_per_byte, uint32_t remaining_uses) {
    if (!dist || access_tile >= TILEPRO64_NUM_TILES || !accesses_per_byte || !remaining_uses) return 0;
    uint64_t savings_per_use = 0;
    uint64_t transfer_cost = 0;
    uint64_t moved_bytes = 0;
    for (uint16_t home = 0; home < TILEPRO64_NUM_TILES; home++) {
        uint64_t bytes = dist->bytes_per_tile[home];
        if (!bytes || home == access_tile) continue;
        uint32_t remote = latency_get(access_tile, home);
        uint32_t local = latency_get(access_tile, access_tile);
        uint32_t hops = tile_distance(home, access_tile);
        savings_per_use += ((bytes + CACHE_LINE_BYTES - 1) / CACHE_LINE_BYTES) * (remote - local);
        transfer_cost += (bytes * (g_latency_model.remote_base + g_latency_model.per_hop * hops) +
                          MIGRATION_BYTES_PER_CYCLE - 1) / MIGRATION_BYTES_PER_CYCLE;
        moved_bytes += bytes;
    }
    if (!moved_bytes || !savings_per_use) return 0;
    uint64_t expected_savings = savings_per_use * accesses_per_byte * remaining_uses;
    if (transfer_cost >= expected_savings) return 0;

    tile_t* destination = tile_get(access_tile);
    if (!destination || dist->total_bytes > destination->l2_bank_size_bytes) return 0;

    memset(dist->bytes_per_tile, 0, sizeof(dist->bytes_per_tile));
    dist->bytes_per_tile[access_tile] = dist->total_bytes;
    g_scheduler.total_migration_cost += transfer_cost;
    g_scheduler.total_migrated_bytes += moved_bytes;
    return transfer_cost;
}

void scheduler_charge_migration(uint64_t bytes, uint16_t from_tile, uint16_t to_tile) {
    if (!bytes || from_tile >= TILEPRO64_NUM_TILES || to_tile >= TILEPRO64_NUM_TILES || from_tile == to_tile) return;
    uint64_t cost = (bytes * (g_latency_model.remote_base +
        g_latency_model.per_hop * tile_distance(from_tile, to_tile)) + MIGRATION_BYTES_PER_CYCLE - 1) /
        MIGRATION_BYTES_PER_CYCLE;
    g_scheduler.total_migration_cost += cost;
    g_scheduler.total_migrated_bytes += bytes;
    g_scheduler.total_cycles_including_migration += cost;
}

uint16_t scheduler_deal_task(task_t* task, uint16_t creator_tile) {
    if (!task) return creator_tile;
    uint16_t target_tile = creator_tile;
    if (g_scheduler.policy != SCHED_POLICY_WS && task->data_dist.total_bytes > g_scheduler.llc_per_core_bytes) {
        uint64_t min_cost = ULLONG_MAX;
        for (uint16_t i = 0; i < TILEPRO64_NUM_TILES; i++) {
            uint64_t cost = scheduler_calc_comm_cost(&task->data_dist, i);
            if (cost < min_cost) { min_cost = cost; target_tile = i; }
        }
    }
    task->assigned_tile = target_tile;
    task->state = TASK_STATE_ASSIGNED;
    queue_push(tile_get(target_tile)->queue, task);
    g_scheduler.total_tasks_dealt++;
    return target_tile;
}

task_t* scheduler_find_task(uint16_t worker_tile) {
    tile_t* tile = tile_get(worker_tile);
    if (!tile) return NULL;
    task_t* task = queue_pop_local(tile->queue);
    if (task) { task->is_stolen = false; return task; }
    if (g_scheduler.policy != SCHED_POLICY_WS) {
        for (uint16_t d = 1; d <= g_scheduler.vicinity; d++) {
            for (uint16_t v = 0; v < TILEPRO64_NUM_TILES; v++) {
                if (v != worker_tile && tile_distance(worker_tile, v) == d) {
                    task = queue_steal(tile_get(v)->queue);
                    if (task) { task->is_stolen = true; g_scheduler.total_tasks_stolen++; return task; }
                }
            }
        }
    } else {
        for (uint16_t v = 0; v < TILEPRO64_NUM_TILES; v++) {
            uint16_t candidate = (worker_tile + v + 1) % TILEPRO64_NUM_TILES;
            task = queue_steal(tile_get(candidate)->queue);
            if (task) { task->is_stolen = true; g_scheduler.total_tasks_stolen++; return task; }
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
    if (worker_tile == task->assigned_tile) g_scheduler.local_executions++;
    else g_scheduler.remote_executions++;
    g_scheduler.tile_busy_cycles[worker_tile] += task->total_cycles;
    g_scheduler.total_cycles_including_migration += task->total_cycles;
    if (g_scheduler.tile_busy_cycles[worker_tile] > g_scheduler.max_tile_cycles)
        g_scheduler.max_tile_cycles = g_scheduler.tile_busy_cycles[worker_tile];
}

const char* scheduler_policy_name(scheduler_policy_t pol) {
    switch (pol) {
        case SCHED_POLICY_WS: return "ws";
        case SCHED_POLICY_LA: return "la";
        case SCHED_POLICY_AM: return "am";
        default: return "unknown";
    }
}
