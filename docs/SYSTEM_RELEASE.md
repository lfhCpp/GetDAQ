# 结束运行与撤销准备：Breakup、Unprepare 和 Reset 的边界

本文对应原生 C++11 库 `SystemAcquisitionClient::Breakup`、`SystemPreparationClient::Unprepare` 和 `CoBoAlarmClient::ValidateUnsubscribe`。这些接口接续同一准备对象、自有订阅、对齐快照及实际连接的发送器集合。它们没有 Qt 或原始帧 API，也没有新增第三方运行库。

本页的 `SystemAcquisitionClient` 和 `SystemPreparationClient` 库接口本身不提交 ECC 状态，也不管理接收进程；它们由 `NativeEccCoordinator` 组合后，native ECC 模式已接入持久状态、远程 receiver 生命周期及 Qt 的低频控制入口。旧 SOAP 模式仍保留，行为不等同于该新闭环。真实硬件全局控制权、加载版本核对和任意冷启动 UNKNOWN 恢复仍未完成。独立 CLI 不能取代长期持有的准备/运行对象，因此不能绕过它们直接复位或断电。

源文件：

- [系统采集接口](../apps/hardware/systemacquisition.h)和[实现](../apps/hardware/systemacquisition.cpp)。
- [系统准备接口](../apps/hardware/systempreparation.h)和[实现](../apps/hardware/systempreparation.cpp)。
- [AsAd 观察及断电计划](../apps/hardware/cobounprepare.cpp)、[自有告警实现](../apps/hardware/coboalarmclient.cpp)。
- [完整 TCP 测试](../tests/hardware_system_release_test.cpp)与[真实旧函数独立参考](../tests/system_release_original_reference.py)。

## 1. 先区分四种“停止”

| 动作 | 做什么 | 完成之后仍需处理什么 |
| --- | --- | --- |
| Stop | 停止硬件生产，再停止并刷新正在使用的 sender | TCP 连接仍保留；配置仍有效 |
| DisconnectData | 断开已停止或尚未启动的自有数据连接 | 配置仍有效；receiver EOF/排空/封存须独立核对 |
| Breakup | 按当前阶段停止生产/刷新、断开自有连接、清除全部声明 CoBo 的就绪/模式位 | 保留准备对象、对齐快照与告警；同一配置可重新 Configure |
| Unprepare | 在生产已停止后观察实际上电的 AsAd、断电、注销本对象的告警订阅 | 当前准备对象不可再次 Prepare；不销毁板端描述 |

Stop 成功不能当作 EOF。普通 TCP sender 只有断开 socket 后才让接收端收到 EOF。Breakup 的断开应答也不能替代接收器的结构检查、正常退出、目录锁释放和 seal。

“Reset”在收到的旧 GET 状态机中表示向前一个配置阶段退回，而不是把所有板恢复到出厂值。旧 `BackEnd::onUnPrepare` 调用 `SystemManager::unprepare`；旧 `onUnDescribe` 只忘记主机拓扑。新 `Unprepare` 对应其中的撤销准备阶段，完整生产 Reset 的网络/状态提交及忘记拓扑仍待接入。

## 2. 为什么原始数据不进入这些接口

~~~mermaid
flowchart LR
    UI[Qt 配置和信息界面] -.生产接口待接.-> E[原生 ECC 控制队列]
    E --> A[同一采集对象：Breakup]
    A --> S[自有 sender：低频 Stop / Disconnect]
    A --> P[同一准备对象：清状态 / Unprepare]
    P --> H[CoBo / MuTaNT：寄存器与告警]
    S -->|独立 TCP 原始 MFM| R[原生 receiver]
    R --> F[graw / seal]
~~~

控制队列的 JSON 日志、寄存器读回、Ice RPC 和 fsync 都不位于逐帧接收路径。操作台可以读取有限的状态摘要；不能让每个原始事件经过 GUI 信号、界面线程或这套控制日志。

性能验收仍须单独测板端、网络、接收队列和磁盘；本机控制夹具通过不表示已经满足多板持续数据量。

