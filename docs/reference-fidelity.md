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

## Why the default bank stores short source images

The selected representation stores the complete authored event at 22,050 Hz and
seven successively prefiltered half-rate levels. Only seven selected recipes are
stored. There is no multisample for each keyboard key and no opaque long-form
sample collection. The actual PCM footprint is **185,518 bytes**, with 16 bytes of
playback state per voice and no sample buffer in RAM. Metadata and decoder code
add a small amount of flash; the final target linker map is authoritative.

This preserves approximately 291–303 ms of original attack, nonlinear evolution,
rhythmic movement and tail at the reference pitch. The highest retained root-band
content is reduced by the 22.05 kHz source rate. It is a deliberate bandwidth
tradeoff against the 292 KiB Fune plugin region, not a full-rate fidelity claim.

Single-cycle tiling was implemented as a comparison condition. It costs less
flash but discards the source's pitch and amplitude trajectory, ring phase,
stochastic evolution and character motion. Its waveform SNR against the full
event is poor; that phase-sensitive number is evidence of nonidentity, not a
listener preference score. A full 44.1 kHz bank and several full-source pitch
multisamples would consume much more flash. The selected short images retain
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

Eight mip levels cover the full MIDI range without the former 384 Hz pitch
clamp. High notes necessarily retain few source harmonics. The exact prepared
playback step is tested against a 64-bit arithmetic oracle, but computed with
split 32-bit products on target to avoid the pi32v2 wide-multiply hazard.
When glide crosses a mip boundary, a bounded 32-output-sample crossfade changes
bandwidth without immediately switching waveforms. The source cursor continues.

### Held notes and release

Finite-image mode plays the complete source once. Sustain mode retains the
authored attack and then repeats a measured matching segment using a 512-source-
sample linear overlap. It needs no second sample buffer. A release during the
overlap completes that overlap before continuing to the remaining source tail;
disabling the overlap immediately would cause a discontinuity. This loop is a
documented real-time sustain reinterpretation of a finite desktop event.

In the measured one-second held-note probes at 48 Hz, note-off produced another
4,911–8,037 samples (111–182 ms at 44.1 kHz), depending on the current source
position. The instrument envelope may impose its separately selected release.

## Measured image conformance

The first column below measures the actual C decoder at root pitch against the
authenticated source resampled to 44.1 kHz. The two forms both receive the same
explicit 1/8 audition trim. No fitted gain, hidden normalization or alignment
optimization is used. The final two columns compare 8× varispeed playback with
ideal polyphase varispeed; they show the benefit of the prefiltered mip levels.

| Source | Root-pitch waveform SNR | Root correlation | 8× with mip SNR | 8× unfiltered SNR |
|---|---:|---:|---:|---:|
| Locked Bloom | 31.59 dB | 0.99965 | 73.77 dB | 23.87 dB |
| Bloom Bark | 14.72 dB | 0.98264 | 85.58 dB | 5.75 dB |
| Formant Snarl | 14.67 dB | 0.98304 | 82.09 dB | 3.34 dB |
| Upper Chop | 16.04 dB | 0.98766 | 81.56 dB | 7.17 dB |
| Split Maul | 23.12 dB | 0.99757 | 85.40 dB | 13.18 dB |
| Crushed Teeth | 13.10 dB | 0.97552 | 81.57 dB | 5.70 dB |
| Harmonic Rip | 12.77 dB | 0.97378 | 83.04 dB | 2.47 dB |

The very high 8× scores occur because the mip compiler and ideal comparator use
the same declared polyphase filter; those scores primarily validate integer
decoding and quantization of that representation. They do not imply that the
finite images reproduce desktop duration-preserving transposition, or that
native nonlinear processing is alias-free. Root-pitch error includes deliberate
bandwidth loss and linear interpolation. Crushed Teeth's authored sample-hold
images are part of its original sound.

The native C probe also produces WAVs for every family. Its measured spectral
centroids differ substantially from the compiled references, as expected from a
causal 16-partial reinterpretation: in this probe Bloom Bark is about 399 Hz
versus 1,858 Hz for its image, while Formant Snarl is about 2,251 versus 1,970 Hz.
This supports retaining the source-preserving path for authentic character and
the native path for live control. It does not replace physical listening.

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
python -m unittest discover -s tests -p test_source_compiler.py
```

`assets/source_recipes.json` is the explicit desktop-to-embedded source recipe
list. The compiler admits 1–8 selected inspected source IDs, verifies the pinned
reference revision and recovered payload, rejects unrecognized fields, enforces
a flash ceiling, and records source parameters, PCM hashes and numeric tool
versions. Custom source overrides require the separate `custom.legacy` identity
and are confined to the inspected legacy synthesis parameter schema. Protected
`locked_bloom` and registered family IDs cannot be silently changed. Sample rate,
root frequency and beat count remain fixed in bank version 1. Arbitrary Python,
plugin code and unknown graph operations are never loaded from a recipe.

### Import an actual desktop RenderRecipe

An actual upstream `RenderRecipe` JSON can also be compiled. The included
example is produced with upstream's `freeze_legacy()` API from the authenticated
`locked_bloom` parameters, not a hand-invented approximation of its schema.

```sh
python tools/compile_sources.py --reference .upstream/zaaggenz \
  --render-recipe examples/locked_bloom_render_recipe.json
```

This writes a separate 25,570-byte one-source bank under `build/custom-bank/`,
leaving the factory data intact. Repeat `--render-recipe` for up to eight selected
single-event recipes, subject to the same total flash ceiling. The compiler
validates the actual upstream contract, renders its genuine source, executes up
to 16 inspected graph nodes and bounded automation, and bakes its declared output
policy into the custom asset. The manifest records the original complete recipe
hash, source PCM hash, graph order and baked output policy. The live instrument's
final mix policy remains explicit; imported source output gain is authored asset
content, not automatically reapplied live.

Admission requires mono synth mode, a 48 Hz source authored at 48 kHz, a
20 ms–1 second event, one tempo and zero time origin. The importer rejects
PhrasePlan, arrangement, reversebass, legacy sculpt owner domains and spectral
racks instead of silently discarding their behavior. The inspected explicit
`legacy.sculpt.v1` graph node is supported because its execution order is clear.
The other admitted graph nodes are identity, gain, tanh, hard clip, oversampled
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

The same selector applies to a custom instrument build. A custom bank's asset
indices and preset selections should be authored together; compiling a different
bank does not automatically invent new musical preset names or assignments.

Compilation with NumPy 2.3.5 and SciPy 1.17.0 reproduced the generated bank
byte-for-byte. The observed runtime was Python 3.12.14; the upstream project
declares Python 3.13. The exact protected PCM hash matched, so this is evidence
for this source-render path, not a claim that the entire desktop application was
qualified on Python 3.12.

The C decoder tests cover exact integer-grid playback, deterministic finite and
held renders, note-off overlap continuity, finite tails, extreme valid playback
steps and exact high-note preparation. AddressSanitizer and UBSan passed locally;
LeakSanitizer was disabled because this execution host runs under ptrace. The
decoder performs no allocations.

## Licensing boundary

The reference repository supplies no general open-source license and identifies
its legacy material as `LicenseRef-Owner-Provided-Unspecified`. These generated
source-derived assets retain that scoped identification. They are not represented
as MIT-licensed upstream material. The independently written compiler, decoder
and tests follow this destination's license for new code. No recovered Python
runtime source is vendored into this destination's tracked implementation.
