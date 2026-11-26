/* SPDX-License-Identifier: MIT */
#include "kreisel_sdk.h"
#include "zg/engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

const KPlugin *kreisel_plugin(void);
void zg_fune_test_reset(void);
const zg_engine *zg_fune_test_engine(const track_t *);
const voice_t *zg_fune_test_owner(const track_t *);

static track_t tracks[5];
static voice_t voices[5][6];
static uint32_t age;
static const engine_t *dsp;

static void initialize(void)
{
    unsigned t, k;
    const KPlugin *plugin = kreisel_plugin();
    zg_fune_test_reset();
    memset(tracks, 0, sizeof tracks);
    memset(voices, 0, sizeof voices);
    age = 0;
    dsp = plugin->dsp;
    assert(plugin->abi == K_PLUGIN_ABI && plugin->fs == 44100);
    assert(plugin->needs == 0 && plugin->cost == 0);
    assert(plugin->desc->nparams == 20);
    assert(zg_preset_count == 36);
    for (t = 0; t < 5; ++t)
        for (k = 0; k < plugin->desc->nparams; ++k)
            tracks[t].p[P_E0 + k] = plugin->desc->edit[k].def;
}

static void note(unsigned t, unsigned slot, unsigned key)
{
    voice_t *v = &voices[t][slot];
    if (!v->active) v->env_out = 0;
    v->note = (uint8_t)key;
    v->vel = 100;
    v->gate = v->active = 1;
    v->stage = 1;
    v->age = ++age;
    dsp->note_on(&tracks[t], v);
}

static int energy(unsigned t, unsigned slot)
{
    int32_t out[32] = {0};
    voice_t *v = &voices[t][slot];
    vmod_t m = {0};
    unsigned k;
    int sum = 0;
    m.inc = zg_pitch_increment(v->note, ZG_12EDO, 44100);
    m.pitch16 = v->note * 16;
    m.amp0 = m.amp1 = 20000;
    m.shape = 16384;
    dsp->render(&tracks[t], v, out, 32, &m);
    v->env_out = m.amp1;
    for (k = 0; k < 32; ++k) {
        assert(out[k] >= -32768 && out[k] <= 32767);
        sum += out[k] < 0 ? -out[k] : out[k];
    }
    return sum;
}

static void mono_ownership(void)
{
    zg_engine before;
    unsigned b;
    initialize();
    note(0, 0, 48);
    dsp->block(&tracks[0]);
    assert(zg_fune_test_owner(&tracks[0]) == &voices[0][0]);
    for (b = 0; b < 20; ++b) assert(energy(0, 0) >= 0);
    note(0, 1, 60);
    dsp->block(&tracks[0]);
    assert(zg_fune_test_owner(&tracks[0]) == &voices[0][1]);
    before = *zg_fune_test_engine(&tracks[0]);
    assert(energy(0, 0) == 0);
    assert(memcmp(&before, zg_fune_test_engine(&tracks[0]), sizeof before) == 0);
    assert(energy(0, 1) > 0);
    voices[0][1].gate = 0;
    voices[0][1].stage = 3;
    dsp->block(&tracks[0]);
    assert(zg_fune_test_owner(&tracks[0]) == &voices[0][0]);
    assert(zg_fune_test_engine(&tracks[0])->note == 48);
    assert(energy(0, 1) == 0);
    assert(energy(0, 0) > 0);
    voices[0][0].gate = 0;
    voices[0][0].stage = 3;
    dsp->block(&tracks[0]);
    assert(zg_fune_test_owner(&tracks[0]) == &voices[0][0]);
    assert(zg_fune_test_engine(&tracks[0])->gate == 0);
    voices[0][0].active = 0;
    voices[0][0].stage = 0;
    dsp->block(&tracks[0]);
    assert(zg_fune_test_owner(&tracks[0]) == 0);
    /* Reuse that same host voice after silence: it must be a fresh source hit. */
    note(0, 0, 55);
    assert(zg_fune_test_engine(&tracks[0])->note == 55);
    assert(zg_fune_test_engine(&tracks[0])->native.age == 0);
}

