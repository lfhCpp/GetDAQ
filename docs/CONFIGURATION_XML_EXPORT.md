# XML 配置导出、整文件副本与另存后编辑

更新时间：2026-10-10。

配置窗口现在可以把一个对象或整份配置导出为可重新打开的 `.xcfg`/`.xml` 文件。它与 JSON有效快照、编译工件和发布包是不同用途：XML副本用于继续编辑或作为本地include内容；快照用于版本审阅；发布包用于固定工件和来源。导出XML不会改变板卡正在使用的配置。

入口是 **GET configuration files... → Review XML export / Save As...**。CLI是 `getdaq-config export-xml`。生成文件必须使用新名称；已有文件、目录或符号链接目标均拒绝覆盖。

## 1. 先选择需要的结果

| 模式 | 输出内容 | 默认声明 `*` | 外层继承 | include |
| --- | --- | --- | --- | --- |
| Flatten declarations (keep defaults) | 选定来源声明及其实际子内容 | 保留内部模板 | 不补进选定声明之外的继承值 | 递归展开为本文件内容 |
| Current effective concrete object | 当前具体对象可查询的全部有效对象、参数 | 不输出内部模板；只输出已有具体实例 | 固定到输出中 | 全部展开，不留引用 |

如果路径选择文件根，例如 `Setup`，就导出整份配置副本。第一种模式保留模板，适合继续编辑默认值；第二种模式适合固定当前已经存在的具体对象及其参数。

如果路径选择 `Setup/Node[CoBo]/Instance[0]`，导出根变成 `Instance[0]`。第一种模式不会自动把其他祖先中的默认值复制进来；第二种会把当前可见的有效值一起固定。因此“声明副本为空”不一定是错误，可能该实例的值全来自外层模板。

导出根是独立容器。若声明模式选中 `Board[*]`，输出根写为 `<Board>`，只去掉这个独立根的 `id="*"`；内部对象模板仍保留。没有生成某个真实板号，输出也不能因此直接作为完整系统配置加载。

## 2. 一个能看清区别的例子

原配置：

```xml
<Setup>
  <Board id="*">
    <gain type="int" unit="mV" range="[0,9]">2</gain>
  </Board>
  <Board id="0"/>
</Setup>
```

选中 `Setup/Board[0]`：

- **声明模式**：输出只有 `Board[0]` 空对象。`gain`属于外层默认声明，没有被当成本地内容。
- **有效对象模式**：输出 `Board[0]`，其中有明确 `type="int"`、值2、单位mV和范围`[0,9]`的gain。

选中 `Setup`：

- **声明模式**保留 `Board[*]` 和 `Board[0]`，Board[0]继续从模板继承gain。
- **有效对象模式**只输出 `Board[0]` 的当前具体值。以后修改原模板，不会改变这个已经生成的副本。

不会从一个模板凭空生成没有声明的板、AsAd、AGET或通道。导出也不证明实际硬件存在、固件字段相符或已加载这些值。

## 3. 界面逐步操作

1. 打开需要导出的主配置。包含文件要能在当前机器上读取。
2. 如需子对象，在 **Lookup** 中输入从主文件根开始的完整对象路径。单独的scalar不能作为导出根，请选择其父对象。
3. 点击 **Review XML export / Save As...**。Object query path默认使用Lookup输入；Lookup为空时默认文件根。
4. 选择导出模式。需要具体有效对象时，路径不能带 `[*]`。
5. 填写新输出路径，或点击 **Choose new file...**。父目录必须已经存在；例如先创建 `config/copies/`，再选择 `config/copies/run-0001.xcfg`。
6. 点击 **Build preview**。任务在后台运行，输入项暂时禁用；它不创建所选输出文件，也不修改原文件。内部使用自动清理的小型临时文件重新验证候选XML。
7. 阅读 **Proposed self-contained XML**。声明模式保留可保留的内部注释/CDATA及未知属性，但开始标签、属性顺序和排版可能规范化；有效模式重新生成XML，不保留原排版/注释。
8. 阅读 **Source guards**中的来源路径、字节数和SHA-256。来源集合包含当前已加载的全部文件，包括选定对象之外可能影响共享属性的文件。
9. 查看对象/参数数量和输出有效摘要。有效模式还要求原具体子树的对象、值、类型、单位和语义范围逐项一致，不能只凭“文件能打开”放行。
10. 如需另存后编辑，勾选 **After saving, open the new copy...**。这是显式切换本窗口文档，不切换ECC/板卡；原会话Undo/Redo历史会清空。
11. 点击 **Save reviewed NEW file**。保存前重新构建预览，核对原来源、XML字节、摘要和数量；创建时再次使用“不覆盖”语义。
12. 等待成功信息。生成文件会重新打开并核对字节、有效摘要及原来源；之后可以正常用配置窗口打开，或在确认合适后用include引用其内容。

