# 数据节点已经受理、但退休执行失败时怎样处理

本页处理一个特定问题：上一轮数据已经正常收尾，父 ECC 发起接收计划更替，某个数据节点的 `retire-plan` 已经受理，随后在保存下一代证明时失败。新的 `complete-plan-retirement` 允许操作者在**原数据服务仍然运行**时，明确补齐同一原请求的证明。父 ECC 再以 GET 核对该补齐结果，收集完整的计划退休回执。

这是本轮源码新增接口。70.71 及以前已封存的候选包没有这个节点命令；不要只更新操作台，就把新请求发送给旧节点服务。整套系统仍在重构，本文的验证范围见最后一节。

应在正常停采、封存和明确更替服务时预先部署支持本接口的新版本，再用于此后发生的失败。**已经在旧版节点中失败的操作，不能靠重启升级后套用本入口**：重启会更换实例，本功能拒绝冷接管。不要为尝试新命令先停止仍需保留证据的原失败服务。

## 1. 先分清三个操作

假设节点 A 成功，节点 B 失败。它们有各自的节点操作 ID，外面还有一个父 ECC 操作 ID。

| 操作 | 命令 | 执行者 | 成功后原失败记录怎样处理 |
| --- | --- | --- | --- |
| 原计划更替 | 父 `retire-data-plans`，节点 `retire-plan` | 父 ECC 和数据节点 | 原终态保留 |
| B 节点证明补齐 | `complete-plan-retirement` | 操作者明确提交给原 B 节点 | B 原操作仍为 `failed`；另存补齐成功操作 |
| 父回执补齐 | `complete-data-handoff`；CLI 动作为 `complete-submit` | 操作者明确提交给原父 ECC | 原父操作仍为 `failed`；另存父补齐操作 |

节点补齐 ID 和父补齐 ID 是两个不同 ID。`failed_retirement_operation_id` 指向 B 的原节点退休 ID，不是父 ECC ID，也不是前一轮 Start ID。

**成功不会重新启用旧 ECC 或旧数据节点。** 旧控制继续冻结；新采集仍要按[接收计划更替](ECC_PLAN_TRANSITION.md)明确启动下一代服务和新 ECC。原 Run 不恢复为正在采集，板端 `getHwServer` / `IceEirfu` 保持现有部署。

## 2. 哪些情况可以用

必须同时满足以下条件：

1. 原节点退休操作已经有明确的 `failed` 终态。正在运行、回复丢失或 `unknown` 时先查询原 ID，不能另起修复操作。
2. 原节点服务还是原来的活实例，原固定计划、控制归属、持有期和原请求都没有改变。重启服务后生成的新实例不能冒充旧实例。
3. 原会话已经正常 EOF、排空、接收器正常退出、被回收并确认封存。失败采集、损坏的 seal 或独立存储故障不在本功能范围内。
4. 下一代仍是原请求承诺的那一个计划：原计划文件路径、准确内容摘要、下一代控制范围及持有期不变；所需目录和程序检查仍通过。
5. 以前的节点补齐尝试没有处于 accepted / running / unknown；已经成功补齐过的原操作也不能再次发起新补齐。

本入口不支持普通同计划 `handoff-control` 的执行失败，不清除独立硬件故障，不做任意硬件冷恢复。旧 ECC 冷补齐另有[独立说明](COLD_DATA_HANDOFF_COMPLETION.md)；本轮新增链的正向联调范围是原父实例仍在运行，不能据此推定所有冷补齐组合已验收。

## 3. 为什么只恢复计划文件还不够

节点在受理退休请求时就冻结旧控制。随后即使保存证明失败，这个冻结也不能回滚：另一个节点可能已经退休，父请求的网络回复也可能丢失。

因此，把缺失的计划文件放回去不会自动重跑原操作。原 ID 查询仍返回原 `failed`。操作者需要另发一个绑定原 ID 的明确补齐操作，服务才能记录“谁在什么请求下补齐了原证明”。父 ECC 不会自动代发这个节点补齐请求。

