// English source text -- the reference table.
//
// This file is included with different macros from three places, which is what
// keeps the enum, the English wording and the translation in step:
//
//   newisp/i18n.h        NEWISP_TEXT_EN(id, kind, text)  -> enum class Msg
//   src/core/i18n.cpp    NEWISP_TEXT_EN(id, kind, text)  -> the message table
//   src/i18n/*.inc       referenced entry by entry        -> one language each
//
// Rules for this file, all of them enforced by tests/test_i18n.cpp:
//
//   * the text is the complete line the user sees, slots included: put the
//     whole sentence here and fill {0}, {1}, ... at the call site, never
//     concatenate fragments around a translated string;
//   * {0}, {1}, ... are the only slots. Braces are not escaped, so a message
//     cannot contain a literal brace;
//   * identifiers are named after the English wording with the punctuation
//     dropped (Msg::EraseOk is "Erase complete."), so a translation can be
//     checked against its neighbour at a glance;
//   * the kind is what the logger needs to know about the line: Packet for raw
//     dumps (hidden unless --verbose), Attention for findings that must be
//     visible even in --quiet mode, Warning and Error for text that is always
//     shown, Normal for everything else.
//
// The Log* entries are lines the protocol layer emits through the LogFn
// callback; the Cli*, Inspect*, Usage* and Version* entries are printed by the
// CLI. Both travel the same path, so they live in one table.

// ---- chip table ----
NEWISP_TEXT_EN(ChipUnknownPlaceholder, Normal,  "unknown part")

// ---- option-byte clock source labels ----
NEWISP_TEXT_EN(LogClockSourceInternalIrc,     Normal, "internal IRC")
NEWISP_TEXT_EN(LogClockSourceExternalCrystal, Normal, "external crystal")

// ---- session: chip identification block ----
NEWISP_TEXT_EN(Separator,            Normal,    "--------------------------------------------------")
NEWISP_TEXT_EN(ChipModel,            Normal,    "Chip model : {0}")
NEWISP_TEXT_EN(Magic,                Normal,    "Magic      : {0}")
NEWISP_TEXT_EN(ProtocolName,         Normal,    "Protocol   : {0}")
NEWISP_TEXT_EN(BslVersion,           Normal,    "BSL version: {0}")
NEWISP_TEXT_EN(ClockFrequency,       Normal,    "Clock      : {0} MHz")

// ---- session: EEPROM report ----
NEWISP_TEXT_EN(CurrentEeprom,        Normal,    "Current EEPROM: {0} bytes (code area {1} bytes)")
NEWISP_TEXT_EN(CurrentEepromInvalid, Normal,    "Current EEPROM: invalid value")
NEWISP_TEXT_EN(CurrentEepromUnset,   Normal,    "Current EEPROM: not configured")

// ---- session: clamping ----
NEWISP_TEXT_EN(FrequencyClamped,     Normal,    "[Frequency] this family tops out at {0} MHz; target frequency lowered")
NEWISP_TEXT_EN(BaudClamped,          Normal,    "[Baud] this family tops out at 115200; lowered from {0}")

// ---- session: probe / handshake ----
NEWISP_TEXT_EN(DetectingHid,         Normal,    "Detecting (USB HID)...")
NEWISP_TEXT_EN(DetectingSerial,      Normal,    "Detecting (up to 500 auto-reset rounds)...")
NEWISP_TEXT_EN(DetectFailedNotHid,   Error,     "Failed to open the HID device; stopped")
NEWISP_TEXT_EN(DetectFailedSerial,   Error,     "Failed to open the serial port; stopped")
NEWISP_TEXT_EN(ChipNotFound,         Error,     "No chip detected")
NEWISP_TEXT_EN(DetectOk,             Normal,    "Detected: {0}")
NEWISP_TEXT_EN(Handshaking,          Normal,    "[1/7] Handshake (up to 500 auto-reset rounds)...")

