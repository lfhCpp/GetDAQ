新版可直接使用主界面 **主控机 / 从控机** 按钮，见[最少操作教程](SIMPLE_MASTER_SLAVE.md)。以下保留本机服务管理入口说明。

# GetDAQ 内部管理本机后台

主界面的布局和操作按钮保持原样。ECC、数据监管和接收器仍是独立进程，正常操作由 GetDAQ 管理，不需要执行后台管理脚本。

## 日常操作

1. 打开 GetDAQ。主程序记住当前安装中最后一次手动打开或选用的配置；首次安装可通过 `site/gui-config.txt` 指定初始 YAML。显式 `--config` 优先。
2. 点击 **Connect**。本机原生部署会先检查监管服务与 ECC：匹配的进程直接复用；缺失的服务通过离线配置检查后启动。随后核对监管计划身份和 ECC 身份，再执行原来的连接流程。只有身份查询，不自动 Describe、Prepare、Configure 或开始采集。
3. 通过原有 Progress all、Run start、Run stop 操作硬件。远程 ECC 配置继续连接已有远程服务，不在操作台上冒充启动远端服务。
4. 关闭窗口时选择 **Close window; keep services**，后台保持运行；或选择 **Finish Run and close services**，由正常 Run stop 完成当前 Run、排空接收器并保存记录，再关闭后台。仅已配置未采集的会话先通过正常 Regress/Breakup 释放。

等待正常收尾失败或超时会保留后台与记录，不转成成功退出。如果窗口不拥有当前运行、硬件状态 UNKNOWN 或有未解决的操作，不能通过关闭动作绕过核对。

## 服务管理窗口

Tools 原有服务入口与 Nodes 中的服务入口提供四个常用操作：

- Start / check backend：启动缺失的本机后台，复用匹配实例，并核对身份。
- Stop backend safely：读取服务状态；仅在已确认停止、无待核对操作且接收器已结束时关闭进程。
- Refresh process status：显示实际 ECC、监管和接收进程；进程存在不等于硬件或接收就绪。
- Terminate backend processes：故障修复入口，需要明确确认。按当前用户、安装目录和配置定位 ECC/监管，接收器还须匹配源 ID、输出目录、地址端口及参数/工件摘要。先 TERM；仍未退出才再次询问 KILL。它不停止板卡，不完成 Run，也不解除 UNKNOWN。

个别服务和独立接收器的旧入口放进 Advanced；常规受管采集不用单独开 receiver。

## 启动与隔离

GetDAQ 程序内部负责单实例检查，重复打开唤起已有窗口。短暂启动锁不传给后台，实例通信 socket 设置 close-on-exec，避免 ECC 继承主界面锁造成无法重开。`start-getdaq.sh` 只负责选择运行包及加载现场网络环境，不再依赖 Python 的 GUI 启动/查进程脚本；也可以直接启动运行包中的 GetDAQ。

控制与数据传输仍隔离：GUI → ECC → 监管/receiver；CoBo 原始数据直接进入 receiver。后台独立进程保持故障隔离，但用户不再需要记住多个启动命令。

本节旧本机入口的自动配套启动针对同一台 Linux 主机上的固定 ECC 和数据监管。新版 Master / Slave 入口可通过已配对节点管理跨机多套后台；由节点启动固定程序，不执行任意远程命令，详见[主从控最少操作教程](SIMPLE_MASTER_SLAVE.md)。

## 同一台主机上的多实例

多实例指多套独立 ECC、数据监管和 receiver 后台。正常入口只有一个主界面，在 **Configuration → ECC / data service instances…** 或 **Tools → ECC / data service manager…** 中管理。

1. 点击 **Add service instance…**，填写另一组板卡 IP、接收 IP 和数据目录。基于已有实例新增时，软件复制电子学设置、建议空闲端口和独立目录，并清空板卡 IP。
2. 在同一个管理窗口的实例列表中选择一套，使用 Start / check backend、Stop backend safely 或 Refresh process status。列表显示 ECC 端口和板卡地址。
3. 点击 **Use selected instance in Run Setup** 才将所选实例用于主窗口采集控制。其他实例的后台继续独立运行。当前已连接或正在执行动作时，须先断开再切换 Run Setup。

每套实例保留独立的端口、凭据、状态/审计和数据目录。启动前检查端口和板卡代理冲突，不能用两个 ECC 同时控制同一块板。停止和故障终止只作用于所选配置。

首次配置内置单板 pedestals 预设。日常只需编辑网络、端口和目录；电子学模板、服务程序目录、回调内部端口等收在 Advanced。预设明确标注硬件v5、AGET2.2和AsAd槽0，不代表已探测实物，实际连接不同时在高级设置中选择对应电子学模板。

单个主界面的采集按钮控制当前选用的实例；服务管理窗口可以启动和管理多套后台。手工命令行指定不同配置仍可打开独立操作台，但不是界面“新增服务实例”的行为。