如果只是在受理前明确收到 409/412，原操作根本没有建立，请按[普通部分补齐](PARTIAL_HANDOFF_COMPLETION.md)处理，不要伪造一个 failed 节点操作。

## 4. 找到准确的原节点请求

在该数据主机上，原节点控制目录的结构如下：

```text
<原 receiver_plan.audit_root>/control/
  operations.jsonl
  operations/
    <原节点退休 ID>/
      request.json
      successor-plan.json       # 失败时可能还没有
      cycle-evidence.json       # 失败时可能还没有
      plan-retirement.json      # 失败时可能还没有
```

`request.json` 是原节点的规范请求，通常为 0600 权限。父 ECC 的原意图也保留各节点请求。不要拿父服务的 `network/operations/.../request.json` 直接当节点请求，它们的协议不同。

先用 `getdaq-supervisor-query operation` 读取该节点原 ID，核对 `command=retire-plan`、`phase=failed`、实例、原计划和控制者。保留查询输出及原请求，不改原目录或日志。

## 5. 生成一份新的补齐请求

以下脚本只读原节点请求，以独占新建方式保存补齐 JSON。示例路径和 ID 应换成现场已经核对的值；目标文件必须尚不存在。

```python
import json
import os
from pathlib import Path

original_file = Path('/home/daq/data-old/control/operations/ORIGINAL_NODE_ID/request.json')
output_file = Path('/home/daq/review/repair_retirement_001.json')

original = json.loads(original_file.read_text(encoding='utf-8'))
assert original['command'] == 'retire-plan'
request = dict(original)
request['command'] = 'complete-plan-retirement'
request['operation_id'] = 'repair_retirement_001'
request['failed_retirement_operation_id'] = original['operation_id']

descriptor = os.open(str(output_file), os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
with os.fdopen(descriptor, 'w', encoding='utf-8') as stream:
    json.dump(request, stream, ensure_ascii=False, indent=2)
    stream.write('\n')
```

除上述三个字段，其他字段全部来自原请求，不能改动。尤其不要换 `instance_id`、`expected_session_id`、`parent_audit_reference`、配置/工件摘要、下一代计划、控制范围或持有期。服务会把补齐请求还原为原退休请求，再比较规范字节；同一文件名或看起来相似的参数不能代替这个核对。

原计划文件缺失时，只能先恢复原路径下可信的准确内容及 0400 权限；不是重新生成一个不同目标。现有元数据的不同字节也不能靠删除、截断或覆盖消除。

## 6. 提交一次，之后只查同一个 ID

`getdaq-supervisor-query` 使用凭据文件认证，凭据值不放入命令行。下面的尖括号参数需要替换为原节点信息：

```bash
bin/getdaq-supervisor-query submit \
  --host <原节点IPv4> --port <原控制端口> \
  --credential-file /home/daq/secrets/node-b.token \
  --expected-agent <原agent_id> --controller <原controller_id> \
  --expected-plan <原节点plan_sha256> \
  --request-file /home/daq/review/repair_retirement_001.json
```

202 只表示受理，不表示完成。收到受理、超时或丢回复后，均查询同一个补齐 ID：

```bash
bin/getdaq-supervisor-query operation \
  --host <原节点IPv4> --port <原控制端口> \
  --credential-file /home/daq/secrets/node-b.token \
  --expected-agent <原agent_id> --controller <原controller_id> \
  --expected-plan <原节点plan_sha256> \
  --operation-id repair_retirement_001
```

成功结果为 `phase=complete`，`plan_retirement.proof_sha256` 非空。原节点退休 ID 查询仍为 `failed`，这是预期结果。同 ID、同请求再次提交只返回原操作，不重新补写。另一个新 ID 也不能再次修复已经成功补齐的原操作。

补齐明确失败时保留原结果和部分文件，先查明原因。只有前一次确为 `failed`、原绑定和所有前提仍满足时，才能明确发起另一补齐尝试；`unknown` 不能用换新 ID 绕过。

