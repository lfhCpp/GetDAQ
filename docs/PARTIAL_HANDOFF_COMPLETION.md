# 部分交接或计划退休失败后怎样补齐

本页说明新源码中的`complete-data-handoff`、`complete-submit`和`complete-inspect`。原生接口四场景、HTTP普通/新控制代码插桩两组各两编码的新实例采集验证已通过。70.60的后端候选目录名含`partial-handoff-completion-candidate`；70.61包含Qt补齐入口的新候选目录名含`qt-partial-completion-candidate`。旧`cobo-maintenance-candidate`包不包含本页新增功能。不要把新命令发送给旧版服务。

## 1. 这个功能解决什么问题

正常更换数据服务时，原ECC先让每个数据节点保存下一代目标和退休证明。假设节点A已经完成，节点B因为找不到此前审阅的计划文件而在受理前拒绝；或者节点实际完成了请求，但返回结果的网络连接断开。此时父ECC请求会失败，原ECC保持禁止新控制。

这里处理的是切换请求失败；前一个采集周期必须已正常封存。失败采集、原始文件损坏或板卡加载失败，不能用本功能改成正常结束。

“父请求失败”不能直接解释为“所有节点都没执行”。重发整个切换流程可能重复操作A；删除日志、换一个新实例或回滚A也不能恢复原来的控制关系。

新的接续流程保留原请求，只处理还没有得到确认的部分：

| 原节点情况 | 接续怎样处理 |
| --- | --- |
| 原请求已经完成 | GET原操作，核对原实例、请求键、正常封存周期和证明；不再POST |
| 原请求已受理、仍在执行 | 只GET原操作，等待同一操作完成；不再POST |
| 此前明确收到409/412拒绝，当前原操作404，原服务及正常封存周期仍匹配 | 在这次明确发起的接续中，最多补交一次完全相同的原节点请求 |
| 原请求在发送前就没有发出，当前原操作404，其他条件均匹配 | 在本次显式接续中提交原节点请求 |
| 此前发送结果不明，现在只返回404 | 保持失败；404本身不能证明以前没有受理，不允许据此再POST |
| 原节点 `retire-plan` 已失败，但同一活节点已通过明确 `complete-plan-retirement` 补齐 | 新父后端只 GET 核对补齐请求、完成操作及准确原证明；先按[节点失败补齐步骤](FAILED_PLAN_RETIREMENT_COMPLETION.md)操作，父端不代发节点修复 |
| 原节点请求 UNKNOWN、普通同计划交接执行失败、尚未明确补齐的失败退休，或节点换实例/计划/归属、原控制证据损坏 | 保持失败，不接管、不回滚，也不自动修改目标 |

这里的“原节点请求”是父ECC为某一个数据节点保存的子操作，不是新建一套目标计划。补交时操作ID、请求字节、目标计划、原会话和父审计引用都保持原值。

## 2. 成功后哪些东西仍然保持

原父请求仍是`failed`，不会被改写成成功。接续使用另一个操作ID，保存新的意图、逐节点观察和完成记录。只有所有原节点都已核对完成，原ECC才提供完整回执。

原ECC、原数据控制者和原退休节点继续拒绝普通采集命令。这是成功接续后的预期状态，不是需要清掉的故障标记。接续只把下一代需要的证明补齐；随后仍需按[计划切换流程](ECC_PLAN_TRANSITION.md)明确停止旧服务、启动新数据服务和新ECC，再人工发起新采集。

接续不重新Prepare、不写板端、不读取或搬运raw，也不会恢复上一次Run。Qt和ECC仍只传低频控制元数据，原始事件由独立原生receiver处理。板端getHwServer/IceEirfu保持现有部署。

## 3. 开始前先核对条件

1. 原ECC必须仍是发生部分失败的那个运行实例。不要先重启原ECC或数据服务。冷实例暂不支持本入口。
2. 原父操作必须是`handoff-data-control`或`retire-data-plans`，且终态为`failed`。正在执行的父请求先只读观察，不另行接续。
3. 使用原客户端、原`client_epoch`和原固定配置。新的接续请求从已保存的原请求读取这些值，不能随机生成持有期。
4. 原生控制日志仍确认Idle或Described，且没有硬件UNKNOWN、原生存储错误、输入漂移、告警或接收故障等独立保持。数据交接自身的保持会被保留，而不是当作普通控制恢复。
5. 保留原证据目录。当前命令行工具要求其中有`request.json`和提交前的`original-status.json`；原`submit`或`retire-submit`会保存这些文件。不能用失败后的状态文件替代提交前的原状态。
6. 如果原因是审阅计划文件缺失，先从可信备份恢复此前生成的准确文件，保持原路径、准确字节和0400权限。不要生成另一份新计划来冒充原目标；内容不同会因摘要不符被拒绝。

