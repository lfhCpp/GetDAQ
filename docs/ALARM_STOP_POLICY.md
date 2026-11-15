# 后台告警：自动停生产，保留核对与Run记录

原接收故障之后、同一原采集的独立告警现有单独的[明确告警确认与只读恢复入口](ACQUISITION_RECONCILIATION.md#明确确认原告警后核对同一次失败采集)。它要求原接收与告警负向停止均完成，以及连接/数据退休/断电全部证明；普通已知Stop告警、Prepare告警、未知发送源和冷接管仍不在该入口范围内。

## 1. 旧系统做了什么，新入口解决什么

收到的2021主机源码中，`get/mt/AlarmLogger.cpp`把AsAd告警写成带 `#STOP#` 前缀的日志；注释说明这是一条发给Ganil框架的停Run请求。这个回调本身没有直接完成ECC Stop，也没有确认接收器EOF。因此，只有这段旧软件而没有对应外部框架，不能假定Run会自动停下来。

新固定ECC服务可以明确启用后台告警策略。它观察自己长期持有的CoBo回调缓存，先持久保存告警依据和控制门槛，再使用已有串行命令入口执行一次停止。它不依赖log4cxx或外部日志服务，没有新增运行库。

原始数据仍是板端sender → 独立原生receiver → 磁盘。告警回调、策略线程、Qt HTTP前端和GUI只处理低频元数据。

## 2. 怎样启用

固定 `getdaq.ecc-service` 配置可增加一个可选字段：

```json
"alarm_stop_policy": {
    "enabled": true,
    "poll_interval_ms": 100
}
```

这是完整JSON对象中的字段片段。放在与 `senders`、`remote_receivers` 相同的层级，按JSON格式补逗号；不是单独可运行的服务配置。

| 字段 | 含义 | 约束 |
| --- | --- | --- |
| enabled | 是否由此服务消费保持的告警并安排一次停止 | 必须是JSON布尔值；省略整个policy时默认false |
| poll_interval_ms | 两次缓存观察之间的等待 | 整数25..5000毫秒；默认100毫秒 |

示例配置 [service.example.json](../config/ecc/service.example.json)显式写false。启用前核对现场回调地址、反向网络、板型、供电/保护和实际控制归属；离线检查不能代替这些确认。

启用时，每块活动CoBo必须配置自有告警订阅，即实验工件中的 `prepare.monitoring.subscribeAlarms=true`。缺少一块、错误类型、非法周期或多余policy字段，离线检查就拒绝；停用CoBo不要求加入监听集合。

配置文件属于固定输入绑定。增加或改变policy会改变输入摘要及父快照；运行中修改文件不会热加载，还会触发保持的配置故障。部署方法和 `--check-config` 命令见[固定ECC网络服务](ECC_NETWORK_SERVICE.md)。不能把换审计目录当成旧故障已恢复。

## 3. 不同阶段收到告警会怎样

| 当前证据 | 策略动作 | 完成后还保留什么 |
| --- | --- | --- |
| 健康Running | 一次普通ECC Stop：先停全部生产，全部确认后刷新sender | 原TCP和receiver会话保留；ECC可成为Ready，但网络告警门槛仍保持 |
| 健康Paused | 一次普通ECC Stop，重新观察停止前提，沿用已停止的sender | 不再次发送前次已确认的生产停止；原连接/receiver保留 |
| 已UNKNOWN并持有原准备/采集对象 | 一次 `stop-production-after-failure` | 原UNKNOWN及原失败操作不变，不碰未知sender/连接；适用条件见[故障停止](PREPARATION_FAILURE_STOP.md) |
| 健康Prepared/Ready等非生产阶段 | 持久保存告警并保持门槛，不发送新的Stop | 原准备/连接/接收记录保留，不允许正向命令继续 |
| 服务重开 | 只保留历史和启动核对门槛 | 不重建旧订阅，不重新执行旧告警停止 |

后台不会在Ice告警回调线程里调用硬件Stop。回调负责记录告警、设置保持信号；策略线程观察缓存，使用与用户命令相同的入口锁。已有命令执行期间不插入另一组硬件写入，不取消或重放正在执行的命令。

Prepare/Configure/Start本身仍逐步检查联锁。若告警使这些步骤失败，原状态和记录按照原生流程处理；后台在命令终止后才能核对缓存和安排后续动作。这里没有承诺固定的毫秒级停机期限：正在执行的RPC、命令超时、回调持久化和控制调度都会影响响应。主机软件不代替AsAd硬件保护。

## 4. 为什么只自动执行一次

告警是保持信号，不是一个可以通过读取清零的按钮。第一次确认观察后，服务保持 `control_needs_reconciliation=true`，保存 `alarm-held` 记录；需要动作时创建一个 `internal-alarm-...` 操作ID，并保存规范请求及摘要，再交给唯一后台命令线程。

同一服务的重复通知、第二块板随后通知、队列溢出或再次轮询，不产生新的自动停止。停止写入已生效但应答丢失时，按未确认结果处理，不自动重发；原生生产停止对象还保存各目标的尝试记录。

`internal-alarm-` 是保留前缀，外部POST不能使用它创建或重发操作。可以通过正常GET读取其历史。告警动作沿用已经绑定的operator client_epoch，不把后台处理伪装成另一个控制者接管。

若告警意图、输入绑定、目录或日志写入失败，保持门槛并报告未知，不绕过审计发送盲目的硬件命令。服务重开也不以“重新扫描到同一告警”为理由再次停止。

## 5. Stop成功为什么仍不能Start

Stop成功只确认这一停止序列及其正常后段；它不说明AsAd故障消失、不证明实际加载配置、控制权或运行数据完整。网络门槛会继续保持。

因此可能同时看到：

```text
告警动作 phase = complete
ECC state = Ready
control_needs_reconciliation = true
```

这表示生产已经按当前对象停止，但仍须核对告警和系统状态。不能仅因为state为Ready就Start，也不能用Reset、换client_epoch、新建对象或恢复原配置字节清门槛。

普通Stop刷新sender但保持TCP；receiver尚没有因此获得自然EOF。告警处理不会自动Breakup、断开连接、kill/restart receiver、关闭供电、注销自有订阅、移动数据或完成GUI Run。同一保留对象的已知停止告警可按第10节明确收尾；UNKNOWN使用对应故障流程。现场完整硬件恢复尚未验收。

## 6. 从哪里查看结果

身份和状态沿用 `/v1/identity`、`/v1/status`。新增只读、需要认证的 `/v1/alarms`，只返回后台发布的缓存，不等待硬件RPC、不实时拉取数据节点、不读取.graw内容。

| 告警字段 | 阅读方法 |
| --- | --- |
| enabled | 固定部署是否启用策略 |
| latched | 是否已经保持告警/启动核对处理，不能当作硬件告警已清除 |
| operation_id | 已安排的自动操作ID；非生产阶段只保持告警时可以为空 |
| phase | watching、held/accepted、complete、failed、unknown、held-no-run或reopened-held等阶段 |
| observation | 与告警意图绑定的缓存观察：父状态、原操作ID、各活动监听器的保持信号和计数 |

收到告警并保持后，`observation`保留触发时的依据；重复通知不会改写它来改变原意图。received/durable/lost/invalid使用无符号十进制字符串，避免大计数经过浮点数丢精度。pending和alarm_mask是有界小整数。

如果operation_id非空，用 `/v1/operations/<ID>` 只读观察原操作。HTTP请求中断不证明未执行，不能重新POST这个内部请求。GUI状态门槛与message反映告警保持；还可按第9节启用告警缓存显示。界面不读取原始帧，也不自动完成告警Run。

## 7. 持久记录怎样对应

```text
controller/
  snapshot.json                         含固定输入绑定
  ecc.jsonl                             普通Stop或原UNKNOWN的状态记录
  network/
    operations.jsonl                    alarm-held → accepted → running → terminal
    operations/internal-alarm-.../
      request.json                      唯一规范请求
  operations/internal-alarm-.../
    operation.jsonl                     父原生操作或独立负向操作
    stop/ 或 stop-production/            具体硬件/生产阶段
```

`alarm-held`保存观察字节及 `getdaq.ecc-alarm-observation:1` 域的SHA-256，绑定父快照和已选择的操作ID。观察可能先保存、后续请求尚未接受；重开时保留这一不完整边界，不自动补交请求。

健康运行使用普通Stop，原生ECC日志可提交Ready；UNKNOWN路径使用独立负向操作，不改原失败记录。非生产阶段只保存alarm-held，没有虚构的硬件操作目录。不同文件的作用不能互相替代。

## 8. 怎样复现本机验证

```bash
cmake --build build-ice-compat --target ecc-network-service-tests getdaq-ecc-server -j3
python3 tests/run_ecc_network_service_tests.py \
  build-ice-compat/ecc-network-service-tests build-receiver/getdaq-receiver \
  build-native-catalog-service/getdaq-supervisor build-ice-compat/getdaq-ecc-server \
  --output-parent build-ice-compat/ecc-alarm-stop-evidence \
  --scenarios alarm-running-0 alarm-running-1 alarm-paused alarm-stop-loss \
              alarm-ready alarm-disabled alarm-restart alarm-during-stop --jobs 3
```

这是模拟板端、同一物理主机上的实际ECC、数据服务与receiver进程测试，不连接现场板。故障场景可能保持UNKNOWN/告警门槛或监听会话；退出时的测试夹具清理不属于产品恢复。最终普通/插桩结果、具体执行范围及尚未验收项目见[验证记录](VALIDATION.md)。

新增UNKNOWN及实际Qt告警组可单独运行：

```bash
python3 tests/build_qt_control_validation.py \
  tests/native_gui_probe.pro build-native-gui-probe --jobs 2
python3 tests/run_ecc_network_service_tests.py \
  build-ice-compat/ecc-network-service-tests build-receiver/getdaq-receiver \
  build-native-catalog-service/getdaq-supervisor build-ice-compat/getdaq-ecc-server \
  --qt-probe build-native-gui-probe/native-gui-probe \
  --output-parent build-ice-compat/ecc-alarm-gui-evidence --jobs 3 \
  --scenarios alarm-unknown-prepare-0 alarm-unknown-prepare-1 \
              alarm-unknown-start qt-alarm-0 qt-alarm-1
```

Prepare两个场景在Configure前失败，没有receiver；Start失败及两个Qt场景使用实际receiver。Qt探针先进入Running，测试夹具才从模拟板端注入通知；通知经过自有Ice回调和后台策略，界面通过认证HTTP读取结果，不直接替模拟服务执行Stop。探针验证界面所发信号、原告警缓存和未完成Run，不是人工实机操作或目标Windows Kit验收。完整证据以验证记录为准。

## 9. 在GUI里启用查看，逐步理解结果

### 9.1 两个开关承担不同职责

GUI的YAML在 `getdaq` 下增加可选字段；不要放进要求恰好11字段的 `native_ecc` 对象：

```yaml
getdaq:
  native_alarm_status: true
  # native_ecc 和 nodes 等其他配置仍须完整填写。
```

这是配置片段，不是完整部署文件。完整模板见[envoy.native.example.yml](../config/envoy.native.example.yml)。旧配置省略该字段时默认false，继续使用原身份/状态入口。

| 服务端alarm_stop_policy.enabled | GUI native_alarm_status | 实际行为 |
| --- | --- | --- |
| false | false | 不自动调度告警停止，GUI只用原状态入口 |
| false | true | GUI要求并查看有效告警缓存，显示disabled；不会启用后台停止 |
| true | false | 服务端照常处理告警，GUI仍受状态中的核对门槛约束；不查询详细告警缓存 |
| true | true | 服务端处理告警，GUI另核对/记录原依据、自动操作ID及阶段 |

后台是否自动停生产由ECC固定部署决定。勾选GUI查看框不会改变硬件订阅、ECC固定配置、sender或receiver。

### 9.2 怎样操作和查看

1. 在未连接、没有活动Run时打开 **Nodes and paths**，选用原生整系统模式，填写已核对的服务、摘要、持有期和来源映射。
2. 勾选 **Require and display ECC alarm cache (/v1/alarms)**，保存配置。该选项要求当前ECC支持此认证端点；旧服务没有该端点时，不能在启用状态下继续正向操作。
3. Connect后先核对身份和状态，再读取告警缓存。未取得有效缓存期间，Start/Progress等正向动作保持禁用；HTTP轮询只处理小段元数据。
4. 无保持时可按正常流程运行。收到告警后，状态行显示服务给出的保持原因，控制阶段显示 **Native requires inspection**，日志显示原自动操作ID及held/accepted/complete等阶段。
5. 日志中的received、durable、lost是触发时保存的告警通知计数，alarm mask是告警位。它们不是MFM帧计数或字节速率；后续重复通知不会改写原触发依据。
6. 活动Run的journal.json保存 `native_alarm_cache` 原JSON字符串，包括实例/快照、原操作ID和原观察。GUI轮询看到阶段改变会更新这个本地引用，不生成另一条硬件停止请求。

较大的计数也按十进制字符串解析并显示。例如9007199254740993仍显示全部数字，不经double取整。这是显示精度证明，不代表真实运行已有这么多通知。

### 9.3 哪些显示容易误解

| 显示/文件 | 正确解释 | 后续处理 |
| --- | --- | --- |
| watching | 尚无保持的缓存观察 | 仍须检查ECC/接收状态及现场条件，不是实时健康证明 |
| complete + Ready + latched=true | 此自动停止已记录成功，生产进入已知停止阶段，告警仍保持 | 查看原操作与告警依据，不Start、不把Ready当恢复完成 |
| complete + controller_outcome_unknown=true | 原失败操作仍UNKNOWN，负向停止仅完成其允许的生产停止 | 保留原失败ID、sender/receiver记录；不以内部停止ID替换故障结论 |
| held-no-run | 非生产阶段收到告警，只保持门槛 | 没有新的Stop请求，不意味着通知可忽略 |
| reopened-held/startup-reconciliation | 重开后的历史或启动核对边界 | 不重建旧对象、重发停止或接管新实例 |
| 缓存超时、身份/摘要错误或解析失败 | 当前GUI不能证明告警观察有效 | 保持正向门槛并核查固定部署/网络；恢复旧返回值也不会清保持故障 |

告警发生在活动Run时，GUI不自动Breakup、断开sender、写run.csv、推进Run号或触发完成信号。Finalize也不能绕过门槛。退出GUI会把本地Run标记Interrupted，但不撤销ECC已接受动作，不将其变成Complete。

只读观察故障和硬件停止必须分开。相同已核对实例下的人工明确生产停止仍使用既有负向入口；实例/摘要或凭据变化时不能用负向动作接管未知对象。该入口不清原UNKNOWN，也不构成完整恢复。告警/硬件恢复、实机保护和断电持久性尚未验收，详见[故障停止](PREPARATION_FAILURE_STOP.md)及[GUI记录与恢复](ECC_GUI_CLIENT.md)。

Prepare失败且原自动告警停止已完成时，可在原断电/订阅收尾后明确确认该告警并只读核对，见[Prepare告警确认操作](PREPARATION_RECONCILIATION.md#9-prepare-告警策略已保持时的明确确认)。此入口不处理普通Running告警成功Stop后的恢复，也不证明物理故障已修复。

## 10. 普通告警已经成功 Stop 后，怎样明确收尾

本节处理父状态仍然**已知**的告警：正在 Running/Paused 时自动 Stop 成功，父状态回到 Ready；或者本来就在 Ready，策略只保持告警、没有另外发 Stop。它与失败 Prepare 或 receiver UNKNOWN 的恢复入口不同。

停止成功后，原发送连接和接收器仍然存在。新的 **Acknowledge stopped alarm, close session and power off...** 操作会由 ECC 后台执行正常收尾：

1. 核对当前实例、固定配置、原已知 Ready 操作、自有告警观察和原请求记录。拒绝源、receiver、输入、存储或其他 UNKNOWN。
2. 沿用原采集对象执行 Breakup，关闭原连接，让接收器正常排空、同步、退出并核对 seal。不会改为强制 Abort，也不会换会话接收剩余数据。
3. 数据侧确认停止后，沿用原准备对象执行 Unprepare：监测、逐板断电，全部确认后注销原自有订阅。
4. 只读核对当前停止/未就绪/断电位、自有订阅关闭和原描述，保存实际观察与收尾审计摘要；最后数据查询后再检查这些证明。
5. 原生 Reset 提交已知 Described，网络在同一完成行记录原告警确认与策略复位。只有该完成行保存成功，才重新监视告警。
6. 操作台保存原 Run 为 Interrupted，保留时长不完整和此次确认结果，再允许操作者手动执行新的 Prepare。

**接收文件成功 seal 不等于原 Run 正常完成。** seal 证明这次已经收到的数据通过了接收侧结束检查；发生告警的 Run 仍保留中断标记，不自动生成 run.csv、不自动推进编号。原文件、会话 ID、PID 和目录身份保留；新采集仍应使用新的未占用运行目录。

### 窗口操作顺序

启用 `native_alarm_status`，连接原 ECC 服务，先在日志中核对原告警以及 `complete` 或 `held-no-run`。确认父状态为 Ready、结果已知；如果显示 UNKNOWN，使用对应故障清理流程，不能套用本入口。

选择 **Acknowledge stopped alarm, close session and power off...**。对话框默认 Cancel，取消不提交；Yes 才先保存本地新意图，再提交一次 `release-stopped-alarm`。等待原请求 ID 的终态，HTTP 202 只表示接受。正常关闭、接收排空和断电需要时间，不要按新 ID 再次提交。回复丢失只观察原 ID。

成功时同时检查：本次命令 complete、父状态 Described、无控制/数据待核对、原 Run 为 Interrupted。然后再按 Progress all 重新 Prepare。确认旧告警不证明温度、电源或接线等物理原因已经修复；新的 Prepare 必须重新监测。

### 两个原操作字段怎样填

命令沿用严格协议中的 `failed_operation_id` 字段，但本入口中它表示**原已知 Ready 对应的操作 ID**，原操作记录仍是成功，不会改写成失败。

| 情况 | failed_operation_id | alarm_operation_id |
| --- | --- | --- |
| Running/Paused 告警自动 Stop 成功 | 原自动 Stop ID | 同一个原自动 Stop ID |
| 本来就在 Ready，只保持告警 | 该原 Ready 的 Configure/Stop ID | 空字符串，字段仍必须存在 |

新 `operation_id` 必须不同于原操作 ID。只有此已知状态收尾命令允许两个原 ID 相同、或允许 `held-no-run` 对应的空告警操作 ID；UNKNOWN 的明确告警恢复命令仍要求三份不同 ID。空字符串不能代替一个实际存在的自动 Stop ID。

### 收尾失败与重启

若连接关闭、排空、seal、断电或只读核对有一步失败，保留已经发生的效果和原告警，不回滚、不重放。尤其可能出现“接收文件已 seal，但断电结果 UNKNOWN”：这时不能宣称硬件恢复，也不能因接收侧已完成就 Start。恢复条件须根据原失败阶段重新核对；本命令不允许用另一个 ID 重做已发生的步骤。

原自动 Stop、派生的原生 Breakup、本次原生 Reset 和网络确认分别保留记录。原生最终审计包含实际观察/收尾文件的 SHA-256 清单；网络完成行保存 `alarm_policy_reset`、`alarm_stop_operation_id` 和 `alarm_observation_sha256`，不清接收故障策略。网络完成记录保存失败时，即使子层已到 Described，控制入口仍保持核对门槛。

服务冷重启只验证历史绑定，不重新建立旧采集对象，不重复关闭/断电，也不依据历史成功自动解除启动门槛。目标平台、真机和持续吞吐的验证范围以[验证记录](VALIDATION.md)为准。

### 重新 Prepare 时恰好遇到状态轮询

恢复后的状态和告警缓存需要重新查询，旧回复不能把新状态覆盖回旧告警。界面会等待恢复后的有效元数据；新的查询失败则显示需要检查，不以“之前恢复成功”为理由清除后来的故障。

如果人工按下 Progress 时恰好有只读查询在途，操作台最多暂存一次这次点击。新观察仍然属于原状态和原控制操作 ID、全部门槛仍通过，才保存正式请求并提交一次；重复点击不增加请求。观察期间状态或原 ID 改变、出现故障或等待过期，就取消这次待处理点击并记入日志，需要人工重新检查后操作。这不是自动恢复，也不跨重启重放；提交后的请求仍按原 ID 观察，不能用新 ID 重发。
