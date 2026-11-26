#ifndef NOVA_COORDINATOR_H
#define NOVA_COORDINATOR_H 1

#include <stdint.h>
#include "mir_types.h"
#include "mir_worker.h"

BEGIN_C_DECLS

#define NOVA_EPOCH_OPS      64      /* ops per node before a Coordinator refresh, doc 10 section5.4 */
#define NOVA_SHADOW_K        8      /* candidates inspected per steal attempt, doc 10 section5.4 */
#define NOVA_STALE_MAX     512      /* ops since last successful refresh before degrading, section10 */
#define NOVA_RADIUS_MIN      1

/* One published, immutable snapshot. Readers dereference the current pointer once per decision and
 * use that snapshot throughout -- never re-read mid-decision (doc 10 section7). radius[] is indexed
 * by worker->id (doc 10 section5.1); occ_snapshot/steal_*_ewma are indexed by node id and sized to
 * runtime->arch->num_nodes at create() time (not a fixed MIR_ARCH_MAX_NODES -- no such constant
 * exists in this MIR snapshot, so we size dynamically like mir_sched_pol_numa.c's own queues[] does).
 */
struct alloc_context_t {
    uint64_t epoch;
    float    alpha;
    uint32_t radius[MIR_WORKER_MAX_COUNT];
    uint64_t* occ_snapshot;       /* [num_nodes] */
    float*    steal_fail_ewma;    /* [num_nodes] */
    float*    steal_ok_ewma;      /* [num_nodes] */
};

/* Lifecycle -- called from each ablation policy's create()/destroy() (nova-a/nova-ab/nova all share
 * this one Coordinator implementation; only which of its outputs they *read* differs). */
void nova_coordinator_create(void);
void nova_coordinator_destroy(void);

/* Data-plane read: one atomic load, no lock. Call once per decision and reuse the pointer. */
const struct alloc_context_t* nova_coordinator_snapshot(void);

/* Hot-path hooks, called from push/pop. Increments op_counter[node]; if this increment lands this
 * worker the epoch's election (CAS on refreshing[node]), it does the O(nodes) recompute and
 * publishes a new snapshot. Every other caller pays only the one atomic increment. */
void nova_coordinator_on_op(uint16_t node);

/* Called once per steal attempt (success or failure) by the Stealer/Radius Controller. O(1) atomic
 * add to a pending counter; the EWMA decay itself is applied only at the next epoch refresh (doc 10
 * section7's "hot-path update is one atomic add, not a read-modify-write of a float"). */
void nova_coordinator_report_steal(uint16_t node, int ok);

/* True if the current snapshot is older than NOVA_STALE_MAX ops since it was published -- the
 * degradation trigger in doc 10 section10. */
int nova_coordinator_is_stale(void);

END_C_DECLS

#endif
