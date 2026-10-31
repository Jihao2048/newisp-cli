// Command-line parsing.
//
// Hand-written rather than pulled from a library: the option set is small, the
// tool has to build with nothing but a C++17 compiler, and a dependency here
// would be the largest thing in the tree.

#include "newisp/options.h"

#include "newisp/chip_logic.h"
#include "newisp/i18n.h"

#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <string>

namespace newisp {

    using i18n::Msg;
    using i18n::Tr;

    namespace {

        // Option matching ignores case.
        //
        // People type -H, -h, -HID and -hid interchangeably, and on a command
        // line there is no reason to be strict about it. The one consequence to
        // keep in mind is that -v (verbose) and -V (version) therefore become
        // the same option, so -V no longer means "version": --version does, and
        // the usage text documents that.
        bool EqualsNoCase(const std::string& a, const char* b)
        {
            size_t i = 0;
            for (; i < a.size() && b[i]; ++i) {
                unsigned char ca = (unsigned char)a[i];
                unsigned char cb = (unsigned char)b[i];
                if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca - 'A' + 'a');
                if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb - 'A' + 'a');
                if (ca != cb) return false;
            }
            return i == a.size() && b[i] == '\0';
        }

        // True when `a` is any of the listed spellings of one option.
        bool IsOption(const std::string& a, const char* first, ...)
        {
            (void)first;
            va_list args;
            va_start(args, first);
            bool found = false;
            for (const char* name = first; name; name = va_arg(args, const char*)) {
                if (EqualsNoCase(a, name)) { found = true; break; }
            }
            va_end(args);
            return found;
        }

        // Accept "115200", "115.2k" and "1M" for baud rates and similar
        // quantities, because those are how the numbers appear in datasheets.
        bool ParseRate(const char* s, uint32_t& out)
        {
            if (!s || !*s) return false;
            char* end = nullptr;
            double v = std::strtod(s, &end);
            if (end == s) return false;
            if (*end == 'k' || *end == 'K') { v *= 1000.0; ++end; }
            else if (*end == 'm' || *end == 'M') { v *= 1000000.0; ++end; }
            if (*end != '\0') return false;
            if (v <= 0) return false;
            out = (uint32_t)(v + 0.5);
            return true;
        }

        // --eeprom 4K / --eeprom 4096 / --eeprom 0
        bool ParseBytes(const char* s, uint32_t& out)
        {
            if (!s || !*s) return false;
            char* end = nullptr;
            double v = std::strtod(s, &end);
            if (end == s) return false;
            if (*end == 'k' || *end == 'K') { v *= 1024.0; ++end; }
            else if (*end == 'm' || *end == 'M') { v *= 1024.0 * 1024.0; ++end; }
            if (*end != '\0') return false;
            if (v < 0) return false;
            out = (uint32_t)(v + 0.5);
            return true;
        }

        // ASCII-lowercase, for comparing a value the user typed against the
        // series names. Deliberately ASCII-only: the names are ASCII, and a
        // locale-sensitive tolower would make the result depend on the machine.
        std::string LowerAscii(const char* s)
        {
            std::string r(s ? s : "");
            for (char& c : r) {
                if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            }
            return r;
        }

        // Canonical spelling for a series the user typed: lower-case the input,
        // then return the name in the form the rest of the tool expects.
        //
        // The chip-name predicates in chip_logic.h match uppercase prefixes
        // ("STC12", "STC8H"), so what is stored has to be uppercase even though
        // the user's spelling is not. An unrecognised word comes back as typed
        // (lower-cased for the message) and is rejected by the caller.
        std::string SeriesNameFromUserInput(const char* v)
        {
            const std::string lower = LowerAscii(v);
            if (lower == "stc89" || lower == "stc90") return "STC89";
            if (lower == "stc10" || lower == "stc11" || lower == "stc12")
                return "STC12";
            if (lower == "stc15" || lower == "iap15" || lower == "irc15")
                return "STC15";
            if (lower == "stc8g" || lower == "ai8g" || lower == "jx8g")
                return "STC8G";
            if (lower == "stc32" || lower == "ai32") return "STC32";
            if (lower == "stc8h" || lower == "ai8h") return "STC8H";
            if (lower == "8051u" || lower == "stc8051u" || lower == "ai8051u")
                return "STC8051U";
            if (lower == "stc8") return "STC8";
            if (lower == "stc8a" || lower == "ai8a") return "STC8A";
            if (lower == "stc8f") return "STC8F";
            if (lower == "stc8c" || lower == "ai8c") return "STC8C";
            return lower;
        }

        // Map a series name onto the menu index used as a probe fallback.
        //
        // stc8h and 8051u share the stc32 slot (one implementation, one probe),
        // which is why they are not separate names here; stc8a has its own
        // frequency menu but rides the same 8-family protocol slot.
        //
        // Accepts any case, so it works both on the canonical spelling and on
        // whatever the user typed.
        int SeriesToIndex(const std::string& name)
        {
            const std::string n = LowerAscii(name.c_str());
            if (n == "stc89" || n == "stc90") return kProtoIdxStc89;
            if (n == "stc12" || n == "stc10" || n == "stc11")
                return kProtoIdxStc12;
            if (n == "stc15" || n == "iap15" || n == "irc15")
                return kProtoIdxStc15;
            if (n == "stc8g" || n == "ai8g" || n == "jx8g")
                return kProtoIdxStc8G;
            if (n == "stc32" || n == "ai32" || n == "stc8h" || n == "ai8h" ||
                n == "8051u" || n == "stc8051u" || n == "ai8051u")
                return kProtoIdxStc32;
            if (n == "stc8" || n == "stc8a" || n == "stc8f" || n == "stc8c" ||
                n == "ai8a" || n == "ai8c")
                return kProtoIdxStc8;
            return -1;
        }

        // ------------------------------------------------------------------
        //  Option-name matching
        // ------------------------------------------------------------------
        //
        // Every option is matched case-insensitively, and the long names also
        // accept a unique prefix: -eeprom can be typed -ee, -device -de. Two
        // spelling hazards make that rule need an exception:
        //
        //   -s          serial, and the first letter of -series
        //   -F          the frequency, and -f would read as a near miss of it
        //               (the firmware image has no option of its own any more;
        //               it is the first positional argument instead)
        //
        // So a name is marked LITERAL when an abbreviation of it would become
        // ambiguous. -series is the one that matters here: the user asked for
        // it to be spelled out in full precisely because -s must stay serial,
        // and "series" is also the name of the setting rather than a family
        // label ("family" was the old spelling and is gone).
        enum class Match {
            Literal,    // exact spelling only, case-insensitive
            Prefix,     // exact spelling, or a unique prefix of it
        };

        // One option and every spelling it answers to.
        //
        // The first spelling is the canonical one: it is what the parser's
        // IsOpt call and the usage text use, and what a "did you mean" hint
        // names. The rest are aliases accepted on input. The array is sized for
        // the longest alias list in kLongOptions (-freq has three), and is
        // null-terminated.
        struct OptionDef {
            const char* spelling;
            Match       how;
            const char* aliases[4];
        };

        // Three names are Literal -- spelled out in full, no abbreviation:
        //
        //   -serial  shares its prefix with -series, so -ser/-seri would be
        //            ambiguous between them. Marking it literal makes -ser fail
        //            as unknown rather than quietly meaning "serial".
        //   -series  the user's rule: a special option that is always spelled
        //            out, because -s is the serial transport and -series must
        //            stay distinguishable from it.
        //   -freq    its spellings are listed in full; an abbreviation such as
        //            -fr or the near-miss -frequncy must not resolve, because a
        //            typo in the frequency would burn option bytes at a rate
        //            nobody asked for.
        //
        // Matching is case-insensitive everywhere, so each spelling is listed
        // exactly once, in the case that reads best. -f is not a separate entry
        // from -F: they are the same one-letter spelling, and enumerating both
        // would be hard-coding a case distinction the matcher does not make.
        // The same is why the table no longer has -h/-H or -V/-v pairs -- those
        // options either became commands (help, version) or now share the
        // letter (-h is the HID transport).
        //
        // Short forms, one per option:
        //
        //    -s  serial      -h  hid         -o  output     -d  device
        //    -f  freq        -e  eeprom      -c  clock
        //
        // -s takes the baud rate as an optional value ("-s 921600"), so there is
        // no -b: the rate belongs to the link it configures. -series has no
        // short form because -s is serial, and there is no file short form
        // because the image is positional.
        //
        // help and version are not in this table at all: both are commands
        // (newisp help, newisp version), not options.
        const OptionDef kLongOptions[] = {
            { "-serial",  Match::Literal, { "-s", nullptr } },
            { "-hid",     Match::Prefix,  { "-h", nullptr } },
            { "-output",  Match::Prefix,  { "-o", nullptr } },
            { "-device",  Match::Prefix,  { "-d", nullptr } },
            { "-freq",    Match::Literal, { "-f", "-frequency", nullptr } },
            { "-eeprom",  Match::Prefix,  { "-e", nullptr } },
            { "-clock",   Match::Prefix,  { "-c", nullptr } },
            { "-series",  Match::Literal, { nullptr } },   // always in full
        };

        // True when `a` is a proper prefix of `full`, ignoring case.
        bool IsPrefixNoCase(const std::string& a, const char* full)
        {
            size_t k = 0;
            for (; k < a.size(); ++k) {
                char cf = full[k];
                if (cf == '\0') return false;          // a is longer than full
                char ca = a[k];
                if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
                if (cf >= 'A' && cf <= 'Z') cf = (char)(cf - 'A' + 'a');
                if (ca != cf) return false;
            }
            return full[k] != '\0';                    // proper, not exact
        }

        // Abbreviation only, or nullptr.
        //
        // Only meaningful for a spelling long enough to abbreviate: one or two
        // characters is a short form (-f, -s, -H) and those are exact by
        // definition, never prefixes. Letting -f abbreviate would make it
        // resolve to whichever long name it prefixes first, which is how it
        // would end up meaning a longer option it merely prefixes.
        //
        // A unique prefix resolves; an ambiguous one resolves to nothing.
        //
        // A name marked Literal never abbreviates: that is what keeps -s from
        // reaching -series, -ser from reaching -serial, and -fr from reaching
        // -freq.
        const char* MatchAbbreviated(const std::string& a)
        {
            if (a.size() < 3 || a[0] != '-') return nullptr;

            const char* hit = nullptr;
            for (const auto& d : kLongOptions) {
                if (d.how != Match::Prefix) continue;
                if (!IsPrefixNoCase(a, d.spelling)) continue;
                if (hit) return nullptr;    // ambiguous
                hit = d.spelling;
            }
            return hit;
        }

        // Canonical spelling of a long option as typed, or nullptr when `a`
        // names none of them. Aliases resolve to the canonical spelling, so
        // callers only ever compare against one name per option.
        //
        // One-letter aliases are exact, case included. That is what lets the
        // short forms distinguish -h (help) from -H (HID) and -f/-F from each
        // other; folding case here would merge those pairs. Longer aliases
        // (-frequency, -?) and the long spellings themselves stay
        // case-insensitive.
        const char* MatchLongOption(const std::string& a)
        {
            if (a.size() < 2 || a[0] != '-') return nullptr;

            for (const auto& d : kLongOptions) {
                for (const char* alias : d.aliases) {
                    if (!alias) break;
                    if (EqualsNoCase(a, alias)) return d.spelling;
                }
                if (EqualsNoCase(a, d.spelling)) return d.spelling;
            }
            return MatchAbbreviated(a);
        }

        // The canonical spelling of the option `a` most likely meant, when it
        // is a near miss of a registered spelling; nullptr when there is no
        // useful suggestion.
        //
        // Two messages are worth not conflating. "unknown option -frequncy"
        // says the tool did not understand the word; "did you mean -freq"
        // says what to type instead. The match is deliberately narrow -- the
        // edit distance has to be small relative to the word's length -- so a
        // genuinely different word (-frq, -f) gets no guess attached to it.
        // Levenshtein distance, case-insensitive, over the option name without
        // its leading dash.
        int EditDistanceNoCase(const std::string& a, const std::string& b)
        {
            const size_t n = a.size(), m = b.size();
            std::vector<int> prev(m + 1), cur(m + 1);
            for (size_t j = 0; j <= m; ++j) prev[j] = (int)j;
            for (size_t i = 1; i <= n; ++i) {
                cur[0] = (int)i;
                for (size_t j = 1; j <= m; ++j) {
                    char ca = a[i - 1], cb = b[j - 1];
                    if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
                    if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
                    const int cost = (ca == cb) ? 0 : 1;
                    int best = prev[j] + 1;
                    if (cur[j - 1] + 1 < best) best = cur[j - 1] + 1;
                    if (prev[j - 1] + cost < best) best = prev[j - 1] + cost;
                    cur[j] = best;
                }
                prev.swap(cur);
            }
            return prev[m];
        }

        // The registered spelling (canonical or alias) nearest to `a`, or
        // nullptr when nothing is close enough to be worth naming.
        //
        // The threshold is what keeps this a hint rather than a guess: at most
        // one edit in a three-letter word, two in a longer one. A transposed or
        // dropped letter is caught; a different word is not.
        //
        // Ties go to the longer spelling, which is the one that reads as the
        // option's name. That is also what keeps a single-letter alias like -F
        // from being suggested for every short input: it only wins when it is
        // strictly nearer, which for a real one-letter option name is the
        // right answer.
        const char* NearestSpelling(const std::string& a, int maxDistance)
        {
            std::string word = a.size() && a[0] == '-' ? a.substr(1) : a;
            if (word.empty()) return nullptr;

            const char* bestSpelling = nullptr;
            int bestDistance = maxDistance + 1;
            size_t bestLength = 0;
            for (const auto& d : kLongOptions) {
                const char* candidates[5] = { d.spelling, d.aliases[0],
                                              d.aliases[1], d.aliases[2],
                                              d.aliases[3] };
                for (const char* cand : candidates) {
                    if (!cand) break;
                    std::string c = cand;
                    if (c.size() && c[0] == '-') c = c.substr(1);
                    const int dist = EditDistanceNoCase(word, c);
                    const bool better =
                        dist < bestDistance ||
                        (dist == bestDistance && c.size() > bestLength);
                    if (better) {
                        bestDistance = dist;
                        bestLength = c.size();
                        bestSpelling = cand;
                    }
                }
            }
            return bestSpelling;
        }

        const char* SuggestOption(const std::string& a)
        {
            // Distance is measured on the name without its leading dash, so an
            // alias like -F is a one-letter word and a one-letter allowance
            // would make it the nearest match for every two-letter input. The
            // allowance is therefore derived from the input as typed: short
            // input gets nothing (two letters is too little to tell a typo
            // from a different word), three or four get one edit, longer words
            // get two.
            const size_t letters = a.size() > 1 ? a.size() - 1 : 0;
            if (letters < 3) return nullptr;
            return NearestSpelling(a, letters <= 4 ? 1 : 2);
        }

        // True when `a` names the option spelled `canonical`, honouring that
        // option's abbreviation and alias rules.
        //
        // Everything goes through MatchLongOption rather than comparing `a`
        // against `canonical` directly: that comparison would have to ignore
        // case to accept -DEVICE, and doing so would also make -v match -V.
        // The matcher knows which spellings are one-letter (exact) and which
        // are not.
        bool IsOpt(const std::string& a, const char* canonical)
        {
            const char* resolved = MatchLongOption(a);
            return resolved && EqualsNoCase(resolved, canonical);
        }

        // The command line no longer knows the chip yet, so it cannot say which
        // menu applies. It can say when the value is above every menu there is:
        // 60 MHz is the highest entry any series lists. A value that clears this
        // but is not in the probed series' menu is caught after the handshake
        // (ResolveFrequency in session.cpp), once the chip is known.
        bool IsAboveValidatedRange(uint32_t freq)
        {
            return freq > kFreqTableStc32G144K246[kFreqMaxIdxStc32];
        }

    } // namespace

    std::string UsageText(const char* programName)
    {
        // Show only the file name, not the path it was invoked by.
        //
        // argv[0] is whatever the shell was given, which is frequently an
        // absolute path: "C:\Users\...\Release\newisp.exe" is 50 characters of
        // noise that pushes every description off the right edge of the block.
        // The user already knows where their binary lives, and the examples
        // read as commands they can type. Both separators are handled so the
        // same code is correct on Windows and POSIX.
        std::string p = programName ? programName : "newisp";
        size_t slash = p.find_last_of("/\\");
        if (slash != std::string::npos && slash + 1 < p.size())
            p = p.substr(slash + 1);
        if (p.empty()) p = "newisp";

        // Pad the command column rather than hard-coding a run of spaces, so
        // the block still lines up if the executable is renamed.
        //
        // The width has to clear the longest invocation, which is
        // "<name> burn -f FILE [options] [device]" -- about 46 characters for
        // "newisp.exe". A narrower column silently collapses the last two rows
        // into one run of text, which is what it did at 40.
        const size_t cmdWidth = 48;
        auto usage = [&](const std::string& syntax,
                         const std::string& description) {
            std::string line = "  " + p + " " + syntax;
            if (line.size() < cmdWidth) line.append(cmdWidth - line.size(), ' ');
            else line += ' ';
            return line + description + "\n";
        };

        // A settings row: "  -x, --long VALUE" padded, then the description.
        // Rows whose first line names the value get their continuation lines
        // indented by valueIndent so they line up under the description.
        const size_t optionWidth = 28;
        auto option = [&](const std::string& flags,
                          const std::string& description) {
            std::string line = "  " + flags;
            if (line.size() < optionWidth) line.append(optionWidth - line.size(), ' ');
            else line += ' ';
            return line + description + "\n";
        };

        // Every line below comes from the message table, so the English and
        // Chinese banners stay structurally identical: only the wording and the
        // descriptions differ, never the layout.
        std::string out;
        // The version comes from the build through i18n::BuildVersion rather
        // than from the macro directly: the macro expands to a run of tokens
        // that only some syntactic positions accept as a string.
        out += Tr(Msg::UsageTitle, std::string(i18n::BuildVersion())) + "\n\n";
        out += Tr(Msg::UsageUsage) + "\n";
        out += usage("", Tr(Msg::UsageCmdNone));
        out += usage("help", Tr(Msg::UsageCmdHelp));
        out += usage("version", Tr(Msg::UsageCmdVersion));
        out += usage("list", Tr(Msg::UsageCmdList));
        out += usage("detect [transport] [device]", Tr(Msg::UsageCmdDetect));
        out += usage("inspect FILE", Tr(Msg::UsageCmdInspect));
        out += usage("burn FILE [options] [device]", Tr(Msg::UsageCmdBurn));

        // Order matters: this block is what a user reads first. Options are
        // grouped by what they belong to, not by the order they were added:
        //
        //   -baud sits with the transports, immediately after -serial, because
        //   it is a serial-port setting and means nothing over USB HID. Under
        //   "Programming" it read as though it shaped the burn; it does not,
        //   it shapes the link the burn travels over. (The HID handshakes
        //   discard the argument outright -- see ProtocolStc32Hid::Handshake.)
        //
        // Everything else keeps its section: device, then the programming
        // options that go into the option bytes, then output.
        out += "\n" + Tr(Msg::UsageTransport) + "\n";
        out += option("-s, -serial [BAUD]", Tr(Msg::UsageSerial));
        out += std::string(optionWidth, ' ') + Tr(Msg::UsageSerialBaud) + "\n";
        out += std::string(optionWidth, ' ') + Tr(Msg::UsageSerialBaudForms) + "\n";
        out += option("-h, -hid", Tr(Msg::UsageHid));
        out += "\n" + Tr(Msg::UsageDevice) + "\n";
        out += option("-d, -device NAME", Tr(Msg::UsageDeviceName));
        out += std::string(optionWidth, ' ') + Tr(Msg::UsageDeviceDefault) + "\n";
        out += std::string(optionWidth, ' ') + Tr(Msg::UsageDeviceDefault2) + "\n";
        out += "\n" + Tr(Msg::UsageProgramming) + "\n";
        out += option("-f, -freq, -frequency HZ", Tr(Msg::UsageFreq));
        out += std::string(optionWidth, ' ') + Tr(Msg::UsageFreqForms) + "\n";
        out += std::string(optionWidth, ' ') + Tr(Msg::UsageFreqForms2) + "\n";
        out += option("-e, -eeprom SIZE", Tr(Msg::UsageEeprom));
        out += option("-c, -clock internal|external", Tr(Msg::UsageClockSrc));
        out += option("    -series NAME", Tr(Msg::UsageSeries));
        out += std::string(optionWidth, ' ') + Tr(Msg::UsageSeriesNames) + "\n";
        out += std::string(optionWidth, ' ') + Tr(Msg::UsageSeriesNames2) + "\n";
        out += std::string(optionWidth, ' ') + Tr(Msg::UsageSeriesFull) + "\n";
        out += "\n" + Tr(Msg::UsageOutput) + "\n";
        // The language is a build-time choice, so it is documented rather than
        // offered as a flag: a flag would imply a second table is in the
        // binary, and there is none.
        out += option("    (language)", Tr(Msg::UsageLangBuild));
        out += std::string(optionWidth, ' ') + Tr(Msg::UsageLangBuildSet) + "\n";
        out += std::string(optionWidth, ' ') + Tr(Msg::UsageLangBuildThis) +
            i18n::BuildLanguage() + "\n";
        out += option("-o, -output LEVEL", Tr(Msg::UsageOutputFlag));
        out += std::string(optionWidth, ' ') + Tr(Msg::UsageOutputLevels) + "\n";
        out += std::string(optionWidth, ' ') + Tr(Msg::UsageOutputDefault) + "\n";
        out += "\n" + Tr(Msg::UsageExamples) + "\n";
        out += "  " + p + " detect -device /dev/ttyUSB0\n";
        out += "  " + p + " burn app.hex -d COM7 -F 35M -e 4K\n";
        out += "  " + p + " burn app.hex -HID         # " +
            Tr(Msg::UsageExampleHid) + "\n";
        out += "  " + p + " inspect app.hex\n";
        out += "\n" + Tr(Msg::UsageNotes) + "\n";
        out += "  " + Tr(Msg::UsageNoteSerial) + "\n";
        out += "  " + Tr(Msg::UsageNoteSerial2) + "\n";
        out += "\n";
        out += "  " + Tr(Msg::UsageNoteHid) + "\n";
        out += "\n";
        out += "  " + Tr(Msg::UsageNote8051u) + "\n";
        return out;
    }

    int ParseCommandLine(int argc, char** argv, CliOptions& out,
        std::string& error)
    {
        // The output language is fixed at build time (see NEWISP_LANG in
        // CMakeLists.txt), so there is nothing to select here: every message
        // below already comes from the table this binary carries.

        if (argc < 2) {
            out.command = Command::None;
            return 0;
        }

        // ---- subcommand ----
        std::string cmd = argv[1];
        int i = 2;

        // -output is the only output setting, so there is one value to carry
        // rather than a quiet/verbose pair that could both be true.
        OutputLevel output = OutputLevel::Normal;
        bool outputExplicit = false;

        // help and version are commands, not options: they stand where list /
        // detect / inspect / burn stand. Neither has a flag form -- no -help,
        // no -h, no -?, no -version, no -V -- so the only way to ask for either
        // is the word itself. That is also what keeps -h free for the HID
        // transport and -v from being mistaken for a version request.
        if (EqualsNoCase(cmd, "help")) {
            out.command = Command::Help;
            return 0;
        }
        if (EqualsNoCase(cmd, "version")) {
            out.command = Command::Version;
            return 0;
        }
        if (EqualsNoCase(cmd, "list"))         out.command = Command::List;
        else if (EqualsNoCase(cmd, "detect"))  out.command = Command::Detect;
        else if (EqualsNoCase(cmd, "inspect")) out.command = Command::Inspect;
        else if (EqualsNoCase(cmd, "burn"))    out.command = Command::Burn;
        else if (IsOpt(cmd, "-output")) {
            // Only -output was given. Treat it as the no-command case so the
            // setting is still honoured, and the no-command reply points at
            // help and detect.
            out.command = Command::None;
            i = 1;   // re-scan from the flag itself
        }
        else {
            error = Tr(Msg::CliErrUnknownCommand, cmd);
            return 2;
        }

        // ---- options ----
        for (; i < argc; ++i) {
            std::string a = argv[i];

            if (out.command == Command::Inspect &&
                (IsOpt(a, "-freq") ||
                 IsOpt(a, "-eeprom") || IsOpt(a, "-clock") ||
                 IsOpt(a, "-series"))) {
                error = Tr(Msg::CliErrInspectOption);
                return 2;
            }

            // detect answers one question -- which chip is on the link -- so the
            // programming options that shape a burn (frequency, EEPROM split,
            // clock source, series override) have no meaning there. Rejecting
            // them is better than accepting and ignoring them.
            //
            // The transport (-s/-serial, -h/-hid) and -device still apply: they
            // choose which link to probe. So does the baud rate -s may carry:
            // it configures that link.
            if (out.command == Command::Detect &&
                (IsOpt(a, "-freq") ||
                 IsOpt(a, "-eeprom") || IsOpt(a, "-clock") ||
                 IsOpt(a, "-series"))) {
                error = Tr(Msg::CliErrDetectOption);
                return 2;
            }

            auto needValue = [&](const char* what) -> const char* {
                if (i + 1 >= argc) {
                    error = Tr(Msg::CliErrNeedsValue, what);
                    return nullptr;
                }
                return argv[++i];
            };

            // Every option below goes through IsOpt, which resolves the long
            // spelling, any unique prefix of it (-ee for -eeprom) and the short
            // forms listed in kLongOptions. Short forms are exact and
            // case-sensitive, so -H (HID) is distinct from a hypothetical -h.
            // Neither help nor version appears here: both are commands.
            if (IsOpt(a, "-serial")) {
                // -s is the only way to spell the serial transport in one
                // letter, which is why -series has no short form: the two
                // would be indistinguishable.
                //
                // The baud rate belongs to this option rather than to one of its
                // own: it is a property of the serial link and means nothing
                // over HID (whose handshakes discard it), so writing it as
                // "-s 921600" keeps it next to the thing it configures and
                // leaves no -b to remember.
                //
                // Optional value, told apart by shape: a token that reads as a
                // number is the baud rate, anything else is not consumed and is
                // left to the positional rules. That matters because
                // "-s COM7" and "-s app.hex" are both legitimate -- -s followed
                // by the device or the image -- and neither looks like a rate.
                out.useHid = false;
                out.transportExplicit = true;
                if (i + 1 < argc) {
                    uint32_t rate = 0;
                    if (ParseRate(argv[i + 1], rate)) {
                        ++i;
                        out.burn.baudRate = rate;
                    }
                }
            }
            else if (IsOpt(a, "-hid")) {
                out.useHid = true;
                out.transportExplicit = true;
            }
            else if (IsOpt(a, "-output")) {
                const char* v = needValue("-output");
                if (!v) return 2;
                if (!ParseOutputLevel(v, output)) {
                    error = Tr(Msg::CliErrBadOutput, v);
                    return 2;
                }
                outputExplicit = true;
            }
            else if (IsOpt(a, "-device")) {
                const char* v = needValue("-device");
                if (!v) return 2;
                out.burn.deviceId = v;
            }
            else if (IsOpt(a, "-freq")) {
                // -frequency, -freq, -F and -f all land here; see kLongOptions
                // for why the alias list is closed.
                const char* v = needValue("-freq");
                if (!v) return 2;
                if (!ParseRate(v, out.burn.targetFreq)) {
                    error = Tr(Msg::CliErrBadFreq, v);
                    return 2;
                }
                out.burn.freqExplicit = true;
            }
            else if (IsOpt(a, "-eeprom")) {
                const char* v = needValue("-eeprom");
                if (!v) return 2;
                if (!ParseBytes(v, out.burn.eepromBytes)) {
                    error = Tr(Msg::CliErrBadEeprom, v);
                    return 2;
                }
            }
            else if (IsOpt(a, "-clock")) {
                const char* v = needValue("-clock");
                if (!v) return 2;
                std::string s = v;
                if (EqualsNoCase(s, "external") || EqualsNoCase(s, "ext") ||
                    EqualsNoCase(s, "crystal")) {
                    out.burn.clockExternal = true;
                }
                else if (EqualsNoCase(s, "internal") || EqualsNoCase(s, "int") ||
                         EqualsNoCase(s, "irc")) {
                    out.burn.clockExternal = false;
                }
                else {
                    error = Tr(Msg::CliErrBadClock, s);
                    return 2;
                }
                out.burn.clockExplicit = true;
            }
            else if (IsOpt(a, "-series")) {
                const char* v = needValue("-series");
                if (!v) return 2;
                // The series name is matched case-insensitively, but stored in
                // the spelling the chip-name predicates expect: they compare
                // against uppercase prefixes ("STC12", "STC8H"), so storing
                // "stc12" would make every one of them miss and fall through to
                // the 8H defaults.
                out.seriesName = SeriesNameFromUserInput(v);
                int idx = SeriesToIndex(out.seriesName);
                if (idx < 0) {
                    error = Tr(Msg::CliErrBadSeries, v);
                    return 2;
                }
                out.burn.protoOverride = idx;
            }
            else if (!a.empty() && a[0] == '-') {
                // A near miss is worth naming: "-frequncy" is a typo for
                // -freq, not an option the tool lacks, and telling the user
                // which one they meant saves a round of reading the help. The
                // option is still rejected -- a suggested spelling is never
                // silently substituted, because acting on a guess about which
                // frequency was meant is exactly what must not happen.
                const char* hint = SuggestOption(a);
                if (hint) {
                    error = Tr(Msg::CliErrUnknownOptionHint, { a, hint });
                }
                else {
                    error = Tr(Msg::CliErrUnknownOption, a);
                }
                return 2;
            }
            else {
                // A bare argument is positional, and which position it fills
                // depends on the command:
                //
                //   inspect app.hex        the image, and nothing else
                //   burn app.hex [device]  the image, then the device
                //   detect [device]        the device
                //   list                   no positionals
                //
                // The image comes first because it is what the command is
                // about: "newisp burn app.hex" reads as one instruction, where
                // "-f app.hex" made the file look like an afterthought.
                if (out.command == Command::Inspect ||
                    out.command == Command::Burn) {
                    if (out.firmwarePath.empty()) {
                        out.firmwarePath = a;
                        continue;
                    }
                }
                if (out.burn.deviceId.empty()) {
                    out.burn.deviceId = a;
                }
                else {
                    error = Tr(Msg::CliErrExtraArgument, a);
                    return 2;
                }
            }
        }

        // ---- consistency ----
        if ((out.command == Command::Burn || out.command == Command::Inspect) && out.firmwarePath.empty()) {
            // The two commands take a different tail (burn also takes a
            // device), so the hint spells out the actual invocation rather
            // than one shared template that would be wrong for inspect.
            if (out.command == Command::Burn)
                error = Tr(Msg::CliErrNeedFileBurn);
            else
                error = Tr(Msg::CliErrNeedFile, std::string("inspect"));
            return 2;
        }
        if (out.command == Command::Inspect &&
            (out.transportExplicit || !out.burn.deviceId.empty() ||
             !out.seriesName.empty() || out.burn.clockExplicit ||
             out.burn.eepromBytes != 0)) {
            error = Tr(Msg::CliErrInspectDevice);
            return 2;
        }

        // Above every series' menu is a mistake worth reporting rather than a
        // request to be honoured or quietly clamped. Silence here would leave
        // the user believing they had asked for something the tool never
        // attempted. Anything below that but still outside the probed series'
        // menu is reported after the handshake instead.
        //
        // A named series narrows the check: -series stc8h rejects 60 MHz here
        // rather than waiting for the chip to answer.
        //
        // The menu is looked up by the name the user typed, not by the probe
        // slot: stc8h, stc32 and 8051u all map to the stc32 slot (one
        // implementation), but only the 144K246 part gets the 60 MHz menu, and
        // the slot cannot tell them apart.
        if (out.burn.freqExplicit && IsAboveValidatedRange(out.burn.targetFreq)) {
            error = Tr(Msg::CliErrFreqTooHigh, {
                std::to_string(out.burn.targetFreq),
                std::to_string(kFreqTableStc32G144K246[kFreqMaxIdxStc32] / 1000000) });
            return 2;
        }
        if (out.burn.freqExplicit && !out.seriesName.empty() &&
            HasSelectableFrequency(out.seriesName) &&
            !IsFrequencyInMenu(out.seriesName, out.burn.targetFreq)) {
            error = Tr(Msg::CliErrFreqNotInMenu, {
                FormatDouble(out.burn.targetFreq / 1e6, 4), out.seriesName });
            return 2;
        }

        // A series named with -series may have a feature that does not apply to
        // it. Each is rejected here rather than accepted and silently dropped,
        // because "-clock external" on an 8H looks like it set something when
        // it did nothing. This can only be checked when the series is known up
        // front; otherwise the same rules run after the chip is identified
        // (see RunBurn).
        //
        //   -freq    STC10/11/12 and STC89/90 have no frequency menu: the
        //            former picks a clock source, the latter runs from a crystal
        //   -eeprom  only the STC8/STC32 generation has an EEPROM split
        //   -clock   only STC10/11/12 record a clock source
        //
        // Each test is on whether the user *asked*, not on the value: the
        // frequency defaults to 24 MHz, so testing the value would make every
        // STC12 burn look like a frequency request.
        if (!out.seriesName.empty()) {
            if (out.burn.freqExplicit && !HasSelectableFrequency(out.seriesName)) {
                error = Tr(Msg::CliErrNoFreqForSeries, out.seriesName);
                return 2;
            }
            if (out.burn.eepromBytes && !HasSelectableEeprom(out.seriesName)) {
                error = Tr(Msg::CliErrNoEepromForSeries, out.seriesName);
                return 2;
            }
            if (out.burn.clockExplicit && !HasSelectableClockSource(out.seriesName)) {
                error = Tr(Msg::CliErrNoClockForSeries, out.seriesName);
                return 2;
            }
        }

        out.burn.useHid = out.useHid;

        // detect needs to know whether the transport was chosen or merely
        // defaulted: with no explicit flag it probes both.
        out.burn.transportExplicit = out.transportExplicit;

        out.output = output;
        out.outputExplicit = outputExplicit;

        return 0;
    }

    bool ParseOutputLevel(const std::string& text, OutputLevel& out)
    {
        std::string s = LowerAscii(text.c_str());
        if (s == "quiet" || s == "q")    { out = OutputLevel::Quiet;   return true; }
        if (s == "normal" || s == "n" || s == "default" || s == "d") {
            out = OutputLevel::Normal;
            return true;
        }
        if (s == "verbose" || s == "v")  { out = OutputLevel::Verbose; return true; }
        return false;
    }

} // namespace newisp
