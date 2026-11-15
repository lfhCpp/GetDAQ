# CoBo PLL 与 AsAd LCM 时钟计算

## 1. 时钟与数据传输有什么关系

AGET 先按写时钟保存探测器信号，再按读时钟送出采样值。时钟影响实际采样间隔、ADC 相位和板间同步；它与 TCP 吞吐、Qt 窗口刷新是不同问题。不能用网络收到了字节来证明时钟或 ADC 校准正确。

| 名称 | 作用 |
| --- | --- |
| CKW | CoBo 提供的写/采样时钟 |
| WCKn | AsAd LCM1 调整后送给第 n 个 AGET 的采样时钟 |
| CKR | CoBo 提供的读时钟 |
| RCKn | AsAd LCM2 调整后送给 AGET 的读时钟 |
| ACK | LCM2 调整后的 ADC 时钟 |
| PLL | 锁相环；µTCA CoBo 用 LMK04806 分配时钟 |
| LCM | AsAd 本地时钟管理器；LCM1 管写时钟，LCM2 管读时钟/ADC 相位与延迟 |

**当前已有整板纯 C++11 时钟编译、离线 xcfg/CLI 和原生库 ClockInit 命令子流程，完整 Prepare 尚未完成。** 计算库 common/experiment/coboclockconfiguration.h/.cpp 不依赖 Qt/Ice/YAML/Boost；Qt Core 仅解析配置与生成离线工件，原生 apps/hardware/coboclock.cpp 通过隔离的 Ice 兼容层执行命令。尚未连接实验设备，也没有 PLL 锁定、实际频率或相位测量、生产 ECC 提交及 JSON 硬件部署入口。

## 2. 构建与调用

    bash tools/build-linux.sh experiment
    (cd build-experiment && ctest --output-on-failure)

主要接口：

    CompileAsAdSamplingClock(ckwMHz, samplingDivider, expertFields)
    CompileAsAdReadingClock(ckrMHz, phaseDegrees, delayPs, expertFields)
    CompileCoBoPll(writingMHz, rawOverrides)
    CompileReducedCoBoWritingDivider(writingMHz)
    CompileCoBoClock(typedSettings)

默认 expertFields/rawOverrides 是空 map。计算不读取硬件，不写配置文件。输入非法会抛出 std::invalid_argument；返回工件后仍需检查输出的名义值及标志，不能把返回成功理解为实机频率一致或 PLL 已锁定。

单独对照收到的旧代码：

    python3 tests/cobo_clock_original_reference.py

这个测试从保留的 CEA GET asad/utl.cpp、CoBoPLLManager.cpp 和 CoBoConfigParser.cpp 提取原始 LCM 函数、PLL 数组/分频/覆盖/同步算法与命名字段解析，在临时目录中编译为独立参考程序，再与新计算比较。旧算法没有被改成调用新函数；只替换日志、集合构造、位字段工具和异常/FIFO 测试外围，原始文件字节保持不变。不安装 CCfg/Boost，不连接网络或设备。Python 和 C++ 编译器只用于此开发验证，不成为软件运行依赖。

## 3. 为什么 LCM 不是普通 64 位字段

LCM 线上寄存器实际是 80 位。旧 AsAdBus 设备策略会在 64 位表示中补入固定 0x0001 两字节。新计算仍返回这一 64 位表示，不能直接把它当作任意总线上的完整 10 字节数据。

LCM 每个参数的位序还要反转。例如 FINDIV 的七位数值先反转这七位，再放到 bit57..63；这不等于整字节倒序或整个 64 位字倒序。新实现使用有界无符号运算，专家字段越界时拒绝，避免静默截断。

| LCM1 字段 | 位置/宽度 | LCM2 字段 | 位置/宽度 |
| --- | --- | --- | --- |
| FINDIV | 57 / 7 | FINDIV | 57 / 7 |
| FBDIV | 50 / 7 | FBDIV | 50 / 7 |
| OADIV | 45 / 5 | OADIV / OBDIV | 45 / 5、40 / 5 |
| OAMUX | 32 / 3 | OAMUX / OBMUX | 33 / 2、30 / 2 |
| FBSEL | 24 / 2 | FBSEL | 24 / 2 |
| FBDLY | 19 / 5 | FBDLY | 19 / 5 |
| xDlySel | 18 / 1 | xDlySel | 18 / 1 |
| DlyGLA | 13 / 5 | DlyGLA / DlyGLB | 13 / 5、8 / 5 |
| VCOSel | 3 / 2 | — | — |

