#!/usr/bin/env python3
"""Qualify the extracted delivery, including paths containing spaces.

This opens no device and installs nothing. The Windows smoke opens the native
application, exercises controls/MIDI handlers and exports real core audio.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import struct
import subprocess
import sys
import wave
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def pe_imports(path: Path) -> list[str]:
    """Read ordinary PE imports without requiring a compiler or Python package."""
    data = path.read_bytes()
    if data[:2] != b'MZ': raise ValueError(f'Not a Windows executable: {path}')
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe+4] != b'PE\0\0': raise ValueError('Missing PE signature')
    count = struct.unpack_from('<H', data, pe+6)[0]
    opt_size = struct.unpack_from('<H', data, pe+20)[0]
    opt = pe+24
    magic = struct.unpack_from('<H', data, opt)[0]
    directories = opt + (112 if magic == 0x20b else 96)
    import_rva = struct.unpack_from('<I', data, directories+8)[0]
    sections = []
    for index in range(count):
        at = opt+opt_size+index*40
        virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from('<IIII', data, at+8)
        sections.append((virtual_address, max(virtual_size, raw_size), raw_pointer))
    def offset(rva):
        for start, size, raw in sections:
            if start <= rva < start+size: return raw+rva-start
        raise ValueError(f'Unmapped PE address: {rva}')
    if not import_rva: return []
    at = offset(import_rva)
    names = []
    while any(data[at:at+20]):
        name_rva = struct.unpack_from('<I', data, at+12)[0]
        name_at = offset(name_rva)
        names.append(data[name_at:data.index(b'\0', name_at)].decode('ascii'))
        at += 20
    return sorted(names)


def run(args, cwd):
    result = subprocess.run([str(x) for x in args], cwd=cwd, text=True,
                            capture_output=True, timeout=30)
    if result.returncode:
        raise RuntimeError(f'{Path(str(args[0])).name} failed ({result.returncode}):\n'
                           f'{result.stdout}\n{result.stderr}')
    return result.stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--kind', choices=['linux', 'windows', 'fm1'], required=True)
    parser.add_argument('--archive', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    archive = args.archive or ROOT/'dist'/f'ZaaggenZ-FM1-{args.kind}-{revision[:8]}.zip'
    destination = ROOT/'build'/'Delivery extraction with spaces'/args.kind
    if destination.exists(): shutil.rmtree(destination)
    destination.mkdir(parents=True)
    with zipfile.ZipFile(archive) as z:
        for item in z.infolist():
            path = PurePosixPath(item.filename)
            if path.is_absolute() or '..' in path.parts:
                raise ValueError('Unsafe archive path')
        z.extractall(destination)
        if os.name != 'nt':
            for item in z.infolist():
                mode = (item.external_attr >> 16) & 0o777
                if mode and not item.is_dir(): (destination/item.filename).chmod(mode)
    manifests = list(destination.glob('*/MANIFEST.json'))
    if len(manifests) != 1: raise ValueError('Expected one complete delivery')
    manifest = json.loads(manifests[0].read_text())
    directory = manifests[0].parent
    if manifest['source_revision'] != revision or manifest['kind'] != args.kind:
        raise ValueError('Package revision or kind differs from checked out source')
    if os.environ.get('CI') == 'true' and not manifest.get('source_tree_clean', False):
        raise ValueError('CI delivery was packaged from a dirty source tree')
    actual = {p.relative_to(directory).as_posix() for p in directory.rglob('*') if p.is_file()}
    if actual != set(manifest['files']) | {'MANIFEST.json'}:
        raise ValueError('Package contains missing or unmanifested files')
    for name, expected in manifest['files'].items():
        if hashlib.sha256((directory/name).read_bytes()).hexdigest() != expected:
            raise ValueError(f'Package hash mismatch: {name}')
    report = dict(status='pass', kind=args.kind, source_revision=revision,
                  source_tree_clean=manifest.get('source_tree_clean', False),
                  archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),
                  verified_files=len(manifest['files']), path_with_spaces=True,
                  physical_hardware_qualified=False)
    if args.kind == 'fm1':
        report['offline_plugin_check'] = run([sys.executable, 'tools/install_fune.py',
                                             'check', 'zaaggenz.kpl'], directory).strip()
        run([sys.executable, 'tools/observe_fm1.py', '--help'], directory)
        for folder, key, count in [('fune-presets', 'presets', 36), ('fune-songs', 'songs', 6)]:
            assets = json.loads((directory/folder/'manifest.json').read_text())[key]
            if len(assets) != count: raise ValueError(f'Incomplete {folder}')
            report[key] = count
    else:
        exe = directory/('zaag-render.exe' if args.kind == 'windows' else 'zaag-render')
        report['renderer'] = json.loads(run([exe, '--preset', '0', '--pattern', 'hold',
                                            '--seconds', '1', '--out', 'package-render.wav'], directory).splitlines()[-1])
        with wave.open(str(directory/'package-render.wav'), 'rb') as wav:
            if wav.getnchannels() != 1 or wav.getframerate() != 44100 or wav.getnframes() <= 44100:
                raise ValueError('Incomplete package audio or release tail')
        if args.kind == 'windows':
            imports = {p.name: pe_imports(p) for p in [exe, directory/'ZaaggenZ-Audition.exe']}
            for names in imports.values():
                if any(name.upper().startswith(('VCRUNTIME', 'MSVCP', 'CONCRT')) for name in names):
                    raise ValueError('Portable evaluator unexpectedly requires a VC runtime DLL')
            report['windows_imports'] = imports
            run([directory/'ZaaggenZ-Audition.exe', '--smoke'], directory)
            smoke = json.loads((directory/'windows-smoke.json').read_text())
            for key in ['native_window_created', 'core_rendered', 'control_handlers_pass',
                        'keyboard_and_midi_logic_pass', 'wav_exported']:
                if not smoke[key]: raise ValueError(f'Extracted Windows smoke failed: {key}')
            report['extracted_native_ui'] = smoke
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2)+'\n')
    print(f'{args.kind} extracted delivery PASS: {report["verified_files"]} hashed files; {revision[:12]}')


if __name__ == '__main__': main()
