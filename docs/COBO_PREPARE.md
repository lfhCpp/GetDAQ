# CoBo Prepare 初始化：基础配置、输入保护、ADC 与 DAC

## 1. Prepare 负责什么

Prepare 是正式实验参数 Configure 之前的硬件准备阶段。它需要确认型号、停止流水线、初始化缓冲区，随后上电、监控电源、配置时钟、校准串行控制和 ADC 等。任何一个子步骤的成功，都不能独立说明整个 CoBo 已经 Prepared。

**基础配置、输入保护/槽位、ADC/DAC、AsAd上电与监控、AGET/自动读取延迟、LED及告警服务现已由CoBoPrepareClient组合。SystemPreparationClient把该单板流程与MuTaNT、系统对齐、跨板联锁及同步组合；NativeEccCoordinator的native ECC Prepare通过固定HTTP入口调用整套流程，并与数据生命周期协调。** 这说明主机控制路径已接通，不表示板端物理效果已经验收。故障Stop/排空由生产ECC及数据服务按生命周期处理；独立告警观察命令本身不执行Prepare或硬件Stop，详见[服务手册](COBO_SERVICES.md)。

这里不处理原始事件，也不建立数据输出连接。纯计算文件 `common/experiment/coboprepareconfiguration.*` 仅使用标准库；硬件计划位于 `apps/hardware/coboprepare.cpp`，复用本地 Ice 兼容层。Qt 操作台和独立原生接收进程的职责保持分开。

## 2. 如何构建与验证

只构建纯计算核心，不需要 Qt 或 Ice：

    bash tools/build-linux.sh experiment
    (cd build-experiment && ctest --output-on-failure)

构建可选原生硬件库，依赖源码已在本地，不下载软件包：

    bash tools/build-linux.sh ice
    (cd build-ice-compat && ctest --output-on-failure)

硬件回归使用本机 TCP 模拟 GET Node/Device，并分别测试 Ice 1.0/1.1 编码。它不会连接现场硬件；通过结果不能代替实机时钟、波形、电源或长期跑数验收。实际结果见 [验证记录](VALIDATION.md)。

这些完整Prepare步骤经native ECC生产网络路径调用；独立`getdaq-hardware` CLI仍不是完整Prepare入口，`getdaq-config`也不单独执行硬件流程。native ECC与旧SOAP兼容路径的配置和操作差异见[操作台手册](ECC_GUI_CLIENT.md)。不要在现场绕过系统协调器逐个调用底层子流程。

## 3. 初始化流程为什么拆开

旧 CoBoNode::impl_prepare 的顺序如下。表中“已迁移”指子流程代码，不表示整条生产流程已经能运行。

| 次序 | 工作 | 当前实现 |
| --- | --- | --- |
| 1 | initDone=0、版本信息、流水线、LEMO、触发初值、循环缓冲区 | CoBoPrepareFoundation |
| 2 | AsAd 上电、连接/电源检查、监控与 VDD 检查 | 原生CoBoAsAdPrepare已组合上电、慢控制校准、监控和VDD等待；LED策略明确选择，生产协调仍待接；见 [上电监控](ASAD_MONITORING.md) |
| 3 | 初始输入保护：抗放电开启，采样/读出禁止 | CoBoAsAdInputProtections；应在第 2 步确认之后 |
| 4 | CoBo PLL、AsAd LCM1/2、CKW/CKR | 整板计算、xcfg/CLI 及原生 ClockInit 命令已迁移；未验证真实锁定/相位，已接单板CoBoPrepareClient，生产系统Prepare仍待接；见 [时钟说明](COBO_CLOCK_CONFIGURATION.md) |
| 5 | 手动读取延迟、各槽位 ADC/输入管理器配置 | CoBoManualReadDataDelay、CoBoAsAdInputsInit |
| 6 | AGET 串行控制及读出初始化/校准 | CoBoAgetInit/Calibrate 已迁移实际 hit/ADC 测试及校准后型号检查；见 [AGET 校准](AGET_CALIBRATION.md) |
| 7 | ADC 复位、deskew、bit-slip 命令与默认数据格式 | CoBoAsAdAdcInit |
| 8 | DAC 初始寄存器 | CoBoAsAdDacInit |
| 9 | 电源/温度保护阈值、告警订阅、监控读出、全局阈值、自动读取延迟 | 保护/读出/阈值/自动延迟子流程及原生主机订阅已迁移；单板订阅生命周期已接CoBoPrepareClient；完整系统故障Stop/排空仍待生产协调 |

