"""Dependency-light validation of the checked-in source bank and recipe gate."""
from pathlib import Path
import hashlib
import importlib.util
import json
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("compile_sources", ROOT / "tools/compile_sources.py")
compiler = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compiler)


class SourceCompilerTests(unittest.TestCase):
    def test_checked_bank_integrity(self):
        manifest = json.loads((ROOT / "assets/source_bank_manifest.json").read_text())
        source = (ROOT / "src/source_bank_data.inc").read_bytes()
        self.assertEqual(hashlib.sha256(source).hexdigest(), manifest["generated_sha256"])
        self.assertEqual(manifest["upstream_commit"], compiler.PIN)
        self.assertEqual(manifest["recovered_payload_sha256"], compiler.PAYLOAD_SHA)
        self.assertEqual(manifest["sources"][0]["source_pcm_sha256"], compiler.LOCKED_SHA)
        self.assertEqual(len(manifest["sources"]), 7)
        self.assertLess(manifest["pcm_flash_bytes"], 190000)
        for item in manifest["sources"]:
            self.assertGreater(item["full_image_measurements"]["snr_db"], 12.)
            self.assertGreater(item["full_image_measurements"]["correlation"], .97)
            self.assertGreater(item["loop_end"], item["loop_start"]+512)
            self.assertLess(item["loop_end"], item["frames"][0])

    def test_recipe_rejects_relabelled_family(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp) / "recipe.json"
            for entry in ({"source_id": "unknown"},
                          {"source_id": "zaag.bloom-bark", "synth_overrides": {"drive_db": 1}},
                          {"source_id": "locked_bloom", "synth_overrides": {"drive_db": 1}},
                          {"source_id": "locked_bloom", "synth_overrides": {"f0_hz": 72}},
                          {"source_id": "locked_bloom", "script": "untrusted.py"}):
                p.write_text(json.dumps({"version": 1, "sources": [entry]}))
                with self.assertRaises(ValueError): compiler.read_spec(p)

    def test_custom_source_has_explicit_separate_identity(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp) / "custom.json"
            entry = {"source_id": "custom.legacy", "synth_overrides": {"drive_db": 9.0}}
            p.write_text(json.dumps({"version": 1, "sources": [entry]}))
            self.assertEqual(compiler.read_spec(p), [entry])

    def test_quantizer_rejects_out_of_range_instead_of_normalizing(self):
        with self.assertRaises(ValueError): compiler.quantize([0., 9., -1.])
        self.assertEqual(compiler.quantize([1., -1.]).tolist(), [4096, -4096])


if __name__ == "__main__": unittest.main()
