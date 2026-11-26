#!/usr/bin/env python3
"""Acoustic contracts for actual C DSP, not an alternate Python synthesizer.

Usage: python tests/dsp/test_acoustics.py --probe build/dsp_probe --out report.json
Needs NumPy. Metrics describe differentiation/images, never musical preference.
"""
import argparse
import json
from pathlib import Path
import subprocess
import numpy as np

NAMES = ['Bloom Bark','Formant Snarl','Upper Chop','Split Maul','Crushed Teeth','Harmonic Rip']
def render(probe, family, quality=0, sine=False, increment=4294967):
    raw=subprocess.check_output([str(probe),str(family),str(quality),str(int(sine)),str(increment)])
    a=np.frombuffer(raw,dtype='<i2').astype(np.float64)/32768.
    if a.size!=96000:raise AssertionError(f'wrong probe length: {a.size}')
    return a

def measure(probe):
    audio=[render(probe,f) for f in range(6)]
    correlation=np.corrcoef(audio)
    upper=correlation[np.triu_indices(6,1)]
    # Catch a regression to six near-identical scalar-gain variants.
    assert np.max(np.abs(upper))<.97, f'families too correlated: {upper.max()}'
    features=[]
    for name,a in zip(NAMES,audio):
        part=a[12000:48000];power=np.abs(np.fft.rfft(part*np.hanning(len(part))))**2
        hz=np.fft.rfftfreq(part.size,1/48000)
        centroid=float(np.sum(power*hz)/np.sum(power))
        features.append(dict(family=name,rms=float(np.sqrt(np.mean(a*a))),peak=float(np.max(np.abs(a))),
                             spectral_centroid_hz=centroid,
                             energy_above_12khz_db=float(10*np.log10((np.sum(power[hz>12000])+1e-30)/np.sum(power)))))
        assert .02<features[-1]['rms']<.6, f'{name} silent or flattened'
        assert features[-1]['peak']<.99, f'{name} reaches PCM clipping'
    assert max(f['spectral_centroid_hz'] for f in features)>3*min(f['spectral_centroid_hz'] for f in features)
    # Coherent tone avoids FFT-window leakage: folded harmonic-bin energy is
    # an identified nonlinear-image metric, not simply all high-frequency power.
    size=32768;bin0=2219;inc=bin0*(2**32//size)
    in_band=set(range(bin0,size//2,bin0));aliases=[]
    for harmonic in range(size//(2*bin0)+1,28):
        b=(bin0*harmonic)%size;b=min(b,size-b)
        if b not in in_band:aliases.append(b)
    alias=[]
    for q in (0,1):
        a=render(probe,0,q,True,inc)[32768:65536]
        power=np.abs(np.fft.rfft(a))**2
        alias.append(float(10*np.log10(np.sum(power[aliases])/np.sum(power))))
    assert alias[1]<alias[0]-2., f'2x local shaper failed its Bark probe: {alias}'
    return dict(method='zg-native-dsp-acoustic-v1',sample_rate_hz=48000,
                input_source='actual compiled C DSP',features=features,
                maximum_family_correlation=float(np.max(np.abs(upper))),
                family_correlation=correlation.tolist(),
                local_shaper_alias_probe=dict(frequency_hz=48000*bin0/size,
                                             direct_folded_harmonic_db=alias[0],
                                             local_2x_folded_harmonic_db=alias[1]),
                limitation='A coherent Bark probe does not establish broadband alias freedom; intentional crushing images remain.')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--probe',required=True,type=Path);parser.add_argument('--out',type=Path)
    args=parser.parse_args();report=measure(args.probe.resolve())
    if args.out:
        args.out.parent.mkdir(parents=True,exist_ok=True);args.out.write_text(json.dumps(report,indent=2)+'\n')
    print('Acoustics PASS: six differentiated native families; maximum correlation '
          f"{report['maximum_family_correlation']:.3f}; local Bark 2x alias probe "
          f"{report['local_shaper_alias_probe']['direct_folded_harmonic_db']:.2f} → "
          f"{report['local_shaper_alias_probe']['local_2x_folded_harmonic_db']:.2f} dB")