## 7. 节点成功后，继续父 ECC 补齐

原父 ECC 仍需要自己的明确补齐操作。CLI 的 `complete-submit` 参数和[已有操作步骤](PARTIAL_HANDOFF_COMPLETION.md)相同；计划切换窗口也仍使用既有“补齐原退休”入口。

本轮父后端增加以下只读核对：

1. GET 原节点退休 ID，确认它仍是准确原请求的失败结果。
2. GET `/v1/plan-retirement-completion`，取得原活实例已经完成的补齐请求和操作结果。
3. 还原补齐请求，与父端保留的原节点规范请求逐字节比较；核对 agent、请求键和完成证明摘要。
4. GET 最新状态及 `/v1/plan-retirement`，核对同一正常封存周期、准确证明字节摘要、原请求及下一代计划。
5. 所有节点证明完整后，保存父补齐完成记录，导出仍引用原节点请求的完整回执。

这条分支不向节点 POST 原退休或新修复请求。节点尚未明确补齐、换了实例或证明不匹配时，父补齐失败，已成功的其他节点保持原结果。父端新操作 ID 与前面的节点补齐 ID 分开保存。

只读补齐端点需要认证；无本实例完成证明时返回 503。它不会通过“查看”触发修复，冷重开也不会把旧完成历史冒充当前活实例的完成授权。

## 8. 磁盘上到底改了什么

节点只处理原退休目录中的三个小型元数据文件。处理前先检查三个目标，避免发现已有冲突后才写入其他缺失文件。

| 文件情况 | 处理方式 |
| --- | --- |
| 文件缺失 | 独占新建准确原目标元数据，再封存 |
| 准确既有 0600 普通文件 | 核对属主、单链接和内容，保留 inode，封存为 0400 |
| 准确既有 0400 普通文件 | 核对后使用，保留内容 |
| 字节不同、不安全链接、错误属主或权限 | 拒绝，不覆盖、不删除 |

新补齐目录保存独立请求及 `original-request.json`；日志追加补齐操作的 accepted / running / complete 或失败记录。原 failed 记录和原请求不改写。证明格式仍是原 `getdaq.supervisor-plan-retirement` schema 1，下一代数据服务核对原失败和明确补齐完成的日志链后，才允许原目标计划启动。

只读查看结果不代表板端物理状态已恢复，文件系统检查也不等于真实断电持久性验收。

## 9. 性能和依赖边界

补齐只读写控制日志、计划、seal 清单和证明。Qt 低频 HTTP 前端把核对放到工作线程，不接收或转发 `.graw` 原始载荷；父 ECC 同样只处理元数据。原生 receiver 的收包、缓冲、分帧、写盘和在线导出热路径没有加入 Qt 或新依赖。

故障注入共享库仅在 Linux 的 `BUILD_TESTING=ON` 下构建，用于让自有夹具在受理后遇到缺失计划；它不是生产依赖，不应设置到实际运行服务的 `LD_PRELOAD`。正常生产构建仍可关闭测试。

## 10. 验证范围

本轮结果入口为 `build-failed-retirement/`，具体通过项及当前限制见[验证记录](VALIDATION.md)和[重构状态](REFACTOR_STATUS.md)。节点测试覆盖原失败保留、缺失/完整/部分元数据、异字节拒绝、独立 seal 故障、冷实例拒绝、后继数据服务的新周期及封存。父库联调使用两个实际 HTTP 数据服务和实际 receiver，板端是本机合成 servant，分别覆盖 Ice 1.0 / 1.1。

父库联调、完整 ECC HTTP 服务、实际 Qt 窗口确认、新计划完整 GUI 采集、独立目标机器和真实多板高速采集是不同证据。不能因节点或父库场景通过，就声称这些层次全部通过。全部测试流合计保持不超过 20 GiB，结束删除自有传输文件；原始生产数据不清理。
