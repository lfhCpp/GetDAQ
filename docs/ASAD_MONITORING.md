# AsAd 上电、监控与保护：操作和原理

本页解释收到的 GET 旧源码中 AsAd 上电与监控的行为，以及已迁移的配置计算和原生硬件库子流程。**整套 Prepare 尚未完成，也未连接真实板卡验收。** GUI/CLI 当前只能编译本页的离线工件，不能直接把 JSON 部署到设备。

## 1. 这些功能分别解决什么问题

AsAd 是连接 CoBo 与 AGET 的前端板。CoBo 的控制寄存器负责连接状态、上电开关和慢控制选择；AsAd 自身的监控器读取电压、电流和温度，并根据阈值触发板端保护。

| 功能 | 含义 | 当前入口 |
| --- | --- | --- |
| 电源开关及连接检查 | 先断电，检查连接/供电，再给活动板上电并检查结果 | `CoBoAsAdPowerCycle` 原生库 |
| 初始监控 | 复位监控器、设置阈值、关闭自动保护，启动测量 | `CoBoAsAdMonitoringInit` 原生库 |
| 等待主电压稳定 | 电压至少达到请求值才继续，读取次数和等待时间有上限 | `CoBoAsAdPrepare` 的 VDD 轮询 |
| 保护设置 | 写入电源/电流/外部温度阈值及保护掩码，按配置启用告警 | `CoBoAsAdProtectionInit` 原生库 |
| 一次监控读取 | 读取 VDD、VDD1、IDD、内部/外部温度的原始码 | `CoBoAsAdMonitoringRead` 原生库 |
| 离线参数检查 | 读取 xcfg、计算寄存器码、输出来源与策略 | `getdaq-config compile-cobo-monitoring` |

原生库复用本地 Ice/GET 接口，不链接 Qt。Qt 只解析配置和呈现信息。这些低频控制/状态消息与独立接收器传输的高流量 MFM 原始事件不同；监控器不会成为事件数据的中转站。

板端LED管理RPC和原生主机告警客户端已另行实现，详见 [LED与主机告警](COBO_SERVICES.md)。完整ECC网络服务、故障停机/排空协调及Prepared提交仍未接入。设置保护寄存器不等于已经启动检查器和主机订阅。

## 2. 最容易误解的三条状态

旧代码使用三个状态：PLG 表示 AsAd 与 CoBo 的连接；PWU 对应 `asadEnable.powerONn` 的开关状态；AL 是 `asadStatus.alarm` 中对应板的位。

**AL 的判断必须带上上电阶段。** 收到的旧上电流程在断电后、活动板 PLG=1 的前提下，要求 AL=1，解释为供电连接存在；上电后要求 AL=0，解释为没有告警。不能把所有 AL=1 都当作故障，也不能一直把 AL=1 当作供电正常。

| 检查阶段 | 新计划要求 | 失败含义 |
| --- | --- | --- |
| 上电前 | 活动板 PLG=1 | 前端板未连接，不能继续给它上电 |
| 上电前供电检查 | AL=1，默认严格 | 旧软件据此判断供电连接可能缺失 |
| 上电后 | PLG=1、AL=0、PWU=1 | 连接、告警或开关状态不满足计划 |

`Control/checkPowerSupply=false` 只明确放宽**上电前供电检查**。发生 AL 差异时日志保留 `matched_with_warning` 和实际位值。上电后的 AL 告警仍会中止，不随这个选项一起忽略。新计划还明确检查上电后的 PLG 和 PWU；旧上电函数读取了三项，但该处只用 AL 决定是否抛出异常。

这些解释来自收到的源码，不代表已经核对了现场电气接线、固件或板端协议版本。实际验收必须确认同一套硬件定义仍适用。

## 3. 先离线生成工件

Qt 构建完成后，在工程根目录执行；新输出路径必须尚不存在：

```bash
build-suite/bin/getdaq-config compile-cobo-monitoring \
  source/get/GetBench-software-release-20210208/data/config/configure-pedestals.xcfg \
  0 declared /tmp/asad-monitoring-declared.json
```

Windows 用 Qt Creator 构建 `GetDAQ.pro`，再在构建生成的 `bin` 目录寻找 `getdaq-config.exe`，把以上输入/输出改为自己的 Windows 路径。CLI 只需 Qt Core，与操作台使用同一套配置解析代码；Windows Qt 5.14.2 MinGW 尚需在用户电脑实际验收。

