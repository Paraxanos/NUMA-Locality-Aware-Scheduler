#ifndef NOVA_DEALER_H
#define NOVA_DEALER_H 1

#include "mir_worker.h"
#include "mir_task.h"
#include "nova_shadow_index.h"

BEGIN_C_DECLS

/* N-A Load-Aware Dealer -- docs/10-nova-architecture.md section4.3, docs/06-novelties-alloc.md N-A.
 * shadow_by_node/shadow_alt_by_node are arrays of length runtime->arch->num_nodes, owned by the
 * calling policy (mir_sched_pol_nova.c) -- same arrays the Stealer reads from. Returns 1 if the
 * task was enqueued, 0 if the queue was full and MIR_INLINE_TASK_IF_QUEUE_FULL inlined it instead
 * (same contract as push_numa). */
int nova_deal(struct mir_worker_t* this_worker, struct mir_task_t* task,
              struct shadow_index_t* shadow_by_node, struct shadow_index_t* shadow_alt_by_node);

END_C_DECLS

#endif
