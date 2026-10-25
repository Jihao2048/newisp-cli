# STC32 development status

This is a safety/preflight contribution, **not completed STC32 flash support**.
It extends the existing cross-platform tool, not a second flashing application.

## Reproduced problems

At upstream `4971163cf74af94b150c34f67a5bbd098004961f`, the HEX parser did not
validate checksums, accepted truncated records and missing EOF, silently merged
overlapping addresses and retained data when a reused output object failed.
Several original positive test fixtures consequently had incorrect checksums;
their payloads are unchanged and their checksum bytes have been corrected.

The programming code serializes only two address bytes, while the parser
previously materialized a vector from zero through the highest address. An
MCS-251 image at `0xFE0000` / `0xFF0000` therefore became a large zero-based
programming request whose addresses wrapped. Parsing now preserves sparse
absolute addresses; `BuildLinearFirmware` refuses rather than remapping them.

STC32G12K128 status detection (magic `0xF7E3`) does not establish support for
banked programming. `RunBurn` rejects STC32 model names before destructive or
clock-setting commands. This includes artificially flattened input: stripping
the HEX bank records is not a workaround. No experimental override is supplied.
The normal non-STC32 path additionally checks the existing model-capacity
estimate and requested EEPROM split. That estimate is not flash readback and
this patch does not establish a complete per-part memory-map database.

## Still required before STC32 writes can be enabled

1. A documented or captured, successful factory **USB HID** exchange for the
   exact device and bootloader: address/bank selection, first/continuation block
   commands, block length and acknowledgements, including the bank boundary.
2. The option-block layout, with independently checked preservation of reset,
   watchdog, clock and EEPROM settings. A trim table for AI8051U is not evidence
   for STC32G12K128. Erase effects on those options must also be established.
3. Explicit mapping from the compiler's MCS-251 addresses to wire addresses;
   tests must cover both banks, gaps, final partial blocks and wrong responses.
4. A recoverable hardware test using a known-good image and a recovery route,
   followed by verification of the running program. An ACK alone is not a
   readback of all programmed bytes.

No device flash, option programming or recovery test was performed for this
contribution. A programmer can inspect valid firmware even while its programming
capability is unavailable. The inspect command never opens a transport.

## Research references and limits

- [Manufacturer STC32G12K128 page](https://www.stcmicro.com/stc/stc32g12k128.html):
  device capabilities; not a complete USB ISP wire contract.
- [stcgal](https://github.com/grigorig/stcgal): useful UART implementation;
  its advertised USB support does not establish STC32 factory HID bank handling.
- [stc8usb.py](https://github.com/wagiminator/MCU-Flash-Tools/blob/main/stc8usb.py):
  STC8H devices, not verification of STC32G12K128's two banks.
- [STC32 HID example](https://gist.github.com/robinkrens/47a98e58087623059d809175a30b0f42):
  author reports only STC32F12K54 testing, no error checking, two address bytes.
  It is not used as a verified two-bank implementation or copied into this code.

Only synthetic fixtures are included. User firmware, device identifiers beyond
the public chip model/magic, and private captures are not part of this repository.

## Verification

`cmake -S . -B build -DNEWISP_BUILD_TESTS=ON`, build, then `ctest --test-dir build
--output-on-failure` runs the core and real CLI preflight tests. On POSIX it also
runs a PTY test proving a detected STC32 is refused before calibration, erase,
programming, options or reset. The existing `tests/pty_test.py` separately
exercises the unchanged supported STC8H UART sequence. Neither emulates a
verified STC32 USB bootloader. Python 3 is required only when building tests.
