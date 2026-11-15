# 运行故障后关闭原发送器连接

## 1. 目的和当前状态

`breakup-after-failure` 是完整Run故障恢复中的一个明确收尾步骤，普通及ASan/UBSan/LSan本机核心/实际网络故障验证已通过；完整Running主窗口两编码也已验证，包内Release完整窗口两编码已验，最终范围以验证记录为准。它解决“生产已停止，但原发送器线程/连接仍保留”的问题。真正停止和断开在原ECC主机由原采集对象执行；Qt只发送小型控制请求，没有接收或转发原始事件。

本命令成功仍保持父ECC UNKNOWN、原失败ID及未完成Run。它不调用receiver的Finish/Abort，不伪造数据封存，也不清除数据侧待核对。只有后续硬件与数据的独立核对流程完成，才可能再次开始采集；当前不要把此入口当完整恢复按钮。

## 2. 应该怎样安排顺序

正常结束继续使用Stop/Breakup，不使用本入口。故障后应当保留同一服务实例、配置、审计根、原对象和原请求ID，并按下列顺序判断：

1. 检查故障原因和原Run，保留原始文件。
2. 明确停生产并等待原对象停止请求complete。仅某个生产位为0不代替完整原停止记录。
3. 若全部原发送器结果仍已知，明确关闭它们的线程和连接；本入口只接续这一步。
4. 数据接收器可能因真正EOF自行退出/封存，但仍须数据服务核对；不能由关闭连接的ACK代替。
5. 对失败数据会话另行明确清理，并保留正常封存兄弟节点。数据不完整仍要标为不完整。
6. 只有独立数据/硬件恢复凭据满足前提时，才解除对应门槛。Run完成、编号推进和CSV另有条件。

已有接收器异常退出时，发送器停止/刷新也可能失败。出现未知结果后不要换ID重发；本入口会保留故障，不能用新对象猜测实际旧连接。完整冷服务接管不在本入口范围。

## 3. 操作台按钮

在连接原ECC的Windows或Linux操作台选择 **Close original failed acquisition connections...**。窗口应显示原UNKNOWN，当前没有待核对请求或正在执行的命令。默认Cancel不发请求；明确Yes才保存本地意图并提交。

接口名为 `breakup-after-failure`，认证POST使用既有实例、快照、客户端/持有期和新的操作ID，不携带failed_operation_id。后端自己绑定当前原失败采集操作及已确认停生产ID。HTTP202只表示接受记录，仍需GET原ID查看终态。

成功返回complete时父状态和原ID仍未知；有本地Run记录时保存native_failure_breakup_operation，duration_incomplete保持true，不写正常完成CSV，不增加Run编号。等待期间状态查询仍可用，其他控制请求不排队。

## 4. 后端检查和执行

前置要求包括同一失败采集对象、同一Configured准备对象，或已经确认Broken且同一Prepared对象；全部原生产停止步骤必须有确认结果。全部CoBo名称/描述及firmwareEnable/intEnable，全部MuTaNT run都重新核对。

发送器只允许本对象中的Fresh、Connected、Running、Stopped或Disconnected，且outcomeUnknown=false。Unknown、正在转换、陌生实例或接口变化拒绝；不重新发现或接管连接。Fresh表示本对象未创建连接，不能推广为全局远端停止。

| 原发送器状态 | 动作 |
| --- | --- |
| Fresh | 不发送Stop或Disconnect |
| Disconnected | 不重复Disconnect |
| Connected | 直接明确Disconnect，没有启动过DAQ的本对象不补发Stop |
| Running | 先明确Stop，确认后再Disconnect |
| Stopped | 不重复Stop，只明确Disconnect |
| Unknown/转换中 | 全目标预检查拒绝 |

某一目标Stop失败不向它继续Disconnect；其他独立目标仍尝试各自Stop/Disconnect。只有全部确认后才撤销全部CoBo initDone/mutantMode，准备对象回Prepared、采集对象为Broken。电源、旧自有告警及对齐快照保留。

如果原Breakup早已完成、只因receiver封存失败而父UNKNOWN，原停止核对已确认且发送器Fresh/Disconnected，则本入口只复查原停止结果和只读状态，不重发生产停止、断开或配置撤销。

## 5. 证明与失败处理

每个已确认Stop/Disconnect返回实际命令审计写入链；配置撤销和只读状态核对也保存各自写入链。原生ECC对全部文件身份、权限、字节及实际链进行绑定，父收尾审计引用原失败ID/原停生产ID，并明确receiver_lifecycle_changed=false、receiver_sealed=false、run_complete=false。

这里“接收生命周期未改”是没有向数据服务发送Finish/Abort等请求；实际TCP断开可以使receiver自然收到EOF。不能因此说receiver仍一定运行，也不能把尚未核对的退出说成已经由ECC确认封存。

| 失败 | 保持的状态 |
| --- | --- |
| 停生产缺少完整确认 | 不关闭连接 |
| 原发送器未知 | 不执行新Stop/Disconnect |
| Stop或Disconnect已应用但ACK丢失 | 未确认，不重发；独立兄弟目标继续收尾 |
| 一目标收尾未确认 | 不撤销就绪配置，父UNKNOWN保持 |
| 配置撤销未确认 | 保留原对象/审计，不能换ID重复 |
| 审计失败 | 不执行无法可靠记录的后续动作 |

原对象与外层ECC都有一次尝试门槛。相同请求ID仅观察原结果，新ID也不能重放本次已尝试的连接收尾。最终验证、实际receiver范围和交付情况以VALIDATION.md为准。


## 6. 实际窗口怎样保留未完成Run

本机模拟验证从真实MainWindow依次Progress到Ready，再Start进入Running，随后自有测试receiver异常退出。原ECC自动停生产完成后，窗口先对连接收尾Cancel（零请求）再Yes（一次），等待native_failure_breakup_operation保存complete；之后对接收器清理也Cancel/Yes。

最终原实例、原Run、原会话/PID及目录保持；receiver均回收但不被当成ECC已经确认的完整采集，UNKNOWN及数据待核对保持。duration_incomplete=true，RunNumber不递增，run.csv不存在，journal不是Complete。这里有真实工作线程、ECC、两个数据服务和原生receiver；板端仍是回环夹具，不代表现场/Windows人工操作。

这项联调还暴露并修复了本地准入问题：先前存储在故障门槛下允许连接收尾，却漏了后续abort-failed-receivers。因此接收器清理被本地拒绝，未向服务发送请求。修复只补明确负向准入；Start/Configure仍拒绝，重开记录后门槛仍保持。测试失败记录保留，不能计通过。

```mermaid
flowchart LR
    F[原Run故障] --> P[确认原生产停止]
    P --> C[确认原已知发送器Stop和Disconnect]
    C --> H[撤销就绪配置，父UNKNOWN保持]
    H --> D[独立核对EOF、接收退出和数据结果]
    D --> A[必要时明确清理失败接收会话]
    A --> R[后续数据与硬件恢复仍待核对]
```

控制ACK、接收EOF、封存证明、Run完成是不同证据。配置重新加载、告警解除、数据会话退休/重建和冷服务接管不能由本收尾结果替代，完整恢复仍继续实现。
