#ifndef NOVA_SHADOW_INDEX_H
#define NOVA_SHADOW_INDEX_H

#include "nova_types.h"
#include "mir_task.h"

void nova_shadow_init(struct shadow_index_t* idx);
void nova_shadow_destroy(struct shadow_index_t* idx);
void nova_shadow_append(struct shadow_index_t* idx, struct mir_task_t* task);
uint32_t nova_shadow_peek(struct shadow_index_t* idx, struct shadow_entry_t* out_candidates, uint32_t max_count);
void nova_shadow_invalidate(struct shadow_index_t* idx, struct mir_task_t* task);

// Ablation control
void nova_shadow_set_disabled(bool disabled);
bool nova_shadow_is_disabled(void);

#endif // NOVA_SHADOW_INDEX_H
