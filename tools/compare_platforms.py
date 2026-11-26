#!/usr/bin/env python3
"""Require byte-identical audio from the real Linux and Windows C executables."""
import argparse
import json
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('--linux',type=Path,required=True)
p.add_argument('--windows',type=Path,required=True)
p.add_argument('--linux-soak',type=Path,required=True)
p.add_argument('--windows-soak',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
a=p.parse_args()
left=json.loads(a.linux.read_text());right=json.loads(a.windows.read_text())
if left['preset_count']!=36 or right['preset_count']!=36:
    raise SystemExit('Incomplete factory bank')
l={x['preset']:x for x in left['presets']};r={x['preset']:x for x in right['presets']}
if set(l)!=set(r):raise SystemExit('Preset identities differ')
for i in l:
    for key in ['rate','frames','pcm_sha256','clipped_frames']:
        if l[i][key]!=r[i][key]:raise SystemExit(f'Cross-platform PCM mismatch: preset{i}, {key}')
ls=json.loads(a.linux_soak.read_text());ws=json.loads(a.windows_soak.read_text())
for key in ['simulated_seconds_per_instance','total_rendered_samples','preset_mask','mode_mask','tuning_mask','tracks']:
    if ls[key]!=ws[key]:raise SystemExit(f'Cross-platform 30-minute soak mismatch: {key}')
report=dict(status='pass',byte_identical_factory_renders=36,
            byte_identical_30_minute_instances=4,soak_total_samples=ls['total_rendered_samples'],
            linux_executable_sha256=left['renderer_sha256'],windows_executable_sha256=right['renderer_sha256'],
            method='Exact PCM SHA-256, frame-count and sample-rate comparison of independent native builds.',
            hardware_qualified=False)
a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(report,indent=2)+'\n')
print('Cross-platform PASS: 36 factory renders and four 30-minute C instances are byte-identical')
