#pragma once
// Chip-family classification and device-list helpers.
//
// These are pure functions: they depend on chip names, protocol names and
// device descriptors, but never on XAML controls. Keeping them out of
// MainWindow.xaml.cpp lets the window code read as UI plumbing, and makes the
// classification rules testable and greppable in one place.
//
// The tables here mirror what the protocol layer implements; see
// protocol_factory.h for the authoritative chip-name -> protocol mapping.

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

#include "newisp/IChannel.h"
#include "newisp/hid_channel.h"
#include "newisp/chip_table.h"
#include "newisp/stc_protocol.h"
#include "newisp/protocol_factory.h"

namespace newisp {

    // ========================================================================
    //  Frequency / baud tables
    // ========================================================================

    // Selectable IRC frequencies, per family.
    //
    // There is no single global table any more, and that is the point: each
    // family has its own menu in AiCube-ISP, and the menus do not agree. The
    // ones here are transcribed from that software; the transcription, with the
    // screenshots it came from and the reasoning for each omission, is
    // docs/irc-frequency-tables.md.
    //
    // The old single table was wrong in three ways: it carried frequencies no
    // family's menu lists (the STC32G144K246 menu has neither 33.1776 nor
    // 22.1184), it stopped at 45.000 so an 8H/8051U could not reach the
    // 45.1584 entry the vendor does offer, and it omitted the 27.000 entry that
    // five of the six families list even though 27.000 had a trim triple all
    // along. Per-family tables fix all three.
    //
    // Order follows the vendor menu, lowest first. A menu entry is only
    // *selectable* when the trim table in protocol_stc8*.h also has a triple
    // for it, since writing the option block without one leaves the IRC
    // untrimmed; the two menus and their trim tables are kept in step by
    // test_chip_logic.cpp (see HasTrimFor).

    // STC32G144K246: 17 entries in the vendor menu, 15 selectable here.
    //
    // The ceiling comes from the IRC's 72 MHz band plus the programmable HPLL.
    // This must not be handed to an older STC32G (12K128 and relatives): those
    // stop at the 48 MHz calibration band and have no 49.152-or-above entry,
    // and note that this menu has no 27.000 entry either.
    //
    // 32.000 and 50.8032 are in the vendor menu but have no trim triple on this
    // bench yet, so they are left out rather than guessed: a frequency with no
    // trim writes an untrimmed IRC. Everything else the menu offers is here.
    inline const uint32_t kFreqTableStc32G144K246[] = {
        5529600, 6000000, 11059200, 12000000, 18432000, 20000000,
        22118400, 24000000, 30000000, 40000000, 45158400, 48000000,
        49152000, 55296000, 60000000
    };
    inline const int kNumFreqStc32G144K246 =
        (int)(sizeof(kFreqTableStc32G144K246) / sizeof(uint32_t));

    // STC8051U / AI8051U, STC8H and STC8A: 16 entries each.
    //
    // The 8051U and 8H menus are identical entry for entry, and 8A differs in
    // exactly one place: its last entry is 45.000, not 45.1584. That is a 0.35%
    // difference, not a rounding difference, so each family keeps its own
    // constant even where the content coincides: the vendor validates the menus
    // separately, and folding them into one is how the 45.000/45.1584 pair got
    // confused before.
    inline const uint32_t kFreqTableStc8051U[] = {
        5529600, 6000000, 11059200, 12000000, 18432000, 20000000,
        22118400, 24000000, 27000000, 30000000, 33177600, 35000000,
        36864000, 40000000, 44236800, 45158400
    };
    inline const int kNumFreqStc8051U =
        (int)(sizeof(kFreqTableStc8051U) / sizeof(uint32_t));

    inline const uint32_t kFreqTableStc8H[] = {
        5529600, 6000000, 11059200, 12000000, 18432000, 20000000,
        22118400, 24000000, 27000000, 30000000, 33177600, 35000000,
        36864000, 40000000, 44236800, 45158400
    };
    inline const int kNumFreqStc8H =
        (int)(sizeof(kFreqTableStc8H) / sizeof(uint32_t));

    inline const uint32_t kFreqTableStc8A[] = {
        5529600, 6000000, 11059200, 12000000, 18432000, 20000000,
        22118400, 24000000, 27000000, 30000000, 33177600, 35000000,
        36864000, 40000000, 44236800, 45000000
    };
    inline const int kNumFreqStc8A =
        (int)(sizeof(kFreqTableStc8A) / sizeof(uint32_t));

