# Physical FM-1 evaluation

This is the owner evaluation for the supplied Fune plugin. Compilation, host
rendering and the supplied WAVs have been qualified separately. Record physical
results before treating an FM-1 configuration as qualified. The pinned firmware
and recovery details are in [FM-1 installation](fm1-install.md).

## Install and keep recovery available

Start from the compatible **Fune v0.0.2-alpha** host. The official firmware
download, exact hash, stock-bootloader requirement and flash-chip qualification
boundary are recorded in [FM-1 installation](fm1-install.md). The `.kpl` package
does not install that host firmware. The published prerequisite is available,
but it does not establish a physical ZaaggenZ plugin pass.

Run these commands from the extracted FM-1 bundle. Use Python 3 with `pyserial`
and the FM-1's actual serial port:

```sh
python -m pip install pyserial
python tools/install_fune.py check zaaggenz.kpl
python tools/install_fune.py --port COM4 list
python tools/fune_slots.py --port COM4 list
```

Save any song or preset slot that will be replaced with `fune_slots.py pull`.
Stop the transport and install with
`python tools/install_fune.py --port COM4 install zaaggenz.kpl`. Keep the USB cable
connected until the tool reports its verified completion. The wrapper allows
large-image sector erases to complete and reads each packet acknowledgement
without the pinned upstream reader's extra short-read delay. Its flash protocol,
checksums and final image publication are the unchanged upstream implementation.

Run `list` again and record the entry with UID `zgf1`, its version, bytes, RAM
and live status. The plugin's target report identifies the intended artifact;
the observer cannot prove that exact hash is installed. Start with a modest
MASTER setting and select ZaaggenZ before increasing the output level.

If the plugin prevents normal use, hold **HOME during power-on** to skip plugin
loading. List the images, turn off the `zgf1` entry, restart, list again, and erase
that inactive entry. For replacement, use this same off/restart/erase sequence
before installing the new image because the RAM pool does not hold two copies.
This uses the existing Fune loader; it does not alter the bootloader.

## Exercise the instrument in increasing workloads

| Stage | Play and manipulate | Record |
| --- | --- | --- |
| One image track | Pure Locked Image, then Elastic Bloom; low, middle and high keys, held notes, releases and glide | Clean note starts, tails, pitch movement and stable output |
| Ownership and articulation | Hold one key, add a higher key, release it, then release all; repeatedly articulate one key | Latest key wins, fallback returns, no stuck note; EXCITER renews while legato SYNTHLINE continues |
| Six families | Sweep CHAR, GRIT, MOTN and VOWL on sustained notes; repeat with native and hybrid modes | Audible differentiation and no dropout during live changes |
| Sequencing | Load Image Launch, then the other songs; test 24 EDO and 13-step tritave | Correct scene transitions, independent track sounds and transport stop |
| Four-track maximum workload | Four active hybrid tracks, long overlapping notes, dense sequencer notes, maximum GRIT/MOTION/VOWEL and thirty-second internal rolls; move the knobs while playing | Worst interrupt, voice cap, guard pauses and recorded audio; this workload has no hardware pass yet |

Use the visible mixer faders to establish the desired balance. The demonstration
WAVs have no post-render normalization and leave the host limiter inactive, but
extreme live edits can change that balance. Host ENV1 controls the final release.
The internal RATE clock is free-running; use Fune's note grid for synchronized
articulation.

## Capture timing and audio

For a sustained test, loop a pattern rather than letting an eight-scene song end.
Keep the chosen workload playing throughout the observation:

```sh
python tools/observe_fm1.py --port COM4 --seconds 60 \
  --label one-track-pure-image --artifact zaaggenz.kpl \
  --output one-track-image.json
python tools/observe_fm1.py --port COM4 --seconds 120 \
  --label four-track-hybrid-max-roll --artifact zaaggenz.kpl \
  --output four-track-hybrid.json
```

The recorder sends only `stat`. It retains raw replies and checks the pinned
firmware's 5,221 µs load-guard threshold, reduced voice caps, guard pauses,
repeated DMA halves, stopped transport and reset/stalled counters. Its worst-time
value is cumulative and may include earlier activity. A reduced cap or paused
audio is a failed observation for that workload even if playback later recovers.

Keep an audio recording from the physical output, the observation JSONs,
`zaaggenz.json`, the Fune revision, the FM-1 hardware/flash variant when known,
and the exact sounds/controls used. Note any clicks, unexpected pitch or tuning,
cut tails, frozen controls or recovery action. A clean timing window alone does
not qualify physical sound, stack high-water or every other preset combination.
