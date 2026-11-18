#ifndef MOCK_MIR_TASK_H
#define MOCK_MIR_TASK_H

#include <stdint.h>
#include <stdbool.h>

enum mir_data_access_t {
    MIR_DATA_ACCESS_READ = 0,
    MIR_DATA_ACCESS_WRITE,
    MIR_DATA_ACCESS_NUM_TYPES
};

struct mir_task_t {
    uint32_t id;
    uint32_t taken;
    uint32_t done;
    uint32_t num_data_footprints;
    void* dist_by_access_type[MIR_DATA_ACCESS_NUM_TYPES];
};

#endif
