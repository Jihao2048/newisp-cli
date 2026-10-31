# 未提交改动总结

基于工作区当前未提交状态（`git diff` + 未跟踪文件）整理。

- 已修改文件：36 个，+2868 / −1099
- 未跟踪新增：10 个文件 / 约 250 行消息表条目（详见第九节清单）

---

## 一、核心主线

这次的改动可以归纳为五条主线，其余都是它们的连带调整：

| # | 主线 | 一句话概括 |
|---|---|---|
| 1 | **编译期双语** | 新增完整的消息表与 `i18n` 层，语言由 `NEWISP_LANG` 在 configure 时选定，二进制只含一种语言 |
| 2 | **按系列区分的 IRC 频率表** | 全局单一频率表拆成每系列一张表，频率必须命中该系列菜单且有 trim 才能写入 |
| 3 | **命令行接口重构** | 固件改为位置参数；选项名大小写不敏感 + 唯一前缀；`-help/-version` 变为主命令；`-q/-v` 合并为 `-output` |
| 4 | **能力校验取代一刀切拒绝** | 移除 STC32 禁止烧录、8051U 串口限制、`-force-serial` 等规则，改为按系列校验 `-freq/-eeprom/-clock` 是否适用 |
| 5 | **HID 变为强制依赖** | 删除 `NEWISP_ENABLE_HID`，macOS/Linux 缺 hidapi 直接 configure 失败 |

---

## 二、国际化的编译期消息表（主线 1）

### 新增文件

| 文件 | 作用 |
|---|---|
| `include/newisp/i18n.h` | 消息枚举 `Msg`、`Tr()` 格式化、日志类型 `LogKind`、日志标记读写 |
| `include/newisp/text_en.inc` | 英文原文表，同时是**基准表**（同时也是 `Msg` 枚举的唯一来源） |
| `src/i18n/text_zh_cn.inc` | 中文译文表，条目与英文表一一对应 |
| `src/core/i18n.cpp` | 按编译宏选择一份语言表；提供 `BuildLanguage()`、`BuildVersion()`、`Tr()`、`SplitLogLine()` 等 |
| `tests/test_i18n.cpp` | 校验译文保留 `{n}` 占位符、无漏译、自报语言与实际输出一致、日志标记经翻译后仍成立 |

消息条目数：**250 条**（英文表与中文表各 250 条）。

### 设计要点

- 仅被选中语言的表编入二进制，另一份语言根本不在可执行文件里；因此**同一二进制不能在运行期切换语言**，要两种语言就 configure 两次。
- 消息标识符按英文原文命名（`Msg::EraseOk` ≈ "Erase complete"），便于中英表并排校对。
- 日志类型随行携带：`%p`（报文转储）与 `%a`（需要注意的结论）作为前缀标记，打印前剥离。这取代了原先按**中文前缀**过滤日志的写法，从而与语言无关。

### 受影响的存量代码

- `src/cli/main.cpp`：`Logger` 由 `quiet/verbose` 两个布尔改为单一 `OutputLevel`，并改用 `SplitLogLine`/`StripLogMarker`；进度条、设备列表、错误提示全部走 `Tr()`。
- `src/cli/options.cpp`：`--help` 文本整段改为从消息表拼装，中英结构完全一致，仅文字不同。
- `src/cli/session.cpp`、全部 `include/newisp/protocol_*.h`：所有硬编码中文日志替换为 `I18n::Tr(Msg::...)`。
- `include/newisp/stc_protocol.h`：新增 `namespace stc::I18n { using namespace ::newisp::i18n; }`（MSVC 不接受全局命名空间外的 namespace alias）。

---

## 三、按系列区分的 IRC 频率表（主线 2）

`include/newisp/chip_logic.h` 是本次改动最大的头文件（+470 行）。

### 表结构变化

删除全局 `kFreqTable[10]` / `kNumFreq`，改为六张独立表：

