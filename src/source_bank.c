#include "zg/source_bank.h"

#ifndef ZG_SOURCE_BANK_INCLUDE
#define ZG_SOURCE_BANK_INCLUDE "source_bank_data.inc"
#endif
#include ZG_SOURCE_BANK_INCLUDE

/* Explicit truncation toward zero. JieLi clang -Os leaves a hardware divide
 * for signed /32768, so ordinary C constant division is not sufficient here. */
static int32_t trunc_shift15(int32_t x)
{
    return x < 0 ? -(int32_t)(((uint32_t)(-(x + 1)) + 1u) >> 15)
                 : (int32_t)((uint32_t)x >> 15);
}
static int32_t trunc_shift5(int32_t x)
{
    return x < 0 ? -(int32_t)(((uint32_t)(-(x + 1)) + 1u) >> 5)
                 : (int32_t)((uint32_t)x >> 5);
}

const zg_source_info *zg_source_get_info(unsigned index)
{
    return index < zg_source_count ? &zg_source_bank[index] : (const zg_source_info *)0;
}

void zg_source_reset(zg_source_state *s, unsigned index)
{
    s->position_q16 = 0;
    s->step_q16 = 32768u;
    s->asset = (uint8_t)(index < zg_source_count ? index : 0u);
    s->mip = 0;
    s->ended = 0;
    s->reserved = 0;
    s->previous_mip = 0;
    s->mip_fade = 32;
    s->padding = 0;
}

void zg_source_set_step(zg_source_state *s, uint32_t step)
{
    unsigned mip = 0;
    if (step < 4096u) step = 4096u;
    if (step > 33554432u) step = 33554432u;
    s->step_q16 = step;
    while (mip + 1u < ZG_SOURCE_MIPS && step > (65536u << mip)) ++mip;
    if (s->mip != mip) {
        s->previous_mip = s->position_q16 ? s->mip : (uint8_t)mip;
        s->mip_fade = (uint8_t)(s->position_q16 ? 0u : 32u);
        s->mip = (uint8_t)mip;
    }
}

void zg_source_prepare(zg_source_state *s, uint32_t inc, uint32_t fs)
{
    (void)fs;
    /* inc * 22050 / (48 * 65536), reduced and split into two 16-bit products.
     * This is exactly round(inc*7350/2^20), without a 64-bit multiply. All
     * intermediate values fit uint32 even at inc=UINT32_MAX; avoids the
     * pi32v2 compiler's wide-multiply/halfword-load hazard entirely. */
    {
        uint32_t high = (inc >> 16) * 7350u;
        uint32_t low = (inc & 65535u) * 7350u;
        uint32_t step = (high >> 4) + ((((high & 15u) << 16) + low + 524288u) >> 20);
        zg_source_set_step(s, step);
    }
}

static int32_t read_pcm(const zg_source_info *a, unsigned mip, uint32_t pos)
{
    uint32_t p = pos >> mip;
    uint32_t i = p >> 16;
    uint32_t fraction = (p & 65535u) >> 1;
    int32_t x, y;
    if (i >= a->frames[mip]) return 0;
    x = a->pcm[mip][i];
    y = a->pcm[mip][i + 1u]; /* Generated arrays contain a zero guard. */
    return x + trunc_shift15((y - x) * (int32_t)fraction);
}

static int32_t read_source(const zg_source_state *s, const zg_source_info *a, uint32_t pos)
{
    int32_t value = read_pcm(a, s->mip, pos);
    if (s->mip_fade < 32u) {
        int32_t previous = read_pcm(a, s->previous_mip, pos);
        value = previous + trunc_shift5((value - previous) * (int32_t)s->mip_fade);
    }
    return value;
}

int32_t zg_source_sample(zg_source_state *s, int gate, int sustain)
{
    const zg_source_info *a;
    uint32_t pos, end;
    int32_t value;
    int loop;
    if (s->ended || s->asset >= zg_source_count) return 0;
    a = &zg_source_bank[s->asset];
    pos = s->position_q16;
    end = a->frames[0] << 16;
    if (pos >= end) { s->ended = 1; return 0; }
    value = read_source(s, a, pos);
    /* Finish an overlap already in progress when the key is released. Simply
     * disabling its crossfade at note-off would jump back to the old window.
     * reserved bit 0 = preceding gate, bit 1 = finish-overlap release latch. */
    if (!gate && (s->reserved & 1u) && sustain &&
        a->loop_end > a->loop_start + ZG_SOURCE_CROSSFADE &&
        pos >= ((a->loop_end - ZG_SOURCE_CROSSFADE) << 16) &&
        pos < (a->loop_end << 16)) s->reserved |= 2u;
    s->reserved = (uint8_t)((s->reserved & 2u) | (gate ? 1u : 0u));
    loop = sustain && (gate || (s->reserved & 2u));
    if (loop && a->loop_end > a->loop_start + ZG_SOURCE_CROSSFADE) {
        uint32_t blend_begin = (a->loop_end - ZG_SOURCE_CROSSFADE) << 16;
        uint32_t loop_end = a->loop_end << 16;
        if (pos >= blend_begin && pos < loop_end) {
            uint32_t offset = pos - blend_begin;
            uint32_t other = (a->loop_start << 16) + offset;
            int32_t next = read_source(s, a, other);
            /* Q16 position / 512 samples -> Q15 crossfade. */
            int32_t weight = (int32_t)(offset >> 10);
            value += trunc_shift15((next - value) * weight);
        }
        pos += s->step_q16;
        if (pos >= loop_end) {
            pos -= (a->loop_end - a->loop_start - ZG_SOURCE_CROSSFADE) << 16;
            s->reserved &= 1u;
        }
    } else {
        pos += s->step_q16;
    }
    s->position_q16 = pos;
    if (s->mip_fade < 32u) ++s->mip_fade;
    /* Multiplication avoids undefined signed left shift for negative PCM. */
    return value * (int32_t)(1u << a->pcm_shift);
}
