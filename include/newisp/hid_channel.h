#pragma once
// USB HID transport for the STC factory ISP interface.
//
// The WinUI build talked to the device through SetupAPI + hid.dll. This port
// uses hidapi instead, which wraps the same Windows HID class, IOKit on macOS
// and hidraw/libusb on Linux, so one implementation covers all three.
//
// The protocol behaviour is unchanged: the ISP interface presents a 64-byte
// report (report ID 0), short payloads are zero padded and long payloads are
// split across reports. Unlike the serial link there is no line coding and no
// reset line, so baud rate / parity / DTR / RTS are accepted and ignored.

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

#include "newisp/IChannel.h"

// USB HID support is no longer optional: the STC factory ISP interface is the
// only transport for the 8051U family, so a build that silently omitted it
// would be a build that cannot program part of the catalogue. Windows uses the
// system HID class driver (hid.dll + SetupAPI, no external dependency); macOS
// and Linux use hidapi, which is a hard requirement of the build there.
//
// NEWISP_HAVE_HID is kept as 1 so code written against the old optional flag
// still compiles, but nothing in the project switches it off any more.
#ifndef NEWISP_HAVE_HID
#define NEWISP_HAVE_HID 1
#endif

#if !NEWISP_HAVE_HID
#error "USB HID support is required: the 8051U family can only be programmed over HID"
#endif

namespace stcisp {

    // STC factory USB ISP interface HID identifiers.
    constexpr uint16_t STC_HID_VID = 0x34BF;
    constexpr uint16_t STC_HID_PID = 0x1001;

    // Fixed report size of the ISP interface, in bytes.
    constexpr size_t STC_HID_REPORT_SIZE = 64;

    // Descriptor of a discovered HID ISP device.
    struct HidDeviceInfo {
        std::string devicePath;   // platform device path
        std::string displayName;  // product/manufacturer string, may be empty
        uint16_t    vid = 0;
        uint16_t    pid = 0;
    };

    // True when this build includes HID support. Always true now; the function
    // stays because callers use it to report capability rather than to branch
    // on a configuration that no longer exists.
    bool HidSupported();

    // Enumerate all HID devices matching vendor/product (0 = wildcard).
    std::vector<HidDeviceInfo> EnumerateHidDevices(uint16_t vid, uint16_t pid);

    class HidChannel : public IChannel {
    public:
        HidChannel();
        ~HidChannel() override;

        HidChannel(const HidChannel&) = delete;
        HidChannel& operator=(const HidChannel&) = delete;

        // portName is a HID device path from EnumerateHidDevices. baudRate and
        // evenParity are accepted for interface compatibility and ignored.
        bool Open(const std::string& portName, uint32_t baudRate = 0,
                  bool evenParity = true) override;
        void Close() override;
        bool IsOpen() const override;

        bool Reopen(uint32_t baudRate, bool evenParity) override;

        // No-ops on HID; retained so the protocol layer stays transport
        // agnostic.
        bool SetBaudRate(uint32_t baudRate) override;
        bool SetParity(bool evenParity) override;

        // No hardware reset line on the HID ISP interface.
        bool SetDtr(bool asserted) override;
        bool SetRts(bool asserted) override;
        bool PulseReset(int pin, uint32_t holdMs = 250,
                        uint32_t releaseMs = 30) override;

        // Write one or more reports. Short payloads are zero padded; long
        // payloads are split across several reports.
        bool Write(const std::vector<uint8_t>& data) override;

        // Read up to maxBytes of report payload, waiting up to timeoutMs.
        std::vector<uint8_t> Read(size_t maxBytes, uint32_t timeoutMs) override;

        // Drain any pending input reports.
        void Flush() override;

        ChannelKind Kind() const override { return ChannelKind::Hid; }
        bool EvenParity() const override { return true; }
        uint32_t CurrentBaud() const override { return m_currentBaud; }
        const std::string& PortName() const override { return m_portName; }

        // Report size in bytes, including the leading report-ID byte.
        size_t ReportSize() const { return m_reportSize; }

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
        uint32_t    m_currentBaud = 0;
        std::string m_portName;
        size_t      m_reportSize = STC_HID_REPORT_SIZE;
        size_t      m_inputReportSize = STC_HID_REPORT_SIZE;
    };

} // namespace stcisp
