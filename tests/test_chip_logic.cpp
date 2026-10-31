// Chip-family predicate and protocol-selection tests.
//
// These rules decide which protocol implementation talks to a chip and which
// limits apply to it, so a mistake here shows up as a chip that cannot be
// programmed at all.

#include "test.h"

#include "newisp/chip_logic.h"

using namespace newisp;

namespace {

    void TestFamilyPredicates()
    {
        CHECK(IsStc12Family("STC12C5A60S2"));
        CHECK(IsStc12Family("STC10F08XE"));
        CHECK(IsStc12Family("IAP11F06"));
        CHECK(!IsStc12Family("STC15F104E"));
        CHECK(!IsStc12Family("STC8H3K60S4"));

        CHECK(IsStc15Family("STC15F104E"));
        CHECK(IsStc15Family("IAP15W4K58S4"));
        CHECK(IsStc15Family("IRC15W4K"));
        CHECK(IsStc15Family("STC15H2K32S4"));

        CHECK(IsStc8gFamily("STC8G1K08"));
        CHECK(IsStc8gFamily("AI8G1K08"));
        CHECK(IsStc8gFamily("JX8G1K08"));
        CHECK(!IsStc8gFamily("STC8H3K60S4"));

        CHECK(IsExternalCrystalFamily("STC89C516RD"));
        CHECK(IsExternalCrystalFamily("STC90C58RD"));
        CHECK(!IsExternalCrystalFamily("STC12C5A60S2"));
    }

    void TestHidOnlyDetection()
    {
        // The predicate still recognises the family so the tool can mention it,
        // but it no longer gates anything.
        CHECK(IsHidOnlyChip("STC8051U34K64"));
        CHECK(IsHidOnlyChip("AI8051U34K64"));
        CHECK(!IsHidOnlyChip("STC8H3K60S4"));
        CHECK(!IsHidOnlyChip("STC32G12K128"));

        // No chip is refused on transport grounds any more.
        //
        // The rule that used to reject an 8051U over serial was inherited from
        // the vendor material and never measured. Measured on an AI8051U34K64
        // with BSL 7.4U, the serial monitor answers the 0x7F wakeup with a
        // 0x50-prefixed status packet and a full erase/program/option cycle
        // succeeds, so the refusal was wrong. The probe decides now.
        CHECK(!IsUnsupportedForTransport("AI8051U34K64", false));
        CHECK(!IsUnsupportedForTransport("AI8051U34K64", true));
        CHECK(!IsUnsupportedForTransport("STC8H3K60S4", false));
        CHECK(!IsUnsupportedForTransport("", false));
    }

    void TestProtoIndexFromChipName()
    {
        CHECK_EQ(ProtoIdxFromChipName("STC89C516RD"), kProtoIdxStc89);
        CHECK_EQ(ProtoIdxFromChipName("STC90C58RD"), kProtoIdxStc89);
        CHECK_EQ(ProtoIdxFromChipName("STC12C5A60S2"), kProtoIdxStc12);
        CHECK_EQ(ProtoIdxFromChipName("STC15F104E"), kProtoIdxStc15);
        CHECK_EQ(ProtoIdxFromChipName("STC8G1K08"), kProtoIdxStc8G);
        CHECK_EQ(ProtoIdxFromChipName("STC8H3K60S4"), kProtoIdxStc32);
        CHECK_EQ(ProtoIdxFromChipName("STC32G12K128"), kProtoIdxStc32);
        CHECK_EQ(ProtoIdxFromChipName("STC8051U34K64"), kProtoIdxStc32);
        CHECK_EQ(ProtoIdxFromChipName("STC8F2K64S4"), kProtoIdxStc8);

        // Unknown and empty names have no family.
        CHECK_EQ(ProtoIdxFromChipName(""), -1);
        CHECK_EQ(ProtoIdxFromChipName("ATMEGA328P"), -1);
    }

