# 全系统采集：连接、启动、暂停、恢复和停止

## 1. 这部分解决什么问题

系统已完成 Prepare/Configure 后，硬件仍不会因为“配置成功”而自动开始采集。必须先为板端建立数据连接，再按正确顺序重置 CoBo 流水线、启动发送器、开放固件和启动 MuTaNT。结束时则要先停止产生数据，再刷新读出，最后断开数据连接。

新增原生 C++11 库 `SystemAcquisitionClient` 将这些命令组合起来。它接续**同一个已 Configured 的 `SystemPreparationClient`**，沿用不可变参数及长期保存的告警监听器。它不接收、不复制、不转发原始 MFM；没有 Qt 依赖，没有新增第三方运行库。

这是原生后端的库接口，本身不提交ECC状态。现由NativeEccCoordinator连接持久状态、固定网络入口和本机/远程receiver生命周期，GUI原生模式只调用全局控制接口，旧SOAP模式保留。实际控制权/加载版本、后台告警自动调度、硬件UNKNOWN恢复和真机验收仍待完成。注册测试使用本机模拟板端，不能证明硬件、持续吞吐或目标平台兼容。

代码入口：

- [运行协调接口](../apps/hardware/systemacquisition.h)与[实现](../apps/hardware/systemacquisition.cpp)。
- [CoBo/MuTaNT 命令计划](../apps/hardware/acquisitionplans.cpp)。
- [本机 TCP 测试](../tests/hardware_system_acquisition_test.cpp)与[旧函数独立对照](../tests/system_acquisition_original_reference.py)。
- 前置流程：[系统 Prepare/Configure](SYSTEM_PREPARATION.md)、[完整实验工件](SYSTEM_EXPERIMENT_CONFIGURATION.md)。
- 发送器与数据文件：[发送器控制](DATA_SENDER_CONTROL.md)、[接收器操作](DATA_SERVICE.md)、[实际验证记录](VALIDATION.md)。

## 2. 为什么控制软件和数据软件必须分开

~~~mermaid
flowchart LR
    UI[Qt 配置与信息界面] -->|低频控制/状态| ECC[原生 ECC 后端]
    ECC --> P[同一 SystemPreparationClient 与告警]
    P --> A[SystemAcquisitionClient]
    A -->|低频寄存器及管理器命令| H[CoBo / MuTaNT]
    A -->|低频连接和启停命令| S[板端 DaqCtrlNode]
    H --> S
    S -->|独立普通 TCP：原始 MFM| R[原生 getdaq-receiver]
    R --> D[graw 与封存文件]
~~~

Qt 负责配置和查看信息。原生协调器负责发送有限数量的控制命令；板端直接向原生接收器传送原始字节。命令日志的 fsync、GUI 绘图、JSON 参数和 Ice 控制连接都没有进入每帧数据的接收路径。

这不等于已经达到某个带宽。真实瓶颈仍可能在板端 DDR/中断、网卡、内核 TCP、接收队列或磁盘。大于页缓存规模的持续性能、多板并发及端到端事件数需要单独验收。

## 3. 调用之前必须理解的条件

1. 同一个准备对象已经依次成功完成 Prepare 和 Configure，仍然存活。运行对象保存它的引用，准备对象必须更晚析构。
2. 原生后端已经安排好接收器监听、目录与运行归属。ConnectData 会让板端主动建立数据 TCP，当前库不启动或检查接收进程。
3. 调用者提供与准备配置对应的发送器 ID、控制代理和接收地址，并声明独占、停止前提。这些声明不建立真正的远端锁。
4. 现场代理与物理板的对应关系已核对。旧 DaqCtrlNode 没有远端名称、所有权或完整运行状态查询；成功的 ice_isA 只能证明接口身份。
5. 所有运行命令进入同一串行控制队列；不能同时从另一线程更改准备对象、订阅或现场硬件。

构造函数只复制配置、编译计划、检查发送器集合，不发送板端命令。未 Configured 的准备对象立即拒绝。

集合绑定按配置的实体 ID 匹配，恢复配置中的节点顺序：

