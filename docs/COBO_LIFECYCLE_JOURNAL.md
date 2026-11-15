# CoBo 生命周期持久记录

每次新的系统 Prepare 在首个硬件步骤前创建 `cobo-lifecycle.jsonl`。同一准备对象及随后创建的采集对象共用这份日志，连续记录 Prepare、单板重新准备/配置、Connect、Start、Pause、Resume、Stop、Disconnect 和配置失效所涉及的逐板转移。它补充既有 `system.jsonl` 和每个操作的 `system-run.jsonl`，不改写旧日志，也不改变板端接口。

Native ECC 中的典型位置是 `<ecc-root>/operations/<prepare-operation-id>/prepare/cobo-lifecycle.jsonl`。准备目录的 `system.jsonl` 中，`phase=cobo_lifecycle_journal` 条目保存文件名和 `scope_sha256`。scope 绑定唯一准备目录及其 intent、配置/产物摘要、固定节点与硬件策略审计前缀；直接硬件客户端没有配置摘要时仍绑定本次目录和实际审计前缀。

## 写入与停止行为

- 文件独占创建为 `0600`，持有进程级写锁，创建时同步父目录；每次追加完整行并 `fsync` 后才发布新状态。
- 首行绑定 schema、scope、固定 CoBo ID 顺序和 enabled 标记；后续每行包含递增序号、固定索引、ID、前态、后态和 SHA256 链。禁用板保持 Disabled。
- 每次硬件联锁检查和状态追加都会核对文件身份、权限、大小与已写字节。单份日志最多 16 MiB；不自动滚动或截断。达到预算或发生写入/同步错误后，当前对象不能恢复正常写入，须走明确停止与后续核对流程。
- 普通转移写入失败时，目标板立即成为 UNKNOWN；已尝试硬件动作后的 UNKNOWN 也不会因日志失败而丢失。准备或采集审计镜像写入失败同样保留 UNKNOWN。
- 新日志的初始化审计条目及每条成功的生命周期镜像附带 `lifecycle_checkpoint`，包含独立版本schema、scope、转移数、精确字节数和终点SHA256。先同步生命周期日志，再取健康检查点并同步原操作审计；写入者故障后不能导出新的可信检查点。
- 生命周期日志不可用时，显式停止仍执行原本可审计的独立 pulser、firmware、interrupt 停止步骤。各停止步骤的去重与成功证明继续由采集对象持有；其新操作审计记录 `lifecycle_persisted=false`、`lifecycle_checkpoint=null` 和 UNKNOWN，不把硬件停止 ACK 宣称为状态持久化成功或 Run 恢复完成。若该独立停止操作自己的审计也无法建立，仍遵守原有拒绝规则。

## 重启后的含义

`CoBoLifecycleJournal::ReadHistory(path, scope, topology)` 只读解析已关闭写入者的历史，校验 scope、固定拓扑、序号、合法状态转移和散列链；不完整行、额外字段、错配的 ID/索引、权限或文件身份变化均被拒绝。进程在 Preparing 或 Starting 中断时，可以保留这个未完成阶段作为历史证据。三参数旧接口返回 `terminalVerified=false`，只说明当前文件结构/链一致，不能证明末尾没有丢失完整记录。

这个接口不会构造可操作的准备/采集对象，也不会恢复控制权。Native ECC 原有重启 UNKNOWN、原 operation 核对和禁止自动重放门槛继续生效；GUI/MCP 当前状态观察没有改成从历史文件授权控制。旧版单板操作行为逐项对齐及跨服务恢复仍需继续实现与验证；旧Node回调与全局ECC状态机的区别见[单板维护说明](COBO_MAINTENANCE.md#旧源码中的单板操作边界)。

## 独立终点检查

`CoBoLifecycleLedger::PersistenceCheckpoint()` 在账本锁内核对持久文件后返回终点；未附持久日志或文件故障时拒绝。`EncodeCoBoLifecycleCheckpoint` / `DecodeCoBoLifecycleCheckpoint` 处理小型、严格字段/整数/摘要校验的JSON：

```json
{"schema":"getdaq.cobo-lifecycle-checkpoint:1","scope_sha256":"<64 lowercase hex>","transitions":14,"bytes":4096,"chain_sha256":"<64 lowercase hex>"}
```

示例数字仅说明字段，不是实际日志终点。Prepare目录的 `system.jsonl` 初始化条目保存0次转移的检查点，准备/配置镜像保存当时终点；采集操作的 `system-run.jsonl` 保存后续各阶段终点。生命周期文件格式本身仍是schema_version 1，不改写旧文件。

四参数 `ReadHistory(path, scope, topology, expectedCheckpoint)` 在原有校验之后，要求记录数、字节数和终点链全部精确匹配，成功才返回 `terminalVerified=true`。删掉一行或多行完整记录、使用过期检查点、同长度同记录数的另一条合法历史均拒绝；不会自动截断、接受前缀、补齐或重放命令。`terminalVerified`表示与调用者提供终点相符，检查点来源真实性仍由调用者验证。

调用者必须从**独立保留且可信的原操作审计证据**取得终点，并确认它是待核对边界的最新终点；不能从待验证文件本身计算一个终点再宣称独立验证成功。散列链不是签名，不防止日志和检查点同时被篡改或同时回滚。旧操作审计没有此字段时，三参数读取仍可用，但不得将其当作已核对终点。若进程在日志追加成功而审计镜像未同步前退出，独立终点可能落后；严格读取会拒绝，保持人工核对/UNKNOWN，不猜测尾部动作是否执行成功。

新增镜像和读取入口只补充恢复证据基础；当前Native冷启动仍不从历史日志恢复准备/采集对象、控制权或自动运行。即使终点精确匹配，历史Stopped/Running也不是当前硬件读回、receiver封存或跨服务恢复成功的证明。恢复必须结合原ECC持久操作和新鲜硬件/数据服务证据。

## 验证入口

- `hardware-cobo-lifecycle`：状态矩阵与固定拓扑约束。
- `hardware-cobo-lifecycle-journal`：正常读回、scope/拓扑/转移校验、写入者锁、文件损坏与替换、失败锁存、真实部分写入及进程中断；检查点codec、整行尾部丢失、过期终点与同长度/记录数的另一条合法链拒绝。
- `hardware-cobo-lifecycle-persistence-tcp`：Prepare → Configure → Connect → Start → Pause → Resume → Stop → Disconnect 的连续历史；日志损坏后拒绝 Start；运行中日志故障仍执行停止，且后续停止不重复发送命令；逐一用原操作审计检查点验证实际历史前缀、以最终终点拒绝完整尾行丢失，并检查损坏日志的负向停止不生成可信终点。
- `hardware-system-preparation-tcp`、`hardware-system-acquisition-tcp` 和 Native/Breakup 回归：核对原工作流与故障处理。

集成夹具仅用 localhost Ice 模拟硬件及测试 sender，不代表实板、现场接收器、持续吞吐或目标平台验收。相关实现位于 [生命周期账本](../apps/hardware/cobolifecycle.cpp)、[持久日志](../apps/hardware/cobolifecyclejournal.cpp)、[准备协调器](../apps/hardware/systempreparation.cpp) 和 [采集协调器](../apps/hardware/systemacquisition.cpp)。
