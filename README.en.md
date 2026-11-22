# GetDAQ 1.0

GetDAQ is a graphical control application for GET / AT-TPC data acquisition, integrating original SOAP ECC, DataExporter / DataRouter, configuration distribution, and waveform viewing.

[中文安装教程](README.md)

## Ubuntu installation

### 1. Install build tools and Qt 5

```bash
sudo apt update
sudo apt install -y git build-essential cmake python3 qtbase5-dev libqt5svg5-dev qttools5-dev-tools openssl
```

Required GET, Ice, Boost, CompoundConfig, Qwt, and other dependency sources are bundled in `third_party/`. No separate legacy GET installation or `source/` directory is required; the build does not download dependencies.

### 2. Get the source

```bash
git clone --branch v1.0 https://github.com/lfhCpp/GetDAQ.git GetDAQ-src
cd GetDAQ-src
```

### 3. Build and install

```bash
python3 tools/verify-product-boundary.py
python3 tools/verify-original-backends.py
python3 tools/build-suite.py --profile full --jobs 2 --stage "$HOME/GetDAQ-1.0"
python3 tools/build-suite.py --verify-stage "$HOME/GetDAQ-1.0"
```

The installation directory must not already exist. Choose a new directory if needed. Use `--jobs 1` on machines with limited memory.

### 4. Launch

```bash
cd "$HOME/GetDAQ-1.0"
./start-getdaq.sh
```

`bin/` contains the GUI and backend programs, `config/` includes the project's 26 configuration and companion files, and `docs/` contains the manuals. The launcher locates executables automatically.

On first use, set this computer's IP addresses, ports, and project data directory. Check the CoBo addresses, board topology, and experiment settings in `config/` against the hardware. On the master, select this folder as the configuration source. On a slave, choose its own data directory and pair it with the master, which distributes configuration. Existing ECC / DataRouter services can be connected directly. For operation instructions, open the bilingual Guide in the software's Configuration menu or read the [English PDF](docs/manual/GetDAQ-Quick-Guide-en.pdf).

## macOS installation

Build on the Mac; Ubuntu executables cannot run on macOS. Install Xcode Command Line Tools. With Homebrew already installed, install the build dependencies:

```bash
xcode-select --install
brew install git cmake python qt@5 openssl@3
git clone --branch v1.0 https://github.com/lfhCpp/GetDAQ.git GetDAQ-src
cd GetDAQ-src
./build-macos.command --jobs 2
open dist-macos/GetDAQ.app
```

`dist-macos/config/` contains the same 26 files. Select it as the configuration source during initial setup and use the Mac's own network and data paths. Do not copy Ubuntu `site/` state. The output directory must not exist; use `./build-macos.command --output dist-macos-new --jobs 2` for another build. Select a standalone Qt 5 kit with `--qmake /path/to/qmake` if needed.

macOS adaptations are provided; native compilation, application launch, and hardware acquisition still require validation on the target Mac. See the [macOS installation notes](docs/MACOS_INSTALL.md).
