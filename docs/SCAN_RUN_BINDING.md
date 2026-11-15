# 扫描点与 Run 归档关联

本工具把已经完成的扫描点采集与已经封存的接收归档关联起来。它填补“扫描器生成了配置点、Run产生了数据，但以后难以确认该归档对应哪个配置点”的记录缺口。绑定和索引命令只检查元数据；阈值分析读取MFM帧头，AutoGain/Pulser波形分析只读读取绑定归档中的波形载荷。所有命令都不连接ECC/硬件、不启动/停止DAQ、不复制或修改原始数据。

## 1. 输入之间怎样建立可信关联

扫描计划的`scan-manifest.json`将有序点号映射到完整系统工件，并记录每点的`configuration_sha256`和`artifact_sha256`。每个点工件自身也会校验这两个摘要。

接收归档包含原始`request.json`、`manifest.json`和`catalog-complete.json`。请求记录实验名、Run编号、完整接收计划以及配置/工件摘要；归档manifest记录会话和全部来源封存信息；完成回执将请求键和manifest摘要绑定。`getdaq-catalog --bind-scan-point`先调用完整归档检查，再要求接收计划的两个摘要与所选扫描工件完全相同。摘要不匹配时拒绝生成凭据。

成功后生成独立JSON凭据，包含扫描类型、CoBo、点号、工件名、两个摘要、Run编号、会话ID、目录路径、目录请求键、manifest摘要和完成回执摘要。凭据写到用户指定的新文件；拒绝覆盖已有目标。输出不得放进扫描目录或归档目录，以保持两者原字节不变。

## 2. 操作步骤

也可以在 Envoy 的“Scan plan and Run control”窗口完成这两个元数据步骤：先打开并校验已有扫描计划；“Bind a completed Run catalog to one scan point”会询问从0开始的点号、已封存 catalog 目录和现有凭据目录，并独占创建对应的`point-NNN.json`。所有点都有凭据后，选择“Build the complete scan-to-Run index”，再选凭据目录和一个新的JSON输出路径。Envoy通过独立`QProcess`调用`getdaq-catalog`，因此界面线程不负责扫描归档内容；每次操作前重新校验计划摘要和点数。catalog程序可与Envoy同目录、位于`PATH`中，或在提示时手动选择；Windows程序名通常为`getdaq-catalog.exe`。必须选择与创建这些receiver catalog时相同源码版本的数据工具。程序自身负责完整catalog校验、禁止覆盖和路径边界检查。

窗口只显示工具输出，并不代替对实验Run号、封存状态及点号的人工确认。整个绑定/汇总步骤只读取计划和catalog元数据，不连接硬件或receiver、不操作运行中的采集，也不读取DAQ raw载荷。失败后先检查已有输出和catalog状态；不要改写凭据，也不要用同一文件名覆盖重试。

先照常生成扫描计划。每个点都要由操作员使用支持的ECC流程单独执行完整Configure和独立Run；停采后再通过正常receiver收尾、catalog封存。不能在运行中的Run里热切换配置。

确认某个Run对应的receiver归档已经完成后执行：

```bash
getdaq-catalog --inspect-catalog /data/catalog/Experiment/run_0007/deployment
getdaq-catalog --bind-scan-point /data/plans/peaking-scan-plan 3 \
  /data/catalog/Experiment/run_0007/deployment \
  /data/records/Experiment-run_0007-point_03-binding.json
```

点号从0开始。命令会从catalog请求读取实验名和Run编号，不接受用户另外输入一组可能彼此矛盾的身份字段。输出目录必须已存在，凭据文件必须尚不存在。成功时标准输出同时打印凭据JSON。可将凭据和扫描计划一并纳入实验记录；它不改变接收目录。

## 3. 汇总整套扫描

所有点完成并绑定后，可生成只读汇总索引。PeakingTime、Pulser、AGET Threshold和AutoGain计划均可使用此通用索引。先将每个点的绑定凭据放入一个单独的新目录，按连续点号命名为`point-000.json`、`point-001.json`等（最多256点）。每个凭据仍指向其原始封存catalog；不要移动、修改catalog或手工改凭据。然后运行：

```bash
getdaq-catalog --summarize-scan /data/plans/peaking-scan-plan \
  /data/records/Experiment-run-0007-bindings \
  /data/records/Experiment-run-0007-scan-index.json
```