// ---- session: transport open failures ----
NEWISP_TEXT_EN(BurnFailHidOpen,      Error,     "[Failed] HID device - could not open; check whether it is in use")
NEWISP_TEXT_EN(BurnFailSerialOpen,   Error,     "[Failed] serial port - could not open; check whether it is in use")
NEWISP_TEXT_EN(BurnFailHidProbe,     Error,     "[Failed] probe - no chip detected (hold BOOT and power-cycle)")
NEWISP_TEXT_EN(BurnFailSerialProbe,  Error,     "[Failed] handshake - still no chip after 500 rounds (check that the family is right)")
NEWISP_TEXT_EN(BurnChipIdentified,   Normal,    "[1/7] Chip identified")
NEWISP_TEXT_EN(BurnHandshakeOk,      Normal,    "[1/7] Handshake succeeded")

// ---- session: transport / family mismatches ----
NEWISP_TEXT_EN(HidOnlyAnsweredSerial, Attention, "[Attention] {0} is documented as HID-only, but it answered a serial probe.")
NEWISP_TEXT_EN(HidOnlyContinuing,    Attention, "[Attention] continuing with the serial flow. The manual disagrees with the chip; verify the result yourself.")
NEWISP_TEXT_EN(UnexpectedUsbChip,    Attention, "[Attention] {0} is not one of the families documented with a USB interface (STC8H, STC32, 8051U), yet it answered over HID. Verify the result.")
NEWISP_TEXT_EN(NoUsbForFamily,       Error,     "[Failed] {0} has no USB interface; only STC8H, STC32 and the 8051U can be programmed over HID. Use --serial.")
NEWISP_TEXT_EN(NoSerialIspMode,      Error,     "[Failed] this chip did not answer on the serial ISP monitor; try -H to use USB HID instead")
NEWISP_TEXT_EN(NoSerialIspWarning,   Warning,   "[Warning] {0} has no serial ISP monitor according to the documentation; continuing over serial.")
NEWISP_TEXT_EN(ProtocolSelectFailed, Error,     "[Failed] protocol - no implementation available for this chip")
NEWISP_TEXT_EN(ParitySwitchFailed,   Warning,   "[Warning] failed to switch parity")
NEWISP_TEXT_EN(FirmwareUnusable,     Error,     "[Failed] firmware is empty or exceeds the 16-bit writer; no device opened")

// ---- session: parameter block ----
NEWISP_TEXT_EN(BlockSize,            Normal,    "Block size : {0} bytes")
NEWISP_TEXT_EN(IapWait,              Normal,    "IAP wait   : 0x{0}")
NEWISP_TEXT_EN(FiveAFive,            Normal,    "5A A5      : {0}")
NEWISP_TEXT_EN(Required,             Normal,    "required")
NEWISP_TEXT_EN(NotRequired,          Normal,    "not required")
NEWISP_TEXT_EN(EepromWrite,          Normal,    "EEPROM     : {0} K (will be written, code area {1} bytes)")
NEWISP_TEXT_EN(EepromAuto,           Normal,    "EEPROM     : auto (keep the chip setting)")

