#ifndef NOVA_RADIUS_H
#define NOVA_RADIUS_H 1

#include <stdint.h>
#include "mir_worker.h"

BEGIN_C_DECLS

/* N-C Adaptive Radius Controller -- docs/10-nova-architecture.md section4.5. Thin wrappers over
 * the Coordinator, which owns the actual EWMA/radius state (section4.2). */
uint32_t nova_radius(struct mir_worker_t* worker);
void nova_report_steal(uint16_t node, int ok);

END_C_DECLS

#endif
