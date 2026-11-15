# 源端读出线程：设备停止、join 与 ECC 接入

本页对应 `apps/sourcereader/sourceringreadout.h/.cpp` 的 SourceRingReadout。它把已经实现的 [SourceRingReader](SOURCE_RING_READOUT.md) 接到可复用的 C++11 读出线程和设备生命周期。该模块没有 Qt/Ice 依赖，不接收 GUI 的波形，不负责接收端存储。

板端应用仍需要提供真实 DMA/MMIO、生产计数、读指针、停止检查及中断等待；本库不声称已经替换 VxWorks 驱动。完整 ECC 联调用的是模拟设备写内存的线程和真实控制/接收进程。

## 1. 线程与设备职责

SourceRingReadout 拥有一个消费线程，线程从各 SourceRing 借用连续字节并调用同步发送回调。FPGA/DMA 或应用中的生产者属于 driver，不能由消费线程假装完成硬件动作。

| driver 回调 | 必须完成的职责 |
| --- | --- |
| verifyIdle | 确认实际设备生产已停止，生产线程/设备状态满足可检查前提；不能只是查询读出线程标志 |
| start | 启动本次设备生产；设备必须有背压，在消费线程启动前也不能覆盖未读内存 |
| stop | 停止实际设备生产并确认；应用有生产线程时等待它退出，不能等待消费线程已经退出才释放生产者 |
| wait(maximum) | 只读等待中断/条件，必须在 maximum 内返回，即使没有事件；不得偷偷复位设备或执行控制动作 |

回调必须全部存在。构造还要求非零 chunkBytes、batchBytes 和 workBytes。原生产总数必须在多次 Start/Stop 中连续，不因 RunInit 或设备读指针复位就将软件观察计数清零；真实驱动要按实际硬件计数周期提供一致适配，旧地址寄存器的限制见环形手册。

## 2. 正常启停次序

Start 先核对消费线程已 join、driver 空闲、已启用缓冲没有旧尾部，记录启动尝试，然后启动设备和消费线程。设备 start 异常或创建消费线程失败都保持 UNKNOWN，不自动再启动或发送 End；显式失败停止仍可对本次设备执行一次 stop。

消费线程以 workBytes 为每次读出预算，继续轮流处理各缓冲。无可发字节时调用 driver.wait(50ms)，避免空闲忙循环；有数据时继续有界批次。wait 抛异常也停止消费并锁存 UNKNOWN。

StopAndJoin 先调用 driver.stop，再取消并 join 消费线程，最后检查实际设备空闲。消费线程在设备 stop 等待期间仍可以发送，使生产者可以退出、背压可以释放。正常停止后 Drain 强制发送尾部，仍不发送应用 End。

最终源端 closeIceStream/Disconnect 通过 ControlledIceSource 执行停止检查、Drain、再次核对静止，然后才从源原数据连接声明结束。Pause/Stop 后可以在同一源会话 Start，SourceRingReadout 的累计缓冲位置也保留。

## 3. 设备停止应答丢失

每次 Start 的设备 stop 在调用前标记 stopIssued，只允许一次尝试。若 stop 已写入设备但应答丢失，本库仍取消/join 本地消费线程，保留 UNKNOWN 和 deviceStopAcknowledged=false。后续 StopAndJoin 不再次发送同一个设备 stop，Drain/Start/Configure 也拒绝。

这种情况下，join 只证明消费线程已退出，不能证明 FPGA 真正停止；不能因此继续排空或补 End。真实设备恢复需要独立受控观察和恢复程序。保留原硬件/源对象、位置、接收会话与审计记录，不能新建对象解除历史门槛。

发送、读指针、wait 或 driver 回调错误通过 fault 回调向同一已连接的 SourceControlServant.ReportFailure/ControlledIceSource.ReportFailure 锁存。fault 只能记录/报告，不递归调用 readout 的控制/Status 方法，也不能从消费线程 join 自身。控制线程再决定显式失败停止。

## 4. 排空为什么有上界

Drain 要求设备和消费线程已静止，启用缓冲最多存有全部 ring.capacity 的总和。排空每次都使用 force，并限制本轮总发送不超过这一静态容量。如果所谓空闲设备仍不断增加写总数，排空在容量上界后拒绝，不无限发送或假装排空成功。

