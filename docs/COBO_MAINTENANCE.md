# 选定CoBo维护：重准备、LED状态/闪烁/翻转/脉冲与单次AsAd pulser诊断

本页描述固定目标单板维护入口。重准备、LED设置、顺序闪灯、GPIO状态翻转、任意周期LED脉冲和单次pulser诊断均沿原生ECC、HTTP及Qt维护链路；硬件指令仍由独立控制服务执行，Qt不参与数据传输。硬件应答不证明实体灯态，Linux/Qt5.12.8本机端到端测试也不代表实板或持续DAQ性能验收。使用前核对[功能表](REFACTOR_STATUS.md)及[验证记录](VALIDATION.md)，旧运行包不含本次新入口。

## 旧源码中的单板操作边界

`source/get/GetBench-software-release-20210208/src/get/rc/Node.h` 的 `call_prepare/call_unprepare/call_configure/call_start/call_stop` 直接转发到各板 `impl_*`；该Node类没有自己保存一套Idle/Prepared/Running状态机。`BackEnd.cpp::createEngine()`创建的是全局 `SM_ECC/SM_ACTIVE`，其 `prepareNode/configureNode` 则直接选择一个Node调用上述回调。不要据“有单板回调”推断存在可任意独立运行的完整单板ECC状态机，也不要把多个独立旧ECC端点的Progress/Regress与一个多板ECC实例混为一谈。

当前Native单板维护复用了这类固定目标Prepare/Configure行为，并记录逐板阶段；父ECC仍遵守整机数据接收计划和MuTaNT顺序。主界面在恰好一个活动CoBo、没有MuTaNT时可用下方Progress/Regress控制整个单板ECC；多板时该入口打开选定CoBo维护，不能伪装成可独立Start/Stop某板。仍需逐项对齐旧工具实际使用的单板诊断流程、配置部署、结果核验和跨服务恢复；当前状态日志与维护入口不代表这些工作已全部完成。

## 1. 为什么不能直接再做全局Prepare

用户提供的2018年CoBo clock realignment教程第4—5页要求：先从Ready执行Break，停在Prepared，不要继续向上Reset。教程第13—14页对故障CoBo执行Load Hw和Configure，第20—21页要求退出诊断后再全局Configure，并提醒不要Reset。这里的重点是保留其他正常CoBo已经完成的准备/对齐。

旧GetController的Load Hw不是一次简单的“加载文件”。收到的TestController::loadAndPrepare实际执行硬件描述、流水线、AsAd上电、时钟、AGET/ADC/DAC与保护、告警订阅、阈值和自动读出延迟等步骤。Configure又写入实验参数。只重新写一个时钟位或调用pipeInit不能替代该流程。

新实现复用已经迁移的完整单板Prepare计划，选择固定配置中的一个活动CoBo执行；不会重新调用系统槽位定位、机箱内/机箱间对齐，不写其他CoBo或MuTaNT。操作结束仍为Prepared，后续Configure必须明确执行。

## 2. 哪些状态可以进入

维护要求同一个仍存活的系统准备对象处于Prepared：可以是原Prepare刚完成，也可以是正常采集后完成Stop、Breakup，所有发送器断开且receiver正常结束之后。不能在Ready、Running、Paused、Failed或冷重开后仅凭缓存进入。

调用者必须重新确认所有数据进程已停止；库接口的布尔值是上层责任声明，不能替代生产ECC对真实数据服务的查询。原配置中的目标ID必须准确存在、活动且保留成功准备对象。库先核对所有声明节点的名称、设备/寄存器权限和生产停止位，包括停用但仍在拓扑中的板卡。所有initDone必须为零。

维护不会接受一个新IP或任意寄存器地址，也不在此接口改变实验配置。目标仍使用构造时复制的原描述、参数、超时/编码和回调策略。需要不同诊断配置时，不能把临时文件内容塞入原对象冒充原固定工件；其受控部署流程仍需后续实现和验收。

