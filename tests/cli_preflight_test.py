"""Real CLI checks with synthetic firmware; no USB or serial hardware used."""
import pathlib
import subprocess
import sys
import tempfile
import unittest

BINARY = str(pathlib.Path(sys.argv.pop(1)).resolve())


def record(kind, address, data=b""):
    body = bytes((len(data), address >> 8, address & 255, kind)) + data
    return ":" + (body + bytes((-sum(body) & 255,))).hex() + "\n"


class PreflightTests(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory(prefix="newisp-preflight-")
        self.addCleanup(self.scratch.cleanup)
        self.path = pathlib.Path(self.scratch.name) / "image.hex"

    def invoke(self, command, text, *options):
        self.path.write_text(text, encoding="ascii")
        return subprocess.run([BINARY, command, "-f", str(self.path), *options],
                              capture_output=True, text=True, check=False)

    def test_offline_banks(self):
        text = (record(4, 0, b"\x00\xfe") + record(0, 0, b"\x11\x22") +
                record(4, 0, b"\x00\xff") + record(0, 0, b"\x33\x44") + record(1, 0))
        result = self.invoke("inspect", text)
        self.assertEqual(result.returncode, 0, result.stderr)
        for value in ("4 data bytes", "0x00FE0000", "0x00FF0000", "Programming unavailable", "Offline inspection only"):
            self.assertIn(value, result.stdout)
        # Invalid device guarantees this fails if preflight is moved after open.
        result = self.invoke("burn", text, "--device", "/device-must-not-open")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("banked firmware", result.stderr)
        self.assertEqual(result.stdout, "")

    def test_corruption_before_device(self):
        for text in (":0100000001FF\n:00000001ff\n", ":04\n", "junk\n"):
            with self.subTest(text=text):
                result = self.invoke("burn", text, "--device", "/device-must-not-open")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("HEX line 1", result.stderr)
                self.assertEqual(result.stdout, "")

    def test_valid_linear_image(self):
        result = self.invoke("inspect", record(0, 0xFFFF, b"\xA5") + record(1, 0))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("1 data bytes", result.stdout)
        self.assertIn("0x0000FFFF", result.stdout)
        self.assertIn("target capacity/options still require validation", result.stdout)

    def test_inspect_rejects_device_options(self):
        for options in (("--hid",), ("--device", "anything"), ("--freq", "35M"),
                        ("--eeprom", "0"), ("--family", "stc32")):
            with self.subTest(options=options):
                result = self.invoke("inspect", record(0, 0, b"\x01") + record(1, 0), *options)
                self.assertEqual(result.returncode, 2)
                self.assertIn("inspect is offline", result.stderr)

    def test_missing_input(self):
        result = subprocess.run([BINARY, "inspect"], capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn("requires a firmware image", result.stderr)


if __name__ == "__main__":
    unittest.main()
