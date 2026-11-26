/* SPDX-License-Identifier: MIT */
/* Actual portable source decoder; PCM stdout has the same explicit 1/8 trim as
 * the reference audition WAVs. No Python synthesis substitutes for this path. */
#include "zg/source_bank.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    zg_source_state state;
    unsigned asset = argc > 1 ? (unsigned)strtoul(argv[1], 0, 10) : 0u;
    uint32_t step = argc > 2 ? (uint32_t)strtoul(argv[2], 0, 10) : 32768u;
    unsigned hold = argc > 3 ? (unsigned)strtoul(argv[3], 0, 10) : 0u;
    int mip = argc > 4 ? atoi(argv[4]) : -1;
    unsigned n = 0;
    if (asset >= zg_source_count || mip >= (int)ZG_SOURCE_MIPS) return 2;
    zg_source_reset(&state, asset);
    zg_source_set_step(&state, step);
    if (mip >= 0) state.mip = (uint8_t)mip;
    while (!state.ended && n < 441000u) {
        int32_t sample = zg_source_sample(&state, n < hold, hold != 0u);
        int16_t out = (int16_t)(sample / 8);
        if (fwrite(&out, sizeof(out), 1u, stdout) != 1u) return 3;
        ++n;
    }
    return ferror(stdout) ? 3 : 0;
}
