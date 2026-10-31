#pragma once
// Command-line option parsing.

#include <string>
#include <vector>

#include "newisp/i18n.h"
#include "newisp/session.h"

namespace newisp {

    // How much of the log to show.
    //
    // One setting rather than two independent flags: -output=quiet and
    // -output=verbose are opposites, so a separate -quiet and -verbose could be
    // given together with no sensible meaning. Making it a single choice also
    // means the three levels are exhaustive -- there is no state that is both
    // quiet and verbose.
    enum class OutputLevel {
        Quiet,      // errors, warnings and the final summary only
        Normal,     // the default: progress lines, no packet dumps
        Verbose,    // adds the raw packet dumps
    };

    // Parse an -output= value: quiet, normal, verbose, plus the usual short
    // spellings (q/n/v, and "default" for normal). Case-insensitive; returns
    // false when the word is not one of them.
    bool ParseOutputLevel(const std::string& text, OutputLevel& out);

    enum class Command {
        None,       // no command given; print usage
        List,       // list serial and HID devices
        Detect,     // probe and report the chip
        Inspect,    // validate firmware without enumerating/opening devices
        Burn,       // detect, erase, write, write options
        Help,
        Version,
    };

    struct CliOptions {
        Command     command = Command::None;
        BurnOptions burn{};

        std::string firmwarePath;
        bool        useHid = false;      // --hid / --serial
        bool        transportExplicit = false;

        // Raw values as given, before any series clamping.
        //
        // The clock source lives in burn.clockExternal / burn.clockExplicit and
        // the frequency in burn.targetFreq / burn.freqExplicit rather than
        // being duplicated here: the session layer is what decides whether an
        // option applies, and it only ever sees BurnOptions.

        std::string seriesName;   // -series stc32, etc.

        // Log verbosity, consumed by the logger in main.cpp.
        OutputLevel output = OutputLevel::Normal;
        bool        outputExplicit = false;

        // There is no language field: the output language is a build-time
        // choice (NEWISP_LANG in CMakeLists.txt) and only that table is in the
        // binary, so nothing here can change it.
    };

    // Parse argv into CliOptions. Returns 0 on success, non-zero when the
    // command line is malformed (in which case `error` explains why).
    int ParseCommandLine(int argc, char** argv, CliOptions& out,
        std::string& error);

    // The usage text, shared by --help and the malformed-input path.
    std::string UsageText(const char* programName);

} // namespace newisp
