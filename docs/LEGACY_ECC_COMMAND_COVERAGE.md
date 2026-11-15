# 旧 ECC 控制命令迁移核对表

本文把收到的 GetBench 2021 源码中 `GetEccCmd.cpp` 注册的操作台命令，与当前主机端 C++ 实现逐组对照。目标是核实实验主流程和维护能力是否有归属，不要求保留旧命令名、旧 Ice RPC 或逐条兼容旧终端脚本。

核对范围是旧控制台入口和当前源码；它不证明现场实际使用过每个旧命令，也不证明任何硬件动作已在实板上验证。板端 `getHwServer`、`IceEirfu` 及其服务端实现仍按项目范围保留，不在此重写。

## 分类

| 分类 | 含义 |
| --- | --- |
| 主流程已替换 | 新系统有明确的配置、状态机或 UI 流程；协议/API 与旧实现不同 |
| 流程化替换 | 功能由固定工件和完整 Prepare/Configure/采集步骤调用，不提供旧式即时命令 |
| 部分对应 | 找到相邻核心实现，但没证明语义、操作入口和边界完全等价 |
| 未找到迁移入口 | 在当前主机应用/核心中未找到可供操作者调用的对应入口，列为待核实范围 |

## 命令映射

| 旧控制台命令 | 旧行为 | 当前实现与入口 | 状态及差异 |
| --- | --- | --- | --- |
| `sm-describe`, `sm-prepare`, `sm-configure`, `sm-reset` | 系统描述、准备、配置、复位 | Native ECC 状态机、`SystemDescriptionClient`、`SystemPreparationClient`、实验工件及 GetDAQ 窗口 | 主流程已替换；新服务用固定 HTTP/JSON、操作 ID、审计和严格 UNKNOWN 保持，不提供旧 Ice servant |
| `sm-start`, `sm-stop`, `sm-pause`, `sm-resume`, `sm-breakup`, `sm-status` | 运行生命周期和状态查询 | `NativeEccCoordinator`、`SystemAcquisitionClient`、RunControl 和接收器生命周期管理 | 主流程已替换；接收器 EOF/封存是独立状态，Stop 应答不代表数据文件已完成 |
| `sm-links` | 返回旧发送器/路由链接 | 固定系统配置、发送器拓扑与接收计划审阅页面 | 主流程已替换为配置审阅；不再通过临时字符串直接重绑定数据目标 |
| `config-list`, `config-get`, `config-add` | 列出、读取和添加旧 ECC 配置集 | 本地 `.xcfg` 工作区、配置发布工件、`ConfigurationRepository` 及其 CLI/窗口 | 概念上已替换，存储格式和服务协议不同。旧数据库默认关闭且现场用法未知；不声称支持旧数据库导入或线协议互通。旧命令 `config-add` 在收到源码中有序列化并调用 `addSubConfig` 的实现，但现场后端未知 |
| `pll-configure`, `pll-write`, `pll-reset` | 直接配置/写入/复位 CoBo PLL | 类型化 CoBo 时钟设置、`CoBoClockConfiguration` 编译和受审计 Configure/Prepare 写入 | 流程化替换；旧命令允许的即时寄存器操作没有一对一复刻，避免绕开当前时钟/状态前提 |
| `cobo-mutant-link-set`, `mutant-align`, `mutant-align-multishelve` | `cobo-mutant-link-set`切换当前 CoBo 链路模式；`mutant-align`在当前 MuTaNT 上调用 GMC/WSCA TDC 对齐并报告各AMC槽；`mutant-align-multishelve`对当前 MuTaNT 执行指定时钟的三步跨机箱对齐 | `SystemAlignmentClient`、`getdaq-align`和全系统 Prepare；旧 `AmcSlotManager::findSlotOfCoBo` 的定位行为由完整拓扑中的 CoBo 反馈链路及所有 MuTaNT 扫描实现 | 流程化替换，不是逐命令即时映射。新对齐先预检完整拓扑和停机/未初始化条件，再运行绑定的系统链路与对齐步骤；GUI不提供运行中的随手切链，也不伪称旧TDC操作可只影响一块CoBo。故障CoBo恢复沿用2018教程的单板Load Hw/Configure意图，由选定CoBo完整重准备保留其他板及已有系统对齐；该动作不同于重新运行MuTaNT TDC/跨机箱对齐。 |
| `asad-alarm-monitor`, `node-alarm-subscribe` | 控制板端周期告警监控及当前节点订阅 | `CoBoAsAdAlarmMonitorInit`、`CoBoAlarmClient`、独立告警 watcher 和维护审计 | 主机侧新流程已覆盖告警启停/订阅状态；新告警事件保持与“硬件 Stop 已执行”分离 |
| `ispm-set` | 按信号名称即时切换 AsAd inspection multiplexer | `AsAdConfiguration.inspection` 编译为 Configure 中受限的 `ISPM` 写入和确认 | 流程化替换；没有任意信号名的即时命令 |
| `asad-pulser-trigger`（别名 `asad-inject`） | 对当前 ECC 节点立即调用 `triggerPulser(voltage_mV)` | Configure 在 `externalTriggerMode=true` 时按固定配置触发；另有 `Selected CoBo maintenance...` 单次诊断入口，限健康 Prepared、全部数据服务停止、固定活动 CoBo 和 `-1000..1000 mV` | 有受控的单次入口；请求目标/电压绑定持久操作并只发送一次，不自动重试。硬件应答不证明物理脉冲/波形。周期 pulser 仍由采集生命周期管理 |
| `asad-pulser-start`, `asad-pulser-stop` | 旧 `start` 先设定幅度和周期再立即启动；`stop` 直接停止周期定时器 | `ASAD_CONFIGURATION.md` 编译默认/目标幅度、周期、双脉冲和随机模式；Configure 只设置参数，采集 Start 后按 `periodicModeEnabled` 启动，正常 Stop/Breakup 停止 | 周期脉冲已并入受控采集生命周期，不再提供运行之外的任意即时启动。旧的 start 参数即时覆盖语义不保留；须编辑实验配置并走完整 Configure/Start |
| `cobo-sc-slow`, `cobo-sc-fast` | 立即切换控制模式，fast 模式还会重新启用固件 | `CoBoConfigure` 的受控配置顺序及硬件计划 | 流程化替换；没有独立即时切换入口，避免跳过准备/Configure 状态门槛 |
| `aget-readout-channel-select`, `aget-test-channel-select`, `aget-channel-lsb-treshold-set` | 慢控制模式下，对每片活动 AGET 即时写单通道 always/readout 掩码、reg3/reg4 测试位图或阈值 nibble；通道 68 表示清除测试/读出选择或对所有普通通道统一设阈值 | `AgetConfiguration` 的 `Reading`、`isSelectedforTestMode`、`LSBThresholdValue` 可表达目标值；通过系统工件和完整 Configure 应用 | 配置目标已按旧寄存器实现映射；不是即时命令替代。测试/读出目标通道须 `isActive=true`，FPN 无阈值字段，完整 Configure 还应用 Module 级模式。见[AGET 即时命令迁移说明](AGET_CONFIGURATION.md#71-旧-ecc-aget-即时命令如何迁移) |
| `mutant-bram-check` | 对指定 MuTaNT BRAM 地址范围写入并读回测试数据 | 未找到可由新主机操作台调用的等价入口 | 未迁移。旧实现固定设备名`CARD_B_BRAM`、`MemBus16`、2字节寄存器宽度、基址`0x80F10000`，但由命令行直接接收写入值、字数和首偏移，并按`firstOffset + 2*i`写同一值后读回；没有在该命令中校验BRAM安全边界，也没有保存/恢复原内容。不能据这段旧代码推断可安全的可写区域或现场诊断流程。 |
| `cobo-led-blink-all`, `cobo-led-set`, `cobo-led-flip`, `cobo-led-pulse` | 操作板卡 LED 管理器 | `CoBoMaintenanceWindow`提供选定活动CoBo的单灯设置、顺序闪烁、GPIO位翻转和`cobo-led-pulse <ledId> <period_ms>`入口；服务端使用`set-cobo-led`、`blink-cobo-leds`、`flip-cobo-led`、`pulse-cobo-led` | 旧`set`的Off/On/Slow/Fast状态以及`pulse`均使用单灯`modifyLED`路径。固定半周期1000/500 ms仍由状态按钮提供；任意周期脉冲绑定CoBo、LED位和毫秒数，0表示停止所选pulser，16表示全板。所有操作限健康Prepared、接收数据停止和目标互锁，使用原操作审计且不自动重试；flip为非幂等操作。板端应答不验证物理灯态 |

## 对范围的结论

旧 `GetEccCmd` 混合了实验主流程、配置存储、即时寄存器调试和指示灯/BRAM 维护。当前重构已把主流程和配置驱动的硬件操作迁入新的类型化工件、C++11 硬件计划和 Qt 操作台；这不等于每一个旧终端调试命令都有同名按钮。

BRAM命令的历史实现已核实，但它是未校验边界且不恢复原内容的写测试；只有取得MuTaNT内存映射、现场允许区域与维护恢复流程后，才能考虑新增固定边界的安全诊断入口，现阶段不开放BRAM写操作。周期 pulser 已由实验配置和采集生命周期管理；LED已有受限维护入口，但旧命令并非逐项等价替换。现场数据库仍未知，不添加推测的 DBMS 依赖。AGET 旧命令的配置目标已映射，真实硬件效果仍需验收；任何板端动作都只能在仿真/测试夹具中验证自动化逻辑，现场操作和电气效果需另行验收。

旧命令和接口依据：`source/get/GetBench-software-release-20210208/src/get/GetEccCmd.cpp`、`GetEcc.ice`、`rc/BackEnd.cpp`。新组件入口和验收边界见[总体进度](REFACTOR_STATUS.md)、[架构](ARCHITECTURE.md)、[硬件后端](HARDWARE_BACKEND.md)、[系统配置工件](SYSTEM_EXPERIMENT_CONFIGURATION.md)、[系统采集](SYSTEM_ACQUISITION.md)及[单板维护](COBO_MAINTENANCE.md)。

### 对齐命令的范围（源码复核 2026-10-22）

这几条旧命令的目标层级不同，不能合称“单板重对齐”：`mutant-align`和`mutant-align-multishelve`都调用当前 MuTaNT 的 AlignmentServer；后者由操作者分别启动步骤1、2、3。旧 `AmcSlotManager::findSlotOfCoBo` 才以某一 CoBo ID 为输入，但它先切该 CoBo 到 GMC/WSCA 反馈链路，再扫描拓扑内所有 MuTaNT 的 TDC 结果并要求恰好找到一个槽，最后恢复Normal链路。它仍会调用共享 MuTaNT 服务，且旧代码的异常路径不保证自动恢复链路。

新主机把可写的 TDC/链路顺序放在 `SystemAlignmentClient` 的完整拓扑预检和审计下；生产GUI在 System Prepare 中调用它，独立 `getdaq-align` 用于显式离线/维护执行。选定CoBo维护窗口的“重准备”执行完整CoBo Prepare计划并保持其他板及已保存对齐，不重复定位或触发跨机箱TDC流程。这与2018教程保留其他正常CoBo对齐的修复意图一致。当前没有隔离、可证明只影响选定板的TDC realignment接口，因此不增加看起来像单板对齐、实际会触及共享MuTaNT的GUI按钮；若现场需要独立TDC诊断，应先确认硬件影响范围、操作状态和恢复流程。