汇总器按计划顺序读取每个完整凭据，重新打开并检查对应catalog，重算该点绑定内容并作精确比较；缺点、重复Experiment/Run、重复接收会话、变化的计划/凭据/catalog或已存在的输出都会令整个命令失败，不会生成一个标为complete的部分索引。成功文件`getdaq.scan-run-index`包含整套计划摘要及每点的配置/工件摘要、Experiment、Run、session、catalog路径和catalog回执摘要。输出独占创建，且必须位于计划、凭据目录和各catalog之外。

它是供追溯和后续分析脚本读取的JSON目录，不是自动结果分析：不会算FWHM/增益/阈值，不判断波形质量，不读取`.graw`载荷，也不证明板端真实加载或硬件读回。若某点失败，应保留已有证据，先单独完成该点对应Run/catalog，再重新生成整套索引到一个新的输出文件名。

## 4. 检查和失败边界

- 支持PeakingTime、Pulser幅值、AGET Threshold和AutoGain配置计划schema版本1。绑定只比较公共的完整工件摘要；特定扫描值及通道号保存在扫描manifest中。
- 计划点号必须从0连续排列，点工件名必须是计划目录中的单个文件名，SHA与工件内部摘要必须一致。
- Pulser/AutoGain归档分析还会重新核验manifest声明的起止电压、步长、完整点数及点序；AutoGain同时核对通道范围和旧控制器的Reg3/Reg4通道顺序。只重新生成点绑定不能使字段互相矛盾的计划变为有效计划。
- Threshold归档分析会核对`start_code`/`stop_code`/`step_code`、完整点数和每点阈值码，再将其与完整工件中的粗/细寄存器值比对；这仍不解释hit-pattern统计的物理含义。
- 归档须通过原有catalog检查：receiver正常退出、会话/计划/封存内容/文件集合和完成回执都要一致。检查器只读元数据及文件身份，不读取DAQ载荷字节。
- 输出凭据采取独占创建；运行失败时保留任何已生成的结果供人工检查，不覆盖、重试或改写原文件。
- 汇总要求每个点都有独立、内容仍匹配的绑定凭据和已封存catalog；输出仅表示所有扫描点的归档关联完整，不表示采集结果通过质量判定。
- 若归档所绑定的配置摘要与扫描点不符，检查器拒绝写入。应查明实际Run配置并使用正确计划点，不要手工修改SHA。
- 只有摘要与归档声明一致的证据。`hardware_loaded_verified`固定为`false`：归档无法证明硬件寄存器真实读回或现场实际采集时的板端状态。运行、采集质量、外部Pulser电压精度及分析结果仍需其他来源验证。

## 5. 验证范围

`receiver-catalog-filesystem`回归还验证汇总完整单点计划、重建索引字段、拒绝覆盖既有索引、拒绝已过期凭据，以及封存catalog检查前后回执不变。夹具使用小型元数据及原有稀疏文件系统测试文件，不传输DAQ数据；该测试不是真实扫描采集、板卡加载或人工流程验收。平台交付状态以[验证记录](VALIDATION.md)和[重构状态](REFACTOR_STATUS.md)为准。

## 6. AutoGain 绑定归档波形响应

AutoGain 每个点都完成独立配置、Run、正常停止和catalog封存，并已按连续点号将绑定凭据放入目录后，可让原生 `getdaq-catalog` 直接按凭据找到归档文件并汇总：

```bash
getdaq-catalog --analyze-autogain-waveforms /data/plans/autogain-plan \
  /data/records/autogain-bindings 0 \
  /data/records/autogain-waveform-response.json
```

数值 `0` 是MFM帧头中的CoBo编号，需由操作人员确认与计划源ID（如`C0`）的对应关系。程序逐点重算绑定、检查catalog及接收源，核对manifest中的通道/幅值与该点完整配置工件，然后只读流式处理该CoBo/AsAd/AGET/通道的样本，报告各Run的帧数、缺失/无效/有效波形数、平均峰值ADC、peaking和FWHM buckets。已有输出不会被覆盖。

当前接受普通MFM中的CoBo type-1 revision 0..5、compact type-2 revision 5，以及`0xFF01` revision 1分层容器中的这些CoBo子帧。type-2必须包含完整的4×68×512 AGET样本；分层容器最大32MiB、最多8层。命中目标源但遇到其他revision、坏帧布局、重复bucket或归档身份变化时会报错，不输出不完整的成功报告。每个测试点必须有独立Run和session绑定；失败时修复证据并使用新报告路径重跑。

