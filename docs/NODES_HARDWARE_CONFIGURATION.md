# Nodes / Paths：实物配置

原生 ECC 模式点击主界面的 **Nodes / Paths**，打开实际硬件与服务配置编辑器。主界面布局不变。

## 常用操作

1. 已打开部署配置时，窗口从该目录的 `setup-inputs.json` 读回输入。旧部署没有这个文件时，从 `ecc.json`、`topology.json`、`supervisor.json` 和实验工件导入；不以环境变量中的 IP 代替保存值。
2. 修改表格中的板卡 IP、板卡控制端口、接收 IP、数据端口，以及本机控制网 IP、ECC/监管服务端口、数据目录。原生 ECC 是整套系统共用的服务，板卡控制端口是每块板的 Ice 端口。
3. 选择 CoBo 后点 **Add CoBo (copy selected settings)**，输入新 ID，填写新增板的实际 IP；新板复制选中板的电子学参数和硬件描述。只有硬件配置相同才应直接沿用；不同型号、AsAd/AGET 数量等需要先选择正确模板。表格末列显示电子学模板来源。可删除选中板，也可取消 CoBo 的活动勾选。
4. 选一个新的配置保存目录，确认旧采集和发送已停止且由当前操作员独占控制，点 **Save configuration files and check**。程序生成文件、计算摘要并运行离线检查；不发 Describe/Prepare/Start。
5. 检查通过后可点 **Use in Run Setup** 加载生成的 `envoy.yml`。修改服务地址后，须通过 **Manage current ECC / data services…** 管理旧服务，再使用新配置启动；**Start ECC + data service** 仅启动刚生成的配置集。

首次设置需要导入已核对的实验 `.xcfg` 和硬件 profile，或已有的实验工件 `.json`。这部分及程序路径、硬件描述、服务对象名位于高级设置中。电子学参数无法由 IP 推断；已有配置会自动带入。

## 界面与文件的对应关系

| 界面输入 | 实际保存位置 |
| --- | --- |
| 板卡列表、IP、板卡控制端口、硬件描述 | `topology.json` |
| 增删 CoBo、活动状态、电子学参数模板 | `experiment.json` |
| 接收 IP、数据/状态端口、数据目录 | `supervisor.json` 的接收计划，以及 `ecc.json` 的 sender 目标 |
| ECC 端口、本机控制网 IP/回调端口 | `ecc.json` |
| 监管服务端口 | `supervisor.json` 和 `ecc.json` 的接收节点连接 |
| 主程序连接地址、自动计算的摘要与认证路径 | `envoy.yml` |
| 下次编辑要读回的输入与模板来源 | `setup-inputs.json`、`template-experiment.json` |

生成到新目录，保留原配置与运行审计；保存不等于服务已经切换，也不清除旧硬件操作的 UNKNOWN 状态。认证值由软件创建，不要求用户手填。

同机需要新增 ECC/数据后台时，使用 Configuration → ECC / data service instances…，在同一个管理窗口中点击 Add service instance…。软件复制电子学模板、建议新的端口和保存目录，并清空板卡 IP。主界面保持一个，已有后台可继续运行；选择 Use selected instance in Run Setup 才切换当前采集所用的实例。详见 [同机多实例说明](INTEGRATED_LOCAL_BACKEND.md#同一台主机上的多实例)。
