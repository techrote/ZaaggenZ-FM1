"""Synthetic parser/gate tests; these are not physical FM-1 measurements."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("observer", Path(__file__).resolve().parents[1] / "tools/observe_fm1.py")
observer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(observer)


def rows():
    baseline = "stat stage=11 up_ms=1000 audio_max_us=2000 audio_halves=172 audio_over=0 audio_on=1 audio_peak=4000 audio_same_half=0 audio_offs=0 vcap=6 playing=1 page=8:MIXER frame_mean_ms=12.345"
    a = observer.parse_stat(baseline)
    b = dict(a, up_ms=2000, audio_halves=344)
    return [{"stat": a}, {"stat": b}]


class ObserverTests(unittest.TestCase):
    def test_healthy_window_still_is_not_complete_hardware_qualification(self):
        report = observer.assess(rows(), [])
        self.assertFalse(report["reasons"])
        self.assertFalse(report["hardware_qualified"])
        self.assertEqual(report["host_guard_budget_us"], 5221)

    def test_missing_or_error_console_data_is_rejected(self):
        for text in ("", "err unknown command", "stat up_ms=100"):
            with self.assertRaises(ValueError):
                observer.parse_stat(text)

    def test_each_failure_independently_rejects_window(self):
        for key, value in (("up_ms", 0), ("audio_halves", 172), ("audio_on", 0),
                           ("playing", 0), ("vcap", 5), ("audio_over", 1),
                           ("audio_offs", 1), ("audio_same_half", 1), ("audio_max_us", 5221)):
            sample = rows()
            sample[1]["stat"][key] = value
            with self.subTest(key=key):
                self.assertTrue(observer.assess(sample, [])["reasons"])

    def test_empty_incomplete_or_errored_window_never_passes(self):
        self.assertTrue(observer.assess([], [])["reasons"])
        self.assertTrue(observer.assess(rows()[:1], [])["reasons"])
        self.assertTrue(observer.assess(rows(), ["serial timeout"])["reasons"])


if __name__ == "__main__":
    unittest.main()