// ---- session: burn steps ----
NEWISP_TEXT_EN(PrepareClock,         Normal,    "[2/7] Preparing clock parameters...")
NEWISP_TEXT_EN(PrepareClockFailed,   Error,     "[Failed] [2/7] parameters - failed")
NEWISP_TEXT_EN(PrepareClockReady,    Normal,    "[2/7] Parameters ready")
NEWISP_TEXT_EN(SetBaudHid,           Normal,    "[2/7] Setting baud rate (0x01)...")
NEWISP_TEXT_EN(SetBaudFailed,        Error,     "[Failed] [2/7] 0x01 - no response")
NEWISP_TEXT_EN(SetBaudBadReply,      Error,     "[Failed] [2/7] 0x01 - unexpected response packet")
NEWISP_TEXT_EN(SetBaudDone,          Normal,    "[2/7] Baud rate set")
NEWISP_TEXT_EN(CalibrateBaud,        Normal,    "[2/7] Calibrating and switching baud rate...")
NEWISP_TEXT_EN(CalibrateFailed,      Error,     "[Failed] [2/7] calibration - failed")
NEWISP_TEXT_EN(BaudSwitched,         Normal,    "[2/7] Switched to {0}")
NEWISP_TEXT_EN(PrepareProgramming,   Normal,    "[3/7] Preparing programming (0x05)...")
NEWISP_TEXT_EN(Prepare05Failed,      Error,     "[Failed] [3/7] 0x05 - no response")
NEWISP_TEXT_EN(Prepare05BadReply,    Error,     "[Failed] [3/7] 0x05 - unexpected response packet")
NEWISP_TEXT_EN(Prepare05Ok,          Normal,    "[3/7] Programming prepared")
NEWISP_TEXT_EN(Prepare05Skipped,     Normal,    "[3/7] Skipped (this family has no 0x05 step)")
NEWISP_TEXT_EN(EraseStart,           Normal,    "[4/7] Erasing...")
NEWISP_TEXT_EN(EraseFailed,          Error,     "[Failed] [4/7] erase - failed")
NEWISP_TEXT_EN(EraseOk,              Normal,    "[4/7] Erase complete")
NEWISP_TEXT_EN(WriteStart,           Normal,    "[5/7] Writing code...")
NEWISP_TEXT_EN(WriteBlockFailed,     Error,     "[Failed] [5/7] write block - failed")
NEWISP_TEXT_EN(WriteOk,              Normal,    "[5/7] Blocks written")
NEWISP_TEXT_EN(OptionsStart,         Normal,    "[6/7] Writing option bytes...")
NEWISP_TEXT_EN(OptionsEcho,          Normal,    "[6/7] Writing back {0}")
NEWISP_TEXT_EN(OptionsEchoClock,     Normal,    "[6/7] Writing back {0} (clock source={1})")
NEWISP_TEXT_EN(OptionsFailed,        Error,     "[Failed] [6/7] options - failed")
NEWISP_TEXT_EN(OptionsOk,            Normal,    "[6/7] Option bytes written")
NEWISP_TEXT_EN(BurnDone,             Normal,    "[7/7] Burn complete.")

// ---- session: bring-up log ----
NEWISP_TEXT_EN(LogSerialOpenFailed,  Error,     "[{0}] could not open the serial port; check whether it is in use, or pick another")
NEWISP_TEXT_EN(LogResetPulse,        Normal,    "[{0}] auto-reset pulse DTR 250 ms")
NEWISP_TEXT_EN(LogHandshakeRound,    Normal,    "[{0}] round {1}: handshaking...")
NEWISP_TEXT_EN(LogPulseProgress,     Normal,    "[{0}] round {1}: {2}/{3} pulses sent")
NEWISP_TEXT_EN(LogRoundNoResponse,   Normal,    "[{0}] round {1}: no response, retrying...")
NEWISP_TEXT_EN(LogIdentifiedAs,      Normal,    "[{0}] identified as {1}")
NEWISP_TEXT_EN(LogHidOpenFailed,     Error,     "[{0}] could not open the HID device; check whether it is in use")
NEWISP_TEXT_EN(LogProbeWriteFailed,  Warning,   "[{0}] failed to write the probe packet ({1})")
NEWISP_TEXT_EN(LogNoAnswer,          Normal,    "[{0}] {1} did not answer ({2} bytes received)")
NEWISP_TEXT_EN(LogRxPayload,         Packet,    "[receive] ({0}) payload {1}: {2}")

// ---- protocol: framing and packet dumps ----
NEWISP_TEXT_EN(LogTx,                Packet,    "[send] {0} {1}")
NEWISP_TEXT_EN(LogRx,                Packet,    "[receive] {0} {1}")
NEWISP_TEXT_EN(LogUid,               Packet,    "[UID] {0} {1}")

