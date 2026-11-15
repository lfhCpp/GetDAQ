# ICE 源端控制与 ECC 会话绑定

本页解释 `SourceControl.ice`、源进程中的 `SourceControlServant`、控制端的 `DataSenderClient`，以及接收器会话怎样传到源端。原始字节仍走 `IceSourceClient → getdaq-ice-receiver`，不经过 ECC 或 Qt。生产线程/停止/尾缓冲的基础说明见 [源端生命周期](CONTROLLED_ICE_SOURCE.md)。

## 控制接口与数据接口分开

源端控制扩展 `GetDAQ::DataSourceControl` 继承原 `mdaq::DaqCtrlNode`。它保留 Start/Stop 的低频命令，增加只读 describeSource，以及明确绑定会话的 connectIceStream、合并策略 configureSourcePolicy、最终 closeIceStream 和一次失败停止 stopIceSourceAfterFailure。这个接口没有 ByteSeq、波形或 MFM 帧参数。

源端程序把 SourceControlServant 嵌入自己的 Ice 服务，注册在 DaqCtrlNode 对象身份。真正的读出线程和策略由应用回调提供；它不是新板端驱动，也不让控制主机代替源进程处理尾缓冲。接收端的数据接口仍是 DataProcessor/DataIngress，使用另一条数据连接。

普通 TCP/ZBUF/FDT 发送器沿用旧 DaqCtrlNode。配置的 ICE 发送器必须实际实现新增能力，只有字符串 `protocol: ICE` 不够；真正旧接口服务会在第一条改变状态的命令前被拒绝。

## 源端实例和接收会话各是什么

| 标识 | 产生位置 | 用途 |
| --- | --- | --- |
| sourceId | 源端部署配置 | 与固定发送器来源 ID 核对 |
| instanceId | 每个源控制适配器构造时产生 | 区分被替换或重开的源对象，不能将新对象自动当成旧源 |
| receiverAddress | 源端固定目标配置 | 与 ECC/数据主机计划中的 IPv4:port 核对 |
| receiverSession | 数据主机 Start 创建 | 绑定本次实际接收进程/目录，会话不从当前端口随意猜测 |

这些是身份标签和版本边界，不是密码、板端认证证书或全局硬件锁。真实所有权和实际加载版本的验收仍独立进行。实例重开不会解除 ECC 原持久 UNKNOWN。

ECC Configure 先完成已有硬件配置，再启动并核对接收器。只有获得完整来源集合中 ICE 来源的已验证会话，才创建采集对象并绑定发送器。缺来源、重复 ID、非法会话、未知来源或重复绑定都拒绝，整个集合先检查再绑定。普通来源不接受 ICE 会话项。

本地 ReceiverProcessManager 核对自有进程的 Ready/PID/来源/会话；远端 RemoteReceiverManager 经认证数据服务核对原计划和原会话，然后返回同一接收周期的 ICE 来源会话。它们不读取或传递 MFM 字节。

## 固定控制连接怎样防止自动接管

控制端先通过真实 TCP 核对原 DaqCtrlNode 和新 DataSourceControl，读取源能力/实例/目标，随后用实际 Connection.createProxy 固定控制代理。源端数据库独立固定自己的数据连接。

后续策略、Start、Stop、最终结束和状态核对继续使用原控制连接。源进程绑定发出合法 connectIceStream 的控制连接，另一连接只能做只读发现，不能替原控制器启动或关闭流。控制重试关闭；连接失去后不自动连到同端口新服务。

即使对象在同一服务/同一连接的原身份下被替换，describeSource 中的 instanceId 改变也会被拒绝，已连接的控制对象进入 UNKNOWN。只保持 IP/端口或对象名字相同不够。

源端每次正常结束后可以接收新接收会话；之前会话 ID 禁止重用，单实例最多保留 256 个已尝试会话。达到上限时拒绝新的连接，需要受控部署/历史核对，不能清空集合继续。历史只在当前适配器实例中保留，不声称源进程重开有持久恢复能力。

## 如何配置源程序

源端应用配置 `SourceControlOptions.sourceId` 和 receiver 的固定地址/端口、编码、超时与块大小。expectedSession 必须留空，由合法控制连接带入本次实际会话。应用还要提供四个生产回调和一个策略回调。

```cpp
GetDAQ::SourceControlOptions options;
options.sourceId = "C0";
options.receiver.address = "192.0.2.10";
options.receiver.port = 46000;
options.receiver.encodingMinor = 0;

IceUtil::Handle<GetDAQ::SourceControlServant> source =
    new GetDAQ::SourceControlServant(options, producerCallbacks,
        [&] (unsigned int mask, bool alwaysFlush) {
            readout.ApplyBufferPolicy(mask, alwaysFlush);
        });
adapter->add(source, communicator->stringToIdentity("DaqCtrlNode"));
// 源应用的实际生产/排空线程将字节交给 source->Feed(data, bytes)。
```

