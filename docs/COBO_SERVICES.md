# CoBo LED 与主机告警：操作、原理和故障处理

本模块处理少量控制命令和告警通知。代码是 C++11，复用仓库本地 Ice/GET 接口，不依赖 Qt，不接收或转发原始采集事件。原始事件仍由独立 [数据接收进程](DATA_SERVICE.md) 处理。

新增功能包括原生 LED 命令、板端 AsAd 告警检查器启停、主机告警回调客户端，以及可独立运行的 `watch-asad-alarms` 命令。这里的“原生”表示使用 C++ 和已有兼容库直接调用旧板端接口。选定CoBo的单灯命令已接入ECC持久操作和Qt维护窗口；告警检查器与独立回调watcher仍不等同于生产采集停机/封存协调。**收到告警本身不表示硬件已经停止，也不表示当前数据已经排空和封存。** 所有硬件结果目前只经过本机模拟测试，现场服务、GPIO、电气状态和目标平台仍需验收。

## 1. 先分清三种监控

| 功能 | 在哪里工作 | 做什么 | 能否证明采集已停止 |
| --- | --- | --- | --- |
| AsAd 自动保护 | 板端监控寄存器及电路 | 根据阈值和保护配置处理电源/温度异常 | 不能；应核对实际固件/电气行为 |
| 板端告警检查器 | 旧 getHwServer 的 `AsAdAlarmWatcher` | 默认每30秒检查活动 AsAd 的 AL 位，通知订阅者并尝试清除 initDone | 不能；不是完整 Stop 流程 |
| 主机告警回调 | 新 `CoBoAlarmClient` | 接收通知、持久化、保存有界事件队列并保持停机请求 | 不能；停机请求必须由生产 ECC 消费 |

一次电压/温度读取是另外的停止维护子流程，详见 [上电监控](ASAD_MONITORING.md)。它会改变选择寄存器，不能当作采集期间的被动监听。本手册的观察命令只订阅已有告警服务，不会自动上电、改变阈值或开启板端检查器。

## 2. 构建与最小准备

从 GetDAQ 仓库根目录执行：

```bash
cmake -S third_party/ice-compat -B build-ice-compat -DBUILD_TESTING=ON
cmake --build build-ice-compat --parallel 4
build-ice-compat/getdaq-hardware --help
```

Ice、GET Slice、bzip2 和 mcpp 来自本地源码/归档。测试脚本需要 Python3；程序运行不需要 Python。未安装 Python 时，CMake 仍构建原生程序和 C++ 服务测试，但不会登记 Python CLI 测试。板端 IceEirfu 仍不齐全，不能用上述主机构建证明板端也可重建。

观察前需要知道三个地址/身份信息：

1. 实际 CoBo **CtrlNode** 代理，而不是 ECC SOAP 地址或数据接收端口。代理应由现场配置/服务提供，不能猜测。
2. 该节点 `name()` 返回的 CoBo ID。它可能是 `CoBo0`，也可能采用机箱/槽位命名；必须完全一致。
3. 主机用于接收回调的监听地址。板端必须能连接到这个地址和端口。仅主机能够连接板端，并不能证明反向回调可达。

## 3. 独立观察告警

以下是参数模板。尖括号内的内容必须换成现场值；不要把模板原样作为地址使用。

```bash
build-ice-compat/getdaq-hardware --encoding 1.0 watch-asad-alarms \
  '<实际CtrlNode代理>' '<实际CoBo ID>' \
  'tcp -h <主机供板端访问的IP> -p 12050' \
  120 \
  /absolute/path/alarms-new.jsonl \
  /absolute/path/subscribe-new.jsonl \
  /absolute/path/unsubscribe-new.jsonl
```

| 位置 | 参数 | 含义 |
| --- | --- | --- |
| 1 | PROXY | 旧板端 CtrlNode 代理 |
| 2 | COBO_ID | 预期节点名称，订阅前先核对 |
| 3 | LISTEN_ENDPOINTS | 本机 Ice 回调监听端点，整个字符串加引号 |
| 4 | SECONDS | 观察时长1..86400秒，使用单调时钟计算截止时间 |
| 5 | NEW_EVENT_FILE | 持久化通知/监听状态的新 JSONL 文件 |
| 6 | NEW_SUBSCRIBE_FILE | 此次订阅动作的新意图/开始/应答日志 |
| 7 | NEW_UNSUBSCRIBE_FILE | 此次注销动作的新意图/开始/应答日志 |

三个日志路径必须不同，文件不能已存在，父目录必须事先存在并可写。程序不会覆盖旧记录或自动创建父目录。它逐步创建日志：事件文件在准备监听器时创建，订阅/注销文件在对应动作前创建；若后面的日志无法创建，先前已成功的订阅不一定会被注销，应以状态和日志为准。

