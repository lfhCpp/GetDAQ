# 本地配置撤销与重做：审阅、操作和边界

本文对应 `common/config/XcfgEditHistory` 和 `apps/envoy/ConfigWindow`。它记录本机已保存的配置编辑，不向ECC或板卡发命令，不接收原始采集数据；原生数据服务不增加Qt或其他依赖。

## 1. 哪些修改进入历史

同一窗口、同一次打开文件期间，下列动作按成功保存的时间进入同一条历史：

| 保存动作 | 目标 | 撤销恢复内容 |
| --- | --- | --- |
| Save selected declaration | 选中scalar的实际来源文件，可能是include | 修改前完整文件字节 |
| Apply previewed override | 具体实例的本地声明目标 | 删除新增覆盖，恢复继承及来源 |
| Apply reviewed change | 结构预览的一个来源文件 | 恢复对象/标量/属性/顺序和include关系 |

结构包括十种当前操作：新增对象、新增标量、删除、改ID、有效子树物化、改名称、深复制、同父前移、同父后移、类型/单位/范围编辑。它们与参数修改、实例覆盖可以混合撤销。

这里只恢复文件。已导出的JSON、发布包、仓库版本、ECC绑定和板端已加载的寄存器不会随撤销改变。历史不是硬件回滚或部署凭据。

## 2. 第一次操作

1. 未连接时打开配置窗口，使用 `Open .xcfg...` 选择主文件。
2. 修改参数或审阅结构预览，点击相应保存按钮。输入框中尚未保存的文字不会进入文件历史。
3. 点击变为可用的 `Review undo...`。此时只是预览，没有写文件。
4. 弹窗显示动作、目标来源及有效scalar/对象变化表，检查共享影响。
5. `Current XML`是当前完整文本，`Proposed XML`是恢复后的文本，`Full effective/source review`保留完整参数/对象/来源变化。
6. 审阅后点击 `Apply reviewed undo`。程序重新核对预览，提交一个文件，再重载整树并验证实际结果。
7. 成功后主树重建；`Review redo...`可按相同步骤重新得到刚撤销的准确字节。

关闭预览而不Apply不会改变文件。输入框自身的文本撤销和已保存文件的历史是两个层次。连接模式只读，两个历史按钮也禁用。

重新Open会开始新历史；关闭窗口后历史丢弃，不写隐藏历史日志或云端。旧审阅弹窗不能跨Open或另一次编辑继续保存。

## 3. 共享来源例子

默认Board通过include提供n=2，两个具体Board继承它：

```text
初始：Board[0]/n=2，Board[1]/n=2
修改include的n=3：两个实例都为3
为Board[0]保存具体覆盖n=7：Board[0]为7，Board[1]仍为3
撤销覆盖：两个实例都为3，新增覆盖XML消失
撤销include值修改：两个实例回到2，两个文件原字节恢复
```

重做按相反顺序恢复。来源变化能解释为什么数值相同也可能更换来源，只看数值不够。

撤销后保存新编辑会丢弃尚未重做的分支。例如2→3→4，撤销到3再保存8，变成2→3→8，不能再重做到4。

## 4. 物化后的旧include也要核对

物化可能把include参数写成本地声明并移除include属性。该文件不再属于当前有效树，但撤销会重新引用它。

历史守卫包含变更前后所有来源，包括当前不可达的旧include。预览时重新读文件并核对字节摘要；被别人修改、删除或更换路径时拒绝，避免恢复引用一个不同的文件。不会自动覆盖include来恢复旧依赖。

错误后先检查文件，决定恢复相同来源字节，或重新打开当前主文件开始新会话。反复点击Apply不能绕过核对。

## 5. 保存前后检查流程

```text
已加载来源仍匹配磁盘 → 当前快照摘要匹配历史当前端
  → 全部依赖守卫匹配 → 替换一个文件字节形成内存候选
  → 原解析器/对象冲突/类型/共享范围检查 → 完整差异审阅
  → Apply重算预览/再查来源 → QSaveFile提交单文件
  → 重载全部来源，核对实际完整快照
```

