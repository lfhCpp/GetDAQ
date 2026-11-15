# ECC与数据节点一起更换接收计划

本页接续[数据节点计划切换](DATA_PLAN_TRANSITION.md)和[原ECC服务交接](ECC_SERVICE_HANDOFF.md)。新增父ECC命令为 `retire-data-plans`，操作工具提供 `retire-submit` 和 `retire-inspect`。本机普通/插桩及包内进程联调已通过，验证范围见本页末尾及 `VALIDATION.md` 70.55；整套重构尚未完成。

## 1. 流程包含哪些进程

一次切换涉及原ECC、每个原数据服务、每个新数据服务和新ECC。原始数据始终在板端/发送端与原生receiver之间传输；ECC与Qt只交换命令、身份、状态及证明。

```text
原ECC：结束采集 → Breakup → Reset到已知Idle/Described
     ↓ 原控制者提交retire-data-plans
原数据节点：核对正常封存 → 封存新计划与退休证明 → 拒绝新控制
     ↓ 原ECC收集全部节点的完成结果，导出父收据
正常关闭原ECC及原数据服务
     ↓ 每个新数据服务使用自己的新配置与原退休证明启动
新ECC：使用保存的接班身份及父收据启动
     ↓ 核对每个新数据实例的前驱证明、目标计划和新归属
人工发起Describe → Prepare → Configure → Start，形成新周期
```

新ECC启动不会自动发起Describe、Prepare或Start。上一周期仍保留原会话、原目录、原PID/退出状态和封存历史。新数据服务开始时为Fresh，不能把上一代的Stopped周期冒充为自己的周期。

## 2. 当前允许改变什么

- 每个数据节点的接收计划及其配置/工件摘要可以改变，仍须满足数据层的程序、路径和内存策略校验。
- 新ECC配置、审计根、快照、数据控制范围及持有期由真实固定配置和保存的接班计划决定。
- 当前要求数据节点集合、控制地址/端口、控制者名称和超时策略保持。操作工具与原ECC把每个目标计划摘要代入原端点集合，重新计算数据绑定；必须与新ECC保存的绑定相等。
- 每个节点都使用不同的计划摘要。无需改变采集参数的节点也需准备新的部署标识/审计根，不能混用一部分原服务的同计划交接和另一部分的退休。

本入口不升级receiver程序、不部署板端固件，不提供运行中热修改、节点增删、控制端点迁移或连续板卡占用移交。操作台审阅/提交/回执导出入口见[详细窗口说明](ECC_PLAN_TRANSITION_GUI.md)；服务更替仍明确执行。正常关闭旧ECC后新ECC再取得自身主机占用，与不中断占用的原子交接不同。

## 3. 准备阶段

1. 在各数据节点准备新的完整服务配置。输出根和新审计根的父目录按[数据层说明](DATA_PLAN_TRANSITION.md)准备。
2. 在各节点用 `getdaq-supervisor --config NEXT_SERVICE --write-plan REVIEWED_PLAN` 生成0400规范计划，保存准确摘要。该路径是节点本机路径。
3. 让新ECC的固定配置引用这些节点和新计划摘要，并准备相应实验工件/发送端配置。使用新审计根和新的部署标识。
4. 按[接班身份说明](ECC_SUCCESSOR_IDENTITY.md)离线生成新ECC的 `successor.json`。生成本身不取得板端或数据节点控制权。
5. 在操作工具所在主机准备一份目标清单。它只列节点、已审阅计划摘要及节点本机路径，不包含认证值或raw。

目标清单格式：

```json
{
  "schema": 1,
  "kind": "getdaq.reviewed-data-plan-targets",
  "plans": [
    {
      "agent_id": "data-node-0",
      "plan_sha256": "替换为该节点实际生成的64位小写十六进制摘要",
      "plan_file": "/home/daq/reviewed/next-data/next-plan.json"
    }
  ]
}
```

示例仅演示一个节点；实际必须覆盖每个准确节点，按 `agent_id` 升序排列且不重复。不能把清单路径误当成各节点计划路径。协议请求仍有8KiB总预算；清单中的目标必须能编码进该预算，不会自动拆成未经整体审阅的多批切换。

