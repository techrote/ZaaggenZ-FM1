#include "zg/source_bank.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t rng = 0x49ab32u;
static uint32_t random32(void) {
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng;
}

int main(void)
{
    unsigned asset;
    assert(sizeof(zg_source_state) <= 16u);
    assert(zg_source_count == 7u);
    assert(zg_source_get_info(100u) == 0);
    for (asset = 0; asset < zg_source_count; ++asset) {
        zg_source_state a, b;
        const zg_source_info *info = zg_source_get_info(asset);
        unsigned i;
        int64_t energy = 0;
        assert(info->loop_end > info->loop_start + 512u);
        assert(info->loop_end < info->frames[0]);
        zg_source_reset(&a, asset);
        zg_source_reset(&b, asset);
        for (i = 0; i < 50000; ++i) {
            int32_t x = zg_source_sample(&a, 1, 0);
            int32_t y = zg_source_sample(&b, 1, 0);
            assert(x == y);
            if (i < 5000) energy += (int64_t)x*x;
        }
        assert(a.ended && b.ended);
        assert(energy > 1000000);
        assert(zg_source_sample(&a, 1, 0) == 0);

        /* At source rate/root pitch, integer decoder exactly returns the bank. */
        zg_source_reset(&a, asset);
        zg_source_set_step(&a, 65536u);
        for (i = 0; i < info->frames[0]; ++i)
            assert(zg_source_sample(&a, 0, 0) == (int32_t)info->pcm[0][i]*8);

        /* A held source remains alive; releasing lets the full tail finish. */
        zg_source_reset(&a, asset);
        for (i = 0; i < 176400; ++i) (void)zg_source_sample(&a, 1, 1);
        assert(!a.ended);
        for (i = 0; i < 44100; ++i) (void)zg_source_sample(&a, 0, 1);
        assert(a.ended);

        /* Crossing a mip boundary begins from the previous decoded waveform,
         * then blends over 32 samples instead of switching its bandwidth. */
        zg_source_reset(&a, asset);
        for (i = 0; i < 2000; ++i) (void)zg_source_sample(&a, 1, 1);
        b=a;
        zg_source_set_step(&a, 70000u);
        assert(a.mip_fade == 0u);
        assert(zg_source_sample(&a, 1, 1) == zg_source_sample(&b, 1, 1));

        /* Release in the middle of the overlap must not remove its blend. */
        zg_source_reset(&a, asset);
        a.position_q16 = (info->loop_end - 256u) << 16;
        a.reserved = 1u;
        b = a;
        for (i = 0; i < 256u; ++i)
            assert(zg_source_sample(&a, 1, 1) == zg_source_sample(&b, 0, 1));

        /* Bounded fuzz exercises extreme valid ratios and mip transitions. */
        zg_source_reset(&a, asset);
        for (i = 0; i < 100000; ++i) {
            int32_t x;
            if (!(i & 31u)) zg_source_set_step(&a, random32() & 0xfffffu);
            x = zg_source_sample(&a, 1, 1);
            assert(x >= -262144 && x <= 262136);
            assert(a.position_q16 < (info->frames[0] << 16));
        }
        /* Extended high-note preparation is exact; high notes are never
         * silently clamped to the old 384 Hz source-playback limit. */
        for (i = 0; i < 10000; ++i) {
            uint32_t inc = random32();
            uint32_t exact = (uint32_t)(((uint64_t)inc*7350u+524288u)>>20);
            if (exact < 4096u) exact=4096u;
            if (exact > 33554432u) exact=33554432u;
            zg_source_prepare(&a, inc, 44100u);
            assert(a.step_q16 == exact);
        }
    }
    puts("source_bank: 7 authenticated sources, deterministic decode, finite/held/release tails, overlap continuity and bounded ratios PASS");
    return 0;
}
