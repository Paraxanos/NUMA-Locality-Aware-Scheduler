#include "benchmark.h"
#include <stdlib.h>
#include <string.h>

#define H9_LINKS 112
#define H9_TRANSFERS 256
#define H9_BW_BYTES_PER_CYCLE 64

typedef struct {
    uint8_t path[15];
    uint8_t hops;
    uint8_t next_hop;
    uint32_t bytes;
    uint32_t release;
    uint8_t priority;
    uint16_t id;
} h9_transfer_t;

typedef struct {
    uint64_t cycles;
    uint64_t busy[H9_LINKS];
    uint64_t queued_cycles;
    uint64_t max_queue;
} h9_result_t;

static uint8_t h9_link(uint16_t a, uint16_t b) {
    uint16_t lo = a < b ? a : b;
    uint16_t hi = a < b ? b : a;
    /* Horizontal links 0..55, vertical links 56..111. */
    if (lo / 8 == hi / 8) return (uint8_t)((lo / 8) * 7 + lo % 8);
    return (uint8_t)(56 + (lo % 8) * 7 + lo / 8);
}

static void h9_route(h9_transfer_t* t, uint16_t src, uint16_t dst) {
    uint16_t cur = src;
    while (cur % 8 != dst % 8) {
        uint16_t next = (cur / 8) * 8 + (cur % 8 < dst % 8 ? cur % 8 + 1 : cur % 8 - 1);
        t->path[t->hops++] = h9_link(cur, next); cur = next;
    }
    while (cur / 8 != dst / 8) {
        uint16_t next = (cur % 8) + (cur / 8 < dst / 8 ? (cur / 8 + 1) * 8 : (cur / 8 - 1) * 8);
        t->path[t->hops++] = h9_link(cur, next); cur = next;
    }
}

static void h9_add_transfer(h9_transfer_t* t, uint16_t id, uint16_t src, uint16_t dst,
                            uint32_t bytes, uint32_t release, uint8_t priority) {
    memset(t, 0, sizeof(*t));
    t->id = id; t->bytes = bytes; t->release = release; t->priority = priority;
    h9_route(t, src, dst);
}

static h9_result_t h9_simulate(h9_transfer_t* input, uint32_t count, bool priority) {
    h9_result_t result = {0};
    uint64_t remaining[H9_TRANSFERS];
    bool finished[H9_TRANSFERS] = {false};
    for (uint32_t i = 0; i < count; i++) remaining[i] = (input[i].bytes + H9_BW_BYTES_PER_CYCLE - 1) / H9_BW_BYTES_PER_CYCLE;
    uint32_t finished_count = 0;
    while (finished_count < count && result.cycles < 10000000) {
        bool advance[H9_TRANSFERS] = {false};
        for (uint8_t link = 0; link < H9_LINKS; link++) {
            int chosen = -1;
            for (uint32_t i = 0; i < count; i++) {
                h9_transfer_t* t = &input[i];
                if (finished[i] || t->release > result.cycles || t->next_hop >= t->hops || t->path[t->next_hop] != link) continue;
                if (chosen < 0 || (priority ? (t->priority > input[chosen].priority ||
                    (t->priority == input[chosen].priority && t->id < input[chosen].id)) : t->id < input[chosen].id)) chosen = (int)i;
            }
            if (chosen >= 0) advance[chosen] = true;
        }
        for (uint32_t i = 0; i < count; i++) {
            if (finished[i]) continue;
            if (input[i].release > result.cycles) continue;
            if (!advance[i]) { result.queued_cycles++; continue; }
            uint8_t link = input[i].path[input[i].next_hop];
            result.busy[link]++;
            remaining[i]--;
            if (remaining[i] == 0) {
                input[i].next_hop++;
                remaining[i] = (input[i].bytes + H9_BW_BYTES_PER_CYCLE - 1) / H9_BW_BYTES_PER_CYCLE;
                if (input[i].next_hop >= input[i].hops) { finished[i] = true; finished_count++; }
            }
        }
        result.cycles++;
    }
    for (uint8_t l = 0; l < H9_LINKS; l++) if (result.busy[l] > result.max_queue) result.max_queue = result.busy[l];
    return result;
}

