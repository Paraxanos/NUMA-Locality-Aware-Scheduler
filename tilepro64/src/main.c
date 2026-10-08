#include "benchmark.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_csv_header(void) {
    printf("benchmark,scheduler,data_distribution,vicinity,simulated_communication_cost,simulated_execution_time,task_count,steal_count,local_execution_count,remote_execution_count\n");
}

static void print_csv_row(const benchmark_result_t* r) {
    printf("%s,%s,%s,%u,%llu,%llu,%u,%u,%u,%u\n",
           r->benchmark_name,
           r->scheduler_name,
           r->dist_name,
           r->vicinity,
           (unsigned long long)r->simulated_comm_cost,
           (unsigned long long)r->simulated_execution_time,
           r->task_count,
           r->steal_count,
           r->local_exec_count,
           r->remote_exec_count);
}

static void print_usage(const char* prog) {
    printf("TILEPro64 Manycore Locality-Aware Scheduler Simulator\n");
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --benchmark <map|vecmul>    Benchmark to run (default: map)\n");
    printf("  --scheduler <ws|la>         Scheduler policy: ws (work-stealing) or la (locality-aware) (default: la)\n");
    printf("  --dist <coarse|fine>        Data distribution policy (default: coarse)\n");
    printf("  --vicinity <1..14>          Vicinity search radius in hops (default: 4)\n");
    printf("  --csv                       Print output in CSV format\n");
    printf("  --all                       Run complete benchmark matrix and emit CSV\n");
    printf("  --help                      Show this help message\n");
}

int main(int argc, char* argv[]) {
    const char* bench_str = "map";
    scheduler_policy_t sched = SCHED_POLICY_LA;
    data_dist_policy_t dist = DATA_DIST_COARSE;
    uint16_t vicinity = 4;
    bool csv_mode = false;
    bool all_mode = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--benchmark") == 0 && i + 1 < argc) {
            bench_str = argv[++i];
        } else if (strcmp(argv[i], "--scheduler") == 0 && i + 1 < argc) {
            const char* s = argv[++i];
            if (strcmp(s, "ws") == 0) sched = SCHED_POLICY_WS;
            else if (strcmp(s, "la") == 0) sched = SCHED_POLICY_LA;
        } else if (strcmp(argv[i], "--dist") == 0 && i + 1 < argc) {
            const char* d = argv[++i];
            if (strcmp(d, "fine") == 0) dist = DATA_DIST_FINE;
            else if (strcmp(d, "coarse") == 0) dist = DATA_DIST_COARSE;
        } else if (strcmp(argv[i], "--vicinity") == 0 && i + 1 < argc) {
            vicinity = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--csv") == 0) {
            csv_mode = true;
        } else if (strcmp(argv[i], "--all") == 0) {
            all_mode = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    if (all_mode) {
        print_csv_header();
        const char* benches[] = { "map", "vecmul" };
        scheduler_policy_t scheds[] = { SCHED_POLICY_WS, SCHED_POLICY_LA };
        data_dist_policy_t dists[] = { DATA_DIST_COARSE, DATA_DIST_FINE };
        uint16_t vicinities[] = { 1, 2, 3, 4, 6, 8, 14 };

        for (int b = 0; b < 2; b++) {
            for (int s = 0; s < 2; s++) {
                for (int d = 0; d < 2; d++) {
                    for (int v = 0; v < 7; v++) {
                        benchmark_result_t res;
                        if (strcmp(benches[b], "map") == 0) {
                            res = run_benchmark_map(scheds[s], dists[d], vicinities[v]);
                        } else {
                            res = run_benchmark_vecmul(scheds[s], dists[d], vicinities[v]);
                        }
                        print_csv_row(&res);
                    }
                }
            }
        }
        return 0;
    }

    benchmark_result_t res;
    if (strcmp(bench_str, "map") == 0) {
        res = run_benchmark_map(sched, dist, vicinity);
    } else if (strcmp(bench_str, "vecmul") == 0) {
        res = run_benchmark_vecmul(sched, dist, vicinity);
    } else {
        fprintf(stderr, "Unknown benchmark: %s\n", bench_str);
        return 1;
    }

    if (csv_mode) {
        print_csv_header();
        print_csv_row(&res);
    } else {
        printf("============================================================\n");
        printf("TILEPro64 Manycore Simulation Result\n");
        printf("============================================================\n");
        printf("Benchmark:                 %s\n", res.benchmark_name);
        printf("Scheduler Policy:          %s\n", res.scheduler_name);
        printf("Data Distribution:         %s\n", res.dist_name);
        printf("Vicinity Search Radius:    %u hops\n", res.vicinity);
        printf("Simulated Comm Cost:       %llu cycles\n", (unsigned long long)res.simulated_comm_cost);
        printf("Simulated Execution Time:  %llu cycles\n", (unsigned long long)res.simulated_execution_time);
        printf("Task Count:                %u\n", res.task_count);
        printf("Steal Count:               %u\n", res.steal_count);
        printf("Local Executions:          %u\n", res.local_exec_count);
        printf("Remote Executions:         %u\n", res.remote_exec_count);
        printf("============================================================\n");
    }

    return 0;
}
