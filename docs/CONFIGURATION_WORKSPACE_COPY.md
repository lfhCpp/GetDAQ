# 保留 include 的工作区副本与引用隔离

更新时间：2026-10-10。

现在可以把主XML及它加载的include复制到一个新的可编辑目录，保留模板和include关系，并把include地址改成副本内部的相对路径。原文件不修改。该操作补充此前的单文件XML导出，主要用于多文件另存、试验配置分支和拆分共享来源。

界面入口：**GET configuration files... → Review source workspace copy...**；CLI为preview-workspace-copy和copy-workspace。

## 1. 两种模式怎么选

| 模式 | 重复引用如何处理 | 后续编辑影响 |
| --- | --- | --- |
| Keep shared source files / shared | 同一原文件只复制一次，所有原引用指向同一副本 | 编辑一个来源仍可能影响多个使用位置 |
| Isolate each include reference / isolated | 每次include及其后续引用链分别复制 | 可以编辑一处引用的文件，其他使用位置保留自己的副本 |

界面默认isolated。它会多生成一些小型配置文件，适合需要独立调整各板/实例的情况。需要保留统一模板来源时选择shared；不要把shared当成只修改一个逻辑使用者的承诺。

两种模式的初始有效对象、值、type、单位和语义范围都必须与原配置一致，否则拒绝创建副本。不凭模板生成不存在的硬件ID。

## 2. 一个有嵌套引用的例子

原主文件中的Board[0]和Board[1]都include同一个shared.xcfg；shared.xcfg又include leaf.xcfg。

```text
shared模式
  main.xcfg → Board[0] ─┐
                       ├→ sources/source-000001.xcfg → source-000002.xcfg
  main.xcfg → Board[1] ─┘

isolated模式
  main.xcfg → Board[0] → sources/source-000001.xcfg → source-000002.xcfg
  main.xcfg → Board[1] → sources/source-000003.xcfg → source-000004.xcfg
```

编号只是确定且无歧义的复制名称，不代表板号。主文件中的地址为sources/source-000001.xcfg；sources目录内的嵌套引用只写同目录文件名。复制后移动原目录不会破坏这些include。

## 3. 界面逐步操作

1. 打开主配置，确认当前加载的include都是你需要的来源。只复制已经由文档加载的本地XML/include。
2. 点击 **Review source workspace copy...**。
3. 选择shared或isolated。默认isolated。
4. 输入一个尚不存在的新目录完整路径。父目录必须已经存在，例如先创建config/copies，再输入config/copies/run-001。不能选已有目录；失败不会覆盖其中内容。
5. 如需另存后继续编辑，勾选 **Open the copied main.xcfg after saving...**。连接只读模式下该选项禁用，但仍可创建只读来源的副本。
6. 点击 **Build preview**。后台处理期间输入暂时禁用；原文件和目标目录不会写入。内部会在自动清理的临时目录重载候选。
7. 阅读 **Source mapping**。每行列新文件、原文件、输出字节数及输出SHA；isolated中一个原文件可以出现多行，这是有意拆分。
8. 阅读 **Initial inventory / include edges**。from/to表示复制后的引用关系，declaration_path和line定位原来源中的引用声明。
9. 阅读 **Copied main XML**，确认include地址已指向副本。其他值、注释、CDATA和未知属性保持；仅修改include开始标签。
10. 状态应说明effective configuration unchanged。预览中的清单是拟写内容，其中complete表示预计最终清单；顶层saved=false和“target directory not created”才是当前状态，不能用预览JSON证明已保存。
11. 点击 **Create reviewed NEW workspace**。程序重新构造并比较来源字节、输出文件、初始清单及有效摘要；输入改动或来源漂移要求重新预览。
12. 等待成功信息。勾选打开时，本窗口载入新的main.xcfg并清空旧会话Undo/Redo；原配置仍在原处。未勾选时，当前编辑器不切换。

改变目录/模式会撤下旧Save；重开文档、编辑或Undo/Redo导致窗口版本改变时也拒绝旧审阅。若保存任务开始后切换文档，完成后说明副本属于先前文档，不替换当前编辑器。关闭窗口不取消已经开始的文件工作。

## 4. 新目录里有什么

```text
NEW_DIRECTORY/
  main.xcfg
  sources/
    source-000001.xcfg
    source-000002.xcfg
    ...
  workspace.json
```

workspace.json是**初始来源清单**，记录模式、原来源路径/摘要、复制后的文件/摘要、include边和初始有效摘要。它不是experiment.json、publication.json，也不是ECC已经接受或板卡已经加载的版本。

文件可编辑，Linux目录0700、文件0600。正常保存后workspace.incomplete被移除；workspace.json在全部文件重载/字节/语义/来源检查后最后写入。没有硬件调用。

## 5. 失败时怎么判断

