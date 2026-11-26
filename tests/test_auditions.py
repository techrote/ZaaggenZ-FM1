#!/usr/bin/env python3
"""Qualification of WAVs produced by the actual portable/Fune instrument.

The renderer appends release tails. This test catches inaudible presets,
clipping, tail truncation and collapse of the bank's spectral range. It does
not use acoustic metrics as preference or fidelity scores.
"""
from __future__ import annotations
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import wave
import numpy as np


def read_wav(path):
    with wave.open(str(path), 'rb') as wav:
        assert wav.getnchannels()==1 and wav.getsampwidth()==2
        rate=wav.getframerate();frames=wav.getnframes()
        audio=np.frombuffer(wav.readframes(frames), dtype='<i2').astype(np.float64)/32768.
    assert audio.size==frames and frames>rate
    return audio,rate


def qualify(renderer, directory, seconds=3, workers=2):
    listing=subprocess.check_output([str(renderer),'--list'],text=True).splitlines()
    presets=[]
    for line in listing:
        index,name,*description=line.split('\t')
        presets.append((int(index),name))
    assert len(presets)>=36, 'factory bank lost presets'
    directory.mkdir(parents=True,exist_ok=True)
    def one(item):
        index,name=item;path=directory/f'preset-{index+1:02d}.wav'
        result=json.loads(subprocess.check_output([
            str(renderer),'--preset',str(index),'--pattern','riff',
            '--note','36','--seconds',str(seconds),'--out',str(path)],text=True))
        a,rate=read_wav(path)
        active=a[:seconds*rate]
        size=4096;hop=2048;window=np.hanning(size)
        power=np.mean([np.abs(np.fft.rfft(active[i:i+size]*window))**2
                       for i in range(0,len(active)-size,hop)],axis=0)
        hz=np.fft.rfftfreq(size,1/rate)
        total=float(power.sum())
        assert total>1e-12, f'{name}: silence'
        result.update(name=name,pcm_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                      spectral_centroid_hz=float(np.dot(power,hz)/total),
                      dc_mean=float(a.mean()),
                      maximum_adjacent_jump=float(np.max(np.abs(np.diff(a)))),
                      final_50ms_rms=float(np.sqrt(np.mean(a[-rate//20:]**2))),
                      final_sample=float(a[-1]),
                      duration_s=len(a)/rate)
        assert result['clipped_frames']==0, f'{name}: clipped {result["clipped_frames"]} frames'
        assert .005<result['rms']<.5, f'{name}: unexpectedly silent/flattened RMS {result["rms"]}'
        assert result['final_50ms_rms']<.003, f'{name}: release tail truncated {result["final_50ms_rms"]}'
        assert abs(result['final_sample'])<.003, f'{name}: nonzero endpoint {result["final_sample"]}'
        assert result['duration_s']>seconds, f'{name}: no appended tail'
        return result
    with ThreadPoolExecutor(max_workers=workers) as pool:
        reports=list(pool.map(one,presets))
    centroids=[r['spectral_centroid_hz'] for r in reports]
    assert max(centroids)>3*min(centroids), 'factory bank lost spectral differentiation'
    return dict(method='zg-factory-audition-qualification-v1',
                renderer_sha256=hashlib.sha256(renderer.read_bytes()).hexdigest(),
                source='Actual C instrument linked into the FM-1 plugin',
                scenario=dict(note=36,pattern='riff',musical_duration_s=seconds),
                preset_count=len(reports),total_clipped_frames=sum(r['clipped_frames'] for r in reports),
                spectral_centroid_range_hz=[min(centroids),max(centroids)],
                maximum_final_50ms_rms=max(r['final_50ms_rms'] for r in reports),
                hardware_qualified=False,presets=reports)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--renderer',required=True,type=Path)
    parser.add_argument('--out',type=Path);parser.add_argument('--audio-dir',type=Path)
    parser.add_argument('--seconds',type=int,default=3);parser.add_argument('--workers',type=int,default=2)
    args=parser.parse_args();renderer=args.renderer.resolve()
    if args.audio_dir:
        report=qualify(renderer,args.audio_dir,args.seconds,args.workers)
    else:
        with tempfile.TemporaryDirectory(prefix='zg-auditions-') as temp:
            report=qualify(renderer,Path(temp),args.seconds,args.workers)
    if args.out:
        args.out.parent.mkdir(parents=True,exist_ok=True);args.out.write_text(json.dumps(report,indent=2)+'\n')
    print(f"Auditions PASS: {report['preset_count']} playable preset WAVs, zero clipped frames; "
          f"maximum final-50ms RMS {report['maximum_final_50ms_rms']:.6f}; "
          f"centroid range {report['spectral_centroid_range_hz'][0]:.0f}–{report['spectral_centroid_range_hz'][1]:.0f} Hz")