## 4. LCM1 采样频率

CKW 必须是有限的 1..200MHz。samplingDivider 必须为正的有限数，目标 WCK=CKW/samplingDivider 也必须在 1..200MHz。保留旧 float 算法和六种标准频率的特殊参数：

| CKW=WCK | 64 位参考值 |
| --- | --- |
| 100MHz | 0x48900001020000cf |
| 50MHz | 0x91200001020000d7 |
| 25MHz | 0x20400001020000c7 |
| 12.5MHz | 0x41420001020000c7 |
| 6.25MHz | 0x81c30001020000c7 |
| 3.125MHz | 0x01c38001020000c7 |

还保留 100MHz/100、50MHz/50 生成 1MHz 的两个特殊参数。其他输入使用旧通用搜索，按整数 N/M/U 编码：

    nominal WCK = CKW × (FBDIV+1) / ((FINDIV+1) × (OADIV+1))

通用搜索不保证任意目标精确可达。名义值由寄存器分频模型计算，**不是测量值**。例如旧算法对 CKW=1MHz、divider=1 的 N 钳制会得到低于 1MHz 的名义输出，新库保留参考寄存器值但把 nominalMatchesRequested 和 nominalWithinAgetRange 明确置为 false。

调用者必须检查 nominalOutputKnown、nominalMatchesRequested、nominalWithinAgetRange。完整部署入口不得忽略这些标志后自动推进 Prepared。专家 OAMUX=0 按输入旁路计算；0..3 中其他未确认输出源不猜测频率，nominalOutputKnown=false、nominalOutputMHz=0。

## 5. LCM2 的相位与延迟

纯 LCM2 算法接受 CKR=25、12.5、6.25MHz，默认分频使名义 RCK/ACK 都为 25MHz。phaseDegrees 只接受 0/90/180/270；旧软件对其他角度默默用 0，新计算明确拒绝。

| 请求相位 | OBMUX | 说明 |
| --- | --- | --- |
| 0° | 2 | 保留旧软件对板上相位的 180°补偿 |
| 90° | 1 | 相位编码与角度数值不相同 |
| 180° | 0 | 同上 |
| 270° | 3 | 同上 |

delayPs 范围 -5335..5335。LCM 延迟码 0 对应 225ps，码 1 对应 760ps，后续每档加 160ps。通过 DlyGLA/DlyGLB 的差设置 ACK 相对 RCK 的延迟：正值增加 B，负值增加 A。

保留旧整数截断，因此任意 ps 请求不一定精确实现。374ps 和 375ps 的名义差为 0，376ps 编码为 535ps；输入 535ps、5335ps 可以按档精确表示。结果 quantizedAdcDelayPs 表示分频模型中的相对延迟，不能替代线缆/器件测量。越界请求拒绝，不沿用旧超范围饱和。

LCM2 参数不证明上游 CoBo CKR 已匹配。旧 R-CoBo setCoBoReadingClockFrequency 在请求 12.5MHz 时仍写 ckrDivider=2（25MHz）；从这段代码与分频公式推算，单独沿用对应 LCM2 会得到不同输出。µTCA 默认 PLL 的 CKR 也为 25MHz。完整时钟编译/部署必须校验上游和下游，不能因为纯 LCM2 支持三个输入就声称整套 CoBo 支持三种读频率。

## 6. 专家字段如何使用

传入非空 expertFields 意味着整组专家模式，LCM1 必须完整提供 9 个字段，LCM2 完整提供 11 个字段，名称见上表。未知名字、缺项、额外项和超出实际宽度均拒绝。空 map 表示普通计算，不是半组专家配置。

名义分频结果按专家字段重新计算，但频段、反馈路径、延迟、电气锁定等仍需完整验证。特别是改变 VCOSel/FBSEL/FBDLY 等，不应只看一个计算频率就认为可用。

## 7. CoBo PLL 包与整数分频

µTCA PLL 保留旧默认 26 包，包括第一包 R0 RESET。默认 CKR 路径保持 25MHz；写频率分频改在 R3/R4/R5 的 bit5..15。按旧 VCO=2000MHz 假设：

    divider = 向零截断(2000 / writingMHz)
    nominalDefaultWritingMHz = 2000 / divider

