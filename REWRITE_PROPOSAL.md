# GetDAQ C++ 重写方案（按 MUSIC 框架修订）

> 历史方案说明：用户已将范围扩大为必要的主机端软件全套重构；板端 `getHwServer` / `IceEirfu` 保留现有部署。下文保留最初 envoy 方案，其中“仅替换控制台”等限制已失效。当前实现、验证和未完成项以 [重构进度](docs/REFACTOR_STATUS.md) 为准，原始源码审阅与分阶段目标见 [GET 源码与资料审阅方案](docs/SOURCE_REVIEW_AND_PLAN.md)。

用户已确认开始实现。范围：仅替换 envoy 控制台，Ubuntu 24.04 优先，兼顾 macOS；Windows 检查环境已明确为 **Qt 5.14.2 + MinGW**。MUSIC 的代码风格和框架优先，Geant4 次之；使用 C++11。第一版工程、配置和测试已经落地，使用方法见 `README.md`，验证结果见 `docs/VALIDATION.md`。

## 源码基线与范围

- 上游：https://github.com/ATTPC/attpc_envoy
- 已检查提交：`d877924caa68cb14b448cd5cfd26b555beafb5fd`。
- 原程序使用 Rust、egui、Tokio，负责分布式 DAQ 控制、监控与运行记录。
- ECC 使用 HTTP/SOAP/XML，sentry 使用 HTTP/JSON；原始事件传输由外部 dataRouter/DataExporter 承担。
- 源码默认配置为 11 个 CoBo 和 1 个 MuTaNT；这是上游默认值，不代表用户实际设备配置。

第一版完整替换 envoy 的功能，兼容现有 getECCServer、attpc_sentry 及采集服务。原始数据接收、转发和落盘服务不在本次重写范围。MUSIC 作为代码风格和框架参考保留，不直接将其 FPGA 二进制命令移植为 ECC 控制协议。

## 技术选型

| 部分 | 建议 | 理由 |
| --- | --- | --- |
| 语言与构建 | 优先 C++11、qmake `.pro` | 沿用 MUSIC 的 Qt Creator 工作方式；显式设置语言标准 |
| 桌面界面 | Qt 5 Widgets、Designer `.ui` | 使用 Qt 5.12 起的 API，兼容用户 Qt 5.14.2；Ubuntu 24.04 提供 Qt 5.15 开发包 |
| 控制通信 | Qt Network 异步适配层 | 现有通信为 HTTP 请求，便于处理请求完成、错误与取消 |
| 核心逻辑 | 独立 QObject 控制类，可使用 Qt Core 类型 | 不依赖窗口；沿用 QString、QVector、信号槽等熟悉接口 |
| 配置 | 兼容现有 YAML；解析库待构建环境确定 | 保留实验、Run 编号、说明和自定义字段 |
| 图表 | 独立 WidgetPlot2D，使用 QPainter | 显示有界低频速率历史；无需另外安装绘图库 |

业务层与界面解耦即可，不再要求所有业务对象排除 Qt 类型。Qt 6 要求 C++17，因此严格 C++11 与 Qt 6 不能作为同一套构建要求。可选路线是 Qt 6 + C++17 编译，但业务代码仍使用简单的 C++11 风格，不引入协程或复杂模板；该路线为建议选项，尚未视为用户接受升级。MUSIC.pro 未显式声明 C++ 标准，不能据此断言其实际构建使用 C++11。

macOS 兼容以源码移植为目标：使用 Qt 的路径、文件、时间和网络接口，避免写死目录及 Linux 专有调用；具体 Qt 版本、macOS 最低版本、Apple Silicon/Intel 架构及应用打包必须实际验证。Ubuntu 构建通过不等于 macOS 已验证。第一版以 `.pro` 为主，不同时维护两套构建文件；需要自动化构建时再评估 CMake。

Qt 信号槽存在额外开销，但不能由此推断采集吞吐量明显下降。原始数据不经过控制台界面线程；界面刷新与状态查询独立调度。若增加高速数据模块，采用独立工作线程、批量处理、有界缓冲和汇总统计，并按目标流量测量吞吐、延迟和丢失情况。C++ 重写本身不保证比 Rust 更快。

## 模块与线程边界

