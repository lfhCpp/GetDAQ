# AsAd 慢控制校准：原理、操作和日志

## 1. 这个步骤解决什么问题

CoBo 通过慢控制线路访问连接的 AsAd。线路和器件有时序延迟，校准的目的之一是让慢控制读写能够正确采样。旧 `CoBoNode::asadCal()` 会启动相应的固件校准，再读取监控芯片的厂商 ID，并写入和读回 AsAd 用户标签来检查访问是否正确。

本次把这个子流程迁移为无 Qt 的 `CoBoAsAdCalibrate(mask)`，交给 `HardwareClient::Execute` 执行。它不是整个 Prepare；上电、初始寄存器、时钟、AGET 校准、MuTaNT 对齐等需要相应的其他流程。它也不是保留全部校准结果的单板重新对齐入口。

当前 GUI 不会自动调用这个命令。工具没有运行状态联锁；在完整 ECC 后端接好以前，它是明确的维护入口，只适用于已经确认停止采集、完成上电和所需初始化的目标。不要让旧 ECC、其他寄存器工具和本命令同时操作同一块 CoBo。

## 2. mask 如何填写

mask 的低四位分别代表 AsAd 0、1、2、3。合法范围为 1～15；0 和范围外的值在连接板端前就拒绝。

| 启用的 AsAd | 二进制 | 十六进制 | 十进制 |
| --- | --- | --- | --- |
| 0 | 0001 | 0x1 | 1 |
| 1 | 0010 | 0x2 | 2 |
| 0 和 2 | 0101 | 0x5 | 5 |
| 1 和 3 | 1010 | 0xA | 10 |
| 全部四块 | 1111 | 0xF | 15 |

这个数字是 AsAd 位图，不是 CoBo 编号、AGET 位图或板卡数量。填写 2 表示 AsAd 1，不表示“前两块”。启用哪些 AsAd 必须与实际连线和准备配置一致，程序不通过探测来替你猜测。

## 3. 操作前先准备硬件描述

该命令强制要求 `--description`。这是之前离线编译出的硬件描述 JSON/YAML，包含设备、寄存器、位域布局和本地只读策略。详细编译步骤见 [硬件描述流程](HARDWARE_DESCRIPTIONS.md)。

例如已有描述文件 `config/cobo-hardware.json`，目标代理是 `node:tcp -h 192.168.0.2 -p 46001`：

```sh
# 离线校验文件格式，不连接板卡。
build-ice-compat/getdaq-hardware validate-description config/cobo-hardware.json

# 只查询设备元数据，确认绑定描述与远端布局一致。
build-ice-compat/getdaq-hardware --description config/cobo-hardware.json \
  describe 'node:tcp -h 192.168.0.2 -p 46001' ctrl
build-ice-compat/getdaq-hardware --description config/cobo-hardware.json \
  describe 'node:tcp -h 192.168.0.2 -p 46001' asad
```

示例地址需要改成实际板端地址。不要为执行校准重复创建非空硬件节点。使用旧板端时还需确认其实际 Ice 编码版本；默认 1.0，需要 1.1 时显式传 `--encoding 1.1`。

绑定只核对结构和本地权限。它不检查 AsAd 是否供电、当前是不是 Running，也不建立跨进程独占控制。描述必须与部署的板端版本相符。

## 4. 执行命令

先创建日志目录，然后指定一个尚不存在的日志文件。以下例子只校准 AsAd 0 和 2：

```sh
mkdir -p hardware-audit
build-ice-compat/getdaq-hardware --description config/cobo-hardware.json \
  asad-calibrate 'node:tcp -h 192.168.0.2 -p 46001' 0x5 \
  hardware-audit/asad-calibration-001.jsonl
```

成功时标准输出：

```json
{"phase":"complete","operation":"asad-calibrate","asad_mask":5,"full_prepare":false}
```

退出码 0 表示定义的校准子流程完成，1 表示执行/描述/元数据等错误，2 表示命令参数错误。不要把 `full_prepare:false` 当成整个系统已经准备完成。

若日志路径已经存在，程序拒绝执行并保留文件原文。即使换一个新文件名可以再次调用，也不意味着出错后应该立即重试；先阅读日志和检查硬件。

## 5. 实际执行顺序

以下顺序对照提供的 GET 20210208 `CoBoNode.cpp::asadCal`，以及 `selectAsAdForSlowControl`、`asadSetBoardUserID`、`asadGetIdentifier`。保留原始 CEA/CeCILL 源码及许可证。

