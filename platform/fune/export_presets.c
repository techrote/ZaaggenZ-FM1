/* SPDX-License-Identifier: MIT
 * Use the real pinned Fune serializer to create portable KRSP preset files.
 * Parameter IDs, checksums and host envelope records are never reimplemented.
 */
#include "kreisel.h"
#include "kreisel_plugin_load.h"
#include "kreisel_engine_api.h"
#include "kreisel_song.h"
#include "zg_fune_recipe_metadata.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static KApp app, loaded;
static uint8_t bytes[4096];

int main(int argc, char **argv)
{
    static const KDeviceCaps desktop = {44100, 1u << 20, 2000, 6, K_NEED_FLOAT};
    const KPlugin *plugin;
    unsigned i, j;
    int index;
    char path[4096];
    if (argc != 2) { fprintf(stderr, "usage: zg_fune_export OUTPUT_DIRECTORY\n"); return 2; }
    k_plugin_set_caps(&desktop);
    index = k_plugin_load_file(ZG_PLUGIN_PATH);
    assert(index >= K_ENGINE_COUNT);
    plugin = k_plugin_at(index);
    for (i = 0; i < ZG_EXPORT_RECIPE_COUNT; ++i) {
        FILE *f;
        size_t count;
        k_init(&app);
        app.sound[0] = index;
        for (j = 0; j < plugin->desc->nparams; ++j)
            app.params[0][K_PAGE_SOUND][4 + j] = plugin->desc->edit[j].def;
        app.params[0][K_PAGE_SOUND][4 + 4] = (int)i;
        for (j = 0; j < 4; ++j)
            app.params[0][K_PAGE_ENV1][j] = ZG_EXPORT_RECIPE[i].env[j];
        count = k_preset_save(&app, 0, ZG_EXPORT_RECIPE[i].name, bytes, sizeof bytes);
        assert(count > 0 && count <= 384); /* physical FM-1 preset cell */
        assert(k_preset_check(bytes, count) == K_SONG_OK);
        k_init(&loaded);
        assert(k_preset_load(&loaded, 0, bytes, count) == K_SONG_OK);
        assert(loaded.sound[0] == index);
        for (j = 0; j < plugin->desc->nparams; ++j)
            assert(loaded.params[0][K_PAGE_SOUND][4 + j] == app.params[0][K_PAGE_SOUND][4 + j]);
        for (j = 0; j < 4; ++j)
            assert(loaded.params[0][K_PAGE_ENV1][j] == app.params[0][K_PAGE_ENV1][j]);
        if (snprintf(path, sizeof path, "%s/%s.fune", argv[1], ZG_EXPORT_RECIPE[i].filename) >= (int)sizeof path) return 3;
        f = fopen(path, "wb");
        if (!f) { perror(path); return 4; }
        if (fwrite(bytes, 1, count, f) != count || fclose(f)) return 5;
        printf("%s: %zu bytes, Fune preset/parameter/envelope round-trip PASS\n", ZG_EXPORT_RECIPE[i].filename, count);
    }
    return 0;
}
