# GetController PeakingTime 扫描计划

操作台内也可通过[离线扫描计划窗口](SCAN_PLAN_GUI.md)以图形界面调用这些生成命令。

本文介绍 `getdaq-config plan-peaking-scan`。它是旧 GetController 单板自动测试的一段迁移：把系统实验工件中的 AGET/ASTRE peaking time 改成16个候选值，并生成16份完整、不可变的系统配置工件。程序是离线配置工具；它不会连接 ECC、写寄存器、启动/停止 DAQ、创建 Run 或接收数据。

## 1. 为什么不照搬旧线程

旧 `GetController-release-20210208/src/get/TestController.cpp` 的 PeakingTime 分支逐点停止采集，切换到慢控制后直接写每片 AGET 寄存器，再清空显示缓存并重启采集。这个在线热修改路径绕过了新 ECC 的完整 Configure 状态检查，也无法保证寄存器设置、Run 参数和接收器归档彼此对应。旧源码中的 FWHM 计算和日志输出位置只有注释，没有实现。当前已另提供**归档后的离线波形汇总命令**，见本文件第 4 节；它不执行旧线程的在线控制，也不自动把扫描点与 Run/归档绑定。

新计划把每个 peaking time 变成独立配置工件。操作者应先审阅计划，然后为每个点单独完成完整 Configure 和采集 Run，并把 Run 与对应工件/归档关联。分析在采集后进行。计划生成CLI本身不连接ECC或自动执行这些步骤；Envoy现可对已由操作者装载并身份核对的单个当前点执行完整Configure、必填事件数Run/Stop和receiver/catalog收尾，但不自动装载下一点或跨点继续。它也不替用户选择每点事件数、阈值、停止条件或结果表格式；完整边界见[扫参GUI手册](SCAN_PLAN_GUI.md)与[验证记录71.322](VALIDATION.md#71322-单扫描点run至归档绑定的fake-ecc集成回归2026-10-23)。

## 2. 命令

先由现有配置流程生成完整系统实验工件，例如：

```text
getdaq-config compile-system experiment.xcfg profile.json system-experiment.json
getdaq-config plan-peaking-scan system-experiment.json C0 peaking-scan-plan
```

输入必须是当前 `getdaq.system-experiment-settings` 完整工件，包含至少一个启用 CoBo 和活动 AGET 芯片。版本必须显式声明为 AGET `0x201`、`0x202` 或 ASTRE `0x203`。程序不猜测芯片类型；基准工件中原有 peaking time 必须已符合芯片型号对应的合法表，否则整个命令拒绝。

命令必须显式指定一个启用的 `COBO_ID`，与系统工件中的 CoBo ID 完全匹配；目标板必须含至少一颗活动 AGET/ASTRE。其他 CoBo（启用或禁用）的配置逐字段保持不变。这样保留旧工具“单板扫描”的目标边界，而完整系统工件仍能保留其余系统的配置和来源。

目标目录必须不存在，父目录必须已存在。命令先在目标父目录下创建私有临时目录，完整写入并关闭所有文件后再发布目录；已有目标不会被覆盖。发生写入错误时临时目录由 Qt 自动清理。不要把输入路径指向会被其他进程同时修改的工件。

## 3. 输出

```text
peaking-scan-plan/
  scan-manifest.json
  point-00.json
  point-01.json
  ...
  point-15.json
```

每个 `point-NN.json` 都是完整系统工件，而不是寄存器补丁。输入中来源路径和来源文件摘要会保留；执行配置摘要会因扫描参数不同而变化，工件摘要重新计算。Manifest记录选中的 `target_cobo`，列出16点顺序、文件名、配置摘要和文件摘要，并明确标注 `hardware_loaded_verified: false`、`requires_full_configure_per_point: true` 及 `acquisition_event_count_unspecified: true`。

索引 `0..15` 对应 AGET 时间表：`70, 117, 232, 280, 334, 383, 502, 541, 568, 632, 721, 760, 831, 870, 976, 1014`。ASTRE使用其独立时间表：`68, 116, 180, 225, 343, 442, 541, 641, 739, 836, 943, 1039, 2034, 3985, 5770, 7700`。多板/多片配置在同一个扫描点上对每颗芯片写入相同索引，但数值按明确芯片型号选表。

## 4. 归档后的离线波形汇总

`getdaq-waveform-viewer` 增加命令行入口，可对操作者明确指定的 `.graw` 文件映射运行既有离散波形分析，并按扫描点汇总有效波形数、缺失/无效波形数、平均峰值 ADC、平均 peaking buckets 和平均 FWHM buckets：

```text
getdaq-waveform-viewer --analyze-peaking-scan COBO ASAD AGET CHANNEL POINT_INDEX FILE [POINT_INDEX FILE ...]
```

例如，同一点可以对应多个分段归档文件；相同点号会聚合到同一行：

```text
getdaq-waveform-viewer --analyze-peaking-scan 2 1 0 7 0 run-001.graw 0 run-001-part2.graw 1 run-002.graw
```

点号必须是 0..15 并按非递减顺序传入；每个输入文件必须能被当前 `.graw` reader 完整索引。分析器按指定 CoBo/AsAd 筛选帧，并从指定 AGET/channel 读取样本。输出是 CSV，时间宽度单位为采样 bucket。CLI 不打开 Qt GUI，也不连接 ECC 或 DAQ。

文件与点号关系由操作者提供；该命令不读取扫描 manifest、Run 绑定凭据或封存 catalog，因此不验证文件确属所声明的扫描点，也不验证采集参数曾加载到硬件。它是离线查看辅助功能，不是完整扫描执行器或数据接收器；建议在确认每点 Run/归档关系后使用，并保留原始归档。

## 5. 安全与解释边界

- 工件只表示计划参数，不表示参数已经加载到任何硬件，也不表示完整准备/校准已完成。
- 不要在 Running 时直接应用下一个工件或手动热写寄存器。每个点必须走新 ECC 完整状态流程，并产生独立 Run/归档。
- 每个扫描点只改变所选 CoBo 的活动芯片 peaking-time；系统工件内其他 CoBo 配置不变。不要在计划生成后手工删改 JSON。
- PeakingTime工具没有 event count、FWHM、增益或结果文件格式策略。Threshold现有[独立离线配置点计划](THRESHOLD_SCAN_PLAN.md)只生成显式单颗AGET的完整工件，不包含旧事件统计；旧 `hitPat_0` 含待确认注释。`CoBoEvent`源码解释72位容器服务68个实际通道、后4个结果列不被旧循环更新；其物理阈值解释和下游结果格式仍未通过真实数据确认。
- `.json` 文件及摘要可用于离线比对和记录，但摘要不证明源工作区仍未变化，也不替代正式配置发布、操作者复核或硬件读回。

## 6. 与其他扫参功能的关系

当前PeakingTime计划覆盖16个GET实验离散时间值。Pulser幅值、AGET阈值和AutoGain配置点的独立离线计划分别见[Pulser amplitude scan plan](PULSER_AMPLITUDE_SCAN_PLAN.md)、[AGET阈值计划](THRESHOLD_SCAN_PLAN.md)和[AutoGain计划](AUTOGAIN_SCAN_PLAN.md)。四者只生成完整配置，不执行自动采集。

采集完成并封存后，可以用`getdaq-catalog --bind-scan-point`把匹配的配置点、Run编号和归档会话写入独立凭据；全部点完成后，可用`getdaq-catalog --summarize-scan`重新核验每份凭据及其封存catalog，并生成完整扫描Run索引。两个命令都只核对元数据/摘要，不改变DAQ控制状态、不读`.graw`。操作、字段和证据边界见[扫描点与Run归档关联](SCAN_RUN_BINDING.md)。

### `hittedChannel` 分支的迁移边界

旧 `TestController.cpp` 还含有名为 `hittedChannel` 的线程分支。源码显示它读取当前 Pulser 幅值、注入电荷并等待事件；当 `nbEvent == statNb` 时停止发送、切慢控制、左移通道掩码并选择 Reg3 或 Reg4，再恢复快控制和发送。该分支没有自己的逐点结果表、命中判定或文件输出实现。循环最初的通道寄存器选择也不是在这个分支里初始化的，因此不能只从该代码断言它的首个通道或与 AutoGain 完全相同的序列。

旧 `WorkspaceManager.cpp` 默认列出的工作区类型是 `Default`、`StdAcquisition`、`Acq2pMode` 和 `PeakingTime`，没有 `hittedChannel`。分支存在不等于它是现场仍使用的入口。AutoGain 离线计划允许把起始、终止Pulser幅值设成同一个mV值，从而表达“多通道、固定幅值”的一组配置点；这只覆盖参数列表，不覆盖旧线程的事件数、在线寄存器切换、发送启停或结果解释，不能标记成 `hittedChannel` 的完整替代。

## 7. 当前未完成

### 旧 Gain / AutoGain 循环的源码行为核对

对照旧 `GetController-release-20210208/src/get/TestController.cpp` 可确认：`Gain` 从 `Generator/amplitudeStart` 开始，以 `amplitudeStep` 递增，`amplitudeStop`作为上界；每个幅值采集到 CoBo `Module/nbEvent` 个事件后停止并切慢控制，再改幅值、恢复快控制并继续。旧配置格式回退读取AsAd0下的`Generator/Amplitude`、`Increment`和`AmplitudeStop`。`AutoGain`在相同幅值循环上再切换Reg3/Reg4 one-hot掩码；初始掩码为1，然后逐位左移。

这两个分支没有在循环中实现物理增益公式、最佳增益选择、结果表或结果文件输出。因此当前 Pulser/AutoGain计划器只替换参数点生成，并不等于旧采集循环迁移，更不等于自动增益判定。旧循环使用单板线程中的运行时改寄存器/切快慢控制；新系统要求每个点走完整Configure并独立Run/归档，不能照搬运行中热写。接下来的执行器必须显式获得每点事件数和安全停止/失败恢复规则；旧代码的`nbEvent`来源可追到`CoBo/Module/nbEvent`，但现场是否仍采用这套约定以及运行中断后的处理尚未确认。

目前PeakingTime、Pulser幅值、Threshold和AutoGain都有离线配置点展开工具；旧`hittedChannel`分支仅有逐通道寄存器切换、固定幅值注入和事件等待代码，未确认是仍使用的工作区入口；单幅值AutoGain计划只能表达类似的配置点列表，不能替代其执行语义。另有归档完成后的离线Run关联凭据和操作者提供文件映射的PeakingTime波形汇总CLI；该CLI不验证manifest、Run绑定或catalog。AutoGain计划保留逐通道选择和Pulser幅值扫描参数，但旧事件测量循环、增益公式/判定、结果文件、事件数策略、停止/失败恢复都仍未迁移。Threshold hit-pattern统计/结果消费也未全部迁移。旧 Threshold `hitPat_0` 有待确认注释；源码已解释72位容器对应68个实际通道，但物理阈值解释及结果下游仍待现场核实，因此当前实现不生成自动测量结果。完整执行器应先核实现场仍使用的工作区/测试类型及下游结果格式。现有ECC Run固定绑定配置快照，不能在Running中切换参数；每个点必须形成独立配置绑定和Run，或扩展受审计的原生编排协议，不能只在GUI里轮流替换工件。当前关联凭据只证明所选配置摘要与已完成catalog记录一致，不证明真实寄存器读回或硬件已加载。Qt配置工具只负责低频计划管理；原始数据仍由独立原生C++接收路径传输和保存。

源码：[`TestController.cpp`](../source/get/GetController-release-20210208/src/get/TestController.cpp)中的`getGeneAmpStop`、`getGeneIncr`、`getEventNb`与`run()`。
