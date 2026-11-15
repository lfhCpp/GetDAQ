# 原生 ECC 协调器：持久状态与硬件流程怎么连接

## 1. 这轮完成了什么

`NativeEccCoordinator` 将已有的真实原生 Describe、Prepare、Configure、采集和释放流程接到 `EccStateMachine` 与 `EccJournal`。调用者提交一个命令和一个操作 ID，协调器先检查，再持久保存意图，执行子流程，最后提交目标状态。

这里“真实原生流程”指代码调用 `SystemDescriptionClient`、`SystemPreparationClient`、`SystemAcquisitionClient` 和板端 Ice 接口。当前验证使用本机模拟板端，尚未连接现场板。它是可链接的后端库；固定HTTP入口由 [getdaq-ecc-server](ECC_NETWORK_SERVICE.md) 提供，复用同一长期对象。操作台保留旧 ECC SOAP，并已加入 [原生全局GUI模式](ECC_GUI_CLIENT.md)。

数据进程协调通过必需的 `EccDataLifecycle` 接口接入，接口没有默认成功实现。`ReceiverProcessManager`提供本机独立receiver的启动、身份/就绪、EOF/排空/退出/解锁/seal核对，`RemoteReceiverManager`将多个数据主机服务接到相同父协调器；旧阶段测试替身仍明确标记。实际控制权/加载版本、后台告警调度和完整硬件恢复仍需接续，详见[本机管理](RECEIVER_PROCESS_MANAGEMENT.md)与[远程ECC生命周期](REMOTE_ECC_DATA_LIFECYCLE.md)。

## 2. 为什么它不会降低原始数据传输性能

本库只处理低频控制命令、配置工件、元数据、状态和审计。没有接收、返回、复制或解析原始事件帧的 API，也不链接 Qt。已有本地 Ice/GET 用于板端控制，已有 yaml-cpp 用于持久日志；没有新增第三方库。

~~~text
Qt操作台 ──低频控制/状态协议── 原生ECC协调器
                                             │
                                             ├─低频Ice命令── CoBo/MuTaNT
                                             └─进程状态/封存证据── 独立receiver

CoBo/MuTaNT ─────────── 大量原始MFM字节 ───────────→ 独立receiver → 磁盘
~~~

操作日志的逐条 fsync 是控制操作的持久化条件，不进入原始数据接收线程。GUI 重绘、Qt 信号槽或配置解析也不进入接收热路径。持续高速吞吐仍须在现场网络、磁盘和真实帧条件下测量。

数据接口的`BindController`在父快照及持久根核对后调用，不执行远端命令；绑定父快照和实际审计根。`NeedsReconciliation`仅返回短缓存，数据侧已保持的故障会反映到父`Status().needsReconciliation`。Running或Paused时的负向Stop/Breakup不被数据预检查阻挡，先让保留的采集对象停生产，再核对数据条件；后续失败保持UNKNOWN，正向Start/Resume没有这个例外。

## 3. 输入包含哪些内容

`NativeEccOptions` 是构造时复制的配置，包含以下项。

| 输入 | 用途 | 要求 |
| --- | --- | --- |
| `auditRoot` | 这套部署的持久目录 | 绝对、规范化路径；父目录已经存在；不能用换目录表示故障已经恢复 |
| `deploymentId` | 稳定的部署名称 | 有界 UTF-8 文本；同目录重开必须一致 |
| `experimentArtifactBytes` | `compile-system` 产生的完整工件字节 | 内部重新严格解码、核对参数摘要；保留原字节，不重新编码后冒充原文件 |
| `description` | 全节点描述和部署策略 | 明确 `UseExisting/CreateEmpty/ReplaceExisting`；替换时提供原描述 |
| `endpoints` | 板端、链路、对齐和告警回调地址 | 实验参数由工件绑定；回调地址和事件预算仍由控制端提供 |
| `senders` | 板端 sender 与 receiver 目的地址 | 活动 ID、掩码和 Ice 编码须与工件一致，不接收多余 sender |

