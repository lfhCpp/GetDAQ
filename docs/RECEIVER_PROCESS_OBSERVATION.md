# 接收进程即时观察：查询、原理与故障解释

## 1. 为什么需要另一种查询

`getdaq-supervisor-query status` 显示管理动作留下的生命周期缓存。例如，Start完成后是`ready`；接收器随后遇到磁盘写入错误并退出，缓存可能仍是`ready`，直到下一次核对动作发现故障。缓存适合快速查看操作进度，不能单凭它判断进程还在运行。

新增`receivers`查询本数据服务实际启动并持有的子进程。它只读取操作系统进程结束记录，输出小型JSON；原始MFM数据仍由板端直接发送给原生receiver，不通过Qt、管理器或CLI。

数据主机服务和原生CLI直接使用本接口。ECC和GUI可选聚合/显示另一路进程观察，开启方法见[操作台观察](RECEIVER_GUI_OBSERVATION.md)；ECC的`/v1/status`及GUI文件/字节/帧数仍是生命周期缓存。查询不是硬件健康或自动联锁，不会自动停止硬件或恢复采集。

## 2. 如何使用

沿用[远程管理手册](REMOTE_RECEIVER_CONTROL.md)建立服务配置和0600认证文件。将已有identity/status命令中的模式换为receivers，其他身份参数保持一致：

```bash
build-receiver-observation/getdaq-supervisor-query receivers \
  --host 127.0.0.1 --port 24000 \
  --credential-file /srv/getdaq/credentials/node-A.token \
  --expected-agent node-A --controller ecc-A \
  --expected-plan '<部署实际的64位十六进制计划SHA256>'
```

示例中的IP、端口、节点/控制器标识、文件路径和摘要必须换成实际配置。程序先查询identity，核对完整固定计划的规范摘要，再请求观察；不会向错误节点或已改变的服务实例提交操作。认证值只从本地文件读取，不放在命令行中。

对应网络接口为经相同认证保护的`GET /v1/receivers`，不允许请求正文。顶层恰有schema、service、agent_id、controller_id、instance_id、plan_sha256、observation。正常HTTP 200表示已返回观察结果，包括busy或unavailable；CLI退出0不代表采集成功。401表示缺少或错误认证，503可能表示认证文件权限/身份故障或服务不能回答。不要因此重发Start/Finish。

## 3. 输出怎样读

`observation`包含schema=1、busy、observed_at_unix_ms、lifecycle、receivers。lifecycle使用原状态格式；其中接收量/帧数/文件数仍是缓存，没有因进程查询而刷新。

| 字段/状态 | 含义 | 下一步 |
| --- | --- | --- |
| busy=true | 生命周期操作持有锁，此次没有查询子进程，receivers为空 | 等待当前操作进展，再只读查询；不能把空列表解释为所有接收器停止 |
| running | 查询时该PID仍是本服务拥有、尚未产生终止记录的子进程 | 只说明尚未退出；不证明线程未卡住、网络通畅、磁盘健康或硬件已配置 |
| exited，exit_code=0 | 子进程正常结束，但尚未由管理器回收/核对seal | 按正常停止流程执行或等待Finish；退出0本身不证明实际EOF和封存 |
| exited，exit_code非0 | 接收器错误退出，或因信号终止 | 保留Run、输出目录和stderr，核对故障原因；不要自行换操作ID重启 |
| exited，signal非0 | 操作系统记录因该信号结束，exit_code=128+signal | 例如SIGKILL通常为137；不说明是谁发送信号或是否发生OOM |
| reaped | 管理器已经回收该子进程，exit_code为回收结果 | 结合lifecycle里的seal_verified和整体状态；reaped不等于已正常归档 |
| unavailable | 子进程所有权或结束记录无法确认 | 保留未知；不会通过kill(pid,0)猜测，也不会接管可能复用的PID |
| not-started | 已有子项还没有有效PID | 不能当作运行进程或成功停止 |

