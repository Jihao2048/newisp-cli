# 未提交改动总结

基于工作区当前未提交状态（`git diff`）整理，上一笔提交 `ff9a8dc` 之后的增量。

- 已修改文件：10 个，+162 / −67
- 无新增文件、无删除文件

---

## 一、一句话概括

这次改动全部指向同一个方向：**不再擅自改写用户没有指定的参数**。

具体是四件事：

| # | 改动 | 之前 | 现在 |
|---|---|---|---|
| 1 | `-output` 默认级别 | `normal` | `quiet` |
| 2 | 未给 `-freq` 时的频率 | 写 24 MHz 默认值 | 保留芯片当前频率 |
| 3 | 未给 `-eeprom` 时的分割 | 写 `BLOCK2` 固定的 `0xFE` | 回写芯片上报的 `msr[4]` |
| 4 | HID 下 `detect` 结束时 | 发 0xFF 复位 + 运行 | 留在 ISP 模式不复位 |

配套还修正了一处 `-s` 的语义和四份文档。

---

## 二、输出级别默认改为 quiet

### 改动

| 文件 | 内容 |
|---|---|
| `include/newisp/options.h` | `OutputLevel` 枚举注释重写；`CliOptions::output` 默认 `Normal` → `Quiet` |
| `src/cli/main.cpp` | `Logger::m_level` 默认 `Normal` → `Quiet` |
| `src/cli/options.cpp` | `ParseCommandLine` 内局部默认值同步为 `Quiet` |
| `include/newisp/text_en.inc` | `UsageOutputLevels` / `UsageOutputDefault` 改写 |
| `src/i18n/text_zh_cn.inc` | 同上，中文同步 |
| `tests/test_options.cpp` | 断言默认值由 `Normal` 改为 `Quiet` |

### 同时移除的隐式升级规则

原来 `main.cpp` 里有一条自动升级：

```cpp
// 删除前
const newisp::OutputLevel level = (!opts.outputExplicit && !tty)
    ? newisp::OutputLevel::Verbose
    : opts.output;
```

即「输出被重定向且没写 `-output` 时，自动当 verbose 处理」。这条规则被**整体删除**，现在的逻辑是：

```cpp
// 删除后
const newisp::OutputLevel level = opts.output;
```

新的理由写在代码注释里：*"no -output" means quiet, and a script that wants the handshake trace can say "-output verbose" explicitly.* —— 默认输出少的可预测性，比猜测用户想看更多更重要。

### 帮助文本变化

| 条目 | 英文 | 中文 |
|---|---|---|
| `UsageOutputLevels` | `quiet (default): errors and the summary only;` | `quiet（默认）：只显示错误和最终结果；` |
| `UsageOutputDefault` | `normal: adds progress; verbose: raw packets` | `normal：加进度行；verbose：含原始报文` |

---

## 三、未给 -freq 时保留芯片当前频率

这是本次行为差异最大的一处。

### 问题

`BurnOptions::targetFreq` **始终有 24 MHz 的默认值**，所以以前即使命令行只写了 `newisp burn app.hex`，工具也会把目标频率当成 24 MHz 写进选项字节 —— 等于**默默重编程了一个用户从未要求改动的振荡器**。

### 改动

| 文件 | 内容 |
|---|---|
| `src/cli/session.cpp` | `proto->m_status.userTargetFreq = opts.freqExplicit ? opts.targetFreq : 0`；未给时记录 `FreqFromChip` |
| `src/cli/main.cpp` | 回显条件由 `if (opts.burn.targetFreq)` 改为 `if (opts.burn.freqExplicit)` |
| `include/newisp/text_en.inc` | 新增 `FreqFromChip` |
| `src/i18n/text_zh_cn.inc` | 新增中文对应条目 |

`session.cpp` 中的关键改写（`RunBurn` 段）：

