# 源端 MFM 边界检查：任意写前缀、半帧和停止

本页解释 `apps/sourcereader/sourcemfmring.h/.cpp` 的 SourceMfmRing，以及源端/接收端共用的 `apps/datareceiver/mfmheader.h`。该适配器从源端环形内存识别完整 MFM 帧，不再要求设备每次更新写指针都恰好停在帧末尾。它仍需要真实驱动提供 CPU 可见、不会覆盖未读内存的累计写位置。

SourceMfmRing 是标准 C++11 模块，无 Qt/Ice/yaml/Boost/curl 依赖；只在选择 ICE 时把已经检查的缓冲交给原有源发送库。它不创建 GUI，不展开逐通道波形，不把原始事件送进 ECC。

## 1. 为什么只有“完整帧标志”还不够

上一阶段的多缓冲读出只在完整消息批次末尾切换来源，避免把缓冲 A 的半帧和缓冲 B 的半帧交错。但它要求调用方提供的 writeTotal 恰好指向完整帧边界。真实设备可能先写头、再写数据，并在中间更新可见写位置；直接把这种位置声明为完整边界会破坏多来源流。

SourceMfmRing 在原始写位置之前另保留一个经过检查的完整帧位置。举例：原始写位置 4300，第一帧 4096 字节，第二帧只写了 204 字节；适配视图只提供 4096。消费线程先发送第一帧，第二帧等设备补全后再成为可发送范围。

```mermaid
flowchart LR
    Raw[设备 CPU可见原始写位置] --> Scan[少量头检查 / 环形跨尾读取]
    Scan --> Complete[完整 MFM 前缀位置]
    Complete --> Reader[SourceRingReader 分块发送]
    Reader --> Publish[成功返回后发布原设备读指针]
    Stop[设备停止 + 消费线程join] --> Tail[原始位置与完整位置再核对]
    Tail --> End[无尾部才允许源生命周期结束]
```

## 2. 三个位置与扫描计数

| 状态字段 | 含义 |
| --- | --- |
| rawWriteTotal | 最近观察到的设备原始累计写位置，可能在半帧处 |
| completeWriteTotal | 已验证的完整 MFM 前缀末尾 |
| publishedReadTotal | 设备读指针发布回调明确返回的位置，允许处于正在发送帧内部 |
| frames | 当前适配对象检查过的完整帧数，不是板端触发总数或接收端持久事件总数 |
| unknown/error | 无法继续正常读出；原对象没有 Reset/重试入口 |

初始化位置必须是已确认的帧起点；适配器从那里向后扫描，不在任意内存中猜帧头或跳过坏字节。正常多次 Start/Stop 及完整结束后的新接收会话可以保留设备累计位置；接收会话的发送总数由源生命周期单独计算。

原始 writeTotal、完整位置及发布位置必须处于同一设备计数周期。原始计数倒退/复位或原始未读量超过环形容量会保持 UNKNOWN，即使目前一个完整帧也没有发现。不能只检查完整帧位置而忽略正在写入的半帧已经覆盖缓冲。

## 3. 解析做什么，不做什么

每帧先读取八字节主头，检查声明长度与配置上限；再复制最多 28 字节头前缀，复用接收端相同的大小、字节序、standard header/item 范围、CoBo 来源/事件/AsAd 及 MuTaNT 事件规则。大端、小端、旧 topology blob 等保持已有规则。

头已经完整但字段非法时，立即失败，不等整帧载荷到齐。头合法而剩余载荷尚未写完时，保持完整位置不动，等待后续观察。只有该帧所有声明字节已 CPU 可见，才把完整位置推进到下一帧起点。

这只是结构与边界检查，不能证明波形有效、CRC 正确、硬件没有漏触发或设备身份与头字段一致。未知类型继续遵循现有接收器规则，不猜内部载荷。FDT 心跳/GOODBYE 是发送器在帧间加入的传输控制字，不是板内 MFM；此适配器不对含 FDT 控制字的内存自动探测。

共享 InspectMfmHeader 接受“头前缀字节数”和“整帧声明长度”两个参数，不能把 28 字节头副本误当成 28 字节完整帧。MfmStream::Inspect 沿用原调用接口，完整帧路径也使用共享检查，不维护两份会慢慢分叉的解析规则。

## 4. 拷贝、内存和扫描预算

跨环形末尾的头通过固定 28 字节数组读取；载荷保持原来的借用内存，不分配整帧副本。SourceRingReader 再按连续区间和发送片段预算借用原始字节。Ice/内核/接收写盘仍可能有复制，所以不能称整链零复制。

