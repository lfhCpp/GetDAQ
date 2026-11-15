# 接收进程故障后停止生产

## 1. 这项功能解决什么问题

原始数据的路径仍然是板端发送器或升级后的原生源程序 → 独立接收进程 → 数据主机磁盘。Qt 操作台和 ECC 的 Qt HTTP 前端只处理配置、命令和少量状态，不搬运 MFM 载荷。

接收进程意外退出时，生产端可能还在运行。只在界面显示红色提示不能处理这段时间内持续生产的数据。固定 ECC 服务现在可以启用后台观察策略：核对当前绑定的接收进程，保存故障依据，安排一次只停止生产的负向操作。

这里的“完成”只指停止生产的操作得到确认。已经丢失或未完整写入的数据不会被补回；原 Run 不会因此变成完整采集。

## 2. 如何启用

在完整的固定 ECC 服务 JSON 中，与 alarm_stop_policy、senders、remote_receivers 同层加入：

~~~json
"receiver_stop_policy": {
    "enabled": true,
    "poll_interval_ms": 1000
}
~~~

这是一个字段片段。实际文件须保持完整的 JSON 格式；示例见 [service.example.json](../config/ecc/service.example.json)。

| 参数 | 含义 | 约束 |
| --- | --- | --- |
| enabled | 是否让 ECC 自动处理接收进程明确失败 | JSON 布尔值；不写整个策略时默认 false |
| poll_interval_ms | 完成一轮观察后的等待时间 | 整数 100..5000 毫秒；默认 1000 |

先用 [固定服务文档](ECC_NETWORK_SERVICE.md) 中的 --check-config 检查完整配置，再按正常部署步骤启动。离线检查不会连接现场硬件或证明板端控制归属。

策略是固定输入的一部分，修改后会改变绑定摘要。运行期间编辑文件不会热更新；已观察到的输入变化会保留核对门槛。应在部署前确定策略，不能通过换目录或恢复旧字节来宣称故障已恢复。

## 3. 哪些观察会触发

服务只在保留当前采集对象的 Ready、Running、Paused 阶段观察；有正在执行的命令时不插入硬件写入。HTTP 观察使用独立客户端，每轮整体最多 1500 毫秒，完成后才进入命令接受锁核对当前状态。因此响应时间包括当前命令耗时、观察时间、等待周期、日志落盘和硬件停止，不能用 poll_interval_ms 推导停机上限。

触发必须同时满足：

1. 观察属于本 ECC 实例、父快照和固定数据计划。
2. 数据服务 agent、实例及计划摘要与保留的管理记录一致。
3. 接收生命周期仍为同一个 Ready 会话；每个接收器的 ID、会话、输出目录和 PID 都一致。
4. 观察不 busy，拥有观察时间；不是查询失败后拼出的状态。
5. 至少一个已绑定进程被明确观察为 exited/reaped，退出码大于零或被信号终止。
6. 观察前后的父状态和原操作 ID 一致，且没有并发命令、生命周期切换或后台已处理的接收故障。

这里的 Ready 是数据主机“接收会话已建立”的状态；父 ECC 可以同时处于 Running。不要把这两个状态名称混在一起。

| 观察 | 策略结果 |
| --- | --- |
| 同会话、同 PID 的非零退出或信号退出 | 保持故障依据并安排一次负向停止 |
| 正常退出码 0 | 不推断成进程失败；正常 EOF 与最终封存仍由原流程核对 |
| 服务连接失败、超时、查询 busy | 没有确定退出证据，不发送自动停止 |
| 数据服务换实例、接收器换 PID 或会话 | 不接管新对象，不把它的退出套到原采集 |
| 新鲜部署尚无接收器，或原会话已经 Stopped/sealed | 不创造故障或重新停止 |
| 策略关闭 | 只有原有只读观察，不自动发送生产命令 |
| 原命令正在执行 | 等命令结束后的新观察，不取消、重放或插入另一组硬件动作 |

这项功能尚不提供“节点失联即停采”的超时策略。失联可能是控制网络问题，不能冒充已知进程退出；现场若需要该策略，还须明确失联期限、控制归属和故障依据。

