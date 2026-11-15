# `.graw` 到 ROOT 离线转换

`getdaq-root` 是旧 `graw2root` 的可选离线转换器。它读取已存在的 MFM/CoBo `.graw` 文件，输出旧 CoBoFrameViewer 约定的 ROOT 对象树。它不监听网络、不接入 DAQ、不依赖 Qt，也不会在采集运行时处理事件。实时接收仍由独立原生 `getdaq-receiver` 执行。

默认模式保留旧的 `tree` TTree 和 `GDataFrame` 对象分支；这是为了让旧 `root2disp` 等通过对象分支读取器继续识别文件。`--t2k` 模式保留旧的 `TT2K` 树及 `T2KEvent` 对象分支，布局为 272 个通道、每通道 512 个桶。缺失采样桶写成 0。测试会重新打开输出文件，检查树名、分支名、分支类名、事件头、样本对象及 T2K 的零填充。

## 构建与依赖

ROOT 是体积较大的外部分析框架，因此只在显式打开选项后才查找和链接。默认接收器、GUI、控制器、FITS 和其他原生工具都不依赖 ROOT。本项目为ROOT转换器target显式设置C++17，以匹配本机ROOT 6.26构建参数；DAQ核心库仍按C++11构建。其他ROOT发行构建的ABI须与其自身编译器设置匹配。需要 ROOT 6 的 `Core`、`RIO` 和 `Tree` 组件。

在 Ubuntu 上准备 ROOT 6 开发环境后，使用 ROOT 安装的 CMake 配置目录构建：

```sh
cmake -S apps/datareceiver -B build-root \
  -DCMAKE_BUILD_TYPE=Release \
  -DGETDAQ_BUILD_OPTIONAL_ROOT_TOOLS=ON \
  -DBUILD_TESTING=ON \
  -DCMAKE_PREFIX_PATH=/path/to/root
cmake --build build-root --parallel 2 --target getdaq-root root-tests
(cd build-root && ctest -R '^root-conversion$' --output-on-failure)
```

本次已在 ROOT 6.26/16 与 GCC 9.4 环境构建并测试。转换器使用 POSIX 文件暂存/发布接口，目前仅面向 Linux/macOS 类 UNIX 构建；Windows ROOT 及 macOS 原生编译尚未验证。ROOT 版本、编译器 ABI 和运行库必须与使用者安装相匹配；本机构建不能直接作为跨平台交付包。

旧 CoBoFrameViewer 的 ROOT 数据模型源码连同 `COPYING` 保存在 `third_party/coboframe-root-model/`，因为输出对象类名、成员结构和字典必须与既有消费者保持一致。ROOT 字典由构建时的 `rootcling` 生成，不提交生成物。模型源码带有旧许可条款，分发和修改时应保留对应许可文件。

## 使用

```sh
./build-root/getdaq-root --input run.graw --output run.root
./build-root/getdaq-root --input run.graw --output run-t2k.root --t2k
./build-root/getdaq-root --input run.graw --output selected.root \
  --start 100 --count 500 --config run.xcfg \
  --max-output-bytes 21474836480
```

未指定输出名时，程序在当前目录使用输入文件名（去掉扩展名）加 `.root`。`--start` 和 `--count` 按所有 MFM 帧计数，包含转换器会跳过的非 CoBo 帧；这与旧工具基于帧偏移/帧数的含义一致。配置文件以 `TObjString` 加到所选 TTree 的 `UserInfo` 中，单文件最大 8 MiB。

输入只读。输出必须是尚不存在的路径；工具在目标旁新建私有暂存目录，完成后才以不覆盖方式发布。默认输出上限 20 GiB；磁盘空间较少时应显式设置更低上限。失败时只清理由本次调用创建的暂存内容，不删除或覆盖输入及已有输出。

## ROOT事件查看器

`getdaq-root-viewer` 是一个独立的可选 Qt Widgets 程序，使用ROOT只读 `tree/GDataFrame` 或 `TT2K/T2KEvent`，不加载 `.graw`，也不会启动网络接收。需要ROOT 6与Qt 5.12以上；数据核心和普通Qt控制软件仍不依赖ROOT。逐事件浏览只读取当前选择的ROOT树entry；统计视图扫描整个树两遍，第一遍确定连续变量范围，第二遍累计直方图，扫描可取消且只保留一个事件与固定大小的计数数组。统计扫描在独立的Qt工作线程中执行，工作线程自行打开ROOT文件并调用`ROOT::EnableThreadSafety()`；窗口用定时器读取原子进度值，取消时设置原子标志，worker在当前entry结束后停止。每次只保留一个解码事件和固定大小计数数组。逐事件导航仍在界面线程读取当前entry，单个超大entry的解码可能短暂影响窗口响应。

在上面的CMake配置基础上再打开查看器选项：

```sh
cmake -S apps/datareceiver -B build-root \
  -DGETDAQ_BUILD_OPTIONAL_ROOT_VIEWER=ON \
  -DBUILD_TESTING=ON \
  -DCMAKE_PREFIX_PATH=/path/to/root
cmake --build build-root --parallel 2 --target getdaq-root-viewer root-viewer-tests root-statistics-tests root-statistics-scan-tests
(cd build-root && ctest -R '^root-(viewer-reader|statistics|statistics-scan)$' --output-on-failure)
QT_QPA_PLATFORM=offscreen ./build-root/getdaq-root-viewer --surface-smoke
./build-root/getdaq-root-viewer
```

