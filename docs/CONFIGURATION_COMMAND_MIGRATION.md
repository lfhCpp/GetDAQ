# 配置编辑命令的迁移对应表

更新时间：2026-10-20。依据收到的ConfigWidgets `Commands.h`实际十个CommandId及模型访问器职责，以及旧GetBench数据库管理器与GUI槽函数实现；表中的“已接”限于本机支持域，不等于全部旧接口或平台通过。

| 原命令 | 新入口 | 范围与剩余事项 |
| --- | --- | --- |
| RenameConfigNode | ChangeName / ChangeId | 本地声明及全有效差异审阅；简单XML名称、ID和冲突检查。另有工作区跨文件现有声明移动，保持原名称/ID并改写相对include；来源文件路径迁移另由独立GUI/CLI入口处理，已接预览、迁移和原始/候选恢复（70.68，71.113复验）。不提供任意外部读者可见的跨文件原子提交 |
| SetConfigNodeValue | 声明值保存、XcfgEditHistory::SetValue | 按来源编辑，核对全部加载来源及共享范围，准确字节撤销；单有效值片段的首尾注释/PI及CDATA保留后编辑，不能表示的CDATA值转单文本片段；分裂文本/自闭合值仍拒绝 |
| SetConfigNodeUnit | ChangeScalarProperties | 按类共享单位及来源审阅；不换算值 |
| SetConfigNodeRange | ChangeScalarProperties | 类型化范围/等价/全部实例约束，准确源与历史守卫 |
| SetConfigNodeIncludeUrl | ChangeInclude | 本地文件地址修改/移除，保留本地子项；数据库/网络资源不接 |
| InsertConfigNode | AddObject / AddScalar | 本地结构插入，明确scalar类型，预览/冲突/预算/历史 |
| OverrideConfigNode | scalar PreviewOverride / ApplyOverride；OverrideInheritedObject | 具体scalar覆盖、继承对象浅层/缺祖先/同名模板兄弟已接并有旧实际模型redo/undo/redo对照；include物理来源复用会影响所有投影，完整多次引用声明等价不作保证。见[对象覆盖](CONFIGURATION_OBJECT_OVERRIDE.md) |
| DeleteConfigNode | RemoveDeclaration | 删除选定声明并审阅所有有效影响；不删除文件或外部调用方 |
| IncludeConfig | ReplaceWithInclude | 替换子内容并保留本地引用，前瞻来源守卫及历史 |
| ImportConfig | ImportConfiguration | 替换子内容、递归展开本地引用、保留默认声明和可保留原片段；外部原文件不改 |

文件/工作区动作不在这十个CommandId之内，也必须单独核对：

| 原动作 | 当前对应 | 边界 |
| --- | --- | --- |
| 文件打开/选择 | ConfigWindow::Open | UTF-8、本地include、按真实来源查看；只读模式保留 |
| 对象Export File | XML声明导出 | 自包含、默认块、共享元数据冻结、旧方法实际对照；有效对象导出是额外明确模式 |
| Save As | 新XML副本；保留include的工作区副本+明确打开 | 单文件自包含或新目录复制所有加载来源，保持共享/逐引用隔离，清窗口历史；独立的来源路径迁移入口已接，但不声称完整复刻旧就地多文件SaveAs语义；跨文件原子事务仍未实现，见[工作区副本](CONFIGURATION_WORKSPACE_COPY.md)和[路径迁移手册](CONFIGURATION_PATH_MIGRATION.md) |
| Undo/Redo | XcfgEditHistory审阅恢复；XcfgWorkspace整组来源历史 | 独立单文件声明树保留自己的历史；多文档工作区的文本和14种结构操作现共用来源组历史，按成功审阅的一组修改恢复来源，重新校验全部当前根，保存后也仅暂存反向字节、重新审阅才写盘。另有持久化保存恢复记录；不是旧命令栈全量等价或硬件回滚 |
| Save All/多文档工作区 | 独立多根来源文本工作区、统一暂存/审阅及Save All | 最多16根，规范路径共享include一份来源，内存编辑后全根审阅、准备全部临时文件再逐个替换；范围/外部漂移/旧审阅拒绝，部分失败列出结果并停止重试。旧Workspace::saveAll也是逐模型保存，不是跨文件事务；工作区树编辑与文本共用历史已接；已打开本地文件间的声明移动已接；保存意图与人工原始/候选恢复已接；来源路径迁移另有独立入口且本轮已复验；外部可见的跨文件原子事务仍待，见[多文档手册](CONFIGURATION_MULTI_DOCUMENT.md)和[路径迁移手册](CONFIGURATION_PATH_MIGRATION.md) |
| 二进制配置读写 | XcfgBinary、转换窗口、import-binary/export-binary CLI | 旧版本1的对象/五种标量/单位/范围及实际旧库对照已接；signed32/unsigned32限制，不保留XML词法/include分布，不证明板端加载，见[二进制配置](CONFIGURATION_BINARY_FORMAT.md) |
| Import/Export/Include Database UI | `source/cconfig/ConfigWidgets-release-20210208/src/CCfg/Control/ConfigEditorController.cpp`中的三个`on...DB`槽函数只有空`try`块；`source/get/GetBench-software-release-20210208/src/get/rc/ConfigDatabaseManager.cpp`则实现了读取、列举和新增版本 | 不能把三个可见动作当成可工作的旧GUI功能，也不能说旧源码完全没有数据库代码。GetBench的`configure.ac`中`--enable-database-support`默认关闭；启用后依赖外部`ConfigDatabase`包，按`description`、`prepare`、`configuration`、`hardwareDescription`四类保存带时间版本的配置。旧README中出现的PostgreSQL连接串只是历史示例，不是当前部署凭据或实际使用证据。新程序不放空按钮，也不臆造通用SQL兼容层。若现场确实使用，先取得实际DBMS/URL方案、配置样例、读写流程和部署包，再定义有版本/凭据/恢复约束的连接器 |

新增复制、同父排序、属性改type及当前具体子树物化是独立操作；对象浅层覆盖由独立路径操作接续，不能用这些功能填充跨文件迁移或SaveAll的完成状态。XML保存和旧运行支持域见[导出手册](CONFIGURATION_XML_EXPORT.md)、[结构编辑](CONFIGURATION_STRUCTURE_EDITOR.md)、[运行对照](LEGACY_CONFIGURATION_RUNTIME.md)。

多次引用可在新工作区按每次使用拆分并独立编辑，初始有效语义保持；另有多根文本工作区统一Save All。这不证明整个旧工作区/所有内存副本接口等价。本工作区内树编辑与文本已经共用历史；已接跨文件现有声明移动和独立来源路径迁移；仍需处理跨文件持久原子事务及更广泛旧接口，并按真实用途评估数据库或网络资源。原`source/`和MUSIC继续保留。控制界面、配置仓库及低频管理始终不进入原始事件接收/写盘通路。
