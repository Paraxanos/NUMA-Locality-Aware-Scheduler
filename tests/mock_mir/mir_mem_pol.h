#ifndef MOCK_MIR_MEM_POL_H
#define MOCK_MIR_MEM_POL_H

#include <stdint.h>
#include <stddef.h>

struct mir_mem_node_dist_t {
    size_t* buf;
};

static inline uint64_t mir_dist_get_bytes(const struct mir_mem_node_dist_t* dist, uint16_t node) {
    if (!dist || !dist->buf) return 0;
    return (uint64_t)dist->buf[node];
}

#endif