默认编码是 Ice1.0。只有确认目标支持时才选择 `--encoding 1.1`。命令不接受 `--description`：它不读写普通设备寄存器，订阅与 LED/板端检查器的权限绑定是不同职责。

观察时长从订阅应答后开始计算；准备监听、订阅、注销和文件同步还有额外耗时。这个时长不是整个进程的硬性退出期限，文件系统阻塞也不由RPC超时控制。

回调监听地址若是 `127.0.0.1`，只能用于同机模拟联调，真实板卡无法访问主机的回环地址。跨网卡/NAT 时可明确填写对外发布端点：

```bash
build-ice-compat/getdaq-hardware --encoding 1.0 \
  --callback-published 'tcp -h <板端能够访问的主机IP> -p 12050' \
  watch-asad-alarms '<CtrlNode代理>' '<CoBo ID>' \
  'tcp -h <本地监听IP> -p 12050' 120 \
  /absolute/path/events-2.jsonl /absolute/path/subscribe-2.jsonl /absolute/path/unsubscribe-2.jsonl
```

发布端点是给板端使用的地址声明，不会自动创建端口转发或修改网络配置。API允许 `-p 0` 由系统分配端口；现场需要固定通信规则时，应明确使用实际端口。

## 4. 如何读输出和退出码

标准输出每行是一条 JSON。首先出现 `subscription_active`，表示订阅 RPC 的应答和动作日志已保存。通知到达时输出 `alarm`；命令会保持停机请求，结束观察并尝试一次正常注销。自然达到截止时间时，也执行一次注销。

| 退出码 | 含义 | 操作员应如何理解 |
| --- | --- | --- |
| 0 | 观察结束，注销已应答，没有保持的告警/故障 | 不证明检查器运行、反向网络健康或硬件无异常 |
| 1 | 初始化、日志或订阅/注销动作失败 | 看标准错误、最后状态及三份日志；UNKNOWN 不能重发 |
| 2 | 参数或命令格式错误 | 修正时长、路径或选项；相关格式检查在连接前执行 |
| 3 | 已保持告警或通知故障，正常注销已应答 | 硬件Stop尚未执行；交给运行控制处理 |

`received`/`durable`/`lost`/`invalid`和时间、序号使用字符串保存64位数，避免通用 JSON 读者丢失整数精度。`alarm_bits`/`alarm_mask`是四块 AsAd 的位图：1/2/4/8对应板0/1/2/3；例如9表示0和3。不能把位图9理解为“AsAd9”。

`hardware_stop_performed=false` 是实际行为边界。`callback_health_verified=false` 表示订阅接口没有心跳确认；即使已收过通知，后续安静期的持续可达也没有得到证明。故障时若没有 `finished`，不能只根据进程退出推断远端已经注销。

## 5. 原生回调 API 与边界

公有头文件 `apps/hardware/coboalarmclient.h` 只包含标准 C++ 类型，不把 Ice/Qt 类型暴露给调用者。它使用 PIMPL；应用只需要链接 `getdaq-hardware-core` 及其 CMake 传递依赖。

```cpp
#include "coboalarmclient.h"

GetDAQ::CoBoAlarmOptions options;
options.nodeProxy = actualCtrlNodeProxy;
options.expectedCoBoId = actualCoBoName;
options.callbackEndpoints = actualListenerEndpoints;
options.eventAuditPath = newAbsoluteEventPath;
options.encodingMinor = 0;
GetDAQ::CoBoAlarmClient alarms(options);
alarms.Subscribe(newAbsoluteSubscribeAuditPath);

// 由控制线程周期调用；这里只得到通知，不在Ice回调线程发硬件Stop。
const auto events = alarms.Drain(256);
const auto status = alarms.Snapshot();
// 完整ECC应消费status.stopRequested并协调Stop/排空/封存，当前尚待接入。

// 仅在已知Active时显式调用；结果未知时禁止再次调用。
alarms.Unsubscribe(newAbsoluteUnsubscribeAuditPath);
```

这段代码展示 API 调用关系，不是可直接开始真实采集的完整程序。构造函数准备监听器和事件文件，不自动订阅。析构只销毁本地资源，**不**隐式注销或重试网络动作；已知 Active 也应由拥有者显式注销。异常后的远端残留订阅须由恢复协调器处理。

| 选项 | 默认值 | 可用范围/用途 |
| --- | --- | --- |
| encodingMinor | 0 | 0或1 |
| timeoutMs | 3000 | 100..60000ms；RPC/连接等待上限，非告警检测周期 |
| maxPendingEvents | 256 | 1..4096；待消费队列上限 |
| maxEventAuditBytes | 16MiB | 64KiB..64MiB；单实例事件文件预算，包含监听/订阅状态记录 |
| publishedEndpoints | 空 | 可选对外端点；没有默认猜测地址 |