| 节点 | 必须提供的发送器 | 缓冲区掩码 |
| --- | --- | --- |
| 活动 CoBo | 每块一个 | 必须等于本次编译的活动 AsAd 掩码 |
| 停用 CoBo | 不提供 | 不连接、不启动；仍观察其固件/中断停止和 initDone=0 |
| readoutEnabled=true 的 MuTaNT | 每块一个，包括从模块 | 1 |
| readoutEnabled=false 的 MuTaNT | 不提供 | 不连接 |

MuTaNT 从模块的连接策略保持收到的系统 Configure 行为：若开启读出就连接，但 Start/Resume 不启动它的 sender。主模块只有任一 level0/1/2 触发开启时才设置 run；它的 sender 还要求 readoutEnabled=true。全部触发关闭的主模块保持停止，连接则保留到最终 Disconnect。

缺少/额外/重复 ID、活动范围不符、掩码错误、编码与准备对象不一致，在任何网络写入前拒绝。地址和代理的严格范围、端点去重及物理别名限制沿用发送器手册。

## 4. 状态和动作对应关系

这里的 Ready/Running/Stopped 是**本对象命令阶段**，不是已提交的生产 ECC 状态或实际硬件验收结论。

| 当前状态 | 可调用的动作 | 成功后的状态 | 数据连接 |
| --- | --- | --- | --- |
| Configured | ConnectData | Ready | 建立，尚未启动读出 |
| Ready | Start | Running | 保留 |
| Stopped | Start | Running | 保留原连接，重新RunInit |
| Running | Pause | Paused | 刷新并停止读出，连接保留 |
| Paused | Resume | Running | 使用同一 sender 和连接 |
| Running | Stop | Stopped | 刷新并停止读出，连接保留 |
| Paused | Stop | Stopped | 再次观察停止前提，不重发停止命令 |
| Ready / Stopped | DisconnectData | Closed | 显式断开 |
| Failed | StopProductionAfterFailure | 仍为 Failed | 不刷新、不清理、不断开未知 sender |
| Configured / Ready / Running / Paused / Stopped；父ECC已UNKNOWN | StopProductionAfterFailure(path,true) | Failed | 原对象只停生产，保留已有或尚未开始的连接记录，不触碰sender或清UNKNOWN |
| Ready / Running / Paused / Stopped / Closed | Breakup | Broken；同一准备对象 Prepared | 按阶段停止、刷新并断开；Closed不重复断开 |

Connecting/Starting/Pausing/Resuming/Stopping/Disconnecting/Breaking 是执行中的阶段。所有变更方法由运行对象自己的 mutex 串行化；GetState 为原子快照，SenderStatus 只返回快照，不暴露可绕开队列的子客户端。

Breakup清除配置有效标志但保留原准备对象、告警和对齐掩码；Broken对象不可再次运行。之后同一不可变参数可重新Configure，或者在本次核对数据进程停止后Unprepare。实际保护观察、断电、自有注销及失败边界见[结束与撤销准备](SYSTEM_RELEASE.md)。ConnectData记录数据链审计目录，Breakup引用同一目录，不创建第二套sender连接。

**Closed 不等于接收文件已完整封存。** 断开成功后仍须等待每个接收器 EOF、尾帧检查、排空/fsync、正常退出、有效 seal 和目录锁释放，才能归档。普通 TCP 的 Stop 保留 socket，详见发送器手册中的旧源码依据。

同一个运行对象只能执行一次 ConnectData，Closed 后不能重新连接或开启新 Run。新 Run 必须由生产后端重新核对运行归属、接收器、硬件与配置；不能用“新建对象”来清除 UNKNOWN。

## 5. ConnectData 和运行前的全集合检查

ConnectData、Start、Resume 先检查保持告警。在任何 RunInit 写入或连接命令之前，检查全部节点：

- 实际 CoBo/MuTaNT 名称与绑定端点一致。
- 所有后续 RunInit、流水线、快控制、周期 pulser、停止及温度目标的远端描述、位宽、布局和本地权限。
- 所有参与连接的 DaqCtrlNode 的实际远端 ice_isA。
- 活动 CoBo 的 firmwareEnable=0、intEnable=0、initDone=1；停用板前两项为0、initDone=0。
- 所有 MuTaNT 的 TRIGGER_CTRL.run=0。
- 活动 CoBo 的 memStart/memEnd 与本次不可变 Configure 参数一致。

