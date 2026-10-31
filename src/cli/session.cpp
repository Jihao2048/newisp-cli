// Session layer implementation.
//
// The sequences here are ported from the WinUI build's OpenSession,
// DetectWorker and BurnWorker. The wire protocol is untouched; what changed is
// that UI calls became callbacks, and the process-global stop flag became an
// atomic reference so the caller keeps ownership.

#include "newisp/session.h"

#include "newisp/chip_table.h"
#include "newisp/hex_file.h"
#include "newisp/hid_channel.h"
#include "newisp/serial_channel.h"
#include "newisp/text.h"

#include <cstdio>

using namespace stc;

namespace newisp {

    // Log text goes through the message table; Tr fills the {0}, {1}, ...
    // slots at the call site. See include/newisp/text_en.inc.
    using i18n::Msg;
    using i18n::Tr;

    namespace {

        // ---------------------------------------------------------------
        //  Probing
        // ---------------------------------------------------------------

        // Try every protocol's packet parser against the receive buffer.
        //
        // A reply is only accepted when the framing parses *and* the status
        // decoder recognises it: the packet header alone is far too weak a
        // signal, since several families share 0x46 0xB9 framing and the
        // checksum is a simple sum.
        bool TryParseAll(std::vector<std::unique_ptr<IStcProtocol>>& protoList,
            std::vector<uint8_t>& rxBuf,
            SessionResult& res,
            const LogFn& log,
            const char* tag)
        {
            if (rxBuf.empty()) return false;

            for (auto& proto : protoList) {
                std::vector<uint8_t> pl;
                size_t consumed = 0;
                if (!proto->ProbeParse(rxBuf, pl, consumed)) continue;

                McuStatus trySt{};
                if (!proto->ParseStatus(pl, trySt)) continue;

                log(LogLine(Msg::LogRxPayload, { tag, std::to_string(pl.size()),
                    HexDump(pl) }, i18n::LogKind::Packet));

                res.got = true;
                res.st = trySt;
                res.protoName = proto->Params().name;
                rxBuf.erase(rxBuf.begin(),
                    rxBuf.begin() + (ptrdiff_t)consumed);
                log(Tr(Msg::LogIdentifiedAs, { tag, res.protoName }));
                return true;
            }

            // Bound the buffer: a chatty or miswired line must not grow this
            // without limit across 500 handshake rounds.
            if (rxBuf.size() > 512) {
                rxBuf.erase(rxBuf.begin(),
                    rxBuf.begin() + (ptrdiff_t)(rxBuf.size() - 256));
            }
            return false;
        }

    } // namespace

    // =======================================================================
    //  Bring-up
    // =======================================================================