    // STC8G and STC15: 12 entries, they stop at 35 MHz.
    //
    // 35 MHz is also the ceiling the manual's own guidance lands on for the
    // 8H1K08/8H1K28 two-band parts ("建议用户在ISP下载时设置IRC频率不要高于
    // 35MHz"): some of those die out at 39.5 MHz.
    inline const uint32_t kFreqTableStc8G[] = {
        5529600, 6000000, 11059200, 12000000, 18432000, 20000000,
        22118400, 24000000, 27000000, 30000000, 33177600, 35000000
    };
    inline const int kNumFreqStc8G =
        (int)(sizeof(kFreqTableStc8G) / sizeof(uint32_t));

    inline const uint32_t kFreqTableStc15[] = {
        5529600, 6000000, 11059200, 12000000, 18432000, 20000000,
        22118400, 24000000, 27000000, 30000000, 33177600, 35000000
    };
    inline const int kNumFreqStc15 =
        (int)(sizeof(kFreqTableStc15) / sizeof(uint32_t));

    // The ceiling per family, as an index into that family's table.
    //
    //   45.1584 MHz  STC8051U, STC8H   (the top of their tables)
    //   45.000 MHz   STC8A             (its last entry, deliberately different)
    //   60.000 MHz   STC32G144K246
    //   35.000 MHz   STC8G, STC15
    //   n/a          STC12 and STC89/90 -- these take a clock *source*, not a
    //                frequency; see HasSelectableFrequency
    //
    // What is left out of these tables is left out on purpose; the full list
    // with reasons is in docs/irc-frequency-tables.md.
    //
    //   44.2368   a vendor menu entry for 8051U/8H/8A/8G/15 with no trim
    //             triple here, and the frequency at which USB stops
    //             enumerating in the community reports. These parts are
    //             programmed over USB, so it stays out.
    //   32.000    a 32G144K246 menu entry with no measured trim.
    //   50.8032   a 32G144K246 menu entry with no measured trim.
    //
    // 55.296 and 60.000 *are* in the 32G144K246 list, and 49.152 too. Removing
    // an entry from a table is what makes it unselectable: this table is what
    // the option stage filters on, so there is no second place to keep in step.
    inline const int kFreqMaxIdxStc8G = kNumFreqStc8G - 1;   // 35.000 MHz
    inline const int kFreqMaxIdxStc15 = kNumFreqStc15 - 1;   // 35.000 MHz
    inline const int kFreqMaxIdxStc32 = kNumFreqStc32G144K246 - 1; // 60 MHz
    inline const int kFreqMaxIdxStc8H = kNumFreqStc8H - 1;   // 45.1584 MHz
    inline const int kFreqMaxIdxStc8A = kNumFreqStc8A - 1;   // 45.000 MHz
    inline const int kFreqMaxIdx8051U = kNumFreqStc8051U - 1; // 45.1584 MHz

    // Serial baud rates, in slider order.
    inline const uint32_t kBaudTable[] = {
        2400, 4800, 9600, 19200, 38400, 115200, 230400, 460800
    };
    inline const int kNumBaud = 8;

    // STC89/90/10/11/12 are locked to 115200 (index 5).
    //
    // Their BRT register is 8 bits wide with a fixed 16x divider, i.e.
    // baud = clock / (16 * (256 - BRT)). At 24 MHz, BRT = 0xF3 gives 115200;
    // reaching 230400 needs (256 - BRT) = 6.51, and the rounding error exceeds
    // 7% (UART tolerance is about 3%), so the handshake would always fail.
    //
    // stcgal behaves the same way: its calculate_baud() raises
    // "requested baudrate cannot be set" for this case (protocols.py:1123).
    // We clamp in the UI instead, so the user cannot pick a rate that fails
    // mid-burn.
    inline const int kBaudMaxIdxStc12 = 5;

    // ========================================================================
    //  Clock source
    // ========================================================================

    // The overclock band (46-50 MHz) stays out of every family table.
    //
    // It is none of it in the vendor menus, so there is no menu entry to
    // transcribe it from. Only 46 and 47 MHz were ever measured to work on this
    // bench, 48 MHz was measured to *drop* the clock, and 49/50 MHz were
    // extrapolated from a fitted curve rather than tested. A clock the chip
    // cannot actually reach is worse than no option at all: the program runs at
    // an unknown speed and the only symptom is timing that is subtly wrong.

    // STC12 family: clock source replaces the frequency menu entirely.
    // 0 = internal IRC, 1 = external crystal.
    //
    // Deprecated: kept for callers that want the English spelling. Use
    // ClockSourceText() so the field follows the UI language.
    inline const char* kClockSrcText[] = { "internal IRC", "external crystal" };

