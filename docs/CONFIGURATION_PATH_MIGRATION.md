# 配置文件路径迁移与中断恢复

本功能实际移动配置来源文件，并改写所有已打开根配置中相关的XML `include`。它与[跨文件移动声明](CONFIGURATION_MULTI_DOCUMENT.md#10-跨文件移动现有声明)、[创建隔离副本](CONFIGURATION_WORKSPACE_COPY.md)不同：成功以后，旧来源路径不存在，新来源路径保存迁移后的内容。

迁移只处理本地配置文件。它不移动`.graw`，不调用板卡、不切换运行中的ECC配置，也不解除UNKNOWN。已有发布工件及历史Run记录保留原绑定；文件移动成功不表示新配置已部署到硬件。

## 1. 一个具体例子

原来有两个实验根文件，共用一份模板；模板又引用自己的子文件：

```text
cfg/
  experiment-a.xcfg       include="templates/shared.xcfg"
  experiment-b.xcfg       include="templates/shared.xcfg"
  templates/
    shared.xcfg           include="range.xcfg"
    range.xcfg
  common/                 已经存在的目标目录
```

把`templates/shared.xcfg`迁移成`common/shared.xcfg`以后：

- 两个实验根的引用都改为`common/shared.xcfg`。
- 新位置的模板引用改为`../templates/range.xcfg`，仍使用原来的范围文件。
- `range.xcfg`不移动，也不修改。
- 旧`templates/shared.xcfg`在新文件和全部引用提交成功后才删除。

预览逐个列出原路径、新路径、引用原字符串和新字符串，以及每个已打开根的有效配置差异。有效对象、标量值、类型、单位和范围必须保持一致；只有来源路径、来源字节和快照版本等记录可以变化。

## 2. 使用前准备

1. Disconnect后进入配置工作区。不要让旧工具、外部编辑器、另一个操作系统环境或其他主机同时编辑这些来源。
2. 将所有受影响的根配置加入同一个多文档工作区。例如上例要同时加入experiment-a和experiment-b，不能只打开其中一个。
3. 保存或撤销已有的暂存修改。路径迁移从已经核对的磁盘版本开始，不将未保存文本混入一次文件删除操作。
4. 先创建目标父目录；目标文件必须不存在。软件不会覆盖同名文件，不自动创建整棵目录。
5. 保留配置备份及足够的恢复记录空间。迁移记录保存原始和候选版本；配置来源总量仍受64MiB限制，与DAQ数据目录分开。

迁移预览只知道当前工作区内的根和来源，仍会以`unopened_referrers_checked=false`标明它没有把发现结果自动加入迁移。现在可以先用只读扫描器列出选定工程目录中的引用者和候选顶层根，但它不能证明目录外、其他主机或其他文件格式中不存在引用。扫描只有在选定目录下的常规`.xcfg`/`.xml`文件都可读、XML有效且没有越界include、符号链接、未完成保存或预算超限时，才返回`complete=true`。`complete`只表示此扫描范围内的清单完整，不等同于所有实验引用均已发现；执行迁移前仍须人工核对并把所有受影响根加入工作区。

## 3. 在窗口操作

1. 打开 **GET configuration files...**，进入 **Multi-document workspace / Save All...**。
2. 使用 **Add root configuration...**加入相关根。保存或撤销待保存文本，直到没有未保存来源。
3. 点击 **Migrate a saved source file path...**。
4. 在来源列表选择要移动的准确文件；填写新文件路径，或点击 **Choose destination...**选择。文件名相同但目录不同时，核对完整路径。
5. 点击 **Preview all roots and references**。检查将新增、替换、删除的路径，所有`include_rewrites`以及各根`root_reviews`；再在来源列表逐份比较原始文本和候选文本，新增/删除路径会明确显示不存在状态。
6. 点击 **Confirm reviewed path migration...**。确认框默认取消。确认后会创建持久记录，再创建新文件、更新引用，最后删除旧文件。
7. 成功后工作区重新打开新的根及来源，使用新的空历史。状态栏保留恢复记录路径。旧路径上的Undo历史不能直接用于新路径。

改变来源或目标会使旧预览失效；保存时还会重新核对准确来源字节、目标不存在、工作区代次、候选字节和报告。不能用过期预览覆盖后来出现的同名文件。

只读模式不允许确认写入。已审阅但未保存的文本、正在输入的未审阅文字以及部分保存冻结状态也会禁用迁移入口。GUI一次迁移一个来源；核心能够一次校验多个源到新路径的映射，适用于根文件及其依赖同时迁移。

## 4. 命令行入口

所有参数都是配置文件路径，与数据传输地址无关。先预览：

可先对包含工程配置的目录做只读发现。退出码`0`表示扫描范围内清单完整，退出码`3`表示报告不完整；两种情况都会输出JSON，不完整报告不能用于证明没有其他引用。

```bash
getdaq-config discover-project-referrers /home/daq/cfg /home/daq/cfg/templates/shared.xcfg
```

报告列出所有已发现的直接/传递引用者、候选顶层根、扫描文件数和问题。它不改文件，也不会代替下面的迁移预览：

```bash
getdaq-config preview-path-migration \
  /home/daq/cfg/templates/shared.xcfg \
  /home/daq/cfg/common/shared.xcfg \
  /home/daq/cfg/experiment-a.xcfg \
  /home/daq/cfg/experiment-b.xcfg
```

前两个路径是旧来源和新来源，后面一个或多个路径是需要一起核对的根文件。根文件可以就是要移动的文件，但旧来源必须属于这些根加载到的已知来源集合。

检查JSON后，明确执行：

```bash
getdaq-config migrate-path \
  /home/daq/cfg/templates/shared.xcfg \
  /home/daq/cfg/common/shared.xcfg \
  /home/daq/cfg/experiment-a.xcfg \
  /home/daq/cfg/experiment-b.xcfg
```

执行命令会重新计算和核对当时的全部输入，不直接采用上一次stdout中的报告。CLI本身没有交互确认框；调用`migrate-path`就是明确写入意图。成功输出`complete`、`recovery_directory`和新`roots`；失败输出错误及已经创建的记录路径，不自动重试。

Windows使用同一个命令和参数顺序，给带空格的路径加引号。Windows原生工具与WSL工具不共享配置锁域，不能同时操作同一批来源。

## 5. 中断以后如何判断

迁移依次执行：

1. 完整保存原始/候选来源及路径映射。
2. 为所有相关路径建立待核对标记，包括尚未创建的新路径。
3. 准备全部新增和替换文件的完整临时内容。
4. 发布新增文件，保持目标不存在才可创建。
5. 替换需要更新的已有文件，包括引用文件。
6. 删除不再属于候选集合的旧来源。
7. 核对全部文件及不存在路径，保存完成记录，再清理自有标记。

步骤4到6之间发生进程中断时，可以同时存在新旧文件、部分引用已更新、或者旧文件已经删除。普通加载仍检查标记，包括已经消失的旧路径；出现`unfinished Save All`时，应按报错记录检查，不能通过删标记继续运行。

读写协调沿用[共享读、独占写协议](CONFIGURATION_SOURCE_TRANSACTIONS.md)。它保护同域的新版合作工具；不保证外部文本编辑器、旧程序或其他主机永远只看见全旧或全新。

## 6. 使用原记录恢复

记录放在第一个原根文件的父目录：

```text
.getdaq-workspace-recovery/
  move-UUID/
    manifest.json         原/新根、迁移映射、各路径前后存在状态和摘要
    original/             原来存在的文件，按manifest索引保存
    candidate/            迁移后应当存在的文件，按manifest索引保存
    writer.lock           当前活保存者或恢复者的锁
    complete.json         全部目标状态核对后才创建
```

窗口中使用 **Inspect interrupted save...**选择准确的`move-UUID`目录。恢复窗口会识别迁移记录，显示当前或目标路径不存在的状态。

- **Finish the reviewed candidate**：完成原迁移目标，包含创建新路径、更新引用和删除旧路径。
- **Restore the recorded original sources**：恢复原来源及引用，删除本次迁移新增的路径。

先预览所有当前/目标字节及存在状态，再明确确认。原保存者还活着、出现第三种内容、记录或摘要被改动、路径变成符号链接/目录、属于另一个待核对记录时，恢复拒绝。已有完成记录也拒绝重放。

恢复命令同样分为预览和明确执行：

```bash
getdaq-config preview-path-recovery /home/daq/cfg/.getdaq-workspace-recovery/move-UUID candidate
getdaq-config recover-path /home/daq/cfg/.getdaq-workspace-recovery/move-UUID candidate
```

回到原始版本时，将最后一个参数改为`original`。恢复本身再次中断，仍保留原记录，重新核对同一记录和所选方向；不需要另造一个迁移请求。恢复后重新打开报告列出的根文件。

## 7. 边界与实现

迁移只改已加载XML的字面`include`，不猜测标量文本中是否藏了文件名。不更新YAML、外部profile、硬件描述路径、固件引用、程序启动脚本或其他未加载工程。根文件移动后，其他位置保存的原根路径也需要使用者明确更新。不同盘符下的引用可能成为绝对路径，以预览为准。

不覆盖现有目标，不支持以路径互换或仅修改大小写来绕过目标存在检查。不自动删除空目录，不保留原文件的NTFS自定义ACL、所有权或完整元数据；新来源使用当前用户的私有读写权限。硬链接别名和未打开引用者不属于全局发现范围。

`XcfgSourceMigration`复用已有解析器、结构属性改写、快照和工作区校验。候选允许“尚未存在的新路径”，但只允许从已审阅的封闭内存来源图读取；普通`XcfgDocument::Load`没有启用这个内部模式，不能借此跳过缺文件或来源检查。

原始、候选各最多256来源/64MiB，每文件8MiB；路径并集最多512，根最多16。恢复根最多256记录、已有原始与候选载荷累计上限2GiB，达到上限时在来源写入前拒绝。没有自动清理恢复记录，防止删除未完成操作的唯一副本。

新增文件先在目标父目录内写完整临时文件并同步，再使用平台原子新路径发布，拒绝替换已存在目标。已有来源仍逐文件原子替换并同步。POSIX同步文件和父目录；Windows同步文件，不宣称实现等价的目录断电持久性。进程被立即终止可能留下自有临时配置文件，应保留现场核对；不得按模糊名称批量删除别的会话文件。

本轮验证采用小型自有XML，不生成DAQ数据。最终普通、插桩、Windows、真实进程退出及窗口测试范围见[验证记录](VALIDATION.md)。这不是实机、真实断电、任意外部读者的跨文件原子事务或目标平台现场验收证明。
