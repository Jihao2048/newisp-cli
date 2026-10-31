// Simplified Chinese translation table.
//
// Read this file next to include/newisp/text_en.inc: the entries are listed in
// exactly the same order and carry the same identifiers, so a mistranslation or
// a misalignment is visible by comparing two neighbouring lines.
//
// Rules (checked by tests/test_i18n.cpp):
//
//   * same identifier, same order, same count as text_en.inc -- the array in
//     src/core/i18n.cpp is sized from the English table, so a mismatch is a
//     compile error;
//   * the {0}, {1}, ... slots must match the English entry exactly: same
//     number, same indices. Values are substituted after translation, so a
//     dropped slot loses information;
//   * keep the bracketed prefixes ([1/7], [4/7], [Failed], [Warning], ...)
//     identical to the English ones: users and support threads grep for them,
//     and they must not depend on the UI language. Translate the step names and
//     the wording, not the tags;
//   * leave identifiers alone (COM7, /dev/ttyUSB0, stc32, --hid, MSR, IRC, BSL,
//     Hex, MHz, kHz, Hz, EEPROM, opts[39], trim_adj, ...) -- they are the names
//     of things on the wire, in the datasheet or on the command line;
//   * keep column alignment inside the chip report and the parameter block: the
//     label column is padded to the same width as in English.

// ---- chip table ----
NEWISP_TEXT_EN(ChipUnknownPlaceholder, Normal,  "未知型号")

// ---- option-byte clock source labels ----
NEWISP_TEXT_EN(LogClockSourceInternalIrc,     Normal, "内部 IRC")
NEWISP_TEXT_EN(LogClockSourceExternalCrystal, Normal, "外部晶振")

// ---- session: chip identification block ----
NEWISP_TEXT_EN(Separator,            Normal,    "--------------------------------------------------")
NEWISP_TEXT_EN(ChipModel,            Normal,    "芯片型号   : {0}")
NEWISP_TEXT_EN(Magic,                Normal,    "Magic      : {0}")
NEWISP_TEXT_EN(ProtocolName,         Normal,    "协议       : {0}")
NEWISP_TEXT_EN(BslVersion,           Normal,    "BSL 版本   : {0}")
NEWISP_TEXT_EN(ClockFrequency,       Normal,    "时钟频率   : {0} MHz")

// ---- session: EEPROM report ----
NEWISP_TEXT_EN(CurrentEeprom,        Normal,    "当前 EEPROM: {0} 字节（程序区 {1} 字节）")
NEWISP_TEXT_EN(CurrentEepromInvalid, Normal,    "当前 EEPROM: 值异常")
NEWISP_TEXT_EN(CurrentEepromUnset,   Normal,    "当前 EEPROM: 未设置")

// ---- session: clamping ----
NEWISP_TEXT_EN(FrequencyClamped,     Normal,    "[频率] 该系列最高 {0} MHz，目标频率已下调")
NEWISP_TEXT_EN(BaudClamped,          Normal,    "[波特率] 该系列最高 115200，已从 {0} 下调")

// ---- session: probe / handshake ----
NEWISP_TEXT_EN(DetectingHid,         Normal,    "检测中（USB HID）...")
NEWISP_TEXT_EN(DetectingSerial,      Normal,    "检测中（最多 500 轮自动重启）...")
NEWISP_TEXT_EN(DetectFailedNotHid,   Error,     "打开 HID 设备失败，已停止")
NEWISP_TEXT_EN(DetectFailedSerial,   Error,     "打开串口失败，已停止")
NEWISP_TEXT_EN(ChipNotFound,         Error,     "未检测到芯片")
NEWISP_TEXT_EN(DetectOk,             Normal,    "检测成功: {0}")
NEWISP_TEXT_EN(Handshaking,          Normal,    "[1/7] 握手（最多 500 轮自动重启）...")

