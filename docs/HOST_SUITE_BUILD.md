# 主机端统一构建与部署：从源码到完整目录

本次重构的是电脑端软件。板端 `getHwServer` 和嵌入式通信库 `IceEirfu` 保留现场部署。主机端仍须与实际板端联调；下面的构建成功只能说明程序已编译和整理，不能说明硬件、固件或高速采集已经验收。

**最近一次 Windows control 构建（71.773，2026-11-10）：** Qt 5.14.2/MinGW 7.3 x64 从当前源码构建9个 x86_64 PE 程序，1,054项输入记录和离线依赖核验通过，编译日志无 warning/error 行；未运行测试、启动或硬件流程。回执：`build-windows-refactor-772/windows-build-receipt.json`；[验证记录71.773](VALIDATION.md#71773-current-source-windows-qt-5142-control-suite-build-2026-11-10)。

**最新 Linux full 源码构建（71.772，2026-11-10）：** Linux/WSL2、CMake 3.16.3、Qt 5.12.8/GNU Make 4.2.1，从当前源码构建30个主机程序；1,053项构建输入记录绑定回执，109项嵌入依赖离线校验通过。未运行测试、启动或硬件流程，目标平台及硬件仍未验收。回执：`/tmp/getdaq-full-refactor-772/full/build-receipt.json`；[验证记录71.772](VALIDATION.md#71772-current-source-linux-full-host-suite-build-after-waveform-viewer-cli-split-2026-11-10)。

**最新 Windows control 源码构建（71.748，2026-11-04）：** 完成Sentry封存校验边界拆分后，使用实际 Qt 5.14.2 / MinGW 7.3 x64 Kit 重建并校验 9 个控制程序均为 x86_64 PE；新构建日志没有 warning/error 输出。收据为`build-windows-refactor-745/windows-build-receipt.json`。此次未运行 QtTest、独立部署或人工 GUI 操作；GitHub Actions、macOS、Ubuntu 24 独立主机、真实双机和现场 DAQ 吞吐仍需分别验收。详见[验证记录71.748](VALIDATION.md#71748-sentry-receiver-seal-validation-boundary-2026-11-04)。

**历史Linux full启动smoke（71.319，2026-10-23）：** 在Ubuntu20.04.6/WSL2、Qt5.12.8下从当时源码构建28/28程序；549项源码摘要绑定构建收据，程序SHA/`ldd`、109项嵌入依赖、临时188文件运行目录manifest与28程序启动smoke通过，GUI仅离屏打开初始窗口并截图；未访问硬件、未留下raw。该构建早于71.334新增MCP，不能代表当前30程序清单。构建关闭CTest，不能与单元/集成测试通过混称。见[验证记录71.319](VALIDATION.md#71319-linux-full-当前源码重建与程序启动smoke2026-10-23)。

**历史候选快照（71.276/71.223/71.274，2026-10-22）：** Linux host候选71.276包含28/28程序、严格C++11核心151/151、Widgets122/122、CTest25/25、固定ECC模拟集成、187项清单及程序启动smoke；其当时报告的ECC后续实现状态已由71.30x扫描工作推进，不再作为当前功能描述。Ubuntu24用户空间候选71.223包含Core151/151、Widgets116/116、CTest23/23及传输/交接验证，但未纳入后续查看器/扫描身份变化。Windows控制候选71.274按当时源码构建8/8程序，Widgets121通过/0失败/1项POSIX跳过，ECC客户端64/64，287项manifest、SDK-free启动和原生窗口smoke通过；它不包含之后的变化。以上是各自历史快照结果，不可当作当前源码目标机验收。**这些旧候选包目录已于2026-10-23清理以回收磁盘空间，历史验证摘要保留在本文件，不再提供候选二进制或逐文件receipt。** Linux/Ubuntu24用户空间运行于WSL2共享内核；独立Ubuntu24、Windows当前源码、macOS、双机网络、现场服务/板卡及持续DAQ吞吐仍需分别验收。

## 1. 五种构建分别适合什么机器

| profile | 适用机器 | 编译的程序 | Qt / Ice |
| --- | --- | --- | --- |
| `control` | Ubuntu/Windows 操作电脑 | GetDAQ、sentry、模拟器、配置编译/检查/上传工具、supervisor-query、波形查看器及 ECC 控制 MCP，共九个 | GUI用Qt；supervisor-query是原生客户端；MCP仅用Qt Core/Network访问ECC控制HTTP，不触碰raw；不构建主机端 Ice 后端 |
| `data` | 只需要原生数据工具的 Linux/macOS 主机 | receiver、inspect、merge、matrix、supervisor-query、catalog，共六个 | 不需要 Qt、Ice；query 是客户端，不是常驻管理服务 |
| `data-host` | 需要远程管理接收进程的数据主机 | 上述六个，加 supervisor 服务 | 管理服务需要 Qt Core/Network/Concurrent；原始数据仍进入无 Qt 的 receiver |
| `analysis` | 离线 ROOT/FITS 转换和查看 | receiver、inspect、merge、matrix、getdaq-fits、getdaq-root、getdaq-root-viewer | 显式需要 CFITSIO 与 ROOT 6；只有 ROOT 事件查看器使用 Qt Widgets，receiver和两个转换器不依赖Qt；不包含 ECC/Ice 服务 |
| `full` | 主机端开发与完整部署 | 九个操作工具、原生 receiver/inspect/merge/matrix、硬件/ECC/对齐/参数工具、ECC接班CLI及独立冷交接恢复服务、supervisor/query/catalog、配置仓库服务/CLI、可选 ICE 接收/发送和ECC journal工具，共 30 个 | Qt 用于低频控制；兼容 Ice 从本地源码静态构建；默认 TCP receiver/inspect/merge/matrix 无 Qt/Ice |

`apps/datareceiver`使用CMake标准`BUILD_TESTING`开关：开启时注册`mfm-merge`、`gmat-conversion`、`receiver-core`及Linux上的`exporter-lock`测试，并提供内存基准工具；发布构建显式关闭该开关，只生成接收器、检查器、合并器和矩阵转换器，不会暴露测试/基准目标。开发验证配置应显式设为`-DBUILD_TESTING=ON`。

选择 `data` 不代表已经启动接收，更不代表配置好了硬件。配置 JSON、认证文件、端口、接收计划和 Run 目录需要按对应手册准备。`full` 中的 ICE 接收工具用于需要该入口的部署，不会自动改变板端发送协议。

Windows 当前只构建 `control`。原生服务器与接收程序使用 POSIX 接口，部署在 Linux；Windows GUI 通过网络控制它们。工作流配置了Qt 5.14.2/MinGW 7.3 x64当前源码job：构建九个操作程序及Widgets/ECC客户端QtTest，再运行完整QtTest并保留14天回执/日志。Windows control 源码于71.747已由本机Kit重新构建9/9；QtTest/GUI人工操作未运行，GitHub Actions也尚未运行。独立部署及现场验收仍需单独完成。MCP 是 stdio 客户端进程，Windows 可本机运行并经 HTTP 访问 ECC 主机。

macOS 应先用 `data` profile 构建无 Qt 原生接收/存储/归档程序，再按需构建 `control`、`data-host` 或 `full`。构建收据现在会通过`otool -L`记录Mac目标的动态依赖，并对数据程序执行Qt/Ice/yaml/Boost/curl边界检查；`lipo -archs`会记录每个Mach-O程序的架构并要求包含当前构建机架构（Universal Binary可包含多个架构）。CI还会逐项确认收据报告的主机架构与Mac runner实际架构一致。对目标Mac拷贝后的RPATH、目标架构兼容性和运行时加载仍需按[macOS构建与验收指南](MACOS_BUILD.md)检查。Apple文件系统独占归档发布、receiver loopback和完整人工验收步骤也见该指南。`.github/workflows/native-data-path.yml`另设`macos-host-suite` job，使用Qt 5.15.2 clang_64并从当前源码构建`full` profile的30个主机程序，随后运行receiver/Sentry进程生命周期、接收和封存回环集成，并检查测试后没有`.graw`残留；该job要等GitHub Actions实际运行通过后才构成macOS构建/联调证据，配置文件本身不代表Mac已验收。

同一工作流设有`ubuntu24-current-source-full-suite` job，在GitHub `ubuntu-24.04` runner上从当前源码完整构建30个程序，运行配置核心、Widgets、CTest、迁移/事务以及receiver传输和四路1GiB回归；它在失败后也检查并清理专用`/tmp`工作目录内遗留的`.graw`。详细命令和证据边界见[Ubuntu24验证指南](UBUNTU24_VALIDATION.md#8-github-actions上的ubuntu24当前源码全套验证)。workflow通过本地静态检查不等于job已经运行，需以Actions实际结果作为Ubuntu24托管runner验收证据。

## 2. 需要准备的工具

构建入口 `tools/build-suite.py` 只使用 Python 3 标准库，不安装 Python 第三方包，也不联网下载源码。Python 是构建和包检查工具，不是 C++ 程序的运行依赖。

- `control`：Python 3.8 及以上、Qt 5.12 及以上的 Qt 5、匹配 Qt Kit 的 C++ 编译器和 make。Windows 使用用户指定的 Qt 5.14.2 MinGW。
- `data`：Python、CMake 3.12 及以上、64 位 C++11 编译器、make 或相应 CMake 生成器、POSIX 线程库；不需要 qmake。
- `data-host` / `full`：以上原生工具，加 Qt Core/Network/Concurrent；完整操作台还需要 Qt Widgets。
- `analysis`：Qt 5、ROOT 6（含 ROOT CMake 配置）、CFITSIO及其开发包；ROOT/CFITSIO只供显式选择的离线分析程序，不进入`data`或`full`默认接收器依赖。ROOT转换/查看组件按上游接口使用C++17，DAQ数据核心仍为C++11。
- Linux 运行依赖检查使用 `ldd`，一般由系统 libc 工具包提供。

Ubuntu 24.04 可使用发行版的开发包，例如：

```sh
sudo apt install build-essential cmake python3 qtbase5-dev qt5-qmake
```

这条命令由使用者在需要时执行；构建脚本不会调用 sudo 或包管理器。软件的专用 Ice/GET 接口、bzip2、mcpp 和 yaml-cpp 已保存在本地工程，仍需操作系统、编译器及 Qt SDK。板端 IceEirfu 不需要为了构建主机端而重新编译。

## 3. 先检查依赖、看计划，再构建

在源码根目录可先执行`python3 tools/verify-dependencies.py --profile full`。它核对固定归档/本地yaml源码及许可，完全离线，不运行编译器或硬件。对应control不要求Ice，data不要求任何本地第三方库。详细错误处理见[依赖检查](DEPENDENCIES_AND_OFFLINE_BUILD.md#9-编译前检查本地依赖)。

构建会自动执行该检查，并把准确文件摘要保存在build-receipt.json的embedded_dependencies字段。--plan与--verify-stage保持只读，不要求先执行完整依赖检查。源码包和运行包不同：运行目录带程序/模板/文档/许可，不含可重建所需的全部源代码；离线重编译应完整备份apps、common、third_party、所需source中的Slice与构建入口，并另备本平台Qt/编译器。


在工程根目录运行：

```sh
python3 tools/build-suite.py --profile full --plan
python3 tools/build-suite.py --profile full --jobs 4
```

第一条只输出计划，不创建目录、不编译、不运行程序。第二条依次构建各组件。默认目录为 `build-delivery/full/`，不会覆盖旧的 `bin/`、已有各类 `build-*` 验证目录或现场配置。

原 `bash tools/build-linux.sh all` 已改为调用同一个 `full` 入口；`control` / `data` / `supervisor` 分别对应 `control` / `data` / `data-host`。原单独的 `ecc` / `experiment` / `ice` 构建方式仍保留。不要再把 `all` 理解为只构建早期几个小工具，也不要把含管理服务的 `supervisor` 理解为无 Qt。

只编译数据工具时：

```sh
python3 tools/build-suite.py --profile data --jobs 4
```

只编译离线分析程序时：

```sh
python3 tools/verify-dependencies.py --profile analysis
python3 tools/build-suite.py --profile analysis --jobs 2 --plan
python3 tools/build-suite.py --profile analysis --jobs 2 --stage build-packages/getdaq-analysis
python3 tools/build-suite.py --verify-stage build-packages/getdaq-analysis
```

此profile会查询`qmake`与`root-config`，并同时启用FITS转换、ROOT转换和ROOT事件查看器。候选运行时依赖仍需在目标机安装匹配的Qt、ROOT和CFITSIO；常规DAQ接收主机无需因此安装这些可选分析依赖。

内存较少的机器把 `--jobs 4` 改成 `--jobs 2`。这是编译并行数，与 DAQ 并行接收通道数无关。

指定 Qt SDK：

```sh
python3 tools/build-suite.py --profile full --qmake /opt/Qt/5.14.2/gcc_64/bin/qmake --jobs 4
```

需要 Qt 的 profile 会查询这个 qmake 的 Qt 版本及安装根目录，并把根目录交给相关 CMake 组件。应使用与该 Qt Kit 一致的编译器。混用 MinGW 与 MSVC、32 位与 64 位，或多个不同 Qt SDK，不能仅靠修改 qmake 路径解决。

## 4. 输出目录怎样读

完整构建输出为：

```text
build-delivery/full/
  bin/                    六个 Qt/qmake 工程的程序
  receiver/               getdaq-receiver、getdaq-inspect、getdaq-merge、getdaq-matrix
  native/                 主机端 ECC/Ice/硬件与接收管理程序
  repository/             配置仓库 CLI 和 HTTP 服务
  control/                Qt 工程编译中间文件
  receiver.log            各组件的完整构建日志
  native.log
  repository.log
  control.log
  build-receipt.json       工具环境、命令、输入 SHA256、程序 SHA256、动态依赖
```

小 profile 只包含需要的子目录。`build-receipt.json` 中 `status: complete` 表示此次构建成功；`failed` 表示失败。`target_platform_and_hardware_accepted` 始终为 false，不能把构建收据当作现场验收证书。

同一 profile 重复构建可复用中间文件，但每次重新执行配置和构建命令。不同 profile 使用不同目录。同目录并发构建会被 `.build-lock` 拒绝。若构建被强制终止，先确认原构建进程已经停止，再移除该锁目录；不要因为日志暂时没有变化就另开同目录构建。

## 5. 整理成一个新部署目录

```sh
python3 tools/build-suite.py --profile full --jobs 4 --stage build-packages/getdaq-host
python3 tools/build-suite.py --verify-stage build-packages/getdaq-host
```

`--stage` 必须是尚不存在的新目录，避免覆盖已部署程序。程序全部复制到该目录的 `bin/`，同时复制中文文档、示例配置、第三方许可证和依赖说明；所有文件记录 SHA256。现用认证文件、现用用户配置、Run 和原始数据不会打包。

```text
getdaq-host/
  bin/                    本 profile 的可执行程序
  docs/                   中文操作、原理、架构与验证文档
  share/config/           *.example.* 和 simulation.yml 模板
  share/third_party/      许可证与依赖清单
  README.md
  manifest.json           包内文件哈希及编译环境；新包还标记并校验完整文件清单
```

中途失败的目录带有 `BUILD_INCOMPLETE`，不得作为完整包部署。新包的清单还要求包内每个普通文件都有摘要，并拒绝未登记文件或符号链接；旧版清单继续执行原有逐项核验。该检查不执行程序、不访问硬件，也不证明新机器的运行依赖已经安装。

这是一个**适用于匹配系统环境的程序目录**，不是跨 Linux 发行版通用的静态包。Linux 系统 Qt、libstdc++、libc 及 GUI 平台插件由目标机器提供，脚本不擅自复制系统库。Linux 构建会检查动态库是否缺失，并拒绝把 Qt/Ice/yaml/Boost/curl 动态依赖带入默认原生接收与查询工具；可选 ICE 数据工具也拒绝 Qt 依赖。

软件专用依赖源码仍在源码工程的 `third_party/`，构建需要的 GET Slice 接口仍在 `source/`。程序目录没有包含全部源码或 SDK；要保留可离线重建能力，须同时保存完整源码工程，详见 [依赖本地保存](DEPENDENCIES_AND_OFFLINE_BUILD.md)。

文档中的源码/测试跳转链接按原源码工程的相对目录编写，单独打开程序包时可能没有对应文件；操作步骤、文档之间的链接和图片仍在包内。需要查看实现与测试时使用源码工程中的同名文档。

源码文档更新后，如果只需让现有候选包带上最新 README/手册，可刷新文档并重算SHA清单，不必重新编译程序：

```sh
python3 tools/refresh-package-docs.py --kind host \
  --candidate build-packages/getdaq-ubuntu24-20261020-split-scalar-candidate
```

工具先验原包，再替换README和文档，确认程序、运行库、许可、配置及验证回执的SHA未变，最后重验整个包。目标必须是`build-packages/`下已存在的有效候选；它不会改变任何程序，也不会把源码或验证记录伪装成新的构建。

如果源码中的`config/*.example.*`或`simulation.yml`发生变化，而候选包需要继续分发，应使用单独的示例配置刷新器。它依据`tools/build-suite.py`当前打包清单同步全部`share/config/`文件，要求候选文件清单与源码清单完全一致，重算包内SHA并重验候选；程序、运行库、README、手册及已有验证回执必须保持原摘要。Ubuntu24与Windows Qt运行包均可刷新：

```sh
python3 tools/refresh-package-examples.py --kind ubuntu24 \
  --candidate build-packages/getdaq-ubuntu24-20261020-71-34-current-source-candidate
python3 tools/refresh-package-examples.py --kind windows \
  --candidate build-packages/getdaq-windows-qt5142-20261021-71-80-current-source-candidate
```

该命令只对`build-packages/`内已有且通过校验的候选执行。刷新期间在候选同一目录暂存完整副本，全部检查通过后才替换候选；不要在候选有手工修改时运行，以免覆盖这些修改。

若仅一个支持的CLI源码变化，可使用`tools/refresh-ubuntu24-program.py`对已有完整Ubuntu24候选做单程序增量刷新。`getdaq-supervisor-query`、`getdaq-catalog`、`getdaq-merge`和`getdaq-matrix`分别要求对应回归收据；刷新器核对测试源程序SHA/清理状态，以Ubuntu24 loader核对依赖和help，并保留未重建程序摘要。首次加入`getdaq-merge`或`getdaq-matrix`时也允许从相应旧程序候选增量升级，但必须提供绑定精确二进制的专项回归收据。该过程不是全量构建，也不能替代独立目标机验收；当前增量包路径和验证结果见[重构状态](REFACTOR_STATUS.md)及[验证记录71.37](VALIDATION.md#7137-coboframeviewer-gmat波形矩阵转换2026-10-20)。

## 6. Windows Qt 5.14.2 MinGW 的操作

先打开对应 Kit 的 Qt 命令提示符，让 qmake 和 mingw32-make 位于 PATH 中。任选一种方式：

1. 用 Qt Creator 打开根目录 `GetDAQ.pro`，选择 **Qt 5.14.2 MinGW**，构建九个子工程。默认输出仍在工程 `bin/`。
2. 在已安装 Python 的命令提示符中运行：

```bat
python tools\build-suite.py --profile control --jobs 4
```

指定路径时：

```bat
python tools\build-suite.py --profile control --qmake C:\Qt\5.14.2\mingw73_64\bin\qmake.exe --make C:\Qt\Tools\mingw730_64\bin\mingw32-make.exe
```

以上路径是示例，须换成机器上的实际 Kit；选择同套 64 位 MinGW，不要把示例当成自动安装位置。统一构建输出在 `build-delivery\control\bin\`。

发布到另一台 Windows 机器时，复制生成目录，再在发布副本中使用同一 Kit 的 `windeployqt --release GetDAQ.exe`。还应对副本中的 sentry、simulator、config 和 config-upload 执行 windeployqt，以包含各自需要的 Qt 模块，并检查 MinGW 运行库及 Qt 平台插件。原生 `getdaq-config-check` 不需要 Qt，但可能需要 MinGW 运行库。

若使用了通用 `--stage`，清单仅覆盖脚本打包时的程序、文档和模板；后来由 windeployqt 添加的 DLL/插件没有包含在该清单内，须单独验收。

2026-10-11 已从 WSL 调用用户实际安装的 Windows Qt 5.14.2 / MinGW 7.3，六个操作端程序 Release 构建通过。新增 `tools/build-windows-from-wsl.py` 和 `tools/deploy-windows-from-wsl.py`，后者创建包含 DLL/插件及其哈希的新运行目录，检查移除 SDK PATH 后的启动；本 Kit 的 windeployqt 使用自动模式，强制 `--release` 会误拒绝平台插件。实际路径、现成运行包、逐步复制/构建/检查和剩余范围见 [Windows 专用手册](WINDOWS_QT5142.md)。Windows 完整功能、独立干净机器与真机仍待验收。

## 7. 上线前还需要检查什么

1. 在目标机器先检查构建包及程序依赖，查看各服务 `--help`；GUI 使用模拟配置进行人工操作验收。
2. 按 [ECC 服务手册](ECC_NETWORK_SERVICE.md)、[远程接收管理](REMOTE_RECEIVER_CONTROL.md) 和 [配置仓库](CONFIGURATION_REPOSITORY.md) 准备自己的固定配置、认证和权限；不能直接照搬示例路径启动现场采集。
3. 核对现场板端版本、身份、端口、配置与实际数据协议，先只读检查，再进行受控联调。新目录不代表加载了新硬件配置。
4. 按 [未完成验收](VALIDATION.md#71-仍需完成的验收) 检查平台、真机、长时间吞吐和数据可靠性。传输测试每轮所有输入/输出合计不超过 **20GiB**，结束删除本轮自有数据，只保留小日志和校验凭据。

常见错误：没有 qmake 就选择 `data` 或安装匹配 Qt Kit；原生构建提示缺少网络服务 target 时检查 Qt 开发模块；运行时缺少动态库时安装匹配运行库；打包目标已存在时选择新目录。遇到失败先看相应组件 `.log`，不要跳过缺失程序后把包标记为完整。
