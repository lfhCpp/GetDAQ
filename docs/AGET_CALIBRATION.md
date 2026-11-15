# AGET 串行校准与自动读取延迟：操作、原理和执行边界

本文说明已迁移的 AGET 初始化、串行控制校准、全局阈值和 SCR 自动延迟子流程。**完整生产 Prepare、ECC 恢复/网络入口、主机告警和真机验收仍未完成。** 普通实验采集继续使用现有 ECC；这里的离线命令不操作设备，新原生接口目前供后端组合使用。

## 1. 这几种“校准”有什么区别

| 名称 | 主要对象 | 新接口 | 执行后的实际边界 |
| --- | --- | --- | --- |
| AsAd 慢控制校准 | CoBo 与 AsAd 的控制连接 | CoBoAsAdCalibrate | 不是 AGET 校准，详见 [AsAd 校准](ASAD_CALIBRATION.md) |
| AGET 串行控制校准 | CoBo 经 AsAd 与活动 AGET 的串行控制时序 | CoBoAgetCalibrate | 执行实际 hit/控制器/ADC 测试序列，再核对每片型号 |
| AGET 初始化 | 串行控制、AGET 初值和 SCA 读出命令 | CoBoAgetInit | 包含上一项；未初始化正式 ADC 数据格式，也未完成 Prepare |
| ADC deskew / bit-slip | AsAd ADC 数据链路 | CoBoAsAdAdcInit | 使用另外的测试图样和两个 1000ms 等待，不由 AGET 校准替代 |
| SCR 自动读取延迟计算 | SCR 上升沿到数据抵达 CoBo 的延迟 | CoBoAutomaticReadDelayCalibrate | 生成新的延迟结果；没有真实电气测量验收 |
| 自动/手动延迟选择 | 使用哪个延迟来源 | CoBoSelectReadDataDelay | 只选择已存储的结果，不重新计算 |

AGET 型号读取正常，是串行控制校准的一项必要检查；它不能证明 ADC 波形、PLL 锁定或真实 SCR 数据延迟全部正确。`complete` 只描述这份计划定义的动作和检查完成。

## 2. 为什么 Qt 不影响这里的数据吞吐

目录和职责分为三层：

1. `common/config/calibrationcompiler.*` 使用 Qt Core 读配置、检查来源并导出 JSON。它没有设备连接，也没有原始事件接收。
2. `common/experiment/cobocalibrationconfiguration.*` 使用纯 C++11 计算活动掩码、校准片选择和延迟策略。没有 Qt、Ice、YAML 或其他第三方运行库。
3. `apps/hardware/cobocalibration.cpp` 生成原生命令计划，复用 HardwareClient 与本地 Ice/GET 兼容接口。公共头文件没有 Qt/Ice 类型。

高流量原始 MFM 数据仍由 `apps/datareceiver` 的独立进程接收、写盘和导出。这些校准是低频控制动作，不把事件流送入 Qt 或控制服务。200ms/1000ms 等待来自旧硬件流程，不能靠 GUI 更快刷新来缩短。

## 3. 先进行离线配置检查

构建 Qt 工程，获得 `getdaq-config`：

```bash
bash tools/build-linux.sh control
build-suite/bin/getdaq-config --help
```

使用收到的 pedestal 配置示例：

```bash
build-suite/bin/getdaq-config compile-cobo-calibration \
  source/get/GetBench-software-release-20210208/data/config/configure-pedestals.xcfg \
  0 '0=0x202,1=0x202,2=0x202,3=0x202' global \
  /tmp/pedestals-calibration-review.json
```

最后的文件必须尚不存在；重复运行不会覆盖已有结果。成功返回 0；配置/写文件错误返回 1；命令行版本表或布局格式错误返回 2。工具不把 JSON 作为硬件脚本执行，没有部署按钮。

命令参数依次是输入 xcfg、具体 CoBo 实例、版本表、延迟字段布局和新输出文件。CoBo 实例使用原配置的实际 `Instance id`，例如 `0` 或 `Crate00_Slot00`，不能写 `*`。版本表是调用者的**预期型号**，不是离线工具测得的实物型号。

全局芯片编号为 `4 × AsAd编号 + AGET位置`。例如 AsAd0 的位置2为2，AsAd1 的位置1为5，AsAd3 的位置3为15；对应版本表可写 `'2=0x201,5=0x202,15=0x203'`。只允许旧实现认识的 `0x201/0x202/0x203`，列表必须恰好覆盖全部活动芯片。

Windows 用户可在 Qt Creator 使用现有 qmake 工程构建；命令格式相同，将可执行文件换为本机 `getdaq-config.exe`，路径按实际情况填写。用户的 Qt 5.14.2 MinGW 是目标环境，当前本机验证仍为 Linux Qt 5.12.8，尚未实际完成 Windows 构建验收。

