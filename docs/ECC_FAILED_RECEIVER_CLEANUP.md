# ECC 与操作台：清理原失败接收会话

本功能把数据服务的失败会话清理接入主机端 ECC 和 Qt 操作台。操作者在原 ECC 完成明确停生产后，可以清理同一保留采集对象中的剩余接收器。原始文件保留，UNKNOWN、待核对和未完成 Run 保持；这一步不是硬件恢复，也不会重新开始采集。

## 1. 操作台怎样使用

1. 使用 native_ecc 模式连接原 ECC，查看原实例、操作、数据节点和失败原因。
2. 等待后台失败停生产完成，或者使用 `Native fault: stop production only (keep UNKNOWN)` 执行原对象的明确停生产。
3. 确认当前状态仍属于原 ECC 的 UNKNOWN，并查看停生产操作结果。失败或不确定的停生产不能继续清理。
4. 选择 `Native fault: terminate original failed receivers (keep UNKNOWN)...`。
5. 确认框说明原始文件保留、UNKNOWN 和未完成 Run 保持。取消不会提交请求；确认后只提交一次新操作。
6. 按该操作 ID 观察结果。成功只表示原失败接收器清理完成；不能把它当成数据完整或重新 Start 的许可。

入口只在 native 模式可见，健康状态、断开或忙碌状态禁用。客户端还要求新鲜状态和原身份，恢复凭据、换实例或旧回复不会使门槛自动清除。网络应答丢失后保留原操作 ID，只查询原结果，不自动换 ID 重发。

如果存在本地 Run journal，清理结果以 `native_receiver_cleanup_operation` 保存，`duration_incomplete` 保持 true。它不递增 Run、不写 Complete、不生成正常结束 CSV，也不自动断开。GUI 的 Run Recover 仍只恢复原 Run 关系，不解除硬件 UNKNOWN。

## 2. ECC 为什么能拒绝未经确认的清理

ECC 的新命令是 `abort-failed-receivers`。它沿用固定实例、快照、客户端 ID/epoch、新操作 ID、持久意图和原 ID 查询规则，不把它加入普通九个状态迁移命令。

原 `stop-production-after-failure` 成功完成后，ECC 保留其原失败操作 ID、停生产操作 ID，以及绑定 inode/权限/完整字节的停生产审计文件。清理前必须同时满足：

- 仍是同一 ECC 进程，仍持有同一个失败采集对象。
- 当前仍 UNKNOWN，原失败操作 ID 与已确认停生产记录一致。
- 停生产的完整审计文件没有变动。
- 本对象尚未尝试过接收器清理。
- 固定配置、工件、父目录及控制存储完整性检查通过。

冷重启、仅有准备对象、部分接收启动、没有完整停生产、失败/丢失应答或被修改的停生产审计都不能据此接管清理。审计核对失败会保持存储故障；恢复原字节也不会自动重新允许清理。

目前只接续成功启动且已保留的采集会话。未完成启动仍按原 `abort-startup` 与准备失败处理核对，不能借这个命令把它假装成原完整会话。

### 2.1 Breakup 已完成，但数据封存失败

一次停止包含不同工作：禁止硬件生产、发送器停止/断开、撤销就绪配置、数据接收器排空/封存。前几步成功不保证最后一步成功。例如，一个来源末尾出现截断帧，它的封存会失败；另一来源仍可能正常封存。

这时硬件采集对象已经是 `Broken`，同一准备对象回到 `Prepared`，但父 ECC 保持原失败操作和 UNKNOWN。不要再次点击普通 Breakup 或重新建立新会话来覆盖它。

现在可以先提交原有 `stop-production-after-failure` 命令。对这个特定 Broken 对象，该命令只核对，**不再发送停止、断开或撤销配置命令**：

1. 必须仍持有原采集对象和同一 Prepared 对象，父 ECC 已 UNKNOWN。
2. 必须保留原成功的全部生产停止结果：活动 CoBo 的 pulser/firmware/interrupts，各 MuTaNT 的停止阶段。缺少结果时拒绝，不能只看一个寄存器是零就认为全部停妥。
3. 原发送器的本地结果必须为 Fresh 或 Disconnected，且没有未知结果；这不是远端全局所有权证明。
4. 核对所有原板身份和描述，再用只读 Expect 步骤读取 CoBo 的 firmwareEnable、intEnable、initDone、mutantMode，均应为零；MuTaNT 的 run 应为零。
5. 结果写入新的独立停止审计。仍保持 Failed/UNKNOWN；不会恢复 Ready，也不会宣称全量配置、全局控制权或整批数据完整。

任何身份、原结果或读回不符都会保持核对失败门槛。即使之后数值恢复，也不会改用普通负向 Stop 重发来绕过这次失败。

停止核对成功后，才继续显式提交 `abort-failed-receivers`。远程管理器保留已经正常封存的节点，只清理失败节点。仍使用原实例、会话、PID和输出目录；生产 raw 不删除，整个 Run 仍不完整。

实际本机两编码 ECC→两数据服务→receiver 链路及读回失败拒绝已经通过；包含 MuTaNT 的底层回归与插桩范围以最新[验收记录](VALIDATION.md)为准。本功能是失败后的清理接续，完整 UNKNOWN 恢复和冷重启接管仍未实现。

