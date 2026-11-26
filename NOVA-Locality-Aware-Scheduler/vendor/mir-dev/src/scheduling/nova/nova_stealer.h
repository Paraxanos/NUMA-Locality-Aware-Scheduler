#ifndef NOVA_STEALER_H
#define NOVA_STEALER_H 1

#include "mir_task.h"
#include "mir_task_queue.h"
#include "nova_shadow_index.h"

BEGIN_C_DECLS

/* N-B Locality-Aware Stealer -- docs/10-nova-architecture.md section4.4, docs/06-novelties-alloc.md
 * N-B. Picks the best (lowest comm_cost to thief_node) candidate among up to NOVA_SHADOW_K entries
 * cached in shadow for this queue, then removes exactly that task from queue. Returns NULL if the
 * Shadow Index was empty, or if the chosen candidate was already removed by a concurrent path by
 * the time this tries to remove it -- both are the documented degradation case (doc 10 section10):
 * the caller must fall back to a plain mir_task_queue_pop() on queue, exactly like pop_numa. */
struct mir_task_t* nova_steal_best(struct mir_task_queue_t* queue, struct shadow_index_t* shadow,
                                    uint16_t thief_node);

END_C_DECLS

#endif
