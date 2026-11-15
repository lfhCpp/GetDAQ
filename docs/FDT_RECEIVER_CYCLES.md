# FDT多连接接收会话：私有结束控制与封存边界

## 1. 当前完成范围

FDT板端stop会断开TCP连接。为了把暂停/恢复的数据保留在同一个Run中，数据主机接收器现在可以在同一个PID、session和目录内接收多次真实EOF，直到管理器明确请求结束会话。此页对应原生receiver和数据主机DataSupervisor的基础机制。

整系统ECC的FDT启停/重连及同会话接法已接续，详见[受管理FDT](MANAGED_FDT.md)。普通F请求仍要求至少一次真实发送器EOF；新增明确未使用来源E例外保留EOF=false和零连接/数据，不把空来源当完成采集。此页说明receiver/DataSupervisor机制；真实固件/Narval和目标平台仍待验。

原始数据不通过Qt或Ice；私有控制仅传一个字节，接收、MFM分帧、控制字过滤和写盘都在原生进程。原TCP/ZBUF默认路径不创建新控制通道。

## 2. 为什么EOF后不立即退出

独立FDT --once模式仍在第一连接EOF后退出。新受管理FDT周期则在每连接EOF后核对帧尾、同步当段文件、重置FDT控制字状态，并保留监听器、会话、文件编号和累计计数。下次连接接着写该会话，不覆盖前一段文件。

心跳和GOODBYE只在帧边界过滤。GOODBYE不代替真实EOF，波形载荷中的相同四字节保留；非法头、截断帧、GOODBYE后字节仍失败。多段也不证明板端事件完整、连接断开正常或实际无丢失，现场需端到端计数。

## 3. 私有控制通道

DataSupervisor在启动每个FDT receiver前创建匿名AF_UNIX socketpair。两端非阻塞并设置close-on-exec；posix_spawn只把接收端复制为该子进程stdin。父进程保留另一端，其他receiver或exec进程不继承此写端。

它不是对外HTTP接口、文件路径或数据端口，没有额外认证文件或公开密钥。只有实际持有父端文件描述符的管理器能请求结束；网络上的sender、GUI和ECC客户端不能向原始数据流插入这条控制命令。Linux发送使用MSG_NOSIGNAL，支持的平台使用SO_NOSIGPIPE，断开的子进程不会通过SIGPIPE终止管理服务。

私有命令如下：

| 一个字节 | 意图 | receiver实际行为 |
| --- | --- | --- |
| F | 结束接收会话 | 有活动连接时继续接收，等待其真实EOF；没有活动连接时要求此前至少一次真实EOF；随后停止接纳下一段并排空/同步/退出/seal |
| E | 明确结束从未使用的FDT来源 | 仅零连接/数据且无待接纳连接时确认，退出0、目录结束凭据明确source_unused_confirmed=true及EOF=false；已有连接则拒绝并继续录盘 |
| A | 明确中止未接入数据的失败启动 | 仅connections=0、received=0且未连接时确认中止，错误退出，不生成正常seal；已有任何连接则拒绝中止，继续录盘 |

其他字节为控制协议错误，保留已确认帧并错误退出。命令不包含事件内容或可执行文件、输出目录修改。不是硬件Stop，也不向发送器制造EOF。

## 4. Finish仍不能代替发送器断开

管理器先保存每个子进程的cycle-finish-intent，再向所有FDT子进程发送F，然后等待退出。先提交所有请求，避免等待第一源时其他源根本未收到请求。每个接收器仍必须看到实际EOF；活动连接不关闭时，Finish超时而不kill或重启。

空会话收到F时退出1，错误为要求actual sender EOF，不产生成功seal。已有半帧、写盘错误或控制失效时同样不能提交正常周期。请求和退出0也不是完成依据，管理器仍核对PID/session/配置、输出目录锁、排空、最终计数、seal、文件集合/大小及后续归档。

正常数据seal保持原格式，但FDT允许connections大于1且至少1；明确未使用来源在凭据中另有source_unused_confirmed=true、connections=0、EOF=false及空文件清单，不是正常数据seal。TCP/ZBUF仍严格一连接。归档按固定计划对应源的input_protocol解释这个差别，不放宽符号链接、文件身份或原始计数核对。

## 5. 管理器退出后会怎样

父进程在发送F之前消失，会关闭其控制描述符；receiver把控制EOF记录为cycle_control_connected=false，继续接受/保存原始数据，不把控制器退出当发送器EOF，也不自动停板或退出。

失去控制后不能正常seal。即使之后各数据连接自然EOF，接收器仍保留监听和录盘；人工信号结束也会错误退出，明确缺少私有Finish请求。重开数据服务不接管旧PID或旧控制描述符，保持既有核对门槛。现场恢复和跨进程接管尚未实现，不通过重新创建目录或换ID解除UNKNOWN。

