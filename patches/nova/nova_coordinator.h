#ifndef NOVA_COORDINATOR_H
#define NOVA_COORDINATOR_H

#include "nova_types.h"

void nova_coordinator_init(struct alloc_coordinator_t* coord);
struct alloc_context_t* nova_coordinator_get_snapshot(struct alloc_coordinator_t* coord);
void nova_coordinator_step_op(struct alloc_coordinator_t* coord, uint16_t node_id);
void nova_coordinator_report_steal(struct alloc_coordinator_t* coord, uint16_t node_id, bool success);

#endif // NOVA_COORDINATOR_H