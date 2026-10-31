# 近三次提交总结（b606b3a → ff9a8dc → ef306c8）

整理时间：2026-10-31

| 顺序 | 提交 | 日期 | 主题 | 规模 |
|---|---|---|---|---|
| 旧 → 新 1 | `b606b3a` | 2026-10-25 14:59 | fix(protocol_stc8_hid)：改进频率 trim 处理与校验 | 2 文件，+47 / −15 |
| 旧 → 新 2 | `ff9a8dc` | 2026-10-31 20:17 | 编译期双语消息表、逐系列 IRC 频率菜单、CLI 全面重构 | 46 文件，+5149 / −1098 |
| 旧 → 新 3 | `ef306c8` | 2026-10-31 21:38 | 选项处理重构：不再擅自改动用户未指定的参数 | 12 文件，+1789 / −972 |

三次提交合计：约 57 个文件改动，**+6985 / −2085**。主线只有一条：**从"尽量替用户猜"转向"只做用户明确要求的事，其余一律保持芯片原状"**。

---

## 一、一句话概括

| 提交 | 一句话 |
|---|---|
| `b606b3a` | 频率没有对应 trim 三元组时**宁可中止**，也不再拿芯片时钟凑数；并把可写 trim 表提升为单一数据源 |
| `ff9a8dc` | 把散落的中文字符串收进**编译期双语消息表**；频率表按系列拆分；CLI 改为「位置参数固件 + 大小写不敏感/唯一前缀选项 + 按能力校验」 |
| `ef306c8` | 修掉最后一类隐式改写：默认输出改 `quiet`、未给 `-freq` 保持芯片频率、未给 `-eeprom` 保持原分割、HID 下 `detect` 不复位 |

演进关系：

```
b606b3a  「频率不该被猜」          —— 协议层先立规矩
ff9a8dc  「所有文案与频率菜单集中」 —— 基础设施 + CLI 重构
ef306c8  「所有默认值不该改写芯片」 —— 把规矩扩展到 output / freq / eeprom / detect
```

---

## 二、提交 1：`b606b3a` 频率 trim 严格化（+47 / −15）

> `fix(protocol_stc8_hid): improve frequency trim handling and validation`

### 变更

| 文件 | 改动 |
|---|---|
| `include/newisp/protocol_stc8_hid.h` | `kTable` → `kTrimTable`（提到类作用域的 `inline static`），使其成为「可用频率」的**单一数据源** |
| `include/newisp/protocol_stc8_hid.h` | `ProgramOptions` 删除回退逻辑，改为"无 trim 或不在支持列表即中止" |
| `include/newisp/protocol_stc8_hid.h` | 新增 `kHz[]` / `kText[]` 支持频率清单（17 档，5.5296–60.000 MHz），拒绝时整行列出 |
| `src/core/hex_file.cpp` | Windows 分支补 `#define NOMINMAX`，防止 `<windows.h>` 的 `min/max` 宏污染 `std::numeric_limits` |

### 行为变化

```cpp
// 之前：找不到 trim 就悄悄退回芯片当前时钟，照样写选项字节
if (!ft && m_status.clockHz) { freq = m_status.clockHz; ft = FindTrim(freq); }

// 之后：无 trim 或不在支持清单 → 报错中止，并列出可用频率
if (!ft || !supported) {
    log("目标频率 ... Hz 不在支持的 IRC 频率内，已中止写入");
    log("支持频率 (MHz): 5.5296, 6.000, ...");
    return false;
}
```

理由（代码注释）：写一个猜测的 trim 可能让 IRC 未校准、芯片无法起振 —— 宁可失败，也不要交付一颗跑不起来的芯片。

### 遗留问题（后续提交才修）

- 支持频率清单一度是**第二份硬编码**，与 `chip_logic.h` 的系列菜单重复维护；`ff9a8dc` 中改为复用 `ListFrequencyMenu()`。
- 日志文案仍为硬编码中文；`ff9a8dc` 统一迁到消息表。
- 该提交把 `protocol_stc8_hid.h` 与 `hex_file.cpp` 的文件末尾换行删掉了（diff 中的 `\ No newline at end of file`）。

---

## 三、提交 2：`ff9a8dc` 双语消息表 + 逐系列频率菜单 + CLI 重构（+5149 / −1098，46 文件）

这是三次提交中最大的一次，包含五条主线。

### 3.1 编译期双语消息表（i18n）

| 新增文件 | 作用 |
|---|---|
| `include/newisp/i18n.h` | `Msg` 枚举、`Tr()`、`LogKind`、日志标记读写、`SplitLogLine()` |
| `include/newisp/text_en.inc` | 英文基准表，**同时是 `Msg` 枚举的唯一来源** |
| `src/i18n/text_zh_cn.inc` | 中文译文表，与英文表逐条对应（各约 250 条） |
| `src/core/i18n.cpp` | 编译宏选一份表，提供 `BuildLanguage()` / `BuildVersion()` / `Tr()` |
| `tests/test_i18n.cpp` | 校验占位符 `{n}` 一致、无漏译、语言自报正确 |