// ---- protocol: calibration ----
NEWISP_TEXT_EN(LogTargetUnknown,     Warning,   "[{0}] cannot determine the target frequency; skipped")
NEWISP_TEXT_EN(LogHidNoBaudCalib,    Normal,    "[{0}] HID needs no baud calibration")
NEWISP_TEXT_EN(LogHidTargetFreq,     Normal,    "[{0}] target frequency {1} kHz (written to the option bytes)")
NEWISP_TEXT_EN(LogBaudRateFailed,    Error,     "[{0}] could not set the target baud rate")
NEWISP_TEXT_EN(LogBaudSwitchFailed,  Error,     "[{0}] failed to switch the baud rate")
NEWISP_TEXT_EN(LogResponseBad,       Error,     "[{0}] {1}: unexpected response")
NEWISP_TEXT_EN(LogNoResponse,        Error,     "[{0}] {1}: no response")
NEWISP_TEXT_EN(LogBaudSwitchOk,      Normal,    "[{0}] baud rate switched")
NEWISP_TEXT_EN(LogPingTimeout,       Error,     "[{0}] ping-pong timed out")
NEWISP_TEXT_EN(LogPingBadReply,      Error,     "[{0}] ping-pong got an unexpected response")
NEWISP_TEXT_EN(LogCoarseNoResponse,  Error,     "[{0}] coarse sweep: no response")
NEWISP_TEXT_EN(LogCoarseReadFailed,  Error,     "[{0}] coarse sweep: could not read the packet")
NEWISP_TEXT_EN(LogCoarseBadLength,   Error,     "[{0}] coarse sweep: wrong length or first byte")
NEWISP_TEXT_EN(LogCoarseSameCount,   Error,     "[{0}] coarse sweep failed: ca equals cb")
NEWISP_TEXT_EN(LogCoarseTrimOutOfRange, Error,  "[{0}] coarse sweep: programming trim out of range")
NEWISP_TEXT_EN(LogCoarseRangeInvalid, Error,    "[{0}] coarse sweep: invalid refinement window")
NEWISP_TEXT_EN(LogCoarseStartOutOfRange, Error, "[{0}] coarse sweep: refinement start out of range")
NEWISP_TEXT_EN(LogFineNoResponse,    Error,     "[{0}] fine sweep: no response")
NEWISP_TEXT_EN(LogFineReadFailed,    Error,     "[{0}] fine sweep: could not read the packet")
NEWISP_TEXT_EN(LogFineBadLength,     Error,     "[{0}] fine sweep: wrong length or first byte")
NEWISP_TEXT_EN(LogRound1NoResponse,  Error,     "[{0}] round 1: no response")
NEWISP_TEXT_EN(LogRound1ReadFailed,  Error,     "[{0}] round 1: could not read the packet")
NEWISP_TEXT_EN(LogRound1BadReply,    Error,     "[{0}] round 1: unexpected response")
NEWISP_TEXT_EN(LogRound2NoResponse,  Error,     "[{0}] round 2: no response")
NEWISP_TEXT_EN(LogRound2ReadFailed,  Error,     "[{0}] round 2: could not read the packet")
NEWISP_TEXT_EN(LogRound2ShortReply,  Error,     "[{0}] round 2: response shorter than expected")
NEWISP_TEXT_EN(LogRangeSelectFailed, Error,     "[{0}] could not pick a coarse calibration window")
NEWISP_TEXT_EN(LogExternalBaudOutOfRange, Error, "[{0}] external-clock baud divider out of range")
NEWISP_TEXT_EN(LogHostBaudSwitchFailed, Error,  "[{0}] the host failed to switch baud rate")
NEWISP_TEXT_EN(LogHostBaudSwitched,  Normal,    "[{0}] host switched to {1}")
NEWISP_TEXT_EN(LogTrimUserProg,      Normal,    "[{0}] user trim={1} range=0x{2}, programming trim={3} range=0x{4}")
NEWISP_TEXT_EN(LogTrimFinal,         Normal,    "[{0}] final user trim={1} range=0x{2}, programming trim={3} range=0x{4}")
NEWISP_TEXT_EN(LogTrimSummary,       Normal,    "[{0}] trim_adj={1}, trim_range=0x{2}, trim_div={3}, trim_freq={4} Hz")
NEWISP_TEXT_EN(LogPulseStats,        Normal,    "[{0}] pulse 0xFE returned={1}, took {2} ms")
NEWISP_TEXT_EN(LogRxBytes,           Normal,    "[{0}] {1} bytes: {2}")
NEWISP_TEXT_EN(LogNoBytes,           Normal,    "none")
NEWISP_TEXT_EN(LogBandBoundaries,    Normal,    "[{0}] band boundaries: {1} / {2} / {3} / {4} / {5} kHz")
NEWISP_TEXT_EN(LogBandChosen,        Normal,    "[{0}] band {1} selected (band=0x{2})")
NEWISP_TEXT_EN(LogOscParams,         Normal,    "[{0}] osc_div={1}, osc_trimx={2}, osc_bandx=0x{3}")
NEWISP_TEXT_EN(LogBestTrim,          Normal,    "[{0}] best idx={1}, trim={2}, band=0x{3}, actual {4} MHz, error {5}%")
NEWISP_TEXT_EN(LogCalibrationOk,     Normal,    "[{0}] calibration succeeded")

