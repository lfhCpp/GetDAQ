# 在界面直接连接 CoBo（无 AsAd 也可使用）

主窗口的 Connect 用于模拟/旧ECC/native ECC采集控制。仅检查板端通信时，使用独立入口：

**Tools → Connect CoBo directly (read-only)…**

也可从 **Nodes / Paths → Connect CoBo directly (read-only)…** 打开。

在Linux完整运行套件中填写：

| 字段 | 本次已验证的现场值 |
| --- | --- |
| CoBo IPv4 (board) | `192.168.40.64` |
| Board control port | `46001` |
| Ice object identity | `HwNode` |
| Installed bin directory | 当前完整运行包的 `bin`，包含 `getdaq-hardware` |

点击 **Connect / refresh CoBo**。成功后显示真实节点名（本现场为 `CoBo[0]`）、设备列表、hardwareVersion、firmwareRelease、firmwareDate，以及firmwareEnable/intEnable两个控制状态位。窗口只调用设备元数据、版本ROM和固定状态位读取，没有写入、上电、校准或采集入口。

这些是真实板端RPC查询结果，不走模拟分支。它们是短时只读快照，不是采集Ready证明，也不代替ECC连接；30秒后撤回过期值，需点按钮重新读取。修改地址或断开观察会清空旧值并取消未完成查询，不向板卡发送Stop/Reset。

环境变量 `GETDAQ_COBO_IPV4` 可预填板卡地址；`GetDAQ --cobo-readonly` 可在启动时打开该窗口并执行一次只读连接检查。普通启动不会自动查询板卡。关闭窗口只结束本地读取，不改变板卡状态。

当前没有接AsAd时，可以完成上述通信检查，但不能因此执行pedestals采集。接好AsAd并确认槽位、芯片型号及实验参数后，再按 [实机配置窗口](RUN_SETUP_HARDWARE.md) 建立正常ECC采集配置。
