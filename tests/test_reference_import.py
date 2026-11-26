"""Optional authenticated-reference compiler integration (runs when clone exists)."""
from pathlib import Path
import importlib.util
import json
import os
import tempfile
import unittest
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = Path(os.environ.get("ZG_REFERENCE_PATH", ROOT / ".upstream/zaaggenz"))
spec = importlib.util.spec_from_file_location("source_compiler_import_test", ROOT / "tools/compile_sources.py")
compiler = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compiler)


@unittest.skipUnless((REFERENCE / ".git").exists(), "pinned reference checkout is optional")
class RenderRecipeImportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        compiler.recover(REFERENCE, Path(cls.temp.name) / "runtime")
        from uptempo_harmony.synth import PRESETS
        from zaaggenz_contracts.legacy import freeze_legacy
        cls.recipe = freeze_legacy(PRESETS['locked_bloom'].to_dict()).to_dict()

    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()

    def import_data(self, data):
        path = Path(self.temp.name) / "recipe.json"
        path.write_text(json.dumps(data))
        return compiler.import_render_recipe(path)

    def test_actual_render_recipe_reproduces_protected_source(self):
        entry = self.import_data(self.recipe)
        audio, _, diag = compiler.source_render(entry)
        self.assertEqual(compiler.pcm_sha(audio), compiler.LOCKED_SHA)
        self.assertTrue(entry['source_id'].startswith('compiled.'))
        self.assertEqual(diag['recipe_sha256'], entry['render_recipe_sha256'])

    def test_declared_graph_and_output_are_executed(self):
        d = json.loads(json.dumps(self.recipe))
        d['nodes'] = [dict(kind="DSPNodeSpec", version="1.0.0", id="trim",
            type_id="core.gain.v1", inputs=["source"], channels=1,
            params=dict(gain_db=-6.020599913279624), state_policy="stateless",
            phase_policy="source-derived", latency_samples=0, lookahead_samples=0,
            bypass="identity", automation=[])]
        d['output_node'] = 'trim'
        audio, _, diag = compiler.source_render(self.import_data(d))
        raw, _, _ = compiler.source_render({"source_id": "locked_bloom"})
        np.testing.assert_array_equal(audio, raw*.5)
        self.assertEqual(diag['graph_order'], ['trim'])

    def test_no_implicit_root_retuning(self):
        d = json.loads(json.dumps(self.recipe))
        d['source']['params']['f0_hz'] = 60.
        with self.assertRaisesRegex(ValueError, '48 Hz source'):
            self.import_data(d)

    def test_non_source_owner_domains_rejected(self):
        d = json.loads(json.dumps(self.recipe))
        from zaaggenz_contracts.legacy import freeze_legacy
        d = freeze_legacy(d['source']['params'], sculpt={}).to_dict()
        with self.assertRaisesRegex(ValueError, 'sculpt'):
            self.import_data(d)


if __name__ == '__main__': unittest.main()
