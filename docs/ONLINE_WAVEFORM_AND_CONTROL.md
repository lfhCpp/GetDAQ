# 控制警告、从控配对与在线波形

## Start / Stop / Disconnect

连接后这三个按钮保持可用。未结束的本地记录、失联监控、状态过期、未应用配置及打开的离线工具不再直接锁死操作：需要确认的情况弹出英文警告，由操作者决定是否继续。自动采集不能代替人工确认。

实际硬件命令仍串行执行：Stop 可以排到正在执行的命令后；不会因为点击多次并发重放命令。ECC 自身拒绝的操作仍然报告失败。已确认原始数据磁盘已满仍须先处理。

- Start：Ready 时正常开始；已 Running 时附着观察，不重复发送 Start。未知状态经确认可发送一次，由 ECC 判定。
- Stop：即使采集来自其他界面、或本地记录不完整，也能向连接的 ECC 请求停止。只有实际状态确认 Ready 才报告停止完成。
- Disconnect：经确认可以脱离未完成工作；不停止远端采集，也不撤销已经接受的命令。
- 本地记录写入失败、配置备份缺失、catalog 不确定：保留已有记录并警告，不声称归档成功。可手动开始下一次采集；有记录错误时不自动续跑。已有 Run 仍需覆盖确认，不删除原始数据。

磁盘上新的 XML/XCFG 和板卡已加载的参数是两个版本。同步不再要求 Idle；已知硬件转换期间暂缓写文件。通过本界面成功 Configure 才将候选快照关联到后续采集。重新连接已有 Ready / Running ECC 时不能由源文件推断板卡当前参数，备份会明确提示未验证。

## 从机配对

1. Slave 选择本机配置缓存、项目数据目录，点击 Enable / check slave。
2. 新邀请码出现后点 Copy pairing code，到主机 Add slave 粘贴。邀请码有有效期，一次使用。
3. 已配对时显示主控信息；不会显示已消费的邀请码。主控暂时不在线也保留配对关系。
4. 需要更换主控时点击 Renew / re-pair… 并确认。等待已接受的管理操作完成后，旧管理凭据失效，再复制新邀请码到主控。原有后台、数据和审计记录保留；该操作不会自动接管它们。

## 在线波形

通过 Tools → Online waveform viewer… 打开独立程序。关闭主界面不影响查看器，关闭查看器不影响 ECC 或落盘。也可直接运行：

```sh
./bin/getdaq-online-waveform-viewer --endpoint 192.168.50.16:46491
# 独立命令行仍可重复 --endpoint；GUI 使用当前配置的实例选择框。
```

从 **DataExporter** 下拉框选择当前配置中的接收器，或选择 All configured DataExporters。受管项目自动读取实际 TCP export 端口；界面不再要求填写地址。Refresh choices 刷新列表，Pause preview 仅暂停预览连接。外部 DataExporter 可在连接设置中选择 Receiver type / TCP preview export port，或在 Nodes / Paths 设置 Preview host / Preview TCP port；识别到运行中的 DataExporter 时也可使用默认导出端口。旧 DataRouter 只有在确实提供相同的长度前缀 TCP 导出协议时才能在线查看；仅接收落盘的 DataRouter 不因此自动具备导出功能。

1. 选择实例后自动连接预览，选 Source、CoBo、AsAd、AGET。
2. Channels… 勾选通道，复用离线查看器的横轴 Bucket 和纵轴 ADC、3D、PNG / CSV 导出。3D 纵深为实际通道号，通道曲线只描线、不填充脉冲下方区域，也不加遮挡后方通道的白色描边；全局背景为白色，不填未选通道；Ctrl+滚轮调间距，普通滚轮滚动长图，拖动调深度角，双击重置。
3. Follow latest 跟随最新缓存事件。取消后选择事件，或输入 Event ID 点 Go；暂停显示期间仍继续接收预览。
4. 缓存仅保留最近 100 个事件、4096 帧且原始帧不超过 256 MiB。冻结选择可额外保留至多一个缓存事件，最坏另占 256 MiB；解码与渲染另有少量工作内存。更老的事件用离线文件查看。
5. New session 清空预览会话。旧流没有 Run ID：重连、事件计数回退或同来源时间戳冲突会保守清缓存，乱序也可能触发清理。跨来源不承诺完整物理事件重建。没有 pad 映射，事件一直标为可能不完整的预览。

## Cache 与 Received total 的含义

- **Cache** 是此刻缓存的原始帧有效字节，显示为当前值 / 256 MiB。达到 100 个事件、4096 帧或 256 MiB 中任意一项，自动移除最旧事件并释放对应帧引用；不是永久保存每一帧。
- **Received total** 是本次查看器进程累计接收的字节计数。它会一直增加，但只占一个整数，不会把累计收到的数据全部保留。例如 Cache 5.3 MiB / Received total 234.4 MiB 表示累计收过 234.4 MiB，当前帧缓存约 5.3 MiB。
- Cache 不等于查看器全部内存：固定事件的冻结副本、解码波形、接收组包和 Qt 绘图有额外开销。取消 Follow latest 后可额外保留一个已选事件，即使它已从滚动缓存淘汰；切换事件、New session 或关闭查看器会释放相应旧引用。
- **New session** 清除预览帧及冻结选择，累计接收计数仍保留；输入流继续运行时会立即建立新的少量缓存。关闭查看器结束独立进程，其内存由系统回收，DataExporter 落盘继续。
- 分配器可能保留已释放空间用于复用，所以任务管理器里的进程内存不一定在清空缓存后立刻下降。它不应随 Received total 等比例持续增长；仅凭累计计数增加不能判断泄漏。

## 并发与落盘保护

接收、解码、绘图分离：查看器线程接收，后台一次解码一个选择，界面最多 5 Hz 更新。最多 32 个来源，单条 TCP 消息最多 64 MiB，所有待组装消息合计不超过 64 MiB；超过预算整条排空丢弃，不从帧中间恢复。复用已有 CoBo 帧解析与格式检查。

随项目构建的 DataExporter 只给导出旁路增加有界缓冲：全局待发送不超过 128 MiB / 256 条，每客户端不超过 64 MiB / 64 条，最多 4 个客户端。磁盘写入仍先执行；导出入口不等待队列锁，拥塞时丢弃尚未发送的预览，保留正在发送帧的长度前缀及完整帧体。关闭客户端也不会释放尚在异步发送的缓冲区。日志每秒汇总预览丢帧。

这些改动通过独立补丁应用，固定的第三方源码未修改。外部旧 DataExporter 不会自动获得这个保护，应使用随软件更新的版本。附加预览仍有 CPU / 内存复制开销，不承诺零性能成本；生产速率、真实网络和实板需现场验证。

测试命令：

```sh
QT_QPA_PLATFORM=offscreen ./build-tests/getdaq-tests
python3 tests/preview_backpressure.py ./bin --evidence /tmp/getdaq-preview-test
```

实现参考 [ATTPC attpc_conduit](https://github.com/ATTPC/attpc_conduit)：采用同类 DataExporter TCP 输入；本实现复用项目 Qt / MFM 解码，不引入 Rust、Python、Rerun 或 pad 映射依赖。
