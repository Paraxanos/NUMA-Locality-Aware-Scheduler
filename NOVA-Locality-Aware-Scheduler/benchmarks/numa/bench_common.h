#ifndef BENCH_COMMON_H
#define BENCH_COMMON_H

/* Shared interface every benchmark implements. No file referencing this header may reference
 * a node count, node id, or distance value directly — all of that comes from whichever arch
 * model (MIR_ARCH_NAME) and distribution policy (MIR_CONF -m) the harness selected for the run.
 * See docs/14-numa-opteron-implementation-plan.md Step 3.
 */

void bench_init(void);
void bench_run(void);
void bench_teardown(void);
/* Returns 1 if the benchmark's own correctness check passed, 0 otherwise. */
int bench_check(void);

#endif
