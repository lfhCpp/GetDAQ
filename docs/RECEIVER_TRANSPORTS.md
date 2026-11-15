# TCP、ZBUF、FDT 输入：选择、操作与兼容边界

本文对应 `apps/datareceiver` 当前实现。接收器继续使用标准 C++11、POSIX socket 和系统线程库，不链接 Qt、Ice、Boost、yaml-cpp 或额外网络库。Qt 操作台处理配置、控制和查看信息；原始事件直接到接收器。

## 1. 先确定板端实际选择的协议

| 输入参数 | 线上字节 | 新接收器如何处理 | 当前适用范围 |
| --- | --- | --- | --- |
| `--input-protocol TCP` | 连续原始 MFM 帧 | 按 MFM 帧头分帧；不识别 FDT 控制字 | 默认模式，既有 ECC/数据管理服务继续使用 |
| `--input-protocol ZBUF` | 与 TCP 相同的连续原始 MFM 帧 | 与 TCP 共用分帧、写盘、导出路径 | 独立及固定计划/ECC受管理模式；实际VxWorks发送端待验，见[管理手册](MANAGED_ZBUF.md) |
| `--input-protocol FDT` | MFM 帧之间可以有 4 字节控制字 | 只在帧边界过滤心跳和 GOODBYE，原始帧保持 | 独立及数据主机多连接接收基础；完整ECC生命周期已有本机验证；现场互通待验 |
| `ICE` | Ice RPC 的 `DataProcessor.processData(ByteSeq)` 调用 | `getdaq-receiver`启动参数明确拒绝；旧GET/MDaq入口由独立`getdaq-ice-receiver`兼容 | 需要旧Ice数据源时使用[可选ICE入口](ICE_DATA_INGRESS.md)，不要发送到原始TCP端口 |

参数值区分大小写。不填参数就是 `TCP`。不进行自动探测：选错协议时应明确失败，不能为了继续写文件而猜测或丢弃未知字节。

这些名称来自收到的本地软件。旧 `DataRouter::createDataReceiver` 对 `TCP` 和 `ZBUF` 都创建 `TcpDataReceiver`，对 `FDT` 创建另一种接收器。`ZBufConnection::trySend` 使用 VxWorks 的 `zbufSockBufSend` 减少板端复制，没有加入新的应用层包头。因此这里的 ZBUF 是已有原始 TCP 字节流兼容入口，**不代表新主机实现了 VxWorks 零复制 API，也不证明实际板端性能**。ICE是另一种输入边界：旧发送端通过Ice RPC提交ByteSeq，不能与普通TCP流混用。独立ICE入口复用同一MFM和存储核心，不会把Ice依赖加入`getdaq-receiver`；固定ECC生命周期和旧固件升级状态见[ICE入口手册](ICE_DATA_INGRESS.md)。

依据文件：

- `source/get/GetBench-software-release-20210208/src/get/daq/DataRouter.cpp`
- `source/get/GetBench-software-release-20210208/src/get/daq/FdtDataReceiver.cpp`
- `source/mordicus/MDaq-release-20210208/src/mdaq/daq/FdtDataSender.hpp`
- `source/mordicus/MDaq-release-20210208/src/mdaq/utl/net/ZBufConnection.cpp`

它们保留为参考，不编译进接收器。本机检查不是旧框架整体运行对照。

## 原样字节存储模式

默认 `--storage-mode frame-storage` 会解析 MFM 帧，按 CoBo/AsAd 或来源分别写入 `.graw`，并检查帧长度、事件号和协议边界。兼容旧 DataRouter `ByteStorage` 用法时，可以显式指定 `--storage-mode byte-storage`：接收器不解析MFM，按收到的顺序把 TCP/ZBUF 字节原样写入 `.graw`，正常EOF后同步文件并生成接收封存记录。

```bash
./build-receiver/getdaq-receiver \
  --output /data/getdaq/byte-storage-run \
  --bind 10.0.0.10 --port 46005 --status-port 46015 \
  --input-protocol TCP --storage-mode byte-storage \
  --rotate-mib 1024 --min-free-mib 4096 --once
```

`--once`要求发送端关闭TCP连接后才结束；命令退出码为0、最终JSON中`stream_sealed=true`且`.getdaq-receiver-seal.json`存在，三项应同时满足。字节模式仍使用固定有界接收队列和独立写盘线程，不经过Qt；磁盘余量门槛、目录独占锁和不覆盖文件名仍生效。轮转在接收块边界执行，因此单个文件可能比`--rotate-mib`稍大一个接收块；各文件按顺序拼接后与输入流完全一致。