| 系列 | 条目数 | 上限 |
|---|---|---|
| `kFreqTableStc32G144K246` | 15（厂商菜单 17，2 档无 trim） | 60.000 MHz |
| `kFreqTableStc8051U` | 16 | 45.1584 MHz |
| `kFreqTableStc8H` | 16 | 45.1584 MHz |
| `kFreqTableStc8A` | 16（末档 45.000，与 8H 唯一差别） | 45.000 MHz |
| `kFreqTableStc8G` | 12 | 35.000 MHz |
| `kFreqTableStc15` | 12 | 35.000 MHz |

新增 `FrequencyMenuFor()`（返回 `{hz, count, maxIdx}`）、`IsFrequencyInMenu()`、`HasTrimFor()`、`ListFrequencyMenu()`。

### 规则变化

- **原表的三处错误被修正**：带上不属于任何菜单的频率（33.1776、22.1184）、上限卡在 45 MHz 导致 8H/8051U 无法选到 45.1584、以及漏掉六个系列里五个都有的 27.000。
- 选择判定从「范围判断」改为「**菜单成员判断**」：45.100 落在 8H 范围内但不是菜单档位，仍拒绝。
- 8A 命名陷阱：`STC8A8K64S4A12` / `STC8A2K64D4` 是 8A 硅片（45.000），`STC8A8K64D4` 是 8H 硅片（45.1584），靠部件号而非前缀区分。
- `kClockSrcText` 改为英文并标注废弃，新增 `ClockSourceText(int)`；`kUnknownChipName` 保留为英文常量，新增 `UnknownChipName()`。

### 相关文件

- `src/core/chip_table.cpp`：新增 `ListFrequencyMenu()` 实现；未知型号占位符改为走消息表。
- `protocol_stc8_hid.h` / `protocol_stc8_uart.h`：trim 表新增 `45.1584 MHz → (0xA9, 0x30, 1)`；选项阶段拒绝时列出**该系列真实菜单**，不再维护第二份硬编码清单。
- `src/cli/session.cpp`：`ClampFrequency` → `ResolveFrequency`，区分「超出上限（下调并提示）」与「不在菜单（拒绝）」两种错误；另有 `HasTrimFor` 的前置拒绝。
- 新增文档 `docs/irc-frequency-tables.md`：逐系列整理 AiCube-ISP 下拉菜单、截图来源、省略条目的理由。

---

## 四、命令行接口重构（主线 3）

`src/cli/options.cpp` 改动 +835 行，是本次最大的单文件改动。

### 选项表与匹配规则

新增 `OptionDef` 表，定义每个选项的全部拼写与匹配方式（`Literal` / `Prefix`）：

| 选项 | 拼写 | 短形式 |
|---|---|---|
| 串口 | `-serial`（Literal） | `-s`，可带可选波特率 `-s 921600` |
| HID | `-hid` | `-h` |
| 输出 | `-output` | `-o` |
| 设备 | `-device` | `-d` |
| 频率 | `-freq`（Literal，别名 `-f`、`-frequency`、`-F`） | `-f` |
| EEPROM | `-eeprom` | `-e` |
| 时钟源 | `-clock` | `-c` |
| 系列 | `-series`（Literal，必须全拼，无短形式） | — |

- 选项名**大小写不敏感**，长名接受**唯一前缀缩写**（`-ee` → `-eeprom`）。
- `-serial`、`-series`、`-freq` 标记为 Literal：分别避免与 `-series` 撞首字母、避免 `-frequncy` 之类拼写错误静默命中。
- 未知选项用 **Levenshtein 距离**给建议（`"did you mean -freq"`），但只提示不自动替换。

### 命令与位置参数

- **固件改为位置参数**：`newisp burn app.hex`、`newisp inspect app.hex`；删除 `-f` / `--file`、`-b` / `--baud`。
- `-f` 现在表示频率，`-h` 表示 HID，`-H` 不再使用。
- `-help` / `-version` 不再是选项，改为**主命令** `help` / `version`。
- `list` 与 `detect` 不接受编程选项（频率、EEPROM、时钟、系列），在解析阶段即拒绝。
- 无参数运行不再打印设备列表 + 完整帮助，只给一行提示指向 `help` 与 `list`。

### 输出级别

