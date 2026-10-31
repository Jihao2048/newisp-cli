# newisp

STC 全系列单片机跨平台烧录工具（命令行版）。

---

## 目录

- [特性](#特性)
- [支持的芯片](#支持的芯片)
- [两种传输方式](#两种传输方式)
- [编译](#编译)
  - [Windows](#windows)
  - [Linux](#linux)
  - [macOS](#macos)
  - [HID 是必需依赖](#hid-是必需依赖)
  - [输出语言（编译期决定）](#输出语言编译期决定)
- [编码注意事项](#编码注意事项)
- [用法](#用法)
  - [命令](#命令)
  - [参数](#参数)
  - [示例](#示例)
  - [不擅自改写你没指定的参数](#不擅自改写你没指定的参数)
  - [从旧版命令行迁移](#从旧版命令行迁移)
- [测试](#测试)
- [代码结构](#代码结构)
- [硬件相关说明](#硬件相关说明)
- [验证状态](#验证状态)
- [已知限制](#已知限制)
- [许可证](#许可证)

---

## 特性

- **一套代码，三个平台**：Windows / macOS / Linux，CMake 构建。
- **两种传输方式**：串口（STC 经典 ISP 监视器）和 USB HID（出厂 USB ISP 接口）。
- **芯片自动识别**：自带 1300 多条芯片 ID 表，探测到就自动选对应协议。
- **串口波特率校准**：实测芯片 IRC 频率后现场算 trim，不是死查表。
- **按系列校验参数**：每个系列有自己的 IRC 频率菜单；`-freq`、`-eeprom`、`-clock`
  只在目标系列真的存在该字段时才接受，其余情况明确报错，而不是写进含义不同的选项位。
- **只改你要求改的东西**：不给 `-freq` 就沿用芯片当前频率，不给 `-eeprom` 就沿用芯片当前
  分割，默认输出也足够安静——不再"顺手"把振荡器或 EEPROM 划分改掉。
- **烧录前检查**：严格检查 HEX；烧录前校验镜像容量、EEPROM 分割与目标芯片容量（含 STC32）。
- **Windows 零外部依赖**：HID 直接用系统自带的 `hid.dll` + SetupAPI，不需要 hidapi、不需要 vcpkg。
- **可脚本化**：纯命令行工具，退出码规范（成功 0，失败非 0），默认输出可预测，方便集成到构建流程。

---

## 支持的芯片

| 系列 | 型号 |
|---|---|
| STC89/90 | STC89C52RC、STC89C516RD、STC90C58RD 等；仅串口 |
| STC10/11/12 | STC12C5A60S2、STC12LE5A60S2、IAP11F06 等（含 12A / 12B 变体）；仅串口 |
| STC15 | STC15F104E、STC15W408AS、IAP15W4K58S4 等（含 15A 变体）；仅串口 |
| STC8 | STC8F、STC8A、STC8C、GX8S 等；串口 + USB HID（同为探测驱动，8A 硅片确有 USB）|
| STC8G | STC8G1K08、AI8G1K08、JX8G 等；串口 + USB HID（手册未列出，以探测结果为准）|
| STC8H | STC8H1K08 ~ STC8H8K64U、AI8H 等；串口 + USB HID |
| STC32 | STC32G144K246 等；串口 + USB HID，与 8051U 同一套协议；支持 64 KiB 以内镜像 |
| STC8051U | STC8051U34K64、AI8051U34K64 等；串口 + USB HID（手册写"仅 HID"，但串口实测可用，见[下文](#8051u-确实有串口-isp-监视器)）|

> **参数不是"一刀切"拒绝，而是按系列校验**：工具不再笼统地按系列名拒绝某种传输方式或某个
> 选项，而是判断该系列**有没有这个字段**——没有就报错，有就接受（见[硬件相关说明](#硬件相关说明)）。
> STC16 已从型号表中移除：仍然能探测并报出 ID，但没有型号名。

---

## 两种传输方式

### 串口（UART）

**所有支持的芯片系列都能走串口**，这是本工具唯一覆盖全系列的传输方式；某个系列能不能用，
由探测结果决定，而不是由手册或系列名决定。

STC 经典 ISP 监视器。芯片可能还在跑用户程序，所以工具会：

1. **拉低 DTR 250ms 做复位脉冲**
2. **反复发送 `0x7F` 唤醒字节**（最多 500 轮，每轮 200 次）
3. 芯片应答后**校准波特率**：发 `0x00 0x08` 粗扫测 IRC 频率，再发 `0x00 0x0C` 细扫，
   最后把算出的 trim 通过 `0x01` 下发并切换波特率

`burn` 支持 2400 ~ 460800 bps（STC89/12 系列因 8 位 BRT 限制，最高 115200）。
`detect` 的握手固定 2400 bps，**不使用** `-s` 后面的数字。

官方手册把 8051U 系列标为"仅 USB HID"，早期版本据此在串口上直接拒绝；这条规则已删除，
实测 8051U 的串口 ISP 监视器正常应答（见[下文](#8051u-确实有串口-isp-监视器)）。

### USB HID

STC 出厂 USB ISP 接口（`VID 0x34BF / PID 0x1001`），STC32、STC8H、8051U 等型号支持。
STC8G **不在厂商的 USB 列表里**，但它的**硅片确实带 USB 控制器**（`ProtocolStc8GHid` 就是为 8G 的 08A 变体写的）；能不能用仍以探测为准。

- **不需要唤醒**：用户按住 BOOT 上电时芯片已经在 ISP 监视器里了
- **不需要波特率协商**：USB 链路由主控同步，直接写选项字节里的频率和 trim
- **平台实现不同**：
  - Windows：直接调 `hid.dll` + SetupAPI（系统自带，零依赖）
  - macOS / Linux：用 hidapi（IOKit / hidraw 后端）

> **HID 下 `detect` 之后芯片停在 ISP 模式，不复位**。串口上工具会发 `0xFF` 复位包让芯片回到
> 用户程序；HID 上不能这么做——芯片一旦退出 ISP 监视器，USB 设备就会从总线上消失，紧接着的
> `burn` 会报"未找到 STC USB ISP 设备"。所以 `detect` 后面可以直接接 `burn`；想让芯片跑原程序，
> 请断电重启。

---

## 编译

需要 **CMake 3.20+** 和 **C++17 编译器**。macOS / Linux 上还需要 **hidapi**（见下）。

### Windows

用 **Visual Studio 2022 x64 开发者命令提示符**。**不需要任何外部依赖**：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

产物：`build\Release\newisp.exe`

也可以直接用仓库里的脚本：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\win-build.ps1
powershell -ExecutionPolicy Bypass -File scripts\win-smoke.ps1    # 冒烟测试
```

Windows 版只链接 `setupapi` 和 `hid` 两个系统库，**没有要下载或附带的东西**。

> 仓库里的 `.vscode/` 提供了一套免 preset 的配置：`settings.json` 固定生成器与 `NEWISP_LANG`，
> `tasks.json` 把"选语言 + 选构建类型"做成构建任务（`Ctrl+Shift+B`）。

### Linux

```sh
sudo apt install build-essential cmake pkg-config libudev-dev libhidapi-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

**权限配置**：

```sh
# 串口：把用户加入 dialout 组（需要重新登录）
sudo usermod -aG dialout $USER

# HID：hidraw 节点默认是 root:root 0600，需要 udev 规则
sudo tee /etc/udev/rules.d/60-newisp.rules <<'EOF'
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="34bf", MODE="0666"
SUBSYSTEM=="tty",    ATTRS{idVendor}=="34bf", MODE="0666"
EOF
sudo udevadm control --reload-rules && sudo udevadm trigger
```

> **WSL2 默认看不到 USB 设备**。需要用
> [`usbipd-win`](https://github.com/dorssel/usbipd-win) 把设备转发进去：
> ```powershell
> usbipd list                                    # 找到 BUSID
> usbipd bind --busid <BUSID>                    # 需要管理员权限
> usbipd attach --wsl --busid <BUSID>
> ```
> 仓库里的 `scripts/usbipd-attach.ps1` 封装了这个流程。

### macOS

**Apple Silicon（M 系列）**可以用 Homebrew：

```sh
brew install cmake hidapi
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

**Intel Mac 注意**：Homebrew 7.0.0 起已把 Intel Mac 降级为 "Tier 3"，
安装脚本会直接拒绝运行在非 Apple Silicon 上。Intel 机器需要手动装依赖：

```sh
# 1. CMake（macOS 自带 python3，用 pip 装）
python3 -m pip install --user cmake
echo 'export PATH="$HOME/Library/Python/3.9/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc

# 2. hidapi（从源码编译，走 IOKit 后端）
cd ~
curl -L -o hidapi.tar.gz \
  https://github.com/libusb/hidapi/archive/refs/tags/hidapi-0.14.0.tar.gz
tar xzf hidapi.tar.gz && cd hidapi-hidapi-0.14.0
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build -j4
sudo cmake --install build

# 3. newisp
cd ~/newisp-cross
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

> `CMAKE_POLICY_VERSION_MINIMUM=3.5` 是必需的：hidapi 0.14.0 要求的 CMake 版本低于 3.5，
> 而 CMake 4.x 会直接拒绝这类工程。

macOS 的串口设备是 **`/dev/cu.*`**。**优先用 `cu` 而不是 `tty`**：
打开 `tty.*` 会等待 DCD 信号，而自供电的开发板永远不会给，会导致 open 卡住。

更详细的 macOS 步骤（含命令行工具安装）见 [`docs/macos-build.md`](docs/macos-build.md)。

### HID 是必需依赖

**没有"不编译 HID"的开关**（`NEWISP_ENABLE_HID` 已删除，`NEWISP_HAVE_HID=1` 恒为开启）。
STC32、STC8H、8051U 都依赖 USB ISP 接口，缺了 HID 就等于缺了一部分型号的烧录能力——
与其让它在谁的实验台上失败，不如在 configure 阶段就失败：

- Windows：不额外需要任何东西，直接用系统 HID 类驱动
- macOS / Linux：**hidapi 是硬依赖**。缺了它 `cmake` 会直接停在 `FATAL_ERROR`，并提示要安装
  哪个包（`brew install hidapi` / `libhidapi-dev`）

### 输出语言（编译期决定）

语言在**编译时选定**，用 `NEWISP_LANG` 指定哪一种语言表参与实际输出：

```sh
cmake -S . -B build-zh -DNEWISP_LANG=zh-CN     # 中文（默认）
cmake -S . -B build-en -DNEWISP_LANG=en        # 英文
```

`NEWISP_LANG` 接受 `zh-CN`、`zh`、`en`、`en-*`，其他值在 configure 阶段直接报错。

所以**同一个二进制不能在运行时切换语言**；要两种都要，就 configure 两次。
这对一个小巧的自包含烧录工具来说是刻意的取舍：启动时不做 locale 查询、运行时没有语言表切换、
目标机上也没有东西可以配错。

> 严格地说：**英文基准表在任何构建里都会编进去**（`src/core/i18n.cpp` 里它同时也是测试逐条比对的
> 依据，相对协议代码体量很小），中文构建额外带一份中文表；英文构建没有第二张表，直接返回英文原文。
> 也就是说，两种构建**都只输出一种语言**，但"另一种语言完全不在可执行文件里"这种说法并不准确。

文案表分两处：`include/newisp/text_en.inc` 是英文源文本（同时也是**基准表**、`Msg` 枚举的唯一
来源），`src/i18n/` 下每个语言一个文件，每条都以英文原意为名字（`Msg::EraseOk` ≈ "Erase complete"），
便于中英并排校对。新增一种语言 = 加一个 `.inc`、加一个 `NEWISP_LANG` 取值、加一个编译宏，
调用方代码一行都不用改。`tests/test_i18n.cpp` 会检查译文是否保留了每个 `{n}` 占位符、有没有
漏译的条目，以及二进制自报的语言和它实际打印的语言是否一致。

---

## 编码注意事项

**源码文件是 UTF-8 with BOM，且 MSVC 编译时加了 `/utf-8`。**

两个都需要，原因是文案表和注释里有中文：不加 BOM 的话，MSVC 会按系统 ANSI 代码页（中文 Windows 上是
936/GBK）读源文件，把中文字符串当成 GBK 解析，直接报 `常量中有换行符` 编译错误。

**请勿去掉 BOM。** 换行符统一用 **LF**（`.gitattributes` 里已强制）。

---

## 用法

### 命令

```sh
newisp                                 只打印一行提示：用 help 看用法，用 list 列出设备
newisp help                            显示完整帮助
newisp version                         显示版本与本次构建包含的传输方式
newisp list                            列出串口和 HID 设备
newisp detect   [传输方式] [设备]       探测并识别芯片
newisp inspect  FILE                   离线校验 HEX 并显示地址范围
newisp burn     FILE [参数] [设备]      擦除、写入、写选项字节
```

`-help` / `-version` 不再是选项，改成了 `help` / `version` 两个主命令：既没有 `-help`，
也没有 `-?`、`-version` 或 `-V`。

`newisp` 不带任何参数时**只打印一行提示**（指向 `help` 和 `list`），既不再枚举设备，也不打印帮助。

### 参数

| 参数 | 说明 |
|---|---|
| `FILE` | 固件文件是紧跟命令的**位置参数**：`newisp burn app.hex`、`newisp inspect app.hex`。`burn` 和 `inspect` 必填，没有对应的选项（`-f` / `--file` 已删除）|
| `-s`, `-serial [BAUD]` | 用串口 ISP（默认）。可选 `BAUD` 是握手波特率，默认 115200，支持 `115200`、`115.2k`、`1M` 写法。例：`-s 921600`。也可以只写设备名：`-s COM7`、`-s /dev/ttyUSB0`。**`detect` 的握手固定 2400，`-s` 后的数字会被忽略（不报错）** |
| `-h`, `-hid` | 用 USB HID ISP（`-h` 已不再表示 help；`-H` 因大小写不敏感同样解析为 HID）|
| `-d`, `-device NAME` | 设备名：`COM7`、`/dev/ttyUSB0`、`/dev/cu.usbserial-*` 或 HID 设备路径。省略时若只有一个候选设备则自动选，多个则报错并列出 |
| `-f`, `-freq`, `-frequency`, `-F` HZ | 目标 IRC 频率。支持 `24000000`、`24M`、`11.0592M`。**不给就沿用芯片当前频率**；给了就必须是该系列频率表里的档位（超过上限会下调并提示，不在菜单则报错）。不存在"缺省就是 24MHz"这种隐式设定 |
| `-e`, `-eeprom SIZE` | EEPROM 分割，如 `4K`（也接受 `4096`）。`0` = 保留芯片设置；**不给 `-eeprom` 就保留芯片当前分割** |
| `-c`, `-clock internal\|external` | 时钟源。只有 STC10/11/12 有这一位，其他系列会拒绝。也接受 `internal` / `int` / `irc`、`external` / `ext` / `crystal` |
| `-series NAME` | 探测失败时的兜底系列：`stc89`、`stc12`、`stc15`、`stc8g`、`stc8h`、`stc8a`、`stc32`、`stc8`（帮助里列的是这八个；解析器另外还接受 `stc90`、`stc10`、`stc11`、`iap15`、`irc15`、`ai8g`、`jx8g`、`ai32`、`ai8h`、`8051u`、`stc8051u`、`ai8051u`、`stc8f`、`stc8c`、`ai8a`、`ai8c`，且都不区分大小写）。**必须全拼**（`-ser` 无效，也没有短形式）|
| `-output LEVEL` | 日志详细程度：`quiet`（**默认**，只有错误、警告和最终结果）、`normal`（加进度行）、`verbose`（含原始报文）。也接受 `q` / `n` / `v` / `default` |
| `help` | 显示帮助（是主命令，不是选项）|
| `version` | 显示版本（是主命令，不是选项）|

**只有三个选项锁定全拼**：`-serial`、`-freq`、`-series`。其余长名都接受**唯一前缀缩写**
（`-ee` 即 `-eeprom`，`-de` 即 `-device`，`-cl` 即 `-clock`，且要求至少三个字符）。
`-serial` 若允许缩写，`-ser` 会在它与 `-series` 之间摇摆；`-freq` 锁死全拼是为了避免
`-frequncy` 这类拼写错误被当成唯一前缀静默接受。

选项名**大小写不敏感**（`-DEVICE`、`-SERIAL`、`-OUTPUT` 都能用）。频率项有四个不同名字：
`-frequency`、`-freq`、`-f` / `-F`（**不接受** `-fr` 之类的缩写）。
未知选项会按编辑距离给出建议（`unknown option: -frequncy (did you mean -freq?)`），
但**只提示、不自动替换**——猜测用户想要哪个频率正是不能做的事。

固件文件不是选项，而是命令后的第一个参数；选项也可以写在它前面。工具接受单破折号写法
（`-output`、`-series`），文中表格的 `--` 形式为便于阅读的惯用写法。

> `inspect` 是纯离线命令：传输方式（`-s` / `-h`）、设备（`-d`）、`-series`、`-clock`、`-eeprom`
> 全部拒绝。
> `detect` 接受传输方式、设备和 `-s` 的波特率（忽略），但拒绝 `-freq`、`-eeprom`、`-clock`、
> `-series`——这些选项在解析阶段就被拦下，而不是"接受后忽略"。

> **没有 `--lang` 参数**：输出语言在编译时用 `-DNEWISP_LANG=zh-CN|en` 选定，
> 见 [输出语言（编译期决定）](#输出语言编译期决定)。

### 示例

**检测芯片：**

```sh
newisp detect -h                                 # USB HID
newisp detect -s /dev/ttyUSB0                    # Linux 串口
newisp detect -s /dev/cu.usbserial-1420          # macOS 串口
newisp detect -s COM13                           # Windows 串口
```

**烧录固件：**

```sh
# 只烧固件：频率和 EEPROM 划分都沿用芯片当前值（默认 quiet 输出）
newisp burn firmware.hex -h

# 串口，460800 波特率，显式指定目标 24MHz
newisp burn firmware.hex -d /dev/ttyUSB0 -s 460800 -F 24M

# 带 EEPROM 分割和进度行
newisp burn firmware.hex -d COM13 -F 35M -e 4K -output normal

# 需要看握手的原始报文
newisp burn firmware.hex -d COM13 -output verbose

# STC12 系列选择外部晶振
newisp burn firmware.hex -d COM13 -c external
```

### 不擅自改写你没指定的参数

这是一条贯穿整个工具的原则：**命令行没说的，就不改**。

| 情形 | 以前 | 现在 |
|---|---|---|
| `-output` 默认级别 | `normal` | `quiet`：只打印警告、错误、需要提醒的结论和最终结果 |
| 输出被重定向且没写 `-output` | 自动升级为 `verbose` | 不做任何升级，仍是 `quiet`；要报文自己写 `-output verbose` |
| 没给 `-freq` | 写 24MHz 默认值进选项字节 | **保留芯片当前 IRC 频率**，并打印一行 `未指定 -freq，沿用芯片当前的 xx MHz` |
| 没给 `-eeprom` | 写死 `0xFE`（约 64KB EEPROM，代码区所剩无几） | **回写芯片上报的当前分割值**（`0xFF` 视为"未划分"，原样保留） |
| HID 下 `detect` 结束 | 发 `0xFF` 复位，USB 设备随即消失 | 停在 ISP 模式，可以直接接着 `burn` |
| `detect -s 9600` | —— | 数值被忽略（握手固定 2400），不报错 |

`burn` 启动时也只回显你确实给了的参数：没给 `-freq` 就不会打印"目标频率"那一行，
因为打印一个不会写入的默认值，等于宣称一次不会发生的改动。同理，`-freq` 不在该系列菜单里时，
选项阶段就会报 `... 不在 xx 频率菜单中；未写入任何内容` 并退出。

> **quiet 到底隐藏了什么**：设备行、传输方式行、固件行、目标频率/波特率/EEPROM 回显、
> 以及 `[1/7]` … `[7/7]` 每一步的进度都属于"普通"级别，`quiet` 下**全部不打印**。
> 所以一次成功的 `quiet` 烧录，屏幕上的实质内容基本只有一行"烧录成功"（失败时为错误原因）。
> 需要看到分步进度请用 `-output normal`；进度条本身也只在 `quiet` 之外且输出是终端时出现。

### 从旧版命令行迁移

`-f`、`-h`、`-b`、`-q`、`-v`、`-family` 的语义都变过，旧脚本需要改：

| 旧写法 | 现在 | 说明 |
|---|---|---|
| `newisp burn -f app.hex` | `newisp burn app.hex` | 固件改成位置参数；**`-f` 现在是频率** |
| `-f 24M`（旧语义指频率） | 仍是 `-f 24M` | 语义不变，但建议写 `-freq` / `-frequency`，可读性更好 |
| `-b` / `--baud 460800` | `-s 460800` | 波特率并入 `-s`；设备名也可以直接跟在 `-s` 后 |
| `--help` / `--version` | `help` / `version` | 变成了主命令，没有 `-help`、`-H`、`-?`、`-V` 之类的写法 |
| `-v`（verbose） | `-output verbose` | 现在 `-v` 是 `-output` 的取值，不是选项；`-q` 同理也不存在 |
| `-family xxx` | `-series xxx` | 且必须全拼（`-ser` 会作为 `-serial` 的歧义前缀被拒绝） |
| `--clock external` | `-c external`（或 `-clock external`） | 未变，仅示例写法统一 |
| `-force-serial` | —— | 已删除：串口是否可用由探测决定 |

---

## 测试

### 单元测试（不需要硬件）

```sh
cmake -S . -B build-tests -DCMAKE_BUILD_TYPE=Debug -DNEWISP_BUILD_TESTS=ON
cmake --build build-tests -j
./build-tests/tests/newisp_tests        # 或 ctest --test-dir build-tests
```

单元测试是一个 `newisp_tests` 可执行文件，源文件包括 `test_hex_file.cpp`、`test_chip_table.cpp`、
`test_chip_logic.cpp`、`test_protocol_framing.cpp`、`test_i18n.cpp`、`test_options.cpp`、
`test_main.cpp`，并直接编译 `src/cli/options.cpp`，以便对参数解析器做端到端断言。

覆盖内容：

- **Intel HEX 解析器**：扩展线性/段地址、CRLF 输入、小写十六进制、截断记录、EOF 记录、
  空隙填 `0xFF`
- **芯片表**：已知 ID、未知 ID 占位符、无空条目
- **芯片系列判定与协议选择**：逐系列频率上限与菜单条目数、菜单互异性（STC8A 的 45.000
  与 STC8H 的 45.1584 是两个不同档位）、菜单与 trim 表的一致性、legacy 115200 限制、
  哪些系列需要 `0x05` 准备步骤、哪些能走 HID
- **命令行解析**（`tests/test_options.cpp`）：位置参数、`-series` 必须全拼、`-s` 的归属
  （以及 `detect -s 9600` 时数值被忽略、波特率保持默认）、唯一前缀缩写、`-output` 只设置一次、
  频率的各种拼写与近似拼写拒绝、默认输出级别为 `quiet`、拼写建议
- **报文帧格式**：精确字节和校验和、所有协议的往返、容忍前导噪声、拒绝错误校验和和丢失结束符、
  处理跨两次读到的分包、拒绝错误系列或不合理时钟的状态包
- **文案表**（`tests/test_i18n.cpp`）：每条译文的 `{n}` 占位符与英文源一致、没有漏译的正文、
  二进制自报的语言和它实际打印的语言一致、区分"报文原样转储"和"进度行"的日志标记在译文里仍然成立。
  它按构建所选的语言运行，所以发版前请把 `-DNEWISP_LANG=zh-CN` 和 `-DNEWISP_LANG=en` 各编一次测一次。
  注意它校验的是**结构与形状**，不是措辞是否正确。
- **CLI 预检**（`tests/cli_preflight_test.py`）刻意只断言退出码、十六进制地址等不随翻译变化的
  内容，因此对任一语言的构建都成立。

CTest 一共注册三个条目：`newisp_tests`、`newisp_cli_preflight`，以及仅 UNIX 平台注册的
`newisp_pty_preflight`。`tests/` 下的 `pty_test.py`、`pty_diag.py`、`pty_opts_dump.py`
是手工运行的辅助脚本，不在 CTest 里。

> 中文 Windows 上 Python 会用 936 代码页解码工具的 UTF-8 输出，直接抛 `UnicodeDecodeError`；
> `tests/CMakeLists.txt` 已为**三个** CTest 条目都设置 `PYTHONUTF8=1`。手动跑脚本时请自行设置该变量。

### 伪终端协议测试（不需要硬件）

用 **PTY 对**模拟真实串口，另一端跑一个脚本化的假 STC 芯片，驱动**真实二进制**跑完整烧录流程：

```sh
python3 tests/pty_test.py ./build/newisp
```

**期望输出**：

```
PASS
exit code      : 0
saw wakeup 0x7F: True
erase seen     : True
blocks written : 4
options written: True
```

这个测试**只能跑在 Linux / macOS**（Windows 没有 PTY）。

### 辅助脚本

| 脚本 | 用途 |
|---|---|
| `scripts/wsl-build.sh` | WSL 里构建（支持语言参数、独立构建目录、`WSL_BUILD_DIR` 覆盖）|
| `scripts/wsl-tests.sh` | WSL 里跑单元测试 |
| `scripts/wsl-ptytest.sh` | WSL 里跑 PTY 协议测试 |
| `scripts/wsl-smoke.sh` | WSL 里跑冒烟测试 |
| `scripts/wsl-install.sh` | 把 WSL 构建装到交互用户可读的位置 |
| `scripts/wsl-env-report.sh` | 报告 WSL 构建环境（编译器、CMake、依赖）|
| `scripts/wsl-burn.sh` | WSL 里真机烧录 |
| `scripts/wsl-detect.sh` | WSL 里真机检测 |
| `scripts/wsl-udev.sh` | 装 udev 规则并修设备权限（需 sudo）|
| `scripts/wsl-device-check.sh` | 检查 WSL 里串口 / HID 设备是否可见 |
| `scripts/wsl-status.sh` | 汇总 WSL 环境与设备状态 |
| `scripts/wsl-usb.sh` | WSL 里列 USB 设备 |
| `scripts/usbipd-attach.ps1` | 把 USB 设备转发进 WSL（需管理员）|
| `scripts/win-build.ps1` | Windows 构建 |
| `scripts/win-smoke.ps1` | Windows 冒烟测试 |
| `tests/pty_diag.py` | 查看 tty 实际接受哪些 termios 设置 |
| `tests/pty_opts_dump.py` | 打印工具写入的选项字节（含解码后的频率和 trim）|

> `scripts/wsl-verify-forceserial.sh` 和 `scripts/wsl-verify-nooverclock.sh` 验证的是已删除的
> `-force-serial` 与旧超频行为，属于遗留脚本，可以忽略。

---

## 代码结构

```
include/newisp/
  IChannel.h            传输接口（Open/Write/Read/Flush/SetBaudRate/DTR/RTS）
  serial_channel.h      串口后端
  hid_channel.h         USB HID 后端（始终编入，不可关闭）
  stc_protocol.h        协议基类：报文封帧、等待/脉冲辅助函数
  protocol_stc89.h      STC89/90（ProtocolStc89 旧版 1 字节校验和，ProtocolStc89A）
  protocol_stc12.h      STC10/11/12（12A / 12B 变体）
  protocol_stc15.h      STC15（15A 变体）
  protocol_stc8_uart.h  STC32/STC8G/STC8 串口实现
  protocol_stc8_hid.h   STC32/STC8G/STC8 USB HID 实现
  protocol_factory.h    芯片名 → 协议
  chip_table.h          芯片 ID（magic）→ 型号名，1300 多条
  chip_logic.h          系列判定、逐系列频率菜单、波特率表
  hex_file.h            Intel HEX 解析
  session.h             检测/烧录流程
  options.h             命令行参数
  i18n.h                消息枚举 Msg、Tr() 格式化、日志类型标记
  text_en.inc           英文文案表（基准表，也是 Msg 枚举的唯一来源）
  text.h                UTF-8 文本工具、十六进制格式化、单调时钟

src/core/               平台无关：芯片表、芯片逻辑、HEX 解析、i18n 选表
src/i18n/               各语言译文表（text_zh_cn.inc 等）
src/transport/          平台相关：串口（Win32 或 termios）、USB HID
src/cli/                参数解析、会话驱动、main
tests/                  单元测试和 PTY 协议测试
```

协议层只依赖 `stcisp::IChannel` 接口，**从不提及具体传输方式**。

STC32/STC8G/STC8 这三个系列**每种传输方式有独立实现**，因为它们的
状态包帧格式和校准流程有实质性差异。其余系列（STC89/12/15）只有串口实现。

**日志类型随行携带**：报文转储用 `%p`、需要提醒的结论用 `%a` 作为前缀标记，打印前剥离。
这样日志过滤与界面语言无关——原先按**中文前缀**过滤的写法一旦界面被翻译就会失效。
### 传输后端

| 平台 | 串口 | USB HID |
|---|---|---|
| Windows | `CreateFile` + `DCB` + `EscapeCommFunction` | `setupapi.dll` + `hid.dll`（系统 HID 类驱动）|
| macOS | `termios` + `TIOCMGET`/`TIOCMSET` | hidapi（IOKit 后端）|
| Linux | `termios` + `TIOCMGET`/`TIOCMSET` | hidapi（hidraw 后端）|

串口枚举也是各平台一套：Windows 走 SetupAPI 端口类，macOS 走 IOKit 的
`IOSerialBSDClient` 服务，Linux 扫 `/dev` 并用 sysfs 交叉验证。Windows 上还会从设备实例 ID
解析出 VID/PID，并把系统给的通用 `usbser` 名字升级成 HID 侧上报的 USB 产品名，
输出成 `STC USB Serial (COM12)` 这种更好认的形式。

**两个平台无关的细节值得说明**：

1. **打开串口后会立刻清掉 DTR 和 RTS**。多数平台打开串口时默认拉高 DTR，
   而很多 USB 转串口芯片（CH340、CP2102、FT232）的 DTR 就是接在板子复位脚上的，
   不清掉的话复位脉冲还没发，板子就已经被按住复位了。

2. **校验位是尽力而为**。有些 tty 设备（尤其伪终端，以及部分 USB 桥驱动）会用 `EINVAL`
   拒绝 `PARENB`。这时波特率和数据格式照样生效，端口记录**实际生效的**校验位，
   `SetParity` 也如实返回结果而不是谎报成功。老 STC 系列本来就是 8N1。

---

## 硬件相关说明

### 时钟范围

**没有全局统一的 IRC 频率表**：每个系列都有自己的菜单（来自 AiCube-ISP），
选频时按该系列的菜单过滤，而不是按一个数值窗口：

| 系列 | 菜单条目 | 上限 |
|---|---|---|
| STC8G | 12 | 35.000 MHz |
| STC15 | 12 | 35.000 MHz |
| STC8A（含 8F / 8C / GX8S）| 16 | 45.000 MHz |
| STC8051U | 16 | 45.1584 MHz |
| STC8H | 16 | 45.1584 MHz |
| STC32G144K246 | 15（厂商菜单 17，2 档无 trim）| 60.000 MHz |

STC10/11/12 与 STC89/90 **没有频率菜单**，这些系列不接受 `-freq`。

各系列菜单的完整列表、截图来源以及省略条目的理由见
[IRC 频率表](docs/irc-frequency-tables.md)。

**没有超频档**，因为菜单之外的那些频率没有可靠依据：

- 46、47MHz 在 AI8051U34K64 上实测通过
- **48MHz 实测会掉频**（不是升频），说明 IRC 物理上限略低于 48MHz
- 49、50MHz 是拟合曲线外推的，从未验证

**规则是"菜单成员判定"，不是数值范围判定**：

- 不在该系列菜单里的 `-freq` **明确报错**，并列出该系列真实的菜单档位：比如 45.100MHz 落在
  8H 的数值范围内，但它不是菜单档位，写进去就是以别人没要的速度烧选项字节
- 超过**所有**系列菜单的最高档位（60 MHz）在解析参数时就拒绝，连设备都不会去找
- 高于该系列上限、但恰好是上限档位本身时，会带着提示下调到上限——这是唯一保留的夹取行为
- 就算频率在菜单里，**该档位没有对应的 trim 数据**同样会被拒绝
- 8A 有个命名陷阱：`STC8A8K64S4A12` / `STC8A2K64D4` 是 8A 硅片（45.000MHz），
  而 `STC8A8K64D4` 是 8H 硅片（45.1584MHz），靠部件号而不是系列前缀区分
- STC32、STC8H、8051U 共用同一套协议实现（同一个探测槽位），而 60MHz 菜单只属于
  `STC32G144K246`。因此在一颗普通的 STC32G12K128 上，60MHz 档位会先被列出、再在选项阶段因为
  没有 trim 数据被拒绝

### 选项字节

选项字节包含看门狗、复位和时钟设置，写错可能让芯片无法启动。不要把它当作
「原样保留」的保证：选项块由各协议自带的常量和 trim 表拼接而成，并非芯片当前
选项的字节级拷贝。STC32 的限制已放开，这个提醒适用于工具会写入的所有系列——
烧录前先确认目标芯片的选项布局。

**但你不要求改的，工具不会动**：不给 `-freq` 时目标频率留空（`userTargetFreq = 0`），
协议层据此回退到握手状态包里上报的当前 IRC 频率；不给 `-eeprom` 时分割字段回写芯片上报的
`msr[4]`（`0xFF` = 未划分，同样原样保留）。这两处以前分别是"写死 24MHz"和"写死 `0xFE`"。

### 按系列校验的参数

工具不再对整类芯片"一刀切"拒绝，而是看目标系列**有没有这个字段**：

| 选项 | 适用系列 | 其余系列 |
|---|---|---|
| `-freq` | STC32 / STC8H / STC8G / STC8 / STC15 / 8051U 等有频率菜单的系列 | STC10/11/12、STC89/90 没有频率菜单，`-freq` 被拒绝 |
| `-eeprom` | STC32 / STC8H / STC8051U 以及其余 8 系列（STC8G / STC8F / STC8A / STC8C）| STC15 / STC12 / STC10 / STC11 / STC89/90 把 EEPROM 当作固定的独立区域，请求分割会**报错**，而不是被忽略 |
| `-clock` | 仅 STC10/11/12 有时钟源位 | 其余系列拒绝（STC89/90 没有该位；其他系列由 `-freq` 决定时钟）|

### 离线固件检查

`newisp inspect app.hex` 严格检查校验和、长度、记录格式、EOF、重叠和地址溢出，
保留完整的 32 位地址，不打开或枚举任何设备。输入上限为 16 MiB。检查成功不代表可以烧录。

写入器以 16 位地址寻址 64 字节块，所以所有系列（含 STC32）的烧录可用地址范围都是
`0x0000..0xFFFF`。`inspect` 本身不做这个限制：它会把超过 `0xFFFF` 的记录照常列出来，并显示
`Programming unavailable: <原因>`；而 `burn` 会在打开任何设备之前直接拒绝这类镜像，
既不会截断高位，也不会重映射。类型 03/05 的入口信息只作为元数据保留，不执行。

> `inspect` 的报告是**硬编码英文**，不经过文案表，所以中文构建下同样显示英文。

### 串口自动复位

如果板子**没有自动复位电路**（DTR 没接到复位脚），需要在握手过程中**手动给板子断电重上电**。

---

## 验证状态

本仓库实际验证过的内容，避免夸大：

| 平台 | 编译 | 单元测试 | 冒烟测试 | 真机验证 |
|---|---|---|---|---|
| Linux（Ubuntu 22.04 / GCC 11 / CMake 3.22）| 通过 | 全部通过 | 通过 | 通过：PTY 协议测试通过；AI8051U34K64 走 USB HID 烧录 45MHz |
| Windows（MSVC 19.44 / CMake 4.4）| 通过 | — | 通过：枚举到真实串口和 HID 设备 | 通过：AI8051U34K64 走 USB HID 烧录 24/40/45MHz，**并走串口烧录**；STC8H8K64U 串口 460800 |
| macOS（13.1 / AppleClang 14 / CMake 4.4）| 通过 | 待补 | 通过 | 通过：AI8051U34K64 走 USB HID 烧录 45MHz |

### 真机烧录验证

在真实硬件上验证，每次烧录退出码都是 0，全流程芯片应答正常：

| 步骤 | 结果 |
|---|---|
| 探测 | 识别出 `AI8051U34K64`，magic `0x78B4`，BSL 7.4U |
| `0x01` 波特率设置 | 应答 `01` |
| `0x05` 准备编程 | 应答 `05` |
| `0x03` 擦除 | 应答 `03`，返回真实 UID `78 B4 C9 28 09 FA 88` |
| `0x02`/`0x22` 写块 | 810 字节镜像 13 个块全部成功，每块应答 `02 54` |
| `0x07` 结束 | 应答 `07 54` |
| `0x04` 选项字节 | 应答 `04 54` |

三个目标频率都写入后用**重新探测读回**验证（芯片重新加载选项块，状态包里的频率计数器
会报告新时钟）：

| 目标 | `opts[24..26]` 计数 | 写入的 trim | 读回时钟 |
|---|---|---|---|
| 24 MHz | `0x016E36` | `48 20 01` | 24.000 MHz |
| 40 MHz | `0x02625A` | `73 30 01` | — |
| 45 MHz | `0x02AEA5` | `A6 30 01` | 44.999 MHz |

**44.999MHz 是 IRC 的正常误差**，官方 ISP 工具对同样的选项字节也报同样的值。

串口路径另在 **STC8H8K64U** 上验证：460800 波特率下 265 个块全部写入成功，
校准握手实测 trim `67 / 0x20 / 1`。

三个平台**读出的 UID 完全相同**（`78 B4 C9 28 09 FA 88`），证明协议实现一致。

### 8051U 确实有串口 ISP 监视器

官方手册把 8051U 系列列为「仅 USB HID」，早期版本据此在串口模式下直接拒绝：
探测到 8051U 就提示「请切换到 USB HID」。

这条规则是继承来的、从未实测过，而且**它是错的**。在 AI8051U34K64（BSL 7.4U）上，
串口 ISP 监视器会用 `0x50` 前缀的状态包应答 `0x7F` 唤醒字节，校准握手正常完成，
完整流程跑通：

```
[1/7] 握手成功
[2/7] 已切到 115200，trim_adj=90, trim_range=0x20, trim_freq=24031200 Hz
[3/7] 准备编程成功
[4/7] 擦除成功 → UID 78 B4 C9 28 09 FA 88
[5/7] 13/13 块
[6/7] 选项写入成功
[7/7] 烧录完成！
```

烧录后重新探测可以确认选项字节真的生效：状态包报告 24.031MHz，trim 三元组正是刚写入的值。

该限制已移除。**由探测决定，而非由手册决定**——芯片应答就说明能用；不应答就自然超时，
后续步骤也不会执行。手册与实测不一致这件事仍会被**提醒**（`[Attention]` 级日志），
但它只是提醒，不再是拒绝。

需要注意：串口和 HID 的状态包**布局并不相同**（串口带 `0x50` 前缀，报告的 MSR 字节也不同），
这也是两种传输方式从一开始就有各自协议实现的原因。

### 一个只有真机能发现的 Bug

移植过程中发现了一个**跨平台 HID 差异**，只有跑真机才能暴露：

| 平台 | `hid_read` 返回的首字节 |
|---|---|
| Windows（`hid.dll`）| **report ID**（HID 栈保留首字节），数据从 index 1 开始 |
| Linux（`hidraw`）| **数据本体**，没有 report ID 字节，数据从 index 0 开始 |

原来无脑跳过首字节，导致 Linux 上**整包偏移 1 字节**，`46 B9` 帧头错位 →
所有协议解析失败 → 报"未检测到芯片"。

**修复方式是不依赖编译期假设，直接看数据**：STC 报文一定以 `46 B9` 开头，
哪个位置有帧头就从哪开始。这个办法对三个平台都稳，也能容忍未来 hidapi 行为变化。

---

## 已知限制

- **帮助文本已与实现同步**：`-freq` 不再声称有 24MHz 默认值，无参数那行说明的是"只打印一行提示"，
  结尾注记写明**所有系列（含 8051U）都能走串口**，`-output` 不带已不存在的 `-o` 短形式，
  `list` 的排查提示指向 `-output verbose` 而不是已删除的 `-v`。
  仅剩一处英文报错仍写着 `use inspect -f FILE`（`hex_file.cpp` 里，属于非译文字面量）——
  正确写法是把镜像作为位置参数：`inspect app.hex`。
- **`inspect` 的报告不随语言变化**：它是硬编码英文。
- **PTY 协议测试只验证主机侧**的线路协议：封帧、校验和、命令序列、应答处理。
  它的假芯片只应答 STC32 系列的串口协议，所以老系列靠单元测试覆盖而非端到端测试。
- **PTY 测试只能跑在 Linux / macOS**，因为需要真实的终端设备对。Windows 的串口后端
  是完全不同的 API，没有对应实现。
- **只有 HID 传输在真机上验证过**。串口传输在 Windows（COM13，STC8H8K64U，460800）和
  Linux（WSL + usbipd，PTY 测试）上验证过，但**没有在实体 Linux 主机的串口上烧录过**。
- **部分实测数据早于当前的选项写入逻辑**：上面那张 24/40/45MHz 读回表是早期版本测出来的。
  选项构造的改动（不给 `-freq` / `-eeprom` 时保留芯片原值）只影响**未显式指定**它们的分支，
  显式指定频率的烧录路径不受影响——但这些数字之后没有重新实测过。
- **macOS 的单元测试还没跑**（编译和真机烧录都已通过）。
- **二进制不能"双击即用"**：这是命令行工具，且没有 Apple 开发者签名。
  发给别人需要去掉隔离属性（`xattr -d com.apple.quarantine newisp`）并确保可执行权限。
  Intel 二进制在 Apple Silicon 上还需要 Rosetta 2。
- **STC16 已不在型号表里**：探测仍能报出 ID，但没有型号名。
- **只非 USB 系列走 HID 时只会被提醒**：对 STC8G/STC8 这类非 USB 系列，`detect -h` 能探测成功，
  但工具会打印一条 `[Attention]` 提示"该系列文档未标明带 USB"，不会拒绝——这是为了不把可能的
  真机情况一刀切拒掉；需要硬性拒绝时由你自己判断。
- **镜像上限是 16 位块地址**：任何系列都无法写入地址超过 `0xFFFF` 的镜像（见
  [离线固件检查](#离线固件检查)）。

---

## 许可证

本项目采用 **GNU General Public License v3.0**，详见 [LICENSE](LICENSE)。
