#include "../src/tilepro64.h"
#include "../src/home_cache.h"
#include "../src/latency.h"
#include "../src/data_distribution.h"
#include "../src/task.h"
#include "../src/queue.h"
#include "../src/scheduler.h"
#include "../src/benchmark.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (cond) { \
        printf("  [PASS] %s\n", msg); \
        g_tests_passed++; \
    } else { \
        printf("  [FAIL] %s\n", msg); \
        g_tests_failed++; \
    } \
} while(0)

// Validation 1: 64 tiles exist
void test_1_sixty_four_tiles_exist(void) {
    tilepro64_init();
    int count = 0;
    for (int i = 0; i < TILEPRO64_NUM_TILES; i++) {
        if (tile_get(i) != NULL && tile_get(i)->id == i) {
            count++;
        }
    }
    TEST_ASSERT(count == 64, "Validation 1: Exactly 64 tiles exist (ID 0 to 63)");
    tilepro64_cleanup();
}

// Validation 2: Tiles form an 8 x 8 mesh
void test_2_eight_by_eight_mesh(void) {
    tilepro64_init();
    bool mesh_ok = true;
    for (int i = 0; i < TILEPRO64_NUM_TILES; i++) {
        tile_t* t = tile_get(i);
        if (t->x != (i % 8) || t->y != (i / 8)) {
            mesh_ok = false;
        }
    }
    // Check Manhattan distance from (0,0) to (7,7) == 14
    uint16_t max_dist = tile_distance(0, 63);
    TEST_ASSERT(mesh_ok && max_dist == 14, "Validation 2: Tiles form an 8x8 mesh (corners distance = 14 hops)");
    tilepro64_cleanup();
}

// Validation 3: Each tile has one queue
void test_3_each_tile_one_queue(void) {
    tilepro64_init();
    bool queues_ok = true;
    for (int i = 0; i < TILEPRO64_NUM_TILES; i++) {
        tile_t* t = tile_get(i);
        if (t->queue == NULL || queue_size(t->queue) != 0) {
            queues_ok = false;
        }
    }
    TEST_ASSERT(queues_ok, "Validation 3: Each of the 64 tiles has its own dedicated task queue");
    tilepro64_cleanup();
}

// Validation 4: Data can be assigned to home caches
void test_4_data_assigned_to_home_caches(void) {
    data_dist_t dist;
    data_dist_init(&dist);
    data_dist_add(&dist, 5, 1024);
    data_dist_add(&dist, 12, 2048);
    TEST_ASSERT(dist.bytes_per_tile[5] == 1024 && dist.bytes_per_tile[12] == 2048 && dist.total_bytes == 3072,
                "Validation 4: Data successfully assigned to designated tile home caches");
}

// Validation 5: Task's D[64] distribution is correct
void test_5_task_distribution_vector(void) {
    task_t* t = task_create(1, 1000);
    distribute_fine(&t->data_dist, 6400); // 100 bytes per tile across 64 tiles
    bool fine_ok = (data_dist_total(&t->data_dist) == 6400);
    for (int i = 0; i < 64; i++) {
        if (t->data_dist.bytes_per_tile[i] != 100) fine_ok = false;
    }

    distribute_coarse(&t->data_dist, 7, 5000);
    bool coarse_ok = (t->data_dist.bytes_per_tile[7] == 5000 && data_dist_total(&t->data_dist) == 5000 && t->data_dist.bytes_per_tile[0] == 0);

    TEST_ASSERT(fine_ok && coarse_ok, "Validation 5: Task D[64] distribution vector accurately reflects fine and coarse schemes");
    task_destroy(t);
}

// Validation 6: Locality cost is calculated correctly
void test_6_locality_cost_calculation(void) {
    latency_model_init(10, 38, 2);
    // Tile 0 to Tile 0: latency is 10 cycles
    // Tile 0 (0,0) to Tile 1 (1,0): distance 1 hop -> latency 38 + 2*1 = 40 cycles
    // Tile 0 (0,0) to Tile 63 (7,7): distance 14 hops -> latency 38 + 2*14 = 66 cycles
    uint32_t lat_local = latency_get(0, 0);
    uint32_t lat_hop1 = latency_get(0, 1);
    uint32_t lat_hop14 = latency_get(0, 63);

    data_dist_t dist;
    data_dist_init(&dist);
    data_dist_add(&dist, 0, 64); // 1 cache line on tile 0
    uint64_t cost_on_0 = scheduler_calc_comm_cost(&dist, 0);  // 1 line * 10 = 10
    uint64_t cost_on_63 = scheduler_calc_comm_cost(&dist, 63); // 1 line * 66 = 66

    TEST_ASSERT(lat_local == 10 && lat_hop1 == 40 && lat_hop14 == 66 && cost_on_0 == 10 && cost_on_63 == 66,
                "Validation 6: Locality access cost calculated correctly (local=10, 1-hop=40, 14-hop=66)");
}