主机限制接收消息为64KiB、回调工作线程1..2个；告警不携带波形。对端身份和非零四位位图必须通过检查。队列/日志预算独立：成功落盘但未能排队的通知仍记为 lost；排入队列但未落盘的通知同样记为 lost，单个事件不会因为两处失败重复计数。计数达到 uint64 上限后保持饱和。

## 6. 状态、持久化与 UNKNOWN

正常路径是 Fresh → Subscribing → Active → Unsubscribing → Closed。订阅/注销发出后若连接断开，或应答记录未能持久化，进入 Unknown 并保持停机请求。Unknown 不允许再次订阅、注销或 reset；这不是“服务器一定失败”，而是主机不能确认结果。

所有命令日志用 `O_EXCL` 创建权限0600的新文件，先同步父目录，再逐行写入/fsync。动作的 `begin` 落盘后才发送 RPC。Ice 即使把 subscribe/unsubscribe 声明为 idempotent，新客户端也设置 `Ice.RetryIntervals=-1` 禁用运行库自动重发。

订阅者 ID 是每实例随机 `getdaq/<UUID>`，不会复用旧 ECC ID。收到旧 `AlarmServiceI` 的源码表明：同名订阅会覆盖旧回调；reset会删除所有客户端。因此新代码从不调用 reset，仅注销自己持有的 ID。

通知先尝试落盘，再回应板端，然后提供给主机控制线程。队列满、预算用尽、部分文件写入失败或非法通知都会保持停机请求；Drain只清空队列，不清除状态位。日志失败后不继续写该事件文件，避免把半行后的内容误作可靠记录。普通预算用尽仍可完成注销：远端应答成功后状态为Closed，但若事件文件不能保存注销状态，API仍报告日志失败，不能再次注销来“消除错误”。

逐通知fsync只作用于低频控制记录，不作用于原始事件流。旧板端在持有订阅互斥锁时逐客户端调用回调；本地磁盘过慢可能造成回调超时和旧服务移除订阅。因此需在实际系统核对控制日志磁盘延迟，不能把持久化实现说成绝不会丢告警。

## 7. LED 类型、节奏与上电故障

| 类型 | 数值 | 状态 | 数值/节奏 |
| --- | --- | --- | --- |
| P/B/U/D | 0/1/2/3 | Off/On | 0/1 |
| 同上 | 同上 | SlowPulse | 2；每1000ms翻转，完整周期约2秒 |
| 同上 | 同上 | FastPulse | 3；每500ms翻转，完整周期约1秒 |

每块 AsAd 四盏灯，旧 GPIO 索引为 `4*asadId+type`，AsAd ID为0..3。收到的旧 LED 实现只识别 hardwareVersion2或5；没有 `HAVE_XGPIO` 时部分方法是空操作。因此命令应答不证明真实灯亮，也不能推测板型3或其他新版本必然支持。

原生库提供 `CoBoLedSetAll(enabled, hardwareVersion)`、`CoBoLedModify(type, asadId, state, hardwareVersion)`及`CoBoLedBlinkAll()`。前两者执行前绑定匹配描述并核对型号；blink只调用有类型定义的板端`LedManager::blinkLEDs()`，表示一次顺序闪烁所有LED，不用寄存器写入模拟。GPIO不属于普通寄存器描述，不能通过写 `debug.led` 假装替代该服务。当前 Qt `Selected CoBo maintenance...` 窗口支持健康Prepared且数据服务停止时对固定活动CoBo执行单灯Off/On/Slow/Fast或一次整板顺序blink；不提供旧CLI的LED flip和任意周期脉冲。

LED操作经普通ECC原生控制通道持久化并串行执行，使用当前ECC实例/配置摘要、固定活动CoBo目标及唯一operation ID；同ID不同目标或状态会拒绝。界面确认默认Cancel，丢失应答时只查询原操作，不自动重试。结束状态须仍为Prepared。命令ACK记录不验证灯的物理状态，因此现场需目视核验；异常时保留审计并按未知结果处理。端到端的两种控制编码、GUI意图、独立raw读取拦截、真实ECC进程/两个数据服务/原生receiver合成生命周期回归见验证记录71.139；该回归没有实机或吞吐证明。

`CoBoMonitoringSettings::ledPolicy`默认NoLedManager；明确需要旧LED服务时选RequiredLedManager并填写2或5。Required模式上电前清灯，活动板检查成功后点P灯；连接/严格供电/上电后AL/PWU失败时将对应B灯设为SlowPulse。选择NoLedManager不会调用管理器；Required模式缺方法或应答丢失会立即停止，禁止写失败后自动降级。

