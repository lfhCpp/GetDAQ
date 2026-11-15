# 已失败接收会话的受控终止

当一台数据主机上的接收器已经失败，而其他接收器仍在运行时，需要明确终止同一原会话中的剩余进程，才能进行后续数据检查和恢复。本功能新增数据管理命令 `abort-failed-session`，只处理该管理服务在本次进程中亲自启动、仍保留归属信息的接收器。

它是负向清理动作。完成后状态仍是 `aborted` / `needs_reconciliation:true`，不会显示采集完成、不会重新启动接收器、不会解除 ECC 的 UNKNOWN。原始数据文件保留，供人工检查；它不参与帧接收、写盘或在线导出。

## 1. 三个动作不能混用

| 动作 | 什么时候用 | 结果含义 |
| --- | --- | --- |
| `finish` | 正常停生产后，等待发送器真实 EOF、数据排空与封存 | 成功时数据生命周期 stopped，具体接收器有封存凭据 |
| `abort-startup` | 接收器启动失败，并且还没有连接或数据 | 明确终止空启动；仍需核对，不代表采集完成 |
| `abort-failed-session` | 原会话已有接收器异常退出，或本生命周期已经失败，剩余进程需要受控终止 | 回收自有接收进程，保留未完成数据与故障门槛 |

正常 EOF 退出不是异常退出。本命令拒绝仅有正常退出记录的健康会话；不能借它跳过正常 Finish。硬件停生产仍由原 ECC 执行，接收器退出不能证明板卡、发送器、中断线程或残余缓冲已经停止。

## 2. 使用顺序

1. 在原 ECC 上观察失败操作和原数据会话，按原流程明确停生产。保留停生产结果；失败时继续保留 UNKNOWN，不据接收器退出清除硬件故障。
2. 从原数据管理服务读取 `/v1/identity`、`/v1/status` 和 `/v1/receivers`，核对服务实例、计划、控制者、控制者快照/epoch 及原会话 ID。
3. 为这次受控终止选择一个新的操作 ID，生成请求文件。`expected_session_id` 必须来自原会话记录，不能替换成刚发现的其他会话来接管。
4. 用原认证文件向同一服务提交一次请求。202 只表示接受，继续按原操作 ID 查询结果。
5. 结果为 complete 时，检查 result 中每个接收器的原 PID、reaped、exit_code、seal_verified，以及 `aborted` / `needs_reconciliation`。这里的 complete 是**终止操作完成**，不是采集数据完整。
6. 核对残留数据与日志，处理未完成 Run。当前仍不能以此动作直接 Reset、重新 Start 或恢复硬件 UNKNOWN。

数据管理核心、认证控制服务和原生查询 CLI 已接入。主机端 ECC/Qt 现新增 [原失败接收器清理入口](ECC_FAILED_RECEIVER_CLEANUP.md)，要求同一保留采集对象及完整停生产审计；全系统 UNKNOWN 恢复仍未完成，不把 GUI 的 Run Recover 当成此命令。

## 3. 请求字段怎样填写

请求保持 supervisor schema 1，只有新命令增加必需的 `expected_session_id`。所有原有请求的规范编码保持原样，其他命令出现该字段会被拒绝。

会话不匹配在接受操作之前返回前提错误，不产生该次操作记录或发送信号；不会因为误填会话 ID 就把健康会话升级为新的失败操作。真正的正常 Finish 失败后，服务保持故障门槛，仍允许原所有者按正确原会话执行这个负向动作。

```json
{
  "schema": 1,
  "operation_id": "abort-failed-001",
  "command": "abort-failed-session",
  "plan_sha256": "从原identity复制的64位小写十六进制摘要",
  "instance_id": "从原identity复制的64位小写十六进制实例ID",
  "controller_id": "原配置的控制者ID",
  "controller_snapshot_sha256": "原status中控制者快照摘要",
  "controller_epoch": "原status中控制者epoch",
  "configuration_sha256": "原计划的配置摘要",
  "artifact_sha256": "原计划的工件摘要",
  "parent_audit_reference": "原ECC失败及停生产操作的审计引用",
  "expected_session_id": "原失败接收会话的64位小写十六进制ID"
}
```

这段 JSON 用中文说明待替换字段，**不能直接提交**。每个摘要/实例/会话字段必须换成实际原记录的 64 个小写十六进制字符。parent_audit_reference 是追溯引用，数据服务不会据它声称已经独立核对硬件停止。

例如在 Linux 数据管理终端中：