// ---- session: transport open failures ----
NEWISP_TEXT_EN(BurnFailHidOpen,      Error,     "[Failed] HID 设备 - 打开失败，请检查设备是否被占用")
NEWISP_TEXT_EN(BurnFailSerialOpen,   Error,     "[Failed] 串口 - 打开失败，请检查设备是否被占用")
NEWISP_TEXT_EN(BurnFailHidProbe,     Error,     "[Failed] 探测 - 未检测到芯片（确认已按住 BOOT 重新上电）")
NEWISP_TEXT_EN(BurnFailSerialProbe,  Error,     "[Failed] 握手 - 500 轮后仍未检测到芯片（检查类型是否选对）")
NEWISP_TEXT_EN(BurnChipIdentified,   Normal,    "[1/7] 已识别芯片")
NEWISP_TEXT_EN(BurnHandshakeOk,      Normal,    "[1/7] 握手成功")

// ---- session: transport / family mismatches ----
NEWISP_TEXT_EN(HidOnlyAnsweredSerial, Attention, "[Attention] {0} 按官方手册只有 USB HID 下载方式，但它在串口上应答了探测。")
NEWISP_TEXT_EN(HidOnlyContinuing,    Attention, "[Attention] 继续按串口流程处理。这是手册与实际不符的情况，结果需要自行确认。")
NEWISP_TEXT_EN(UnexpectedUsbChip,    Attention, "[Attention] {0} 不在文档标明带 USB 接口的系列（STC8H、STC32、8051U）之内，却在 HID 上应答了。请自行确认结果。")
NEWISP_TEXT_EN(NoUsbForFamily,       Error,     "[Failed] {0} 没有 USB 接口；只有 STC8H、STC32 和 8051U 能走 HID。请改用 --serial。")
NEWISP_TEXT_EN(NoSerialIspMode,      Error,     "[Failed] 该芯片没有应答串口 ISP；可改用 -H 走 USB HID")
NEWISP_TEXT_EN(NoSerialIspWarning,   Warning,   "[Warning] {0} 按文档说明没有串口 ISP 监视器，本次按串口流程继续。")
NEWISP_TEXT_EN(ProtocolSelectFailed, Error,     "[Failed] 协议 - 无法为该芯片选择协议实现")
NEWISP_TEXT_EN(ParitySwitchFailed,   Warning,   "[Warning] 切换校验位失败")
NEWISP_TEXT_EN(FirmwareUnusable,     Error,     "[Failed] 固件为空或超出 16 位写入器范围；未打开设备")

// ---- session: parameter block ----
NEWISP_TEXT_EN(BlockSize,            Normal,    "块大小     : {0} 字节")
NEWISP_TEXT_EN(IapWait,              Normal,    "IAP 等待   : 0x{0}")
NEWISP_TEXT_EN(FiveAFive,            Normal,    "5A A5      : {0}")
NEWISP_TEXT_EN(Required,             Normal,    "需要")
NEWISP_TEXT_EN(NotRequired,          Normal,    "不需要")
NEWISP_TEXT_EN(EepromWrite,          Normal,    "EEPROM     : {0} K（将写入，程序区 {1} 字节）")
NEWISP_TEXT_EN(EepromAuto,           Normal,    "EEPROM     : 自动（保留芯片设置）")

