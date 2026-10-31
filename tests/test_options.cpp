// Command-line option parsing tests.
//
// The parser is the one place the tool decides what a user typed, and the rules
// there are not obvious: every option matches case-insensitively, long options
// take a unique prefix, three spellings are deliberately exempt from that, and
// the firmware image is a positional argument rather than a flag. Getting it
// wrong does not fail loudly -- a mistyped frequency would burn option bytes at
// a rate nobody asked for -- so each rule is pinned here.

#include "test.h"

#include "newisp/options.h"

#include <string>
#include <vector>

using namespace newisp;

namespace {

    // Run the parser over a vector of arguments. argv[0] is the program name.
    int Parse(std::vector<const char*> args, CliOptions& out, std::string& error)
    {
        args.insert(args.begin(), "newisp");
        return ParseCommandLine((int)args.size(),
            const_cast<char**>(args.data()), out, error);
    }

    // A burn invocation: the image is the first positional, right after the
    // command.
    std::vector<const char*> Burn(std::initializer_list<const char*> rest)
    {
        std::vector<const char*> a = { "burn", "a.hex" };
        a.insert(a.end(), rest.begin(), rest.end());
        return a;
    }

    void TestImageIsPositional()
    {
        // The image follows the command directly: "newisp burn app.hex". It is
        // required, so omitting it is a parse error rather than a burn against
        // an empty image.
        {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "burn", "app.hex" };
            CHECK_EQ(Parse(a, o, e), 0);
            CHECK_STREQ(o.firmwarePath, "app.hex");
            CHECK(o.burn.deviceId.empty());
        }
        {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "inspect", "app.hex" };
            CHECK_EQ(Parse(a, o, e), 0);
            CHECK_STREQ(o.firmwarePath, "app.hex");
        }

        // For burn, the second positional is the device: "burn app.hex COM7".
        {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "burn", "app.hex", "COM7" };
            CHECK_EQ(Parse(a, o, e), 0);
            CHECK_STREQ(o.firmwarePath, "app.hex");
            CHECK_STREQ(o.burn.deviceId, "COM7");
        }

        // A third positional has nowhere to go.
        {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "burn", "app.hex", "COM7", "extra" };
            CHECK_EQ(Parse(a, o, e), 2);
        }

        // Without an image both commands fail, and the message names the
        // command so the fix ("put the file after the command") is obvious.
        for (const char* cmd : { "burn", "inspect" }) {
            CliOptions o; std::string e;
            std::vector<const char*> a = { cmd };
            CHECK_EQ(Parse(a, o, e), 2);
            CHECK(e.find(cmd) != std::string::npos);
        }

        // Options may come before the image as well as after it.
        {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "burn", "-F", "27M", "app.hex" };
            CHECK_EQ(Parse(a, o, e), 0);
            CHECK_STREQ(o.firmwarePath, "app.hex");
            CHECK_EQ(o.burn.targetFreq, 27000000u);
        }

        // The old spellings are gone: -f and -file are not options any more,
        // and passing the image that way leaves the image missing.
        for (const char* flag : { "-f", "-file" }) {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "burn", flag, "app.hex" };
            CHECK_EQ(Parse(a, o, e), 2);
            CHECK(o.firmwarePath.empty());
        }
    }

    void TestSeriesMustBeSpelledOut()
    {
        // -series is the rule that matters: it is spelled out in full, always.
        // -s belongs to the serial transport, and the two must never be
        // confusable, so no abbreviation of -series is accepted either.
        for (const char* spelling : { "-series", "-SERIES", "-Series" }) {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "burn", "a.hex", spelling,
                                           "stc8h", "-d", "X" };
            CHECK_EQ(Parse(a, o, e), 0);
            // Stored in the canonical spelling the chip-name predicates expect,
            // whatever case the user typed.
            CHECK_STREQ(o.seriesName, "STC8H");
            CHECK(o.burn.protoOverride >= 0);
        }

        // Abbreviations must fail rather than resolve to something else. In
        // particular -ser and -seri are *not* -serial: they are unknown.
        for (const char* bad : { "-ser", "-seri", "-serie", "-serieS2" }) {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "burn", "a.hex", bad, "stc8h" };
            CHECK_EQ(Parse(a, o, e), 2);
            CHECK(o.seriesName.empty());
        }

        // The value is case-insensitive too.
        {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "burn", "a.hex", "-series",
                                           "STC8H", "-d", "X" };
            CHECK_EQ(Parse(a, o, e), 0);
            CHECK_STREQ(o.seriesName, "STC8H");
        }

        // An unknown series name is rejected, and the name is reported as
        // typed.
        {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "burn", "a.hex", "-series", "stc99" };
            CHECK_EQ(Parse(a, o, e), 2);
            CHECK(!e.empty());
        }
    }

    void TestSerialStaysOnShortS()
    {
        // -s is serial and only serial. If -series ever became a prefix match,
        // this is the assertion that would catch the collision.
        {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "detect", "-s" };
            CHECK_EQ(Parse(a, o, e), 0);
            CHECK(!o.useHid);
            CHECK(o.transportExplicit);
        }
        {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "detect", "-serial" };
            CHECK_EQ(Parse(a, o, e), 0);
            CHECK(o.transportExplicit);
            CHECK(!o.useHid);
        }
        // -H is HID, and -h is still help.
        {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "detect", "-H" };
            CHECK_EQ(Parse(a, o, e), 0);
            CHECK(o.useHid);
        }
        {
            // help and version are commands, not options. "newisp help" and
            // "newisp version" print and exit; there is no -h, -?, -help flag,
            // nor -V / -version.
            CliOptions o; std::string e;
            std::vector<const char*> a = { "help" };
            CHECK_EQ(Parse(a, o, e), 0);
            CHECK(o.command == Command::Help);
        }
        {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "version" };
            CHECK_EQ(Parse(a, o, e), 0);
            CHECK(o.command == Command::Version);
        }
        for (const char* notACommand : { "-h", "-V" }) {
            CliOptions o; std::string e;
            std::vector<const char*> a = { notACommand };
            CHECK_EQ(Parse(a, o, e), 2);
        }
    }

    void TestUniquePrefixesResolve()
    {
        // Long names abbreviate by any unique prefix, case-insensitively. These
        // are the ones with no competing spelling.
        {
            CliOptions o; std::string e;
            CHECK_EQ(Parse(Burn({ "-ee", "4K" }), o, e), 0);
            CHECK_EQ(o.burn.eepromBytes, 4096u);
        }
        {
            CliOptions o; std::string e;
            CHECK_EQ(Parse(Burn({ "-de", "COM1" }), o, e), 0);
            CHECK_STREQ(o.burn.deviceId, "COM1");
        }
        {
            CliOptions o; std::string e;
            CHECK_EQ(Parse(Burn({ "-cl", "irc" }), o, e), 0);
            CHECK(!o.burn.clockExternal);
            CHECK(o.burn.clockExplicit);
        }
    }

    void TestSeriesIsRejectedWhereItHasNoMeaning()
    {
        // inspect is offline and detect only identifies; the series override
        // shapes a burn, so neither accepts it.
        for (const char* cmd : { "inspect", "detect" }) {
            CliOptions o; std::string e;
            std::vector<const char*> a = { cmd, "a.hex", "-series", "stc8h" };
            CHECK_EQ(Parse(a, o, e), 2);
            CHECK(!e.empty());
        }
    }

    void TestOutputIsOneSetting()
    {
        // -output replaced the old -quiet and -verbose pair. One setting means
        // the levels are exhaustive: there is no way to ask for quiet and
        // verbose at once, which is what the two flags allowed.
        struct Case { const char* word; OutputLevel want; };
        const Case cases[] = {
            { "quiet",   OutputLevel::Quiet   },
            { "QUIET",   OutputLevel::Quiet   },
            { "q",       OutputLevel::Quiet   },
            { "normal",  OutputLevel::Normal  },
            { "n",       OutputLevel::Normal  },
            { "default", OutputLevel::Normal  },
            { "verbose", OutputLevel::Verbose },
            { "VERBOSE", OutputLevel::Verbose },
            { "v",       OutputLevel::Verbose },
        };
        for (const auto& c : cases) {
            OutputLevel got = OutputLevel::Normal;
            CHECK(ParseOutputLevel(c.word, got));
            CHECK(got == c.want);
        }

        // A word that is not one of the three is rejected, not silently
        // treated as normal.
        for (const char* bad : { "", "loud", "silent", "vv" }) {
            OutputLevel got = OutputLevel::Normal;
            CHECK(!ParseOutputLevel(bad, got));
        }

        // The default is normal, and -output sets the level for a real command.
        {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "inspect", "a.hex" };
            CHECK_EQ(Parse(a, o, e), 0);
            CHECK(o.output == OutputLevel::Normal);
            CHECK(!o.outputExplicit);
        }
        for (const char* cmd : { "quiet", "normal", "verbose" }) {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "inspect", "a.hex", "-output", cmd };
            CHECK_EQ(Parse(a, o, e), 0);
            CHECK(o.outputExplicit);
        }

        // A bad level fails the parse with an explanation.
        {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "inspect", "a.hex", "-output", "loud" };
            CHECK_EQ(Parse(a, o, e), 2);
            CHECK(!e.empty());
        }

        // -o is the short form.
        {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "inspect", "a.hex", "-o", "v" };
            CHECK_EQ(Parse(a, o, e), 0);
            CHECK(o.output == OutputLevel::Verbose);
        }

        // The old spellings are gone: they must not silently mean anything.
        // -q and -v are not aliases of -output either: -V is the version
        // command's short form, and -v is simply unknown.
        for (const char* gone : { "-q", "-quiet", "-verbose" }) {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "inspect", "a.hex", gone };
            CHECK_EQ(Parse(a, o, e), 2);
        }
        {
            // -v is unknown: version is a command (newisp version), not an
            // option, so neither -v nor -V is a spelling of it.
            CliOptions o; std::string e;
            std::vector<const char*> a = { "inspect", "a.hex", "-v" };
            CHECK_EQ(Parse(a, o, e), 2);
        }
        {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "inspect", "a.hex", "-V" };
            CHECK_EQ(Parse(a, o, e), 2);
        }
    }

    void TestFrequencyHasFourSpellings()
    {
        // The frequency accepts exactly four spellings: the full word, the
        // short word, and the one-letter forms in either case. Nothing else
        // reaches it. -f is available because the image is positional now, so
        // no file option competes for the letter.
        for (const char* flag : { "-frequency", "-freq", "-F", "-f",
                                  "-FREQ", "-Frequency" }) {
            CliOptions o; std::string e;
            CHECK_EQ(Parse(Burn({ flag, "27M" }), o, e), 0);
            CHECK_EQ(o.burn.targetFreq, 27000000u);
            // The positional image is untouched by the frequency.
            CHECK_STREQ(o.firmwarePath, "a.hex");
        }

        // Near misses are rejected, not resolved. A permissive prefix rule here
        // would turn a typo into a burn at a frequency the user never asked
        // for, which is the failure this closed alias list exists to prevent.
        for (const char* bad : { "-fr", "-frequncy", "-frequence",
                                 "-frequencyy", "-frq", "-frequ" }) {
            CliOptions o; std::string e;
            CHECK_EQ(Parse(Burn({ bad, "27M" }), o, e), 2);
            CHECK(o.burn.targetFreq != 27000000u);
        }
    }

    void TestUnknownOptionSuggestsNearestSpelling()
    {
        // A typo gets a suggestion alongside "unknown option", but the option
        // is still rejected: the tool never acts on the guess.
        {
            CliOptions o; std::string e;
            CHECK_EQ(Parse(Burn({ "-frequncy", "27M" }), o, e), 2);
            // The value followed the rejected flag, so the default stands and
            // 27 MHz was never taken as the target.
            CHECK(o.burn.targetFreq != 27000000u);
            // The message names the suggested spelling; the wording around it
            // is translated, the spelling is not.
            CHECK(e.find("-frequency") != std::string::npos);
        }
        {
            CliOptions o; std::string e;
            CHECK_EQ(Parse(Burn({ "-eeprome", "4K" }), o, e), 2);
            CHECK(e.find("-eeprom") != std::string::npos);
        }

        // A word that is nothing like an option gets no guess attached.
        for (const char* unrelated : { "-xyz", "-q", "-fr" }) {
            CliOptions o; std::string e;
            CHECK_EQ(Parse(Burn({ unrelated }), o, e), 2);
            CHECK(e.find("?") == std::string::npos);
        }
    }

    void TestSerialCarriesTheBaudRate()
    {
        // The baud rate is -s's optional value: "-s 921600". There is no -b.
        // It configures the serial link, so it belongs to the option that
        // selects that link.
        {
            CliOptions o; std::string e;
            CHECK_EQ(Parse(Burn({ "-s", "921600" }), o, e), 0);
            CHECK(!o.useHid);
            CHECK(o.transportExplicit);
            CHECK_EQ(o.burn.baudRate, 921600u);
        }

        // The usual rate shorthands work, because the value goes through the
        // same parser as any other rate.
        for (auto [text, want] : { std::pair{"115.2k", 115200u},
                                   std::pair{"1M", 1000000u},
                                   std::pair{"9600", 9600u} }) {
            CliOptions o; std::string e;
            CHECK_EQ(Parse(Burn({ "-s", text }), o, e), 0);
            CHECK_EQ(o.burn.baudRate, want);
        }

        // -s with no value still selects serial and keeps the default rate.
        {
            CliOptions o; std::string e;
            CHECK_EQ(Parse(Burn({ "-s" }), o, e), 0);
            CHECK(!o.useHid);
            CHECK_EQ(o.burn.baudRate, 115200u);
        }

        // A token that is not a rate is not consumed: "-s" followed by the
        // image or the device leaves the positionals alone. This is the reason
        // the value is optional and shape-tested rather than mandatory.
        {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "burn", "-s", "app.hex" };
            CHECK_EQ(Parse(a, o, e), 0);
            CHECK_STREQ(o.firmwarePath, "app.hex");
            CHECK_EQ(o.burn.baudRate, 115200u);
        }
        {
            CliOptions o; std::string e;
            std::vector<const char*> a = { "detect", "-s", "-d", "COM7" };
            CHECK_EQ(Parse(a, o, e), 0);
            CHECK_STREQ(o.burn.deviceId, "COM7");
            CHECK_EQ(o.burn.baudRate, 115200u);
        }

        // -b is gone: it is not an option any more.
        for (const char* gone : { "-b", "-baud", "-ba" }) {
            CliOptions o; std::string e;
            CHECK_EQ(Parse(Burn({ gone, "9600" }), o, e), 2);
        }

        // -h still wins over -s when both appear: the last transport flag
        // decides, and HID ignores the rate entirely.
        {
            CliOptions o; std::string e;
            CHECK_EQ(Parse(Burn({ "-s", "921600", "-h" }), o, e), 0);
            CHECK(o.useHid);
        }
    }

} // namespace

void RunOptionsTests()
{
    TestImageIsPositional();
    TestSeriesMustBeSpelledOut();
    TestSerialStaysOnShortS();
    TestUniquePrefixesResolve();
    TestSeriesIsRejectedWhereItHasNoMeaning();
    TestOutputIsOneSetting();
    TestFrequencyHasFourSpellings();
    TestUnknownOptionSuggestsNearestSpelling();
    TestSerialCarriesTheBaudRate();
}
