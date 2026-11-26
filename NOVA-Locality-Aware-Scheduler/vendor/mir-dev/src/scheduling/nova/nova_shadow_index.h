#ifndef NOVA_SHADOW_INDEX_H
#define NOVA_SHADOW_INDEX_H 1

#include <stdint.h>
#include "mir_task.h"
#include "nova_coordinator.h" /* NOVA_SHADOW_K */

BEGIN_C_DECLS

/* One per per-node task queue -- docs/10-nova-architecture.md section4.4.1/section5.2. Fixed-
 * capacity ring, holds the most recent NOVA_SHADOW_K tasks pushed to that queue. */
struct shadow_entry_t {
    struct mir_task_t* task;
    struct mir_mem_node_dist_t* dist; /* == task->dist_by_access_type[MIR_DATA_ACCESS_READ] */
    volatile uint32_t generation; /* volatile: a reader uses this as a before/after fence around
                                      its task+dist reads (nova_shadow_peek) to detect a torn read
                                      against a concurrent append on the same slot -- needs a real
                                      memory read each time, not a value the compiler can cache in
                                      a register across the two checks. */
};

struct shadow_index_t {
    struct shadow_entry_t entries[NOVA_SHADOW_K];
    volatile uint32_t head; /* next write slot, wraps mod NOVA_SHADOW_K; single-writer (the node's
                                own dealer, same invariant push_numa already relies on) */
};

void nova_shadow_index_init(struct shadow_index_t* idx);

/* Lock-free, single-writer append -- called by the Dealer at the same point it pushes the task to
 * the real queue (section4.4.1: "this matches MIR's existing invariant that only local dealing
 * pushes to a node's queue"). */
void nova_shadow_append(struct shadow_index_t* idx, struct mir_task_t* task);

/* Lock-free, multi-reader peek. Copies up to NOVA_SHADOW_K entries (skipping any whose task is
 * already task->taken) into out_tasks/out_dists; returns the count actually copied. */
int nova_shadow_peek(struct shadow_index_t* idx, struct mir_task_t** out_tasks,
                      struct mir_mem_node_dist_t** out_dists);

END_C_DECLS

#endif