## 3. 调用者必须保证什么

1. 所有准备、采集、订阅及配置动作进入同一个串行控制队列。各对象自身的 mutex 不构成跨对象、跨进程或远端硬件锁。
2. `SystemPreparationClient` 比采集对象更长寿；它仍拥有原来的客户端、监听器和对齐掩码。不能在运行过程中重新绑定另一份配置。
3. 节点/代理与物理板对应关系正确。设备全集、名称和元数据检查能发现映射变化，但无法检测所有 DNS/多 IP 物理别名。
4. 对调用 Unprepare 的这一刻，重新核对数据进程已经停止，并传入本次声明。初次 Prepare 的 `dataProcessesStopped=true` 不能代表经过一次采集后仍然停止。
5. 所有 UNKNOWN 按生产恢复流程核对。不能通过新目录、新对象、另一个 CLI 进程或重启 GUI 清除未知结果。

Unprepare 的布尔参数只是新的调用者声明。当前库不会读取 receiver PID、验证 seal、停止旧 dataRouter、实施全局控制权或证明 sender 归属。

## 4. Breakup 的状态规则

| 运行对象当前状态 | 本次额外停止/刷新 | 本次断开 | 成功后的状态 |
| --- | --- | --- | --- |
| Ready | 无；尚未 Start，重新观察生产停止 | 自有 sender 各一次 | 运行 Broken；准备 Prepared |
| Running | 先全部生产停止确认，再刷新正在使用的 sender | 刷新确认后各一次 | 同上 |
| Paused | 不重复已完成的停止/刷新，重新观察停止 | 各一次 | 同上 |
| Stopped | 不重复已完成的停止/刷新，重新观察停止 | 各一次 | 同上 |
| Closed | 不重发已完成的断开，重新观察停止 | 无 | 同上 |
| Failed、Broken 或执行中 | 拒绝 | 无 | 状态保留 |

父准备对象必须仍是同一个 Configured 对象。`Breakup` 成功后，原运行对象进入终态 Broken，不能再次 Start/ConnectData/Breakup。准备对象仍可对**同一份不可变配置**重新 Configure；新一轮运行使用新的采集对象和已经核对的新接收目录。

Breakup 不删除 Level2 配置、不重做 PLL/上电/校准，不重建告警订阅，也不丢弃保存的机箱内时钟掩码。保持告警不阻止负向停止和释放，但仍阻止随后的 Configure/Start；Breakup 不是清除故障的方法。

## 5. Breakup 的具体顺序

1. 新建本次审计目录，记录操作意图。
2. 如果当前 Running，先主/从 MuTaNT、再各活动 CoBo 的 pulser/固件/中断停止；一个目标失败仍尝试其他独立生产停止。全部确认后才停止/刷新正在使用的 sender。
3. 重新核对全部节点名称、停止目标和真实 sender 接口，并观察全部声明 CoBo/MuTaNT 的生产停止值。停用实验板仍属于本次受控拓扑。
4. Closed 跳过断开；其他允许阶段按私有集合的配置顺序断开，每个 sender 只调用一次。部分失败不继续后面的断开，不清状态位。
5. 记录所有自有断开应答，以及最初 ConnectData 创建的数据链审计目录。
6. 同一准备对象重新核对所有设备全集、完整绑定描述及生产停止前提。然后先全部声明 CoBo 写 `initDone=0` 并读回，再全部写 `mutantMode=0` 并读回。
7. 完成记录持久化后，准备对象回到 Prepared，运行对象进入 Broken。

生产停止未知时不会刷新/断开 sender；断开未知时不会清除就绪位；第一块状态位写入未知时不会继续下一块状态位。成功的前段动作保留在日志中，不补偿或重放。

旧 `SystemManager::breakup` 对全部声明 Node 调用 `daqDisconnect`，包括可能没有读出连接的节点；新实现只操作本运行对象实际拥有的 sender，避免断开其他客户端的连接。旧 `BackEnd::onBreakup` 总调用 onStop；新实现依据已经确认的本地阶段避免重复 Stop/flush。实际停止值仍需重新观察。

