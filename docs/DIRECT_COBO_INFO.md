# 直连 CoBo 查看信息

从 **Tools → CoBo information (direct, read-only)…** 打开。

1. 填 CoBo 板卡 IP，不是 ECC 或接收电脑 IP。
2. 默认端口 `46001`、Ice object identity `HwNode`。
3. 点击 Connect / refresh CoBo。

显示实际节点名称、设备列表、hardwareVersion、firmwareRelease、firmwareDate、
firmwareEnable 和 intEnable。结果是一次查询的快照，30 秒后过期；修改地址会清除旧结果。
不需要 AsAd 配置或 ECC 采集工程。它不更改 ECC 状态，也不执行初始化、Prepare、
Configure、Start、寄存器写入或权限覆盖。

GUI 调用独立的 `getdaq-cobo-info`。该工具只接受设备列表和上述固定读取命令，
不支持其他寄存器或写入操作。Ice 编码固定 1.0，RPC 超时 4 秒，自动重试关闭。
例如：

```sh
./bin/getdaq-cobo-info devices 'HwNode:tcp -h 192.168.40.64 -p 46001'
./bin/getdaq-cobo-info read-register 'HwNode:tcp -h 192.168.40.64 -p 46001' ctrl firmwareRelease
```

原 GetController 应用及其独占构建/绘图代码已移除。ECC、DataExporter、配置编辑器共用的
原版接口、解析和底层依赖保留；历史 `source/` 对照树不动。
