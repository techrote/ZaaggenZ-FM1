# Architecture decision and qualification

ZaaggenZ-FM1 has three executable source paths in one portable integer engine.
The factory default uses a compiled source image followed by live character
processing. The native procedural path and the blend remain available because
they provide different musical control, rather than pretending to be identical
ways of transposing the original source.

The [GitHub Actions workflow](../.github/workflows/qualify.yml) is the acceptance
record for an exact integration commit. Its downloadable packages carry a source
revision, clean-tree flag, source-bank identity and hashes of every delivered
file. The target report additionally hashes every compilation input. A draft PR
and passing host/build tests do not constitute physical hardware qualification.

## Why source images are the default

The original source's harmonic lattice, pitch microtrajectory, amplitude contour
and nonlinear topology interact before melodic transposition. Changing the native
oscillator's pitch changes that interaction. The compiled path captures the
completed topology, then changes playback speed. This retains the authored
waveform subject to filtering, companding and interpolation; it also changes the
finite event's duration and recorded motion. It does not reproduce the desktop's
duration-preserving FFT operation.

| Path | Measured strengths | Deliberate limits |
|---|---|---|
| Native | Continuous procedural control; persistent oscillator state; distinct causal implementations of all six families. | Sixteen partials and causal filters replace a richer offline source; pitch changes occur before nonlinear synthesis. |
| Source image | Captures all seven selected reference events, including their transients and tails; lowest measured host cost; strongest numerical reference match. | Finite varispeed changes duration; matched sustain loops are an instrument extension; high-register filtering/interpolation has measured limits. |
| Blend | Combines recorded topology and procedural continuity in the same layered instrument. | Executes both source paths and has the highest measured host cost. |

The selected bank uses ten prefiltered levels: 44.1 kHz, 29.4 kHz, 22.05 kHz and
seven further half-rate levels. A shared 256-entry decoder expands 8-bit mu-law
data. The encoding uses one fixed amplitude scale, with no per-source gain
normalization. PCM data plus the decode table occupy **248,361 bytes**. Each
player uses 16 bytes of state and reads constants directly from flash.

At the 48 Hz source root, measured actual-C reconstruction SNR is approximately
**37.4–37.9 dB** across the seven sources. The initial 22.05 kHz linear-PCM bank
occupied 185,518 bytes but lost substantially more upper-band information in the
six aggressive families. A full-rate companded bank improved that tradeoff; the
additional 29.4 kHz level then improved ordinary melodic ratios without breaking
the target flash limit. At C2, approximately 1.362633 times the source root,
Locked Bloom measures **34.13 dB** and the six character sources approximately
**18.03–28.21 dB** against ideal varispeed. Phase-sensitive SNR is a conformance
measurement, not a listener preference score.

Single-cycle tiling was measured and rejected as the primary representation
because it discards source evolution and transient/tail information. A further
512-fold decimated level would remove the 48 Hz fundamental. A narrowly filtered
extra upper level also worsened actual reconstruction, so neither is shipped.
At the extreme 413.4375-fold ratio the retained decoder preserves energy but
SNR falls to **11.79–17.94 dB**; those events contain only 32–33 output samples.
This is an explicit upper-register bandwidth/aliasing limit, not a steady-tone
pitch or alias-free claim. See [source comparison evidence](evidence/architecture_comparison.json)
and [the rejected upper-level experiment](evidence/source_high_register_experiment.json).

## Processing cost and target footprint

The host timing experiment renders ten-second roll patterns through the actual
C instrument, in six families, three modes, two quality settings and five
repetitions. The following values are medians across family medians and include
renderer statistics and timing instrumentation on an AMD EPYC host with GCC 14.
They are **not FM-1 execution times**.

| Mode | Quality 0, µs per 32 frames | Quality 1, µs per 32 frames |
|---|---:|---:|
| Native | 6.378 | 6.652 |
| Source image | 3.705 | 3.925 |
| Blend | 7.128 | 7.372 |

Image mode used approximately 42% less host time than native and 48% less than
the blend in this experiment. Local shaper oversampling adds about 4.3% to the
aggregate native median. A coherent Bark probe measures a 5.42 dB reduction in
identified folded-harmonic energy with that option. It remains available; it is
not whole-path oversampling and does not remove intentional crushing images.
The [timing receipt](evidence/host_mode_timing.json) records the exact tested
input hashes; the later low-note source-floor correction does not change these
C2 test cases.

The real JieLi compiler produces the following factory target. These figures
were independently reproduced from a clean checkout of the integrated source:

| Target item | Measured result |
|---|---:|
| XIP image | 271,076 bytes |
| `.kpl` file with relocation records | 272,756 bytes |
| Flash allocation | 67 sectors / 274,432 bytes |
| Reserved plugin flash region | 292 KiB / 73 sectors |
| Static `.data` + `.bss` | 1,792 bytes |
| Remaining actual static plugin pool | 256 bytes |
| Portable core state | 412 bytes per track |
| Supported independent adapter instances | Four monophonic tracks |
| Render direct-call stack estimate | 560 bytes |
| Note/block direct-call stack estimate | 164 bytes |
| Image relocations | 420 |

The stack numbers come from generated code and a conservative direct-call walk;
they exclude host callers and interrupt overhead. They are not a measured stack
high-water result. The image's SHA-256 is:

```text
b6f42cac5e95105a9918bbd0f1f7e06e9c6ad56e11c267a2bb700d03be2b916b
```

