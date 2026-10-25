#include "newisp/hex_file.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <limits>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace stc {

    bool ParseHexFile(const std::string& text, HexImage& out, std::string* error)
    {
        out = HexImage{};
        if (error) error->clear();
        size_t lineNumber = 0;
        auto fail = [&](const std::string& reason) {
            if (error) *error = "HEX line " + std::to_string(lineNumber) + ": " + reason;
            return false;
        };
        if (text.size() > kMaxHexTextBytes) return fail("input exceeds 16 MiB limit");
        HexImage parsed;
        bool eof = false;
        uint32_t base = 0;
        auto nib = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
            };
        auto hb = [&](const std::string& s, size_t o) -> int {
            if (o + 1 >= s.size()) return -1;
            int hi = nib(s[o]), lo = nib(s[o + 1]);
            return (hi < 0 || lo < 0) ? -1 : ((hi << 4) | lo);
            };

        size_t start = 0;
        while (start < text.size()) {
            size_t end = text.find('\n', start);
            if (end == std::string::npos) end = text.size();
            std::string line = text.substr(start, end - start);
            start = end + 1;
            ++lineNumber;
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            if (line.empty()) continue;
            if (eof) return fail("content after EOF");
            if (line[0] != ':') return fail("expected ':' record marker");

            int bc = hb(line, 1), ah = hb(line, 3), al = hb(line, 5), rt = hb(line, 7);
            if (bc < 0 || ah < 0 || al < 0 || rt < 0) return fail("invalid record header");
            if (line.size() != 11u + 2u * static_cast<size_t>(bc))
                return fail("record length does not match byte count");
            unsigned checksum = 0;
            for (size_t pos = 1; pos < line.size(); pos += 2) {
                int b = hb(line, pos);
                if (b < 0) return fail("invalid hexadecimal digit");
                checksum += static_cast<unsigned>(b);
            }
            if ((checksum & 0xFFu) != 0) return fail("checksum mismatch");
            // Both halves were just bounds-checked to 0..255 by hb(), so the
            // cast to uint16_t cannot truncate; be explicit about it.
            uint16_t addr = (uint16_t)(((uint16_t)ah << 8) | (uint16_t)al);

            if (rt == 0x04 || rt == 0x02) {
                if (bc != 2 || addr != 0) return fail("invalid extended-address record");
                int hi = hb(line, 9), lo = hb(line, 11);
                base = static_cast<uint32_t>((hi << 8) | lo) << (rt == 4 ? 16 : 4);
                continue;
            }
            if (rt == 0x01) {
                if (bc != 0 || addr != 0) return fail("invalid EOF record");
                eof = true;
                continue;
            }
            if (rt == 0x03 || rt == 0x05) {
                if (bc != 4 || addr != 0 || parsed.entryType != 0)
                    return fail("invalid or duplicate entry-point record");
                parsed.entryType = static_cast<uint8_t>(rt);
                for (int i = 0; i < 4; ++i)
                    parsed.entryValue = (parsed.entryValue << 8) | static_cast<uint32_t>(hb(line, 9 + 2 * i));
                continue;
            }
            if (rt != 0x00) return fail("unsupported record type");
            if (bc == 0) continue;

            uint64_t abs = static_cast<uint64_t>(base) + addr;
            if (abs + static_cast<uint64_t>(bc) - 1 > std::numeric_limits<uint32_t>::max())
                return fail("32-bit address overflow");
            if (static_cast<unsigned>(addr) + bc > 0x10000u)
                return fail("data record crosses a 64 KiB boundary; split the record");
            if (parsed.data.size() + bc > kMaxHexDataBytes)
                return fail("data exceeds 1 MiB limit");
            for (int i = 0; i < bc; ++i) {
                int b = hb(line, 9 + i * 2);
                uint32_t a = static_cast<uint32_t>(abs + i);
                if (!parsed.data.emplace(a, static_cast<uint8_t>(b)).second)
                    return fail("overlapping data records");
                if (a < parsed.minAddr) parsed.minAddr = a;
                if (a > parsed.maxAddr) parsed.maxAddr = a;
            }
        }
        if (!eof) return fail("missing EOF record");
        if (parsed.data.empty()) return fail("no data records");
        out = std::move(parsed);
        return true;
    }

    bool BuildLinearFirmware(const HexImage& image, std::vector<uint8_t>& out,
        std::string& error)
    {
        out.clear();
        error.clear();
        if (image.data.empty()) {
            error = "empty firmware image";
            return false;
        }
        // Use the actual keys, never cached min/max metadata as an authority.
        uint32_t last = image.data.rbegin()->first;
        if (last > 0xFFFFu) {
            error = "extended/banked firmware is not supported by the 16-bit writer; "
                    "no device opened, no erase issued (use inspect -f FILE)";
            return false;
        }
        out.assign(static_cast<size_t>(last) + 1, 0xFF);
        for (const auto& byte : image.data) out[byte.first] = byte.second;
        return true;
    }

    bool LoadHexFile(const std::string& path, HexImage& out, std::string& error)
    {
        out = HexImage{};
        error.clear();
        std::string text;

#if defined(_WIN32)
        // std::ifstream takes a narrow path and interprets it in the system
        // ANSI code page, so a path containing non-ASCII characters would be
        // looked up wrongly. The same path arrives here as UTF-8, so convert it
        // to UTF-16 and use the wide stream, which is unambiguous.
        int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(),
            (int)path.size(), nullptr, 0);
        if (wlen <= 0) {
            error = "cannot convert path to UTF-16: " + path;
            return false;
        }
        std::wstring wpath((size_t)wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), (int)path.size(),
            &wpath[0], wlen);

        std::ifstream in(wpath.c_str(), std::ios::binary);
        if (!in) {
            error = "cannot open file: " + path;
            return false;
        }
#else
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            error = "cannot open file: " + path;
            return false;
        }
#endif
        in.seekg(0, std::ios::end);
        const auto length = in.tellg();
        if (length < 0 || static_cast<uint64_t>(length) > kMaxHexTextBytes) {
            error = "cannot determine file size or HEX input exceeds 16 MiB: " + path;
            return false;
        }
        in.seekg(0);
        text.resize(static_cast<size_t>(length));
        if (length > 0 && !in.read(&text[0], static_cast<std::streamsize>(length))) {
            error = "cannot read complete HEX file: " + path;
            return false;
        }
        return ParseHexFile(text, out, &error);
    }

} // namespace stc