    // Clock-source label for the active language: 0 = internal, 1 = external.
    inline std::string ClockSourceText(int which)
    {
        return newisp::i18n::Tr(which == 0
            ? newisp::i18n::Msg::LogClockSourceInternalIrc
            : newisp::i18n::Msg::LogClockSourceExternalCrystal);
    }

    // ========================================================================
    //  Chip-family menu indices (order of GetProtoItems in MainWindow)
    // ========================================================================
    inline const int kProtoIdxStc89 = 0;
    inline const int kProtoIdxStc12 = 1;
    inline const int kProtoIdxStc15 = 2;
    inline const int kProtoIdxStc8G = 3;
    inline const int kProtoIdxStc32 = 4;
    inline const int kProtoIdxStc8 = 5;

    // ========================================================================
    //  Chip-name predicates
    // ========================================================================

    inline bool StartsWith(const std::string& s, const char* prefix)
    {
        return s.rfind(prefix, 0) == 0;
    }

    // Capacity is the only thing that decides this, not the part family.
    //
    // STC32 and the 8051U share one implementation: ProtocolStc32 (serial and
    // HID) writes 64-byte blocks whose address is two bytes wide, so both are
    // programmed identically and both are good up to 64 KiB of code. There is
    // no STC32-specific refusal here any more: what limits a target is the
    // image size and the EEPROM split, which are checked below for every
    // family alike.
    //
    // What this cannot do is a banked image larger than 64 KiB: the wire format
    // carries a 16-bit address, so a larger image is rejected rather than
    // truncated. That affects the big STC32 parts just as it affects a banked
    // 8051U image, and the message says so.
    inline bool ValidateProgrammingTarget(const std::string& chipName,
        size_t imageBytes, uint32_t totalFlash, uint32_t eepromBytes,
        std::string& error)
    {
        (void)chipName;
        error.clear();
        if (imageBytes == 0 || imageBytes > 0x10000u || totalFlash == 0 ||
            eepromBytes > totalFlash || imageBytes > totalFlash - eepromBytes) {
            error = "firmware exceeds the writer/target capacity or EEPROM split is invalid; no erase issued";
            return false;
        }
        return true;
    }

    inline std::string ProtoIdxToChipName(int idx)
    {
        switch (idx) {
        case kProtoIdxStc89: return "STC89";
        case kProtoIdxStc12: return "STC12";
        case kProtoIdxStc15: return "STC15";
        case kProtoIdxStc8G: return "STC8G";
        case kProtoIdxStc32: return "STC32";
        case kProtoIdxStc8:  return "STC8";
        default:             return "STC32";
        }
    }

    // STC10/11/12 need a clock-source choice instead of a frequency.
    inline bool IsStc12Family(const std::string& chipName)
    {
        return StartsWith(chipName, "STC10") || StartsWith(chipName, "STC11") ||
            StartsWith(chipName, "STC12") || StartsWith(chipName, "IAP10") ||
            StartsWith(chipName, "IAP11") || StartsWith(chipName, "IAP12");
    }

    inline bool IsStc8gFamily(const std::string& chipName)
    {
        return StartsWith(chipName, "STC8G") || StartsWith(chipName, "AI8G") ||
            StartsWith(chipName, "JX8G");
    }

    inline bool IsStc15Family(const std::string& chipName)
    {
        return StartsWith(chipName, "STC15") || StartsWith(chipName, "IAP15") ||
            StartsWith(chipName, "IRC15");
    }

    // STC89/90 always run from an external crystal, so the frequency menu is
    // irrelevant for them.
    inline bool IsExternalCrystalFamily(const std::string& chipName)
    {
        return StartsWith(chipName, "STC89") || StartsWith(chipName, "STC90");
    }

    // The 8051U family, which the vendor material frames as a HID part.
    //
    // The framing does not hold up: an 8051U answers on its serial ISP monitor
    // like every other part here (see IsUnsupportedForTransport). What is left
    // of this predicate is a label for a warning, so that a failed serial
    // attempt on one of these parts can mention that the USB interface is the
    // documented route. It gates nothing.
    inline bool IsHidOnlyChip(const std::string& chipName)
    {
        return chipName.find("8051U") != std::string::npos ||
            chipName.find("8051u") != std::string::npos;
    }

    // True only when the chip should not be programmed over the active
    // transport.
    //
    // This always returns false: every part this tool knows about answers on
    // its serial ISP monitor. There used to be a rule refusing an 8051U over
    // serial because the manual frames it as a HID part, and a -force-serial
    // flag to override that rule -- but the rule was inherited rather than
    // measured, and the probe already answers the question. A wakeup byte that
    // gets a framed, checksum-valid status packet back proves the part is
    // listening. A part that is not listening fails the probe on its own, with
    // a clear timeout, and nothing after it runs.
    //
    // The flag is gone with the rule it overrode. The function is kept as a
    // named place for the answer, and as the thing to change if a transport
    // restriction ever turns out to be real.
    inline bool IsUnsupportedForTransport(const std::string& chipName,
        bool useHid)
    {
        (void)chipName;
        (void)useHid;
        return false;
    }