描述列表与准备列表逐一核对角色、ID、代理文本、最终节点名称和完整权限/布局策略。准备对象规范化 MuTaNT 数量等执行参数，sender 按绑定后的节点顺序保存。活动 CoBo、读出 MuTaNT 必须有对应 sender；停用节点不能混入运行集合。

`exclusiveOwnerConfirmed`、`dataProcessesStopped` 和 sender 的 owner 标志都是显式前提声明，**不是全局远端锁或发送器停止查询的实现**。旧接口缺少可靠的所有权/状态查询，本库不能凭这些布尔量声称已取得现场控制权。

构造期间只检查本地输入、解析代理、建立持久目录；不发送硬件动作。缺少工件、摘要不符、拓扑不符、sender 掩码错误或回调策略非法时，拒绝构造。完成构造后修改原来的输入变量不会改变已绑定的执行配置。

## 4. 持久目录是什么样的

~~~text
auditRoot/
  experiment.json                 收到的原工件，逐字节保存
  snapshot.json                   工件/接收计划摘要、部署、拓扑、策略、回调与sender绑定
  ecc.jsonl                       持久ECC意图、成功、UNKNOWN及重开记录
  operations/
    describe-001/
      operation.jsonl             本操作与参数/工件/快照/目标状态的绑定
      describe/                   原生描述子流程
    prepare-001/
      operation.jsonl
      prepare/                    同一准备对象长期使用的原审计和告警记录
    configure-001/
      operation.jsonl             引用保留的prepare目录
      receivers-start/            数据进程实现自己的审计
      connect-data/               自有sender连接审计
    start-001/
      operation.jsonl
      start/                      采集子流程
    breakup-001/
      operation.jsonl
      breakup/                    停止、断开与清就绪
      receivers-finish/           EOF/排空/退出/封存证据
~~~

目录以 0700、文件以 0600 创建，创建和写入后同步目录/文件。现有目录必须已有完整快照、工件、操作目录和日志；不会在残留的半初始化目录里新建一个日志来忘记旧结果。

快照覆盖配置/工件摘要、节点描述策略、链路/对齐地址、回调地址与配额、sender 目的地址/掩码/刷新/超时策略。日志 scope 使用快照 SHA-256。同目录输入不一致拒绝重开。修改一个回调端点也会改变绑定，不能悄悄让旧日志描述另一套部署。

运行时检查根目录和操作目录的设备号/inode、属主/权限，检查快照和工件的 inode、长度及全部字节。相同 inode 的原地篡改也会被拒绝。检测到绑定存储异常后保持 `needsReconciliation`，把内容改回去不会在同一对象里自动解除门槛。操作根审计在提交前再次核对目录和文件身份。

这些检查以及 `ecc.jsonl` 的本地 flock 约束**同一持久部署目录**；它们不是跨主机的板卡锁。不能通过新建另一个目录接管同一现场设备或清除 UNKNOWN。

## 5. 一个操作怎样提交

正常命令使用以下顺序。

1. 取得协调器命令 mutex。已有操作在执行时，立即拒绝新请求，不隐藏排队。
2. 核对操作 ID、绑定存储、ECC 状态和子对象状态。ID 限 1～128 个 ASCII 字母、数字、`_`、`-`，不能包含路径。
3. 进行只读检查。正向命令先核对全节点描述、实际接口、停止观察和必要数据状态；失败不消耗操作 ID，不发送硬件写入。
4. `EccJournal::Begin` 写入并 fsync 意图。请求键由快照摘要、操作 ID、命令和操作目录派生。
5. 创建唯一操作目录，记录相同 ID、请求键、配置摘要、工件摘要、快照摘要、来源和目标状态，然后执行原生子流程。
6. 子流程和数据条件全部完成，根审计写 `backend_complete`。
7. `EccJournal::Complete` 持久提交目标 ECC 状态。此后不补写一个可能失败的附加“成功日志”。

`backend_complete` 表示后端完成，还不能单独证明第 7 步已提交。查看当前控制状态用 `Status()`；查看持久结果用 `Operation(id)` 或只读日志工具。子日志里的 `production_ecc_state_committed:false` 保持原来的含义：子对象自己不提交生产状态，提交记录在父 ECC 日志。

