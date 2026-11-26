#!/usr/bin/env python3
"""Regenerate the factory bank and qualify a real RenderRecipe custom C bank.

All generated files stay beneath --work. The reference and checked-in factory
assets are read-only. Requires CMake, a native C compiler and the pinned numeric
dependencies in requirements-reference.txt. No FM-1 device is accessed.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(arguments: list[str | Path]) -> None:
    environment = os.environ.copy()
    environment['PYTHONDONTWRITEBYTECODE'] = '1'
    subprocess.run([str(value) for value in arguments], cwd=ROOT,
                   env=environment, check=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--work', type=Path,
                        default=ROOT/'build/source-compiler-qualification')
    parser.add_argument('--out', type=Path)
    parser.add_argument('--strict-toolchain', action='store_true',
                        help='Also require identical tool-version provenance bytes')
    args = parser.parse_args()
    reference = args.reference.resolve()
    work = args.work.resolve()
    if work == ROOT or work == reference or reference in work.parents:
        parser.error('--work must be a separate output directory outside the reference')
    work.mkdir(parents=True, exist_ok=True)
    compiler = ROOT/'tools/compile_sources.py'
    expected_manifest_path = ROOT/'assets/source_bank_manifest.json'
    expected_bank_path = ROOT/'src/source_bank_data.inc'
    original_inputs = {str(path): sha(path)
                       for path in (expected_manifest_path, expected_bank_path)}
    factory = work/'factory'
    custom = work/'custom'
    common = [sys.executable, compiler, '--reference', reference,
              '--work', work/'recovered-runtime']

    run([*common, '--spec', ROOT/'assets/source_recipes.json',
         '--output', factory/'source_bank_data.inc',
         '--manifest', factory/'source_bank_manifest.json',
         '--wav-dir', factory/'auditions'])
    if expected_bank_path.read_bytes() != (factory/'source_bank_data.inc').read_bytes():
        raise RuntimeError('Factory source bytes differ from authenticated regeneration')
    expected_manifest = json.loads(expected_manifest_path.read_text())
    regenerated_manifest = json.loads((factory/'source_bank_manifest.json').read_text())
    exact_manifest = expected_manifest_path.read_bytes() == (factory/'source_bank_manifest.json').read_bytes()
    provenance_fields = {'python', 'numpy', 'scipy'}
    for manifest in (expected_manifest, regenerated_manifest):
        if set(manifest.get('toolchain', {})) != provenance_fields:
            raise RuntimeError('Unexpected toolchain provenance fields; explicit review required')
    expected_content = {k: v for k, v in expected_manifest.items() if k != 'toolchain'}
    regenerated_content = {k: v for k, v in regenerated_manifest.items() if k != 'toolchain'}
    if expected_content != regenerated_content:
        raise RuntimeError('Factory manifest content differs beyond numeric-tool version provenance')
    if args.strict_toolchain and not exact_manifest:
        raise RuntimeError('Full manifest bytes differ: use the recorded numeric toolchain or omit --strict-toolchain')

    recipe = ROOT/'examples/locked_bloom_render_recipe.json'
    run([*common, '--render-recipe', recipe,
         '--output', custom/'source_bank_data.inc',
         '--manifest', custom/'source_bank_manifest.json',
         '--wav-dir', custom/'auditions'])
    custom_manifest = json.loads((custom/'source_bank_manifest.json').read_text())
    if len(custom_manifest['sources']) != 1:
        raise RuntimeError('The reference single-event recipe did not produce one source')
    if custom_manifest['sources'][0]['source_pcm_sha256'] != expected_manifest['sources'][0]['source_pcm_sha256']:
        raise RuntimeError('The real RenderRecipe failed to preserve the protected source')

    native = work/'custom-native'
    run(['cmake', '-S', ROOT, '-B', native, '-DCMAKE_BUILD_TYPE=Release',
         '-DZG_BUILD_FUNE_HOST=OFF',
         f'-DZG_SOURCE_BANK_FILE={(custom/"source_bank_data.inc").as_posix()}'])
    run(['cmake', '--build', native, '--config', 'Release', '--parallel', '4',
         '--target', 'source-probe', 'test-source-bank'])
    run(['ctest', '--test-dir', native, '-C', 'Release', '-R', '^source-bank$',
         '--output-on-failure'])
    executable = 'source-probe.exe' if os.name == 'nt' else 'source-probe'
    probe = next((path for path in (native/'Release'/executable, native/executable)
                  if path.is_file()), None)
    if probe is None:
        raise RuntimeError('CMake completed without the requested native source probe')
    decoder_receipt = work/'custom-decoder.json'
    run([sys.executable, ROOT/'tools/validate_source_bank.py', '--source-probe', probe,
         '--manifest', custom/'source_bank_manifest.json', '--full-midi',
         '--output', decoder_receipt])
    decoded = json.loads(decoder_receipt.read_text())
    if any(sha(Path(path)) != value for path, value in original_inputs.items()):
        raise RuntimeError('Qualification unexpectedly changed a checked-in factory input')

    receipt = {
        'status': 'PASS',
        'qualification': 'Actual compiler CLI, real RenderRecipe, native CMake C decoder; no hardware claim',
        'compiler_sha256': sha(compiler),
        'source_decoder_sha256': sha(ROOT/'src/source_bank.c'),
        'upstream_commit': regenerated_manifest['upstream_commit'],
        'factory': {
            'generated_sha256': sha(factory/'source_bank_data.inc'),
            'identical_generated_bytes': True,
            'identical_manifest_bytes': exact_manifest,
            'identical_manifest_content_excluding_toolchain': True,
            'recorded_manifest_sha256': sha(expected_manifest_path),
            'regenerated_manifest_sha256': sha(factory/'source_bank_manifest.json'),
            'allowed_provenance_differences': sorted('toolchain.'+key for key in provenance_fields),
            'recorded_toolchain': expected_manifest['toolchain'],
            'observed_toolchain': regenerated_manifest['toolchain'],
            'mip_count': regenerated_manifest['mip_count'],
            'total_asset_flash_bytes': regenerated_manifest['total_asset_flash_bytes'],
        },
        'custom': {
            'recipe_file_sha256': sha(recipe),
            'generated_sha256': sha(custom/'source_bank_data.inc'),
            'total_asset_flash_bytes': custom_manifest['total_asset_flash_bytes'],
            'source_bank_ctest_pass': True,
            'decoder_conformance': decoded,
        },
        'checked_in_factory_inputs_unchanged': True,
        'physical_hardware_qualification': False,
    }
    output = (args.out or work/'qualification.json').resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes((json.dumps(receipt, indent=2, sort_keys=True)+'\n').encode('utf-8'))
    print(json.dumps({'status': 'PASS', 'receipt': str(output),
                      'factory_manifest_byte_identical': exact_manifest}))


if __name__ == '__main__':
    main()
