# Prepare/Configure 失败后怎样明确停止生产

## 1. 这个入口解决什么问题

Prepare 并不只是检查参数。它可能已经复位板卡、打开 AsAd 供电、做校准，并在部分步骤临时打开固件。第一块 CoBo 完成、第二块中途失败时，系统不能假装什么都没发生，也不能直接重新 Prepare。

以前的 `stop-production-after-failure` 要求服务已经创建 `SystemAcquisitionClient`。Prepare 失败时还没有这个对象；Configure 在启动 receiver 前失败时也可能没有。所以原来的停止入口在这些情况下会拒绝，留下了准备阶段的缺口。

现在 `SystemPreparationClient` 也有显式 `StopProductionAfterFailure`。原生 ECC 仍使用自己保留的准备对象，在没有采集对象时转给它；已有采集对象时沿用采集对象的停止记录，包括仍为 Configured 或刚完成连接的 Ready。这个动作只关闭生产源，保留未知结果和后续核对要求。

原始数据路径保持为 sender → 独立 receiver → 磁盘。停止命令、返回状态和日志都是低频元数据，Qt 不接收或转发原始 MFM。

## 2. 三种“停止”分别表示什么

| 动作/证据 | 已确认内容 | 还没有确认的内容 |
| --- | --- | --- |
| 普通 Stop 成功 | 健康运行对象停止生产并刷新 sender，ECC 提交 Ready | sender 连接仍保留；receiver 尚未自然 EOF/封存 |
| 故障后的生产停止操作 complete | 此保留对象的独立生产停止阶段均已确认 | 原故障仍 UNKNOWN；不提交 Ready，不断开 sender，不确认 EOF/封存 |
| Breakup 和 receiver 封存成功 | 正常生命周期的连接断开、自然 EOF、尾帧检查、排空、同步及正常退出 | 不证明所有板端物理事件都完整，也不代替持续吞吐验收 |

网络操作的 `phase: complete` 和控制器的 `outcome_unknown: true` 可以同时成立。前者表示这次负向动作成功，后者表示原 Prepare/Configure/采集故障仍未完成系统核对。两者没有矛盾。

## 3. 什么时候允许调用

原生 ECC 要求原状态机已经 UNKNOWN，并仍保留同一准备或采集对象。仅仅 `needs_reconciliation` 或操作员声称硬件停止，都不能代替这个条件。

没有采集对象时，准备对象须是 Failed，或在父 ECC 已 UNKNOWN 的情况下保留 Prepared/Configured。后一种情况包括子准备步骤成功、但父完成记录或接收启动随后失败。执行后准备对象保持 Failed。

独立使用准备库时，只允许其自身 Failed；可传入 `parentOutcomeUnknownConfirmed=true` 的调用者必须是真正持有父 UNKNOWN 证据的协调层。这个参数不是解除门槛的工具。准备阶段仍要求保留原独占控制声明，该声明也不是已实现的远端全局锁。

没有保留对象、服务重启后只有旧日志、尚未进入动作的只读预检查错误、错误的节点绑定、输入/日志已篡改等情况，不能用新建对象或改审计根绕过。完整硬件重启恢复仍待实现。

已有采集对象时，允许 Failed；父 ECC 已 UNKNOWN 时另允许 Configured/Ready/Running/Paused/Stopped。Configured 表示采集对象已创建，ConnectData 还没开始；Ready 表示本对象已完成连接，但父 ECC 可能因为接收观察或日志失败仍未提交 Ready。两者都使用原对象停生产，不退回准备对象，也不清空或重建连接记录。Closed/Broken 等完成释放的对象仍不允许通过这里重发停止。

| 失败位置 | 保留对象 | 连接记录 | 明确停止后的边界 |
| --- | --- | --- | --- |
| 部分 Prepare，尚无采集对象 | 原准备对象 Failed | 没有新建采集连接 | 准备对象仍 Failed；父原操作 UNKNOWN |
| 采集对象创建后、ConnectData 前收到告警 | 原采集对象 Configured | 尚未调用 sender Connect | 采集对象 Failed；receiver 原监听会话保留 |
| ConnectData 完成，父记录或最后接收观察失败 | 原采集对象 Ready | 原 sender 连接保留 | 采集对象 Failed；不关闭 TCP、不确认 receiver EOF |