`Status()` 只读取状态机短锁和存储故障标志，不等待几秒或几分钟的 Prepare/RPC；运行中的状态含 `transition`。并发调用 `Execute`、`Reconcile` 和显式负向停止共用一个入口锁。

## 6. 九个命令分别执行什么

| 命令 | ECC 转换 | 实际流程 |
| --- | --- | --- |
| Describe | Idle → Described | 全节点 Describe；清全部 CoBo 状态位 |
| Prepare | Described → Prepared | MuTaNT准备、系统对齐、CoBo准备；保留同一对象/掩码/自有告警 |
| Configure | Prepared → Ready | 同一准备对象配置；启动并核对数据进程，再连接sender；最后再次核对接收就绪 |
| Start | Ready → Running | 同一运行对象 Start；包括 RunInit |
| Pause | Running → Paused | 停全部生产，再停sender并刷新；保留连接/接收进程 |
| Resume | Paused → Running | 同一sender/连接恢复；不重新RunInit |
| Stop | Running/Paused → Ready | 停止生产/读出并刷新；保留连接/接收进程 |
| Breakup | Ready/Running/Paused → Prepared | 按阶段停止、断开自有sender、清就绪；再等待数据进程完整结束 |
| Reset | Prepared → Described；Described → Idle | 前者Unprepare并断电/释放自有订阅；后者仅忘记本地拓扑，不destroy远端描述 |

ECC 的 Ready 与采集对象的 Ready/Stopped 对应。**Stop 后再次 Start 复用原连接，但重新执行 RunInit。** Pause 后 Resume 则保留这次运行的计数边界。再次 Start 不是 Resume，两者不能混用。

Breakup 后保留同一准备对象、告警和对齐快照，可以再次 Configure；这会创建新的运行/数据连接周期。Reset 撤销准备成功后才丢弃准备和运行对象。Described → Idle 不删除板端设备；以后 Describe 按原绑定策略重新执行，仍须逐次核对。

运行中的 Pause、Stop 和 Breakup 不会先要求接收端健康或所有远端可达才允许发出停止请求：原生流程先尝试各独立生产停止。Stop/Pause 后再核对 receiver 是否仍就绪，失败则保留 ECC UNKNOWN，不能提交一个接收进程已经失效的 Ready/Paused。

## 7. 数据进程接口必须满足的条件

当前代码不会提供空成功实现。实现 `EccDataLifecycle` 时必须满足下表。

| 方法 | 必须实际确认的内容 |
| --- | --- |
| `BindingSha256` | 不可变数据计划摘要，加入父ECC快照和请求作用域 |
| `Validate` | 接收父配置/工件摘要；对应数据流程已经实现，sender目标与接收计划完全匹配；不启动/停止进程，不写硬件 |
| `VerifyStopped` | 本次数据进程确实已经停止，无在途原始写入；不能只沿用Prepare前的布尔声明 |
| `StartReceivers` | 启动独立原生receiver，保存进程/目标/输出目录的绑定和审计；部分失败保留证据，不自动重复启动 |
| `VerifyReady` | 全部接收端已经监听、存活且仍对应本次周期；端口可连本身不等于身份正确 |
| `FinishReceivers` | 每一路EOF、完整尾帧、队列排空、文件fsync、正常退出、目录锁释放与有效seal；异常必须抛出 |

接口只传sender选项、两个配置摘要、计划摘要和审计目录，不传原始事件。Stop 不是 EOF；普通 TCP 的停止会刷新并保留连接，真正 Disconnect 后接收端才能等 EOF。Breakup 原生子流程完成而 seal 失败时，ECC 保持最后确认状态并标记 UNKNOWN；不能重发 Breakup 来制造第二次断开。

既有 `TestData` 的日志明确写 `test_double:true`、`receiver_executed:false`，其证据只证明控制门槛。新增真实 `ReceiverProcessManager` 的进程/EOF/seal及原生ECC组合联调有独立夹具与目录；详细操作、代码和验证范围见[进程管理手册](RECEIVER_PROCESS_MANAGEMENT.md)。不得用旧阶段替身通过替代真实接收联调。

