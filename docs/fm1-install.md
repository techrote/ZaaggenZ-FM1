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

This is also the exact commit tagged **Fune v0.0.2-alpha**, released on
19 November 2026. Its compatible FM-1 firmware is publicly available; compiling
an unreleased host is not an installation prerequisite. Use the
[official v0.0.2-alpha release](https://github.com/hugelton/Fune/releases/tag/v0.0.2-alpha)
and its [FM-1 firmware image](https://github.com/hugelton/Fune/releases/download/v0.0.2-alpha/fune-fm1.fwsc),
or the [official Fune web installer](https://hugelton.com/fune/install/).
The release describes that download as the same image used by the web installer.
For this qualified software baseline, verify the selected version rather than
assuming a future web-installer update is identical.

The downloaded `fune-fm1.fwsc` is **612,223 bytes**, with SHA-256:

```text
12bd8bc3610694b91943ae6f6f1220495cb11179fd6405553f2196c479d530db
```

An independent, unchanged build of that Fune source also passed its target gates.
Its linker reserves the expected 2 KiB plugin RAM pool and keeps the application,
plugin flash, update staging and save regions separate. The published image's
plugin validation/loading routines match the rebuilt machine code. The
[firmware prerequisite receipt](evidence/fune_firmware_prerequisite.json) records
the hashes and memory bounds. This verifies the available host prerequisite;
physical installation and real-time plugin performance remain unqualified.

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

The installation commands below run from the root of the extracted FM-1 delivery
bundle, beside `zaaggenz.kpl` and its `tools/` directory. When working directly in
the source repository, use `build/fm1/zaaggenz.kpl`, `build/fune-presets/` and
`build/fune-songs/` for the corresponding build outputs.

```sh
python -m pip install pyserial
python tools/install_fune.py check zaaggenz.kpl
python tools/install_fune.py --port COM4 list
python tools/install_fune.py --port COM4 install zaaggenz.kpl
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
are CHARACTER, GRIT, MOTION and VOWEL. Turn SELECT to reach the next row, then
choose a sound with PRST. The image contains all 36 sounds; no user preset slots
are needed merely to use them.

If the FM-1 currently runs stock firmware, install the compatible Fune release
above before using these plugin commands. Fune's release recommends its web
installer in Chrome or Edge over USB MIDI. Its current release requirements are
**M-VAVE's official bootloader**, with testing reported only on earlier-flash
units; it says it is not compatible with Felucca or derived custom firmware.
Read those release requirements before following the older detailed
[upstream installation document](https://github.com/hugelton/Fune/blob/dc1eb1b15a6bc108e09098e554ea8b0a7dbe8a1b/docs/FM1_INSTALL.md),
which retains some superseded prerelease wording. Establish the supported Fune
baseline first. ZaaggenZ supplies no bootloader modification or custom-firmware
migration procedure.

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
| EDIT 2 | SUB balance | ROLL subdivision | RATE in BPM | SUBM: half pitch or fixed pedal |
| EDIT 3 | SEPR spectral separation | GAIN final gain | TAIL: finite image or sustain loop | GLID offset |

Pure Locked Image and Bare Source Lab bypass the live character stage so that
their compiled source can be heard directly. Selecting an explicit CHAR value
enables that stage, including on those two recipes. Return CHAR to `Preset` to
restore the recipe's bypass decision.

Fune's ENV1 is the amplitude envelope. Its LFO/ENV FLT destination moves VOWEL;
SHP moves GRIT. Pitch modulation, pitch bend, MIDI, host sequencing, step rolls,
arpeggiation, transport, effects and saving remain Fune workflows. Generic plugin
voices do not receive the built-in ANALOG engine's glide, so GLID is implemented
in the shared ZaaggenZ engine.

Fune's default MIDI mapping assigns CC16–19 to CHARACTER, GRIT, MOTION and
VOWEL, CC7 to the selected track's mixer level and CC10 to its pan. MIDI pitch
bend follows Fune's two-semitone range. The fixed SUB pedal uses tuning degree
24; that is C1 in 12 EDO. The moving SUB follows half the SYNTHLINE frequency,
including when the keyboard uses a non-octave tuning.

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
python tools/fune_slots.py check fune-presets/01-elastic-bloom.fune
python tools/fune_slots.py --port COM4 push fune-presets/01-elastic-bloom.fune 1
```

To open an imported preset, tap HOME as needed until **MIXER** is shown, then
SAVE to open song slots, then SAVE again to select SOUND slots. Turn SELECT to
the chosen slot, press OCT+ for its menu, choose **Load** with SELECT, then press
OCT+ to execute. Starting from MIXER
avoids the synth page's separate quick-save action. A transfer changes the stored
slot, not the sound currently being played. Existing songs retain plugin values
by the stable `zgf1` ID and parameter labels. The actual-host test verifies both
range extremes of every parameter survive serialization.

## Six playable demonstration songs

`build/fune-songs/` contains six genuine Fune `.fune` songs, plus stereo WAVs
rendered after loading those same files through the pinned Fune host. Each song
has four editable patterns per active track and an eight-scene arrangement.
The first song uses two source-image tracks; the later songs introduce more
layers and native/hybrid synthesis. Four-track arrangements are useful hardware
evaluation material, with their simultaneous real-time cost still unmeasured.

| File | BPM | Active tracks | Musical focus |
| --- | ---: | ---: | --- |
| `01-image-launch.fune` | 192 | 2 | Pure Locked Image hook and low Bloom foundation; immediate starting point |
| `02-vowel-relay.fune` | 200 | 3 | Formant Snarl call-and-response, copper pedal and offbeat syllables |
| `03-roofline-bounce.fune` | 210 | 3 | Upper Chop bounce, half-time pedal and sparse Harmonic Rip replies |
| `04-contrary-gears.fune` | 186 | 4 | Split Maul hybrid lead, native bass and complementary rhythmic parts |
| `05-pixel-choir.fune` | 184 | 3 | Crushed Teeth in 24 EDO, quartertone passing degrees and pedal support |
| `06-tritave-wire.fune` | 174 | 4 | Harmonic Rip in 13 divisions of a tritave, glide and non-octave harmony |

Use one of the six song slots. As with presets, an explicit push replaces the
chosen stored slot; back it up first when needed:

```sh
python tools/fune_slots.py --port COM4 pull song 1 my-song-backup.fune
python tools/fune_slots.py check fune-songs/01-image-launch.fune
python tools/fune_slots.py --port COM4 push fune-songs/01-image-launch.fune 1
```

On the device, tap HOME as needed until **MIXER** is shown, then press SAVE to
open SONG slots. Turn SELECT to the
chosen slot, press OCT+, choose **Load** with SELECT, then press OCT+ to load it.
Open **SONG** with GLO, select the first scene, and press **PLAY** to play its
eight-scene arrangement. PLAY from a pattern page loops the
selected patterns instead. Loading deliberately leaves the transport stopped.
The compact display names fit Fune's ten-character name field; the filenames and
manifest preserve their full titles.

The exporter verifies each file against Fune's real 3,584-byte song buffer,
checks its CRC and every saved note, scene, instrument parameter and envelope,
then renders it twice in fresh processes and requires identical audio. The WAVs
use MASTER 100, visible track faders and no normalization. Their measured peaks
and limiter state appear in `manifest.json`; the authored arrangements are
required to leave Fune's master limiter inactive and finish with no sounding
voices. The device's physical MASTER setting remains the user's own setting and
is not stored in the song. Fune retains its own mixer, master DC blocker and
limiter after the plugin's explicit final-output stage.

## Replace or remove the plugin

Use `list` to find ZaaggenZ's current number; numbers are device-local and may
change. To replace it, first turn it off, restart, then erase its inactive image.
The image pool is too small to assume two simultaneous copies of its state fit.

```sh
python tools/install_fune.py --port COM4 off 1
# Restart the FM-1, then check the current image numbers again.
python tools/install_fune.py --port COM4 list
python tools/install_fune.py --port COM4 erase 1
python tools/install_fune.py --port COM4 install zaaggenz.kpl
```

Holding **HOME while powering on** tells this Fune loader to skip plugins. Its
crash/watchdog marker also skips plugins after an unhealthy run. From that state,
list images, disable ZaaggenZ, restart and erase the disabled image. A song whose
plugin is missing retains its plugin identity/values; Fune displays NO PLUGIN and
uses its fallback sound. Reinstalling a compatible plugin restores the identity.
This describes the pinned source's recovery behavior; interrupted physical flash
writes have not been exercised by this project.

## Reproduce the target image and host qualification

Run these commands from the source checkout.

```sh
# Linux x86-64, including WSL2; compiler download is checksum-pinned.
python tools/get_fune_toolchain.py
python tools/build_fune.py
python tools/build_fune.py --info build/fm1/zaaggenz.kpl

# Actual host integration and portable preset export.
python tools/test_fune_host.py
python tools/export_fune_presets.py
python tools/export_fune_songs.py
```

`build_fune.py` checks every vendored SDK file against `UPSTREAM.json`, compiles
the same core sources, links four address variants and requires relocation
agreement. It checks the linked image for float helpers and the known pi32v2
64-bit-multiply/16-bit-load erratum, enforces actual image RAM and flash bounds,
and writes the ELF, linker map, symbols, disassembly and a JSON resource report.
The disassembly check also requires that the actual sample loop and all its
reachable helpers contain no divide instruction. Its stack report adds generated
register saves and stack allocations along the longest direct callback path;
this is a conservative static estimate that excludes the host caller and interrupt
context, not a measured hardware stack high-water. The source files are hashed
before and after compilation so a concurrent edit cannot silently produce a
manifest for a different source state.
The installer/build helpers from Fune are vendored unchanged with MIT/MPL notices.

To compile an admitted desktop RenderRecipe into an alternate plugin source bank:

```sh
python tools/compile_sources.py --reference .upstream/zaaggenz \
  --render-recipe examples/locked_bloom_render_recipe.json
python tools/build_fune.py \
  --source-bank build/custom-bank/source_bank_data.inc \
  --output build/fm1-custom/zaaggenz.kpl --work build/fm1-custom/sdk-work
```

The target manifest records the custom bank's SHA-256 alongside the exact core
sources. IMAG uses that bank's generated source names and exact asset count,
including an eighth source when present. This replaces the embedded source bank;
factory preset source indices then refer to the replacement assets, with
out-of-range indices wrapped into the new bank's range. It is an experimental
sound-bank build, so the factory audio comparisons
do not qualify its changed sounds. Keep the factory plugin and custom plugin in
separate output directories. The same RAM, relocation, CRC, float, erratum,
per-sample division and flash checks apply to both.

For physical evaluation, start with one track and one sustained key, then exercise
the six families, source modes, note fallback, release, parameter motion and
maximum roll density. Add tracks one at a time while observing Fune's underrun
and load diagnostics. Only a measured clean run on the actual FM-1 can establish
the final simultaneous-track and CPU budget.

The supplied read-only recorder can capture that timing evidence while you play
or loop a pattern. Start a repeating pattern first, keep it playing throughout the
window, then run from the extracted bundle:

```sh
python tools/observe_fm1.py --port COM4 --seconds 60 \
  --label image-launch-two-tracks --artifact zaaggenz.kpl \
  --output fm1-image-launch-timing.json
```

It sends only `stat`, preserves every raw reply, and reports host load-guard
events, reduced voice caps, repeated DMA halves, stalled counters and timing
margin. The pinned firmware renders 256-frame DMA halves at 44,117 Hz, about
5.803 ms each, and begins reducing voice capacity beyond its 90% guard budget
of 5,221 µs. Those are source-derived deadlines, not measured plugin timings.
The reported worst interrupt is cumulative since the firmware last reset its
diagnostic counters and can include earlier activity. The recorder does not
reset them, alter playback, or write flash. A clean timing observation still
does not establish audible quality, hardware stack high-water or exact installed
binary identity; the report keeps those qualifications open.

Use the concise [physical owner evaluation](owner-evaluation.md) for the first
installation, recovery sequence and increasing one-track through four-track
workloads. It includes the required `pyserial` dependency and recording commands.
