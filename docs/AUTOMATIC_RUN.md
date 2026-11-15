# Automatic Run 与日志级别

Run setup 顶部右侧提供 Automatic Run，与左侧标题对齐。Apply 与标题/Enable 同行，进度文字在条件下方独占整行并支持换行，双条件同时显示不会挤在按钮旁。较窄窗口会收为同一区域标题右侧的 Automatic Run… 按钮；点击可打开完整设置。右上方 Run record parameters 和下方左曲线、右日志的等宽布局保持。

## 设置与触发

1. 勾选 Enable。
2. 独立选择 Time、File size 中的一项或两项。两项同时选择时是 **OR**：先达到任一阈值就触发；不要求同时达到。不使用文件数量作为条件。
3. Time 可选秒、分钟、小时；File size 可选十进制 MB/GB，统计本计数窗口内所有配置 CoBo 的 raw 字节增量，MuTaNT 不计入，同一 Sentry 路径不会重复累加。
4. 选择 Stop 或 Next Run。Next Run 的 Runs 是本次序列总轮数，包含第一轮；0/Unlimited 表示持续循环。
5. 点击 Apply 应用，或通过 Run start 应用当前设置并开始采集。只修改值未应用时显示 Changes pending。Run 开始后条件和 Apply 锁定，自动间隔期间也保持锁定。Enable 始终可切换；关闭只取消自动触发/续跑，当前采集或已开始的收尾继续；重新开启按已保存条件重建计数窗口。需要停止采集时使用 Run stop。

时间从确认 Running 后开始；大小以本窗口的原始接收会话/路径作为基准。不会用速率积分、磁盘容量或文件数量冒充大小。轮询、缓冲和停止所需时间会使最终采集时间/大小略超过阈值。

## 两轮之间的收尾与间隔

达到阈值后复用现有 Run stop、receiver 停止/封存、catalog/归档、Run snapshot 和 journal 完成流程。只有收到当前 Run **成功完成**的信号，才递增已完成轮数并考虑续跑。单纯硬件到达 Ready/Prepared 不算完成。

Next Run 在上一轮成功收尾后 **至少等待 3 秒**，随后再次核对新鲜状态、无待完成操作、无 UNKNOWN/故障及控制可用性。Native 在需要时先 Configure 新的接收会话，确认 Ready 后才 Start；Ready 接收端必须满足既有空会话校验。慢收尾和慢准备会延长实际间隔。不能靠等满 3 秒绕过收尾或就绪检查。

收尾失败、超时、观察失效、会话/PID 改变或字节计数倒退都会取消自动续跑，并显示诊断。下一个 Run 号已经存在时也停止自动循环，交给用户明确处理；自动模式不授予覆盖旧 Run 的许可。

## 手动操作

- 手动 Run start 开始新的计数窗口和轮数序列；Run stop 立即取消后续自动循环。
- Pause 暂停自动触发，Resume 从零重新计算时间和大小窗口。Run 的真实累计统计及 raw 数据不被清零。
- Apply 只在 Run/自动序列开始前可用；运行中、暂停中及两轮间隔均不能变更阈值/动作/轮数，控制层也会拒绝绕过界面的修改。Enable 使用独立接口，只开关已保存策略，不改变条件；确认 Running/Paused 后可开启，计数从启用时重新开始（大小基准使用新鲜观察）。关闭时不会发 Stop，已开始的收尾不会撤销；在两轮间隔关闭后，重新开启仅为下次明确 Run start 做准备。更改 Run 号、手动 Progress/Regress、断开、恢复和维护操作会取消待执行的自动续跑。
- Native 观察到外部/MCP Pause/Resume 时遵守同样的窗口规则；观察到其他外部状态操作会取消续跑。观察有轮询延迟。自动 Run 不接管扫描点自身的事件数停止流程。
- YAML 保存的是设置，启动/恢复程序不会自动重新武装；需要明确 Run start。手动查看状态或选择节点不会重置计数。

## 文件大小数据与部署

Legacy/Sentry 由原目录扫描返回 `data_written_bytes` 十进制字符串；兼容旧 Sentry 的实际 `data_written_gb` 目录大小报告。建议同时更新 `getdaq-sentry` 以保留整数字节精度。

Native 需启用 `getdaq.native_receiver_status`，并更新 ECC、数据主机 supervisor 与 receiver。新增认证的只读 `/v1/receiver-file-bytes/<source>`，沿固定 source、deployment、plan、instance、session、PID 绑定返回 `accepted_file_bytes`。现有 receiver-events 协议及事件数扫描保持兼容；旧服务不支持新接口时取消自动大小触发，不猜测数据。

Native 的 accepted_file_bytes 来自接收写入路径的累计计数，最终正确性仍由原封存/归档流程确认；它不是实时 fsync 落盘证明。控制软件只读取计数元数据，不拉取 raw 内容。新增查询不会发送硬件动作；没有改变接收数据的热路径。

## Event log

信息使用深灰色，警告使用橙色，错误使用红色。新自动 Run 日志和明确失败携带级别；旧无级别消息按原文本中的诊断词兼容分类。原消息文字、时间戳、多行内容、复制和纯文本导出、每次连接保存的日志文件保持不变；颜色仅作用于显示。