- `-q/-v` 合并为单一 `-output`：`quiet` / `normal`（默认）/ `verbose`，另有 `q/n/v/default` 等短写法。
- 输出重定向且未显式指定 `-output` 时，自动按 `verbose` 处理（报文转储留在日志里方便 grep）。

---

## 五、能力校验取代一刀切拒绝（主线 4）

`chip_logic.h` 新增四个谓词，`session.cpp` 与 `options.cpp` 各调用一次：

| 谓词 | 含义 |
|---|---|
| `HasSelectableFrequency()` | STC12/89 没有频率菜单，`-freq` 被拒 |
| `HasSelectableEeprom()` | 仅 8/32 代有 EEPROM 分割字段，STC15/12/89 拒 `-eeprom` |
| `HasSelectableClockSource()` | 仅 STC10/11/12 有时钟源位，其余拒 `-clock` |
| `IsUsbCapableChip()` | 仅 8H / STC32 / 8051U 含 USB 控制器 |

同时**移除**的旧规则：

- `ValidateProgrammingTarget` 中「STC32 禁止烧录」的整段拒绝 → 只按容量（≤64 KiB）+ EEPROM 分割校验。
- `IsUnsupportedForTransport` 的串口闸门与 `allowHidOnlySerial` / `-force-serial` → 完全删除，探测本身即判定依据。
- `IsHidReachableProto` 由「8G 或 32」收窄为「仅 32 槽位」。
- chip 表删除 STC16 三条型号及 `protocol_factory.h` 中的 STC16 分支。
- `hex_file.cpp` 报错文案从「banked firmware 不支持」改为「超出 0xFFFF 的 16 位块地址无法表示」。

---

## 六、HID 强制化与构建系统（主线 5）

### `CMakeLists.txt`

- 删除 `NEWISP_ENABLE_HID` 选项；`NEWISP_HAVE_HID=1` 恒为 PUBLIC 定义。
- macOS/Linux 缺 hidapi 时由 `message(STATUS ...)` 降级改为 `message(FATAL_ERROR ...)` 并给出安装提示。
- 新增 `NEWISP_LANG` 缓存变量（`zh-CN` 默认 / `en`），转成 `NEWISP_LANG_CODE_CHINESE` / `NEWISP_LANG_CODE_ENGLISH` 私有宏。
- `src/core/i18n.cpp` 加入 `newisp_core`，并对其设置 `OBJECT_DEPENDS` 依赖两个 `.inc` 文件。
- `NEWISP_VERSION` 定义从可执行目标移到 `newisp_core`。
- 文件头出现 BOM（`CMakeLists.txt` 首行）。

### `include/newisp/hid_channel.h`

- 默认值 `NEWISP_HAVE_HID` 由 0 改为 1，并加 `#error` 防止被关掉。

### `src/transport/serial_enum.cpp`

- `SerialPortInfo` 新增 `portName` / `vid` / `pid` 字段。
- Windows 下从设备实例 ID 解析 VID/PID（手写十六进制解析，避免引入 wide CRT）。
- 新增 `RefreshSerialPortIdentity()`：把 generic usbser 的本地化名字升级为 HID 侧上报的 USB 产品名，输出形如 `STC USB Serial (COM12)`。

### 工程配置

- `.gitignore`：`.vscode/` 改为白名单（仅跟踪 `settings.json`）；新增忽略 `scripts/build-variant.cmd`、`scripts/reset-cmake-preset.ps1`、`CMakePresets.json` 三个被取代的辅助文件。
- 新增未跟踪 `.vscode/`：`settings.json`（固定生成器与 `NEWISP_LANG`，不用 preset）、`tasks.json`（构建任务，Ctrl+Shift+B 选语言与构建类型）、`c_cpp_properties.json`。
- 新增 `scripts/wsl-install.sh`（构建到交互用户可读的位置）、`scripts/wsl-env-report.sh`（报告 WSL 构建环境）。
- `scripts/wsl-build.sh` / `scripts/wsl-smoke.sh`：支持语言参数、独立构建目录、`WSL_BUILD_DIR` 覆盖；冒烟用例同步改为新 CLI（`-version`、`-help`、`-s/-serial/-SERIAL`、`-device`）。

---

## 七、测试与文档

### 测试

