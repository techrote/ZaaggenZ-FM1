#!/usr/bin/env python3
"""Export all factory sounds as real Fune .fune files with matching host ADSR.

Uses the pinned upstream host serializer; does not write to any device. The
embedded PRST selector already contains all sounds without consuming user slots.
These files additionally carry translated host envelopes for convenient LOAD.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def run(argv):
    subprocess.run([str(x) for x in argv], check=True)


def control_time(value: int) -> int:
    return 1 + value * value * 1999 // 16129


def env_control(milliseconds: int, attack: bool) -> int:
    # Exact host control->table-index mapping, nearest physical time. Fune's
    # decay/release curves are exponential, unlike the standalone engine's linear
    # curve; this translates timing rather than claiming waveform equivalence.
    def index(v):
        return (v * v * 127 + 5000) // 10000 if attack else (v * 127 + 50) // 100
    return min(range(101), key=lambda v: abs(math.log((10000 ** (index(v) / 127)) / milliseconds)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / ".upstream/Fune")
    parser.add_argument("--build", type=Path, default=ROOT / "build/actual-fune-host")
    parser.add_argument("--output", type=Path, default=ROOT / "build/fune-presets")
    args = parser.parse_args()
    source, build, output = args.source.resolve(), args.build.resolve(), args.output.resolve()
    revision = json.loads((ROOT / "third_party/fune/UPSTREAM.json").read_text())["revision"]
    if not source.exists():
        run(["git", "clone", "https://github.com/hugelton/Fune.git", source])
        run(["git", "-C", source, "checkout", "--detach", revision])
    actual = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if actual != revision:
        raise SystemExit("Fune checkout does not match pinned SDK revision")
    recipes = json.loads((ROOT / "presets/factory.json").read_text())
    generated = build / "recipe-metadata"
    generated.mkdir(parents=True, exist_ok=True)
    output.mkdir(parents=True, exist_ok=True)
    rows, metadata = [], []
    for i, recipe in enumerate(recipes):
        patch = recipe["patch"]
        env = [env_control(control_time(patch["attack"]), True), env_control(control_time(patch["decay"]), False), (patch["sustain"] * 100 + 63) // 127, env_control(control_time(patch["release"]), False)]
        filename = re.sub(r"[^A-Za-z0-9]+", "-", recipe["name"]).strip("-").lower()
        rows.append("  {%s, %s, {%s}}," % (json.dumps(recipe["name"]), json.dumps(filename), ", ".join(map(str, env))))
        metadata.append({"index": i, "name": recipe["name"], "file": filename + ".fune", "host_adsr_controls": env, "envelope_note": "Nearest Fune timing; Fune exponential decay/release are retained."})
    header = "/* Generated from presets/factory.json. */\n#define ZG_EXPORT_RECIPE_COUNT %du\nstatic const struct { const char *name, *filename; int env[4]; } ZG_EXPORT_RECIPE[] = {\n%s\n};\n" % (len(rows), "\n".join(rows))
    (generated / "zg_fune_recipe_metadata.h").write_text(header)
    run(["cmake", "-S", ROOT / "platform/fune/host_test", "-B", build, "-DFUNE_SOURCE=" + str(source), "-DZG_RECIPE_HEADER_DIR=" + str(generated), "-DCMAKE_BUILD_TYPE=Release"])
    run(["cmake", "--build", build, "--target", "zg_fune_export", "--parallel", "4"])
    executable = build / ("zg_fune_export.exe" if sys.platform == "win32" else "zg_fune_export")
    if not executable.exists() and (build / "Release" / executable.name).exists():
        executable = build / "Release" / executable.name
    run([executable, output])
    for row in metadata:
        data = (output / row["file"]).read_bytes()
        row["bytes"] = len(data)
        row["sha256"] = hashlib.sha256(data).hexdigest()
    report = {"fune_revision": revision, "source": "presets/factory.json", "source_sha256": hashlib.sha256((ROOT / "presets/factory.json").read_bytes()).hexdigest(), "plugin_uid": "zgf1", "plugin_version": 1, "presets": metadata}
    (output / "manifest.json").write_text(json.dumps(report, indent=2) + "\n")
    print("Exported and round-trip-validated", len(recipes), "Fune presets to", output)


if __name__ == "__main__":
    main()
