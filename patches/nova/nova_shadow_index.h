#ifndef NOVA_SHADOW_INDEX_H
#define NOVA_SHADOW_INDEX_H

#include "nova_types.h"
#include "../../mir_task.h"

void nova_shadow_init(struct shadow_index_t* idx);
void nova_shadow_append(struct shadow_index_t* idx, struct mir_task_t* task);
uint32_t nova_shadow_peek(struct shadow_index_t* idx, struct shadow_entry_t* out_candidates, uint32_t max_count);

#endif // NOVA_SHADOW_INDEX_H