#include "benchmark.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_csv_header(void) {
    printf("benchmark,scheduler,data_distribution,vicinity,simulated_communication_cost,simulated_migration_cost,migrated_bytes,simulated_total_work,simulated_execution_time,congestion_queue_cycles,max_link_busy_cycles,idle_probe_rounds,total_steal_distance,task_count,steal_count,local_execution_count,remote_execution_count\n");
}

static void print_csv_row(const benchmark_result_t* r) {
    printf("%s,%s,%s,%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%u,%u,%u,%u\n",
           r->benchmark_name,
           r->scheduler_name,
           r->dist_name,
           r->vicinity,
           (unsigned long long)r->simulated_comm_cost,
           (unsigned long long)r->simulated_migration_cost,
           (unsigned long long)r->migrated_bytes,
           (unsigned long long)r->simulated_total_work,
           (unsigned long long)r->simulated_execution_time,
           (unsigned long long)r->congestion_queue_cycles,
           (unsigned long long)r->max_link_busy_cycles,
           (unsigned long long)r->idle_probe_rounds,
           (unsigned long long)r->total_steal_distance,
           r->task_count,
           r->steal_count,
           r->local_exec_count,
           r->remote_exec_count);
}

static void print_usage(const char* prog) {
    printf("TILEPro64 Manycore Locality-Aware Scheduler Simulator\n");
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --benchmark <map|map_h8|vecmul|h9|h9_worst|h10a|h10b> Benchmark to run (default: map)\n");
    printf("  --scheduler <ws|la|am|dp>   Scheduler policy; h9 accepts simple, fifo, priority\n");
    printf("  --dist <coarse|fine|1d_striped|2d_block> Data distribution policy (default: coarse)\n");
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
            else if (strcmp(s, "am") == 0) sched = SCHED_POLICY_AM;
        } else if (strcmp(argv[i], "--dist") == 0 && i + 1 < argc) {
            const char* d = argv[++i];
            if (strcmp(d, "fine") == 0) dist = DATA_DIST_FINE;
            else if (strcmp(d, "coarse") == 0) dist = DATA_DIST_COARSE;
            else if (strcmp(d, "1d_striped") == 0) dist = DATA_DIST_1D_STRIPED;
            else if (strcmp(d, "2d_block") == 0) dist = DATA_DIST_2D_BLOCK;
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
    const char* benches[] = { "map", "map_h8", "vecmul" };
        scheduler_policy_t scheds[] = { SCHED_POLICY_WS, SCHED_POLICY_LA, SCHED_POLICY_AM };
        data_dist_policy_t dists[] = { DATA_DIST_COARSE, DATA_DIST_FINE };
        uint16_t vicinities[] = { 1, 2, 3, 4, 6, 8, 14 };

        for (int b = 0; b < 3; b++) {
            for (int s = 0; s < 3; s++) {
                for (int d = 0; d < 2; d++) {
                    for (int v = 0; v < 7; v++) {
                        benchmark_result_t res;
                        if (strcmp(benches[b], "map_h8") == 0 && scheds[s] == SCHED_POLICY_WS) continue;
                        if (strcmp(benches[b], "map") == 0) {
                            res = run_benchmark_map(scheds[s], dists[d], vicinities[v]);
                        } else if (strcmp(benches[b], "map_h8") == 0) {
                            res = run_benchmark_map_h8(scheds[s], dists[d], vicinities[v]);
                        } else {
                            res = run_benchmark_vecmul(scheds[s], dists[d], vicinities[v]);
                        }
                        print_csv_row(&res);
                    }
                }
            }
        }
        benchmark_result_t h9_fifo = run_benchmark_h9("fifo");
        benchmark_result_t h9_priority = run_benchmark_h9("priority");
        benchmark_result_t h9_simple = run_benchmark_h9("simple");
        print_csv_row(&h9_simple);
        print_csv_row(&h9_fifo);
        print_csv_row(&h9_priority);
        benchmark_result_t worst_simple = run_benchmark_h9_worst("simple");
        benchmark_result_t worst_fifo = run_benchmark_h9_worst("fifo");
        benchmark_result_t worst_priority = run_benchmark_h9_worst("priority");
        print_csv_row(&worst_simple);
        print_csv_row(&worst_fifo);
        print_csv_row(&worst_priority);
        benchmark_result_t h10b_ws = run_benchmark_h10b("ws", 14);
        benchmark_result_t h10b_la = run_benchmark_h10b("la", 2);
        benchmark_result_t h10b_dp = run_benchmark_h10b("dp", 14);
        print_csv_row(&h10b_ws);
        print_csv_row(&h10b_la);
        print_csv_row(&h10b_dp);
        benchmark_result_t h10a_ws_1d = run_benchmark_h10a(SCHED_POLICY_WS, DATA_DIST_1D_STRIPED, 4);
        benchmark_result_t h10a_ws_2d = run_benchmark_h10a(SCHED_POLICY_WS, DATA_DIST_2D_BLOCK, 4);
        benchmark_result_t h10a_la_1d = run_benchmark_h10a(SCHED_POLICY_LA, DATA_DIST_1D_STRIPED, 4);
        benchmark_result_t h10a_la_2d = run_benchmark_h10a(SCHED_POLICY_LA, DATA_DIST_2D_BLOCK, 4);
        print_csv_row(&h10a_ws_1d);
        print_csv_row(&h10a_ws_2d);
        print_csv_row(&h10a_la_1d);
        print_csv_row(&h10a_la_2d);
        return 0;
    }

    benchmark_result_t res;
    if (strcmp(bench_str, "map") == 0) {
        res = run_benchmark_map(sched, dist, vicinity);
    } else if (strcmp(bench_str, "map_h8") == 0) {
        res = run_benchmark_map_h8(sched, dist, vicinity);
    } else if (strcmp(bench_str, "vecmul") == 0) {
        res = run_benchmark_vecmul(sched, dist, vicinity);
    } else if (strcmp(bench_str, "h9") == 0) {
        const char* p = "fifo";
        for (int i = 1; i + 1 < argc; i++) if (strcmp(argv[i], "--scheduler") == 0) p = argv[i + 1];
        res = run_benchmark_h9(p);
    } else if (strcmp(bench_str, "h9_worst") == 0) {
        const char* p = "fifo";
        for (int i = 1; i + 1 < argc; i++) if (strcmp(argv[i], "--scheduler") == 0) p = argv[i + 1];
        res = run_benchmark_h9_worst(p);
    } else if (strcmp(bench_str, "h10a") == 0) {
        res = run_benchmark_h10a(sched, dist, vicinity);
    } else if (strcmp(bench_str, "h10b") == 0) {
        const char* p = "dp";
        for (int i = 1; i + 1 < argc; i++) if (strcmp(argv[i], "--scheduler") == 0) p = argv[i + 1];
        res = run_benchmark_h10b(p, vicinity);
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
        printf("Simulated Migration Cost:  %llu cycles\n", (unsigned long long)res.simulated_migration_cost);
        printf("Migrated Bytes:            %llu\n", (unsigned long long)res.migrated_bytes);
        printf("Total Simulated Work:      %llu cycles\n", (unsigned long long)res.simulated_total_work);
        printf("Simulated Execution Time:  %llu cycles\n", (unsigned long long)res.simulated_execution_time);
        printf("Congestion Queue Delay:    %llu cycles\n", (unsigned long long)res.congestion_queue_cycles);
        printf("Max Link Busy Time:        %llu cycles\n", (unsigned long long)res.max_link_busy_cycles);
        printf("Task Count:                %u\n", res.task_count);
        printf("Steal Count:               %u\n", res.steal_count);
        printf("Local Executions:          %u\n", res.local_exec_count);
        printf("Remote Executions:         %u\n", res.remote_exec_count);
        printf("============================================================\n");
    }

    return 0;
}