// ---- session: burn steps ----
NEWISP_TEXT_EN(PrepareClock,         Normal,    "[2/7] 准备时钟参数...")
NEWISP_TEXT_EN(PrepareClockFailed,   Error,     "[Failed] [2/7] 参数 - 失败")
NEWISP_TEXT_EN(PrepareClockReady,    Normal,    "[2/7] 参数就绪")
NEWISP_TEXT_EN(SetBaudHid,           Normal,    "[2/7] 设置波特率 (0x01)...")
NEWISP_TEXT_EN(SetBaudFailed,        Error,     "[Failed] [2/7] 0x01 - 无响应")
NEWISP_TEXT_EN(SetBaudBadReply,      Error,     "[Failed] [2/7] 0x01 - 响应包错误")
NEWISP_TEXT_EN(SetBaudDone,          Normal,    "[2/7] 波特率设置完成")
NEWISP_TEXT_EN(CalibrateBaud,        Normal,    "[2/7] 校准并切换波特率...")
NEWISP_TEXT_EN(CalibrateFailed,      Error,     "[Failed] [2/7] 校准 - 失败")
NEWISP_TEXT_EN(BaudSwitched,         Normal,    "[2/7] 已切到 {0}")
NEWISP_TEXT_EN(PrepareProgramming,   Normal,    "[3/7] 准备编程 (0x05)...")
NEWISP_TEXT_EN(Prepare05Failed,      Error,     "[Failed] [3/7] 0x05 - 无响应")
NEWISP_TEXT_EN(Prepare05BadReply,    Error,     "[Failed] [3/7] 0x05 - 响应包错误")
NEWISP_TEXT_EN(Prepare05Ok,          Normal,    "[3/7] 准备编程成功")
NEWISP_TEXT_EN(Prepare05Skipped,     Normal,    "[3/7] 跳过（该系列无 0x05 步骤）")
NEWISP_TEXT_EN(EraseStart,           Normal,    "[4/7] 擦除...")
NEWISP_TEXT_EN(EraseFailed,          Error,     "[Failed] [4/7] 擦除 - 失败")
NEWISP_TEXT_EN(EraseOk,              Normal,    "[4/7] 擦除成功")
NEWISP_TEXT_EN(WriteStart,           Normal,    "[5/7] 写入代码...")
NEWISP_TEXT_EN(WriteBlockFailed,     Error,     "[Failed] [5/7] 写块 - 失败")
NEWISP_TEXT_EN(WriteOk,              Normal,    "[5/7] 写块完成")
NEWISP_TEXT_EN(OptionsStart,         Normal,    "[6/7] 写入选项...")
NEWISP_TEXT_EN(OptionsEcho,          Normal,    "[6/7] 将回写 {0}")
NEWISP_TEXT_EN(OptionsEchoClock,     Normal,    "[6/7] 将回写 {0}（时钟源={1}）")
NEWISP_TEXT_EN(OptionsFailed,        Error,     "[Failed] [6/7] 选项 - 失败")
NEWISP_TEXT_EN(OptionsOk,            Normal,    "[6/7] 选项写入成功")
NEWISP_TEXT_EN(BurnDone,             Normal,    "[7/7] 烧录完成！")

// ---- session: bring-up log ----
NEWISP_TEXT_EN(LogSerialOpenFailed,  Error,     "[{0}] 打开串口失败，请检查端口是否被占用或选择正确")
NEWISP_TEXT_EN(LogResetPulse,        Normal,    "[{0}] 自动复位脉冲 DTR 250ms")
NEWISP_TEXT_EN(LogHandshakeRound,    Normal,    "[{0}] 第 {1} 轮握手...")
NEWISP_TEXT_EN(LogPulseProgress,     Normal,    "[{0}] 第 {1} 轮: 已 pulse {2}/{3}")
NEWISP_TEXT_EN(LogRoundNoResponse,   Normal,    "[{0}] 第 {1} 轮无响应，重试...")
NEWISP_TEXT_EN(LogIdentifiedAs,      Normal,    "[{0}] 识别为 {1}")
NEWISP_TEXT_EN(LogHidOpenFailed,     Error,     "[{0}] 打开 HID 设备失败，请检查设备是否被占用")
NEWISP_TEXT_EN(LogProbeWriteFailed,  Warning,   "[{0}] 写入探测包失败 ({1})")
NEWISP_TEXT_EN(LogNoAnswer,          Normal,    "[{0}] {1} 无应答（已收 {2} 字节）")
NEWISP_TEXT_EN(LogRxPayload,         Packet,    "[接收] ({0}) payload {1}: {2}")

// ---- protocol: framing and packet dumps ----
NEWISP_TEXT_EN(LogTx,                Packet,    "[发送] {0} {1}")
NEWISP_TEXT_EN(LogRx,                Packet,    "[接收] {0} {1}")
NEWISP_TEXT_EN(LogUid,               Packet,    "[UID] {0} {1}")