后一块板的元数据、停止观察或内存范围错误，不能留下前面板的 RunInit 写入。全部通过后才连接第一块板；运行前再核对每个本地 sender 阶段。首次 Start 要求 Connected；Stopped后的Start与Resume都要求真正使用过的sender为Stopped，未使用的MuTaNT sender仍为Connected。再次Start重新RunInit；Resume不重置本次计数边界。

这些观察是本次时刻的读回，不能防止另一控制者随后改动硬件。它们也没有验证全部配置寄存器或给出实际已加载配置摘要；根日志明确 actual_loaded_configuration_verified=false。

## 6. 首次 Start 的详细时序

### 6.1 所有活动 CoBo 先 RunInit

先依次读取各活动 AsAd 的内部/外部温度原始值，再按 useExternalTimeReset 策略决定是否写 resetTime=0。所有 CoBo 的这部分完成后，才启动第一块板。

温度保留原始0..255值，非零前最多读取10次；全零也记录原始观察，不当成健康证明。本阶段没有复用旧温度代码的符号位转换表达式，也没有根据未经实机核对的值判断温度正常。

### 6.2 逐块 CoBo 启动

每块板依次执行：

1. firmwareEnable=0 和 intEnable=0，普通字段写后读回。
2. pipeCtrl.reset=1，backpressure=1。
3. 再次核对实际 memStart，然后将 readPtr 复位到已编译的对应起点，写后读回。
4. sender.daqStart。此应答没有证明最终 graw 完整。
5. backpressure=0，reset=0，intEnable=1，firmwareEnable=1。
6. 切换 AGET 快控制：agetController.reset=1/0；SC_En=0；pulseEnable=活动 AGET 掩码；hitRegister=1；calibration=0；aget_sc_rw=1；等待100ms；SC_En=活动掩码；pulseEnable=0。
7. 若配置 periodicModeEnabled=true，调用板端 startPeriodicPulser。

reset、resetTime、aget_sc_rw 等命令不要求立即回显；普通状态字段保留读回检查。100ms 等待保留旧顺序，并在等待分段检查保持告警；不是网络/线程调度的硬实时保证。

readPtr 有 per-AsAd 四块与 shared 单块两种明确布局。新实现不会先写四块、失败后再猜共享布局，也不会把被其他控制者改变的 memStart 接受为新配置。硬件描述和准备/配置策略必须一致。

### 6.3 MuTaNT 从模块先于主模块

所有活动 CoBo 完成后：

1. 按配置顺序逐个从模块 RunInit：只执行启用的 scaler/event 计数清除；然后设置从模块 run=1。
2. 主模块执行 RunInit。两种清除都关闭时只保留该计划的停止观察，不清计数器。
3. 主模块有触发且开启读出，先启动主 sender。
4. 主模块有任一触发时才设置 run=1。

run 使用描述中的字段位置；收到的夹具 run 位在 TRIGGER_CTRL 的第15位，不按第0位猜测。Slave 不启动自己的 sender；没有 MuTaNT 的独立 CoBo 模式跳过上述全部动作。

## 7. Pause 和 Resume 会保留什么

Pause 按下一节的停止次序停止硬件和读出、刷新余量，**保留 TCP 连接、sender 对象、准备对象、配置和告警监听器**。

Resume 再检查全部节点，重新执行 CoBo 流水线复位、读指针复位、sender 启动、固件开放与快控制，然后从模块、主模块启动。它不重新执行 CoBo resetTime 或 MuTaNT RunInit，保持收到的 SystemManager::resume 行为。

特别注意：Resume 仍会复位 CoBo 的读/写流水线指针。当前没有证明未知残留数据可以安全恢复，因此只能从成功 Paused 进入；Pause 失败必须保留 Failed，不能假装暂停成功继续跑数。

## 8. 正常停止分成三个阶段

### 8.1 先停止产生数据

