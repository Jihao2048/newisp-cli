# newisp 1.1.0 发行说明

- 基线版本：`1.0.0`（提交 `ca39695`）
- 本版本提交范围：`b4344cc`（不含）至 `ef306c8`，共 6 次提交，其中 3 次为主要变更
- 文档日期：2026-10-31

---

## 一、概述

本版本围绕一项原则重构了命令行行为：**除用户显式指定的参数外，工具不写入任何配置**。此前版本在未指定 `-freq`、`-eeprom` 或 `-output` 时会采用内置默认值并写入芯片，本版本改为沿用芯片当前状态，并在需要时以日志说明。

同时完成两项基础设施改造：用户可见文本全部迁入编译期双语消息表；IRC 频率表按芯片系列拆分，参数校验改由系列能力决定。

---

## 二、主要变更

### 2.1 不再改写用户未指定的参数

| 参数 | 1.0.0 行为 | 1.1.0 行为 |
|---|---|---|
| `-output` 未指定 | 输出级别为 `normal`；标准输出重定向时自动升级为 `verbose` | 输出级别为 `quiet`；不因重定向改变级别 |
| `-freq` 未指定 | 写入默认值 24 MHz | 沿用芯片当前 IRC 频率，并输出一行说明 |
| `-eeprom` 未指定（HID） | 写入 `BLOCK2` 固定的 `0xFE`，约等于声明 64 KB EEPROM | 回写握手状态中芯片上报的当前分割值（`msr[4]`） |
| HID 下 `detect` 结束 | 发送 `0xFF` 复位包 | 保持在 ISP 监视器，便于连续执行 `burn` |

`BurnOptions` 新增 `freqExplicit` 与 `clockExplicit` 字段，用于区分“显式指定”与“使用默认值”，避免将默认值当作写入目标。

### 2.2 频率 trim 校验严格化

目标频率没有对应 trim 三元组，或不属于受支持频率集合时，选项字节写入将中止，并列出全部支持频率（5.5296–60.000 MHz，共 17 档），不再回退至芯片实测时钟。原回退逻辑可能以占位 trim 写入选项块，导致 IRC 未校准。

`kTrimTable` 提升至类作用域，作为受支持频率的单一数据源。

### 2.3 按系列区分的 IRC 频率菜单

全局频率表拆分为六张按系列划分的表：

| 系列 | 条目数 | 上限 |
|---|---|---|
| STC32G144K246 | 15 | 60.000 MHz |
| STC8051U | 16 | 45.1584 MHz |
| STC8H | 16 | 45.1584 MHz |
| STC8A | 16 | 45.000 MHz |
| STC8G | 12 | 35.000 MHz |
| STC15 | 12 | 35.000 MHz |

频率判定由区间判断改为菜单成员判断：处于系列区间内但不在菜单档位的频率（如 8H 系列的 45.100 MHz）予以拒绝。超出系列上限时下调并记录日志，其余不在菜单的频率直接拒绝。新增文档 `docs/irc-frequency-tables.md`。

### 2.4 编译期双语输出

用户可见文本全部迁入消息表，语言在构建期由 `-DNEWISP_LANG=zh-CN|en` 选择，二进制仅包含所选语言的文本：

| 文件 | 作用 |
|---|---|
| `include/newisp/i18n.h` | 消息枚举、格式化、日志类型与标记 |
| `include/newisp/text_en.inc` | 英文基准表，消息枚举的唯一来源 |
| `src/i18n/text_zh_cn.inc` | 中文译文表 |
| `src/core/i18n.cpp` | 按构建宏选择并实现 `Tr()` |

日志类型通过行前缀标记（`%p` 报文转储、`%a` 需注意的结论）随行传递，替代此前按中文前缀过滤日志的实现，过滤逻辑不再依赖输出语言。

### 2.5 命令行接口重构

- 固件改为位置参数：`newisp burn app.hex`、`newisp inspect app.hex`。
- 选项名大小写不敏感；长选项接受唯一前缀缩写（如 `-ee` 等价于 `-eeprom`），有歧义的前缀不予解析。
- `help`、`version` 由选项改为主命令。
- `list`、`detect` 拒绝编程类选项（频率、EEPROM、时钟源、系列），在解析阶段报错。
- 未知选项按编辑距离给出近似建议。
- 帮助文本由消息表生成，中英文结构一致。

### 2.6 能力驱动的参数校验

新增系列能力判定函数，替代此前按系列硬编码的禁止规则：

| 函数 | 语义 |
|---|---|
| `HasSelectableFrequency()` | 系列是否存在频率菜单（STC10/11/12、STC89/90 无） |
| `HasSelectableEeprom()` | 系列是否存在 EEPROM 分割字段 |
| `HasSelectableClockSource()` | 系列是否存在时钟源位（STC10/11/12） |
| `IsUsbCapableChip()` | 芯片是否集成 USB 控制器（STC8H、STC32、8051U） |

据此移除 STC32 烧录禁令（改为按 ≤64 KiB 容量与 EEPROM 分割校验）、`-force-serial` 及 `allowHidOnlySerial` 串口限制。STC16 系列自芯片表移除，其型号仍可被探测并返回芯片 ID，但不再解析为型号名。

