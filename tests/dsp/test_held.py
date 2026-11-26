#!/usr/bin/env python3
"""Check held source presence and macro response in the actual C instrument."""
from __future__ import annotations
import argparse,hashlib,json,subprocess
from pathlib import Path
import numpy as np

RATE=44100
NAMES=['Bloom Bark','Formant Snarl','Upper Chop','Split Maul','Crushed Teeth','Harmonic Rip']

def rms(a):return float(np.sqrt(np.mean(a*a)))
def centroid(a):
    power=np.abs(np.fft.rfft(a*np.hanning(len(a))))**2
    hz=np.fft.rfftfreq(len(a),1/RATE)
    return float(np.dot(power,hz)/max(float(power.sum()),1e-30))

def run(probe,family,mode=1,macro=99,value=0,source=None):
    args=[str(probe),str(family),str(mode),str(macro),str(value)]
    if source is not None:args.append(str(source))
    raw=subprocess.check_output(args)
    assert len(raw)==RATE*3*3*4, 'probe PCM size/encoding changed'
    return np.frombuffer(raw,'<i4').reshape(-1,3).astype(np.float64)/32768.

def qualify(probe):
    rows=[]
    for family in range(6):
        for mode in range(3):
            audio=run(probe,family,mode);attack=audio[:13230];held=audio[RATE*2:RATE*5//2]
            row=dict(family=NAMES[family],mode=['native','image','hybrid'][mode],source=family+1,
                     raw_attack_rms=rms(attack[:,0]),raw_held_rms=rms(held[:,0]),
                     bus_attack_rms=rms(attack[:,1]),bus_held_rms=rms(held[:,1]),
                     persistent_held_rms=rms(held[:,2]),
                     bus_attack_centroid_hz=centroid(attack[:,1]),bus_held_centroid_hz=centroid(held[:,1]))
            row['bus_held_vs_attack_db']=float(20*np.log10(row['bus_held_rms']/max(row['bus_attack_rms'],1e-30)))
            row['bus_vs_persistent_held_db']=float(20*np.log10(row['bus_held_rms']/max(row['persistent_held_rms'],1e-30)))
            assert row['raw_held_rms']>.005, f'{row["family"]} {row["mode"]}: source disappeared'
            assert row['bus_held_vs_attack_db']>-12, f'{row["family"]} {row["mode"]}: held line collapsed'
            rows.append(row)
    macros=[]
    for family in range(6):
        for macro in range(1,4):
            low=run(probe,family,1,macro,0)[RATE*2:RATE*5//2]
            high=run(probe,family,1,macro,127)[RATE*2:RATE*5//2]
            row=dict(family=NAMES[family],macro=['','GRIT','MOTION','VOWEL'][macro],
                     source_bus_change_rms=rms(high[:,1]-low[:,1]),
                     source_bus_change_relative_rms=rms(high[:,1]-low[:,1])/max(rms(low[:,1]),1e-30),
                     persistent_change_rms=rms(high[:,2]-low[:,2]),
                     source_low_centroid_hz=centroid(low[:,1]),source_high_centroid_hz=centroid(high[:,1]))
            if macro in (1,3):assert row['source_bus_change_relative_rms']>.01, f'{row["family"]}: ineffective {row["macro"]}'
            else:assert row['persistent_change_rms']>.001, f'{row["family"]}: ineffective layered MOTION'
            macros.append(row)
    default=run(probe,0,1,99,0,0)
    default_row=dict(bus_attack_rms=rms(default[:13230,1]),bus_held_rms=rms(default[RATE*2:RATE*5//2,1]),
                     persistent_held_rms=rms(default[RATE*2:RATE*5//2,2]))
    assert default_row['bus_held_rms']>.03
    return dict(method='actual-C-engine-held-source-v1',probe_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),
                note=36,attack_seconds=[0,.3],held_seconds=[2,2.5],architectures=rows,macros=macros,
                default_locked_image=default_row,hardware_qualified=False,
                interpretation='Presence and measurable control response, not preference. Bark MOTION operates on persistent roles; other characters also modulate the source bus.')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--probe',required=True,type=Path);p.add_argument('--out',type=Path);a=p.parse_args()
    result=qualify(a.probe.resolve())
    if a.out:a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(result,indent=2)+'\n')
    print('Held source PASS: 18 family/architecture cases retain SYNTHLINE after 2 seconds; GRIT/VOWEL and layered MOTION respond in all six families')
