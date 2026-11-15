# 操作手册：从编译到一次运行

完整主机端构建与目录交付请先读 [统一构建与部署](HOST_SUITE_BUILD.md)。该入口区分操作电脑、原生数据工具、带管理服务的数据主机和完整主机套件；原有各子工程的独立命令仍可使用。

本手册面向第一次接触这套工程的使用者。先完成模拟操作，再联调独立服务，最后接硬件。程序已接通新原生ECC、配置/采集/正常收尾、独立数据服务和操作台；仍有单板维护整合、部分/冷实例恢复及目标平台/真机验收缺口。请结合 [验证记录](VALIDATION.md) 区分“实现了”和“已经在你的硬件上验证了”。

配置发布包上传与版本检查使用独立仓库窗口；它不启动采集或加载板卡。第一次建立服务器、Windows界面操作、失联恢复和完整原理见[配置仓库手册](CONFIGURATION_REPOSITORY.md)。

## 1. 先分清操作、控制和接收程序

| 程序 | 在哪里运行 | 做什么 | 是否接收原始数据 |
| --- | --- | --- | --- |
| `GetDAQ` | 操作电脑，Windows/Linux/macOS 目标 | 配置、启停、状态、Run 记录 | 否 |
| `getdaq-ecc-server` | ECC控制主机 | 新原生硬件协调、低频HTTP及数据节点管理 | 否 |
| `getdaq-supervisor` | 每台数据主机 | 固定计划、receiver进程与封存证明管理 | 否 |
| `getdaq-sentry` | 数据磁盘所在机器 | 进程/磁盘/文件统计、归档 | 否，只处理元数据和重命名 |
| `getdaq-receiver` | Linux 数据接收机器；macOS 待验 | MFM TCP 接收、写盘、可选导出 | 是，原生 C++11，无 Qt |
| `getdaq-inspect` | 保存有数据文件的机器 | 离线检查帧边界/来源，或汇总指定芯片的type-1帧头hit位 | 只读已有文件，不接入实时采集；hit位不等于阈值物理判定 |
| `getdaq-merge` | 保存有已封存数据的机器 | 将 CoBo/RIBF MFM 按事件号合并 | 只读输入文件，生成独立 `.gmer` 输出 |
| `getdaq-matrix` | 保存有已封存数据的机器 | 将完整CoBo type 1波形帧转换为CoBoFrameViewer `.gmat`矩阵 | 只读输入文件；每帧1,114,112字节，默认输出上限20GiB；不覆盖已有输出 |
| `getdaq-fits` | 安装CFITSIO的离线分析机 | 将支持的CoBo `.graw` 导出为逐事件/通道FITS及GROUPING索引 | 可选离线工具；默认不构建，不进入接收路径；输出上限20GiB |
| `getdaq-root` | 安装ROOT 6的离线分析机 | 将支持的CoBo `.graw` 转为旧 `tree/GDataFrame` ROOT对象树；兼容模式可输出 `TT2K/T2KEvent` | 可选离线工具；默认不构建，不进入接收路径；输出上限20GiB |
| `getdaq-root-viewer` | 安装ROOT 6与Qt 5的离线分析机 | 打开 `tree/GDataFrame` ROOT事件树，按entry查看四个AGET的通道波形或桶×通道热图 | 可选离线GUI；只读本地文件，不接收DAQ数据；不进入常规控制台依赖 |
| `getdaq-waveform-viewer` | 离线分析工作站 | 从`.graw`或`.gmat`选择帧/AGET/通道并显示波形、计算旧版FWHM和peaking time | Qt Widgets离线GUI；raw索引后台读取帧头，选中后只读一个CoBo帧；不接收DAQ数据 |
| `getdaq-simulator` | 开发电脑 | 模拟 ECC、运行 sentry、生成测试文件 | 不是硬件接收器 |

旧SOAP模式连接原 `getEccSoapServer`，native模式连接新 `getdaq-ecc-server`；二者是可选择的控制入口，不串联运行。板端 `getHwServer` 保持现有部署。旧 `getEccServer` 是Ice接口，不能把它的端口填成新HTTP服务地址。实际控制先读[原生ECC配置与操作](ECC_GUI_CLIENT.md)。