不能将表中已实现的行顺序拼起来，就宣布 Prepared；缺失步骤会影响硬件工作和故障保护。完整协调器必须先验证全部输入、目标描述、采集/读取停止、控制权及未确认操作，然后才开始执行。

## 4. CoBoPrepareFoundation 的选择与初值

设置类型为 CoBoPrepareFoundationSettings。hardwareVersion 默认 2，1 表示 R-CoBo，>=2 对应旧代码的 µTCA 分支。默认要求实际 hardwareVersion 读值一致。0 和 0xffffffff 无效，不以读取失败猜测型号。

极老 R-CoBo 如果确实没有版本寄存器，必须明确选择 hardwareVersion=1、verifyHardwareVersion=false。firmwareInfoRegisters、mutantStatusRegister、interruptRateRegister 默认 true；仅在已审阅的目标描述确认缺失时显式关闭。新代码不会在一个写入异常后自动跳过寄存器，或换成另一套布局再写。

前段先检查 firmwareEnable/intEnable=0，再写并检查 mutantConfig.initDone=0。此位向 MuTaNT 表示“未配置”，不是硬件控制权锁。随后读取 firmwareRelease/firmwareDate 到日志，核对 hardwareVersion，保留旧流水线 15 次写入和 500ms 等待，然后写入下面的初值并显式检查。

| 设置 | 初值 | 含义 |
| --- | --- | --- |
| readDataDelay.AGET0..3 | 14 | 旧触发初始化延迟；最终手动值在时钟初始化后设置 |
| triggerDelay / triggerDelay_2p / triggerWindowStart_2p | 0 | 触发冻结/第二触发窗口起点 |
| triggerWindowEnd_2p | 100000 | 10ns 单位，即 1ms |
| triggerTime_10ns_2p | 1000 | 10ns 单位，即 10µs |
| multThreshold / multThreshold_2p | 3000 | 多重性初始阈值 |
| triggerPeriod_10ns | 500000000 | 5 秒 |
| triggerMask | 0 | 不选择触发来源 |
| multWindSize / readDepth | 16 / 512 | 滑动窗口与读出深度初值 |
| readAlwaysMask 三组 | 0 | 不设必读通道 |
| readIfHitMask 三组 | 0xf / 0xffffffff / 0xffffffff | 全部 68 通道按命中读出 |
| interruptRate | 1 | 每个事件产生中断的旧初值；旧描述缺失须明确选择 |
| aFullLevel / aEmptyLevel | 0x01000000 | 16MiB 缓冲区空间阈值 |

Prepare 的读出掩码初值沿用旧例程，作用于当前 maskSelect 银行；这里没有声称初始化所有芯片银行。Configure 会逐活动芯片设置实际掩码。

bufferLayout 必须明确选择 PerAsAdBuffers 或 SharedBuffer。前者初始化四块各 32MiB 的半开范围：AsAd0 为 0x08000000..0x0a000000，AsAd3 为 0x0e000000..0x10000000，并将各 readPtr 设为起点。**Prepare 初值覆盖四块，不按活动数量重新分配。** SharedBuffer 使用 0x08000000..0x10000000 的单个范围。随后 Configure 才按实验配置重设，因此不要将两阶段的分配规则混淆。

这些操作会重置流水线、缓冲区指针和触发设置，不能用于保持其他 CoBo 校准的单板重对齐。2018 对齐教程所需的维护流程仍应单独迁移。

## 5. LEMO 功能怎么选

µTCA 分支对 lemoMux.L0..L3 写入功能编码；R-CoBo 分支不写 lemoMux，但非法名字仍拒绝。每个连接器缺省 none=0x9。区分大小写，不接受未知名字并静默改为 none。

