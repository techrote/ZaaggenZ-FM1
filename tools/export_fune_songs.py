#!/usr/bin/env python3
"""Export six playable Fune songs and their actual host-rendered stereo WAVs.

Uses the pinned upstream KRSG serializer, sequencer and integer audio renderer.
Each song is saved, reloaded, checked against its notes/parameters/scenes, rendered
with MASTER=100, and required to leave the host master limiter untouched. A second
fresh process must produce the identical WAV. No connected device is accessed.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
from export_fune_presets import control_time, env_control, run

ROOT = Path(__file__).resolve().parents[1]
PARAMS = ("CHAR", "GRIT", "MOTN", "VOWL", "PRST", "MODE", "IMAG", "TUNE",
          "LINE", "EXCT", "BODY", "AUX", "SUB", "ROLL", "RATE", "SUBM",
          "SEPR", "GAIN", "TAIL", "GLID")


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def array(values) -> str:
    return "{" + ", ".join(str(value) for value in values) + "}"


def metadata(songs, presets):
    rows = []
    for song in songs:
        assert len(song["display"]) <= 10 and 40 <= song["bpm"] <= 240
        assert len(song["tracks"]) == 4 and 1 <= len(song["scenes"]) <= 16
        tracks = []
        for track in song["tracks"]:
            selected = int(track["preset"])
            assert 0 <= selected < len(presets)
            patch = presets[selected]["patch"]
            env = track.get("envelope", [env_control(control_time(patch["attack"]), True),
                        env_control(control_time(patch["decay"]), False),
                        (patch["sustain"] * 100 + 63) // 127,
                        env_control(control_time(patch["release"]), False)])
            params = [0] * 20
            params[4] = selected
            # Internal motion is free-running. Match the nearest representable
            # BPM; synchronized articulations are explicitly in the note grid.
            params[14] = min(range(1, 128), key=lambda v: abs(40 + (v - 1) * 260 // 126 - song["bpm"]))
            for key, value in track.get("overrides", {}).items():
                assert key in PARAMS
                params[PARAMS.index(key)] = int(value)
            assert all(-128 <= value <= 127 for value in params)
            assert all(0 <= value <= 100 for value in env)
            assert 0 <= track["level"] <= 100 and 0 <= track["pan"] <= 100
            notes = track["notes"]
            assert len(notes) <= 64
            occupied = set()
            for p, step, note, length, velocity in notes:
                assert 0 <= p <= 3 and 0 <= step <= 15 and 0 <= note <= 127
                assert 1 <= length <= 16 - step and 1 <= velocity <= 127
                assert (p, step, note) not in occupied
                occupied.add((p, step, note))
            tracks.append("{%d, %d, %s, %s, %d, {%s}}" %
                          (track["level"], track["pan"], array(env), array(params), len(notes),
                           ", ".join(array(note) for note in notes) or "{0}"))
        for scene in song["scenes"]:
            assert len(scene) == 4 and all(0 <= cell <= 4 for cell in scene)
        rows.append("{%s, %s, %d, %d, %d, {%s}, {%s}}" %
                    (json.dumps(song["display"]), json.dumps(song["filename"]), song["bpm"],
                     song.get("swing", 0), len(song["scenes"]),
                     ", ".join(array(scene) for scene in song["scenes"]), ",\n".join(tracks)))
    return """/* Generated from presets/fune_songs.json; do not edit. */
struct zg_export_track {
  unsigned char level, pan, env[4];
  short params[20];
  unsigned count;
  unsigned char note[64][5];
};
struct zg_export_song {
  const char *display, *filename;
  unsigned bpm, swing, scenes;
  unsigned char scene[16][4];
  struct zg_export_track track[4];
};
#define ZG_EXPORT_SONG_COUNT %du
static const struct zg_export_song ZG_EXPORT_SONG[] = {
%s
};
""" % (len(rows), ",\n".join(rows))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / ".upstream/Fune")
    parser.add_argument("--build", type=Path, default=ROOT / "build/actual-fune-host")
    parser.add_argument("--output", type=Path, default=ROOT / "build/fune-songs")
    args = parser.parse_args()
    source, build, output = args.source.resolve(), args.build.resolve(), args.output.resolve()
    revision = json.loads((ROOT / "third_party/fune/UPSTREAM.json").read_text())["revision"]
    if not source.exists():
        run(["git", "clone", "https://github.com/hugelton/Fune.git", source])
        run(["git", "-C", source, "checkout", "--detach", revision])
    actual = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if actual != revision:
        raise SystemExit("Fune checkout does not match pinned SDK revision")
    recipe_path = ROOT / "presets/fune_songs.json"
    factory_path = ROOT / "presets/factory.json"
    songs = json.loads(recipe_path.read_text())
    generated = build / "song-metadata"
    generated.mkdir(parents=True, exist_ok=True)
    output.mkdir(parents=True, exist_ok=True)
    (generated / "zg_fune_song_metadata.h").write_text(metadata(songs, json.loads(factory_path.read_text())))
    run(["cmake", "-S", ROOT / "platform/fune/host_test", "-B", build,
         "-DFUNE_SOURCE=" + str(source), "-DZG_SONG_HEADER_DIR=" + str(generated), "-DCMAKE_BUILD_TYPE=Release"])
    run(["cmake", "--build", build, "--target", "zg_fune_export_songs", "--parallel", "4"])
    executable = build / ("zg_fune_export_songs.exe" if sys.platform == "win32" else "zg_fune_export_songs")
    if not executable.exists() and (build / "Release" / executable.name).exists():
        executable = build / "Release" / executable.name
    report = {"fune_revision": revision, "plugin_uid": "zgf1", "plugin_version": 1,
              "source_sha256": {"presets/fune_songs.json": digest(recipe_path), "presets/factory.json": digest(factory_path)},
              "audio_path": "Actual pinned Fune sequencer -> shared ZaaggenZ core -> Fune mixer -> integer I2S renderer -> PCM16 WAV",
              "master": 100, "normalisation": "none", "physical_hardware_qualification": False, "songs": []}
    for i, song in enumerate(songs):
        run([executable, output, i])
        paths = {ext: output / (song["filename"] + "." + ext) for ext in ("fune", "wav")}
        hashes = {ext: digest(path) for ext, path in paths.items()}
        run([executable, output, i])
        assert hashes == {ext: digest(path) for ext, path in paths.items()}, "Fresh-process render changed"
        metrics = json.loads((output / (song["filename"] + ".metrics.json")).read_text())
        report["songs"].append({"name": song["name"], "file": paths["fune"].name,
                                "audio": paths["wav"].name, "bpm": song["bpm"],
                                "scenes": len(song["scenes"]), "description": song["description"],
                                "sha256": hashes, "deterministic": True, "metrics": metrics})
    (output / "manifest.json").write_text(json.dumps(report, indent=2) + "\n")
    print("Exported", len(songs), "real Fune songs; CRC/parameters/notes/scenes/release/headroom/determinism PASS:", output)


if __name__ == "__main__":
    main()
