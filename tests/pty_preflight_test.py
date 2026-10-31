"""Prove that STC32 and the 8051U take the same path through the writer.

Reuses the PTY emulator from pty_test.py; this drives the real binary against a
scripted fake chip, so it exercises framing and the burn sequence but is not a
substitute for hardware.

History: this test used to assert the opposite -- that a detected STC32 was
refused before calibration, erase or option programming. That refusal was
removed once it became clear the two families share one implementation
(ProtocolStc32, serial and HID) and differ only in flash size, so what the test
checks now is that an STC32 runs the sequence to completion like an 8051U does.
"""
import os
import pathlib
import pty
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pty_test  # noqa: E402

binary = str(pathlib.Path(sys.argv[1]).resolve())

# STC32G12K128 instead of the emulator's STC8H default, so the part name
# resolves to a chip starting with "STC32".
pty_test.MAGIC = 0xF7E3

master, slave = pty.openpty()
chip = pty_test.FakeChip(master)
try:
    chip.start()
    with tempfile.TemporaryDirectory(prefix="newisp-stc32-") as scratch:
        image = os.path.join(scratch, "synthetic.hex")
        pty_test.make_hex(image)
        result = subprocess.run(
            [binary, "burn", image, "-d", os.ttyname(slave)],
            capture_output=True, text=True, timeout=60)

        # The burn runs to the end: erase, blocks and option bytes are all
        # written, and the chip is released afterwards.
        assert result.returncode == 0, result.stdout + result.stderr
        assert chip.saw_wakeup, "the fake chip never received a wakeup byte"
        assert chip.erased, "erase was not issued"
        assert chip.blocks, "no code blocks were written"
        assert chip.options is not None, "option bytes were not written"

        # Nothing about the old refusal may survive: a stale message here would
        # mean the gate is still in the code path.
        assert "unavailable" not in result.stdout

        print("PASS: STC32 programmed end to end (erase, blocks, options)")
finally:
    chip.stop()
    chip.join(timeout=2)
    os.close(slave)
    os.close(master)