## 4. 正常结束原采集

通过原ECC完成Stop、Breakup和正常Reset收尾，确认Idle或Described，且没有UNKNOWN、告警/接收故障、输入或审计门槛。所有原receiver必须正常退出并封存。

新鲜无采集Idle不满足条件；故障会话退休也不等于正常封存。不能用本入口清除原失败历史或补一个假的正常结束周期。

## 5. 只提交一次父请求

替换以下所有示例路径与标识。`ORIGINAL_CLIENT_EPOCH` 来自实际原客户端持有期；操作ID必须是本次新的合法ID。证据目录必须尚不存在，工具会以0700创建。

```bash
bin/getdaq-ecc-handoff retire-submit \
  /home/daq/old/ecc-service.json \
  /home/daq/next/ecc-service.json \
  /home/daq/next/successor.json \
  ORIGINAL_CLIENT_EPOCH ORIGINAL_RETIRE_OPERATION_ID \
  /home/daq/reviewed/switch-evidence \
  /home/daq/reviewed/data-plan-targets.json
```

工具离线核对新旧固定输入及接班计划，再查询原ECC身份和原状态，保存准确父请求/目标/原状态后只POST一次。提交期间持有接班计划共享锁；新ECC启动所需的独占锁无法同时取得，防止提前消费身份。

原ECC受理时验证目标绑定、已知Idle/Described及完整正常周期，随后持久禁止原实例的后续新控制。它先保存所有节点意图，再对每个节点最多POST一次。节点完成结果关联原实例/计划/会话、父审计、新计划和新控制者。

断线或超时后，保留相同文件和原操作ID，只读核对：

```bash
bin/getdaq-ecc-handoff retire-inspect \
  /home/daq/old/ecc-service.json \
  /home/daq/next/ecc-service.json \
  /home/daq/next/successor.json \
  ORIGINAL_CLIENT_EPOCH ORIGINAL_RETIRE_OPERATION_ID \
  /home/daq/reviewed/switch-evidence \
  /home/daq/reviewed/data-plan-targets.json
```

`retire-inspect` 不POST，不换ID，不重新创建证据目录。原请求、目标、实例或输入不同会拒绝。全部完成时，证据目录保存 `operation.json` 与 `receipt.json`；后者类型为 `getdaq.remote-plan-retirement`，含每个节点的原请求、请求键和退休证明摘要。

## 6. 逐节点更换数据服务

只有父操作全部完成且收据核对通过后，正常关闭原ECC。再正常关闭各原数据服务，用本节点的新配置及原证明启动新数据服务：

```bash
bin/getdaq-supervisor --config /home/daq/next/data-service.json \
  --predecessor-proof /home/daq/audit/old/control/operations/NODE_OPERATION_ID/plan-retirement.json
```

`NODE_OPERATION_ID` 是父收据中该节点的实际子操作ID，通常不等于父操作ID。不要猜目录名，应从节点请求/回执取得。

原数据服务仍运行会阻止新服务取得原部署租约。不能删锁或修改证明来强行接班。新服务一次性消费激活标记，使用新计划并直接绑定已指定的新控制范围/持有期。此时没有receiver在采集。

## 7. 启动新ECC并核对

```bash
bin/getdaq-ecc-server --config /home/daq/next/ecc-service.json \
  --successor-plan /home/daq/next/successor.json \
  --handoff-receipt /home/daq/reviewed/switch-evidence/receipt.json
```

新ECC重新查询每个节点的真实身份和 `/v1/predecessor`，核对原退休请求、证明摘要、目标计划、原正常封存会话，以及自己的新控制范围/持有期。每个节点必须是不同于原实例的新实例，且初次接纳时仍为Fresh，没有被其他操作使用。

收据只提供需要核对的历史引用，不会把一个没有前驱证明的新鲜数据服务变成合法接班者。新节点使用了错误计划、缺失/不同证明、旧身份或错误归属都会拒绝启动。

