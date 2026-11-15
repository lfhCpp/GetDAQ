# 操作台扫描计划与Run控制窗口

操作台提供一个图形界面，帮助操作员把已审核的完整系统实验工件转换为 PeakingTime、Pulser幅值、单颗AGET阈值或 AutoGain 配置扫描计划。计划生成由同目录交付的 `getdaq-config` 命令行程序完成；计划创建本身不连接 ECC、不操作板卡或接收数据。计划审核后的独立运行区可连接Native ECC，对**已由操作员按正常发布流程装载并配置的当前点**执行整系统生命周期及受事件数约束的单点Run。Qt仍不接收、缓存或转发DAQ原始数据；实时数据由原生接收进程处理。

## 1. 打开窗口

启动 `GetDAQ` 后，在顶部菜单选择 **Scan plans and runtime checks...**。窗口标题为 **Scan plan and Run control**。计划创建与计划校验仍为离线工作；后面的Native运行区会连接ECC做身份/状态核对，并可在满足条件时向整套ECC提交生命周期操作。如果程序提示找不到 `getdaq-config`，请确认它与 `GetDAQ` 主程序位于同一目录，并且两者来自同一版本的交付包。不要把另一个版本的 `getdaq-config` 单独复制过来。

扫描计划成功打开或创建并校验后，窗口也会提供单点 Run 归档绑定和整套扫描索引按钮。这两项由 `getdaq-catalog` 独立命令行工具执行；若它不在Envoy同目录或系统 `PATH`，窗口会让操作员定位单独安装的数据工具程序并在当前窗口会话中复用所选路径。该工具必须与生成receiver catalog的版本相符。完整操作步骤、凭据目录约束、失败处理和证据边界见[扫描点与Run归档关联](SCAN_RUN_BINDING.md)。成功的单点自动Run在达到事件数后执行整系统Stop及常规Breakup、receiver Finish/seal、catalog和Run记录收尾；之后仍需显式把封存Run绑定到扫描点。窗口不会从运行中的采集读取数据。

## 2. 准备输入

先按[系统实验配置手册](SYSTEM_EXPERIMENT_CONFIGURATION.md)编译并审核完整系统工件。输入应为正常配置流程产生的 `getdaq.system-experiment-settings` JSON，而不是旧 `.xcfg`、单板参数片段或手工拼接的 JSON。工具会再次校验工件、目标 CoBo 和参数范围；不通过时不会生成计划。

**Complete system artifact** 选择该 JSON 文件；**Target enabled CoBo ID** 填写工件中明确启用的板号，例如 `C0`。目标板必须存在且包含活动 AGET/ASTRE 芯片。计划会保留其他板的系统配置，只调整所选板的扫描参数。

## 3. 选择扫描类型并生成

- **Peaking time** 使用该芯片类型定义的16个离散时间点；界面会禁用 Pulser 的 mV 范围控件。
- **Pulser amplitude** 使用起点、终点和正步长（单位 mV）生成升序闭区间，步长必须恰好到达终点，最多256点。起点还必须与输入工件中该板的当前 Pulser 起始幅值一致。
- **AGET threshold code** 还需指定目标活动芯片的 AsAd 与 AGET 编号（各0..3），并用0..127的起点、终点和正步长生成升序闭区间，最多128点。起始码高3位必须等于该芯片基准配置的全局阈值；低4位作为首点通道阈值。详细字段和与旧 Threshold 循环的差异见[阈值计划手册](THRESHOLD_SCAN_PLAN.md)。
- **AutoGain** 还需指定目标活动芯片及0..67内的活动通道闭区间，并输入整数mV范围。起点必须与基准配置 Pulser 起始幅值一致，通道数 × 幅值档数最多256点。每个点只选择一个通道并设置目标 Pulser 幅值，完整点工件按旧 Reg3/Reg4 单热位顺序排列；manifest 会记录每点通道号。它只生成配置，不采集事件、不判断增益。详见[AutoGain计划手册](AUTOGAIN_SCAN_PLAN.md)。