## 3. 为什么要更换目标板的告警订阅

原Prepare后保留了本控制者的告警订阅。重新准备包含目标板的电源/时钟和校准步骤，如果直接沿用旧订阅，正常的重新上电过程也可能被旧状态当成故障保持。

因此顺序是：

1. 核对原对象Prepared状态、固定配置中的活动目标及上层数据停止声明。
2. 保存维护意图与原准备目录/配置/工件绑定。
3. 核对所有声明节点的元数据、生产停止与未Ready条件，再检查目标完整计划和原订阅；尚不写硬件、不注销订阅。
4. 明确注销目标板上本对象自己的旧订阅，保存准确应答与审计链。
5. 只有注销确认后，销毁旧目标回调监听器，再按原设置建立新准备对象。这样固定回调端口也有机会重新绑定。
6. 完整重新准备选定板，在原计划规定的时刻订阅新告警并开启板端检查器。
7. 核对全系统仍停止且未Ready，记录维护完成，返回Prepared。

不会调用reset清除整个告警服务的订阅者。其他板的监听器、订阅者和准备对象都保留。注销或新订阅应答丢失时保持Failed，不猜测结果、不重发、不自动回滚。

## 4. 校准脉冲与采集启动不同

原自动读出延迟校准会暂时切换firmwareEnable等控制位。这些是完整单板校准计划的一部分，不能为使测试“看起来没有启动”而删除。维护不连接或启动数据发送器，不启动receiver；校准结束必须重新核对firmwareEnable/intEnable为零。

因此软件测试既检查选定板完整执行原计划，也检查结束时生产停止、其他板没有新增写入、MuTaNT及系统对齐调用计数不变。单纯检查“某个控制位从未为1”不是这里正确的成功条件。

## 5. 日志与失败结果

维护目录必须全新，重复使用会拒绝；不覆盖或删除旧准备历史。

| 文件 | 用途 |
| --- | --- |
| maintenance.jsonl | 目标ID、原准备目录、配置/工件、全局前提检查和最终结果 |
| old-subscription-close.jsonl | 原目标订阅的明确注销意图/应答 |
| board-prepare/ | 新目标准备对象的完整命令、校准和新告警审计 |
| 原准备目录system.jsonl | 维护开始/完成引用及维护日志链摘要，原系统对象继续保留 |
| 原准备目录cobo-lifecycle.jsonl | 准备与采集对象共用的逐板状态转移；写入失败保留UNKNOWN，详见[持久记录说明](COBO_LIFECYCLE_JOURNAL.md) |

初始状态/目标选择不合法时，在建立维护操作前拒绝。进入维护后的元数据、停止观察、订阅、硬件调用或持久化失败会使准备对象保持Failed。正常Configure不能把它变成成功；部分硬件结果保留，后续需要明确故障处理。

维护完成并不证明电气相位已经修复，也不证明诊断波形良好。教程中的样本检查仍需要实际数据和独立诊断工具，原始事件不进入Qt控制界面。教程提到“忽略timeout”是其历史环境中的操作说明，不是新版软件的错误处理规则。

## 6. ECC持久操作与网络调度

NativeEccCoordinator::ReprepareCoBo(operationId, coboId)在固定配置中核对活动目标，通过与普通控制命令相同的互斥和输入绑定检查，重新查询数据停止状态，再执行单板重准备。新命令内部编号11，10继续保留给日志中的恢复记录，旧1—9编号不变。

包含编号11的日志应使用本版或支持该维护编号的新工具读取；旧工具会拒绝未知命令，不能通过改写日志中的编号使其勉强加载。新版继续接受原1—9命令和编号10的恢复历史。

