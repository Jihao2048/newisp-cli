// Intel HEX parser tests.

#include "test.h"

#include "newisp/hex_file.h"

#include <string>
#include <cstdio>

using stc::HexImage;
using stc::ParseHexFile;

namespace {

    void TestSimpleImage()
    {
        // Four bytes at address 0x0100, then EOF.
        const std::string hex =
            ":0401000001020304F1\n"
            ":00000001FF\n";

        HexImage img;
        CHECK(ParseHexFile(hex, img));
        CHECK_EQ(img.minAddr, 0x0100u);
        CHECK_EQ(img.maxAddr, 0x0103u);
        CHECK_EQ(img.data.size(), (size_t)4);
        CHECK_EQ((int)img.data[0x0100], 1);
        CHECK_EQ((int)img.data[0x0101], 2);
        CHECK_EQ((int)img.data[0x0102], 3);
        CHECK_EQ((int)img.data[0x0103], 4);

        // Gaps are not data. Only a validated linear programming image pads.
        CHECK(img.data.find(0) == img.data.end());
        std::vector<uint8_t> firmware;
        std::string error;
        CHECK(stc::BuildLinearFirmware(img, firmware, error));
        CHECK_EQ(firmware.size(), (size_t)0x0104);
        CHECK_EQ((int)firmware[0], 0xFF);
        CHECK_EQ((int)firmware[0x0100], 1);
    }

    void TestExtendedLinearAddress()
    {
        // Type 04 sets the upper 16 bits: base = 0x0001 << 16 = 0x00010000.
        const std::string hex =
            ":020000040001F9\n"
            ":04000000AABBCCDDEE\n"
            ":00000001FF\n";

        HexImage img;
        CHECK(ParseHexFile(hex, img));
        CHECK_EQ(img.minAddr, 0x00010000u);
        CHECK_EQ(img.maxAddr, 0x00010003u);
        CHECK_EQ((int)img.data[0x00010000], 0xAA);
        CHECK_EQ((int)img.data[0x00010003], 0xDD);
    }

    void TestExtendedSegmentAddress()
    {
        // Type 02 sets the upper bits shifted left by 4: base = 0x1000 << 4.
        const std::string hex =
            ":020000021000EC\n"
            ":020000001234B8\n"
            ":00000001FF\n";

        HexImage img;
        CHECK(ParseHexFile(hex, img));
        CHECK_EQ(img.minAddr, 0x00010000u);
        CHECK_EQ((int)img.data[0x00010000], 0x12);
        CHECK_EQ((int)img.data[0x00010001], 0x34);
    }

    void TestCrlfAndBlankLines()
    {
        // Files produced on Windows have CRLF endings, and tools often leave a
        // trailing newline; neither may upset the parser.
        const std::string hex =
            "\r\n"
            ":0401000001020304F1\r\n"
            "\r\n"
            ":00000001FF\r\n";

        HexImage img;
        CHECK(ParseHexFile(hex, img));
        CHECK_EQ(img.minAddr, 0x0100u);
        CHECK_EQ(img.maxAddr, 0x0103u);
    }

    void TestLowercaseHexDigits()
    {
        // Some assemblers emit lowercase digits; the Intel HEX spec allows it.
        const std::string hex =
            ":04010000aabbccdded\n"
            ":00000001ff\n";

        HexImage img;
        CHECK(ParseHexFile(hex, img));
        CHECK_EQ((int)img.data[0x0100], 0xAA);
        CHECK_EQ((int)img.data[0x0103], 0xDD);
    }

    void TestEmptyInputIsRejected()
    {
        HexImage img;
        CHECK(!ParseHexFile("", img));
        CHECK(!ParseHexFile("\n\n\n", img));
    }

    void TestUnsafeImagesAreRejected()
    {
        HexImage img;
        CHECK(!ParseHexFile(":0401000001020304F3\n:00000001FF\n", img));
        CHECK(!ParseHexFile(":040100000102\n:00000001FF\n", img));
        CHECK(!ParseHexFile(":0401000001020304F1\n", img));
        CHECK(!ParseHexFile(":0401000001020304F1\n"
                            ":0401000001020304F1\n:00000001FF\n", img));
        CHECK(!ParseHexFile(":0401000001020304F1\n:00000001FF\n"
                            ":0401000001020304F1\n", img));
        CHECK(ParseHexFile(":0401000001020304F1\n:00000001FF\n", img));
        CHECK(!ParseHexFile("", img));
        CHECK(img.data.empty());
    }

    void TestGarbageLinesAreRejected()
    {
        // Invalid input must not silently become a partially valid image.
        const std::string hex =
            "not a record\n"
            ":04\n"
            ":0401000001020304F1\n"
            ":00000001FF\n";

        HexImage img;
        CHECK(!ParseHexFile(hex, img));
        CHECK(img.data.empty());
    }

    void TestEofRejectsTrailingContent()
    {
        // A hidden second image after EOF must not be silently ignored.
        const std::string hex =
            ":0401000001020304F1\n"
            ":00000001FF\n"
            ":04020000FFFFFFFFFE\n";

        HexImage img;
        CHECK(!ParseHexFile(hex, img));
        CHECK(img.data.empty());
    }

