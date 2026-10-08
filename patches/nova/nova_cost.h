#ifndef NOVA_COST_H
#define NOVA_COST_H

#include "nova_types.h"
#include "../../mir_task.h"

bool nova_is_significant(struct mir_task_t* task, uint64_t llc_per_core_bytes);
uint64_t nova_comm_cost(void* dist, uint16_t target_node);

#endif // NOVA_COST_H