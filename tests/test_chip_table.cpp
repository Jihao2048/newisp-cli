// Chip table lookup tests.

#include "test.h"

#include "newisp/chip_table.h"

namespace {

    void TestKnownParts()
    {
        // A representative part from each family the table covers. These IDs
        // are the ones the protocol code keys its behaviour on.
        CHECK_STREQ(ChipNameFromTable(0xFFFF), "STC89C516RD");
        CHECK_STREQ(ChipNameFromTable(0xF8B4), "STC8051U34K64");
        CHECK_STREQ(ChipNameFromTable(0xF743), "STC8H3K60S4");
        CHECK_STREQ(ChipNameFromTable(0x7701), "AI8C1K02");
        CHECK_STREQ(ChipNameFromTable(0xF8D1), "STC32G144K246");
    }

    void TestUnknownIdGetsPlaceholder()
    {
        // 0x0000 is not a real chip ID. The placeholder is prose shown to the
        // user, so it is translated and the exact wording depends on the build
        // language -- but it must never be empty, or the chip report would show
        // a blank name and look like a parsing bug.
        std::string name = ChipNameFromTable(0x0000);
        CHECK(!name.empty());
        CHECK_STREQ(name, UnknownChipName());

        // A plausible-looking but absent ID must take the same path.
        CHECK_STREQ(ChipNameFromTable(0x0001), UnknownChipName());

        // The English constant stays English whatever the build language, so
        // callers and logs that compare against it keep working.
        CHECK_STREQ(kUnknownChipName, "unknown part");
    }

    void TestTableIsNotEmpty()
    {
        CHECK(kChipTable.size() > 1000);
    }

    void TestNoNullNames()
    {
        // Every entry must be a real name: a null pointer would be turned into
        // a std::string by the lookup and crash.
        for (const auto& entry : kChipTable) {
            if (entry.second == nullptr) {
                ::newisp_test::Report(__FILE__, __LINE__,
                    "null name for chip id " + std::to_string(entry.first));
                continue;
            }
            if (entry.second[0] == '\0') {
                ::newisp_test::Report(__FILE__, __LINE__,
                    "empty name for chip id " + std::to_string(entry.first));
            }
        }
    }

} // namespace

void RunChipTableTests()
{
    TestKnownParts();
    TestUnknownIdGetsPlaceholder();
    TestTableIsNotEmpty();
    TestNoNullNames();
}