这里“对象Ready”是采集库的内部阶段；GUI看到的父ECC最后确认状态仍可能是Prepared。不能只看这个状态数值继续Start：必须同时检查UNKNOWN、网络核对门槛和原Configure结果。生产停止的complete也不会把父状态变成Ready。

## 4. 实际发送哪些动作

不要求对齐成功，不读不存在的对齐快照，也不要求 Configure 或 sender 连接成功。停止计划与正常采集的停止计划共用编译函数。

1. 有 MuTaNT 时，先主模块，再各从模块：写 `CARD_B_EMC/TRIGGER_CTRL/run=0`，读回确认。
2. 按固定拓扑顺序处理每个活动 CoBo：停止周期 pulser；写 `ctrl/pipeCtrl/firmwareEnable=0` 并读回；写 `ctrl/pipeCtrl/intEnable=0` 并读回。
3. 一个独立阶段失败，继续尝试其他独立停止阶段。比如 C0 固件停止的应答丢失，仍处理 C0 中断停止和 C1 的生产停止。
4. 全部独立阶段确认后，记录本次生产停止完成；任何阶段未确认，返回失败并保留全部证据。

停用 CoBo 不发送这组生产写入。每个实际节点的名称和绑定描述/接口/权限在本阶段执行前检查，节点名称变化时不写该节点，继续处理其他独立节点。非法重复代理先拒绝，避免把同一硬件别名当作多个独立目标。

告警保持不会阻断这个负向停止。停止不会关闭 AsAd 供电，不清 `initDone`，不改 PLL、同步脉冲或内存指针，不调用 sender Stop/Disconnect，不注销/重建订阅，也不 kill/restart receiver。供电、订阅和未知 sender 的系统恢复需要另外核对。

## 5. 为什么换操作 ID 也不会再发旧步骤

准备对象在内存中保存每个 `(节点ID, 停止阶段)` 的首次尝试及确认结果。硬件调用前已经创建本阶段审计并标记尝试；应答丢失后该阶段保留未确认。

第二次明确停止即使使用新 ID，也只记录 `stop_not_repeated`，不再发送这些已经尝试过的步骤。先前未确认的阶段不会因为服务恢复响应或名称改回而被认定为成功。所有前次阶段已确认时，第二次可以返回完成，但仍没有新的硬件效果。

这一记录只适用于同一长期对象。服务重启不能重建它来推断停止已执行或允许重放；重启入口仍保持原核对门槛。真正的掉电持久性和硬件恢复尚未验收。

## 6. 从 GUI 和 HTTP 怎样操作

GUI 的原生模式提供明确的“只停止生产”动作，走已有全局 ECC 协议。先核对固定实例/快照和凭据，发送前保存原请求；它不作为 Prepare 失败后的自动动作。失联后只查询原 ID，不重复点击来制造新请求。

HTTP 命令名称仍为 `stop-production-after-failure`，使用现有七字段规范请求：schema、operation_id、command、instance_id、snapshot_sha256、client_id、client_epoch。实例/快照来自已经核对的 `/v1/identity`，不是手工猜测；操作 ID 必须未使用，client_epoch 必须属于原控制者。认证和完整请求格式见 [网络服务手册](ECC_NETWORK_SERVICE.md)。

操作流程如下：

1. GET 原失败操作 `/v1/operations/<原ID>` 及 `/v1/status`，保存原始结果；核对是当前服务实例，不通过重启接管。
2. 明确提交一次生产停止请求，保存此请求原 ID。HTTP 202 只表示已接受，不表示板端已经停止。
3. 只用 GET 查询这个 ID，直到 complete/failed/unknown，另外读取最新 `/v1/status`。
4. complete 后仍应看到父 UNKNOWN/核对门槛；failed/unknown 时保留各目标日志，不能再发未知阶段来试探。
5. 不调用 Prepare、Configure、Reset、Breakup 或 Finalize 来消除错误。查明原硬件、订阅、sender、receiver 和审计证据后，才能设计后续恢复。

库调用形态如下，`controller` 必须是执行失败动作的原对象：

```cpp
// 调用前读取并保留原失败操作与状态；本次动作必须由操作员明确选择。
controller.StopProductionAfterFailure("operator-production-stop-001");
const GetDAQ::EccStatus status = controller.Status();
// 即使函数返回，status.outcomeUnknown 仍保持 true。
```

## 7. 日志如何查

