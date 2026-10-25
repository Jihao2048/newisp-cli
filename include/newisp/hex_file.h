#pragma once
// Strict, address-preserving Intel HEX input. Parsing is not flash mapping.

#include <string>
#include <vector>
#include <cstdint>
#include <map>

namespace stc {

    struct HexImage {
        std::map<uint32_t, uint8_t> data;
        uint32_t minAddr = 0xFFFFFFFF;
        uint32_t maxAddr = 0;
        // Type 03/05 entry records are metadata, not bytes to program.
        uint8_t entryType = 0;
        uint32_t entryValue = 0;
    };

    inline constexpr size_t kMaxHexTextBytes = 16 * 1024 * 1024;
    inline constexpr size_t kMaxHexDataBytes = 1024 * 1024;

    // Checks every record and its checksum; requires one EOF and rejects
    // overlaps, unknown records, trailing content and address overflow.
    // Blank lines and CRLF are allowed. Failure always clears out.
    bool ParseHexFile(const std::string& text, HexImage& out,
        std::string* error = nullptr);

    // Only the existing 16-bit linear programming path is implemented.
    // Never rebase an extended-address image or silently truncate a bank.
    // Missing bytes below the final address are erased-flash padding (0xFF).
    bool BuildLinearFirmware(const HexImage& image, std::vector<uint8_t>& out,
        std::string& error);

    // Read a file from disk and parse it. Returns false if the file cannot be
    // read or contains no data records.
    bool LoadHexFile(const std::string& path, HexImage& out, std::string& error);

} // namespace stc