命令参数依次是输入文件、具体 CoBo 实例 ID、模式映射策略、新 JSON 路径。实例 ID 不能用 `*`；`*` 是配置默认声明，不代表某一块真实板。

退出码 0 表示离线编译成功，1 表示配置或文件操作失败，2 表示命令参数非法。失败不会覆盖已有工件；配置非法时不创建输出。**编译成功不表示已给任何板上电。** 工具没有连接硬件的操作。

## 4. 为什么必须选择 declared 或 legacy

旧函数的形参顺序是：

```cpp
enableAlarms, averagingDisabled, singleChannelMode, singleChannelItem, fastMode
```

两处旧调用却依次传入：

```cpp
enableAlarms, fastMode, singleChannelItem, singleChannelMode, averagingDisabled
```

其中整数 item 会隐式变成布尔值，singleChannelMode 又会变成整数 item。默认模式可能看不出差异，非默认模式则会写出不同寄存器。因此不能无说明地修改，也不能把旧调用顺序当作参数名称所表达的含义。

| 策略 | `configuration2` | `configuration3` |
| --- | --- | --- |
| declared | item 码 \| single<<4 \| averagingDisabled<<5 | fastConversionEnabled |
| legacy | bool(single) \| bool(item!=0)<<4 \| fastConversionEnabled<<5 | averagingDisabled |

`declared` 按参数名称和旧函数内的位定义修正顺序。`legacy` 保留实际旧调用行为，包括隐式类型转换。CLI 强制明确选择；原生类型化设置默认 `DeclaredParameters`，调用者可显式指定 `LegacyCallArguments`，审计记录所用策略。

例：item=IDD、single=true、fast=true、averagingDisabled=false 时，declared 生成 cfg2=21、cfg3=1；legacy 生成 cfg2=49、cfg3=0。修改策略前要核对真实转换模式，而不是只看到两份 JSON 都能编译。

## 5. xcfg 字段、单位与默认值

选中的 CoBo 必须活动，并声明四个具体 AsAd ID 0..3；只计算其中 `Control/isActive=true` 的板。至少一块活动，不依赖 AGET 配置才能计算监控参数。include 和实例默认回溯沿用当前 XcfgDocument 的规则，详见 [配置编辑](CONFIGURATION_EDITOR.md)。

每块活动板的字段在 `AsAd[n]/Control` 或 `AsAd[n]/Monitoring`：

| 字段 | 类型/单位 | 缺项行为 |
| --- | --- | --- |
| Control/checkPowerSupply | true/false | 默认 true，并记录代码默认来源 |
| Monitoring/isAlarmSet | true/false | 必填 |
| averagingDisabled | true/false | 默认 false |
| fastConversionEnabled | true/false | 默认 false |
| singleChannelModeEnabled | true/false | 默认 false |
| singleChannelItem | VDD1/Tint/Text/VDD/IDD | 必填，未知名称拒绝 |
| LimitsVDD/lowerValue、upperValue | 有限十进制实数，V | 必须成对 |
| LimitsIDD/lowerValue、upperValue | 有限十进制实数，A | 必须成对 |
| LimitsText/lowerValue、upperValue | 有符号整数，C | 必须成对，范围 -128..127 |
| LimitsText/disableAlertsTooHigh、TooLow、OutOfOrder | true/false | 各自默认 false |
| LimitsVDD/disableAlerts、LimitsIDD/disableAlerts | true/false | 各自默认 false |

单位声明可缺省；声明后必须匹配 V/A/C。比如 mA 不会自动换算成 A，避免无声改变实验参数。`nan`、`inf`、十六进制实数、单位混在数字正文、尾随字符和空格均拒绝；声明的 type/range 也由共用 ExperimentReader 检查。

旧式平铺字段仍可使用：VddLowLimit/VddHighLimit、IddLowLimit/IddHighLimit、ExternalTempLowLimit/ExternalTempHighLimit。每种现代字段的 lower/upper 全部缺失时才使用完整旧式字段对；仅缺一半会拒绝。旧程序曾在现代字段缺项时把整对改读旧字段，新规则避免一个现代值被悄悄忽略。

旧等待 VDD 的函数单独读取现代 `LimitsVDD/lowerValue`，缺项采用 3.2V。因此旧式平铺字段即使低阈值为 3.0V，上电稳定最低值仍保持 3.2V；JSON 会分别显示。现代配置则使用其 lowerValue 作为最低稳定电压。