    void TestFrequencyCaps()
    {
        // Ceilings, per family. Each one is the last entry of that family's own
        // table, so the ceilings are asserted through the tables rather than as
        // bare index numbers:
        //   60.000 MHz  STC32G144K246
        //   45.1584 MHz STC8H, 8051U
        //   45.000 MHz  STC8A  (deliberately not 45.1584)
        //   35.000 MHz  STC8G, STC15
        //   none        STC12, STC89/90 (see TestFrequencyIsSelectable)
        CHECK_EQ(FreqMaxIdxFor("STC8G1K08"), kFreqMaxIdxStc8G);
        CHECK_EQ(FreqMaxIdxFor("AI8G1K08"), kFreqMaxIdxStc8G);
        CHECK_EQ(FreqMaxIdxFor("STC15F104E"), kFreqMaxIdxStc15);
        CHECK_EQ(FreqMaxIdxFor("IAP15W4K61S4"), kFreqMaxIdxStc15);

        CHECK_EQ(FreqMaxIdxFor("STC32G144K246"), kFreqMaxIdxStc32);
        CHECK_EQ(FreqMaxIdxFor("AI32G144K246"), kFreqMaxIdxStc32);
        CHECK_EQ(FreqMaxIdxFor("STC8H8K64U"), kFreqMaxIdxStc8H);
        CHECK_EQ(FreqMaxIdxFor("AI8H8K64U"), kFreqMaxIdxStc8H);
        CHECK_EQ(FreqMaxIdxFor("AI8051U34K64"), kFreqMaxIdx8051U);
        CHECK_EQ(FreqMaxIdxFor("STC8051U34K64"), kFreqMaxIdx8051U);
        // Both 8A naming styles land on the 8A table: the S4A12 parts are the
        // original 8A silicon, and the D4 parts are its later revision.
        CHECK_EQ(FreqMaxIdxFor("STC8A8K64D4"), kFreqMaxIdxStc8A);
        CHECK_EQ(FreqMaxIdxFor("STC8A8K64S4A12"), kFreqMaxIdxStc8A);

        CHECK_EQ(kFreqTableStc8G[kFreqMaxIdxStc8G], 35000000u);
        CHECK_EQ(kFreqTableStc15[kFreqMaxIdxStc15], 35000000u);
        CHECK_EQ(kFreqTableStc32G144K246[kFreqMaxIdxStc32], 60000000u);
        CHECK_EQ(kFreqTableStc8H[kFreqMaxIdxStc8H], 45158400u);
        CHECK_EQ(kFreqTableStc8051U[kFreqMaxIdx8051U], 45158400u);
        CHECK_EQ(kFreqTableStc8A[kFreqMaxIdxStc8A], 45000000u);

        // Entry counts, straight from docs/irc-frequency-tables.md. The
        // 32G144K246 menu lists 17 there; two of them have no trim triple yet
        // and are not selectable, which is what this table counts.
        CHECK_EQ(kNumFreqStc32G144K246, 15);
        CHECK_EQ(kNumFreqStc8051U, 16);
        CHECK_EQ(kNumFreqStc8H, 16);
        CHECK_EQ(kNumFreqStc8A, 16);
        CHECK_EQ(kNumFreqStc8G, 12);
        CHECK_EQ(kNumFreqStc15, 12);
    }