    // True when the family is reachable over the factory USB HID ISP interface.
    //
    // Only three families put a USB controller inside the part:
    //
    //   STC8H   the 8H range, plus the AI8H and 8A8K/8A2K relatives
    //   STC32   STC32G / STC32F and the AI32 equivalents
    //   8051U   STC8051U / AI8051U
    //
    // Everything else -- the STC8F/8A/8C/GX8S group served by ProtocolStc8, and
    // the whole 89/12/15 generation -- has no USB interface at all. Those parts
    // only ever appear on a serial port, so listing them as HID targets would
    // send the user looking for a device that can never enumerate.
    //
    // Note that STC8G is *not* on this list even though a ProtocolStc8GHid class
    // exists: the 8G parts are serial-only, and the HID class is there for the
    // 08A variant of that range, which is a different silicon.
    inline bool IsUsbCapableChip(const std::string& chipName)
    {
        if (chipName.empty()) return false;
        if (StartsWith(chipName, "STC8H") || StartsWith(chipName, "AI8H"))
            return true;
        if (StartsWith(chipName, "STC32") || StartsWith(chipName, "AI32"))
            return true;
        if (StartsWith(chipName, "STC8051U") || StartsWith(chipName, "AI8051U"))
            return true;
        if (StartsWith(chipName, "STC8A8K") || StartsWith(chipName, "AI8A8K") ||
            StartsWith(chipName, "STC8A2K") || StartsWith(chipName, "AI8A2K"))
            return true;
        return false;
    }

    // The same question asked of a protocol index, for callers that only have
    // the family menu index. Only the STC32 slot covers USB-capable silicon:
    // that slot is shared by STC8H, STC32 and the 8051U, which is why those
    // three cannot be told apart here and why IsUsbCapableChip is preferred
    // whenever a part name is available.
    inline bool IsHidReachableProto(int idx)
    {
        return idx == kProtoIdxStc32;
    }

    // True when the family lets the user pick a clock source.
    //
    // Only the STC10/11/12 generation does. Its option byte records one bit --
    // internal RC or external crystal -- because those parts have no
    // programmable IRC menu at all; the clock source *is* the only clock
    // setting they have (see ProtocolStc12 / ProtocolStc12A ProgramOptions).
    //
    // Every other family either has a frequency menu instead (the IRC is the
    // clock source, tuned by -freq) or takes a crystal with no option byte for
    // it (STC89/90). Writing a clock-source bit on those parts would poke at
    // an unrelated option bit, so -clock is rejected for them rather than
    // accepted and silently dropped.
    inline bool HasSelectableClockSource(const std::string& chipName)
    {
        if (chipName.empty()) return true;   // unknown yet: do not restrict
        return IsStc12Family(chipName);
    }

    // True when the part has no USB interface, so --hid can never find it.
    inline bool IsSerialOnlyChip(const std::string& chipName)
    {
        if (chipName.empty()) return false;
        return !IsUsbCapableChip(chipName);
    }

    // True when the family lets the user pick a target IRC frequency.
    //
    // The STC12 generation (10/11/12, IAP10/11/12) has no selectable frequency
    // at all: it runs from whichever clock source is wired to it -- an external
    // crystal, or the internal RC at whatever it happens to be -- and the
    // option byte only records which of the two. STC89/90 is the same story
    // with no internal option: those parts only ever run from a crystal.
    //
    // Setting a frequency for those families would mean writing a trim value
    // for an oscillator the part either does not have or does not use, so the
    // command line rejects -F for them instead of silently ignoring it.
    inline bool HasSelectableFrequency(const std::string& chipName)
    {
        if (chipName.empty()) return true;   // unknown yet: do not restrict
        if (IsStc12Family(chipName)) return false;
        if (IsExternalCrystalFamily(chipName)) return false;
        return true;
    }