这个预算防止错误 driver 的无限 drain，不能证明其 idle 回调说的是真话。真实静止、CPU/DMA可见性和不覆盖前提仍必须由设备实现及实机测试证明。存在禁用缓冲尾部时也不能通过改变掩码丢弃，规则见环形手册。

## 5. 和源控制服务连接

```cpp
GetDAQ::SourceReadoutDriver driver;
driver.verifyIdle = [&] { board.VerifyProductionStopped(); };
driver.start = [&] { board.StartProduction(); };
driver.stop = [&] { board.StopProductionAndConfirm(); };
driver.wait = [&](std::chrono::milliseconds maximum) {
    board.WaitReadoutInterrupt(maximum);
};

GetDAQ::SourceRingReadout readout(
    rings, driver,
    [&](const std::uint8_t *data, std::size_t count) { source->Feed(data, count); },
    [&](const std::string &error) { source->ReportFailure(error); },
    256 * 1024, 256 * 1024, 4 * 1024 * 1024);

GetDAQ::IceProducerCallbacks callbacks;
callbacks.verifyIdle = [&] { readout.VerifyIdle(); };
callbacks.start = [&] { readout.Start(); };
callbacks.stop = [&] { readout.StopAndJoin(); };
callbacks.drain = [&] { readout.Drain(); };

source = new GetDAQ::SourceControlServant(options, callbacks,
    [&](unsigned int mask, bool flush) { readout.Configure(mask, flush); });
```

board 和 rings 是待实现/核对的真实设备适配，source 是应用持有的 Ice::Handle，options 等固定目标/实例/会话约束沿用 [源控制手册](ICE_SOURCE_CONTROL.md)。先构造对象并绑定 callbacks，再暴露 adapter，不能在 source 尚未连接时让设备开始发送。

必须管理对象寿命：运行期间 source、readout、driver 和借用内存都有效；关闭应用先停止生产和 join，再撤下服务，最后销毁对象。SourceRingReadout 析构只取消/join 自己的消费线程，不执行设备 stop、Drain 或 End，不代表设备正常关闭。没有遵守 wait 有界回调合同的驱动也可能让 join 等待；本库不通过强杀线程伪造成功。

## 6. 状态怎样解释

SourceReadoutStatus 包含 workerRunning、startIssued、stopIssued、deviceStopAcknowledged、unknown/error 和缓冲位置。workerRunning=false 可能是线程尚未调度或因故障退出，也可能是已停止；确认已 join 必须使用 VerifyIdle，不能只看布尔状态。

Status 是本地观察 API，缓冲观察锁可能等待正在执行的发送调用。它不取得硬件所有权，不证明接收端文件已经同步，也不解除 UNKNOWN。读出类的操作锁只串行化控制方法；driver/发送/fault 回调不可以重入相同 readout。

## 7. 本机联调范围

独立核心测试覆盖三周期回绕、正常/空停止、启动应答丢失、停止已生效但应答丢失且不重发、wait 非标准异常、运行期间策略拒绝、有界 drain 和析构只取消本地线程。均不生成 raw 文件。

完整 ECC 夹具改为生产线程为实际掩码0/3各写一个完整MFM帧，确认一批完整帧就绪后启动消费；这是确定性的合成中断批次，不是持续生产吞吐测试。生产线程写环形内存，实际 SourceRingReadout 消费线程读出，SourceControlServant 连接固定 receiverSession；普通/新代码插桩的本机和远程流程核对两编码、暂停恢复/多Run、空源、原字节和独立结束凭据。测试内存不参与真实板端寄存器命令，生产计数跨周期保留。

所有测试原始输入/输出总计仍受每轮20GiB上限；此专项只用MiB级预算，并清理生成raw。实际程序、编译配方、范围和最终结果见 [验证记录](VALIDATION.md)。旧vendor Ice/GET/bzip和系统Qt未整体插桩，不能把本机联调当目标平台、真实设备背压/中断或生产吞吐验收。

SourceRingReadout现于Start前和Drain后调用reader.VerifyDrained，检查可选原始尾部回调。SourceMfmRing识别任意写前缀而不复制载荷，停止时半帧保持UNKNOWN；完整接法见[源MFM手册](SOURCE_MFM_BOUNDARIES.md)。