// ---- protocol: erase / write / options failures ----
NEWISP_TEXT_EN(LogEraseTimeout,      Error,     "[{0}] erase timed out")
NEWISP_TEXT_EN(LogEraseBadReply,     Error,     "[{0}] erase response invalid")
NEWISP_TEXT_EN(LogBlockTimeout,      Error,     "[{0}] block {1} timed out")
NEWISP_TEXT_EN(LogBlockBadReply,     Error,     "[{0}] block {1}: unexpected response")
NEWISP_TEXT_EN(LogBlockProgress,     Normal,    "[{0}] {1}/{2} blocks")
NEWISP_TEXT_EN(LogFinishTimeout,     Error,     "[{0}] final packet timed out")
NEWISP_TEXT_EN(LogFinishBadReply,    Error,     "[{0}] final packet: unexpected response")
NEWISP_TEXT_EN(LogOptionsTimeout,    Error,     "[{0}] option block timed out")
NEWISP_TEXT_EN(LogOptionsBadReply,   Error,     "[{0}] option block: unexpected response")
NEWISP_TEXT_EN(LogOptionsTailTimeout, Error,    "[{0}] option closing packet timed out")
NEWISP_TEXT_EN(LogOptionsTailBadReply, Error,   "[{0}] option closing packet: unexpected response")
NEWISP_TEXT_EN(LogOptionsFreqUnsupported, Error, "[{0}] {1} Hz is not a supported IRC frequency; write aborted")
NEWISP_TEXT_EN(LogOptionsFreqList,   Error,     "[{0}] supported frequencies (MHz): {1}")
NEWISP_TEXT_EN(LogOptionsTrimUnmeasured, Warning, "[{0}] no calibration value and no table entry; trim left at its placeholder")
NEWISP_TEXT_EN(LogOptionsTrimTable,  Normal,    "[{0}] using table trim (uncalibrated): freq={1} Hz, trim={2} {3} {4}")
NEWISP_TEXT_EN(LogOptionsEeprom,     Normal,    "[{0}] EEPROM={1}, total={2}, opts[39]=0x{3} (code={4})")
NEWISP_TEXT_EN(LogOptionsWritten,    Normal,    "[{0}] freq={1} Hz, count=0x{2}, vrtrim=0x{3}, trim={4} {5} {6}, EEPROM={7}, opts[39]=0x{8}")
NEWISP_TEXT_EN(LogOptionsMsr,        Normal,    "[{0}] MSR={1} (clock source={2}, watchdog={3})")
NEWISP_TEXT_EN(LogOptionsMsrClock,   Normal,    "[{0}] MSR={1} (clock source={2})")
NEWISP_TEXT_EN(LogClockSourceExternal, Normal,  "external")
NEWISP_TEXT_EN(LogClockSourceInternal, Normal,  "internal")
NEWISP_TEXT_EN(LogWatchdogOff,       Normal,    "off")
NEWISP_TEXT_EN(LogWatchdogOn,        Normal,    "on")