    // True when the family lets the user split off part of the code flash as
    // EEPROM.
    //
    // The split is a field in the option block, and only two generations have
    // it:
    //
    //   STC32 / STC8H / 8051U     the option block carries the code/EEPROM
    //                             boundary (opts[39], in 256-byte units)
    //   STC8G / STC8F / 8A / 8C   the same layout, through ProtocolStc8
    //
    // The older generations have no such field. STC15 and STC12 keep their
    // EEPROM as a fixed separate region that the ISP option bytes do not move,
    // and STC89/90 have no EEPROM at all -- on those parts the option byte is
    // a clock/watchdog byte, and writing a "split" into it would corrupt
    // settings rather than reserve space. So --eeprom is rejected for them
    // instead of being silently ignored.
    inline bool HasSelectableEeprom(const std::string& chipName)
    {
        if (chipName.empty()) return true;   // not identified yet: do not restrict
        if (StartsWith(chipName, "STC32") || StartsWith(chipName, "AI32"))
            return true;
        if (StartsWith(chipName, "STC8H") || StartsWith(chipName, "AI8H"))
            return true;
        if (chipName.find("8051U") != std::string::npos)
            return true;
        if (StartsWith(chipName, "STC8G") || StartsWith(chipName, "AI8G") ||
            StartsWith(chipName, "JX8G"))
            return true;
        if (StartsWith(chipName, "STC8F") || StartsWith(chipName, "STC8A") ||
            StartsWith(chipName, "AI8A") || StartsWith(chipName, "STC8C") ||
            StartsWith(chipName, "AI8C") || StartsWith(chipName, "GX8S"))
            return true;
        // STC8A8K / 8A2K are 8H silicon with a different part number.
        if (StartsWith(chipName, "STC8A8K") || StartsWith(chipName, "AI8A8K") ||
            StartsWith(chipName, "STC8A2K") || StartsWith(chipName, "AI8A2K"))
            return true;
        // STC15, STC12 (and the 10/11 and IAP equivalents), STC89/90: no.
        return false;
    }

    // Frequency menu for a family: the table and how many of its entries are
    // selectable.
    //
    // This lives here, ahead of the predicates below, because of the include
    // order: protocol_factory.h pulls in protocol_stc8_hid.h, whose refusal
    // message calls FrequencyMenuFor. Everything this block needs is declared
    // above it, so the HID header sees it; anything added here must keep that
    // property.
    struct FrequencyMenu {
        const uint32_t* hz;   // nullptr when the family has no menu at all
        int count;            // entries in hz
        int maxIdx;           // last selectable index, normally count - 1
    };

    inline bool StartsWith(const std::string& s, const char* prefix);

    // Highest selectable frequency index for a family.
    //
    //   60 MHz      STC32G144K246
    //   45.1584 MHz STC8H and the 8051U
    //   45.000 MHz  STC8A (its last entry; see the table above)
    //   35 MHz      STC8G and STC15
    //
    // STC12 and STC89/90 never reach this function with a meaningful answer;
    // ask HasSelectableFrequency first. The number returned only means anything
    // as an index into the table from FrequencyMenuFor, which is why this is
    // kept as a thin wrapper over it rather than a second set of rules.
    inline FrequencyMenu FrequencyMenuFor(const std::string& chipName)
    {
        if (StartsWith(chipName, "STC8G") || StartsWith(chipName, "AI8G") ||
            StartsWith(chipName, "JX8G"))
            return { kFreqTableStc8G, kNumFreqStc8G, kFreqMaxIdxStc8G };
        if (StartsWith(chipName, "STC15") || StartsWith(chipName, "IAP15") ||
            StartsWith(chipName, "IRC15"))
            return { kFreqTableStc15, kNumFreqStc15, kFreqMaxIdxStc15 };
        if (StartsWith(chipName, "STC32") || StartsWith(chipName, "AI32"))
            return { kFreqTableStc32G144K246, kNumFreqStc32G144K246,
                     kFreqMaxIdxStc32 };
        if (StartsWith(chipName, "STC8H") || StartsWith(chipName, "AI8H"))
            return { kFreqTableStc8H, kNumFreqStc8H, kFreqMaxIdxStc8H };
        if (chipName.find("8051U") != std::string::npos)
            return { kFreqTableStc8051U, kNumFreqStc8051U, kFreqMaxIdx8051U };
        // STC8A is the one family whose last entry is 45.000 rather than
        // 45.1584, so it has to be matched before the fallbacks below.
        //
        // Which parts are 8A is decided by the part number, not by the prefix:
        // the catalogue's 8A parts are STC8AxK y S4A12/S2A12 (the original 8A),
        // STC8A8K y D4 (its later revision) and STC8A2K y D4. They share the
        // 8A menu even though only some spell an 8A8K/8A2K prefix. The 8F/8C/
        // GX8S group is the same oscillator and the same 45.000 ceiling.
        //
        // Deliberately not keying on a substring: "D4" or "S4A12" appearing in
        // an unrelated part number would silently change its ceiling.
        const bool is8A = StartsWith(chipName, "STC8A") ||
                          StartsWith(chipName, "AI8A") ||
                          StartsWith(chipName, "STC8F") ||
                          StartsWith(chipName, "STC8C") ||
                          StartsWith(chipName, "AI8C") ||
                          StartsWith(chipName, "GX8S");
        if (is8A) {
            // The genuine 8H silicon under 8A part numbers -- the 8A8K/8A2K
            // entries described in IsUsbCapableChip -- uses the 8H menu. Those
            // are exactly the D4 revisions listed as 8A8K/8A2K, so the two
            // spellings are told apart here.
            const bool is8HSilicon =
                (StartsWith(chipName, "STC8A8K") || StartsWith(chipName, "AI8A8K") ||
                 StartsWith(chipName, "STC8A2K") || StartsWith(chipName, "AI8A2K")) &&
                chipName.find("S4A12") == std::string::npos &&
                chipName.find("S2A12") == std::string::npos;
            if (is8HSilicon)
                return { kFreqTableStc8H, kNumFreqStc8H, kFreqMaxIdxStc8H };
            return { kFreqTableStc8A, kNumFreqStc8A, kFreqMaxIdxStc8A };
        }
        // Everything else -- including the 8A8K/8A2K relatives, which are 8H
        // silicon -- sits at the 8H ceiling.
        return { kFreqTableStc8H, kNumFreqStc8H, kFreqMaxIdxStc8H };
    }