程序提供打开文件、上一事件/下一事件、entry跳转和显示模式切换。`Channel waveforms` 在四个AGET面板分别叠加当前事件中出现的通道；`Bucket × channel heatmap` 按桶与通道画样本强度；`3D surface` 把每个AGET的完整68×512样本网格投影成带颜色的曲面，并允许按住鼠标左键拖动旋转。T2K树按`channel=AGET*68+chan`解开为四组AGET、每组68通道、每通道512桶；矩阵中的0值也会绘出，以保留旧转换器对缺失桶的零填充。T2K树没有`GDataFrame`头字段，因此界面以矩阵entry和固定通道/桶尺寸标识。五种旧统计字段也已提供Qt直方图：事件时间按1e8 ticks/s换算为秒；事件频率按相邻事件时间差换算为Hz（时间差为0时频率为0）；事件周期按相邻差值换算为ms；桶分布统计所有样本对象；每事件通道数统计非空通道。统计视图采用线性坐标和1000连续变量bins（桶/通道数使用512/273个整数bins），保留统计量定义，不复制旧ROOT Canvas的log轴外观；这些统计仅适用于具有对应头字段和样本对象的`GDataFrame`树，打开T2K树时会禁用。状态栏显示扫描entry数；取消或读取失败会恢复已选事件。界面是Qt实现，不要求旧ROOT GUI组件。

### 与旧 `root2disp` 显示选项的对应关系

旧命令列出`time`、`freq`、`period`、`bucket`、`data`、`nchannels`六个字段；其中只有五项是统计量。`data`是默认事件浏览器：每个entry显示四片AGET各自的通道波形，对应当前的`Channel waveforms`。旧浏览器的3D复选框在`CoBoFrameBrowser::DrawFrame3D()`中创建`TH2F`并以`COLZ`绘制桶×通道色图，并非可旋转的3D曲面；当前`Bucket × channel heatmap`对应这条色图路径。拟合相关代码在旧ROOT浏览器中被注释，没有把未启用的拟合功能列为已迁移要求。

另一个CoBoFrameViewer组件有真正可选的QwtPlot3D曲面显示，只有旧构建探测到QwtPlot3D时才编译启用；当前ROOT查看器已用Qt Widgets/QPainter实现可旋转的三维投影曲面，不需要QwtPlot3D或Qt OpenGL，但不复制旧库的材质、相机和绘图外观。它使用完整512桶数据网格，属于离线显示，不进入DAQ传输路径。查看器也带有单独编译的ROOT5时代`get::GDataFrame`类字典及读取适配器；与ROOT6的`GET::`新类字典隔离，读入后转换为只读内存事件结构。合成测试由ROOT6产生同名/同版本lowercase对象树并验证头字段和样本引用，不等同于真实ROOT5文件兼容证明；文件Schema演进、旧ROOT Canvas精确样式、Windows/macOS ROOT及现场用户文件仍待验收。此查看器仍不能替代所有ROOT宏和分析脚本。

## 输入范围与结构

当前接受 CoBo type-1 revision 0–5，以及紧凑 CoBo type-2 revision 5。未知帧仍由共享 MFM parser 检查完整边界；非 CoBo 帧跳过。type-1 每个4字节数据项按固定网络字节序解出 AGET、通道、桶和12位样本；type-2 每个2字节数据项按 AGET 及顺序通道/桶解码。每帧最多4×68×512个样本。头字段按 MFM 头部字节序读取，旧对象的 `GFrameHeader` 和 `GDataFrame/GDataChannel/GDataSample` 类名保持兼容。

旧对象树的含义如下，供排查下游宏和分析脚本：

| ROOT位置 | 旧类/数据 | 内容 |
| --- | --- | --- |
| `tree` 的每个 entry | `GET::GDataFrame` | 一个CoBo事件帧及其头字段、通道集合和样本集合 |
| `GDataFrame.fHeader` | `GET::GFrameHeader` | revision、source、48位事件时间、事件号、CoBo/AsAd、read offset、status、四个AGET命中位图与multiplicity；rev3起有windowOut，rev4起有lastCell |
| `GDataFrame.fChannels` | `GET::GDataChannel` | 每个出现过的AGET/通道的身份，以及指向其样本的ROOT引用 |
| `GDataFrame.fSamples` | `GET::GDataSample` | 桶号0–511和12位样本值；channel中的引用指向此集合对象 |
| `TT2K` 的每个 entry | `T2KEvent` | 按 `AGET*68+channel` 展平的272×512有符号整型矩阵 |

因为旧的 `GDataChannel` 使用 `TRefArray` 指向 `GDataFrame` 所有的 `GDataSample`，只把列改成普通数组虽然看起来更简单，却会改变旧树的对象关系；本实现保留旧模型类和ROOT字典。T2K模式则把桶号位置写成ADC值，未出现的采样点为0。

目前有合成数据的结构兼容回归，但还没有用实验室真实 `.graw` 与旧 `graw2root` 输出逐字段比较，也没有在现场旧 `root2disp` 中验收。不能据此宣称所有历史 ROOT 文件、所有旧格式描述、ROOT 宏或分析工作流完全兼容。正式分析前应拿代表性原始文件与旧转换结果差分，并在实际消费者中打开结果。

## 验证边界

`root-conversion` 用小型合成帧检查旧对象树、T2K 树、元数据/样本读回、缺失桶零值、输出容量、拒绝覆盖和截断帧拒绝。`root-viewer-reader` 分别写入小型`tree/GDataFrame`与`TT2K/T2KEvent`文件后重开，核对头字段/矩阵维度、channel/sample读回、T2K零填充、entry越界、错误矩阵尺寸和缺失文件拒绝；统计公式与文件扫描也有单独测试。Qt查看器另做offscreen启动检查。转换夹具合成raw小于1 KiB并清理；viewer测试只创建临时ROOT文件并删除。测试不代表实验数据正确性、旧分析程序端到端兼容、性能或目标平台验收。