// ---- CLI: device listing ----
NEWISP_TEXT_EN(CliNoSerialPorts,     Normal,    "No serial ports found")
NEWISP_TEXT_EN(CliSerialPorts,       Normal,    "Serial ports ({0}):")
NEWISP_TEXT_EN(CliHidNotBuilt,       Normal,    "This build has no USB HID support")
NEWISP_TEXT_EN(CliStcIspDevices,     Normal,    "STC ISP devices (VID {0} PID {1}): {2}")
NEWISP_TEXT_EN(CliOtherStcHid,       Normal,    "Other STC HID devices (VID {0}, excluding ISP): {1}")
NEWISP_TEXT_EN(CliNoStcDevices,      Normal,    "No STC devices found. If the chip is powered up with BOOT held, -v lists every HID device to help diagnose it.")
NEWISP_TEXT_EN(CliAllHidDevices,     Normal,    "All HID devices: {0} (including mice and keyboards; diagnostic only)")

// ---- CLI: no-command prompt ----
NEWISP_TEXT_EN(CliInvite,            Normal,    "Run '{0} help' for usage, or '{0} list' to list available devices")

// ---- CLI: device selection errors ----
NEWISP_TEXT_EN(CliErrNoHidSupport,   Error,     "This build has no USB HID support; use --serial or rebuild")
NEWISP_TEXT_EN(CliErrNoIspDevice,    Error,     "No STC USB ISP device found (hold BOOT and power-cycle)")
NEWISP_TEXT_EN(CliErrManyIspDevices, Error,     "Several STC USB ISP devices found; choose one with --device:")
NEWISP_TEXT_EN(CliErrNoSerialPorts,  Error,     "No serial ports found")
NEWISP_TEXT_EN(CliErrManySerial,     Error,     "Several serial ports found; choose one with --device:")

// ---- CLI: argument errors ----
NEWISP_TEXT_EN(CliErrPrefix,         Error,     "error: {0}")
NEWISP_TEXT_EN(CliErrInspectOption,  Error,     "inspect is offline; programming options are not allowed")
NEWISP_TEXT_EN(CliErrDetectOption,   Error,     "detect only identifies the chip; programming options such as -freq, -eeprom, -clock and -series do not apply")
NEWISP_TEXT_EN(CliErrNeedsValue,     Error,     "option {0} needs a value")
NEWISP_TEXT_EN(CliErrBadBaud,        Error,     "cannot parse baud rate: {0}")
NEWISP_TEXT_EN(CliErrBadFreq,        Error,     "cannot parse frequency: {0}")
NEWISP_TEXT_EN(CliErrBadEeprom,      Error,     "cannot parse EEPROM size: {0}")
NEWISP_TEXT_EN(CliErrBadClock,       Error,     "--clock expects internal or external, got: {0}")
NEWISP_TEXT_EN(CliErrBadOutput,      Error,     "--output expects quiet, normal or verbose, got: {0}")
NEWISP_TEXT_EN(CliErrBadSeries,      Error,     "unknown series: {0}")
NEWISP_TEXT_EN(CliErrUnknownCommand, Error,     "unknown command: {0}")
NEWISP_TEXT_EN(CliErrUnknownOption,  Error,     "unknown option: {0}")
NEWISP_TEXT_EN(CliErrUnknownOptionHint, Error,  "unknown option: {0} (did you mean {1}?)")
NEWISP_TEXT_EN(CliErrExtraArgument,  Error,     "unexpected extra argument: {0}")
NEWISP_TEXT_EN(CliErrNeedFile,       Error,     "{0} needs a firmware image as its first argument: newisp {0} FILE [options]")
NEWISP_TEXT_EN(CliErrNeedFileBurn,   Error,     "burn needs a firmware image as its first argument: newisp burn FILE [options] [device]")
NEWISP_TEXT_EN(CliErrInspectDevice,  Error,     "inspect is offline; device/programming options are not allowed")
NEWISP_TEXT_EN(CliErrFreqTooHigh,    Error,     "target frequency {0} Hz is above every family's menu (highest entry {1} MHz)")
NEWISP_TEXT_EN(CliErrFreqNotInMenu,  Error,     "{0} MHz is not an entry of the {1} frequency menu; nothing was written")
NEWISP_TEXT_EN(CliErrFreqNoTrim,     Error,     "{0} MHz is in the menu but has no trim triple yet; nothing was written")
NEWISP_TEXT_EN(CliErrNoFreqForSeries, Error,    "{0} has no selectable frequency: it runs from its clock source, so -freq does not apply")
NEWISP_TEXT_EN(CliErrNoEepromForSeries, Error,  "{0} has no selectable EEPROM split: its option block does not carry one, so -eeprom does not apply")
NEWISP_TEXT_EN(CliErrNoClockForSeries, Error,   "{0} has no selectable clock source: only STC10/11/12 record one, so -clock does not apply")