| 顺序 | 操作 | 目的 |
| --- | --- | --- |
| 1 | `ctrl.ASAD_sc_mask.select = mask` | 为校准选择启用的 AsAd |
| 2～3 | `ctrl.asadController.reset = 1`、`0` | 复位慢控制相关标志 |
| 4 | `manualCal = 0` | 设置旧流程使用的手动延迟值 |
| 5 | `calibration = mask` | 对启用的 AsAd 开启自动校准 |
| 6 | 写整个 `ctrl.asad_sc_start = 1` | 启动慢控制操作 |
| 7 | `calibration = 0` | 结束该触发阶段 |
| 8 | 按 AsAd 0→3 顺序遍历，仅处理 mask 中的板卡 | 跳过未启用板卡 |
| 每块 a | 写 `ASAD_sc_mask.select = 1 << board`，再写 `asadSelect.d = board` | 选择写入目标和读取来源 |
| 每块 b | 读整个 `asad.monitorID`，要求等于 `0x41` | 检查监控芯片厂商 ID |
| 每块 c | 写整个 `asad.AIM0 = board + 1` | 设置用户标签 1～4 |
| 每块 d | 依次读取 `AIM0`、`AIM1`、`AIM2`、`AIM3` | 检查标签并保存板卡标识原始值 |
| 最后 | 恢复 `ASAD_sc_mask.select = mask` | 正常结束时恢复启用板卡的选择范围 |

AIM0 用户标签检查取低 8 位，符合旧代码把 AIM 读值转换为 `uint8_t` 的语义。AIM1～AIM3 没有固定的期望值，原始读取值记入日志。按每个值的低 8 位，可以重建四字节标识：AIM0 放在位 31～24，AIM1 在 23～16，AIM2 在 15～8，AIM3 在 7～0；用户标签以外的 24 位是板卡标识信息。

这里按真实旧函数的 AIM0→AIM3 读取顺序实现。不要根据某处排列说明或自己的猜测改变读取顺序。

新实现有一项明确收紧：旧 `asadGetIdentifier()` 会捕获读取异常并返回已拼接的部分值；新实现任何读取失败都会停止。标签不符时也立即停止，不继续访问剩余板卡。失败后不会自动恢复 mask、清空日志或回滚，因为额外写操作会掩盖部分执行状态。

## 6. 日志如何阅读

日志是 JSON Lines，每行一个 JSON 对象。第一行 `intent` 包含完整计划、目标代理、编码版本和开始时间；每一步先持久化 `begin`，才进行对应操作。写入收到应答记录 `acknowledged`，有期望的读取记录 `matched`，仅记录数值的读取记录 `observed`。所有步骤结束才有 `complete`。

| 最后可见记录 | 含义和处理 |
| --- | --- |
| `intent`，尚无 `begin` | 计划已保存；仍须核对文件是否完整及进程状态 |
| `begin`，没有对应完成记录 | 对应步骤可能已经执行，必须核对，不自动重发 |
| `mismatch` | monitorID 或 AIM0 标签不符；后续步骤停止 |
| `read_failed` | 读取出错；前面的写操作仍可能已生效 |
| `unknown` | 写入出错或响应丢失；该写入可能已经生效 |
| `observed` | 记录了实际读值，不代表它与预期校验过 |
| `complete` | 本子流程完成；不证明全部电子学准备、物理参数或采集链路完成 |

`step` 从 0 开始，可用它对应 intent 中的 steps 数组。数值以十进制字符串保存，避免 64 位整数经过普通 JSON 数字处理后丢失精度。对于启用 AsAd 0 和 2 的例子，共有 24 个步骤，包含 4 个期望检查和 6 个纯读取记录。

## 7. 本机验证与实机边界

```sh
cd build-ice-compat
ctest --output-on-failure
cd ..
python3 tests/calibration_integration.py \
  build-ice-compat/getdaq-hardware \
  build-ice-compat/hardware-client-tests
```

本机 TCP 测试使用原 GET Ice 接口，分别验证 1.0/1.1 编码、mask 1/5/10/15、用户标签、AIM 读取顺序、AIM0 低八位语义、失败立即停止和完整计划预检查。跨进程测试检查真实命令行、描述绑定、JSON 日志、非法 mask 和已存在日志拒绝。

当前尚未验证真实线路延迟、供电与固件的校准行为。完整准备流程仍需继续迁移，并在对应固件与实际板卡上验收。主机端无 Qt、使用原生 C++ 并不能证明校准结果或实际数据吞吐。