    void TestFrequencyMenusAreDistinct()
    {
        // 45.1584 and 45.000 are 0.35% apart and are not interchangeable: 8H
        // and 8051U end at the former, 8A at the latter. Getting this wrong is
        // invisible at compile time and shows up as a clock that is slightly
        // off, which is exactly the failure the per-family tables exist for.
        CHECK(IsFrequencyInMenu("STC8H8K64U", 45158400u));
        CHECK(!IsFrequencyInMenu("STC8H8K64U", 45000000u));
        CHECK(IsFrequencyInMenu("STC8A8K64S4A12", 45000000u));
        CHECK(!IsFrequencyInMenu("STC8A8K64S4A12", 45158400u));

        // 60 MHz belongs to the 32G144K246 menu, whose ceiling comes from its
        // 72 MHz IRC band and programmable HPLL. Note what this does *not*
        // claim: an older STC32G (12K128 and relatives) is not separated from
        // the 144K246 here, so it is offered the same 60 MHz menu entry. The
        // option stage has no trim triple for an old STC32G at that frequency,
        // and refusing by name is the correct outcome -- adding a part-number
        // special case would replace that honest refusal with a guess about
        // which silicon a name maps to.
        CHECK(IsFrequencyInMenu("STC32G144K246", 60000000u));
        CHECK(IsFrequencyInMenu("STC32G12K128", 60000000u));
        CHECK(IsFrequencyInMenu("STC32G12K128", 48000000u));

        // 27.000 is in five of the six menus but not the 32G144K246 one.
        CHECK(IsFrequencyInMenu("STC8H8K64U", 27000000u));
        CHECK(IsFrequencyInMenu("STC8A8K64S4A12", 27000000u));
        CHECK(IsFrequencyInMenu("AI8051U34K64", 27000000u));
        CHECK(IsFrequencyInMenu("STC8G1K08", 27000000u));
        CHECK(IsFrequencyInMenu("STC15F104E", 27000000u));
        CHECK(!IsFrequencyInMenu("STC32G144K246", 27000000u));

        // 32.000 is a 32G144K246 menu entry with no measured trim, so it is not
        // selectable; no other family offers it at all.
        CHECK(!IsFrequencyInMenu("STC32G144K246", 32000000u));
        CHECK(!IsFrequencyInMenu("STC8H8K64U", 32000000u));

        // 8G and 15 have 12 identical entries, but they are still two tables.
        CHECK_EQ(kNumFreqStc8G, kNumFreqStc15);
        for (int i = 0; i < kNumFreqStc8G; ++i)
            CHECK_EQ(kFreqTableStc8G[i], kFreqTableStc15[i]);

        // Same for 8051U and 8H, which the vendor still lists separately.
        CHECK_EQ(kNumFreqStc8051U, kNumFreqStc8H);
        for (int i = 0; i < kNumFreqStc8051U; ++i)
            CHECK_EQ(kFreqTableStc8051U[i], kFreqTableStc8H[i]);

        // 8A differs from 8H in exactly one place: the last entry.
        CHECK_EQ(kNumFreqStc8A, kNumFreqStc8H);
        for (int i = 0; i < kNumFreqStc8A - 1; ++i)
            CHECK_EQ(kFreqTableStc8A[i], kFreqTableStc8H[i]);

        // Membership is exact, not a range check: 45.100 sits between two real
        // 8H entries and is still not selectable.
        CHECK(!IsFrequencyInMenu("STC8H8K64U", 45100000u));
        CHECK(!IsFrequencyInMenu("STC8H8K64U", 0u));
    }

    void TestEveryMenuEntryHasTrim()
    {
        // The option stage can only write a trim triple it has. If a menu grew
        // an entry the trim table lacks, the tool would offer a frequency and
        // then refuse it after the erase started -- the menu and HasTrimFor
        // have to agree, and this is where they are made to.
        //
        // Two entries are deliberately outside that agreement yet:
        //   44.2368  a real 8H/8A/8G/15 menu entry with no measured triple
        //   32.000   a real 32G144K246 menu entry with no measured triple
        // Both are listed in the menu and refused by name at the option stage,
        // so nothing is written blind. They are skipped here, not excused.
        const uint32_t noTrimYet[] = { 44236800u, 32000000u };
        const char* names[] = {
            "STC32G144K246", "STC8H8K64U", "STC8A8K64S4A12",
            "AI8051U34K64", "STC8G1K08", "STC15F104E"
        };
        for (const char* n : names) {
            FrequencyMenu m = FrequencyMenuFor(n);
            for (int i = 0; i <= m.maxIdx; ++i) {
                const uint32_t f = m.hz[i];
                bool skipped = false;
                for (uint32_t skip : noTrimYet) if (f == skip) skipped = true;
                if (skipped) continue;
                if (!HasTrimFor(f)) {
                    std::fprintf(stderr, "  no trim for %u on %s\n", (unsigned)f, n);
                }
                CHECK(HasTrimFor(f));
            }
        }

        // 27.000 is the entry this branch newly exposes: its triple was in the
        // table all along, but the old global frequency table never offered it.
        CHECK(HasTrimFor(27000000u));
        // 45.1584 was added with the 8H/8051U menu; 8A's 45.000 must still
        // resolve to its own triple, not to this one.
        CHECK(HasTrimFor(45158400u));
        CHECK(HasTrimFor(45000000u));

        // Sub-MHz divider values stay in the trim table without appearing in
        // any menu: no family offers them as an option-byte frequency.
        CHECK(HasTrimFor(6000000u));
        CHECK(!IsFrequencyInMenu("STC8H8K64U", 1000000u));
    }