先设置主 MuTaNT run=0，再依次设置从模块 run=0；之后逐活动 CoBo 停周期 pulser、firmwareEnable=0、intEnable=0。

每个独立停止阶段只尝试一次。某个阶段失败后，继续尝试其他独立生产停止目标，不重发同一个失败动作。如果其中任一阶段未确认，保留 Failed 和全部发送器连接，不继续 sender flush 或 Disconnect。

“已确认”只指本次命令应答/相应字段读回。板端管理器的应答不证明周期任务或实际电气信号已经停止，仍需现场观察。

### 8.2 再停止读出并刷新

只有全部生产停止阶段确认，才停止真正使用过的主 MuTaNT sender，然后逐活动 CoBo sender.daqStop。某个 sender 停止应答丢失时，继续尝试其他独立 sender 的停止，但不重发失败 sender、不 Disconnect，也不声称封存。

生产与刷新都成功后，读取 AsAd 温度作为附加信息。温度读取失败单独记录，不把此前明确完成的生产停止和刷新改称未执行。

### 8.3 最终断开和接收封存

Stop 成功后仍保留连接。DisconnectData 再核对名称、元数据、停止观察和完整 sender 集合，才逐个发送 disconnect。中途失败保持 Failed，不继续后面的节点。

接收器自己的正常退出、尾帧、数据 seal 和目录锁是另外一组证据。所有连接断开前，尤其是已经连接但未启动读出的 MuTaNT 从模块连接仍打开时，不能因为活动 CoBo 停止就归档整个 Run。

## 9. 启动途中失败后如何停止生产

某次 Start/Resume 已发出部分命令后出现 UNKNOWN 或保持告警，协调器保持 Failed，不自动回滚、停止、重新连接或重发。

生产后端可以响应**显式停止请求**调用 StopProductionAfterFailure：

- 不受保持告警的启动 checkpoint 阻断。
- 按主/从 MuTaNT、CoBo pulser/固件/中断的生产停止次序处理独立目标。
- 本次运行边界内已尝试的停止阶段不再次发送；已知成功者保留成功记录，先前未确认者仍不确认。
- 不触碰未知 sender，不调用 sender Stop/Disconnect，不清除 UNKNOWN，不提交 ECC Ready/Stopped。
- 即使全部生产停止命令确认，对象仍为 Failed，日志写 recovery_required=true。

这使“停止进一步产生数据”与“恢复未知读出状态”分开。例如 C0 sender Start 应答丢失，可以显式停硬件生产；C0 sender 仍是 UNKNOWN，连接和数据残留必须另行核对。若 firmwareEnable=0 的停止写入已生效但应答丢失，后续调用不会再次写同一停止；恢复网络也不能自动证明那次停止。

原生ECC协调器还处理“子运行步骤成功，父完成日志失败”的情况：父持久状态已UNKNOWN时，可显式传入`parentOutcomeUnknownConfirmed=true`，允许原Running/Paused/Stopped子对象只停生产并保持Failed。默认false仍只接受Failed；这个参数不是普通Stop或解除故障入口。具体父操作ID、审计和恢复门槛见[原生ECC协调器](NATIVE_ECC_COORDINATOR.md)。

该入口是一次失败后的生产停止请求，**不是完整恢复工具，也不是自动告警消费线程**。Running 期间的后台告警停机调度、真实控制权核对与跨进程恢复仍要接入生产 ECC。

## 10. 日志怎么阅读

每个动作接收一个全新目录。父目录由调用者安排；动作目录0700、文件0600，不覆盖已有文件。每条硬件/发送器意图先持久化，才发送动作。

典型结构：

~~~text
run-control/
  connect/
    system-run.jsonl
    step-0.jsonl ...
    data-links/
      data-links.jsonl
      connect-0.jsonl ...
  start/
    system-run.jsonl
    step-0.jsonl ...
  pause/
  resume/
  stop/
  disconnect/
~~~

system-run.jsonl 包含操作、输入 parameter/artifact SHA、step_begin 与子文件、step_complete、最终阶段或 failed；根文件最多16MiB。具体步骤/观察/已尝试/应答在子审计中。发送器集合目录的实际子文件名以根日志为准。

