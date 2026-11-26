# Playing ZaaggenZ-FM1

Start with **01 Elastic Bloom** around C2, play short notes, then hold one and
move GRIT and VOWEL. The source attack comes from the authenticated desktop
renderer; the continuing character, layers, glide and retriggers run in the
native C engine. This is the same substantive engine in the Windows player,
command-line renderer and FM-1 plugin.

## First five minutes on Windows

Extract the complete Windows ZIP into a writable folder. Run
`Start-Audition.cmd`; no Python or compiler is needed. The evaluator targets
64-bit Windows with WinMM audio and MIDI. It opens the default audio output;
selecting a MIDI input is optional.

1. Press **Play phrase**. Change presets while it runs to compare their
   articulation and tonal balance. This is a deterministic audition phrase,
   separate from the richer Fune songs included in the hardware package.
2. Press **Stop / panic**, then play the lower keyboard row
   `Z S X D C V G B H N J M` from C2 in the default 12-EDO tuning. The upper row
   `Q 2 W 3 E R 5 T 6 Y 7 U` starts at the shared C3 anchor. Use `[` and `]`
   to move both rows by twelve keyboard degrees.
3. Hold a low note. Sweep **VOWEL**, add **GRIT**, and bring **MOTION** up slowly.
   Each family uses these controls differently; the table below is a guide.
4. Raise **EXCITER**, choose a roll subdivision and change tempo. The longer
   SYNTHLINE continues while the short exciter articulates it. Legato notes
   preserve an active source and independently retrigger the exciter. If a
   finite source has already ended, the next note starts that source again.
5. Choose **Export current WAV** to render the current patch and note, including
   the release tail, to `ZaaggenZ-current.wav` in the working folder. The launcher
   sets that folder to the extracted application directory.

The player queues four 512-frame WinMM buffers at 44.1 kHz: about 46 ms of
queued audio, before device/driver latency. It is a practical audition utility;
it does not promise an ASIO-style latency figure. Audio output and physical MIDI
devices need evaluation on your Windows computer. The hosted Windows tests
exercise the actual window, control handlers, MIDI message logic and WAV export.

## Four performance dimensions

| Control | What changes | A useful gesture |
|---|---|---|
| CHARACTER | Selects one of six substantive nonlinear topologies. An explicit family selection enables character processing. | Keep a note held and compare the six families at moderate GRIT. |
| GRIT | Saturation/folding strength, or quantization and hold aggression in Crushed Teeth. | Increase into an accent, then relax it for the tail. |
| MOTION | Character rhythm plus complementary BODY/AUX movement. Bloom Bark uses the persistent layers for motion. | Build rhythmic contrast while retaining the low foundation. |
| VOWEL | Spectral splits and edge coloration; explicit moving resonances in Formant Snarl. | Sweep over a held or gliding phrase. |

Source selection and character selection are independent. A compiled family
already contains its original character; applying a different live character
creates a deliberate second topology. **Character bypass** removes the live
character stage while retaining source playback, layers and the declared output
gain. It does not normalize or undo the compiled source.

## Family starting points

| Family | First preset | Explore next | Mechanism to hear |
|---|---|---|---|
| Bloom Bark | 01 Elastic Bloom | 03 Native Bark; 06 Bloom Relay | Biased saturation, short bark bloom, upper bite and exciter contrast. |
| Formant Snarl | 07 Vowel Teeth | 08 Throat Walk; 11 Quartertone Snarl | Moving dual resonances; use VOWEL during a held note. |
| Upper Chop | 13 Upper Pogo | 14 Clean Floor Dirty Roof; 15 Flea Circuit | The lower band persists while the upper band gates and rings. |
| Split Maul | 19 Split Maul | 21 Contrary Gears; 23 Industrial Elastic | Opposed low/high emphasis and moving ring color. |
| Crushed Teeth | 25 Crushed Teeth | 27 Broken Ratchet; 30 Held Voltage | Sample-hold and coarse quantization above an independent bass role. |
| Harmonic Rip | 31 Harmonic Rip | 32 Wire Lattice; 34 Non-Octave Arc | Short-delay interference, moving sidebands and harmonic offsets. |

The 36 presets include sustained and percussive articulation, moderate and
strong drive, pedal and moving low roles, rolls, native/source-image/hybrid
choices and four tuning systems. Their exact authoring data is in
`presets/factory.json`; the package calls that directory `preset-recipes`.

## Layers have separate jobs

**SYNTHLINE** carries the long melodic source. **EXCITER** injects short note
and roll articulations into the same nonlinear graph. Turning down the latter
does not replace the line with repeated slices.

