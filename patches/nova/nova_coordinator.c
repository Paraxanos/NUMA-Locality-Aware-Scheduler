#include "nova_coordinator.h"
#include "mir_task_queue.h"
#include "mir_arch_sim.h"
#include <string.h>

void nova_coordinator_init(struct alloc_coordinator_t* coord) {
    if (!coord) return;
    memset(coord, 0, sizeof(*coord));
    for (int b = 0; b < 2; b++) {
        coord->buf[b].epoch = 0;
        coord->buf[b].alpha = 0.5f;
        for (int w = 0; w < NOVA_MAX_WORKERS; w++) {
            coord->buf[b].radius[w] = NOVA_RADIUS_MIN;
        }
    }
    atomic_init(&coord->current, &coord->buf[0]);
    for (int i = 0; i < NOVA_MAX_NODES; i++) {
        atomic_init(&coord->op_counter[i], 0);
        atomic_init(&coord->refreshing[i], 0);
        atomic_init(&coord->steal_ok_count[i], 0);
        atomic_init(&coord->steal_fail_count[i], 0);
        coord->last_steal_ok[i] = 0;
        coord->last_steal_fail[i] = 0;
        coord->steal_ok_ewma[i] = 0.0f;
        coord->steal_fail_ewma[i] = 0.0f;
    }
    pthread_rwlock_init(&coord->buf_lock, NULL);
}

void nova_coordinator_destroy(struct alloc_coordinator_t* coord) {
    if (!coord) return;
    pthread_rwlock_destroy(&coord->buf_lock);
}

struct alloc_context_t* nova_coordinator_get_snapshot(struct alloc_coordinator_t* coord) {
    if (!coord) return NULL;
    return atomic_load_explicit(&coord->current, memory_order_acquire);
}

void nova_coordinator_report_steal(struct alloc_coordinator_t* coord, uint16_t node_id, bool success) {
    if (!coord || node_id >= NOVA_MAX_NODES) return;
    if (success) {
        atomic_fetch_add_explicit(&coord->steal_ok_count[node_id], 1, memory_order_relaxed);
    } else {
        atomic_fetch_add_explicit(&coord->steal_fail_count[node_id], 1, memory_order_relaxed);
    }
}

void nova_coordinator_step_op(struct alloc_coordinator_t* coord, uint16_t node_id) {
    if (!coord || node_id >= NOVA_MAX_NODES) return;

    uint64_t ops = atomic_fetch_add_explicit(&coord->op_counter[node_id], 1, memory_order_relaxed);
    if ((ops % NOVA_EPOCH_OPS) == 0) {
        uint32_t expected = 0;
        if (atomic_compare_exchange_strong_explicit(&coord->refreshing[node_id], &expected, 1,
                                                    memory_order_acq_rel, memory_order_relaxed)) {
            struct alloc_context_t* cur = atomic_load_explicit(&coord->current, memory_order_relaxed);
            struct alloc_context_t* next_buf = (cur == &coord->buf[0]) ? &coord->buf[1] : &coord->buf[0];

            pthread_rwlock_wrlock(&coord->buf_lock);

            next_buf->epoch = ops;

            uint16_t num_nodes = mir_arch_sim_num_nodes();
            uint64_t total_recent_steals = 0;
            const float beta = 0.7f; // EWMA smoothing factor

            for (uint16_t n = 0; n < num_nodes; n++) {
                next_buf->occ_snapshot[n] = mir_get_node_task_queue_size(n);

                uint64_t cur_ok = atomic_load_explicit(&coord->steal_ok_count[n], memory_order_relaxed);
                uint64_t cur_fail = atomic_load_explicit(&coord->steal_fail_count[n], memory_order_relaxed);

                uint64_t delta_ok = (cur_ok >= coord->last_steal_ok[n]) ? (cur_ok - coord->last_steal_ok[n]) : cur_ok;
                uint64_t delta_fail = (cur_fail >= coord->last_steal_fail[n]) ? (cur_fail - coord->last_steal_fail[n]) : cur_fail;

                coord->last_steal_ok[n] = cur_ok;
                coord->last_steal_fail[n] = cur_fail;

                // Update EWMA
                coord->steal_ok_ewma[n] = (beta * coord->steal_ok_ewma[n]) + ((1.0f - beta) * (float)delta_ok);
                coord->steal_fail_ewma[n] = (beta * coord->steal_fail_ewma[n]) + ((1.0f - beta) * (float)delta_fail);

                next_buf->steal_ok_ewma[n] = (uint64_t)(coord->steal_ok_ewma[n] + 0.5f);
                next_buf->steal_fail_ewma[n] = (uint64_t)(coord->steal_fail_ewma[n] + 0.5f);

                total_recent_steals += delta_ok + delta_fail;
            }

            // Dynamic alpha modulation
            if (total_recent_steals > 50) {
                next_buf->alpha = 0.3f; // Heavy contention: bias towards load balance
            } else if (total_recent_steals < 5) {
                next_buf->alpha = 0.8f; // Low contention: bias towards data locality
            } else {
                next_buf->alpha = 0.5f; // Balanced default
            }

            uint16_t max_diameter = mir_arch_sim_diameter();
            for (int w = 0; w < NOVA_MAX_WORKERS; w++) {
                uint16_t wn = mir_arch_sim_node_of(w);
                uint32_t r = cur->radius[w];
                if (coord->steal_fail_ewma[wn] > 5.0f && r < max_diameter) {
                    r++;
                } else if (coord->steal_ok_ewma[wn] > 5.0f && r > NOVA_RADIUS_MIN) {
                    r--;
                }
                next_buf->radius[w] = r;
            }

            atomic_store_explicit(&coord->current, next_buf, memory_order_release);
            pthread_rwlock_unlock(&coord->buf_lock);

            atomic_store_explicit(&coord->refreshing[node_id], 0, memory_order_release);
        }
    }
}