    SessionResult ConnectSerial(stcisp::IChannel& ch,
        const std::string& deviceId,
        std::vector<uint8_t>& rxBuf,
        std::vector<std::unique_ptr<IStcProtocol>>& protoList,
        const LogFn& log,
        std::atomic<bool>& stopReq,
        const char* tag)
    {
        SessionResult res;
        const int MAX_ROUNDS = 500;
        const int PULSE_PER_ROUND = 200;
        const int RETRY_WAIT_MS = 500;

        if (protoList.empty()) return res;
        bool resetDone = false;

        for (int round = 1; round <= MAX_ROUNDS && !res.got; ++round) {
            if (stopReq.load()) break;

            if (!ch.Open(deviceId, 2400, protoList[0]->UseEvenParity())) {
                log(Tr(Msg::LogSerialOpenFailed, tag));
                res.openFailed = true;
                return res;
            }

            if (!resetDone) {
                resetDone = true;
                log(Tr(Msg::LogResetPulse, tag));
                ch.PulseReset(0, 250, 30);
            }

            rxBuf.clear();
            ch.Flush();
            log(Tr(Msg::LogHandshakeRound, { tag, std::to_string(round) }));

            std::vector<uint8_t> wake = { 0x7F };

            for (int p = 0; p < PULSE_PER_ROUND && !res.got; ++p) {
                if (stopReq.load()) break;

                ch.Write(wake);

                bool gotAny = false;
                {
                    uint64_t t0 = NowMs();
                    while (NowMs() - t0 < 30) {
                        if (stopReq.load()) break;
                        auto c = ch.Read(1024, 0);
                        if (!c.empty()) {
                            rxBuf.insert(rxBuf.end(), c.begin(), c.end());
                            gotAny = true;
                            break;
                        }
                        SleepMs(2);
                    }
                }

                if (gotAny) {
                    // Something answered. Keep reading until the line goes
                    // quiet: a status packet can arrive in several USB chunks,
                    // and parsing mid-packet would reject it.
                    uint64_t lastDataMs = NowMs();
                    uint64_t startMs = NowMs();
                    const uint64_t MAX_MS = 400;
                    const uint64_t QUIET_MS = 60;

                    while ((NowMs() - startMs) < MAX_MS) {
                        if (stopReq.load()) break;

                        if (TryParseAll(protoList, rxBuf, res, log, tag)) break;
                        if (res.got) break;

                        auto c2 = ch.Read(1024, 0);
                        if (!c2.empty()) {
                            rxBuf.insert(rxBuf.end(), c2.begin(), c2.end());
                            lastDataMs = NowMs();
                        }
                        else {
                            if ((NowMs() - lastDataMs) >= QUIET_MS) break;
                            SleepMs(3);
                        }
                    }
                }

                if (res.got) break;

                if ((p + 1) % 20 == 0) {
                    log(Tr(Msg::LogPulseProgress, { tag,
                        std::to_string(round), std::to_string(p + 1),
                        std::to_string(PULSE_PER_ROUND) }));
                }
            }

            if (res.got) break;
            ch.Close();
            log(Tr(Msg::LogRoundNoResponse, { tag, std::to_string(round) }));
            for (int ms = 0; ms < RETRY_WAIT_MS; ms += 50) {
                if (stopReq.load()) break;
                SleepMs(50);
            }
        }

        return res;
    }

    SessionResult ConnectHid(stcisp::IChannel& ch,
        const std::string& deviceId,
        std::vector<uint8_t>& rxBuf,
        std::vector<std::unique_ptr<IStcProtocol>>& protoList,
        const LogFn& log,
        std::atomic<bool>& stopReq,
        const char* tag)
    {
        SessionResult res;

        if (protoList.empty()) return res;

        if (!ch.Open(deviceId, 0, protoList[0]->UseEvenParity())) {
            log(Tr(Msg::LogHidOpenFailed, tag));
            res.openFailed = true;
            return res;
        }

        rxBuf.clear();
        ch.Flush();

        // Probe every protocol with its own init payload; the ISP monitor
        // answers only for the family it belongs to.
        for (auto& proto : protoList) {
            if (stopReq.load() || res.got) break;

            std::vector<uint8_t> init = proto->ProbeBuild({ 0x00 });
            if (!ch.Write(init)) {
                // A failed write is silent otherwise, and it is the one thing
                // that makes the whole probe look like an absent chip. Say so.
                log(Tr(Msg::LogProbeWriteFailed, { tag, proto->Params().name }));
                continue;
            }

            uint64_t t0 = NowMs();
            while (!res.got && (NowMs() - t0) < 1000) {
                if (stopReq.load()) break;
                auto c = ch.Read(512, 50);
                if (!c.empty()) rxBuf.insert(rxBuf.end(), c.begin(), c.end());
                if (TryParseAll(protoList, rxBuf, res, log, tag)) break;
            }
            if (!res.got) {
                log(Tr(Msg::LogNoAnswer, { tag, proto->Params().name,
                    std::to_string(rxBuf.size()) }));
            }
            if (res.got) break;
        }

        if (!res.got) ch.Close();
        return res;
    }

    SessionResult OpenSession(stcisp::IChannel& ch,
        const std::string& deviceId,
        std::vector<uint8_t>& rxBuf,
        std::vector<std::unique_ptr<IStcProtocol>>& protoList,
        const LogFn& log,
        std::atomic<bool>& stopReq,
        const char* tag)
    {
        if (ch.Kind() == stcisp::ChannelKind::Hid)
            return ConnectHid(ch, deviceId, rxBuf, protoList, log, stopReq, tag);
        return ConnectSerial(ch, deviceId, rxBuf, protoList, log, stopReq, tag);
    }