    inline int FreqMaxIdxFor(const std::string& chipName)
    {
        return FrequencyMenuFor(chipName).maxIdx;
    }

    // True when freq is an entry of the family's menu.
    //
    // This is membership, not a range check: 45.100 is inside the 8H range but
    // is not a menu entry, and the option stage can only write a trim for an
    // entry the vendor calibrated.
    inline bool IsFrequencyInMenu(const std::string& chipName, uint32_t freq)
    {
        FrequencyMenu m = FrequencyMenuFor(chipName);
        for (int i = 0; i <= m.maxIdx; ++i) {
            if (m.hz[i] == freq) return true;
        }
        return false;
    }

    // True when the protocol layer has a trim triple for freq.
    //
    // The trim tables live in protocol_stc8*.h, which this header deliberately
    // does not include, so the set is restated here. It is not a copy of the
    // whole table -- only the frequencies any family menu can produce -- but it
    // has to agree with the menus: every entry of every table above except the
    // sub-MHz divider values must answer true, or the option stage would offer
    // a frequency it then refuses. test_chip_logic.cpp enforces that.
    //
    // 27.000 is the entry this work adds: its trim triple was in the table all
    // along, but the old global frequency table never offered it, so the
    // vendor's 27 MHz calibration could not be selected.
    inline bool HasTrimFor(uint32_t freq)
    {
        switch (freq) {
        case   5529600: case   6000000: case  11059200: case  12000000:
        case  18432000: case  20000000: case  22118400: case  24000000:
        case  27000000:
        case  30000000: case  33177600: case  35000000: case  36864000:
        case  40000000: case  44236800: case  45000000: case  45158400:
        case  48000000: case  49152000: case  55296000: case  60000000:
            return true;
        default:
            return false;
        }
    }

    // Family index shown in the chip-family menu, or -1 when unrecognised.
    inline int ProtoIdxFromChipName(const std::string& chipName)
    {
        if (chipName.empty()) return -1;
        if (StartsWith(chipName, "STC89") || StartsWith(chipName, "STC90"))
            return kProtoIdxStc89;
        if (StartsWith(chipName, "STC10") || StartsWith(chipName, "STC11") ||
            StartsWith(chipName, "STC12") || StartsWith(chipName, "IAP10") ||
            StartsWith(chipName, "IAP11") || StartsWith(chipName, "IAP12"))
            return kProtoIdxStc12;
        if (IsStc15Family(chipName))  return kProtoIdxStc15;
        if (IsStc8gFamily(chipName))  return kProtoIdxStc8G;
        if (StartsWith(chipName, "STC8H") || StartsWith(chipName, "AI8H") ||
            StartsWith(chipName, "STC32") || StartsWith(chipName, "AI32") ||
            StartsWith(chipName, "STC8051U") || StartsWith(chipName, "AI8051U") ||
            StartsWith(chipName, "STC8A8K") || StartsWith(chipName, "AI8A8K") ||
            StartsWith(chipName, "STC8A2K") || StartsWith(chipName, "AI8A2K"))
            return kProtoIdxStc32;
        if (StartsWith(chipName, "STC8F") || StartsWith(chipName, "STC8A") ||
            StartsWith(chipName, "STC8C") || StartsWith(chipName, "AI8C") ||
            StartsWith(chipName, "GX8S"))
            return kProtoIdxStc8;
        return -1;
    }

