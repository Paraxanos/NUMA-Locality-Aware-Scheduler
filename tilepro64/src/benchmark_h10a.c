#include "benchmark.h"
#include <string.h>

/* ============================================================================
 * H10a: 2D Stencil / Jacobi Placement Mismatch (1D-Striped vs 2D-Blocked)
 * ============================================================================
 * Models an 8x8 tile grid of 2D Jacobi/stencil tasks across 4 iterations
 * (64 tasks/sweep = 256 tasks total). Each task (tx, ty) accesses its 64 KB
 * interior block plus 4 neighbor halo borders (16 KB each: North, South,
 * East, West).
 *
 * Compares matched:
 *   - 1D Striped (DATA_DIST_1D_STRIPED): linear striping scatters N/S/E/W
 *     neighbors across distant tiles and saturates center-bisection links.
 *   - 2D Blocked (DATA_DIST_2D_BLOCK): maps 2D block (tx, ty) to tile (tx, ty)
 *     so all 4 neighbor halos stay strictly 1 Manhattan hop away.
 * ============================================================================ */

#define H10A_ITERS 4
#define H10A_BLOCK_BYTES (64 * 1024)
#define H10A_HALO_BYTES  (16 * 1024)
#define H10A_COMPUTE_CYCLES 80000
#define H10A_LINKS 112

static uint8_t xy_link_id(uint16_t a, uint16_t b) {
    uint16_t lo = a < b ? a : b;
    uint16_t hi = a < b ? b : a;
    if (lo / 8 == hi / 8) return (uint8_t)((lo / 8) * 7 + lo % 8);
    return (uint8_t)(56 + (lo % 8) * 7 + lo / 8);
}

static bool is_center_bisection_link(uint8_t link) {
    if (link < 56) return (link % 7) == 3;
    return ((link - 56) % 7) == 3;
}

static void record_xy_traffic(uint16_t src, uint16_t dst, uint64_t bytes, uint64_t link_cycles[H10A_LINKS]) {
    if (src == dst || bytes == 0) return;
    uint64_t flit_cycles = (bytes + 63) / 64; /* 64 B/cycle link width */
    uint16_t cur = src;
    while (cur % 8 != dst % 8) {
        uint16_t next = (uint16_t)((cur / 8) * 8 + (cur % 8 < dst % 8 ? cur % 8 + 1 : cur % 8 - 1));
        link_cycles[xy_link_id(cur, next)] += flit_cycles;
        cur = next;
    }
    while (cur / 8 != dst / 8) {
        uint16_t next = (uint16_t)((cur % 8) + (cur / 8 < dst / 8 ? (cur / 8 + 1) * 8 : (cur / 8 - 1) * 8));
        link_cycles[xy_link_id(cur, next)] += flit_cycles;
        cur = next;
    }
}

benchmark_result_t run_benchmark_h10a(scheduler_policy_t sched, data_dist_policy_t dist, uint16_t vicinity) {
    tilepro64_init();
    latency_model_init(TILEPRO64_LOCAL_LATENCY_CYCLES, TILEPRO64_REMOTE_BASE_LATENCY_CYCLES, TILEPRO64_PER_HOP_LATENCY_CYCLES);
    scheduler_init(sched, vicinity);

    uint64_t link_cycles[H10A_LINKS];
    memset(link_cycles, 0, sizeof(link_cycles));

    for (uint32_t iter = 0; iter < H10A_ITERS; iter++) {
        task_t* tasks[TILEPRO64_NUM_TILES];
        for (uint16_t tid = 0; tid < TILEPRO64_NUM_TILES; tid++) {
            uint16_t tx = (uint16_t)(tid % TILEPRO64_MESH_WIDTH);
            uint16_t ty = (uint16_t)(tid / TILEPRO64_MESH_WIDTH);
            tasks[tid] = task_create(iter * TILEPRO64_NUM_TILES + tid, H10A_COMPUTE_CYCLES);

            if (dist == DATA_DIST_2D_BLOCK) {
                distribute_2d_stencil_blocked(&tasks[tid]->data_dist, tx, ty, H10A_BLOCK_BYTES, H10A_HALO_BYTES);
            } else {
                distribute_2d_stencil_1d_striped(&tasks[tid]->data_dist, tx, ty, H10A_BLOCK_BYTES, H10A_HALO_BYTES);
            }
            scheduler_deal_task(tasks[tid], (sched == SCHED_POLICY_WS) ? 0 : tid);
        }

        uint32_t done = 0;
        while (done < TILEPRO64_NUM_TILES) {
            bool progress = false;
            for (uint16_t w = 0; w < TILEPRO64_NUM_TILES; w++) {
                task_t* t = scheduler_find_task(w);
                if (!t) continue;
                scheduler_execute_task(w, t);
                for (uint16_t h = 0; h < TILEPRO64_NUM_TILES; h++) {
                    if (t->data_dist.bytes_per_tile[h] > 0 && h != w) {
                        record_xy_traffic(h, w, t->data_dist.bytes_per_tile[h], link_cycles);
                    }
                }
                done++;
                progress = true;
            }
            if (!progress) break;
        }

        for (uint16_t tid = 0; tid < TILEPRO64_NUM_TILES; tid++) {
            task_destroy(tasks[tid]);
        }
    }

    uint64_t center_bisection_cycles = 0;
    uint64_t max_link = 0;
    for (uint8_t l = 0; l < H10A_LINKS; l++) {
        if (link_cycles[l] > max_link) max_link = link_cycles[l];
        if (is_center_bisection_link(l)) center_bisection_cycles += link_cycles[l];
    }

    benchmark_result_t r = {
        .benchmark_name = "h10a",
        .scheduler_name = scheduler_policy_name(sched),
        .dist_name = data_dist_policy_name(dist),
        .vicinity = vicinity,
        .simulated_comm_cost = g_scheduler.total_comm_cost,
        .simulated_migration_cost = g_scheduler.total_migration_cost,
        .migrated_bytes = g_scheduler.total_migrated_bytes,
        .simulated_total_work = g_scheduler.total_cycles_including_migration + center_bisection_cycles,
        .simulated_execution_time = g_scheduler.max_tile_cycles + max_link,
        .congestion_queue_cycles = center_bisection_cycles,
        .max_link_busy_cycles = max_link,
        .idle_probe_rounds = 0,
        .total_steal_distance = g_scheduler.total_steal_distance,
        .task_count = (uint32_t)g_scheduler.total_tasks_executed,
        .steal_count = (uint32_t)g_scheduler.total_tasks_stolen,
        .local_exec_count = (uint32_t)g_scheduler.local_executions,
        .remote_exec_count = (uint32_t)g_scheduler.remote_executions
    };

    tilepro64_cleanup();
    return r;
}
