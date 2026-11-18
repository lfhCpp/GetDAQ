# 配置与数据：按项目目录运行

新建托管项目使用原版 `getEccSoapServer`，接收器默认 **DataExporter**，也可选原版 **dataRouter**。已经启动的外部 ECC / DataRouter 仍可直接连接，不要求改成托管部署。

## 先设置三个路径

| 界面设置 | 在哪台电脑 | 用途 |
| --- | --- | --- |
| Configuration source folder | 主控 | 默认安装目录下的 `config`；放完整配置目录及其相对引用资源，可输入或 Browse。 |
| Project data folder | 每个执行主机 | ECC、接收器和 Sentry 的实际工作目录；配置同步到目录根，原始 `.graw` 写在这里。 |
| GUI Run records root | 操作界面主机 | YAML、journal、CSV 和配置备份，不是远端原始数据目录。 |

例如主控源为 `/opt/GetDAQ/config`，本机项目为 `/data/project`，从机项目为 `/data/project-b`：

```text
主控 /opt/GetDAQ/config（完整源目录）
 ├─ 同步 → 本机 /data/project
 └─ 网络分发 → 从机私有缓存 → 自动同步到 /data/project-b
```

从机只需选自己的 Configuration cache 与 Project data folder，再 Enable 并配对；不需要填写主控的源文件路径。
每个主机的项目目录可以不同。新建项目不会额外生成隐藏的实例原始数据子目录。
旧部署记录保留旧路径，编辑采用新项目设置后才切换；不会自动搬动已有数据。

## 最短操作顺序

1. 本机运行：选 Master，Use this computer；多主机则先在从机选 Slave、设置项目目录、Enable，主控 Add slave。
2. 主控选择 Configuration source folder，保留 **Sync configuration to project folders** 勾选。
3. 添加 ECC / board group，选择 ECC 与接收执行主机、项目目录、接收器类型。
4. 填 ECC 电脑控制 IP / SOAP 端口、接收电脑控制 IP / 管理端口、接收数据 IP / TCP 数据端口。CoBo 地址和电子学设置仍在原始 XML/XCFG 中。
5. 点 **Start services and use this configuration**。软件先分发完整配置，再在指定项目目录启动后端和共享 Sentry；日志显示实际 `cd` 路径及命令。不执行硬件 Prepare 或 Start。
6. 回主界面 Connect → Progress（Describe / Prepare / Configure）→ Ready → Run start；Run stop 后确认所有 ECC Ready，再看归档结果。

同一主机多个实例用不同 ECC / 接收端口区分，可共享同一项目目录。一个项目内的板卡组使用同一 Experiment，统一 Start / Stop。
每个主机、每个项目目录只有一个 Sentry；状态按板卡显示，项目总速率、文件大小和归档不重复累计。

## 两个接收 IP，不是同一用途

例如先进入项目目录，再启动原版路由器：

```sh
cd /data/project
dataRouter 192.168.48.16:46003 192.168.50.16:46005 TCP FrameStorage
```

第一个地址是该电脑的控制网络地址和 Ice 管理端口；第二个是 CoBo 数据目标的网络地址和 TCP 数据端口。
GUI 使用原版类名 `FrameStorage`。DataExporter 同样分开控制端和数据端，还提供独立的波形 TCP 导出端口。
上述地址须实际配置在接收主机上。ECC SOAP 端口、接收管理端口、数据端口、Sentry 端口不可混用。

## 配置文件如何对应 ID

| GUI 字段 / 命令 | ECC 读取文件（`.xcfg` 或 `.xml`） |
| --- | --- |
| Describe ID = cobo0 | `describe-cobo0` |
| Prepare ID = pedestals | `configure-pedestals` |
| Configure ID = pedestals | `configure-pedestals` |

原版 Prepare 使用 Configure 类型，**不是 `prepare-pedestals`**。Experiment 为默认 ID 和 Run 名称；明确填写的节点 ID 优先。
配置中引用的拓扑、电子学文件和其他资源应放在源目录中，使用可随目录复制的相对路径。
完整源目录同步，保留相对结构，不需要逐个挑选配置。缺少根 `CoboFormats.xcfg` 时补入随软件安装的原版格式定义及依赖文件。
ECC 的 `--config-repo-url`、接收器 cwd 与 Sentry 监控目录都指向该主机的项目目录。

