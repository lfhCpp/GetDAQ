# 冷数据交接：恢复服务和操作窗口

本功能将[冷数据交接CLI](COLD_DATA_HANDOFF_COMPLETION.md)连接到操作台。窗口可以运行在Windows；恢复服务在原ECC主机上，以原ECC使用的同一账户运行。它处理原数据节点的交接，不恢复旧硬件对象，不解除旧ECC的禁用，不恢复中断的Run。

“冷”指原ECC进程已经退出。原ECC退出后，原来的HTTP端口也随之关闭，因此窗口使用一个独立的`getdaq-handoff-service`，不能继续把冷恢复命令发给已经退出的ECC。

原始采集数据仍由sender、原生receiver及磁盘处理。窗口和恢复服务只传有界JSON意图、状态及回执，不读取或转发`.graw`。恢复服务以准确参数直接启动原生CLI，不经过shell，也不允许客户端提供命令、可执行文件或任意服务器路径。

## 1. 各程序负责什么

| 程序 | 职责 | 退出后的影响 |
| --- | --- | --- |
| 原ECC | 保存原控制、网络父操作及正常周期证据 | 必须退出后才能取得原生冷交接独占只读锁 |
| `getdaq-handoff-service` | 认证、固定配置、持久任务、调用原生CLI、查询结果 | 正常退出等待当前子任务；异常退出后不自动重放 |
| `getdaq-ecc-handoff` | 核对原父记录/节点/周期，按原ID明确补齐或只读重导出 | 其后端审计决定能否证明完成；普通进程退出码不能取代回执 |
| `Cold data handoff recovery...`窗口 | 审阅身份、保存本地意图、确认提交、按原任务ID查询和导出 | 关窗只停止本地查询，不取消服务器任务 |
| 下一代ECC/节点服务 | 按原接班计划和回执明确启动新周期 | 不由本窗口自动启动 |

恢复服务不会替代原CLI的检查。原ECC仍活着、原生UNKNOWN、当前节点换实例/换周期、工件变化、接班身份已经消费等情况，仍由原生入口拒绝。恢复服务身份能够读取，只证明该服务的固定部署；不证明原任务当前可以补齐。

## 2. 准备原文件和账户

