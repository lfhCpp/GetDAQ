# 继承对象的浅层覆盖、相关兄弟与撤销

更新时间：2026-10-10。

配置结构窗口新增 **Shallow override inherited object by query path**。它把继承的对象层级变成本地空声明，继续从原模板继承参数；不会复制整棵参数树，也不会改当前有效值。原始数据通路和硬件命令不参与这项操作。

## 1. 为什么对象覆盖不能用参数覆盖或物化代替

```xml
<Setup>
  <Board id="*">
    <Module><gain type="int">2</gain></Module>
  </Board>
  <Board id="0"/>
</Setup>
```

此时Lookup `Setup/Board[0]/Module/gain`得到2，但Board[0]没有自己的Module。

- **浅层对象覆盖**创建Board[0]中的空Module。gain仍来自模板，值还是2。
- **scalar覆盖**只为具体gain写入本地值，可以改变该参数。
- **物化**把当前有效子树完整写成具体内容，固定已有参数和对象。

浅层覆盖后的关键部分是：

```xml
<Board id="0">
  <Module></Module>
</Board>
```

如果再覆盖Module下继承的Group，会连同缺少的本地Module祖先一起创建空Group；未选中的其他子对象和参数仍继承。不会从只有默认模板的配置凭空生成不存在的板号或通道。

## 2. 选择 `*` 时为什么还要复制同名兄弟

原模板中有Module[*]、Module[1]和Module[2]，Board[0]全部继承它们。选择Board[0]/Module[*]，旧ConfigWidgets实际算法会浅层补齐这三个同名对象，清除新复制对象的include地址，保留原继承链。

因此界面会列出三个 **Created object layers**；实际具体投影只有Module[1]和Module[2]，另列在 **Affected concrete projections**。选择Module[1]时则只补齐它。已有本地具体兄弟保持；其他元素名不复制。

顺序按旧视图的实际来源顺序枚举，再逐项前插；不是按数字ID重新排序。原13种结构操作的编号保持，新对象覆盖追加为第14项。

## 3. 界面逐步操作

1. 断开操作台连接后打开本地主配置。连接模式禁用结构写入。
2. 在Lookup中输入完整的继承对象路径，例如 `Setup/Board[0]/Module`。
3. 在来源树中选一个声明，点击 **Edit selected declaration structure...**。这个选择打开审阅窗口；本操作的真正目标由Object override query指定。
4. 选择 **Shallow override inherited object by query path**。Object override query默认取Lookup输入，可在窗口中修改。
5. 点击 **Preview change**。预览不写任何来源文件。
6. 先核对提示中的 **Destination**。它可能是主文件，也可能是实际拥有最近非继承父对象的include文件。
7. 查看 **Created object layers**，确认新增的空对象、缺失祖先和相关同名兄弟。
8. 查看 **Affected concrete projections**，确认所有变成本地声明的具体路径。特别关注同一个include文件的其他使用者。
9. 查看原/拟写XML、输入依赖和完整来源差异。Effective parameters changed应为no；对象、参数、类型、单位和语义范围的整体有效摘要必须保持。
10. 点击 **Apply reviewed change**。保存会重新生成预览，并核对全部来源字节、两个快照、差异、创建路径和受影响投影，然后原子替换一个来源文件并重载。
11. 操作进入同一会话Undo/Redo。点击Review undo，审阅原文件准确字节后应用；撤销会同时移除本次创建的祖先和兄弟，不影响原模板。

修改路径或操作会撤下旧Apply。重开文档/窗口版本改变、任一加载来源变动或审阅记录被修改，都会拒绝旧保存。已经是直接声明的对象不再覆盖；需要改它时用已有结构/值操作。

## 4. include来源与多个投影

当前层include直接引入的对象在旧模型中不是继承对象。例如Board[0]直接include一个含Module的文件，Module已经是直接声明，本操作拒绝重复创建以免碰撞。

如果Module[0]本身直接来自include文件，但其Group继承自Module[*]，可以覆盖Group；目标文件就是实际包含Module[0]的那个文件，主文件保持原字节。

