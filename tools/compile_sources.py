#!/usr/bin/env python3
"""Compile authenticated Zaaggenz source images into a bounded XIP PCM bank.

The upstream repository is read-only. Its own strict recovery verifier writes
the legacy runtime beneath --work, never into the reference checkout. Run with
Python 3 and NumPy/SciPy; the manifest records all numeric tool versions.
"""
from __future__ import annotations

import argparse
from dataclasses import replace
import hashlib
import importlib.util
import json
import math
import os
from fractions import Fraction
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
import numpy as np
import scipy
from scipy import signal
from scipy.io import wavfile

PIN = "d7e580c190d00e1d00a573a6b845c75ddddc1d4c"
PAYLOAD_SHA = "eaf5d3ab822dfa121cd3b795d62619af9e62c041fb02de19f8dbeafb0014c919"
LOCKED_SHA = "25eabbe29b4ea4a2930acd700a32af141cfe72337b1b88edc7dee3476d09fe65"
DEFAULT_IDS = ["locked_bloom", "zaag.bloom-bark", "zaag.formant-snarl",
               "zaag.upper-chop", "zaag.split-maul", "zaag.crushed-teeth",
               "zaag.harmonic-rip"]
RATE = 44100
REFERENCE_RATE = 48000
PCM_SHIFT = 3
CROSSFADE = 1024
MIP_DENOMINATORS = [160, 240, *[320 << j for j in range(8)]]
MIP_COUNT = len(MIP_DENOMINATORS)
MULAW = 255.
MAX_BANK_BYTES = 260000
RECIPE_NODE_TYPES = {
    "core.identity.v1", "core.gain.v1", "core.tanh.v1", "core.hard_clip.v1",
    "core.tanh_aa.v1", "core.hard_clip_aa.v1", "core.multiband_gain.v1",
    "legacy.sculpt.v1",
}


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def pcm_sha(x) -> str:
    return sha(np.asarray(x, dtype="<f4").tobytes())


def recover(reference: Path, work: Path):
    reference = reference.resolve()
    head = subprocess.check_output(["git", "-C", str(reference), "rev-parse", "HEAD"], text=True).strip()
    if head != PIN:
        raise ValueError(f"reference must be pinned to {PIN}, found {head}")
    tracked = subprocess.check_output(["git", "-C", str(reference), "status", "--porcelain", "--untracked-files=no"], text=True)
    if tracked:
        raise ValueError("reference tracked files are modified; refusing unauthenticated source")
    file = reference / "baseline/recovered_source/materialize_v2.py"
    spec = importlib.util.spec_from_file_location("_zaag_verified_recovery", file)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    count, payload = mod.materialize(file.parent, work)
    if payload != PAYLOAD_SHA or count != 34:
        raise ValueError("recovered runtime identity mismatch")
    sys.path[:0] = [str(work.resolve() / "app"), str(reference)]
    return head


def read_spec(path: Path | None):
    if path is None:
        return [{"source_id": name} for name in DEFAULT_IDS]
    if path.stat().st_size > 2_000_000:
        raise ValueError("source recipe exceeds two-megabyte JSON bound")
    def pairs(items):
        out = {}
        for key, value in items:
            if key in out: raise ValueError("duplicate source recipe key: " + key)
            out[key] = value
        return out
    def bad_constant(value):
        raise ValueError("nonfinite source recipe number: " + value)
    d = json.loads(path.read_text(), object_pairs_hook=pairs, parse_constant=bad_constant)
    if not isinstance(d, dict) or set(d) != {"version", "sources"} or type(d["version"]) is not int or d["version"] != 1:
        raise ValueError("source spec must contain exactly version:1 and sources")
    if not isinstance(d["sources"], list) or not 1 <= len(d["sources"]) <= 8:
        raise ValueError("source spec requires 1..8 source entries")
    out = []
    for entry in d["sources"]:
        if not isinstance(entry, dict) or set(entry) - {"source_id", "synth_overrides"}:
            raise ValueError("unknown source entry field")
        if entry.get("source_id") not in [*DEFAULT_IDS, "custom.legacy"]:
            raise ValueError("only registered, inspected source IDs are supported")
        overrides = entry.get("synth_overrides", {})
        if not isinstance(overrides, dict):
            raise ValueError("synth_overrides must be an object")
        if any(k in overrides for k in ("sr", "f0_hz", "beats")):
            raise ValueError("sr, f0_hz and beats are fixed by bank version 1")
        if entry["source_id"] != "custom.legacy" and overrides:
            raise ValueError("protected/registered identities cannot be overridden; use custom.legacy")
        out.append(entry)
    return out