## 4. 停止顺序与状态

明确故障先保持网络核对门槛，再将完整观察和原绑定状态持久保存为 receiver-held。保存成功后，原生协调器将当前采集结果保持为需要核对，并通过现有 stop-production-after-failure 停止原生产对象。

该负向操作先处理生产端，保留独立目标的尝试记录。普通 TCP/ZBUF/FDT 发送器不会被刷新或断开；接收进程不会被 kill、重启、回收或自动封存。使用升级 ICE 源时，沿用既有故障停止回调停止其生产线程，不发送数据结束声明；细节见 [源观察](SOURCE_GUI_OBSERVATION.md) 和 [故障停止](PREPARATION_FAILURE_STOP.md)。

成功停止后可能看到：

~~~text
接收故障策略 phase = complete
父 ECC state = Running（原已提交状态）
父 ECC outcome_unknown = true
control_needs_reconciliation = true
~~~

原状态与原操作 ID 保留，避免虚构一次普通 Stop/Ready 提交。此时 Running 表示最后的已提交状态，不再是“当前硬件正在健康采集”的保证。UNKNOWN 和策略依据优先，Start、Resume、Reset 等正向操作会被拒绝。

失败或应答丢失时，策略显示 failed/unknown，不自动重发。第二次轮询、第二个接收器退出、重新打开服务都不能清除门槛或再次执行旧停止。新服务只读取原依据并进入 reopened-held；未完成的操作按原网络日志规则保持 unknown。

与硬件告警策略同时启用时，两者共用唯一命令入口，不能并发下发停止。原生产对象也保留已经尝试的目标，重复负向操作不再次发送已尝试的生产停止。

## 5. 查看与核对

认证 GET /v1/receiver-failure 返回后台缓存，不等待硬件 RPC，不访问原始 .graw：

| 字段 | 含义 |
| --- | --- |
| schema / kind | 版本 1 / getdaq.ecc-receiver-failure |
| instance_id / snapshot_sha256 | 本次 ECC 实例和固定父绑定 |
| enabled | 当前固定部署是否启用 |
| latched | 是否已经保持接收故障或启动核对状态 |
| operation_id | 唯一内部负向停止 ID；未处理时为空 |
| phase | disabled、watching、observation-unavailable、held、accepted、complete、failed、unknown、reopened-held、startup-reconciliation |
| evidence | null 或 bound_status/observation 两份原始 JSON 字符串 |

evidence 中保留触发时的状态字节，重复轮询不改写。完整状态字符串采用显式元数据容量上限；普通配置标量仍保持原有 4 KiB 限制。

operation_id 非空时，使用 GET /v1/operations/ID 读取原操作。internal-receiver- 是保留前缀，外部提交不能伪装该动作。HTTP 请求丢失也不证明动作未执行。

GUI 的控制核对和 UNKNOWN 门槛会阻止继续采集。可按第8节启用详细结果列、触发依据提示及未完成 Run 记录；不会因此启用后台策略或发送自动控制命令。

## 6. 审计记录

~~~text
controller/
  ecc.jsonl
  network/
    operations.jsonl
    operations/internal-receiver-.../request.json
  operations/internal-receiver-.../
    operation.jsonl
    stop-production/
~~~

network/operations.jsonl 中 receiver-held 包含父快照、唯一操作 ID、原依据字节和 getdaq.ecc-receiver-failure:1 域的 SHA-256。后续 accepted → running → complete/failed/unknown 属于同一个规范控制请求。

如果依据已保存但接受请求失败，仍保留这个不完整边界。重开服务不会“补做”旧请求。输入、审计文件身份或权限变化也不会绕过保持门槛发送盲目停止。

## 7. 本机测试与边界

专用测试入口：

~~~bash
cmake --build build-ice-compat \
  --target ecc-network-service-tests getdaq-ecc-server \
  fixed-ecc-configuration-tests ecc-protocol-tests --parallel 3

python3 tests/run_receiver_failure_ecc_tests.py \
  build-ice-compat/ecc-network-service-tests \
  build-source-mfm/receiver/getdaq-receiver \
  build-managed-ice/supervisor/getdaq-supervisor \
  build-ice-compat/getdaq-ecc-server \
  build-receiver-failure/ordinary