`getdaq-receiver`的字节模式接受普通TCP/ZBUF。可选`getdaq-ice-receiver`也能通过`--storage-mode byte-storage`原样保存ICE `processData`载荷；ICE仍要求源端在同一连接发出带确切总字节数的`finishStream`，之后才封存。DataSupervisor固定计划可为来源写入`"storage_mode":"byte-storage"`，并核对运行状态和收据。FDT帧间心跳/GOODBYE需要MFM帧边界识别；把FDT控制字节原样写入归档会改变旧FDT接收器的行为，所以必须使用默认`frame-storage`。`byte-storage`也不能启用面向解析帧的`--export-port`。管理式FDT周期及在线帧导出仍由帧存储模式承载。

旧 `ByteStorage` 不检查MFM完整性，也不按来源/事件分文件；不要将此模式的成功封存解释为帧格式正确或每个事件完整。重构版额外提供准确字节数和可核对的归档收尾记录。入口与限制见[验证记录71.33](VALIDATION.md#7133-datarouter-bytestorage字节存储模式补齐2026-10-20)及[ICE/DataSupervisor集成验证71.34](VALIDATION.md#7134-ice-bytestorage接入datasupervisor固定计划2026-10-20)。

## 2. FDT 的字节排列

旧 FDT sender 发送两种控制字：

| 控制字 | 四个字节 | 含义 |
| --- | --- | --- |
| IMALIVE | `00 00 00 00` | 连接心跳，没有 MFM 数据 |
| GOODBYE | `FF FF FF FF` | 发送端声明准备结束连接 |

这两个字在大小端主机上的字节排列相同。MFM 帧本身仍按各自帧头的字节序解码；控制字不是额外长度前缀。例如：

```text
心跳4字节 → MFM帧A → 心跳4字节 → MFM帧B → GOODBYE4字节 → TCP EOF
```

保存文件只有 `MFM帧A → MFM帧B`。在线导出同样只有完整原始帧，各帧前仍是本项目已有的 little-endian uint64 长度。不要把 FDT 输入模式理解成下游导出也改成 FDT。

分帧器知道当前是在帧边界还是在帧内部，所以帧的波形载荷包含四个 `00` 或四个 `FF` 时完全保留。TCP 可以把一个控制字分成 1+3、2+2 或任意分段，也可以把控制字和多帧合并；实现不依赖一次 recv 的边界。

未完整到达的控制字在 EOF 时是错误。非法 MFM 长度、坏头或未完整到达的帧也仍然是错误，不会寻找下一个疑似头重新同步。发生错误时尽量保留此前完整帧，退出码为 1，不生成正常 seal。

## 3. GOODBYE 之后为什么继续等待

GOODBYE 只是应用层四字节声明。收到它后，接收器仍等待该 socket 的真实 EOF，期间不生成成功 seal。GOODBYE 后若还有任何字节，包括另一个心跳或 GOODBYE，明确失败。

正常结束的顺序是：发送端关闭连接 → 实际 TCP EOF → 完整帧核对 → 排空/同步文件 → 接收器正常退出 → 持久 seal。没有设置 `--once` 时，接收器可在一个连接自然结束后等待下一连接；下一连接重新开始 FDT 边界状态，不继承上一连接的 GOODBYE。

自然 EOF 前没有 GOODBYE，但所有 MFM 帧完整时，允许结束，与现有 EOF 封存路径保持一致。**这只能证明本机收到的字节结构完整；不能证明断链之前板端事件全部发出。** 心跳、GOODBYE、EOF、文件 fsync 都不能单独证明端到端事件零丢失。

旧`FdtDataSender`的start会连接、stop会断开；旧普通TCP/ZBUF sender的stop保留连接。生命周期不同，不能仅增加接收参数便把ECC现有命令序列套到FDT。完整ECC计划现支持TCP/ZBUF/FDT，非默认模式绑定规范计划、实际接收状态和sender控制参数，见[管理手册](MANAGED_ZBUF.md)。数据主机FDT管理基础已采用私有Finish控制保持同PID/session多次实际EOF，见[周期手册](FDT_RECEIVER_CYCLES.md)。完整ECC已接入FDT重连及明确未使用来源结束，本机范围详见[管理手册](MANAGED_FDT.md)；真实板端/Narval互通仍待验。

## 4. 独立启动步骤

先构建，接收机器不用安装 Qt：

```sh
cmake -S apps/datareceiver -B build-receiver -DCMAKE_BUILD_TYPE=Release
cmake --build build-receiver --parallel 2
./build-receiver/getdaq-receiver --help
```

建立一个空的 incoming 目录。它不能与另一个接收器或管理服务共用，也不能包含上一轮 `.graw`：

```sh
mkdir -p /tmp/getdaq-fdt/incoming
./build-receiver/getdaq-receiver \
  --output /tmp/getdaq-fdt/incoming \
  --bind 127.0.0.1 --port 46005 \
  --status-bind 127.0.0.1 --status-port 46015 \
  --input-protocol FDT --once
```

此例只接本机测试发送端。现场需要改成实验接收网卡的实际地址，并单独核对板端 FDT 目标；不要直接对真实板卡运行合成测试。ZBUF 输入只需把参数改成 `--input-protocol ZBUF`。恢复普通 TCP 可改成 `TCP` 或省略参数。

查看状态在另一个终端执行：

```sh
curl http://127.0.0.1:46015/status
```

curl 只是可选查看工具，不是接收器构建或运行依赖。发送端先自然结束，`--once` 接收器随后退出。不要以无数据超时或 Ctrl+C 代替发送端 EOF；实际连接中断会保持失败。异常数据保留供检查，产品不会自动删除真实 `.graw`。

## 5. FDT 状态中的计数

| 字段 | 含义 |
| --- | --- |
| `protocol` | FDT 时为 `MFM over FDT TCP`；TCP/ZBUF 保持原 `MFM over plain TCP` |
| `wire_received_bytes` | recv 实际接收的字节数，包括控制字和异常尾部 |
| `fdt_control_bytes` | 已在合法帧边界识别的控制字总字节数 |
| `fdt_heartbeats` / `fdt_goodbyes` | 已识别的两种控制字数 |
| `received_bytes` | 线上字节减去已识别控制字；异常尾部也可能包含其中 |
| `accepted_file_bytes` | 完整帧已交给文件存储层的字节，可能仍在批量缓存中 |
| `sealed_bytes` | 存储层已完成同步的帧字节数 |
| `sender_eof_confirmed` | 所有已建立连接都收到真实自然 EOF；GOODBYE 不增加 EOF 计数 |
| `stream_sealed` | 排空、无错误、EOF 后状态及保存字节关系符合要求 |

FDT 的四个专用计数字段只在 FDT 模式出现，原 TCP 状态键和 seal 格式保持。seal 的 `received_bytes` 仍是保存的 MFM 文件总字节，不含控制字。因此正常最终状态满足 `received_bytes == accepted_file_bytes == sealed_bytes`，而 `wire_received_bytes` 可更大。运行中的多线程计数是低频观察值，不是所有字段同一时刻的事务快照。

GOODBYE 不触发写盘线程外的状态推断。seal 和进程退出仍是后续归档依据；接收进程持有目录锁期间，sentry 拒绝归档。`getdaq-inspect` 读取的是过滤后的 `.graw`，继续使用普通 MFM 检查，无需知道输入曾是 FDT。

## 6. 如何复查本机证据

```sh
python3 tests/build_receiver_validation.py build-receiver
./build-receiver/receiver-core-tests
python3 tests/receiver_transport_integration.py \
  build-receiver/getdaq-receiver build-receiver/transports-new-check
python3 tests/byte_storage_integration.py build-receiver/getdaq-receiver
```

核心测试覆盖两种 MFM 字节序和 FDT 各种分块大小，检查载荷中的控制字相同字节不会被删除、GOODBYE 后字节拒绝、下一连接重置和部分控制字 EOF 错误。实际 TCP 专项覆盖 12 场景，总线上字节 **49,739**，其中包含真实导出客户端、GOODBYE 后保持 socket 不关闭时尚未 seal 的状态，以及独立模式/非法协议管理门槛。

专项只在自身新建的私有证据目录生成 `.graw`；成功、失败及可处理的 SIGINT/SIGTERM 都先结束子进程，再删除合成载荷，保留小型日志、seal、原字节 SHA-256 和 `payload-cleanup.json`。SIGKILL、断电或文件系统故障仍可能阻止清理。禁止拿这个清理规则处理真实 Run。所有传输测试每轮全部流合计最多 **20GiB**；本专项远低于上限，检查至少 64MiB 可用空间后才启动。

普通与新接收器 ASan/UBSan/LeakSanitizer 各通过上述 12 场景和核心测试；原 TCP/慢导出/错误尾帧/目录锁/封存/sentry 归档完整回归也分别通过。回归中 sentry 是已有普通 Qt 程序，并没有随新接收器整体插桩。实际环境仍是 Ubuntu20.04.6/GCC9.4，全部为本机模拟 sender；没有 VxWorks 板端、Narval、Windows、Ubuntu24、macOS 或持续大流量结论。

独立归档位于 `build-receiver/validation-transports/`，只保留源码、程序及小型证据，不保存生成载荷；`tests/archive_receiver_transports_validation.py` 默认核对当前源码，`--historical-only` 只证明不可变归档。旧性能、文件上限及配置证据包保持原字节。

## 隔离网络命名空间的普通 TCP 回归

为把传输测试从 `127.0.0.1` loopback 扩展到真正经过 IP 路由的路径，`tests/run_receiver_netns_tcp_test.py` 会建立两个独立 Linux 网络命名空间，并用临时 veth 连接。测试在接收端启动本项目 `getdaq-receiver`，发送端向另一命名空间的地址发起普通 TCP 连接。

该回归用 `byte-storage` 保存一份32MiB确定性合成字节流，验证发送端/落盘文件SHA-256相同、TCP自然EOF后接收封存成功、程序退出码为0。源文件和接收文件共64MiB合成 `.graw`，运行结束即删除；回执确认raw剩余0，并记录接收程序和测试runner SHA。它不需要Qt，也不加入接收器运行依赖。

在支持非特权用户网络命名空间及veth的Linux上运行：

```sh
unshare -Urn python3 tests/run_receiver_netns_tcp_test.py \
  --receiver build-packages/getdaq-linux-host-20261021-71-147-current-source-full-candidate/bin/getdaq-receiver \
  --result build-receiver/receiver-netns-tcp-71-148.json
```

也可以把 `--receiver` 指向待验证的接收器程序。回归只能证明同一Linux内核中两个隔离网络栈之间的有界普通TCP字节接收；测试使用ByteStorage而非MFM解析模式，不能替代两台物理主机、实际NIC/防火墙/目标磁盘、板端TCP/ZBUF/FDT互通或持续吞吐验收。它也没有验证 `getdaq-catalog` 归档复制器的跨机路径。当前通过回执保存在Linux host候选 `validation/receiver-netns-tcp.json`。

## 归档器普通 TCP 跨命名空间回归

`tests/run_catalog_netns_tcp_test.py` 使用两个独立Linux网络命名空间及临时veth，接收端运行Linux host候选包中的真实 `getdaq-catalog --listen-catalog-tcp`，发送端运行同一包的 `--send-catalog-tcp`。测试源catalog由 `catalog-transfer-tests --prepare-netns-catalog` 从当前C++源码生成，随后检查源catalog、发送完成回执和目标端 `--verify-transfer` 全量内容校验。

在支持非特权用户网络命名空间和veth的Linux上，先构建测试夹具，再运行：

```sh
cmake -S apps/datasupervisor -B build-receiver/netns-catalog-test \
  -DGETDAQ_BUILD_SUPERVISOR_SERVICE=OFF -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-receiver/netns-catalog-test \
  --target catalog-transfer-tests getdaq-catalog -j2
PYTHONDONTWRITEBYTECODE=1 unshare -Urn python3 tests/run_catalog_netns_tcp_test.py \
  --catalog build-packages/getdaq-linux-host-20261021-71-147-current-source-full-candidate/bin/getdaq-catalog \
  --fixture-tool build-receiver/netns-catalog-test/catalog-transfer-tests \
  --result build-receiver/catalog-netns-tcp-71-149.json
```

71.149回归实际跨veth传输了含四个合成raw的完整归档；源目录检查与目标独立全量校验通过，发送完成回执和目标回执相同。测试按文件路径保守累计25,190,400字节（约24.0MiB），远小于20GiB上限；源fixture、目标归档和临时目录在结束时全部删除，回执记录raw为0。证据保存在Linux候选 `validation/catalog-netns-tcp.json`。普通TCP未认证、未加密，只适合可信且受限网络。

该结果验证的是同一主机同一Linux内核上的隔离网络栈，不是两台物理主机；它不证明NIC、防火墙、真实目标磁盘、断线后的真实部署流程或现场吞吐。物理双机验收仍按[现场步骤](CATALOG_TRANSFER.md#135-两台独立主机的现场验收步骤)执行。

## 可选ICE入口

原`getdaq-receiver`继续仅接原始TCP/ZBUF/FDT，不猜测Ice封装。旧DataProcessor接口另由无Qt的`getdaq-ice-receiver`接收，构建默认关闭、使用本地Ice兼容库；支持MFM帧存储及显式原样字节存储。旧接口无结束时不正常封存；新显式同连接结束和总字节核对产生独立ICE凭据，不制造TCP EOF。DataSupervisor固定计划可选择并绑定ICE字节存储模式，详见[ICE数据入口](ICE_DATA_INGRESS.md)与[验证记录71.34](VALIDATION.md#7134-ice-bytestorage接入datasupervisor固定计划2026-10-20)。