// ---- CLI: inspect ----
NEWISP_TEXT_EN(InspectValid,         Normal,    "Intel HEX: valid; {0} data bytes")
NEWISP_TEXT_EN(InspectRange,         Normal,    "  0x{0}-0x{1} : {2} bytes")
NEWISP_TEXT_EN(InspectEntry,         Normal,    "Entry metadata: type {0}, value 0x{1}")
NEWISP_TEXT_EN(InspectAddressMap,    Normal,    "Address map: 16-bit linear; target capacity/options still require validation")
NEWISP_TEXT_EN(InspectUnavailable,   Normal,    "Programming unavailable: {0}")
NEWISP_TEXT_EN(InspectOffline,       Normal,    "Offline inspection only; no device enumerated, opened, reset or written")

// ---- CLI: run header and summary ----
NEWISP_TEXT_EN(DeviceLine,           Normal,    "Device     : {0}")
NEWISP_TEXT_EN(TransportLine,        Normal,    "Transport  : {0}")
NEWISP_TEXT_EN(TransportSerial,      Normal,    "serial")
NEWISP_TEXT_EN(FirmwareLine,         Normal,    "Firmware   : {0} ({1} bytes, address 0x{2}-0x{3})")
NEWISP_TEXT_EN(TargetFreqLine,       Normal,    "Target freq: {0} MHz")
NEWISP_TEXT_EN(BaudLine,             Normal,    "Baud rate  : {0}")
NEWISP_TEXT_EN(EepromLine,           Normal,    "EEPROM     : {0} bytes")
NEWISP_TEXT_EN(ProgressWrite,        Normal,    "Writing: {0}%")
NEWISP_TEXT_EN(CliFirmwareEmpty,     Error,     "error: the firmware is empty")
NEWISP_TEXT_EN(BurnSucceeded,        Normal,    "Burn succeeded")
NEWISP_TEXT_EN(BurnFailed,           Error,     "Burn failed")

// ---- CLI: version banner ----
NEWISP_TEXT_EN(VersionProtocols,     Normal,    "Protocols: STC89/90/10/11/12/15/8/8G/8H/32/8051U")
NEWISP_TEXT_EN(VersionTransports,    Normal,    "Transport: serial")
NEWISP_TEXT_EN(VersionTransportsHid, Normal,    "Transport: serial + USB HID")

