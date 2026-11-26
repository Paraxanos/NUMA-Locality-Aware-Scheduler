#include "benchmark.h"
#include <stdlib.h>
#include <string.h>

#define VECMUL_NUM_TASKS 64
#define VECMUL_CHUNK_BYTES (96 * 1024)
#define VECMUL_COMPUTE_CYCLES 180000
#define H10B_TASKS 64

benchmark_result_t run_benchmark_h10b(const char* policy, uint16_t vicinity) {
    scheduler_policy_t sched = SCHED_POLICY_DP;
    uint16_t search_radius = vicinity;
    if (policy && strcmp(policy, "ws") == 0) sched = SCHED_POLICY_WS;
    else if (policy && strcmp(policy, "la") == 0) { sched = SCHED_POLICY_LA; search_radius = 2; }

    tilepro64_init();
    latency_model_init(TILEPRO64_LOCAL_LATENCY_CYCLES, TILEPRO64_REMOTE_BASE_LATENCY_CYCLES, TILEPRO64_PER_HOP_LATENCY_CYCLES);
    scheduler_init(sched, search_radius);

    task_t* tasks[H10B_TASKS];
    for (uint32_t i = 0; i < H10B_TASKS; i++) {
        uint64_t compute = (i % 8 == 0) ? 30000 : 5000;
        tasks[i] = task_create(i, compute);
        distribute_coarse(&tasks[i]->data_dist, 63, 4096);
        scheduler_deal_task(tasks[i], 63);
    }

    uint32_t idle_rounds[TILEPRO64_NUM_TILES] = {0};
    uint64_t idle_probes = 0;
    uint32_t completed = 0;
    uint32_t rounds = 0;
    while (completed < H10B_TASKS && rounds < 10000) {
        bool progress = false;
        for (uint16_t worker = 0; worker < TILEPRO64_NUM_TILES; worker++) {
            task_t* task = (sched == SCHED_POLICY_DP)
                ? scheduler_find_task_dynamic(worker, idle_rounds[worker], H10B_TASKS - completed)
                : scheduler_find_task(worker);
            if (task) {
                scheduler_execute_task(worker, task);
                completed++;
                idle_rounds[worker] = 0;
                progress = true;
            } else if (completed < H10B_TASKS) {
                idle_rounds[worker]++;
                idle_probes++;
            }
        }
        rounds++;
        if (!progress && completed == H10B_TASKS) break;
    }

    benchmark_result_t result = {
        .benchmark_name = "h10b", .scheduler_name = policy ? policy : "dp",
        .dist_name = "corner-home-63", .vicinity = search_radius,
        .simulated_comm_cost = g_scheduler.total_comm_cost,
        .simulated_migration_cost = 0, .migrated_bytes = 0,
        .simulated_total_work = g_scheduler.total_cycles_including_migration,
        .simulated_execution_time = g_scheduler.max_tile_cycles,
        .congestion_queue_cycles = 0, .max_link_busy_cycles = 0,
        .idle_probe_rounds = idle_probes,
        .total_steal_distance = g_scheduler.total_steal_distance,
        .task_count = (uint32_t)g_scheduler.total_tasks_executed,
        .steal_count = (uint32_t)g_scheduler.total_tasks_stolen,
        .local_exec_count = (uint32_t)g_scheduler.local_executions,
        .remote_exec_count = (uint32_t)g_scheduler.remote_executions
    };
    for (uint32_t i = 0; i < H10B_TASKS; i++) task_destroy(tasks[i]);
    tilepro64_cleanup();
    return result;
}

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
