# 有效配置快照和变更审阅

## 1. 它解决什么问题

配置文件中的一行不一定就是某个板卡最终查询到的值。具体实例可能继承`id="*"`中的默认值，参数可能来自include，unit/range也可能由同类另一条声明提供。修改一处默认gain，可能改变几十个具体实例；只看文本差异容易遗漏这种影响。

有效快照把当前已加载树中的具体对象、最终标量值及其来源保存到JSON。比较两个快照时，可以看到哪些具体路径发生了变化，以及变化来自参数、约束还是来源。操作只读取本机配置；没有ECC请求、寄存器写入或原始MFM接收。

这里有两个不同的版本：

| 字段 | 表示什么 | 哪些变化会改变它 |
| --- | --- | --- |
| `effective_sha256` | 已支持语义下的具体对象路径、标量类型/规范值、有效单位和语义范围 | 实例增加/删除、数值或类型变化、单位变化、范围边界/类型/开闭变化 |
| `snapshot_sha256` | 本次已加载来源及完整有效记录 | 以上变化，以及原文件内容、路径、原始值写法、声明类型、范围写法、来源行号变化 |

例如int值`02`和`2`的有效值相同；枚举`{1,2,1}`和`{2,1}`范围相同。只改变这些写法时，第一个摘要保持，第二个摘要改变。把文件移到别处或修改注释也会改变来源摘要。`int 2`与`real 2.0`的类型不同，第一个摘要会改变。

## 2. 在界面中操作

1. 打开主窗口的**GET configuration files...**，再选择实验`.xcfg`入口。include会一起加载，不需要分别导出每个文件。
2. 点击**Export effective snapshot...**，输入一个新的JSON文件名，例如`before-adjustment.json`。已有文件不会覆盖；应另起名字保留历史。
3. 程序在后台核对已加载来源和有效树，成功后显示保存路径与有效摘要。若提示来源变化，重新打开入口后再导出，不能把旧内存内容当成当前磁盘内容。
4. 修改配置并重新打开，或使用页面中的声明保存/实例覆盖功能。然后点击**Compare with saved snapshot...**，选此前保存的快照。
5. 状态栏显示有效参数是否变化、来源记录是否变化，以及标量、对象和来源变化数量。下面的只读JSON区域给出每个路径的before/after和具体字段。比较操作不会保存配置或发送硬件命令。
6. 若需要保留比较结果，可用下节命令行导出审阅文件。GUI当前用于查看结果；导出的快照本身可以另行长期保留。

连接期间禁止编辑本地配置，但允许导出与比较。后台任务使用开始时复制的已加载文档；在任务期间打开其他配置或成功保存修改，会使正在返回的比较失效。若导出已经完成，界面明确说明它属于先前配置，并给出实际保存路径，不把它标成当前配置。

## 3. 命令行完整例子

Linux构建根工程后，工具通常位于构建目录的`bin/`中。下面假设构建目录为`build-suite`：

```bash
./build-suite/bin/getdaq-config snapshot /path/to/prepare-experiment.xcfg before.json
# 调整配置后，使用另一个新文件名。
./build-suite/bin/getdaq-config snapshot /path/to/prepare-experiment.xcfg after.json
./build-suite/bin/getdaq-config diff-snapshot before.json after.json review.json
```

Windows在Qt Creator中构建`configtool`子工程后，可在实际输出目录调用`getdaq-config.exe`，参数相同。带空格的路径用双引号。这里没有宣称Windows Qt5.14.2 MinGW实际运行已经通过，仍须按目标Kit检查。

`snapshot`成功退出0；加载、有效约束、输出或摘要错误退出1。`diff-snapshot`即使发现配置变化也退出0，表示比较完成，应读取报告中的`effective_changed`；文件损坏/未知格式/输出失败退出1。参数用法错误退出2。输出必须是新文件，原文件及其他历史输出不会覆盖。

## 4. 快照字段怎样读

| 字段 | 内容 |
| --- | --- |
| `schema_version` / `kind` | 格式1 / `getdaq.effective-configuration-snapshot` |
| `root_file` / `root_path` | 加载入口的真实文件路径、入口元素路径 |
| `sources` | 所有已加载来源的真实路径、原字节SHA-256和十进制字符串字节数，按路径排序 |
| `objects` | 有效树中的具体对象路径，包括空对象，按路径排序 |
| `scalars` | 按具体查询路径排序的有效标量记录 |
| `scalars[].value` | XML层读取的值，沿用原解析器的首尾空白裁剪 |
| `declared_type` / `type` | 原声明type，及解析后的实际类型；未声明type时前者为空 |
| `normalized_value` | int十进制、hex小写`0x`前缀、bool小写true/false、real可往返的有限double、string原内容 |
| `unit` / `range` | 共享类解析后的有效属性；不会换算数值 |
| `value_source` / `unit_source` / `range_source` | 分别记录声明文件、声明路径和行号；未声明单位/范围时对应对象为空 |

所有整数和实数值保存为字符串，64位hex不会经过JSON double。例如`0xffffffffffffffff`保持精确。real采用C locale和`max_digits10`，`-0.0`与`0.0`的有效值相同；它们的原文本仍分别保存。

默认`*`声明不会作为真实硬件实例输出。它提供给已存在实例的值，会按那些具体路径保存；没有声明的Board[99]不会因为默认模板存在而出现。尚未被具体实例使用的模板或未知XML属性仍通过`sources[].sha256`追踪原字节，不把它们声称为新模型已解释的硬件语义。

## 5. 审阅报告怎样读