// ---- protocol: calibration ----
NEWISP_TEXT_EN(LogTargetUnknown,     Warning,   "[{0}] 未能确定目标频率，跳过")
NEWISP_TEXT_EN(LogHidNoBaudCalib,    Normal,    "[{0}] HID 传输无需校准波特率")
NEWISP_TEXT_EN(LogHidTargetFreq,     Normal,    "[{0}] 目标频率 {1} kHz（写入选项字节）")
NEWISP_TEXT_EN(LogBaudRateFailed,    Error,     "[{0}] 目标波特率无法设置")
NEWISP_TEXT_EN(LogBaudSwitchFailed,  Error,     "[{0}] 切波特率失败")
NEWISP_TEXT_EN(LogResponseBad,       Error,     "[{0}] {1} 响应错")
NEWISP_TEXT_EN(LogNoResponse,        Error,     "[{0}] {1} 无响应")
NEWISP_TEXT_EN(LogBaudSwitchOk,      Normal,    "[{0}] 切波特率成功")
NEWISP_TEXT_EN(LogPingTimeout,       Error,     "[{0}] ping-pong 超时")
NEWISP_TEXT_EN(LogPingBadReply,      Error,     "[{0}] ping-pong 响应错")
NEWISP_TEXT_EN(LogCoarseNoResponse,  Error,     "[{0}] 粗扫无响应")
NEWISP_TEXT_EN(LogCoarseReadFailed,  Error,     "[{0}] 粗扫读包失败")
NEWISP_TEXT_EN(LogCoarseBadLength,   Error,     "[{0}] 粗扫响应长度或首字节错")
NEWISP_TEXT_EN(LogCoarseSameCount,   Error,     "[{0}] 粗扫失败：ca 等于 cb")
NEWISP_TEXT_EN(LogCoarseTrimOutOfRange, Error,  "[{0}] 编程 trim 越界")
NEWISP_TEXT_EN(LogCoarseRangeInvalid, Error,    "[{0}] 细扫区间无效")
NEWISP_TEXT_EN(LogCoarseStartOutOfRange, Error, "[{0}] 细扫起点越界")
NEWISP_TEXT_EN(LogFineNoResponse,    Error,     "[{0}] 细扫无响应")
NEWISP_TEXT_EN(LogFineReadFailed,    Error,     "[{0}] 细扫读包失败")
NEWISP_TEXT_EN(LogFineBadLength,     Error,     "[{0}] 细扫响应长度或首字节错")
NEWISP_TEXT_EN(LogRound1NoResponse,  Error,     "[{0}] 第一轮无响应")
NEWISP_TEXT_EN(LogRound1ReadFailed,  Error,     "[{0}] 第一轮读包失败")
NEWISP_TEXT_EN(LogRound1BadReply,    Error,     "[{0}] 第一轮响应异常")
NEWISP_TEXT_EN(LogRound2NoResponse,  Error,     "[{0}] 第二轮无响应")
NEWISP_TEXT_EN(LogRound2ReadFailed,  Error,     "[{0}] 第二轮读包失败")
NEWISP_TEXT_EN(LogRound2ShortReply,  Error,     "[{0}] 第二轮响应长度不足")
NEWISP_TEXT_EN(LogRangeSelectFailed, Error,     "[{0}] 选择粗扫区间失败")
NEWISP_TEXT_EN(LogExternalBaudOutOfRange, Error, "[{0}] 外部时钟切波特率越界")
NEWISP_TEXT_EN(LogHostBaudSwitchFailed, Error,  "[{0}] 主机切波特率失败")
NEWISP_TEXT_EN(LogHostBaudSwitched,  Normal,    "[{0}] 主机已切到 {1}")
NEWISP_TEXT_EN(LogTrimUserProg,      Normal,    "[{0}] 用户 trim={1} range=0x{2}，编程 trim={3} range=0x{4}")
NEWISP_TEXT_EN(LogTrimFinal,         Normal,    "[{0}] 最终用户 trim={1} range=0x{2}，编程 trim={3} range=0x{4}")
NEWISP_TEXT_EN(LogTrimSummary,       Normal,    "[{0}] 成功 trim_adj={1}, trim_range=0x{2}, trim_div={3}, trim_freq={4} Hz")
NEWISP_TEXT_EN(LogPulseStats,        Normal,    "[{0}] pulse 0xFE 返回={1}, 耗时={2} ms")
NEWISP_TEXT_EN(LogRxBytes,           Normal,    "[{0}] {1} 字节: {2}")
NEWISP_TEXT_EN(LogNoBytes,           Normal,    "无")
NEWISP_TEXT_EN(LogBandBoundaries,    Normal,    "[{0}] 频段边界: {1} / {2} / {3} / {4} / {5} kHz")
NEWISP_TEXT_EN(LogBandChosen,        Normal,    "[{0}] 选择频带 {1} (band=0x{2})")
NEWISP_TEXT_EN(LogOscParams,         Normal,    "[{0}] 振荡器分频={1}, trim={2}, 频带=0x{3}")
NEWISP_TEXT_EN(LogBestTrim,          Normal,    "[{0}] 最优 idx={1}, trim={2}, band=0x{3}, 实际 {4} MHz, 误差 {5}%")
NEWISP_TEXT_EN(LogCalibrationOk,     Normal,    "[{0}] 校准成功")

