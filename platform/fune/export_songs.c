/* SPDX-License-Identifier: MIT
 * Author and validate KRSG songs through the real pinned Fune implementation.
 * Auditions use its sequencer and integer I2S renderer, not a parallel MIDI model.
 */
#include "kreisel.h"
#include "kreisel_audio.h"
#include "kreisel_plugin_load.h"
#include "kreisel_engine_api.h"
#include "kreisel_song.h"
#include "zg_fune_song_metadata.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static KApp app, loaded;
static KAudio audio;
static uint8_t bytes[4096];
static int32_t samples[64];

static void u16(FILE *f, unsigned v)
{
    assert(fputc((int)(v & 255u), f) != EOF);
    assert(fputc((int)((v >> 8) & 255u), f) != EOF);
}
static void u32(FILE *f, uint32_t v)
{
    u16(f, v & 65535u); u16(f, v >> 16);
}
static void wav_header(FILE *f, uint32_t frames)
{
    assert(fwrite("RIFF", 1, 4, f) == 4); u32(f, 36u + frames * 4u);
    assert(fwrite("WAVEfmt ", 1, 8, f) == 8); u32(f, 16);
    u16(f, 1); u16(f, 2); u32(f, 44100); u32(f, 44100 * 4);
    u16(f, 4); u16(f, 16);
    assert(fwrite("data", 1, 4, f) == 4); u32(f, frames * 4u);
}