点击 **Nodes / Paths** 时，传统逐节点模式会提示核对预填的 CoBo/MuTaNT 地址与端口。默认地址只是配置初值，没有现场硬件验证；关闭模拟或使用旧 SOAP 服务前，必须按实验室实际网络、节点角色和数据接收主机检查并修改。选择 Native whole-system mode 后，窗口会说明控制台只连接一个 ECC 服务端点，板端代理和拓扑来自该服务的固定配置；传统逐节点主机字段会隐藏。

## 2. Windows：用已有 Qt 5.14.2 MinGW 检查窗口

1. 把工程目录完整复制到 Windows，包含 `apps`、`common`、`third_party/yaml-cpp`、`config`。不要只复制根目录 `.pro` 文件。
2. 打开 Qt Creator，选择“打开文件或项目”，打开根目录 `GetDAQ.pro`。
3. Kit 选择你安装的 **Desktop Qt 5.14.2 MinGW**。不要选 MSVC Kit，也不要沿用 MUSIC 的 `.pro.user`。
4. 选择独立构建目录，如 `D:/build/GetDAQ-qt5142-mingw-release`；不要把 Linux 构建目录一起复用。
5. 选择 Release，点击构建。根工程构建9个控制程序：操作台、sentry、模拟器、配置编译/检查/上传、supervisor查询、波形查看器和独立MCP控制进程。
6. 在“项目 → 运行”中选 `GetDAQ`，启动界面；默认模拟模式不需要硬件。

命令行等价操作，在该 MinGW Kit 的命令环境中执行：

```bat
mkdir build-windows
cd build-windows
qmake ..\GetDAQ.pro
mingw32-make -j4
bin\GetDAQ.exe
```

运行目标若在不同配置子目录，以 Qt Creator 显示的程序路径为准。打包给未安装 Qt 的机器时，在同一个 Kit 的终端运行 `windeployqt --compiler-runtime <GetDAQ.exe实际路径>`。保留生成的 DLL 与 `platforms` 子目录。

这里没有把原生 POSIX 数据接收器加入 Qt 根工程，因此 GUI 检查不要求 Windows 安装 CMake、Ice 或旧 GET 库。已使用用户的Qt5.14.2/MinGW真实Kit构建及初始启动；独立Windows机器和全部人工功能仍待验收。套件路径见[Windows手册](WINDOWS_QT5142.md)。

## 3. Linux：分别编译控制与数据组件

Ubuntu 24.04 是目标平台；现有自动测试机器为 Ubuntu 20.04.6，不能把两者混称。联网安装构建工具的常见命令为：

```sh
sudo apt install build-essential cmake qtbase5-dev qt5-qmake
```

离线机器先按照 [依赖与离线构建](DEPENDENCIES_AND_OFFLINE_BUILD.md) 准备对应平台工具链。项目构建本身不需要在线拉取 yaml-cpp 或 Ice。

在源码根目录：

```sh
bash tools/build-linux.sh all
./build-delivery/full/bin/GetDAQ
```

只构建窗口/代理和配置工具：`bash tools/build-linux.sh control`。只构建无 Qt 数据工具：`bash tools/build-linux.sh data`。需要 Qt 低频接收管理服务时用 `supervisor`。状态约束核心单独构建用 `ecc`，纯 C++11 参数计算核心用 `experiment`。`all` 现在调用统一入口构建完整主机套件，包含主机端 Ice/ECC、接收管理和配置仓库，输出在 `build-delivery/full/`；`ice` 保留原兼容工程构建入口。`GETDAQ_JOBS=2 bash tools/build-linux.sh all` 可减少并行编译占用。

macOS 的 GUI 使用 Qt 5 Kit 打开 `.pro`；数据程序使用 CMake。代码使用 POSIX socket/flock/fsync，但 macOS 目前未实际验收，不能直接保证所有文件系统上的行为相同。

## 4. 跑通内部模拟