```cpp
// 之前：无论是否显式指定都走解析流程
uint32_t targetFreq = opts.targetFreq;
if (targetFreq) {
    targetFreq = ResolveFrequency(chipName, targetFreq, log);
    ...
}

// 现在：只有显式指定才解析，否则 0 表示"沿用芯片值"
uint32_t targetFreq = 0;
if (opts.freqExplicit) {
    targetFreq = ResolveFrequency(chipName, opts.targetFreq, log);
    if (targetFreq == 0) { closeCh(); return 1; }
}
else {
    log(Tr(Msg::FreqFromChip, { FormatDouble(st.clockHz / 1e6, 4) }));
}
proto->m_status.userTargetFreq = targetFreq;
```

`userTargetFreq == 0` 在协议层已有既定含义：**保持芯片上报值**。每个握手状态包都会在 `clockHz` 里带上芯片当前 IRC 频率（见 `ParseStatus`），`ProgramOptions` 会回退到它。

### 新增消息

| id | 英文 | 中文 |
|---|---|---|
| `FreqFromChip` | `[Frequency] -freq not given; keeping the chip's current {0} MHz` | `[频率] 未指定 -freq，沿用芯片当前的 {0} MHz` |

### 回显行为

`main.cpp` 里 `目标频率 :` 这一行改成只在显式给出时打印，注释写明了原因：默认的 24 MHz 不会再被写入，打印它等于宣称一个不会发生的改动。

---

## 四、未给 -eeprom 时保留芯片当前 EEPROM 划分

### 问题

`include/newisp/protocol_stc8_hid.h` 的 `ProgramOptions` 里，`BLOCK2` 常量自带一个 `opts[39] = 0xFE`。以前 `-eeprom` 未给时，这个 `0xFE` 会被原样写入，等于**声明约 64 KB EEPROM、几乎没有程序区** —— 一个没有任何人要求过的划分。

### 改动

```cpp
// 新增分支
else {
    opts[39] = m_status.msr[4];
}
```

保留的是握手状态包里上报的当前分割值；`msr[4] == 0xFF` 表示"未划分"，同样按原样保留。

注释解释了为什么不能只依赖 `BLOCK2`：*"BLOCK2 alone would write its fixed 0xFE, i.e. claim ~64 KB of EEPROM and very little code -- a split nobody asked for."*

> 注意：只有 HID 协议的这份实现补了这一分支。`protocol_stc8_uart.h` 中的串口实现本来就带 `msr[4]` 回写逻辑，未受影响。

---

## 五、detect 在 HID 下不复位芯片

### 问题

`detect` 结束时会发一个 `0xFF` 复位包让芯片跑用户程序。这在串口上是对的，但在 **USB HID 上是错的**：芯片一旦退出 ISP 监视器，USB 设备就会从总线上消失，接着 `burn` 会报"未找到 STC USB ISP 设备"。

### 改动

| 文件 | 内容 |
|---|---|
| `src/cli/session.cpp` | 复位包只在 `!opts.useHid` 时发送；HID 分支打印提示 |
| `include/newisp/text_en.inc` | 新增 `DetectHidLeftInIsp` |
| `src/i18n/text_zh_cn.inc` | 新增中文对应条目 |

```cpp
if (ch->IsOpen()) {
    if (!opts.useHid && !probeList.empty()) {
        auto reset = probeList.front()->ProbeBuild({ 0xFF });
        if (!reset.empty()) ch->Write(reset);
    }
    ch->Close();
}
if (opts.useHid) {
    log(Tr(Msg::DetectHidLeftInIsp));
}
```

| id | 英文 | 中文 |
|---|---|---|
| `DetectHidLeftInIsp` | `[detect] chip left in its ISP monitor (not reset), so a burn can follow` | `[detect] 芯片仍停在 ISP 模式（未复位），可直接接着烧录` |

---

## 六、detect 的 -s 不再接收波特率

### 说明

`detect` 的握手是**固定 2400 波特率**且从不切换（见 `OpenSession`），所以给 `-s` 带一个数字对它没有任何意义。

### 改动