| 文件 | 变化 |
|---|---|
| `tests/test_i18n.cpp` | 新增：语言自报、占位符对齐、无漏译、日志标记 |
| `tests/test_options.cpp` | 新增：位置参数、`-series` 全拼、`-s` 归属、唯一前缀、`-output` 单一设置、频率四种拼写、拼写建议 |
| `tests/test_chip_logic.cpp` | +265 行：逐系列上限与条目数、菜单互异性（45.000 vs 45.1584）、菜单与 trim 表一致性 |
| `tests/test_chip_table.cpp` | 未知占位符改比对 `UnknownChipName()` |
| `tests/test_hex_file.cpp` | 断言由 `"banked"` 改为 `"0xFFFF"` |
| `tests/cli_preflight_test.py` | 断言改为只依赖退出码 / 十六进制 / 字节数等非译文字符串；临时目录改到构建树旁（沙箱友好）；新增位置参数用例 |
| `tests/pty_preflight_test.py` | **语义反转**：原断言「STC32 被拒」→ 现断言「STC32 端到端烧录成功」 |
| `tests/CMakeLists.txt` | 加入两个新测试源；`test_options.cpp` 直接编译 CLI 的 `options.cpp`；所有 CTest 条目设 `PYTHONUTF8=1`（中文 Windows 代码页 936 解码 UTF-8 会抛 `UnicodeDecodeError`） |

### 文档

- `README.md` / `README.zh-CN.md`：型号支持表重写（STC8 系列仅串口、STC32 改为串口+HID）、删除 8051U「仅 HID」与 45 MHz 上限的旧结论、新增「输出语言」章节、选项表按新 CLI 全面重写、测试说明补充 i18n 与 `PYTHONUTF8`。
- `docs/macos-build.md`：示例命令更新为新 CLI。
- `docs/irc-frequency-tables.md`（新增）：频率表来源与逐条取舍。
- `README.zh-CN.md` 末尾去掉了结尾换行（`\ No newline at end of file`）。

---

## 八、需要注意的行为变化（兼容性）

1. **`-f` 含义变了**：从「固件文件」变成「频率」。旧脚本里的 `burn -f app.hex` 会直接解析失败。
2. **`-h` 现在是 HID**，不再是 help；`-V` 不再是 version。
3. **`-family` 改名为 `-series`**，且必须全拼。
4. **`-b/--baud` 删除**，改用 `-s <rate>`。
5. **`-q/-v` 删除**，改用 `-output quiet|verbose`。
6. **无参数运行不再输出设备列表**。
7. **HID 无法再关闭**，macOS/Linux 必须装 hidapi 才能 configure。
8. **STC16 型号从芯片表移除**：仍能探测并报出 ID，但不再有型号名。
9. **STC32 现在可以烧录**（≤64 KiB），此前是明确拒绝。
10. **语言不在运行期可切换**，需分别构建。

---

## 九、附：文件改动清单

### 已修改（36）

```
.gitignore  CMakeLists.txt  README.md  README.zh-CN.md  docs/macos-build.md
include/newisp/{chip_logic,chip_table,hid_channel,options,protocol_factory,
                protocol_stc12,protocol_stc15,protocol_stc89,protocol_stc8_hid,
                protocol_stc8_uart,serial_channel,session,stc_protocol}.h
scripts/{wsl-build,wsl-smoke}.sh
src/cli/{main,options,session}.cpp
src/core/{chip_table,hex_file}.cpp
src/transport/{hid_channel,serial_enum}.cpp
tests/{CMakeLists.txt,cli_preflight_test.py,pty_opts_dump.py,pty_preflight_test.py,
       pty_test.py,test_chip_logic.cpp,test_chip_table.cpp,test_hex_file.cpp,test_main.cpp}
```

### 未跟踪新增（10）

```
.vscode/{settings,tasks,c_cpp_properties}.json
docs/irc-frequency-tables.md
include/newisp/{i18n.h,text_en.inc}
scripts/{wsl-env-report,wsl-install}.sh
src/core/i18n.cpp
src/i18n/text_zh_cn.inc
tests/{test_i18n.cpp,test_options.cpp}
```
