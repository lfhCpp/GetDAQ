# GetDAQ

GetDAQ 是 GET 采集操作界面。当前固定方案使用原版 **getEccSoapServer** 控制硬件，
由 ATTPC **DataExporter** 独立接收、落盘和导出数据；也兼容其他主机上已经运行的
**DataRouter**，对方不需要安装或启动 DataExporter。

主界面布局保持原样。主从机只管理配置分发、缓存和后台实例；原始数据不经过 GUI 或 ECC。

- [当前功能范围与已清理路线](docs/PRODUCT_SCOPE.md)
- [原版后端配置、启动及外部 DataRouter 连接](docs/ORIGINAL_BACKENDS.md)
- [构建与安装](docs/HOST_SUITE_BUILD.md)
- [原版配置编辑器](docs/ORIGINAL_CCONFIG_EDITOR.md)
- [直连 CoBo 只读信息工具](docs/DIRECT_COBO_INFO.md)
- [自动采集](docs/AUTOMATIC_RUN.md)
- [波形查看、事件与通道](docs/WAVEFORM_VIEWER.md)
- [简易 PDF 说明书](docs/manual/GetDAQ-Quick-Guide.pdf)
- [macOS 安装](docs/MACOS_INSTALL.md)

## 使用

已有 ECC：选择 **Master → Connect existing ECC**，填写 ECC 电脑 IP 和 SOAP 端口，
把数据目标设为实际 DataRouter / DataExporter 的接收 IP、TCP 数据端口；Sentry 单独配置。

新建服务：选择 **Master → Create ECC / data services**，添加执行主机，选择主控机的
原始 XML/XCFG 配置目录，填写 IP / 端口，启动并使用配置。Slave 自动缓存文件。
支持同机多套后台实例，各自使用独立端口、数据目录和进程身份。

原版配置编辑器和 CoBo 只读信息工具从菜单打开。运行记录、已有 Run 覆盖确认、告警管理、
自动采集及波形的事件/通道选择保留。重写的原生 ECC、receiver/supervisor、原生扫描与交接
等路线已从当前构建和界面移除。旧原生配置会明确报错，不会自动转换后操作硬件。

## 验证边界

必需的 GET/Ice/Boost 等源码随项目构建，无需另装旧 GET；基础编译工具和 Qt 仍需要安装。
原版核心源码保持原样，少量入口适配保存在独立补丁中。

当前可执行的软件测试不能代替实物采集验收；原生 macOS 编译和实机功能仍需对应主机验证。

Configuration and storage paths: [配置文件与数据目录说明](docs/CONFIGURATION_AND_DATA_PATHS.md).
