#include "benchmark.h"
#include <stdlib.h>
#include <stdio.h>

#define MAP_NUM_TASKS 64
#define MAP_VEC_BYTES (128 * 1024) // 128 KB per vector (clears 64 KB threshold)
#define MAP_COMPUTE_CYCLES 150000

benchmark_result_t run_benchmark_map(scheduler_policy_t sched, data_dist_policy_t dist, uint16_t vicinity) {
    tilepro64_init();
    latency_model_init(TILEPRO64_LOCAL_LATENCY_CYCLES, TILEPRO64_REMOTE_BASE_LATENCY_CYCLES, TILEPRO64_PER_HOP_LATENCY_CYCLES);
    scheduler_init(sched, vicinity);

    task_t* tasks[MAP_NUM_TASKS];
    for (uint32_t i = 0; i < MAP_NUM_TASKS; i++) {
        tasks[i] = task_create(i, MAP_COMPUTE_CYCLES);
        if (dist == DATA_DIST_COARSE) {
            distribute_coarse(&tasks[i]->data_dist, (uint16_t)(i % TILEPRO64_NUM_TILES), MAP_VEC_BYTES);
        } else {
            distribute_fine(&tasks[i]->data_dist, MAP_VEC_BYTES);
        }
        // Master worker on tile 0 deals all tasks
        scheduler_deal_task(tasks[i], 0);
    }

    // Run simulated execution loop until all tasks are executed
    uint32_t completed = 0;
    uint32_t idle_rounds = 0;
    while (completed < MAP_NUM_TASKS && idle_rounds < 1000) {
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
        .benchmark_name = "map",
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

    for (uint32_t i = 0; i < MAP_NUM_TASKS; i++) {
        task_destroy(tasks[i]);
    }
    tilepro64_cleanup();

    return res;
}
