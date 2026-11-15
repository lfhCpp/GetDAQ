# GetDAQ

> 发布范围：本仓库不包含本地历史 `source/` 目录。构建完整原生 ECC／可选 ICE 组件时，需要在本地另行提供版本匹配的 GET/MDaq 历史接口；请勿将这些本地依赖提交到仓库。普通数据程序不依赖该目录。

GET / AT-TPC 的主机端DAQ软件套件，使用C++11重写操作台及必要配套程序。代码优先沿用MUSIC的类、信号槽和 `.h/.cpp/.ui` 结构；专用依赖保留本地来源与许可，尽量离线构建。

Qt负责配置、操作与状态显示。高流量MFM直接进入独立原生receiver，再落盘或导出；原始事件不经过GUI或ECC控制服务。板端 `getHwServer` / `IceEirfu` 保留现有部署。

整套重构仍在进行，尚未通过实机、独立目标平台或持续跨主机吞吐验收。当前实现与剩余工作见[重构进度](docs/REFACTOR_STATUS.md)，可复核证据见[验证记录](docs/VALIDATION.md)。

## 从哪里开始

| 需要做什么 | 阅读入口 |
| --- | --- |
| 无AsAd时在界面直接验证CoBo硬件连接 | [CoBo只读直连](docs/COBO_DIRECT_CONNECTION.md) |
| 从界面独立启动ECC、数据管理服务或receiver | [独立服务启动](docs/LOCAL_SERVICES.md) |
| 在Run Setup界面填写板卡/接收IP并生成、启动本机实机服务 | [实机配置窗口](docs/RUN_SETUP_HARDWARE.md) |
| 第一次安装、编译、生成整套运行目录 | [整套构建与交付](docs/HOST_SUITE_BUILD.md)、[依赖与离线构建](docs/DEPENDENCIES_AND_OFFLINE_BUILD.md) |
| 接入支持 MCP 的 AI 客户端，读取 ECC 状态或显式提交控制操作 | [ECC 控制 MCP 配置、权限和测试](docs/ECC_CONTROL_MCP.md) |
| 在 macOS 构建原生数据工具或 Qt 操作端 | [macOS构建与验收指南](docs/MACOS_BUILD.md)；包含接收、归档、Qt分层及尚未验证的边界 |
| 在Ubuntu24验证当前源码 | [Ubuntu24直接验证及隔离环境说明](docs/UBUNTU24_VALIDATION.md)；71.183在Ubuntu24.04.5用户空间从当前源码全量构建28个程序，Core151/151、Widgets114/114、CTest22/22及四路1GiB合成接收回归通过；28程序候选loader smoke和新波形查看器3D旋转检查通过。该环境共享WSL2宿主内核，独立Ubuntu机器、双主机、实板和持续DAQ吞吐仍待验收 |
| 用模拟器学习操作、准备一次Run | [逐步操作手册](docs/OPERATIONS.md)，以及本页下方的快速开始 |
| 编辑GET参数、include和本地配置文件 | [配置编辑器](docs/CONFIGURATION_EDITOR.md)、[多文档保存](docs/CONFIGURATION_MULTI_DOCUMENT.md)、[文件路径迁移](docs/CONFIGURATION_PATH_MIGRATION.md)、[并发读写及中断恢复](docs/CONFIGURATION_SOURCE_TRANSACTIONS.md) |
| 配置独立数据接收/存储/导出 | [高速数据服务](docs/DATA_SERVICE.md)、[发送器控制](docs/DATA_SENDER_CONTROL.md) |
| 将已封存CoBo波形转成旧Viewer `.gmat` | [矩阵格式、完整样本约束与转换命令](docs/DATA_MATRIX.md) |
| 离线浏览 `.graw` / `.gmat` 通道波形 | [离线波形查看器说明](docs/WAVEFORM_VIEWER.md) |
| 了解配置如何编译并用于整套实验 | [实验参数工件](docs/SYSTEM_EXPERIMENT_CONFIGURATION.md)、[全系统采集](docs/SYSTEM_ACQUISITION.md) |
| 正常结束后更换ECC或接收计划 | [操作台逐步审阅](docs/ECC_PLAN_TRANSITION_GUI.md)、[不同配置与接收计划](docs/ECC_PLAN_TRANSITION.md)、[同计划更替](docs/ECC_SERVICE_HANDOFF.md) |
| 部分切换失败后核对并补齐原请求 | [原实例接续的条件与命令](docs/PARTIAL_HANDOFF_COMPLETION.md)；原失败记录及旧控制禁用保留；当前源码已接入[Qt显式补齐与只读观察](docs/ECC_PLAN_TRANSITION_GUI.md#71-部分失败后在窗口显式补齐) |
| ECC停止后查看原交接证据 | [离线审阅与报告说明](docs/COLD_HANDOFF_REVIEW.md)；不需要保留认证文件，不执行冷恢复 |
| 节点已受理退休、但保存证明失败 | [原节点显式补齐及父回执核对](docs/FAILED_PLAN_RETIREMENT_COMPLETION.md)；保留原失败和旧控制冻结，仅用于本轮新服务源码 |
| 节点交接已失败、但原周期正常停止封存 | [失败节点交接补齐手册](docs/FAILED_CONTROL_HANDOFF_COMPLETION.md)；CLI只读校验并生成请求，操作者审阅后单次提交，父ECC只读核对 |
| 原ECC已关闭，补齐原数据交接并取得接班回执 | [冷数据交接补齐CLI](docs/COLD_DATA_HANDOFF_COMPLETION.md)；仅处理绑定的原数据请求，旧硬件控制仍禁用 |
| 保留其他板状态，只重新准备一块CoBo | [单板维护的操作、原理与接口](docs/COBO_MAINTENANCE.md)；使用包含此功能的新程序，旧70.57包不含该入口 |
| 旧/新ECC状态机及单板按钮的语义 | [ECC状态归属与单板维护入口](docs/ECC_STATE_OWNERSHIP.md) |
| 处理告警或失败采集 | [告警策略](docs/ALARM_STOP_POLICY.md)、[接收故障策略](docs/RECEIVER_FAILURE_STOP_POLICY.md)、[原对象收尾与恢复](docs/ACQUISITION_RECONCILIATION.md) |
| 理解代码和协议，继续开发 | [软件架构](docs/ARCHITECTURE.md)、[工作原理](docs/PRINCIPLES.md)、[协议](docs/PROTOCOLS.md) |
| 查找以前阶段的功能专题 | [功能与历史说明索引](docs/FEATURE_HISTORY.md) |

## 程序怎样分工

| 程序或组件 | 运行位置与职责 |
| --- | --- |
| `GetDAQ` | Qt操作台；配置、命令、状态和Run记录，支持用户Windows Qt5.14.2 MinGW Kit |
| `getdaq-ecc-server` | 主机ECC；硬件控制、状态转换及各数据节点协调，兼容现有GET/Ice边界 |
| `getdaq-control-mcp` | 本机MCP stdio适配器；让AI读取ECC状态，控制工具默认关闭，显式启用后复用ECC认证HTTP/状态机，不触碰DAQ数据流 |
| `getdaq-supervisor` | 每个数据节点的控制服务；管理独立receiver，返回低频状态与证明 |
| `getdaq-receiver` | 原生C++11/POSIX热路径；MFM接收、缓冲、文件轮转和可选导出，不链接Qt |
| `getdaq-ice-receiver` | 可选ICE数据兼容进程；与普通TCP接收器及GUI分别部署 |
| 配置、查询、归档、节点监控及模拟工具 | 按任务独立使用；具体程序清单见构建手册 |

目标环境为Ubuntu24.04，Windows操作端使用Qt5.14.2 MinGW；macOS仍待实际验证。Linux本机回环测试不能代替这些目标环境的完整验收。

## 无 Qt 数据程序

```sh
cmake -S apps/datareceiver -B build-receiver -DCMAKE_BUILD_TYPE=Release
cmake --build build-receiver --parallel 4
```

输出 `getdaq-receiver`、`getdaq-inspect`、`getdaq-merge` 和 `getdaq-matrix`。一条来源一个接收进程；固定容量接收池、完整 MFM 分帧、分来源写盘，可选导出客户端慢时断开。默认TCP，独立模式另可选ZBUF原始TCP或FDT帧间心跳过滤，见[输入协议手册](docs/RECEIVER_TRANSPORTS.md)。归档使用本机目录锁与正常退出后的封存凭据，不能仅凭等待一段时间移动仍在写入的文件。启动、停止见[数据服务手册](docs/DATA_SERVICE.md)。`getdaq-inspect --hit-pattern FILE COBO ASAD AGET`可离线流式累计CoBo type-1帧头中0..67通道hit位；`--hit-pattern-scan`可按显式阈值码映射多份raw并生成逐通道汇总CSV，映射由操作员确认，工具不进入接收链，也不判定阈值物理响应，详见[数据服务手册](docs/DATA_SERVICE.md#9-离线检查与异常恢复)。离线按事件合并见[MFM合并手册](docs/DATA_MERGE.md)；制作完整波形矩阵见[矩阵转换手册](docs/DATA_MATRIX.md)，查看单通道波形见[离线波形查看器](docs/WAVEFORM_VIEWER.md)。

下游分析端断连后会在空闲时回收连接名额；客户端应保持单向读取连接、不半关闭写方向，重连不重放旧帧。实现规则和复查见[导出重连手册](docs/EXPORTER_RECONNECTION.md)。

接收文件默认native名，也可选择GET时间戳/分源四位编号样式，命名选择进入固定接收计划绑定，见[文件命名与归档](docs/RECEIVER_FILE_NAMING.md)。两种模式均保留独占创建和严格封存。

已封存归档可由独立无Qt的`getdaq-catalog`显式复制到另一台归档主机；推荐TCP/IP命令行，使用普通TCP/IP，不加认证或加密；请限制在防火墙保护的可信网络中，且不接入实时DAQ接收路径。接收端准备、错误处理、续传和验证步骤见[TCP归档手册](docs/CATALOG_TRANSFER.md#13-普通-tcpip-发送已完成归档)。当前只有本机回环证据，双机部署仍需现场验收。

真实私有tmpfs空间/inode ENOSPC，以及写入/同步/封存错误处理已有本机验证，操作和验证边界见[写盘可靠性手册](docs/RECEIVER_IO_RELIABILITY.md)。测试数据小于20GiB且结束清理。

数据服务新增`getdaq-supervisor-query receivers`，即时区分自有receiver的运行、正常/异常退出和查询忙碌；它不改变生命周期或代替EOF/seal核对，操作及GUI缓存边界见[进程观察手册](docs/RECEIVER_PROCESS_OBSERVATION.md)。

操作台可开启`native_receiver_status`，通过ECC的只读后台查询显示异常退出、忙碌、失联或信息过期；不增加原始数据流和自动硬件动作，配置及故障处理见[GUI接收进程观察](docs/RECEIVER_GUI_OBSERVATION.md)。

升级 ICE 源可另开启`native_source_status`，独立查看源生命周期、UNKNOWN、查询失败和过期；按原实例及接收会话核对，观察不会自动发送采集命令。启用步骤、状态解释及故障处理见[源端状态观察](docs/SOURCE_GUI_OBSERVATION.md)。

ZBUF原始TCP字节流已接固定接收计划与ECC控制，发送/接收协议必须一致且实际状态核对，说明见[受管理ZBUF](docs/MANAGED_ZBUF.md)。数据主机FDT已支持同一接收会话多次连接/EOF和私有Finish，见[接收周期](docs/FDT_RECEIVER_CYCLES.md)；完整ECC的FDT启停/重连、未使用来源结束及同Run接法已通过本机联调，见[受管理FDT](docs/MANAGED_FDT.md)。真实板端零复制/吞吐尚待验。

需要旧ICE数据接口时，可单独启用`getdaq-ice-receiver`：接收原DataProcessor单向字节、原生分帧/写盘/可选在线导出，并通过同数据连接的显式结束及总字节核对发布独立凭据，见[ICE数据入口](docs/ICE_DATA_INGRESS.md)。旧源没有结束确认时保留数据且不冒充完整Run；源侧另有无Qt的IceSourceClient库和getdaq-ice-send文件/管道工具，固定连接/不自动重连和UNKNOWN行为见[ICE源手册](docs/ICE_DATA_SOURCE.md)。该入口已接固定ECC/源会话和管理归档；真实板端升级仍待完成，普通TCP/FDT程序仍不链接Ice。

从 Linux 项目根目录执行 `bash tools/build-linux.sh all` 现在构建完整主机套件（含主机端 Ice/ECC 和配置仓库），输出到 `build-delivery/full/`。`control` 构建六个操作工具；`data` 构建无 Qt/Ice 原生数据工具；`supervisor` 构建带 Qt 低频管理服务的数据主机工具。`ecc` / `experiment` 保留独立无 Qt 核心构建，`ice` 保留原兼容工程入口。原生接收器当前不支持 Windows 构建。

## Windows / Qt Creator

2026-10-20当前源码的[Windows Qt功能回归](docs/WINDOWS_FUNCTIONAL_VALIDATION.md)已在用户Qt5.14.2 MinGW环境复跑：Widgets101通过、1项Unix锁测试跳过，ECC客户端63通过，均无失败；独立机器、现场硬件和DAQ吞吐仍待验收。

实际 Windows Qt 5.14.2 MinGW 七程序已经从当前源码构建，并生成含 DLL/插件的运行目录；Widgets QtTest 105项通过、1项Unix专属锁测试跳过，ECC客户端63/63通过。当前运行包的位置、复制到 Windows 后的检查、WSL 构建入口及验证边界见 [Windows 详细手册](docs/WINDOWS_QT5142.md)。完整人工功能和现场验收仍未完成。

1. 将完整目录复制到 Windows，包含 `third_party/`。
2. Qt Creator → 打开项目 → 选择根目录 **`GetDAQ.pro`**，不是 `MUSIC/MUSIC.pro`。
3. 选择 **Desktop Qt 5.14.2 MinGW** Kit，使用独立构建目录，选择 Release，构建并运行。
4. 默认启用 Simulation，不需要硬件即可检查窗口和启停流程。

仅需 Qt 的 Core、Gui、Widgets、Network、Concurrent。yaml-cpp 源码已随工程提供，无需另外安装 YAML 库。不要混用 MSVC 和 MinGW 编译的 Qt。

新增`configcheck`子工程生成`getdaq-config-check`，仅用标准C++11，检查有效快照、发布包和完整实验工件，无Qt/Ice/yaml/curl/Boost依赖。也可独立从`common/experiment`的CMake工程构建；操作、摘要兼容和实际加载边界见[有效快照手册](docs/EFFECTIVE_CONFIGURATION_SNAPSHOTS.md)。

`getdaq-config plan-peaking-scan SYSTEM.json COBO_ID NEW_DIRECTORY`可离线生成旧GetController PeakingTime流程的16个完整AGET/ASTRE配置点，只修改显式选择的单块CoBo。它不连接硬件、不启动DAQ，也不会替代逐点完整Configure/Run；操作边界见[扫描计划手册](docs/GETCONTROLLER_SCAN_PLAN.md)。

操作台顶部菜单 **Offline scan plan builder...** 提供同一功能的图形入口，并调用同目录 `getdaq-config` 异步生成 PeakingTime、Pulser 幅值、单颗 AGET 阈值或 AutoGain 配置计划。AutoGain 需要指定芯片、活动通道范围和整数 mV 幅值范围；它只生成完整配置点，不执行旧事件采集、增益判定或自动多点运行。此窗口只处理配置，不进入 DAQ 数据通路；使用步骤和边界见[图形界面操作手册](docs/SCAN_PLAN_GUI.md)、[AGET阈值计划](docs/THRESHOLD_SCAN_PLAN.md)及[AutoGain计划](docs/AUTOGAIN_SCAN_PLAN.md)。

命令行方式（在该 Kit 的 Qt 命令提示符内）：

```bat
mkdir build-windows
cd build-windows
qmake ..\GetDAQ.pro
mingw32-make -j4
bin\GetDAQ.exe
```

不同 Qt Creator Kit 可能将可执行文件放在构建目录的 `bin` 或其配置子目录；可以在 Creator 的运行配置中查看路径。转移可执行文件到未安装 Qt 的 Windows 时，在同一个 Kit 的终端运行 `windeployqt --compiler-runtime <GetDAQ.exe路径>`，保留生成的 DLL 和 platforms 插件目录。

## Ubuntu

```sh
sudo apt install build-essential qtbase5-dev qt5-qmake
mkdir -p build
cd build
qmake ../GetDAQ.pro
make -j4
./bin/GetDAQ
```

可用 `--config /path/to/experiment.yml` 加载配置。macOS 安装 Qt 5 Kit 后同样通过 Qt Creator 打开 `.pro`，或使用 Kit 内的 qmake；当前没有提供预编译 macOS 包。

## 先用模拟模式检查

1. 启动程序，或打开 `config/simulation.yml`。
2. 点击 **Connect**，等待节点显示 Idle、sentry 显示 Online。
3. 依次点击三次 **Progress all**，等待每一步完成：Described → Prepared → Ready。这里推进控制状态，不会开始采集。
4. 点击 **Run start**。CoBo 全部确认 Running 后才启动 MuTaNT。
5. 观察状态表、MB/s 曲线和日志，然后点击 **Run stop**。
6. 控制器先停止 MuTaNT，再停止 CoBo，随后模拟归档、保存运行记录；成功后 Run 编号加一。

模拟与硬件的运行记录分别位于输出目录的 `simulation/` 和 `hardware/` 下。模拟模式不会创建网络客户端，也不访问真实 GET 配置。已存在的 Run 目录不会被覆盖。

界面文案使用英文，以便匹配 ECC 状态名称和跨平台字体；源码注释及此说明使用中文。窗口布局可在 Qt Designer 中打开 `apps/envoy/mainwindow.ui` 调整。速率曲线采用独立 `WidgetPlot2D` 类，基于 QPainter 绘制，不需要安装图表插件。

## 配置及真实服务

旧 envoy YAML 的 `experiment`、`run_number`、`description`、`fields` 保持兼容。仅导入旧配置时默认仍为模拟模式，避免加载配置就误连接硬件。

**Nodes / Paths** 编辑每个节点的 ID、角色、独立 ECC/sentry/数据主机、端口、配置 ID 和本地目录。至多一个 MuTaNT，至少一个 CoBo，允许单 CoBo；不固定 11 台。默认端口是 ECC 8083、sentry 8080、router 46005、exporter 46007。数据目标端口可设为 0 禁用。

- 实验名称应对应后端 GET 配置 ID，使用字母、数字、下划线或连字符。
- `config_dir` 是控制台能读取的 GET 配置目录，应包含各节点 describe/prepare/configure ID 对应的入口及全部 include 文件；支持 `.xcfg` 优先、`.xml` 回退。默认 CoBo 入口通常是 `describe-cobo<编号>.xcfg`，自定义 ID 以节点设置为准。可以是本地目录或已挂载的共享目录，程序不会自动通过 SSH 获取它。
- `output_dir` 保存本地运行记录与配置备份。sentry 中的数据目录位于远端 DAQ 机器，二者不是同一个目录。
- YAML 中的相对路径以 YAML 所在目录为基准；保存后写出已解析路径。迁移到 Windows 时在 Nodes / Paths 中重新选择适合 Windows 的目录。
- 旧模式取消Simulation后使用ECC SOAP/XML和sentry HTTP；native_ecc模式只向一个整系统HTTP服务提交命令，独立数据服务管理receiver。GUI没有SSH或原始数据接收功能；新模式的Stop/Breakup/封存与记录边界见[操作手册](docs/ECC_GUI_CLIENT.md)。
- Connect 后实验、节点和路径配置保持锁定；Run 编号、说明和自定义参数可以在正式 **Run start** 前继续修改，开始采集后锁定。正式采集的开始/停止分别使用 **Run start / Run stop**，Run start 位于左侧。
- 上方 **Progress all / Regress all** 和下方 **Progress selected node / Regress selected node** 只推进/回退控制状态，不会启动/停止正式采集；两组的按钮顺序均为 Regress 在左、Progress 在右。Native 单 CoBo维护入口仍是独立的 **Maintain selected CoBo…** 操作。
- 底部左侧参数表约占三分之一宽度，右侧只读操作日志约占三分之二。顶部 **Save YAML** 保存配置文件；它不会导出原始采集数据。
- **Stop run** 也可以停止连接时已经在运行的设备；没有本地 Run 上下文时，仅停止，不擅自归档数据。

GET 准备、配置和启停顺序沿用上游。SOAP 操作应答和随后实际状态查询分别处理；不会把“请求发出”当成“转换完成”。请求有独立超时，阶段有总体超时。

## 运行文件与恢复

每次运行写入 `<输出目录>/<simulation或hardware>/<实验>/run_NNNN/`：

| 文件 | 内容 |
| --- | --- |
| `run.yml` | 启动时的配置快照，后续运行不覆盖 |
| `journal.json` | 阶段、错误、每个节点的归档结果和时长 |
| `run.csv` | 本次 Run、说明、时长和自定义参数，正确转义逗号/引号/换行 |
| `configuration/` | 自定义配置 ID、递归 include 的原文件及 SHA-256 清单 |
| `backup.complete` | 所有配置文件成功备份后生成的完成标记 |

配置依赖快照在收尾时读取本机镜像，不代表远端硬件读回；说明见 [配置快照](docs/CONFIGURATION_SNAPSHOTS.md)。可选的本地 Ice 构建和只读连接工具见 [Ice 兼容组件](docs/ICE_COMPATIBILITY.md)。

CSV 按 Run 独立保存，防止新增字段后改坏已有实验表头；这是相较旧版单个实验 CSV 的有意调整。成功后原配置文件的 Run 编号自动保存；从 `run.yml` 快照打开时不会覆盖历史快照。程序日志优先保存在 Qt 用户数据目录下的 `logs/`；目录不可写时依次回退到缓存目录和按进程隔离的系统临时目录，启动日志会显示实际路径。每次点击 Connect 都会新建独立的 `logs/connections/connection-*.log`，记录本次连接请求、操作和断开；主程序日志仍保留完整进程记录。

若停止成功而本地备份失败，修复目录/权限后点击 **Retry finalization**：已确认归档的节点不再重复发送 Catalog，继续本地备份和记录。

Run setup 的 **Automatic Run** 支持时间、raw 大小单条件或任一达到即触发，可自动停止或成功收尾后至少间隔3秒续开 Run；不使用文件数量。运行期间阈值和 Apply 锁定，Enable 可即时关闭/开启已保存策略，关闭不会停止当前采集。Event log 警告橙色、错误红色，原文/导出不变。规则、运行中开关和 Native 服务要求见 [Automatic Run](docs/AUTOMATIC_RUN.md)。

若程序中断，重新打开对应运行的 `run.yml`，Connect 后点击 **Recover journal**。先检查实际设备状态，仍在采集则 Stop；全部 Ready 后可 Retry finalization。程序不会自动重发超时的 Start 或 Catalog。中断后时长可能不完整，会在操作日志中说明。

若归档记录为 `sent` 或 `unknown`，服务器可能已经执行了请求。应先在 DAQ 端核对目标 Run 和文件；确认已完成后，退出控制台并备份 `journal.json`，将该节点的 `catalog` 条目标记为 `complete`，重新恢复记录。只有确认从未执行时才移除该节点条目允许重新提交。不要仅为消除错误而修改记录。

关闭控制台不会自动停止远端采集。存在活动运行时会提示，并保留中断记录。多台控制台同时控制同一设备尚无服务端互斥机制，应由实验操作流程保证单一控制端。

## 测试

```sh
mkdir -p build-tests
cd build-tests
qmake ../tests/tests.pro
make -j4
QT_QPA_PLATFORM=offscreen ./getdaq-tests
```

Windows 可用 Qt Creator 打开 `tests/tests.pro`，选择同一个 Qt 5.14.2 MinGW Kit 构建运行。测试启动随机端口的 **127.0.0.1 模拟 HTTP 服务**，不会连接 DAQ 硬件；测试环境需要允许本机监听 socket。

覆盖旧 YAML、多行与 Unicode 参数、SOAP/JSON 错误解析、重复 Run 防护、连续模拟运行、真实 HTTP 客户端的命令顺序、启动失败阻断 MuTaNT、状态确认超时、归档超时不重复发送，以及 GUI 启停和关闭线程。

## 当前验证边界

当前已有 Linux/Qt 5.12.8、Ubuntu 24.04 用户空间（运行于 WSL2 内核）以及用户 Windows 主机 Qt 5.14.2/MinGW 的当前源码构建和自动化回归记录，详见 `docs/VALIDATION.md`。这些结果不能代替独立 Ubuntu/Windows 部署、macOS 原生运行、真实 GET 硬件和物理双机验收；Windows Qt5.14.2/MinGW的当前八程序候选和QtTest已于71.184更新，查看器3D旋转通过Windows原生进程检查；独立干净机器安装、完整远程部署与人工交接流程仍未验收。不能用模拟或同机测试替代这些验收。

传统 SOAP 控制模式仍沿用旧接收流程，停止后等待和文件检查不能独立证明完整落盘。原生 ECC 模式已把 receiver 生命周期接入 Prepare/Configure/Breakup：控制服务按固定计划启动独立接收进程，核对就绪、sender 断开、自然退出、文件同步和封存凭据；Qt 只发送低频控制命令并显示状态，不启动 receiver 子进程，也不收取、排队或转发原始帧。原始数据仍由独立 C++11 接收器直接写盘。该流程还不能证明板端没有漏事件，真实板端端到端及持续吞吐需实机验收。操作顺序见[远程生命周期手册](docs/REMOTE_ECC_DATA_LIFECYCLE.md)和[接收进程管理手册](docs/RECEIVER_PROCESS_MANAGEMENT.md)。

## 代码入口

以下控制台文件位于 `apps/envoy/`；节点服务位于 `apps/sentry/`，原生数据核心位于 `apps/datareceiver/`。

- `mainwindow.*`：界面、信号连接与线程生命周期。
- `runcontrol.*`：运行阶段、状态确认、超时和恢复。
- `eccclient.*`、`sentryclient.*`、`httpclient.*`、`protocol.*`：网络及协议。
- `runconfig.*`：YAML 和数据类型。
- `runrecorder.*`：持久化记录、备份和 CSV。
- `ipwindow.*`：节点和目录编辑。
- `widgetplot2d.*`：速率曲线。

上游与第三方来源见 `third_party/README.md`；保留 attpc_envoy 和 yaml-cpp 的 MIT 声明。

可选 ICE 数据源、独立接收器及数据主机混合管理/归档的完整说明：[源库与工具](docs/ICE_DATA_SOURCE.md)、[接收原理](docs/ICE_DATA_INGRESS.md)、[配置和管理步骤](docs/MANAGED_ICE.md)。完整 ECC ICE 源端接法已有本机联调，旧板端升级仍需继续。

数据源进程的生产线程/暂停/尾缓冲/最终结束，以及无需Qt的ICE数据组件构建，见[源端生命周期手册](docs/CONTROLLED_ICE_SOURCE.md)。

升级源程序的低频控制接口、源实例/接收会话绑定和完整ECC接法见[ICE源控制手册](docs/ICE_SOURCE_CONTROL.md)。只有实际实现扩展的源接受ICE模式，旧板端接口不会自动获得新结束能力。

源端环形缓冲的借用分块、读指针发布、刷新策略及故障不重放，见[读出手册](docs/SOURCE_RING_READOUT.md)。该核心可独立构建，无Qt/Ice依赖；真实板端DMA/中断仍需接入。

源端可复用读出线程、设备停止/join/有界排尾，以及多缓冲完整帧切换接完整ECC的说明，见[源读出生命周期手册](docs/SOURCE_READOUT_LIFECYCLE.md)。真实DMA/MMIO、中断与设备所有权仍待现场接入。

源端SourceMfmRing现可从任意设备写前缀识别完整MFM边界，共用接收头规则，停止于半帧时拒绝End；接法和边界见[源端MFM手册](docs/SOURCE_MFM_BOUNDARIES.md)。

配置编辑新增本地引用、修改引用和一次导入，支持来源审阅与撤销；不会运行硬件命令或传输原始事件。操作见[配置文件引用与导入](docs/CONFIGURATION_INCLUDE_IMPORT.md)。

旧版本1 `.bcfg` 配置现可在转换窗口或 `getdaq-config import-binary/export-binary` 中离线转换，先预览再保存新文件；原配置和硬件保持原状。步骤、格式原理及32位兼容限制见[二进制配置手册](docs/CONFIGURATION_BINARY_FORMAT.md)。

原 Describe 在同一服务实例中失败时，现可通过 **Verify original failed Describe...** 明确只读核对全部描述及停机位，记录凭据后恢复到 Described，再刷新状态并执行Prepare；它不重发CREATE，不接管冷服务或恢复Run。步骤和失败处理见[Describe核对恢复](docs/DESCRIPTION_RECONCILIATION.md)。

原 Prepare 失败后，可先明确停生产，再通过 **Power off original failed preparation...** 对同一原准备对象监测实际供电、断电并关闭确认属于本对象的告警订阅。完成仍保留 UNKNOWN，后续正向动作继续禁止；操作顺序、丢应答处理和验证范围见[准备故障收尾手册](docs/FAILED_PREPARATION_UNPREPARE.md)。操作台只传低频控制信息，原始数据仍由独立原生进程接收。

原 Prepare 的停止、断电与自有订阅收尾均确认后，可使用 **Verify original failed Prepare after cleanup...** 明确只读核对，保存新证明后恢复到 Described，再刷新状态并提交新的 Prepare。原失败记录保留，不自动开始采集，不用于 Run 或冷服务恢复；步骤和限制见[准备核对恢复](docs/PREPARATION_RECONCILIATION.md)。

运行故障的原停生产确认后，**Close original failed acquisition connections...** 可接续同一已知发送器的停止/断开，并撤销就绪配置；UNKNOWN和未完成Run保持，接收器核对/清理另行处理。顺序、丢应答和数据封存区别见[故障连接收尾手册](docs/FAILED_ACQUISITION_BREAKUP.md)。

原失败接收会话 Abort 完成后，数据核心和服务支持明确 `retire-failed-session`：核对原进程回收、目录锁释放与审计，保留旧 raw 和未封存标记，为手动新数据周期准备入口。父 ECC 的 `retire-failed-receivers` 和操作台 **Retire original failed data sessions...** 已接入原停止/连接/Abort 证据；完成仍保留父 UNKNOWN 和中断 Run，完整 Run 正向恢复仍待。操作台步骤见[父 ECC 数据退休](docs/ECC_FAILED_SESSION_RETIREMENT.md)，数据侧原理见[失败数据会话退休](docs/FAILED_DATA_SESSION_RETIREMENT.md)。

数据退休后可使用 **Power off original failed acquisition...** 沿用原准备对象监测实际供电、断电并关闭确认拥有的订阅；真实审计链和新的只读观察绑定原停止/连接/退休证据，完成仍保留 UNKNOWN 和 Run 中断。流程、丢应答及合法日志追加的证明处理见[原采集故障断电收尾](docs/FAILED_ACQUISITION_UNPREPARE.md)。

冷数据交接的独立恢复服务、Windows操作窗口和异常处理见[逐步手册](docs/COLD_DATA_HANDOFF_WINDOW.md)。
