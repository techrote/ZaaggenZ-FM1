/* SPDX-License-Identifier: MIT
 * ZaaggenZ-FM1 / Fune engine adapter. The pinned SDK is under its own licenses.
 * One independent monophonic instrument per FM-1 track; latest held note wins.
 * The host owns note allocation, sequencing, ADSR, velocity and MIDI.
 */
#include "kreisel_sdk.h"
#include "zg/engine.h"
#include <string.h>

/* The upstream image builder takes one translation unit. These are exactly the
 * source files also built by desktop CMake; there is no target-only DSP model. */
#ifdef ZG_FUNE_TARGET
#include "../../src/dsp.c"
#include "../../src/source_bank.c"
#include "../../src/presets.c"
#include "../../src/engine.c"
#endif

#define ZG_FUNE_TRACKS 4u
#define ZG_FUNE_VOICES 6u

enum {
    F_CHAR, F_GRIT, F_MOTION, F_VOWEL, F_PRESET, F_MODE, F_IMAGE, F_TUNING,
    F_LINE, F_EXCITER, F_BODY, F_AUX, F_SUB, F_ROLL, F_RATE, F_SUBMODE,
    F_SEPARATION, F_GAIN, F_SUSTAIN, F_GLIDE, F_PARAM_COUNT
};

typedef struct zg_fune_track {
    zg_engine engine;
    const track_t *track;
    voice_t *voices[ZG_FUNE_VOICES];
    voice_t *owner;
    uint32_t owner_age;
} zg_fune_track;

static zg_fune_track ZG_TRACK[ZG_FUNE_TRACKS];
/* Host pointers are wider in desktop tests. The actual image uses 32-bit
 * pointers; the build wrapper independently enforces .data + .bss <= 2048. */
_Static_assert(sizeof(void *) != 4 || sizeof(ZG_TRACK) <= 2048,
               "FM-1 plugin state exceeds the actual image RAM pool");

