# 配置发布包：从编辑到ECC固定绑定

## 1. 当前实现和边界

本地发布包把一次编译所用的原始`.xcfg`及include、完整system profile、有效配置快照和`compile-system`工件放在同一个新目录中。ECC可以选择要求这个包，并在启动、提交命令及故障核对时检查发布清单、来源文件和文件身份。GUI只负责本地生成，原始MFM数据仍由独立receiver接收。

目前已经实现本地包生成、无Qt包校验、固定ECC输入绑定，以及独立[配置仓库上传/登记/完整版本查询](CONFIGURATION_REPOSITORY.md)。仓库目前只经过本机真实HTTP联调，自动将版本选择为ECC输入、受控切换、全局硬件控制权及实际加载读回仍待接；不会因包校验或登记通过而把`actual_loaded_configuration_verified`改成true。若手动复制包到ECC主机，必须复制完整目录、保持包内字节，并设置ECC运行用户的属主/权限，随后重新离线检查。

发布目录在软件契约中只创建一次，禁止覆盖/补写已有版本；文件保存为只读。文件属主仍可通过操作系统改变权限或替换目录，所以它不是操作系统不可修改存储。ECC发现来源、权限、内容或目录身份变化会保持故障，改回原文件不自动解除同一对象的门槛。

## 2. 为什么不能只发布一个工件

完整工件确定执行参数，但还需要知道这些参数由哪份源文件、默认模板、共享范围及硬件兼容profile编译而来。如果分别复制文件，很容易出现“工件来自上午，profile来自下午”的混合版本。

包清单同时绑定以下摘要：

| 摘要 | 范围 |
| --- | --- |
| `configuration_sha256` | 纯执行参数的确定编码，与已有system工件相同 |
| `effective_sha256` | 已加载有效对象/标量、单位与范围的支持语义，与[有效快照](EFFECTIVE_CONFIGURATION_SNAPSHOTS.md)相同 |
| `snapshot_sha256` | 有效快照的完整来源和记录版本 |
| 每个文件的`sha256` | 收到并保存的完整原字节 |
| 发布清单文件SHA-256 | 本次包的完整绑定；由生成结果提供，填写进ECC配置 |

这些域不可混用。修改原文注释可能保持执行参数摘要，却改变来源/快照/发布摘要；移动包目录不会改变包内字节摘要，但ECC的本地固定路径/身份绑定会改变。旧持久ECC目录不能因此被直接用于另一套发布输入。

## 3. GUI完整操作

1. 打开**GET configuration files...**，选择实验入口`.xcfg`。先查询或比较有效值，核对默认/include及共享单位/范围的影响。
2. 点击**Build configuration publication...**，选择已审阅的system profile JSON。这里必须明确给出板型、芯片版本、读指针/延迟布局及兼容策略，不能由工具猜测；profile格式见[系统工件](SYSTEM_EXPERIMENT_CONFIGURATION.md)。
3. 接着输入一个**尚不存在的新目录名**，例如`experiment-20261007-v1`。父目录必须已经存在；即使目标目录是空的，也不会被复用。
4. 程序在后台核对来源、生成有效快照、编译system工件，再核对来源/profile并检查包内一致性。运行时不发送ECC或板端命令。
5. 成功后显示`publication.json`完整路径及其SHA-256。保存这些信息用于部署审阅。若期间切换到另一份配置，界面明确说明该包属于先前加载的配置。
6. 连接期间仍允许生成包，但禁止通过配置页面修改源文件；生成包并不使当前ECC改用它。实际选用哪个包由ECC固定输入和后续操作流程决定。

## 4. 命令行生成和检查

在Linux工作目录中，例如：

```bash
mkdir -p ./reviewed-publications
./build-suite/bin/getdaq-config publish-system \
  /path/to/configure-Experiment.xcfg \
  /path/to/reviewed-system-profile.json \
  ./reviewed-publications/Experiment-v1

./build-ice-compat/getdaq-system-config inspect-publication \
  ./reviewed-publications/Experiment-v1/publication.json
```

生成成功返回JSON收据，含manifest_file/manifest_sha256、artifact_file/artifact_sha256及三种配置摘要。`hardware_loaded_verified:false`明确表示没有板端加载证明。失败退出1，参数错误退出2；已有目录拒绝，不擦除旧内容。

`inspect-publication`检查清单格式、全部文件字节、工件执行参数/profile摘要和来源表，并由标准C++11核心重新校验快照结构、类型、规范值、范围、来源和两个内部摘要。输出`effective_snapshot_semantics_recomputed:true`表示已完成这些快照语义检查；这不重新解析原XML或执行旧CompoundConfig库，也不证明板端加载。它不连接硬件、不创建ECC或receiver根。

无需Qt/Ice的独立工具`getdaq-config-check`也提供相同检查：`publication PACKAGE/publication.json`检查包，`snapshot SNAPSHOT.json`检查单个有效快照，`experiment EXPERIMENT.json`检查完整工件。从`common/experiment`的CMake工程构建，或在根Qt Creator工程中构建无Qt的configcheck子工程；详细构建和兼容边界见[有效快照](EFFECTIVE_CONFIGURATION_SNAPSHOTS.md)。

## 5. 目录和清单内容