| 功能 | 编码 | 功能 | 编码 |
| --- | --- | --- | --- |
| trigger_pulse_output | 0x0 | busy_output | 0x1 |
| pll_holdover_output | 0x2 | pll_ld_output | 0x3 |
| pll_clkin0_output | 0x4 | pll_clkin1_output | 0x5 |
| pll_sync_output | 0x6 | external_trigger_input | 0x8 |
| none | 0x9 | pll_clkin0_input | 0xc |
| pll_clkin1_input | 0xd | pll_sync_input | 0xe |
| external_timestamp_reset_input | 0xf | | |

功能选择与线缆接法必须一致；选择“输入”不会让 GUI 参与信号时序。

## 6. AsAd 输入槽位与保护

每个 AsAd 有四个 AGET 物理位置。AsAdInputSettings 必须恰好列出活动 AsAd，每块含四个位置；拒绝缺失、重复、不活动板或 ID 超出 0..3。编译结果按板号排序。

位置的 active 表示逻辑芯片活动；enabled 对应 enableAsAdPosition。只有二者均为 true 才开启该位置的 ADC、采样和读出。

| 结果 | 位 | 1 的含义 |
| --- | --- | --- |
| ADC0 | 1+位置号 | 关闭相应 ADC 通道；注意与 enabled 的极性相反 |
| IM1.ENR | 位置号 | 允许读出/RCK |
| IM1.ENW | 4+位置号 | 允许采样/WCK |
| IM0.Dbi | 位置号 | 开启抗放电保护，缺省 true |
| IM0.SWZ | 4+位置号 | ZAP 接地，缺省 false（接 VDD） |

关闭位置不会自动将其 logical active 改成 false；编译结果的 agetMask 按 active 计算，以免无意改变型号核对及其他参数范围。保护和 ZAP 位同样针对四个物理位置计算，包含不活动位置。

CoBoAsAdInputProtections 是前期保护：向活动板广播 IM0=0x0f、IM1=0。正式槽位配置逐板选择写 ADC0、IM0、IM1，结束恢复完整 AsAd mask。它们只记录写入应答，没有添加未经确认的 ADC/IM 读回。

## 7. ADC 校准命令的时序与读回限制

CoBoAsAdAdcInit(mask, doubleDataRate=true) 保留旧 DDR 默认，也可明确选择 SDR：

1. 向活动 AsAd 广播 ADC0=0x400，软件复位。
2. 设置 ADC2=0x2c0（deskew 图样）、ADC3=0xc3、ADC4=0、ADC5=0x401（DDR）或 0x403（SDR）。
3. pipeCtrl.adcDeskew=1，等待 1000ms，再写 0。
4. ADC2=0x2a0，选择自定义图样。
5. pipeCtrl.bitSlipCalibrate=1，等待 1000ms，再写 0。
6. ADC2=0x200 恢复普通数据；ADC5 再写所选 DDR/SDR 值。

旧源码明确说明 ADC0..7 是只写的 11 位寄存器，ADC0 的复位位还会自清零。新流程不能要求读回 0x400，也不能通过一份模拟寄存器的回显宣布 ADC 电气校准成功。complete 表示这些命令及等待执行完毕；实际 deskew、bit-slip、数据格式和波形仍需硬件验收，完整 Prepared 提交还要其他准备步骤和结果核对。

**ADC 软件复位会清除之前的 ADC 配置。** 旧 Prepare 在槽位设置之后又执行复位，不能把“槽位配置成功”理解成最终 ADC0 仍保存这些位。当前子流程保留各自的原始行为，单独调用子流程时不改其他步骤；新CoBoPrepareClient已在最后ADCInit后、DAC前调用CoBoAsAdAdcChannelsFinal，只写ADC0重应用通道位，不再次复位或覆盖ADC2/5。其模拟只写ADC/独立IM结果已验证，电气效果仍待验。IM 寄存器属于另一设备，不由 ADC 复位代替。

## 8. DAC、读取延迟与失败处理

CoBoAsAdDacInit 写 DAC0=0x81，DAC1/DAC3..7=0。DAC2 为只读 VERSION，始终不写。DAC6 使用旧软件的 0，而非资料中提到的另一默认值 0x38；这一差异明确保留，实机验收时应核对。

