# 受管理ZBUF：固定配置、控制流程与兼容边界

## 1. ZBUF是什么

这里的ZBUF是旧GET板端一种发送器选择。收到的DaqCtrlNodeI.cpp中，connect("TCP",地址)创建StdDataSender，connect("ZBUF",地址)创建ZBufDataSender；后者在VxWorks中使用zbufSockBufSend减少板端复制。两者向主机发送相同的连续原始MFM字节流，主机不需要VxWorks库，也没有额外的ZBUF包头。

新接收器原有独立ZBUF输入，现在接入固定接收计划和ECC。Qt继续只处理配置、控制与查看信息；数据由板端直接到原生C++11/POSIX receiver。此功能不证明真实板端零复制、吞吐、固件兼容或端到端无丢失，现场仍须核对实际板端构建。

原参考保留在source/get/GetBench-software-release-20210208/src/get/daq/DaqCtrlNodeI.cpp、source/mordicus/MDaq-release-20210208/src/mdaq/daq/TcpDataSender.hpp和src/mdaq/utl/net/ZBufConnection.cpp，没有编译入接收热路径。

## 2. 需要同时设置两端

数据主机固定receiver_plan.sources中，为目标源增加：

```json
"input_protocol": "ZBUF"
```

ECC固定配置的senders表中，对相同ID增加：

```json
"protocol": "ZBUF"
```

这是已有完整JSON中的字段片段，不是独立配置。源ID、receiver_address、IP/端口、队列、输出根、工件摘要、可执行文件摘要、服务身份和认证配置继续按[远程管理](REMOTE_RECEIVER_CONTROL.md)与[ECC网络服务](ECC_NETWORK_SERVICE.md)填写。

两个字段只能是实际JSON字符串，区分大小写。本页说明TCP/ZBUF；FDT另采用[多连接管理周期](MANAGED_FDT.md)。ICE、小写zbuf、空值、布尔值或数值都会拒绝。省略字段默认TCP。显式TCP在接收计划和原生ECC发送器规范快照中按默认值省略，保持原默认规范字节；固定输入原文件摘要仍会反映用户修改的文件字节。

同一个系统可为不同源选择TCP或ZBUF，但每个源的sender.protocol必须与对应receiver.input_protocol一致。不能只在一端勾选或把同一ID映射到另一节点。源/地址/控制代理唯一性、配置/工件身份和独占停止声明仍需通过旧前置检查。

## 3. 更改协议为什么会改变摘要

非默认ZBUF字段进入数据服务的规范计划和计划SHA，也进入ECC原生固定快照。切换原有部署时，不能只修改某个启动脚本并继续使用旧SHA；数据服务、ECC固定数据绑定和GUI审阅的快照/输入绑定应按原发布与部署流程重新核对。

重开旧持久根仍保持核对门槛，不能把更改协议作为清除旧状态的方法。不要恢复旧文件、改操作ID或换进程后宣称控制权/硬件UNKNOWN已经解决。完整部署关系见[原生协调器](NATIVE_ECC_COORDINATOR.md)。

## 4. 接收器实际怎样启动

数据服务根据固定计划启动receiver，非默认模式增加--input-protocol ZBUF，并继续传入once、durable、会话、发送源和配置/工件身份。启动后的状态必须报告input_protocol=ZBUF；要求ZBUF却实际TCP、或被包装脚本忽略参数时，启动核对失败并禁止新的Start。

旧状态缺少input_protocol字段按历史TCP解释，因此旧接收器仍可服务默认TCP计划，但不能冒充ZBUF计划。ZBUF仅选择已有原始TCP分帧、写盘、导出路径，没有把数据送入Qt或Ice。

状态中protocol文字仍为MFM over plain TCP，这是线上格式；input_protocol说明具体的板端/主机选择。两者含义不同。seal沿用既有格式，协议由启动核对、不可变运行配置及同一PID/session绑定；没有为同一字节格式新增或伪造EOF凭据。

## 5. Configure、Start和Pause/Resume

Configure先建立并核对接收器，ECC再用实际Ice DaqCtrlNode.connect("ZBUF",地址)选择板端发送器，继续设置循环缓冲区掩码和alwaysFlush。调用审计记录实际protocol字符串，默认TCP审计不改变。

Start调用daqStart并按原CoBo/MuTaNT时序启动生产。Pause停止生产并刷新发送器，但保留TCP连接和接收进程；Resume继续使用同一接收会话、PID及连接。普通Stop同样不单独证明EOF或完整接收。最终Breakup/断开发送器产生实际EOF，接收器排空/同步/退出后由Finish验证seal。

同一Run的暂停/恢复不会因此替换接收会话。结束后新Configure创建独立周期及目录；原始文件不覆盖，GET文件名样式、独占创建、异常数据保留和原生归档继续适用。

## 6. FDT为什么还需要另一套周期机制

旧FdtDataSender.start会建立连接，stop会发送结束信息并关闭连接。它的Stop实际产生EOF；当前TCP/ZBUF流程的Pause则需要保留同一接收会话。因此不能只把上面的两个字符串改为FDT，就套用同一Run流程。

FDT现已另外接入显式接收周期结束、Pause/Resume/Stop后的重连及明确未使用来源核对，详见[受管理FDT](MANAGED_FDT.md)。这保持接收会话而允许物理TCP连接更换；它是后续独立验证范围，不由此前ZBUF测试推断。ICE仍不能直接发到原始TCP端口。

## 7. 如何检查与排障

先对两个服务执行原离线check-config，核对新的计划与固定输入；不要对真实板卡发送合成帧。连接后按[GUI手册](ECC_GUI_CLIENT.md)执行Configure/Start及结束流程。可用[进程观察](RECEIVER_GUI_OBSERVATION.md)查看实际进程，但字节/帧数仍是缓存，running也不证明链路健康。

遇到input protocol differs时，检查数据服务固定计划、实际receiver可执行文件摘要及启动包装器是否保留--input-protocol。协议与sender映射不一致会在管理前置校验被拒绝，不应放宽检查来继续采集。板端connect不支持ZBUF或应答不确定时，保持原审计和UNKNOWN，不自动回退TCP或重发；现场必须确认板端实际实现。

所有正常结束仍需发送器实际断开、自然EOF、完整帧、退出0、成功seal及后续归档。真实Run和异常.graw保留供核对；测试清理规则仅用于自己创建的合成测试目录。

## 8. 验证与剩余工作

测试使用本机模拟板端的真实Ice控制、原生receiver和小型合成TCP帧。覆盖ZBUF选择、Pause/Resume连接保持、两接收周期、EOF/seal、协议不符拒绝及原TCP回归。具体普通/插桩结果以[验证记录](VALIDATION.md)为准。

每轮全部流合计不超过用户20GiB预算，包装器结束后删除生成.graw，保留小型计数、日志与SHA。实际VxWorks/Narval、板端发送器内部复制、物理多主机、Ubuntu24/Windows/macOS、长期大流量及完整事件计数仍需后续验收。
