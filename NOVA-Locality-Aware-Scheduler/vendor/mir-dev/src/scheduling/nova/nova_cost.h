#ifndef NOVA_COST_H
#define NOVA_COST_H 1

#include "mir_task.h"
#include "mir_mem_pol.h"
#include "mir_types.h"

BEGIN_C_DECLS

/* Thin wrapper over the (D1-fixed, shared) mir_mem_node_dist_get_comm_cost -- doc 10 section4.6.
 * No new cost model: cost(i) = sum_j D[j] * comm_cost_of(i,j), unchanged from Algorithm 1. */
unsigned long nova_cost(const struct mir_mem_node_dist_t* dist, uint16_t node);

/* The corrected, D1-free significance gate -- doc 06 N-A: keep sum(D) > LLC/C, drop StdDev(D) > 0
 * (replaced by N-A's continuous alpha blend, so no gate on variance is needed at all). */
int nova_is_significant(const struct mir_mem_node_dist_t* dist);

END_C_DECLS

#endif