~~~

这些路径对应当前工作区已有构建，不是全新安装的统一编译入口；依赖与部署见 [离线构建指南](DEPENDENCIES_AND_OFFLINE_BUILD.md)。脚本顺序执行限定场景，每组两个合成发送器各发送两个 4096 字节帧，按每场景 2 MiB 保守计算所有传输及落盘副本，九个场景总上界 18 MiB，远小于用户规定的每轮总量 20 GiB。

成功、失败、超时和可捕获中断都停止脚本自己的测试进程组，再删除本轮 .graw，保留小型日志、计数、SHA 和清理凭据。SIGKILL/断电不能保证 finally 执行。脚本不会删除真实 Run、原始参考数据或其他目录。

测试包括两种 Ice 编码、Running/Paused/Ready、停止应答丢失、策略关闭、正常 EOF、数据服务失联及服务重开。本轮九项普通联调及三项新控制代码插桩故障检查已通过，另通过原有正常 Run/后台告警两项回归。具体程序、环境、记录和插桩范围见 [验证记录](VALIDATION.md) 第70.8节。

本机模拟硬件和实际接收进程只能证明本机软件行为，不能代替真实板端、Ubuntu 24、Windows Qt 5.14.2 MinGW、macOS、断电恢复、跨物理主机网络和持续大流量验收。尚须补充观察与人工命令的确定性竞争、告警并发、意图落盘中断及升级 ICE 源接收故障的组合验证。

## 8. 操作台查看与未完成 Run 的记录

### 8.1 两个开关分别控制后台策略和界面查询

操作台 YAML 的 getdaq 层增加：

~~~yaml
getdaq:
  native_receiver_failure_status: true
  # native_ecc 和 nodes 仍须完整配置。
~~~

这是配置片段。不要把这个开关放进要求恰好十一项固定客户端字段的 native_ecc 对象。完整模板见 [envoy.native.example.yml](../config/envoy.native.example.yml)。

旧配置省略此字段时默认 false。节点设置窗口中勾选 **Show receiver failure stop results** 可启用查看；保存配置时该选择会保留。

| 后台 receiver_stop_policy.enabled | GUI native_receiver_failure_status | 行为 |
| --- | --- | --- |
| false | false | 不自动处理，也不查询详细结果 |
| false | true | 查看 disabled，不能通过 GUI 勾选启用后台停采 |
| true | false | 后台处理照常进行；GUI 仍受原状态里的 UNKNOWN/核对门槛约束 |
| true | true | 后台处理，GUI 另查看并记录原触发依据和停止结果 |

查询只访问认证的 /v1/receiver-failure 元数据缓存，不接收原始帧，不读取 .graw，也不提交 Stop、重启或封存请求。

### 8.2 状态表怎样读

原生模式新增 **Receiver stop result** 列。将鼠标停在单元格上，可以查看：

- 策略阶段及唯一停止操作 ID。
- 触发时的父控制操作。
- 触发接收器 ID、PID、退出码、信号及会话。
- 依据是否仍对应当前保留会话，或属于历史会话。

complete 表示后台生产停止操作已确认。它不能表示丢失的数据已经恢复，也不能表示该 Run 已完整封存。failed/unknown 表示未确认的停止结果，不能手动重复提交内部 ID 试图补做。

保持故障时，全系统相关行显示需要核对；**只有匹配当前实例/会话/PID 的行**才被标记为当前触发接收器。另一个数据节点也受到全系统停生产影响，但不能把它标成发生了同样的进程退出。

reopened-held 保留旧实例的原依据。即使旧 PID 恰好与新 PID 相同，也不能仅凭数字相等认成当前进程；实例和会话仍须同时匹配。历史依据只作核对，不接管旧生产对象。

颜色表示缓存阅读结果和核对门槛，不是硬件健康证明。查询失败、非法响应或缓存超过 status_max_age_ms 时，界面撤下旧的正常结果并显示未确认/过期。已确认过的故障控制门槛不会因为后来查询失败或返回 watching 就自动清除。