历史保存准确字节，所以注释、CDATA、空白、未知属性和声明顺序可还原，不用格式化XML模拟恢复。预览绑定历史版本、方向、目标字节和完整变化；篡改、过期或外部变更会拒绝。

普通scalar/override保存也新增重载后的来源字节核对：目标应为本次预期字节，其他来源保持。保存成功但重载/验证失败，错误明确含 `saved, but`，失效模型/历史清空，窗口清旧树，要求重新Open检查；不能解释为文件没写过或盲目自动回滚。

QSaveFile单文件提交和读回不是跨文件事务，也没有锁住其他编辑器，检查与写入之间仍有竞争窗口。保存后失败路径的代码检查不等于所有并发时序或断电验收，不能声称原子恢复多个文件。

## 6. 历史预算

默认最多32条编辑、64MiB保留字节预算。每步保存一个文件前后字节、根/动作/摘要及其他来源摘要，不永久保留整棵模型和全量快照。

超预算时删除最早步骤，保留最近步骤；不改变当前已保存配置。撤销到最早保留状态后按钮禁用，不保证回到Open时的初始文件。

预算核算文件字节和保存标识，不是整个RSS上限；Qt容器、当前树、候选、差异报告和窗口另占内存。原8MiB单来源、64MiB总来源等读取限制保持。

## 7. 常见提示

| 提示 | 处理 |
| --- | --- |
| No edit to undo/redo | 没有步骤，或历史已清空/裁剪 |
| History changed / Configuration changed | 预览过期，关闭后重新审阅 |
| Current configuration differs from history | 核对来源，重新Open |
| History dependency changed | include或不可达旧来源改变，不覆盖它 |
| History preview differs | 内容与重算不一致，重新预览 |
| saved, but ... | 文件可能已保存，检查实际文件后重新Open |

普通范围校验失败且文件未变时，不消耗现有撤销记录。失效历史不能恢复未知配置，也不能恢复板端控制状态。

## 8. 复查与验证边界

```sh
python3 tests/build_qt_control_validation.py tests/xcfg_shared.pro build-xcfg-history
./build-xcfg-history/xcfg-shared-tests
python3 tests/build_qt_control_validation.py tests/tests.pro build-suite-tests
QT_QPA_PLATFORM=offscreen ./build-suite-tests/getdaq-tests
```

模型普通和严格C++11 ASan/UBSan/LeakSanitizer各44 passed，包含十种结构操作字节往返、混合include/覆盖、物化移除include后的漂移拒绝、篡改/过期/外部变更、失败保留历史、新分支、条数及4MiB保留XML的字节预算裁剪。系统Qt未整体插桩。

普通完整GUI67 passed，包含实际窗口保存→审阅撤销→审阅重做、预览不写、差异表、Open清历史/拒绝旧弹窗、只读和结构操作接同一历史。最后同版操作台/配置CLI构建通过，独立快照CLI回归通过。

初始混合测试对具体对象直接include请求覆盖，被现有冲突规则拒绝；改为默认Board通过include提供继承、具体Board再覆盖，未放宽碰撞规则。最初43通过/1失败保留，最终普通/插桩44项和GUI67项为同版结果。

本阶段无原始DAQ传输、真机命令；临时XML私有目录自动清理，最大预算用例约4MiB。后续所有流每轮合计20GiB和测试结束删除约束继续有效。

入口`common/config/xcfgedithistory.*`、`xcfgdocument.*`、`apps/envoy/configwindow.*`。普通/插桩模型日志results-history-current.txt，GUI日志results-history-gui-current.txt。独立`build-xcfg-history/validation-history/`保存同版源码/五构建及小型证据，无凭据/DAQ载荷；`tests/archive_configuration_history_validation.py`默认复查当前输入，`--historical-only`只核对归档。

环境Ubuntu20.04.6/GCC9.4/Qt5.12.8。Qt5.14.2 MinGW、Ubuntu24、macOS、跨文件迁移、数据库/网络include、旧Spirit全库/全部旧命令、在线切换、真实加载/恢复和断电仍待完成。历史覆盖上述当前本地编辑入口，不等于整个旧框架已替换。