The matching firmware prerequisite is available as the official
[Fune v0.0.2-alpha release](https://github.com/hugelton/Fune/releases/tag/v0.0.2-alpha),
whose tag points to the pinned SDK revision. Its `fune-fm1.fwsc` download was
verified against the published SHA-256. An unchanged full firmware build also
completed; its plugin loader/flash-management machine code and update loader
were compared with the official image. The entire upstream app is not claimed
byte-identical. The [firmware prerequisite receipt](evidence/fune_firmware_prerequisite.json)
records that scope, linker reservations, release identity and hashes. Use the
official firmware route documented in the installation guide; this project
distributes the separate plugin rather than a repackaged upstream firmware.

The build gates check actual linker sections, four-link relocation consistency,
CRCs, the pi32v2 multiply erratum, absence of float helpers, and divide-free
generated sample paths. Control-rate divisions remain bounded. The fixed-point
choice is also required by this integration: the pinned FM-1 host advertises no
fast FPU and refuses a plugin that requests one.

## Musical and numerical qualification

The following mechanisms are exercised by compiled C tests and WAV measurements:

| Area | Coverage and observed result |
|---|---|
| Determinism | Copied engine checkpoints continue exactly; repeated renders match. The final cross-platform job compares all 36 factory PCM hashes and four long-running instance hashes between native Linux and Windows builds. |
| Pitch | Twelve isolated clean-layer measurements stay within 0.023 cents. A separate test checks 2,560 key/rate/tuning combinations, actual source-player steps and 12,800 bend/reset boundaries. Out-of-band degrees saturate at the declared ceiling. |
| Source ownership | Raw SYNTHLINE/EXCITER enter their shared character graph; BODY/AUX/SUB are separate. Muting a role leaves persistent phase progression intact. |
| Articulation | Legato preserves a continuing SYNTHLINE while each note-on renews EXCITER. Same-key host voice reuse, note priority and held-note fallback are tested. Exhausted finite sources restart on the next articulation. |
| Sustains and tails | All 18 family/mode combinations retain the source after two seconds. Matched-loop release preserves an active overlap before exiting into the original tail. Long layer releases complete before renderer output ends. |
| Controls | Dedicated tests exercise gain smoothing, structural fades, macro extremes and repeated preset changes. GRIT/VOWEL measurably affect every family; MOTION affects the intended character or persistent roles. |
| Family distinction | Native probe maximum pairwise correlation is 0.903. Whole-instrument factory centroid range is approximately 94–4,146 Hz under the tested riff. |
| Factory bank | All 36 three-second riffs plus appended tails are audible, have zero clipped frames and end with final-50-ms RMS at most approximately 0.000307. |
| Long execution | Four actual-core instances each play 30 simulated minutes plus five seconds of release: 318,402,000 samples, 25,400 note events, all presets/modes/tunings, zero clipped samples and silent final releases. This is simulated musical time; host wall time is separately recorded. |
| Memory/arithmetic | Assertions remain enabled in Release. Separate address/undefined-behavior sanitizer builds exercise the same C tests. No heap allocation, blocking I/O or unbounded work occurs in the audio callbacks. |

The actual pinned Fune host also loads the plugin, allocates four tracks, cycles
its 36 presets, round-trips parameter extrema and missing-plugin identities,
and switches repeatedly between built-in and plugin engines. Thirty-six preset
files and six authored songs are serialized by that host. The song renders are
deterministic across fresh processes, release all voices, and leave the host
limiter inactive. Their peaks range from approximately -7.62 to -5.71 dBFS with
no post-render normalization.

## Deliverable and compiler checks

Windows is built natively with MSVC and a static C runtime. Its executable
imports only `KERNEL32`, `USER32`, `GDI32`, `COMCTL32` and `WINMM` system DLLs;
the console renderer imports only `KERNEL32`. The native application smoke
opens the real window, exercises keyboard/MIDI/control handlers, renders the
core and exports a WAV. Both executables are then checked again after ZIP
extraction into a folder containing spaces. The hosted Windows machine has no
audio device, so these checks do not claim a live device-playback or latency pass.

Each package is extracted, checked against its file hashes and exercised from
the extracted location. Every packaged WAV must appear exactly once in its local
`Listen.html` index. The FM-1 bundle runs its actual offline plugin checker and
contains its installer dependencies at the paths the pinned tools expect. The
transport override has simulated serial tests for short acknowledgements, line
preservation and the larger image's bounded erase timeout.

`tools/qualify_source_compiler.py` invokes the real compiler CLI, requires exact
factory source bytes and deterministic manifest content, then compiles a real
desktop RenderRecipe into a separate native C bank and checks its decoded
bytes and full MIDI range. Only explicitly recorded tool-version provenance
fields may differ. A custom package must carry its matching compiler manifest;
its generated-bank hash is checked against the target build and appears in the
archive name. The original reference checkout and factory bank remain unchanged.

## Physical acceptance still outstanding

The remaining boundary is a real FM-1: installation and recovery with the exact
compatible Fune firmware, audible DAC output, device/flash variants, XIP/cache
cost, worst interrupt duration, stack high-water and the maximum sustainable
simultaneous workload. Neither desktop speed nor a correct target binary proves
those results. The plugin reports no invented measured CPU cost.

Use [the owner evaluation guide](owner-evaluation.md) to progress from one image
track through four hybrid tracks and dense rolls. The read-only timing recorder
captures the pinned host's counters and raw console replies without claiming
complete qualification. Keep the integration in owner review until those
physical results and musical preferences are evaluated.
