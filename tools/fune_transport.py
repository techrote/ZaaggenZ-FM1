#!/usr/bin/env python3
"""Bounded line transport for the unchanged pinned Fune console utilities.

The upstream reader requests 256 bytes for short packet acknowledgements, which
can wait for the serial read timeout on every packet. Read one newline-delimited
reply instead, preserving subsequent lines for the upstream `plugs` list reader.
Large images also need an erase timeout proportional to their sector count.
Image linking, checksums, flash allocation and publication stay upstream's code.
"""
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


def response_timeout(command: str, requested: float) -> float:
    words = command.split()
    if words and words[0] == "plugput" and len(words) >= 2:
        try:
            sectors = max(0, (int(words[1], 10) + 4095) // 4096)
        except ValueError:
            return requested
        # Pinned upstream documents up to 0.4 s per erased 4 KiB sector.
        # Allow 0.5 s/sector and 5 s response margin, bounded for the full region.
        return max(requested, min(45.0, 5.0 + sectors * 0.5))
    if words and words[0] == "plugerase":
        # The command carries an image number, not its size. Use the bound for
        # the entire 292 KiB plugin region (73 sectors), with the same margin.
        return max(requested, 41.5)
    return requested


def ask(serial, command: str, timeout: float = 1.5) -> str:
    serial.write(command.encode("ascii") + b"\r")
    deadline = time.monotonic() + response_timeout(command, timeout)
    reply = bytearray()
    while time.monotonic() < deadline and len(reply) < 4096:
        # pyserial read_until returns as soon as the newline arrives. Its normal
        # short timeout also permits checking the enclosing command deadline.
        reply.extend(serial.read_until(b"\n", size=4096 - len(reply)))
        if reply.endswith(b"\n"):
            return reply.decode("ascii", "replace").strip()
    return ""  # Incomplete data must never be mistaken for a successful ack.


def configure_pinned_console():
    """Install only the transport override before runpy loads an upstream CLI."""
    sys.path.insert(0, str(ROOT / "third_party/fune/platforms/fm1hw"))
    import fm1_con
    fm1_con.ask = ask
    return fm1_con