### 8.3 为什么关闭界面或恢复网络不等于恢复采集

GUI 使用异步 HTTP，同一种查询最多一份在途；重复轮询不排队，也不增加新的控制动作。错误实例、父快照或数据绑定不能覆盖当前结果。服务重开必须通过正常身份/状态核对，客户端不会自动切换到另一个实例。

活跃 Run 第一次收到保持故障及后续阶段变化时，将原缓存保存到 journal.json 的 native_receiver_failure_cache，同时标记 duration_incomplete=true。该 JSON 内含 bound_status 和 observation 两份触发时的原 JSON 字符串，不是采集数据文件。

这条记录保留未完成 Run，不生成 run.csv、不前进 Run 编号，不把“后台停生产成功”改成“采集完成”。退出界面也不会因此发送结束声明、断开发送器或清理真实数据。恢复网络后应核对原停止操作、数据会话、硬件状态和实际控制归属，再按现场恢复流程处理。

### 8.4 查看功能的验证边界

纯协议检查缓存字段、阶段、触发退出及绑定一致性；Qt 客户端和模型验证错误绑定、并发查询、过期/失联撤下结果、历史会话不冒充当前，以及保持未完成 Run/不新增 POST。

实际 Qt RunControl 进程联调使用模拟板端、两个本机数据服务和真正 receiver，在 GUI 进入 Running 后由测试夹具终止原 PID，验证回传依据、生产停止和未完成记录。此测试使用操作台的生产控制模型，不是人工窗口操作或 Windows Kit 验收；实际窗口字段和显示另有 QWidget 回归。具体执行结果以 [验证记录](VALIDATION.md) 为准。

## 9. 接收故障与升级 ICE 源的组合

### 9.1 与普通 TCP 发送器的差别

普通 TCP/ZBUF/FDT sender 的故障负向停止不会调用其刷新或断开。升级 ICE 源则额外持有本机生产/读出线程；只停板端而不处理这些线程，源程序可能继续等待或尝试发送。

负向停止沿用已绑定的原控制连接、源实例和 receiver_session，调用一次 stopIceSourceAfterFailure。源端进入 UNKNOWN，依次调用自己的停止和空闲核对回调，确认生产设备回调停止、消费线程退出并 join。原控制对象保留已经尝试/已确认标志；即使当前观察又能读取，也不会恢复为健康或重发。

这个回调不是正常 Stop/Drain/Finish：没有排空待发送输入、没有补发数据、没有 End、没有 receiver 封存。父采集结果继续需要核对，不能用源线程停了来证明此前数据已经全部落盘。

### 9.2 板端停止未确认时，源线程仍应独立处理

一次负向操作对不同目标分别保留结果。板端某个停止应答丢失时，不因该目标失败而跳过其他板或已绑定 ICE 源的停止；也不重复发送已尝试的板端写入。

因此可以同时出现：

~~~text
后台停采操作 = failed
原板端停止结果 = 未确认
两路 ICE 源 failure_stop_acknowledged = true
两路源状态 = unknown
接收会话 = 原会话，未封存
~~~

这是部分停止证据，不能解释成整个系统已经恢复。保留原操作 ID、各目标记录、来源和数据会话，继续现场核对。

### 9.3 本机组合检查如何执行

~~~bash
cmake --build build-ice-compat \
  --target ecc-network-service-tests getdaq-ecc-server --parallel 3

python3 tests/run_receiver_failure_ecc_tests.py \
  build-ice-compat/ecc-network-service-tests \
  build-source-control/data-only/getdaq-ice-receiver \
  build-managed-ice/supervisor/getdaq-supervisor \
  build-ice-compat/getdaq-ecc-server \
  build-ice-receiver-failure/ordinary \
  --scenarios ice-receiver-running-0 ice-receiver-running-1 \
              ice-receiver-stop-loss ice-receiver-restart
~~~

ICE 接收场景必须传 getdaq-ice-receiver，不能把同名旧构建或默认纯 TCP receiver 当作它的替代。专用服务/程序的构建入口见 [ICE 数据入口](ICE_DATA_INGRESS.md) 和 [升级源控制](ICE_SOURCE_CONTROL.md)。

