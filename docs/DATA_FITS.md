# `.graw` 到 FITS 离线导出工具

`getdaq-fits` 把受支持的 CoBo MFM `.graw` 事件帧转换成旧 `CoBoFrameViewer` 分析链使用的 FITS 组织形式：每个事件一个压缩 FITS 文件，内部每个 AGET/通道是一张 `GET-EVTS-CHAN` binary table；另生成一个 `GROUPING` 索引文件。该工具只读输入，独立于控制界面和高吞吐接收器，不会打开网络连接或参与采集。

这是旧 `cobo-frame-graw2fits` 功能的安全替代路径，不是完整 CCfits 兼容声明。现阶段输入限 CoBo type-1 revision 0–5，以及完整 4×68×512 读出的 compact type-2 revision 5。compact type-2按每个AGET自身的到达顺序恢复通道和桶号；缺失或越界读出会拒绝。不支持ROOT格式、ROOT显示、拟合、完整旧 Viewer 功能或未知 MFM 子类型的解释。测试以混合type-1/type-2合成帧验证索引行、事件/通道元数据、样本桶和值、FITS校验和及失败清理。实验组尚未提供真实 `.graw` 与旧转换器输出作逐字段差分，投入正式分析前必须用代表性实验文件核对旧工具结果和下游消费者。

## 依赖与构建

CFITSIO 是唯一额外库，且仅在显式启用本选项时查找。默认构建、DAQ接收器、控制端和Qt界面不链接CFITSIO。当前主机检测到系统CFITSIO 3.47；项目不把它复制进采集核心，也不把系统版本视作跨平台已验证版本。

```sh
sudo apt install cmake g++ pkg-config libcfitsio-dev
cmake -S apps/datareceiver -B build-receiver-fits \
  -DCMAKE_BUILD_TYPE=Release \
  -DGETDAQ_BUILD_OPTIONAL_FITS_TOOLS=ON \
  -DBUILD_TESTING=ON
cmake --build build-receiver-fits --parallel 2 --target getdaq-fits fits-tests
(cd build-receiver-fits && ctest -R '^fits-conversion$' --output-on-failure)
```

Windows MinGW、macOS及其他发行版需要相同平台可用的 CFITSIO 开发包与 `pkg-config` 元数据；这些平台本轮没有构建验证。不要为了让默认接收器可构建而安装或启用该依赖。

## 运行

输出目录必须是目标父目录下尚不存在的新目录。程序在该目录旁创建私有暂存目录；全部转换、校验和和落盘同步成功后，才原子发布整个结果目录。已有目录、符号链接路径、同一事件文件名冲突、坏帧和超过容量上限都会使转换失败，已有内容不会被覆盖。失败时只清理本次创建的暂存文件。

```sh
./build-receiver-fits/getdaq-fits \
  --input run.graw \
  --output-dir run-fits \
  --basename run \
  --start 0 \
  --count 100 \
  --max-output-bytes 21474836480
```

`--start` 跳过指定数量的已识别 CoBo type-1/type-2 帧，`--count` 限制输出的事件数。compact type-2必须包含4个AGET各自完整的68×512样本；不完整读出会明确报错。默认 `--count` 不限帧数，但输出总量硬上限默认为20GiB。只在磁盘有足够余量时运行，单次测试/转换保持低于用户指定的20GiB限制。

## 文件结构和索引

例如 `--basename run` 会生成：

```text
run-fits/
  run_index.fits.gz
  run_CoBo1_AsAd2_EVT42.fits.gz
  run_CoBo1_AsAd2_EVT43.fits.gz
```

事件文件包含空 primary HDU，之后每个通道一张 `GET-EVTS-CHAN` binary table，列为16位无符号 `BUCKET` 和 `SAMPLE`。HDU关键字保存事件号、CoBo、AsAd、AGET、channel、分组索引URI和扩展版本。索引文件的 `GROUPING` 表按通道各一行，保存事件/板卡/通道标识、扩展名/版本/位置及对应事件文件名。事件与索引 HDU 写入 FITS checksum。

解析沿用共享 MFM 流解析器，因此只在完整帧边界上处理输入；样本字段按网络字节序解出，写入 FITS 标准列。工具拒绝超出安全边界的采样通道、损坏/截断帧、重复事件文件名和超出索引行上限的输入。文件总量在每个事件后及发布前复核，默认不能超过20GiB。

## 验证边界

`fits-conversion` 测试生成并删除混合type-1/type-2合成raw文件，检查事件分组索引、事件文件位置、type-1与compact type-2通道桶号/样本读回、primary/channel checksum、帧数限制、输出容量限制、截断和不完整type-2读出拒绝、已有输出拒绝及失败无发布。测试输入约280KiB，远低于20GiB预算，并在测试结束时删除。本测试不构成真实实验样本兼容性、长时间转换性能或跨平台部署验收。完整记录见[验证71.159](VALIDATION.md#71159-fits导出支持compact-type-2帧2026-10-21)。