1. 启动 `GetDAQ`，保持 Simulation 勾选，或加载 `config/simulation.yml`。
2. 点击 **Nodes / Paths**，确认本地 Run 记录目录可写。内部模拟不会读真实硬件配置，但仍需要正常目录设置。
3. 点击 **Connect**。节点应显示 Idle，监控应在线。
4. 点击 **Progress all**，等全部 Described；再点，等 Prepared；第三次到 Ready。不要连续抢点。
5. 填写实验说明和参数，点击 **Start run**。Run 目录在启动前占用；已存在时拒绝覆盖。
6. 观察 Running、速率曲线和日志。模拟曲线是人为生成的统计量，不是真实吞吐量。
7. 点击 **Stop run**。依次等待停止、Settling、Cataloging、备份/记录完成。
8. 成功后 Run 编号加一。再运行一次，检查新 Run 没有覆盖旧 Run。

没有 MuTaNT 的单 CoBo 配置也可以运行；至少一个 CoBo，最多一个 MuTaNT。内部模拟不创建网络客户端、不写真实 `.graw`。它适合先检查窗口和控制状态机。

## 5. 跑通独立 HTTP 联调

在一个终端启动：

```sh
./bin/getdaq-simulator --root /tmp/getdaq-demo --cobos 2
```

模拟器按整个`--root`统计带标记测试`.graw`，默认累计预算 **1 GiB**；达到上限后不再写文件，但不会代替操作员停止ECC模拟Run。可用`--max-generated-bytes`降低或提高总预算，合法范围为64 KiB至20 GiB。预算会先统计所有模拟CoBo `incoming`目录及归档子目录中的已有`.graw`，所以使用相同`--root`重启时旧测试文件也占预算。程序不会自动删除这些文件；请在检查完成后清理整个专用模拟目录，切勿把真实数据放进该目录。

程序输出生成的 `envoy.yml` 路径。打开另一终端：

```sh
./bin/GetDAQ --config /tmp/getdaq-demo/envoy.yml
```

该配置指向 127.0.0.1 上的真实 HTTP 服务，因此不使用内部 Simulation 分支。默认 ECC 端口从 18083 开始，sentry 从 19080 开始；不要将它与真实实验配置混用。

按上一节完成 Prepare/Configure/Start/Stop，检查模拟目录中的 `.graw` 被归档、本地备份及 journal 生成。这里的 `.graw` 包含模拟标记字节，**不是合法硬件 MFM**，不要用它测试原生接收器或波形分析程序。

改变节点数或端口后，使用新的 `--root`，或更新该目录中已有的 envoy.yml。模拟器不会为了方便覆盖用户已经编辑的配置。

并行联调时可使用 `--ecc-port 0 --sentry-port 0`，由各实际服务绑定系统分配的空闲端口，再将实际地址写入新生成的 envoy.yml。不要先探测端口再关闭探测 socket：这不能保证下一次绑定成功，也不能保证相邻端口空闲。使用这一选项再次启动时仍需新的 `--root` 或更新旧配置，因为分配的端口可能变化；已有配置不会自动覆盖。

故障演练可用 `--fail-start 0`，让 CoBo0 启动失败；应看到 MuTaNT 没有继续启动。`--delay-ms 500` 可观察“命令应答已返回但状态转换尚未完成”的过程。

## 6. 配置字段与三个不同的目录

### 6.1 三个目录

`config_dir`：控制台能访问的 GET 配置目录。用于快照/备份，不代表文件已经部署到 ECC。可为本地副本或手工挂载路径；没有自动 SSH 上传。

`output_dir`：控制台保存 Run 记录的目录。Windows 上可以是 `D:/GetDAQ/runs`。这里不是高速原始数据盘。

sentry 的 `data_dir` 与 receiver 的 `--output`：接收机器上的原始数据 incoming 目录，二者应一致。归档后位于该目录下面的 `<实验>/run_NNNN`。

### 6.2 节点表

| 字段 | 含义和示例 |
| --- | --- |
| ID | 硬件逻辑 ID；不是表格行号 |
| Role | `cobo` 或 `mutant` |
| ECC host/port | 实际 SOAP ECC 地址，例如控制服务器:8083 |
| Sentry host/port | 接收主机上的代理；主机留空沿用 ECC host |
| Router host/port | 硬件发送原始数据的目的地；端口 0 表示禁用该目标 |
| Exporter host/port | 旧第二条板端数据目标；不是新接收器向分析端的导出监听口 |
| Describe/Prepare/Configure ID | 后端实际加载的三个 SubConfigId；留空使用兼容默认值 |