control_lock_verified、actual_loaded_configuration_verified、production_ecc_state_committed、receiver_sealed 均明确为 false。不能把输入 SHA 当硬件实际版本，也不能把最后一行 complete 当数据文件封存。

已有目录或根意图无法建立时，当前动作没有硬件命令；保留此前阶段。开始动作后的日志/网络/元数据/观察失败则保持 Failed，检查子文件判断实际已发出哪些动作。析构只释放本地资源，不发送隐式 Stop/Disconnect；也不能因为对象已析构就判定现场停止。

## 11. 后端接法与构建测试

示例只展示成功路径库组合，前提是接收器及准备对象已经由完整后端正确安排：

~~~cpp
#include "systemacquisition.h"

// prepared 更早构造、更晚析构；auditRoot 已存在且由本次运行独占。
void RunConfigured(GetDAQ::SystemPreparationClient &prepared,
                   const std::vector<GetDAQ::DataSenderOptions> &senders,
                   const std::string &auditRoot)
{
    GetDAQ::SystemAcquisitionClient run(prepared, senders);
    run.ConnectData(auditRoot + "/connect");
    run.Start(auditRoot + "/start");
    // 实验正在运行；控制队列可以处理下列请求。
    run.Pause(auditRoot + "/pause");
    run.Resume(auditRoot + "/resume");
    run.Stop(auditRoot + "/stop");
    run.DisconnectData(auditRoot + "/disconnect");
    // 后端还须验证全部receiver的EOF、退出与封存，再归档。
}
~~~

生产代码必须在对象外捕获并保存错误，保留同一运行对象与准备对象供显式停止/核对。不能复制示例后在异常栈展开时遗失唯一的状态对象。

在仓库根目录构建和验证：

~~~sh
cmake -S third_party/ice-compat -B build-ice-compat -DBUILD_TESTING=ON
cmake --build build-ice-compat -j 3
cd build-ice-compat
ctest -R '^hardware-system-acquisition-tcp$' --output-on-failure
cd ..
python3 tests/system_acquisition_original_reference.py build-ice-compat/system-acquisition-audits.jsonl
~~~

测试会启动127.0.0.1模拟 Ice TCP 服务、使用收到的完整 CoBo/MuTaNT 描述，先真正执行原生 Prepare/Configure，再测试运行命令。测试目录在/tmp，未连接现场板。旧函数对照需要 Python 和标准 C++ 编译器，仅用于验证，不成为运行依赖。

当前原生兼容构建以 POSIX 为前提，实际开发环境 Ubuntu20.04.6/GCC9.4；Ubuntu24、macOS 仍需实测。Windows Qt5.14.2 MinGW GUI 构建独立，不要求把原生硬件后端加入 GUI 或原始数据链路。

## 12. 与收到的旧实现有哪些明确差异

| 项目 | 收到的旧行为 | 新运行协调 |
| --- | --- | --- |
| readPtr 布局 | per-AsAd 写失败后尝试共享 | 构造时明确选择，范围先核对 |
| 内存起点 | 直接复制当时远端 memStart | 必须与本次不可变 Configure 相符 |
| 停 MuTaNT 主模块 | sender Stop 后才清 run | 先停止全部生产，再停止/刷新 sender |
| CoBo/各 sender 停止 | 单节点内部交错执行 | 全部生产停止确认后才进入 sender 阶段 |
| 周期 pulser 故障 | 部分位置捕获记录后继续 | 启动停止保持失败，不提交成功阶段 |
| Stop 的温度 | 在固件停止前读取 | 生产/读出停止完成后读取附加信息 |
| 失败清理 | 部分 Ice 异常只警告 | 记录实际边界、保留 Failed/UNKNOWN，无自动重发 |
| 停用 CoBo 连接 | 系统旧循环可能仍连接 | 不连接；观察停止与 initDone=0 |

保留的主机顺序由真实旧 SystemManager 的 start/pause/resume/stop 以及 CoBo 的 RunInit、读指针、impl_start、postStart 和两种快控制重载独立提取对照。刻意变化不冒充逐指令完全相同；接收数据、温度转换、真实时钟/中断和物理效果不由该参考证明。
