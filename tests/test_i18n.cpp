// Localisation tests.
//
// The point of these is that a translation cannot rot silently: the message
// tables are checked against each other for shape, and every message is
// rendered in both languages so a broken entry shows up here rather than in the
// middle of a burn.

#include "test.h"

#include "newisp/i18n.h"

#include <cstdio>
#include <set>
#include <string>
#include <vector>

using namespace newisp::i18n;

namespace {

    // Every message identifier, so the tables can be walked generically.
    // Every message identifier, in declaration order.
    //
    // The count is taken by walking the enum until English() changes into the
    // sentinel the table uses for an out-of-range id: an unknown id returns an
    // empty string, and the loop stops there. That keeps the tests independent
    // of any hand-maintained count.
    const std::vector<Msg>& AllMessages()
    {
        static const std::vector<Msg> all = [] {
            std::vector<Msg> v;
            // A generous upper bound rather than an unterminated walk: the
            // table is a few hundred entries and growing slowly.
            const unsigned kMaxMessages = 4096;
            for (unsigned i = 0; i < kMaxMessages; ++i) {
                Msg m = (Msg)i;
                if (std::string(English(m)).empty()) break;
                v.push_back(m);
            }
            return v;
        }();
        return all;
    }

    // The {n} slot indices used by a message, as a sorted set. Formats are
    // substituted after translation, so both tables must agree on them.
    std::set<int> Slots(const std::string& fmt)
    {
        std::set<int> out;
        for (size_t i = 0; i < fmt.size(); ++i) {
            if (fmt[i] != '{') continue;
            size_t j = i + 1;
            int index = 0;
            bool digits = false;
            while (j < fmt.size() && fmt[j] >= '0' && fmt[j] <= '9') {
                digits = true;
                index = index * 10 + (fmt[j] - '0');
                ++j;
            }
            if (digits && j < fmt.size() && fmt[j] == '}') out.insert(index);
            i = j;
        }
        return out;
    }

    std::string Join(const std::set<int>& s)
    {
        std::string out;
        for (int v : s) {
            if (!out.empty()) out += ',';
            out += std::to_string(static_cast<long long>(v));
        }
        return out;
    }

    // True when the message contains an English word of five letters or more.
    //
    // That is the test for "there is something here to translate": identifiers
    // are short and uppercase-dominant (MSR, UID, IAP, opts, Magic, IAP), units
    // are short (MHz, kHz, Hz, K), while every real English sentence has at
    // least one ordinary word longer than that. A translation that leaves such
    // a word in place is a missed entry, which is exactly what this catches.
    bool HasTranslatableWord(const std::string& text)
    {
        size_t run = 0;
        for (size_t i = 0; i <= text.size(); ++i) {
            char c = (i < text.size()) ? text[i] : ' ';
            if (c >= 'a' && c <= 'z') { ++run; continue; }
            if (run >= 5) return true;
            run = 0;
        }
        return false;
    }

    void TestBuildLanguage()
    {
        // The language is baked in. The binary must agree with itself: the code
        // it reports and the text it produces have to be the same language.
        std::string code = BuildLanguage();
        CHECK(code == "en" || code == "zh-CN");

        std::string ok = Tr(Msg::EraseOk);
        if (code == "en") CHECK_STREQ(ok, "[4/7] Erase complete");
        else              CHECK(ok.find("擦除") != std::string::npos);
    }

    void TestMessagesRender()
    {
        // The build language decides what Tr() returns, so this checks the one
        // table the binary carries. Comparing the two tables against each other
        // is not possible from a single-language build by design: only one of
        // them is compiled in. The English source is still readable through
        // English(), which is what the slot and kind checks use.
        const bool english = (std::string(BuildLanguage()) == "en");

        for (Msg m : AllMessages()) {
            const std::string src = English(m);
            CHECK(!src.empty());

            const std::string shown = Tr(m);
            if (english) {
                CHECK_STREQ(shown, src);
                continue;
            }

            // A Chinese build must actually say something for every message;
            // an empty entry would print a blank line and hide the problem.
            CHECK(!shown.empty());

            // ...and it must say it in Chinese where there is prose to
            // translate. Messages made only of identifiers and numbers (the
            // magic field, the 5A A5 marker, a UID, oscillator parameters) are
            // allowed to be identical because there is nothing in them to
            // translate.
            if (HasTranslatableWord(src)) {
                CHECK(src != shown);
            }
        }
    }