CoBoManualReadDataDelay(delay) 在时钟初始化之后设置四个 readDataDelay.AGET 字段，范围 0..255，按照收到的 8 位硬件描述检查。微 TCA 中旧手动字段可能不生效，函数成功不表示自动延迟校准完成，也不改变实际 SCR 的自动/手动选择。

所有子流程都先检查 firmwareEnable/intEnable=0。完整调用者还必须绑定审核过的硬件描述，确认停止采集及相关读取并独占控制权。位检查不能替代完整联锁，也不能证明数据接收进程已经排空。

HardwareClient::Execute 会对整份组合计划预检查目标元数据、位宽、权限、步骤数及等待范围。末尾 DAC7 缺失或 ADC0 本地只读时，不能先执行前面的 ADC 复位再报错。执行后读回不符、远端异常或断线立即停止；已生效但应答丢失的 ADC 写入记 unknown，只发送一次，不重发、不回滚，不继续后面的 DAC。

硬件动作日志成功也不代表 ECC 状态可提交。恢复时不得根据旧日志自动重新复位或校准；需要真实硬件核对，详见 [硬件后端](HARDWARE_BACKEND.md)、[ECC 状态机](ECC_STATE_MACHINE.md)、[ECC 日志](ECC_JOURNAL.md)。

## 9. 校准迁移处理的旧行为和后续限制

以下是收到源码的行为核对；AGET/延迟的新子流程已实现并由CoBoPrepareClient组合进native ECC生产Prepare，详情见[校准手册](AGET_CALIBRATION.md)。**完整硬件序列尚未在真实板卡上验证**：

- AGET 串行校准先选 calibrationAget，每块 AsAd 两位；旧用户配置的0表示沿用自动找第一片活动芯片，非零才覆盖。旧代码会在没有活动芯片时回退0，完整编译应提前检查活动芯片/校准片一致性，不能让回退掩盖配置问题。旧缺寄存器兼容应在元数据阶段明确选定，不能写失败后假设没有生效再切换布局。
- `agetCal` 包含 ADC 复位/测试、200ms等待、反复 reg5 读取及 `hit=0xA000000000000005` 的实际校准动作，之后逐片读取型号。它会改动控制状态和寄存器；不能用此前 Configure 的普通参数写入替代。
- `calibrateAutoReadDataDelay` 会先改 AGET reg1/reg2，临时打开 firmwareEnable、手动生成 SCW/SCR，并保留两次200ms等待，最后关闭固件使能与校准/override位。这是 Prepare 内部校准动作，不能当作完整采集 Start，也不能把它放在最终实验寄存器配置之后。
- 自动/手动延迟选择的旧兼容 catch 计算了板号字段名，却实际总写 `SCRAutoDelayEnable0`；共同字段与逐板字段还可能适用不同固件。迁移必须根据绑定描述明确布局，核对每板策略，不能把旧异常后重试或固定0的行为无说明地复制。
- 旧主机告警订阅调用alarm service reset会移除所有订阅者。新CoBoAlarmClient使用自己的随机ID，只注销自身；板端保护、检查器和主机订阅分别管理。NativeEccCoordinator在会改变硬件状态的命令前检查保持告警，并提供明确的停止/恢复工作流；独立原生观察程序本身不执行自动硬件Stop，具体边界见[告警服务](COBO_SERVICES.md)。

## 10. 来源与下一步

来源为收到的 CEA GET CoBoNode.cpp 中 impl_prepare、setLemoModes、triggerInit、circularBufferInit、asadSetAgetInputProtections、asadAgetInputManagerInit、adcReset/adcCal、asadAdcInit、asadDacInit 与 setReadDataDelay，以及 CoBoConfigParser.cpp 的 LEMO 表。CeCILL 声明和完整旧源码保留在 source 与 third_party/licenses/getbench/COPYING。

下一步把已验证单板组合与SystemAlignmentClient、MuTaNT接入全系统生产Prepare/Configure，绑定控制权、配置版本和状态观察；随后接CoBo PLL之后的同步、Start/Stop、故障排空、恢复与网络服务。Windows Qt5.14.2 MinGW、Ubuntu24、macOS和实机均仍待验收。