当前每行对应一个独立 ECC endpoint，重复 endpoint 会被拒绝。历史系统“一套 ECC 管多块板”的完整拓扑模型仍待迁移，不要复制多行同一 ECC 来假装支持。

`soap_inline_xml` 在 YAML 中控制兼容方式，节点表保存时会保留它。默认 false：按本地 gSOAP 接口的字符串定义，对内层 XML 做 SOAP 字符串转义。某些旧 envoy 部署使用内嵌 XML；只有核对实机请求/回包后才设 true。

### 6.3 计时参数

`poll_ms` 决定状态查询频率；默认 2000 ms，通常不需为了“显示更快”改成几十毫秒。

`timeout_ms` 是一次普通 HTTP 请求等待；`step_timeout_ms` 是一次硬件状态转换总体等待；`catalog_timeout_ms` 是归档请求等待。旧 sentry 先等 30 秒，归档超时不能比这个还短。

控制台 `settle_ms` 是 ECC 停止后的等待；sentry JSON 中同名字段是代理收到归档请求后的等待。两者不是同一个计时器。对于原生接收器，等待只是操作延迟，不能替代目录锁和封存校验。

## 7. 第一次连接硬件前

若GetDAQ和服务运行于同一台Linux电脑，可从Run Setup的 **Set up board and receiver addresses…** 在界面生成并检查板卡/接收配置，自动填写操作台绑定并启动本机服务。操作与前提见 [实机配置窗口](RUN_SETUP_HARDWARE.md)。

本地 `.xcfg` 查看和编辑可从主窗口 **GET configuration files...** 进入；显示真实来源文件、声明值/单位/范围，支持 include 检查和默认值来源查找。连接期间只读，不自动上传或下发。详见 [配置页面说明](CONFIGURATION_EDITOR.md)。

先确认使用原生ECC模式还是旧SOAP兼容模式，再核对实际端口、所管理板卡、配置ID、板端getHwServer/Ice版本、数据链路类型和输出格式。原生模式连接固定HTTP ECC服务；旧SOAP模式连接历史ECC。源码里的默认IP/端口与PDF示例都不是现场事实。

先用旧系统保留一份已知正常的配置、启动参数、版本信息和原始帧样本。新程序初次连接只看 GetState，核对状态号与 Transition；然后在允许的实验窗口逐阶段验证。已有校准的系统不要为了测试按钮随意 Undo/Reset。

2018年单板对齐教程要求保留其他板的Prepared/校准边界。普通单节点Progress/Regress不能代替单板维护。新源码的原生模式已加入`Selected CoBo maintenance...`入口，操作顺序、当前验证状态和旧运行包限制见[单板维护说明](COBO_MAINTENANCE.md)。重准备完成不代表波形或物理对齐已经正常。

原生接收器第一次联调参照[数据服务手册](DATA_SERVICE.md)，先一个CoBo、一个专用incoming、一个端口；记录字节/帧一致性再扩展。原生模式中操作台通过ECC和独立数据服务管理receiver，事件直接进入原生receiver；旧SOAP兼容模式的监控界面不能视为这条原生生命周期入口。Ice源码已归档不等于板端服务已更换或现场协议已验收，板端getHwServer/IceEirfu继续保留现有部署。

## 8. 一次真实 Run 的顺序

1. 接收与监控服务准备就绪，数据盘容量足够，incoming 没有未处理旧数据。
2. Connect 后检查所有 ECC/代理在线，确认目标配置和实验/Run 编号。
3. Describe → Prepare → Configure。Prepare 先 MuTaNT 后 CoBo，Configure 顺序相反。
4. Start 时先记录 Run 快照，MuTaNT Breakup/Configure 重建启动边界，CoBo 全部确认 Running 后启动 MuTaNT；单 CoBo 模式省略 MuTaNT 步骤。
5. 运行期间观察每节点状态、盘空间和日志；状态离线不代表设备自动停止。
6. Stop 先 MuTaNT，确认后再 CoBo；所有目标 Ready 且 Transition=0 后才进入后续阶段。
7. 使用原生 receiver 时按其手册确认发送端 EOF、接收器正常结束和凭据。当前尚需外部操作管理该进程，与自动归档的时间配合必须在联调时验证；验证前不要将其当无人值守方案。
8. sentry 归档、控制台备份配置并写记录。全部成功才推进 Run 编号。

