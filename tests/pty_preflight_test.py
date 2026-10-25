"""Prove that a detected STC32 is rejected before clock/erase/write commands.

Reuses the existing PTY emulator; this is not hardware protocol validation.
"""
import os
import pathlib
import pty
import subprocess
import sys
import tempfile
import pty_test

binary = str(pathlib.Path(sys.argv[1]).resolve())
pty_test.MAGIC = 0xF7E3  # STC32G12K128, not the emulator's STC8H default.
master, slave = pty.openpty()
chip = pty_test.FakeChip(master)
try:
    chip.start()
    with tempfile.TemporaryDirectory(prefix="newisp-stc32-preflight-") as scratch:
        image = os.path.join(scratch, "synthetic.hex")
        pty_test.make_hex(image)
        result = subprocess.run([binary, "burn", "-f", image, "-d", os.ttyname(slave)],
                                capture_output=True, text=True, timeout=15)
        assert result.returncode != 0, result.stdout
        assert "STC32 programming is unavailable" in result.stdout, result.stdout + result.stderr
        assert chip.saw_wakeup
        assert not chip.erased
        assert not chip.blocks
        assert chip.options is None
        # Not even the calibration/prepare exchanges may precede this refusal.
        assert not [entry for entry in chip.log if entry[0] == "rx"], chip.log
        print("PASS: detected STC32 refused before calibration, erase, program, options or reset")
finally:
    chip.stop()
    chip.join(timeout=2)
    os.close(slave)
    os.close(master)