报告kind为`getdaq.effective-configuration-review`，包含before/after的两种摘要。`effective_changed`表示支持语义下的有效树不同；`snapshot_changed`表示来源或完整记录不同。`root_changed`表示入口文件或入口路径变化。

`object_changes`列出对象added/removed；这包括空对象。`scalar_changes`列出标量added/removed/changed，每条给出具体路径和before/after。changed的`fields`含义如下：

| 字段名 | 含义 |
| --- | --- |
| `value` | 实际类型或规范值改变 |
| `text_or_declared_type` | 原值写法或显式type改变，可能有效值仍相同 |
| `unit` | 有效单位改变，未做数值换算 |
| `range` | 范围语义改变 |
| `range_text` | 范围写法改变，解析后的语义相同 |
| `value_source` / `unit_source` / `range_source` | 对应来源文件、声明路径或行号改变 |

`source_changes`列出来源文件的added/removed/changed及原字节摘要。只有注释变化时，可能出现来源变化而没有标量变化；默认值改变时会列出所有真正受影响的具体标量。两种摘要都应结合报告查看，不能把“有效值未变”等同“文件没有变化”。

## 6. 不安装Qt或Ice也能检查快照

共享校验器EffectiveConfigurationRecord只依赖标准C++11。Qt页面、发布包读取器和固定ECC复用同一实现，避免维护两套不同的范围/摘要算法。独立程序可这样构建和使用：

```bash
cmake -S common/experiment -B build-experiment -DCMAKE_BUILD_TYPE=Release
cmake --build build-experiment --target getdaq-config-check -j2
./build-experiment/getdaq-config-check snapshot before.json
./build-experiment/getdaq-config-check publication /path/to/Experiment-v1/publication.json
./build-experiment/getdaq-config-check experiment /path/to/Experiment-v1/experiment.json
```

根qmake工程也增加无Qt的configcheck子工程，程序为getdaq-config-check。Linux已检查其动态依赖，无Qt、Ice、yaml、curl或Boost；系统C++运行库仍需要。三个目标平台的实际构建/Unicode文件路径/部署验收仍待完成，不能把标准C++11语法视为平台已验收。

检查器严格读取JSON，重新计算两个快照摘要，并检查类型/规范值、范围成员、有效树父对象/重复/排序、来源及预算。成功退出0、错误退出1、用法错误退出2，输出少量JSON结果。它只读取文件，不保存修改、连接ECC/板卡或接收MFM。独立工具检查内容；固定ECC还持续核对所持有的包文件/目录身份和权限，两者作用不同。

即使手工改动有效JSON并重新计算外层文件/清单SHA，错误的规范值、范围或内部有效摘要仍会被拒绝。有效快照正确不代表原XML已由该工具重新编译；原XML展开仍由XcfgDocument/已支持的配置读取器完成，旧库/Spirit全量对照另待完成。

## 7. 摘要协议和兼容边界

摘要不依赖JSON键顺序或缩进。内部对每个字段编码为**十进制UTF-8字节数 + 冒号 + 字段字节**，然后按以下确定顺序计算SHA-256，长度前缀避免字符串拼接歧义：

1. 有效摘要域前缀是`getdaq.effective-configuration:1`加换行；后接root_path、对象数量和全部对象路径、标量数量，再逐标量加入path/type/normalized_value/unit/规范范围。
2. 范围规范编码也使用长度前缀。未声明为`undeclared`；枚举为`enum`、去重后的数量、按类型和值排序的各项type/value；区间为`interval`、下界`unbounded|closed|open`及有界时的type/value、上界相同字段。无界端点忽略方向，遵循已实现的范围等价规则。
3. 完整记录摘要域前缀是`getdaq.effective-configuration-snapshot:1`加换行；后接root_file/root_path、对象数量/路径、来源数量与每个path/sha256/bytes、标量数量与每个path/value/declared_type/type/normalized_value/unit/range，最后加入该标量三种来源各自的file/path/十进制line。空来源编码为空file、空path和line `0`。

读取时重新核对字段集合、类型、来源归属、具体树父对象、排序/重复、范围和值、两个摘要。重复JSON键（含转义后同名）、未知字段、坏UTF-8、截断、摘要或类型不一致都拒绝。文件重新缩进或JSON键重排可以读取，不会产生新的版本。

schema1沿用Qt的UTF-16码元词法顺序，包含补充平面字符的路径也保持这一顺序；不能改成UTF-8字节排序后沿用旧摘要。标准核心显式实现该顺序，生产者不需要Qt才能完成校验。

快照JSON最多64MiB，单字符串最多1MiB，最多256来源；对象和标量合计最多200000个，具体路径最多65536 UTF-8字节，累计有效路径最多64MiB。索引缓存累计最多2000000子项，重复查询复用索引，但不改变具体ID优先、默认回溯或include冲突规则。普通控制配置JSON仍保留原4096字节字符串/4096数组项预算，只有快照读取显式选择扩展预算。

这是本机已加载配置的持久审阅记录。SHA-256提供内容核对，不能证明操作者身份、发布授权、远端控制权或板卡实际加载。来源捕获前后检查也不是跨进程文件锁。保存为新文件，写入失败会清理本次未完成文件；进程崩溃/断电仍可能留下不完整文件，读取会拒绝，当前没有目录fsync或硬件加载事务保证。

新int64/uint64是旧32位Int/Hex之外的明确扩展；旧Spirit/完整CompoundConfig运行对照、完整对象物化/结构编辑、不可变远端发布和实际加载版本绑定仍待完成。旧模式的Run收尾原字节备份是另一种记录，见[配置依赖快照](CONFIGURATION_SNAPSHOTS.md)，不能互相冒充远端加载凭据。