请求 writingMHz 必须有限且在 1..200MHz。六种旧标准频率精确可达；其他请求会标记 nonStandardFrequency。例如 123MHz 得 divider=16、名义默认写频率 125MHz，不能把请求值 123 当实际采样频率。

rawOverrides 使用完整 32 位包，索引只允许 R0..16 和 R24..31，包的低五位地址必须与索引相同。拒绝未知索引及地址不一致，不把错误包送给另一寄存器。保留高 32 位数据。expertRegisters=true 时，nominalDefaultWritingMHz 仍表示专家覆盖之前的分频假设，不能代表专家覆盖后的实机输出。

若最终 R13.EN_CLKin1（bit6）为 1，R11 后追加第二包 R11，并清除 SYNC_POL_INV（bit16），保留旧独立 CoBo 同步行为。普通默认 26 包，满足此条件时 27 包；不能从数组位置猜寄存器编号，因为还有第一包 RESET 和跳过的 R17..23。

R-CoBo 不走这套 PLL 包，CompileReducedCoBoWritingDivider 只接受 100、50、25、12.5、6.25、3.125MHz，返回 ckwDivider=0..5。不能在 PLL 操作失败后自动改用 R-CoBo 分频器。

## 8. 验证范围与后续

核心测试覆盖全部字段的反位序/宽度、标准频率参考值、泛化搜索、ADC 相位/延迟全部整数范围、PLL 高位覆盖/地址及第二包同步。原始算法对照比较 4743 个 LCM1、128052 个 LCM2 和 829 个 PLL 结果。严格 C++11 和 ASan/UBSan/LeakSanitizer 结果见 [验证记录](VALIDATION.md)。

这些结果证明代码计算与收到的旧算法一致，**不证明旧算法在所有输入下都能达到目标或满足器件限制**。后续接 xcfg 来源/单位、完整 CoBo 上下游时钟约束、FIFO 部署与锁定/相位验证，再接完整 Prepare/ECC。参见 [Prepare](COBO_PREPARE.md)、[硬件后端](HARDWARE_BACKEND.md)。CEA GET 许可和来源保留在 source 与 third_party/licenses/getbench/COPYING。


## 9. 从原始实验配置生成整板时钟工件

先构建控制软件（需要已安装的 Qt SDK）：

    bash tools/build-linux.sh control

在项目根目录用已有示例生成**新的**输出文件：

    build-suite/bin/getdaq-config compile-cobo-clock \
      source/get/GetBench-software-release-20210208/data/config/configure-pedestals.xcfg \
      0 2 /tmp/pedestals-clock.json

各参数含义：

| 参数 | 示例 | 意义 |
| --- | --- | --- |
| INPUT.xcfg | configure-pedestals.xcfg | 实验配置，必须是 Setup 根节点；include 在本地解析 |
| COBO_INSTANCE | 0 | CoBo 实例的具体 ID，也可为 Crate00_Slot00 等字符串；不能写 * |
| HARDWARE_VERSION | 2 | 用户提供的板型版本：1 为 R-CoBo，>=2 为 M-CoBo；正十进制，禁止 02/负数/最大无效值 |
| OUTPUT.json | /tmp/pedestals-clock.json | 全新文件，已存在时拒绝覆盖 |

Windows 可在配置工具所在目录运行 getdaq-config.exe，同样传四个参数；使用 Windows 路径并给含空格的路径加引号。此命令只读取配置和保存 JSON，不访问控制服务器、不启动数据传输。板型来自命令参数，不是硬件自动检测。

退出码 0 表示工件生成成功，1 表示配置/编译/文件错误，2 表示命令格式或板型参数错误。正常生成仍需查看 warnings 和名义输出；它不保证参数能在实际器件上工作。失败不生成部分 JSON，已有文件字节保持不变；原始 xcfg 不被修改。

### 9.1 读取哪些参数

必须有活动 CoBo，恰好四个具体 AsAd ID 0..3，至少一个活动 AsAd。只编译活动 AsAd，**不要求声明 AGET 或通道**：时钟初始化发生在后续 AGET 配置之前。