// ---- protocol: erase / write / options failures ----
NEWISP_TEXT_EN(LogEraseTimeout,      Error,     "[{0}] 擦除超时")
NEWISP_TEXT_EN(LogEraseBadReply,     Error,     "[{0}] 擦除包错误")
NEWISP_TEXT_EN(LogBlockTimeout,      Error,     "[{0}] 第 {1} 块超时")
NEWISP_TEXT_EN(LogBlockBadReply,     Error,     "[{0}] 第 {1} 块响应异常")
NEWISP_TEXT_EN(LogBlockProgress,     Normal,    "[{0}] {1}/{2} 块")
NEWISP_TEXT_EN(LogFinishTimeout,     Error,     "[{0}] 结束包超时")
NEWISP_TEXT_EN(LogFinishBadReply,    Error,     "[{0}] 结束包响应异常")
NEWISP_TEXT_EN(LogOptionsTimeout,    Error,     "[{0}] 选项超时")
NEWISP_TEXT_EN(LogOptionsBadReply,   Error,     "[{0}] 选项响应异常")
NEWISP_TEXT_EN(LogOptionsTailTimeout, Error,    "[{0}] 选项收尾包超时")
NEWISP_TEXT_EN(LogOptionsTailBadReply, Error,   "[{0}] 选项收尾包响应异常")
NEWISP_TEXT_EN(LogOptionsFreqUnsupported, Error, "[{0}] 目标频率 {1} Hz 不在支持的 IRC 频率内，已中止写入")
NEWISP_TEXT_EN(LogOptionsFreqList,   Error,     "[{0}] 支持频率 (MHz): {1}")
NEWISP_TEXT_EN(LogOptionsTrimUnmeasured, Warning, "[{0}] 警告：无校准值与查表项，trim 保持占位值")
NEWISP_TEXT_EN(LogOptionsTrimTable,  Normal,    "[{0}] 使用查表 trim（未校准）: freq={1} Hz, trim={2} {3} {4}")
NEWISP_TEXT_EN(LogOptionsEeprom,     Normal,    "[{0}] EEPROM={1}, 总容量={2}, opts[39]=0x{3} (code={4})")
NEWISP_TEXT_EN(LogOptionsWritten,    Normal,    "[{0}] 频率={1} Hz, count=0x{2}, vrtrim=0x{3}, trim={4} {5} {6}, EEPROM={7}, opts[39]=0x{8}")
NEWISP_TEXT_EN(LogOptionsMsr,        Normal,    "[{0}] MSR={1} (时钟源={2}, 看门狗={3})")
NEWISP_TEXT_EN(LogOptionsMsrClock,   Normal,    "[{0}] MSR={1} (时钟源={2})")
NEWISP_TEXT_EN(LogClockSourceExternal, Normal,  "外部")
NEWISP_TEXT_EN(LogClockSourceInternal, Normal,  "内部")
NEWISP_TEXT_EN(LogWatchdogOff,       Normal,    "关")
NEWISP_TEXT_EN(LogWatchdogOn,        Normal,    "开")

// ---- CLI: device listing ----
NEWISP_TEXT_EN(CliNoSerialPorts,     Normal,    "未发现串口设备")
NEWISP_TEXT_EN(CliSerialPorts,       Normal,    "串口设备 ({0}):")
NEWISP_TEXT_EN(CliHidNotBuilt,       Normal,    "本版本未编译 USB HID 支持")
NEWISP_TEXT_EN(CliStcIspDevices,     Normal,    "STC ISP 设备 (VID {0} PID {1}): {2} 个")
NEWISP_TEXT_EN(CliOtherStcHid,       Normal,    "其它 STC HID 设备 (VID {0}，不含 ISP): {1} 个")
NEWISP_TEXT_EN(CliNoStcDevices,      Normal,    "未发现 STC 设备。若芯片已按 BOOT 上电，可用 -v 列出全部 HID 设备排查。")
NEWISP_TEXT_EN(CliAllHidDevices,     Normal,    "全部 HID 设备: {0} 个（含鼠标、键盘等，仅供排查）")

