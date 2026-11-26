#!/usr/bin/env python3
"""Package real compiled executables and measured audio, with source/file hashes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile
from write_audition_index import write_index

ROOT = Path(__file__).resolve().parents[1]


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--out', type=Path, default=ROOT/'dist')
    p.add_argument('--kind', choices=['windows', 'linux', 'fm1'], required=True)
    p.add_argument('--audio', type=Path)
    p.add_argument('--comparison', type=Path)
    p.add_argument('--presets', type=Path)
    p.add_argument('--songs', type=Path)
    p.add_argument('--source-manifest', type=Path,
                   help='Compiler manifest for a custom bank; required for custom FM-1 packages')
    args = p.parse_args()
    revision = subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    source_tree_clean = not subprocess.check_output(
        ['git','status','--porcelain','--untracked-files=normal'],cwd=ROOT,text=True).strip()
    source_manifest = args.source_manifest or ROOT/'assets/source_bank_manifest.json'
    provenance = json.loads(source_manifest.read_text())
    bank_kind = 'custom' if args.source_manifest else 'factory'
    if args.kind == 'fm1':
        if not (args.build/'zaaggenz.kpl').exists(): raise SystemExit('Missing compiled FM-1 plugin')
        target = json.loads((args.build/'zaaggenz.json').read_text())
        if target['source_bank']['kind'] == 'custom' and args.source_manifest is None:
            raise SystemExit('Custom FM-1 delivery requires --source-manifest from that compiler run')
        if provenance['generated_sha256'] != target['source_bank']['sha256']:
            raise SystemExit('Source provenance does not match the bank compiled into this plugin')
        if hashlib.sha256((args.build/'zaaggenz.kpl').read_bytes()).hexdigest() != target['artifact_sha256']:
            raise SystemExit('Plugin bytes differ from their target qualification report')
        bank_kind = target['source_bank']['kind']
    elif (args.build/'CMakeCache.txt').is_file():
        for line in (args.build/'CMakeCache.txt').read_text().splitlines():
            if line.startswith('ZG_SOURCE_BANK_FILE:FILEPATH=') and line.split('=',1)[1]:
                bank_kind = 'custom'
                if args.source_manifest is None:
                    raise SystemExit('Custom desktop delivery requires --source-manifest from that compiler run')
                generated=Path(line.split('=',1)[1])
                if hashlib.sha256(generated.read_bytes()).hexdigest() != provenance['generated_sha256']:
                    raise SystemExit('Custom source manifest differs from the configured source bank')
    suffix = '-bank-' + provenance['generated_sha256'][:8] if bank_kind == 'custom' else ''
    name = f'ZaaggenZ-FM1-{args.kind}-{revision[:8]}{suffix}'
    dest = args.out / name
    if dest.exists(): shutil.rmtree(dest)
    dest.mkdir(parents=True)
    if args.kind == 'fm1':
        for f in args.build.glob('zaaggenz*'):
            if f.is_file(): shutil.copy2(f,dest/f.name)
        # Installer dependencies remain at their original relative locations.
        shutil.copytree(ROOT/'third_party/fune/platforms',dest/'third_party/fune/platforms')
        shutil.copytree(ROOT/'third_party/fune/tools',dest/'third_party/fune/tools')
        shutil.copytree(ROOT/'third_party/fune/src',dest/'third_party/fune/src')
        shutil.copy2(ROOT/'third_party/fune/UPSTREAM.json',dest/'third_party/fune/UPSTREAM.json')
        (dest/'tools').mkdir()
        for n in ['install_fune.py','fune_slots.py','fune_transport.py','observe_fm1.py']:
            shutil.copy2(ROOT/'tools'/n,dest/'tools'/n)
    else:
        executable='zaag-render.exe' if args.kind=='windows' else 'zaag-render'
        candidates=[args.build/executable,args.build/'Release'/executable]
        renderer=next((f for f in candidates if f.exists()),None)
        if renderer is None: raise SystemExit('Missing compiled renderer')
        shutil.copy2(renderer,dest/executable)
        if args.kind=='windows':
            player=next((f for f in [args.build/'ZaaggenZ-Audition.exe',args.build/'Release/ZaaggenZ-Audition.exe'] if f.exists()),None)
            if player is None: raise SystemExit('Missing actual Windows native audition executable')
            shutil.copy2(player,dest/player.name)
            (dest/'Start-Audition.cmd').write_bytes(b'@echo off\r\ncd /d "%~dp0"\r\nstart "" "ZaaggenZ-Audition.exe"\r\n')
            (dest/'Render-Example.cmd').write_bytes(b'@echo off\r\ncd /d "%~dp0"\r\nzaag-render.exe --preset 0 --pattern riff --seconds 8 --out Elastic-Bloom.wav\r\npause\r\n')
    for src,sub in [(args.audio,'auditions'),(args.comparison,'architecture-comparison'),(args.presets,'fune-presets'),(args.songs,'fune-songs')]:
        if src:
            if not src.is_dir(): raise SystemExit(f'Missing deliverable directory: {src}')
            shutil.copytree(src,dest/sub,ignore=shutil.ignore_patterns('recovered','*.pyc','__pycache__'))
    for f in ['README.md','LICENSE','THIRD_PARTY_NOTICES.md']:
        shutil.copy2(ROOT/f,dest/f)
    shutil.copytree(ROOT/'docs',dest/'docs')
    shutil.copytree(ROOT/'presets',dest/'preset-recipes')
    shutil.copy2(source_manifest,dest/'source-bank-provenance.json')
    shutil.copy2(ROOT/'third_party/fune/LICENSE',dest/'Fune-MPL-2.0.txt')
    shutil.copy2(ROOT/'third_party/fune/sdk/LICENSE',dest/'Fune-SDK-MIT.txt')
    write_index(dest)
    files={str(f.relative_to(dest)).replace(os.sep,'/'):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(dest.rglob('*')) if f.is_file()}
    (dest/'MANIFEST.json').write_text(json.dumps(dict(source_revision=revision,source_tree_clean=source_tree_clean,kind=args.kind,hardware_qualified=False,
        source_bank=dict(kind=bank_kind,generated_sha256=provenance['generated_sha256'],sources=len(provenance['sources'])),
        note='Target compilation and host qualification do not establish physical FM-1 timing or installation success.',files=files),indent=2)+'\n')
    archive=args.out/(name+'.zip')
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for f in sorted(dest.rglob('*')):
            if f.is_file():z.write(f,f.relative_to(args.out))
    print(json.dumps(dict(artifact=str(archive),bytes=archive.stat().st_size,sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),revision=revision)))


if __name__=='__main__':main()