## 3. 多数据节点怎样绑定原会话

远程管理器保留每个数据节点的原实例、计划、控制者 scope/epoch，以及成功 Start 时收到的 source、PID、session、output_directory。

清理前对所有节点进行身份和状态查询，并比较上述原记录。不能采用查询中出现的新会话或新 PID；任何一个节点不匹配时，在首次清理 POST 前拒绝。

然后保存整组请求意图，逐节点最多一次 POST，并按原操作 ID GET 观察。单节点失败仍记录其他独立节点结果；父审计失效会停止后续动作。不自动回滚、接管或重新 POST。

已经在原会话中确认正常封存的节点保留原 seal，记录 `node_already_sealed`，不再发送终止请求。其他节点清理后必须仍是原 PID/会话/目录，全部 reaped，生命周期 aborted 并保持待核对；再查询的新鲜状态也要一致。

## 4. 为什么有时需要终止仍健康的兄弟节点

全系统中某个数据节点失败后，另一节点的接收器可能仍正常等待发送器。只允许“本节点已有异常退出”会让这些兄弟进程一直留下。

ECC 在核对原停生产审计后，将原失败操作 ID 作为 `parent_failure_operation_id` 传给同一控制者原会话的请求。数据服务允许原所有者明确声明全系统失败，以终止这个尚健康的兄弟会话；原有没有该字段的请求继续要求本节点确实异常或 Failed。

该字段是受认证的原所有者声明。数据服务记录 `parent_failure_independently_verified:false`、`hardware_stopped_verified:false`，不会声称自己独立读取并确认了远端硬件。ECC 入口负责核对原停生产审计；直接使用数据服务 CLI 时，应理解此声明的边界，不把引用字符串当硬件证明。

其他 supervisor 命令不得携带该字段。声明进入规范请求和 request_key；同 ID 换引用不能重用原操作。原会话、实例、计划和控制者 scope/epoch 检查仍保持。

## 5. 结果、失败和残留文件

服务或 ECC 操作的 `phase:complete` 表示这次清理操作结束。父 ECC 仍 UNKNOWN，数据管理器保持待核对，采集不完整。它不会调用普通 Breakup、断电、配置重新加载、发送器 End 或正常封存。

接收器用 SIGTERM 退出，没有产品 SIGKILL 回退。超时、部分回收或审计故障保持不确定结果，不以新 ID 自动重放。检查原操作、每节点请求和结果、仍存活的进程及残留目录，继续恢复核对。

原始 .graw 不删除。先前已经确认的单个来源 seal 可以保留；它不能证明整个系统完整。未封存来源计数仍是此前确认缓存，实际残留数据需要单独检查。测试夹具最后删除合成 .graw，与产品保留生产数据是不同的行为。

## 6. 结构与实际验证范围

- `NativeEccCoordinator`：绑定原停生产审计和原采集对象，创建独立负向清理日志；不调用 Reconcile。
- `RemoteReceiverManager` / `ReceiverProcessManager`：只传元数据，保持原会话和不确定结果；原始帧不进入 Qt 或 ECC。
- `EccControlJobs`：固定协议命令、同一原客户端 epoch、一次接受与原 ID 查询，不把清理加入自动停采策略。
- `EccSystemClient` / `NativeRunControl`：新鲜原 UNKNOWN 状态、一次提交、保存清理结果和未完成 Run。
- `MainWindow`：健康状态禁用、明确确认/取消入口，沿用 MUSIC 风格信号/槽。

本机生产 `MainWindow`→工作线程 `RunControl`→独立 ECC→两个数据服务→原生 receiver→模拟板端的链路已在两种 Ice 编码下通过。夹具点击实际窗口按钮进入 Running，再使原接收器异常退出；点击真实确认框的取消按钮时不提交清理，点击确认按钮时只接受一次清理。Run journal 保存清理结果，仍为 Interrupted、duration_incomplete，原 Run 编号不增加、不生成完成 CSV，ECC 保持 UNKNOWN。

另外单独验证了远程管理器的已封存节点跳过：一个来源因截断帧而 Finish 失败，另一个来源正常封存；清理只向失败节点发一次请求，已封存节点零次请求，seal 和原始字节保持一致，重试被拒绝。这项证明直接调用管理器，不经过 ECC 清理入口，也不证明停生产成功；尤其不能将它解释为 Breakup 后的完整 ECC 恢复。

以上窗口验证使用 Linux/Qt 5.12.8 offscreen 模式，运行生产窗口类，只有测试驱动负责自动点击；并非 Windows/现场人工验收。详见 [新增全链路验收](VALIDATION.md#7021-生产窗口清理全链路与已封存节点保留2026-10-12)及[此前核心与拒绝验证](VALIDATION.md#7020-ecc-与操作台失败接收器清理2026-10-12)。这些证明不覆盖所有 Run 故障持久性、物理多主机、ICE 原始接收、冷重启接管、断电或完整硬件 UNKNOWN 恢复。

底层数据服务字段和 CLI 操作见 [失败接收会话受控终止](FAILED_RECEIVER_SESSION_ABORT.md)，停生产依据与缓存见 [接收故障停生产](RECEIVER_FAILURE_STOP_POLICY.md)，本地 Run 关系恢复见 [ECC GUI 手册](ECC_GUI_CLIENT.md)。
