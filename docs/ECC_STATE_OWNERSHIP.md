# ECC状态属于整套系统，单板维护是独立操作

本页记录旧版ECC状态机与当前Envoy/native ECC控制的对应关系，解释主界面的“单板”按钮。

## 旧版状态机的作用范围

用户提到的旧主程序 `source/get/GetController-release-20210208/src/get/GetTest.cpp` 也包含单 CoBo 工作流，但它的 `Ctrl::State`（`Off/Idle/Loaded/Configured/Running`）是 `GetTest` 窗口级状态，不是每块板各自保存的一份状态机。`loadHw()`调用单个`TestController::loadAndPrepare()`，`configure()`调用单个`TestController::configure()`，启动/停止则作用于该controller线程。`TestController::configure()`中的CoBo索引固定为0，并在该板内遍历4块AsAd和活动AGET；`loadAndPrepare()`同样通过一个`targetEndpoint`初始化当前目标。因此它能控制一个选定CoBo的生命周期，但源码没有在一个窗口中并行维护多个独立CoBo FSM的证据。

旧`GetTest::startDaq()`在调用线程开始前先把窗口状态设为`Running`，`stopDaq()`则用事件循环等待controller结束；重构时应以异步确认状态和持久操作证据驱动UI，不能照搬这种先更新界面、后执行硬件操作的次序。

旧版源码 `source/get/GetBench-software-release-20210208/src/get/rc/BackEnd.cpp` 的 `BackEnd::createEngine()` 创建名为 `SM_ECC` 的一个ECC状态机：

- `Idle --describe--> Described --prepare--> Prepared --configure--> Active/Ready`
- `Active/Ready --start--> Running --stop--> Active/Ready`
- `Running --pause--> Paused --resume--> Running`
- `Paused --stop--> Active/Ready`
- `Active --break--> Prepared`

同一个 `BackEnd` 对象执行这些转移。其 `onConfigure()` 调用 `systemMgr_.configure(...)`，`onStart()`、`onStop()`分别调用 `systemMgr_.start()`、`systemMgr_.stop()`；`onBreakup()`先执行全局Stop，再调用`systemMgr_.breakup()`。所以该FSM描述的是一个ECC所管理的系统生命周期。CoBo是系统准备和配置计划中的目标对象，不代表每块CoBo都拥有一个可独立Configure/Start/Stop的ECC状态机。

旧主机界面和SOAP兼容模式存在选定节点的Progress/Regress操作。它们属于旧接口的节点级兼容功能，不能直接解释为新native协调器中每块CoBo都有完整的系统状态。尤其是Configure、Start、Stop和Breakup，不能通过逐板按钮拆成独立生命周期，因为旧后端把这些动作委托给SystemManager。

## 当前native控制行为

当前 `apps/envoy/nativeruncontrol.cpp` 明确把native状态推进、回退、采集启动与停止交给一个ECC服务控制整套固定部署。主窗口的全局按钮仍作用于整个系统。多板native模式不开放旧的逐行Progress/Regress，以免状态表每行重复显示的系统状态被误认为单板状态；唯一CoBo、无MuTaNT的部署按下述条件开放明确标注的单CoBo系统快捷操作。

主窗口现在对真正的单CoBo固定部署开放选中行生命周期快捷入口：连接后只读读取服务端固定活动目标；只有服务端恰好返回一个CoBo、该CoBo与唯一配置数据源ID匹配、且配置没有MuTaNT时，选中行Progress/Regress才转发为整套ECC的Progress/Regress。按钮明确标注“single-CoBo system”，它复用已有Describe/Prepare/Configure/Breakup/Reset状态机，状态依旧属于ECC实例。目标不唯一、配置不匹配、含MuTaNT或目标列表不可用时，快捷入口不出现，原Prepared-only单板维护仍按原门槛工作。此功能不是多板部署内每CoBo独立Prepare/Configure/Start FSM。

主窗口将入口按 **Configuration、Tools、Run、Recovery and safety** 分组，减少原来所有动作横向挤在菜单栏的情况。顶部模式和连接相位仍描述控制会话；节点表中Native ECC重复显示的状态列仍描述同一个整系统状态。界面样式不改变旧SOAP节点级兼容路径，也不改变Native命令作用范围。

选中行后的 **Maintain selected CoBo…** 打开 `CoBoMaintenanceWindow`。界面从固定的活动CoBo目标清单中按目标ID优先、节点名称唯一匹配的方式预选目标；找不到唯一匹配时不猜测。它执行的是显式的单板维护，不是ECC状态机转移：只有服务端重新确认同一实例处于健康Prepared、数据进程停止且互斥条件通过后，才允许对固定活动目标执行重准备或诊断命令。重准备成功后系统仍为Prepared，其他板及已保存系统对齐保持不变，Configure/Start仍需要用户明确操作。目标ID与预期板名必须在确认框中复核。

菜单中的单板维护入口和行内快捷入口进入相同窗口、使用相同确认和操作审计。行内入口只在当前连接、非忙碌并且存在合格Prepared观察和唯一目标匹配时启用。没有独立可信状态时，按钮保持禁用；不会用缓存或行文本推断板卡状态。

## 数据路径边界

GUI只处理低频控制元数据。高吞吐原始事件接收仍由独立、无Qt依赖的数据服务负责；该窗口不会收取或转发raw事件。主窗口的样式变更不会改变接收协议或DAQ数据路径。

## 代码和验证入口

- 旧ECC状态机：`source/get/GetBench-software-release-20210208/src/get/rc/BackEnd.cpp`，`BackEnd::createEngine()`、`toSmState()`及各`on*()`动作。
- 旧GetController单CoBo窗口：`source/get/GetController-release-20210208/src/get/GetTest.cpp` 的`loadHw()`、`configure()`、`startDaq()`、`stopDaq()`和`setControlStatus()`；硬件准备与板内Configure循环见`TestController.cpp`的`loadAndPrepare()`、`configure()`。
- 当前主界面：`apps/envoy/mainwindow.cpp`，`SelectedMaintenanceTarget()`、`OpenCoBoMaintenance()`与`ShowControlSlot()`。
- 单板维护确认：`apps/envoy/cobomaintenancewindow.cpp`。
- 固定活动目标来源：`apps/ecc/native/ecccontroljobs.cpp` 的`MaintenanceTargetsJson()`。
- 维护的Prepared前提与保留语义：[单板维护说明](COBO_MAINTENANCE.md)。

测试`nativeGuiMetadataAndWholeSystemControls`核对native模式的整系统提示、单板维护入口以及多板模式不暴露逐板Regress；`nativeSingleCoBoLifecycleUsesWholeEccTransitions`核对单目标按钮仅转发整系统状态机信号，并拒绝多目标、含MuTaNT和配置目标不匹配。维护服务和单板动作另由单板维护及ECC网络集成回归覆盖。GUI单元测试不等同于真实硬件验收。
