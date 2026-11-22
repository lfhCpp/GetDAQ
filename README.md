# GetDAQ 1.0

GetDAQ 是用于 GET / AT-TPC 数据采集的图形化控制软件，集成原版 SOAP ECC、DataExporter / DataRouter、配置分发与波形查看。

[English installation guide](README.en.md)

## Ubuntu 安装

### 1. 安装编译工具和 Qt 5

```bash
sudo apt update
sudo apt install -y git build-essential cmake python3 qtbase5-dev libqt5svg5-dev qttools5-dev-tools openssl
```

必需的 GET、Ice、Boost、CompoundConfig、Qwt 等源码已内置在 `third_party/`，无需另装旧版 GET，也无需 `source/` 目录；构建依赖不在编译期间下载。

### 2. 获取源码

```bash
git clone --branch main https://github.com/lfhCpp/GetDAQ.git GetDAQ-src
cd GetDAQ-src
```

### 3. 编译并安装

```bash
python3 tools/verify-product-boundary.py
python3 tools/verify-original-backends.py
python3 tools/build-suite.py --profile full --jobs 2 --stage "$HOME/GetDAQ-1.0"
python3 tools/build-suite.py --verify-stage "$HOME/GetDAQ-1.0"
```

安装目录必须不存在；如已有同名目录，请换一个新目录。内存不足时将 `--jobs 2` 改为 `--jobs 1`。

### 4. 启动

```bash
cd "$HOME/GetDAQ-1.0"
./start-getdaq.sh
```

`bin/` 保存 GUI 和后台程序，`config/` 携带项目的 26 个配置及配套文件，`docs/` 保存说明书。启动器自动定位程序，无需修改可执行文件路径。

首次使用时，在软件中设置本机 IP、端口和项目数据目录，并核对 `config/` 中的 CoBo 地址、板卡拓扑和实验配置与实物一致。主控机将此目录选作配置源；从控机选择自己的数据目录并配对，由主控分发配置。已有 ECC / DataRouter 可直接连接。完整操作说明见软件 Configuration 菜单中的中英 Guide，或 [中文 PDF](docs/manual/GetDAQ-Quick-Guide.pdf)。

## macOS 安装

必须在 Mac 上编译，Ubuntu 的可执行文件不能直接运行。安装 Xcode Command Line Tools；已有 Homebrew 时安装构建依赖：

```bash
xcode-select --install
brew install git cmake python qt@5 openssl@3
git clone --branch main https://github.com/lfhCpp/GetDAQ.git GetDAQ-src
cd GetDAQ-src
./build-macos.command --jobs 2
open dist-macos/GetDAQ.app
```

`dist-macos/config/` 包含这 26 个配置文件；首次设置时将其选作配置源，并填写 Mac 自己的网络和数据目录。不要复制 Ubuntu 的 `site/` 状态。输出目录需不存在；再次构建可使用 `./build-macos.command --output dist-macos-new --jobs 2`。独立 Qt 5 安装可通过 `--qmake /path/to/qmake` 指定。

macOS 适配已提供，但实际 Mac 编译、启动和实物采集仍需目标机验证。详细说明见 [macOS 安装文档](docs/MACOS_INSTALL.md)。
