#ifndef NOVA_TYPES_H
#define NOVA_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

#define NOVA_SHADOW_K       8
#define NOVA_EPOCH_OPS      64
#define NOVA_STALE_MAX      512
#define NOVA_RADIUS_MIN     1
#define NOVA_MAX_NODES      64
#define NOVA_MAX_WORKERS    256

// Double-buffered coordinator context
struct alloc_context_t {
    uint64_t epoch;
    float    alpha;
    uint32_t radius[NOVA_MAX_WORKERS];
    uint64_t occ_snapshot[NOVA_MAX_NODES];
    uint64_t steal_fail_ewma[NOVA_MAX_NODES];
    uint64_t steal_ok_ewma[NOVA_MAX_NODES];
};

struct alloc_coordinator_t {
    struct alloc_context_t      buf[2];
    _Atomic(struct alloc_context_t*) current;
    _Atomic uint64_t            op_counter[NOVA_MAX_NODES];
    _Atomic uint32_t            refreshing[NOVA_MAX_NODES];
};

// Shadow Index entry
struct shadow_entry_t {
    struct mir_task_t*          task;
    void*                       dist_read;
    uint32_t                    generation;
};

struct shadow_index_t {
    struct shadow_entry_t       entries[NOVA_SHADOW_K];
    _Atomic uint32_t            head;
};

#endif // NOVA_TYPES_H