当 isAlarmSet=false 时不读取各 disableAlerts 字段。否则，新适配层对缺失的每个关闭选项独立使用 false；旧代码的整段 try/catch 会在首次缺项后跳过剩余选项。完整字段配置与旧保护位一致，部分字段的默认处理是明确改进，不宣称逐一复现旧缺项副作用。

## 6. 物理量怎样成为寄存器码

电压与电流保护阈值按旧公式向下取整：

```text
VDD code = floor(512 × volts / 7.524)
IDD code = floor(193.28 × amps / 2.28)
```

上下限必须有限、非负且有序；转换后整数必须在 0..255，越界拒绝。不会沿用旧 uint8_t 隐式转换造成的截断/回绕。示例：VDD 3.0..3.5V 对应 204..238；IDD 0..2A 对应 0..169。寄存器是离散码，转换得到的实际阈值与请求实数存在量化差异。

外部温度按有符号 8 位 Celsius 解释。17..50°C 发送 17..50；-10°C 发送 246；-128°C 发送 128。旧 xcfg 某些温度 range 写成 [0,255]，不能据此把 200 当作正 200°C。使用负温度时必须同时修正配置中的声明范围。

旧 Prepare 真正写入的只有 Text/VDD/IDD 六个阈值；Tint/VDD1 阈值函数调用被注释，interruptMask2 也保持默认。新子流程保留这一边界，没有假装写了这些未启用的设置。VDD1/Tint 仍可以读取测量值。

## 7. 等待电压与读取测量值

监控读取使用旧比例：

```text
VDD [V]  = (2.28 / 256) × (3.3 / 2) × raw
IDD [A]  = (2.28 / 256) × (1 / 0.755) × raw
VDD1 [V] = 0.06 + 7 × raw / 256，raw=0 时保留旧程序的 0
```

VDD 最低稳定原始码按**向上取整**得到，再用实际读取公式核对临界点。请求 3.0V 时，原始码 204 仅约 2.99784V，必须等到至少 205；保护下阈值 204 不能直接拿来作为稳定检查的最低码。旧式 3.2V 等待对应至少 218。

默认每次测量原始零码最多读 10 次。等待 VDD 默认最多 10 个外层尝试，每个失败尝试之后等待 100ms，包括最后一次；非零但偏低的值不会消耗“零值重读”的整组预算。零码既可能是真实零值，也可能是未就绪，不能单凭它判定连接故障。一次观察读取耗尽零值重读后允许记录零；VDD 稳定检查仍按最低码判断。

原生计划每块板先选择慢控制/读取对象，再在该选择下轮询；没有在每个采样内部偷偷写选择寄存器。调用者必须保证独占控制。旧读取函数每次尝试都重写选择，改写为显式选择步骤后，审计能完整展示所有写动作。

`DecodeAsAdMeasurements` 是纯计算接口，输入依次为 VDD、VDD1、IDD、internalTemperature、externalTemperature 的五个原始 uint64 值；每个必须 ≤255。温度直接解释为 raw>=128 ? raw-256 : raw，避免依赖实现相关的 unsigned-to-int8 转换。

`CoBoAsAdMonitoringRead` 的原始样本存于审计 `observed.actual`。当前尚无该原生子流程的 GUI 展示/结构化 RPC 返回入口；不要把离线 JSON 中的阈值当作测得电压。该读取计划仍修改选择寄存器并检查停止位，**它不是可以在采集期间随意执行的无副作用查询**。

## 8. 保护掩码怎样读

`interruptMask1` 中，1 表示关闭对应保护条件。isAlarmSet=true 的基值为 0x23；false 则为 0xff。位 0/1/5 必须为 1。

| 位 | 为 1 时关闭的条件 |
| --- | --- |
| 2 | 外部温度过高 |
| 3 | 外部温度过低 |
| 4 | 外部温度 out-of-order |
| 6 | VDD 超限 |
| 7 | IDD 超限 |

每个 disableAlerts=true 只把相应位设为 1。初始电压稳定阶段写 mask1=0xff、cfg1=0x2d，监控启动但保护关闭；后续最终保护写配置计算的掩码，告警启用时 cfg1=0x0d，否则仍0x2d。不要在电源尚未稳定时把两段顺序互换。

## 9. 原生子流程顺序

`CoBoAsAdPrepare(settings)` 组合旧 AsAd 初始化中的主要操作：