## 4. 配置字段和默认值

下表路径相对当前 CoBo 实例。标量沿用共用单位、类型、声明范围、include 和默认继承检查。

| xcfg 路径 | 允许值/默认 | 用途 |
| --- | --- | --- |
| isActive | 必须 true | 本次编译的 CoBo 活动 |
| AsAd[n]/Control/isActive | 必须声明 true/false | 活动 AsAd 位图 |
| AsAd[n]/Aget[k]/Control/isActive | 活动 AsAd 下四片均须声明 | 活动芯片位图 |
| AsAd[n]/Control/calibrationAgetId | 0..3，缺省0 | 0自动选择第一片活动芯片；1..3明确选择对应位置 |
| Module/calibrateAutoReadDataDelay | true/false，缺省true | 是否重新计算自动延迟 |
| AsAd[n]/Control/enableAutoReadDataDelay | true/false，优先于 Module | 当前板使用自动延迟还是手动延迟 |
| Module/enableAutoReadDataDelay | true/false，板级值缺失时使用；两处均缺省为false | 模块级默认选择 |

必须声明四个具体 AsAd ID 0..3；每块活动 AsAd 必须声明四个 Aget ID 0..3。只检查活动芯片的型号，不读取完整 AGET Global/68 通道配置，因此这个子编译可以用于最小校准配置。至少需要一块活动板和一片活动芯片。

不活动 AsAd 的 AGET 内容不参与型号覆盖；活动 AsAd 中没有活动芯片则明确标为未串行校准，不用回退到0伪装成功。对这种板设置非零 calibrationAgetId 会失败。

`calibrationAgetId=0` **不是强制选择位置0**。如果只有位置2活动，0会选择2。非零请求必须指向活动芯片，否则拒绝。旧程序允许无效请求继续，新的检查会在生成硬件计划之前报错。

示例：

```xml
<Module>
  <calibrateAutoReadDataDelay>false</calibrateAutoReadDataDelay>
  <enableAutoReadDataDelay>true</enableAutoReadDataDelay>
</Module>
<AsAd id="0">
  <Control>
    <isActive>true</isActive>
    <calibrationAgetId>0</calibrationAgetId>
    <enableAutoReadDataDelay>false</enableAutoReadDataDelay>
  </Control>
  <!-- 其余 Aget 声明仍需提供；这里只展示策略字段。 -->
</AsAd>
```

这里不重新计算，AsAd0使用手动延迟；其他活动板继承自动延迟。能否表达这种差异取决于下一节的字段布局。布尔值写 `true/false`，不用1/0；芯片 ID 使用0..3的单字符整数，不接受02、负数或带单位值。

## 5. global 与 per-asad 怎么选

| 命令行布局 | 原生字段 | 策略约束 |
| --- | --- | --- |
| global | pipeCtrl.SCRAutoDelayEnable | 所有活动 AsAd 必须使用相同最终策略；只写一次 |
| per-asad | pipeCtrl.SCRAutoDelayEnable0..3 | 逐活动板写实际编号，可各板不同；不写不活动板 |

共同字段只有一个开关，无法表示“一块手动、另一块自动”。新编译明确拒绝这种冲突，不采用旧程序的最后一块覆盖前面板的行为。

旧异常分支虽然计算了带 AsAd 编号的字符串，实际写入却固定为 `SCRAutoDelayEnable0`。新逐板计划使用每块板的正确字段。**收到的完整硬件描述只证明 global 布局，per-asad 的实物固件覆盖尚未验证**；它目前通过的是类型化规则和模拟 TCP 测试。必须用真实匹配的硬件描述和板端验收决定布局，不能把字段名字存在的历史注释当作固件支持证明。

布局是调用者在执行前作出的明确选择，没有先写 global、失败后再尝试 per-asad 的路径。超时可能代表写入已生效，不能据此猜测字段缺失。

## 6. 离线结果怎么看

JSON 的 `kind` 为 `getdaq.cobo-calibration-image`，主要字段：

- `asad_mask` / `aget_mask`：活动板/芯片位图；例如板0、1、3为11，芯片2、5、15为32804。
- `calibration_asad_mask`：有活动芯片可进行串行校准的板，可能少于活动板位图。
- `first_calibration_global_chip`：串行阶段前三次观察所用的有效读取选择器。
- `boards[].requested_calibration_aget_id` / `calibration_aget_id`：请求与最终选择；无芯片时最终值是 null。
- `compute_automatic_read_delay` 与 `automatic_read_delay_mask`：计算开关和使用策略，二者独立。
- `delay_layout` / `delay_policy_path`：明确布局与每板实际配置查询路径。
- `chips[].expected_version`：期望型号；原生串行校准结束后才检查实物读值。
- `parameters` / `sources`：继承或代码默认值、来源文件/行及 SHA-256。
- `complete_prepare` / `hardware_deployment_supported` / `physical_delay_verified`：当前均为false。