    void TestFrequencyIsSelectable()
    {
        // STC12 (10/11/12, IAP10/11/12) has no selectable frequency: it runs
        // from whichever clock source is fitted, and the option byte records
        // only the source. STC89/90 runs from a crystal and has no option.
        CHECK(!HasSelectableFrequency("STC12C5A60S2"));
        CHECK(!HasSelectableFrequency("STC12LE5A60S2"));
        CHECK(!HasSelectableFrequency("IAP12C5A60S2"));
        CHECK(!HasSelectableFrequency("STC10F08XE"));
        CHECK(!HasSelectableFrequency("STC11F60XE"));
        CHECK(!HasSelectableFrequency("STC89C52RC"));
        CHECK(!HasSelectableFrequency("STC90C58RD"));

        // Everything else takes a target IRC frequency.
        CHECK(HasSelectableFrequency("STC15W408AS"));
        CHECK(HasSelectableFrequency("STC8G1K08"));
        CHECK(HasSelectableFrequency("STC8H8K64U"));
        CHECK(HasSelectableFrequency("STC32G12K128"));
        CHECK(HasSelectableFrequency("AI8051U34K64"));

        // An empty name means "not identified yet": do not reject -F before the
        // chip has been probed.
        CHECK(HasSelectableFrequency(""));
    }

    void TestEepromIsSelectable()
    {
        // The EEPROM split is a field in the option block, and only the
        // STC8/STC32 generation has it.
        CHECK(HasSelectableEeprom("STC8H8K64U"));
        CHECK(HasSelectableEeprom("AI8H8K64U"));
        CHECK(HasSelectableEeprom("STC32G12K128"));
        CHECK(HasSelectableEeprom("AI8051U34K64"));
        CHECK(HasSelectableEeprom("STC8G1K08"));
        CHECK(HasSelectableEeprom("STC8F1K08"));
        CHECK(HasSelectableEeprom("STC8A8K64S4A12"));
        CHECK(HasSelectableEeprom("GX8S405"));

        // The older generations cannot: STC15 and STC12 keep their EEPROM as a
        // fixed region, and STC89/90 have none at all.
        CHECK(!HasSelectableEeprom("STC15W408AS"));
        CHECK(!HasSelectableEeprom("STC15F104E"));
        CHECK(!HasSelectableEeprom("IAP15W4K61S4"));
        CHECK(!HasSelectableEeprom("STC12C5A60S2"));
        CHECK(!HasSelectableEeprom("IAP12C5A60S2"));
        CHECK(!HasSelectableEeprom("STC10F08XE"));
        CHECK(!HasSelectableEeprom("STC89C52RC"));
        CHECK(!HasSelectableEeprom("STC90C58RD"));

        // Not identified yet: do not reject -e before the probe.
        CHECK(HasSelectableEeprom(""));
    }

    void TestLegacyBaudAndPrepareRules()
    {
        // STC89/12 have an 8-bit BRT with a fixed divider and cannot reach past
        // 115200, so the UI and the burn path both clamp them.
        CHECK(IsLegacyBaudProto("stc89"));
        CHECK(IsLegacyBaudProto("stc89a"));
        CHECK(IsLegacyBaudProto("stc12"));
        CHECK(IsLegacyBaudProto("stc12a"));
        CHECK(IsLegacyBaudProto("stc12b"));
        CHECK(!IsLegacyBaudProto("stc15"));
        CHECK(!IsLegacyBaudProto("stc32"));

        // The 0x05 prepare step does not exist on those older families.
        CHECK(!NeedsPrepare05("stc89"));
        CHECK(!NeedsPrepare05("stc89a"));
        CHECK(!NeedsPrepare05("stc12"));
        CHECK(!NeedsPrepare05("stc12a"));
        CHECK(!NeedsPrepare05("stc12b"));
        CHECK(NeedsPrepare05("stc15"));
        CHECK(NeedsPrepare05("stc15a"));
        CHECK(NeedsPrepare05("stc32"));
        CHECK(NeedsPrepare05("stc8g"));
    }