1. 检查停止位和绑定描述，复位 AsAd 控制器。
2. 对全部四个位置先观察 PLG；已连接的板选择后读取旧 interruptStatus1，仅记录，不清除。
3. 对所有位置 powerOFF 并等待；只给活动板做供电检查、powerON 和上电后核对。已连接的非活动板也断电，但不重新上电。
4. 执行已有 `CoBoAsAdCalibrate`，核对 monitorID=0x41、写入标签，并按 AIM0..3 顺序读取。
5. 执行初始监控，各活动板复位、关闭保护、写六个限值和转换模式。
6. 逐板等待 VDD 达到最低值。
7. 广播初始输入保护 IM0=0x0f、IM1=0，正常结束保留完整活动掩码。

这段默认不包含LED RPC；类型化调用者可明确选择RequiredLedManager并核对板型2或5，故障LED与失败检查共用一次观察。完整Prepare接着还要ClockInit、输入槽位、AGET/ADC/DAC、最终保护、主机告警、自动读取延迟和MuTaNT协调。CoBoAsAdProtectionInit单独提供后段保护，不提前接到上电初始化结尾。全过程位置见 [Prepare说明](COBO_PREPARE.md)，服务边界见 [LED/告警手册](COBO_SERVICES.md)。

组合片段请用 `AppendHardwareSteps(destination,fragment)`，不要直接 vector::insert。条件引用是片段内已观察 ReadValue 的索引，组合时必须按偏移重新编号；错误索引可能让别的板的观察值控制当前操作。该函数拒绝非法条件引用或超额片段时保持 destination 不变；整体执行前仍要检查全部参数。

## 10. 条件执行、轮询与故障审计

新增条件只引用前面**无条件 ReadValue** 已取得的值，不隐式再读。未连接位置的诊断步骤记录 `skipped`，不调用 AsAd 读取；即使分支最终跳过，整份计划中该目标的元数据和权限仍提前核对。

有界轮询每次 RPC 前落盘 poll_begin，收到结果后记录 observed，再记录 matched 或 outside_range。遇到读取异常立即 read_failed 并停止，不把异常当作零码重试；轮询失败不会回滚先前已执行的上电等动作。

普通计划最多 4096 步、intent 最多 4MiB；轮询合计最多 4096 次可能读取，等待合计最多 300 秒。单个轮询最多100个尝试，每次零值重读1..10，等待≤60000ms。生产默认次数远低于上限；组合多个子流程时按整体预算检查。

缺少绑定时不访问远端元数据；后段缺失寄存器、只读阈值等在首个上电/复位动作前拒绝。停止位和物理状态是在运行时检查，后面的板出现物理故障时，前面板可能已经上电。两个停止位通过不证明没有第二个控制者。

写入后断线/异常保存 UNKNOWN，禁止自动重发、回滚或补写恢复选择。电源动作可能已经生效；保持审计和现场状态，由完整恢复协调器核对。`complete` 只证明计划中的命令/观察按定义完成，不能设置整个 ECC 为 Prepared。

## 11. 阅读 JSON

工件含 kind、实例 ID、asad_mask、mode_mapping、timing、boards、parameters、sources、warnings。每块板含请求上下限和字符串形式的码值、保护掩码、转换模式、上电前检查策略、最低稳定电压和原始码，以及采用现代/旧式字段的来源布局。

parameters保留query_path、实际文件/行、声明属性或代码默认；sources保存已读取文件的SHA-256。Alarms/checkAsAd和Alarms/subscribe默认true，输出alarm_policy及来源。native_*_available=true表示可选原生库提供API；旧主机告警/LED支持标志仍false，表示离线JSON没有服务部署入口。hardware_deployment_supported和complete_prepare仍false；不自动选择端点/LED板型或执行订阅策略。

## 12. 验证与下一步

核心测试覆盖全部活动掩码、两种模式映射、关闭保护的所有位组合、8位测量/有符号温度及稳定码边界。独立对照脚本直接提取收到的原函数和实际旧调用；不会调用新函数充当旧参考，也不安装 CCfg/Boost。原生 TCP 使用会拒绝命令回显的模拟设备，核对预检查、条件跳过、零值/偏低读取和写入后断线。

```bash
python3 tests/cobo_monitoring_original_reference.py
python3 tests/cobo_monitoring_integration.py build-suite/bin/getdaq-config
(cd build-experiment && ctest --output-on-failure)
(cd build-ice-compat && ctest -R hardware-monitoring-tcp --output-on-failure)
```

实际结果及插桩范围见 [验证记录](VALIDATION.md)。真机保护动作、温度解释、电源时序、持续告警和平台验收仍需完成；模拟调用一致不构成这些项目的验收。
