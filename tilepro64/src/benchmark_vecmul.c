#include "benchmark.h"
#include <stdlib.h>
#include <stdio.h>

#define VECMUL_NUM_TASKS 64
#define VECMUL_CHUNK_BYTES (96 * 1024) // 96 KB per task chunk
#define VECMUL_COMPUTE_CYCLES 180000

benchmark_result_t run_benchmark_vecmul(scheduler_policy_t sched, data_dist_policy_t dist, uint16_t vicinity) {
    tilepro64_init();
    latency_model_init(TILEPRO64_LOCAL_LATENCY_CYCLES, TILEPRO64_REMOTE_BASE_LATENCY_CYCLES, TILEPRO64_PER_HOP_LATENCY_CYCLES);
    scheduler_init(sched, vicinity);

    task_t* tasks[VECMUL_NUM_TASKS];
    for (uint32_t i = 0; i < VECMUL_NUM_TASKS; i++) {
        tasks[i] = task_create(i, VECMUL_COMPUTE_CYCLES);
        if (dist == DATA_DIST_COARSE) {
            // In Vecmul with coarse distribution, each task accesses a primary chunk homed at tile i
            // and partially overlaps with neighbor tile (i+1)%64
            uint16_t homes[2] = { (uint16_t)(i % TILEPRO64_NUM_TILES), (uint16_t)((i + 1) % TILEPRO64_NUM_TILES) };
            uint64_t bytes[2] = { (VECMUL_CHUNK_BYTES * 3) / 4, VECMUL_CHUNK_BYTES / 4 };
            distribute_multi_coarse(&tasks[i]->data_dist, homes, bytes, 2);
        } else {
            distribute_fine(&tasks[i]->data_dist, VECMUL_CHUNK_BYTES);
        }
        // Master worker on tile 0 deals tasks
        scheduler_deal_task(tasks[i], 0);
    }

    uint32_t completed = 0;
    uint32_t idle_rounds = 0;
    while (completed < VECMUL_NUM_TASKS && idle_rounds < 1000) {
        bool progress = false;
        for (uint16_t w = 0; w < TILEPRO64_NUM_TILES; w++) {
            task_t* t = scheduler_find_task(w);
            if (t) {
                scheduler_execute_task(w, t);
                completed++;
                progress = true;
            }
        }
        if (!progress) {
            idle_rounds++;
        } else {
            idle_rounds = 0;
        }
    }

    benchmark_result_t res = {
        .benchmark_name = "vecmul",
        .scheduler_name = scheduler_policy_name(sched),
        .dist_name = data_dist_policy_name(dist),
        .vicinity = vicinity,
        .simulated_comm_cost = g_scheduler.total_comm_cost,
        .simulated_execution_time = g_scheduler.max_tile_cycles,
        .task_count = (uint32_t)g_scheduler.total_tasks_executed,
        .steal_count = (uint32_t)g_scheduler.total_tasks_stolen,
        .local_exec_count = (uint32_t)g_scheduler.local_executions,
        .remote_exec_count = (uint32_t)g_scheduler.remote_executions
    };

    for (uint32_t i = 0; i < VECMUL_NUM_TASKS; i++) {
        task_destroy(tasks[i]);
    }
    tilepro64_cleanup();

    return res;
}