    void TestHidReachability()
    {
        // Only the STC32 menu slot covers silicon with a USB controller: it is
        // shared by STC8H, STC32 and the 8051U. The 8G parts are serial-only,
        // so their slot is not USB reachable even though a HID protocol class
        // exists for the 08A variant of that range.
        CHECK(IsHidReachableProto(kProtoIdxStc32));
        CHECK(!IsHidReachableProto(kProtoIdxStc8G));
        CHECK(!IsHidReachableProto(kProtoIdxStc89));
        CHECK(!IsHidReachableProto(kProtoIdxStc12));
        CHECK(!IsHidReachableProto(kProtoIdxStc15));
        CHECK(!IsHidReachableProto(kProtoIdxStc8));
    }

    void TestUsbCapableChips()
    {
        // Exactly three families have a USB controller inside the part.
        CHECK(IsUsbCapableChip("STC8H3K60S4"));
        CHECK(IsUsbCapableChip("STC8H8K64U"));
        CHECK(IsUsbCapableChip("AI8H8K64U"));
        CHECK(IsUsbCapableChip("STC32G12K128"));
        CHECK(IsUsbCapableChip("STC32G144K246"));
        CHECK(IsUsbCapableChip("AI32G12K128"));
        CHECK(IsUsbCapableChip("STC8051U34K64"));
        CHECK(IsUsbCapableChip("AI8051U34K64"));

        // The 8H slot also covers the 8A8K/8A2K relatives, which are 8H silicon.
        CHECK(IsUsbCapableChip("STC8A8K64S4A12"));
        CHECK(IsUsbCapableChip("AI8A2K17S4"));

        // Everything else is serial-only: no USB controller at all.
        CHECK(!IsUsbCapableChip("STC8G1K08"));
        CHECK(!IsUsbCapableChip("AI8G1K08"));
        CHECK(!IsUsbCapableChip("STC8F1K08"));
        CHECK(!IsUsbCapableChip("STC8C2K64S4"));
        CHECK(!IsUsbCapableChip("GX8S405"));
        CHECK(!IsUsbCapableChip("STC15W408AS"));
        CHECK(!IsUsbCapableChip("IAP15W4K61S4"));
        CHECK(!IsUsbCapableChip("STC12C5A60S2"));
        CHECK(!IsUsbCapableChip("STC89C516RD"));
        CHECK(!IsUsbCapableChip(""));
        CHECK(!IsUsbCapableChip(UnknownChipName()));

        // The inverse predicate has to agree with it everywhere.
        CHECK(IsSerialOnlyChip("STC8G1K08"));
        CHECK(IsSerialOnlyChip("STC15W408AS"));
        CHECK(!IsSerialOnlyChip("STC8H8K64U"));
        CHECK(!IsSerialOnlyChip("AI8051U34K64"));
        // An empty name means "not identified yet", not "serial-only".
        CHECK(!IsSerialOnlyChip(""));
    }

    void TestProbeListSizes()
    {
        // Serial probes all ten variants, because any family may answer.
        auto serial = MakeProbeList(false);
        CHECK_EQ(serial.size(), (size_t)10);

        // HID only probes the three implementations the USB parts use.
        auto hid = MakeProbeList(true);
        CHECK_EQ(hid.size(), (size_t)3);
    }

    void TestProtocolFactory()
    {
        // The factory lives in the protocol namespace, while the predicates
        // above are in newisp; name it explicitly here.
        using stc::CreateProtocol;

        // It must return something for every family the tool claims to support,
        // and nullptr only for genuinely unknown names.
        CHECK(CreateProtocol("STC89C516RD", false) != nullptr);
        CHECK(CreateProtocol("STC12C5A60S2", false) != nullptr);
        CHECK(CreateProtocol("STC15F104E", false) != nullptr);
        CHECK(CreateProtocol("STC8G1K08", false) != nullptr);
        CHECK(CreateProtocol("STC8G1K08", true) != nullptr);
        CHECK(CreateProtocol("STC32G12K128", false) != nullptr);
        CHECK(CreateProtocol("STC32G12K128", true) != nullptr);
        CHECK(CreateProtocol("AI8051U34K64", true) != nullptr);
        CHECK(CreateProtocol("ATMEGA328P", false) == nullptr);

        // The 12A/12B variants are selected by name substring, which is the
        // one place the mapping is subtle enough to be worth pinning down.
        CHECK(CreateProtocol("STC12C5A052", false) != nullptr);
        CHECK(CreateProtocol("STC12LE5A60S2", false) != nullptr);
    }

