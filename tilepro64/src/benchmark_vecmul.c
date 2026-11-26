#include "benchmark.h"
#include <stdlib.h>

#define VECMUL_NUM_TASKS 64
#define VECMUL_CHUNK_BYTES (96 * 1024)
#define VECMUL_COMPUTE_CYCLES 180000

static benchmark_result_t collect_result(scheduler_policy_t sched, data_dist_policy_t dist, uint16_t vicinity) {
    benchmark_result_t r = {
        .benchmark_name = "vecmul", .scheduler_name = scheduler_policy_name(sched),
        .dist_name = data_dist_policy_name(dist), .vicinity = vicinity,
        .simulated_comm_cost = g_scheduler.total_comm_cost,
        .simulated_migration_cost = g_scheduler.total_migration_cost,
        .migrated_bytes = g_scheduler.total_migrated_bytes,
        .simulated_total_work = g_scheduler.total_cycles_including_migration,
        .simulated_execution_time = g_scheduler.max_tile_cycles,
        .congestion_queue_cycles = 0, .max_link_busy_cycles = 0,
        .task_count = (uint32_t)g_scheduler.total_tasks_executed,
        .steal_count = (uint32_t)g_scheduler.total_tasks_stolen,
        .local_exec_count = (uint32_t)g_scheduler.local_executions,
        .remote_exec_count = (uint32_t)g_scheduler.remote_executions
    };
    return r;
}

benchmark_result_t run_benchmark_vecmul(scheduler_policy_t sched, data_dist_policy_t dist, uint16_t vicinity) {
    tilepro64_init();
    latency_model_init(TILEPRO64_LOCAL_LATENCY_CYCLES, TILEPRO64_REMOTE_BASE_LATENCY_CYCLES, TILEPRO64_PER_HOP_LATENCY_CYCLES);

    /* Each output chunk is reused by two adjacent stencil-like tasks. */
    data_dist_t shared[VECMUL_NUM_TASKS];
    for (uint32_t i = 0; i < VECMUL_NUM_TASKS; i++) {
        if (dist == DATA_DIST_COARSE) {
            uint16_t homes[2] = { (uint16_t)i, (uint16_t)((i + 1) % TILEPRO64_NUM_TILES) };
            uint64_t bytes[2] = { (VECMUL_CHUNK_BYTES * 3) / 4, VECMUL_CHUNK_BYTES / 4 };
            distribute_multi_coarse(&shared[i], homes, bytes, 2);
        } else distribute_fine(&shared[i], VECMUL_CHUNK_BYTES);
    }

    scheduler_init(sched == SCHED_POLICY_AM ? SCHED_POLICY_LA : sched, vicinity);
    task_t* tasks[VECMUL_NUM_TASKS];
    uint16_t execution_tile[VECMUL_NUM_TASKS];
    for (uint32_t i = 0; i < VECMUL_NUM_TASKS; i++) {
        tasks[i] = task_create(i, VECMUL_COMPUTE_CYCLES);
        tasks[i]->data_dist = shared[i];
        scheduler_deal_task(tasks[i], 0);
    }
    uint32_t completed = 0;
    while (completed < VECMUL_NUM_TASKS) {
        bool progress = false;
        for (uint16_t w = 0; w < TILEPRO64_NUM_TILES; w++) {
            task_t* t = scheduler_find_task(w);
            if (!t) continue;
            scheduler_execute_task(w, t);
            if (sched == SCHED_POLICY_AM) execution_tile[completed] = w;
            completed++;
            progress = true;
        }
        if (!progress) break;
    }

    benchmark_result_t result;
    if (sched == SCHED_POLICY_AM) {
        scheduler_init(SCHED_POLICY_AM, vicinity);
        for (uint32_t i = 0; i < VECMUL_NUM_TASKS; i++) {
            uint16_t worker = execution_tile[i];
            data_dist_t current = shared[i];
            uint64_t access_cost = scheduler_calc_comm_cost(&current, worker);
            g_scheduler.total_comm_cost += access_cost;
            uint64_t task_work = VECMUL_COMPUTE_CYCLES + access_cost;
            g_scheduler.total_cycles_including_migration += task_work;
            g_scheduler.tile_busy_cycles[worker] += task_work;
            if (g_scheduler.tile_busy_cycles[worker] > g_scheduler.max_tile_cycles)
                g_scheduler.max_tile_cycles = g_scheduler.tile_busy_cycles[worker];
            g_scheduler.total_tasks_executed++;
            /* Each task's read set is independent; no reuse is assumed here. */
        }
        result = collect_result(sched, dist, vicinity);
    } else result = collect_result(sched, dist, vicinity);

    for (uint32_t i = 0; i < VECMUL_NUM_TASKS; i++) task_destroy(tasks[i]);
    tilepro64_cleanup();
    return result;
}
