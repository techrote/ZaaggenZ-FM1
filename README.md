# ZaaggenZ-FM1

**A fixed-point Zaag instrument for Fune on M-VAVE FM-1, with a native Windows audition player and the same portable C synthesis core.**

This first implementation is available for owner review and physical evaluation. The plugin compiles with the real pi32v2 toolchain. Host tests and target image validation are separate from an **unperformed physical FM-1 qualification**.

## Play on Windows

Download the Windows package from the latest successful **Native instrument qualification** run in [Actions](https://github.com/techrote/ZaaggenZ-FM1/actions). Extract the ZIP and run `Start-Audition.cmd` or `ZaaggenZ-Audition.exe`. Playback needs no Python, compiler, external audio framework or installer.

- Press **Play phrase** for an immediate melodic demonstration.
- Play **Z S X D C V G B H N J M** from C2 or **Q 2 W 3 E R 5 T 6 Y 7 U** from C3.
- **[ / ]** changes octave, **Space** toggles the phrase, **Escape** releases all notes.
- Select a MIDI input to use an external keyboard.
- Choose one of **36 factory instruments** and explore **CHARACTER, GRIT, MOTION and VOWEL**. Five layer faders, glide, release, tuning, source selection, rolls and tempo remain accessible.
- **Export current WAV** renders the embedded core with its complete bounded release tail. The separate `zaag-render.exe` supports riffs, raw/source-bus/layer stems and repeatable benchmarks.

The initial sound uses a preserved source image with live character processing. It retains the authored transient and measured lower host cost. Native and hybrid paths remain distinct musical choices; desktop timing does not establish FM-1 polyphony.

## Implemented instrument

| Area | Behavior |
|---|---|
| Sources | Native harmonic lattice, seven authenticated post-topology source images, or hybrid blend |
| Characters | Bloom Bark, Formant Snarl, Upper Chop, Split Maul, Crushed Teeth, Harmonic Rip |
| Image playback | Eight prefiltered mips, interpolated varispeed, full attack/tail, optional matched sustain loop, smoothed mip changes |
| Layers | Shared SYNTHLINE + EXCITER character graph; independent persistent BODY, AUX and SUB |
| Motion | Complementary layers, rhythmic retriggers, moving-root/pedal SUB and bounded glide |
| Tuning | 12-, 19-, 24-EDO and 13 equal divisions of a tritave |
| Mix | Explicit smoothed gains, source-role mute before topology, downward-only separation, one final output policy |
| Fune | ABI 1.4 plugin, four independent monophonic tracks, last-note priority and held-note fallback |
| Presets | 36 authored recipes plus real Fune files exported and round-tripped by the pinned host serializer |
| Compiler | Authenticated legacy recovery, selected recipes and bounded real RenderRecipe import, const/XIP banks and provenance |
| Evaluation | Windows WinMM player, portable CLI, actual-C comparative WAVs and automated numeric/acoustic tests |

This is a causal embedded interpretation, not a bit-identical replacement for offline filters or duration-preserving FFT transposition. Image varispeed changes duration with pitch; sustain looping is an explicit instrument extension. The desktop reference remains untouched and authoritative.

## Install on FM-1

Read [installation, controls and recovery](docs/fm1-install.md). The artifact is a **Fune `.kpl` sound-engine plugin**, not a stock firmware or bootloader image. Fune must already be installed with the compatible ABI. The upstream manager provides checking, installation, enable/disable and removal. This project does not flash hardware automatically or claim an unverified stock-firmware recovery method.

The actual image loader supplies **2 KiB total static plugin RAM**. Builds check real `.data` + `.bss`, XIP fit, relocation consistency, CRC, floating-point absence, CPU multiply errata and divide-free generated sample paths. Target maps, ELF/disassembly and conservative direct-call stack estimates accompany the image.

## Build and qualify

The portable core needs a C11 compiler and CMake:

```sh
cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --config Release --parallel
ctest --test-dir build/host -C Release --output-on-failure
build/host/zaag-render --preset 0 --pattern riff --seconds 8 --out elastic-bloom.wav
```

Visual Studio places executables under `build/host/Release/`. Assertions remain enabled in Release tests. The Windows player uses only system libraries.

Real FM-1 compilation on Linux x86-64 or WSL2:

```sh
python tools/get_fune_toolchain.py
python tools/build_fune.py
python tools/install_fune.py check build/fm1/zaaggenz.kpl
python tools/test_fune_host.py
python tools/export_fune_presets.py
```

The vendor compiler download is pinned and hash checked. For acoustic/reference tests, install `requirements-reference.txt` and use the commands in the [qualification workflow](.github/workflows/qualify.yml). Audition material is rendered by compiled C, not a separate Python synth.

See [reference fidelity and compiler usage](docs/reference-fidelity.md), [native mechanisms and measurements](docs/native-dsp.md), and `assets/source_bank_manifest.json`. The manifest records exact recipes, source/render hashes, numeric tool versions, common scaling and representation metrics. `tools/compare_architectures.py` produces source/reference/native comparisons. `tests/test_auditions.py` checks all 36 actual-C sounds for audibility, clipping, tail completion and spectral diversity.

`examples/locked_bloom_render_recipe.json` demonstrates real desktop recipe import. Custom banks go to a separate directory by default. Unsupported owner domains are rejected explicitly rather than silently dropping their responsibilities.

## Qualification boundaries

**Physical installation, DAC sound, flash-chip variation, sustained XIP/cache behavior, audio deadlines and maximum safe simultaneous native/hybrid tracks require owner hardware evaluation.** CPU cost is left unspecified in the plugin instead of fabricating a measurement. Stack estimates exclude the host caller and interrupts.

The current generic Fune plugin path does not expose transport tempo or implement its older advertised glide hook. The adapter implements glide itself and provides an explicit internal RATE; match it to the song tempo. Fune retains sequencing, MIDI routing and amplitude envelopes.

## Provenance and licenses

- Zaaggenz: `d7e580c190d00e1d00a573a6b845c75ddddc1d4c`.
- Fune: `dc1eb1b15a6bc108e09098e554ea8b0a7dbe8a1b`.
- JieLi compiler: `20260730.1`, hash pinned in the downloader.

New code is MIT. The Fune subset preserves MIT/MPL-2.0 boundaries. Derived source audio retains `LicenseRef-Owner-Provided-Unspecified`; this project does not invent an upstream redistribution grant. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