```sh
getdaq-supervisor-query status \
  --host 192.168.0.10 --port 8085 \
  --credential-file /path/to/original-credential \
  --expected-agent original-agent --controller original-controller \
  --expected-plan ORIGINAL_PLAN_SHA256

getdaq-supervisor-query submit \
  --host 192.168.0.10 --port 8085 \
  --credential-file /path/to/original-credential \
  --expected-agent original-agent --controller original-controller \
  --expected-plan ORIGINAL_PLAN_SHA256 \
  --request-file /path/to/abort-failed-request.json

getdaq-supervisor-query operation \
  --host 192.168.0.10 --port 8085 \
  --credential-file /path/to/original-credential \
  --expected-agent original-agent --controller original-controller \
  --expected-plan ORIGINAL_PLAN_SHA256 \
  --operation-id abort-failed-001
```

示例地址、端口、路径、ID 和摘要均需替换。凭据只从 0600 文件读取，不放进 JSON、命令行明文或日志。CLI 先核对服务/计划，POST 只发一次；同 ID 同请求返回原操作记录，同 ID 不同会话产生不同请求摘要并被拒绝。

## 4. 如何防止误停其他进程

数据管理核心仍持有原 posix_spawn 子进程记录，按 source ID、会话、PID 和输出目录绑定核对。它用 `waitid(...WNOWAIT)` 检查仍是自己可以观察的子进程；没有重新按 PID 查找并认领其他进程。

存活接收器还必须持有原输出目录锁。所有来源先检查，再写入持久终止意图。每个 SIGTERM 前记录原来源/PID，不发送 Finish、FDT 正常结束字节或 ICE source End。先向仍存活的自有接收器发 SIGTERM，再按受控 deadline 等待退出并回收。

核心和服务会拒绝以下情形：

- 未有控制者归属、控制者快照/epoch 改变、服务实例或计划不匹配。
- 请求会话不是本对象原会话；启动没有完成；管理服务重启后没有原子进程对象。
- 会话仍健康，或者只观察到正常退出；没有异常/失败依据。
- 进程归属不可观察，或输出目录/锁身份变化。
- 存储、计划、部署锁或审计完整性已经出问题。
- 本会话已经尝试过这次终止；部分失败和超时不会重放信号。

该接口只证明接收进程归属和清理结果，不能证明板端全局控制权，也不能让另一个 ECC/控制者接管。它不处理服务重启后的孤儿进程；那需要独立核对与恢复方案。

ECC 确认原停生产后可附带 `parent_failure_operation_id`，由原认证所有者声明全系统故障，清理仍健康的兄弟节点。引用必须非空合法操作 ID，并进入 request_key；它不改变原会话和控制者校验。数据服务记录父故障未独立验证、硬件停止未验证；声明的含义及 ECC 审计门槛见上述接续手册。

## 5. 失败、超时和残留数据

SIGTERM 可能使接收器保留已写入的帧，但不能提供发送器真实 EOF。命令不会删除或重写 .graw，不验证整套数据完整性，不创建假的封存凭据。状态计数仍是此前确认的生命周期缓存；没有完成封存时，不能用它代替实际残留文件检查。

若某个接收器没有在计划的 finish_timeout_ms 内退出，命令报告失败，保持 aborted/待核对。产品代码没有 SIGKILL 回退、信号重发、自动清理或自动重启。检查原操作日志、已回收/尚存活的进程及原数据目录，继续人工核对。

如果已成功回收部分进程后审计失败，结果也不能提升为完成。原故障、操作意图和数据保留；不要换一个操作 ID 自动重新杀进程。正常采集之外，需要另行处理硬件、源端残留缓冲、未完成 Run 和恢复门槛。

## 6. 软件结构与验证边界

- `DataSupervisor::AbortFailedSession`：C++11/POSIX 核心，直接管理原子进程，不链接 Qt/Ice。
- `SupervisorControlRequest`：新命令的会话字段进入规范 JSON 和 request_key。
- `SupervisorControl`：沿用认证、实例/计划/控制者绑定、一次接受和持久操作记录；允许已有待核对状态下的这个负向动作，保持其他正向动作禁止。
- `getdaq-supervisor-query`：无 Qt 的低频 CLI，提交与查询原操作。
- 可选 Qt supervisor 服务只处理控制元数据，原始帧始终进入独立 receiver。

本机验证使用实际 TCP/FDT/ZBUF 接收进程、合成小帧及独立服务/生产 CLI，覆盖错误控制者/会话、健康会话拒绝、故障后原 PID 回收、原帧保留、未伪造封存、同 ID 无信号重放以及服务重启不接管。测试之后删除本轮自有 .graw 和临时凭据，仅留小型日志与摘要。

实际 ICE 失败会话终止、物理多数据主机、卡死接收器、真实 EIO/断电及全 ECC UNKNOWN 恢复仍需继续验证和接续。已有 [接收故障停生产](RECEIVER_FAILURE_STOP_POLICY.md) 与本步骤不同；不得把任何一步的成功扩大为整套故障恢复已完成。当前证据见 [验收记录](VALIDATION.md#7019-已失败接收会话受控终止2026-10-12)。
