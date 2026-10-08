/* Driver: mir_create() -> bench_init() -> time bench_run() -> bench_check() -> bench_teardown()
 * -> mir_destroy(). Prints one CSV row to stdout matching the schema in
 * docs/11-reproduction-plan.md Section 3.5 (columns present here; steal_count etc. added once
 * worker-stats wiring lands). Labels (benchmark/scheduler/dist/topology) come from env vars the
 * harness sets, so this binary doesn't need to parse MIR_CONF itself.
 */
#include <stdio.h>
#include <stdlib.h>
#include "bench_common.h"
/* Internal headers only (not mir_public_int.h -- its struct/enum copies conflict with these
 * when both are included in one TU). mir_runtime.h pulls in mir_task.h/mir_worker.h/mir_utils.h
 * transitively, giving mir_create/mir_destroy/mir_task_wait/mir_get_cycles and the worker
 * statistics struct we need to aggregate comm_cost. */
#include "mir_runtime.h"
#include "mir_worker.h"
#include "mir_utils.h"

static const char* env_or(const char* name, const char* dflt)
{
    const char* v = getenv(name);
    return v ? v : dflt;
}

int main(void)
{
    mir_create();

    bench_init();

    uint64_t start = mir_get_cycles();
    bench_run();
    uint64_t end = mir_get_cycles();
    uint64_t exec_cycles = end - start;

    int check_ok = bench_check();

    /* Aggregate per-worker statistics before mir_destroy() frees them. Only populated when
     * MIR_CONF includes --worker-stats (the scheduler's comm_cost computation itself always
     * runs; --worker-stats only gates whether it's *recorded* for us to read back). */
    unsigned long long comm_cost_total = 0;
    unsigned long long comm_tasks_total = 0;
    unsigned long long tasks_stolen_total = 0;
    unsigned long long tasks_owned_total = 0;
    if (runtime->enable_worker_stats == 1) {
        for (int w = 0; w < runtime->num_workers; w++) {
            struct mir_worker_statistics_t* st = runtime->workers[w].statistics;
            if (!st) continue;
            comm_cost_total += st->total_comm_cost;
            comm_tasks_total += st->num_comm_tasks;
            tasks_stolen_total += st->num_tasks_stolen;
            tasks_owned_total += st->num_tasks_owned;
        }
    }

    bench_teardown();

    mir_destroy();

    /* benchmark,scheduler_policy,dist_policy,topology,run_id,exec_cycles,comm_cost_total,
       comm_tasks_total,comm_cost_avg,tasks_stolen,tasks_owned,check_ok */
    double comm_cost_avg = comm_tasks_total ? (double)comm_cost_total / (double)comm_tasks_total : 0.0;
    printf("%s,%s,%s,%s,%s,%llu,%llu,%llu,%.1f,%llu,%llu,%d\n",
           env_or("BENCH_NAME", "unknown"),
           env_or("BENCH_SCHED", "unknown"),
           env_or("BENCH_DIST", "unknown"),
           env_or("BENCH_TOPO", "unknown"),
           env_or("BENCH_RUN_ID", "0"),
           (unsigned long long)exec_cycles,
           comm_cost_total,
           comm_tasks_total,
           comm_cost_avg,
           tasks_stolen_total,
           tasks_owned_total,
           check_ok);

    return check_ok ? 0 : 1;
}
