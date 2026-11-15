# FDT受管理采集：重连、同一Run和未使用来源

本页说明新实现的FDT控制与接收接法。原生ECC与两数据服务/receiver的普通及新代码插桩联调均已通过本机验证，最终各项范围以[验证记录](VALIDATION.md)第62节为准。不要把本文当作真实板端、Narval、目标平台或持续吞吐验收结果。

## 1. FDT为什么需要单独处理

旧FDT发送器构造时保存接收地址，start时才建立数据TCP连接、发送心跳并开始读取，stop刷新后发送GOODBYE并关闭数据连接。普通TCP/ZBUF则在Connect时建立连接，Stop后仍可保持同一个socket。因此同样按Pause、Resume操作，FDT会产生多个物理连接，普通TCP通常只有一个。

新系统保留同一个板端sender对象、receiver PID、session及输出目录；FDT每次Start或Resume建立下一段连接，每次Stop或Pause结束当前段。在Breakup前，receiver继续等待下一段，文件编号和字节/帧/连接计数累积。这是同一Run的分段接收，不能把每次EOF当作整个Run结束。

旧行为依据是source/mordicus/MDaq-release-20210208/src/mdaq/daq/FdtDataSender.hpp和本地DaqCtrlNodeI。新Ice客户端仍只发送低频命令；原始MFM由板端直接到数据主机，Qt GUI、ECC及HTTP控制不转发事件。

## 2. 如何选协议

在固定ECC启动JSON的对应sender上设置真正字符串`"protocol": "FDT"`，在实际数据主机固定计划的对应source上设置`"input_protocol": "FDT"`。ID、接收地址及协议必须同时匹配，各自非默认字段进入规范计划、摘要及父ECC快照。只改其中一端会被拒绝；不自动探测或退回TCP。

示例字段片段（不是完整启动文件）：

```json
{"id": "C0", "protocol": "FDT", "receiver_address": "192.0.2.10:46005"}
```

```json
{"id": "C0", "input_protocol": "FDT", "receiver_address": "192.0.2.10:46005"}
```

地址是说明用途的占位地址，现场必须使用真实接收网卡地址及该机固定端口。其余proxy、配置/工件摘要、队列、目录、控制身份和认证设置仍按[远程接收控制](REMOTE_RECEIVER_CONTROL.md)及[ECC网络服务配置](ECC_NETWORK_SERVICE.md)填写。默认省略协议仍是TCP；区分大小写，不接受布尔、数字或小写fdt。

不需要手动运行有公开控制输入的receiver。DataSupervisor会为FDT创建匿名AF_UNIX socketpair，将接收端继承为该receiver的stdin，并启动durable、多连接模式。不能将这个stdin替换成终端、普通文件或公开FIFO；详见[接收周期原理](FDT_RECEIVER_CYCLES.md)。

## 3. 一次完整采集怎样走

| 用户操作 | 板端控制 | FDT数据连接 | 接收会话 |
| --- | --- | --- | --- |
| Configure进入Ready | 创建sender并设置缓冲策略 | 此时可以尚无连接 | receiver已运行、目录已锁定，准备接收 |
| Start | 先核对全系统前提，再daqStart | 建立连接并开始发送 | 同一个PID/session接第1段 |
| Pause | 先停生产，再daqStop/刷新 | GOODBYE后关闭、实际EOF | 同会话继续等待下一段，不发布Run完成 |
| Resume | 同一sender重新daqStart | 新连接 | 同会话接第2段，编号继续 |
| Stop | 停生产/刷新、关闭当前连接 | 当前段实际EOF | 回Ready，仍保留本周期 |
| Ready后再次Start | 同一sender重新启动 | 新连接 | 同会话再接下一段 |
| Breakup | 确认sender Disconnect，再结束接收会话 | 必须核对已接纳连接的真实EOF | 请求私有结束、排空、同步、退出、核对目录/计数/凭据 |

sender的Stop应答、GOODBYE、当前无数据、receiver仍运行和Finish请求各是不同证据。出现半帧、磁盘错误、异常退出或失联时，不自动Kill/restart、重发未知硬件动作或换ID重新Configure。

## 4. Ready之后从未Start就结束

FDT从未Start的来源可能没有数据TCP连接，所以不能声称已有发送器EOF。ECC保留的DataSenderSnapshot记录是否曾发出daqStart；在写入call_begin之后、发出调用之前保守记录startIssued。应答丢失、Stop、Disconnect都不会清掉这个历史；服务重开不能用新对象重新猜测。

Breakup仅在同一对象确认Disconnect后，为FDT且startIssued=false的来源列出未使用ID。已发出Start但未收到任何事件/连接的来源仍必须按正常路径核对，不能因计数0而冒充未使用。未知sender不会进入这个结束分支。

父ECC将ID按数据节点分组，写入持久Finish意图中的`unused_source_ids`，然后发送每个节点自己的请求。DataSupervisor先核对这些ID确实是本计划的FDT来源、当前从未接入连接/数据；receiver读取私有E时再次核对，包括正在等待接纳的连接。已有连接或数据时拒绝E并继续录盘，管理操作需保留失败/核对门槛。