`calibration_aget_register=true` 是 CLI 所选的正常新布局前提。确实没有这个历史寄存器的旧板，可由类型化调用者明确设置 false；此时仅支持每块校准片0，禁止非零选择。该选项不是写入失败后的自动降级，CLI 当前没有此旧板开关。

## 7. AGET 串行校准的动作和影响

CoBoAgetCalibrate 先检查停止位和绑定描述，明确设置 AsAd 广播范围及第一片有效读选择器。正常布局为全部四块 calibrationAget.AsAdn 写入最终选择，不活动板为0。随后保留旧动作：

1. ADC0=0x400复位；ADC2=0x2a0、ADC3=1、ADC4=0、ADC5=0x401，选择 DDR 校准测试向量。
2. 快时钟分频设0，复位 AGET 控制器，设置 SC_En/pulseEnable 活动掩码，关闭 hitRegister/calibration，等待200ms。
3. 读取 reg5，再以不同 calibration/pulseEnable 状态复位和读取。
4. 在旧定义的两种控制状态下各写一次 `hit=0xA000000000000005`，再读取 reg5并复位。
5. 执行慢控制切换，保留1000ms等待；逐活动芯片选择并读 reg5，低16位必须等于调用者预期型号。

前三次 reg5 读取是校准动作中的观察，原始值写入日志，没有给它们编造固定期望。最后逐片检查比旧“只检查是否为已知型号”更严格：实际为0x203而预期0x202也会失败。

hit向量的最高位为1，必须完整保留64位；不能转为 JSON double 或经有符号数值转换丢失高位。新执行器使用64位位模式传输，日志使用十进制字符串，固定向量为 `11529215046068469765`。

ADC0复位会清除之前的 ADC 通道配置；结束后仍是 ADC 测试向量。**必须随后执行正常 ADCInit，再重应用最终ADC通道位**；新 [CoBoPrepareClient](COBO_PREPARE_CLIENT.md) 已按此顺序组合，不能把“校准完成”当作 ADC 已恢复普通采样格式。广播 ADC 仍面向所有活动 AsAd，只有有活动芯片的板会报告串行校准覆盖。

CoBoAgetInit 在上述动作前增加控制器0→1→0、广播 SC_En、AsAd resetFast；成功后沿用旧 `aget.reg1.DAC=1` 和 initSCAReadOutAfterPowerOnNow=1。后者是命令寄存器，不能要求回显1。

## 8. 全局阈值为什么使用 DACthreshold

旧 setGlobalRegThresholds 只改 reg1 的第19..21位。收到硬件描述中 `DAC` 宽4位，额外包含第22位极性；`DACthreshold` 宽3位。CoBoAgetGlobalThreshold(settings,value) 使用后者，value范围0..7，以保留极性和其他 reg1 位。

计划先进入慢控制，再逐活动芯片设置单片 SC_En 和读取选择器，执行板端字段写入并显式检查阈值，最后恢复完整 SC_En。旧代码由主机先读整寄存器、改3位再写，新代码复用板端已有字段操作；收到的 DynRegister::fieldWrite 也采用读、修改字段、写回。控制必须独占，这不是原子跨客户端更新。

CoBoAgetInit 的 `DAC=1` 则明确保持旧4位字段操作，两者用途不同。初始化的广播字段操作会以当前读取选择器的寄存器为基值，不能当作逐片保留各自正式参数；它本来就在最终 Configure 之前。

## 9. 自动延迟计算与使用是两件事

compute=true 时执行：广播 SC_En，写 reg1=0x3e3a41a7、reg2=0x0028a197；使能所选布局的自动延迟字段和 SCRAutoDelayCalibrate；关闭中断，设 SCR/SCW=0、override=1和reset=0；临时 firmwareEnable=1，SCW=1并等待200ms；SCW=0、SCR=1→0，再等待200ms；最后 firmwareEnable=0、校准位=0、override=0，并显式核对三者。

临时 firmwareEnable 是旧 Prepare 内部生成校准信号的动作，不提交 ECC Start，不启动接收服务。计算完成后自动延迟开关暂时为1，必须随后通过独立 CoBoSelectReadDataDelay 应用最终策略。

| compute | use | 含义 |
| --- | --- | --- |
| true | true | 计算新结果，选择自动 |
| true | false | 仍计算新结果，最终选择手动 |
| false | true | 不计算，选已有自动结果；有效性必须由实际状态核对 |
| false | false | 不计算，选择手动 |