新ECC启动成功后再人工Describe、Prepare、Configure、Start；这些命令仍检查新实验工件与发送端/接收计划对应关系。切换本身不能证明现场寄存器已经加载了新参数。原始载荷继续由独立receiver处理。

## 8. 部分失败与保留文件

若一节点已退休而另一节点失败，原ECC保持不确定/禁止新控制，已完成节点的退休也不撤销。保留全部节点意图和结果，不重发父请求、不回滚、不自动重启任何服务。本阶段未提供这种部分切换的自动恢复入口。

后续新增的[原实例显式补齐流程](PARTIAL_HANDOFF_COMPLETION.md)可观察已受理/完成的原节点请求，并在明确未受理且原实例/封存周期仍匹配时补交原请求。它使用独立接续操作、保留原父请求failed及旧控制禁用，不是重发退休或清故障；命令行入口与[Qt显式补齐窗口](ECC_PLAN_TRANSITION_GUI.md#71-部分失败后在窗口显式补齐)均已实现；Qt要求原退休提交前已保存并绑定准确正常状态，缺失时不补造。已经受理但执行失败的节点和冷实例仍不在该入口范围内。

旧ECC冷重开仍禁止新控制，不能以新实例身份导出旧实时收据。消费过的接班身份不能直接再次启动；这不是常规进程重启方案。后续冷恢复需要单独的、能核对原证据的明确流程。

旧raw可在新数据服务激活后按独立归档流程移走，但原审计根、计划、日志、部署锁、节点退休/激活证明和父收据必须保留。详细边界见[数据节点计划切换](DATA_PLAN_TRANSITION.md)。


## 9. 已验证范围与证据

最终普通组包含两种编码的不同配置周期、第二节点拒绝后的部分退休保持，以及两种编码的原同计划交接回归，共五场景；新控制ASan/UBSan/LeakSanitizer组包含两种新周期与一个部分退休场景，共三场景；新Linux候选包另运行编码1.1完整新周期。合计九场景，包括五次新配置周期、两次部分保持、两次同计划回归。

新实验使用合成工件，实际改变一个AsAd触发延迟，并改变receiver队列、最大帧和轮转参数；新ECC完成Describe、Prepare、Configure、Start、Stop、Breakup与正常Reset。两代实例、两代数据节点、计划/工件/配置摘要及采集会话均分别绑定。新数据服务初次核对为Fresh，旧会话没有被当作新会话。

部分场景实际删除第二节点的自有审阅文件，验证第一节点只退休一次、第二节点没有受理、父操作失败且没有完整收据、旧ECC及冷重开都拒绝继续控制。它证明保持行为，不代表已经实现部分切换恢复。操作工具在编码1.1执行retire-submit/retire-inspect，重复submit目录拒绝；本阶段没有声称新退休命令经历了实际丢POST应答的注入。

独立检查器重新核对真实父请求和唯一受理行、各节点原退休证明/完成记录、新计划、原会话、接班身份与激活标记，以及新周期正常封存。纯C++11协议测试另读取315份实际身份/状态/意图/终态记录。普通数据服务、receiver、旧GET/Ice和系统Qt未在插桩组整体插桩。

入口为 `build-ecc-plan-transition/checkpoint.json`，只读复核：

```bash
python3 tests/check_ecc_plan_transition_checkpoint.py
```

最终46个测试.graw共671744字节已删除；含前期探索和汇总脚本修复前的组，整个阶段自有raw均为零。每组预算512MiB，重验期间最多四组并发的保守预算为2GiB，仍遵守所有流合计20GiB上限。Linux和用户Windows Kit已重建，新运行目录已核对；Windows初始窗口检查不代表新增GUI切换向导，；新增窗口入口的独立验证见后续70.56记录。

当前限制：连续板卡占用移交、部分/冷切换恢复、改变控制端点/节点集合，以及Ubuntu24/macOS/独立Windows、现场硬件协议/真实加载和持续跨主机吞吐验收仍待完成。板端getHwServer/IceEirfu继续保留现有部署。
