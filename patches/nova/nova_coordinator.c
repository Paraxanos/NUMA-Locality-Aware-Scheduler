#include "nova_coordinator.h"
#include "../../mir_queue.h"
#include "../../arch/mir_arch_sim.h"
#include <string.h>

void nova_coordinator_init(struct alloc_coordinator_t* coord) {
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
    }
}

struct alloc_context_t* nova_coordinator_get_snapshot(struct alloc_coordinator_t* coord) {
    return atomic_load_explicit(&coord->current, memory_order_acquire);
}

void nova_coordinator_report_steal(struct alloc_coordinator_t* coord, uint16_t node_id, bool success) {
    struct alloc_context_t* snap = nova_coordinator_get_snapshot(coord);
    if (success) {
        snap->steal_ok_ewma[node_id]++;
    } else {
        snap->steal_fail_ewma[node_id]++;
    }
}

void nova_coordinator_step_op(struct alloc_coordinator_t* coord, uint16_t node_id) {
    uint64_t ops = atomic_fetch_add_explicit(&coord->op_counter[node_id], 1, memory_order_relaxed);
    if ((ops % NOVA_EPOCH_OPS) == 0) {
        uint32_t expected = 0;
        if (atomic_compare_exchange_strong_explicit(&coord->refreshing[node_id], &expected, 1,
                                                    memory_order_acq_rel, memory_order_relaxed)) {
            struct alloc_context_t* cur = atomic_load_explicit(&coord->current, memory_order_relaxed);
            struct alloc_context_t* next_buf = (cur == &coord->buf[0]) ? &coord->buf[1] : &coord->buf[0];

            next_buf->epoch = ops;

            uint16_t num_nodes = mir_arch_sim_num_nodes();
            uint64_t total_steals = 0;
            for (uint16_t n = 0; n < num_nodes; n++) {
                next_buf->occ_snapshot[n] = mir_get_node_queue_size(n);
                total_steals += next_buf->steal_fail_ewma[n] + next_buf->steal_ok_ewma[n];
            }

            // High steal rate => lower alpha to prioritize load balancing
            if (total_steals > 100) {
                next_buf->alpha = 0.3f;
            } else if (total_steals < 10) {
                next_buf->alpha = 0.8f;
            } else {
                next_buf->alpha = 0.5f;
            }

            uint16_t max_diameter = mir_arch_sim_diameter();
            for (int w = 0; w < NOVA_MAX_WORKERS; w++) {
                uint16_t wn = mir_arch_sim_node_of(w);
                uint32_t r = cur->radius[w];
                if (cur->steal_fail_ewma[wn] > 10 && r < max_diameter) {
                    r++;
                } else if (cur->steal_ok_ewma[wn] > 10 && r > NOVA_RADIUS_MIN) {
                    r--;
                }
                next_buf->radius[w] = r;
            }

            atomic_store_explicit(&coord->current, next_buf, memory_order_release);
            atomic_store_explicit(&coord->refreshing[node_id], 0, memory_order_release);
        }
    }
}