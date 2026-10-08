#ifndef NOVA_STEALER_H
#define NOVA_STEALER_H

#include "nova_types.h"
#include "../../mir_task.h"

struct mir_task_t* nova_steal(struct alloc_coordinator_t* coord, struct shadow_index_t* shadow_indices,
                              uint32_t worker_id);

#endif // NOVA_STEALER_H