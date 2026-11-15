# 状态、警告接受与 Idle 恢复

主界面布局保持原样。使用 **Recovery → Status, warnings and reset…** 打开集中状态窗口。

已有本机配置重新打开 **Nodes / Paths** 时，可直接点击 **Start / check ECC + data service**，无需重新生成配置或连接板卡。程序核对配置与服务身份，复用匹配的后台，不自动执行 Describe、Prepare 或 Start。首次配置或修改参数后，也可直接点击启动按钮：缺少模板、控制IP或停采确认时，窗口日志说明缺项；输入齐全后自动保存、离线校验并启动服务。按钮仅在检查或启动进行中暂时禁用；失败后可修改并重试，不会静默变灰。

Local backend services 窗口没有当前配置时，Start / check backend 仍可点击并打开配置向导；选择 Use in Run Setup 后返回服务管理窗口，再点 Use selected instance in Run Setup 才由主窗口采用并记住该实例。其余停止/进程管理按钮在选定本地部署后可用，空配置时日志会解释原因。

## 通知与真实状态

- Acknowledge selected：确认已读，不修改后台结果。
- Accept selected warning：记录接受普通警告，按当前配置保存。警告不会因为已接受而被改写成“读回已验证”。
- Clear acknowledged notifications / Clear displayed log：清理界面显示，原文件日志保留。
- Recheck state：重新读取当前 ECC 身份和状态，不执行硬件写入。

可继续的兼容性警告应当明确接受后继续工作。执行结果未知与告警通知不同：连接丢失后的一次 Start 可能已经执行、未确认停止时板卡可能仍在发送，这类情况需要核对结果，不能仅凭“硬件未损坏”认定下一步可安全执行。

## 当前 readDataDelay 兼容警告

旧 CoBoNode 的 triggerInit 写入 readDataDelay.AGET0–3=14，没有紧接着强制读回。新实现增加的读回检查在当前板卡上得到255；这不能单独证明是硬件损坏，也不能证明该时序值已生效。

在 Nodes 的配置编辑器中可以明确接受 **Accept legacy readDataDelay=255 readback as a warning**。保存后产生配置专属的 `readback-warning-policy.json`，包含被接受的板端代理和节点名。ECC 启动时接收该文件路径；默认仍严格。

该策略只允许已绑定板卡的 ctrl.readDataDelay.AGET0–3、8位字段返回255时继续。其他返回值、其他寄存器、写入/通信失败、停止/初始化位与保护检查不放宽。执行审计记录实际值、差异、策略摘要及 `accepted_warning_unverified`，明确保留 `readback_verified=false`，不伪造读回相符。

## 重置与初始 Idle

关闭主窗口不会使物理硬件或独立 ECC 回到 Idle。全新控制会话连接后应为 Idle，已经 Prepared/Running 的硬件不能只因窗口关闭而改写状态。

- 已知、停止且没有待核对项：**Reset to Idle** 走原有 Regress/Breakup/Reset 流程，逐步核对结果后到 Idle；失败停止，不无限重试。
- 只因空闲/已描述状态的服务冷重开而待核对：可用 **Recover Idle / accept compatibility warning…**，读取并确认停止、AsAd未上电、没有接收会话，然后保留旧审计并建立新的 Idle 会话。
- 本次特定早期 Prepare 失败也可使用该恢复入口：必须验证原失败恰为订阅/上电之前的延迟255读回差异，原写入应答已收到，没有未知写入或活动订阅；还会重新核对原配置绑定和现场状态。

目前这个冷恢复入口严格限于单块 CoBo、无 MuTaNT、硬件版本5，以及生产使能/中断/initDone/AsAd上电掩码均为0、无接收进程和接收会话的情况。它不会自动重放旧 Prepare，不会删除旧日志，不会给过去的 Run 补写成功。更晚的 Prepare、已有订阅、已上电/运行硬件或未结束数据会话仍需对应的清理/恢复流程。

核对通过后，程序保存原审计文件摘要和新鲜只读观察，停止本配置的旧后台，生成并检查新配置，启动新的 Idle 后台并打开对应窗口。下一次 Describe/Prepare/Configure/Start 仍由操作员明确执行。时序读回被接受不代表波形或数据质量已完成实机验证。
