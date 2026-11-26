/* arch_gem5numa8 -- gem5 FS-mode NUMA topology for the Opteron/NUMA reproduction track, replacing
 * arch_sim_numa8 (HAL-Sim) per docs/17-gem5-migration-plan.md.
 *
 * node_of()/vicinity_of()/comm_cost_of() are unchanged from arch_sim_numa8/arch_gothmog -- these are
 * the scheduler's logical decision-layer functions and stay the same regardless of how a node is
 * physically realized (doc 17 section5.1).
 *
 * What's new: create()/destroy() now actually back each logical node with a real physical memory
 * arena, instead of being no-ops. gem5's x86 FS mode has no ACPI SRAT (no guest-visible NUMA), and
 * Ruby's directory home is a pure function of PHYSICAL address (system.mem_ranges[0] split into 8
 * equal contiguous chunks -- see doc 17 section4/section11's 2026-11-15 validation note, NOT the
 * --numa-high-bit bit-decode originally sketched). So node placement is done directly via /dev/mem
 * into kernel `memmap=` reservations (one per directory, excluded from the guest's own memory
 * management so poking them can't corrupt live state) -- validated empirically at 128MiB/node.
 *
 * Kernel command line this arch model requires (mem-size=2GB, chunk=256MiB/directory,
 * reservation=128MiB at the top of each chunk):
 *   memmap=128M$128M memmap=128M$384M memmap=128M$640M memmap=128M$896M
 *   memmap=128M$1152M memmap=128M$1408M memmap=128M$1664M memmap=128M$1920M
 */
#include "arch/mir_arch.h"
#include "mir_types.h"
#include "mir_defines.h"
#include "mir_utils.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>

#define GEM5NUMA8_CORES_PER_NODE 3
#define GEM5NUMA8_NODES 8
#define GEM5NUMA8_CHUNK_SIZE (256UL * 1024 * 1024)
#define GEM5NUMA8_RESERVED_OFFSET (128UL * 1024 * 1024)
#define GEM5NUMA8_ARENA_SIZE (128UL * 1024 * 1024)

static int devmem_fd = -1;
static void* arena_base[GEM5NUMA8_NODES];
static volatile size_t arena_cursor[GEM5NUMA8_NODES];
static volatile int arena_init_state = 0; /* 0=not started, 1=done */

void create_gem5numa8(); /* forward decl -- defined below, also used as the mir_arch_t hook */

static void ensure_arenas_mapped()
{
    /* MIR's runtime never actually calls arch->create() (grep confirms only destroy() is
     * invoked, from mir_runtime.c) -- so the arena mmaps have to happen lazily, on first use,
     * rather than relying on that lifecycle hook. Safe without a lock: bench_init() (the only
     * caller of mir_mem_pol_allocate() before parallel task execution starts) runs on the main
     * thread, single-threaded, by construction of main.c's driver. The CAS still guards against
     * calling this twice if that assumption is ever violated. */
    if (arena_init_state == 1) {
        return;
    }
    if (!__sync_bool_compare_and_swap(&arena_init_state, 0, 1)) {
        return;
    }
    create_gem5numa8();
}

void create_gem5numa8()
{
    devmem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (devmem_fd < 0) {
        MIR_LOG_ERR("arch_gem5numa8: could not open /dev/mem (errno=%d) -- "
                    "are the memmap= kernel reservations and root privileges in place?", errno);
        return;
    }
    for (int d = 0; d < GEM5NUMA8_NODES; d++) {
        off_t offset = (off_t)d * GEM5NUMA8_CHUNK_SIZE + GEM5NUMA8_RESERVED_OFFSET;
        void* p = mmap(NULL, GEM5NUMA8_ARENA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                        devmem_fd, offset);
        if (p == MAP_FAILED) {
            MIR_LOG_ERR("arch_gem5numa8: mmap of node %d arena (phys offset 0x%lx) failed "
                        "(errno=%d)", d, (unsigned long)offset, errno);
            arena_base[d] = NULL;
        } else {
            arena_base[d] = p;
        }
        arena_cursor[d] = 0;
    }
}

void destroy_gem5numa8()
{
    for (int d = 0; d < GEM5NUMA8_NODES; d++) {
        if (arena_base[d] != NULL) {
            munmap(arena_base[d], GEM5NUMA8_ARENA_SIZE);
            arena_base[d] = NULL;
        }
    }
    if (devmem_fd >= 0) {
        close(devmem_fd);
        devmem_fd = -1;
    }
}

/* Bump-allocate sz bytes from node nodeid's physical arena. Called by the allocator patch in
 * mir_mem_pol.c (allocate_coarse/allocate_fine under MIR_ARCH_GEM5SIM) -- this is the gem5
 * equivalent of arch_sim_numa8's plain malloc() stamping, except the bytes returned are now
 * genuinely backed by that node's Ruby directory. Returns NULL if the node's arena is exhausted
 * or was never mapped. */
void* gem5numa8_arena_alloc(uint16_t nodeid, size_t sz)
{
    ensure_arenas_mapped();
    if (nodeid >= GEM5NUMA8_NODES || arena_base[nodeid] == NULL) {
        return NULL;
    }
    size_t old_cursor = __sync_fetch_and_add(&arena_cursor[nodeid], sz);
    if (old_cursor + sz > GEM5NUMA8_ARENA_SIZE) {
        MIR_LOG_ERR("arch_gem5numa8: node %d arena exhausted (requested %zu, used %zu/%lu)",
                    nodeid, sz, old_cursor, GEM5NUMA8_ARENA_SIZE);
        return NULL;
    }
    return (void*)((unsigned char*)arena_base[nodeid] + old_cursor);
}

uint16_t sys_cpu_of_gem5numa8(uint16_t cpuid)
{
    return cpuid;
}

uint16_t node_of_gem5numa8(uint16_t cpuid)
{
    return cpuid / GEM5NUMA8_CORES_PER_NODE;
}

void cpus_of_gem5numa8(struct mir_sbuf_t* cpuids, uint16_t nodeid)
{
    MIR_ASSERT(cpuids != NULL);
    cpuids->size = 0;
    if (nodeid >= GEM5NUMA8_NODES) {
        MIR_LOG_ERR("CPUs of node %d not found.", nodeid);
        return;
    }
    cpuids->size = GEM5NUMA8_CORES_PER_NODE;
    uint16_t basecpu = nodeid * GEM5NUMA8_CORES_PER_NODE;
    for (int i = 0; i < GEM5NUMA8_CORES_PER_NODE; i++)
        cpuids->buf[i] = basecpu + i;
}

/* Verbatim from arch_gothmog.c / arch_sim_numa8.c. */
uint16_t vicinity_of_gem5numa8(uint16_t* neighbors, uint16_t nodeid, uint16_t diameter)
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
uint16_t comm_cost_of_gem5numa8(uint16_t from_nodeid, uint16_t to_nodeid)
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

struct mir_arch_t arch_gem5numa8 = {
    .name = "gem5numa8",
    .num_nodes = GEM5NUMA8_NODES,
    .num_cores = GEM5NUMA8_NODES * GEM5NUMA8_CORES_PER_NODE,
    .diameter = 3,
    .llc_size_KB = 5000,
    .create = create_gem5numa8,
    .destroy = destroy_gem5numa8,
    .sys_cpu_of = sys_cpu_of_gem5numa8,
    .node_of = node_of_gem5numa8,
    .cpus_of = cpus_of_gem5numa8,
    .vicinity_of = vicinity_of_gem5numa8,
    .comm_cost_of = comm_cost_of_gem5numa8
};