// ---- CLI: usage text ----
//
// One entry per line of the banner. {0} is the program name as invoked.
NEWISP_TEXT_EN(UsageTitle,           Normal,    "newisp {0} -- STC MCU programmer (serial + USB HID)")
NEWISP_TEXT_EN(UsageUsage,           Normal,    "Usage:")
NEWISP_TEXT_EN(UsageCmdNone,         Normal,    "list devices, then this help")
NEWISP_TEXT_EN(UsageCmdHelp,         Normal,    "this help")
NEWISP_TEXT_EN(UsageCmdVersion,      Normal,    "version and the transports this build has")
NEWISP_TEXT_EN(UsageCmdList,         Normal,    "list serial and HID devices")
NEWISP_TEXT_EN(UsageCmdDetect,       Normal,    "probe and identify a chip")
NEWISP_TEXT_EN(UsageCmdInspect,      Normal,    "validate HEX and show address ranges; no device I/O")
NEWISP_TEXT_EN(UsageCmdBurn,         Normal,    "erase, write and set options")
NEWISP_TEXT_EN(UsageTransport,       Normal,    "Transports:")
NEWISP_TEXT_EN(UsageSerial,          Normal,    "use the serial ISP monitor (default)")
NEWISP_TEXT_EN(UsageHid,             Normal,    "use the factory USB HID ISP interface")
NEWISP_TEXT_EN(UsageDevice,          Normal,    "Device:")
NEWISP_TEXT_EN(UsageDeviceName,      Normal,    "COM7, /dev/ttyUSB0, or a HID interface path")
NEWISP_TEXT_EN(UsageDeviceDefault,   Normal,    "(default: the only device found, or a")
NEWISP_TEXT_EN(UsageDeviceDefault2,  Normal,    "friendly error naming the candidates)")
NEWISP_TEXT_EN(UsageProgramming,     Normal,    "Programming:")
NEWISP_TEXT_EN(UsageSerialBaud,      Normal,    "BAUD: handshake rate, default 115200")
NEWISP_TEXT_EN(UsageSerialBaudForms, Normal,    "e.g. -s 921600, -s 115.2k; serial only")
NEWISP_TEXT_EN(UsageFreq,            Normal,    "target IRC frequency (default 24 MHz)")
NEWISP_TEXT_EN(UsageFreqForms,       Normal,    "accepts 24000000, 24M, 11.0592M;")
NEWISP_TEXT_EN(UsageFreqForms2,      Normal,    "must be an entry of that family's menu")
NEWISP_TEXT_EN(UsageEeprom,          Normal,    "EEPROM split, e.g. 4K (0 = keep chip setting)")
NEWISP_TEXT_EN(UsageClockSrc,        Normal,    "clock source for the STC10/11/12 option byte")
NEWISP_TEXT_EN(UsageSeries,          Normal,    "fallback series when probing identifies")
NEWISP_TEXT_EN(UsageSeriesNames,     Normal,    "nothing: stc89, stc12, stc15, stc8g,")
NEWISP_TEXT_EN(UsageSeriesNames2,    Normal,    "stc32, stc8h, stc8a, stc8")
NEWISP_TEXT_EN(UsageSeriesFull,      Normal,    "always spelled out in full (no -ser):")
NEWISP_TEXT_EN(UsageOutput,          Normal,    "Output:")
NEWISP_TEXT_EN(UsageLangBuild,       Normal,    "language is chosen when the tool is built,")
NEWISP_TEXT_EN(UsageLangBuildSet,    Normal,    "not at run time: configure with")
NEWISP_TEXT_EN(UsageLangBuildThis,   Normal,    "NEWISP_LANG=en or zh-CN. This build speaks ")
NEWISP_TEXT_EN(UsageOutputFlag,      Normal,    "how much of the log to show")
NEWISP_TEXT_EN(UsageOutputLevels,    Normal,    "quiet: errors and the final summary only;")
NEWISP_TEXT_EN(UsageOutputDefault,   Normal,    "normal (default) or verbose (raw packets)")
NEWISP_TEXT_EN(UsageGeneral,         Normal,    "General:")
NEWISP_TEXT_EN(UsageVersion,         Normal,    "show the version")
NEWISP_TEXT_EN(UsageExamples,        Normal,    "Examples:")
NEWISP_TEXT_EN(UsageExampleHid,      Normal,    "hold BOOT while plugging in")
NEWISP_TEXT_EN(UsageNotes,           Normal,    "Notes:")
NEWISP_TEXT_EN(UsageNoteSerial,      Normal,    "Serial: the tool pulses DTR and spams the wakeup byte, so it can")
NEWISP_TEXT_EN(UsageNoteSerial2,     Normal,    "catch a chip that is still running user code. If the board has no")
NEWISP_TEXT_EN(UsageNoteSerial3,     Normal,    "auto-reset circuit, power-cycle it during the handshake.")
NEWISP_TEXT_EN(UsageNoteHid,         Normal,    "HID: hold the BOOT pin low while plugging the board in. There is")
NEWISP_TEXT_EN(UsageNoteHid2,        Normal,    "no auto-reset over USB.")
NEWISP_TEXT_EN(UsageNote8051u,       Normal,    "The 8051U family has no serial ISP monitor at all and can only be")
NEWISP_TEXT_EN(UsageNote8051u2,      Normal,    "programmed over HID.")