选择一个已经存在的 **Existing output parent**，并填写一个尚不存在的简单 **New plan folder name**。程序拒绝覆盖已有目标目录。点击 **Create new offline plan** 后，界面通过异步子进程调用同目录的 `getdaq-config`；生成过程中创建/打开计划按钮会禁用，窗口不能提前关闭，避免异步完成结果与另一计划目录混淆。完成或启动失败后按钮恢复。完成后，状态栏显示扫描类型、目标 CoBo、配置点数及点文件在场数。窗口把计划目录交给 Qt-free 核心校验器：按计划类型精确核对manifest及点对象的字段集合（缺字段、未识别字段都会拒绝），检查schema版本和安全边界声明、完整扫描网格与点数、连续索引、点文件名安全与唯一性、文件大小和普通文件属性，然后流式校验文件SHA-256并完整解码每个系统实验工件。最后逐点比对工件摘要及manifest配置摘要，并核实扫描值确实写在对应的配置参数中，包括四种扫描各自的完整点序和值域。相同校验可脱离GUI运行：`getdaq-config-check scan-plan PLAN_DIRECTORY`。任何检查失败都会报告原因并要求检查输出目录，不会显示计划成功摘要。摘要只用于检测目录内部篡改或自相矛盾，不是签名，也不证明配置被硬件加载。失败时会显示错误文本，原有目标文件不会被覆盖。

## 4. 审核输出与后续采集

输出目录包含 `scan-manifest.json` 和每个扫描点的完整系统工件（例如 `point-00.json`）。先阅读 manifest，再逐点审阅工件和摘要。计划只描述参数，不代表硬件已经加载这些值。

如果计划是在之前的操作台会话中创建的，可点击 **Open an existing offline plan for review** 并选择该计划目录。窗口会重新检查manifest、完整点网格、每个点工件及摘要；任何文件缺失、被改动或语义不匹配时都不会打开，也不会启用审阅按钮。必须先创建有效的有限停止策略，再确认执行输入已审阅。确认前核心会重验计划、策略和全部点工件，在本机用户私有的`scan-journals/`目录保存快照及journal；journal记录快照文件SHA-256，后续操作intent必须引用已绑定的快照身份。重开计划时会重新复核该快照。重复确认相同输入会显示已经审阅。旧的未绑定journal只有在还没有任何外部操作历史时才能安全补绑；已有操作历史会保留并拒绝迁移。此记录只固定“准备执行什么”，不会自动配置硬件或重跑采集。

**Inspect or reconcile a pending operation** 可查看该计划对应的本机执行journal。如果进程曾在保存操作意图后异常退出，journal会把未完成的原操作隔离为 `NeedsReconciliation`，而不会自动重发。只有确有未知操作时，窗口才允许操作者选择“Confirmed completed”或“Confirmed not sent”，填写操作者标识和人工观察说明；窗口会再次校验计划摘要、点数和原operation ID，再把选择及说明摘要写入journal。选择“Confirmed completed”会按原操作定义推进状态；“Confirmed not sent”会恢复原状态，但旧operation ID仍不可复用。若日志状态已变化，必须重新打开状态检查，过期操作不会被接受。

当前点状态卡片显示的是本地journal阶段，不是实时ECC/CoBo读回。若看到`RunStarted`或`RunStopped`琥珀提示，应先重新读取主窗口的新鲜ECC和receiver状态，再按原Run/Stop操作记录核对收尾阶段；这不是自动Resume按钮，也不表示可以直接开始下一点。只有journal确有未决或未知操作时才可使用人工对账入口，不能用手工对账代替ECC/receiver状态观察。

