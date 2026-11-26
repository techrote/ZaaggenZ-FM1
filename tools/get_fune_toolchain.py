#!/usr/bin/env python3
"""Fetch the hash-pinned official JieLi Linux compiler used for the FM-1 image.

The archive is not redistributed by this project. Linux x86-64 runs it directly;
other hosts can build through the supplied CI workflow or an x86-64 Linux VM.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import tarfile
import urllib.request

VERSION = "20260730.1"
NAME = "jieli-linux-toolchains-" + VERSION
URL = "https://jl-update.oss-cn-shenzhen.aliyuncs.com/" + NAME + ".tar.xz"
SHA256 = "a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd"
ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", type=Path, default=ROOT / ".toolchains")
    args = parser.parse_args()
    dest = args.destination.resolve()
    dest.mkdir(parents=True, exist_ok=True)
    archive = dest / (NAME + ".tar.xz")
    if not archive.exists():
        part = archive.with_suffix(".partial")
        print("Downloading", URL, flush=True)
        urllib.request.urlretrieve(URL, part)
        if hashlib.sha256(part.read_bytes()).hexdigest() != SHA256:
            part.unlink()
            raise SystemExit("Toolchain checksum mismatch; nothing was extracted.")
        part.replace(archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != SHA256:
        raise SystemExit("Existing archive checksum mismatch; nothing was extracted.")
    # A hash check authenticates the expected archive; still reject path traversal.
    with tarfile.open(archive, "r:xz") as tar:
        for member in tar.getmembers():
            path = (dest / member.name).resolve()
            if dest != path and dest not in path.parents:
                raise SystemExit("Unsafe path in toolchain archive")
        tar.extractall(dest, filter="data")
    print(dest / NAME)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