操作响应成功与状态转换成功是两次确认。程序不会把“发出 Start”直接显示成“所有设备已运行”。

## 9. 文件解释

控制台目录：`<output_dir>/<simulation或hardware>/<实验>/run_NNNN/`。

| 文件 | 看什么 |
| --- | --- |
| `run.yml` | 当时的节点/路径/参数快照；恢复时打开它 |
| `journal.json` | 正在或曾经执行的阶段、错误、每节点归档状态 |
| `run.csv` | 本次 Run 信息及参数；每 Run 独立保存 |
| `configuration/` | 按节点配置 ID 收集的入口、递归 include 与内容哈希清单 |
| `backup.complete` | 完整配置备份完成的标记，没有它不能认为备份完整 |

当前备份按各节点实际配置 ID 查找 `.xcfg` / `.xml`，递归收集 include，保留原字节及 SHA-256 清单。目录外引用也会保存；恢复外部路径时需根据清单处理。它捕获的是收尾时的本地镜像，不是采集开始时的远端硬件读回。旧 `backup.partial/` 历史运行保持不动。具体目录、重试、恢复和边界见 [配置依赖快照](CONFIGURATION_SNAPSHOTS.md)。

接收端归档目录另外保存 `.graw` 与 `catalog.json`；原生接收器目录还包含 `receiver-seal.json`。不要把控制台记录目录和接收端数据目录混为一处。

控制台日志和接收端数据分开保存。主窗口在每次连接后自动创建独立操作日志，优先放在用户数据目录下的`logs/connections`；无法写入时回退缓存目录或进程临时目录。日志面板会保留最近2000行供即时查看，点击 **Export visible log…** 可导出当前显示的文本；完整连接历史以自动生成的会话日志文件为准。

## 10. 失败后如何处理

| 现象 | 含义 | 下一步 |
| --- | --- | --- |
| ECC 请求超时 | 结果未知，不等于没执行 | 保留日志，继续查状态；不要连点 Start |
| State=6 | Paused | 作为活动采集处理，可以 Stop；不是“Busy” |
| Transition 非零 | 操作仍在转换 | 等待完成或总体超时，不进入下一阶段 |
| 归档 409 | 锁、已有 Run、凭据或文件清单冲突 | 阅读返回原因，核对接收器与目标目录 |
| 归档 500 | 文件/清单操作失败，可能部分移动 | 查 catalog.json 的 planned/moved |
| 备份失败但归档成功 | 原始文件可能已经归档 | 修复本地路径，Retry finalization；不重复 Catalog |
| 接收器 error 非空 | 数据完整性不确定 | 停源，保留现场，使用离线检查；不造成功凭据 |
| 在线导出断开 | 消费者慢/连接异常/预算超限 | 检查分析进程；确认主文件记录状态 |

控制台退出后，重新打开中断 Run 的 run.yml，Connect，再 **Recover journal**。核对真实硬件仍运行则先 Stop；全部 Ready 且状态新鲜后再 **Retry finalization**。

journal 的 catalog 值为 `sent` 或 `unknown` 时，服务器可能已完成移动，只是响应没回来。先查远端目录和 catalog.json；必要时退出控制台，备份 journal，再由操作员根据证据修正该节点状态。不能为了让界面变绿就删除未知条目。`complete` 节点不会被重发 Catalog。

关闭 GUI 不自动停止远端采集；仍有运行时会提示并保留记录。当前没有跨多个控制台的硬件互斥服务，同一套设备应由一个控制入口操作。

## 11. 提交检查结果时记录什么

Windows 编译问题：Qt 完整版本、Kit 名称、MinGW 版本、构建模式、第一条编译错误和其上下文。后面的连锁错误通常不如第一条有用。

运行问题：对应 run.yml、journal、应用日志、发生时的 State/Transition、ECC/代理回包、接收器退出码和状态。路径/主机名可按需要脱敏，但保持节点关系和端口类型可辨。

