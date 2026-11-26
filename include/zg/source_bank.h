#ifndef ZG_SOURCE_BANK_H
#define ZG_SOURCE_BANK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Post-topology, source-derived PCM. The bank is const/XIP; no sample copies. */
#define ZG_SOURCE_BANK_VERSION 2
#define ZG_SOURCE_RATE 44100u
#define ZG_SOURCE_ROOT_HZ 48u
#define ZG_SOURCE_MIPS 10u
#define ZG_SOURCE_CROSSFADE 1024u

typedef struct zg_source_state {
    uint32_t position_q16;
    uint32_t step_q16;
    uint8_t asset;
    uint8_t mip;
    uint8_t ended;
    uint8_t reserved;
    uint8_t previous_mip;
    uint8_t mip_fade;
    uint16_t padding;
} zg_source_state;

typedef struct zg_source_info {
    const char *name;
    const uint8_t *pcm[ZG_SOURCE_MIPS];
    uint32_t frames[ZG_SOURCE_MIPS];
    uint32_t loop_start;
    uint32_t loop_end;
    uint16_t root_hz;
    uint16_t sample_rate;
    uint8_t pcm_shift;
} zg_source_info;

extern const unsigned zg_source_count;
extern const zg_source_info zg_source_bank[];
extern const int16_t zg_source_decode_table[256];

const zg_source_info *zg_source_get_info(unsigned index);
void zg_source_reset(zg_source_state *state, unsigned index);
/* Control-rate preparation. inc is the oscillator phase increment at fs.
 * The factor fs cancels for this bank's 48 Hz root and 44100 Hz sample rate. */
void zg_source_prepare(zg_source_state *state, uint32_t inc, uint32_t fs);
/* A step of 65536 is root pitch at 44100 Hz. Supported step: 1..67108864.
 * This low-level control also enables deterministic decoder conformance. */
void zg_source_set_step(zg_source_state *state, uint32_t step_q16);
/* gate=0 stops future sustain looping; the remaining source tail continues.
 * sustain=0 is finite source-image playback, independent of gate.
 * Returns Q15 in int32 with the bank's explicit common encoding scale undone.
 * No clipping, gain compensation, allocation, division or floating point. */
int32_t zg_source_sample(zg_source_state *state, int gate, int sustain);

#ifdef __cplusplus
}
#endif
#endif