正常F请求没有自动空会话例外。独立调用Finish而不列未使用ID、来源又从未连接时，仍会失败。只有明确的未使用声明加实际空来源核对才允许结束。

## 5. 未使用凭据应怎样理解

未使用来源生成目录结束凭据，供管理器核对进程退出、目录内容和后续Run元数据归属。它使用已有seal文件名，但包含明确字段：

```json
{
  "complete": true,
  "source_unused_confirmed": true,
  "sender_eof_confirmed": false,
  "connections": 0,
  "received_bytes": "0",
  "files": []
}
```

这是省略身份和持久性字段的解释片段，不是可手工提交的有效凭据。这里complete表示这个目录的结束凭据已经发布；它**不表示成功采到事件、不表示实际板端EOF，也不证明完整采集时长**。正常数据来源没有source_unused_confirmed=true，仍要求真实EOF与文件/计数一致。

receiver最终状态保持stream_sealed=false，另有source_unused_confirmed=true；管理状态保留该字段，GUI显示not used for acquisition。seal_verified表示管理器已经验证该来源的结束凭据，必须和未使用字段一起解释。

数据主机归档保留完整来源集合，未使用来源只有元数据和显式空清单，不伪造.graw或硬链接载荷。有数据的来源仍核对实际文件类型、设备/inode、大小和链接；拷贝后的seal和Run请求摘要仍绑定同一session/计划。全为空的周期可以结束管理对象，但不能作为真实采集或吞吐证据。

## 6. 控制失联和竞态

F/E之前父控制端消失时，receiver继续接收原始数据，标记控制失联，不把控制EOF当板端EOF，不正常封存。请求E时若已接入或等待接纳数据连接，receiver拒绝未使用声明，数据处理继续；随后没有合法F时不会自行结束。

F/E已经被接收器接受之后，局部结束凭据仍不能单独证明整个ECC操作完成。必须核对原父意图、实际操作结果、sender断开历史、数据节点身份、PID/session、目录和Run。重启服务不重新获取旧匿名控制端，不默认恢复UNKNOWN。

## 7. HTTP字段及资源边界

低频认证Finish请求可以在既有完整身份字段之外增加非空`unused_source_ids`数组，例如`["C0", "C1"]`。它仅允许Finish，ID必须按字符串顺序排列且不重复，最多195项，完整请求仍有8KiB上限；真正数组/字符串类型检查不宽容转换。列表进入规范请求字节和request_key，不能用同operation_id换一个列表重放。普通TCP默认请求不增加此字段，原规范字节保持。

该数组是调用者的明确声明，不是允许客户端指定命令、程序、目录或原始数据。真实GUI经父ECC自动构造，不应由操作员用空计数自行推断。若独立维护数据节点，应保留相同的板端停止/未Start及所有权证据，再经既有认证/身份/计划控制入口提出声明。

原始接收仍C++11/POSIX：固定容量池、有界队列、完整帧写盘与可选导出。新增控制只在低频通道传1字节，不经过Qt事件传递波形，也没有新运行依赖。共享/持久文件与实际系统库行为仍需目标平台测试。

## 8. 当前验证与复现

接收器私有控制专项现含明确未使用结束、已接入拒绝E，以及原F/A/失联/错误请求。数据管理专项增加全空和混合来源归档；HTTP专项增加全空/混合来源且由原生查询解码保留未使用字段。实际ECC联调包含FDT两种Ice编码的Pause/Resume、Stop/Start、两周期及Ready从未Start的结束，并复查默认TCP。 同机ECC/receiver普通及插桩另各两场景覆盖重连/两周期和Ready未Start结束；Qt控制模型普通/新代码插桩各17项核对未使用来源映射及无自动命令，普通完整GUI67项通过。

```bash
python3 tests/receiver_cycle_control_test.py RECEIVER EVIDENCE_CONTROL
python3 tests/run_receiver_observation_tests.py DATA_BUILD RECEIVER EVIDENCE_MANAGER
python3 tests/receiver_cycle_http_test.py DATA_BUILD RECEIVER EVIDENCE_HTTP
python3 tests/run_managed_fdt_tests.py NATIVE_BUILD RECEIVER DATA_SERVICE EVIDENCE_ECC
```

参数必须是对应同版程序和新的私有证据父目录。脚本只使用本机模拟控制与合成MFM，并在finally先结束自有测试进程，再删除本轮.graw，保留小型SHA/计数/日志；全部流合计最多20GiB，不能按来源或轮转文件拆分绕过。SIGKILL/断电不保证清理，真实Run文件不能按测试规则删除。

本机通过也不证明真实FDT固件、Narval分析接口、端到端无丢失、板端零复制、持续多CoBo吞吐、物理多主机、Ubuntu24、Qt5.14.2 MinGW或macOS已验收。最终通过项、程序/源码绑定及剩余范围见[验证记录](VALIDATION.md)。

已冻结包可用`python3 tests/archive_managed_fdt_validation.py --historical-only`核对历史字节；核对当前源码则去掉该参数。`--create`只用于创建尚不存在的包，不覆盖已保存的历史证据。