状态转换只能为Prepared→Prepared。目标ID进入操作请求键，并保存在operation.jsonl；省略目标、未知/停用目标、重复操作ID、运行中状态、数据未停止、UNKNOWN或冷实例保持均拒绝。实际动作失败仍由原状态机记录UNKNOWN，不会把部分维护写成Prepared成功。重开日志能够识别维护记录，但不恢复原硬件对象或自动重放。

两个编码的本机ECC测试在维护后完成明确的Configure/Start/Stop/Breakup，正常Breakup后又维护另一块板，再完成第二周期和Reset。数据生命周期在该测试中明确为替身，没有真实receiver或封存证明；网络服务和真实接收器链路不能据此声称完成。

### 单板库接口


SystemPreparationClient::ReprepareCoBo(coboId, newAuditDirectory, dataProcessesStoppedConfirmed)由调用者串行执行；忙碌时立即拒绝，不排队。它使用Maintaining瞬态，成功返回Prepared，失败保持Failed。瞬态期间不返回可被外部并发使用的告警对象。

硬件库还提供 `SystemPreparationClient::ConfigureCoBo(coboId, newAuditDirectory, dataProcessesStoppedConfirmed)`：复用构造时绑定的该板 Configure 计划，操作前核验完整拓扑仍停止且全部 `initDone=0`，只执行选中板配置；操作后再次核验所有节点停止且未Ready。成功后父对象仍为Prepared，后续整机Configure须由操作者另行明确执行。该方法不执行对齐、MuTaNT Configure、`initDone`、发送器或Run start。失败若发生在目标硬件调用之前（例如审计目录已存在）则维持Prepared；硬件调用一旦开始，异常会失败锁定，不自动重试。

