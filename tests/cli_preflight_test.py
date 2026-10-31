"""Real CLI checks with synthetic firmware; no USB or serial hardware used.

The tool's output language is fixed when it is built (NEWISP_LANG in
CMakeLists.txt, zh-CN by default), so these assertions cannot expect English
text: they check behaviour instead. Every case below asserts on an exit code and
on something that is not prose -- a hex range, a byte count, a --quiet line --
which keeps the tests valid for a Chinese build, an English build, and any
language added later.

The one place prose matters is telling the "rejected by the offline preflight"
error apart from "the file could not be parsed", and that is done by which part
of the output the message went to and what else was printed.
"""
import pathlib
import shutil
import subprocess
import sys
import unittest
import uuid

BINARY = str(pathlib.Path(sys.argv.pop(1)).resolve())


def record(kind, address, data=b""):
    body = bytes((len(data), address >> 8, address & 255, kind)) + data
    return ":" + (body + bytes((-sum(body) & 255,))).hex() + "\n"


class PreflightTests(unittest.TestCase):
    def setUp(self):
        # A directory beside the binary rather than one under tempfile: some
        # sandboxes create temp directories but deny writing inside them, which
        # turns every case into an environment failure instead of a test
        # result. The build tree is writable whenever the build is.
        root = pathlib.Path(BINARY).parent / "cli_preflight_scratch"
        self.scratch = root / uuid.uuid4().hex
        self.scratch.mkdir(parents=True)
        self.addCleanup(shutil.rmtree, self.scratch, True)
        self.path = self.scratch / "image.hex"

    def invoke(self, command, text, *options):
        # The image is the first positional argument, right after the command:
        # "newisp inspect image.hex", "newisp burn image.hex -d ...".
        self.path.write_text(text, encoding="ascii")
        return subprocess.run([BINARY, command, str(self.path), *options],
                              capture_output=True, text=True, check=False)

    def test_offline_banks(self):
        text = (record(4, 0, b"\x00\xfe") + record(0, 0, b"\x11\x22") +
                record(4, 0, b"\x00\xff") + record(0, 0, b"\x33\x44") + record(1, 0))
        result = self.invoke("inspect", text)
        self.assertEqual(result.returncode, 0, result.stderr)
        # Addresses are hex digits, so they survive every translation. The
        # banked image is valid HEX and its ranges must be laid out as given.
        self.assertIn("0x00FE0000", result.stdout)
        self.assertIn("0x00FF0000", result.stdout)

        # Invalid device guarantees this fails if preflight is moved after open.
        # The binary-format diagnosis mentions the 16-bit writer; that token is
        # an identifier, not prose, so it is stable across languages.
        result = self.invoke("burn", text, "-device", "/device-must-not-open")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("16-bit", result.stderr)
        self.assertEqual(result.stdout, "")

    def test_corruption_before_device(self):
        for text in (":0100000001FF\n:00000001ff\n", ":04\n", "junk\n"):
            with self.subTest(text=text):
                result = self.invoke("burn", text, "-device", "/device-must-not-open")
                self.assertNotEqual(result.returncode, 0)
                # The parser reports the line number; the wording around it is
                # translated, the number is not.
                self.assertIn("HEX line 1", result.stderr)
                self.assertEqual(result.stdout, "")

    def test_valid_linear_image(self):
        result = self.invoke("inspect", record(0, 0xFFFF, b"\xA5") + record(1, 0))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("0x0000FFFF", result.stdout)
        # "target capacity/options still require validation" is the English
        # wording; the Chinese build says the same thing in Chinese, so check
        # for the stable technical token instead.
        self.assertIn("16-bit", result.stdout)

    def test_inspect_rejects_device_options(self):
        for options in (("-H",), ("-device", "anything"), ("-freq", "35M"),
                        ("-eeprom", "0"), ("-family", "stc32")):
            with self.subTest(options=options):
                result = self.invoke("inspect", record(0, 0, b"\x01") + record(1, 0), *options)
                self.assertEqual(result.returncode, 2)
                # Rejected during argument parsing: exit code 2, a message on
                # stderr, and no firmware was inspected.
                self.assertNotEqual(result.stderr, "")
                self.assertNotIn("0x00000000", result.stdout)

    def test_missing_input(self):
        # The image is a required positional, so omitting it is a parse error
        # naming the command rather than anything about a flag.
        for command in ("inspect", "burn"):
            with self.subTest(command=command):
                result = subprocess.run([BINARY, command],
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 2)
                self.assertIn(command, result.stderr)

    def test_image_is_positional(self):
        # The old -f / -file spellings are gone; the image has to follow the
        # command directly.
        for flag in ("-f", "-file"):
            with self.subTest(flag=flag):
                self.path.write_text(record(0, 0, b"\x01") + record(1, 0),
                                     encoding="ascii")
                result = subprocess.run([BINARY, "inspect", flag, str(self.path)],
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 2)
                self.assertNotIn("0x00000000", result.stdout)

        # And the positional form works for both commands that take an image.
        result = self.invoke("inspect", record(0, 0, b"\x01") + record(1, 0))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("0x00000000", result.stdout)


if __name__ == "__main__":
    unittest.main()