## 更新配置

编辑主控源文件后，用 **Tools → Sync project configuration…** 选择本机、指定从机或全部主机。
发布和启动也会重新读取源目录，不沿用过时的内存副本。

- Idle / 停服务：应用到项目目录，被替换的配置备份到 `.getdaq/configuration-backups/`。
- Running、Ready 或无法确认安全状态：只接收缓存，标记 pending，不替换活动配置；停服务后或下次 Idle → Describe 前自动检查并应用。
- 取消同步勾选：仍可分发缓存，但不覆盖项目文件；使用项目内自行维护的配置，Run 记录明确提示配置备份未验证。
- XML 参数更新可复用同一套已核对的后端进程；程序、端口、工作目录或接收格式字典变化，须先停服务再重新启动。

不删除原始数据、已有 Run 或日志。同步不是硬件配置命令，也不自动重放以前的硬件步骤。
`.getdaq/` 存缓存、配置备份、进程记录和日志；不要手动复制该目录作为实验源。

## 数据与日志在哪里

采集中：`<Project data folder>/*.graw`。
归档成功后：`<Project data folder>/<Experiment>/run_NNNN/`。
归档警告时可能仍在原处，以 Sentry 实际路径和记录为准。原始数据不通过配置同步传输。

主界面原 Event log 区域保留位置：点 **Event log / ECC log / DataExporter log 或 DataRouter log**，在右侧选择主机和实例；接收日志也可选共享 Sentry。
进程日志位于项目目录 `.getdaq/logs/<instance>/`，共享 Sentry 位于 `.getdaq/sentry/sentry.log`。
远端日志通过已配对管理服务认证读取，分段并限制大小，不把进程输出写成采集状态。

## 已有外部 ECC / DataRouter

继续选择 Connect existing ECC，填实际 SOAP 地址、接收数据目标及 Sentry。
外部 ECC 用其启动时指定的仓库或 NFS，GUI 发送 ID，不擅自更换仓库、cwd 或启动外部接收器。
外部程序未托管时，GetDAQ 不声称能同步其目录或附着进程日志；本地镜像仍供编辑、备份。

无硬件测试已覆盖原版两个接收器、共享 Sentry、完整目录分发、活动配置暂存、日志认证和进程复用；实际板卡参数与接线仍需现场验证。

## Nodes 保存与真正分发的区别

Nodes / Paths 的 Save and apply 保存 YAML 中的连接 IP、端口、路径和同步开关，
**不自动分发 XML/XCFG**。配置文件的发布、复制由 Tools → Sync project configuration、
项目中的保存同步或 Start project services 触发。Idle → Describe 前会检查并应用已接收的
pending 版本，不会把尚未发布的主控源文件自动上传。外部已有 ECC 的 NFS/仓库继续由其自身维护。

## 合并导出日志与 Clear

Tools → Logs → **Export combined logs…** 导出 UTF-8 TXT。界面后台轮询所有已配对项目的
ECC、Data 和共享 Sentry，即使当前停在 Event log，也会收集其他来源；最多四个并行读取，
一个主机不可用不会阻塞其余来源。

每段使用醒目的来源、主机、实例分隔线，并按本 GUI 收到时间（UTC）和单一顺序号排列。
后端原文中的时间保留，但没有原始时间戳的日志不能推断其跨主机真实发生时间；接收时间
不冒充硬件事件时间，也不依赖远端主机时钟完全同步。

每次读取固定当时的文件大小边界，跨读取的半行会拼接。界面线程串行接收结果，导出复制
一份不再变化的快照，用 QSaveFile 原子提交；期间继续产生的日志留给后续导出，不混写当前 TXT。
文件轮转、截断、最后一条未完成行以及无法读取的来源会保留相应信息。

导出范围是本次 GUI 会话已经收集的记录，初次连接只读取各源最近的尾部，不声称包含所有历史文件。
历史有约 8 MiB 文本 / 10,000 条记录预算，TXT 头部报告裁剪数量和在途读取数；原始后端日志文件仍保留。

Data log 右侧的 **Clear** 仅清除当前页/实例的显示，不删除原始文件、Event 审计日志或合并导出历史。
清屏前已经发出的读取响应不会把旧字节刷回显示，但其游标和导出历史仍正常推进。
