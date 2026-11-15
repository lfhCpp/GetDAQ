# 数据服务更换接收计划：操作、文件与原理

本页描述70.54数据节点的受控接口。父ECC编排与Qt审阅窗口已分别由[ECC计划切换](ECC_PLAN_TRANSITION.md)和[窗口操作说明](ECC_PLAN_TRANSITION_GUI.md)接入。它允许已正常结束采集的数据服务，把新的接收参数和指定控制身份交给新的数据服务进程；服务更替仍需显式停止旧服务并启动新服务。连续板卡占用移交、真实硬件参数加载及目标平台/现场验收仍待。`getdaq-ecc-handoff submit/inspect` 保持同计划语义，不同计划使用 `retire-submit/retire-inspect`。

## 1. 为什么需要另一个入口

原来的 `handoff-control` 保留接收计划，只换控制者的范围和持有期。计划中的队列、最大帧、文件轮转或配置摘要改变后，原计划摘要也随之改变，因此不能借原命令继续使用。

新的流程为：

```text
原采集正常Finish，全部receiver退出并封存
  → 离线生成并审阅下一计划
  → 原控制者提交retire-plan，原服务永久保持
  → 取得并核对原操作完成及退休证明
  → 正常关闭原数据服务
  → 新服务以新配置和原证明启动一次
  → 核对新身份、新控制归属和前驱证明
  → 由获得授权的新控制者明确启动新的采集周期
```

这是更换进程及计划，不是让运行中的接收器动态改变参数。它不会搬运旧数据，也不重新读取事件内容。板端 `getHwServer` 和 `IceEirfu` 保持现有部署。

## 2. 使用前必须满足什么条件

1. 原数据服务仍是原来运行的实例；控制者名称、范围、持有期及请求中的原实例和原计划摘要均匹配。
2. 原会话为正常Stopped，全部接收器已回收、退出码为0、正常封存，存储及控制审计健康。Fresh、正在采集、故障、Abort或仅退休的失败会话不满足条件。
3. 新计划使用新的 `deployment_id` 和不存在的 `audit_root`；新旧审计根不能互相包含。新审计根的父目录已存在、为规范路径且可写，输出根已存在且权限为0700。
4. 继续使用已核对的receiver可执行文件及其摘要；不能通过切换计划替换程序或引入新的ICE可执行程序。升级程序是另一项部署操作。
5. 新计划的总队列和总最大帧内存上限不超过原主机策略。单源参数可以在该预算和已有校验范围内变化。
6. 新的控制范围与持有期分别不同于原值，均来自将要接班的实际控制对象。不能手填随机摘要或复制文档中的示意值。
7. 先完成原服务退休，核对持久完成记录，再关闭原服务。不能直接终止一个仍在采集的服务后尝试接管。

认证文件、配置及审阅文件只能由实验运行账户管理。这里没有超时抢占或自动故障恢复。

## 3. 先认识四类摘要

| 字段 | 绑定的内容 | 不应怎样理解 |
| --- | --- | --- |
| `plan_sha256` | 原数据节点规范接收计划 | 不是整套ECC快照摘要 |
| `successor_plan_sha256` | 离线生成的新规范计划文件的准确字节 | 不能对另一次格式化后的JSON计算并替换 |
| `controller_snapshot_sha256` | 当前控制对象的范围 | 不是可以从配置摘要直接复制的值 |
| `successor_controller_epoch` | 下一实际控制对象的一次持有期 | 不是可在后续启动中反复复用的通用密码 |

`configuration_sha256` 和 `artifact_sha256` 也继续参与计划绑定。摘要变化证明所引用内容不同；它本身不证明新配置已经加载进板卡。

## 4. 生成下一计划：不启动接收器

先在受控目录准备下一份数据服务配置。配置结构沿用原数据服务配置，新接收参数位于 `receiver_plan`。服务监听地址、认证等仍属于服务配置。新配置在此阶段只用于检查和计算。

以下路径均为示例，执行前替换。`next-service.json` 必须是已准备好的真实配置，不能把原审计目录改名当成新的审计根。

```bash
install -d -m 700 /home/daq/reviewed/next-data
bin/getdaq-supervisor --config /home/daq/reviewed/next-data/next-service.json --check-config
bin/getdaq-supervisor --config /home/daq/reviewed/next-data/next-service.json --plan-config
bin/getdaq-supervisor --config /home/daq/reviewed/next-data/next-service.json \
  --write-plan /home/daq/reviewed/next-data/next-plan.json
```

三种模式的用途不同：

- `--check-config` 检查配置；不监听、不创建数据部署根、不连接板卡。
- `--plan-config` 输出规范计划的准确字节，便于离线比较。
- `--write-plan` 独占新建0400计划文件，刷新文件与父目录，输出包含文件路径和 `plan_sha256` 的JSON摘要。目标已经存在会拒绝覆盖。

