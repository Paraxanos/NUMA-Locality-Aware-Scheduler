/* N-C Adaptive Radius Controller -- docs/10-nova-architecture.md section4.5. */
#include "nova_radius.h"
#include "nova_coordinator.h"

uint32_t nova_radius(struct mir_worker_t* worker)
{
    const struct alloc_context_t* ctx = nova_coordinator_snapshot();
    return ctx->radius[worker->id];
}

void nova_report_steal(uint16_t node, int ok)
{
    nova_coordinator_report_steal(node, ok);
}