## 8. 失败、重启与显式停生产

只读预检查失败不会进入 Apply，不标记“动作可能生效”。意图持久化后，Apply、子流程、数据确认或完成日志失败都保留可能部分生效的 UNKNOWN；最近确认的 `state` 仅供诊断，不代表当前板端状态。新操作 ID 不允许重放未知动作。

`StopProductionAfterFailure(id)` 是独立负向入口。它只允许 ECC 已 UNKNOWN 且仍持有原准备或运行对象；已有运行对象时，其阶段可以是 Configured/Ready/Running/Paused/Stopped/Failed，连接前后故障都保留同一运行对象。没有运行对象时，转给同一 Failed/Prepared/Configured 准备对象，补齐 Prepare 或接收启动前的 Configure 故障停止。它不改sender、不清连接、不处理原始数据、不关闭供电或注销告警，不提交 Ready，也不清UNKNOWN。子对象随后保持Failed。

同一停止边界里，原生运行对象不会重发已经尝试过的生产停止阶段；第二个负向操作 ID 也不能绕过这个限制。负向操作保存在自己唯一的 `operations/id/operation.jsonl`，不伪装成普通 ECC Stop，因此 `Operations()` 返回九个状态机命令记录，不包含这些独立负向操作。原失败操作仍保留原来的UNKNOWN记录。

准备阶段的停止不要求成功对齐或Configure：先主/从MuTaNT停止，再活动CoBo的pulser/固件/中断；单阶段失败仍尝试其他独立目标。已尝试阶段不因换ID或恢复响应而重发。没有保留对象、已完成Breakup或重启后只剩日志时仍拒绝，不能新建Configured对象绕过；详细适用条件、操作与日志见[准备故障停止](PREPARATION_FAILURE_STOP.md)。未知订阅、实际控制权和数据残留的完整恢复仍待实现。

构造时重开任何已有可写 ECC 日志，都会置 `needsReconciliation`，即使最后记录是 Idle 或 complete。没有自动创建订阅、连接或重新Prepare来猜测硬件状态。

`EccBackend::VerifyRecovery` 的基类默认实现仍然拒绝恢复，独立使用 `EccStateMachine` 时不能只凭调用者提供的状态和文字证据清除 UNKNOWN。`NativeEccCoordinator` 覆盖了这一默认拒绝，但只接受由它自己生成并绑定的观察证明：同一仍存活的原服务、准确的原失败操作、对应的保留对象、停生产/清理审计和新的只读状态观察必须全部匹配。当前受支持的专用入口包括 `ReconcileDescription`、`ReconcilePreparation`、`ReconcileAcquisition` 和明确退休 ICE 源后的 `ReconcileAcquisitionWithSources`；成功后均回到 Described，旧 Run 保持中断，必须由操作者重新 Prepare。