人工对账不是硬件核验。操作者标识只是自我声明，不是账户认证；说明文字本身不写入journal，只保存SHA-256摘要。请把说明和实际查看的独立证据保存在实验记录中。该入口不发送ECC/板卡命令，不恢复采集，不检查receiver、catalog或raw，也不能代替完整错误恢复流程。不能仅凭GUI的对账结果判断硬件一定完成了操作。

窗口尚不能自动逐点执行完整扫描。打开或创建并成功校验计划后，PeakingTime、Pulser和AutoGain可在 **Finite stopping policy** 区域填写每点事件数及最长运行毫秒数；数值没有默认值，0表示尚未选择。确认框会列出计划身份和停止条件，需明确确认才会继续。策略固定保存为计划目录下的 `scan-execution-policy.json`，只允许新建，不能覆盖已有文件。GUI随后调用只读检查器复核计划摘要绑定、策略类型和点数；失败时会保留文件及错误输出供检查。Threshold计划当前显示为 **manual-only**，没有自动Run策略入口；每点事件数停止条件及Threshold触发/Pulser语义完成核实前，只能按单独审阅的人工流程逐点操作。旧的定时窗口策略文件也不会被新检查器接受。

策略本身只是可审阅的元数据，不会改变扫描计划或点工件。运行窗口允许对一个已绑定的当前点逐次提交 **Describe、Prepare、Configure**：每个动作前都要对照新鲜ECC身份、状态和receiver观察，并单独确认；应用先把该操作意图写入扫描journal，再向整个Native ECC系统提交一次转换，收到成功应答后还要新查询ECC状态，确认一致后才推进journal。操作失败、超时或状态不一致时会保留未知操作供检查，不会自动重发。这里的Configure使用ECC实例已固定的系统工件，**不会从扫描点JSON加载/切换参数**；执行前必须确认当前ECC工件摘要已经与选中点一致。多板系统状态仍属于整个ECC实例，界面动作也作用于全系统，不表示只改变目标CoBo。

当前已接通的是**单点运行闭环**，不是全自动多点扫描：操作员先按正常发布流程把匹配点工件装载到ECC，再逐次确认生命周期动作；显式事件数达到后，程序自动Stop、Breakup并完成receiver封存、catalog和Run记录。真实硬件加载及receiver行为仍须现场验证。Run归档绑定后，还要由操作员确认该点退休，并在ECC外切换到下一点工件、重启/重新接纳新部署和重新核验身份；GUI不会自动进行这些会影响硬件配置的跨点操作。跨点失败/崩溃恢复也未闭环。不要在Running状态下切换工件或热写寄存器。旧Gain/AutoGain物理测量和判定，以及Threshold触发/Pulser动作和结果消费仍待实现或核实。

## 5. 与数据接收的边界

本功能仅把路径、CoBo ID 和少量参数交给配置 CLI。Qt 使用 `QProcess` 异步等待低频计划生成，既不承担任何高吞吐 socket，也不打开 `.graw` 文件。实时采集仍由独立的原生 C++ 数据接收进程负责；GUI 增加此窗口不会把 Qt 引入 DAQ 数据通路。

## 6. 构建与验证

窗口源码列在 `apps/envoy/envoy.pro` 和 `tests/tests.pro` 中，随现有 qmake 工程构建；项目没有单独的 `apps/envoy/CMakeLists.txt`。当前QtTest由离屏真实窗口调用同目录CLI，覆盖计划生成、有限停止策略创建/只读复核、无停止条件时拒绝创建、既有计划/策略防覆盖、非法输入及CLI缺失反馈；另有本机fake ECC成功链覆盖单点事件数Stop到Run/catalog绑定。完整Widgets QtTest数量和平台范围以最新[验证记录](VALIDATION.md)为准。fake服务及其它测试夹具不连接真实硬件，也不模拟真实receiver文件吞吐；这不代表现场硬件、多点自动执行或DAQ性能验收。用户Windows Qt Creator可按[Windows Qt 5.14.2构建手册](WINDOWS_QT5142.md)打开根目录 `GetDAQ.pro` 构建整套应用。
