# AGET 阈值离线扫描计划

`getdaq-config plan-threshold-scan`从完整系统实验工件生成一个指定 AGET 的完整配置扫描点。它只准备配置文件，不连接 ECC 或板卡，不写寄存器，不启动采集，也不处理事件数据。

## 命令

```text
getdaq-config plan-threshold-scan SYSTEM.json COBO_ID ASAD_ID AGET_ID START_CODE STOP_CODE STEP_CODE NEW_DIRECTORY
```

例如，若目标 AGET 当前全局阈值是0，可创建0、4、8三个阈值点：

```text
getdaq-config plan-threshold-scan system-experiment.json C0 0 0 0 8 4 threshold-plan
```

CoBo ID必须在系统工件中唯一且启用；AsAd和AGET ID均为0..3，并且必须是该板上已配置的活动 AGET。阈值码范围为0..127，采用升序闭区间，步长必须准确到达终点，最多128点。起始码的高3位必须等于目标芯片基准配置的`Reg1/GlobalThresholdValue`；低4位会作为扫描首点的通道细码，因此首点会把所选芯片所有68个通道模型的细码统一为该值。

## 阈值码含义

阈值码是旧 GetController `Threshold` 分支使用的7位组合：

```text
code = (global_threshold << 4) | channel_threshold
global_threshold = (code >> 4) & 0x7
channel_threshold = code & 0xf
```

计划只修改显式选中的一颗 AGET：全局部分写入`Reg1/GlobalThresholdValue`，细码写入该 AGET 的68个通道配置字段`threshold`。在现有 AGET 配置编译器中，非活动通道和 FPN 的细码不进入物理通道寄存器映像；计划也没有为它们定义新的硬件语义。其他 AGET、CoBo、MuTaNT 和配置来源保持不变。每一点重算完整配置摘要与工件摘要。

## 输出与审阅

```text
threshold-plan/
  scan-manifest.json
  point-000.json
  point-001.json
  ...
```

Manifest记录CoBo/AsAd/AGET、阈值区间、每点code和摘要，并声明`hardware_loaded_verified: false`、`requires_full_configure_per_point: true`及`acquisition_event_count_unspecified: true`。输出目录必须是新目录；发生写入失败时先在父目录的私有临时目录中构建，不覆盖已有计划。

逐点查看完整工件后，操作者仍需按正常流程完成独立的完整 Configure、Run 和封存，并可使用[扫描点与 Run 归档关联工具](SCAN_RUN_BINDING.md)绑定实际归档。生成计划本身不表示硬件读回，也不表示某一点的 Run 已经完成。

## 与旧 Threshold 测试的差异

旧`TestController.cpp`在运行时停启 DAQ、切换慢/快控制并直接写寄存器；它根据事件数累计`hitPat_0`，但紧邻处明确留有该字段解释待确认的注释。旧数据模型`CoBoEvent`把每个AGET的hit-pattern描述为72位容器，并注明实际68个通道；旧循环只累计前68位，而`ThresResult[128][72]`的后4列不会被循环写入，打印时仍会显示。因此源码可解释容器宽度与物理通道数差异，但不能据此确认hit-pattern位如何对应具体阈值响应、其他4位是否应被消费者忽略，或旧结果消费语义。该判断来自同版本静态源码，不是现场MFM样本或硬件验证。

新的离线计划仅迁移可表示的配置点，不移植旧自动采集循环、hit pattern 统计、空帧停止条件、结果文件格式或失败恢复。不能把生成的JSON计划当作已完成的 Threshold 自动测试。

## 验证边界

核心回归验证范围、目标ID、基准粗码、逐点完整设置、摘要与编码往返；CLI集成验证manifest、每个工件摘要、原生工件检查器和拒绝覆盖。验证使用合成配置、不生成DAQ raw、不访问硬件。现有完整Widgets窗口提供PeakingTime、Pulser和Threshold三种离线计划入口；Windows Qt5.14.2/MinGW与macOS仍需各自平台构建及运行验证。