### 2.7 构建系统

- 移除 `NEWISP_ENABLE_HID` 选项，USB HID 成为必需依赖；在 macOS/Linux 上缺少 hidapi 时 configure 直接失败并给出安装指引。
- 新增 `NEWISP_LANG` 构建选项，默认 `zh-CN`。
- 版本号定义移至 `newisp_core`。

### 2.8 其他

- `detect` 的握手固定为 2400 波特，`-s` 后的数值被忽略，既不生效也不报错。
- 串口设备信息新增 `portName`、`vid`、`pid` 字段及 `RefreshSerialPortIdentity()`，USB 串口设备在设备列表中优先显示 USB 产品字符串。
- 新增 `stc32_hid_program.py`，演示 HID 烧录协议的最小实现（无错误检查，仅在 STC32F12K54 上验证）。
- `hex_file.cpp` 的 Windows 分支补充 `NOMINMAX` 定义。

---

## 三、不兼容变更

以下变更会导致既有脚本无法运行，升级前需修改调用方式。

| 项目 | 1.0.0 | 1.1.0 |
|---|---|---|
| 固件指定方式 | `-f, -file app.hex` | 位置参数 `app.hex` |
| 频率选项 | 无 | `-freq`、`-frequency`、`-f`、`-F` |
| 波特率 | `-b, --baud 115200` | `-s 115200`（跟随 `-serial`） |
| 传输方式 | `-HID` | `-h, -hid` |
| 帮助 / 版本 | `-help`、`--help`、`-V` | `help`、`version` 命令 |
| 输出级别 | `-q`、`-v` | `-output quiet|normal|verbose` |
| 系列选项 | `-family` | `-series`（须完整拼写） |
| HID 支持 | 可通过 `NEWISP_ENABLE_HID=OFF` 关闭 | 必需依赖 |
| STC16 系列 | 可解析型号名 | 自芯片表移除 |
| 输出语言 | 运行期固定为中文 | 构建期选择，运行期不可切换 |
| 无参数运行 | 输出设备列表与帮助 | 仅输出一行提示 |

### 迁移示例

```diff
- newisp burn -f app.hex -d COM7 -b 115200 -q
+ newisp burn app.hex -d COM7 -s 115200 -output quiet
- newisp inspect -f app.hex
+ newisp inspect app.hex
- newisp -V
+ newisp version
- newisp burn app.hex -HID
+ newisp burn app.hex -h
- newisp detect --family stc32 -d COM7
+ newisp detect -series stc32 -d COM7
```

依赖默认值自动写入的流程需同步修改：若需固定频率或 EEPROM 分割，必须显式传入 `-freq`、`-eeprom`；否则工具将保留芯片当前设置。

---

## 四、缺陷修复

| 编号 | 问题 | 处理 |
|---|---|---|
| 1 | 频率无对应 trim 时回退至芯片实测时钟，可能以占位 trim 写入选项字节，导致 IRC 未校准 | 中止写入并列出受支持频率 |
| 2 | 未指定 `-freq` 时写入默认 24 MHz，覆盖用户未要求修改的振荡器设置 | 沿用芯片当前频率 |
| 3 | 未指定 `-eeprom`（HID）时写入 `BLOCK2` 固定的 `0xFE`，实际声明约 64 KB EEPROM | 回写芯片上报的分割值 |
| 4 | HID 下 `detect` 发送复位包后 USB 设备从总线消失，导致后续 `burn` 失败 | 检测完成后保持在 ISP 监视器 |
| 5 | 全局频率表存在三处错误：包含不属于任何菜单的频率、上限限制导致 8H/8051U 无法选择 45.1584 MHz、遗漏多系列共有的 27.000 MHz | 按系列拆分频率表并修正 |
| 6 | 日志按中文前缀过滤，与非中文输出或译文改动冲突 | 改为行内类型标记 |
| 7 | `hex_file.cpp` 受 `<windows.h>` 的 `min`/`max` 宏影响 | 定义 `NOMINMAX` |

---

## 五、验证

- 单元测试新增 `test_i18n.cpp`，校验占位符一致性、译文完整性及日志标记行为。
- `test_options.cpp` 新增约 460 行，覆盖位置参数、选项前缀匹配、`-output` 单一设置、`detect -s` 语义等。
- `test_chip_logic.cpp` 新增约 265 行，覆盖逐系列频率上限、菜单互异性与 trim 表一致性。
- `pty_preflight_test.py` 断言由“STC32 被拒绝”改为“STC32 端到端烧录成功”。
- 全部 CTest 条目设置 `PYTHONUTF8=1`，规避中文 Windows 代码页 936 下的解码错误。

---

## 六、已知限制

1. 输出语言在构建期固定，单个二进制无法在运行期切换，双语文档需分别构建。
2. 受支持频率清单在 `protocol_stc8_hid.h` 与 `chip_logic.h` 中仍各存一份，存在维护不一致的风险。
3. `stc32_hid_program.py` 为协议示例，无错误检查，仅经 STC32F12K54 验证。
4. `-freq` 与 `-eeprom` 的“沿用芯片当前值”分支尚无单元测试覆盖。
