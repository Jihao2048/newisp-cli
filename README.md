# newisp

A cross-platform command-line programmer (flasher) for **STC 8051-family microcontrollers**.

`newisp` was ported from a Windows-only WinUI 3 / C++/WinRT desktop application to a portable
C++17 CLI built with CMake. It runs on Windows, macOS and Linux, and speaks to a target chip over
either a serial (UART) link or the factory USB HID ISP interface.

> **中文文档**：见 [README.zh-CN.md](README.zh-CN.md)。 / Chinese documentation:
> see [README.zh-CN.md](README.zh-CN.md).

## Table of contents

- [Features](#features)
- [Supported chip families](#supported-chip-families)
- [Transports](#transports)
- [Building](#building)
  - [Linux (Ubuntu/Debian)](#linux-ubuntudebian)
  - [macOS (Homebrew)](#macos-homebrew)
  - [Windows](#windows)
  - [Building without HID](#building-without-hid)
- [Encoding note (important)](#encoding-note-important)
- [Usage](#usage)
  - [Commands](#commands)
  - [Options](#options)
  - [Examples](#examples)
- [Testing](#testing)
- [Architecture](#architecture)
- [Hardware notes](#hardware-notes)
- [Verification status](#verification-status)
- [Limitations](#limitations)
- [License](#license)

## Features

- Single self-contained CLI binary for Windows, macOS and Linux.
- Two transports: classic STC serial ISP monitor and the factory USB HID ISP interface.
- Automatic chip identification by magic (chip ID) lookup against a table of roughly 1300 parts.
- Full burn sequence: erase, 64-byte block writes, and option-byte programming.
- Strict, sparse Intel HEX parsing and offline address inspection.
- Frequency and baud rate handling with human-friendly argument forms
  (`115.2k`, `24M`, `11.0592M`, ...).
- Measured IRC calibration on the serial transport: the tool derives the trim
  triple from the chip's real frequency rather than looking it up.

## Supported chip families

| Family | Notes |
| --- | --- |
| STC89 / STC90 | Serial only |
| STC10 / STC11 / STC12 | Includes 12A / 12B variants; serial only |
| STC15 | Includes 15A variant; serial only |
| STC8 / STC8A / STC8C / STC8F / STC8G | Serial only (no USB controller) |
| STC8H | Serial and USB HID |
| STC32 | Serial and USB HID, same protocol as the 8051U; images up to 64 KiB |
| STC8051U / AI8051U | Serial and USB HID |

The **8051U** family is documented by the vendor as USB-HID-only, and the tool
used to refuse it on the serial transport for that reason. The rule turned out
to be wrong: on an AI8051U34K64 with BSL 7.4U the serial ISP monitor answers the
wakeup and a full burn succeeds. The refusal has been removed, so what decides
is the probe -- a part that answers is programmable.

The selectable IRC range stops at **45 MHz**, the vendor's validated ceiling.
There is no overclock option: the frequencies above it were either measured to
misbehave or extrapolated from unverified points.

## Transports

### Serial (UART)

The classic STC ISP monitor. The tool pulses DTR and spams the `0x7F` wakeup byte in order to catch
a chip that is still executing user code. The baud rate is negotiated with the chip, starting at
2400 baud and moving up to the user-selected rate (default `115200`).

### USB HID

The factory USB ISP interface, identified by **VID `0x34BF` / PID `0x1001`**, implemented by STC32,
STC8H, STC8G and 8051U parts. The user must hold the **BOOT pin low while plugging the board in**.
No baud rate negotiation exists over USB.

## Building

Requirements:

- CMake 3.20 or newer
- A C++17 compiler

Source layout:

| Path | Contents |
| --- | --- |
| `include/newisp/` | Public headers |
| `src/core/` | Platform-independent code: chip tables, Intel HEX parser, ISP protocols (header-only classes) |
| `src/transport/` | Platform-specific transport implementations |
| `src/cli/` | Command-line front end |

### Linux (Ubuntu/Debian)

```sh
sudo apt install build-essential cmake pkg-config libudev-dev libhidapi-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

On Linux the user usually needs to be in the `dialout` group in order to open serial ports:

```sh
sudo usermod -aG dialout $USER   # then log out and back in
```

For HID access, a udev rule may be needed. Opening a HID device as a normal user requires read and
write permission on `/dev/hidrawN`; a rule for the STC ISP interface looks like this:

```
# /etc/udev/rules.d/60-newisp.rules
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="34bf", ATTRS{idProduct}=="1001", MODE="0666"
```

Reload and re-plug afterwards:

```sh
sudo udevadm control --reload-rules && sudo udevadm trigger
```

> Under WSL2 the USB device tree is not exposed by default, so `hidraw` and `/dev/ttyUSB*` nodes are
> absent. Pass the board through with `usbipd-win` (`usbipd list`, then `usbipd attach --wsl`) if you
> want to burn from WSL itself.

### macOS

With Homebrew, on Apple silicon:

```sh
brew install cmake hidapi
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

**Homebrew no longer supports Intel Macs.** Version 7.0.0 moved them to "Tier 3"
and the install script now refuses to run on anything that is not Apple silicon,
so an Intel machine needs the two dependencies built by hand. This is the route
that was used to verify the macOS port in this repository:

```sh
# 1. CMake. macOS ships python3, so pip can supply it.
python3 -m pip install --user cmake
echo 'export PATH="$HOME/Library/Python/3.9/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
cmake --version

# 2. hidapi, built from source against IOKit.
cd ~
curl -L -o hidapi.tar.gz \
  https://github.com/libusb/hidapi/archive/refs/tags/hidapi-0.14.0.tar.gz
tar xzf hidapi.tar.gz
cd hidapi-hidapi-0.14.0
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build -j4
sudo cmake --install build

# 3. newisp.
cd ~/newisp-cross
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

`CMAKE_POLICY_VERSION_MINIMUM=3.5` is needed because hidapi 0.14.0 asks for a
CMake older than 3.5 and CMake 4.x refuses those projects outright.

Serial ports on macOS are `/dev/cu.*`. Prefer `cu` over `tty`: opening a `tty`
waits for DCD, which a self-powered board never asserts, so the open hangs.

There is also `docs/macos-build.md`, which is a longer walkthrough of the same
steps including the Command Line Tools install.

### Windows

Use the **Visual Studio 2022 x64 developer prompt**. No external dependency is needed: the HID
backend talks to the system HID class driver through `hid.dll` and SetupAPI, both of which ship
with Windows.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

There is also `scripts/win-build.ps1`, which locates MSVC through `vcvars64.bat`, finds a CMake
installation, and configures with the Visual Studio generator. `scripts/win-smoke.ps1` exercises
the argument handling and device enumeration of the resulting binary.

The Windows build links only against `setupapi` and `hid`; nothing has to be downloaded, installed
or shipped alongside the executable.

### USB HID is required, not optional

There is no switch to build without HID. The 8051U family has no serial ISP monitor at all, so a
build without HID would be a build that cannot program part of the catalogue -- and it would fail on
somebody's bench instead of at configure time. On macOS and Linux `hidapi` is therefore a hard
dependency: if it is missing, `cmake` stops and says which package to install. On Windows nothing is
needed beyond the system HID class driver.

### Choosing the output language

The language is fixed **when the tool is built**, and only that language's table is compiled into
the binary -- the other one is not in the executable at all:

```sh
cmake -S . -B build-zh -DNEWISP_LANG=zh-CN     # Chinese (the default)
cmake -S . -B build-en -DNEWISP_LANG=en        # English
```

That means one binary cannot switch languages at run time; to ship both, configure twice. The
trade-off is deliberate for a small self-contained programmer: no locale lookup at start-up, no
runtime table switching, nothing to misconfigure on the target.

The message table lives in `include/newisp/text_en.inc` (the English source text, which is the
reference) and `src/i18n/` (one file per language, each entry named after its English wording).
Adding a language means adding one `.inc` file, one `NEWISP_LANG` value and one compile definition;
no calling code changes. `tests/test_i18n.cpp` checks that a translation keeps every `{n}` slot and
that no entry was left untranslated.

## Encoding note (important)

All source files are **UTF-8 with BOM**, and on MSVC the build additionally adds `/utf-8`.

Both are required because the message tables and the comments contain Chinese text. Without them,
MSVC reads the sources in the system ANSI code page (936 / GBK) and fails with `newline in constant`
errors.

**Do not strip the BOMs.**

## Usage

```
newisp detect   [transport] [device]     probe and identify a chip
newisp inspect FILE                      validate HEX and show address ranges, offline
newisp burn FILE [options] [device]      erase, write and set options
```

### Options

| Option | Description |
| --- | --- |
| `FILE` | The Intel HEX image is the first positional argument, right after the command: `newisp burn app.hex`, `newisp inspect app.hex`. It is required for `burn` and `inspect`; there is no flag for it. |
| `-s`, `--serial [BAUD]` | Use the serial transport (default). The optional BAUD is the handshake rate, default `115200`; accepts forms such as `115200`, `115.2k`, `1M`. Example: `-s 921600` |
| `-h`, `--hid` | Use the USB HID transport |
| `-d`, `--device NAME` | Device to open: `COM7`, `/dev/ttyUSB0`, `/dev/cu.usbserial-*`, or a HID interface path. If omitted and exactly one candidate exists, it is used; if several exist, the tool refuses and lists them. |
| `-f`, `-freq`, `-frequency HZ` | Target IRC frequency, default 24 MHz. Accepts forms such as `24000000`, `24M`, `11.0592M`. It has to be an entry of that series' menu; anything else is rejected |
| `-e`, `--eeprom SIZE` | EEPROM split, e.g. `4K`. `0` keeps the chip's current setting |
| `-c`, `--clock internal\|external` | Clock source. Only STC10/11/12 record one; other series reject the option |
| `-series NAME` | Fallback series to use when probing identifies nothing: `stc89`, `stc12`, `stc15`, `stc8g`, `stc8h`, `stc8a`, `stc32`, `stc8`. Must be spelled out in full (`-ser` is not accepted) |
| `-o`, `--output LEVEL` | How much of the log to show: `quiet` (errors and the final summary only), `normal` (default), `verbose` (adds the raw packet dumps) |
| `help` | Show help (a command, not an option) |
| `version` | Show version (a command, not an option) |

Option names are case-insensitive, and long names accept a unique prefix (`-ee` for
`-eeprom`), except `-serial` and `-series`: `-s` is the serial transport and the first
letter of `-series`. The frequency has exactly four accepted spellings --
`-frequency`, `-freq`, `-F` and `-f` -- and an abbreviation such as `-fr` is rejected
with a hint rather than resolved. The tool takes the single-dash spelling (`-output`,
`-series`); the `--` forms in the table above are the conventional long-option spelling.

There is no `--lang` flag: the output language is chosen when the tool is built, with
`-DNEWISP_LANG=zh-CN|en`. See [Choosing the output language](#choosing-the-output-language).

### Examples

Inspect firmware without enumerating or opening any device:

```sh
newisp inspect app.hex
```

Checksums, lengths, record shapes, EOF and non-overlapping 32-bit addresses must
be valid. Blank lines, CRLF, lowercase hex and trailing spaces are accepted;
other non-record text, records after EOF and records crossing a 64 KiB boundary
are rejected. Type 03/05 entry records are retained as metadata, not executed.
Limits are 16 MiB of input text and 1 MiB of actual data, independent of the
highest address. A high address never causes a multi-gigabyte allocation.

`inspect` succeeding means **valid input**, not a supported programming target.
It displays a separate programming-unavailable message for extended/banked
images. `burn` rejects those images **before device discovery**, rather than
dropping the high address bits or rebasing them. The writer addresses a 64-byte
block with 16 bits, so `0x0000..0xFFFF` is the reachable range for every family,
STC32 included.

Probe a chip on a specific serial port:

```sh
newisp detect -s -d /dev/ttyUSB0
```

Probe a chip over USB HID:

```sh
newisp detect -h
```

Burn an image at 24 MHz:

```sh
newisp burn firmware.hex -d COM7
```

Burn with an explicit baud rate, frequency and EEPROM split:

```sh
newisp burn firmware.hex -s 115.2k -F 24M -e 4K -d /dev/cu.usbserial-1420
```

Force a family when identification fails, with the external clock source:

```sh
newisp burn firmware.hex -series stc8h --clock external
```

## Testing

### Unit tests

The unit tests need no hardware and no framework. Configure with
`-DNEWISP_BUILD_TESTS=ON` and run `ctest`, or invoke the binary directly:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DNEWISP_BUILD_TESTS=ON
cmake --build build -j
./build/tests/newisp_tests
```

They cover the Intel HEX parser (extended linear and segment addresses, CRLF input, lowercase
digits, truncated records, the EOF record, filling gaps with `0xFF`), the chip table (known IDs,
the unknown-ID placeholder, no null entries), the family predicates and protocol selection
(including the frequency caps, the legacy 115200 clamp, which families support the `0x05` prepare
step and which are reachable over HID), and the packet framing (exact bytes and checksums, round
trips for every protocol, tolerance of leading noise, rejection of bad checksums and lost
terminators, waiting for a packet split across two reads, and status decoding that rejects the
wrong family or an implausible clock).

`tests/test_i18n.cpp` covers the message table: every `{n}` slot in a translation matches the
English source, no prose entry was left untranslated, the text the binary reports as its language is
the text it actually prints, and the log markers that separate packet dumps from progress lines
survive translation. It runs against whichever language the build was configured for, so build both
`-DNEWISP_LANG=zh-CN` and `-DNEWISP_LANG=en` before release.

`tests/cli_preflight_test.py` deliberately asserts on exit codes, hex addresses and other untranslated
tokens rather than on English sentences, so it passes for either language.

On a Chinese Windows, Python decodes the tool's UTF-8 output with code page 936 and raises
`UnicodeDecodeError`; `tests/CMakeLists.txt` sets `PYTHONUTF8=1` for the CTest entries to avoid
that. If you run the script by hand, set the same variable.

### Protocol test over a pseudo-terminal

| Test / script | Purpose |
| --- | --- |
| `tests/pty_test.py` | End-to-end protocol test. Creates a pseudo-terminal (PTY) pair, runs a scripted fake STC chip on the master side, and drives the real binary against it. Validates the full burn sequence: wakeup, probe, calibration handshake, `0x05` prepare, erase, 64-byte block writes, option-byte write. Run with `python3 tests/pty_test.py /path/to/newisp`. **Linux/macOS only** (needs a PTY). |
| `tests/pty_diag.py` | Reports which termios settings a tty actually accepts (useful when a driver rejects parity). |
| `tests/pty_opts_dump.py` | Dumps the exact option-block bytes the tool writes, with decoded frequency and trim fields. |
| `scripts/wsl-build.sh` | Build inside WSL. |
| `scripts/wsl-tests.sh` | Configure with tests enabled, build and run them inside WSL. |
| `scripts/wsl-smoke.sh` | Smoke test inside WSL. |
| `scripts/wsl-ptytest.sh` | Run the PTY protocol test inside WSL. |
| `scripts/win-build.ps1` | Windows build helper. |
| `scripts/win-smoke.ps1` | Windows smoke test. |

## Architecture

```
include/newisp/
  IChannel.h            transport interface (Open/Write/Read/Flush/SetBaudRate/DTR/RTS)
  serial_channel.h      serial backend
  hid_channel.h         USB HID backend
  stc_protocol.h        protocol base: packet framing, wait/pulse helpers, shared formatting
  protocol_stc89.h      STC89/90 (ProtocolStc89 legacy 1-byte-checksum framing, ProtocolStc89A)
  protocol_stc12.h      STC10/11/12 (12A/12B variants)
  protocol_stc15.h      STC15 (15A variant)
  protocol_stc8_uart.h  STC32/STC8G/STC8 over serial
  protocol_stc8_hid.h   STC32/STC8G/STC8 over USB HID
  protocol_factory.h    chip name -> protocol
  chip_table.h          chip ID (magic) -> part name, ~1300 entries
  chip_logic.h          family predicates, frequency and baud tables
  hex_file.h            Intel HEX parser
  session.h             detect/burn sequences
  options.h             CLI options
  text.h                UTF-8 text helpers, hex formatting, monotonic clock

src/core/               platform-independent: chip table, chip logic, HEX parser
src/transport/          platform-specific: serial (Win32 or termios), USB HID
src/cli/                argument parsing, session driver, main
tests/                  unit tests and the PTY protocol test
```

The protocol layer is written against `stcisp::IChannel` and never names a concrete transport.

The STC32/STC8G/STC8 families have a separate class per transport, because their status framing and
calibration differ genuinely. STC89, STC12 and STC15 are serial-only.

### Transport backends

| Platform | Serial | USB HID |
| --- | --- | --- |
| Windows | `CreateFile` + `DCB` + `EscapeCommFunction` | `setupapi.dll` + `hid.dll` (system HID class driver) |
| macOS | `termios` + `TIOCMGET`/`TIOCMSET` | hidapi (IOKit) |
| Linux | `termios` + `TIOCMGET`/`TIOCMSET` | hidapi (hidraw) |

The Windows HID backend uses the system HID class driver directly rather than hidapi, so the Windows
build has no external dependency at all. On macOS and Linux, hidapi is packaged everywhere and keeps
the code short.

Serial enumeration is platform-specific too: the SetupAPI port class on Windows, IOKit
`IOSerialBSDClient` services on macOS, and a scan of `/dev` cross-checked against sysfs on Linux.

One behavioural note carried over from the original: opening a serial port asserts DTR on most
platforms, and on many USB-serial bridges (CH340, CP2102, FT232) that is wired to the board's reset
line. Both backends therefore clear DTR and RTS immediately after opening, so the board is not held
in reset before the reset pulse is sent deliberately.

Parity is applied on a best-effort basis. Some tty devices — pseudo-terminals in particular, and a
few USB bridge drivers — reject it with `EINVAL`. When that happens the speed and framing still
take effect, the port records the parity it actually has, and `SetParity` reports the truth rather
than claiming success. The older STC families run at 8N1 anyway.

## Hardware notes

### Clock range

The selectable IRC range stops at **45 MHz**, the vendor's validated ceiling. There is no overclock
option, because the frequencies above it were never on solid ground:

- 46 and 47 MHz were measured to work on an AI8051U34K64
- **48 MHz was measured to drop the clock** rather than raise it, so the physical IRC limit sits
  just under 48 MHz
- 49 and 50 MHz were extrapolated from a fitted curve, not measured

A target the chip cannot reach is worse than a missing option: the program runs at an unknown speed
and the only symptom is timing that is subtly wrong. A `-F` above 45 MHz is therefore rejected with
an explanation rather than silently clamped.

The STC8G and STC15 families top out at 35 MHz; a request above that is brought down with a note.

### Option bytes

Option bytes carry the watchdog, reset behaviour and clock configuration. A wrong
value can leave a chip unable to start. Do not interpret the displayed MSR as a
universal preservation guarantee: the current HID implementation constructs an
option block using constants and a trim table measured on a different family.
This is **not validated for STC32**, whose programming path is now refused.
This change does not validate or redesign other families' option programming.

### Serial auto-reset

If the board has no auto-reset circuit (DTR wired to reset), the user must power-cycle the board
during the handshake.

### EEPROM split

Only the STC32 relatives expose a user-selectable EEPROM/code split. Setting `-e` for a family that
does not support it is ignored.

## Verification status

What has actually been exercised for this port, so the state of the code is not overstated:

| Platform | Build | Unit tests | Smoke test | Real hardware |
| --- | --- | --- | --- | --- |
| Linux (Ubuntu 22.04, GCC 11, CMake 3.22) | yes | yes, all passing | yes | PTY protocol test passes; AI8051U34K64 burned over USB HID at 45 MHz |
| Windows (MSVC 19.44, CMake 4.4) | yes | not run | yes; device enumeration finds real ports and HID devices | AI8051U34K64 burned over USB HID at 24/40/45 MHz **and over serial**; STC8H8K64U over serial at 460800 baud |
| macOS 13.1 (AppleClang 14, CMake 4.4) | yes | not run | yes; IOKit serial scan and hidapi enumeration both answer | AI8051U34K64 burned over USB HID at 45 MHz |

The Windows HID enumeration was checked against the machine's actual HID devices: 17 were found and
their product strings and vendor/product IDs decoded correctly.

The macOS build was done in a VM with the dependencies built from source (Homebrew no longer
installs on Intel Macs). Its serial list came back empty, which is correct for a VM with no serial
hardware, and the HID enumeration found the board:

```
未发现串口设备
STC ISP 设备 (VID 34BF PID 1001): 1 个
  USB-ISP [34BF:1001] (DevSrvsID:4294968331)
```

### Hardware burn test (AI8051U34K64 over USB HID)

A real AI8051U34K64 core board was programmed over the factory USB HID ISP interface. Every burn
completed with exit code 0 and the full sequence was acknowledged by the chip:

| Step | Result |
| --- | --- |
| Probe | identified `AI8051U34K64`, magic `0x78B4`, BSL 7.4U |
| `0x01` baud set | answered `01` |
| `0x05` prepare | answered `05` |
| `0x03` erase | answered `03`, returned the real UID `78 B4 C9 28 09 FA 88` |
| `0x02`/`0x22` program | 13 of 13 blocks for an 810-byte image, each answered `02 54` |
| `0x07` finish | answered `07 54` |
| `0x04` options | answered `04 54` |

Three target frequencies were written and each was then read back by re-probing the chip, which is
the real test: the chip reloads the option block and the frequency counter in its status packet
reports the new clock.

| Target | `opts[24..26]` count | trim written | Re-probed clock |
| --- | --- | --- | --- |
| 24 MHz | `0x016E36` | `48 20 01` | 24.000 MHz |
| 40 MHz | `0x02625A` | `73 30 01` | — |
| 45 MHz | `0x02AEA5` | `A6 30 01` | 44.999 MHz |

The 44.999 MHz reading is normal IRC tolerance, and the vendor ISP tool reports the same value for
the same option bytes. The written option block was verified byte for byte against the constants in
`protocol_stc8_hid.h`, including the frequency count, the reference-voltage trim and the trim triple.

The same 45 MHz burn was then repeated unchanged on Windows, Linux (through usbipd into WSL) and
macOS. All three read back the identical chip UID `78 B4 C9 28 09 FA 88`, which is the strongest
evidence that the protocol implementation matches across platforms.

### The serial path on real hardware

An STC8H8K64U was programmed over serial at 460800 baud. The calibration handshake ran for real:
the tool measured the IRC, derived `trim_adj=67, trim_range=0x20, trim_divider=1` and switched the
host to 460800, after which all 265 blocks of a 16920-byte image were written and the option block
was accepted.

### The 8051U does have a serial ISP monitor

The vendor documentation lists the 8051U family as programmable over USB HID only, and the tool
originally enforced that: a serial probe that identified an 8051U was refused with a message
telling the user to switch transport.

That rule was inherited rather than measured, and it is wrong. On an AI8051U34K64 with BSL 7.4U the
serial monitor answers the `0x7F` wakeup with a `0x50`-prefixed status packet, the calibration
handshake completes, and the whole sequence runs:

```
[1/7] 握手成功
[2/7] 已切到 115200，trim_adj=90, trim_range=0x20, trim_freq=24031200 Hz
[3/7] 准备编程成功
[4/7] 擦除成功 → UID 78 B4 C9 28 09 FA 88
[5/7] 13/13 块
[6/7] 选项写入成功
[7/7] 烧录完成！
```

Reading the chip back afterwards confirms the option block took effect: the status packet reports
24.031 MHz with the trim triple that was just written.

The refusal has been removed. What decides is the probe: a part that answers is programmable,
whatever the manual says about that transport. A part that does not answer fails on its own timeout
and nothing after that runs.

Note that the serial and HID status packets are not laid out identically -- the serial one carries a
`0x50` prefix and reports different MSR bytes -- which is why the two transports had separate
protocol implementations all along.

### A cross-platform bug that only hardware could expose

The port had a genuine bug that no amount of compiling or unit testing would have caught, because
it only appears when the same code talks to a real device through two different HID stacks:

| Platform | First byte returned by `hid_read` |
| --- | --- |
| Windows (`hid.dll`) | a **report ID** -- the HID stack reserves the first byte, so the payload starts at index 1 |
| Linux (`hidraw`) | **payload** -- the kernel hands over the report body only, so it starts at index 0 |

Skipping the first byte unconditionally shifted every packet by one on Linux, so the `46 B9` frame
marker never landed where the parser expected it and every probe reported "no chip detected".

The fix does not encode an assumption about the backend. It looks at the data: an STC packet always
begins with `46 B9`, so whichever index holds that marker is where the payload starts. That works on
all three platforms and tolerates a future change in hidapi's behaviour.

## Limitations

- The PTY test validates the **host side** of the wire protocol: framing, checksums, command
  sequence and reply handling. Its fake chip answers the STC32-family serial protocol, so the older
  families are covered by the unit tests rather than end to end.
- The PTY test is Linux/macOS only, because it needs a real terminal device pair. There is no
  equivalent on Windows, where the serial backend is a different API entirely.
- Serial burning has been exercised on Windows (COM13, STC8H8K64U, 460800 baud) and inside WSL
  through usbipd, but not on a physical Linux host with a real adapter.
- The macOS unit tests have not been run yet. The build and a real burn both succeeded there.
- The binaries are not "double-click and go". They are command-line tools with no Apple developer
  signature; handing one to someone else requires clearing the quarantine attribute
  (`xattr -d com.apple.quarantine newisp`) and keeping the executable bit, and an Intel binary needs
  Rosetta 2 on Apple silicon.

## License

This project is licensed under the **GNU General Public License v3.0**. See [LICENSE](LICENSE).