def import_render_recipe(path: Path):
    """Admit an actual upstream RenderRecipe as one compiled source event.

    This intentionally does not turn a phrase/arrangement into an unidentified
    sample. The unsupported owner domains fail before any rendering is started.
    """
    from zaaggenz_contracts import Contract
    if path.stat().st_size > 2_000_000:
        raise ValueError("RenderRecipe exceeds two-megabyte JSON bound")
    contract = Contract.from_json(path.read_bytes())
    d = contract.to_dict()
    if d.get("kind") != "RenderRecipe":
        raise ValueError("RenderRecipe required")
    if d['render_mode'] != 'synth' or d['channels'] != 1:
        raise ValueError("embedded recipe import supports mono synth mode only")
    for key in ('phrase', 'arrangement', 'reversebass', 'sculpt'):
        if d[key] is not None:
            raise ValueError(f"unsupported recipe owner domain: {key}; compile a single source event")
    if 'rack' in d:
        raise ValueError("spectral rack recipes remain desktop-only")
    if d['phase_policy'] not in ('legacy-v1.2.1', 'source-derived'):
        raise ValueError("source import has no prior phase checkpoint; phase policy unsupported")
    if d['tail']['mode'] not in ('legacy', 'preserve') or d['tail']['maximum_samples'] != 0:
        raise ValueError("source import preserves the natural event; gated/capped tail policy requires a phrase")
    if d['source']['method'] != 'legacy.synth.1.2.1':
        raise ValueError("unsupported source method")
    params = d['source']['params']
    if params['sr'] != 48000 or params['f0_hz'] != 48:
        raise ValueError("bank version 1 requires an authored 48 kHz, 48 Hz source; no implicit retuning")
    frames = round(48000 * 60. / params['bpm'] * params['beat_fill'])
    if not 960 <= frames <= 48000:
        raise ValueError("source event must fit 20 ms..1 second before rendering")
    tm = d['time_map']
    if tm['origin_sample'] != 0 or len(tm['tempo_segments']) != 1:
        raise ValueError("source compilation requires a single tempo and zero time origin")
    segment=tm['tempo_segments'][0]
    if segment['beat'] != '0/1' or abs(float(Fraction(segment['bpm']))-float(params['bpm'])) > 1e-10:
        raise ValueError("single source tempo must agree with its time map at beat zero")
    if len(d['nodes']) > 16 or any(n['type_id'] not in RECIPE_NODE_TYPES for n in d['nodes']):
        raise ValueError("unsupported or excessive DSP graph: maximum 16 inspected node types")
    if any(len(n['automation']) > 8 or any(len(lane['points']) > 256 for lane in n['automation']) for n in d['nodes']):
        raise ValueError("DSP automation exceeds embedded compiler admission bounds")
    return {"source_id": "compiled." + contract.sha256[:12],
            "render_recipe": d, "render_recipe_sha256": contract.sha256}


def render_imported_recipe(entry):
    from uptempo_harmony.synth import KickParams, synthesize_one
    from zaaggenz_zaag import family, render_family_source, PRODUCTION_PRESET_IDS
    from zaaggenz_dsp.graph import execute_graph, apply_output_policy
    d = entry['render_recipe']
    params = d['source']['params']
    sid = d['source']['id']
    if sid in PRODUCTION_PRESET_IDS:
        source = render_family_source(family(sid), 48000, beats=1, bpm=params['bpm'])
        if source.source_params != params:
            raise ValueError("registered family parameters do not match their accepted identity")
        raw = source.audio
    else:
        raw, _ = synthesize_one(KickParams(**params))
    graph = execute_graph(raw, 48000, d['nodes'], d['output_node'], sid, False)
    output, output_diagnostics = apply_output_policy(graph.output, d['output'])
    diagnostics = dict(method="authenticated-render-recipe-source-graph.v1",
        recipe_sha256=entry['render_recipe_sha256'], original_source_id=sid,
        original_source_pcm_sha256=pcm_sha(raw), graph_order=list(graph.order),
        pre_output_pcm_sha256=pcm_sha(graph.output), output_policy=d['output'],
        output_diagnostics=output_diagnostics,
        compilation_scope="one source event, graph and declared output baked into an asset",
        keyboard_tuning="not compiled; no phrase events admitted; FM1 patch owns live tuning",
        phase_policy=d['phase_policy'], tail_policy=d['tail'])
    return output, params, diagnostics


