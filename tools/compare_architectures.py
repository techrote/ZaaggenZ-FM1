#!/usr/bin/env python3
"""Measure native C and source-image C renders against authenticated evidence.

Numbers describe engineering differences, not listener preference. The source
decoder and native DSP probes execute the exact portable C implementation.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys
from fractions import Fraction
sys.dont_write_bytecode = True
import numpy as np
from scipy import signal
from scipy.io import wavfile
import compile_sources as compiler


def probe(executable, args):
    raw = subprocess.check_output([str(executable.resolve()), *map(str,args)])
    return np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.


def metrics(x, rate):
    x = np.asarray(x, dtype=np.float64)
    n = min(len(x), round(rate*.29))
    a = x[:n]
    p = abs(np.fft.rfft(a*np.hanning(n)))**2
    f = np.fft.rfftfreq(n, 1/rate)
    total = max(float(p.sum()), 1e-30)
    return dict(rms=float(np.sqrt(np.mean(a*a))), peak=float(np.max(abs(a))),
        crest_db=float(20*np.log10(max(float(np.max(abs(a))),1e-20) / max(float(np.sqrt(np.mean(a*a))),1e-20))),
        centroid_hz=float(np.sum(f*p)/total),
        energy_above_6khz=float(p[f>6000].sum()/total),
        zero_crossings_per_second=float(np.sum(np.diff(np.signbit(a)))/ (n/rate)))


def comparison(reference, x):
    n = min(len(reference), len(x))
    a, b = np.asarray(reference[:n]), np.asarray(x[:n])
    e = np.mean((a-b)**2)
    return dict(samples=n, snr_db=float(10*np.log10(max(np.mean(a*a),1e-30)/max(e,1e-30))),
                rms_error=float(np.sqrt(e)), correlation=float(np.corrcoef(a,b)[0,1]))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--reference", type=Path, required=True)
    p.add_argument("--source-probe", type=Path, required=True)
    p.add_argument("--native-probe", type=Path)
    p.add_argument("--out", type=Path, default=Path("build/architecture-comparison"))
    a=p.parse_args()
    compiler.recover(a.reference, a.out / "recovered")
    a.out.mkdir(parents=True, exist_ok=True)
    report = dict(version=1, reference_commit=compiler.PIN,
        qualification="host C execution; no FM-1 timing or listening pass inferred",
        gain_policy="all source/reference WAVs explicit fixed 1/8; native probe likewise trimmed 1/8",
        interpretation="spectral/temporal differences are not preference scores",
        sources=[])
    root = Path(__file__).resolve().parents[1]
    report['implementation_sha256'] = {
        str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in (root/'src/dsp.c', root/'src/source_bank.c', root/'src/source_bank_data.inc',
                     root/'include/zg/source_bank.h', root/'tests/dsp/dsp_probe.c', root/'tests/source_probe.c')}
    for i, name in enumerate(compiler.DEFAULT_IDS):
        raw, _, _ = compiler.source_render({"source_id": name})
        original = compiler.taper_tail(raw)
        source = probe(a.source_probe, [i,65536,0])
        reference = signal.resample_poly(original,147,160,window=("kaiser",8.6))/8.
        row = dict(source_id=name, c_image_metrics=metrics(source,44100),
                   c_image_reference=comparison(reference,source), transpositions=[])
        for ratio in (1,1.3626331526072845,1.5,2,4,8,32,256,413.4375):
            step = round(65536 * ratio)
            c_mip = probe(a.source_probe, [i,step,0])
            c_naive = probe(a.source_probe, [i,step,0,0])
            exact=Fraction(44100*65536,48000*step).limit_denominator(20000)
            ideal = signal.resample_poly(original,exact.numerator,exact.denominator,window=("kaiser",8.6))/8.
            row['transpositions'].append(dict(ratio=ratio,
                antialiased=comparison(ideal,c_mip), unfiltered=comparison(ideal,c_naive),
                note="against ideal polyphase varispeed, not duration-preserving FFT pitch shift"))
        held = probe(a.source_probe, [i,65536,44100])
        row['sustain'] = dict(rendered_samples=len(held), held_samples=44100,
            post_release_samples=max(0,len(held)-44100),
            finite=bool(np.isfinite(held).all()),
            max_sample_delta=float(np.max(abs(np.diff(held)))),
            note="release completes an active overlap then continues remaining source tail")
        wavfile.write(a.out / f"{i:02d}-source-image.wav",44100,np.asarray(source,dtype=np.float32))
        wavfile.write(a.out / f"{i:02d}-reference.wav",44100,np.asarray(reference,dtype=np.float32))
        wavfile.write(a.out / f"{i:02d}-held-source.wav",44100,np.asarray(held,dtype=np.float32))
        if a.native_probe and i:
            native = probe(a.native_probe,[i-1,0,0])/8.
            row['native_metrics'] = metrics(native,48000)
            row['native_fidelity_note'] = "causal 16-partial reinterpretation at 48 Hz; temporal/level mismatch is intentional, not an exact source replacement"
            wavfile.write(a.out / f"{i:02d}-native.wav",48000,np.asarray(native,dtype=np.float32))
        report['sources'].append(row)
    (a.out / "comparison.json").write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")
    print(json.dumps({"sources":len(report['sources']),"report":str(a.out/'comparison.json')}))


if __name__ == "__main__": main()
