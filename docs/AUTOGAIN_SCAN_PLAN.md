# GetController AutoGain 离线配置计划

`getdaq-config plan-autogain-scan`把已审核的完整系统配置工件展开成一组 AutoGain 配置点。它迁移的是旧程序“逐通道选择并扫描 Pulser 幅值”的**配置准备部分**。它不会启动 DAQ、操作 ECC/板卡、接收数据、计算增益或判断哪个增益合适。

## 1. 命令格式

```text
getdaq-config plan-autogain-scan SYSTEM.json COBO_ID ASAD_ID AGET_ID \
  FIRST_CHANNEL LAST_CHANNEL START_MV STOP_MV STEP_MV NEW_DIRECTORY
```

例如，基准配置的 Pulser 起始幅值为 `-1000 mV`，对 C0 的 AsAd 0、AGET 0 中活动通道 0 到 3 扫描三档幅值：

```bash
getdaq-config plan-autogain-scan system-experiment.json C0 0 0 0 3 -1000 -900 50 autogain-plan
```

这会生成 4 × 3 个完整系统工件。先从操作台正常配置流程生成、检查完整系统工件；不能直接使用旧 `.xcfg`、单板片段或手工拼接的 JSON。

## 2. 选择范围和限制

- `COBO_ID` 必须唯一对应一个启用的 CoBo；AsAd 和 AGET 编号必须在 0..3，且该 AGET 必须已包含在该 CoBo 的活动配置中。
- 通道闭区间必须在 0..67 内；区间中的每个通道都必须在基准配置中处于 active 状态。程序不会替操作员猜测未配置或停用通道是否应该参与测试。
- 幅值是整数 mV，范围为 -1000..1000，升序闭区间，步长为正数且必须恰好到达终点。起点必须与所选 CoBo 的基准 `Generator/amplitudeStart` 相等。
- 通道数乘以幅值档数不得超过 256。超过上限时应缩小范围，分成多个经人工审核的新计划；已有目录绝不会被覆盖。

## 3. 点的顺序与配置变化

每个点都是完整、可独立审核的系统配置工件，并分别计算 `configuration_sha256` 和 `artifact_sha256`。点按“单个通道扫完所有幅值，再换下一个通道”的顺序排列；同一通道内幅值从小到大。

通道顺序保留旧 `TestController` 的单热位寄存器遍历方式，然后只保留用户指定的通道区间：Reg3 的 bit 0..33 对应通道 33..0，随后 Reg4 的 bit 0..33 对应通道 34..67。比如通道范围 0..3 的点顺序是 3、2、1、0。界面仍用普通的起止通道号表达闭区间；manifest 每点写出实际 `channel_id`，下游应使用该字段关联结果，不要根据点号自行推算。

目标 CoBo 的 `Generator/amplitudeStart` 被设置为该点幅值；目标 AGET 的 `selectedForTest` 仅选择该点的一个通道。AGET 编译器按既有硬件映射重新生成 Reg3/Reg4。其余配置字段、其他芯片和其他 CoBo 保持原值。输出目录中的 `scan-manifest.json`还明确记录硬件未验证、每点需完整 Configure、事件数未指定、增益结果解释未验证。

旧实现以一热位掩码从 Reg3 低位递增，再切到 Reg4；新版将该映射编码成显式通道号，并可选择任意已配置 CoBo/AsAd/AGET 和一个通道子区间。点内容沿用相同的寄存器映射，但新版不假设每个现场都要扫描全部68个通道。

## 4. 现场操作边界

计划生成后，操作员必须逐点使用现有正常流程完整配置硬件并启动独立 Run。每个 Run 应按现场已验证的事件数量和终止流程运行；本计划没有替现场填写事件数。停止采集、接收器正常结束并封存 catalog 后，可用 `getdaq-catalog --bind-scan-point`把该 Run 与具体点关联，见[扫描点与 Run 归档关联](SCAN_RUN_BINDING.md)。

当前没有迁移旧 AutoGain 的事件采集循环、停止/失败恢复策略、增益公式、结果表/文件格式和质量判据。因此它不能自动执行 AutoGain，也不能宣称扫描结果已判定。归档绑定只证明运行计划声明的配置摘要与所选配置点摘要相同，不证明硬件寄存器读回。

所有点完成独立Run并封存后，可使用[扫描点与Run归档关联](SCAN_RUN_BINDING.md)为每点生成绑定凭据，再由原生工具按凭据直接分析：

```bash
getdaq-catalog --analyze-autogain-waveforms autogain-plan autogain-bindings \
  0 autogain-waveform-response.json
```

数值CoBo ID由操作人员确认与计划源ID的映射。工具检查manifest中的通道/幅值与完整配置工件一致，然后输出每点平均峰值ADC、peaking/FWHM buckets和波形计数；当前支持CoBo type-1 revision 0..5、完整compact type-2 revision 5及`0xFF01` revision 1分层容器（最多8层、每帧32MiB）。它不计算或判断物理增益，不证明硬件读回、Pulser实际电压或结果质量。完整命令及格式限制见[扫描绑定文档](SCAN_RUN_BINDING.md)。

## 5. 界面和数据通路

操作台 **Scan plans and runtime checks...** 菜单打开 **Scan plan and Run control** 窗口，其中计划生成区提供 AutoGain 模式、目标活动芯片、通道闭区间和 mV 范围。Qt 通过异步 `QProcess` 调用同目录的 `getdaq-config`，计划编辑只处理低频配置文件；DAQ 高吞吐数据继续由独立原生 C++ 接收软件处理。窗口另有受限的Native ECC单点控制区，不能自动切换或装载下一点工件。

界面操作详见[离线扫描计划窗口](SCAN_PLAN_GUI.md)。

## 6. 验证

纯 C++11 核心测试覆盖通道/幅值顺序、完整配置差异、摘要重算、禁用/缺失 CoBo、缺失芯片、范围/步长/起点/幅值/点数上限和非活动通道拒绝。配置 CLI 集成测试重新检查每个点的完整参数及摘要，并使用原生工件检查器验证工件；catalog测试包含208字节合成波形raw，验证AutoGain点绑定和波形响应统计，并在测试后清理夹具。Qt 离屏测试验证操作台真实生成多通道、多幅值计划并在无输出的情况下拒绝非法步长。精确平台和执行范围见[验证记录](VALIDATION.md)。

## 7. Pulser 幅值扫描的归档响应

已有`getdaq-config plan-pulser-scan`生成的计划，可以在每点独立Configure、Run、正常停止并封存catalog后，使用`getdaq-catalog --analyze-pulser-scan`按Run绑定分析波形。命令还要求操作员提供AsAd、AGET和通道，因为Pulser计划只指定CoBo和幅值，不包含通道选择。分析器会验证所选通道在每个点完整工件中处于活动状态，输出按点的波形响应统计；它不会根据峰值ADC和mV计算物理增益或作合格判断。详细参数和边界见[扫描点与Run归档关联](SCAN_RUN_BINDING.md#8-pulser-幅值扫描归档波形响应)。