def source_render(entry):
    if 'render_recipe' in entry:
        return render_imported_recipe(entry)
    from uptempo_harmony.synth import PRESETS, synthesize_one
    from zaaggenz_zaag import family, render_family_source
    from zaaggenz_contracts.legacy import adapt_parameters
    name = entry["source_id"]
    if name in ("locked_bloom", "custom.legacy"):
        overrides = entry.get("synth_overrides", {})
        params = PRESETS["locked_bloom"]
        if overrides:
            # The frozen contract validates unknown names, ranges and types.
            validated = adapt_parameters("synth", {**params.to_dict(), **overrides})
            params = type(params)(**validated)
        x, _ = synthesize_one(params)
        if name == "locked_bloom" and pcm_sha(x) != LOCKED_SHA:
            raise ValueError(f"locked_bloom anchor PCM mismatch: {pcm_sha(x)}")
        return x, params.to_dict(), {"method": "authenticated-legacy.synth.1.2.1",
                                    "protected_anchor_equal": pcm_sha(x) == LOCKED_SHA}
    recipe = family(name)
    out = render_family_source(recipe, REFERENCE_RATE, beats=1)
    return out.audio, out.source_params, out.diagnostics


def taper_tail(x):
    y = np.asarray(x, dtype=np.float64).copy()
    n = min(96, len(y))
    y[-n:] *= np.linspace(1., 0., n)
    return y


def decode_table():
    return np.asarray([(-1 if q & 128 else 1) * round(np.expm1((q & 127)/127*np.log1p(MULAW))/MULAW*32767)
                       for q in range(256)], dtype='<i2')


def quantize(x):
    # A common, explicit binary encoding scale preserves inter-family level.
    # No peak or RMS normalization and no waveform-dependent compensation.
    x = np.asarray(x, dtype=np.float64)
    if not np.isfinite(x).all() or np.max(abs(x), initial=0.) >= 8.:
        raise ValueError("source exceeds the common ±8 encoding range; reject instead of normalize")
    q = np.rint(np.log1p(MULAW*np.abs(x)/8.) / np.log1p(MULAW) * 127).astype(np.uint8)
    return q | ((x < 0).astype(np.uint8) << 7)


def choose_loop(x, rate=22050, crossfade=512):
    """Select a four-plus-period segment with similar crossfade windows.

    This is a causal sustain reinterpretation, not a claim that the transient
    source was originally periodic. The overlap is a fixed 512 source samples.
    """
    x = np.asarray(x, dtype=np.float64)
    n = len(x)
    if n < 4096:
        return 0, 0, {"overlap_samples": 0, "policy": "finite-only; source too short for bounded sustain"}
    start = int(n * .32)
    candidates = range(int(n * .72), int(n * .88))
    a = x[start:start+crossfade]
    best = None
    for end in candidates:
        b = x[end-crossfade:end]
        e = float(np.mean((a-b)**2))
        if best is None or e < best[0]: best = (e, end)
    end = best[1]
    return start, end, {
        "overlap_samples": crossfade,
        "overlap_rms_error": math.sqrt(best[0]),
        "raw_boundary_jump": float(abs(x[end-1]-x[start+crossfade])),
        "loop_period_ms": 1000. * (end-start-crossfade) / rate,
        "policy": "matched-segment-linear-overlap; release-finishes-current-overlap",
    }


def decode(q, frames, step=1.):
    pos = np.arange(int(math.ceil(frames / step)), dtype=np.float64) * step
    words = decode_table()[np.asarray(q, dtype=np.uint8)]
    return np.interp(pos, np.arange(len(q)), words, right=0.) * ((1 << PCM_SHIFT) / 32768.)


def describe(reference, reconstructed):
    n = min(len(reference), len(reconstructed))
    a, b = np.asarray(reference[:n]), np.asarray(reconstructed[:n])
    err = np.mean((a-b)**2)
    power = np.mean(a*a)
    return {
        "rms_reference": float(np.sqrt(power)),
        "rms_reconstructed": float(np.sqrt(np.mean(b*b))),
        "snr_db": float(10*np.log10(max(power, 1e-30) / max(err, 1e-30))),
        "correlation": float(np.corrcoef(a, b)[0,1]),
        "peak_absolute_error": float(np.max(abs(a-b), initial=0.)),
    }


