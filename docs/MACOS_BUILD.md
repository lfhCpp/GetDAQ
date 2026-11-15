# macOS 构建与验收指南

本指南说明如何在真正的 macOS 主机上构建 GetDAQ、检查原生数据服务和归档程序，并区分已验证内容与必须由 Mac 主机补做的验收。仓库里有 Apple 专用文件时间戳、独占目录发布和 `SO_NOSIGPIPE` 处理，但这些源码分支尚未在本项目中用 Apple SDK 实际编译；Linux 上模拟 `__APPLE__` 不能替代 Mac 构建。

仓库提供 [Ubuntu 24/macOS 原生数据面自动化](../.github/workflows/native-data-path.yml)和独立的`macos-host-suite`。数据面workflow在两个原生runner上构建接收器、归档/传输工具并运行有限回归；Linux runner另安装Qt，只构建低频Sentry并做进程归档联调，高速原始接收器与归档/传输工具仍不链接Qt。数据面job中的macOS runner保持无Qt。`macos-host-suite`安装Qt 5.15.2 clang_64，从当前源码构建完整POSIX host suite，并检查每个程序的Mach-O架构与runner一致；然后构建并运行Core、Widgets和ECC客户端QtTest（Core/Widgets分别固定为151/115个通过用例）、receiver/Sentry进程生命周期、接收和封存联调。QtTest的HOME、XDG缓存/配置/数据/运行目录和TMPDIR统一放入私有清理树，结束或失败都会清理并核查`.graw`残留。工作流在Linux/macOS分别使用`ldd`/`otool -L`确认receiver、inspect、catalog和supervisor-query没有Qt动态库依赖；Linux runner因另装Qt不会掩盖误链接，并显式确认该检查器会拒绝Qt链接的Sentry。`catalog-transfer-filesystem`除了检查退出码，还必须输出无认证普通TCP中断续传、完整读回和完成后零载荷复续传的通过标记；这样Qt边界或普通TCP续传用例缺失时，CI不会把它误判为通过。两个macOS job都要等Actions实际运行通过后才构成对应的Mac构建/回归证据；本地Linux通过不能代替它。

## 1. 先了解 macOS 上运行哪些程序

macOS 使用 Apple Clang 和 POSIX 文件/网络接口。原生 `getdaq-receiver` 不依赖 Qt；Qt 只用于操作台和低频管理服务。Windows 上的 MinGW 二进制不能复制到 macOS 使用，Mac 必须用本机工具链重新构建。

| 构建配置 | 适用场景 | 主要程序 | Qt |
| --- | --- | --- | --- |
| `data` | 只接收、检查和管理数据目录 | receiver、inspect、supervisor-query、catalog | 不需要 |
| `data-host` | 需要在节点运行低频监督服务 | `data` 程序及 supervisor | Qt Core、Network、Concurrent |
| `control` | 只在 Mac 上运行操作台/配置工具 | GetDAQ、sentry、simulator及配置工具 | Qt Widgets 等 |
| `full` | 在一台 Mac 上重建完整主机端套件 | 上述原生/控制程序、ECC、硬件和可选ICE兼容工具 | Qt 5；可选Ice构建使用本地源码 |

这些配置都不重写板端 `getHwServer` 或 `IceEirfu`。构建完成也不表示已连接真实硬件、VxWorks发送端或生产磁盘。

## 2. 准备工具并记录机器架构

在 Mac 打开“终端”，进入 GetDAQ 源码根目录。先记录 macOS 版本、CPU 架构和命令行工具路径：

```sh
sw_vers
uname -m
xcode-select -p
clang++ --version
cmake --version
python3 --version
```

需要 Xcode Command Line Tools、Python 3、CMake 3.12 或更新版本，以及 Apple Clang。`data` 配置不要求 Qt。构建 `data-host`、`control` 或 `full` 时，还需安装 Qt 5.12 或更新的 Qt 5 Mac Kit，并让 `qmake` 与所用编译器来自同一 Kit。不要把 Windows Qt 5.14.2 MinGW 的 `qmake`、库或 DLL 路径传给 Mac 构建。

如果机器提供多个 Qt Kit，先确认选中的位置和版本：

```sh
which qmake
qmake -query QT_VERSION
qmake -query QT_INSTALL_PREFIX
```

