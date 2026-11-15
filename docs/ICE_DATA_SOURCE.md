# ICE数据源库与命令行发送：同连接、总字节和UNKNOWN

本页对应`apps/iceingress/icesourceclient.h/.cpp`及`getdaq-ice-send`。它补齐[可选ICE入口](ICE_DATA_INGRESS.md)的源侧显式结束。它是数据源程序使用的原生C++库，不能让Qt/ECC通过这个库转发波形。原板端IceDataSender没有被悄悄替换；其升级/实际VxWorks工具链及完整ECC发送器已按新增源控制能力接续，真实板端接入仍待完成；数据主机的固定计划/专属归档见[管理ICE](MANAGED_ICE.md)。

## 1. 与操作软件的关系

一个系统有两条路径：操作台→ECC/板端控制用于准备、Start、Stop和查看信息；真正数据源→接收器用于原始字节。IceSourceClient属于后者。公共头文件只有C++11标准类型，Ice细节留在实现，复用本地旧Ice/GET兼容组件，没有Qt。

不要在GUI的信号槽里读取graw或调用Send来模拟正常DAQ路径。命令行工具供本机联调、明确文件发送或原生数据管道使用，不把成功的回环传输当成真实板端吞吐验收。

## 2. 为什么固定连接而不是只保留相同IP/端口

发送端先核对原DataProcessor及新增DataIngress接口，取得实际TCP连接，再通过`connection->createProxy`构造固定代理。processData、会话发现和finishStream使用这个同一连接；它不是每次都按端口重新寻找服务的普通代理。

如果原接收进程消失、相同端口起来另一个进程，原源不能自动接入新进程。连接错误保持UNKNOWN，即使新服务看似健康也不猜测旧字节是否送达。这样旧流的计数、会话和完成不会落到另一目录/进程上。

本地Ice3.5固定代理不允许随意修改collocation/endpoint/locator/router属性；这些前提在固定之前核对。编码1.0/1.1保持选定版本。重试间隔设为-1，析构只释放本地资源，不发finishStream。

## 3. API如何用

```cpp
#include "icesourceclient.h"

GetDAQ::IceSourceOptions options;
options.address = "192.0.2.10";
options.port = 46105;
options.encodingMinor = 0;
options.chunkBytes = 256 * 1024;
// 可以填从运行配置确认的HEX64；留空则查询接收器本次标识。
options.expectedSession = "";

GetDAQ::IceSourceClient source(options);
source.Connect();
// 由实际数据源在收到缓冲块时调用；调用期间缓冲区须保持有效。
source.Send(data, byteCount);
// 实际源确认没有后续字节时才显式结束，Pause/短暂无数据不能代替。
source.Finish();
```

192.0.2.10是说明用占位地址，须换实际接收网卡。此片段的data/byteCount是源提供的字节指针/长度，不是完整可运行程序。空Send允许，但不伪造事件或远端结束；非空数据要求有效指针。需要源码重编译的板端应先进行接口/工具链/时序验收，不能直接将该片段当VxWorks替换证明。

选项为明确单播IPv4、1..65535端口、编码0/1、100..600000ms超时；chunkBytes最多64MiB−512，默认256KiB。接收器Ice消息预算还包括封装，源块不能超过实际服务器上限。总结束计数最多2^63−1，不截断成32位值。

## 4. 会话发现不表示控制权

DataIngress.streamSession()返回这个接收进程启动时的64字符小写HEX标识。它只读，不绑定数据源、不接收事件、不请求结束，也不是认证令牌。

expectedSession非空时必须精确匹配，不符在任何Send前失败；为空可以发现当前流，但仍需部署/控制端单独确认这是目标接收进程。会话发现不会验证实际加载配置、独占控制权、板端停止、完整事件计数或Run归档。

旧DataProcessor-only服务器没有新结束接口，源库在Connect前置检查拒绝并发送零字节。它不降级为“发出去以后再假造EOF”。旧发送器需要继续接收时，使用[入口手册](ICE_DATA_INGRESS.md)的未确认兼容边界。

## 5. 状态与计数怎样解释

| 状态 | 意义与允许动作 |
| --- | --- |
| Fresh | 尚未Connect，只允许原对象一次连接前置 |
| Connecting | 接口/连接/会话正在核对，尚未发送数据 |
| Connected | 可Send或显式Finish；每次Send完成回到此状态 |
| Sending | 当前缓冲块分割成多个单向调用，源缓冲须仍有效 |
| Finishing | 已尝试源结束；不能并发或自动再发 |
| Finished | 收到结束应答；拒绝重复Finish/Send，不表示接收端文件封存已核对 |
| Unknown | 发送/发现/结束出现不确定结果；原对象拒绝Connect/Send/Finish重放 |

