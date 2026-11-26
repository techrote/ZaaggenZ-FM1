# FM-1 installation, controls and recovery

ZaaggenZ-FM1 is a **Fune sound-engine plugin**, identity `zgf1`, plugin version 1,
built against engine API **1.4**. The deliverable is `zaaggenz.kpl`; it is not a
stock-firmware update image. The project does not replace or patch a bootloader.

The native synthesis core, native audition programs and plugin use the same C
source. The plugin has one independent monophonic instrument per FM-1 track, so
four tracks can play separate sounds. Within a track the newest held key wins;
releasing it returns to the newest remaining held key. The current note keeps its
release when the last key is released. Host envelopes and velocity are applied once.

## Compatibility and what has been qualified

The exact Fune revision is
[`dc1eb1b15a6bc108e09098e554ea8b0a7dbe8a1b`](https://github.com/hugelton/Fune/tree/dc1eb1b15a6bc108e09098e554ea8b0a7dbe8a1b).
Its source supports loadable `.kpl` images, whereas older introductory paragraphs
in its SDK manual describe an earlier built-in-only stage. This project follows
the actual image builder and loader at the pinned revision.

| Item | Verified contract |
| --- | --- |
| Processor | JieLi AC791N/WL82, pi32v2; no hardware floating-point unit |
| Plugin audio | Signed integer Q15, additive mono output, blocks of 32 samples |
| DSP build rate | 44,100 Hz; Fune's physical I2S rate is 44,117 Hz and its host corrects note increments |
| Controls | Four encoders, 27 keys, 14 buttons; 240 × 240 display |
| Flash | Plugin images execute directly from the reserved 292 KiB XIP region, flash offsets `0x097000` through `0x0DFFFF` |
| Plugin static state | **2,048 bytes total** for all plugin images, at `0x01C08000`; this actual image pool is smaller than the generic 16 KiB capability value mentioned elsewhere upstream |
| Host voice bookkeeping | Six host voices per track; ZaaggenZ renders only its selected mono voice |
| Compiler | Official JieLi Linux x86-64 toolchain `20260730.1`, clang 4.0.1; archive hash pinned by the fetch script |

Target compilation, relocation agreement, image checksums, integer-only linkage,
the pi32v2 multiply-erratum scan, static RAM and flash limits are automated. The
plugin also loads and renders through the **actual pinned Fune desktop host**,
including its registry, audio scheduler and song serializer. These are host and
target-build results. **Physical FM-1 audio, worst-case block time, XIP stalls and
stack high-water have not been qualified.** Fune's own pinned SDK documentation
also states that its plugin-image route had been host-tested but not run on a
device. A successful build is not a measured real-time hardware pass.

The reverse-engineering reference is
[`AL-255/FM-1-RE@ec832f281ca165a7d5e0722985f3acb1465046d5`](https://github.com/AL-255/FM-1-RE/tree/ec832f281ca165a7d5e0722985f3acb1465046d5).
Its stock-firmware map is a hardware reference, not this plugin's linker map.

## Install on an FM-1 already running compatible Fune

Use a stopped Fune transport and a USB data cable. Python 3 with `pyserial` is
needed for the supplied console tools. The `--port` examples use Windows `COM4`;
select the port shown for your own FM-1. On Linux supply its `/dev/ttyACM...` path.

```sh
python -m pip install pyserial
python tools/install_fune.py check build/fm1/zaaggenz.kpl
python tools/install_fune.py --port COM4 list
python tools/install_fune.py --port COM4 install build/fm1/zaaggenz.kpl
python tools/install_fune.py --port COM4 list
```

The upstream installer asks Fune to allocate reserved plugin flash and RAM,
relocates the image to the addresses Fune supplies, transfers it with a CRC and
publishes the image header only after verification. It does not write application,
bootloader, Bluetooth-address or song-save regions. Fune refuses an install while
the transport is playing or when the plugin does not fit. This instrument uses
most of the small plugin RAM pool; the exact current allocation is reported in
`zaaggenz.json` beside the image. Other stateful plugins may need to be removed.

Choose **ZaaggenZ** with Fune's ALGORITHM control. On SOUND, the first four knobs
are CHARACTER, GRIT, MOTION and VOWEL. Press SELECT to reach the next row, then
choose a sound with PRST. The image contains all 36 sounds; no user preset slots
are needed merely to use them.

If the FM-1 currently runs stock firmware, use
[Fune's own installation instructions](https://github.com/hugelton/Fune/blob/dc1eb1b15a6bc108e09098e554ea8b0a7dbe8a1b/docs/FM1_INSTALL.md)
to establish a supported Fune installation first. Fune requires M-VAVE's stock
bootloader and refuses modified bootloaders. The pinned upstream README reports
earlier flash-chip units tested, newer chips expected but untested. ZaaggenZ does
not supply a bootloader modification or claim compatibility with Felucca images.

## Performance controls

All bipolar controls are **offsets from the selected factory recipe**. Zero
restores its intended balance. A full negative layer offset mutes that role;
positive values add level only up to the explicit 127 limit. Changes to PRST keep
the offsets, so the four primary knobs continue to express your performance.

| Page / row | Knob 1 | Knob 2 | Knob 3 | Knob 4 |
| --- | --- | --- | --- | --- |
| SOUND 1 | CHAR: recipe family or override | GRIT offset | MOTN offset | VOWL offset |
| SOUND 2 | PRST: 36 factory recipes | MODE: native / source image / hybrid | IMAG: captured source | TUNE: 12, 19, 24 EDO or 13-step tritave |
| EDIT 1 | LINE balance | EXCT balance | BODY balance | AUX balance |
| EDIT 2 | SUB balance | ROLL subdivision | RATE in BPM | SUBM: moving root or C pedal |
| EDIT 3 | SEPR spectral separation | GAIN final gain | TAIL: finite image or sustain loop | GLID offset |

Fune's ENV1 is the amplitude envelope. Its LFO/ENV FLT destination moves VOWEL;
SHP moves GRIT. Pitch modulation, pitch bend, MIDI, host sequencing, step rolls,
arpeggiation, transport, effects and saving remain Fune workflows. Generic plugin
voices do not receive the built-in ANALOG engine's glide, so GLID is implemented
in the shared ZaaggenZ engine.

RATE controls the instrument's internal motion/roll clock. The current plugin API
does **not expose host tempo or transport phase**, so internal motion is free
running and is not claimed to be clock-locked to Fune. Set RATE to the song BPM
for approximate matching; use Fune's sequencer rolls for transport-locked note
retriggering. RATE choices are encoded in 0–127 because Fune's song format stores
plugin values as signed bytes. Its displayed choices span 40–300 BPM; `Preset`
uses each recipe's own BPM.

Source-image pitch changes resample an already nonlinear source. Native mode
changes the procedural source frequency before its nonlinear stage. They have
deliberately different sounds. Tuning keys are consecutive degrees anchored at
MIDI key 48; 13-step tritave tuning repeats at a factor of three, not an octave.

## Factory files with envelopes

`build/fune-presets/` contains 36 `.fune` files written by the actual upstream
Fune serializer. Each carries the PRST selection, zeroed offsets and a translated
ENV1 envelope, and is checked by saving then loading it through the real host.
Host decay and release remain Fune's exponential curves; these files translate
the standalone recipe's timing without claiming sample-identical envelope shape.
The manifest records full names, checksums and translated controls. Fune may
truncate the displayed saved-preset name; the file and PRST names remain complete.

There are 24 user preset slots on the FM-1. Import whichever sounds you want in
those slots; all 36 remain reachable from PRST. An explicit `push` overwrites the
chosen slot, so first read its contents if it contains a sound you want to keep:

```sh
python tools/fune_slots.py --port COM4 list
python tools/fune_slots.py --port COM4 pull preset 1 my-preset-backup.fune
python tools/fune_slots.py check build/fune-presets/01-elastic-bloom.fune
python tools/fune_slots.py --port COM4 push build/fune-presets/01-elastic-bloom.fune 1
```

Open the slot with LOAD on Fune's SAVE/SOUND page. A transfer changes the stored
slot, not the sound currently being played. Existing songs retain plugin values
by the stable `zgf1` ID and parameter labels. The actual-host test verifies both
range extremes of every parameter survive serialization.

## Replace or remove the plugin

Use `list` to find ZaaggenZ's current number; numbers are device-local and may
change. To replace it, first turn it off, restart, then erase its inactive image.
The image pool is too small to assume two simultaneous copies of its state fit.

```sh
python tools/install_fune.py --port COM4 off 1
# Restart the FM-1, then check the current image numbers again.
python tools/install_fune.py --port COM4 list
python tools/install_fune.py --port COM4 erase 1
python tools/install_fune.py --port COM4 install build/fm1/zaaggenz.kpl
```

Holding **HOME while powering on** tells this Fune loader to skip plugins. Its
crash/watchdog marker also skips plugins after an unhealthy run. From that state,
list images, disable ZaaggenZ, restart and erase the disabled image. A song whose
plugin is missing retains its plugin identity/values; Fune displays NO PLUGIN and
uses its fallback sound. Reinstalling a compatible plugin restores the identity.
This describes the pinned source's recovery behavior; interrupted physical flash
writes have not been exercised by this project.

## Reproduce the target image and host qualification

```sh
# Linux x86-64, including WSL2; compiler download is checksum-pinned.
python tools/get_fune_toolchain.py
python tools/build_fune.py
python tools/build_fune.py --info build/fm1/zaaggenz.kpl

# Actual host integration and portable preset export.
python tools/test_fune_host.py
python tools/export_fune_presets.py
```

`build_fune.py` checks every vendored SDK file against `UPSTREAM.json`, compiles
the same core sources, links four address variants and requires relocation
agreement. It checks the linked image for float helpers and the known pi32v2
64-bit-multiply/16-bit-load erratum, enforces actual image RAM and flash bounds,
and writes the ELF, linker map, symbols, disassembly and a JSON resource report.
The installer/build helpers from Fune are vendored unchanged with MIT/MPL notices.

For physical evaluation, start with one track and one sustained key, then exercise
the six families, source modes, note fallback, release, parameter motion and
maximum roll density. Add tracks one at a time while observing Fune's underrun
and load diagnostics. Only a measured clean run on the actual FM-1 can establish
the final simultaneous-track and CPU budget.
