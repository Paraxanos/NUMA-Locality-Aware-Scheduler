#include "arch/mir_arch.h"
#include "mir_utils.h"
#include "mir_defines.h"
#include "mir_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct mir_arch_t arch_this;      // 1
extern struct mir_arch_t arch_adk;       // 2
extern struct mir_arch_t arch_firenze;   // 3
extern struct mir_arch_t arch_gothmog;   // 4
extern struct mir_arch_t arch_tilepro64; // 5
extern struct mir_arch_t arch_sim_numa8; // 6 -- added for HAL-Sim reproduction,
                                          // see docs/14-numa-opteron-implementation-plan.md
extern struct mir_arch_t arch_gem5numa8; // 7 = MIR_ARCH_NUM_PREDEF -- added for the gem5 FS-mode
                                          // migration, see docs/17-gem5-migration-plan.md

// WARNING and NOTE: Make sure predef architecture count == num entries in predef architecture struct
#define MIR_ARCH_NUM_PREDEF 7
static struct mir_arch_t* mir_arch_predef[MIR_ARCH_NUM_PREDEF] = { /*{{{*/
    &arch_this,
    &arch_adk,
    &arch_firenze,
    &arch_gothmog,
    &arch_tilepro64,
    &arch_sim_numa8,
    &arch_gem5numa8
}; /*}}}*/

struct mir_arch_t* mir_arch_create_by_query()
{ /*{{{*/
    struct mir_arch_t* arch = &arch_this;

#ifdef __tile__
    arch = &arch_tilepro64;
#else
    // Explicit override for HAL-Sim work: MIR_ARCH_NAME, if set, selects a registered
    // arch by name directly, bypassing the uname -n match below. Added so simulated
    // arch models (e.g. sim_numa8) can be selected without renaming the host.
    // See project docs/14-numa-opteron-implementation-plan.md.
    const char* forced_arch_name = getenv("MIR_ARCH_NAME");
    if (forced_arch_name != NULL) {
        for (int i = 0; i < MIR_ARCH_NUM_PREDEF; i++) {
            if (0 == strcmp(forced_arch_name, mir_arch_predef[i]->name)) {
                return mir_arch_predef[i];
            }
        }
        MIR_LOG_ERR("MIR_ARCH_NAME=%s does not match any registered architecture.", forced_arch_name);
    }

    // Run uname -n to get node name
    FILE* fpipe;
    fpipe = popen("uname -n", "r");
    MIR_CHECK_FILE(fpipe != NULL);

    // Get node name into buffer and strip it of the newline at end
    char arch_name[MIR_SHORT_NAME_LEN];
    MIR_CHECK_FILE(fgets(arch_name, MIR_SHORT_NAME_LEN, fpipe) != NULL);
    for (int i = 0; i < MIR_SHORT_NAME_LEN; i++)
        if (arch_name[i] == '\n')
            arch_name[i] = '\0';

    // Compare node name with known architectures
    // and set architecture
    for (int i = 0; i < MIR_ARCH_NUM_PREDEF; i++) {
        if (0 == strcmp(arch_name, mir_arch_predef[i]->name))
            arch = mir_arch_predef[i];
    }
#endif

    return arch;
} /*}}}*/

