#!/usr/bin/env python3
"""Qualify the plugin in the actual pinned Fune host (no device and no UI needed)."""
from pathlib import Path
import argparse
import json
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def run(args):
    subprocess.run([str(x) for x in args], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / ".upstream/Fune")
    parser.add_argument("--build", type=Path, default=ROOT / "build/actual-fune-host")
    args = parser.parse_args()
    source = args.source.resolve()
    revision = json.loads((ROOT / "third_party/fune/UPSTREAM.json").read_text())["revision"]
    if not source.exists():
        run(["git", "clone", "https://github.com/hugelton/Fune.git", source])
        run(["git", "-C", source, "checkout", "--detach", revision])
    actual = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if actual != revision:
        raise SystemExit(f"Fune revision {actual} differs from pinned {revision}; select a matching read-only checkout.")
    run(["cmake", "-S", ROOT / "platform/fune/host_test", "-B", args.build.resolve(), "-DFUNE_SOURCE=" + str(source), "-DCMAKE_BUILD_TYPE=Release"])
    run(["cmake", "--build", args.build.resolve(), "--target", "zg_actual_host_test", "--parallel", "4"])
    run(["ctest", "--test-dir", args.build.resolve(), "-R", "^actual_fune_host$", "--output-on-failure"])


if __name__ == "__main__":
    main()