记录 `uname -m` 的结果（例如 `arm64` 或 `x86_64`）、Qt 版本、Xcode Command Line Tools 版本和 CMake 版本。原生构建只产生当前工具链/架构的程序；本指南不声称生成 Universal 2 包，也不自动转换 Intel 与 Apple Silicon 架构。

## 3. 首先构建不依赖 Qt 的数据工具

`data` 配置最适合先确认 Mac 原生数据路径。构建计划只读，不会创建构建目录：

```sh
python3 tools/verify-dependencies.py --profile data
python3 tools/build-suite.py --profile data \
  --build-root build-delivery/macos \
  --jobs 4 --plan
```

检查输出中的组件是 `receiver` 与 `supervisor`，且不包含 Qt 服务。然后执行构建，并把产物复制到一个新的、此前不存在的候选目录：

```sh
python3 tools/build-suite.py --profile data \
  --build-root build-delivery/macos \
  --jobs 4 \
  --stage build-packages/getdaq-macos-data-candidate

python3 tools/build-suite.py \
  --verify-stage build-packages/getdaq-macos-data-candidate
```

成功时构建收据位于 `build-delivery/macos/data/build-receipt.json`，程序和使用文档位于 `build-packages/getdaq-macos-data-candidate/`。候选目录只在新路径创建；如果路径已经存在，先选择一个新的候选名称，不要覆盖旧证据或现场运行目录。

Mac 构建入口会用 `otool -L` 检查动态库、用 `lipo -archs` 检查每个程序的 Mach-O 架构，并将结果写入构建收据；数据程序中出现 Qt、Ice、yaml-cpp、Boost 或 curl 动态依赖时会拒绝生成完整候选。二进制必须包含构建机器当前架构；Universal Binary 可包含多个架构。候选复制到目标Mac后，仍应检查目标机器架构、RPATH及实际运行库加载：

```sh
for program in build-packages/getdaq-macos-data-candidate/bin/*; do
  echo "===== $program"
  file "$program"
  otool -L "$program"
done
```