三种模式互斥。审阅目录须为0700，计划文件须为0400。保留生成时的准确字节和摘要；不要编辑已审阅计划。若需要修改，应重新审阅，并在原服务尚未受理退休前准备另一份计划。

这些命令不会提前取得原部署或板卡的控制权，也不会创建新receiver。原服务受理时仍会重新检查目标文件、程序、路径和内存策略。

## 5. 由原控制者提交退休请求

完整请求应由控制软件从实际对象生成。下面只列新增字段，**不是可独立提交的完整JSON**：

```json
{
  "command": "retire-plan",
  "expected_session_id": "原正常Stopped会话的真实标识",
  "successor_plan_file": "/home/daq/reviewed/next-data/next-plan.json",
  "successor_plan_sha256": "离线计划生成命令返回的64位小写十六进制摘要",
  "successor_controller_snapshot_sha256": "下一实际控制对象的范围摘要",
  "successor_controller_epoch": "下一实际控制对象的持有期"
}
```

完整请求还包含 `schema`、新的 `operation_id`、原 `plan_sha256`、原 `instance_id`、原 `controller_id`、原控制范围/持有期、原配置/工件摘要及 `parent_audit_reference`。请求中的配置/工件字段绑定原计划，新值已包含在被审阅的下一计划中。不能在其他命令中附加这些退休字段。

目标计划路径是**数据节点本机路径**。如果操作台在Windows，而数据服务在Ubuntu，不能填Windows盘符。父ECC编排使用明确的每节点审阅文件和目标身份，详见[ECC计划切换](ECC_PLAN_TRANSITION.md)。

在受控集成中，原生查询工具可提交已经生成的完整请求：

```bash
bin/getdaq-supervisor-query submit \
  --host 192.0.2.20 --port 24001 \
  --credential-file /home/daq/private/data-node.credential \
  --expected-agent data-node-0 --controller experiment-ecc \
  --expected-plan OLD_PLAN_SHA256 \
  --request-file /home/daq/reviewed/retire-plan-request.json
```

认证值从私有文件读取，不放入命令行。202只表示受理。原服务从受理起便永久拒绝后续新的控制命令；退休失败或应答不确定也不能继续旧采集。

使用**原操作ID**查询终态：

```bash
bin/getdaq-supervisor-query operation \
  --host 192.0.2.20 --port 24001 \
  --credential-file /home/daq/private/data-node.credential \
  --expected-agent data-node-0 --controller experiment-ecc \
  --expected-plan OLD_PLAN_SHA256 --operation-id ORIGINAL_RETIRE_OPERATION_ID
```

完成结果的 `plan_retirement` 返回目标计划、控制范围/持有期和证明摘要。随后查询原服务的实时退休证明：

```bash
bin/getdaq-supervisor-query plan-retirement \
  --host 192.0.2.20 --port 24001 \
  --credential-file /home/daq/private/data-node.credential \
  --expected-agent data-node-0 --controller experiment-ecc \
  --expected-plan OLD_PLAN_SHA256
```

工具核对返回内容的类型、节点、原实例/计划及正常结束状态。原生CLI的核对不是整个父ECC切换已经完成的证明。

## 6. 关闭原服务，再启动新服务

仅在原操作持久完成、证明已核对后，使用原部署的服务管理方式正常关闭原数据服务。此时receiver此前已经退出。新服务必须取得旧部署独占租约；原服务仍运行会导致启动拒绝，且不消费激活标记、不创建新部署根。

原证明保留在原目录，不搬到下一目录：

```text
OLD_AUDIT_ROOT/control/operations/ORIGINAL_RETIRE_OPERATION_ID/plan-retirement.json
```

启动命令：

```bash
bin/getdaq-supervisor \
  --config /home/daq/reviewed/next-data/next-service.json \
  --predecessor-proof /home/daq/audit/old/control/operations/ORIGINAL_RETIRE_OPERATION_ID/plan-retirement.json
```

新配置中的规范接收计划必须与证明中的目标准确匹配。新服务先绑定监听器，再核对/消费证明和取得旧部署租约，最后构造新的控制对象及新审计根。启动后没有自动Start。

通过 `identity`、`status` 和新增 `predecessor` 查询核对新实例、目标计划、预先授权的范围/持有期，以及原证明：

```bash
bin/getdaq-supervisor-query predecessor \
  --host 192.0.2.20 --port 24001 \
  --credential-file /home/daq/private/data-node.credential \
  --expected-agent data-node-0 --controller experiment-ecc \
  --expected-plan NEW_PLAN_SHA256
```

预期新生命周期为Fresh，归属已是退休时指定的新控制对象。原实例、原计划或原控制者不能启动新周期。实际新控制对象还应完成父级核对，不能绕过父级核对单独采集。

## 7. 留下哪些文件，为什么不能删除