issuedBytes/issuedChunks在发出每块之前保守计数，locallySentBytes在单向调用返回后累计。单向返回只表示本地handoff；不能把它称接收器应答或磁盘同步字节。

finishIssued表示确实尝试调用，sourceEndAcknowledged表示接收到原结束应答。两者都不是receiver_seal_verified。最终完成须查实际receiver退出、专属ICE凭据、源/文件/字节和后续Run关系。

UNKNOWN不会因为本地析构、新建源对象、重开端口或换标识而自动解决；源调用者应保留原错误、计数、接收目录和控制/运行证据。若本地输入读取失败或捕获中断，工具不发结束，接收端仍标明缺源确认。

## 6. 命令行发送

可选兼容构建开启后：

```bash
cmake --build build-ice-compat --parallel 2 \
  --target getdaq-ice-receiver getdaq-ice-send
```

接收器启动见[ICE入口手册](ICE_DATA_INGRESS.md)，发送示例：

```bash
build-ice-compat/getdaq-ice-send \
  --host 192.0.2.10 --port 46105 \
  --input /path/to/owned-input.graw \
  --encoding 0 --chunk-kib 256 \
  --timeout-ms 10000 --max-bytes 1048576
```

输入必须是可读普通文件，拒绝符号链接；`--input -`读stdin，可接实际数据源管道。工具以有界缓冲逐块读取，不整文件加载。只有自然输入EOF且没有捕获中断/读取/发送错误时才发Finish；stdin仍打开但暂时没数据会继续等待。

max-bytes是明确源侧限制；普通文件在连接前核对大小，超过则零写入拒绝。管道在下一块将超过限制时失败，不继续发送该块、不发结束。省略该参数不表示实验可不受资源约束，源总数仍有签名64位上界；本工作区的测试脚本另严格遵守每轮全部流最多20GiB。

工具不会删除输入文件。只有自动化脚本在自己新建的私有目录下生成的合成文件，才在测试后删除；真实生产/参考数据不能套此清理规则。

## 7. 应答丢失与替换服务

源End可能已被接收器接受而应答没回来，或源单向发送只完成本地排队。库不自动重连、重发块或补Finish。最安全的状态表达是UNKNOWN，接收文件/结束声明可能已发生；不能用UNKNOWN推断全部没发生或假定可以重来。

测试在数据送入后确认原receiver实际接受并解析，再结束原进程，并在同端口启动另一session的进程；源使用固定连接，发送/结束不转到新进程，新进程字节/来源绑定均为0。替换服务就绪通过明确测试标记通知，不能仅靠固定睡眠猜测切换完成。

## 8. 当前验证与复现

源库与实际接收器测试包括两编码、自动会话发现、零流、错误预期会话、析构不发结束、原服务丢失后替换不能重连，以及真正旧接口服务缺扩展时零写入拒绝。命令行测试包括文件/stdin自然EOF、连接前限额和保持stdin打开时的中断不发结束。

```bash
cmake --build build-ice-compat --parallel 2 \
  --target getdaq-ice-receiver getdaq-ice-send \
  ice-ingress-fixture ice-source-client-fixture
python3 tests/run_ice_source_tests.py \
  build-ice-compat/getdaq-ice-receiver \
  build-ice-compat/ice-source-client-fixture \
  build-ice-source/reproduced
```

脚本仅本机模拟源/接收器，合计线上保守上界8MiB；finally先结束自有测试组，再删除本次.graw，保留小型SHA/计数/日志。SIGKILL用于测试原服务丢失，不保证源内存缓存已落盘；脚本不将这些失败残留当数据持久性证明。真实板端、VxWorks升级、完整ECC/Run接法、平台及持续吞吐仍待验，最终构建/测试范围见[验证记录](VALIDATION.md)。

源进程的生产线程、暂停、尾缓冲排空和最终结束模块见[源端生命周期](CONTROLLED_ICE_SOURCE.md)。纯数据构建可跳过Qt控制工具；完整ECC/板端适配仍须继续。

源端低频接口和ECC固定接收会话的完整接法见[ICE源控制](ICE_SOURCE_CONTROL.md)。原接口服务仍需升级，真实板端和硬件恢复不能由本机网络流程替代。
