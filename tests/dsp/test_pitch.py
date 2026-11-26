#!/usr/bin/env python3
"""Measure actual rendered persistent-layer pitch by interpolated crossings."""
import argparse,json,subprocess
from pathlib import Path
import numpy as np

def qualify(probe):
    out=[]
    for tuning,steps,period in [(0,12,2),(1,19,2),(2,24,2),(3,13,3)]:
        for layer in (2,3,4):
            raw=subprocess.check_output([str(probe),str(layer),'36',str(tuning),'0'])
            a=np.frombuffer(raw,'<i2').astype(np.float64)[44100:]
            idx=np.nonzero((a[:-1]<0)&(a[1:]>=0))[0]
            assert idx.size>15
            crossings=idx-a[idx]/(a[idx+1]-a[idx])
            measured=44100/np.median(np.diff(crossings))
            key=36+(7 if layer==3 else 0)
            expected=130.8127826502993*period**((key-48)/steps)/(2 if layer==4 else 1)
            cents=1200*np.log2(measured/expected)
            assert abs(cents)<.15, f'tuning{tuning} layer{layer}: pitch error {cents} cents'
            out.append(dict(tuning=tuning,layer=layer,expected_hz=expected,measured_hz=float(measured),error_cents=float(cents)))
    # Pedal ignores melodic keyboard movement for every tuning, but honors its
    # tuning's declared fixed root at key 36.
    for tuning,steps,period in [(0,12,2),(1,19,2),(2,24,2),(3,13,3)]:
        pcm=[]
        for note in (36,48):
            raw=subprocess.check_output([str(probe),'4',str(note),str(tuning),'1'])
            pcm.append(raw)
        assert pcm[0]==pcm[1], f'tuning{tuning}: pedal moved with melodic note'
    return dict(method='zg-rendered-layer-pitch-v1',cases=out,pedal_cases=4,
                maximum_abs_error_cents=max(abs(r['error_cents']) for r in out),
                limitation='Tests persistent sine role tuning, not a claim of fundamental estimator correctness for intentionally rough or ring-modulated source timbres.')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--probe',required=True,type=Path);p.add_argument('--out',type=Path);a=p.parse_args()
    result=qualify(a.probe.resolve())
    if a.out:a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(result,indent=2)+'\n')
    print(f"Pitch PASS: 12 rendered tuning/layer cases, 4 pedal invariants, maximum error {result['maximum_abs_error_cents']:.4f} cents")