| 文件 | 内容 |
|---|---|
| `src/cli/options.cpp` | `-s` 后跟数值时，`detect` 下只消费不赋值 |
| `include/newisp/text_en.inc` | `UsageSerialBaudForms` 改写 |
| `src/i18n/text_zh_cn.inc` | 同上 |
| `tests/test_options.cpp` | 新增用例 `detect -s 9600 -d COM7` |

```cpp
if (ParseRate(argv[i + 1], rate)) {
    ++i;
    if (out.command != Command::Detect)
        out.burn.baudRate = rate;
}
```

设计取向是**消耗而不报错**：`-s 921600` 是用户习惯的写法，没必要为一个不影响结果的参数报错。

新增测试断言：

```cpp
std::vector<const char*> a = { "detect", "-s", "9600", "-d", "COM7" };
CHECK_EQ(Parse(a, o, e), 0);
CHECK(!o.useHid);
CHECK_STREQ(o.burn.deviceId, "COM7");
CHECK_EQ(o.burn.baudRate, 115200u);   // 保持默认，detect 不使用
```

帮助文本变化：

| 条目 | 英文 | 中文 |
|---|---|---|
| `UsageSerialBaudForms` | `e.g. -s 921600, -s 115.2k; detect ignores it (fixed 2400)` | `如 -s 921600、-s 115.2k；detect 固定 2400，忽略此值` |

---

## 七、文档同步（README.md / README.zh-CN.md）

| 位置 | 改动 |
|---|---|
| 频率上限说明 | 删除"统一 45 MHz 上限"，改为**逐系列上限表格**并链接 `docs/irc-frequency-tables.md` |
| 频率校验规则 | 明确区分：**不在菜单则报错**，**超过上限则下调**（保留的唯一夹取行为） |
| STC32 状态 | 删除"STC32 烧录被拒绝"段落；改为"限制已放开，但选项块非字节级拷贝，烧录前确认目标选项布局" |
| EEPROM 分割 | 从"不支持的系列会被忽略"改为"**会被拒绝**"，并列出支持与不支持的系列清单 |
| `inspect` 地址范围 | 改为"16 位寻址 64 字节块，所有系列（含 STC32）可用范围均为 `0x0000..0xFFFF`"；删除指向 `docs/stc32-development.md` 的链接 |
| 示例命令 | `detect --device /dev/ttyUSB0` → `detect -s /dev/ttyUSB0`（`-s` 后跟设备名，与新语义一致） |

逐系列上限表（中英两份一致）：

| 系列 | 上限 |
|---|---|
| STC8G / STC15 | 35 MHz |
| STC8A | 45.000 MHz |
| STC8051U / STC8H | 45.1584 MHz |
| STC32G144K246 | 60 MHz |

---

## 八、行为变化汇总（对使用者可见）

1. **`burn` 的默认输出变少了**：默认只有错误和最终结果，要进度行需 `-output normal`。
2. **重定向不再自动变 verbose**：日志不再凭空多出握手全过程。
3. **不带 `-freq` 烧录不再把 IRC 改成 24 MHz**：现在保持芯片当前频率，并打印一行说明。
4. **不带 `-eeprom` 烧录不再把分割改成 0xFE**：现在保持芯片当前分割。
5. **HID 下 `detect` 之后芯片停在 ISP 模式**：`detect` 接着 `burn` 可以连着做；若想让它跑原程序，需要断电重启。
6. **`detect -s <数值>` 不再报错也不生效**：数值被忽略，握手固定 2400。

---

## 九、文件清单（10 个）

```
README.md
README.zh-CN.md
include/newisp/options.h
include/newisp/protocol_stc8_hid.h
include/newisp/text_en.inc
src/cli/main.cpp
src/cli/options.cpp
src/cli/session.cpp
src/i18n/text_zh_cn.inc
tests/test_options.cpp
```

改动分布：`src/cli/session.cpp` 与两份 README 占比最大，其余为默认值同步、消息表新增两条、帮助文本措辞。