// Validation 7: Task is placed according to TILEPro64 work-dealing algorithm
void test_7_work_dealing_algorithm(void) {
    tilepro64_init();
    latency_model_init(10, 38, 2);
    scheduler_init(SCHED_POLICY_LA, 4);

    // Create a task with 128 KB homed on Tile 42 (clears 64 KB significance gate)
    task_t* t = task_create(10, 1000);
    distribute_coarse(&t->data_dist, 42, 128 * 1024);

    // Tile 0 deals the task
    uint16_t placed_tile = scheduler_deal_task(t, 0);

    // Locality dealer should select tile 42 (minimizes comm_cost)
    TEST_ASSERT(placed_tile == 42 && t->assigned_tile == 42 && queue_size(tile_get(42)->queue) == 1,
                "Validation 7: Locality-aware work dealer routes task to optimal home tile 42");

    task_destroy(t);
    tilepro64_cleanup();
}

// Validation 8: Worker can execute from local queue
void test_8_worker_executes_local(void) {
    tilepro64_init();
    latency_model_init(10, 38, 2);
    scheduler_init(SCHED_POLICY_LA, 4);

    task_t* t = task_create(20, 5000);
    distribute_coarse(&t->data_dist, 5, 128 * 1024);
    scheduler_deal_task(t, 5);

    task_t* found = scheduler_find_task(5);
    bool popped_ok = (found == t && !t->is_stolen);
    scheduler_execute_task(5, found);

    TEST_ASSERT(popped_ok && t->state == TASK_STATE_EXECUTED && t->executed_tile == 5,
                "Validation 8: Worker 5 executes task directly from its local queue without stealing");

    task_destroy(t);
    tilepro64_cleanup();
}

// Validation 9: Worker can steal only from queues allowed by its vicinity
void test_9_vicinity_bounded_stealing(void) {
    tilepro64_init();
    latency_model_init(10, 38, 2);
    
    // Set vicinity = 1 hop
    scheduler_init(SCHED_POLICY_LA, 1);

    // Place a task on tile 2 (x=2, y=0). Distance from tile 0 (x=0, y=0) is 2 hops.
    task_t* t_far = task_create(30, 5000);
    queue_push(tile_get(2)->queue, t_far);

    // Tile 0 tries to steal with vicinity=1 -> should FAIL (distance 2 > 1)
    task_t* stolen_far = scheduler_find_task(0);
    bool far_rejected = (stolen_far == NULL);

    // Place task on tile 1 (x=1, y=0). Distance from tile 0 is 1 hop.
    task_t* t_near = task_create(31, 5000);
    queue_push(tile_get(1)->queue, t_near);

    // Tile 0 tries to steal with vicinity=1 -> should SUCCEED (distance 1 <= 1)
    task_t* stolen_near = scheduler_find_task(0);
    bool near_accepted = (stolen_near == t_near && t_near->is_stolen);

    TEST_ASSERT(far_rejected && near_accepted,
                "Validation 9: Worker steals strictly within vicinity radius (rejects 2-hop, accepts 1-hop)");

    task_destroy(t_far);
    task_destroy(t_near);
    tilepro64_cleanup();
}

// Validation 10: Task dependencies are respected
void test_10_task_dependencies_respected(void) {
    task_t* t_parent = task_create(100, 1000);
    task_t* t_child = task_create(101, 1000);

    task_add_dependency(t_child, t_parent);
    bool initially_not_ready = !task_is_ready(t_child);

    t_parent->state = TASK_STATE_EXECUTED;
    task_resolve_dep(t_child);
    bool becomes_ready = task_is_ready(t_child);

    TEST_ASSERT(initially_not_ready && becomes_ready,
                "Validation 10: Task dependencies correctly gated and resolved upon predecessor completion");

    task_destroy(t_parent);
    task_destroy(t_child);
}

// Validation 11: Every task executes exactly once
void test_11_every_task_executes_exactly_once(void) {
    tilepro64_init();
    latency_model_init(10, 38, 2);
    scheduler_init(SCHED_POLICY_WS, 14);

    #define BATCH_TASKS 64
    task_t* tasks[BATCH_TASKS];
    for (int i = 0; i < BATCH_TASKS; i++) {
        tasks[i] = task_create(200 + i, 1000);
        // Deal all to tile 0
        scheduler_deal_task(tasks[i], 0);
    }

    uint32_t total_executed = 0;
    while (total_executed < BATCH_TASKS) {
        for (uint16_t w = 0; w < TILEPRO64_NUM_TILES; w++) {
            task_t* t = scheduler_find_task(w);
            if (t) {
                scheduler_execute_task(w, t);
                total_executed++;
            }
        }
    }

    bool all_executed_once = (total_executed == BATCH_TASKS && g_scheduler.total_tasks_executed == BATCH_TASKS);
    for (int i = 0; i < BATCH_TASKS; i++) {
        if (tasks[i]->state != TASK_STATE_EXECUTED) all_executed_once = false;
        task_destroy(tasks[i]);
    }

    TEST_ASSERT(all_executed_once,
                "Validation 11: In a batch of 64 tasks across work-stealing workers, every task executes exactly once");
    tilepro64_cleanup();
}

