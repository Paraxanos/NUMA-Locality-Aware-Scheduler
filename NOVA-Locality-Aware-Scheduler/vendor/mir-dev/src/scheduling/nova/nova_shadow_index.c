/* NOVA Shadow Index -- docs/10-nova-architecture.md section4.4.1. */
#include "nova_shadow_index.h"
#include "mir_task.h"

void nova_shadow_index_init(struct shadow_index_t* idx)
{
    for (int i = 0; i < NOVA_SHADOW_K; i++) {
        idx->entries[i].task = NULL;
        idx->entries[i].dist = NULL;
        idx->entries[i].generation = 0;
    }
    idx->head = 0;
}

void nova_shadow_append(struct shadow_index_t* idx, struct mir_task_t* task)
{
    uint32_t slot = idx->head % NOVA_SHADOW_K;
    uint32_t gen = idx->entries[slot].generation + 1;

    /* Write task/dist first, generation last -- a reader that validates the generation before
     * and after its own task/dist reads (nova_shadow_peek) can then detect, and discard, a read
     * torn by this write landing concurrently on the same slot. __sync_synchronize() is a full
     * compiler+hardware fence: x86 itself preserves store-store program order, but the compiler
     * is still free to reorder these independent-looking field writes at -O2 without it. */
    idx->entries[slot].task = task;
    idx->entries[slot].dist = mir_task_get_mem_node_dist(task, MIR_DATA_ACCESS_READ);
    __sync_synchronize();
    idx->entries[slot].generation = gen;
    idx->head = idx->head + 1;
}

int nova_shadow_peek(struct shadow_index_t* idx, struct mir_task_t** out_tasks,
                      struct mir_mem_node_dist_t** out_dists)
{
    int count = 0;
    for (int i = 0; i < NOVA_SHADOW_K; i++) {
        /* Seqlock-style validation: a concurrent nova_shadow_append() on this exact slot writes
         * task+dist, THEN (after a fence) generation. If generation changed between our before/
         * after reads, our task/dist reads may be a torn mix of the old and new entry (e.g. the
         * new task paired with the old, possibly-already-freed dist) -- discard the whole entry
         * rather than trust it. This is what the generation field is for; found necessary by an
         * ASan run catching an intermittent (not every run) use-after-free-shaped SEGV with the
         * naive unguarded read. */
        uint32_t gen_before = idx->entries[i].generation;
        __sync_synchronize();
        struct mir_task_t* t = idx->entries[i].task;
        struct mir_mem_node_dist_t* d = idx->entries[i].dist;
        __sync_synchronize();
        uint32_t gen_after = idx->entries[i].generation;
        if (gen_before != gen_after)
            continue;
        if (t == NULL)
            continue;
        /* Staleness check (section4.4.1's only correctness-critical addition): skip anything
         * already taken rather than dereferencing it further. */
        if (t->taken)
            continue;
        out_tasks[count] = t;
        out_dists[count] = d;
        count++;
    }
    return count;
}