若F已由原管理器发出，接收器可以在最后真实EOF后完成本地封存；管理器崩溃仍不能由这个文件单独推断整系统操作已完成，必须核对持久意图、Run和硬件状态。

## 6. 状态字段怎样看

除原FDT wire/控制字/MFM计数外，周期模式增加：

| 字段 | 含义 |
| --- | --- |
| cycle_control=stdin-socket-v1 | 确认实际采用继承的匿名控制socket |
| cycle_control_connected | 控制通道尚未在请求前失联；不是硬件控制权证明 |
| cycle_finish_requested | 已实际读取F；不表示实际EOF/排空完成 |
| cycle_startup_aborted | A在从未连接状态实际确认；不是成功采集封存 |
| cycle_abort_refused | A遇到已有连接被拒绝；receiver继续接收，控制操作须保留核对门槛 |
| cycle_unused_refused | E遇到已接入/等待接纳连接被拒绝，receiver继续接收 |
| source_unused_confirmed | 明确未使用来源已经空队列退出/结束，未生成发送器EOF |
| connections | 同一会话累计连接数，允许多段 |
| sender_eof_confirmed | 当前已接纳连接均发生真实EOF；不表示此会话不会再接下一段 |

周期未收到有效F时stream_sealed保持false，即使上一段文件已经同步，也不让界面把它当完成Run。运行统计仍非事务快照；不要用短暂无数据或一个空目录判断停止。

## 7. 启动与使用边界

数据主机固定接收计划可选input_protocol字符串FDT。DataSupervisor自动使用--cycle-control-stdin，传完整管理身份、durable，并取消--once；启动状态必须报告实际FDT、once=false和可用的私有控制通道。

不应手工把--cycle-control-stdin与普通终端、/dev/null、文件或公开FIFO配合。receiver要求stdin为socket、完整session/sender/configuration/artifact身份、FDT、durable且无--once，条件不符拒绝。普通独立FDT仍按[传输手册](RECEIVER_TRANSPORTS.md)使用。

此基础服务可用既有数据主机控制客户端Start/Finish，在本机受控测试中核对。整系统ECC可配置sender.protocol=FDT并匹配接收计划，启停/空周期/归属说明见[完整受管理流程](MANAGED_FDT.md)。本机模拟验证不能替代真实设备启停顺序和固件互通验收。

## 8. 失败启动中止

AbortStartup继续只允许同一个失败启动对象，全部活着子进程先核对从未连接或收到数据，保持已有失败门槛。FDT使用私有A，不发TERM；正常TCP/ZBUF保留原空启动信号策略。receiver在实际读取A时再次核对，连接竞态时拒绝而继续录盘。

管理器仅接受退出1、cycle_startup_aborted=true、确切的未连接中止错误、connections/received=0及无发送器EOF作为FDT空启动确认。不会把其他错误退出1冒充安全中止，也不会生成成功采集seal；重复Abort仍拒绝。

## 9. 测试复现与清理

tests/receiver_cycle_control_test.py直接使用真实socketpair/本机FDT TCP，验证F在活动连接期间不制造EOF、控制器失联继续接收两段、已有连接拒绝A、空启动A、空会话F和非法请求。tests/run_receiver_observation_tests.py复用实际管理进程/HTTP回归，增加三段同PID/session、最终封存/原字节/归档，以及失败启动中止等场景。 tests/receiver_cycle_http_test.py再通过真实认证数据服务和原生查询客户端核对三段封存、活动Finish超时保留进程、空会话失败；超时前确认两源实际接入/解析帧，每段完成分别确认实际EOF。普通及新代码插桩均通过。

测试只在新建私有目录生成小量合成数据，finally先结束自有测试进程再删除.graw，保留小型SHA/计数/日志；每轮全部流上限20GiB，具体上界与结果见[验证记录](VALIDATION.md)。真实Run文件不按此规则删除，SIGKILL/断电仍不保证清理。

封存清单是目录枚举顺序，拼接整流必须按原生文件编号排序，不能依赖readdir或按未补零编号字符串排序。验证分别核对每个文件字节和完整流，不为测试放宽生产seal校验。

可按顺序复现小载荷专项（BUILD是对应普通或插桩管理程序目录，RECEIVER是同构建接收器）：

```bash
python3 tests/receiver_cycle_control_test.py RECEIVER build-fdt-cycle/reproduced-control
python3 tests/receiver_cycle_http_test.py BUILD RECEIVER build-fdt-cycle/reproduced-http
python3 tests/run_receiver_observation_tests.py BUILD RECEIVER build-fdt-cycle/reproduced-supervisor
```

每次产生新的私有证据子目录；成功后只保留小型证据，raw删除。已冻结包可执行`python3 tests/archive_receiver_cycle_validation.py --historical-only`核对历史文件，核对当前源则去掉该参数。它只检查已有包；`--create`仅创建尚不存在的包，不覆盖历史。