```text
Experiment-v1/                  Unix上要求0700
  experiment.json              完整编译工件
  profile.json                 收到的profile原字节
  effective.json               本次有效快照
  source-000.xcfg               第一个来源原字节
  source-001.xcfg               其他来源，最多256份
  publication.json             最后写入的清单
```

Unix包内文件保存为0400。清单格式1，kind为`getdaq.system-configuration-publication`，内容包括：

| 字段 | 内容 |
| --- | --- |
| `root_source` | 原加载入口的来源路径 |
| `configuration_sha256` / `effective_sha256` / `snapshot_sha256` | 上述三种不同域的配置版本 |
| `artifact` / `profile` / `effective_snapshot` | 固定stored文件名、内容sha256、十进制字符串bytes |
| `sources[].source_path` | 原来源标识，按UTF-8字节词法顺序排序 |
| `sources[].file` | source-NNN.xcfg存储名、sha256、bytes |

source文件按原来源排序分配编号，原文、注释、未知属性与include声明不改写。这些文件用于来源复核；其中原有相对/绝对include路径不会因打包而重写。运行ECC消费的是编译工件，不会把`source-000.xcfg`当成已重定位且可直接重新编译的实验入口。原来源路径是追溯标识，包搬到另一主机后仍可校验，不要求那里存在原开发机路径。

清单拒绝未知/重复字段、坏UTF-8、非规范大小、不受支持的格式、重复或未排序来源、错误根来源、非法文件名/越界数量。stored角色为固定文件名，没有由输入指定的`../`或任意外部路径。

## 6. 让固定ECC选择发布包

现有不含此字段的配置继续沿用原工件绑定。要求发布包时，在ECC固定JSON增加：

```json
"configuration_publication": {
  "path": "/home/daq/reviewed-publications/Experiment-v1/publication.json",
  "sha256": "填写生成收据中的manifest_sha256"
}
```

同时把已有`experiment_artifact.path`设为**同一包内**的`experiment.json`，sha256设为收据中的artifact_sha256。即使另一个路径中的工件字节完全相同，也不能用它替代已选发布包的工件。这使输入地址和来源包保持唯一绑定。

其余topology、硬件描述、sender、receiver计划、凭据与控制策略仍需独立审阅；它们不会由system profile自动生成，也没有被源配置包悄悄替换。完整固定服务字段见[网络ECC手册](ECC_NETWORK_SERVICE.md)。

启动前先执行：

```bash
./build-ice-compat/getdaq-ecc-server --check-config \
  --config /home/daq/getdaq-input/ecc-service.json
```

这一步核对本地固定输入，不启动receiver、不创建ECC审计根、不发送远端动作。发布目录必须属于ECC运行用户且权限0700；包内清单/文件必须属于该用户、普通文件、无任何写权限，不能是符号链接。全部引用的大小和原字节再核对，目录设备号/inode与持有的描述符保持一致。

启用包时固定输入总预算为256MiB，未启用时保留128MiB。每个来源最多8MiB、来源合计64MiB；profile最多1MiB、编译工件及有效快照各最多64MiB，清单最多1MiB。包外硬件描述等输入也计入固定总预算，超限拒绝。

构造成功后，全部包内文件加入FixedEccConfiguration的持续输入核对；发现变化会保持fault。NativeEccCoordinator的`snapshot.json`加入`configuration_publication_sha256`，父快照摘要因此绑定所选包；`deployment_source_sha256`也包含新固定输入集合。原`actual_loaded_configuration_verified:false`和控制权声明含义保持不变。

## 7. 发布失败和进程中断怎样处理

在创建目录之前，程序先完成源/profile核对、编译与内存包一致性检查。输入错误不会产生发布目录。目录创建为新目标，其他版本不覆盖。

创建后文件逐个写入并同步：Unix对文件内容、只读权限及目录执行`fsync`；Windows调用CRT `_commit`提交文件内容，并在只读权限设置后再次提交。全部数据文件完成后才写`publication.json`并在Unix再次同步目录。Windows目录项的断电持久性没有作等价保证。创建后失败会保留目录并显示路径供核查，不递归删除，也不在同目录自动续写。使用新目录重新生成，旧目录作为失败现场处理。

没有清单、清单截断、丢文件、字节或摘要不符的目录不能供ECC使用。若在清单写入/最终同步后失联，可能已有可读清单；此时先独立检查包和操作结果，不把相同目录再生成一次当作安全重试。检查能证明当前内容一致，不能证明真实断电后一定保留。

Windows生成侧使用Qt文件写入/flush、只读权限和CRT `_commit`；没有声称目录同步、Windows ACL继承或断电持久性等价于Unix。Qt5.14.2/MinGW发布器原生回归见[验证记录71.188](VALIDATION.md#71188-windows-配置发布器文件提交2026-10-22)，但真实掉电及独立干净安装仍待验收。无需新下载库；无Qt验证/固定后端使用已有标准C++11及本地兼容组件，receiver没有新增链接依赖。

## 8. 尚需完成

本地发布包与ECC固定绑定、独立仓库登记分别提供不同证据，都不等于实机加载版本。无Qt快照语义校验已经完成并与GUI共核；仓库上传/登记及失联观察已另有本机验证。后续还需ECC受控版本选择、状态中明确区分选定包/执行工件/实际读回、控制权与硬件UNKNOWN核对恢复、完整旧配置库对照、断电与目标平台验收。当前证据范围见[验证记录](VALIDATION.md)。