def compile_bank(reference, work, output, manifest_path, wav_dir, spec_path=None, render_recipe_paths=None):
    head = recover(reference, work)
    entries = ([import_render_recipe(p) for p in render_recipe_paths]
               if render_recipe_paths else read_spec(spec_path))
    if not 1 <= len(entries) <= 8:
        raise ValueError("at most eight imported recipes fit the bounded bank format")
    table = decode_table()
    blocks, metadata = ['const int16_t zg_source_decode_table[256] = {\n    ' +
                        ',\n    '.join(', '.join(str(int(v)) for v in table[j:j+16]) for j in range(0,256,16)) + '\n};\n'], []
    total = 0
    if wav_dir: wav_dir.mkdir(parents=True, exist_ok=True)
    for i, entry in enumerate(entries):
        raw, params, diagnostics = source_render(entry)
        raw = np.asarray(raw, dtype=np.float32)
        if not np.isfinite(raw).all() or len(raw) > 48000:
            raise ValueError("bank source must be finite and at most one second")
        conditioned = taper_tail(raw)
        mips = []
        for level, denominator in enumerate(MIP_DENOMINATORS):
            # Explicit polyphase antialias filtering at each declared level.
            filtered = signal.resample_poly(conditioned, 147, denominator,
                                            window=("kaiser", 8.6))
            q = quantize(filtered)
            q = np.r_[q, np.uint8(0)].astype(np.uint8)
            mips.append(q)
            total += q.nbytes
            values = [", ".join(str(int(v)) for v in q[j:j+16]) for j in range(0, len(q), 16)]
            blocks.append(f"static const uint8_t zg_source_{i}_{level}[{len(q)}] = {{\n    " + ",\n    ".join(values) + "\n};\n")
        # Keep the established sustain segment timing: matching is performed on
        # the original 22.05 kHz linear PCM condition, then mapped exactly 2x.
        match = signal.resample_poly(conditioned,147,320,window=("kaiser",8.6))
        match = np.rint(match * 4096.) / 4096.
        start, end, loop = choose_loop(match)
        start *= 2; end *= 2
        loop['matching_rate_hz'] = 22050
        loop['bank_overlap_samples'] = 1024 if end else 0
        target = signal.resample_poly(conditioned, 147, 160, window=("kaiser", 8.6))
        decoded = decode(mips[0], len(mips[0])-1)
        # A single-cycle comparator explicitly measures the discarded temporal
        # detail; it is never substituted for the accepted source silently.
        period = round(RATE / 48.)
        cycle = mips[0][start:start+period]
        tiled = np.tile(cycle, math.ceil((len(mips[0])-1)/period))[:len(mips[0])-1]
        cycle_decoded = decode(np.r_[tiled, 0], len(tiled))
        md = dict(index=i, source_id=entry["source_id"], params=params,
                  source_pcm_sha256=pcm_sha(raw), reference_diagnostics=diagnostics,
                  source_samples=len(raw), source_sample_rate=REFERENCE_RATE,
                  source_duration_ms=1000.*len(raw)/REFERENCE_RATE,
                  peak=float(np.max(abs(raw))), sample_rate=RATE, root_hz=48,
                  pcm_shift=PCM_SHIFT, frames=[len(q)-1 for q in mips],
                  pcm_sha256=[sha(q.tobytes()) for q in mips],
                  decoded_pcm_sha256=[sha(table[q].astype('<i2').tobytes()) for q in mips],
                  loop_start=start, loop_end=end, loop=loop,
                  full_image_measurements=describe(target, decoded),
                  single_cycle_measurements=describe(target, cycle_decoded),
                  reference_path=f"{entry['source_id'].replace('.', '_')}-reference.wav")
        metadata.append(md)
        if wav_dir:
            label = entry['source_id'].replace('.', '_')
            # Audition WAVs all receive the SAME explicit -18.0618 dB trim.
            # Float data and manifest preserve the source's untrimmed identity.
            wavfile.write(wav_dir / f"{label}-reference.wav", 44100,
                          np.asarray(np.rint(np.clip(target/8., -1., 1.)*32767), dtype=np.int16))
            wavfile.write(wav_dir / f"{label}-image.wav", 44100,
                          np.asarray(np.rint(np.clip(decoded/8., -1., 1.)*32767), dtype=np.int16))
    if total + table.nbytes > MAX_BANK_BYTES:
        raise ValueError(f"bank exceeds {MAX_BANK_BYTES}-byte hard limit: {total + table.nbytes}")
    text = "/* Generated by tools/compile_sources.py; DO NOT EDIT.\n"
    text += f" * Source: techrote/zaaggenz@{head}\n"
    text += " * Derived assets: LicenseRef-Owner-Provided-Unspecified.\n"
    text += " * mu-law8, mu=255; common fixed encoding scale = 1/8; no normalization. */\n\n"
    text += f"#define ZG_COMPILED_SOURCE_COUNT {len(metadata)}\n"
    text += "#define ZG_COMPILED_SOURCE_NAMES " + ', '.join(json.dumps(m['source_id']) for m in metadata) + "\n\n"
    text += "\n".join(blocks)
    text += f"\nconst unsigned zg_source_count = {len(metadata)}u;\n"
    text += "const zg_source_info zg_source_bank[] = {\n"
    for md in metadata:
        i = md['index']
        arrays = ", ".join(f"zg_source_{i}_{j}" for j in range(MIP_COUNT))
        frames = ", ".join(f"{v}u" for v in md['frames'])
        text += f'    {{"{md["source_id"]}", {{{arrays}}}, {{{frames}}}, {md["loop_start"]}u, {md["loop_end"]}u, 48u, 44100u, 3u}},\n'
    text += "};\n"
    output.parent.mkdir(parents=True, exist_ok=True)
    # The generated hash is over canonical LF/UTF-8 bytes, also on Windows.
    output.write_bytes(text.encode('utf-8'))
    manifest = dict(format="zg-source-bank", version=2,
        upstream_repository="https://github.com/techrote/zaaggenz", upstream_commit=head,
        recovered_payload_sha256=PAYLOAD_SHA, locked_bloom_anchor_sha256=LOCKED_SHA,
        license="LicenseRef-Owner-Provided-Unspecified",
        toolchain=dict(python=sys.version.split()[0], numpy=np.__version__, scipy=scipy.__version__),
        representation=f"{len(metadata)} bounded post-topology source images, 44.1/29.4/22.05 kHz plus seven half-rate mips",
        encoding="mu-law8 sign-magnitude, mu=255; fixed 256-entry int16 LUT; common gain 8 restored in Q15 decoder",
        output_policy="no per-source normalization; 2ms terminal taper before resampling",
        audition_gain_db=-18.06179973983887,
        transposition="source-derived varispeed; pitch changes source duration",
        state_bytes=16, mip_count=MIP_COUNT,
        mip_rates_hz=[REFERENCE_RATE*147/d for d in MIP_DENOMINATORS],
        pcm_flash_bytes=total, decode_lut_bytes=table.nbytes,
        total_asset_flash_bytes=total+table.nbytes, decode_lut_sha256=sha(table.tobytes()),
        generated_sha256=sha(text.encode()), sources=metadata)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_bytes((json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode('utf-8'))
    print(json.dumps({"bank_bytes": total+table.nbytes, "sources": len(metadata), "generated_sha256": manifest['generated_sha256'],
                      "anchor_exact": any(s['source_id'] == 'locked_bloom' and s['source_pcm_sha256'] == LOCKED_SHA for s in metadata),
                      "output": str(output)}))
    return manifest


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--reference", type=Path, required=True)
    p.add_argument("--work", type=Path, default=Path("build/reference-runtime"))
    p.add_argument("--output", type=Path)
    p.add_argument("--manifest", type=Path)
    p.add_argument("--wav-dir", type=Path)
    group = p.add_mutually_exclusive_group()
    group.add_argument("--spec", type=Path, help="Optional strict version-1 source recipe JSON")
    group.add_argument("--render-recipe", type=Path, action="append", help="Actual upstream RenderRecipe JSON, mono single event; repeat for up to eight recipes")
    a = p.parse_args()
    custom = bool(a.render_recipe)
    output = a.output or Path("build/custom-bank/source_bank_data.inc" if custom else "src/source_bank_data.inc")
    manifest = a.manifest or Path("build/custom-bank/source_bank_manifest.json" if custom else "assets/source_bank_manifest.json")
    wavs = a.wav_dir or Path("build/custom-bank/auditions" if custom else "build/reference-audio")
    try: compile_bank(a.reference, a.work, output, manifest, wavs, a.spec, a.render_recipe)
    except (ValueError, RuntimeError, OSError) as exc: p.exit(2, str(exc) + "\n")


if __name__ == "__main__": main()