先完成[CLI手册的准备步骤](COLD_DATA_HANDOFF_COMPLETION.md#3-提交前准备哪些文件)，保留：

- 准确原ECC配置及其全部引用文件、认证文件和原审计目录。
- 已由原请求冻结的新ECC配置、`successor.json`及准确摘要。
- 原意图目录中的`request.json`和`original-status.json`。不能用失败后的状态替代原正常前状态。
- 实际安装的`getdaq-ecc-handoff`及其SHA-256。

恢复服务、CLI和旧审计使用同一主机账户。新建独立的恢复任务根，位于原/新ECC控制根之外，且不与原始数据目录混用。目录使用0700，服务配置和原请求/状态使用0600，冻结的接班计划仍使用0400。程序文件属于该账户，有执行权限，不能被组或其他用户写入。

下面路径只是示例；替换成实际部署路径，不把示例文件直接当现场配置：

```bash
mkdir -p /home/daq/recovery/auth /home/daq/recovery/tasks/retirement-001
chmod 700 /home/daq/recovery /home/daq/recovery/auth /home/daq/recovery/tasks /home/daq/recovery/tasks/retirement-001
/home/daq/getdaq/bin/getdaq-ecc-server --generate-token /home/daq/recovery/auth/recovery-token
sha256sum /home/daq/getdaq/bin/getdaq-ecc-handoff
```

令牌生成命令只显示成功信息，不输出令牌值。恢复服务的令牌与ECC和节点令牌分开。将令牌文件安全放到操作台主机，窗口填写本地文件路径；不要把令牌值放到命令行或JSON示例中。

## 3. 编写固定恢复配置

复制并编辑模板：源码目录中为`config/recovery/cold-handoff-service.example.json`，运行包中为`share/config/recovery/cold-handoff-service.example.json`。模板的`executable_sha256`是占位文本，必须替换为上一节实际输出的64位小写摘要。

| 字段 | 填什么 |
| --- | --- |
| `service_id` | 该原交接的固定恢复服务名称，窗口用同一名称核对 |
| `bind`、`port` | 恢复服务监听地址和独立端口；与原ECC端口分开 |
| `credential_file` | 原ECC主机上恢复令牌的绝对路径 |
| `executable`、`executable_sha256` | 准确原生CLI路径及摘要 |
| `old_config` | 原ECC固定配置绝对路径 |
| `new_config` | 原请求已经绑定的新ECC配置绝对路径 |
| `successor_plan` | 原请求已经绑定的接班身份文件 |
| `original_evidence` | 保存原`request.json`和`original-status.json`的目录 |
| `audit_root` | 已存在、规范绝对路径的私有任务根；首次使用应为空 |

配置保存为0600，例如`/home/daq/recovery/retirement-001.json`。一个部署绑定一套原交接输入，不是全目录扫描器。服务第一次启动将准确配置摘要和输入摘要写成不可变`policy.json`。之后修改配置、替换CLI或更换原交接文件，即使仍使用相同`service_id`，也不能继续复用这份任务根。

模板监听127.0.0.1，只能从原ECC主机直接访问。Windows操作台需要使用可到达的部署地址或已经配置好的转发端口；HTTP认证不会自动提供网络加密，网络访问策略沿用[控制协议](PROTOCOLS.md)的部署要求。

## 4. 启动恢复服务

```bash
/home/daq/getdaq/bin/getdaq-handoff-service \
  --config /home/daq/recovery/retirement-001.json
```

服务检查固定文件、持有任务根独占租约，再监听配置端口。第二个相同任务根的恢复服务会拒绝启动。

服务启动不创建原ECC控制对象、不发送硬件命令，也不启动receiver。原ECC还未退出时可以读取恢复身份，但提交补齐仍会在原生锁边界失败。建议先按正常关闭流程退出原ECC，再明确提交。

Ctrl+C/SIGTERM请求关闭恢复服务。若当前已有原生子任务，服务等待它结束后退出；它没有“强行取消数据交接”接口。不要把关闭窗口当成取消服务器任务。

## 5. 在操作台审阅并提交一次

打开GetDAQ菜单`Cold data handoff recovery...`：

1. 填写恢复主机、恢复端口、操作台本地令牌文件和准确`Expected recovery service ID`。
2. 点击`Check fixed recovery identity`。查看原操作ID、原请求SHA-256、接班计划SHA-256及服务policy摘要，确认是要处理的那次原交接。
3. 点击`Confirm one cold data completion...`。确认框再次显示原操作和摘要。取消不会创建任务ID或提交请求。
4. 确认后，窗口先把准确JSON意图保存到本机应用数据目录的`cold-handoff-intents/TASK_ID.json`，再POST一次。状态栏显示保存位置。
5. 观察准确同一任务ID。正在执行时仅周期GET，不重新POST。成功显示`complete`后，可点击`Export checked receipt...`导出同字节回执。

窗口不会把启动下一代ECC、切换节点服务或清除旧失败隐含在这个按钮里。随后步骤仍按[CLI手册的接班流程](COLD_DATA_HANDOFF_COMPLETION.md#9-用回执启动原先确定的新服务)明确执行。

## 6. 回复丢失、关窗和恢复观察

提交超时可能发生在服务已经接收之后。保留本地JSON意图和任务ID，不反复点击补齐。

重新打开窗口并填写同一恢复服务，使用`Load saved intent...`载入原JSON，点击身份检查后按`Observe original task`。也可以在未加载意图时填写原任务ID；返回结果必须仍属于核对过的固定policy。载入和观察都不POST。

服务仍记录原任务时，即使服务实例ID因重启改变，也可读取原任务。原请求的重复POST在准确请求键相同的情况下只返回原记录；相同任务ID对应不同意图则409拒绝。新任务必须绑定当前服务实例，旧实例身份不能创建新任务。

GET返回404只表示该服务没有找到记录，不能证明请求从未执行；窗口不会自动重发。

## 7. 只读检查原尝试及重导出回执

在`Original cold attempt ID`填写原提交的attempt ID。通常提交任务ID与attempt ID相同。点击`Read-only inspect original attempt...`并确认：

- 服务保存一个新的inspect任务意图，调用准确原`cold-complete-inspect`。
- CLI不POST节点、不查询板卡，只从本次持久审计核对完整回执。
- 服务任务GET只是读取缓存状态，本身不启动inspect。只有明确确认inspect才启动只读工具。
- 原外部`receipt.json`丢失时，可从后端完整审计同字节重导出；窗口再导出已核对的回执。
- 未完成尝试仍然失败，不会借inspect补齐。

本入口仍要求准确原配置和认证文件，且接班身份尚未激活。无令牌历史审阅请使用[review-cold](COLD_HANDOFF_REVIEW.md)；那种报告不是启动凭据。

## 8. 服务异常退出后怎样处理

恢复服务重开时，已持久完成的任务保留原结果。原`accepted`或`running`任务显示`unknown`，不收养PID、不终止旧子进程、不自动重放。

一个原提交仍为unknown时，服务拒绝新submit。先判断原子进程是否还活着以及原生审计是否完整；不要删除目录或启动第二个补齐绕过。

如果原生任务实际上已经完成，可明确inspect准确原attempt。inspect成功提供独立完成证据，允许后续明确新意图；原unknown任务仍保留unknown，不改写原事实。如果原生任务没有完成，inspect保持失败，不能当成已恢复。当前没有对这种任意中断的自动续跑功能。

## 9. 服务保存哪些文件

| 相对任务根路径 | 用途 |
| --- | --- |
| `policy.json` | 固定配置和准确输入摘要；不同部署不能复用 |
| `service.lock` | 活服务独占租约，不包含令牌 |
| `job-TASK_ID/request.json` | 先于子进程持久保存的准确请求 |
| `job-TASK_ID/operation.json` | accepted/running/complete/failed等任务记录；中断重开派生unknown |
| `job-TASK_ID/diagnostic.log` | 最多64KiB子任务诊断，不是raw数据或完成凭据 |
| `attempt-ATTEMPT_ID/` | 原生CLI独立证据目录，包括准确回执及失败保持 |

回执上限与原CLI一致，为1MiB。回执以JSON字符串传到窗口时需要转义，因此该窗口的HTTP元数据响应采用独立8MiB预算；其他控制请求默认仍为1MiB，任务请求仍受短JSON上限约束。这些预算不是原始数据缓冲。

同一服务最多保留256个任务，一次只执行一个子任务，不积压任务队列。达到历史上限时拒绝新任务；保留原目录并按实际交接划分后续部署，不能删未确定任务来腾出自动重试机会。

## 10. 协议与代码位置

认证后的元数据接口：

| 方法/路径 | 行为 |
| --- | --- |
| `GET /v1/cold-handoff/identity` | 固定服务/实例/policy及原请求、接班计划摘要 |
| `POST /v1/cold-handoff/tasks` | submit或inspect的准确意图，一次子进程调用 |
| `GET /v1/cold-handoff/tasks/TASK_ID` | 原任务状态和已核对回执，不执行工具 |

请求只有schema、kind、task_id、instance_id、policy_sha256、action和attempt_id。`submit`要求task_id等于attempt_id；`inspect`必须引用此服务历史里的原submit。没有shell文本、服务器路径或板端命令字段。错误/重复键/越界输入均拒绝。

实现位于`apps/ecc/recovery`、`common/control/coldhandoffprotocol.*`、`apps/envoy/coldhandoffwindow.h/.cpp/.ui`。界面遵循现有MUSIC式窗口组织，C++11；新增服务不进入receiver构建或采集热路径。

实际验证范围与剩余事项以[验证记录](VALIDATION.md)和[当前清单](REFACTOR_STATUS.md)为准。本功能不能替代真机、目标平台和持续吞吐验收。