    // =======================================================================
    //  Reporting
    // =======================================================================

    void PrintMcuInfo(const McuStatus& st, const std::string& protoName,
        const LogFn& log)
    {
        log(Tr(Msg::Separator));
        log(Tr(Msg::ChipModel, ChipNameFromTable(st.magic)));
        log(Tr(Msg::Magic, Hex(st.magic, 4)));
        if (!protoName.empty())
            log(Tr(Msg::ProtocolName, protoName));

        // BSL stepping byte is an ASCII letter on every family that reports it.
        std::string stepping;
        if (st.bslStepping >= 0x20 && st.bslStepping < 0x7F)
            stepping = std::string(1, (char)st.bslStepping);
        log(Tr(Msg::BslVersion, std::to_string(st.bslVersion >> 4) + "." +
            std::to_string(st.bslVersion & 0x0F) + stepping));

        {
            uint32_t ip = st.clockHz / 1000000;
            uint32_t fp = st.clockHz % 1000000;
            char fb[8];
            std::snprintf(fb, sizeof(fb), "%06u", fp);
            fb[3] = '\0';
            log(Tr(Msg::ClockFrequency, std::to_string(ip) + "." + fb));
        }

        log(Tr(Msg::Separator));
    }

    namespace {

        // The chip-family summary block shared by detect and burn.
        void PrintProtoParams(const std::string& protoName, const McuStatus& st,
            const LogFn& log)
        {
            // Rebuilding a protocol just to read Params() would be wasteful, so
            // the caller logs the params it already has; this helper only
            // covers the EEPROM sizing line, which needs msr[4].
            if (protoName == "stc32" || protoName == "stc8g" ||
                protoName == "stc8") {
                uint8_t msr4 = st.msr[4];
                if (msr4 > 0 && msr4 < 0xFF) {
                    std::string chipName = ChipNameFromTable(st.magic);
                    uint32_t totalFlash = GetChipTotalFlash(chipName, st.magic);
                    uint32_t codeSize = (uint32_t)msr4 * 256;
                    if (codeSize <= totalFlash) {
                        uint32_t eeSize = totalFlash - codeSize;
                        log(Tr(Msg::CurrentEeprom, { std::to_string(eeSize),
                            std::to_string(codeSize) }));
                    }
                    else {
                        log(Tr(Msg::CurrentEepromInvalid));
                    }
                }
                else {
                    log(Tr(Msg::CurrentEepromUnset));
                }
            }
        }

        // Resolve the requested frequency against the family's own menu.
        //
        // This used to be a clamp: anything above the family's ceiling was
        // brought down to it. That is the wrong shape now that the menus differ
        // per family, because the two failures are different mistakes:
        //
        //   above the ceiling   35 MHz asked of an 8H -- a frequency that
        //                       family can reach, just not at that value. Clamp
        //                       to the ceiling and say so, as before.
        //   not in the menu     45.100 on an 8H, or 60 MHz on anything but the
        //                       32G144K246 -- no trim exists for it and none
        //                       can be derived. Refuse rather than round to
        //                       something else.
        //
        // The return value is the frequency to write; 0 means refused, and the
        // reason has already been logged.
        uint32_t ResolveFrequency(const std::string& chipName, uint32_t freq,
            const LogFn& log)
        {
            FrequencyMenu menu = FrequencyMenuFor(chipName);
            uint32_t ceiling = menu.hz[menu.maxIdx];

            if (freq > ceiling) {
                // Only a ceiling overshoot gets the old clamping treatment; an
                // in-range value that is not a menu entry falls through to the
                // refusal below, where it gets an honest message.
                if (IsFrequencyInMenu(chipName, ceiling)) {
                    log(Tr(Msg::FrequencyClamped, FormatDouble(ceiling / 1e6, 4)));
                    return ceiling;
                }
            }

            if (!IsFrequencyInMenu(chipName, freq)) {
                log(Tr(Msg::CliErrFreqNotInMenu, {
                    FormatDouble(freq / 1e6, 4), chipName.empty() ? "?" : chipName }));
                return 0;
            }
            if (!HasTrimFor(freq)) {
                // In the menu, but the protocol layer has no trim triple for it
                // yet. Refusing here names the problem at the top of the run
                // rather than in the middle of an erase.
                log(Tr(Msg::CliErrFreqNoTrim, FormatDouble(freq / 1e6, 4)));
                return 0;
            }
            return freq;
        }