性能问题：同时记录发送端、接收端与磁盘指标，说明数据模式、帧大小、是否 fsync、是否开启导出、测试时长与总量。仅说“Qt 卡”或“TCP 慢”无法确定瓶颈。

## 独立硬件访问工具

`bash tools/build-linux.sh ice`现在同时构建`getdaq-hardware`。它是独立底层硬件工具；Qt操作台的native_ecc模式通过原生ECC服务调用系统硬件流程，旧SOAP/模拟模式仍保留。不要把底层CLI当作完整状态机。地址、字节/位宽区别、操作日志和失败后的处理见[硬件后端手册](HARDWARE_BACKEND.md)；两种操作台模式的配置见[操作台手册](ECC_GUI_CLIENT.md)。

## 硬件描述离线编译

根 Qt 工程现在还构建 `getdaq-config`；配置页面可导出同一 JSON 工件。先离线编译和审阅，再通过独立原生工具创建空节点描述；这不是完整实验 Prepare，不自动操作采集设备。见 [硬件描述全流程](HARDWARE_DESCRIPTIONS.md)。

## AGET 实验参数离线编译

`getdaq-config compile-aget INPUT.xcfg COBO_INSTANCE ASAD_ID AGET_ID CHIP_VERSION OUTPUT.json` 可检查单片 AGET/ASTRE 的参数、13 个配置寄存器及读出掩码。实例和芯片版本需明确指定，输出文件必须是新的。工件包含每个参数的继承来源和源文件摘要；当前不连接硬件，也没有 GUI 部署按钮。示例、FPN/位序和非法参数处理见 [AGET 配置手册](AGET_CONFIGURATION.md)。