static benchmark_result_t run_h9_trace(const char* policy, bool worst_case) {
    h9_transfer_t traffic[H9_TRANSFERS];
    uint32_t n = 0;
    if (worst_case) {
        /* All transfers collide on the same seven-hop row-3 route and center link. */
        for (n = 0; n < 32; n++)
            h9_add_transfer(&traffic[n], (uint16_t)n, 24, 31, 4096, 0, (uint8_t)(1 + (n % 8)));
    } else {
        /* Symmetric pressure across the central vertical bisection. */
        for (uint16_t y = 0; y < 8; y++) for (uint16_t x = 0; x < 4; x++)
            h9_add_transfer(&traffic[n], (uint16_t)n, (uint16_t)(y * 8 + x), (uint16_t)(y * 8 + 7 - x),
                            4096, (uint32_t)((y * 8 + x) % 8), (uint8_t)(1 + (n % 8))), n++;
    }
    h9_result_t r;
    if (policy && strcmp(policy, "simple") == 0) {
        /* Distance-only baseline: same XY route and serialization per transfer,
         * but omit shared-link contention and arbitration between transfers. */
        memset(&r, 0, sizeof(r));
        for (uint32_t i = 0; i < n; i++) {
            uint64_t service = (traffic[i].bytes + H9_BW_BYTES_PER_CYCLE - 1) / H9_BW_BYTES_PER_CYCLE;
            uint64_t finish = traffic[i].release + service * traffic[i].hops;
            if (finish > r.cycles) r.cycles = finish;
            for (uint8_t h = 0; h < traffic[i].hops; h++) r.busy[traffic[i].path[h]] += service;
        }
        for (uint8_t l = 0; l < H9_LINKS; l++) if (r.busy[l] > r.max_queue) r.max_queue = r.busy[l];
    } else {
        r = h9_simulate(traffic, n, policy && strcmp(policy, "priority") == 0);
    }
    return (benchmark_result_t){ .benchmark_name = worst_case ? "h9_worst" : "h9",
        .scheduler_name = policy ? policy : "fifo",
        .dist_name = "center-bisection", .simulated_comm_cost = r.queued_cycles,
        .simulated_migration_cost = 0, .migrated_bytes = 0,
        .simulated_total_work = r.cycles, .simulated_execution_time = r.cycles,
        .congestion_queue_cycles = r.queued_cycles, .max_link_busy_cycles = r.max_queue,
        .task_count = n, .steal_count = 0, .local_exec_count = 0, .remote_exec_count = 0 };
}

benchmark_result_t run_benchmark_h9(const char* policy) { return run_h9_trace(policy, false); }
benchmark_result_t run_benchmark_h9_worst(const char* policy) { return run_h9_trace(policy, true); }

#define MAP_NUM_TASKS 64
#define MAP_VEC_BYTES (64 * 1024)
#define MAP_COMPUTE_CYCLES 150000

static void finish_migration(uint16_t tile, uint64_t cost) {
    g_scheduler.tile_busy_cycles[tile] += cost;
    if (g_scheduler.tile_busy_cycles[tile] > g_scheduler.max_tile_cycles)
        g_scheduler.max_tile_cycles = g_scheduler.tile_busy_cycles[tile];
}