设计要点：

- 语言在 **configure 时**由 `-DNEWISP_LANG=zh-CN|en` 选定，**只编译一种语言**，另一份不在二进制里；要双语需构建两次。
- 消息 id 按英文原文命名（如 `Msg::EraseOk`），中英两表可并排校对。
- 日志类型随行携带：`%p`（报文转储）、`%a`（需注意的结论）作为前缀标记，打印前剥离 —— **不再依赖中文前缀过滤日志**，从而与语言无关。
- `include/newisp/stc_protocol.h` 新增 `namespace stc::I18n { using namespace ::newisp::i18n; }`（MSVC 不接受全局命名空间外的 namespace alias）。

### 3.2 逐系列 IRC 频率菜单（`chip_logic.h` +470 行）

删除全局 `kFreqTable[10]`，拆成六张表：

| 系列 | 条目数 | 上限 |
|---|---|---|
| `kFreqTableStc32G144K246` | 15（厂商菜单 17，2 档无 trim） | 60.000 MHz |
| `kFreqTableStc8051U` | 16 | 45.1584 MHz |
| `kFreqTableStc8H` | 16 | 45.1584 MHz |
| `kFreqTableStc8A` | 16（末档 45.000，与 8H 唯一差别） | 45.000 MHz |
| `kFreqTableStc8G` | 12 | 35.000 MHz |
| `kFreqTableStc15` | 12 | 35.000 MHz |

新增 `FrequencyMenuFor()` / `IsFrequencyInMenu()` / `HasTrimFor()` / `ListFrequencyMenu()`。

规则变化：

- 修正原表三处错误：含不属于任何菜单的频率（33.1776、22.1184）、上限卡 45 MHz 使 8H/8051U 选不到 45.1584、漏掉多系列共有的 27.000。
- 判定由「范围判断」改为「**菜单成员判断**」：45.100 在 8H 范围内但不是档位，仍拒绝。
- 8A 命名陷阱：`STC8A8K64S4A12` / `STC8A2K64D4` 是 8A 硅片（45.000），`STC8A8K64D4` 是 8H 硅片（45.1584）。
- `session.cpp`：`ClampFrequency` → `ResolveFrequency`，区分「超上限（下调并提示）」与「不在菜单（拒绝）」。
- 新增 `docs/irc-frequency-tables.md`：逐系列菜单、来源与省略条目理由。

### 3.3 CLI 重构（`src/cli/options.cpp` +835 行）

**选项表 `OptionDef`，两种匹配方式：**

| 选项 | 拼写 | 短形式 | 匹配 |
|---|---|---|---|
| 串口 | `-serial` | `-s [BAUD]` | Literal |
| HID | `-hid` | `-h` | Prefix |
| 输出 | `-output` | `-o` | Prefix |
| 设备 | `-device` | `-d` | Prefix |
| EEPROM | `-eeprom` | `-e` | Prefix |
| 时钟源 | `-clock` | `-c` | Prefix |
| 频率 | `-freq` / `-frequency` | `-f` / `-F` | Literal |
| 系列 | `-series` | — | Literal |

- 选项名**大小写不敏感**；长名接受**唯一前缀**（`-ee` → `-eeprom`）；歧义前缀（如 `-ser`）不解析。
- 未知选项用 **Levenshtein 距离**提示 `"did you mean -freq"`，只提示不替换。
- **固件改为位置参数**：`newisp burn app.hex`、`newisp inspect app.hex`；删除 `-f/--file`、`-b/--baud`。
- `-help` / `-version` 改为**主命令** `help` / `version`；无参数运行不再输出设备列表，只给一行提示。
- `list` / `detect` 拒绝编程选项（频率、EEPROM、时钟、系列），在解析阶段即报错。
- `-q/-v` 合并为单一 `-output quiet|normal|verbose`（`q/n/v/default` 亦可）。
- 帮助文本整段从消息表拼装，中英结构完全一致。

### 3.4 能力校验取代一刀切拒绝

`chip_logic.h` 新增四个谓词，替代按系列硬编码的禁令：

| 谓词 | 含义 |
|---|---|
| `HasSelectableFrequency()` | STC12/89 无频率菜单，`-freq` 被拒 |
| `HasSelectableEeprom()` | 仅 8/32 代有 EEPROM 分割字段 |
| `HasSelectableClockSource()` | 仅 STC10/11/12 有时钟源位 |
| `IsUsbCapableChip()` | 仅 8H / STC32 / 8051U 含 USB 控制器 |

