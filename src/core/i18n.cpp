#include "newisp/i18n.h"

#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

// Only the language this binary speaks is compiled in. CMake sets exactly one
// of these from the NEWISP_LANG option; a source file compiled outside the
// build system gets the fallback below, which matches the default in i18n.h.
#if !defined(NEWISP_LANG_CODE_ENGLISH) && !defined(NEWISP_LANG_CODE_CHINESE)
#define NEWISP_LANG_CODE_CHINESE 1
#endif

#if defined(NEWISP_LANG_CODE_ENGLISH) && defined(NEWISP_LANG_CODE_CHINESE)
#error "build configuration error: both languages selected"
#endif

namespace newisp {
namespace i18n {

    namespace {

        struct Entry {
            const char* text;
            LogKind     kind;
        };

        // The English source table. It is always compiled, because it is also
        // what the tests read to check a translation entry by entry, and it is
        // small next to the protocol code.
        //
        // An English build returns these strings directly, so the binary has no
        // second copy of anything and Tr() costs one index.
        const Entry kEnglish[] = {
#define NEWISP_TEXT_EN(id, kind, text) { text, LogKind::kind },
#include "newisp/text_en.inc"
#undef NEWISP_TEXT_EN
        };

        constexpr size_t kMessageCount =
            sizeof(kEnglish) / sizeof(kEnglish[0]);

#if defined(NEWISP_LANG_CODE_CHINESE)
        // The Chinese table, listed with the same identifiers in the same
        // order. The explicit size turns a missing or extra entry into a
        // compile error instead of shifting every following message.
        const char* const kChinese[kMessageCount] = {
#define NEWISP_TEXT_EN(id, kind, text) text,
#include "../i18n/text_zh_cn.inc"
#undef NEWISP_TEXT_EN
        };

        // Present only in a Chinese build: the text a Chinese user sees, with
        // the English table as the safety net for an empty entry.
        const char* LocalText(size_t i)
        {
            if (i >= kMessageCount) return "";
            const char* zh = kChinese[i];
            if (zh && zh[0]) return zh;
            return kEnglish[i].text;
        }
#else
        // English build: no second table exists at all.
        const char* LocalText(size_t i)
        {
            return (i < kMessageCount) ? kEnglish[i].text : "";
        }
#endif

        // ---- slot substitution ----
        //
        // {n} is the only slot syntax. A slot without a matching argument is
        // left verbatim, which makes the mistake visible in the output instead
        // of silently dropping text.
        std::string Substitute(const char* fmt,
            std::initializer_list<std::string> args)
        {
            std::string out;
            out.reserve(std::strlen(fmt) + 32);

            const std::vector<std::string> values(args.begin(), args.end());

            for (const char* p = fmt; *p; ++p) {
                if (*p != '{') { out += *p; continue; }

                // Try to read "{digits}".
                const char* q = p + 1;
                size_t index = 0;
                bool digits = false;
                while (*q >= '0' && *q <= '9') {
                    digits = true;
                    index = index * 10 + (size_t)(*q - '0');
                    if (index > 1000) break;
                    ++q;
                }
                if (!digits || *q != '}' || index >= values.size()) {
                    out += *p;
                    continue;
                }

                out += values[index];
                p = q;
            }
            return out;
        }

        const char* MarkerFor(LogKind kind)
        {
            switch (kind) {
            case LogKind::Packet:    return "%p";
            case LogKind::Attention: return "%a";
            case LogKind::Normal:
            case LogKind::Warning:
            case LogKind::Error:
            default:                 return "";
            }
        }

        LogKind KindFromMarker(const char* marker)
        {
            if (marker && marker[0] == '%') {
                if (marker[1] == 'p') return LogKind::Packet;
                if (marker[1] == 'a') return LogKind::Attention;
            }
            return LogKind::Normal;
        }

    } // namespace

    // ========================================================================
    //  Messages
    // ========================================================================

    const char* BuildVersion()
    {
#ifndef NEWISP_VERSION
#define NEWISP_VERSION "0.0.0"
#endif
        return NEWISP_VERSION;
    }

    const char* BuildLanguage()
    {
#if defined(NEWISP_LANG_CODE_ENGLISH)
        return "en";
#else
        return "zh-CN";
#endif
    }

    const char* English(Msg msg)
    {
        size_t i = (size_t)msg;
        if (i >= kMessageCount) return "";
        return kEnglish[i].text;
    }

    const char* English(Msg msg, LogKind& kind)
    {
        size_t i = (size_t)msg;
        if (i >= kMessageCount) { kind = LogKind::Normal; return ""; }
        kind = kEnglish[i].kind;
        return kEnglish[i].text;
    }

    std::string Tr(Msg msg, std::initializer_list<std::string> args)
    {
        size_t i = (size_t)msg;
        if (i >= kMessageCount) return std::string();
        return Substitute(LocalText(i), args);
    }

    std::string Tr(Msg msg, const std::string& arg0)
    {
        return Tr(msg, { arg0 });
    }

    // ========================================================================
    //  Marked log lines
    // ========================================================================

    const char kPacketMarker[] = "%p";
    const char kAttentionMarker[] = "%a";
    const char kPacketContinuation[] = "%p     ";
    const char kErrorMarker[] = "[Failed] ";

    std::string LogLine(Msg msg, LogKind kind)
    {
        return std::string(MarkerFor(kind)) + Tr(msg);
    }

    std::string LogLine(Msg msg, const std::string& arg0, LogKind kind)
    {
        return std::string(MarkerFor(kind)) + Tr(msg, arg0);
    }

    std::string LogLine(Msg msg, std::initializer_list<std::string> args,
        LogKind kind)
    {
        return std::string(MarkerFor(kind)) + Tr(msg, args);
    }

    LogLineParts SplitLogLine(const std::string& line)
    {
        LogLineParts parts;

        // Two-byte markers only; keep the comparison explicit so that an
        // ordinary line starting with '%' cannot be mistaken for one.
        const char* s = line.c_str();
        if (line.size() >= 2 && s[0] == '%' && (s[1] == 'p' || s[1] == 'a')) {
            parts.kind = KindFromMarker(s);
            parts.prefixLength = 2;
        }
        parts.text = s + parts.prefixLength;
        return parts;
    }

    std::string StripLogMarker(const std::string& line)
    {
        LogLineParts parts = SplitLogLine(line);
        if (parts.prefixLength == 0) return line;
        return std::string(parts.text);
    }

} // namespace i18n
} // namespace newisp