这里的 readout、producerCallbacks、adapter 和 communicator 是源应用已有对象。回调的空闲、停止/join、排空、设备与缓冲选择必须由真实实现完成，不能用空函数冒充板端工作。

控制主机固定发送器配置沿用原字段，加 `protocol: ICE`，控制代理指向这个升级源对象。数据主机固定计划的同一来源也设置 input_protocol: ICE，并绑定独立 ICE 接收程序及 SHA，见 [管理 ICE](MANAGED_ICE.md)。来源 ID、目标地址、编码和活动缓冲掩码仍要与整体工件/准备对象一致。

纯数据/源程序构建不引入 Qt；新增控制扩展需要原 DataProcessor、DaqControl、CmdException 三份 GET 接口。控制主机仅链接源控制协议；源端适配器才链接原始发送库。旧完整兼容构建仍可包含 Qt 控制服务，不能把这种构建与纯数据模式混称。

## 正常流程序列

1. 数据主机 Start 创建会话并核对 receiver。
2. 控制端读取源能力、固定实际控制连接，把实际 receiverSession 和 source/instance/目标传给 connectIceStream。
3. 源端核对固定目标/自身实例，实际源数据库核对接收会话并固定数据连接。此时只读握手不等于提交原始数据。
4. configureSourcePolicy 在源端空闲时一次设置缓冲掩码及刷新策略。回调失败保持 UNKNOWN，不能把部分策略写入当成功。
5. 原 Start/Stop 由源端生命周期模块执行。Stop join/排空但不发 End，Pause/Resume 或同一 Run 的多次 Start/Stop 保持原会话。
6. 最终 closeIceStream 必须携带原会话并来自原控制连接。源端再次静止/排空，从自己原数据连接发送 finishStream。
7. 控制端核对源端返回的实例/会话/Stopped/Drained/End 状态。数据主机仍单独等待进程退出、目录锁释放及专属文件凭据。

升级源不使用旧 idempotent disconnect 完成新 ICE 流，也不让旧 connect 在没有接收会话时默认连入。遗留调用会得到明确拒绝。processData 单向返回仍只是本地 handoff；源结束和控制应答都不是全链零丢失证明。

## 控制 UNKNOWN 和数据凭据独立

closeIceStream 可能已让源端发送结束，但控制应答丢失。接收器此时可以有有效独立 ICE 凭据，控制端仍必须保持 UNKNOWN，不能据此提交完整 ECC Prepared 或自动重发最终断开。

ICE 控制对象在 UNKNOWN 中可明确调用一次 StopIceProductionAfterFailure。意图先记录，随后源端将该流保持为未知，只停止/join生产并检查空闲，不排空或补 End。成功停止也不清除原 UNKNOWN；同一次尝试失败不重放。若原固定控制连接或实例已经丢失，不能用新连接偷偷接管执行该动作，真实硬件停止/恢复仍要独立受控核对。

源应用的ControlledIceSource或已连接的SourceControlServant发生策略或设备故障可从本地线程调用 ReportFailure 保持未知。环形读出和读指针发布语义见[读出手册](SOURCE_RING_READOUT.md)。状态提交和Start尝试标记在状态锁内核对，异步故障不会被较晚的正常提交覆盖；若故障发生在实际End等待期间，End应答仍不能清除源UNKNOWN。迟到数据、回调异常与原数据连接丢失也会保留源端 UNKNOWN。源端控制适配器和生命周期模块析构不隐式 Stop/End；应用须先停止/join生产线程并保持回调对象的有效生命周期。

## 本机验证与现场差距

验证包含实际源控制服务、固定控制/数据连接、真实源生产线程、两种 Ice 编码、合成 MFM 原字节，以及实际 ECC 进程和两个认证数据服务。旧接口/另一控制连接/错误会话/实例替换/策略错误/结束应答丢失均有独立用例。

测试生成数据按本轮私有根清理，全部输入/输出流合计上限 20 GiB，专项仅用 MiB 级预算。旧 Ice/GET 和系统 Qt 未整体插桩，真实板端 CircularBuffer、GPIO/中断、VxWorks 构建、硬件控制权/加载版本、目标平台、长期吞吐和断电仍不由本机夹具证明。最终结果见 [验证记录](VALIDATION.md)，整套重构继续进行。

设备回调可以组合SourceRingReadout：生产侧保留自己的累积位置，Source服务每接收会话保留独立发送计数；多缓冲只在完整消息边界切换，不能逐片段交错波形。实际本机/远程ECC接续及适用边界见[读出生命周期](SOURCE_READOUT_LIFECYCLE.md)。