构造要求最大帧至少八字节、不超过环形容量且不超过 1 GiB。如果单帧比整个环形缓冲还大，就无法在不释放未检查字节的条件下等它完整，必须修正配置/缓冲容量；不能设一个更大的数字假装可以接收。

每次原始位置观察最多检查 scanFrames 个新帧，默认 1024，合法范围 1～65536。达到预算后返回当前完整前缀，下次接续。读出批次和发送预算仍由 SourceRingReader/SourceRingReadout 控制，不能用不受限全缓冲解析堵住控制线程。

## 5. 半帧停止为什么必须失败

“可发送完整帧集合为空”和“设备没有未读字节”不是同一个状态。只剩半个帧头时，reader.Empty 可以为 true；若据此发送 End，接收端会看到合法空尾，从而掩盖源端残留。

新增 SourceRing.verifyDrained 回调由适配器提供，SourceRingReader.VerifyDrained 会检查已启用缓冲，SourceRingReadout 在 Start 前和最终 Drain 后调用。停止时 rawWriteTotal、completeWriteTotal 和 publishedReadTotal 必须相同；未检查/半帧字节或未发布的完整数据均拒绝正常结束。

改变掩码前也检查所有缓冲的原始尾部，不能把有半帧的来源禁用后当成结束。只改变刷新策略仍必须遵守设备静止/源生命周期前提。

半帧失败时，先前完整帧可以已经存储，源端保留 UNKNOWN，不跳过尾部、不补字节、不补 End。显式失败停止只停止生产/join/checkIdle，不解除未知。接收端可能需要故障退出才能关闭写盘缓冲；测试中的清理信号不成为生产结束或成功 seal。

## 6. C++11 接法

原始视图可以报告任意已写前缀，不需要手工声明完整帧标志：

```cpp
GetDAQ::SourceRing raw;
raw.id = 0;
raw.memory = driver.CpuMappedBuffer();
raw.capacity = driver.BufferCapacity();
raw.initialReadTotal = driver.ConfirmedFrameStartReadTotal();
raw.writeTotal = [&] { return driver.CpuVisibleRawWriteTotal(); };
raw.publishReadTotal = [&](std::uint64_t total) {
    driver.PublishAndConfirmReadTotal(total);
};

GetDAQ::SourceMfmRing checked(raw, reviewedMaximumFrameBytes, 1024);
GetDAQ::SourceRing view = checked.FramedView();
// 将 view 放入 SourceRingReadout 的 rings；多个 AsAd 各持有自己的 checked。
```

driver 方法是板端适配职责示意，仓库没有声称已提供该真实设备对象。真实 DMA 的缓存失效/内存屏障、背压、地址寄存器转累计位置、实际停止和中断等待仍由 driver 实现并实机验证。

checked 必须在 view、reader/readout 及其线程的整个使用期间有效。FramedView 借用原内存并捕获 checked；不能只保留 view 后销毁 checked。设备/reader 回调不能递归调用同一 checked；发布回调异常也保持 UNKNOWN，不重复发布。

正常应用先停止设备、join 消费线程，再 Drain/VerifyDrained，最后由源生命周期发布 End。操作与线程锁的组合见 [读出生命周期](SOURCE_READOUT_LIFECYCLE.md)，固定源实例/会话见 [源控制](ICE_SOURCE_CONTROL.md)。

## 7. 本机验证范围

源码测试包含逐字节到达、大端/小端、头/载荷跨环形尾、另一来源先完整时可先发送但不交错、原始半头/半体、坏头提前失败、超长声明、原始越界/复位、读指针 ACK 丢失、扫描预算、旧 topology blob 和 Start 前旧半帧拒绝。

共享头解析还从第69节不可变归档提取旧函数，在 20,006 组有效/非法头与字段上对照；比较仅覆盖收到的旧函数及本机域，不表示运行过整个旧 GET/Narval 库。实际源→ICE receiver 六场景包含两编码正常多周期、空流、发布读指针不确定和停止于半头/半体；只发送此前完整帧，未完成流没有 End/成功 seal。

普通和新代码 ASan/UBSan/LeakSanitizer、默认 TCP/FDT/ZBUF 接收回归与完整 ECC 的最终结果见 [验证记录](VALIDATION.md)。旧 vendor 库/系统Qt未整体插桩。本轮全部输入/输出总量小于20GiB，专项仅MiB级，结束删除合成raw；捕获中断也清理，SIGKILL/断电不保证 finally。真实设备、目标平台、生产吞吐与端到端完整性仍须继续验收。