## 6. Unprepare 的状态和检查

接口是：

~~~cpp
void SystemPreparationClient::Unprepare(
    const std::string &newAuditDirectory,
    bool dataProcessesStoppedConfirmed);
~~~

只允许 Prepared。Configured 必须先 Breakup；Failed 必须走真实恢复核对，不能直接 Unprepare 重试。成功进入 Unprepared，该对象不可再次 Prepare/Configure/Unprepare。

首次修改前整组检查：全部 CoBo/MuTaNT 名称、设备全集、完整描述元数据、所有条件观察/断电目标及访问权限、每个 Active 自有订阅的原服务接口。元数据/服务全部通过后，再读取 CoBo firmwareEnable/intEnable/initDone=0 和 MuTaNT run=0。

自有订阅预检查使用订阅时缓存的同一 AlarmService，核对真实 `ice_isA` 与节点名称，不调用 getAlarmService 来替换归属，不订阅、不 reset、不注销。已明确 Closed 的自有订阅可跳过；Unknown、Subscribing 或 Unsubscribing 均拒绝。

## 7. 断电前为什么要读取实际供电掩码

配置中的活动 AsAd 集合不一定等于此刻上电的集合。例如配置只有 0 和 3，但此前维护让位置 1 也上电；也可能保护已使配置中的某个位置断电。不能只遍历配置而漏掉实际上电的板。

每块声明 CoBo 按以下顺序执行：

1. 一次读取整个 `ctrl/asadEnable`，保存实际供电观察。
2. 按位置 0、1、2、3，只有观察中的对应 bit 为 1 才写 `ctrl/asadSelect/d`，读回选择，再读 `asad/interruptStatus1` 原始保护状态。
3. 整个 `ctrl/asadEnable` 写 0，再读回确认 0。

实际掩码为 0 时跳过所有保护状态读，但仍执行断电命令/确认。实验停用但已声明的 CoBo 也处理。选择读取位置不会额外改变 ASAD 慢控制写掩码。状态非零保留原数值，不清保护、不推断故障已经恢复，也不因此阻止负向断电。

旧代码在每块板内“观察→断电→注销固定 ECC 订阅”，并吞掉多种异常。新实现先完成全系统观察/断电确认，再只注销本对象自己的订阅；不会调用 reset 或移除其他客户端。

## 8. 部分失败时为什么不简单退出或重试

| 故障位置 | 后续动作 | 最终结果 |
| --- | --- | --- |
| 全局只读预检查/已有目录冲突 | 不进入执行，不发送释放写入 | 保持原成功阶段；修正前提后可重新检查 |
| 一块 CoBo 的保护观察失败 | 仍独立尝试该板断电及其他板观察/断电 | Failed；保留全部自有告警订阅 |
| 一块断电生效后应答丢失 | 不重发该板断电；仍尝试其他独立板 | Failed；不注销订阅，需要恢复核对 |
| 所有观察/断电确认后，注销应答丢失 | 该订阅 Unknown；不继续下一块注销 | Failed；保留已确认与未知结果 |
| 命令已确认，完成日志失败 | 不重复已确认命令；不能声明整个操作成功 | Failed；保留可用审计 |
| 根日志无法持久记录下一步意图 | 不发未经审计的下一步命令 | Failed；不是日志故障时的无条件紧急停机 |

其他独立板的负向断电不依赖已经失败的保护选择读，因此可以继续尝试；这不等于重新发送未知命令。Unprepare 失败后，方法本身不提供再试一次的路径。需要实际硬件/订阅核对的生产恢复协调器仍待实现。

## 9. 后端接法示例

以下是同一生产控制队列中的调用顺序示意。`prepared` 和 `run` 必须就是此前成功准备/配置/连接并长期保留的对象。

~~~cpp
// 当前 run 为 Ready / Running / Paused / Stopped / Closed。
run.Breakup("audit/run-004-breakup");