| 文件 | 用途 |
| --- | --- |
| 原操作 `request.json` | 原受理请求和请求键的依据 |
| `successor-plan.json` | 原服务封存的新规范计划副本 |
| `cycle-evidence.json` | 切换时旧周期的元数据摘要、目录身份及raw文件名/大小等历史事实 |
| `plan-retirement.json` | 绑定原请求、正常周期、新计划和周期证据摘要的退休证明 |
| `plan-retirement.json.activation.json` | 一次性激活记录，绑定原证明、下一计划及激活标识 |
| 原部署 `plan.json`、`supervisor.jsonl` 和控制审计 | 原计划、原生命周期及原退休受理/完成的依据 |
| 原部署 `.supervisor.lock` | 新服务持有的原部署租约，阻止合作的旧服务重新启动 |

计划副本、周期证据和退休证明封存为0400；请求遵循原控制审计权限。新服务绑定实际文件身份、权限、大小和时间信息；同内容替换文件或篡改后恢复字节仍会保持故障。

证明绑定原绝对位置、原操作和审计。复制证明到另一个目录、另建同名文件、删激活标记或重新选择审计根，不构成合法恢复方式。**不要通过删除标记解除门槛。**

## 8. 旧数据可以归档吗，是否影响吞吐

切换前必须核对旧输出目录、封存记录、接收器锁已释放、raw文件集合及大小。控制软件只读取元数据并对raw做stat，不打开或哈希raw内容；它不提供逐字节数据完整性验证。

新服务激活成功后，旧raw及其receiver输出目录可以由独立归档流程移走；归档工具持有旧receiver输出锁也不应阻碍新计划采集。控制层保留的是切换当时的历史证据，不持续要求旧数据还在原路径。**旧审计根、控制证明、原计划、原生命周期日志和部署锁仍须保留。** 它们与可以移走的receiver输出目录不同。

新服务不会代替用户复制、删除或校验归档数据。原始DAQ继续由独立原生C++11接收器接收、缓冲、落盘和导出；Qt服务只承载认证命令与低频元数据。新证明查询在后台执行并发受限的元数据工作，普通状态使用缓存，不把历史raw扫描放入Qt事件循环。

控制元数据本身也有上限：退休请求8KiB、规范计划1MiB、周期证据16MiB、证明4MiB；仅两类新增证明查询允许5MiB HTTP回答，普通接口仍为原预算。目录/file集合有数量上限，超过后拒绝切换，不无界分配内存。

## 9. 失败时怎样处理

| 现象 | 含义与行动 |
| --- | --- |
| 401 | 认证失败，检查节点及私有认证文件 |
| 412 | 原实例、计划、会话或目标前提不匹配；重新只读核对，不能换ID碰运气 |
| 409 | 状态、归属、已退休或存储门槛不允许当前操作；保留原操作证据 |
| POST后断线/超时 | 结果不确定，只查询原ID；不能假定未受理，也不能自动新建请求重发 |
| 证明查询503 | 原操作未完成、当前不是原实例，或依赖证明已漂移；停止后续切换 |
| 新服务报告旧部署仍被占用 | 正常关闭原服务并核对其终止；不删除锁文件、不杀未确认的进程 |
| 已有激活标记或新部署根 | 不支持直接重复启动消费过的计划；保留现场，进入后续人工恢复流程 |
| 激活后改回被篡改的文件仍拒绝 | 故障有意保持；恢复字节不能抹掉一次发生过的证明失效 |

70.54数据层不实现部分节点切换回滚、多次接力、冷服务自动接管、自动修复审计或跨主机故障恢复。完成退休后旧服务冷重开依然保持，不能导出原实例的实时退休证明。新服务消费一次后也不能把同一启动命令当作通用重启方案。

## 10. 已验证到哪里

本机普通与严格C++11 ASan/UBSan/LeakSanitizer构建各运行12项认证服务场景，真实原生receiver接收合成帧；覆盖正常不同参数新周期、实际丢应答、旧租约占用、复制证明、封存/载荷大小漂移、审阅计划改变、旧服务冷门槛、激活后周期证明/原审计篡改与部署锁替换，以及旧目录归档并持锁后的新周期。

新周期逐项核对实际receiver进程的队列、最大帧和轮转参数已改变，同时比较旧数据内容未变。普通组额外在服务进程拦截对 `.graw` 的读取打开，验证没有raw读取；接收器写入不受该拦截器影响。插桩组未把系统Qt和普通receiver整体插桩。

每组生成载荷总预算2MiB，成功、失败和信号处理路径均清理自有测试raw。所有并行测试仍遵守用户20GiB合计上限。测试不删除用户真实数据。

具体证据、来源摘要与构建边界见 [验证记录](VALIDATION.md)的70.54节 和本阶段检查点。Linux构建及用户Windows Kit重建不等于Ubuntu24、macOS、独立Windows机器、实机或持续跨主机吞吐验收。整套重构仍在继续。