int main(int argc, char **argv)
{
    static const KDeviceCaps desktop = {44100, 1u << 20, 2000, 6, K_NEED_FLOAT};
    const KPlugin *plugin;
    const struct zg_export_song *song;
    unsigned index, t, p, s, j;
    int engine;
    char path[4096];
    FILE *f;
    size_t count;
    uint32_t frame, frames, peak = 0, limiter_max = 0, limited_blocks = 0;
    double square_sum = 0;
    if (argc != 3) { fprintf(stderr, "usage: zg_fune_export_songs OUTPUT_DIRECTORY SONG_INDEX\n"); return 2; }
    index = (unsigned)strtoul(argv[2], 0, 10);
    assert(index < ZG_EXPORT_SONG_COUNT);
    song = &ZG_EXPORT_SONG[index];
    k_plugin_set_caps(&desktop);
    engine = k_plugin_load_file(ZG_PLUGIN_PATH);
    assert(engine >= K_ENGINE_COUNT);
    plugin = k_plugin_at(engine);
    assert(plugin->desc->nparams == 20);
    k_init(&app);
    snprintf(app.song_name, sizeof app.song_name, "%s", song->display);
    app.bpm = song->bpm;
    app.swing = song->swing;
    app.clock_src = 0;
    for (t = 0; t < K_TRACKS; ++t) {
        const struct zg_export_track *track = &song->track[t];
        app.sound[t] = engine;
        for (p = 0; p < 4; ++p)
            for (s = 0; s < 16; ++s)
                k_roll_clear(&app, (int)t, (int)p, (int)s);
        for (j = 0; j < 20; ++j) {
            assert(track->params[j] >= plugin->desc->edit[j].min);
            assert(track->params[j] <= plugin->desc->edit[j].max);
            app.params[t][K_PAGE_SOUND][4 + j] = track->params[j];
        }
        for (j = 0; j < 4; ++j) app.params[t][K_PAGE_ENV1][j] = track->env[j];
        app.params[t][K_PAGE_MIXER][0] = track->level;
        app.params[t][K_PAGE_MIXER][1] = track->pan;
        app.params[t][K_PAGE_MIXER][2] = app.params[t][K_PAGE_MIXER][3] = 0;
        for (j = 0; j < track->count; ++j) {
            const unsigned char *note = track->note[j];
            assert(k_roll_add(&app, (int)t, note[0], note[1], note[2], note[3], note[4]) >= 0);
        }
        for (s = 0; s < song->scenes; ++s)
            k_song_cell_set(&app, (int)s, (int)t, song->scene[s][t]);
    }
    count = k_song_save(&app, bytes, sizeof bytes);
    assert(count > 0 && count <= 3584); /* actual four-track FM-1 song buffer */
    assert(k_song_check(bytes, count) == K_SONG_OK);
    k_init(&loaded);
    assert(k_song_load(&loaded, bytes, count) == K_SONG_OK);
    assert(!loaded.playing && !loaded.song_play);
    assert(loaded.bpm == app.bpm && loaded.swing == app.swing);
    assert(!strcmp(loaded.song_name, app.song_name));
    assert(!memcmp(loaded.song, app.song, sizeof app.song));
    for (t = 0; t < K_TRACKS; ++t) {
        assert(loaded.sound[t] == engine);
        assert(!memcmp(loaded.roll[t], app.roll[t], sizeof app.roll[t]));
        assert(!memcmp(loaded.steps[t], app.steps[t], sizeof app.steps[t]));
        for (j = 0; j < 20; ++j)
            assert(loaded.params[t][K_PAGE_SOUND][4 + j] == app.params[t][K_PAGE_SOUND][4 + j]);
        for (j = 0; j < 4; ++j) {
            assert(loaded.params[t][K_PAGE_ENV1][j] == app.params[t][K_PAGE_ENV1][j]);
            assert(loaded.params[t][K_PAGE_MIXER][j] == app.params[t][K_PAGE_MIXER][j]);
        }
    }
    assert(snprintf(path, sizeof path, "%s/%s.fune", argv[1], song->filename) < (int)sizeof path);
    f = fopen(path, "wb"); if (!f) { perror(path); return 3; }
    assert(fwrite(bytes, 1, count, f) == count); assert(!fclose(f));

    /* Render the reloaded file: sequencer lengths, scene changes and voice
     * ownership take precisely the host path. MASTER=100 makes headroom checks
     * independent of the user's physical volume knob (not saved in a song). */
    loaded.master = 100;
    loaded.song_scene = 0; loaded.song_play = 1; loaded.playing = 1;
    k_audio_init(&audio, 44100);
    k_audio_sync(&audio, &loaded);
    frames = (uint32_t)(44100u * 240u * song->scenes / song->bpm + 44100u * 4u);
    frames = (frames + 31u) & ~31u;
    assert(snprintf(path, sizeof path, "%s/%s.wav", argv[1], song->filename) < (int)sizeof path);
    f = fopen(path, "wb"); if (!f) { perror(path); return 4; }
    wav_header(f, frames);
    for (frame = 0; frame < frames; frame += 32u) {
        int was_playing = loaded.playing;
        k_audio_render_s32(&audio, samples, 32);
        k_audio_song_follow(&audio, &loaded);
        if (was_playing != loaded.playing) k_audio_sync(&audio, &loaded);
        if (audio.lim_red) ++limited_blocks;
        if (audio.lim_red > limiter_max) limiter_max = audio.lim_red;
        for (j = 0; j < 64; ++j) {
            /* Host s32 is a signed 16-bit value shifted seven places. Undo
             * only that transport alignment, with no normalisation. */
            const int32_t value = samples[j] / 128;
            const uint32_t magnitude = (uint32_t)(value < 0 ? -value : value);
            assert(value >= -32768 && value <= 32767);
            if (magnitude > peak) peak = magnitude;
            square_sum += (double)value * (double)value;
            u16(f, (uint16_t)value);
        }
    }
    assert(!fclose(f));
    assert(!loaded.playing && !loaded.song_play);
    assert(k_audio_voices_on() == 0);
    assert(peak > 128 && peak < 29204);
    assert(limiter_max == 0); /* arrange/fader balance, never master compression */
    assert(snprintf(path, sizeof path, "%s/%s.metrics.json", argv[1], song->filename) < (int)sizeof path);
    f = fopen(path, "w"); if (!f) { perror(path); return 5; }
    fprintf(f, "{\n  \"frames\": %u,\n  \"sample_rate\": 44100,\n  \"channels\": 2,\n  \"peak\": %u,\n  \"rms\": %.9f,\n  \"peak_dbfs\": %.5f,\n  \"limiter_reduction_max_q16\": %u,\n  \"limited_blocks\": %u,\n  \"voices_after_tail\": %d,\n  \"song_bytes\": %zu\n}\n",
            frames, peak, sqrt(square_sum / (2.0 * frames)) / 32768.0,
            20.0 * log10(peak / 32768.0), limiter_max, limited_blocks, k_audio_voices_on(), count);
    assert(!fclose(f));
    printf("%s: %zu bytes, %u scenes, %.2f s actual Fune stereo audio, peak %.2f dBFS, limiter untouched, serializer/notes/scenes/release PASS\n",
           song->filename, count, song->scenes, frames / 44100.0, 20.0 * log10(peak / 32768.0));
    return 0;
}