故障LED和失败判断共用一次读取：执行器保存 ReadValue，条件LED引用该值，ExpectObservedValue检查同一观察。否则指示灯调用后重新读取可能“碰巧恢复”，掩盖最初失败。组合计划须用 `AppendHardwareSteps`，它同时平移条件引用和缓存观察引用；不能直接 vector::insert。

## 8. 板端检查器与配置策略

`CoBoAsAdAlarmMonitorInit(settings)`调用旧 `setAsAdAlarmMonitoringEnabled`，与订阅是两个独立动作。它要求绑定描述、停止流水线，并预检查间接访问的asadStatus.alarm、asadEnable和可写mutantConfig.initDone；缺字段/只读错误在任何管理器动作前拒绝。

xcfg中 CoBo 的 `Alarms/checkAsAd` 与 `Alarms/subscribe` 默认true；新的MonitoringCompiler通过共享ExperimentReader检查类型、单位、来源和默认，并输出 `alarm_policy`。这些开关与各AsAd的Monitoring/isAlarmSet不同：后者是板上保护配置，前者是板端检查器和主机订阅策略。

```xml
<Alarms>
  <checkAsAd type="bool">true</checkAsAd>
  <subscribe type="bool">true</subscribe>
</Alarms>
```

离线 `compile-cobo-monitoring` 不连接硬件。`native_led_manager_available`/`native_alarm_subscription_available=true`表示原生 API存在；`led_manager_supported`/`host_alarm_subscription_supported=false`表示这份JSON不能自动部署这些服务。它不会替调用者选择LED板型、监听端点或独占所有权，也不会自动执行subscribeAlarms策略。生产 ECC 尚须组合这些动作。

## 9. 原系统停机链为什么不能照抄日志

旧板端检查器在告警时通知、快闪红灯并尝试清除initDone；旧主机AlarmLogger只输出带 `#STOP#` 的日志。注释中的停机请求依赖log4cxx SocketAppender/6913及外部Ganil服务，不是该函数直接执行硬件Stop。

新代码把告警变成持久化的stopRequested保持信号。完整ECC需要在自己的串行控制线程中核对运行状态/所有权、停止触发与数据发送、等待接收器EOF和排空、确认封存，再提交状态。这个协调器和真实恢复仍未完成，不能把观察命令替代运行控制。

## 10. 常见问题与本机验证

| 现象 | 判断依据与处理 |
| --- | --- |
| Active但一直没有告警 | 接口无心跳；核对检查器、监听/发布地址和现场通信，不能显示已证明健康 |
| 节点名称不符 | 尚未订阅；核对真实name()和配置，不自动接受另一个节点 |
| OperationNotExist/UNKNOWN | Required管理器可能没有该方法，或结果不确定；保留日志/实际状态，禁止自动换实现或重发 |
| lost/queueOverflow/budgetExceeded/auditFailed | 通知处理可靠性下降；停机请求保持，Drain不会恢复健康 |
| Closed但API报告事件日志失败 | 注销已经应答；修复记录/恢复核对，不能再次注销消除错误 |
| 进程异常终止后残留订阅 | 析构不做网络清理；按专用ID及意图日志交恢复协调器处理，不reset全服务 |

在构建目录验证：

```bash
cd build-ice-compat
ctest -R 'hardware-services-tcp|hardware-alarm-cli' --output-on-failure
```

服务测试覆盖两种编码、LED故障使用原观察、间接写权限、应用后断线、其他订阅者保留、订阅应答前回调、有界队列/文件和真实部分写失败。CLI测试启动独立模拟服务/观察进程，验证无事件不等于健康、告警退出3、只注销自己的订阅及日志。它们不访问真实板卡，不证明GPIO、30秒告警时效或硬件停机已经验收。完整证据见 [验证记录](VALIDATION.md)，全部未完成范围见 [重构进度](REFACTOR_STATUS.md)。

来源为收到的 `source/get/GetBench-software-release-20210208/src/get/cobo/` 下LedManagerI、CtrlNodeI、AsAdAlarmWatcher，以及 `src/get/mt/` 下AlarmService接口/实现和AlarmLogger。历史源码保持不变。


## 单板Prepare中的自有订阅

[CoBoPrepareClient](COBO_PREPARE_CLIENT.md)已在首次硬件写入前准备本监听器并执行只读ValidateService。旧Ice checkedCast可能根据本地类型化代理直接返回，这里显式调用远端ice_isA验证AlarmService。单板前段完成后先Subscribe，再启用板端检查器，并将故障保持接到HardwareClient的逐步/等待联锁。成功返回仍保留监听器给后续Configure/运行；失败不自动注销，不发硬件Stop，完整生产故障排空和恢复仍待接入。
