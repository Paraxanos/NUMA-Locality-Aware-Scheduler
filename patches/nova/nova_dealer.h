#ifndef NOVA_DEALER_H
#define NOVA_DEALER_H

#include "nova_types.h"
#include "../../mir_task.h"

uint16_t nova_deal(struct alloc_coordinator_t* coord, struct shadow_index_t* shadow_indices,
                   struct mir_task_t* task, uint32_t worker_id, uint64_t llc_bytes);

#endif // NOVA_DEALER_H