改变路径、模式、输出名称会使旧预览失效。重新打开配置、修改本窗口文档或Undo/Redo后，旧导出审阅也失效，必须重做预览。

## 4. 只读模式和异步任务

操作台连接期间可以导出只读副本，原声明编辑/结构修改仍禁用。此时不能勾选“另存后打开”，以免把导出动作和当前会话编辑混在一起。导出文件本身不表示连接中的ECC改用了该文件。

如果保存任务已开始，此后打开了另一个文档，旧任务完成后会显示输出属于先前配置，不会自动替换当前编辑器。关闭导出窗口不会取消已经开始的文件工作；输出是否存在及完整性以实际结果为准。后台任务持有独立文档副本，不读取或转发原始MFM事件。

## 5. 保存、撤销和发布的区别

- 该操作创建新文件，不改原文件，所以不加入原文件Undo/Redo历史。撤销原编辑不会删除已经导出的副本。
- 勾选“另存后打开”成功后，重新加载新文件，并清空原窗口的文件历史；原文件仍保留。
- 不重写任何原include文件，也不把新路径写入当前envoy YAML、ECC启动配置、配置仓库选择或板卡。
- 导出有效摘要只描述文件支持域的有效内容，不是签名、硬件加载凭据或当前采集版本确认。

真正发布及选择版本分别见[发布包](CONFIGURATION_PUBLICATION.md)、[配置仓库](CONFIGURATION_REPOSITORY.md)、[ECC选择](ECC_CONFIGURATION_SELECTION.md)。

## 6. CLI示例

在项目根目录、已经构建 `bin/getdaq-config` 后执行。下列输入和输出路径请替换为实际文件：

```bash
mkdir -p config/copies

# 整文件声明副本：保留默认块，展开本地include。
bin/getdaq-config export-xml config/experiment.xcfg Setup declarations config/copies/declarations.xcfg

# 固定当前已有具体对象及其有效值。
bin/getdaq-config export-xml config/experiment.xcfg 'Setup/Node[CoBo]/Instance[0]' effective config/copies/cobo-0.xcfg

# 另行生成有效快照进行检查。
bin/getdaq-config snapshot config/copies/declarations.xcfg config/copies/declarations-snapshot.json
```

Windows Qt5.14.2 MinGW环境使用相应的 `getdaq-config.exe`；命令参数相同。带空格的路径需要按所用shell加引号。仍需要实际Windows编译和运行验收，当前本机通过不能代替它。

CLI没有界面预览步骤，但内部执行同样的候选构建、重算、來源字节检查和输出重读。已有输出拒绝，不提供隐式覆盖开关。

## 7. 常见错误及处理