static int zg_fune_bound(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static zg_fune_track *zg_fune_find(const track_t *track, int create)
{
    unsigned i;
    for (i = 0; i < ZG_FUNE_TRACKS; ++i)
        if (ZG_TRACK[i].track == track)
            return &ZG_TRACK[i];
    if (create) {
        for (i = 0; i < ZG_FUNE_TRACKS; ++i) {
            if (!ZG_TRACK[i].track) {
                memset(&ZG_TRACK[i], 0, sizeof(ZG_TRACK[i]));
                ZG_TRACK[i].track = track;
                zg_engine_init(&ZG_TRACK[i].engine, FS);
                return &ZG_TRACK[i];
            }
        }
    }
    /* The FM-1 has four fixed track objects. Refuse an unsupported fifth
     * instead of stealing state from a sounding track. */
    return 0;
}

static void zg_fune_parameters(zg_fune_track *state, const track_t *track)
{
    const int16_t *p = &track->p[P_E0];
    unsigned k;
    const unsigned selected = (unsigned)zg_fune_bound(p[F_PRESET], 0, (int)zg_preset_count - 1);
    zg_patch patch = zg_presets[selected].patch;
    if (p[F_CHAR] > 0) {
        patch.family = (uint8_t)zg_fune_bound(p[F_CHAR] - 1, 0, ZG_FAMILY_COUNT - 1);
        /* Pure source-image recipes bypass the live character by default.
         * An explicit CHARACTER choice must turn that stage back on. */
        patch.bypass = 0;
    }
    patch.grit = (uint8_t)zg_fune_bound(patch.grit + p[F_GRIT], 0, 127);
    patch.motion = (uint8_t)zg_fune_bound(patch.motion + p[F_MOTION], 0, 127);
    patch.vowel = (uint8_t)zg_fune_bound(patch.vowel + p[F_VOWEL], 0, 127);
    if (p[F_MODE] > 0) patch.mode = (uint8_t)zg_fune_bound(p[F_MODE] - 1, ZG_NATIVE, ZG_HYBRID);
    if (p[F_IMAGE] > 0) patch.source = (uint8_t)zg_fune_bound(p[F_IMAGE] - 1, 0, (int)zg_source_count - 1);
    if (p[F_TUNING] > 0) patch.tuning = (uint8_t)zg_fune_bound(p[F_TUNING] - 1, 0, ZG_TUNING_COUNT - 1);
    for (k = 0; k < ZG_LAYER_COUNT; ++k)
        patch.level[k] = (uint8_t)zg_fune_bound(patch.level[k] + p[F_LINE + k], 0, 127);
    if (p[F_ROLL] > 0) patch.roll = (uint8_t)zg_fune_bound(p[F_ROLL] - 1, 0, 6);
    if (p[F_RATE] > 0) patch.bpm = (uint16_t)(40 + (zg_fune_bound(p[F_RATE], 1, 127) - 1) * 260 / 126);
    if (p[F_SUBMODE] > 0) patch.sub_mode = (uint8_t)zg_fune_bound(p[F_SUBMODE] - 1, ZG_SUB_MOVING, ZG_SUB_PEDAL);
    patch.separation = (uint8_t)zg_fune_bound(patch.separation + p[F_SEPARATION], 0, 127);
    patch.gain = (uint8_t)zg_fune_bound(patch.gain + p[F_GAIN], 0, 127);
    if (p[F_SUSTAIN] > 0) patch.source_sustain = (uint8_t)(p[F_SUSTAIN] >= 2);
    patch.glide = (uint8_t)zg_fune_bound(patch.glide + p[F_GLIDE], 0, 127);
    zg_engine_set_patch(&state->engine, &patch);
    state->engine.preset = (uint8_t)selected;
}

static void zg_fune_observe(zg_fune_track *state, voice_t *v)
{
    unsigned i;
    for (i = 0; i < ZG_FUNE_VOICES; ++i)
        if (state->voices[i] == v)
            return;
    for (i = 0; i < ZG_FUNE_VOICES; ++i) {
        if (!state->voices[i]) {
            state->voices[i] = v;
            return;
        }
    }
}

static void zg_fune_note_on(track_t *track, voice_t *v)
{
    zg_fune_track *state = zg_fune_find(track, 1);
    int legato;
    unsigned i;
    if (!state) return;
    zg_fune_observe(state, v);
    zg_fune_parameters(state, track);
    /* voice_start has already rewritten v->gate/active; those fields cannot
     * tell whether a reused owner was silent. env_out survives sounding starts
     * and is zeroed by the host before an inactive voice starts. */
    legato = v->env_out > 0;
    for (i = 0; i < ZG_FUNE_VOICES; ++i)
        if (state->voices[i] && state->voices[i] != v &&
            state->voices[i]->active && state->voices[i]->gate &&
            state->voices[i]->stage != 4)
            legato = 1;
    state->owner = v;
    state->owner_age = v->age;
    zg_engine_note_on(&state->engine, v->note, v->vel, legato);
}

static int zg_fune_newer(uint32_t a, uint32_t b)
{
    /* Wrap-safe for the host's sequential note counter. */
    return (int32_t)(a - b) > 0;
}

static void zg_fune_block(track_t *track)
{
    zg_fune_track *state = zg_fune_find(track, 0);
    voice_t *held = 0;
    unsigned i;
    if (!state) return;
    zg_fune_parameters(state, track);
    for (i = 0; i < ZG_FUNE_VOICES; ++i) {
        voice_t *v = state->voices[i];
        if (v && v->active && v->gate && v->stage != 4 &&
            (!held || zg_fune_newer(v->age, held->age)))
            held = v;
    }
    if (held) {
        if (held != state->owner || held->age != state->owner_age) {
            state->owner = held;
            state->owner_age = held->age;
            /* Return to a still-held key without resetting oscillator/comb
             * phase or source position. The common engine supplies glide toward the host pitch. */
            zg_engine_note_on(&state->engine, held->note, held->vel, 1);
        }
    } else if (state->owner) {
        zg_engine_note_off(&state->engine, state->owner->note);
        if (!state->owner->active || state->owner->stage == 0) {
            state->owner = 0;
            zg_engine_all_notes_off(&state->engine);
        }
    }
}

static void zg_fune_render(track_t *track, voice_t *v, int32_t *out,
                           uint32_t n, const vmod_t *m)
{
    zg_fune_track *state = zg_fune_find(track, 0);
    int16_t samples[ZG_BLOCK];
    uint32_t i;
    if (!state || state->owner != v || !n || n > ZG_BLOCK || !m || !out)
        return;
    /* FLT moves VOWEL, SHP moves GRIT. Fune's SHP center is 64 << 8. */
    state->engine.patch.vowel = (uint8_t)zg_fune_bound((int)state->engine.patch.vowel + m->cutoff / 256, 0, 127);
    state->engine.patch.grit = (uint8_t)zg_fune_bound((int)state->engine.patch.grit + (m->shape - 16384) / 256, 0, 127);
    zg_engine_render_host(&state->engine, samples, n, m->inc, m->amp0, m->amp1, v->gate != 0);
    /* The common engine already applies host envelope and velocity once and
     * its explicit final policy. Do not run voice_amp a second time. */
    for (i = 0; i < n; ++i)
        out[i] += samples[i];
}

static const char *const ZG_FAMILIES[] = {"Preset", "Bloom Bark", "Formant Snarl", "Upper Chop", "Split Maul", "Crushed Teeth", "Harmonic Rip", 0};
static const char *const ZG_MODES[] = {"Preset", "Native", "Source Image", "Hybrid", 0};
#ifdef ZG_FUNE_CUSTOM_SOURCE_BANK
#ifndef ZG_COMPILED_SOURCE_COUNT
#error "Regenerate the custom source bank with the current compiler metadata"
#endif
#define ZG_FUNE_IMAGE_MAX ZG_COMPILED_SOURCE_COUNT
static const char *const ZG_IMAGES[] = {"Preset", ZG_COMPILED_SOURCE_NAMES, 0};
#else
#define ZG_FUNE_IMAGE_MAX 7
static const char *const ZG_IMAGES[] = {"Preset", "Locked Bloom", "Bloom Bark", "Formant Snarl", "Upper Chop", "Split Maul", "Crushed Teeth", "Harmonic Rip", 0};
#endif
static const char *const ZG_TUNINGS[] = {"Preset", "12 EDO", "19 EDO", "24 EDO", "13 EDO tritave", 0};
static const char *const ZG_ROLLS[] = {"Preset", "Off", "Quarter", "Eighth", "Triplet", "Sixteenth", "Sextuplet", "Thirty-second", 0};
static const char *const ZG_SUBMODES[] = {"Preset", "Half pitch", "Fixed pedal", 0};
static const char *const ZG_SUSTAINS[] = {"Preset", "Finite source", "Sustain loop", 0};
static const char *const ZG_RATES[] = {
    "Preset", "40", "42", "44", "46", "48", "50", "52", "54", "56", "58", "60", "62", "64", "66", "68", "70", "73", "75", "77", "79", "81", "83", "85", "87", "89", "91", "93", "95", "97", "99", "101", "103", "106", "108", "110", "112", "114", "116", "118", "120", "122", "124", "126", "128", "130", "132", "134", "136", "139", "141", "143", "145", "147", "149", "151", "153", "155", "157", "159", "161", "163", "165", "167", "170", "172", "174", "176", "178", "180", "182", "184", "186", "188", "190", "192", "194", "196", "198", "200", "203", "205", "207", "209", "211", "213", "215", "217", "219", "221", "223", "225", "227", "229", "231", "233", "236", "238", "240", "242", "244", "246", "248", "250", "252", "254", "256", "258", "260", "262", "264", "266", "269", "271", "273", "275", "277", "279", "281", "283", "285", "287", "289", "291", "293", "295", "297", "300", 0
};

static const engine_t ZG_DSP = {
    .name = "ZaaggenZ",
    .page_title = {"SOURCE", "LAYERS"},
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .note_on = zg_fune_note_on,
    .render = zg_fune_render,
    .block = zg_fune_block,
};

static const KEngine ZG_DESC = {
    .name = "ZaaggenZ",
    .uid = K_UID('z', 'g', 'f', '1'),
    .page_title = {"", "SOURCE", "LAYERS"},
    .edit = {
        {"CHAR", KF_ENUM, 0, 6, 0, ZG_FAMILIES, 0},
        {"GRIT", KF_BIPCT, -127, 127, 0, 0, 0},
        {"MOTN", KF_BIPCT, -127, 127, 0, 0, 0},
        {"VOWL", KF_BIPCT, -127, 127, 0, 0, 0},
        {"PRST", KF_ENUM, 0, 35, 0, zg_preset_names, 0},
        {"MODE", KF_ENUM, 0, 3, 0, ZG_MODES, 0},
        {"IMAG", KF_ENUM, 0, ZG_FUNE_IMAGE_MAX, 0, ZG_IMAGES, 0},
        {"TUNE", KF_ENUM, 0, 4, 0, ZG_TUNINGS, 0},
        {"LINE", KF_BIPCT, -127, 127, 0, 0, 0},
        {"EXCT", KF_BIPCT, -127, 127, 0, 0, 0},
        {"BODY", KF_BIPCT, -127, 127, 0, 0, 0},
        {"AUX", KF_BIPCT, -127, 127, 0, 0, 0},
        {"SUB", KF_BIPCT, -127, 127, 0, 0, 0},
        {"ROLL", KF_ENUM, 0, 7, 0, ZG_ROLLS, 0},
        {"RATE", KF_ENUM, 0, 127, 0, ZG_RATES, "bpm"},
        {"SUBM", KF_ENUM, 0, 2, 0, ZG_SUBMODES, 0},
        {"SEPR", KF_BIPCT, -127, 127, 0, 0, 0},
        {"GAIN", KF_BIPCT, -127, 127, 0, 0, 0},
        {"TAIL", KF_ENUM, 0, 2, 0, ZG_SUSTAINS, 0},
        {"GLID", KF_BIPCT, -127, 127, 0, 0, 0},
    },
    .macro = {0, 1, 2, 3},
    .nparams = F_PARAM_COUNT,
    .page2 = 8,
};

static const KPlugin ZG_PLUGIN = {
    .size = sizeof(KPlugin), .abi = K_PLUGIN_ABI, .version = 1,
    .kind = K_PLUG_SYNTH, .desc = &ZG_DESC, .dsp = &ZG_DSP,
    .fs = FS, .ram = sizeof(ZG_TRACK),
    .cost = 0, /* Unmeasured on physical FM-1; zero means not declared. */
    .needs = 0,
};

KREISEL_PLUGIN_EXPORT const KPlugin *kreisel_plugin(void)
{
    return &ZG_PLUGIN;
}

#ifdef ZG_FUNE_TEST
void zg_fune_test_reset(void) { memset(ZG_TRACK, 0, sizeof(ZG_TRACK)); }
const zg_engine *zg_fune_test_engine(const track_t *track)
{
    zg_fune_track *state = zg_fune_find(track, 0);
    return state ? &state->engine : 0;
}
const voice_t *zg_fune_test_owner(const track_t *track)
{
    zg_fune_track *state = zg_fune_find(track, 0);
    return state ? state->owner : 0;
}
#endif