这个有限恢复不证明 `actual_loaded_configuration_verified` 或 `control_lock_verified`，不重发 CREATE/寄存器写入，也不接管冷启动、已丢失的原对象、任意未知 sender、未退休的源故障或不相关告警。服务重启仍会保持 `needsReconciliation`，不能调用通用状态跳转清错。各路径的前提、失败保持及本机证据分别见[Describe核对](DESCRIPTION_RECONCILIATION.md)、[Prepare核对](PREPARATION_RECONCILIATION.md)、[采集核对](ACQUISITION_RECONCILIATION.md)和[验证记录70.35、70.43、70.47、70.49](VALIDATION.md#7035-原-describe-失败的绑定只读恢复2026-10-13)。现场控制权、实际加载版本及物理板端状态仍需实机验收。

## 9. 如何构建和接入

在仓库根目录执行：

~~~sh
cmake -S third_party/ice-compat -B build-ice-compat -DBUILD_TESTING=ON
cmake --build build-ice-compat --target getdaq-ecc-native native-ecc-coordinator-tests -j 3
cd build-ice-compat
ctest -R '^native-ecc-' --output-on-failure -j 3
~~~

这是已有 Ice 兼容构建的可选静态库，要求 POSIX 持久日志。纯 `apps/ecc` 状态核心仍可独立构建；Qt GUI/原始receiver都不用链接此库。实际开发环境仍为 Ubuntu20.04.6/GCC9.4，Ubuntu24/macOS 尚须实测；Windows Qt5.14.2 MinGW GUI 验收也尚未完成。

服务端未来的调用形态如下。`dataManager` 必须是真正实现上述证据的独立进程管理器，不可直接把测试替身用于现场。

~~~cpp
// options的工件、拓扑、回调、sender和固定审计目录必须先完整绑定。
// dataManager先构造，寿命长于controller；都保存在服务长期对象内。
GetDAQ::NativeEccCoordinator controller(options, dataManager);
controller.Execute(GetDAQ::EccCommand::Describe,  "describe-001");
controller.Execute(GetDAQ::EccCommand::Prepare,   "prepare-001");
controller.Execute(GetDAQ::EccCommand::Configure, "configure-001");
controller.Execute(GetDAQ::EccCommand::Start,     "start-001");
controller.Execute(GetDAQ::EccCommand::Stop,      "stop-001");
controller.Execute(GetDAQ::EccCommand::Start,     "start-002"); // 原连接，新RunInit
controller.Execute(GetDAQ::EccCommand::Breakup,   "breakup-001");
controller.Execute(GetDAQ::EccCommand::Reset,     "reset-prepared-001");
controller.Execute(GetDAQ::EccCommand::Reset,     "reset-described-001");
~~~

异常由服务入口捕获后保留 controller，不在异常处理器里创建替代对象/目录。状态查询可以在另一个线程读取 `Status()`。程序析构只释放本地资源，不自动发送现场Stop、Disconnect或注销。

## 10. 如何复现验证和阅读证据

本机测试使用收到的完整 CoBo/MuTaNT 描述、实际 Ice TCP 1.0/1.1 夹具及真实原生 Prepare/Configure。正常组覆盖九命令、Stop后的第二次Start、再次Configure、两层Reset、并发拒绝和重开门槛。故障组覆盖sender Start应答丢失、receiver启动/封存失败、Reset断电应答丢失、停机时receiver失效、父操作日志丢失和绑定存储篡改。

严格 C++11 和内存检查可执行：

~~~sh
python3 tests/build_native_ecc_sanitizers.py
python3 tests/run_native_ecc_sanitizers.py --include-acquisition
~~~

编译脚本插桩全部新参数、原生硬件、ECC状态/日志/协调器源码和测试；旧Ice/GET/bzip/yaml静态库未整体插桩。运行脚本启动本机回环夹具，使用 ASan/UBSan/LeakSanitizer，不连接现场板。它不测试 Qt GUI 或原始receiver吞吐。

最终通过/失败、用时、旧函数对照和证据位置以[验证记录](VALIDATION.md)为准；不要把“代码已加入构建”当作测试通过。完整软件边界见[重构进度](REFACTOR_STATUS.md)，子流程原理见[描述](SYSTEM_DESCRIPTION.md)、[准备](SYSTEM_PREPARATION.md)、[采集](SYSTEM_ACQUISITION.md)和[释放](SYSTEM_RELEASE.md)。

本轮最终新增9/9、选定既有6/6、九组及既有完整采集的内存检查通过。十个旧函数与普通/插桩各十二个协调器采集阶段、每组30项比较通过；既有采集另有十二阶段/28项比较。原始失败、成功、编译命令、来源副本/SHA、程序SHA和完整审计归档于`build-ice-compat/validation-native-ecc-sanitizers/`；原始审计内容不改写，另加相对路径索引。

完成测试后可复现归档和文件检查：

~~~sh
python3 tests/archive_native_ecc_validation.py
python3 tests/check_native_ecc_validation.py
~~~

归档脚本拒绝覆盖已有目录；已有归档应保留并检查，不是每次运行都重建。它只在最终普通/插桩结果成功且记录源码SHA一致时建立证据副本。归档不是新的现场部署目录，不能用其中的模拟目标或原状态记录接管硬件。