    // ========================================================================
    //  Protocol selection
    // ========================================================================

    // Build the STC32/STC8G/STC8 implementation for a transport.
    //
    // These three families have a separate class per transport (see
    // protocol_stc8.h), because their status framing and calibration differ.
    // The other families are serial-only and have a single implementation.
    inline std::unique_ptr<stc::IStcProtocol> MakeStc8FamilyProto(const std::string& name,
        bool useHid)
    {
        if (name == "stc8g")
            return useHid ? std::unique_ptr<stc::IStcProtocol>(new stc::ProtocolStc8GHid())
                          : std::unique_ptr<stc::IStcProtocol>(new stc::ProtocolStc8GUart());
        if (name == "stc8")
            return useHid ? std::unique_ptr<stc::IStcProtocol>(new stc::ProtocolStc8Hid())
                          : std::unique_ptr<stc::IStcProtocol>(new stc::ProtocolStc8Uart());
        if (name == "stc32")
            return useHid ? std::unique_ptr<stc::IStcProtocol>(new stc::ProtocolStc32Hid())
                          : std::unique_ptr<stc::IStcProtocol>(new stc::ProtocolStc32Uart());
        return nullptr;
    }

    // Clone the protocol implementation that identified itself during probing.
    inline std::unique_ptr<stc::IStcProtocol> CloneProtocolByName(const std::string& name,
        bool useHid)
    {
        if (auto p = MakeStc8FamilyProto(name, useHid)) return p;

        if (name == "stc89")       return std::make_unique<stc::ProtocolStc89>();
        if (name == "stc89a")      return std::make_unique<stc::ProtocolStc89A>();
        if (name == "stc12")       return std::make_unique<stc::ProtocolStc12>();
        if (name == "stc12a")      return std::make_unique<stc::ProtocolStc12A>();
        if (name == "stc12b")      return std::make_unique<stc::ProtocolStc12B>();
        if (name == "stc15")       return std::make_unique<stc::ProtocolStc15>();
        if (name == "stc15a")      return std::make_unique<stc::ProtocolStc15A>();
        return nullptr;
    }

    // Fallback when the chip name could not be resolved to a specific
    // implementation; depends on which families the transport can reach.
    inline std::unique_ptr<stc::IStcProtocol> MakeProtocol(int idx, bool useHid)
    {
        if (useHid) return std::unique_ptr<stc::IStcProtocol>(new stc::ProtocolStc32Hid());

        switch (idx) {
        case kProtoIdxStc89: return std::make_unique<stc::ProtocolStc89A>();
        case kProtoIdxStc12: return std::make_unique<stc::ProtocolStc12>();
        case kProtoIdxStc15: return std::make_unique<stc::ProtocolStc15>();
        case kProtoIdxStc8G: return std::unique_ptr<stc::IStcProtocol>(new stc::ProtocolStc8GUart());
        case kProtoIdxStc32: return std::unique_ptr<stc::IStcProtocol>(new stc::ProtocolStc32Uart());
        case kProtoIdxStc8:  return std::unique_ptr<stc::IStcProtocol>(new stc::ProtocolStc8Uart());
        default:             return std::unique_ptr<stc::IStcProtocol>(new stc::ProtocolStc32Uart());
        }
    }

    // Probe list used by the handshake step.
    //
    // Over serial every family is reachable: the classic parts answer to their
    // own init packets and STC8G/STC8 derive from the STC32 protocol, so all
    // ten variants are probed.
    //
    // Over HID only the parts with a USB interface are ever present in the
    // device list, and they are served by three implementations: ProtocolStc32
    // (STC32 / STC8H / 8051U), ProtocolStc8G (8G parts) and ProtocolStc8 (the
    // 08A variant of 8G). The remaining families cannot appear, so they are not
    // probed.
    inline std::vector<std::unique_ptr<stc::IStcProtocol>> MakeProbeList(bool useHid)
    {
        std::vector<std::unique_ptr<stc::IStcProtocol>> v;
        if (useHid) {
            // The Hid-suffixed classes know the status packet carries no 0x50
            // prefix over USB.
            v.push_back(std::unique_ptr<stc::IStcProtocol>(new stc::ProtocolStc32Hid()));
            v.push_back(std::unique_ptr<stc::IStcProtocol>(new stc::ProtocolStc8GHid()));
            v.push_back(std::unique_ptr<stc::IStcProtocol>(new stc::ProtocolStc8Hid()));
        }
        else {
            v.push_back(std::make_unique<stc::ProtocolStc89>());
            v.push_back(std::make_unique<stc::ProtocolStc89A>());
            v.push_back(std::make_unique<stc::ProtocolStc12>());
            v.push_back(std::make_unique<stc::ProtocolStc12A>());
            v.push_back(std::make_unique<stc::ProtocolStc12B>());
            v.push_back(std::make_unique<stc::ProtocolStc15A>());
            v.push_back(std::make_unique<stc::ProtocolStc15>());
            v.push_back(std::unique_ptr<stc::IStcProtocol>(new stc::ProtocolStc8GUart()));
            v.push_back(std::unique_ptr<stc::IStcProtocol>(new stc::ProtocolStc32Uart()));
            v.push_back(std::unique_ptr<stc::IStcProtocol>(new stc::ProtocolStc8Uart()));
        }
        return v;
    }