// ---- CLI: no-command prompt ----
NEWISP_TEXT_EN(CliInvite,            Normal,    "运行 '{0} help' 查看用法，或运行 '{0} list' 列出可用设备")

// ---- CLI: device selection errors ----
NEWISP_TEXT_EN(CliErrNoHidSupport,   Error,     "本版本未编译 USB HID 支持，请使用 --serial 或重新编译")
NEWISP_TEXT_EN(CliErrNoIspDevice,    Error,     "未发现 STC USB ISP 设备（请按住 BOOT 键重新上电）")
NEWISP_TEXT_EN(CliErrManyIspDevices, Error,     "发现多个 STC USB ISP 设备，请用 --device 指定：")
NEWISP_TEXT_EN(CliErrNoSerialPorts,  Error,     "未发现串口设备")
NEWISP_TEXT_EN(CliErrManySerial,     Error,     "发现多个串口设备，请用 --device 指定：")

// ---- CLI: argument errors ----
NEWISP_TEXT_EN(CliErrPrefix,         Error,     "错误: {0}")
NEWISP_TEXT_EN(CliErrInspectOption,  Error,     "inspect 是离线命令；不允许编程选项")
NEWISP_TEXT_EN(CliErrDetectOption,   Error,     "detect 只负责识别芯片；-freq、-eeprom、-clock、-series 等编程选项不适用")
NEWISP_TEXT_EN(CliErrNeedsValue,     Error,     "选项 {0} 需要一个值")
NEWISP_TEXT_EN(CliErrBadBaud,        Error,     "无法解析波特率: {0}")
NEWISP_TEXT_EN(CliErrBadFreq,        Error,     "无法解析频率: {0}")
NEWISP_TEXT_EN(CliErrBadEeprom,      Error,     "无法解析 EEPROM 大小: {0}")
NEWISP_TEXT_EN(CliErrBadClock,       Error,     "--clock 只接受 internal 或 external，收到: {0}")
NEWISP_TEXT_EN(CliErrBadOutput,      Error,     "--output 只接受 quiet、normal 或 verbose，收到: {0}")
NEWISP_TEXT_EN(CliErrBadSeries,      Error,     "未知系列: {0}")
NEWISP_TEXT_EN(CliErrUnknownCommand, Error,     "未知命令: {0}")
NEWISP_TEXT_EN(CliErrUnknownOption,  Error,     "未知选项: {0}")
NEWISP_TEXT_EN(CliErrUnknownOptionHint, Error,  "未知选项: {0}（是不是想用 {1}？）")
NEWISP_TEXT_EN(CliErrExtraArgument,  Error,     "多余的参数: {0}")
NEWISP_TEXT_EN(CliErrNeedFile,       Error,     "{0} 需要在子命令后紧跟固件文件：newisp {0} FILE [选项]")
NEWISP_TEXT_EN(CliErrNeedFileBurn,   Error,     "burn 需要在子命令后紧跟固件文件：newisp burn FILE [选项] [设备]")
NEWISP_TEXT_EN(CliErrInspectDevice,  Error,     "inspect 是离线命令；不允许设备/编程选项")
NEWISP_TEXT_EN(CliErrFreqTooHigh,    Error,     "目标频率 {0} Hz 高于所有系列的频率表上限 (最高 {1} MHz)")
NEWISP_TEXT_EN(CliErrFreqNotInMenu,  Error,     "{0} MHz 不在 {1} 的频率表内，未写入任何内容")
NEWISP_TEXT_EN(CliErrFreqNoTrim,     Error,     "{0} MHz 在频率表内，但还没有对应的 trim 三元组，未写入任何内容")
NEWISP_TEXT_EN(CliErrNoFreqForSeries, Error,    "{0} 没有可指定的频率：它由时钟源决定，-freq 不适用")
NEWISP_TEXT_EN(CliErrNoEepromForSeries, Error,  "{0} 的 EEPROM 大小不可指定：它的选项字节没有这个字段，-eeprom 不适用")
NEWISP_TEXT_EN(CliErrNoClockForSeries, Error,   "{0} 没有可指定的时钟源：只有 STC10/11/12 有这一位，-clock 不适用")