原生控制边界检查只核对当前保留的控制日志、不可变输入及独立保持，不通过读回少数寄存器推断完整硬件状态，也不解除冷启动的UNKNOWN。

## 4. 显式提交一次接续

命令在ECC主机上运行，需要访问实际的新旧固定配置和接班身份文件。以下路径和操作ID都需要替换。`ORIGINAL_EVIDENCE`指原失败切换的证据目录，`NEW_EVIDENCE`是一个尚不存在的新目录。

```bash
bin/getdaq-ecc-handoff complete-submit \
  /home/daq/old/ecc-service.json \
  /home/daq/next/ecc-service.json \
  /home/daq/next/successor.json \
  /home/daq/reviewed/original-retirement-evidence \
  complete_retirement_001 \
  /home/daq/reviewed/complete_retirement_001
```

工具会先读取原请求和原状态，核对当前服务仍是同一个实例和同一套固定输入，再保存新的接续意图，只提交一次父接续POST。原ECC按照自己保留的原节点请求逐节点核对。工具要求新配置和接班身份仍与原请求中的准确摘要、范围、持有期及数据绑定一致，并在提交/观察期间持有原身份文件的共享锁，阻止提前激活。它不接受另一套节点目标清单，因此不能顺带更改切换目标。

如果某节点仍缺原计划文件，接续仍会失败。已经完成的节点不撤销、不重发。恢复原因后需要再次显式接续时，使用一个新的接续操作ID和新的证据目录，仍引用同一个原失败父请求；不要复用新目录执行第二次`complete-submit`。

## 5. 超时或丢回复后只查原接续操作

保持相同参数和同一个接续证据目录，把动作改成`complete-inspect`：

```bash
bin/getdaq-ecc-handoff complete-inspect \
  /home/daq/old/ecc-service.json \
  /home/daq/next/ecc-service.json \
  /home/daq/next/successor.json \
  /home/daq/reviewed/original-retirement-evidence \
  complete_retirement_001 \
  /home/daq/reviewed/complete_retirement_001
```

`complete-inspect`不POST、不换操作ID、不覆盖已有证据。它观察的是上面那一次接续，而不是重新执行原父请求。只有确定这次接续已终态失败、原因已处理且服务仍符合前提，才考虑上节所说的另一次明确接续。

如果接续完成，但获取回执恰逢服务忙碌，保留证据并再次执行`complete-inspect`。不要把“未取到回执”当作“需要再执行一次”。

## 6. 成功文件怎样使用

新证据目录包含：

| 文件 | 意义 |
| --- | --- |
| request.json | 新接续请求，command为complete-data-handoff，failed_operation_id绑定原父操作 |
| original-request.json | 原handoff/retire请求的准确字节 |
| original-status.json | 原切换提交前的实例、归属和正常封存周期 |
| original-operation.json | 原父请求失败记录的副本，保留历史 |
| submission.json / submission-held.json | 一次POST的回复或提交结果保持记录 |
| operation.json | 新接续操作确认完成的结果 |
| receipt.json | 所有原节点已核对完成的完整交接/计划退休回执 |

继续启动新ECC时，把本次新证据目录的`receipt.json`作为`--handoff-receipt`参数；各新数据服务仍使用自己原节点操作生成的退休证明启动。回执内引用的子操作ID来自原父请求，不是新接续操作ID。具体逐节点启动命令见[计划切换](ECC_PLAN_TRANSITION.md)。

## 7. 实现边界

原生入口为`RemoteReceiverManager::CompletePendingHandoff(newAuditDirectory)`，只作用于保留了原请求及原审计对象的同一个管理器。HTTP入口为认证`POST /v1/operations`，command为`complete-data-handoff`，新增绑定字段只有`failed_operation_id`；客户端不能在接续请求中重新指定目标。

服务端把数据层交接保持与独立原生控制保持分开核对。新的完成记录不会解除旧ECC禁用，也不会把失败Run改成正常Run。冷实例、已经受理但执行失败的节点、部分硬件加载、原数据文件缺失或损坏等情况不属于本入口的成功范围。

当前源码的Qt切换窗口已增加 `Complete original partial retirement...` 和 `Observe saved completion`，操作步骤及本地证据要求见[窗口手册第7.1节](ECC_PLAN_TRANSITION_GUI.md#71-部分失败后在窗口显式补齐)。新操作台在原退休POST前保存并绑定正常封存状态；旧版本没有这份记录时不补造。70.60候选包仅包含本页原有HTTP/CLI能力，Qt新增入口使用70.61的`qt-partial-completion-candidate`候选包；实际窗口两编码、普通交付/插桩服务组合验证通过，具体边界见[验证记录](VALIDATION.md)第70.61节。冷恢复、已受理后失败节点恢复和整套验收仍未完成。
