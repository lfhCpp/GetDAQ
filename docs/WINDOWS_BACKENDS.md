# Native Windows original backends / Windows 原版后台

Windows can build and run the bundled original `getEccSoapServer.exe`,
`DataExporter.exe`, and `dataRouter.exe`. They are native Windows programs;
WSL and a Linux computer are not required at runtime. The read-only
`getdaq-cobo-info.exe` is also built. Automatic Master / Slave deployment
management remains a Linux feature; Windows services are launched explicitly.

## Build / 构建

Install Python 3, CMake 3.16 or newer, and the MinGW compiler matching the Qt kit.
Qt 5.14.2 / MinGW 7.3.0 x86_64 is the validated compiler combination. No separately
installed GET, Ice, Boost, gSOAP or CompoundConfig library is required; the
project contains the source dependencies. CMake and Python are build tools.

安装 Python 3、CMake 3.16 或更高版本，以及与 Qt Kit 配套的 MinGW。把 CMake 加入 PATH。
后台的源码依赖已内置，不需要另外安装 Boost、Ice、gSOAP 或 CompoundConfig。

In a Windows terminal, from the project root:

```bat
python tools\build-windows-backends.py --compiler-bin "D:\Software\QT\Tools\mingw730_64\bin" --jobs 4
```

If CMake is not on PATH, add `--cmake "C:\path\to\cmake.exe"`.
The output goes into `bin`; `--bin-dir` selects another output folder.
The matching compiler runtime DLLs are copied alongside the executables.

Qt Creator can build these services alongside the GUI: add
`GETDAQ_BUILD_BACKENDS=1` to the qmake arguments, then **Run qmake** and build.
`GETDAQ_CMAKE=C:/path/to/cmake.exe` and `GETDAQ_PYTHON=C:/path/to/python.exe`
select explicit build tools. The default GUI-only build stays unchanged.

也可以在 Qt Creator 的 qmake 参数添加 `GETDAQ_BUILD_BACKENDS=1`，重新运行 qmake 后构建。
默认不勾入这几个后台，避免只编译 GUI 时额外要求 CMake。原始硬件文件保持原样；
Windows 适配发生在构建目录，包含套接字、网卡枚举、路径处理与 64 位整数修正。

## One project folder / 一个项目目录

Create a project data folder, then copy the complete configuration folder into
it. Do not copy only `CoboFormats.xcfg`: it includes other format files.

```bat
mkdir C:\GetDAQ-data
copy config\* C:\GetDAQ-data\
scripts\start-windows-backends.cmd "C:\GetDAQ-data" 192.168.40.10 192.168.50.10
```

Replace both IPs with addresses belonging to the Windows computer:

| Setting | Meaning / 含义 |
|---|---|
| `192.168.40.10` | Receiver control-interface IP / 接收电脑的控制网卡 IP |
| `192.168.50.10` | Data-interface IP / 接收电脑的数据网卡 IP |
| `C:\GetDAQ-data` | Both programs' working folder, XML/XCFG repository and incoming raw-data folder / 两个后台的工作目录、配置目录和原始数据接收目录 |

The launcher opens separate ECC and DataExporter consoles. No acquisition
hardware commands are sent merely by starting these services. Configure the
CoBo address and experiment settings in the XML/XCFG files as on Linux.

若不用启动脚本，可以分别打开两个终端，进入同一项目目录后执行：

```bat
cd /d C:\GetDAQ-data
C:\path\to\GetDAQ\bin\DataExporter.exe 192.168.40.10:46003 192.168.50.10:46005 --export-port 46491
```

```bat
cd /d C:\GetDAQ-data
C:\path\to\GetDAQ\bin\getEccSoapServer.exe --port 8083 --alarm-port 46011 --config-repo-url .
```

`dataRouter.exe 192.168.40.10:46003 192.168.50.10:46005 TCP FrameStorage`
can be used instead of DataExporter, with `CoboFormats.xcfg` and all its included
files present in the working folder. It does not provide DataExporter's preview
forwarding port.

## Connect the GUI / 主界面连接

In **Nodes / Paths**, set ECC host to the Windows control IP and ECC port to
`8083`. Set Router host to the Windows data IP and Router port to `46005`.
For online waveform viewing, DataExporter's preview endpoint uses port `46491`.
Port `46003` is the receiver's Ice control port; it is not the board data port.
Sentry is a separate program and is not started by this two-service launcher.

先在后台控制台确认启动成功，再从 GUI 连接 ECC。CoBo 的 IP 仍由 ECC 的 XML/XCFG
指定，与接收电脑的控制 IP、数据 IP 分开。多个实例应使用不同的端口和独立项目目录。
停止采集后，在各后台控制台使用 **Ctrl+Break** 请求正常退出。

Default ports can be overridden before calling the launcher:
`GETDAQ_ECC_PORT`, `GETDAQ_ALARM_PORT`, `GETDAQ_ROUTER_CONTROL_PORT`,
`GETDAQ_DATA_PORT`, and `GETDAQ_PREVIEW_PORT`.

## Validation and scope / 验证范围

Native Windows loopback tests cover SOAP Idle/exit, exact synthetic MFM frame
storage in both receivers, DataExporter preview forwarding, and graceful
Ctrl+Break shutdown. They do not establish physical CoBo acquisition acceptance
or high-volume throughput on an independent Windows computer. Windows data
filenames replace timestamp colons with hyphens; the raw frame contents remain
unchanged. The POSIX timer-based ByteCounter/FrameCounter/Debug diagnostic modes
are not part of this Windows backend build.

Windows 本机模拟数据收发测试通过不等于实物已经验证；实板启动、停止和长时间高流量
采集仍需在目标电脑上验收。建议首次验证使用不含中文字符的项目路径；中文路径尚未验证。
