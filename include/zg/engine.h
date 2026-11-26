/* SPDX-License-Identifier: MIT */
#ifndef ZG_ENGINE_H
#define ZG_ENGINE_H
#include <stddef.h>
#include <stdint.h>
#include "zg/dsp.h"
#include "zg/source_bank.h"
#ifdef __cplusplus
extern "C" {
#endif

#define ZG_BLOCK 32u
#define ZG_SAMPLE_RATE 44100u
#define ZG_LAYER_COUNT 5u
#define ZG_STEM_COUNT 7u
enum zg_layer { ZG_SYNTHLINE, ZG_EXCITER, ZG_BODY, ZG_AUX, ZG_SUB };
enum zg_source_mode { ZG_NATIVE, ZG_SOURCE_IMAGE, ZG_HYBRID };
enum zg_tuning { ZG_12EDO, ZG_19EDO, ZG_24EDO, ZG_13ED3, ZG_TUNING_COUNT };
enum zg_sub_mode { ZG_SUB_MOVING, ZG_SUB_PEDAL };
enum zg_macro { ZG_CHARACTER, ZG_GRIT, ZG_MOTION, ZG_VOWEL };

/* Every gain is explicit. No automatic normalization or master makeup gain.
 * Tuning keys are consecutive degrees, with MIDI key 48 anchored at C3.
 * The Fune host retains its own sequencer, MIDI routing and note allocation. */
typedef struct zg_patch {
    uint8_t family, source, mode, grit, motion, vowel;
    uint8_t level[ZG_LAYER_COUNT]; /* 0..127, independent pre-master roles */
    uint8_t attack, decay, sustain, release; /* milliseconds tables; sustain0..127 */
    uint8_t glide, roll, tuning, sub_mode;
    int8_t body_degree, aux_degree;
    uint8_t separation, gain, quality, source_sustain, bypass;
    uint16_t bpm;
} zg_patch;

typedef struct zg_preset {
    const char *name;
    const char *description;
    zg_patch patch;
} zg_preset;

/* One independent monophonic instrument. No pointers, allocator or external
 * mutable tables: copies are deterministic checkpoints, including release tails.
 * Fune keeps one per track, sharing only read-only flash assets. */
typedef struct zg_engine {
    zg_native_state native;
    zg_character_state character;
    zg_source_state source;
    zg_native_state exciter_native;
    zg_source_state exciter_source;
    zg_patch patch;
    uint32_t rate, pitch, target_pitch, body_phase, aux_phase, sub_phase;
    uint32_t sub_pitch, rhythm_phase;
    int32_t envelope, layer_envelope, exciter_envelope;
    int32_t source_low, master_dc, last_output;
    uint16_t grit, motion, vowel;
    uint16_t level_gain[ZG_LAYER_COUNT], output_gain;
    int16_t bend_cents;
    uint16_t fade;
    uint8_t note, velocity, gate, stage;
    uint8_t preset, mute_mask, bypass_character, held;
} zg_engine;

extern const zg_preset zg_presets[];
extern const unsigned zg_preset_count;
extern const char *const zg_preset_names[];
const char *zg_tuning_name(unsigned tuning);
uint32_t zg_pitch_increment(int key, unsigned tuning, uint32_t sample_rate);
uint32_t zg_time_ms(uint8_t control); /* 0..127 -> 1..2000 ms */
void zg_engine_init(zg_engine *e, uint32_t sample_rate);
void zg_engine_set_preset(zg_engine *e, unsigned preset);
void zg_engine_set_patch(zg_engine *e, const zg_patch *patch);
void zg_engine_set_macro(zg_engine *e, unsigned macro, unsigned value);
void zg_engine_pitch_bend(zg_engine *e, int cents); /* standalone expression, +/-200 cents */
void zg_engine_note_on(zg_engine *e, unsigned note, unsigned velocity, int legato);
void zg_engine_note_off(zg_engine *e, unsigned note);
void zg_engine_all_notes_off(zg_engine *e);
/* Used by standalone evaluation: owns ADSR/velocity, persistent phase, glide. */
void zg_engine_render(zg_engine *e, int16_t *out, size_t frames);
/* Host-provided pitch and amplitude are applied once. The returned mono signal
 * includes explicit final gain/clamp; Fune adapter adds it to its output buffer.
 * Call in chunks <=32. amp0/amp1 are Q15, gate indicates selected held voice.
 * Standalone and host share the identical source/character/layer processing. */
void zg_engine_render_host(zg_engine *e, int16_t *out, unsigned frames,
                           uint32_t phase_inc, int32_t amp0, int32_t amp1, int gate);
/* Optional stems from the same render: seven interleaved signed Q15 values:
 * raw SYNTHLINE, raw EXCITER, BODY, AUX, SUB, processed SOURCE_BUS, PRE_MASTER.
 * Nonlinear raw source roles do not add back to the processed source bus.
 * Muting a role changes assembly participation, never its oscillator history. */
void zg_engine_render_stems(zg_engine *e, int16_t *out, int32_t *stems, size_t frames);
int zg_engine_active(const zg_engine *e);

#ifdef __cplusplus
}
#endif
#endif
