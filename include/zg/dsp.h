/* SPDX-License-Identifier: MIT */
#ifndef ZG_DSP_H
#define ZG_DSP_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Fixed-point causal reinterpretation of the six Zaaggenz characters.
 * Audio is signed Q15; oscillators use 2^32 phase units per cycle.
 * Runtime uses no allocation, floating point or division in sample callbacks.
 * Supported rates 8000..48000 Hz. No runtime peak normalization is performed. */
enum zg_family {
    ZG_BLOOM_BARK = 0, ZG_FORMANT_SNARL, ZG_UPPER_CHOP,
    ZG_SPLIT_MAUL, ZG_CRUSHED_TEETH, ZG_HARMONIC_RIP, ZG_FAMILY_COUNT
};

/* Smooth macro values in the engine before passing them here. */
typedef struct zg_dsp_params {
    uint16_t grit;               /* 0..32767 */
    uint16_t motion;             /* 0..32767 */
    uint16_t vowel;              /* 0..32767 */
    uint8_t family;              /* enum zg_family */
    uint8_t quality;             /* 0 direct shaper, 1 locally 2x shaper */
} zg_dsp_params;

typedef struct zg_native_state {
    uint32_t phase;
    uint32_t rough_phase;
    uint32_t noise;
    uint32_t age;
    int32_t previous;
    int32_t dc;
    int32_t sweep;               /* Q15, decays after trigger */
} zg_native_state;

#define ZG_COMB_SAMPLES 80
/* 208 bytes on ordinary 32-bit ABIs; no pointers or heap. */
typedef struct zg_character_state {
    int16_t delay[ZG_COMB_SAMPLES];
    int32_t low;
    int32_t edge_low;
    int32_t form1_low, form1_band;
    int32_t form2_low, form2_band;
    int32_t dc;
    int32_t previous;
    uint32_t ring_phase;
    uint32_t motion_phase;
    uint16_t bloom;
    uint8_t position, hold_count;
    int16_t held;
    uint16_t reserved;
} zg_character_state;

/* Coefficients prepared at block/control rate; may be shared across voices. */
typedef struct zg_dsp_coeffs {
    zg_dsp_params p;
    uint32_t rate;
    uint32_t beat_inc;           /* quarter notes per sample, unsigned Q32 */
    uint32_t ring_inc;
    uint32_t rough_inc;
    uint16_t low_coef, edge_coef;
    uint16_t form1_coef, form2_coef;
    uint16_t sweep_decay, bloom_step;
    uint8_t harmonics;
    uint8_t delays[4];
    uint8_t padding;
    uint16_t attack_frames, attack_step;
} zg_dsp_coeffs;

void zg_native_init(zg_native_state *s, uint32_t seed);
/* Retrigger restarts micro-sweep/age; preserve_phase keeps phase and noise. */
void zg_native_trigger(zg_native_state *s, int preserve_phase);
void zg_character_init(zg_character_state *s);
/* Trigger keeps causal filter and comb histories, and restarts bloom only. */
void zg_character_trigger(zg_character_state *s);
void zg_dsp_prepare(zg_dsp_coeffs *c, const zg_dsp_params *p,
                    uint32_t sample_rate, uint32_t phase_increment,
                    uint32_t beat_increment);
int32_t zg_native_sample(zg_native_state *s, const zg_dsp_coeffs *c,
                         uint32_t phase_increment);
int32_t zg_character_sample(zg_character_state *s, const zg_dsp_coeffs *c,
                            int32_t source);
/* Utility oscillators use the same fixed integer lookup tables as native DSP. */
int32_t zg_sine(uint32_t phase);
int32_t zg_tanh_q15(int32_t x); /* x in Q15, clipped table domain ±4 */
int32_t zg_mul_q15(int32_t x, int32_t y); /* input x ±65535; y ±32767 */
const char *zg_family_name(unsigned family);
#ifdef __cplusplus
}
#endif
#endif