| 提示或现象 | 含义与处理 |
| --- | --- |
| `XML output already exists` | 原路径已经有文件/目录或链接。使用新名字；失败不会覆盖原内容 |
| `XML output parent directory must exist` | 先创建目录，或选择现有目录 |
| `XML export requires an object` | 选中了参数；改用它的父对象路径 |
| `requires a concrete object path` | 有效模式不能导出模板路径。换声明模式，或选择确实存在的具体对象 |
| `No declared instance/value` | 路径不存在；不凭模板创建新的硬件ID |
| `Configuration changed` / `preview changed` | 文件、窗口版本或输入变了。重新打开并审阅新预览 |
| 声明副本的参数比Lookup少 | Lookup包含外层继承，声明导出只取自己的声明子内容。确认是否应选有效模式 |
| `output created but writing/synchronization failed` | 已创建目标，但写入/同步失败，可能留下部分文件。不要使用它部署，先检查目标和磁盘状态，再选择新名称重试 |
| `output was saved but subsequent verification failed` | 文件已经写过，后续字节/语义或来源检查失败。明确保留“已保存但未核对成功”，不描述成没有写入 |

当前使用新文件创建和Linux文件fsync；输出可能在写入期间被其他进程看到部分内容。没有跨进程文件锁、目录fsync/断电原子发布或跨多个文件的事务保证。需要不可变完整包时使用上述发布流程，不能把普通XML导出当作它。

## 8. 字符串、单位和范围

当前模型会按上下文冻结共享单位和范围；它们可能原来声明在其他实例中。导出将它们写在对应参数上，使独立文件不会因丢失外层类信息而改变约束；数值不做单位换算。

有效模式写明实际type，并用字符引用保存首尾/连续空格、Tab、CR、LF；空字符串保持scalar角色。声明模式保留原scalar的内部片段，包括合法的单段CDATA及前后注释，并补明确type/共享属性。文本规则和旧有意差异见[旧库运行对照](LEGACY_CONFIGURATION_RUNTIME.md)。

限制为8MiB UTF-8输出、128展开深度、200000访问；原输入文件/include/有效树预算继续适用。拆分文本、冲突、类型/范围错误或无法保持具体子树语义时拒绝。尺寸限制针对配置文件，不改变DAQ传输性能或接收器行为。

## 9. 旧实现和新实现的关系

旧 `ConfigEditorController::onExportFile → ConfigModel::exportSubConfig → Document::exportSubConfig` 用NoOpIncludeHandler输出所选对象、展开include。新声明模式接续这一职责，但冻结共享信息、重新解析候选并拒绝覆盖。三组测试实际调用未修改的旧导出方法，比较整个根及两个具体子对象输出的有效语义；不是只阅读旧代码生成期望。

旧 `ConfigModel::saveAs` 调用 `Document::saveToUrl(url,false)`并切换文档URL，包含多文件include保存行为。新版目前提供单文件自包含副本，并可明确打开新副本；没有声称已经复制“保留引用并迁移多个文件”的全部旧SaveAs行为。

完整命令对应关系与仍缺的对象覆盖等见[配置命令迁移表](CONFIGURATION_COMMAND_MIGRATION.md)。独立来源文件路径迁移及恢复已有专门入口；完整复刻旧 `saveAs` 的多文件工作区语义、数据库/网络资源、运行中切换和实际加载仍待实现或验收。

## 10. 验证和复现

普通与新代码ASan/UBSan/LeakSanitizer模型各67；完整普通GUI73；普通/新代码插桩CLI各30进程，分别含3个旧导出实际对照。另运行既有快照CLI12、离线系统编译/原生绑定36和根五程序构建。系统Qt与旧参考库未整体插桩。

```bash
python3 tests/build_legacy_configuration_reference.py --build-dir build-xml-export/reference --probe tests/legacy_configuration_export_probe.cpp --jobs 2
python3 tests/xml_export_integration.py bin/getdaq-config build-xml-export/reference/legacy-configuration-probe --evidence build-xml-export/cli-ordinary
```

当前证据入口为 `build-xml-export/checkpoint.json` 和[验证记录](VALIDATION.md)第70.5。本轮测试只生成自动清理的小型XML/JSON，没有高速数据传输；每轮传输所有流合计不超过20GiB、结束删除的约束继续有效。Ubuntu24、Windows MinGW、macOS、真机、断电和持续采集仍未通过整套验收。
