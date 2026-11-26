/* SPDX-License-Identifier: MIT
 * Loads the actual shared plugin through pinned Fune's registry/audio engine,
 * renders all factory selections, exercises four tracks and song identity.
 */
#include "kreisel.h"
#include "kreisel_audio.h"
#include "kreisel_plugin_load.h"
#include "kreisel_engine_api.h"
#include "kreisel_song.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static KApp app;
static KAudio audio;
static float samples[2048];
static unsigned char song[65536];
static KApp reloaded;

static double render(unsigned count)
{
    double energy = 0;
    unsigned block, i;
    for (block = 0; block < count; ++block) {
        k_audio_render(&audio, samples, 1024);
        for (i = 0; i < 2048; ++i) {
            assert(isfinite(samples[i]) && fabsf(samples[i]) <= 1.0f);
            energy += (double)samples[i] * samples[i];
        }
    }
    return energy;
}

int main(void)
{
    static const KDeviceCaps desktop = {44100, 1u << 20, 2000, 6, K_NEED_FLOAT};
    const KPlugin *plugin;
    int engine, i, t;
    size_t bytes;
    double energy = 0;
    k_plugin_set_caps(&desktop);
    engine = k_plugin_load_file(ZG_PLUGIN_PATH);
    assert(engine >= K_ENGINE_COUNT);
    plugin = k_plugin_at(engine);
    assert(k_plugin_check(plugin, 0) == 0);
    assert(plugin->desc->uid == K_UID('z', 'g', 'f', '1'));
    assert(plugin->needs == 0 && plugin->fs == 44100 && plugin->version == 1);
    k_init(&app);
    for (t = 0; t < 4; ++t) {
        app.sound[t] = engine;
        for (i = 0; i < plugin->desc->nparams; ++i)
            app.params[t][K_PAGE_SOUND][4 + i] = plugin->desc->edit[i].def;
        app.params[t][K_PAGE_SOUND][4 + 4] = t * 6;
    }
    k_audio_init(&audio, 44100);
    k_audio_sync(&audio, &app);
    for (t = 0; t < 4; ++t) k_audio_note(&audio, t, 48 + t * 3, 100, 1);
    energy += render(10);
    assert(energy > 0.001);
    /* Latest held key then release: the mono adapter returns to the first. */
    k_audio_note(&audio, 0, 60, 100, 1);
    assert(render(2) > 0.001);
    k_audio_note(&audio, 0, 60, 100, 0);
    assert(render(2) > 0.001);
    for (i = 0; i < 36; ++i) {
        app.params[0][K_PAGE_SOUND][4 + 4] = i;
        k_audio_sync(&audio, &app);
        energy += render(2);
    }
    /* Real host engine switching includes its four-block fade and voice reset.
     * Return to the plugin repeatedly, then confirm all keys eventually end. */
    for (i = 0; i < 24; ++i) {
        app.sound[0] = 0;
        k_audio_sync(&audio, &app);
        render(1);
        app.sound[0] = engine;
        k_audio_sync(&audio, &app);
        render(1);
        k_audio_note(&audio, 0, 48 + i % 12, 100, 1);
        assert(render(1) > 0.00001);
        k_audio_note(&audio, 0, 48 + i % 12, 100, 0);
    }
    for (t = 0; t < 4; ++t)
        for (i = 0; i < 128; ++i)
            k_audio_note(&audio, t, i, 0, 0);
    render(500); /* More than the host's longest envelope, including its tail. */
    assert(k_audio_voices_on() == 0);
    bytes = k_song_save(&app, song, sizeof song);
    assert(bytes > 0);
    k_init(&app);
    assert(k_song_load(&app, song, bytes) == 0);
    assert(!k_track_missing(&app, 0));
    assert(app.sound[0] == engine && app.params[0][K_PAGE_SOUND][4 + 4] == 35);
    /* The host serializes each plugin value as a signed byte. Exercise both
     * ends of every advertised range, including BPM's encoded choices. */
    for (t = 0; t < 2; ++t) {
        for (i = 0; i < plugin->desc->nparams; ++i)
            app.params[0][K_PAGE_SOUND][4 + i] = t ? plugin->desc->edit[i].max : plugin->desc->edit[i].min;
        bytes = k_song_save(&app, song, sizeof song);
        assert(bytes > 0);
        k_init(&reloaded);
        assert(k_song_load(&reloaded, song, bytes) == 0);
        for (i = 0; i < plugin->desc->nparams; ++i)
            assert(reloaded.params[0][K_PAGE_SOUND][4 + i] == app.params[0][K_PAGE_SOUND][4 + i]);
    }
    k_plugin_reset();
    k_init(&app);
    assert(k_song_load(&app, song, bytes) == 0 && k_track_missing(&app, 0));
    printf("Actual pinned Fune: shared-library load, descriptor validation, four-track render, 36 presets, key fallback, repeated engine switches, all notes released, all parameter extrema round-trip and missing-plugin identity PASS; energy %.6f, song %zu bytes\n", energy, bytes);
    return 0;
}