`lipo -archs` 的自动结果记录在 `build-receipt.json` 的 `mach_o.architectures` 和 `mach_o.host_architecture` 字段；该选项输出空格分隔的架构列表。不要将其替换为输出描述性文件名的`lipo -info`后仍沿用相同解析。目标机仍应确认其CPU架构出现在程序支持列表中。除系统库外，不应出现来自 Linux、Windows 或另一个 Qt Kit 的路径。`otool` 的输出是动态库清单，不是独立安装包证明。把候选复制到另一台 Mac 后，还要在那台机器上启动各个程序并检查其系统运行库可用性。[LLVM lipo 命令参考](https://www.llvm.org/docs/CommandGuide/llvm-lipo.html)说明`-archs`显示架构并以空格分隔。

## 4. 在 Mac 上运行原生接收与归档回归

先用单独目录编译接收器和测试，不覆盖正式交付候选：

```sh
cmake -S apps/datareceiver \
  -B build-macos-validation/receiver \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-macos-validation/receiver --parallel 2
(cd build-macos-validation/receiver && ctest --output-on-failure)
```

macOS 上 `exporter-lock` 的 Linux 链接器故障注入测试不会注册；这不表示该场景已在 Mac 验证。接着运行真实 TCP loopback 集成回归：

```sh
python3 tests/receiver_integration.py \
  build-macos-validation/receiver/getdaq-receiver
```

该回归使用本机临时目录和不超过 32 MiB 的合成 raw，覆盖 TCP 分片、两种 MFM 字节序、两个观察客户端、封存、目录锁、慢观察端和错误帧。成功或异常退出时脚本会清理自己创建的临时目录；结束后仍应检查没有遗留 `.graw`。它验证 Mac 本机内核、文件系统和 socket API，不是板端数据或持续吞吐验收。

再单独编译数据目录、文件系统及独占归档测试。`catalog-transfer-filesystem` 会覆盖 Apple `renamex_np(RENAME_EXCL)` 分支：

```sh
cmake -S apps/datasupervisor \
  -B build-macos-validation/supervisor \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DGETDAQ_BUILD_SUPERVISOR_SERVICE=OFF
cmake --build build-macos-validation/supervisor --parallel 2 \
  --target getdaq-catalog getdaq-supervisor-query \
           receiver-catalog-tests catalog-transfer-tests supervisor-control-tests
(cd build-macos-validation/supervisor && \
  ctest --output-on-failure \
    -R '^(receiver-catalog-filesystem|catalog-transfer-filesystem|supervisor-control-codec)$')
```

独占发布测试需要在 Mac 支持的实际目标卷上运行。临时目录成功只能证明临时卷；HFS+/APFS、网络挂载及外接实验盘的行为应按部署目标单独记录。文件系统不支持独占重命名时，程序应明确失败并保留部分目录，不可退回可能覆盖目标的普通重命名。

## 5. 构建 Qt 操作端或带管理服务的数据主机

使用与本机 Qt Kit 配套的 `qmake`，不使用 MinGW：

```sh
python3 tools/build-suite.py --profile control \
  --build-root build-delivery/macos \
  --qmake "$(which qmake)" --jobs 4 \
  --stage build-packages/getdaq-macos-control-candidate
```

如果要构建提供低频管理接口的 `getdaq-supervisor`，选择 `data-host`：

```sh
python3 tools/build-suite.py --profile data-host \
  --build-root build-delivery/macos \
  --qmake "$(which qmake)" --jobs 4 \
  --stage build-packages/getdaq-macos-data-host-candidate
```

`control` 只构建操作端；它不会把原始数据接收加入 GUI。`data-host` 中的 Qt 服务只管理 receiver 生命周期及小型状态，事件字节仍由不链接 Qt 的 `getdaq-receiver` 接收和写盘。构建 `full` 时使用同一 `--qmake` 参数；若本机 Ice Kit 或编译器不兼容，先保留 `data`/`control` 独立候选和错误日志，不要把失败的可选兼容组件误报为核心 receiver 失败。

Mac 上 qmake 输出的 GetDAQ 是按该 Qt Kit 编译的程序，不自动打包 Qt 框架、插件、签名或公证材料。若需复制到干净 Mac，必须另行完成该 Qt 版本对应的依赖部署，并在没有开发 SDK 搜索路径的账户上验证首次启动、插件加载、配置读写和网络连接；目前仓库未提供 macOS 签名、公证或 Universal 2 发布包。

在真实Mac上还应构建并运行主窗口和ECC客户端的QtTest，确认Qt Widgets、文件对话框适配和本机HTTP服务逻辑可用。CI的`macos-host-suite`会执行相同流程；这里给出手动步骤：

```sh
python3 tests/build_qt_control_validation.py tests/tests.pro \
  build-macos-validation/qt-tests/widgets --jobs 3
python3 tests/build_qt_control_validation.py tests/ecc_system_client.pro \
  build-macos-validation/qt-tests/ecc-client --jobs 3

test_tmp="$(pwd)/build-macos-validation/qt-test-temp"
mkdir -p "$test_tmp"
export TMPDIR="$test_tmp"
export QT_QPA_PLATFORM=offscreen
build-macos-validation/qt-tests/widgets/getdaq-tests \
  -o build-macos-validation/qt-tests/widgets/results.txt,txt
build-macos-validation/qt-tests/ecc-client/ecc-system-client-tests \
  -o build-macos-validation/qt-tests/ecc-client/results.txt,txt

rg '^Totals:' build-macos-validation/qt-tests/{widgets,ecc-client}/results.txt
```

两份结果都应报告非零通过数、0 failed。QtTest用临时配置夹具，不发送DAQ raw；退出后仍须清理自己建立的私有目录，并检查其中没有 `.graw`：

```sh
python3 - "$test_tmp" <<'PY'
from pathlib import Path
import shutil
import sys
root = Path(sys.argv[1]).resolve()
if root.name != 'qt-test-temp' or 'build-macos-validation' not in root.parts:
    raise SystemExit('Refusing to remove a path outside the private QtTest root')
raw = [p for p in root.rglob('*.graw') if p.is_file() or p.is_symlink()] if root.exists() else []
size = sum(p.lstat().st_size for p in raw if not p.is_symlink())
shutil.rmtree(root) if root.exists() else None
print('removed raw bytes:', size, 'files:', len(raw))
if size > 20 * 1024**3 or raw:
    raise SystemExit('QtTest left synthetic raw in its private root; it has been removed')
PY
```

这组QtTest通过只表示当前Mac工具链的组件回归，不代表干净用户账户完整安装、签名应用、真实ECC服务或硬件验收。

如果已按`full` profile构建并且Qt `getdaq-sentry`也生成，可在同一台Mac上运行receiver与Sentry进程生命周期集成：

```sh
python3 tests/receiver_integration.py \
  build-delivery/macos/full/receiver/getdaq-receiver \
  build-delivery/macos/full/bin/getdaq-sentry
```

它会检查接收器运行时Sentry `/status`为200、接收器退出后为503，并继续运行有界接收/慢观察端/封存归档场景。合成输入总量不到34MiB；脚本使用自有临时目录，结束后应确认没有`.graw`遗留。该命令只有在真正macOS上执行才验证Apple的`ps -o comm`行为；Linux本机或CI配置静态检查不能替代Mac执行结果。

## 6. 记录结果和判断失败位置

每次 Mac 验收至少保留：macOS 版本、CPU 架构、Xcode/Command Line Tools、Apple Clang、CMake、Qt 和 qmake 路径/版本；完整构建收据；每个交付程序的 SHA-256、`otool -L` 输出和 `lipo -archs` 结果；CMake/CTest 输出；候选清单摘要；临时目录及 `.graw` 清理结果。不要把本机用户名、访问令牌、认证文件内容或真实数据写入回执。

检查失败时按层定位：

1. `verify-dependencies.py` 失败表示本地依赖/许可材料不完整，先读具体文件错误；不要让正式构建脚本自动联网补库。
2. CMake 配置或编译失败时，保留首条错误、Xcode/Clang 版本和对应组件日志。先确认错误发生于 `receiver`、`supervisor`、`repository`、`ICE` 还是 Qt `control`，不要将全部组件一起升级到不同工具链。
3. `lipo -archs` 不包含当前构建机架构或 `otool -L` 出现缺失/错架构库时，问题在二进制或运行库部署；不能以“程序已链接”当作可分发包。
4. `catalog-transfer-filesystem` 失败时，记录当前目标卷类型、是否临时卷、错误码和 `.partial` 是否保留。不要用普通覆盖式重命名绕过独占发布保护。
5. loopback 测试失败时，保留第一条失败日志，确认端口未被其他进程占用；只清理测试脚本创建的临时目录，不扫删工作区中的生产数据。

## 7. 当前验证边界

本指南和 Apple 条件编译分支给出了可执行的验收方法，但没有在 macOS 上产生任何成功验收结果。只有在实际 Mac 上完成上述构建、`otool` 检查、receiver 和目录回归，并留下带源码摘要的结果后，才可把对应 Mac 架构和系统版本标记为通过。一个架构或一个 macOS 小版本的结果不能外推到所有 Intel/Apple Silicon 机器。

自动化工作流用于持续重复数据面构建和回归；它不打包 Qt、签名/公证应用，不验证真实多机 SSH、实验室目标卷或硬件。Actions runner 的结果只覆盖该 runner 实际报告的系统和架构；若发布对象与 runner 架构不同，仍须增加匹配架构的验收。

`macos-host-suite`工作流产物包含工具链记录、完整host-suite构建回执及各组件构建日志、Widgets/ECC客户端测试回执和结果，以及`qt-test-cleanup.json`。清理由[`tests/cleanup_macos_qttest.py`](../tests/cleanup_macos_qttest.py)执行，只操作固定的私有QtTest临时目录，拒绝符号链接根目录；配套[`tests/test_macos_qttest_cleanup.py`](../tests/test_macos_qttest_cleanup.py)覆盖空目录、raw残留及符号链接拒绝。清理回执记录删除前发现的`.graw`数量/字节数、20GiB上限、目录是否删除和剩余数量；即使测试步骤失败，`always()`清理与产物上传仍会尝试保留诊断信息。该回执证明CI临时目录的清理状态，不表示运行过的DAQ数据或现场磁盘已验收。

高流量性能必须另外使用与目标 CoBo/MuTaNT 固件兼容的真实源，在目标网卡和磁盘上测量丢帧、封存、CPU、内存、慢客户端和断线行为。合成测试的总量仍不得超过用户规定的 20 GiB，测试后删除测试数据；任何 GUI/Qt 性能测量都不能替代原生接收路径验收。
