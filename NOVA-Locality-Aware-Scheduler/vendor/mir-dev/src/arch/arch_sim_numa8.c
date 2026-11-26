/* arch_sim_numa8 — HAL-Sim NUMA topology for the Opteron/NUMA reproduction track.
 *
 * comm_cost_of() and vicinity_of() are copied VERBATIM from arch_gothmog.c, the authors' own
 * real-machine arch model already checked into this repo (hostname gothmog.it.kth.se, 8 nodes,
 * 48 cores, 5MB LLC -- matching the base paper's described Opteron 6172 system exactly, with the
 * comment "These values straight from numactl --hardware" on comm_cost_of). That is the most
 * authoritative calibration available: it is what Algorithm 1's real deployment actually consulted
 * (the paper states NUMA distances come from OS tables), not an approximation from Figure 2's
 * cycle measurements (those motivate the problem; they are not what the scheduler itself reads).
 *
 * num_cores/cpus_of/sys_cpu_of are reduced from gothmog's real 6 cores/node (48 total) to 3
 * cores/node (24 total) to fit within this host's real logical CPU count, so every worker's
 * sched_setaffinity() pin actually succeeds -- see docs/14-numa-opteron-implementation-plan.md
 * "Build log" for why running un-pinnable workers was rejected (adds oversubscription noise to
 * exec_cycles without being necessary: scheduling decisions depend only on node_of()/comm_cost_of(),
 * pure functions of logical cpu_id, not on real pinning succeeding).
 */
#include "arch/mir_arch.h"
#include "mir_types.h"
#include "mir_defines.h"
#include "mir_utils.h"

#define SIM_NUMA8_CORES_PER_NODE 3
#define SIM_NUMA8_NODES 8

void create_sim_numa8()
{
    return;
}

void destroy_sim_numa8()
{
    return;
}

uint16_t sys_cpu_of_sim_numa8(uint16_t cpuid)
{
    return cpuid;
}

uint16_t node_of_sim_numa8(uint16_t cpuid)
{
    return cpuid / SIM_NUMA8_CORES_PER_NODE;
}

void cpus_of_sim_numa8(struct mir_sbuf_t* cpuids, uint16_t nodeid)
{
    MIR_ASSERT(cpuids != NULL);
    cpuids->size = 0;
    if (nodeid >= SIM_NUMA8_NODES) {
        MIR_LOG_ERR("CPUs of node %d not found.", nodeid);
        return;
    }
    cpuids->size = SIM_NUMA8_CORES_PER_NODE;
    uint16_t basecpu = nodeid * SIM_NUMA8_CORES_PER_NODE;
    for (int i = 0; i < SIM_NUMA8_CORES_PER_NODE; i++)
        cpuids->buf[i] = basecpu + i;
}

/* Verbatim from arch_gothmog.c's vicinity_of_gothmog(). */
uint16_t vicinity_of_sim_numa8(uint16_t* neighbors, uint16_t nodeid, uint16_t diameter)
{
    uint16_t count = 0;
    if (diameter == 1) {
        switch (nodeid) {
        case 0: count = 1; neighbors[0] = 1; break;
        case 1: count = 1; neighbors[0] = 0; break;
        case 2: count = 1; neighbors[0] = 3; break;
        case 3: count = 1; neighbors[0] = 2; break;
        case 4: count = 1; neighbors[0] = 5; break;
        case 5: count = 1; neighbors[0] = 4; break;
        case 6: count = 1; neighbors[0] = 7; break;
        case 7: count = 1; neighbors[0] = 6; break;
        default: break;
        }
    } else if (diameter == 2) {
        switch (nodeid) {
        case 0: count = 3; neighbors[0] = 2; neighbors[1] = 4; neighbors[2] = 6; break;
        case 2: count = 3; neighbors[0] = 0; neighbors[1] = 4; neighbors[2] = 6; break;
        case 4: count = 3; neighbors[0] = 0; neighbors[1] = 2; neighbors[2] = 6; break;
        case 6: count = 3; neighbors[0] = 0; neighbors[1] = 2; neighbors[2] = 4; break;
        case 1: count = 3; neighbors[0] = 3; neighbors[1] = 5; neighbors[2] = 7; break;
        case 3: count = 3; neighbors[0] = 1; neighbors[1] = 5; neighbors[2] = 7; break;
        case 5: count = 3; neighbors[0] = 1; neighbors[1] = 3; neighbors[2] = 7; break;
        case 7: count = 3; neighbors[0] = 1; neighbors[1] = 3; neighbors[2] = 5; break;
        default: break;
        }
    } else if (diameter == 3) {
        switch (nodeid) {
        case 0: case 2: case 4: case 6:
            count = 4; neighbors[0] = 1; neighbors[1] = 3; neighbors[2] = 5; neighbors[3] = 7; break;
        case 1: case 3: case 5: case 7:
            count = 4; neighbors[0] = 0; neighbors[1] = 2; neighbors[2] = 4; neighbors[3] = 6; break;
        default: break;
        }
    }
    return count;
}

/* Verbatim from arch_gothmog.c's comm_cost_of_gothmog() -- "straight from numactl --hardware". */
uint16_t comm_cost_of_sim_numa8(uint16_t from_nodeid, uint16_t to_nodeid)
{
    if (from_nodeid == to_nodeid) {
        return 10;
    }
    if (from_nodeid % 2 != to_nodeid % 2) {
        return 22;
    } else {
        return 16;
    }
}

struct mir_arch_t arch_sim_numa8 = {
    .name = "sim_numa8",
    .num_nodes = SIM_NUMA8_NODES,
    .num_cores = SIM_NUMA8_NODES * SIM_NUMA8_CORES_PER_NODE,
    .diameter = 3,
    .llc_size_KB = 5000,
    .create = create_sim_numa8,
    .destroy = destroy_sim_numa8,
    .sys_cpu_of = sys_cpu_of_sim_numa8,
    .node_of = node_of_sim_numa8,
    .cpus_of = cpus_of_sim_numa8,
    .vicinity_of = vicinity_of_sim_numa8,
    .comm_cost_of = comm_cost_of_sim_numa8
};
