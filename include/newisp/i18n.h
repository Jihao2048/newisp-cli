#pragma once
// User-facing text selection.
//
// Every string the tool shows a person goes through this header: the usage
// banner, device listings, session progress, and the log lines the protocol
// implementations emit.
//
// The language is chosen when the program is built, not when it runs
//
//   cmake -DNEWISP_LANG=zh-CN   (Chinese, the default)
//   cmake -DNEWISP_LANG=en      (English)
//
// and only that table is compiled into the binary: the other language's text
// does not exist in the executable at all. That is deliberate for a programmer
// that ships as one small self-contained tool -- there is no locale lookup at
// start-up, no runtime table switching, and nothing to misconfigure on the
// target. The build turns the setting into NEWISP_LANG_CODE_ENGLISH or
// NEWISP_LANG_CODE_CHINESE for src/core/i18n.cpp.
//
// The trade-off is that one binary cannot be switched to the other language at
// run time. That is why the release builds produce two binaries of the same
// source (see CMakeLists.txt) rather than one binary with two tables.
//
// Naming
// ------
// Message identifiers are named after their English wording: Msg::SerialPorts
// is "Serial ports", Msg::EraseOk is "Erase complete". A translation table is
// then read side by side with the English one and a mismatch is obvious.
//
// Format slots
// ------------
// A message may contain {0}, {1}, ... placeholders. Tr() substitutes them in
// order, and a translation must use the same set of slots; the unit tests
// check that, so a translator cannot silently drop a value.

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>

namespace newisp {
namespace i18n {

    // What a log line is for, so the caller can decide whether to print it.
    //
    // The WinUI original had log levels; the console port dropped them and
    // filtered on the Chinese prefix of the message text instead. This brings
    // back the information the prefix was standing in for.
    enum class LogKind {
        Normal,     // ordinary progress line
        Packet,     // raw packet dump, shown only with --verbose
        Warning,    // something suspicious that was worked around
        Error,      // a step that required intervention failed
        Attention,  // a finding worth reading even with --quiet
    };

    // ---------------------------------------------------------------------
    //  Message table
    // ---------------------------------------------------------------------
    //
    // The identifiers are declared in text_en.inc so the English wording, the
    // translation and this enum can never drift apart. The macro is defined
    // next to its only expansion and #undef'd immediately: text_en.inc itself
    // only lists entries.
    enum class Msg : uint16_t {
#define NEWISP_TEXT_EN(id, kind, text) id,
#include "newisp/text_en.inc"
#undef NEWISP_TEXT_EN
    };

    // The language this binary was built for, as a BCP-47 style code:
    // "zh-CN" or "en".
    const char* BuildLanguage();

    // The version string the build stamped in (NEWISP_VERSION). Exposed here
    // rather than read from the macro at each use, because the macro expands to
    // a run of tokens that only a few syntactic positions accept as a string.
    const char* BuildVersion();

    // English source text for a message, regardless of the build language.
    // Used by the tests to check the two tables against each other.
    const char* English(Msg msg);

    // English source text together with the kind most callers want for it.
    const char* English(Msg msg, LogKind& kind);

    // The message in the build language, with {n} slots substituted. A slot
    // with no matching argument is left as it is, which is visible rather than
    // silent.
    std::string Tr(Msg msg,
        std::initializer_list<std::string> args = {});

    // Convenience for the one-slot case, which is most of them.
    std::string Tr(Msg msg, const std::string& arg0);

    // ----------------------------------------------------------
    //  Logging through a LogFn callback
    // ----------------------------------------------------------
    //
    // stc::LogFn hands back a plain string, so the kind has to survive as a
    // compact prefix while the line travels from the protocol layer to the
    // logger. Two printable markers are used:
    //
    //   "%p"  packet dump   (shown only with --verbose)
    //   "%a"  attention     (shown even with --quiet)
    //
    // Warnings and errors are ordinary text; the logger does not filter them.
    // The markers are stripped before anything is written to the console.
    extern const char kPacketMarker[];
    extern const char kAttentionMarker[];

    // Prefix for a packet dump that continues a previous line: a long write is
    // logged as "<header>" followed by an indented hex dump. It carries the
    // packet marker so the logger still hides it without --verbose.
    extern const char kPacketContinuation[];

    // Prefix used by every failure line, in every language, so that a failure
    // is greppable regardless of the build language. Messages that are
    // translated spell it out in their own text; this constant is for the few
    // lines whose text is produced elsewhere.
    extern const char kErrorMarker[];

    // Build "<marker><text in the build language>".
    std::string LogLine(Msg msg, LogKind kind = LogKind::Normal);
    std::string LogLine(Msg msg, const std::string& arg0,
        LogKind kind = LogKind::Normal);
    std::string LogLine(Msg msg, std::initializer_list<std::string> args,
        LogKind kind = LogKind::Normal);

    // Split a line produced by LogLine back into its marker and its text.
    // `text` is a pointer into `line`, so it shares its lifetime.
    struct LogLineParts {
        LogKind    kind = LogKind::Normal;
        const char* text = "";
        size_t      prefixLength = 0;   // bytes to skip before text
    };
    LogLineParts SplitLogLine(const std::string& line);

    // The same line with its marker removed, for callers that do not care
    // about the kind.
    std::string StripLogMarker(const std::string& line);

} // namespace i18n
} // namespace newisp