1. `MainWindow`：主窗口、按钮、状态表与日志展示；`.ui` 负责布局。
2. `IPWindow` / `ConfigWindow`：编辑节点地址、目录与运行参数。
3. `RunControl`：管理运行上下文、Run 编号、阶段及错误；简单 enum + switch 推进状态，不引入额外状态机框架。
4. `EccClient`：管理 ECC SOAP/XML 请求、响应和超时。
5. `SentryClient`：管理 sentry HTTP/JSON 查询及归档请求。
6. `RunConfig`：加载、校验和保存实验参数与设备配置。
7. `RunRecorder`：备份 GET 配置，保存运行记录及 CSV 输出。
8. `WidgetPlot2D`：显示速率及有界历史数据；不直接访问网络对象。
9. 模拟服务：支持无硬件演示、协议场景和故障测试。

GUI 在主线程运行；RunControl 和客户端采用 MUSIC 中 Analyse 已使用的 QObject + moveToThread 方式，在一个控制工作线程处理全部节点。网络对象、定时器在所属工作线程初始化、使用和销毁。后台文件任务通过完成信号返回结果。避免在 GUI 中等待网络、备份或设备状态，也无需为每个节点建立一个线程。

同一设备的状态修改命令顺序执行；独立设备在阶段允许时并行。轮询请求不无限积压。状态快照可合并，控制结果和关键错误必须可靠处理。

## MUSIC 优先的风格约定

- `.h` / `.cpp` / `.ui` / `.qrc`，小写文件名，类名如 MainWindow、RunControl。
- 方法如 `InitializePar()`、`StartRun()`；成员如 `fRunControl`、`fRunNumber`。
- 界面槽沿用 `on_StartRun_clicked()`，业务槽可用 `StartRunSlot()`，状态通过 signals 返回。
- 保留简明中文注释、熟悉的初始化函数和必要的 Geant4 式分隔注释。
- 优先使用 MUSIC 中已经存在的函数指针 connect 写法，以便编译时检查；同一按钮不同时使用自动连接和手工连接。
- Qt 对象明确父子所有权；工作线程对象在正确线程清理。普通资源使用 RAII；不堆叠全局单例和相互访问窗口的 GetInstance 调用。
- 不引入 Geant4 运行库依赖、复杂继承树或无实际用途的抽象层。

建议初期保持 MUSIC 熟悉的平铺工程：GetDAQ.pro、main.cpp、mainwindow.*、ipwindow.*、configwindow.*、runcontrol.*、eccclient.*、sentryclient.*、runconfig.*、runrecorder.*、widgetplot2d.*；配置、测试和第三方文件放在独立子目录。

## MUSIC 源码阅读发现

本轮为静态阅读，未编译或压测 MUSIC；以下用于指导新实现，不改动参考工程。

- mainwindow.cpp 的 InitializePar 已将 Analyse 移入 QThread；新控制逻辑可沿用这个容易理解的模式。
- SaveThread::run 仅调用 SaveThreadStart 后返回，没有长期运行的事件循环；给 QThread 子类增加槽不会让槽自动在新线程执行。Multithread 中也存在类似初始化后返回的 run。
- SaveThreadStart 从工作线程读取窗口状态，SaveSlot 通过另一对象获取 socket 和文件指针；新实现以消息传递和单一所有者避免这种线程边界混用。
- WidgetPlot2D 析构函数删除 fWidgetPlot2D，而该静态指针在构造时被赋为 this；存在递归删除自身的问题。窗口关闭和线程退出必须建立清晰的销毁顺序。
- 多处 broad disconnect/connect 以及反复点击时增加连接，使运行行为难追踪；建议初始化时固定连接，以状态决定允许操作。
- 本次控制台不承担原始数据落盘，不复用 MUSIC 的大缓冲、SaveThread 数据路径或逐包绘图处理。

## 功能兼容与改进