| 位置 | 参数与单位 | 缺项策略 |
| --- | --- | --- |
| Module | writingClockFrequency、readingClockFrequency，MHz | 必须提供，有限十进制实数 |
| Module | skipPLLConfig | 默认 false |
| Module/PLL | R10..13 的 18 个命名字段 | 沿用原命名字段的默认值；R-CoBo 或 skipPLL 时不读取未使用的 PLL 字段 |
| AsAd/Clocking | samplingClockDivider | 默认 1；须为正，CKW/divider 在 1..200MHz |
| AsAd/Clocking | phaseShiftAdcClock，degrees | 必须为 0/90/180/270 |
| AsAd/Clocking | delayAdcClock，ps | 必须提供，在 -5335..5335；输出另记录实际量化值 |
| AsAd/Clocking/Debug/LCM1、LCM2 | debugModeEnabled | 默认 false；开启后各自 9/11 个字段必须完整 |

标量若声明 unit，必须与表中单位完全相同，不能把 Hz 或 ns 当作 MHz/ps；声明 range/type 同样检查。共享标量 class 尚不支持，明确拒绝。十六进制专家整数可以写 0x 前缀，也可以声明 type=hex；布尔只接受 true/false。实数先按 double 检查范围，再缩窄到旧算法使用的 float，避免把 200.000001MHz 舍入成 200 后错误接受。

收到的原解析器实际读取 PLL 的 CLKin_SELECT_MODE，新版保持这一标准拼写；另接受兼容别名 CLKin_Select_MODE，仅在标准项缺失时读取。两者都在时标准拼写优先，不读取闲置别名，并保留实际来源。原源码注释中的混合大小写不是执行代码，不能据此说原解析器忽略了标准字段。R14 的 Status_CLKin1_TYPE 虽存在于历史配置，旧 ClockInit 只编译 R10..13；本阶段不擅自开始写 R14。单项 CompileCoBoPll 的原始寄存器覆盖接口仍可在独立调试中显式覆盖 R14，不能与整板命名参数混为一谈。

### 9.2 如何看 JSON

所有 PLL 包和 64 位 LCM 值用十进制/十六进制**字符串**，避免 JSON 浮点数损失整数位。parameters 给出每个查询的值、声明属性、来源文件/行号及代码默认值标记；sources 给出每个配置文件的 SHA-256。编译前后配置文件不变。

| 字段 | 含义 |
| --- | --- |
| hardware_deployment_supported=false | JSON 本身没有执行硬件的 CLI/GUI 入口 |
| complete_prepare=false | 不代表整个 CoBo 已 Prepared |
| program_pll / program_writing_divider | 本次根据明确板型和 skipPLL 选择的分支，无失败后切换 |
| nominal_upstream_known | 能否按收到的默认时钟模型推算上游；不是实际测量 |
| nominal_targets_matched | 所有已知上下游名义输出是否满足请求；有未知输出也为 false |
| nominal_known_difference | 模型中确实算出了偏差，区别于单纯未知 |
| nominal_outputs_within_range | 已知模型输出是否在 1..200MHz；上游未知时 JSON 为 null |
| effective_nominal_* | 将 AsAd 设置与上游实际名义模型合并后的预测；没有足够模型依据时为 null |
| lcm2.quantized_adc_delay_ps | 写入编码所表示的 ADC 相对延迟，可能不同于请求 |

例如请求 CKW=123MHz：PLL 取 floor(2000/123)=16，上游名义值为 125MHz；LCM1 按旧算法使用请求值编译，合并后的 WCK 名义值也可能是 125MHz。新版显示偏差，不自动改请求或分频策略。

R-CoBo 请求 CKR=12.5MHz 时，收到的旧流程仍写 ckrDivider.d=2，即上游 25MHz。LCM2 按 12.5MHz 请求生成，合并后 RCK/ACK 名义值变为 50MHz。新实现保留原寄存器行为并标记偏差；只有实机与器件资料充分验证后才能决定修改硬件策略。

M-CoBo skipPLL=true 会走旧 ckwDivider 分支，但旧源码明确 µTCA 分频字段不使用，因此不能据此推算 PLL 原先输出，CKW/CKR 均标为未知。专家改变 PLL MODE/VCO_DIV 同样不能继续把默认 2000MHz 假设当作已知输出。LCM1 的未知输出选择也保留 null，不猜测频率。

## 10. 原生库如何执行 ClockInit

