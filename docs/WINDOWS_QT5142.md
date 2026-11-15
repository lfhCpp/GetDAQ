# Windows Qt 5.14.2 MinGW：构建、运行包和检查步骤

**历史候选（71.274，2026-10-22）：** `build-packages/getdaq-windows-qt5142-20261022-current-source-goal-final-candidate/`从当时源码重建8个x86_64控制程序，包含287个清单文件。这个候选和回执保留作历史；71.369后根`GetDAQ.pro`纳入MCP控制进程，当前控制套件为9个程序。新源码验证情况见本手册后续更新和[验证记录](VALIDATION.md)。旧候选不包含MCP。

**最近一次完整 control 构建（71.773，2026-11-10）：** 使用 Qt 5.14.2 / MinGW 7.3 x64 Kit 重建当前源码的9个程序，均为 x86_64 PE；1,054项输入记录和离线嵌入依赖校验通过，构建日志没有 warning/error 行，包含最新 Waveform Viewer CLI 拆分。回执：`build-windows-refactor-772/windows-build-receipt.json`；日志：`build-windows-refactor-772/control-windows.log`。未运行 QtTest、程序启动、GUI交互、部署smoke或硬件流程；回执明确标记未验收实机和独立Windows机器。详见[验证记录71.773](VALIDATION.md#71773-current-source-windows-qt-5142-control-suite-build-2026-11-10)。


这份手册针对 Windows **操作电脑**。GetDAQ 窗口负责配置、低频命令、状态和 Run 记录；大量原始事件由 Linux 数据主机上的独立原生接收器处理，不经过窗口，也不因使用 Qt 而复制到 Windows。板端 getHwServer / IceEirfu 保留现有部署。

旧运行候选均为历史产物；71.748生成的是当前源码构建目录，不是经过DLL部署、SDK-free启动检查的发布包。需要运行包时，按本手册重新构建、部署并逐项检查，不能把旧候选当作当前构建产物。

`windows-qt5142-control-build` GitHub Actions job仍未运行；本次构建由本机已安装的Kit完成。

2026-10-18的70.65候选新增[冷数据交接恢复窗口](COLD_DATA_HANDOFF_WINDOW.md)。六程序已用本页指定Kit重建；恢复服务与原生CLI运行在原ECC的Linux主机，Windows只提交/查询元数据。候选运行包为`build-packages/getdaq-windows-qt5142-20261018-cold-handoff-window-candidate`；本机运行包检查以该包清单及VALIDATION70.65为准，不代表Windows完整冷恢复操作已经人工验收。以下较早日期保留为历史。

2026-10-11 已使用用户实际安装的 Qt 5.14.2 / MinGW 7.3 64 位构建六个操作程序，并制作包含 DLL、插件的运行目录。已验证移除 SDK PATH 后的帮助命令、初始窗口和离线配置快照。完整人工操作、独立干净 Windows 机器和现场硬件尚未验收；本次没有高速传输测试。

2026-10-12 多文档工作区版已重新构建六程序，并用实际 MinGW 严格 C++11 运行配置核心：83 项通过、1 项 Unix 权限夹具跳过。新增 [多文档与 Save All](CONFIGURATION_MULTI_DOCUMENT.md) 入口；Windows 完整人工交互仍待验。前一版 20261011-final 包保留历史字节。

## 1. 实际 Kit 和当前控制程序

20261012-ecc-cleanup 版新增 native 模式下的失败接收器清理菜单和明确确认，保留原身份、新鲜 UNKNOWN、原停生产审计与原数据会话门槛。详细步骤见 [ECC 清理入口](ECC_FAILED_RECEIVER_CLEANUP.md)。该菜单只发送低频控制请求，原始数据和 SIGTERM 处理仍在 Linux 接收主机。

本次核对的安装目录：

```text
Qt:       D:\Software\QT\5.14.2\mingw73_64
qmake:    D:\Software\QT\5.14.2\mingw73_64\bin\qmake.exe
编译器:   D:\Software\QT\Tools\mingw730_64
make:     D:\Software\QT\Tools\mingw730_64\bin\mingw32-make.exe
```

| 程序 | 用途 | 是否处理原始 DAQ 数据 |
| --- | --- | --- |
| GetDAQ.exe | 操作窗口、配置编辑、控制和状态 | 否 |
| getdaq-sentry.exe | 主机监控、控制状态服务 | 否；查询文件计数不等于接收事件 |
| getdaq-simulator.exe | 本机模拟 ECC/SOAP 与监控接口，供操作端联调 | 否；不代表真实固件 |
| getdaq-config.exe | 离线解析、导出、快照与编译配置 | 否；不会自动写寄存器 |
| getdaq-config-check.exe | 原生 C++11 检查配置记录与读回审计 | 否；不依赖 Qt |
| getdaq-config-upload.exe | 上传配置版本到配置仓库 | 否；上传不代表加载硬件 |
| getdaq-supervisor-query.exe | C++11/WinSock 低频查询与单次控制提交客户端 | 否；不接收或转发原始DAQ载荷 |
| getdaq-waveform-viewer.exe | 只读查看已有 `.graw` / `.gmat` 波形并导出图像/数据 | 否；离线文件查看器，不接网络或DAQ接收器 |
| getdaq-control-mcp.exe | stdio 控制接口，经 HTTP 向 ECC 主机发出受约束的低频操作 | 否；不读取或转发原始DAQ载荷 |

窗口、监控、模拟器及部分配置工具使用 Qt；检查器只需要匹配的 C++ 运行库。主机端 ECC、接收管理服务和接收器仍部署在 Linux。原生接收器没有新增 Qt 依赖。

`getdaq-supervisor-query`用于与数据主机的低频控制服务认证通信。Windows凭据文件必须由当前运行用户持有，ACL只允许该用户和LocalSystem访问；程序拒绝符号链接/reparse point、硬链接、继承权限或其他主体的访问ACE。把服务器凭据安全地写入私有文件后，可在PowerShell中重置并收紧该文件ACL（命令不包含凭据内容）：

```powershell
$credential = 'D:\GetDAQ\private\supervisor-token.txt'
$principal = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
icacls $credential /reset
icacls $credential /inheritance:r
icacls $credential /grant:r "$($principal):(R)" 'SYSTEM:(R)'
```

运行GetDAQ和查询helper的Windows用户必须与文件所有者相同。该文件保存的是低频控制服务凭据；归档文件复制使用的普通TCP/IP是另一条无认证协议，两者用途和安全边界不同。

## 2. 直接检查已经生成的运行包

当前没有经部署和运行检查的发布包。71.748 的程序只位于 `build-windows-refactor-745/bin/`，尚未复制Qt/MinGW DLL或执行SDK-free启动检查。以下旧路径仅供历史参考，不能视作当前版本：

```text
build-packages/getdaq-windows-qt5142-20261022-71-188-publication-commit-candidate/
```

在 Windows 资源管理器中可从以下源码目录进入：

```text
\\wsl.localhost\Ubuntu\home\lfh\Work\Codex\GetDAQ
```

其中 Ubuntu 是本次 WSL 发行版名；其他机器需要换为自己的名称。推荐把**整个运行包目录**复制到普通 Windows 本地目录，例如 `D:\GetDAQ\control`，然后双击 `GetDAQ.exe`。不要只复制 exe：Qt DLL、MinGW DLL 和 platforms 子目录都要保留。查询helper没有Qt DLL依赖，但仍需同目录 MinGW C++运行库。

初次启动默认 Simulation，Disconnected 是正常初始状态。未配置节点前不会连接硬件。可以先检查按钮和表格文字、窗口缩放、配置编辑和模拟模式；现场网络参数在核对后填写。

运行目录结构：

```text
GetDAQ.exe                     以及另外八个控制/离线工具
getdaq-waveform-viewer.exe     独立离线波形查看器
Qt5Core.dll / Qt5Gui.dll        Qt 模块，由相同 Kit 部署
Qt5Network.dll / Qt5Widgets.dll
libgcc_s_seh-1.dll             MinGW 运行库
libstdc++-6.dll
libwinpthread-1.dll
platforms/qwindows.dll         正常 Windows 窗口平台插件
platforms/qoffscreen.dll       离屏检查插件
imageformats/ ...              Qt 图像插件等
qt.conf                        插件从运行目录查找
docs/                          中文操作与架构文档
share/config/                  示例配置
share/third_party/              专用依赖说明和许可证
share/licenses/                本机 SDK 与编译器许可证文件
initial-window.png              离屏启动检查截图
native-window.png               Windows 平台初始窗口截图
manifest.json                  包内文件 SHA256 和验证范围
```

目录中的 README 与其他平台手册讲解整套软件；Windows 八程序放在根目录，Linux 完整套件通常放在 bin，不能混用两者路径。

有 Python 和完整源码时，可仅核对文件、不启动程序：

```bat
python tools\deploy-windows-from-wsl.py --verify build-packages\getdaq-windows-qt5142-20261022-71-188-publication-commit-candidate
```

这个 `--verify` 分支可在 Windows 或 Linux 使用，不调用 WSL/Windows 构建工具。检查器核对记录内全部文件哈希、八程序哈希和 PE 64 位格式，并拒绝额外未登记文件。配置和 Run 请保存在运行包外，避免与包完整性检查混在一起。哈希核对只证明文件与本次清单一致，不证明来源可信或硬件功能正确。

## 3. 用 Qt Creator 在 Windows 本机编译

1. 保存完整源码目录，尤其保留 common、apps、third_party；不需要把旧板端软件重新编译。
2. 在 Qt Creator 打开根 `GetDAQ.pro`。
3. 选择 Desktop Qt 5.14.2 MinGW 64 位 Kit，确认它指向上面的 qmake 和匹配 MinGW。不要选 MSVC 或 32 位 Kit。
4. 使用独立构建目录，选择 Release，构建整个 SUBDIRS 工程。根工程包含八个Qt控制端子工程（包括独立stdio MCP进程）及不依赖Qt的`getdaq-supervisor-query`命令行子工程，共9个控制程序。
5. 程序默认输出到源码根 bin；若 Creator 给出不同路径，以运行配置和构建日志为准。选择 GetDAQ 运行目标。

安装了 Python 后，也可以在匹配 Kit 的命令提示符中使用统一入口：

```bat
set "PATH=D:\Software\QT\Tools\mingw730_64\bin;D:\Software\QT\5.14.2\mingw73_64\bin;%PATH%"
python tools\build-suite.py --profile control --qmake D:\Software\QT\5.14.2\mingw73_64\bin\qmake.exe --make D:\Software\QT\Tools\mingw730_64\bin\mingw32-make.exe --jobs 2
```

该入口输出到 `build-delivery\control\bin`。本轮实际构建使用下一节的 WSL 入口；上述 Windows 本机 Python 入口还需要用户在本机独立检查，不能由 WSL 构建结果推断所有启动方式都通过。

## 4. 从 WSL 或 Windows 本机使用用户 Windows Kit

如果 WSL 调用 Windows 程序时报 `UtilBindVsockAnyPort`、`socket failed` 等互操作错误，直接在 Windows 命令提示符或 PowerShell 中从源码目录运行同一入口。脚本会使用系统的 `cmd.exe` 和 PowerShell，不依赖 WSL 互操作。用户当前 Kit 路径为 `D:\Software\QT\5.14.2\mingw73_64`，MinGW 7.3 路径为 `D:\Software\QT\Tools\mingw730_64`：

```bat
python tools\build-windows-from-wsl.py --kit "D:\Software\QT\5.14.2\mingw73_64" --compiler "D:\Software\QT\Tools\mingw730_64" --jobs 2 --plan
python tools\build-windows-from-wsl.py --kit "D:\Software\QT\5.14.2\mingw73_64" --compiler "D:\Software\QT\Tools\mingw730_64" --jobs 2
python tools\deploy-windows-from-wsl.py --kit "D:\Software\QT\5.14.2\mingw73_64" --compiler "D:\Software\QT\Tools\mingw730_64" --destination build-packages\getdaq-windows-native-check --native-window-smoke
python tools\deploy-windows-from-wsl.py --verify build-packages\getdaq-windows-native-check
```

先执行 `--plan` 确认列出的 Qt 和 MinGW 路径正确。构建和部署都在工作区内生成独立目录；部署目标必须尚不存在。该脚本入口名保留 `from-wsl` 以兼容旧用法，但现在也支持 Windows 原生 Python。若源码目录位于 WSL Linux 文件系统而非 Windows 可访问盘符中，Windows 原生 Python 无法直接读取它；这种情况继续使用下方 WSL 入口，或把工作区放在 Windows 盘挂载目录。

WSL 用法：

源码在 WSL、本机已装 Qt 时，不必另装 Linux Qt 5.14.2，也不必联网下载依赖。在源码根执行：

```sh
python3 tools/build-windows-from-wsl.py \
  --kit /mnt/d/Software/QT/5.14.2/mingw73_64 \
  --compiler /mnt/d/Software/QT/Tools/mingw730_64 \
  --jobs 2 --plan

python3 tools/build-windows-from-wsl.py \
  --kit /mnt/d/Software/QT/5.14.2/mingw73_64 \
  --compiler /mnt/d/Software/QT/Tools/mingw730_64 \
  --jobs 2
```

`--plan` 只显示计划。实际构建要求 Qt 版本恰好为 5.14.2，并运行真实 Windows qmake、moc、uic、g++ 与 make。默认目录是 `build-windows-qt5142`，程序在其 bin 子目录，记录在 windows-build-receipt.json，完整编译日志为 control-windows.log。

不能直接把 UNC 绝对源码路径交给 qmake 后就假定 GCC 能读取：本轮生成命令丢失前导斜线而失败。脚本通过 Windows batch 的 pushd 临时映射构建目录，qmake 使用相对工程路径，子工程输出使用映射盘绝对路径，最后 popd。源码和构建输出仍在原工作区，不修改 Qt SDK。

`.windows-build-lock` 防止产品构建入口同目录并发；QtTest构建器使用`.build.lock`独占目录。若被强制终止，先确认进程已停止，再处理遗留锁；不要因日志短暂不变化而另起同目录构建。

## 5. 收集 DLL、插件并创建新运行包

完成上一节构建后执行，destination 必须是**尚不存在的新目录**：

```sh
python3 tools/deploy-windows-from-wsl.py \
  --kit /mnt/d/Software/QT/5.14.2/mingw73_64 \
  --compiler /mnt/d/Software/QT/Tools/mingw730_64 \
  --destination build-packages/getdaq-windows-check \
  --native-window-smoke

python3 tools/deploy-windows-from-wsl.py --verify build-packages/getdaq-windows-check
```

脚本核对编译输入和八程序哈希，要求部署用同一 Kit 和编译器，只对Qt程序调用 windeployqt，纯C++11查询helper不引入Qt运行库；随后补充匹配的 MinGW 运行库和离屏插件。它记录 PE 导入，检查主要 Qt/MinGW/ANGLE 依赖是否已打包；实际启动还用于发现遗漏。它不宣称穷举所有动态加载组件或替代独立干净机器验收。

本 Kit 的插件虽然导入 Qt5Core.dll / Qt5Gui.dll，却被 windeployqt 的 MinGW PE 标志逻辑标作 debug；强制 `--release` 会把它们全部过滤并报“Unable to find the platform plugin”。Qt 5.14.2 自带源码说明 MinGW 的 debug 检测不可靠，自动模式允许匹配插件。因此本入口对已经 Release 编译的程序使用自动模式，未复制 Qt5Cored.dll 等另一套调试模块，也不修改 SDK 插件。

启动检查将 PATH 缩减为 Windows 系统目录，清空 Qt 插件路径变量，用包内 qt.conf 和 DLL 运行八个 `--help` 命令及初始窗口截图。查询helper的导入仅含Windows系统网络/ACL接口和MinGW运行库；加 `--native-window-smoke` 会短暂显示Windows窗口、截图后退出，不连接硬件。离屏截图在这个Kit上可能不绘制文字，不能当作人工GUI验收；native-window.png使用正常qwindows插件，文字已查看。

Qt 的 Windows GUI 程序在没有控制台或明确标准句柄时，会把 `--help` 放进需要关闭的系统对话框，自动检查可能因此卡住。当前入口通过本轮生成的 PowerShell 脚本显式重定向各程序 stdout/stderr，记录包内 help/screenshot 文本；每个自有子进程最多等待10秒，超时按该进程PID终止测试树并报告失败。无需改变程序为控制台应用，也不修改系统执行策略。完整脚本与汇总日志保存在构建目录。

部署未成功时保留 BUILD_INCOMPLETE，不能使用或分发为完整包。先看 deployment-windows.log / runtime-smoke-windows.log / native-runtime-smoke-windows.log。更换新 destination 重试；不要对失败目录随意添加 DLL 后沿用旧清单。

如果Windows程序没有变化、只是需要同步更新后的中文手册，可对已验收的完整候选刷新文档和SHA清单，不用重新编译或重新部署DLL：

```bash
python3 tools/refresh-package-docs.py --kind windows \
  --candidate build-packages/getdaq-windows-qt5142-20261021-71-80-current-source-candidate
```

该操作会先验证原包，刷新README及`docs/`，再验证更新后的文件清单，并逐项确认八个程序、DLL、Qt插件及包中已有的验证材料均保持原SHA。候选无效或路径含符号链接时拒绝处理。

## 6. 离线配置编译怎样检查

在运行包命令提示符中，把配置和输出放在包外的新目录：

```bat
getdaq-config.exe snapshot D:\GetDAQ\my-config\main.xcfg D:\GetDAQ\my-config\snapshot.json
getdaq-config-check.exe snapshot D:\GetDAQ\my-config\snapshot.json
```

第一步将 include/继承解析后的有效配置写成快照，输出文件必须是新的。第二步用独立原生检查器重算语义摘要。`valid:true` / `effective_snapshot_semantics_recomputed:true` 表示支持的记录核对通过；`hardware_loaded_verified:false` 明确表示没有证明硬件加载。

本轮实际 Windows 程序验证了两个具体 Board 共享 include、Module 默认继承、整数范围和中文字符串值；夹具与结果位于 `build-windows-qt5142/offline-configuration`。Windows 中文**内容**通过不等于所有原生工具的 Unicode **文件路径**都已支持；本轮文件路径使用 ASCII，中文路径仍须单独验证。

真正的连接、配置、采集、停止与 Run 操作按 [操作手册](OPERATIONS.md) 和 [ECC GUI](ECC_GUI_CLIENT.md) 进行。先核对 Linux 服务身份、认证、板卡地址与接收目录；历史读回只说明原 Configure 的审计结果，不能自动解除 UNKNOWN 或证明当前全量加载。

## 7. 常见问题与剩余验收

| 现象 | 检查方式 |
| --- | --- |
| 缺 Qt5Core.dll / libstdc++-6.dll | 复制完整包，核对 manifest；不要从另一 Kit 随意找 DLL |
| 找不到 windows platform plugin | 确认 platforms/qwindows.dll 和 qt.conf 保留，清除指向另一 SDK 的插件环境变量 |
| windeployqt 找不到平台插件 | 对本 MinGW Kit 使用上述自动模式，查看完整部署日志 |
| Qt Creator 链接失败 | 检查 64 位 MinGW Kit，确认 compiler/make 与 Qt 对应 |
| WSL 出现 UtilBindVsockAnyPort / socket failed | 当前受限环境不允许 Windows 互操作；需要在允许互操作的终端运行，不是 C++ 编译错误 |
| 运行目录清单检查失败 | 不覆盖旧包，区分新增用户文件与被修改的程序，重新构建/部署到新目录 |

仍需完成：完整人工窗口和模拟控制流程、独立干净 Windows 机器、中文文件路径、持久化/故障操作、真实 Linux ECC 和板端联调。Ubuntu24、macOS、物理多主机、长期高速接收、断电与完整 UNKNOWN 恢复另按 [验收清单](VALIDATION.md#71-仍需完成的验收)。任何传输测试每轮所有流合计不得超过 20GiB，结束清理自有载荷；Windows 本轮仅编译和小型配置/截图检查，没有生成 .graw。

## 8. GitHub Actions 当前源码构建检查

`.github/workflows/native-data-path.yml`中的`windows-qt5142-control-build` job在`windows-2022`上安装Qt 5.14.2 `win64_mingw73`套件和对应MinGW 7.3工具，调用`tools/build-windows-from-wsl.py`构建9个控制程序，再调用`tests/build_windows_qt_validation.py`与`tests/run_windows_qt_validation.py`构建/运行Widgets及ECC客户端QtTest，并用`tests/windows_supervisor_query_integration.py`验证受ACL保护的临时凭据、身份401拒绝不重试、一次202控制提交、POST后断连/HTTP 500不重试及临时目录清理。用户本机71.163的8程序与Widgets/ECC客户端回归属于历史证据；71.746构建当前9程序；71.747清理告警后重新构建9程序，均未运行QtTest。GitHub Actions job本身尚未运行。测试使用正常Windows Qt插件与Fusion风格；runner将自有临时空间限制为512MiB，监视`.graw`合计不超过20GiB并在结束时删除私有临时根。CI合同已配置不代表Actions结果通过。

运行成功后会上传产品/QtTest构建回执、QtTest结果摘要和日志，保留14天；可在Actions页面的`Native data path` workflow中查看。工作流定义和本地命令合同检查通过只表示自动检查已配置，不能写成Windows编译或QtTest通过。该job不打包/启动产品GUI、不连接真实服务或硬件；这些验收需分别看本页前述步骤及实际runner结果。
