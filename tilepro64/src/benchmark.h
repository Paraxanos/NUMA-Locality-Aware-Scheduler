#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "tilepro64.h"
#include "scheduler.h"
#include "data_distribution.h"

typedef struct {
    const char* benchmark_name;
    const char* scheduler_name;
    const char* dist_name;
    uint16_t vicinity;
    uint64_t simulated_comm_cost;
    uint64_t simulated_execution_time;
    uint32_t task_count;
    uint32_t steal_count;
    uint32_t local_exec_count;
    uint32_t remote_exec_count;
} benchmark_result_t;

benchmark_result_t run_benchmark_map(scheduler_policy_t sched, data_dist_policy_t dist, uint16_t vicinity);
benchmark_result_t run_benchmark_vecmul(scheduler_policy_t sched, data_dist_policy_t dist, uint16_t vicinity);

#endif // BENCHMARK_H
