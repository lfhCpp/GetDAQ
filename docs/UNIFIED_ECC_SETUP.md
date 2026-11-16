# 一个入口配置新旧 ECC

打开 **Master / Slave → Master**。**Nodes / Paths** 保留原节点配置表。主界面布局不变，窗口内统一英文。

## 连接已经运行的 ECC

选择 **Connect existing ECC**，填写三项：

| 项目 | 内容 |
| --- | --- |
| Instance name | 自己容易识别的名称，例如 ECC-16 |
| ECC computer IP | 运行ECC的电脑地址，不是CoBo板卡地址 |
| ECC service port | 该ECC实例的服务端口 |

点 **Check connection**。软件只查询身份和状态，自动识别SOAP命名空间或复用新版连接信息。检查通过后点 **Save and use in Run Setup**，回到主界面点 **Connect**。以后从 **Saved ECC connections** 选用，不用反复填IP和端口。

新旧连接统一保存为GetDAQ YAML，名称使用 `getdaq.ecc_name`。底层SOAP或原生服务所需字段由软件维护，不要求用户把两种协议字段手工改成一样。友好名称不改变固定服务身份，也不改CoBo编号。

已知Ice端口会先被只读识别，再检查同机8083的SOAP入口；只有实际收到正确状态响应才允许保存，并明确显示端口变更。不会把Ice端口当成HTTP端口，也不自动创建另一套ECC替代已有服务。SOAP支持 `urn:ecc` 和原有 `urn:http://localhost:8083/`，无需手填命名空间。

## 新版ECC的授权

软件创建的新版实例自动复用已保存的授权和固定配置绑定。对于独立部署、此前未配对的受保护新版ECC，IP和端口不能代替授权：在可选设置中一次选择已有的 **Saved native connection profile**，或通过创建服务页面配对所在主机。这里不要求填写令牌、client epoch或SHA。导入文件引用的私有凭据需要在本机可用；身份不匹配会拒绝使用，不自动接管或清UNKNOWN。

## 创建新的ECC和数据服务

切换到 **Create ECC / data services**，沿用已有主从流程：加入从控或使用本机 → 添加板卡/ECC → 选择运行主机、填板卡与接收地址 → **Start services and use this configuration**。端口建议、服务文件、凭据、缓存及路径绑定由软件生成。同机多实例和多主机都使用这个入口。

从控仍只选择本机缓存/数据目录并启用，实验源文件在主控维护。详见[主从控操作教程](SIMPLE_MASTER_SLAVE.md)。

## 数据目标与采集

连接检查不会执行Describe、Prepare、Configure或Start。旧SOAP ECC的数据接收地址和端口可在可选设置中填写；端口0表示尚未配置数据目标，只能视为连接/状态准备完成。原生ECC的数据目标来自固定部署，通过创建服务页面配置，连接窗口不改写它们。

你现有的192.168.40.16服务已验证：46003是Ice，8083是SOAP，使用urn:ecc可读到Idle。但该服务之前未返回实验配置ID，实际数据接收端口也未确认；统一入口不会把这些未知信息伪装成已经可以采集。

更新GUI后仍复用同一安装中原已登记的节点与仓库运行目录，保留原进程、凭据和审计绑定，不因为切换GUI版本就要求重建后台。新服务可以使用新运行包；既有服务升级/迁移仍需按其实际状态处理。

## 旧 ECC 操作超时

旧 SOAP ECC 的 GetState 使用 `getdaq.timeout_ms`；Describe、Prepare、Configure 等操作请求使用 `getdaq.step_timeout_ms`，与状态转换期限一致。例如状态查询5000 ms、状态转换120000 ms，Prepare最多等待120秒。可在 Nodes / Paths 中调整 **Transition deadline**。请求超时不证明远端失败，也不会自动重发命令，应先刷新实际状态。

## 已停止但本地收尾失败

旧 ECC 已确认 Ready、数据归档完成后，若只是缺少本地配置备份源文件，软件记录警告并正常完成 Run、递增运行号，解除操作锁定；显示 **Complete (configuration backup warning)**，ECC 仍为 Ready。Run 目录保存 `configuration-backup-warning.json`，journal注明备份缺失，不生成 `backup.complete`，不声称配置备份成功。

其他本地收尾错误（例如配置损坏、运行记录无法写入）仍会显示 **Stopped; finalization pending**。此时 **Disconnect** 可用：软件保留未完成的 Run journal 和已完成的归档结果，不重复 Stop，不停止后台。若 ECC 正在运行、暂停或状态查询过期，不能使用这条断开路径。

断开后可以进入 Nodes / Paths。恢复未完成的原 Run 时打开其 `run.yml`、Connect、Recover journal，再 Retry finalization；旧窗口因缺文件卡住的 Run 也可通过此流程按警告完成。不要用名称不同的模板冒充缺失文件；修改硬件目标或配置ID后不能作为原 Run 恢复。升级前已卡在旧窗口时，可以关闭 GUI、确认保留中断记录，再重开新版进行恢复，后台仍独立运行。

## 旧 Sentry 与原生数据管理

旧 SOAP ECC 使用旧 Sentry 的 `/status`、`/catalog`；原生 ECC 使用部署绑定的 supervisor/receiver 观察与归档，不把旧 Sentry 的控制接到原生接收会话。两种模式保持各自接口，不能只切一个复选框就认为旧数据后台已迁移。

旧 Sentry 状态查询与归档请求使用独立通道。归档期间表格显示 **Archiving**；旧服务若阻塞状态响应，显示 **Archiving / status unavailable**，不声称仍有实时数据。数据率优先按两次有效字节计数差计算；旧返回只有GB时沿用兼容解析，目录改变、计数下降或状态过期时不显示虚假速率。Sentry只统计其配置的目录，需与接收程序实际写盘目录一致。

**All configured nodes confirmed stopped (Ready)** 表示 ECC 已确认停止。随后 `/catalog` HTTP 500 属于数据归档失败，不能当成Stop失败或归档成功。按当前旧Sentry兼容策略，全体ECC仍有最新Ready状态、所有已提交归档请求已返回后，软件以警告结束本地Run，显示 **Stopped (archive warning)**，递增运行号并解除锁定。`journal.json` 保留 `catalog=unknown`，阶段为 `CompleteWithWarnings`，并保存 `catalog-warning.json`；不自动重发归档、不删除原始文件、不清理替换Run的旧副本，也不把这个Run作为可覆盖的已验证Complete。自动续Run会取消，操作员可手动开始下一次采集。

已卡住的旧Run可打开原 `run.yml`、Connect、Recover journal、Retry finalization，以同一警告规则结束，不重新提交原归档。Sentry服务端错误、源文件与目标目录仍可后续核查。下一次普通Start要求Sentry正常、有可用磁盘及空接收目录；若选择已存在的同一Run编号并明确确认覆盖，可保留现有接收文件继续采集，确认框说明这些文件可能与本次数据一起归档。旧本地记录移至独立备份且不自动清理。原生ECC的封存/归档门槛不受此旧Sentry策略影响。

旧默认端口：DataRouter管理46003、数据接收46005。Router port填写实际数据接收端口，以现场启动参数为准；管理对象可达不证明该端口可接收TCP原始数据。
