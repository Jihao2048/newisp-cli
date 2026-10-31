// newisp -- STC MCU programmer, cross-platform command line front end.
//
// Ported from the WinUI application of the same name. The protocol, chip tables
// and burn sequence are the same code, recompiled for Windows, macOS and Linux;
// what this file adds is argument handling, device selection and a logger that
// writes UTF-8 to a console.

#include "newisp/options.h"

#include "newisp/chip_logic.h"
#include "newisp/chip_table.h"
#include "newisp/hex_file.h"
#include "newisp/hid_channel.h"
#include "newisp/i18n.h"
#include "newisp/serial_channel.h"
#include "newisp/session.h"
#include "newisp/text.h"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

    std::atomic<bool> g_stopRequested{ false };

    void OnSignal(int)
    {
        g_stopRequested.store(true);
    }

    // ---------------------------------------------------------------------
    //  Console setup
    // ---------------------------------------------------------------------
    //
    // Windows consoles default to the OEM code page, which cannot represent the
    // Chinese log messages. Switching the output code page to UTF-8 is the same
    // thing the tool does for the file system paths below.
    void ConfigureConsole()
    {
#if defined(_WIN32)
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);

        // Enable ANSI escape processing so the progress line can overwrite
        // itself. Best effort: older consoles simply ignore the sequences.
        HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        if (out != INVALID_HANDLE_VALUE) {
            DWORD mode = 0;
            if (GetConsoleMode(out, &mode)) {
                SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
#endif
    }

    bool StdoutIsTty()
    {
#if defined(_WIN32)
        HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        if (out == INVALID_HANDLE_VALUE) return false;
        DWORD mode = 0;
        return GetConsoleMode(out, &mode) != 0;
#else
        return isatty(fileno(stdout)) != 0;
#endif
    }

#if defined(_WIN32)
    // Convert one UTF-16 command-line argument to UTF-8.
    //
    // This is the reason the Windows entry point is wmain rather than main:
    // argv from main() is encoded in the system ANSI code page, which cannot
    // represent a path such as 烧录测试文件, so a firmware path would arrive
    // already mangled and could not be printed correctly or passed on to the
    // wide-character file APIs. wmain hands us UTF-16, which is the real
    // representation Windows uses internally.
    std::string WideToUtf8(const wchar_t* w)
    {
        if (!w || !*w) return std::string();
        int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0,
            nullptr, nullptr);
        if (n <= 1) return std::string();
        std::string s((size_t)(n - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
        return s;
    }
#endif

    // ---------------------------------------------------------------------
    //  Logger
    // ---------------------------------------------------------------------
    //
    // The session layer emits two very different kinds of line: human progress
    // ("[4/7] erase complete") and raw packet dumps ("[send] 0x03: ...").
    // Verbose shows both, Normal hides the dumps, and Quiet shows only what the
    // summary needs.
    //
    // The kind travels with the line as a short marker that the i18n layer
    // attaches when the message is built (see newisp/i18n.h). That is what
    // makes this filter independent of the UI language -- the original port
    // matched Chinese prefixes, which stops working the moment the text is
    // translated.
    class Logger {
    public:
        explicit Logger(newisp::OutputLevel level) : m_level(level) {}

        void operator()(const std::string& line)
        {
            using newisp::i18n::LogKind;
            using newisp::i18n::SplitLogLine;

            LogKind kind = SplitLogLine(line).kind;

            // Packet dumps are noisy and only useful when debugging a
            // handshake, so they are opt-in.
            if (kind == LogKind::Packet && m_level != newisp::OutputLevel::Verbose)
                return;

            // Quiet keeps the findings a user must see even when everything
            // asked for something short: errors, warnings and the attention
            // notes the session raises.
            if (m_level == newisp::OutputLevel::Quiet && kind == LogKind::Normal)
                return;

            std::cout << newisp::i18n::StripLogMarker(line) << '\n';
            std::cout.flush();
        }

        bool Detailed() const { return m_level == newisp::OutputLevel::Verbose; }
        bool Quiet() const { return m_level == newisp::OutputLevel::Quiet; }

    private:
        newisp::OutputLevel m_level = newisp::OutputLevel::Normal;
    };

    // Progress goes to stderr so it cannot corrupt redirected stdout, and it
    // overwrites itself with a carriage return when attached to a terminal.
    //
    // Written through std::cerr for the same reason as the header lines: printf
    // would route the Chinese text through the Windows ANSI code page.
    class ProgressBar {
    public:
        explicit ProgressBar(bool enabled) : m_enabled(enabled) {}

        void operator()(int percent)
        {
            if (!m_enabled) return;
            if (percent == m_last) return;
            m_last = percent;

            // "\033[K" clears to end of line so a shorter value does not leave
            // digits behind from the previous one.
            std::cerr << "\r" << newisp::i18n::Tr(newisp::i18n::Msg::ProgressWrite,
                std::to_string(percent)) << "\033[K";
            std::cerr.flush();
            if (percent >= 100) std::cerr << '\n';
        }

    private:
        bool m_enabled = false;
        int  m_last = -1;
    };

    // ---------------------------------------------------------------------
    //  Device listing
    // ---------------------------------------------------------------------
    //
    // What `list` shows: which serial ports and which HID devices exist. No
    // handshake is sent, nothing is opened, so it is instant and cannot disturb
    // a board that is already in the ISP monitor.

    void PrintSerialPorts()
    {
        auto ports = stcisp::EnumerateSerialPorts();
        if (ports.empty()) {
            std::cout << newisp::i18n::Tr(newisp::i18n::Msg::CliNoSerialPorts) << "\n";
            return;
        }
        std::cout << newisp::i18n::Tr(newisp::i18n::Msg::CliSerialPorts,
            std::to_string(ports.size())) << "\n";

        // One row per port, in the shape "COM12  STC USB Serial", matching what
        // the WinUI build shows in its device list: the port the user types,
        // then the name of the thing behind it.
        //
        // A driver name that already ends in "(COM12)" would repeat the port,
        // so the trailing parenthesised part is dropped before printing; that
        // is what turns "BluetoothLE-Serial port (COM8)" into a name that sits
        // in the same column as "STC USB Serial".
        for (const auto& p : ports) {
            std::cout << "  " << p.device;
            std::string name = p.displayName;
            const std::string suffix = " (" + p.device + ")";
            if (name.size() > suffix.size() &&
                name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
                name.resize(name.size() - suffix.size());
            }
            if (!name.empty() && name != p.device)
                std::cout << "  " << name;
            std::cout << '\n';
        }
    }

    // PrintHidDevices lists what matters by default and everything on request.
    //
    // A typical desktop has a dozen or more HID devices -- mouse, touchpad,
    // keyboard, vendor utilities -- and listing them all buries the one board
    // the user is looking for. The default therefore shows only devices from
    // the STC vendor ID, which covers both the ISP interface (34BF:1001) and
    // the vendor's other HID functions, such as the USB-to-serial bridge
    // (34BF:FF09). Verbose mode still lists every HID device, which is what you
    // want when a board is not appearing where you expect and you need to see
    // whether it enumerated at all.
    void PrintHidDevices(bool verbose)
    {
        if (!stcisp::HidSupported()) {
            std::cout << newisp::i18n::Tr(newisp::i18n::Msg::CliHidNotBuilt) << "\n";
            return;
        }

        auto stc = stcisp::EnumerateHidDevices(stcisp::STC_HID_VID, 0);
        auto isp = stcisp::EnumerateHidDevices(stcisp::STC_HID_VID,
            stcisp::STC_HID_PID);

        std::cout << newisp::i18n::Tr(newisp::i18n::Msg::CliStcIspDevices, {
            newisp::Hex(stcisp::STC_HID_VID, 4),
            newisp::Hex(stcisp::STC_HID_PID, 4),
            std::to_string(isp.size()) }) << "\n";
        for (const auto& d : isp) {
            std::cout << "  " << newisp::FormatHidDevice(d) << '\n';
        }

        // Other devices from the same vendor, with the ISP interface already
        // accounted for above; these are usually the USB-serial bridges.
        std::vector<stcisp::HidDeviceInfo> other;
        for (const auto& d : stc) {
            if (d.pid == stcisp::STC_HID_PID) continue;
            other.push_back(d);
        }
        if (!other.empty()) {
            // These are the vendor's own HID devices other than the ISP
            // interface. The most common is the STC USB-to-serial bridge
            // (34BF:FF09), which despite the name "USB Serial" is a HID device
            // and not a USB CDC/ACM port: it does not appear in the serial
            // list above and cannot be opened as a COM port or a tty. It is
            // listed here so the device is accounted for rather than looking
            // like the tool missed it.
            std::cout << "\n" << newisp::i18n::Tr(newisp::i18n::Msg::CliOtherStcHid, {
                          newisp::Hex(stcisp::STC_HID_VID, 4),
                          std::to_string(other.size()) }) << "\n";
            for (const auto& d : other) {
                std::cout << "  " << newisp::FormatHidDevice(d) << '\n';
            }
        }

        if (!verbose) {
            if (isp.empty() && other.empty()) {
                std::cout << "\n" << newisp::i18n::Tr(newisp::i18n::Msg::CliNoStcDevices) << "\n";
            }
            return;
        }

        // Verbose mode lists everything, which is what you want when a board is
        // not where you expect it: seeing that it enumerated under an
        // unexpected name is the whole point. The header repeats the count so
        // the list can be skipped at a glance.
        auto all = stcisp::EnumerateHidDevices(0, 0);
        std::cout << "\n" << newisp::i18n::Tr(newisp::i18n::Msg::CliAllHidDevices,
                          std::to_string(all.size())) << "\n";
        for (const auto& d : all) {
            std::cout << "  " << newisp::FormatHidDevice(d) << '\n';
        }
    }

    // ---------------------------------------------------------------------
    //  Device selection
    // ---------------------------------------------------------------------

    // Resolve which device to talk to.
    //
    // An explicit --device always wins. Otherwise, if exactly one candidate
    // exists, use it; if several do, refuse and list them, because picking one
    // at random can program the wrong board.
    bool ResolveDevice(newisp::BurnOptions& burn, bool useHid,
        std::string& error)
    {
        if (!burn.deviceId.empty()) return true;

        if (useHid) {
            if (!stcisp::HidSupported()) {
                error = newisp::i18n::Tr(newisp::i18n::Msg::CliErrNoHidSupport);
                return false;
            }
            auto devs = stcisp::EnumerateHidDevices(stcisp::STC_HID_VID,
                stcisp::STC_HID_PID);
            if (devs.empty()) {
                error = newisp::i18n::Tr(newisp::i18n::Msg::CliErrNoIspDevice);
                return false;
            }
            if (devs.size() > 1) {
                error = newisp::i18n::Tr(newisp::i18n::Msg::CliErrManyIspDevices);
                for (const auto& d : devs)
                    error += "\n  " + newisp::FormatHidDevice(d);
                return false;
            }
            burn.deviceId = devs.front().devicePath;
            return true;
        }

        auto ports = stcisp::EnumerateSerialPorts();
        if (ports.empty()) {
            error = newisp::i18n::Tr(newisp::i18n::Msg::CliErrNoSerialPorts);
            return false;
        }
        if (ports.size() > 1) {
            error = newisp::i18n::Tr(newisp::i18n::Msg::CliErrManySerial);
            for (const auto& p : ports) {
                error += "\n  " + p.device;
                if (!p.displayName.empty() && p.displayName != p.device)
                    error += "  -- " + p.displayName;
            }
            return false;
        }
        burn.deviceId = ports.front().device;
        return true;
    }

} // namespace

// The real entry point, shared by both platform wrappers below. It takes
// UTF-8 arguments on every platform, which is what the rest of the program
// assumes.
static int RunMain(const std::vector<std::string>& args)
{
    ConfigureConsole();

    newisp::CliOptions opts;
    std::string error;

    // ParseCommandLine wants a C-style argv; the strings themselves are what
    // matter and they are already UTF-8.
    std::vector<const char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& a : args) argv.push_back(a.c_str());
    argv.push_back(nullptr);

    const char* programName =
        args.empty() ? "newisp" : args.front().c_str();

    int rc = newisp::ParseCommandLine((int)args.size(),
        const_cast<char**>(argv.data()), opts, error);
    if (rc != 0) {
        std::cerr << newisp::i18n::Tr(newisp::i18n::Msg::CliErrPrefix, error) << "\n\n"
                  << newisp::UsageText(programName);
        return 2;
    }

    switch (opts.command) {
    case newisp::Command::None:
        // No arguments: this is someone who ran the tool to find out what it
        // is. Point them at the two commands that answer that -- help for the
        // manual and detect to probe whatever is plugged in -- instead of
        // dumping a device listing and the whole manual on a bare launch.
        // Keep the prompt short by showing only the basename, the same way
        // UsageText does for its own examples.
        {
            std::string name = programName;
            size_t slash = name.find_last_of("/\\");
            if (slash != std::string::npos && slash + 1 < name.size())
                name = name.substr(slash + 1);
            if (name.empty()) name = "newisp";
            std::cout << newisp::i18n::Tr(newisp::i18n::Msg::CliInvite, name) << '\n';
        }
        return 0;

    case newisp::Command::Help:
        std::cout << newisp::UsageText(programName);
        return 0;

    case newisp::Command::Version:
        std::cout << "newisp " << newisp::i18n::BuildVersion() << '\n'
                  << newisp::i18n::Tr(newisp::i18n::Msg::VersionProtocols) << '\n'
#if NEWISP_HAVE_HID
                  << newisp::i18n::Tr(newisp::i18n::Msg::VersionTransportsHid)
#else
                  << newisp::i18n::Tr(newisp::i18n::Msg::VersionTransports)
#endif
                  << '\n';
        return 0;

    case newisp::Command::List:
        PrintSerialPorts();
        std::cout << '\n';
        PrintHidDevices(opts.output == newisp::OutputLevel::Verbose);
        return 0;

    case newisp::Command::Detect:
    case newisp::Command::Inspect:
    case newisp::Command::Burn:
        break;
    }

    // Validate firmware BEFORE device enumeration/opening, so a bad file or
    // unsupported address map cannot reach handshake, erase or option writes.
    stc::HexImage image;
    std::vector<uint8_t> firmware;
    if (opts.command == newisp::Command::Burn || opts.command == newisp::Command::Inspect) {
        if (!stc::LoadHexFile(opts.firmwarePath, image, error)) {
            std::cerr << "Error: " << error << '\n';
            return 1;
        }
        if (opts.command == newisp::Command::Inspect) {
            std::cout << "Intel HEX: valid; " << image.data.size() << " data bytes\n";
            auto it = image.data.begin();
            while (it != image.data.end()) {
                uint32_t first = it->first, last = first;
                size_t count = 1;
                ++it;
                while (it != image.data.end() && static_cast<uint64_t>(last) + 1 == it->first &&
                       (first >> 16) == (it->first >> 16)) {
                    last = it->first;
                    ++count;
                    ++it;
                }
                std::cout << "  0x" << newisp::Hex(first, 8) << "-0x"
                          << newisp::Hex(last, 8) << " : " << count << " bytes\n";
            }
            if (image.entryType)
                std::cout << "Entry metadata: type " << static_cast<unsigned>(image.entryType)
                          << ", value 0x" << newisp::Hex(image.entryValue, 8) << '\n';
            if (stc::BuildLinearFirmware(image, firmware, error))
                std::cout << "Address map: 16-bit linear; target capacity/options still require validation\n";
            else
                std::cout << "Programming unavailable: " << error << '\n';
            std::cout << "Offline inspection only; no device enumerated, opened, reset or written\n";
            return 0;
        }
        if (!stc::BuildLinearFirmware(image, firmware, error)) {
            std::cerr << "Error: " << error << '\n';
            return 1;
        }
    }

    // ---- shared setup for detect and burn ----
    bool useHid = opts.burn.useHid;
    if (!ResolveDevice(opts.burn, useHid, error)) {
        if (opts.command == newisp::Command::Detect) {
            // detect prints its error on stdout, the way it prints everything
            // else: a script reading the answer should not have to merge
            // streams to see why there was none.
            std::cout << error << '\n';
        } else {
            std::cerr << newisp::i18n::Tr(newisp::i18n::Msg::CliErrPrefix, error) << '\n';
        }
        return 1;
    }

    // A Ctrl-C during a long handshake should stop it cleanly rather than
    // leaving the port open.
    std::signal(SIGINT, OnSignal);
#if defined(SIGTERM)
    std::signal(SIGTERM, OnSignal);
#endif

    const bool tty = StdoutIsTty();
    // When stdout is not a terminal the packet dumps would be lost in a log
    // file anyway, so a redirected run without an explicit -output is treated
    // as verbose: dumps are cheap to grep and unreadable if dropped.
    const newisp::OutputLevel level = (!opts.outputExplicit && !tty)
        ? newisp::OutputLevel::Verbose
        : opts.output;
    Logger log(level);
    ProgressBar progress(tty && !log.Quiet());

    std::cout << newisp::i18n::Tr(newisp::i18n::Msg::DeviceLine,
                      opts.burn.deviceId) << '\n';
    std::cout << newisp::i18n::Tr(newisp::i18n::Msg::TransportLine,
                      useHid ? "USB HID"
                             : newisp::i18n::Tr(newisp::i18n::Msg::TransportSerial))
              << '\n';

    if (opts.command == newisp::Command::Detect) {
        // Identify the chip and stop. Nothing about programming is decided
        // here: no frequency, no EEPROM split, no option bytes are written, and
        // the chip is released back to its user code afterwards.
        int r = newisp::RunDetect(opts.burn, log);
        return r;
    }

    // ---- burn ----
    // Print through std::cout rather than printf: on Windows printf writes
    // through the ANSI code page, so a firmware path containing non-ASCII
    // characters (a Chinese directory name, say) comes out mangled. The stream
    // operators pass the UTF-8 bytes through unchanged, and the console code
    // page was already set to UTF-8 above.
    std::cout << newisp::i18n::Tr(newisp::i18n::Msg::FirmwareLine, {
                      opts.firmwarePath, std::to_string(firmware.size()),
                      newisp::Hex(image.minAddr, 4),
                      newisp::Hex(image.maxAddr, 4) }) << '\n';

    if (firmware.empty()) {
        std::cerr << newisp::i18n::Tr(newisp::i18n::Msg::CliFirmwareEmpty) << "\n";
        return 1;
    }

    if (opts.burn.targetFreq)
        std::cout << newisp::i18n::Tr(newisp::i18n::Msg::TargetFreqLine,
                          newisp::FormatDouble(opts.burn.targetFreq / 1e6, 4)) << '\n';
    if (!useHid)
        std::cout << newisp::i18n::Tr(newisp::i18n::Msg::BaudLine,
                          std::to_string(opts.burn.baudRate)) << '\n';
    if (opts.burn.eepromBytes)
        std::cout << newisp::i18n::Tr(newisp::i18n::Msg::EepromLine,
                          std::to_string(opts.burn.eepromBytes)) << '\n';

    int r = newisp::RunBurn(opts.burn, firmware, log, progress);
    if (r == 0) {
        std::cout << newisp::i18n::Tr(newisp::i18n::Msg::BurnSucceeded) << "\n";
    }
    else {
        std::cerr << newisp::i18n::Tr(newisp::i18n::Msg::BurnFailed) << "\n";
    }
    return r;
}

#if defined(_WIN32)

// Windows entry point: UTF-16 arguments, converted to UTF-8 once at the top.
//
// Using wmain instead of main is what makes a firmware path such as
// C:\cmdisp\newisp\烧录测试文件\ai8051u.hex survive: main() would hand us the
// path in the system ANSI code page, which mangles those characters before the
// program can do anything about it.
int wmain(int argc, wchar_t** argv)
{
    std::vector<std::string> args;
    args.reserve((size_t)argc);
    for (int i = 0; i < argc; ++i) args.push_back(WideToUtf8(argv[i]));
    if (args.empty()) args.push_back("newisp");

    return RunMain(args);
}

#else

// POSIX entry point: argv is already whatever byte encoding the user's locale
// uses, and on every modern Linux and macOS system that is UTF-8.
int main(int argc, char** argv)
{
    std::vector<std::string> args;
    args.reserve((size_t)argc);
    for (int i = 0; i < argc; ++i) args.push_back(argv[i] ? argv[i] : "");
    if (args.empty()) args.push_back("newisp");

    return RunMain(args);
}

#endif