| 功能 | 重写要求 |
| --- | --- |
| 设备连接 | 显示实际通信状态；请求可超时、取消，断线后状态不得继续显示为可信在线 |
| ECC 操作 | 对齐 Describe、Prepare、Configure、Start、Stop、Undo、Breakup 及状态查询 |
| 全局配置 | 保留 Prepare 阶段 MuTaNT 优先、Configure 阶段 CoBo 优先的依赖顺序，并以确认状态推进 |
| 启动运行 | 按原流程重配置 MuTaNT，再确认 CoBo 运行，最后启动并确认 MuTaNT |
| 停止运行 | 先确认 MuTaNT 停止，再确认 CoBo 停止；确认具备归档条件后请求 sentry |
| 数据归档 | 保持 sentry 接口兼容；需联调确认接口成功是否足以证明归档完成及写入已结束 |
| 备份与记录 | 备份 GET 配置，记录运行参数、时长及各阶段结果；失败不能报告为全部成功 |
| Run 编号 | 检查冲突；失败运行保留记录；编号推进和恢复策略在行为规格中明确 |
| 配置兼容 | 加载既有 YAML，保留自定义字符串字段；设备地址和目录独立配置化 |
| 监控 | 磁盘、进程、文件数、写入量和速率；使用单调时钟实际间隔计算速率并处理计数重置 |
| 日志与界面 | 全局及单设备操作、阶段进度、错误定位、速率曲线、配置加载保存 |

命令发送成功不等于设备完成转换。任何状态修改命令超时后，先查询实际状态，再决定恢复动作；不得盲目重发。保留原协议行为，修正阻塞忙等、错误后无条件继续、归档状态判断不足等问题；行为调整需在规格与测试中明确。

## 实施与验收

1. 固定上游版本，逐项建立功能清单、请求/响应样例及状态转换表；核对数值状态映射，避免机械复制原实现的不一致。
2. 建立核心和模拟后端，验证完整启停、顺序依赖、请求取消、状态超时和部分设备失败。
3. 完成 Qt 通信、Designer 界面、YAML 兼容、运行日志及备份，提供能用 Qt Creator 打开的 `.pro` 工程及命令行构建说明。
4. 使用模拟 HTTP 服务验证 SOAP/XML 与 JSON 请求及异常响应；验证旧配置导入、CSV 转义和备份失败处理。
5. 在实际 DAQ 环境逐项验证设备配置、启动、停止、归档、备份及断线恢复。模拟测试不能替代硬件验收。
6. 进行长时间运行检查，记录 CPU、内存、界面响应和命令完成延迟；验证反复连接、启停及关闭时的资源清理。Ubuntu 24.04 为首要验收平台，macOS 验证单独记录。

## 已确定与尚待落实

本节为最初实施阶段的决定，后续授权已扩展到必要主机配套程序，当前范围以[重构进度](docs/REFACTOR_STATUS.md)为准。最初已确定：先替换envoy；Ubuntu 24.04 优先、兼顾 macOS；MUSIC 框架和风格优先、Geant4 次之；采用熟悉的 C++ 写法。

实现基线：Qt 5.12+ + C++11 + qmake + Widgets/Designer；Windows 对齐 Qt 5.14.2/MinGW。Qt 6 不属于本版验收目标。真实服务地址、GET 配置目录、可用硬件及 macOS 版本尚未提供；这些信息不妨碍模拟版本开发，但限制实机和 macOS 验收。

## 参考

- [上游 ECC 客户端](https://github.com/ATTPC/attpc_envoy/blob/d877924caa68cb14b448cd5cfd26b555beafb5fd/src/envoy/ecc_envoy.rs)
- [上游 sentry 客户端](https://github.com/ATTPC/attpc_envoy/blob/d877924caa68cb14b448cd5cfd26b555beafb5fd/src/envoy/sentry_envoy.rs)
- [上游状态转换](https://github.com/ATTPC/attpc_envoy/blob/d877924caa68cb14b448cd5cfd26b555beafb5fd/src/envoy/transition.rs)
- [上游运行控制](https://github.com/ATTPC/attpc_envoy/blob/d877924caa68cb14b448cd5cfd26b555beafb5fd/src/ui/app.rs)
- [Qt 异步网络](https://doc.qt.io/qt-6/qnetworkaccessmanager.html)
- [Qt 线程与对象](https://doc.qt.io/qt-6/threads-qobject.html)
- [Qt 信号槽](https://doc.qt.io/qt-6/signalsandslots.html)
- [QThread 对象与工作线程](https://doc.qt.io/qt-6/qthread.html)
- [Qt 6 的 C++ 标准要求](https://doc.qt.io/qt-6/cpp20-overview.html)
- [Ubuntu 24.04 Qt 5.15 开发包](https://packages.ubuntu.com/noble/qtbase5-dev)
- [Geant4 RunAction 示例](https://github.com/Geant4/geant4/blob/master/examples/basic/B1/include/RunAction.hh)