        // Legacy families have an 8-bit BRT register with a fixed 16x divider,
        // which cannot reach past 115200. Clamping here rather than failing
        // mid-burn matches what the UI did.
        uint32_t ClampBaud(const std::string& protoName, uint32_t baud,
            const LogFn& log)
        {
            if (IsLegacyBaudProto(protoName) && baud > 115200) {
                log(Tr(Msg::BaudClamped, std::to_string(baud)));
                return 115200;
            }
            return baud;
        }

        std::unique_ptr<stcisp::IChannel> MakeChannel(bool useHid)
        {
            if (useHid)
                return std::unique_ptr<stcisp::IChannel>(new stcisp::HidChannel());
            return std::unique_ptr<stcisp::IChannel>(new stcisp::SerialChannel());
        }

    } // namespace

    // =======================================================================
    //  Detect
    // =======================================================================

    namespace {

        // Probe one transport and report what answered on it.
        //
        // Returns:
        //    1  a chip was identified and reported
        //    0  the transport was usable but nothing answered
        //   -1  the transport could not be used at all (no such device)
        //
        // The three outcomes are distinct because "nothing on the serial port"
        // is normal when the board is plugged in over USB, while "the HID
        // device could not be opened" is worth saying out loud.
        int DetectOneTransport(const BurnOptions& opts, const LogFn& log)
        {
            auto ch = MakeChannel(opts.useHid);
            std::atomic<bool> stopReq{ false };
            std::vector<uint8_t> rxBuf;

            auto probeList = MakeProbeList(opts.useHid);

            log(opts.useHid ? Tr(Msg::DetectingHid) : Tr(Msg::DetectingSerial));

            auto r = OpenSession(*ch, opts.deviceId, rxBuf, probeList, log,
                stopReq, "detect");

            if (r.openFailed) {
                if (ch->IsOpen()) ch->Close();
                log(opts.useHid ? Tr(Msg::DetectFailedNotHid)
                                : Tr(Msg::DetectFailedSerial));
                return -1;
            }

            if (!r.got) {
                if (ch->IsOpen()) ch->Close();
                log(Tr(Msg::ChipNotFound));
                return 0;
            }

            // Restore the parity the detected family needs before signing off,
            // so the chip leaves the ISP monitor in a defined state. The 0xFF
            // packet is the reset-and-run command.
            bool wantParity = true;
            for (auto& p : probeList) {
                if (p->Params().name == r.protoName) {
                    wantParity = p->UseEvenParity();
                    break;
                }
            }
            if (ch->IsOpen() && ch->EvenParity() != wantParity) {
                ch->SetParity(wantParity);
            }
            if (ch->IsOpen()) {
                auto reset = probeList.empty()
                    ? std::vector<uint8_t>{}
                    : probeList.front()->ProbeBuild({ 0xFF });
                if (!reset.empty()) ch->Write(reset);
                ch->Close();
            }

            std::string chipName = ChipNameFromTable(r.st.magic);
            r.chipName = chipName;

            // The 8051U is documented as HID-only, but it answered a serial
            // probe, and answering is the proof that matters. Report the
            // discrepancy and carry on rather than refusing: the transfer is
            // the experiment.
            if (IsHidOnlyChip(chipName) && !opts.useHid) {
                log(LogLine(Msg::HidOnlyAnsweredSerial, chipName,
                    i18n::LogKind::Attention));
                log(LogLine(Msg::HidOnlyContinuing, i18n::LogKind::Attention));
            }

            // The part answered over HID, which means it really does have a USB
            // controller -- but say so when the name is not one of the three
            // families that are known to: everything has been probed here, so a
            // hit on some other family would mean the vendor put USB on a part
            // the documentation does not list, and the result is worth
            // confirming.
            if (opts.useHid && !IsUsbCapableChip(chipName)) {
                log(LogLine(Msg::UnexpectedUsbChip, chipName,
                    i18n::LogKind::Attention));
            }

            log(Tr(Msg::DetectOk, chipName));
            PrintMcuInfo(r.st, r.protoName, log);
            PrintProtoParams(r.protoName, r.st, log);
            return 1;
        }

    } // namespace

