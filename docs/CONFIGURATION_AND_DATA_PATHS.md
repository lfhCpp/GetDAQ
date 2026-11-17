# 配置文件与数据目录怎么对应

GetDAQ 当前使用原版 SOAP ECC 控制板卡，DataExporter / 外部 DataRouter 接收数据。
主界面布局不变。IP 决定连接哪台主机，路径决定该主机读取或写入哪些文件。

## 四类目录要分开

| 目录 | 所在电脑 | 用途 |
| --- | --- | --- |
| Master configuration directory | 主控 | 原始 XML/XCFG 源文件，你在这里维护配置。 |
| ECC configuration cache / `--config-repo-url` | ECC 执行电脑 | ECC 实际读取的仓库；托管模式由主控自动分发。 |
| Raw data root / DataExporter working folder | 接收数据的电脑 | `.graw` 原始数据。每个托管实例使用数据根目录下的独立子目录。 |
| GUI Run records root | 运行操作界面的电脑 | `run.yml`、journal、CSV、配置备份；修改它不会改变远端原始数据的落盘位置。 |

## 先定数据目录，再启动 DataExporter

你的理解正确：原版 `FrameStorage` 默认输出目录是 `.`，也就是 DataExporter
进程的工作目录，并非可执行文件所在目录。手动运行时，先 `cd` 到期望的目录。
外部软件若另设输出目录，则以实际设置为准。

在 GetDAQ 主从托管模式中，先设置 Raw data root：本机在 Master 的 Hosts 页、
Use this computer 之前选择；远端在 Slave 页、Enable 之前选择。软件自动创建：

```text
<data-root>/<group-id>-<receiver-id>/
```

随后以该目录为工作目录启动 DataExporter，并让 Sentry 监控同一个目录。
例如数据根目录 `/data/getdaq`，实例 `group1-data`，则采集中原始文件在：

```text
/data/getdaq/group1-data/*.graw
```

正常 Stop 后，如果 Sentry 归档成功，则文件位于：

```text
/data/getdaq/group1-data/pedestals/run_0000/
```

归档警告不代表文件已移动，仍要查看 Sentry 的实际目录和记录。
从机已启用后，目录设置会锁定；此轮修改只展示路径，不搬动已有数据或重设现场目录。
启动服务只启动后端，Connect / Describe / Prepare / Configure / Start 才推进采集。

## ECC 如何按 ID 选文件

ECC 启动时的 `--config-repo-url` 决定它的配置仓库。GUI 的 Describe / Prepare /
Configure ID 决定读取哪个配置；未单独填写时，GUI 以 Experiment 作为默认 ID。
**Experiment 同时用于 Run 的实验名称，但不是直接填写完整文件名的地方。**

以 `Describe ID=cobo0`、`Prepare ID=pedestals`、`Configure ID=pedestals` 为例：

| 命令 | 原版 ECC 读取的主文件 |
| --- | --- |
| Describe | `describe-cobo0.xcfg` / `.xml` |
| Prepare | `configure-pedestals.xcfg` / `.xml` |
| Configure | `configure-pedestals.xcfg` / `.xml` |

原版 `ConfigManager::getPrepareConfig` 使用 Configure 类型：**不是 `prepare-pedestals`**。
文件内部引用的硬件描述、拓扑、电子学配置也必须可读。托管分发要求把引用文件放入
所选主控配置目录内，并使用相对引用，保证复制后引用仍有效。

### GetDAQ 启动的托管 ECC

1. 主控选择 Master configuration directory 和 Experiment。
2. 软件校验主文件与相对引用，发布并复制到所选 ECC 主机的缓存仓库。
3. 使用该缓存仓库作为 `--config-repo-url` 启动 ECC；从机不需要再选源配置文件。
4. Nodes / Paths 显示的 ID 随 SOAP 命令发给 ECC，由 ECC 从自己的仓库读取文件。
5. 修改源文件后，须按正常停采/停服务流程重新发布启用，不能把本地编辑当成远端已更新。

### 连接别人已经启动的旧 ECC / DataRouter

ECC 继续使用它自己的启动仓库或 NFS 挂载。GUI 发送配置 ID，不擅自替换它的仓库。
Nodes / Paths 的 Local XML/XCFG mirror 供本地编辑及备份，不是自动上传开关。
接收目录由那台接收电脑上实际的 DataRouter / DataExporter 配置与工作目录决定；
在 GUI 连接后，从 Sentry 的 Data path 核实。不要把 GUI 的记录路径当作远端数据路径。

## 在界面中核实

- Nodes / Paths：一个 Configure and manage ECC / data services 入口；查看配置 ID、
  本地配置镜像、GUI 记录根目录，以及 Sentry 报告的原始数据目录。
- Master：选中 ECC 组，查看配置源、已准备的 ECC 缓存、已准备的 DataExporter 工作目录、
  归档规则和 GUI 记录目录。Planned 表示计划目录，last prepared 表示上次准备的实际目录。
- Slave：Configuration cache 是缓存根；Raw data root 是接收根。实例表显示实际配置缓存
  和数据工作目录。刷新只读，不反复禁用控件或重建未变化的单元格。

源码依据：`apps/deployment/nodeagentoriginal.cpp`、`originaldeployment.cpp`、
`apps/envoy/protocol.cpp`、`apps/sentry/sentryservice_catalog.cpp`，以及内置原版
`getbench/src/get/rc/ConfigManager.cpp` 与 `getbench/src/get/daq/FrameStorage.cpp`。
