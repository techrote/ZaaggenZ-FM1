"""Simulated console transport tests; no connected hardware is accessed."""
import importlib.util
from pathlib import Path
from unittest import mock
import unittest

spec = importlib.util.spec_from_file_location("fune_transport", Path(__file__).resolve().parents[1] / "tools/fune_transport.py")
transport = importlib.util.module_from_spec(spec)
spec.loader.exec_module(transport)


class Serial:
    def __init__(self, chunks):
        self.chunks = list(chunks)
        self.written = []

    def write(self, data):
        self.written.append(data)

    def read_until(self, delimiter, size):
        assert delimiter == b"\n" and size <= 4096
        return self.chunks.pop(0) if self.chunks else b""


class TransportTests(unittest.TestCase):
    def test_short_ack_returns_without_256_byte_read(self):
        serial = Serial([b"ok n=27\r\n"])
        self.assertEqual(transport.ask(serial, "plugdata 00"), "ok n=27")
        self.assertEqual(serial.written, [b"plugdata 00\r"])

    def test_only_first_reply_line_is_consumed(self):
        serial = Serial([b"plugs n=1 live=1\r\n", b"plug 1 uid=zgf1\r\n"])
        self.assertEqual(transport.ask(serial, "plugs"), "plugs n=1 live=1")
        self.assertEqual(serial.chunks, [b"plug 1 uid=zgf1\r\n"])

    def test_partial_reads_are_assembled(self):
        serial = Serial([b"ok base=", b"0x02093000 ram=0x01c08000", b"\r\n"])
        self.assertEqual(transport.ask(serial, "plugput 270956 1792"), "ok base=0x02093000 ram=0x01c08000")

    def test_large_image_erase_budget_and_bound(self):
        self.assertEqual(transport.response_timeout("plugput 270956 1792", 8), 38.5)
        self.assertEqual(transport.response_timeout("plugput 4096 100", 8), 8)
        self.assertEqual(transport.response_timeout("plugput 999999999 1", 8), 45)
        self.assertEqual(transport.response_timeout("plugerase 1", 5), 41.5)
        self.assertEqual(transport.response_timeout("stat", 1.5), 1.5)

    def test_timeout_or_overlong_incomplete_ack_cannot_succeed(self):
        serial = Serial([b"ok n=27"])
        with mock.patch.object(transport.time, "monotonic", side_effect=[0, 0, 2]):
            self.assertEqual(transport.ask(serial, "plugdata 00"), "")
        serial = Serial([b"a" * 4096])
        self.assertEqual(transport.ask(serial, "stat"), "")


if __name__ == "__main__":
    unittest.main()
