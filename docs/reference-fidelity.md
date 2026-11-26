# Reference fidelity and the source-preserving architecture

## Source authority and exact recovery

The authority is [techrote/zaaggenz at d7e580c190d00e1d00a573a6b845c75ddddc1d4c](https://github.com/techrote/zaaggenz/tree/d7e580c190d00e1d00a573a6b845c75ddddc1d4c).
Its recovered v1.2.1 payload was extracted by its own strict verifier into the
destination's build directory. The reference checkout was not changed.

| Evidence | Verified identity |
|---|---|
| Recovered compressed runtime | `eaf5d3ab822dfa121cd3b795d62619af9e62c041fb02de19f8dbeafb0014c919` |
| Recovered regular files | 34 |
| Original owner archive, recorded by upstream | `90be319a4660a637077df935a8ebcbd1bb4c3a629e28e2f6d4bfe62d88dfe8a8` |
| Our 48 kHz `locked_bloom` float32 render | `25eabbe29b4ea4a2930acd700a32af141cfe72337b1b88edc7dee3476d09fe65` |
| Protected upstream 48 kHz anchor | The same hash, exact equality |

The source renderer's lattice is not a plain saw oscillator. It separates pitch
micro-cadence, harmonic parity and tilt, detuning collapse, critical-band AM
roughness and independently controlled noise. Its envelope precedes the nonlinear
stage. Soft saturation, asymmetric bias, an explicit hard-clip branch and wavefold
therefore interact with both the lattice and the envelope. `locked_bloom` has 49
partials, no noise or roughness, a reverse micro-sweep, and an explicit legacy
hard-branch makeup setting used as timbre. That setting is not silently applied
to the instrument's final output.

The recovered renderer itself applies its authored per-source peak policy.
The compiler preserves that rendered result; it performs no additional per-source
peak or RMS normalization. This distinction matters: six accepted character
sources have peaks above full scale after their character transforms, reaching
3.5483 in Bloom Bark. The encoded bank uses one fixed factor of 1/8 and the C
decoder restores that factor. The instrument still owns explicit layer gains and
its single final-output policy.

## Six real character mechanisms

The accepted replacement families live in
[`zaaggenz_zaag/render.py`](https://github.com/techrote/zaaggenz/blob/d7e580c190d00e1d00a573a6b845c75ddddc1d4c/zaaggenz_zaag/render.py)
and their authoring parameters and owner approval live in
[`registry.py`](https://github.com/techrote/zaaggenz/blob/d7e580c190d00e1d00a573a6b845c75ddddc1d4c/zaaggenz_zaag/registry.py).

| Family | Defining accepted mechanism |
|---|---|
| Bloom Bark | High-frequency pre-emphasis into biased saturation; a 25 ms bark bloom; a second high-frequency bite path. |
| Formant Snarl | Moving 900/3200 Hz resonances inside aggressive asymmetric saturation, plus the recipe's separate vowel motion. |
| Upper Chop | A continuous low band under a hard 1/8-beat upper gate and 310 Hz ring component. |
| Split Maul | Separately saturated spectral halves with opposing half-beat gates and moving ring coloration. |
| Crushed Teeth | Strong upper-band drive, three-sample hold, low-level-count quantization and renewed upper edge. |
| Harmonic Rip | Four short delays with opposing signs, moving ring sidebands and high-frequency bite. |

All seven compiled sources, including the separately protected `locked_bloom`,
come from these actual renderers. Native DSP is a bounded causal reinterpretation
of those mechanisms. Its filters and partial budget differ from the offline
zero-phase and 49–74-partial source renderers; it is not labeled bit-identical.

## Desktop concepts and embedded ownership

The inspected desktop modules define a larger instrument and authoring system
than this embedded engine. The following boundaries are deliberate:

| Reference mechanism | Embedded implementation and boundary |
|---|---|
| `zaaggenz_layers` and ZG-029 | SYNTHLINE and EXCITER assemble before their shared live character; BODY, AUX and SUB retain independent state and role gains. One final output stage owns the mix. |
| `zaaggenz_melody` | Complete natural source events are available, with persistent live pitch, glide and release. Image transposition uses the explicitly qualified varispeed path described below. |
| `zaaggenz_dsp` | The recipe compiler executes the inspected bounded graph subset offline. The hardware applies its own causal character and layer processing. |
| `zaaggenz_tuning` | Four explicit equal-division systems, including 13 divisions of a tritave, use the reference's degree/period concept. Arbitrary Scala keyboard maps and adaptive tuning remain desktop features. |
| `zaaggenz_harmony` | BODY/AUX degree offsets and moving or pedal SUB are playable controls. The desktop's constrained multi-voice voicing search is not executed on the FM-1. |
| `zaaggenz_gesture`, `zaaggenz_grammar`, `zaaggenz_phrase` | Live macro gestures, bounded retriggers and the host's sequencing provide performance control. The source importer explicitly rejects full phrase/arrangement contracts; it does not silently flatten those programs into a single note. |

The layer authority is
[`ZG029_RUNTIME.md`](https://github.com/techrote/zaaggenz/blob/d7e580c190d00e1d00a573a6b845c75ddddc1d4c/docs/zaaggenz/ZG029_RUNTIME.md),
and the recovered source paths are recorded in
[`V1_2_1_PATH_MAP.md`](https://github.com/techrote/zaaggenz/blob/d7e580c190d00e1d00a573a6b845c75ddddc1d4c/docs/baseline/V1_2_1_PATH_MAP.md).

## Why the default bank stores short source images

The selected format-v2 representation stores the complete authored event with a
44,100 Hz first level, a 29,400 Hz intermediate level, a 22,050 Hz level and seven
further half-rate levels: **ten levels in total**. Samples use 8-bit mu-law
companding with one 256-entry signed 16-bit decode table. The common encoding
scale is still 1/8, restored by the decoder; companding is an explicitly measured
lossy representation, not normalization or compression of the final mix.

Only seven selected recipes are stored. There is no multisample for every
keyboard key and no opaque long-form sample collection. Compressed source data
plus the decode table occupy **248,361 bytes**, with 16 bytes of playback state
per voice and no sample buffer in RAM. Metadata and decoder code add flash; the
final target linker map is authoritative. The isolated target candidate was
270,956 image bytes, 67 flash sectors and 1,792 bytes of total plugin static RAM.

This preserves approximately 291–303 ms of original attack, nonlinear evolution,
rhythmic movement and tail at the reference pitch. These are the complete default
source events used by the accepted melodic renderer, which calls
`synthesize_one(base_params)` without the optional extended-duration override.
They are not short exciter slices. The encoded waveform is not bit-identical to
the float reference, but its 44.1 kHz first level preserves substantially more
upper-band material than the initial 22.05 kHz linear-PCM experiment.

Single-cycle tiling was implemented as a comparison condition. It costs less
flash but discards the source's pitch and amplitude trajectory, ring phase,
stochastic evolution and character motion. Its waveform SNR against the full
event is poor; that phase-sensitive number is evidence of nonidentity, not a
listener preference score. Uncompressed full-rate banks and several full-source
pitch multisamples consume much more flash. The selected companded images retain
the transient and tail information that a static cycle cannot represent, while
the native engine supplies freely varying live synthesis.

### Transposition is explicitly varispeed

The C player changes read speed **after** the source's nonlinear topology. It
does not change oscillator pitch and rerun the source through that topology.
It therefore preserves the recorded nonlinear waveform, subject to interpolation
and antialias filtering. Pitch changes also change the duration of the finite
image. This is different from the desktop's static phase-vocoder-plus-resampling
path, which preserves duration. The desktop's variable glide path also uses
interpolation, then crossfades back to a duration-preserving static tail.

The playback step spans ordinary MIDI and the implemented non-octave tuning
range without the former 384 Hz upper or 6 Hz lower pitch clamps. High notes
necessarily retain few source harmonics. The exact prepared
playback step is tested against a 64-bit arithmetic oracle, but computed with
split 32-bit products on target to avoid the pi32v2 wide-multiply hazard.
The 29.4 kHz intermediate level uses exact 2/3 position scaling implemented with
bounded shifts, additions and a residual correction; no division is executed in
the sample path. This level improves the ordinary C2 source register, whose
65.406 Hz pitch is approximately 1.362633 times the 48 Hz reference root.
When glide crosses a mip boundary, a bounded 32-output-sample crossfade changes
bandwidth without immediately switching waveforms. The source cursor continues.
This short transition includes the preceding wider-band level, so steady-note
antialias measurements do not imply alias-free pitch jumps.

The last stored level is 256 times slower than the original 44.1 kHz image.
The highest supported 13ED3 keys reach the engine's 19,845 Hz limit, a
413.4375-times source ratio. Their pitch step is correct, but the bank does not
provide complete additional antialias filtering above 256 times. Adding a
conventional 512-times level would put its Nyquist limit below the source's
48 Hz fundamental. An independently tested narrower top-band filter retained
less fundamental energy and worsened numerical error, so it was not adopted.
The comparison receipt includes actual C results at both 256 and 413.4375 times;
these extreme registers remain a documented fidelity limitation.

Recorded motion also follows varispeed: an Upper Chop source's internal gates
speed up with pitch. Live native character motion and EXCITER roll scheduling
have their own host-tempo controls. The finite image player does not claim to
decouple recorded pitch, duration and motion as the desktop FFT path does.

### Held notes and release

Finite-image mode plays the complete source once. Sustain mode retains the
authored attack and then repeats a measured matching segment using a 1,024-source-
sample linear overlap. It needs no second sample buffer. A release during the
overlap completes that overlap before continuing to the remaining source tail;
disabling the overlap immediately would cause a discontinuity. This loop is a
documented real-time sustain reinterpretation of a finite desktop event.

In the measured one-second held-note probes at 48 Hz, note-off produced another
4,911–8,037 samples (111–182 ms at 44.1 kHz), depending on the current source
position. The selected segments retain the timing from the initial measured
linear-PCM bank; their coordinates and overlap were doubled for the new rate.
The instrument envelope may impose its separately selected release. Finite mode
and high-pitched notes can finish earlier because of varispeed; sustain mode is
the intended continuously held-note path.

## Measured image conformance

These measurements execute the actual portable C decoder. Reference and decoded
audio receive the same explicit 1/8 audition trim. No fitted gain, hidden
normalization or alignment optimization is used. Root-pitch measurements compare
against the authenticated source resampled to 44.1 kHz. Transposed measurements
compare against ideal polyphase varispeed, not duration-preserving FFT pitch.

| Source | Initial linear-PCM root SNR | Final mu-law root SNR | Final C2 ratio SNR | Final 1.5× SNR |
|---|---:|---:|---:|---:|
| Locked Bloom | 31.59 dB | 37.42 dB | 34.13 dB | 37.43 dB |
| Bloom Bark | 14.72 dB | 37.81 dB | 19.85 dB | 37.85 dB |
| Formant Snarl | 14.67 dB | 37.67 dB | 19.89 dB | 37.83 dB |
| Upper Chop | 16.04 dB | 37.76 dB | 21.00 dB | 37.76 dB |
| Split Maul | 23.12 dB | 37.85 dB | 28.21 dB | 37.77 dB |
| Crushed Teeth | 13.10 dB | 37.71 dB | 18.62 dB | 37.60 dB |
| Harmonic Rip | 12.77 dB | 37.77 dB | 18.03 dB | 37.77 dB |

Root correlations are approximately 0.99991. Exact mip ratios are mainly limited
by companding quantization; arbitrary ratios also incur linear-interpolation and
conservative-bandwidth error. There is no blanket 38 dB claim across the keyboard.
At 8×, the filtered player scores 37.1–38.0 dB against ideal varispeed, versus
1.9–24.2 dB for the same full-rate companded image without a filtered level.
The original 16-bit representation had much lower quantization error at exact
decimated ratios, but sacrificed upper-band fidelity at the source root. The
selected compromise restores that material and keeps the entire plugin within
the verified target region. These are numerical comparisons, not listener scores.
Crushed Teeth's authored sample-hold images remain part of its original sound.

Receipts are [codec selection](evidence/source_codec_selection.json),
[final architecture comparison](evidence/architecture_comparison.json) and
[actual decoder conformance](evidence/source_decoder_conformance.json), and
[compiler roundtrip](evidence/source_compiler_roundtrip.json). They
include source identities and implementation hashes. WAVs are generated from C
into the build directory and delivery artifacts rather than tracked in Git.

The native C probe also produces WAVs for every family. Its measured spectral
centroids differ substantially from the compiled references, as expected from a
causal 16-partial reinterpretation: in this probe Bloom Bark is about 360 Hz
versus 2,257 Hz for its image, while Formant Snarl is about 2,320 versus 2,361 Hz.
This supports retaining the source-preserving path for authentic character and
the native path for live control. It does not replace physical listening.

These decoder scores are not waveform-fidelity scores for a complete layered
factory preset. `Source image` selects the source generator; the patch can still
apply the shared live character after SYNTHLINE and EXCITER assembly. **Pure
Locked Image** and **Bare Source Lab** explicitly bypass that character, while
other presets intentionally use additional live processing. A compiled family
already contains its authored offline character. BODY/AUX/SUB, instrument
envelopes and final gain also change the resulting mix by explicit design.

## Reproduce and compile recipes

The pinned reference is required only to regenerate the bank and comparison
evidence, not to build or play the instrument. The regular instrument build uses
the checked-in, hash-verified const data.

```sh
python tools/compile_sources.py --reference .upstream/zaaggenz \
  --spec assets/source_recipes.json
cc -std=c11 -O2 -Iinclude src/source_bank.c tests/source_probe.c -o build/source_probe
cc -std=c11 -O2 -Iinclude src/dsp.c tests/dsp/dsp_probe.c -o build/native_probe
python tools/compare_architectures.py --reference .upstream/zaaggenz \
  --source-probe build/source_probe --native-probe build/native_probe
python tools/validate_source_bank.py --source-probe build/source_probe --full-midi
python -m unittest discover -s tests -p test_source_compiler.py
```

The complete compiler qualification regenerates the factory include and
manifest in an isolated directory, compiles the actual example RenderRecipe,
builds its custom C decoder with CMake, and checks its decoded byte hashes,
release behavior and keyboard-range durations:

```sh
python -m pip install -r requirements-reference.txt
python tools/qualify_source_compiler.py --reference .upstream/zaaggenz \
  --out build/source-compiler-qualification.json
```

Generated source bytes and all non-provenance manifest content must match
exactly. Only the explicitly named Python, NumPy and SciPy version fields may
differ; the receipt records both toolchains and both raw manifest hashes.
`--strict-toolchain` also requires full manifest byte identity. A clean virtual
environment installed only from the requirements file passed this command and
the actual reference-import tests; JSON Schema validation dependencies are
explicitly included.

`assets/source_recipes.json` is the explicit desktop-to-embedded source recipe
list. The compiler admits 1–8 selected inspected source IDs, verifies the pinned
reference revision and recovered payload, rejects unrecognized fields, enforces
a flash ceiling, and records source parameters, PCM hashes and numeric tool
versions. Custom source overrides require the separate `custom.legacy` identity
and are confined to the inspected legacy synthesis parameter schema. Protected
`locked_bloom` and registered family IDs cannot be silently changed. Sample rate,
root frequency and beat count remain fixed in the version-1 recipe-list schema;
the encoded bank format is version 2. Arbitrary Python,
plugin code and unknown graph operations are never loaded from a recipe.

### Import an actual desktop RenderRecipe

An actual upstream `RenderRecipe` JSON can also be compiled. The included
example is produced with upstream's `freeze_legacy()` API from the authenticated
`locked_bloom` parameters, not a hand-invented approximation of its schema.

```sh
python tools/compile_sources.py --reference .upstream/zaaggenz \
  --render-recipe examples/locked_bloom_render_recipe.json
```

This writes a separate 34,671-byte one-source bank under `build/custom-bank/`,
leaving the factory data intact. Repeat `--render-recipe` for up to eight selected
single-event recipes, subject to the same total flash ceiling. The compiler
validates the actual upstream contract, renders its genuine source, executes up
to 16 inspected graph nodes and bounded automation, and bakes its declared output
policy into the custom asset. The manifest records the original complete recipe
hash, source PCM hash, graph order and baked output policy. The live instrument's
final mix policy remains explicit; imported source output gain is authored asset
content, not automatically reapplied live.

Admission requires mono synth mode, a 48 Hz source authored at 48 kHz, a
20 ms–1 second event, one consistent tempo and zero time origin. The importer rejects
PhrasePlan, arrangement, reversebass, legacy sculpt owner domains and spectral
racks instead of silently discarding their behavior. The inspected explicit
`legacy.sculpt.v1` graph node is supported because its execution order is clear.
Continuous phase checkpoints and gated/capped tail policies also fail explicitly;
the importer preserves one natural source event. The other admitted graph nodes are identity, gain, tanh, hard clip, oversampled
tanh/hard clip and multiband gain. Live keyboard tuning belongs to the FM-1 patch;
no melodic phrase or tuning program is implicitly extracted from this source-
event importer. Very short custom sources remain finite-only when there is
insufficient material for the bounded sustain overlap.

The generated data can be selected at compile time with
`ZG_SOURCE_BANK_INCLUDE` set to its absolute path. For example, with GCC/Clang:

```sh
cc -std=c11 -O2 -Iinclude \
  -DZG_SOURCE_BANK_INCLUDE='"/absolute/path/build/custom-bank/source_bank_data.inc"' \
  src/source_bank.c tests/source_probe.c -o build/custom_source_probe
```

The same selector is exposed by CMake as `-DZG_SOURCE_BANK_FILE=/absolute/path/...`
and by the target builder as `--source-bank FILE`. The Fune adapter reads the
generated source names/count, including an eighth source when present. A custom
bank's indices and preset selections should still be authored together: factory
indices are reduced modulo the selected bank count, and compiling a bank does
not invent new musical preset names or assignments.

When packaging a custom target, pass the manifest from the same compiler run.
The packager checks its generated-bank hash against the target build receipt:

```sh
python tools/build_fune.py \
  --source-bank build/custom-bank/source_bank_data.inc \
  --work build/fm1-custom/sdk-work --output build/fm1-custom/zaaggenz.kpl
python tools/package_delivery.py --kind fm1 --build build/fm1-custom \
  --source-manifest build/custom-bank/source_bank_manifest.json \
  --out dist/custom
```

Factory presets and songs should be re-authored for custom source indices before
adding them to a custom package; the packager does not invent those assignments.

Compilation with NumPy 2.3.5 and SciPy 1.17.0 reproduced the generated bank
byte-for-byte. The observed runtime was Python 3.12.14; the upstream project
declares Python 3.13. The exact protected PCM hash matched, so this is evidence
for this source-render path, not a claim that the entire desktop application was
qualified on Python 3.12.

The C decoder tests cover exact integer-grid playback, deterministic finite and
held renders, note-off overlap continuity, finite tails, extreme valid playback
steps, exact high-note preparation and the 29.4 kHz coordinate mapping. An
independent validator checks the actual decoded C byte streams against all seven
compiler PCM hashes and checks finite-image duration across all 128 MIDI keys.
AddressSanitizer and UBSan passed locally;
LeakSanitizer was disabled because this execution host runs under ptrace. The
decoder performs no allocations.

## Licensing boundary

The reference repository supplies no general open-source license and identifies
its legacy material as `LicenseRef-Owner-Provided-Unspecified`. These generated
source-derived assets retain that scoped identification. They are not represented
as MIT-licensed upstream material. The independently written compiler, decoder
and tests follow this destination's license for new code. No recovered Python
runtime source is vendored into this destination's tracked implementation.
