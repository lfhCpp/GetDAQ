# CoBoFrameViewer `.gmat` 波形矩阵转换

`getdaq-matrix` 是一个独立的离线工具，用来把选定的 CoBo MFM 波形帧转换成旧 `CoBoFrameViewer` 可直接读取的 `.gmat` 矩阵。它不参与采集、传输、写盘线程或 Qt 界面；输入 `.graw` 只读，转换结果写入一个新文件。

## 为什么要单独重写

旧源码的 Viewer 在 `CoBoFrameViewer::setMatrix()` 和 `CoBoEvent::fromMatrix()` 中按每个 CoBo 帧读取 `4 × 68 × 512` 个 `double`，也就是 **1,114,112 字节/帧**。同一源码树中的旧 `graw2gmat` 却对每个通道直接写512个 `uint16_t`，仅 **278,528 字节/帧**，而且 `Channel::data()` 的向量可能少于512项。文件内容因此与 Viewer 的读取宽度不一致，短通道还可能越界读取。新工具按 Viewer 的读取布局输出，不复制旧写入错误。

## 矩阵字节布局

每帧没有额外文件头、事件号或填充，紧邻上一帧写入：

```text
AGET 0..3
  channel 0..67
    bucket 0..511
      IEEE-754 binary64 little-endian sample
```

输入 type 1 的每项从 CoBo word 解出 AGET、channel、bucket 和12位 ADC sample。bucket编号用于检查完整性；矩阵中每个通道保留该通道样本在原始CoBo帧中的到达顺序。这与旧`Channel::data()`及`graw2gmat`写出的通道向量顺序一致，Viewer随后按矩阵位置将样本编号为bucket 0–511。每一个 AGET/channel 必须恰有512个唯一 bucket；缺项、重复项、越界索引、错误 item 大小或不完整 MFM 帧都会报错。12位整数数值精确转为 double。由于文件格式没有缺样本标记，工具不会填零或静默丢弃坏数据。

当前只接受 CoBo type 1、revision 0–5，并按旧格式描述使用各revision的MFM块编码：revision 0为32字节块，revision 1–4为64字节块，revision 5为256字节块。MFM帧长度和头长度字段按块计数；转换器核对帧末尾的块填充边界。type 2 compact 帧只携带有限的 AGET 标识，不能构成 Viewer 需要的完整四 AGET 矩阵，选中时会明确拒绝。CoBo type 1/2 的其他 revision 若被选中也会明确报错；其他 MFM 类型会跳过。`--start`按type 1/2 CoBo帧编号，跳过的帧不转换。

## 构建与运行

```sh
cmake -S apps/datareceiver -B build-receiver -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-receiver --parallel 4 --target getdaq-matrix

./build-receiver/getdaq-matrix \
  --input run.graw --output run.gmat \
  --start 0 --count 10
```

省略 `--count` 或设为0会转换从起始位置开始的所有剩余 CoBo 帧。默认最大输出为20 GiB，可用 `--max-output-bytes N` 调低；输出空间需求按每帧1,114,112字节增长。目标文件必须不存在，程序不会覆盖文件或跟随目标符号链接；成功前写到同目录临时文件，失败会清理临时文件。开始完整大文件转换前，应先用少量帧检查输出和目标磁盘空间。

查看所有参数：

```sh
./build-receiver/getdaq-matrix --help
```

## 依赖和验证范围

工具使用项目已有的 C++11 MFM 流解析器及系统 C/C++ 库；没有 Qt、Boost、Ice、yaml-cpp、ROOT 或 CFITSIO 运行依赖。原始接收热路径没有调用这个转换器。

`matrix-tests` / `gmat-conversion` 用符合MFM块编码的合成 type 1 CoBo 帧覆盖revision 0–5，检查double小端字节、通道布局及非顺序bucket下的每通道到达顺序、帧选择、已有文件保护、输出字节上限、不完整矩阵、type 2和未知revision拒绝。它证明新写出的布局符合源码中 Viewer 的读取规则；当前没有真实 `.graw` 与旧 Windows Viewer 的联合打开/显示验收，不能据此声称所有历史 CoBo revision 或物理样本都已兼容。