同时移除：STC32「禁止烧录」整段拒绝（改为按 ≤64 KiB + EEPROM 分割校验）、`allowHidOnlySerial` / `-force-serial` 串口闸门、STC16 型号及协议分支。

### 3.5 HID 强制化与构建系统

- 删除 `NEWISP_ENABLE_HID`；`NEWISP_HAVE_HID=1` 恒为 PUBLIC 定义，并在头文件加 `#error` 防止被关。
- macOS/Linux 缺 hidapi 由「降级」改为 `FATAL_ERROR` 并给出安装提示。
- 新增 `NEWISP_LANG` 缓存变量；`i18n.cpp` 对两个 `.inc` 设 `OBJECT_DEPENDS`；`NEWISP_VERSION` 移到 `newisp_core`。
- `SerialPortInfo` 增加 `portName` / `vid` / `pid`；新增 `RefreshSerialPortIdentity()`，把 usbser 的本地化名升级为 HID 侧产品名（`STC USB Serial (COM12)`）。

### 3.6 测试与文档

- `test_options.cpp`（新增 440 行）、`test_chip_logic.cpp`（+265 行）、`test_i18n.cpp`（新增）；`pty_preflight_test.py` **语义反转**：从「STC32 被拒」改为「STC32 端到端烧录成功」。
- 所有 CTest 条目设 `PYTHONUTF8=1`（中文 Windows 代码页 936 解码 UTF-8 会抛 `UnicodeDecodeError`）。
- `README.md` / `README.zh-CN.md` 重写选项表、型号支持表、新增「输出语言」章节。

---

## 四、提交 3：`ef306c8` 不再隐式修改参数（+1789 / −972，12 文件）

核心是四件事 + 一处语义修正 + 一个示例脚本。

### 4.1 `-output` 默认改为 `quiet`

| 文件 | 内容 |
|---|---|
| `include/newisp/options.h` | `OutputLevel` 注释重写；`CliOptions::output` 默认 `Normal` → `Quiet` |
| `src/cli/main.cpp` | `Logger::m_level` 默认同步为 `Quiet` |
| `src/cli/options.cpp` | `ParseCommandLine` 局部默认值同步 |
| `include/newisp/text_en.inc` / `src/i18n/text_zh_cn.inc` | 帮助文本措辞同步 |
| `tests/test_options.cpp` | 断言默认值为 `Quiet` |

同时**删除隐式升级规则**：原来"输出被重定向且未写 `-output`"会自动当 `verbose`，现在等级就是 `-output` 给出的值，未给即 `quiet`：

```cpp
// 删除
const newisp::OutputLevel level = (!opts.outputExplicit && !tty) ? Verbose : opts.output;
// 现在
const newisp::OutputLevel level = opts.output;
```

### 4.2 未给 `-freq` 时保留芯片当前频率

问题：`BurnOptions::targetFreq` 恒为 24 MHz 默认值，所以 `newisp burn app.hex` 会**默默把 IRC 改成 24 MHz**。

| 文件 | 内容 |
|---|---|
| `src/cli/session.cpp` | `m_status.userTargetFreq = opts.freqExplicit ? opts.targetFreq : 0`；`ResolveFrequency` 只在显式给出时调用；未给出时记录 `FreqFromChip` |
| `src/cli/main.cpp` | 回显条件 `if (opts.burn.targetFreq)` → `if (opts.burn.freqExplicit)` |
| 两份消息表 | 新增 `FreqFromChip` |

`userTargetFreq == 0` 在协议层既有含义即「保持芯片上报值」（握手状态包的 `clockHz`，见 `ParseStatus`）。

### 4.3 未给 `-eeprom` 时保留芯片当前 EEPROM 分割

`protocol_stc8_hid.h` 的 `BLOCK2` 常量自带 `opts[39] = 0xFE`，等于声明约 64 KB EEPROM、几乎没有程序区。

```cpp
else {
    opts[39] = m_status.msr[4];   // 回写芯片上报的当前分割；0xFF 表示未划分，同样保留
}
```

> 仅 HID 实现缺这一分支；串口实现 `protocol_stc8_uart.h` 原本就回写 `msr[4]`，未受影响。

### 4.4 HID 下 `detect` 结束不复位

串口上 `0xFF` 复位包是对的；HID 上芯片一旦退出 ISP 监视器，USB 设备就从总线消失，`detect` 后的 `burn` 会报「未找到 STC USB ISP 设备」。

```cpp
if (!opts.useHid && !probeList.empty()) { /* 发 0xFF */ }
if (opts.useHid) log(Tr(Msg::DetectHidLeftInIsp));
```

### 4.5 `detect` 的 `-s` 忽略波特率

