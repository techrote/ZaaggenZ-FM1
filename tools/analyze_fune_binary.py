#!/usr/bin/env python3
"""Inspect the *actual* pi32v2 disassembly for sample-loop divides and stack frames.

Stack results are conservative direct-call estimates from generated prologues,
not a hardware high-water measurement. They exclude the caller/interrupt context.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re


def register_count(text: str) -> int:
    count = 0
    for token in text.split(","):
        token = token.strip()
        span = re.fullmatch(r"r(\d+)-r(\d+)", token)
        count += abs(int(span[1]) - int(span[2])) + 1 if span else 1
    return count


def analyze(disassembly: Path, symbols: Path) -> dict:
    funcs = {}
    for line in symbols.read_text().splitlines():
        match = re.match(r"([0-9a-f]{8})\s+.*\bF\s+\.text\s+([0-9a-f]{8})\s+(\S+)$", line)
        if match:
            funcs[match[3]] = {"address": int(match[1], 16), "bytes": int(match[2], 16), "lines": []}
    # Toolchain libc aliases have zero symbol size and sometimes no F marker.
    for line in symbols.read_text().splitlines():
        match = re.match(r"([0-9a-f]{8})\s+.*\.text\s+00000000\s+(memcpy|memmove|memset)$", line)
        if match:
            funcs[match[2]] = {"address": int(match[1], 16), "bytes": 0, "lines": []}
    current = None
    for line in disassembly.read_text().splitlines():
        label = re.fullmatch(r"([A-Za-z_][A-Za-z0-9_.$]*):", line)
        if label:
            current = label[1] if label[1] in funcs else None
        if current:
            funcs[current]["lines"].append(line)
    report = {}
    for name, function in funcs.items():
        frame, calls, divisions, backedges, call_sites = 0, set(), [], [], []
        for line in function["lines"]:
            saved = re.search(r"\[--sp\] = \{([^}]+)\}", line)
            if saved:
                frame += 4 * register_count(saved[1])
            allocation = re.search(r"\bsp \+= -(\d+)", line)
            if allocation:
                frame += int(allocation[1])
            call = re.search(r"\bcall\s+.*<([^ :+]+)(?:\+[^ :]+)?\s*:", line)
            if call:
                calls.add(call[1])
            address = re.match(r"\s*([0-9a-f]+):", line)
            if call and address:
                call_sites.append((int(address[1], 16), call[1]))
            tail = re.search(r"\bgoto\s+.*<([^ :+]+)\s*:", line)
            if tail and tail[1] != name:
                # Counting the caller's full frame over a tail call overstates
                # rather than understates the stack after the epilogue.
                calls.add(tail[1])
            if address and re.search(r"\br\d+(?:_r\d+)? / r\d+", line):
                divisions.append(int(address[1], 16))
            branch = re.search(r"\bgoto\s+-\d+\s+<[^:]+:\s*([0-9a-f]+)\s*>", line)
            if address and branch and int(branch[1], 16) < int(address[1], 16):
                target = int(branch[1], 16)
                if target >= function["address"]:
                    backedges.append((target, int(address[1], 16)))
        report[name] = {"frame_bytes": frame, "direct_calls": sorted(calls), "divide_addresses": divisions, "call_sites": call_sites}
        if backedges:
            start, end = max(backedges, key=lambda edge: edge[1] - edge[0])
            report[name]["largest_loop"] = {"start": start, "end": end, "divide_instructions": sum(start <= p <= end for p in divisions)}
    unknown = set()

    def peak(name, chain=()):
        if name in chain:
            raise ValueError("Recursive target call graph: " + " -> ".join((*chain, name)))
        if name not in report:
            unknown.add(name)
            return 0, [name]
        row = report[name]
        child = max((peak(c, (*chain, name)) for c in row["direct_calls"]), default=(0, []), key=lambda x: x[0])
        return row["frame_bytes"] + child[0], [name, *child[1]]

    roots = {}
    for name in ("zg_fune_render", "zg_fune_note_on", "zg_fune_block"):
        cost, chain = peak(name)
        roots[name] = {"conservative_direct_call_stack_bytes": cost, "path": chain}
    main_loop = report.get("block", {}).get("largest_loop")
    if not main_loop:
        raise ValueError("Cannot identify common engine sample loop in target binary")
    sample_names = set()

    def sample_children(name):
        if name in sample_names or name not in report:
            return
        sample_names.add(name)
        for child in report[name]["direct_calls"]:
            sample_children(child)

    for address, child in report["block"]["call_sites"]:
        if main_loop["start"] <= address <= main_loop["end"]:
            sample_children(child)
    sample_divides = {name: len(report[name]["divide_addresses"]) for name in sorted(sample_names)}
    sample_divides["block_largest_loop"] = main_loop["divide_instructions"]
    return {
        "method": "pi32v2 generated prologue saves + local allocations + longest direct call path; conservative estimate, not hardware high-water",
        "excludes": ["host caller stack", "interrupt preemption context", "unknown/indirect calls"],
        "unknown_direct_calls": sorted(unknown),
        "entry_points": roots,
        "sample_path_divide_instructions": sample_divides,
        "sample_path_divide_free": not any(sample_divides.values()),
        "common_engine_loop": main_loop,
        "functions": report,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--disassembly", type=Path, default=Path("build/fm1/zaaggenz.dis"))
    parser.add_argument("--symbols", type=Path, default=Path("build/fm1/zaaggenz.sym"))
    parser.add_argument("--output", type=Path, default=Path("build/fm1/zaaggenz-stack.json"))
    parser.add_argument("--require-divide-free", action="store_true")
    args = parser.parse_args()
    result = analyze(args.disassembly, args.symbols)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"entry_points": result["entry_points"], "sample_path_divide_instructions": result["sample_path_divide_instructions"]}, indent=2))
    if args.require_divide_free and not result["sample_path_divide_free"]:
        raise SystemExit("Generated per-sample target path contains divide instructions")


if __name__ == "__main__":
    main()