    std::string Record(unsigned type, unsigned addr, const std::vector<uint8_t>& data)
    {
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), ":%02X%04X%02X", (unsigned)data.size(), addr, type);
        std::string line = buffer;
        unsigned sum = (unsigned)data.size() + (addr >> 8) + (addr & 255) + type;
        for (auto b : data) {
            std::snprintf(buffer, sizeof(buffer), "%02X", b);
            line += buffer;
            sum += b;
        }
        std::snprintf(buffer, sizeof(buffer), "%02X\n", (0u - sum) & 255);
        return line + buffer;
    }

    void TestSparseBanksAndPreflight()
    {
        HexImage img;
        std::string error;
        const auto eof = Record(1, 0, {});
        // Same low address, distinct MCS-251 banks. No aliasing or implicit rebasing.
        CHECK(ParseHexFile(Record(4, 0, {0, 0xFE}) + Record(0, 0, {1, 2}) +
            Record(4, 0, {0, 0xFF}) + Record(0, 0, {3, 4}) + eof, img, &error));
        CHECK_EQ(img.data.size(), (size_t)4);
        CHECK_EQ(img.data.at(0xFE0000), 1);
        CHECK_EQ(img.data.at(0xFF0000), 3);
        std::vector<uint8_t> firmware{42};
        CHECK(!stc::BuildLinearFirmware(img, firmware, error));
        CHECK(firmware.empty());
        // The refusal is about the 16-bit block address, which is the same for
        // every family, so the message names that limit rather than a family.
        CHECK(error.find("0xFFFF") != std::string::npos);
        CHECK(ParseHexFile(Record(4, 0, {0xFF, 0xFF}) + Record(0, 0xFFFF, {7}) + eof, img));
        CHECK_EQ(img.data.size(), (size_t)1);
        CHECK_EQ(img.maxAddr, 0xFFFFFFFFu);
        CHECK(!stc::BuildLinearFirmware(img, firmware, error));
        CHECK(!ParseHexFile(Record(4, 0, {0xFF, 0xFF}) + Record(0, 0xFFFF, {7, 8}) + eof, img, &error));
        CHECK(error.find("overflow") != std::string::npos);
        CHECK(ParseHexFile(Record(0, 0xFFFF, {7}) + eof, img));
        CHECK(stc::BuildLinearFirmware(img, firmware, error));
        CHECK_EQ(firmware.size(), (size_t)65536);
        CHECK_EQ(firmware.back(), 7);
        // A reused parser cannot leak earlier bytes or an earlier entry point.
        CHECK(ParseHexFile(Record(5, 0, {0, 0xFF, 0, 0}) + Record(0, 2, {9}) + eof, img));
        CHECK_EQ(img.entryValue, 0xFF0000u);
        CHECK(ParseHexFile(Record(0, 0, {1}) + eof, img));
        CHECK_EQ(img.data.size(), (size_t)1);
        CHECK_EQ(img.entryType, 0);
    }

    void TestRecordShapesAndDiagnostics()
    {
        const auto data = Record(0, 0, {1});
        const auto eof = Record(1, 0, {});
        const std::vector<std::string> invalid = {
            Record(4, 0, {0}), Record(4, 1, {0, 1}), Record(2, 0, {0}),
            Record(2, 1, {0, 1}), Record(1, 0, {0}), Record(1, 1, {}),
            Record(5, 1, {0, 0, 0, 0}), Record(3, 0, {0, 0}),
            Record(6, 0, {}), ":01000000GGFF\n",
            Record(5, 0, {0, 0, 0, 0}) + Record(3, 0, {0, 0, 0, 0}),
            Record(0, 0xFFFF, {1, 2})
        };
        for (const auto& bad : invalid) {
            HexImage img;
            std::string error;
            CHECK(!ParseHexFile(data + bad + eof, img, &error));
            CHECK(img.data.empty());
            CHECK(error.find("HEX line ") == 0);
        }
        HexImage img;
        std::string error;
        CHECK(ParseHexFile(Record(3, 0, {0x12, 0x34, 0x56, 0x78}) + data + eof, img));
        CHECK_EQ(img.entryType, 3);
        CHECK_EQ(img.entryValue, 0x12345678u);
        CHECK(!ParseHexFile(std::string(stc::kMaxHexTextBytes + 1, ' '), img, &error));
        CHECK(error.find("16 MiB") != std::string::npos);
        // Resource limit applies to actual data, not the highest address.
        std::string large;
        for (unsigned bank = 0; bank <= 16; ++bank) {
            large += Record(4, 0, {0, (uint8_t)bank});
            for (unsigned addr = 0; addr < 65536; addr += 128)
                large += Record(0, addr, std::vector<uint8_t>(128, 0xA5));
        }
        CHECK(!ParseHexFile(large + eof, img, &error));
        CHECK(error.find("1 MiB") != std::string::npos);
        CHECK(img.data.empty());
    }

} // namespace

void RunHexFileTests()
{
    TestSimpleImage();
    TestExtendedLinearAddress();
    TestExtendedSegmentAddress();
    TestCrlfAndBlankLines();
    TestLowercaseHexDigits();
    TestEmptyInputIsRejected();
    TestUnsafeImagesAreRejected();
    TestGarbageLinesAreRejected();
    TestEofRejectsTrailingContent();
    TestSparseBanksAndPreflight();
    TestRecordShapesAndDiagnostics();
}