整板使用 `getdaq-config compile-cobo-aget INPUT.xcfg COBO_INSTANCE VERSIONS OUTPUT.json`，例如版本列表 `0=0x202,1=0x202,2=0x202,3=0x202`。编号是 `4*AsAd+AGET`，列表必须恰好覆盖全部活动芯片；工具还检查全板共用的偏移、深度和 2p 模式。详细示例见 [整板操作](AGET_CONFIGURATION.md#12-整板-aget-编译和原生写入子序列)。

原生库的 `CoBoAgetConfigure` 已有型号核对、寄存器/掩码写入读回和错误后立即停止的实现。它目前只供后端程序调用，不能将离线 JSON 直接交给硬件 CLI；完整 Prepare/校准/所有权联锁及 ECC Configure 仍待接入，实际运行继续使用旧 ECC。

## CoBo 触发、零抑制及缓冲区编译

`getdaq-config compile-cobo-readout INPUT.xcfg COBO_INSTANCE VERSIONS OUTPUT.json` 在 AGET 部分上增加 Module、零抑制、板内循环缓冲区及跨芯片一致性检查。版本表和新文件要求相同。缓冲区地址属于板内 RAM；原生子序列会重置 readPtr，必须在采集/相关数据读取停止后由完整后端使用，不能运行中热更新。CLI 当前只离线编译，具体命令、单位和错误处理见 [CoBo 读出配置手册](COBO_READOUT_CONFIGURATION.md)。

## AsAd 校准及新状态核心

AsAd Configure 参数使用 `getdaq-config compile-cobo-configure INPUT.xcfg COBO_INSTANCE VERSIONS OUTPUT.json`，版本表和新文件要求不变。电压是相对 1102mV 参考值的偏移，WCK 延时不是 ms；周期模式的配置成功不表示定时器已经运行。原生组合库仍需完整 Prepare/校准/独占前提，GUI 没有直接部署入口。逐项解释与示例见 [AsAd 配置操作](ASAD_CONFIGURATION.md)。

AsAd 上电/监控参数使用 `getdaq-config compile-cobo-monitoring INPUT.xcfg COBO_INSTANCE MODE_MAPPING OUTPUT.json`。MODE_MAPPING 必须选择 declared（按参数含义修正旧顺序）或 legacy（保留实际旧调用），输出仍必须为新文件。AL 在上电前后的含义不同；VDD 稳定最低码向上取整，保护阈值向下取整。完整示例、字段/单位、掩码、轮询日志和失败处理见 [AsAd 上电监控手册](ASAD_MONITORING.md)。这个命令不连接设备，也不表示完整 Prepared。

独立工具新增 AsAd 慢控制校准，必须绑定匹配的硬件描述，并由操作员确认停止采集、完成上电与所需初始化。位图填写、命令、日志和出错后的处理见 [AsAd 校准操作](ASAD_CALIBRATION.md)。GUI 当前不触发这项维护操作。

新的 ECC 状态约束核心已经实现和测试，完整服务还在迁移。状态数字、允许的操作顺序和结果不确定时的处理见 [ECC 状态机说明](ECC_STATE_MACHINE.md)。它不证明接收器已经排空；实际采集继续按前面的旧 ECC 和独立数据接收流程操作。

`ecc` 构建还输出只读 `getdaq-ecc-journal`。操作 ID 查询、unknown 与启动待核对的区别、文件损坏/锁冲突处理和无硬件示例见 [ECC 日志手册](ECC_JOURNAL.md)。该工具不会发送控制命令，也不能替代真实硬件恢复检查。

## AGET 与自动读取延迟的离线检查

使用 `getdaq-config compile-cobo-calibration INPUT.xcfg COBO_INSTANCE VERSIONS DELAY_LAYOUT OUTPUT.json`，DELAY_LAYOUT明确填写global或per-asad。共同字段要求各活动板最终策略一致；逐板字段须真实匹配描述。calibrationAgetId的0表示自动找第一片活动芯片；计算自动延迟和选择自动延迟是独立设置。输出不操作设备、不能表示Prepared。新原生校准会重置ADC并覆盖reg1/reg2，必须位于最终Configure之前；操作示例、影响与断线处理见 [AGET 校准手册](AGET_CALIBRATION.md)。

## 独立告警观察

`getdaq-hardware watch-asad-alarms`可在明确的反向回调端点上观察已有CoBo告警服务；传入真实CtrlNode代理/名称、1..86400秒及三份不同的新日志。它不自动开启板端检查器，订阅应答和无事件退出不能证明硬件健康。告警保持时退出3，硬件仍须由运行控制停止，结果未知不可自动重发。完整命令模板、监听与发布地址、LED策略、JSON字段及故障处理见 [LED与主机告警操作](COBO_SERVICES.md)。

使用getdaq-config compile-mutant INPUT.xcfg MUTANT_INSTANCE MODULE_COUNT GMC_MASK WSCA_MASK REGISTER_LAYOUT CLOCK_POLICY OUTPUT.json。实例必须明确声明；布局modern或legacy-names，时钟策略strict或legacy-vhdci，掩码必须来自审阅的系统快照。输出保留请求和量化时间，不部署硬件或证明TDC相位。原生MuTaNT子流程仍需完整系统前提；命令示例、参数、计数脉冲、布局及UNKNOWN处理见 [MuTaNT详细手册](MUTANT_CONFIGURATION.md)。

## 独立系统对齐工具

`getdaq-align validate-topology FILE`只检查本地拓扑/硬件描述；不会连接硬件。`align-system FILE NEW_AUDIT_DIRECTORY --exclusive-stopped`在调用者已独占控制、板端/发送器/接收器停止且MuTaNT先准备好时执行槽位定位和机箱内/间协调。声明不自动获得锁或发送Stop。先核对全部节点再发对齐命令，日志目录不可复用，未知结果不重发/不自动恢复Normal。它不链接Qt、不接收事件，也不提交整个系统Prepared或发送SyncPulse。完整示例、每个字段、调用顺序和故障处理见 [系统对齐详细手册](SYSTEM_ALIGNMENT.md)。
## 原生全系统硬件描述

getdaq-system-describe现提供validate-topology离线检查、check-system只读在线核对和describe-system实际动作；三种明确策略对应核对现有、创建空节点、替换已知描述。它独立于GUI目前使用的旧ECC SOAP。详细拓扑、逐条命令、原理、日志与错误处理见[全系统描述手册](SYSTEM_DESCRIPTION.md)。发生UNKNOWN后，新CLI进程/日志目录不构成恢复，仍须完成实际核对。

原ECC退出后的数据交接补齐，使用[独立恢复服务与窗口](COLD_DATA_HANDOFF_WINDOW.md)。该窗口不恢复旧硬件控制，不恢复中断Run。