compute=false 的计算子计划只检查前提，不发送校准写入；仍可运行选择子计划。选择成功不证明已有结果有效。reg1/reg2被计算覆盖后，正式实验 Configure 必须重新设置它们；不能运行中重新校准。

## 10. 后端组合和失败处理

这些接口全部返回 `std::vector<HardwareStep>`，不直接操作硬件。完整协调器应先编译所有设置，再通过 AppendHardwareSteps 组合全部子流程，最后调用一次 Execute，使末尾描述/权限错误在首个动作前拒绝。已有上电子计划含条件索引，不能直接 vector::insert 拼接。

```cpp
std::vector<GetDAQ::HardwareStep> steps;
GetDAQ::AppendHardwareSteps(steps, GetDAQ::CoBoAgetInit(calibration));
GetDAQ::AppendHardwareSteps(steps, GetDAQ::CoBoAsAdAdcInit(calibration.asadMask));
// DAC、硬件保护、主机告警和监控仍由完整 Prepare 按旧次序加入。
GetDAQ::AppendHardwareSteps(steps, GetDAQ::CoBoAgetGlobalThreshold(calibration, 2));
GetDAQ::AppendHardwareSteps(steps, GetDAQ::CoBoAutomaticReadDelayCalibrate(calibration));
GetDAQ::AppendHardwareSteps(steps, GetDAQ::CoBoSelectReadDataDelay(calibration));
// 仅展示片段组合；不是完整 Prepare，没有 Prepared 提交。
```

执行要求已绑定匹配硬件描述、无采集、中断禁用、时钟/电源初始化完成、接收/相关读取停止、独占控制和没有待核对的未知操作。firmwareEnable/intEnable检查只验证两个当前控制位，不能替代这些前提。

整个计划先检查元数据、权限、字段、位宽及有界步骤/等待。缺末尾 SCA 命令、hit只读或缺自动延迟字段时不能先复位 ADC、覆盖reg1/reg2再尝试另一布局。型号不符属于执行后实物结果，发生时前面的校准可能已经生效；停止不意味着零副作用。

每次执行使用新的独占日志路径，持久化 intent/begin/结果。写入生效但应答丢失为 UNKNOWN：不重发、不回滚、不继续，也不自动补写“恢复”命令。例如 firmwareEnable=1后断线，板端可能仍开启固件及校准位；自动发0会掩盖未核对状态。保留日志和现场，按完整生产恢复核对真实状态。当前恢复后端尚未完整实现，不能直接把旧日志重放作为恢复。

## 11. 验证及仍需完成的部分

可复现的检查：

```bash
bash tools/build-linux.sh experiment
(cd build-experiment && ctest --output-on-failure)
bash tools/build-linux.sh ice
(cd build-ice-compat && ctest -R hardware-calibration-tcp --output-on-failure)
python3 tests/cobo_calibration_integration.py build-suite/bin/getdaq-config
python3 tests/cobo_calibration_original_reference.py
```

纯核心检查15种板掩码、活动子集和显式校准片、全部延迟策略/覆盖拒绝。CLI检查原配置、最小include、默认/来源、布局冲突及失败不覆盖。TCP检查稀疏广播、ADC自清零、两次高位hit、预期型号、阈值极性保留、逐板字段和临时固件使能断线只发送一次。原码对照提取实际 agetCal/自动延迟函数体，使用记录存根比较36个串行动作及20个自动延迟动作；新增版本/描述检查由其他测试验证，不把存根测试说成完整旧后端验收。

完整回归、严格C++11和内存检查结果见 [验证记录](VALIDATION.md)。没有修改 MUSIC、收到的 source、原 PDF 或数据接收热路径。单板组合、自有告警和ADC最终通道代码已接CoBoPrepareClient；全系统Prepare/生产ECC、告警驱动Stop/排空、MuTaNT系统组合、逐板延迟真实固件支持、ADC最终通道电气效果和Ubuntu24/Windows/macOS/硬件验收仍需继续。

## 12. 源码依据

依据收到的 CEA GET CoBoNode.cpp 的 agetInit、coboSetCalibrationAgetChips、agetCal、printAgetChipVersionNumbers、setGlobalRegThreshold(s)、calibrateAutoReadDataDelay 和 selectReadDataDelay；配置优先级来自 CoBoConfigParser.cpp；DAC/DACthreshold 位宽来自 hardwareDescription_fullCoBoStandAlone.xcfg；字段读改写来自 Mordicus DynRegister.hpp。原 CeCILL 声明与源码保持在 source，许可副本位于 third_party/licenses/getbench/COPYING。
