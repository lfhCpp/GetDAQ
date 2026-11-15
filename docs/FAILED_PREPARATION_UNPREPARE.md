# 原 Prepare 故障后的断电与订阅收尾

## 1. 为什么需要这一步

Prepare 失败时，部分 AsAd 可能已经上电，某些板的告警订阅也可能已经建立。此时直接重建准备对象会丢失已经尝试的步骤和订阅身份；普通 Reset 又被 UNKNOWN 门槛阻止。

`unprepare-after-failure` 沿用同一失败准备对象。先检查该原操作的明确停生产证据，再观察实际上电的 AsAd、断电，并关闭本对象已确认拥有的告警订阅。它完成的是故障收尾，**不会解除父 ECC 的 UNKNOWN，不表示可以立即开始下一个 Run**。

当前仅适用 Prepare 阶段失败：最近确认状态为 Described，仍保留 Failed 准备对象，未创建采集对象。Configure/运行失败、服务冷重启接管和完整硬件恢复不使用此入口。

## 2. 在哪台电脑操作

操作台通过网络发送低频命令。真正的寄存器操作和订阅清理由原 ECC 主机执行；Windows Qt 界面不会接收或转发原始事件。

需要保持原服务实例、原配置/工件绑定及控制权。审计根损坏或占用保护失效时，不能借本功能绕过。板端 getHwServer/IceEirfu 不改动；依赖复用本地原生兼容组件，没有增加第三方库。

## 3. 完整操作顺序

1. 查看 Prepare 的原失败ID、当前状态和错误。确认没有活动 Run，也没有遗留的待核对客户端意图。
2. 使用 **Native fault: stop production only (keep UNKNOWN)**，明确停止原对象的生产。等待原ID结果，不换ID自动重发。
3. 必须得到停生产操作的最终 `phase=complete`，且原审计及停生产凭据保持完整。仅看到 firmwareEnable/intEnable 为0不能替代已经确认的停止记录。
4. 点击 **Power off original failed preparation...**。
5. 阅读确认框，默认 **Cancel** 不发请求。选择 **Yes** 才持久记录并提交收尾请求。
6. 后端先核对完整目标集及所有原停止阶段，再清未就绪标志、观察实际上电板、分别尝试断电。
7. 所有观察和断电都确认、读回一致后，才关闭已确认属于本准备对象的告警订阅。未订阅的 Fresh 或已关闭的 Closed 不发送注销。
8. 查看最终结果。成功时原准备对象为 Unprepared；父状态仍为 Described/UNKNOWN，原失败ID仍保留，后续正向命令继续被禁止。
9. 收尾入口本身不解除 UNKNOWN。原 Prepare 的后续明确只读核对见[准备核对恢复](PREPARATION_RECONCILIATION.md)；需使用包含该独立命令的匹配服务/操作台，并检查其验证范围。不会自动重新 Prepare，也不提供 Run 恢复。

同一已接受请求ID只观察旧结果；换一个新ID也不能重复已经尝试过的收尾。数据接收器不会被启动、kill、重启或伪造封存。

## 4. 后端具体做什么

前置要求：

| 要求 | 检查内容 |
| --- | --- |
| 原对象 | 同一活跃 ECC 的原 Prepare 失败对象，无采集对象 |
| 原停止 | 原操作对应的完整停生产审计；全部活动 CoBo 的 pulser/firmware/interrupt 停止及全部 MuTaNT 停止均已确认 |
| 原数据 | 用原数据适配器重新确认停止，不沿用启动时声明 |
| 完整目标 | 全部声明 CoBo，包括未参与本次实验的板；核对名称、完整布局和本地访问权限 |
| 当前状态 | 全部 CoBo firmwareEnable/intEnable 和 MuTaNT run 观察为0 |
| 订阅 | 只允许本对象已知 Active、从未发起的 Fresh 或已确认 Closed；未知订阅拒绝 |

执行顺序：每块板清 initDone/mutantMode，读取实际 asadEnable 掩码，仅针对实际上电的 AsAd 读取保护状态，再将 asadEnable 写为0并读回。前一块的监测或断电失败，在审计还能可靠记录的前提下仍尝试其他独立板的断电。

监测复用旧 CoBo Unprepare 原理：选择实际上电的 AsAd，读取 interruptStatus1。它记录保护信息，不清保护状态。不能用实验配置中的活动掩码代替实际供电掩码。

全部阶段确认后，再检查各板 asadEnable/initDone/mutantMode 为0，随后注销原对象的已知自有订阅。不会注销其他客户端，也不会复用旧 ECC 的订阅ID或 reset 告警服务。MuTaNT 电源不改。

## 5. 失败时怎样理解

| 情况 | 结果 |
| --- | --- |
| 没有完整原停生产凭据 | 不执行断电或订阅操作 |
| 元数据、生产停止或订阅身份不符 | 全目标预检查拒绝，不猜测归属 |
| 监测失败 | 尽量完成其他独立断电，保持故障，不取消订阅 |
| 断电已应用但应答丢失 | 仍为未确认，不因后来读到0而重新执行或宣称成功 |
| 订阅注销未确认 | 保持原未知状态，不用新对象或新ID再次注销 |
| 审计/存储失败 | 停止后续动作，不执行无法可靠记录的命令 |

各阶段在原对象里保存“首次尝试”和“确认”状态。成功阶段不重发，失败或未知阶段也不重发。原生 ECC 外层还保持一次收尾尝试门槛，防止新请求ID让原动作重放。

## 6. API 与原生接口

认证 `POST /v1/operations` 使用原控制请求字段，将 `command` 设为 `unprepare-after-failure`。原实例、快照、客户端和持有期必须一致。HTTP202只表示接受记录；使用 `GET /v1/operations/原请求ID` 查询最终 phase。

服务端接口为 `NativeEccCoordinator::UnprepareAfterFailure`；硬件层为 `SystemPreparationClient::UnprepareAfterFailure`。两层都有自己的新审计目录。外层意图绑定原失败ID、原停生产ID和父快照，并明确记录 `parent_ecc_unknown_preserved:true`、`sender_changed:false`、`receiver_sealed:false`。

成功仍不是父状态恢复凭据。不要手工改这些字段为已恢复，也不要通过删除旧日志重新启动来绕过门槛。

## 7. 当前验证范围

原生模拟硬件验证涵盖两种 Ice 编码成功、缺少停止证据拒绝、停止应答丢失拒绝、断电应答丢失不重复、监测失败后其他板继续断电，以及原订阅仅在全部确认后关闭。实际 ECC/两数据服务联调验证两编码及断电丢应答；数据服务保持 Fresh，未启动 receiver，没有采集 raw。

普通和插桩、界面、交付包的最终结果以[验证记录](VALIDATION.md)为准。它不证明现场硬件恢复、全部固件兼容、跨主机控制权或断电持久性。
