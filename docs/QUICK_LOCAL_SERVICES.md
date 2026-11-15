新版可直接使用主界面 **主控机 / 从控机** 按钮，见[最少操作教程](SIMPLE_MASTER_SLAVE.md)。以下保留本机服务管理入口说明。

# 简化配置：一套主界面，多套后台

打开 **Tools → ECC / data service manager…**。

## 第一次使用

1. 点击 **Start / check backend**，或 **Add service instance…**。
2. 填板卡 IP、接收 IP、控制电脑 IP、端口和数据目录。模板、profile、凭据和服务配置不需要手写。内置预设为单CoBo pedestals、硬件v5、AGET2.2、AsAd槽0；页面显示这些假设，实物不同时在 Advanced 选择正确的电子学设置。
3. 确认本组板卡采集已停止且由本实例控制，点击 **Start ECC + data service**。程序自动保存配置、离线校验和启动后台；成功后点 **Use in Run Setup** 返回服务管理窗口，再点 **Use selected instance in Run Setup** 供主界面使用。

启动服务不需要连接板卡，也不会自动执行 Describe、Prepare 或 Run Start。启动数据管理服务后，receiver 根据正常采集流程启动；无硬件时不能据此声称已开始实物采集。

## 再加一套 ECC 和接收服务

在同一个管理窗口点 **Add service instance…**。新实例自动建议另一组端口和独立目录，填写另一组板卡 IP 后启动。实例列表显示 ECC 端口和板卡地址，可以选择任意实例启动、停止或查看进程。

这些操作不会另开主界面，也不会关闭其他实例。主窗口的采集按钮始终控制当前选中的一套；切换前先断开当前连接。同一块板不能由两套 ECC 同时控制。

现有现场配置可用 **Import existing…** 导入，日常随后从实例列表选择即可。服务管理器只记录配置文件路径，凭据仍在各自私有部署目录。

多主机自动缓存已有 Linux 首版和本机回环验证；物理多机仍待现场验收，结构见[多主机配置部署方案](DISTRIBUTED_CONFIGURATION_DEPLOYMENT_DESIGN.md)。
