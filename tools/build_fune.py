#!/usr/bin/env python3
"""Build and qualify the integer pi32v2 Fune plugin with the pinned upstream builder.

Requires Python 3.11+, Linux x86-64, and the official JieLi compiler. Run
`python tools/get_fune_toolchain.py` first, or set JIELI_TOOLCHAIN explicitly.
The .kpl is for Fune; it is not a stock-firmware update image.
"""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tomllib
from analyze_fune_binary import analyze

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / "third_party/fune"
TOOLCHAIN_VERSION = "20260730.1"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_builder():
    spec = importlib.util.spec_from_file_location("zg_pinned_kpl_build", SDK / "tools/kpl_build.py")
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def verify_vendor() -> dict:
    manifest = json.loads((SDK / "UPSTREAM.json").read_text())
    for name, expected in manifest["files"].items():
        if sha(SDK / name) != expected:
            raise SystemExit("Pinned Fune SDK file differs from manifest: " + name)
    return manifest


def source_snapshot(source: Path, source_bank: Path | None) -> dict[str, str]:
    """Hash inputs before and after linking, rejecting a concurrent edit."""
    inputs = set()
    for folder in ("src", "include", "platform/fune"):
        inputs.update(path for path in (ROOT / folder).rglob("*")
                      if path.is_file() and path.suffix in (".c", ".h", ".inc"))
    inputs.update((source, SDK / "UPSTREAM.json", Path(__file__).resolve(), ROOT / "tools/analyze_fune_binary.py"))
    if source_bank:
        inputs.add(source_bank)
    return {(str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path)): sha(path)
            for path in sorted(inputs)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "build/fm1/zaaggenz.kpl")
    parser.add_argument("--work", type=Path, default=ROOT / "build/fm1/sdk-work")
    parser.add_argument("--source", type=Path, default=ROOT / "platform/fune/zaaggenz_plugin.c")
    parser.add_argument("--source-bank", type=Path,
                        help="Use a compiled source_bank_data.inc instead of the factory bank")
    parser.add_argument("--info", type=Path, help="Inspect a .kpl without compiling")
    args = parser.parse_args()
    provenance = verify_vendor()
    builder = load_builder()
    if args.info:
        builder.info(args.info)
        return 0
    if platform.system() != "Linux" or platform.machine() != "x86_64":
        raise SystemExit("Run the target builder on Linux x86-64 (CI, WSL2, or a VM). Desktop audition builds are portable.")
    tc = Path(os.environ.get("JIELI_TOOLCHAIN", ROOT / ".toolchains" / ("jieli-linux-toolchains-" + TOOLCHAIN_VERSION))).resolve()
    if not (tc / "pi32v2/bin/clang").exists():
        raise SystemExit("Missing compiler. Run python tools/get_fune_toolchain.py or set JIELI_TOOLCHAIN.")
    os.environ["JIELI_TOOLCHAIN"] = str(tc)
    os.environ["JIELI_DOCKER"] = "0"
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    # Keep the upstream source subset byte-identical. Only the build workspace has links.
    for name in ("sdk", "src", "tools"):
        link = work / name
        if not link.exists():
            link.symlink_to(SDK / name, target_is_directory=True)
    builder.ROOT = work
    with (SDK / "platforms/targets/fm1.toml").open("rb") as f:
        definitions = tomllib.load(f)["build"]["defines"]
    builder.defines = lambda: ["-D" + key + ("=" + value if value else "") for key, value in definitions.items()]
    OriginalToolchain = builder.Toolchain
    source = args.source.resolve()
    source_bank = args.source_bank.resolve() if args.source_bank else None
    if source_bank and not source_bank.is_file():
        raise SystemExit("Compiled source bank does not exist: " + str(source_bank))
    source_before = source_snapshot(source, source_bank)

    class ProjectToolchain(OriginalToolchain):
        def run(self, tool, *argv, out=None):
            if tool == "pi32v2/bin/clang":
                argv = (*argv, "-I" + str(ROOT / "include"), "-DZG_FUNE_TARGET=1", "-DZG_TARGET_PI32V2=1")
                if source_bank:
                    argv = (*argv, "-DZG_SOURCE_BANK_INCLUDE=" + json.dumps(str(source_bank)),
                            "-DZG_FUNE_CUSTOM_SOURCE_BANK=1")
            return super().run(tool, *argv, out=out)

    builder.Toolchain = ProjectToolchain
    output = args.output.resolve()
    output.with_suffix(".json").unlink(missing_ok=True)
    output.unlink(missing_ok=True)
    builder.build(source, output)
    if source_snapshot(source, source_bank) != source_before:
        output.unlink(missing_ok=True)
        raise SystemExit("Source files changed during compilation. Re-run after the edit completes.")
    raw = output.read_bytes()
    header = builder.parse(raw)
    ram = header["data_size"] + header["bss_size"]
    if ram > 2048:
        output.unlink()
        raise SystemExit(f"Actual plugin state {ram} bytes exceeds Fune's 2048-byte image RAM pool.")
    clang_hash = sha(tc / "pi32v2/bin/clang")
    known_toolchain = clang_hash == "c45189ee624de0430deb0db02e6dce171fb5538e7fe323e3964dc8ac6236240f"
    result = {
        "status": "target_compiled_not_hardware_qualified",
        "fune_revision": provenance["revision"],
        "toolchain_version": TOOLCHAIN_VERSION if known_toolchain else "external compiler; see exact hash",
        "toolchain_clang_sha256": clang_hash,
        "toolchain_archive_sha256": "a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd" if known_toolchain else None,
        "artifact": output.name,
        "artifact_sha256": sha(output),
        "file_bytes": len(raw),
        "image_bytes": header["size"],
        "flash_sectors": (header["size"] + 4095) // 4096,
        "flash_allocated_bytes": ((header["size"] + 4095) // 4096) * 4096,
        "data_bytes": header["data_size"],
        "bss_bytes": header["bss_size"],
        "ram_bytes": ram,
        "ram_pool_bytes": 2048,
        "relocations": header["nreloc"],
        "api": f"{header['api'] >> 8}.{header['api'] & 255}",
        "checks": ["upstream_hashes", "four_link_relocation_agreement", "image_crc", "header_crc", "no_float_helpers", "pi32v2_multiply_erratum_scan", "flash_region_limit", "actual_image_ram_limit"],
        "not_measured": ["physical_audio", "worst_case_block_time", "hardware_stack_high_water", "flash_xip_runtime_cost"],
        "source_sha256": source_before,
        "source_bank": {
            "kind": "custom" if source_bank else "factory",
            "file": str(source_bank) if source_bank else "src/source_bank_data.inc",
            "sha256": sha(source_bank or ROOT / "src/source_bank_data.inc"),
        },
    }
    build_dir = work / "build/fm1/plugins" / (args.source.stem + ".work")
    for srcname, dstname in (("link_a.map", "zaaggenz.map"), ("link_a.elf", "zaaggenz.elf"), (args.source.stem + ".sym", "zaaggenz.sym"), (args.source.stem + ".dis", "zaaggenz.dis")):
        shutil.copyfile(build_dir / srcname, output.parent / dstname)
    if args.source.resolve() == (ROOT / "platform/fune/zaaggenz_plugin.c").resolve():
        stack = analyze(output.parent / "zaaggenz.dis", output.parent / "zaaggenz.sym")
        (output.parent / "zaaggenz-stack.json").write_text(json.dumps(stack, indent=2) + "\n")
        result["stack_analysis"] = stack["entry_points"]
        result["sample_path_divide_instructions"] = stack["sample_path_divide_instructions"]
        result["sample_path_unknown_calls"] = stack["sample_path_unknown_calls"]
        if not stack["sample_path_divide_free"]:
            output.unlink()
            raise SystemExit("Actual pi32v2 sample path contains divide instructions or unresolved calls; see zaaggenz-stack.json")
        result["checks"].append("actual_sample_path_divide_free")
    report = output.with_suffix(".json")
    report.write_text(json.dumps(result, indent=2) + "\n")
    print("Resource report:", report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