该报告证明配置点与所选封存catalog的关联，并给出该格式支持范围内的波形统计；它不证明板端加载、Pulser真实电压、探测器物理增益、科学质量或合格判据。不会生成物理增益值，也不参与DAQ接收/传输。`.graw`载荷没有逐文件密码学摘要时，catalog凭据不应被误解成原始载荷的防篡改证明。

## 8. Pulser 幅值扫描归档波形响应

Pulser幅值计划只改变CoBo的`Generator/amplitudeStart`，manifest没有记录AsAd、AGET或通道选择。因此，操作员必须从实验配置中确认目标芯片和活动通道，再显式运行：

```bash
getdaq-catalog --analyze-pulser-scan /data/plans/pulser-plan \
  /data/records/pulser-bindings 0 0 0 7 \
  /data/records/pulser-waveform-response.json
```

参数依次为扫描目录、连续点号命名的绑定目录、MFM帧头CoBo编号、AsAd编号、AGET编号、通道号和新报告路径。程序逐点核验Pulser计划/Run/catalog绑定及完整配置工件中的`amplitudeStart`，并确认指定通道在每个点工件中均为活动通道。它按每个封存Run输出幅值mV、帧/缺失/无效/有效计数、平均峰值ADC、peaking/FWHM buckets。当前 MFM 格式支持范围与AutoGain分析相同；报告kind为`getdaq.pulser-waveform-response`。

通道由操作员指定，不是manifest绑定字段；因此`mapping_verified`表示计划点和Run/catalog绑定通过核验，不表示通道是由计划自动确认。分析不证明Pulser实际输出电压或硬件读回，不换算物理增益，不做质量判定。输出采用独占创建，不能覆盖已有文件。

## 9. PeakingTime 扫描归档波形响应

PeakingTime计划的点工件不在manifest中重复写出寄存器值。分析器从每个绑定的完整配置工件读取所选AGET的`Reg1/peackingTime`，并按点号和芯片版本核对旧计划表中的16个合法值。AsAd、AGET和活动通道仍由操作员显式给出：

```bash
getdaq-catalog --analyze-peaking-scan /data/plans/peaking-plan \
  /data/records/peaking-bindings 0 0 0 7 \
  /data/records/peaking-waveform-response.json
```

参数含义与Pulser分析相同。报告每点携带实际配置工件中的`peaking_time`、Run/catalog凭据摘要和波形响应统计，避免按点号自行推测时间值。支持的MFM帧及上限同AutoGain分析。该功能仅做已封存数据的离线响应分析，不执行逐点配置/采集，不证明硬件读回，也不判定物理性能或扫描优劣。

## 7. AGET Threshold 离线 hit-pattern 分析

阈值扫描每点都完成完整 Configure、独立 Run、正常停止和归档封存后，可以先按前述步骤把每个点绑定到 catalog，再把所有 `point-NNN.json` 放在一个绑定目录中。运行：

```bash
getdaq-catalog --analyze-threshold-scan /data/plans/threshold-plan \
  /data/records/threshold-bindings 0 \
  /data/records/threshold-hit-pattern-analysis.json
```

第三个参数 `0` 是 MFM 帧头中的数值 CoBo ID（0..255）；它和计划里的源 ID（例如 `C0`）是不同字段，程序不会猜两者映射。分析器会验证计划中目标源 ID确实存在于每点绑定的 receiver plan，再核验配置摘要、点号、Run、session、封存catalog及回执；只读取目标来源下的 `.graw`，每个文件以只读、拒绝符号链接方式打开，并在分析前后检查设备号、inode、大小和元数据稳定性。随后流式统计选定 CoBo/AsAd/AGET 的 type-1 revision 0..5 帧头 68 通道 hit 位，内存不随 raw 文件大小增长。

输出是不可覆盖的 JSON，每点包含阈值码、配置摘要、Run/session/catalog 回执、已读文件数、匹配帧数及68个通道的hit帧数。字段 `mapping_verified=true` 只表示扫描点与封存归档的摘要关联通过核验；`hardware_loaded_verified=false` 表示软件不能证明硬件已读回该配置。结果只是头位计数，不等于完整波形、阈值效率、噪声率或物理阈值判定。当前不解析 layered `0xFF01` 或 compact type-2 帧；遇到封存文件中损坏/不完整的 MFM 流会整体失败，不生成部分结果。