// Validation 12: Adaptive placement migrates only when future savings repay it.
void test_12_adaptive_migration_break_even(void) {
    tilepro64_init();
    latency_model_init(10, 38, 2);
    scheduler_init(SCHED_POLICY_AM, 4);

    data_dist_t dist;
    data_dist_init(&dist);
    data_dist_add(&dist, 0, 4096);
    data_dist_add(&dist, 8, 4096);
    uint64_t before = scheduler_calc_comm_cost(&dist, 0);
    uint64_t low_reuse_move = scheduler_adapt_data_placement(&dist, 0, 1, 1);
    bool rejected = (low_reuse_move == 0 && dist.bytes_per_tile[8] == 4096);

    uint64_t high_reuse_move = scheduler_adapt_data_placement(&dist, 0, 1, 100);
    uint64_t after = scheduler_calc_comm_cost(&dist, 0);
    bool moved = (high_reuse_move > 0 && dist.bytes_per_tile[8] == 0 &&
                  dist.bytes_per_tile[0] == 8192 && after < before &&
                  g_scheduler.total_migration_cost == high_reuse_move &&
                  g_scheduler.total_migrated_bytes == 4096);

    TEST_ASSERT(rejected && moved,
                "Validation 12: Adaptive migration rejects poor reuse and moves data when estimated savings exceed cost");
    tilepro64_cleanup();
}

// Validation 13: H9 policies arbitrate identical link traffic.
void test_13_h9_link_priority(void) {
    benchmark_result_t fifo = run_benchmark_h9("fifo");
    benchmark_result_t priority = run_benchmark_h9("priority");
    bool same_work = fifo.task_count == priority.task_count &&
                     fifo.max_link_busy_cycles == priority.max_link_busy_cycles;
    bool lower_queue_delay = priority.congestion_queue_cycles < fifo.congestion_queue_cycles;
    TEST_ASSERT(same_work && lower_queue_delay,
                "Validation 13: H9 priority arbitration reduces queued transfer delay for identical traffic");
}

// Validation 14: Dynamic stealing picks the task with the longest estimated completion time.
void test_14_dynamic_steal_uses_remaining_time(void) {
    tilepro64_init();
    latency_model_init(10, 38, 2);
    scheduler_init(SCHED_POLICY_DP, 14);
    task_t* short_a = task_create(1, 1000);
    task_t* long_task = task_create(2, 10000);
    task_t* short_b = task_create(3, 1000);
    distribute_coarse(&short_a->data_dist, 63, 4096);
    distribute_coarse(&long_task->data_dist, 63, 4096);
    distribute_coarse(&short_b->data_dist, 63, 4096);
    scheduler_deal_task(short_a, 63);
    scheduler_deal_task(long_task, 63);
    scheduler_deal_task(short_b, 63);

    task_t* chosen = scheduler_find_task_dynamic(62, 0, 3);
    bool selected_long = chosen == long_task && chosen->is_stolen && queue_size(tile_get(63)->queue) == 2;
    TEST_ASSERT(selected_long,
                "Validation 14: Dynamic steal ranks queued tasks by estimated remaining completion time");
    task_destroy(short_a);
    task_destroy(long_task);
    task_destroy(short_b);
    tilepro64_cleanup();
}

// Validation 15: H10a 2D-blocked stencil placement cuts remote access cost & bisection traffic vs 1D-striped.
void test_15_h10a_2d_blocked_stencil(void) {
    benchmark_result_t striped = run_benchmark_h10a(SCHED_POLICY_LA, DATA_DIST_1D_STRIPED, 4);
    benchmark_result_t blocked = run_benchmark_h10a(SCHED_POLICY_LA, DATA_DIST_2D_BLOCK, 4);
    bool lower_comm = blocked.simulated_comm_cost < striped.simulated_comm_cost;
    bool lower_bisection = blocked.congestion_queue_cycles < striped.congestion_queue_cycles;
    bool faster_makespan = blocked.simulated_execution_time < striped.simulated_execution_time;
    TEST_ASSERT(lower_comm && lower_bisection && faster_makespan,
                "Validation 15: H10a 2D-blocked stencil preserves neighbor locality and slashes center-bisection traffic vs 1D striping");
}

int main(void) {
    printf("============================================================\n");
    printf("TILEPro64 Manycore Simulator: Section 21 Validation Suite\n");
    printf("============================================================\n");

    test_1_sixty_four_tiles_exist();
    test_2_eight_by_eight_mesh();
    test_3_each_tile_one_queue();
    test_4_data_assigned_to_home_caches();
    test_5_task_distribution_vector();
    test_6_locality_cost_calculation();
    test_7_work_dealing_algorithm();
    test_8_worker_executes_local();
    test_9_vicinity_bounded_stealing();
    test_10_task_dependencies_respected();
    test_11_every_task_executes_exactly_once();
    test_12_adaptive_migration_break_even();
    test_13_h9_link_priority();
    test_14_dynamic_steal_uses_remaining_time();
    test_15_h10a_2d_blocked_stencil();

    printf("============================================================\n");
    printf("Summary: %d Passed, %d Failed (out of 15)\n", g_tests_passed, g_tests_failed);
    printf("============================================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
