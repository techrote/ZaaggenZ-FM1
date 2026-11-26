#!/usr/bin/env python3
"""Record read-only Fune timing diagnostics during a physical FM-1 performance.

Start a repeating pattern on the device, then run with an explicit serial port.
Only the pinned console's `stat` query is sent. No notes, transport, presets,
flash or diagnostic counters are changed. The result is an observation report,
never a claim of complete hardware/audio qualification.
"""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
CONSOLE = ROOT / "third_party/fune/platforms/fm1hw"
REQUIRED = ("up_ms", "audio_max_us", "audio_halves", "audio_over", "audio_on",
            "audio_peak", "audio_same_half", "audio_offs", "vcap", "playing")
HALF_FRAMES = 256
PHYSICAL_RATE = 44117
GUARD_US = (HALF_FRAMES * 1_000_000 // PHYSICAL_RATE) * 90 // 100


def parse_stat(reply: str) -> dict:
    if not reply.startswith("stat "):
        raise ValueError("No valid Fune stat response")
    values = {}
    for key, token in re.findall(r"\b([A-Za-z_][A-Za-z0-9_]*)=([^\s]+)", reply):
        try:
            values[key] = float(token) if "." in token else int(token, 10)
        except ValueError:
            values[key] = token
    missing = [key for key in REQUIRED if not isinstance(values.get(key), int)]
    if missing:
        raise ValueError("Unsupported or incomplete stat fields: " + ", ".join(missing))
    return values


def assess(rows: list[dict], errors: list[str]) -> dict:
    reasons = list(errors)
    stats = [row["stat"] for row in rows]
    if len(stats) < 2:
        reasons.append("Fewer than two complete observations")
    if stats:
        if any(b["up_ms"] < a["up_ms"] for a, b in zip(stats, stats[1:])):
            reasons.append("Device uptime decreased: reset or counter wrap")
        if stats[-1]["audio_halves"] <= stats[0]["audio_halves"]:
            reasons.append("Audio half-buffer counter did not advance")
        if any(s["audio_on"] != 1 for s in stats):
            reasons.append("Audio rendering was disabled in an observation")
        if any(s["playing"] != 1 for s in stats):
            reasons.append("Transport was not continuously playing the observation workload")
        if any(s["vcap"] < 6 for s in stats):
            reasons.append("Host voice cap was reduced in an observation")
        if any(s["audio_over"] > 0 for s in stats):
            reasons.append("Host reported an over-budget run")
        if any(s["audio_offs"] > stats[0]["audio_offs"] for s in stats):
            reasons.append("Host load guard stopped audio during the window")
        if any(s["audio_same_half"] > stats[0]["audio_same_half"] for s in stats):
            reasons.append("A DMA half repeated during the window")
        if max(s["audio_max_us"] for s in stats) >= GUARD_US:
            reasons.append("Cumulative worst interrupt has no margin below the host 90-percent guard")
        if max(s["audio_peak"] for s in stats) == 0:
            reasons.append("No nonzero output peak was reported")
    return {
        "status": "no_failures_observed_in_timing_window" if not reasons else "incomplete_or_failed_timing_window",
        "reasons": reasons,
        "observations": len(rows),
        "half_frames": HALF_FRAMES,
        "physical_sample_rate": PHYSICAL_RATE,
        "half_period_us": HALF_FRAMES * 1_000_000 / PHYSICAL_RATE,
        "host_guard_budget_us": GUARD_US,
        "cumulative_worst_interrupt_us": max((s["audio_max_us"] for s in stats), default=None),
        "cumulative_peak_note": "The stat maxima include activity before this window; counters are not reset.",
        "hardware_qualified": False,
        "remaining": ["Audible output and recorded signal evaluation", "Hardware stack high-water", "Installed-image identity verification", "Other patches, controls and simultaneous-track workloads"],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Explicit serial device, for example COM4 or /dev/ttyACM0")
    parser.add_argument("--seconds", type=float, default=60)
    parser.add_argument("--interval", type=float, default=1)
    parser.add_argument("--label", default="manually-selected-workload")
    parser.add_argument("--artifact", type=Path, help="Record the intended .kpl hash; does not verify installed identity")
    parser.add_argument("--output", type=Path, default=Path("fm1-timing-observation.json"))
    parser.add_argument("--analyze", type=Path, help="Reassess a saved report without connecting to a device")
    args = parser.parse_args()
    if args.analyze:
        report = json.loads(args.analyze.read_text())
        result = assess(report["observations"], report.get("errors", []))
        print(json.dumps(result, indent=2))
        return 0 if not result["reasons"] else 1
    if not args.port:
        parser.error("--port is required; no device is selected automatically")
    if not 0.5 <= args.seconds <= 3600 or not 0.2 <= args.interval <= 10:
        parser.error("--seconds must be 0.5..3600 and --interval 0.2..10")
    artifact_hash = hashlib.sha256(args.artifact.read_bytes()).hexdigest() if args.artifact else None
    from fune_transport import ask, configure_pinned_console
    open_console = configure_pinned_console().open_console
    report = {
        "started_utc": datetime.now(timezone.utc).isoformat(),
        "label": args.label, "port": args.port,
        "requested_seconds": args.seconds, "interval_seconds": args.interval,
        "intended_artifact_sha256": artifact_hash,
        "installed_image_hash_verified": False,
        "device_firmware_revision_verified": False,
        "commands_sent": ["stat"], "observations": [], "errors": [],
    }
    started = time.monotonic()
    try:
        with open_console(args.port) as serial:
            while True:
                reply = ask(serial, "stat")
                elapsed = time.monotonic() - started
                try:
                    values = parse_stat(reply)
                    report["observations"].append({"elapsed_seconds": elapsed, "stat": values, "raw": reply})
                    print(f"{elapsed:6.1f}s  worst {values['audio_max_us']} us  cap {values['vcap']}  audio {values['audio_on']}  playing {values['playing']}", flush=True)
                except ValueError as exc:
                    report["errors"].append(f"{elapsed:.3f}s: {exc}; response {reply!r}")
                if elapsed >= args.seconds:
                    break
                time.sleep(min(args.interval, args.seconds - elapsed))
    except KeyboardInterrupt:
        report["errors"].append("Observation interrupted by user")
    except OSError as exc:
        report["errors"].append("Serial observation failed: " + str(exc))
    report["elapsed_seconds"] = time.monotonic() - started
    report["assessment"] = assess(report["observations"], report["errors"])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["assessment"], indent=2))
    print("Saved physical observation:", args.output)
    return 0 if not report["assessment"]["reasons"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