    void TestFlashSizing()
    {
        // The part number encodes the flash size for the 8/32 families.
        CHECK_EQ(GetChipTotalFlash("STC8H3K60S4", 0xF743), 65536u);
        CHECK_EQ(GetChipTotalFlash("STC32G12K128", 0xF8D1), 131072u);
        CHECK_EQ(GetChipTotalFlash("STC8H1K16", 0xF721), 16384u);
        CHECK_EQ(GetChipTotalFlash("STC8G1K08", 0x0000), 12288u);

        // Unknown names fall back to the ID's high byte: 0xfa and above are the
        // 128K parts.
        CHECK_EQ(GetChipTotalFlash("UNKNOWN", 0xFA00), 128u * 1024u);
        CHECK_EQ(GetChipTotalFlash("UNKNOWN", 0xF700), 64u * 1024u);
    }

    void TestDeviceIdExtraction()
    {
        // Both transports use "<friendly name> (<id>)" in their lists.
        CHECK_STREQ(ExtractDeviceId("USB-SERIAL CH340 (COM7)"), "COM7");
        CHECK_STREQ(ExtractDeviceId("STC USB ISP [34BF:1001] (/dev/hidraw0)"),
            "/dev/hidraw0");

        // With no parentheses the text is returned unchanged, so a bare device
        // name pasted from a terminal still works.
        CHECK_STREQ(ExtractDeviceId("/dev/ttyUSB0"), "/dev/ttyUSB0");
        CHECK_STREQ(ExtractDeviceId("COM7"), "COM7");
        // Empty parentheses are not an id.
        CHECK_STREQ(ExtractDeviceId("thing ()"), "thing ()");
    }

    void TestProgrammingPreflight()
    {
        std::string error;

        // STC32 and the 8051U share one implementation, so they pass or fail
        // for the same reasons: image size and EEPROM split, never the name.
        // A 64 KiB image fits an STC32G12K128 exactly and must be accepted.
        CHECK(ValidateProgrammingTarget("STC32G12K128", 65536, 131072, 0, error));
        CHECK(error.empty());
        CHECK(ValidateProgrammingTarget("STC32F12K54", 55296, 55296, 0, error));

        // The 16-bit block address is the real ceiling for every family.
        CHECK(!ValidateProgrammingTarget("STC32G12K128", 65537, 131072, 0, error));
        CHECK(ValidateProgrammingTarget("AI8051U34K64", 65536, 65536, 0, error));
        CHECK(error.empty());
        CHECK(!ValidateProgrammingTarget("AI8051U34K64", 65537, 65536, 0, error));

        // EEPROM split: the code image has to fit beside it.
        CHECK(!ValidateProgrammingTarget("STC8H8K64U", 32769, 65536, 32768, error));
        CHECK(!ValidateProgrammingTarget("STC8H8K64U", 1, 65536, 65537, error));
        CHECK(!ValidateProgrammingTarget("STC8H8K64U", 0, 65536, 0, error));
        CHECK(!ValidateProgrammingTarget("STC8H8K64U", 1, 0, 0, error));
        CHECK(ValidateProgrammingTarget("STC8H8K64U", 32768, 65536, 32768, error));
    }

} // namespace

void RunChipLogicTests()
{
    TestFamilyPredicates();
    TestHidOnlyDetection();
    TestProtoIndexFromChipName();
    TestFrequencyCaps();
    TestFrequencyMenusAreDistinct();
    TestEveryMenuEntryHasTrim();
    TestFrequencyIsSelectable();
    TestEepromIsSelectable();
    TestLegacyBaudAndPrepareRules();
    TestHidReachability();
    TestUsbCapableChips();
    TestProbeListSizes();
    TestProtocolFactory();
    TestFlashSizing();
    TestDeviceIdExtraction();
    TestProgrammingPreflight();
}
