# newisp

A cross-platform command-line programmer (flasher) for **STC 8051-family microcontrollers**.

> **Chinese documentation**: see [README.zh-CN.md](README.zh-CN.md).

---

## Table of contents

- [Features](#features)
- [Supported chip families](#supported-chip-families)
- [Transports](#transports)
  - [Serial (UART)](#serial-uart)
  - [USB HID](#usb-hid)
- [Building](#building)
  - [Windows](#windows)
  - [Linux](#linux)
  - [macOS](#macos)
  - [HID is a hard dependency](#hid-is-a-hard-dependency)
  - [Output language (chosen at build time)](#output-language-chosen-at-build-time)
- [Encoding note](#encoding-note)
- [Usage](#usage)
  - [Commands](#commands)
  - [Options](#options)
  - [Examples](#examples)
  - [Nothing you did not ask to change](#nothing-you-did-not-ask-to-change)
  - [Migrating from the old command line](#migrating-from-the-old-command-line)
- [Testing](#testing)
  - [Unit tests (no hardware)](#unit-tests-no-hardware)
  - [Protocol test over a pseudo-terminal (no hardware)](#protocol-test-over-a-pseudo-terminal-no-hardware)
  - [Helper scripts](#helper-scripts)
- [Architecture](#architecture)
  - [Transport backends](#transport-backends)
- [Hardware notes](#hardware-notes)
  - [Clock range](#clock-range)
  - [Option bytes](#option-bytes)
  - [Options validated per family](#options-validated-per-family)
  - [Offline firmware check](#offline-firmware-check)
  - [Serial auto-reset](#serial-auto-reset)
- [Verification status](#verification-status)
  - [Hardware burn test (AI8051U34K64 over USB HID)](#hardware-burn-test-ai8051u34k64-over-usb-hid)
  - [The 8051U does have a serial ISP monitor](#the-8051u-does-have-a-serial-isp-monitor)
  - [A cross-platform bug that only hardware could expose](#a-cross-platform-bug-that-only-hardware-could-expose)
- [Known limitations](#known-limitations)
- [License](#license)

---

## Features

- **One code base, three platforms**: Windows, macOS and Linux, built with CMake.
- **Two transports**: the classic STC serial ISP monitor and the factory USB HID ISP interface.
- **Automatic identification**: a built-in table of more than 1300 chip IDs, and the protocol is
  chosen from whatever the probe identifies.
- **Measured IRC calibration** on the serial transport: the trim is derived from the chip's real
  frequency at handshake time rather than looked up.
- **Per-family validation**: every family has its own IRC frequency menu, and `-freq`, `-eeprom`
  and `-clock` are accepted only where the target family actually carries that field. Everywhere
  else they are refused with an explicit message instead of being written into an option bit that
  means something else.
- **No unrequested rewriting**: without `-freq` the chip keeps its current frequency, without
  `-eeprom` it keeps its current split, and the default output is quiet -- the tool no longer
  reconfigures an oscillator or an EEPROM partition on its own.
- **Checks before the burn**: strict HEX validation, and the image size, EEPROM split and target
  capacity are verified before anything is erased -- STC32 included.
- **No external dependency on Windows**: HID goes straight through the system `hid.dll` and
  SetupAPI; no hidapi, no vcpkg.
- **Scriptable**: a pure command-line tool with conventional exit codes (0 on success, non-zero on
  failure) and predictable default output, which makes it easy to drop into a build pipeline.

---

## Supported chip families

| Family | Parts |
| --- | --- |
| STC89 / STC90 | STC89C52RC, STC89C516RD, STC90C58RD and relatives; serial only |
| STC10 / STC11 / STC12 | STC12C5A60S2, STC12LE5A60S2, IAP11F06 and relatives (12A / 12B variants included); serial only |
| STC15 | STC15F104E, STC15W408AS, IAP15W4K58S4 and relatives (15A variant included); serial only |
| STC8 | STC8F, STC8A, STC8C, GX8S and relatives; serial and USB HID (probe-driven; the 8A silicon does carry USB) |
| STC8G | STC8G1K08, AI8G1K08, JX8G and relatives; serial and USB HID (the manual does not list USB; the probe decides) |
| STC8H | STC8H1K08 through STC8H8K64U, AI8H and relatives; serial and USB HID |
| STC32 | STC32G144K246 and relatives; serial and USB HID, same protocol as the 8051U; images up to 64 KiB |
| STC8051U | STC8051U34K64, AI8051U34K64 and relatives; serial and USB HID (the manual says "HID only", but the serial monitor answers -- see [below](#the-8051u-does-have-a-serial-isp-monitor)) |

> **Options are validated per family, not refused wholesale.** The tool no longer rejects a
> transport or an option on the strength of a family name; it asks whether that family *has* the
> field. If it does not, the option is an error; if it does, it is accepted (see
> [Hardware notes](#hardware-notes)). STC16 has been dropped from the part table: it can still be
> probed and its ID reported, but it has no part name.

---

## Transports

### Serial (UART)

**Every supported family can be programmed over serial**, which is the one transport that covers the
whole catalogue. Whether a given part can be used is decided by the probe, not by the manual or by
the family name.

The classic STC ISP monitor. The chip may still be running user code, so the tool:

1. **Pulses DTR low for 250 ms** as a reset pulse.
2. **Repeats the `0x7F` wakeup byte** (up to 500 rounds, 200 attempts each).
3. Once the chip answers, **calibrates the baud rate**: `0x00 0x08` runs a coarse sweep of the IRC
   frequency, `0x00 0x0C` refines it, and the resulting trim is sent with `0x01` before the link
   switches to the negotiated rate.

`burn` supports 2400 to 460800 bps (STC89 and STC12 stop at 115200 because of their 8-bit BRT
register). The `detect` handshake is fixed at 2400 bps and **does not use** the number after `-s`.

The vendor manual frames the 8051U family as "USB HID only", and early versions refused it on the
serial transport for that reason. The refusal is gone: a real 8051U answers on its serial ISP
monitor (see [below](#the-8051u-does-have-a-serial-isp-monitor)).

### USB HID

The factory USB ISP interface (`VID 0x34BF / PID 0x1001`), implemented by STC32, STC8H and 8051U
parts. STC8G is **not on the vendor USB list**, but that silicon **does carry a USB controller** -- `ProtocolStc8GHid` exists for the 08A variant of the range -- and the probe decides either way.

- **No wakeup needed**: holding BOOT while the board powers up already leaves the chip in its ISP
  monitor.
- **No baud negotiation**: the USB link is host-synchronised, so the frequency and trim go straight
  into the option bytes.
- **Platform implementations differ**:
  - Windows: `hid.dll` plus SetupAPI (both ship with the OS, zero dependencies)
  - macOS / Linux: hidapi (IOKit and hidraw backends)

> **After `detect` over HID the chip stays in its ISP monitor; it is not reset.** On the serial
> transport the tool sends a `0xFF` reset packet so the chip returns to user code. That is not
> possible over HID: the moment the chip leaves the ISP monitor the USB device disappears from the
> bus, and the `burn` that follows would report "no STC USB ISP device found". So a `burn` can
> follow `detect` directly; to run the original program again, power-cycle the board.

---

## Building

Requirements: **CMake 3.20 or newer** and a **C++17 compiler**. macOS and Linux additionally need
**hidapi** (see below).

### Windows

Use the **Visual Studio 2022 x64 developer prompt**. **No external dependency is needed**:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Output: `build\Release\newisp.exe`

The scripts in the repository do the same thing:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\win-build.ps1
powershell -ExecutionPolicy Bypass -File scripts\win-smoke.ps1    # smoke test
```

The Windows build links only against the two system libraries `setupapi` and `hid`; **there is
nothing to download or ship alongside the executable**.

> `.vscode/` in the repository provides a preset-free setup: `settings.json` pins the generator and
> `NEWISP_LANG`, and `tasks.json` turns "pick a language, pick a build type" into a build task
> (`Ctrl+Shift+B`).

### Linux

```sh
sudo apt install build-essential cmake pkg-config libudev-dev libhidapi-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

**Permissions**:

```sh
# Serial: add the user to the dialout group (log out and back in afterwards)
sudo usermod -aG dialout $USER

# HID: hidraw nodes are root:root 0600 by default, so a udev rule is required
sudo tee /etc/udev/rules.d/60-newisp.rules <<'EOF'
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="34bf", MODE="0666"
SUBSYSTEM=="tty",    ATTRS{idVendor}=="34bf", MODE="0666"
EOF
sudo udevadm control --reload-rules && sudo udevadm trigger
```

> **WSL2 does not expose USB devices by default.** Forward the board with
> [`usbipd-win`](https://github.com/dorssel/usbipd-win):
> ```powershell
> usbipd list                                    # find the BUSID
> usbipd bind --busid <BUSID>                    # needs administrator rights
> usbipd attach --wsl --busid <BUSID>
> ```
> `scripts/usbipd-attach.ps1` wraps this sequence.

### macOS

**Apple silicon (M-series)** can use Homebrew:

```sh
brew install cmake hidapi
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

**Intel Macs**: Homebrew 7.0.0 demoted them to "Tier 3" and the install script refuses to run on
anything that is not Apple silicon, so an Intel machine has to build the dependencies by hand:

```sh
# 1. CMake (macOS ships python3, so pip can supply it)
python3 -m pip install --user cmake
echo 'export PATH="$HOME/Library/Python/3.9/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc

# 2. hidapi, built from source against IOKit
cd ~
curl -L -o hidapi.tar.gz \
  https://github.com/libusb/hidapi/archive/refs/tags/hidapi-0.14.0.tar.gz
tar xzf hidapi.tar.gz && cd hidapi-hidapi-0.14.0
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build -j4
sudo cmake --install build

# 3. newisp
cd ~/newisp-cross
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

> `CMAKE_POLICY_VERSION_MINIMUM=3.5` is required: hidapi 0.14.0 asks for a CMake older than 3.5,
> and CMake 4.x refuses such projects outright.

Serial ports on macOS are **`/dev/cu.*`**. **Prefer `cu` over `tty`**: opening a `tty.*` waits for
DCD, which a self-powered board never asserts, so the open hangs.

A longer macOS walkthrough (including the command-line tools install) is in
[`docs/macos-build.md`](docs/macos-build.md).

### HID is a hard dependency

**There is no "build without HID" switch** (`NEWISP_ENABLE_HID` has been removed and
`NEWISP_HAVE_HID=1` is unconditional). STC32, STC8H and 8051U rely on the USB ISP interface, so a
build without HID would be a build that cannot program part of the catalogue. Rather than let that
fail on somebody's bench, it fails at configure time:

- Windows: nothing extra; the system HID class driver is used directly.
- macOS / Linux: **hidapi is required**. Without it `cmake` stops at `FATAL_ERROR` and names the
  package to install (`brew install hidapi` / `libhidapi-dev`).

### Output language (chosen at build time)

The language is fixed **when the tool is built**, and `NEWISP_LANG` selects which table actually
produces output:

```sh
cmake -S . -B build-zh -DNEWISP_LANG=zh-CN     # Chinese (the default)
cmake -S . -B build-en -DNEWISP_LANG=en        # English
```

`NEWISP_LANG` accepts `zh-CN`, `zh`, `en` and `en-*`; any other value is a configure-time error.

So **one binary cannot switch languages at run time**; to ship both, configure twice. That is a
deliberate trade-off for a small self-contained programmer: no locale lookup at start-up, no
runtime table switching, nothing on the target machine that can be misconfigured.

> Strictly speaking: **the English reference table is compiled into every build** -- it is also what
> the tests compare against entry by entry in `src/core/i18n.cpp`, and it is small next to the
> protocol code -- and a Chinese build carries a Chinese table in addition; an English build has no
> second table and returns the English source directly. Both builds therefore **print exactly one
> language**, but "the other language is not in the executable at all" would be inaccurate.

The message tables live in two places: `include/newisp/text_en.inc` holds the English source text
(also the **reference table** and the single source of the `Msg` enumeration), and `src/i18n/`
holds one file per language, each entry named after its English wording (`Msg::EraseOk` is "Erase
complete"), so the two can be proof-read side by side. Adding a language means adding one `.inc`
file, one `NEWISP_LANG` value and one compile definition; no calling code changes.
`tests/test_i18n.cpp` checks that every `{n}` placeholder survives translation, that no prose entry
was left untranslated, and that the language the binary reports is the language it prints. It
checks **shape and structure, not wording quality**.

---

## Encoding note

**Source files are UTF-8 with BOM, and the MSVC build is given `/utf-8`.**

Both are needed because the message tables and the comments contain Chinese text: without the BOM,
MSVC reads the sources in the system ANSI code page (936 / GBK on a Chinese Windows), mis-parses
the Chinese strings and fails with `newline in constant` errors.

**Do not strip the BOMs.** Line endings are **LF** throughout, as enforced by `.gitattributes`.

---

## Usage

### Commands

```sh
newisp                                   print one line pointing at help and list
newisp help                              show the full help
newisp version                           show the version and the transports this build has
newisp list                              list serial and HID devices
newisp detect   [transport] [device]     probe and identify a chip
newisp inspect  FILE                     validate HEX offline and show address ranges
newisp burn     FILE [options] [device]  erase, write and set options
```

`-help` and `-version` are no longer options; they became the two commands `help` and `version` (so
there is no `-help`, no `-?`, no `-version` and no `-V`).

Run `newisp` with no arguments and it prints **a single line** pointing at `help` and `list`; it no
longer enumerates devices or dumps the manual.

### Options

| Option | Description |
| --- | --- |
| `FILE` | The firmware image is a **positional argument** right after the command: `newisp burn app.hex`, `newisp inspect app.hex`. It is required by `burn` and `inspect`, and there is no option for it (`-f` / `--file` are gone) |
| `-s`, `-serial [BAUD]` | Use the serial ISP monitor (the default). The optional `BAUD` is the handshake rate, default 115200; `115200`, `115.2k` and `1M` are all accepted. Example: `-s 921600`. A device name may also follow: `-s COM7`, `-s /dev/ttyUSB0`. **The `detect` handshake is fixed at 2400, and a number after `-s` is ignored rather than refused** |
| `-h`, `-hid` | Use the USB HID ISP interface (`-h` no longer means help; `-H` resolves to HID too, because matching is case-insensitive) |
| `-d`, `-device NAME` | Device to open: `COM7`, `/dev/ttyUSB0`, `/dev/cu.usbserial-*` or a HID interface path. If omitted and exactly one candidate exists it is used; if several exist, the tool refuses and lists them |
| `-f`, `-freq`, `-frequency`, `-F` HZ | Target IRC frequency. Accepts `24000000`, `24M`, `11.0592M`. **If it is not given the chip keeps its current frequency**; if it is given, it must be an entry of that family's frequency menu (above the ceiling it is lowered with a note; outside the menu it is an error). It is **not** silently floored to a default of 24 MHz |
| `-e`, `-eeprom SIZE` | EEPROM split, e.g. `4K` (`4096` is accepted too). `0` keeps the chip's setting; **omitting `-eeprom` keeps the chip's current split** |
| `-c`, `-clock internal\|external` | Clock source. Only STC10/11/12 record this bit; other families reject it. `internal` / `int` / `irc` and `external` / `ext` / `crystal` are all accepted |
| `-series NAME` | Fallback family when probing identifies nothing: `stc89`, `stc12`, `stc15`, `stc8g`, `stc8h`, `stc8a`, `stc32`, `stc8` (the help lists these eight; the parser also accepts `stc90`, `stc10`, `stc11`, `iap15`, `irc15`, `ai8g`, `jx8g`, `ai32`, `ai8h`, `8051u`, `stc8051u`, `ai8051u`, `stc8f`, `stc8c`, `ai8a` and `ai8c`, all case-insensitive). **Always spelled out in full** (`-ser` is not accepted, and there is no short form) |
| `-output LEVEL` | How much of the log to show: `quiet` (**the default**: warnings, errors, attention notes and the final result only), `normal` (adds the progress lines), `verbose` (adds the raw packet dumps). `q`, `n`, `v` and `default` are also accepted |
| `help` | Show help (a command, not an option) |
| `version` | Show the version (a command, not an option) |

**Exactly three options are locked to their full spelling**: `-serial`, `-freq` and `-series`. Every
other long name accepts a **unique prefix** of at least three characters (`-ee` is `-eeprom`, `-de`
is `-device`, `-cl` is `-clock`). `-serial` cannot abbreviate because `-ser` would waver between it
and `-series`; `-freq` is locked so that a typo such as `-frequncy` cannot be silently accepted as a
unique prefix.

Option names are **case-insensitive** (`-DEVICE`, `-SERIAL` and `-OUTPUT` all work). The frequency
has four distinct names: `-frequency`, `-freq` and the one-letter `-f` / `-F` (an abbreviation such
as `-fr` is **not** accepted). An unknown option is reported with a suggestion based on edit
distance (`unknown option: -frequncy (did you mean -freq?)`), but it is **only a hint, never a
substitution** -- guessing which frequency the user meant is exactly what must not happen.

The firmware image is not an option but the first argument after the command, and options may also
appear before it. The tool takes the single-dash spelling (`-output`, `-series`); the `--` forms in
the tables are the conventional long-option spelling.

> `inspect` is strictly offline and rejects the transport (`-s` / `-h`), the device (`-d`),
> `-series`, `-clock` and `-eeprom`.
> `detect` accepts the transport, the device and the rate after `-s` (which it ignores), but rejects
> `-freq`, `-eeprom`, `-clock` and `-series` -- these are stopped while the command line is parsed,
> not accepted and then ignored.

> **There is no `--lang` flag**: the output language is chosen at build time with
> `-DNEWISP_LANG=zh-CN|en`; see [Output language (chosen at build time)](#output-language-chosen-at-build-time).

### Examples

**Identify a chip:**

```sh
newisp detect -h                                 # USB HID
newisp detect -s /dev/ttyUSB0                    # Linux serial
newisp detect -s /dev/cu.usbserial-1420          # macOS serial
newisp detect -s COM13                           # Windows serial
```

**Burn a firmware image:**

```sh
# Firmware only: frequency and EEPROM split both stay as the chip has them (quiet output)
newisp burn firmware.hex -h

# Serial, 460800 baud, an explicit 24 MHz target
newisp burn firmware.hex -d /dev/ttyUSB0 -s 460800 -F 24M

# With an EEPROM split and the progress lines
newisp burn firmware.hex -d COM13 -F 35M -e 4K -output normal

# Raw packets of the handshake
newisp burn firmware.hex -d COM13 -output verbose

# An STC12 part on its external crystal
newisp burn firmware.hex -d COM13 -c external
```

### Nothing you did not ask to change

One principle runs through the whole tool: **whatever the command line does not mention is left
alone**.

| Situation | Before | Now |
| --- | --- | --- |
| Default `-output` level | `normal` | `quiet`: only warnings, errors, attention notes and the final result |
| Output redirected and no `-output` given | silently upgraded to `verbose` | no upgrade at all, still `quiet`; ask for packets with `-output verbose` |
| No `-freq` | the 24 MHz default was written into the option bytes | **the chip's current IRC frequency is kept**, with a line saying so |
| No `-eeprom` | a hard-coded `0xFE` was written (about 64 KB of EEPROM, almost no code area) | **the split the chip reported is written back** (`0xFF` means "not split" and is preserved as-is) |
| End of `detect` over HID | a `0xFF` reset, after which the USB device vanished | the chip stays in its ISP monitor, so a `burn` can follow |
| `detect -s 9600` | -- | the number is ignored (the handshake is fixed at 2400) and nothing is reported |

`burn` likewise echoes only the settings that were actually given: without `-freq` it does not
print a target frequency line, because printing a default that will not be written amounts to
announcing a change that is not going to happen. For the same reason, a `-freq` that is not in the
family's menu stops the run at the option stage with
`... is not an entry of the ... frequency menu; nothing was written`.

> **What `quiet` actually hides**: the device, transport, firmware, target-frequency, baud-rate and
> EEPROM echo lines, and the `[1/7]` ... `[7/7]` progress line of every step, are all ordinary
> lines and are **not printed**. A successful `quiet` burn therefore shows essentially one line,
> `Burn succeeded` (on failure, the reason). Use `-output normal` to see the step-by-step progress;
> the progress bar itself appears only when the level is not `quiet` and stdout is a terminal.

### Migrating from the old command line

The meaning of `-f`, `-h`, `-b`, `-q`, `-v` and `-family` has changed, so old scripts need edits:

| Old form | Now | Note |
| --- | --- | --- |
| `newisp burn -f app.hex` | `newisp burn app.hex` | the image is positional; **`-f` now means frequency** |
| `-f 24M` (old meaning: frequency) | still `-f 24M` | the meaning is unchanged, but `-freq` / `-frequency` reads better |
| `-b` / `--baud 460800` | `-s 460800` | the rate belongs to `-s`, which may also carry a device name |
| `--help` / `--version` | `help` / `version` | now commands; there is no `-help`, `-H`, `-?` or `-V` |
| `-v` (verbose) | `-output verbose` | `-v` is now a value of `-output`, not an option; `-q` no longer exists either |
| `-family xxx` | `-series xxx` | and it must be spelled out in full (`-ser` is rejected as an ambiguous prefix of `-serial`) |
| `--clock external` | `-c external` (or `-clock external`) | unchanged; only the examples were unified |
| `-force-serial` | -- | removed: whether the serial transport works is decided by the probe |

---

## Testing

### Unit tests (no hardware)

```sh
cmake -S . -B build-tests -DCMAKE_BUILD_TYPE=Debug -DNEWISP_BUILD_TESTS=ON
cmake --build build-tests -j
./build-tests/tests/newisp_tests        # or: ctest --test-dir build-tests
```

The unit tests are a single `newisp_tests` executable built from `test_hex_file.cpp`,
`test_chip_table.cpp`, `test_chip_logic.cpp`, `test_protocol_framing.cpp`, `test_i18n.cpp`,
`test_options.cpp` and `test_main.cpp`, and it compiles `src/cli/options.cpp` directly so the
argument parser can be asserted on end to end.

Coverage:

- **Intel HEX parser**: extended linear and segment addresses, CRLF input, lowercase hex,
  truncated records, the EOF record, gaps filled with `0xFF`.
- **Chip table**: known IDs, the unknown-ID placeholder, no null entries.
- **Chip-family predicates and protocol selection**: the per-family ceiling and menu entry count,
  menu distinctness (STC8A's 45.000 and STC8H's 45.1584 are two different entries), agreement
  between each menu and the trim table, the legacy 115200 limit, which families need the `0x05`
  prepare step and which are reachable over HID.
- **Command-line parsing** (`tests/test_options.cpp`): positional arguments, `-series` spelled out
  in full, which option `-s` belongs to (and that `detect -s 9600` drops the number and keeps the
  default rate), unique-prefix abbreviations, `-output` set only once, the frequency spellings and
  the rejection of near misses, the default output level of `quiet`, and spelling suggestions.
- **Packet framing**: exact bytes and checksums, round trips for every protocol, tolerance of
  leading noise, rejection of bad checksums and lost terminators, a packet split across two reads,
  and status decoding that rejects the wrong family or an implausible clock.
- **Message tables** (`tests/test_i18n.cpp`): every translation keeps the `{n}` placeholders of the
  English source, no prose entry is left untranslated, the language the binary reports is the one it
  prints, and the log markers that separate packet dumps from progress lines still work after
  translation. It runs against whichever language the build selected, so build and test both
  `-DNEWISP_LANG=zh-CN` and `-DNEWISP_LANG=en` before a release. It checks **shape and structure,
  not wording**.
- **CLI preflight** (`tests/cli_preflight_test.py`) deliberately asserts only on exit codes, hex
  addresses and other tokens that do not change with the language, so it holds for a build in either
  language.

CTest registers three entries: `newisp_tests`, `newisp_cli_preflight`, and `newisp_pty_preflight`
(registered on UNIX only). `pty_test.py`, `pty_diag.py` and `pty_opts_dump.py` under `tests/` are
manual helpers and are not CTest entries.

> On a Chinese Windows, Python decodes the tool's UTF-8 output with code page 936 and raises
> `UnicodeDecodeError`; `tests/CMakeLists.txt` sets `PYTHONUTF8=1` for **all three** CTest entries.
> If you run a script by hand, set the same variable yourself.

### Protocol test over a pseudo-terminal (no hardware)

A **PTY pair** stands in for a real serial port, a scripted fake STC chip answers on the other side,
and the **real binary** is driven through the whole burn sequence:

```sh
python3 tests/pty_test.py ./build/newisp
```

**Expected output**:

```
PASS
exit code      : 0
saw wakeup 0x7F: True
erase seen     : True
blocks written : 4
options written: True
```

This test runs on **Linux and macOS only** (Windows has no PTY).

### Helper scripts

| Script | Purpose |
| --- | --- |
| `scripts/wsl-build.sh` | Build inside WSL (accepts a language argument, uses a per-language build directory, honours `WSL_BUILD_DIR`) |
| `scripts/wsl-tests.sh` | Run the unit tests inside WSL |
| `scripts/wsl-ptytest.sh` | Run the PTY protocol test inside WSL |
| `scripts/wsl-smoke.sh` | Smoke test inside WSL |
| `scripts/wsl-install.sh` | Install the WSL build where the interactive user can read it |
| `scripts/wsl-env-report.sh` | Report the WSL build environment (compiler, CMake, dependencies) |
| `scripts/wsl-burn.sh` | Burn on real hardware from WSL |
| `scripts/wsl-detect.sh` | Detect on real hardware from WSL |
| `scripts/wsl-udev.sh` | Install the udev rule and fix device permissions (needs sudo) |
| `scripts/wsl-device-check.sh` | Check whether serial and HID devices are visible inside WSL |
| `scripts/wsl-status.sh` | Summarise the WSL environment and device state |
| `scripts/wsl-usb.sh` | List USB devices inside WSL |
| `scripts/usbipd-attach.ps1` | Forward a USB device into WSL (needs administrator rights) |
| `scripts/win-build.ps1` | Windows build |
| `scripts/win-smoke.ps1` | Windows smoke test |
| `tests/pty_diag.py` | Show which termios settings a tty actually accepts |
| `tests/pty_opts_dump.py` | Dump the option bytes the tool writes (with the decoded frequency and trim) |

> `scripts/wsl-verify-forceserial.sh` and `scripts/wsl-verify-nooverclock.sh` verify the removed
> `-force-serial` option and the old overclock behaviour. They are leftovers and can be ignored.

---

## Architecture

```
include/newisp/
  IChannel.h            transport interface (Open/Write/Read/Flush/SetBaudRate/DTR/RTS)
  serial_channel.h      serial backend
  hid_channel.h         USB HID backend (always built in, cannot be disabled)
  stc_protocol.h        protocol base: packet framing, wait/pulse helpers
  protocol_stc89.h      STC89/90 (ProtocolStc89 legacy 1-byte-checksum framing, ProtocolStc89A)
  protocol_stc12.h      STC10/11/12 (12A / 12B variants)
  protocol_stc15.h      STC15 (15A variant)
  protocol_stc8_uart.h  STC32/STC8G/STC8 over serial
  protocol_stc8_hid.h   STC32/STC8G/STC8 over USB HID
  protocol_factory.h    chip name -> protocol
  chip_table.h          chip ID (magic) -> part name, more than 1300 entries
  chip_logic.h          family predicates, per-family frequency menus, baud table
  hex_file.h            Intel HEX parser
  session.h             detect/burn sequences
  options.h             command-line options
  i18n.h                message enumeration Msg, Tr() formatting, log-kind markers
  text_en.inc           English message table (the reference table and the only source of Msg)
  text.h                UTF-8 text helpers, hex formatting, monotonic clock

src/core/               platform-independent: chip table, chip logic, HEX parser, i18n table selection
src/i18n/               per-language translation tables (text_zh_cn.inc and so on)
src/transport/          platform-specific: serial (Win32 or termios), USB HID
src/cli/                argument parsing, session driver, main
tests/                  unit tests and the PTY protocol test
```

The protocol layer is written against the `stcisp::IChannel` interface and **never names a concrete
transport**.

The STC32/STC8G/STC8 families have a **separate implementation per transport**, because their status
framing and calibration differ genuinely. The remaining families (STC89/12/15) are serial-only.

**The log kind travels with the line**: packet dumps are prefixed with `%p` and conclusions worth
flagging with `%a`, and the marker is stripped before printing. That keeps log filtering independent
of the interface language -- the old code filtered on **Chinese prefixes**, which stops working the
moment the text is translated.

### Transport backends

| Platform | Serial | USB HID |
| --- | --- | --- |
| Windows | `CreateFile` + `DCB` + `EscapeCommFunction` | `setupapi.dll` + `hid.dll` (system HID class driver) |
| macOS | `termios` + `TIOCMGET`/`TIOCMSET` | hidapi (IOKit backend) |
| Linux | `termios` + `TIOCMGET`/`TIOCMSET` | hidapi (hidraw backend) |

Serial enumeration is platform-specific as well: the SetupAPI port class on Windows, IOKit
`IOSerialBSDClient` services on macOS, and a scan of `/dev` cross-checked against sysfs on Linux.
Windows additionally parses the VID/PID out of the device instance ID and upgrades the generic
`usbser` name to the USB product name reported on the HID side, so it prints as something
recognisable like `STC USB Serial (COM12)`.

**Two platform-independent details are worth stating:**

1. **DTR and RTS are cleared straight after the port is opened.** Most platforms raise DTR when a
   port is opened, and on many USB-serial bridges (CH340, CP2102, FT232) DTR is wired to the
   board's reset line -- without clearing it the board is held in reset before the reset pulse has
   even been sent.

2. **Parity is best effort.** Some tty devices (pseudo-terminals in particular, and a few USB bridge
   drivers) reject `PARENB` with `EINVAL`. The speed and framing still take effect, the port records
   the parity it **actually** has, and `SetParity` reports the truth instead of claiming success.
   The older STC families run at 8N1 anyway.

---

## Hardware notes

### Clock range

**There is no single global IRC frequency table**: each family has its own menu (transcribed from
AiCube-ISP), and frequency selection filters against that family's menu rather than against a
numeric window:

| Family | Menu entries | Ceiling |
| --- | --- | --- |
| STC8G | 12 | 35.000 MHz |
| STC15 | 12 | 35.000 MHz |
| STC8A (including 8F / 8C / GX8S) | 16 | 45.000 MHz |
| STC8051U | 16 | 45.1584 MHz |
| STC8H | 16 | 45.1584 MHz |
| STC32G144K246 | 15 (the vendor menu has 17; 2 entries have no trim) | 60.000 MHz |

STC10/11/12 and STC89/90 **have no frequency menu** and do not accept `-freq`.

The complete per-family menus, the screenshot sources and the reasons for the omitted entries are in
[IRC frequency tables](docs/irc-frequency-tables.md).

**There is no overclock band**, because the frequencies outside the menus have no reliable basis:

- 46 and 47 MHz were measured to work on an AI8051U34K64.
- **48 MHz was measured to drop the clock** rather than raise it, so the physical IRC limit sits just
  under 48 MHz.
- 49 and 50 MHz were extrapolated from a fitted curve and never verified.

**The rule is menu membership, not a numeric range:**

- A `-freq` that is not in the family's menu is **rejected** with an explicit message listing the
  family's real entries: 45.100 MHz falls inside the 8H numeric range, but it is not a menu entry,
  and writing it would burn option bytes at a speed nobody asked for.
- A value above the highest entry **of every menu** (60 MHz) is rejected while the command line is
  parsed -- no device is even looked for.
- A value above the family's ceiling **that is the ceiling entry itself** is lowered to the ceiling
  with a note. This is the only clamping behaviour that remains.
- Even a frequency that is in the menu is rejected if **there is no trim data for that entry**.
- 8A has a naming trap: `STC8A8K64S4A12` and `STC8A2K64D4` are 8A silicon (45.000 MHz), while
  `STC8A8K64D4` is 8H silicon (45.1584 MHz). The part number decides, not the family prefix.
- STC32, STC8H and 8051U share one protocol implementation (and one probe slot), while the 60 MHz
  menu belongs to `STC32G144K246` alone. On an ordinary STC32G12K128 a 60 MHz request is therefore
  listed as a menu entry and then refused at the option stage for lack of trim data.

### Option bytes

Option bytes carry the watchdog, reset behaviour and clock configuration, and a wrong value can
leave a chip unable to start. Do not read them as a "preserved as-is" guarantee: the option block is
assembled from the constants and trim tables each protocol carries, not a byte-for-byte copy of the
chip's current options. With the STC32 restriction lifted, that caveat applies to every family the
tool writes -- check the target's option layout before burning.

**What you do not ask to change is left alone**: without `-freq` the target frequency is left empty
(`userTargetFreq = 0`), on which the protocol layer falls back to the current IRC frequency reported
in the handshake status packet; without `-eeprom` the split field is written back from the chip's
reported `msr[4]` (`0xFF` means "not split" and is preserved as-is). These two used to be a
hard-coded 24 MHz and a hard-coded `0xFE`.

### Options validated per family

The tool no longer refuses a whole family wholesale; it checks whether the target family **has the
field**:

| Option | Applies to | Other families |
| --- | --- | --- |
| `-freq` | STC32 / STC8H / STC8G / STC8 / STC15 / 8051U and other families with a frequency menu | STC10/11/12 and STC89/90 have no frequency menu, so `-freq` is refused |
| `-eeprom` | STC32 / STC8H / STC8051U and the rest of the 8-family (STC8G / STC8F / STC8A / STC8C) | STC15 / STC12 / STC10 / STC11 / STC89/90 keep their EEPROM as a fixed separate region, so asking for a split is an **error**, not something quietly ignored |
| `-clock` | Only STC10/11/12 have a clock-source bit | Everyone else refuses it (STC89/90 have no such bit; on the other families the clock is decided by `-freq`) |

### Offline firmware check

`newisp inspect app.hex` checks checksums, lengths, record format, EOF, overlaps and address overflow
strictly, keeps the full 32-bit address, and does not open or enumerate any device. The input limit
is 16 MiB. A successful check does not mean the image can be burned.

The writer addresses 64-byte blocks with a 16-bit address, so `0x0000..0xFFFF` is the burn-reachable
range for every family, STC32 included. `inspect` itself does not enforce that limit: it lists
records above `0xFFFF` like any other and prints `Programming unavailable: <reason>`. `burn` refuses
such an image before any device is opened, neither dropping the high bits nor rebasing them. Type
03/05 entry records are retained as metadata only and never executed.

> The `inspect` report is **hard-coded English** and does not go through the message table, so a
> Chinese build shows English here too.

### Serial auto-reset

If the board has **no auto-reset circuit** (DTR not wired to the reset pin), power-cycle the board
by hand during the handshake.

---

## Verification status

What this repository has actually exercised, so the state of the code is not overstated:

| Platform | Build | Unit tests | Smoke test | Real hardware |
| --- | --- | --- | --- | --- |
| Linux (Ubuntu 22.04 / GCC 11 / CMake 3.22) | yes | yes, all passing | yes | yes: PTY protocol test passes; AI8051U34K64 burned over USB HID at 45 MHz |
| Windows (MSVC 19.44 / CMake 4.4) | yes | not run | yes: real serial ports and HID devices enumerated | yes: AI8051U34K64 burned over USB HID at 24/40/45 MHz **and over serial**; STC8H8K64U over serial at 460800 |
| macOS (13.1 / AppleClang 14 / CMake 4.4) | yes | pending | yes | yes: AI8051U34K64 burned over USB HID at 45 MHz |

### Hardware burn test (AI8051U34K64 over USB HID)

Verified on real hardware: every burn exited with code 0 and the full sequence was acknowledged by
the chip:

| Step | Result |
| --- | --- |
| Probe | identified `AI8051U34K64`, magic `0x78B4`, BSL 7.4U |
| `0x01` baud set | answered `01` |
| `0x05` prepare | answered `05` |
| `0x03` erase | answered `03`, returned the real UID `78 B4 C9 28 09 FA 88` |
| `0x02`/`0x22` program | 13 of 13 blocks for an 810-byte image, each answered `02 54` |
| `0x07` finish | answered `07 54` |
| `0x04` options | answered `04 54` |

All three target frequencies were verified by **re-probing the chip and reading back**: the chip
reloads the option block, and the frequency counter in its status packet reports the new clock.

| Target | `opts[24..26]` count | trim written | Re-probed clock |
| --- | --- | --- | --- |
| 24 MHz | `0x016E36` | `48 20 01` | 24.000 MHz |
| 40 MHz | `0x02625A` | `73 30 01` | -- |
| 45 MHz | `0x02AEA5` | `A6 30 01` | 44.999 MHz |

**44.999 MHz is normal IRC tolerance**; the vendor ISP tool reports the same value for the same
option bytes.

The serial path was verified separately on an **STC8H8K64U**: all 265 blocks were written at 460800
baud, and the calibration handshake measured a trim of `67 / 0x20 / 1`.

All three platforms **read back the identical UID** (`78 B4 C9 28 09 FA 88`), which is evidence that
the protocol implementation agrees across platforms.

### The 8051U does have a serial ISP monitor

The vendor manual lists the 8051U family as "USB HID only", and early versions of the tool refused
it on the serial transport for that reason: an 8051U was reported as "please switch to USB HID".

That rule was inherited rather than measured, and **it is wrong**. On an AI8051U34K64 (BSL 7.4U) the
serial ISP monitor answers the `0x7F` wakeup with a `0x50`-prefixed status packet, the calibration
handshake completes normally, and the whole sequence runs:

```
[1/7] handshake succeeded
[2/7] switched to 115200, trim_adj=90, trim_range=0x20, trim_freq=24031200 Hz
[3/7] programming prepared
[4/7] erase complete -> UID 78 B4 C9 28 09 FA 88
[5/7] 13/13 blocks
[6/7] option bytes written
[7/7] burn complete
```

Re-probing after the burn confirms the option bytes took effect: the status packet reports
24.031 MHz with the trim triple that was just written.

The restriction has been removed. **The probe decides, not the manual** -- a chip that answers is
usable; one that does not answer times out on its own and nothing after that step runs. The
disagreement between manual and measurement is still **reported** (as `[Attention]` and warning
lines), but it is only a note, no longer a refusal.

Note that the serial and HID status packets are **not laid out identically** (the serial one carries
a `0x50` prefix and reports different MSR bytes), which is why the two transports had separate
protocol implementations all along.

### A cross-platform bug that only hardware could expose

The port had a **cross-platform HID difference** that only real hardware could expose:

| Platform | First byte returned by `hid_read` |
| --- | --- |
| Windows (`hid.dll`) | the **report ID** (the HID stack reserves the first byte), so the payload starts at index 1 |
| Linux (`hidraw`) | the **payload** itself, with no report ID byte, so it starts at index 0 |

Unconditionally skipping the first byte shifted every packet by one byte on Linux, so the `46 B9`
frame marker was misaligned, every protocol parse failed, and the tool reported "no chip detected".

**The fix does not depend on a compile-time assumption; it looks at the data**: an STC packet always
starts with `46 B9`, so the payload starts wherever that marker is. That holds on all three
platforms and tolerates future changes in hidapi behaviour.

---

## Known limitations

- **The help text now matches the implementation**: `-freq` no longer claims a 24 MHz default, the
  no-argument row describes the one-line invite, the closing note states that **every family, the
  8051U included, can be programmed over the serial ISP monitor**, `-output` no longer offers a
  non-existent `-o` short form, and the `list` diagnostic points at `-output verbose` instead of the
  removed `-v`. One English error string in `hex_file.cpp` still says `use inspect -f FILE`; the
  correct form is the positional image, `inspect app.hex`.
- **The `inspect` report does not follow the language**: it is hard-coded English.
- **The PTY protocol test validates only the host side** of the wire protocol: framing, checksums,
  command sequence and reply handling. Its fake chip answers only the STC32-family serial protocol,
  so the older families are covered by the unit tests rather than end to end.
- **The PTY test runs on Linux and macOS only**, because it needs a real terminal device pair. The
  Windows serial backend is a completely different API with no equivalent.
- **The serial transport has not been exercised as widely as HID.** It has been used on Windows
  (COM13, STC8H8K64U, 460800) and on Linux (WSL through usbipd, plus the PTY test) but **never on a
  physical Linux host with a real adapter**.
- **Some bench results predate the current option-write path**: the 24/40/45 MHz read-back table
  above was produced by an earlier version. The option-constructor change (preserving the chip's own
  frequency and EEPROM split when the option is absent) only touches the code path taken **without**
  `-freq` / `-eeprom`, so a burn that names them explicitly is unaffected -- but those numbers have
  not been re-measured since.
- **The macOS unit tests have not been run yet** (the build and a real burn both succeeded).
- **The binaries are not "double-click and go"**: they are command-line tools with no Apple
  developer signature. Handing one to someone else requires clearing the quarantine attribute
  (`xattr -d com.apple.quarantine newisp`) and keeping the executable bit; an Intel binary needs
  Rosetta 2 on Apple silicon.
- **STC16 is no longer in the part table**: it can still be probed and its ID reported, but it has
  no part name.
- **HID on a non-USB family is only warned about**: `detect -h` succeeds on an STC8G/STC8 part, but
  the tool prints an `[Attention]` line saying the family is not documented with USB rather than
  refusing -- the probe is honoured; judging whether that result is real is left to you.
- **The image limit is the 16-bit block address**: no family can be written with an image that
  reaches past `0xFFFF` (see [Offline firmware check](#offline-firmware-check)).

---

## License

This project is licensed under the **GNU General Public License v3.0**. See [LICENSE](LICENSE).