    // The 0x05 "prepare programming" step does not exist on the older
    // serial-only families.
    inline bool NeedsPrepare05(const std::string& protoName)
    {
        if (protoName == "stc89")  return false;
        if (protoName == "stc89a") return false;
        if (protoName == "stc12")  return false;
        if (protoName == "stc12a") return false;
        if (protoName == "stc12b") return false;
        return true;
    }

    // Legacy families whose BRT register cannot reach beyond 115200.
    inline bool IsLegacyBaudProto(const std::string& protoName)
    {
        return protoName == "stc89" || protoName == "stc89a" ||
            protoName == "stc12" || protoName == "stc12a" ||
            protoName == "stc12b";
    }

    // ========================================================================
    //  Flash sizing
    // ========================================================================

    inline uint32_t GetChipTotalFlash(const std::string& chipName, uint16_t magic)
    {
        if (chipName.find("12K128") != std::string::npos) return 131072;
        if (chipName.find("8K64") != std::string::npos) return 65536;
        if (chipName.find("8K60") != std::string::npos) return 65536;
        if (chipName.find("8K48") != std::string::npos) return 65536;
        if (chipName.find("8K32") != std::string::npos) return 32768;
        if (chipName.find("8K16") != std::string::npos) return 16384;
        if (chipName.find("4K64") != std::string::npos) return 65536;
        if (chipName.find("4K60") != std::string::npos) return 65536;
        if (chipName.find("4K48") != std::string::npos) return 65536;
        if (chipName.find("4K32") != std::string::npos) return 32768;
        if (chipName.find("4K16") != std::string::npos) return 16384;
        if (chipName.find("2K64") != std::string::npos) return 65536;
        if (chipName.find("2K60") != std::string::npos) return 65536;
        if (chipName.find("2K48") != std::string::npos) return 65536;
        if (chipName.find("2K32") != std::string::npos) return 32768;
        if (chipName.find("2K16") != std::string::npos) return 16384;
        if (chipName.find("1K33") != std::string::npos) return 33792;
        if (chipName.find("1K28") != std::string::npos) return 28672;
        if (chipName.find("1K17") != std::string::npos) return 17408;
        // 1K16 is the one entry the original table was missing. The part number
        // scheme is <series>K<size in KB>, so a 1K16 has 16 KB -- the same
        // silicon as an 8K16 with a different package. Without this line the
        // lookup falls through to the ID-based guess and reports 64 KB, which
        // would make the EEPROM split arithmetic wrong.
        if (chipName.find("1K16") != std::string::npos) return 16384;
        if (chipName.find("1K12") != std::string::npos) return 12288;
        if (chipName.find("1K08") != std::string::npos) return 12288;
        if (chipName.find("1K06") != std::string::npos) return 12288;
        if (chipName.find("1K04") != std::string::npos) return 12288;
        if (chipName.find("1K02") != std::string::npos) return 12288;

        uint8_t hi = (uint8_t)(magic >> 8);
        if (hi >= 0xfa) return 128 * 1024;
        return 64 * 1024;
    }

    // ========================================================================
    //  Device-list formatting
    // ========================================================================

    // The combo box stores "<friendly name> (<id>)" for both transports, where
    // <id> is a COM name for serial and a device interface path for HID. Pull
    // it back out; fall back to the raw text when no parentheses are present.
    inline std::string ExtractDeviceId(const std::string& display)
    {
        size_t lp = display.rfind('('), rp = display.rfind(')');
        if (lp != std::string::npos && rp != std::string::npos && rp > lp + 1)
            return display.substr(lp + 1, rp - lp - 1);
        return display;
    }

    inline std::string FormatHidDevice(const stcisp::HidDeviceInfo& d)
    {
        std::string label = d.displayName;
        if (label.empty()) label = "STC USB ISP";
        char ids[32];
        std::snprintf(ids, sizeof(ids), "%04X:%04X", d.vid, d.pid);
        return label + " [" + std::string(ids) + "] (" + d.devicePath + ")";
    }

} // namespace newisp