新版编辑器以真实来源文件为编辑单位。同一include文件被Board[0]和Board[1]使用时，写入它的Module[0]/Group会使两个投影都使用新的本地Group，界面必须列出两条Affected concrete projections。创建路径描述本次操作请求，投影列表描述所有物理来源使用者，二者数量可以不同。

这是来源文件编辑的行为，不能称为只修改一个逻辑include副本。旧模型分别装载的多个include内存副本与这种物理文件编辑不一定有相同声明投影；本轮没有宣称多次引用的完整声明树等价。需要只影响一个使用者时，先明确拆分配置来源，再重新审阅。没有自动复制外部文件或迁移多个文件来绕过这个边界。

## 5. 保留什么，改动什么

- 新对象保留来源名称、ID及可保留属性/对象注释；include地址清除，以免新层级再次引用同一个文件。
- 对象内的参数和其他子对象不复制；它们继续继承。
- 原模板、其他来源文件、未选本地兄弟和原有效值保持。写入include来源时，其全部使用投影按上一节处理。
- 共享unit/range和type不因此变化，不做单位换算；实际硬件加载和已发布配置也不随编辑改变。

保存仍是单文件QSaveFile替换，非跨文件事务、独占锁或断电恢复。写后重载/核对失败必须报告“已保存但检查失败”，不要假定没有写入。会话撤销不回滚ECC、发布包、配置仓库或真实硬件。

## 6. CLI先预览，再明确写入

```bash
# 只在stdout输出JSON审阅、拟写XML和路径；不写来源。
bin/getdaq-config preview-object-override config/experiment.xcfg 'Setup/Board[0]/Module'

# 明确请求写入，内部重新建立候选并核对来源。
bin/getdaq-config override-object config/experiment.xcfg 'Setup/Board[0]/Module'
```

以上输入/对象路径请替换为实际配置。JSON包含saved、destination_file、created_paths、localized_paths、updated_xml和review。CLI调用相互独立，不提供跨进程持久Undo；需要会话撤销时使用界面。

## 7. 失败提示

| 提示 | 含义与处理 |
| --- | --- |
| already a direct declaration | 该对象不依赖默认回溯，或已完成覆盖；不会再插一个同名对象 |
| requires an object | 选了scalar，请用参数覆盖或选父对象 |
| No declared instance/value | 对象路径不存在；模板不能单独创建未知实例 |
| changes effective values or objects | 候选不能保持原有效语义，拒绝写入，不靠展开/断开include强行放行 |
| preview differs / Source changed | 审阅或来源发生变化，重新打开并预览 |
| saved, but reload/verification failed | 文件已经替换，保留错误现场，重新打开检查 |

## 8. 原代码对照和本机验证范围

本轮构建未修改的ConfigNode、ConfigModelAccessor、OverrideConfigNode、基类/错误处理和实际Mordicus日志，共13次C++11编译，链接54个摘要核对的旧CCfg/TinyXml对象，并由moc生成原QObject代码。这里实际调用原redo/undo/redo及原对象复制和视图，不使用新实现生成旧期望；并未构建整个旧ConfigWidgets界面。

运行对照16场景：13个实际覆盖、3个直接声明/根的无动作，包含嵌套祖先、`*`兄弟、来源顺序、已有本地兄弟、对象注释、继承include、清新include地址及外部父对象。核对旧撤销/重做的有效摘要、前后原来源字节，以及新旧包含默认声明的对象树及顺序。共享文件多投影另外以新模型验证其明确来源编辑行为。

新配置模型、完整GUI、普通和插桩CLI的最终同版结果及检查点见[验证记录](VALIDATION.md)第70.6。临时XML/JSON结束自动清理，没有本轮原始DAQ传输；所有流合计20GiB上限及结束删除继续有效。Qt系统库和旧参考库未整体插桩；目标平台/实机、跨文件工作区、加载/控制权/恢复及持续采集/断电仍待完成。

旧命令对应见[迁移表](CONFIGURATION_COMMAND_MIGRATION.md)，准确字节历史见[Undo/Redo](CONFIGURATION_UNDO_REDO.md)，另存/导出见[XML导出](CONFIGURATION_XML_EXPORT.md)。
