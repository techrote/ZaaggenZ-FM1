#!/usr/bin/env python3
"""Verify actual C decoder bytes against the compiler's independent PCM hashes.

This catches incorrect asset selection, codecs, endianness and Windows text-mode
stdout corruption. Optional full-MIDI validation checks actual varispeed length
and thereby catches silent pitch clamps across all 128 ordinary MIDI keys.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess


def run(probe: Path, asset: int, step: int, hold: int = 0):
    return subprocess.check_output([str(probe.resolve()), str(asset), str(step), str(hold)])


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-probe', type=Path, required=True)
    p.add_argument('--manifest', type=Path, default=Path('assets/source_bank_manifest.json'))
    p.add_argument('--full-midi', action='store_true')
    p.add_argument('--output', type=Path)
    a=p.parse_args()
    manifest=json.loads(a.manifest.read_text())
    if manifest['format'] != 'zg-source-bank' or manifest['version'] != 2:
        p.error('format-v2 source manifest required')
    result={'status':'PASS','format_version':2,'generated_sha256':manifest['generated_sha256'],
            'decoder_bytes':[],'midi_keys_checked':0,'non_octave_edge_steps_checked':[],
            'qualification':'host C decode; not hardware timing or listening qualification'}
    for source in manifest['sources']:
        data=run(a.source_probe,source['index'],65536)
        expected=(source['frames'][0]+1)*2
        if len(data)!=expected:
            raise SystemExit(f"{source['source_id']}: expected {expected} binary PCM bytes, got {len(data)}")
        digest=hashlib.sha256(data).hexdigest()
        if digest!=source['decoded_pcm_sha256'][0]:
            raise SystemExit(f"{source['source_id']}: actual C decoded PCM hash mismatch")
        result['decoder_bytes'].append({'source_id':source['source_id'],'bytes':len(data),'sha256':digest})
    if a.full_midi:
        frames=manifest['sources'][0]['frames'][0]
        for key in range(128):
            hz=440.*2**((key-69)/12.)
            step=round(65536.*hz/48.)
            expected=(math.ceil((frames<<16)/step)+1)*2
            data=run(a.source_probe,0,step)
            if len(data)!=expected:
                raise SystemExit(f'MIDI {key}: pitch/duration mismatch; expected {expected}, got {len(data)} bytes')
        result['midi_keys_checked']=128
        # 13ED3 reaches below ordinary MIDI key 0, and its upper range is
        # bounded by the engine's 0.45 Fs limit. Check actual output duration,
        # not merely the prepared step, at both ends and the former 6 Hz floor.
        for step in (3092,8191,27095040):
            expected=(math.ceil((frames<<16)/step)+1)*2
            data=run(a.source_probe,0,step)
            if len(data)!=expected:
                raise SystemExit(f'non-octave step {step}: pitch/duration mismatch; expected {expected}, got {len(data)} bytes')
            result['non_octave_edge_steps_checked'].append(step)
    if a.output:
        a.output.parent.mkdir(parents=True,exist_ok=True)
        a.output.write_text(json.dumps(result,indent=2,sort_keys=True)+'\n')
    print(json.dumps(result,sort_keys=True))


if __name__=='__main__': main()
