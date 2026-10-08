#include "task.h"
#include <stdlib.h>
#include <string.h>

task_t* task_create(uint32_t id, uint64_t compute_cycles) {
    task_t* t = (task_t*)malloc(sizeof(task_t));
    if (!t) return NULL;
    memset(t, 0, sizeof(task_t));
    t->id = id;
    t->compute_cycles = compute_cycles;
    t->state = TASK_STATE_CREATED;
    data_dist_init(&t->data_dist);
    return t;
}

void task_destroy(task_t* t) {
    if (t) {
        free(t);
    }
}

void task_add_dependency(task_t* task, task_t* dep) {
    if (!task || !dep || task->num_dependencies >= MAX_TASK_DEPS) return;
    task->dependencies[task->num_dependencies++] = dep;
    if (dep->state != TASK_STATE_EXECUTED) {
        task->unresolved_deps++;
    }
}

bool task_is_ready(const task_t* t) {
    return t && (t->unresolved_deps == 0) && (t->state != TASK_STATE_EXECUTED);
}

void task_resolve_dep(task_t* task) {
    if (task && task->unresolved_deps > 0) {
        task->unresolved_deps--;
        if (task->unresolved_deps == 0 && task->state == TASK_STATE_CREATED) {
            task->state = TASK_STATE_READY;
        }
    }
}
