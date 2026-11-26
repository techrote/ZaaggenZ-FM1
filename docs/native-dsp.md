# Native DSP mechanisms and limits

The native source and character processor live in `src/dsp.c`. This same integer
implementation is linked into the Fune plugin, the desktop instrument and its
acoustic probe. Its source is a causal embedded reinterpretation of Zaaggenz,
not a claim of byte-identical Python rendering.

## Source authority

The relevant source is `zaaggenz_zaag/render.py` and `registry.py`, plus the
authenticated `uptempo_harmony/synth.py` recovered from the checked-in payload.
The legacy payload SHA-256 is
`eaf5d3ab822dfa121cd3b795d62619af9e62c041fb02de19f8dbeafb0014c919`.
The destination provenance record pins the upstream revision.

The protected `locked_bloom` source has 49 harmonics, zero noise and roughness,
a reverse pitch micro-sweep, explicit internal hard-clip makeup and a folded
nonlinear path. The native engine is **not** labeled `locked_bloom`: use the
compiled source-image bank for that authority. Source images preserve the
completed nonlinear source before musical transposition; native pitch changes
alter oscillator input before nonlinear processing and intentionally have a
different meaning.

## Native source

A uint32 phase accumulator drives a bounded additive harmonic lattice. Up to 16
partials use family-specific spectral exponents, odd/even weights and tilt
recovered from the six family recipes. A fixed coefficient normalization is
calculated when the mathematical lookup tables are generated. There is no
render-dependent RMS or peak adjustment.

A reverse micro-cadence decays after each trigger. Higher partials share a
controlled roughness modulator; an independent deterministic noise stream adds
small stochastic residue. The amplitude contour is inside the nonlinear path,
so timbre relaxes as a held note settles. Pre-emphasis precedes asymmetric
saturation, hard-branch blending and triangle folding. A causal DC estimate
removes bias. Note-off, persistent layer envelopes and the final output policy
belong to the enclosing engine.

This compact path intentionally reduces the desktop's 55–74 independent
partials, per-partial detune and ERB-rate modulators. It substitutes bounded
causal contours for offline analysis and normalization. The hybrid bank is the
alternative when preserving the original fully rendered source is the goal.

## Six character processors

| Family | Retained defining mechanisms | Embedded changes |
|---|---|---|
| Bloom Bark | Asymmetric saturation, low/upper separation, 25 ms bloom and high-band bite | Causal one-pole splits and bounded table shaper |
| Formant Snarl | Two moving resonant bands, vowel-dependent crossfade, asymmetric nonlinear snarl | Stateful SVFs and extra high-band excitation to maintain perceptual contrast with the reduced native lattice |
| Upper Chop | Continuous low body, eight upper gates per beat, 310 Hz ring | Gates respond to MOTION; sharpness is intentional |
| Split Maul | Opposed low/high emphasis every half beat, independently saturated branches and moving ring | Causal 420 Hz split; frequency modulation integrates phase continuously |
| Crushed Teeth | High-band overdrive, low-bit quantization, sample-hold and upper re-excitation | Power-of-two fixed quantizer; bit depth and hold length follow GRIT/MOTION |
| Harmonic Rip | Four signed comb taps corresponding to 11/23/43/79 samples at 48 kHz, moving ring and saturation | Delay taps scale to the active sample rate; ring phase integrates frequency instead of the offline `f(t)*t` expression |

Formant coefficients remain stable by capping resonance frequency to 0.12 of
the sample rate. At the FM-1's 44.1 kHz build rate the entire exposed formant
range is available. The core accepts 8–48 kHz; low-rate results are explicitly
causal approximations, not the offline renderer's exact formant contract.

The fixed character-stage gain is part of the documented DSP topology, not a
runtime loudness compensation system. Character stages contain intentional
nonlinear bounding. The final master belongs only to the enclosing instrument.

## Antialias scope

The additive lattice omits partials above 0.42 of the sample rate. This does not
make subsequent nonlinear processing alias-free. Quality mode 1 locally runs
nonlinear transfer functions at the interpolated midpoint and applies a
`[1/4, 1/2, 1/4]` reconstruction across the previous endpoint, midpoint and current
endpoint. It adds no heap allocation or variable-length work.

This is local shaper oversampling, not whole-path oversampling. Sample-hold and
quantization images in Crushed Teeth are intentional. Comb/ring sidebands and
native wavefold images still require listening and spectral qualification. The
acoustic regression identifies folded harmonic bins from a coherent tone; it
does not conflate all high-frequency energy with aliasing.

## Memory and execution

At the checked host ABI:

| Structure | Bytes |
|---|---:|
| `zg_native_state` | 28 |
| `zg_character_state` | 208 |
| Block-local `zg_dsp_coeffs` | 48 |

The 80-sample int16 comb buffer is the largest persistent component. Oscillator
and filter state continues across blocks; a character retrigger resets the
bloom but keeps filter/comb history. A native trigger can retain oscillator
phase and noise state. Family changes are smoothed by the enclosing engine. VOWEL shifts spectral
split/edge color in every family and additionally moves the explicit Snarl
resonances. Native attack time scales with the sample rate.

Sample callbacks have no heap allocation, libc call, floating-point arithmetic,
variable division or unbounded loop. All products are bounded signed 32-bit
operations. Coefficients and phase ratios use control-rate division. Lookup
constants are generated reproducibly by `tools/gen_dsp_tables.py --check`.
Actual target linker maps and instruction scanning remain the authority for
pi32v2 resource claims; desktop timing alone is not a hardware real-time pass.

## Qualification

`tests/dsp/test_dsp.c` exercises deterministic repeat rendering, six-family
separation, high/low sample rates, extreme valid macros, hostile external sample
values, both shaper modes, phase retention and causal-history retention.
AddressSanitizer and UndefinedBehaviorSanitizer report no arithmetic or memory
errors on those cases. Leak detection is irrelevant to the allocation-free core;
the container's ptrace runtime requires `ASAN_OPTIONS=detect_leaks=0` locally.

`tests/dsp/dsp_probe.c` writes samples from the actual C implementation.
`tests/dsp/test_acoustics.py` checks them with FFT and correlation measurements.
A near-identical Bark/Snarl pair was found during development (correlation 0.983)
and corrected by restoring strong high-band excitation and resonant contrast.
The current probe's maximum cross-family correlation is 0.903; energy-weighted
centroids span approximately 263–2234 Hz at a 48 Hz note. A coherent 3250.49 Hz
Bark shaper probe reduces identified folded-harmonic energy from approximately
−15.83 dB to −21.25 dB in local 2x mode. These figures establish measured
mechanism differentiation and one alias improvement, not owner preference or
physical hardware qualification.

The integrated `tests/test_auditions.py` qualification renders all 36 factory
presets through the full engine, then checks audible output, clipping, complete
release endpoints and bank-wide spectral differentiation. The tested lower-
register riff pack produced zero clipped frames, spectral centroids spanning
approximately 94–3823 Hz and a maximum final-50-ms RMS of 0.000218. The renderer
appends complete bounded release tails; this test caught and corrected an earlier
0.5-second export tail that truncated long BODY/AUX releases.

`tests/dsp/engine_pitch_probe.c` and `test_pitch.py` independently measure the
actual rendered BODY/AUX/SUB sine layers in 12-, 19- and 24-tone equal octave
tuning and 13 equal divisions of a tritave. All 12 checked pitches are within
0.029 cents of the declared target; all four fixed-pedal invariants pass. This
is a clean-layer tuning measurement, not an assertion that rough nonlinear
source timbres admit an unambiguous fundamental estimate.