| 状态 | 含义与处理 |
| --- | --- |
| 预览失败，无目标目录 | 原文件没有被写入；检查路径、冲突、预算或来源变化 |
| 目标目录已存在 | 使用新的目录名称；不递归覆盖旧目录 |
| 出现workspace.incomplete，没有workspace.json | 文件工作未完成核对；不要把目录作为已经完成的副本使用 |
| workspace.json存在但接口报同步/标记清理失败 | 文件可能已核对并写清单，后续步骤失败。保留现场，检查实际结果，不描述成完全没创建 |
| 来源漂移/预览改变 | 重新打开主配置，再审阅；不能凭旧预览继续 |

失败创建的目录保留供检查，不自动递归删除，避免误删人工添加的文件；重试必须使用新目录名。只有开发测试的自有临时目录自动清理。Linux调用文件和目录fsync，其他目标平台仍待实测；这不是跨文件系统事务或断电原子目录发布。

## 6. 后续编辑与初始清单

后续使用普通配置编辑、浅层对象覆盖或参数修改即可。isolated把每次引用的文件链拆开，因此对某一新来源的编辑可以只影响一处引用；shared继续列出所有实际受影响投影。

编辑后workspace.json中的初始文件摘要可能不再与文件相同，这是正常工作副本变化，不是自动失败或自动回滚。清单不会被当成持续不变的运行绑定；需要记录编辑后的版本时，重新导出快照或建立新的正式发布包。

即使effective SHA与原配置相同，来源路径/字节/include写法已改变，snapshot SHA或编译工件版本可能变化。不要重用原ECC绑定或把“初始有效值一致”当作硬件已经使用新来源。

## 7. 复制范围的边界

复制主XML和文档已经加载的include；不复制envoy YAML、profile JSON、硬件描述、二进制、固件、数据文件，或普通字符串中提到的其他路径。那些字符串值保持原样，仍需核对实际资源位置。整套离线发布使用[配置发布包](CONFIGURATION_PUBLICATION.md)和依赖工具链流程。

这是向**新目录**复制来源并重定位include，不是就地搬走原文件或写多个原文件。也没有实现旧多文档工作区全部SaveAll/多文件事务。原材料、MUSIC、历史source和真实Run不会被清理。

预算：每个XML最多8MiB，所有复制XML合计64MiB，最多256个复制文件、32层引用、10000条include边，清单最多16MiB。isolated按复制后的文件/总量计数，重复引用不能绕过限额；超限在创建目标之前拒绝，不自动切换成shared。

## 8. CLI示例

```bash
mkdir -p config/copies

# 只输出初始来源/引用审阅；不创建目标。
bin/getdaq-config preview-workspace-copy config/experiment.xcfg isolated config/copies/branch-001

# 明确创建隔离副本。
bin/getdaq-config copy-workspace config/experiment.xcfg isolated config/copies/branch-001

# 或保留共享关系，选择另外的新目录。
bin/getdaq-config copy-workspace config/experiment.xcfg shared config/copies/shared-001

# 新副本可继续查询/导出快照。
bin/getdaq-config snapshot config/copies/branch-001/main.xcfg config/copies/branch-001/snapshot-after-copy.json
```

请替换实际输入/目录。Windows对应getdaq-config.exe，参数相同，路径按shell规则加引号；Qt5.14.2 MinGW、macOS、Ubuntu24实际验收仍待完成。

## 9. 验证和原理入口

XcfgWorkspaceCopy复用XcfgDocument、共享快照和已有准确属性扫描，不新增生产依赖。读取原文/include图，按共享策略规划文件，倒序替换include属性；临时候选和最终副本都重载，要求有效摘要与原一致且全部引用严格落在复制文件集合内。

普通和新代码ASan/UBSan/LeakSanitizer模型各78，普通完整GUI76，工作区CLI两组各19进程通过；测试移动原目录后重新加载副本、验证隔离编辑只影响一处，全部自有临时XML/JSON/目录清理。旧对象覆盖16/151、XML导出30回归保持通过；最终快照/离线系统回归和程序绑定见[验证记录](VALIDATION.md)第70.7及build-workspace-copy/checkpoint.json。

本轮没有大体积原始传输文件，数据接收器未改。每轮所有传输流合计20GiB上限及结束删除约束继续保持；Qt控制/配置和原生DAQ数据路径仍分开。目标平台、真实控制权/加载/恢复、持续采集/断电和整套验收没有因此完成。

相关：[XML导出](CONFIGURATION_XML_EXPORT.md)、[对象覆盖及共享投影](CONFIGURATION_OBJECT_OVERRIDE.md)、[准确撤销](CONFIGURATION_UNDO_REDO.md)、[旧命令迁移表](CONFIGURATION_COMMAND_MIGRATION.md)。