static void tracks_are_independent(void)
{
    zg_engine snapshot[4];
    unsigned t;
    initialize();
    for (t = 0; t < 4; ++t) {
        tracks[t].p[P_E0 + 4] = (int16_t)(t * 6);
        note(t, 0, 48 + t * 3);
        dsp->block(&tracks[t]);
        energy(t, 0);
        snapshot[t] = *zg_fune_test_engine(&tracks[t]);
    }
    energy(2, 0);
    for (t = 0; t < 4; ++t)
        if (t != 2) assert(!memcmp(&snapshot[t], zg_fune_test_engine(&tracks[t]), sizeof(zg_engine)));
    note(4, 0, 76);
    dsp->block(&tracks[4]);
    assert(zg_fune_test_engine(&tracks[4]) == 0);
    assert(energy(4, 0) == 0);
}

static void params_presets_and_additive_output(void)
{
    unsigned p, k;
    vmod_t m = {0};
    int32_t out[32];
    initialize();
    note(0, 0, 48);
    for (p = 0; p < 36; ++p) {
        tracks[0].p[P_E0 + 4] = (int16_t)p;
        tracks[0].p[P_E0 + 1] = p & 1 ? 127 : -127;
        tracks[0].p[P_E0 + 2] = p & 2 ? 127 : -127;
        tracks[0].p[P_E0 + 3] = p & 4 ? 127 : -127;
        dsp->block(&tracks[0]);
        assert(zg_fune_test_engine(&tracks[0])->preset == p);
        assert(energy(0, 0) >= 0);
    }
    m.inc = zg_pitch_increment(48, ZG_12EDO, 44100);
    m.shape = 16384;
    /* Zero host amplitude must add exact zero, preserving earlier tracks. */
    for (k = 0; k < 32; ++k) out[k] = 1234;
    dsp->block(&tracks[0]);
    dsp->render(&tracks[0], &voices[0][0], out, 32, &m);
    for (k = 0; k < 32; ++k) assert(out[k] == 1234);
    /* A foreign host block size cannot overrun the adapter's fixed scratch. */
    dsp->render(&tracks[0], &voices[0][0], out, 33, &m);
    for (k = 0; k < 32; ++k) assert(out[k] == 1234);
}

static void held_controls_and_repeated_voice_reuse(void)
{
    unsigned cycle, i;
    initialize();
    note(0, 0, 48);
    for (cycle = 0; cycle < 12; ++cycle) {
        dsp->block(&tracks[0]);
        energy(0, 0);
    }
    /* Identical host block parameters must not continually restart a preset
     * transition; doing that audibly dulls every sustained note. */
    assert(zg_fune_test_engine(&tracks[0])->fade == 32767);
    for (cycle = 0; cycle < 1000; ++cycle) {
        unsigned slot = cycle % 6;
        tracks[0].p[P_E0 + 4] = (int16_t)(cycle % 36);
        tracks[0].p[P_E0 + 1] = (int16_t)((int)(cycle % 255) - 127);
        tracks[0].p[P_E0 + 2] = (int16_t)((int)((cycle * 7) % 255) - 127);
        note(0, slot, 36 + cycle % 48);
        dsp->block(&tracks[0]);
        assert(zg_fune_test_owner(&tracks[0]) == &voices[0][slot]);
        for (i = 0; i < 6; ++i)
            if (i != slot) assert(energy(0, i) == 0);
        energy(0, slot);
        voices[0][slot].gate = 0;
        voices[0][slot].stage = 3;
        dsp->block(&tracks[0]);
    }
    for (i = 0; i < 6; ++i) {
        voices[0][i].active = voices[0][i].gate = 0;
        voices[0][i].stage = 0;
    }
    dsp->block(&tracks[0]);
    assert(!zg_fune_test_owner(&tracks[0]));
    for (i = 0; i < 6; ++i) assert(energy(0, i) == 0);
}

int main(void)
{
    mono_ownership();
    tracks_are_independent();
    params_presets_and_additive_output();
    held_controls_and_repeated_voice_reuse();
    printf("Fune adapter: last-note/fallback/release, four-track isolation, refusal, 36 presets, additive output, stable held controls, 1000 voice reuses PASS; host state %u bytes\n", (unsigned)kreisel_plugin()->ram);
    return 0;
}