// ---- CLI: inspect ----
NEWISP_TEXT_EN(InspectValid,         Normal,    "Intel HEX: 有效；{0} 个数据字节")
NEWISP_TEXT_EN(InspectRange,         Normal,    "  0x{0}-0x{1} : {2} 字节")
NEWISP_TEXT_EN(InspectEntry,         Normal,    "入口元数据: 类型 {0}，值 0x{1}")
NEWISP_TEXT_EN(InspectAddressMap,    Normal,    "地址映射: 16 位线性；目标容量/选项仍需校验")
NEWISP_TEXT_EN(InspectUnavailable,   Normal,    "无法烧录: {0}")
NEWISP_TEXT_EN(InspectOffline,       Normal,    "仅离线检查；未枚举、未打开、未复位、未写入任何设备")

// ---- CLI: run header and summary ----
NEWISP_TEXT_EN(DeviceLine,           Normal,    "设备       : {0}")
NEWISP_TEXT_EN(TransportLine,        Normal,    "传输方式   : {0}")
NEWISP_TEXT_EN(TransportSerial,      Normal,    "串口")
NEWISP_TEXT_EN(FirmwareLine,         Normal,    "固件       : {0}（{1} 字节，地址 0x{2}-0x{3}）")
NEWISP_TEXT_EN(TargetFreqLine,       Normal,    "目标频率   : {0} MHz")
NEWISP_TEXT_EN(BaudLine,             Normal,    "波特率     : {0}")
NEWISP_TEXT_EN(EepromLine,           Normal,    "EEPROM     : {0} 字节")
NEWISP_TEXT_EN(ProgressWrite,        Normal,    "写入进度: {0}%")
NEWISP_TEXT_EN(CliFirmwareEmpty,     Error,     "错误: 固件为空")
NEWISP_TEXT_EN(BurnSucceeded,        Normal,    "烧录成功")
NEWISP_TEXT_EN(BurnFailed,           Error,     "烧录失败")

// ---- CLI: version banner ----
NEWISP_TEXT_EN(VersionProtocols,     Normal,    "协议: STC89/90/10/11/12/15/8/8G/8H/32/8051U")
NEWISP_TEXT_EN(VersionTransports,    Normal,    "传输: 串口")
NEWISP_TEXT_EN(VersionTransportsHid, Normal,    "传输: 串口 + USB HID")