// 调用者等待并逐一核对 receiver 的 EOF、尾帧、fsync、退出、锁释放、seal。
// 还要核对全部相关旧/新数据进程及实际控制权。
const bool dataProcessesStoppedConfirmed = true; // 仅在本次真实核对之后设置。
prepared.Unprepare("audit/run-004-unprepare", dataProcessesStoppedConfirmed);
~~~

示例中的 true 不是接收器检查代码。不能照抄成固定常量部署。生产后端还需要把不可变配置、操作 ID、审计目录、接收目录/运行归属和最终 ECC 状态提交绑定起来。

若只重新配置同一系统，Breakup 成功并核对旧接收器完成之后，可调用 `prepared.Configure()`，再建立新采集对象和新接收运行。不会再次 Prepare、再次订阅或用当前寄存器值替代保存的对齐掩码。若出现保持告警，Configure 仍拒绝，不能以重新配置冒充修复。

## 10. 日志在哪里

Breakup 新目录中的 `system-run.jsonl` 记录停止子步骤、全体断开确认、原数据链目录和终态；`configuration/system-release.jsonl` 记录全部状态位清除。sender 的 Disconnect 子日志保存在最初 ConnectData 的 `data-links/`，不是重新建立一套连接。

Unprepare 新目录：

~~~text
audit/run-004-unprepare/
  system-release.jsonl
  step-0.jsonl          # 第一块板实际供电/保护观察
  step-1.jsonl          # 第一块板断电/读回
  step-2.jsonl          # 下一块板观察
  ...
  step-N.jsonl          # 全部断电确认之后的自有注销
~~~

目录只允许全新创建，0700；文件0600，关键记录 fsync，不覆盖前次结果。释放根日志限16MiB；HardwareClient 子日志沿用自己的计划/观察限制。原准备根日志额外记录 configuration_invalidated/unprepare_complete，但输入 SHA 仍不是已经加载到真机的版本证明。

`receiver_sealed=false`、`production_ecc_state_committed=false`、`control_lock_verified=false` 和 `actual_loaded_configuration_verified=false` 明确保留这些未完成的职责。订阅命令日志最后的 acknowledged 表示 RPC 应答；本地 Closed 及监听事件记录还需一起看。

## 11. 构建和复现验证

~~~sh
cmake -S third_party/ice-compat -B build-ice-compat -DCMAKE_BUILD_TYPE=Release
cmake --build build-ice-compat --parallel 4
cd build-ice-compat
ctest -R '^hardware-system-release-' -j3 --output-on-failure
cd ..
python3 tests/system_release_original_reference.py
~~~

测试实际执行完整 Prepare/Configure，不直接伪造成功状态；使用收到的 CoBo11设备/1310寄存器、MuTaNT5设备/87寄存器和两种 Ice 编码。正常阶段覆盖 Ready/Running/Paused/Stopped/Closed/保持告警，原对象多次 Configure、Breakup→同一对象Unprepare，以及实际掩码11/0/2和未活动板。

注册项分为normal-0/normal-1和fault-0..6，每项拥有独立的回环服务、目录及索引。正常两组索引各八个成功阶段；参考程序默认读取这两个索引。可并行运行，全部九组均须通过；分组没有跳过完整Prepare/Configure或缩短旧硬件等待。单个二进制不加--scenario时仍执行全套，可能需要较长时间。

故障夹具让生产禁止、断开、状态清除、断电和注销真正生效后丢失应答，并检查只调用一次；另外检查观察异常仍尝试独立断电、后段名字/服务预检查零写、已有目录不消耗 Prepared、未订阅时不虚构所有者及不删外部订阅。

旧参考提取九个实际 SystemManager/CoBoNode/Node 函数体，外围仅使用记录存根，不调用新计划生成期望。比较成功断开/全部状态位顺序和实际供电/保护观察/断电；新增读回、失败传播、归属订阅 ID、全体断电后注销及无自有 sender 的旧断开另列边界。它不证明旧后端完整运行或电气效果。

实际结果、插桩范围及归档参见[验证记录](VALIDATION.md)。旧 Ice/GET/bzip/yaml 未整体插桩，板端 TDC/IceEirfu、真机、持续跑数及目标平台仍待验收。