    int RunDetect(const BurnOptions& opts, const LogFn& log)
    {
        // One transport, one device. Which one is decided before this is
        // called: an explicit --device wins, otherwise the single candidate the
        // transport reported, and anything ambiguous was refused earlier with
        // the candidate list. Probing every port in turn was tried and removed:
        // `detect` is about identifying the chip on the chosen link, and
        // listing what is attached is what `list` is for.
        return DetectOneTransport(opts, log) == 1 ? 0 : 1;
    }

    // =======================================================================
    //  Burn
    // =======================================================================

    int RunBurn(const BurnOptions& opts, const std::vector<uint8_t>& firmware,
        const LogFn& log, const ProgressFn& progress)
    {
        if (firmware.empty() || firmware.size() > 0x10000u) {
            log(Tr(Msg::FirmwareUnusable));
            return 1;
        }
        auto ch = MakeChannel(opts.useHid);
        std::atomic<bool> stopReq{ false };
        std::vector<uint8_t> rxBuf;
        std::unique_ptr<IStcProtocol> proto;

        auto fail = [&](Msg msg) {
            log(Tr(msg));
        };
        auto closeCh = [&]() {
            if (ch->IsOpen()) ch->Close();
        };

        auto probeList = MakeProbeList(opts.useHid);

        if (!opts.useHid)
            log(Tr(Msg::Handshaking));

        auto r = OpenSession(*ch, opts.deviceId, rxBuf, probeList, log, stopReq,
            opts.useHid ? "detect" : "burn");

        if (r.openFailed) {
            fail(opts.useHid ? Msg::BurnFailHidOpen : Msg::BurnFailSerialOpen);
            closeCh();
            return 1;
        }

        if (!r.got) {
            fail(opts.useHid ? Msg::BurnFailHidProbe : Msg::BurnFailSerialProbe);
            closeCh();
            return 1;
        }

        auto st = r.st;
        log(opts.useHid ? Tr(Msg::BurnChipIdentified) : Tr(Msg::BurnHandshakeOk));
        progress(5);

        bool wantParity = true;
        for (auto& p : probeList) {
            if (p->Params().name == r.protoName) {
                wantParity = p->UseEvenParity();
                break;
            }
        }
        if (ch->IsOpen() && ch->EvenParity() != wantParity) {
            if (!ch->SetParity(wantParity)) log(Tr(Msg::ParitySwitchFailed));
        }

        std::string chipName = ChipNameFromTable(st.magic);

        // Every part this tool knows answers on its serial ISP monitor, so
        // there is no transport gate to apply; the probe already decided
        // whether this chip is listening. An 8051U over serial is a supported
        // combination, only worth a note if the handshake then fails.
        if (IsHidOnlyChip(chipName) && !opts.useHid) {
            log(Tr(Msg::NoSerialIspWarning, chipName));
        }

        // Prefer the implementation that identified itself during probing; fall
        // back to the name-derived one, then to the series override.
        proto = CloneProtocolByName(r.protoName, opts.useHid);
        if (!proto) proto = CreateProtocol(chipName, opts.useHid);
        if (!proto && opts.protoOverride >= 0)
            proto = MakeProtocol(opts.protoOverride, opts.useHid);
        if (!proto) {
            fail(Msg::ProtocolSelectFailed);
            closeCh();
            return 1;
        }

        proto->m_status = st;
        proto->m_status.userTargetFreq = opts.targetFreq;
        proto->m_status.wantExternalClock = opts.clockExternal;

        // Families with no selectable frequency (STC10/11/12 and STC89/90) get
        // their clock from a crystal or from the internal RC at whatever it runs
        // at; the option byte only records the source. Writing a target
        // frequency there would program a trim for an oscillator the part does
        // not tune, so say so and stop rather than burning option bytes that
        // mean something else on this family.
        //
        // The test is on freqExplicit, not on targetFreq: the frequency has a
        // 24 MHz default, so a burn that never mentioned it still has a value
        // in hand and would otherwise be refused here on every STC12.
        if (opts.freqExplicit && !HasSelectableFrequency(chipName)) {
            log(LogLine(Msg::CliErrNoFreqForSeries, chipName, i18n::LogKind::Error));
            closeCh();
            return 1;
        }

        // Likewise for the EEPROM split: STC15/12/89 have no such field in
        // their option block, so a requested split would be programmed into an
        // option byte that means something else on those parts.
        if (opts.eepromBytes && !HasSelectableEeprom(chipName)) {
            log(LogLine(Msg::CliErrNoEepromForSeries, chipName, i18n::LogKind::Error));
            closeCh();
            return 1;
        }

        // And for the clock source: only STC10/11/12 record one in their option
        // byte. On the other families -freq already chooses the clock (the IRC)
        // or there is no option bit for it at all (STC89/90), so writing the
        // bit would flip an unrelated option.
        if (opts.clockExplicit && !HasSelectableClockSource(chipName)) {
            log(LogLine(Msg::CliErrNoClockForSeries, chipName, i18n::LogKind::Error));
            closeCh();
            return 1;
        }

        uint32_t targetFreq = opts.targetFreq;
        if (targetFreq) {
            // Deliberately not clamping a value that is not in the menu: a
            // clamp turns "60 MHz on an 8H" into "45.1584 MHz", which burns
            // option bytes at a speed the user never asked for. Overshooting
            // the ceiling is clamped (and logged); anything else is refused.
            targetFreq = ResolveFrequency(chipName, targetFreq, log);
            if (targetFreq == 0) {
                closeCh();
                return 1;
            }
        }
        proto->m_status.userTargetFreq = targetFreq;

        uint32_t totalFlash = GetChipTotalFlash(chipName, st.magic);

        // The preflight verdict is already a complete sentence from the chip
        // logic layer, which is shared with the tests, so it is printed as it
        // is rather than wrapped: translating it here would mean keeping a
        // second copy of the same diagnosis.
        std::string preflightError;
        if (!ValidateProgrammingTarget(chipName, firmware.size(), totalFlash,
                opts.eepromBytes, preflightError)) {
            log(i18n::kErrorMarker + preflightError);
            closeCh();
            return 1;
        }

        if (proto->SupportsEepromSplit()) {
            proto->SetEepromSplit(opts.eepromBytes, totalFlash);
        }

        PrintMcuInfo(st, proto->Params().name, log);

        {
            auto pp = proto->Params();
            log(Tr(Msg::BlockSize, std::to_string(pp.blockSize)));
            log(Tr(Msg::IapWait, Hex(pp.iapWait)));
            log(Tr(Msg::FiveAFive, Tr(pp.need5AA5 ? Msg::Required
                                                  : Msg::NotRequired)));
        }
        PrintProtoParams(proto->Params().name, st, log);

        if (proto->SupportsEepromSplit()) {
            if (opts.eepromBytes > 0) {
                log(Tr(Msg::EepromWrite, { FormatDouble(opts.eepromBytes / 1024.0, 1),
                    std::to_string(totalFlash - opts.eepromBytes) }));
            }
            else {
                log(Tr(Msg::EepromAuto));
            }
        }

        // ---- step 2: clock parameters / baud rate ----
        if (opts.useHid) {
            // The USB ISP interface needs no wakeup and has no baud rate to
            // negotiate, so this step only primes the option-block trim values.
            log(Tr(Msg::PrepareClock));
            if (!proto->Handshake(*ch, rxBuf, 0, log)) {
                fail(Msg::PrepareClockFailed);
                closeCh();
                return 1;
            }
            log(Tr(Msg::PrepareClockReady));
            progress(15);

            rxBuf.clear();
            ch->Flush();
            SleepMs(30);

            // 0x01 BAUD_SET must be sent before 0x05 / erase. There is no baud
            // rate to negotiate over USB, but the ISP still expects this
            // exchange; skipping it makes the subsequent erase command time out
            // without a reply.
            log(Tr(Msg::SetBaudHid));
            {
                std::vector<uint8_t> pl = { 0x01, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x80 };
                auto tx = proto->ProbeBuild(pl);
                ch->Write(tx);
                log(LogLine(Msg::LogTx, { "0x01", HexDump(tx) }, i18n::LogKind::Packet));

                std::vector<uint8_t> rr;
                if (!proto->ProbeWait(*ch, rxBuf, rr, 2000)) {
                    fail(Msg::SetBaudFailed);
                    closeCh();
                    return 1;
                }
                log(LogLine(Msg::LogRx, { "", "\b" + HexDump(rr) }, i18n::LogKind::Packet));
                if (rr.empty() || rr[0] != 0x01) {
                    fail(Msg::SetBaudBadReply);
                    closeCh();
                    return 1;
                }
            }
            log(Tr(Msg::SetBaudDone));
        }
        else {
            log(Tr(Msg::CalibrateBaud));
            uint32_t baud = ClampBaud(proto->Params().name, opts.baudRate, log);

            if (!proto->Handshake(*ch, rxBuf, baud, log)) {
                fail(Msg::CalibrateFailed);
                closeCh();
                return 1;
            }
            log(Tr(Msg::BaudSwitched, std::to_string(baud)));
            progress(15);

            rxBuf.clear();
            ch->Flush();
            SleepMs(30);
        }

        // ---- step 3: prepare programming (0x05), where the family has it ----
        if (NeedsPrepare05(proto->Params().name)) {
            log(Tr(Msg::PrepareProgramming));
            std::vector<uint8_t> pl;
            pl.push_back(0x05);
            if (st.bslVersion >= 0x72) {
                pl.push_back(0x00); pl.push_back(0x00);
                pl.push_back(0x5A); pl.push_back(0xA5);
            }
            auto tx = proto->ProbeBuild(pl);
            ch->Write(tx);
            log(LogLine(Msg::LogTx, { "0x05", HexDump(tx) }, i18n::LogKind::Packet));

            std::vector<uint8_t> rr;
            if (!proto->ProbeWait(*ch, rxBuf, rr, 2000)) {
                fail(Msg::Prepare05Failed);
                closeCh();
                return 1;
            }
            log(LogLine(Msg::LogRx, { "", "\b" + HexDump(rr) }, i18n::LogKind::Packet));
            if (rr.empty() || rr[0] != 0x05) {
                fail(Msg::Prepare05BadReply);
                closeCh();
                return 1;
            }
            log(Tr(Msg::Prepare05Ok));
        }
        else {
            log(Tr(Msg::Prepare05Skipped));
        }
        progress(25);

        // ---- step 4: erase ----
        log(Tr(Msg::EraseStart));
        if (!proto->EraseFlash(*ch, rxBuf, firmware.size(), firmware.size(), log)) {
            fail(Msg::EraseFailed);
            closeCh();
            return 1;
        }
        log(Tr(Msg::EraseOk));
        progress(35);

        // ---- step 5: program ----
        log(Tr(Msg::WriteStart));
        if (!proto->ProgramFlash(*ch, rxBuf, firmware, progress, log)) {
            fail(Msg::WriteBlockFailed);
            closeCh();
            return 1;
        }
        log(Tr(Msg::WriteOk));

        // ---- step 6: option bytes ----
        log(Tr(Msg::OptionsStart));
        {
            // Option bytes carry the watchdog, reset behaviour and clock
            // configuration; a wrong value can leave the chip unable to start,
            // so echo exactly what is about to be written.
            std::string cs = proto->ClockSourceText();
            std::string msr = "MSR: " + Hex(st.msr[0]) + " " + Hex(st.msr[1]) +
                " " + Hex(st.msr[2]) + " " + Hex(st.msr[3]);
            if (cs.empty())
                log(Tr(Msg::OptionsEcho, msr));
            else
                log(Tr(Msg::OptionsEchoClock, { msr, cs }));
        }
        if (!proto->ProgramOptions(*ch, rxBuf, log)) {
            fail(Msg::OptionsFailed);
            closeCh();
            return 1;
        }
        log(Tr(Msg::OptionsOk));
        progress(100);

        // ---- step 7: run ----
        log(Tr(Msg::BurnDone));
        proto->Disconnect(*ch);
        closeCh();
        return 0;
    }

} // namespace newisp