| 场景 | 检查重点 |
| --- | --- |
| ice-receiver-running-0 / 1 | 两编码下真实接收进程退出，后台停止原生产及两路源线程 |
| ice-receiver-stop-loss | 板端停止应答丢失仍处理两路源；failed/UNKNOWN 不重放 |
| ice-receiver-restart | 保存原来源/接收依据，重开 ECC 不重放、不接管原源对象 |

每场景按所有传输及落盘副本保守计 2 MiB，四场景组上界 8 MiB；失败、超时和可捕获中断都会处理测试进程组并清理本轮 .graw。源/接收前后状态、线程停止标志、End/封存不存在和重复停止次数存入小型凭据。

本机源服务和 SourceRingReadout/SourceMfmRing 的真实实现嵌入测试夹具进程，ECC、数据服务、receiver 是独立实际进程。测试先在每来源两个合成环形缓冲各写一个 4096 字节完整 MFM 帧，确认读指针已经发布且消费线程仍在运行，再终止接收进程。它证明保持对象的线程停止/join 与控制结果组合；不证明正在大量生产时的 DMA/背压、丢失字节量、物理设备停止或独立现场源进程部署。

每来源源已发/本地发送均为 8192 字节，并在负向停止前后保持，不能据此推断 receiver 全部收到或持久写盘；被终止接收器的末尾数据可能尚未落盘。完整记录与最终通过范围见 [验证记录](VALIDATION.md)。

## 10. 后台观察与人工停止同时发生

后台查询发出时的状态，不一定还是响应返回时的状态。人工命令可能已经改变原操作、停止生产或处理了数据会话。如果后台拿旧响应直接创建停止意图，就可能重复操作，或者把旧进程的退出用于新会话。

因此网络查询期间不占命令入口锁；收到回答后才尝试进入同一个命令入口，重新核对父状态、操作 ID 和管理器保留的实例/计划/会话/PID。已有命令忙时不插入另一组动作；状态或原操作变了，旧观察不能提交 receiver-held。

专用并发夹具使用以下顺序：

1. 实际 ECC/数据服务/receiver 进入 Running。
2. 终止本轮接收进程，数据服务通过只读 waitid 得到真正的失败退出。
3. 测试代理在真实响应产生后保持这一份响应；其他状态和命令请求仍可并发通过。
4. 人工普通 Stop 执行生产停止，但因接收器已退出而保持未确认；随后显式负向操作核对原生产对象。
5. 关闭本轮数据服务，阻止后续新观察再次报告同一失败；在原后台 HTTP 客户端仍未超时、原1500ms预算未改的情况下，释放旧响应。
6. 核对父操作已改变、旧响应无 receiver-held/自动停止、板端和sender停止没有被重发，状态查询在屏障期间仍响应。

这是实际进程、实际失败观察和明确屏障的组合，不靠延时猜测查询是否已经进入，也不把超时/丢响应当“成功丢弃旧观察”。

~~~bash
python3 tests/run_receiver_failure_ecc_tests.py \
  build-ice-compat/ecc-network-service-tests \
  build-source-mfm/receiver/getdaq-receiver \
  build-managed-ice/supervisor/getdaq-supervisor \
  build-ice-compat/getdaq-ecc-server \
  build-receiver-stale/ordinary \
  --scenarios receiver-stale-stop-0 receiver-stale-stop-1
~~~

每场景2MiB保守预算，两编码组4MiB，仍自动删除本轮生成的.graw。测试代理的并发线程、捕获响应和离线控制只存在于tests/，产品没有新增暂停钩子、超时放宽、模拟响应或新的恢复捷径。最初夹具把测试有意关闭目标后的连接失败算成代理错误；该失败组清理后，代理明确返回503供后续观察标为未确认，再以最终同版重跑。

这项检查证明“父操作已经改变时，原失败响应不再安排自动停止”。它没有证明所有人工命令/所有调度组合，也不覆盖硬件告警同时竞争、跨物理主机时钟/网络或掉电。完整结果以[验证记录](VALIDATION.md)为准。