每条子进程观察有id、pid、state、exit_code、signal、message。尚未知道退出码时为数字-1；message仅解释unavailable。观察列表与此次lifecycle缓存的接收项按顺序、id和pid绑定；CLI拒绝未知字段、错类型、错身份和不一致状态。

observed_at_unix_ms是服务端查询开始附近的Unix毫秒墙钟，使用十进制字符串，避免JSON浮点整数精度损失。主机时间可能校正，多个进程也按顺序读取；它不是跨主机同步时间、原子快照或事务序号。查询返回后状态仍可能立即变化。

## 4. 正常EOF和故障的区别

正常停止顺序是控制系统停止生产并断开发送器，使接收器看到实际TCP EOF，排空完整帧、同步文件并写seal；之后管理器Finish回收各子进程并验证身份、计数和文件集合。EOF发生至Finish核对之间，观察可能显示`exited/0`而生命周期仍`ready`。这是允许的中间状态，不能直接锁存为故障。

另一种情况是receiver遇到截断帧、ENOSPC或写盘错误后退出1：观察能在任何新命令之前发现`exited/1`，但不自动改变持久生命周期或控制权。底层故障原因在该次Start审计目录的source-N.stderr以及接收器最后状态输出中。数据主机管理日志与真实数据目录均应保留，由既有Finish/核对流程决定故障结果。

SIGTERM下没有连接过发送器的接收器可能退出0，却没有实际发送器EOF的seal；因此退出0也必须经过Finish。观察不解析或相信任意日志，不读取.graw，不伪造seal，不清除needs_reconciliation或硬件UNKNOWN。

## 5. 实现和性能边界

原生DataSupervisor::ObserveReceivers先对生命周期操作锁try_lock。锁忙立即返回busy及短缓存；锁可用时，在锁内对每个当前自有子进程调用waitid(P_PID, WEXITED | WNOHANG | WNOWAIT)。WNOHANG避免等待退出，WNOWAIT保留终止记录供Finish回收。最多三个EINTR尝试，其他错误返回unavailable；不进行网络探测、文件扫描、日志写入、信号发送或waitpid回收。

已经reaped的子项直接使用本对象记录，不再查询其PID。重新打开持久根的服务并不拥有旧子进程，列表可能为空且needs_reconciliation为true，不能解释为旧进程都已结束。最多195源沿用固定计划限制。低频Qt服务只编码这些小份元数据，原生核心/CLI没有增加第三方依赖。

GUI可选定时显示进程结果及失联/过期提示，详见[GUI手册](RECEIVER_GUI_OBSERVATION.md)；生命周期缓存没有因此变成实时数据计数。没有新增故障联锁；后续联锁必须明确判断采集阶段，避免正常EOF被误判，并处理真实控制权和硬件UNKNOWN，不能只依据PID状态自动Stop或重启。

## 6. 本机验证与未完成项

验证采用真实receiver子进程和本机合成TCP：运行中、正常EOF退出、截断帧退出1、SIGKILL、外部夹具抢先回收导致ECHILD、Finish忙碌、Finish后reaped、服务重开不接管旧PID。重复观察之后Finish仍可回收并核对seal，查询不新增持久操作记录。

源码入口为apps/datasupervisor/datasupervisor.*、controlprotocol.*、controljobs.*、controlhttp.*、service/supervisorservice.cpp和querymain.cpp；测试入口为tests/run_receiver_observation_tests.py。实际结果与构建范围见[验证清单](VALIDATION.md)。该数据主机专项每组线上字节上界8257536，远低于20GiB所有流合计上限，finally删除所有自己生成的.graw，只保留小型SHA/日志/状态。后续GUI专项的范围和凭据单独记录；真实硬件、Ubuntu24/Windows/macOS、长期吞吐、进程卡死判断、自动故障联锁和断电持久性仍需验收。