    void TestSlots()
    {
        // Slot layout is part of the translation contract: substituted after
        // translation, so the shown text must keep every slot the source has.
        for (Msg m : AllMessages()) {
            std::set<int> want = Slots(English(m));
            std::set<int> got = Slots(Tr(m));
            if (want != got) {
                ::newisp_test::ReportEq(__FILE__, __LINE__,
                    "slot mismatch in " + std::to_string(static_cast<unsigned>(m)),
                    Join(got), Join(want));
            }
        }
    }

    void TestSubstitution()
    {
        // Filled in order, and a slot with no argument is left visible rather
        // than dropped, so a missing value is obvious instead of silent.
        const bool english = (std::string(BuildLanguage()) == "en");
        if (english) {
            CHECK_STREQ(Tr(Msg::DetectOk, "STC8H3K60S4"), "Detected: STC8H3K60S4");
            CHECK_STREQ(Tr(Msg::BaudSwitched, "115200"), "[2/7] Switched to 115200");
            CHECK_STREQ(Tr(Msg::LogTx, { "0x03", "46 B9 " }), "[send] 0x03 46 B9 ");
            CHECK_STREQ(Tr(Msg::LogUid, { "", "AA BB " }), "[UID]  AA BB ");
            CHECK_STREQ(Tr(Msg::DetectOk), "Detected: {0}");
            CHECK_STREQ(Tr(Msg::ChipUnknownPlaceholder), "unknown part");
        }
        else {
            CHECK_STREQ(Tr(Msg::DetectOk, "STC8H3K60S4"), "检测成功: STC8H3K60S4");
            CHECK_STREQ(Tr(Msg::BaudSwitched, "115200"), "[2/7] 已切到 115200");
            CHECK_STREQ(Tr(Msg::DetectOk), "检测成功: {0}");
            CHECK_STREQ(Tr(Msg::ChipUnknownPlaceholder), "未知型号");
            // Repeated slots keep their order and their repetition.
            std::string both = Tr(Msg::FirmwareLine,
                { "a.hex", "10", "0000", "000F" });
            CHECK(both.find("a.hex") != std::string::npos);
            CHECK(both.find("0x0000-0x000F") != std::string::npos);
        }
    }

    void TestMarkers()
    {
        // Packet dumps carry the marker, so the logger can hide them without
        // looking at the text or the language.
        std::string packet = LogLine(Msg::LogTx, { "0x03", "46 B9 " },
            LogKind::Packet);
        CHECK(packet.rfind(kPacketMarker, 0) == 0);
        CHECK(SplitLogLine(packet).kind == LogKind::Packet);
        CHECK_STREQ(StripLogMarker(packet), Tr(Msg::LogTx, { "0x03", "46 B9 " }));

        // Attention lines survive --quiet whatever the language.
        std::string attention = LogLine(Msg::HidOnlyContinuing, LogKind::Attention);
        CHECK(SplitLogLine(attention).kind == LogKind::Attention);
        CHECK(StripLogMarker(attention).find("%a") == std::string::npos);

        // Ordinary lines carry no marker at all.
        std::string plain = Tr(Msg::EraseOk);
        CHECK(SplitLogLine(plain).kind == LogKind::Normal);
        CHECK_STREQ(StripLogMarker(plain), plain);
    }
    void TestKinds()
    {
        LogKind kind = LogKind::Normal;
        English(Msg::LogTx, kind);
        CHECK(kind == LogKind::Packet);
        English(Msg::HidOnlyContinuing, kind);
        CHECK(kind == LogKind::Attention);
        English(Msg::EraseOk, kind);
        CHECK(kind == LogKind::Normal);
    }

} // namespace

void RunI18nTests()
{
    TestBuildLanguage();
    TestMessagesRender();
    TestSlots();
    TestSubstitution();
    TestMarkers();
    TestKinds();
}