该操作现可从Prepared状态的维护窗口使用`Configure selected CoBo...`按钮，或在MCP控制模式调用`ecc_configure_cobo`；两者都要求核对固定目标并明确确认，Native ECC再持久化原始请求/审计并复核数据停止与硬件互锁。HTTP/MCP结果不确定时观察原operation ID，不用新ID盲目重发。该入口仍不创建逐板独立Prepare/Configured生命周期；详见[验证71.383](VALIDATION.md#71383-单板-configure-持久ecchttpgui维护网络回归2026-10-24)。

HTTP/Qt接线沿用原实例/固定快照/客户端绑定、先保存单次意图、控制与数据停止核对、维护与普通命令互斥及失联后观察原操作。窗口不会直接调用板端硬件写接口。

板端getHwServer/IceEirfu保持现有部署。此维护功能不会修改板端固件、扩大数据通路依赖或让Qt参与raw传输。

## 7. 操作台的操作顺序

本节描述新源码的入口。使用前确认操作台和ECC服务均来自包含该功能的同一版本，旧运行包不能只替换一个窗口文件。

1. 按正常操作连接固定ECC服务，完成系统Prepare。主窗口应显示Prepared，没有正在运行或尚未收尾的Run，也没有故障、UNKNOWN或待核对保持。正在采集时先按正常流程停止并收尾；不要为了启用维护删除日志。
2. 在菜单栏选择`Selected CoBo maintenance...`，打开单板维护窗口。点击`Inspect fixed targets`读取服务端固定配置中的活动板清单。这一步只读元数据，不初始化板卡。
3. 核对`Original ECC`和`Snapshot`，确认是当前实验实例；同时核对Configuration和Artifact摘要。目标下拉框只列活动CoBo，不列停用板或MuTaNT。选择目标ID，并核对`Expected board name`，不要凭下拉框顺序推断物理板位置。
4. 点击`Reprepare selected CoBo...`。确认框默认是Cancel，取消不会建立维护请求。核对目标后选择Yes，操作台先保存本地意图，再提交一次维护。执行期间维护按钮以及主窗口普通控制按钮不可用，不通过反复点击解决等待。
5. 成功消息为`CoBo ... maintenance completed. System remains Prepared.`。这表示原操作确认完成且保持Prepared；并不表示波形或物理对齐已经正常。需要诊断时按独立诊断流程检查。
6. 如需设置单个指示灯，在仍为健康Prepared且全部数据进程停止时，选择AsAd编号、P/B/U/D灯和Off/On/Slow/Fast状态。确认框默认Cancel；核对完整目标后选择Yes只提交一次。界面忙碌期间Run控制与维护按钮禁用。成功提示`LED command acknowledged`只表示板端命令已确认，**不代表物理灯已点亮或处于所选节奏**。此单灯修改功能只允许硬件版本2或5。
7. 如需执行旧版`cobo-led-blink-all`的对应操作，选择固定活动CoBo并点击`Blink all LEDs once...`。确认框默认Cancel；确认后服务端只调用一次板端`blinkLEDs()`，由板端按其旧实现顺序依次闪烁所有灯一次。健康Prepared、数据进程停止、原实例身份和操作审计仍须通过；请求失败或回复丢失都不自动重发。成功只表示板端管理器应答，物理灯是否闪烁无法由软件确认。
8. 如需单次AsAd pulser诊断，在健康Prepared且全部数据服务停止时，选择固定活动CoBo并输入`-1000..1000 mV`。确认框默认Cancel，提示仅发送一次且物理脉冲无法由软件读回；核对目标和电压后才确认。界面忙碌期间Run控制和维护按钮禁用。成功消息只表示服务端确认该命令，不表示已测得脉冲、幅度或波形。该操作不会修改固定配置，也不会更改周期 pulser 的幅度/周期或启停状态。
9. 如需只Configure一块板，选择`Configure selected CoBo...`，再次核对固定目标及配置/工件摘要并确认。操作复用该板固定Configure计划，父ECC仍为Prepared，不启动采集；之后是否执行整机Configure和Run start由操作者另行决定。LED、blink、flip或pulser操作后可先执行此项。若超时、连接中断或结果未确认，保留本地记录目录及原服务审计目录。操作台按原operation_id查询，不自动生成新ID重发；失败或UNKNOWN必须按对应故障流程处理。flip、单次脉冲或blink应答未知时尤其不能重试，因为首次操作可能已在板端发生。
10. 如需设置旧版`cobo-led-pulse <ledId> <period_ms>`，选择GPIO LED位`0..15`或“All LEDs”(16)，输入半周期毫秒数并确认。`0`表示停止选中灯（或全板）的pulser；正值按旧板端`modifyLED`语义设置半周期。确认框默认Cancel，确认后使用同一operation ID至多提交一次；同ID修改LED或周期会被拒绝。失败或应答未知时只查询原操作，不重发。此入口只在健康Prepared、全部数据服务停止且互锁通过时有效，完成后仍为Prepared；物理灯态没有读回验证。

### GPIO状态翻转

旧`cobo-led-flip <led_id>`按当前GPIO值取反，不是设置成固定亮/灭状态，因此该动作非幂等：同一个flip执行两次会回到初始值。窗口下拉框明确映射一个GPIO位：`4 × AsAd编号 + 灯型编号`，灯型顺序为P/B/U/D，ID范围0–15；另有单独的“All LEDs”选项16。确认框默认Cancel，展示CoBo与GPIO位；取消不提交请求。用户确认后，该固定CoBo、GPIO位和原operation ID会被绑定并持久化，只调用旧板端单灯`flipLED(id)`或全灯`flipLEDs()`一次。

flip只允许原配置中的活动CoBo、健康Prepared状态、全部数据服务停止及原硬件互锁通过。它不重新准备其他CoBo，不改实验配置，也不通过GPIO写入覆盖操作替代旧管理器语义。板端应答不证明物理灯态；连接中断后只查询原operation ID，不生成新ID重试。因为硬件动作可能已发生但应答丢失，操作可能保持UNKNOWN并要求人工核对。

`Inspect fixed targets`读取的是不变的配置清单，不是板卡在线探测，也不解除故障。每次提交时工作线程和服务端仍会重新检查当前状态。若清单、身份或配置不一致，先检查所连服务与配置版本。

## 8. 单次pulser诊断的执行和审计

网络请求使用`command=trigger-asad-pulser`、`cobo_id`和有符号整数`voltage_mv`；范围为`-1000..1000`。请求键和持久原生操作绑定原ECC实例、快照、目标和电压。同一操作ID携带不同电压会拒绝；同一个原请求只会观察已有操作，不会再次执行。

Native ECC在进入操作前重查固定目标、Prepared状态、数据服务停止状态和硬件互锁。原生准备对象只构造一个`TriggerPulser`硬件步骤并只调用一次板端`triggerPulser(voltage)`。审计顺序是操作意图、硬件步骤begin、板端调用应答和完成记录。调用异常或应答/审计中断会保留UNKNOWN/失败，不自动重发。记录明确写`physical_pulse_verified=false`和`readback_verified=false`；软件没有物理脉冲传感器。

此入口属于低频控制面。Qt只处理目标、确认、请求和状态；原始事件数据仍完全走独立receiver路径。操作并不代替 Configure/Start/Stop 的周期 pulser 生命周期，也不开放自由修改周期参数。

## 9. 任意周期LED脉冲的执行和审计

网络请求使用`command=pulse-cobo-led`、`cobo_id`、`led_id`和`period_ms`。LED位为`0..15`，`16`表示全板；周期是旧板端接口使用的半周期毫秒数，范围为0至`INT_MAX`。0传给板端pulser接口时用于停止所选pulser。除该命令外，其他请求不得携带这些字段。

请求键将实例快照、CoBo、LED位和周期一并绑定，因此相同operation ID携带不同目标、位号或周期会被拒绝。服务端在执行前重新核对Prepared、数据服务停止、固定活动目标和互锁；状态机为Prepared→Prepared。Native ECC先持久化意图，再由SystemPreparationClient调用板端单灯或全板周期pulser方法一次。服务不自动重试，审计记录周期语义、零周期停止语义、硬件应答和`physical_lamp_state_verified=false`。

该控制操作不搬运事件数据。Qt只负责低频设置、确认、请求和结果显示；采集raw仍由独立高速receiver接收。控制线程中的Qt性能不能用来推断receiver数据吞吐。

## 10. 接口、进程与数据路径

| 位置 | 职责 |
| --- | --- |
| Qt窗口 | 展示目标和身份、默认取消确认、发出一次操作请求、展示原结果 |
| Qt工作线程与客户端 | 重新核对身份和Prepared条件，保存请求，认证POST及原ID只读查询 |
| ECC HTTP服务 | 认证、固定目标校验、原客户端归属、持久受理和互斥调度 |
| NativeEccCoordinator | 数据停止检查、目标绑定的原生操作日志、重准备/LED设置/blink/flip/LED周期脉冲/单次pulser维护调用 |
| SystemPreparationClient | 注销目标旧订阅、重新准备目标、执行单次LED设置/blink/flip/周期脉冲或AsAd pulser命令、保留其他板和系统对齐 |
| 独立数据服务与receiver | 正常采集时接收、落盘和封存raw，不经过维护窗口 |

只读`GET /v1/cobo-maintenance-targets`返回schema、kind、instance_id、snapshot_sha256、configuration_sha256、artifact_sha256及cobos数组；每项只有id和node_name。它沿用服务认证，响应上限128KiB、最多192个唯一目标。

`POST /v1/operations`的重准备命令使用`command=reprepare-cobo`和`cobo_id`；单板Configure使用`command=configure-cobo`和`cobo_id`，只在健康Prepared且数据服务停止时复用固定CoBo Configure计划，成功状态仍为Prepared；单灯LED命令使用`command=set-cobo-led`、`cobo_id`、`asad_id`、`led_type`和`led_state`；全板单次blink使用`command=blink-cobo-leds`和`cobo_id`；单次GPIO翻转使用`command=flip-cobo-led`、`cobo_id`和`led_id`；周期LED脉冲使用`command=pulse-cobo-led`及`cobo_id`、`led_id`、`period_ms`；单次pulser命令使用`command=trigger-asad-pulser`、`cobo_id`和`voltage_mv`。GPIO位范围为0..15，16代表全部；周期为半周期毫秒，范围0..`INT_MAX`；AsAd范围是0..3，灯型/状态使用上述固定枚举，单次pulser范围是`-1000..1000 mV`；其他命令不得携带这些字段。同一操作ID和完全相同请求可返回原操作；同ID更改目标、LED状态、翻转位、LED脉冲周期或pulser电压返回412。错误Prepared/停止前提返回412，忙碌或归属冲突返回409。丢回复后使用`GET /v1/operations/<原operation_id>`观察，不把新POST当作查询，也不自动重发。

目标ID参与网络请求键和原生维护请求键，因此C0和C1不能共用一条成功记录。运行包交付、实际平台及实机测试状态应查验证记录，不能用协议或窗口单元测试代替。

## 11. GPIO flip 回归测试

`cobo-maintenance-window-0/1`使用生产Qt主窗口、生产`NativeRunControl`、ECC HTTP服务、两份数据Supervisor、receiver和模拟CoBo验证完整交互。两种场景分别覆盖兼容硬件API 0/1。测试先取消一次flip并确认没有请求或持久意图，再确认GPIO位10，检查界面只提交一个operation ID、请求键包含CoBo和位号、ECC在Prepared状态只调用所选CoBo一次、其他管理器不变；同一operation ID改为位11必须返回412，原请求仍可查询。完成后继续两轮Configure/Start/Stop/Breakup以检查系统生命周期。

Qt探针由`tests/cobo_maintenance_window_probe.pro`单独构建，不让DAQ接收器链接Qt。先在独立临时目录构建探针，再配置原生网络测试。CMake配置时将`GETDAQ_NATIVE_QT_PROBE`指向该可执行文件；只有路径存在时才注册两个Qt CTest用例：

```bash
mkdir -p /tmp/getdaq-build/console-probe
cd /tmp/getdaq-build/console-probe
qmake /absolute/path/GetDAQ/tests/cobo_maintenance_window_probe.pro -o Makefile \
  'GETDAQ_DESTDIR=/tmp/getdaq-build/console-probe/bin' \
  'OBJECTS_DIR=obj' 'MOC_DIR=moc' 'UI_DIR=ui' 'RCC_DIR=rcc'
make -j6
cmake -S /absolute/path/GetDAQ/third_party/ice-compat -B /tmp/getdaq-build/native \
  -DBUILD_TESTING=ON \
  -DGETDAQ_BUILD_ICE_RECEIVER=ON \
  -DGETDAQ_ICE_DATA_ONLY=OFF \
  -DGETDAQ_BUILD_ECC_SERVICE=ON \
  -DGETDAQ_BUILD_SUPERVISOR_SERVICE=ON \
  -DGETDAQ_RECEIVER_EXECUTABLE=/absolute/path/getdaq-receiver \
  -DGETDAQ_NATIVE_QT_PROBE=/tmp/getdaq-build/console-probe/cobo-maintenance-window-probe
cmake --build /tmp/getdaq-build/native --target ecc-network-service-tests getdaq-ecc-server getdaq-supervisor
cd /tmp/getdaq-build/native && ctest -R '^ecc-network-service-cobo-maintenance-window-' --output-on-failure
```

这是本机回环模拟测试，不访问真实板卡或外网，不证明物理灯态。测试运行会在自己的临时证据目录生成少量合成`.graw`；测试后需检查总大小低于20GiB并删除这些raw文件，保留操作JSONL和结果证据即可。不要在用户DAQ输出目录执行此测试。