```text
controller/operations/<本次停止ID>/
  operation.jsonl                父请求、原失败ID、快照/参数/工件摘要及保留对象
  stop-production/
    system-release.jsonl         准备对象的负向停止上下文与每阶段结果
    step-0.jsonl                 某个独立硬件阶段的意图、调用和读回
    step-1.jsonl
    ...
```

准备对象的根记录包含 `retained_object: preparation`、`retained_preparation_directory` 和 `ecc_unknown_preserved: true`。子日志含 `negative_stop_context`、`step_begin`、`step_complete/step_failed`、`stop_target_failed` 或 `stop_not_repeated`；最终 complete 还标记 `recovery_required: true`、sender/供电未改变、告警订阅保留。

已有采集对象时，`retained_object: acquisition`，子文件是已有的 `system-run.jsonl`。不要只按文件名推断当前对象阶段，应核对父记录和原失败操作。

父停止目录和 `operation.jsonl` 的意图在子流程执行期间绑定：提交完成前再次核对目录身份，以及意图文件的inode、权限、长度和完整原字节。外部替换、截断或改写不能因硬件停止已经成功而被认定为完整操作；本地存储故障会保持，恢复原文件/目录也不能清除。该检查针对父停止意图，不声称所有历史子日志都有同样的内容绑定。

原 `ecc.jsonl` 中失败操作保持原 UNKNOWN。独立负向动作不伪装为九命令状态提交，所以原生 `Operations()` 不包含它；固定网络入口自己的 `network/operations.jsonl` 另有接受/终态记录。两份记录职责不同。

## 8. 怎样复现验证

仓库根目录执行：

```bash
cmake --build build-ice-compat --target native-ecc-coordinator-tests ecc-network-service-tests getdaq-ecc-server -j3
(cd build-ice-compat && ctest --output-on-failure -j3 -R '^native-ecc-(fault-prepare.*|fault-receiver-start)$')
python3 tests/run_ecc_network_service_tests.py \
  build-ice-compat/ecc-network-service-tests build-receiver/getdaq-receiver \
  build-native-catalog-service/getdaq-supervisor build-ice-compat/getdaq-ecc-server \
  --output-parent build-ice-compat/ecc-preparation-negative-evidence \
  --scenarios prepare-failure-0 prepare-failure-1 prepare-stop-loss --jobs 3
```

上面数据服务路径是本机示例，可换为自己已经构建并核对的程序。测试会创建新的私有证据根、临时本机端口和模拟板端；不要把真实现场配置传给测试程序。

原生协调器用例覆盖两编码部分 Prepare、节点名称改变、停止写入生效后应答丢失、独立目标继续停止、恢复值/新 ID 不重放，以及 Configure 接收启动失败时尚无采集对象的停止。这里数据生命周期是明确测试替身。

网络三场景另使用实际 ECC 进程和两个实际数据服务。Prepare 故障发生在 Configure 之前，receiver 没有启动；这些场景不能声称验证原始跑数或 receiver 封存。测试核对服务接受记录和 GUI 使用的同一协议，没有执行实际 GUI 点击。

运行前四个网络场景分别在 Ice 1.0/1.1 验证 Configured 和 Ready。Configured 场景在数据节点完成 verify-ready 时通过真实告警回调触发联锁；Ready 场景在第二个原始 TCP 连接建立后替换父 Configure 审计。它们实际启动两个独立 receiver，检查停止前后相同PID/会话/原目录；Configured没有sender连接，Ready保留原TCP连接。没有调用Start，没有发送MFM帧，也没有通过这次负向停止确认EOF或seal。退出时的测试夹具清理不属于产品恢复。

```bash
python3 tests/run_ecc_network_service_tests.py \
  build-ice-compat/ecc-network-service-tests build-receiver/getdaq-receiver \
  build-native-catalog-service/getdaq-supervisor build-ice-compat/getdaq-ecc-server \
  --output-parent build-ice-compat/ecc-pre-run-negative-evidence \
  --scenarios pre-run-configured-0 pre-run-configured-1 pre-run-ready-0 pre-run-ready-1 --jobs 3
```

严格 C++11 及内存插桩范围、最终结果和证据路径见 [验证记录](VALIDATION.md)。系统 Qt、旧 Ice/GET/bzip/yaml 库不因此变成整体插桩。真机临时固件效果、MuTaNT 电气行为、生产独占控制、硬件 UNKNOWN 恢复、断电和目标平台仍需验收。
