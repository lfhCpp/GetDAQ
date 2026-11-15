# 从界面启动独立 ECC 与数据程序

在同一台Linux电脑上，可使用 **Tools → Start independent ECC / data services…**，或 **Run Setup → Nodes / Paths → Start independent ECC / data services…**。程序目录选择完整运行包的 `bin`；只编译控制端不会获得Linux数据接收程序。

该窗口用独立参数列表启动程序，不调用shell。启动后的服务是独立进程：关闭启动窗口或GetDAQ均不会停止服务。窗口显示PID和日志路径，启动记录保存在选定日志父目录下的独立私有子目录。`command.json`保存程序及参数（认证文件只记录路径），`process.json`保存PID，`service.log`保存程序输出。

## 选择哪种程序

| 选项 | 如何设置 | 启动后的行为 |
| --- | --- | --- |
| ECC control service | 选择已经检查的 `ecc.json` | 先执行离线check-config，再独立启动getdaq-ecc-server；不自动Describe/Prepare/Start |
| Managed data service (supervisor) | 选择 `supervisor.json` | 先检查，再独立启动getdaq-supervisor；接收器由ECC正常准备/配置流程按固定计划创建 |
| Standalone data receiver | 选择已有空输出目录、填写本机接收IPv4、数据端口和状态端口 | 独立启动getdaq-receiver并立即监听TCP；状态接口仅监听127.0.0.1，导出关闭 |

前两项可选择 [实机配置窗口](RUN_SETUP_HARDWARE.md) 生成的配置；如果当前操作台已加载对应envoy.yml，会预填同目录配置文件。它也可选择已有部署的配置，用于手动启动或重开服务；重开已有审计目录仍受服务端历史/UNKNOWN核对规则约束，不能通过此窗口绕过。

正常native实机采集应启动 **ECC + Managed data service**，再回主窗口Connect并逐阶段操作。不要另外给同一来源手工启动Standalone receiver，否则会与管理计划冲突。

Standalone用于板端发送器由外部程序管理的独立接收场景。需要勾选该来源不由ECC/supervisor管理的声明；输出目录必须已存在且完全空，IP属于当前电脑、数据/状态端口不同且可绑定。当前快捷启动固定使用TCP、64MiB接收池、2GiB最低可用空间保护，其他协议或资源参数沿用receiver命令行部署。它不配置板卡、不操作发送器，也不会自动注册进native Run。

端口预检查只用于减少误启动，最终监听结果仍看服务日志；“独立进程已启动”不是服务就绪或硬件验收。退出可能很快发生，下一步需查看service.log并用主窗口Connect/状态查询确认。窗口不自动结束旧服务。结束采集应先停止发送端并按对应流程核对EOF、正常退出和封存，不能用关闭GetDAQ代替。

该入口只启动本机Linux程序，Windows操作端继续通过native HTTP访问Linux服务。已通过小型本机回环测试：独立receiver在启动窗口销毁后仍可查询状态，随后测试仅停止自己的进程；不发送DAQ数据或访问板卡。其他操作与边界见 [运行手册](OPERATIONS.md) 和 [数据服务](DATA_SERVICE.md)。
