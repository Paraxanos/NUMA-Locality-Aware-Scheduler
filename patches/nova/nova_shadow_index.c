#include "nova_shadow_index.h"
#include <string.h>
#include <stdlib.h>

static int g_shadow_disabled = -1; // -1: uninitialized

bool nova_shadow_is_disabled(void) {
    if (g_shadow_disabled == -1) {
        const char* env = getenv("NOVA_DISABLE_SHADOW_INDEX");
        g_shadow_disabled = (env && (strcmp(env, "1") == 0 || strcmp(env, "true") == 0)) ? 1 : 0;
    }
    return g_shadow_disabled == 1;
}

void nova_shadow_set_disabled(bool disabled) {
    g_shadow_disabled = disabled ? 1 : 0;
}

void nova_shadow_init(struct shadow_index_t* idx) {
    if (!idx) return;
    memset(idx->entries, 0, sizeof(idx->entries));
    idx->head = 0;
    pthread_mutex_init(&idx->lock, NULL);
}

void nova_shadow_destroy(struct shadow_index_t* idx) {
    if (!idx) return;
    pthread_mutex_destroy(&idx->lock);
}

void nova_shadow_append(struct shadow_index_t* idx, struct mir_task_t* task) {
    if (!idx || !task || nova_shadow_is_disabled()) return;

    pthread_mutex_lock(&idx->lock);
    uint32_t slot = idx->head % NOVA_SHADOW_K;
    idx->head++;

    idx->entries[slot].task = task;
    idx->entries[slot].dist_read = task->dist_by_access_type[MIR_DATA_ACCESS_READ];
    idx->entries[slot].generation++;
    pthread_mutex_unlock(&idx->lock);
}

uint32_t nova_shadow_peek(struct shadow_index_t* idx, struct shadow_entry_t* out_candidates, uint32_t max_count) {
    if (!idx || !out_candidates || max_count == 0 || nova_shadow_is_disabled()) return 0;

    pthread_mutex_lock(&idx->lock);
    uint32_t count = 0;
    for (uint32_t i = 0; i < NOVA_SHADOW_K && count < max_count; i++) {
        struct mir_task_t* t = idx->entries[i].task;
        if (t != NULL) {
            // Evict tasks that are already taken to prevent dangling references and wasted work
            if (t->taken != 0) {
                idx->entries[i].task = NULL;
                idx->entries[i].dist_read = NULL;
            } else {
                out_candidates[count++] = idx->entries[i];
            }
        }
    }
    pthread_mutex_unlock(&idx->lock);
    return count;
}

void nova_shadow_invalidate(struct shadow_index_t* idx, struct mir_task_t* task) {
    if (!idx || !task) return;

    pthread_mutex_lock(&idx->lock);
    for (uint32_t i = 0; i < NOVA_SHADOW_K; i++) {
        if (idx->entries[i].task == task) {
            idx->entries[i].task = NULL;
            idx->entries[i].dist_read = NULL;
        }
    }
    pthread_mutex_unlock(&idx->lock);
}