benchmark_result_t run_benchmark_map_h8(scheduler_policy_t sched, data_dist_policy_t dist, uint16_t vicinity) {
    tilepro64_init();
    latency_model_init(TILEPRO64_LOCAL_LATENCY_CYCLES, TILEPRO64_REMOTE_BASE_LATENCY_CYCLES, TILEPRO64_PER_HOP_LATENCY_CYCLES);
    scheduler_init(sched, vicinity);
    for (uint16_t c = 0; c < 2; c++) {
        data_dist_t object;
        if (dist == DATA_DIST_COARSE) distribute_coarse(&object, (uint16_t)(63 - c), MAP_VEC_BYTES / 2);
        else distribute_fine(&object, MAP_VEC_BYTES / 2);
        uint32_t uses = MAP_NUM_TASKS / 2;
        for (uint32_t use = 0; use < uses; use++) {
            uint16_t worker = 0;
            uint64_t access = scheduler_calc_comm_cost(&object, worker);
            uint64_t work = MAP_COMPUTE_CYCLES + access;
            g_scheduler.total_comm_cost += access;
            g_scheduler.total_cycles_including_migration += work;
            g_scheduler.tile_busy_cycles[worker] += work;
            if (g_scheduler.tile_busy_cycles[worker] > g_scheduler.max_tile_cycles)
                g_scheduler.max_tile_cycles = g_scheduler.tile_busy_cycles[worker];
            g_scheduler.total_tasks_executed++;
            if (use == 0 && sched == SCHED_POLICY_AM) {
                uint64_t migration = scheduler_adapt_data_placement(&object, worker, 1, uses - 1);
                g_scheduler.total_cycles_including_migration += migration;
                finish_migration(worker, migration);
            }
        }
    }
    benchmark_result_t result = {
        .benchmark_name = "map_h8", .scheduler_name = scheduler_policy_name(sched), .dist_name = data_dist_policy_name(dist),
        .vicinity = vicinity, .simulated_comm_cost = g_scheduler.total_comm_cost,
        .simulated_migration_cost = g_scheduler.total_migration_cost,
        .migrated_bytes = g_scheduler.total_migrated_bytes,
        .simulated_total_work = g_scheduler.total_cycles_including_migration,
        .simulated_execution_time = g_scheduler.max_tile_cycles,
        .congestion_queue_cycles = 0, .max_link_busy_cycles = 0,
        .task_count = MAP_NUM_TASKS, .steal_count = 0,
        .local_exec_count = 0, .remote_exec_count = MAP_NUM_TASKS
    };
    tilepro64_cleanup();
    return result;
}

static benchmark_result_t collect_result(scheduler_policy_t sched, data_dist_policy_t dist, uint16_t vicinity) {
    benchmark_result_t r = {
        .benchmark_name = "map", .scheduler_name = scheduler_policy_name(sched),
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

benchmark_result_t run_benchmark_map(scheduler_policy_t sched, data_dist_policy_t dist, uint16_t vicinity) {
    tilepro64_init();
    latency_model_init(TILEPRO64_LOCAL_LATENCY_CYCLES, TILEPRO64_REMOTE_BASE_LATENCY_CYCLES, TILEPRO64_PER_HOP_LATENCY_CYCLES);

    /* 128 KB array represented as two 64 KB chunks shared by all tasks. */
    data_dist_t shared[2];
    for (uint16_t c = 0; c < 2; c++) {
    if (dist == DATA_DIST_COARSE) distribute_coarse(&shared[c], (uint16_t)(63 - c), MAP_VEC_BYTES / 2);
        else distribute_fine(&shared[c], MAP_VEC_BYTES / 2);
    }

    scheduler_policy_t placement = sched == SCHED_POLICY_AM ? SCHED_POLICY_LA : sched;
    scheduler_init(placement, vicinity);
    task_t* tasks[MAP_NUM_TASKS];
    for (uint32_t i = 0; i < MAP_NUM_TASKS; i++) {
        tasks[i] = task_create(i, MAP_COMPUTE_CYCLES);
        tasks[i]->data_dist = shared[i % 2];
        scheduler_deal_task(tasks[i], 0);
    }

    uint32_t completed = 0;
    while (completed < MAP_NUM_TASKS) {
        bool progress = false;
        for (uint16_t w = 0; w < TILEPRO64_NUM_TILES; w++) {
            task_t* t = scheduler_find_task(w);
            if (!t) continue;
            scheduler_execute_task(w, t);
            completed++;
            progress = true;
        }
        if (!progress) break;
    }

    benchmark_result_t result = collect_result(sched, dist, vicinity);

    for (uint32_t i = 0; i < MAP_NUM_TASKS; i++) task_destroy(tasks[i]);
    tilepro64_cleanup();
    return result;
}