// ---- CLI: usage text ----
NEWISP_TEXT_EN(UsageTitle,           Normal,    "newisp {0} -- STC 单片机烧录工具（串口 + USB HID）")
NEWISP_TEXT_EN(UsageUsage,           Normal,    "用法:")
NEWISP_TEXT_EN(UsageCmdNone,         Normal,    "列出设备，然后显示本帮助")
NEWISP_TEXT_EN(UsageCmdHelp,         Normal,    "显示本帮助")
NEWISP_TEXT_EN(UsageCmdVersion,      Normal,    "版本，以及本版本支持的传输方式")
NEWISP_TEXT_EN(UsageCmdList,         Normal,    "列出串口和 HID 设备")
NEWISP_TEXT_EN(UsageCmdDetect,       Normal,    "探测并识别芯片")
NEWISP_TEXT_EN(UsageCmdInspect,      Normal,    "校验 HEX 并显示地址范围；不访问设备")
NEWISP_TEXT_EN(UsageCmdBurn,         Normal,    "擦除、写入、写选项字节")
NEWISP_TEXT_EN(UsageTransport,       Normal,    "传输方式:")
NEWISP_TEXT_EN(UsageSerial,          Normal,    "用串口 ISP 监视器（默认）")
NEWISP_TEXT_EN(UsageHid,             Normal,    "用出厂 USB HID ISP 接口")
NEWISP_TEXT_EN(UsageDevice,          Normal,    "设备:")
NEWISP_TEXT_EN(UsageDeviceName,      Normal,    "COM7、/dev/ttyUSB0，或 HID 接口路径")
NEWISP_TEXT_EN(UsageDeviceDefault,   Normal,    "（默认：唯一找到的设备，否则给出")
NEWISP_TEXT_EN(UsageDeviceDefault2,  Normal,    "列出候选设备的友好错误）")
NEWISP_TEXT_EN(UsageProgramming,     Normal,    "编程:")
NEWISP_TEXT_EN(UsageSerialBaud,      Normal,    "BAUD：握手波特率，默认 115200")
NEWISP_TEXT_EN(UsageSerialBaudForms, Normal,    "如 -s 921600、-s 115.2k；仅串口有效")
NEWISP_TEXT_EN(UsageFreq,            Normal,    "目标 IRC 频率（默认 24 MHz）")
NEWISP_TEXT_EN(UsageFreqForms,       Normal,    "支持 24000000、24M、11.0592M；")
NEWISP_TEXT_EN(UsageFreqForms2,      Normal,    "须为该系列频率表中的档位")
NEWISP_TEXT_EN(UsageEeprom,          Normal,    "EEPROM 分割，如 4K（0 = 保留芯片设置）")
NEWISP_TEXT_EN(UsageClockSrc,        Normal,    "STC10/11/12 选项字节的时钟源")
NEWISP_TEXT_EN(UsageSeries,          Normal,    "探测无法识别时的兜底系列：")
NEWISP_TEXT_EN(UsageSeriesNames,     Normal,    "stc89、stc12、stc15、stc8g、")
NEWISP_TEXT_EN(UsageSeriesNames2,    Normal,    "stc32、stc8h、stc8a、stc8")
NEWISP_TEXT_EN(UsageSeriesFull,      Normal,    "必须全拼，不能缩写成 -ser：")
NEWISP_TEXT_EN(UsageOutput,          Normal,    "输出:")
NEWISP_TEXT_EN(UsageLangBuild,       Normal,    "输出语言在编译时选定，")
NEWISP_TEXT_EN(UsageLangBuildSet,    Normal,    "运行时不可切换：配置时用")
NEWISP_TEXT_EN(UsageLangBuildThis,   Normal,    "NEWISP_LANG=en 或 zh-CN。本版本为 ")
NEWISP_TEXT_EN(UsageOutputFlag,      Normal,    "显示多少日志")
NEWISP_TEXT_EN(UsageOutputLevels,    Normal,    "quiet：只显示错误和最终结果；")
NEWISP_TEXT_EN(UsageOutputDefault,   Normal,    "normal（默认）或 verbose（含原始报文）")
NEWISP_TEXT_EN(UsageGeneral,         Normal,    "通用:")
NEWISP_TEXT_EN(UsageVersion,         Normal,    "显示版本")
NEWISP_TEXT_EN(UsageExamples,        Normal,    "示例:")
NEWISP_TEXT_EN(UsageExampleHid,      Normal,    "插入时按住 BOOT")
NEWISP_TEXT_EN(UsageNotes,           Normal,    "说明:")
NEWISP_TEXT_EN(UsageNoteSerial,      Normal,    "串口：工具会脉冲 DTR 并持续发送唤醒字节，因此能捕获仍在运行")
NEWISP_TEXT_EN(UsageNoteSerial2,     Normal,    "用户程序的芯片。若板子没有自动复位电路，请在握手期间断电重上电。")
NEWISP_TEXT_EN(UsageNoteSerial3,     Normal,    "的芯片。若板子没有自动复位电路，请在握手期间断电重上电。")
NEWISP_TEXT_EN(UsageNoteHid,         Normal,    "HID：插入板子时把 BOOT 引脚拉低。USB 通道没有自动复位。")
NEWISP_TEXT_EN(UsageNoteHid2,        Normal,    "没有自动复位。")
NEWISP_TEXT_EN(UsageNote8051u,       Normal,    "8051U 系列完全没有串口 ISP 监视器，只能通过 HID 编程。")
NEWISP_TEXT_EN(UsageNote8051u2,      Normal,    "编程。")