这是供后续完整 ECC 后端调用的类型化 C++11 接口，尚无自动应用 JSON 的按钮：

    GetDAQ::CoBoClockSettings settings;
    settings.hardwareVersion = 2;
    settings.asadMask = 1; // 仅 AsAd0 活动。
    settings.writingMHz = 100;
    settings.readingMHz = 25;
    GetDAQ::AsAdBoardClockSettings board;
    board.asadId = 0;
    board.samplingDivider = 1;
    board.phaseDegrees = 270;
    board.delayPs = 5335;
    settings.boards.push_back(board);
    const auto steps = GetDAQ::CoBoClockInit(settings);
    client.UseDescription(verifiedDescription);
    client.Execute(steps, newAuditPath);

生产调用者还必须建立独占控制权、正确的上电/监控前提和完整 Prepare 次序。两个停止位检查仅是本子流程的局部检查，不证明这些前提已满足。配置适配器 ClockCompiler::Compile 可成功后输出 typed settings；失败时调用者原有 JSON/settings 保持不变。

执行前先完成全板计算。默认拒绝已知范围外的输出；名义频率偏差要求调用者设置 allowNominalDifference，未知上游或 LCM1 输出要求 allowUnverifiedUpstream。两项都是明确的调试确认，并写入 intent 审计，**不会验证硬件或允许提交 Prepared**。已知范围错误不能靠这些确认绕过。GUI/CLI 离线工件没有自动设置这些确认。

顺序如下：

1. 必须已经绑定本地硬件描述；先整体检查计划中的所有远端结构、只读权限、字段宽度和值。后段 LCM2 缺失/只读或宽度不够时，前段 PLL FIFO 不会写入。
2. 检查 pipeCtrl.firmwareEnable/intEnable 均为 0；按设置核对实际 hardwareVersion。板型不符时停止，不改走另一分支。
3. 设置全部活动 AsAd 掩码；M-CoBo 配置 PLL 时先给 WRFIFO_RST/RDFIFO_RST 写 0x0a，再发送完整包序列，最后仅观察 WRFIFO_SR.Full。
4. R-CoBo 或显式 skipPLL 分支写/读回 ckwDivider.d；逐块选中 AsAd，写 LCM1，然后恢复全部活动掩码。
5. 仅 R-CoBo 写/读回 ckrDivider.d=2；逐块选中 AsAd 写 LCM2，成功后再次恢复全部活动掩码。

LCM 和 FIFO 命令不添加隐式读回：不能把这类命令按普通内存寄存器处理。模拟测试中的 LCM/FIFO 普通读取直接抛错，正常流程仍通过。上游读状态 Full=1 也只作为观察记录，不能说明 PLL 锁定或所有包已发送完成；新函数暂不将它作为生产启动许可。

最后恢复活动掩码是独立子流程的明确后置状态。旧 ClockInit 在 LCM2 之后把掩码留给随后输入流程恢复；新版在正常结束时显式恢复。任何步骤失败立即停止，不补写恢复、不重放、不回滚：例如 LCM2 已生效但应答失败，掩码仍可能是单块；必须保留日志并交给完整恢复协调器核对。

每步执行前先 fsync(begin)，执行后 fsync(result)；写入生效后断线记 UNKNOWN，只发送一次。计划要求新日志文件；完成行只表示这些命令得到应答及期望字段一致。annotation 仅用于审计，限制 1024 个可打印 ASCII 字符，全计划仍限制 4MiB/4096 步，不能把它当硬件脚本。

## 11. 边界与下一步

本机测试覆盖全部活动掩码、原始配置/包含文件、命名 PLL 字段、两种 Ice 编码、64 位值、整份预检查和应答丢失。实际日志与复现见 [验证记录](VALIDATION.md)。没有测量 PLL 锁定时间、真实 ADC 相位、电气校准或高速采集；也未验收 Ubuntu24、Windows Qt 5.14.2 MinGW、macOS。

时钟子流程现已接入 [CoBoPrepareClient单板组合](COBO_PREPARE_CLIENT.md)，包括上电监控保护、AGET/自动延迟和自有告警；后续仍需全系统MuTaNT/对齐协调、完整生产Prepare、状态提交和恢复。操作软件仅处理配置和信息；DAQ 原始字节仍由独立原生接收器处理，不经过 Qt、时钟配置库或 ECC。