`detect` 的握手固定 2400 波特且从不切换，故 `-s` 后的数值只消费不赋值（`-s 921600` 这种习惯写法不再报错也无效果）。新增测试 `detect -s 9600 -d COM7`。

### 4.6 新增示例脚本 `stc32_hid_program.py`

51 行的最小 HID 烧录示例（`hid` + `intelhex` + `tqdm`）：`0x46 0xb9` 起始 / `0x16` 结束、包头 + 大端长度 + 载荷 + 16 位和校验，流程为 启动 → 信息 → 解锁 → 擦除 → 按 `0x80` 字节块写 `0x32`/`0x12` → `0xFF` 复位。仅用于演示协议，**无错误检查，仅在 STC32F12K54 上测过**。

### 4.7 文档与帮助文本

`README.md`（+928 / −… 的大幅改写）与 `README.zh-CN.md` 同步：频率上限改为逐系列表格、明确「不在菜单报错 / 超上限下调」、删除 STC32 禁止烧录段落、`inspect` 地址范围、示例命令改 `-s <设备>`。帮助文本中示例统一改为 `# newisp <args>` 形式，选项行 `-o, -output` 简化为 `-output`。`docs/changes-summary-26-10-31-2.md` 本身也是本次提交的一部分。

---

## 五、行为变化汇总（对使用者可见）

| # | 变化 | 来自 |
|---|---|---|
| 1 | 无 trim 的频率**中止写入**并列出支持频率，不再回退芯片时钟 | `b606b3a` |
| 2 | 频率必须命中**该系列菜单**，范围判断改为成员判断 | `ff9a8dc` |
| 3 | `burn/inspect` 的固件改为**位置参数**；`-f` 含义从文件变为频率 | `ff9a8dc` |
| 4 | `-h` 是 HID，`-help/-version` 变为 `help`/`version` 命令，`-q/-v` 变 `-output` | `ff9a8dc` |
| 5 | `-family` → `-series`（须全拼）；`-b/--baud` 删除，改用 `-s <rate>` | `ff9a8dc` |
| 6 | 选项名大小写不敏感、支持唯一前缀缩写；未知选项给拼写建议 | `ff9a8dc` |
| 7 | STC32 现在可以烧录（≤64 KiB）；STC16 型号从芯片表移除 | `ff9a8dc` |
| 8 | HID 不可再关闭，macOS/Linux 必须装 hidapi 才能 configure | `ff9a8dc` |
| 9 | 语言在构建期固定，运行期不可切换 | `ff9a8dc` |
| 10 | 默认输出变少（`quiet`），重定向不再自动变 verbose | `ef306c8` |
| 11 | 不带 `-freq` 烧录不再改 IRC 频率，保持芯片当前值并打印说明 | `ef306c8` |
| 12 | 不带 `-eeprom` 烧录不再写 `0xFE` 分割，保持芯片当前分割 | `ef306c8` |
| 13 | HID 下 `detect` 后芯片停在 ISP 模式，可直接接着 `burn`；要跑原程序需断电重启 | `ef306c8` |
| 14 | `detect -s <数值>` 被忽略，不报错也不生效（固定 2400） | `ef306c8` |

---

## 六、旧脚本迁移提示

```diff
- newisp burn -f app.hex -d COM7 -b 115200 -q
+ newisp burn app.hex -d COM7 -s 115200 -output quiet
- newisp inspect -f app.hex
+ newisp inspect app.hex
- newisp -V / newisp --help
+ newisp version / newisp help
- newisp burn app.hex -HID
+ newisp burn app.hex -h
- newisp detect --family stc32 -d COM7
+ newisp detect -series stc32 -d COM7
```

另需注意：若脚本依赖「不带 `-freq` 就自动写 24 MHz」或「不带 `-eeprom` 就写 `0xFE`」，`ef306c8` 之后这些假定不再成立，如需固定值请显式传参。

---

## 七、质量观察与后续建议

1. **单一数据源仍有残留**：`b606b3a` 引入的 `kHz[]`/`kText[]` 清单与 `chip_logic.h` 的系列菜单是两套数据，后续可完全由 `ListFrequencyMenu()` 派生。
2. **默认值变化缺少回归覆盖**：`ef306c8` 对 `-freq` / `-eeprom` 的「保持芯片值」改动仅靠注释说明，`test_options.cpp` 只覆盖了 `-output` 默认值与 `detect -s`；建议补充针对 `freqExplicit` / `eepromBytes == 0` 分支的单元测试。
3. **文件尾换行**：`ff9a8dc` 起部分文件（如 `README.zh-CN.md`）缺末尾换行，建议统一一次。
4. **BOM 来回变动**：`CMakeLists.txt` 首行新增 BOM，而 `chip_table.h`、`options.h` 等首行 BOM 被删除，建议统一一次。