**BODY** supplies a sustained harmonic degree, **AUX** supplies a complementary
degree, and **SUB** supplies a separately controlled low foundation. BODY and AUX
move in opposition. Separation only reduces these two roles in response to
source energy; it does not rewrite SYNTHLINE or increase the master gain.
Pedal SUB remains at the tuning's degree 24. Moving SUB follows the source at
half its frequency. In a non-octave tuning, those are intentionally different
ways of organizing the low register.

The output uses explicit smoothed gains, DC removal and a final bounded PCM
clamp. The factory bank is qualified for zero clipping under its authored
audition patterns. Extreme combinations of maximum layer levels can reach the
clamp; reduce layer levels or GAIN when deliberately pushing beyond a preset.

## Pitch and source behavior

| Mode | Best reason to choose it | Pitch behavior |
|---|---|---|
| Source image | Preserve the completed Zaaggenz source event with the lowest measured host processing cost. | Varispeed follows the recorded nonlinear waveform; finite duration and recorded motion change with pitch. |
| Native procedural | Continuously reshape the bounded lattice and live topology. | Pitch changes the procedural source before its nonlinear stages. |
| Hybrid blend | Combine source attack/texture and ongoing procedural motion. | Both behaviors coexist; this costs more processing than image mode. |

Most presets use matched sustain loops. **02 Pure Locked Image** and
**36 Bare Source Lab** deliberately use finite source events and bypass the live
character stage. A held key in these two patches does not turn the finite source
into an unlimited sustain. Preset 02 retains a quiet SUB after the image ends;
preset 36 removes all other roles and isolates SYNTHLINE for comparison.

The four tunings are 12, 19 and 24 equal divisions of an octave, and 13 equal
divisions of a tritave. Keyboard numbers are scale degrees anchored at key 48 =
C3; they are not conventional MIDI pitches after selecting an alternative tuning.
Twelve key steps, including the `[`/`]` keyboard shift, span an octave only in
12-EDO. BODY/AUX offsets use the same degree system. Pitch bend is +/-200 cents
in the Windows evaluator. At the extreme upper register, higher requested
degrees saturate at the engine's frequency ceiling.

## Fune performance and demo songs

The FM-1 adapter exposes four independent monophonic tracks. Within a track,
the latest held note leads and release falls back to the previous held note.
Fune owns its sequencer, note/velocity routing and amplitude envelope. The plugin
owns the Zaag source, character, layers, glide and internal motion.

The hardware package includes 36 native Fune preset files and six native Fune
song files, each with four authored patterns per active track and eight scenes:

| Song | Musical focus |
|---|---|
| 01 Image Launch | Immediate source-image hook and low foundation. |
| 02 Vowel Relay | Formant call-and-response with offbeat accents. |
| 03 Roofline Bounce | Upper Chop over a pedal foundation and Harmonic Rip answers. |
| 04 Contrary Gears | Four-track split-band arrangement combining native, image and hybrid sources. |
| 05 Pixel Choir | 24-EDO passing degrees with crushed character. |
| 06 Tritave Wire | Non-octave motion and sustained harmonic degrees. |

Each song also has a WAV rendered through the actual pinned Fune sequencer,
plugin and mixer. These are useful before installing anything. The four-track
scene is also a deliberate physical load test; desktop rendering does not
establish the FM-1's real-time capacity.

On the device, match internal **RATE** to the song tempo. This Fune ABI does not
expose transport tempo to the plugin. Recorded image motion additionally follows
source varispeed; live character motion and exciter rolls follow RATE. See
[installation, control pages, song slots and physical qualification](fm1-install.md)
for exact controls and reversible plugin management.

## Repeatable comparison from the CLI

The renderer accepts zero-based preset indices. These commands render preset 01
through each source architecture with the same authored patch:

```sh
zaag-render --preset 0 --mode image --pattern riff --seconds 8 --out image.wav
zaag-render --preset 0 --mode native --pattern riff --seconds 8 --out native.wav
zaag-render --preset 0 --mode hybrid --pattern riff --seconds 8 --out hybrid.wav
zaag-render --preset 0 --pattern hold --seconds 4 --stems elastic --out elastic.wav
```

On Windows use `zaag-render.exe`. Stem export includes the raw source roles,
the processed source bus and the final premaster sum. Raw SYNTHLINE/EXCITER are
inputs to a shared nonlinear graph; summing their raw stems cannot reconstruct
that graph's output. Audition comparisons retain one fixed output policy and
are not individually loudness-normalized.
