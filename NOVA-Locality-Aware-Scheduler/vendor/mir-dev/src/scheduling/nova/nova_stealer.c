/* N-B Locality-Aware Stealer -- docs/10-nova-architecture.md section4.4. */
#include "nova_stealer.h"
#include "nova_cost.h"
#include "mir_utils.h"
#include "mir_lock.h"

/* Removes exactly `target` from queue if it's still there.
 * To safely modify queue->in and shift elements without racing against concurrent
 * mir_task_queue_push() (which holds enq_lock) or mir_task_queue_pop() (which holds deq_lock),
 * we acquire both enq_lock and deq_lock.
 * Since push holds only enq_lock and pop holds only deq_lock, no deadlock is possible.
 * Scans backward from the tail (queue->in) for at most NOVA_SHADOW_K slots, matching section8's budget.
 * Removal closes the gap by shifting the (at most NOVA_SHADOW_K) newer slots back by one, which
 * preserves FIFO order for every task that isn't `target`. */
static struct mir_task_t* try_remove_task(struct mir_task_queue_t* queue, struct mir_task_t* target)
{
    struct mir_task_t* removed = NULL;

    mir_lock_set(&(queue->enq_lock));
    mir_lock_set(&(queue->deq_lock));

    if (queue->size == 0) {
        mir_lock_unset(&(queue->deq_lock));
        mir_lock_unset(&(queue->enq_lock));
        return NULL;
    }

    uint32_t scan = (NOVA_SHADOW_K < queue->size) ? NOVA_SHADOW_K : queue->size;
    uint32_t idx = queue->in;
    int32_t found_pos = -1;
    for (uint32_t s = 0; s < scan; s++) {
        idx = (idx == 0) ? (queue->capacity - 1) : (idx - 1);
        if (queue->buffer[idx] == target) {
            found_pos = (int32_t)idx;
            break;
        }
    }

    if (found_pos >= 0) {
        uint32_t write = (uint32_t)found_pos;
        uint32_t read = (write + 1) % queue->capacity;
        while (read != queue->in) {
            queue->buffer[write] = queue->buffer[read];
            write = read;
            read = (read + 1) % queue->capacity;
        }
        queue->in = write;
        __sync_fetch_and_sub(&(queue->size), 1);
        removed = target;
    }

    mir_lock_unset(&(queue->deq_lock));
    mir_lock_unset(&(queue->enq_lock));
    return removed;
}

struct mir_task_t* nova_steal_best(struct mir_task_queue_t* queue, struct shadow_index_t* shadow,
                                    uint16_t thief_node)
{
    struct mir_task_t* candidates[NOVA_SHADOW_K];
    struct mir_mem_node_dist_t* dists[NOVA_SHADOW_K];

    int n = nova_shadow_peek(shadow, candidates, dists);
    if (n == 0)
        return NULL;

    unsigned long best_cost = 0;
    int best_i = -1;
    for (int i = 0; i < n; i++) {
        unsigned long c = dists[i] ? nova_cost(dists[i], thief_node) : 0;
        if (best_i == -1 || c < best_cost) {
            best_cost = c;
            best_i = i;
        }
    }

    struct mir_task_t* chosen = candidates[best_i];
    struct mir_task_t* removed = try_remove_task(queue, chosen);
    if (removed != NULL && dists[best_i] != NULL) {
        removed->comm_cost = nova_cost(dists[best_i], thief_node);
    }
    return removed;
}
