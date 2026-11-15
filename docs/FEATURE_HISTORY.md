# 功能专题与阶段说明索引

本页保留此前README的阶段说明及专题链接，供追溯与深入阅读。各段所述“尚未完成”是写入当时的范围，后续可能已接续；当前能力以[重构进度](REFACTOR_STATUS.md)为准，测试范围以[验证记录](VALIDATION.md)为准。日常安装与运行请从[项目入口](../README.md)开始。

新增[父ECC与数据节点一起更换计划](ECC_PLAN_TRANSITION.md)：retire-submit/retire-inspect先由原ECC保存各节点退休意图，再核对原结果；各节点按新计划启动后，新ECC验证前驱证明，从Fresh明确开始新周期。当前要求保留节点和控制端点，GUI与部分/冷切换恢复仍待。

新增[数据服务更换接收计划](DATA_PLAN_TRANSITION.md)：正常封存后的原服务明确退休，新进程按审阅计划和一次性证明接班；新队列、最大帧和轮转参数已做真实receiver回环验证。激活后旧数据目录可独立归档。父ECC不同配置编排、连续板卡占用移交和GUI仍待接入。

新增[旧ECC服务交接与独立接班进程](ECC_SERVICE_HANDOFF.md)：正常收尾后由原客户端明确交接，原生工具保存请求、单次提交并只读核对原回执，新的ECC进程使用固定计划启动。支持保持原实验和接收计划的实例更替；完整实验版本切换、连续板卡占用移交及GUI仍待接续。

当前状态与未完成范围见[重构进度](REFACTOR_STATUS.md)，执行证据见[验证记录](VALIDATION.md)。Qt操作台负责配置、控制与小型状态；原始DAQ数据由独立原生程序接收和写盘。板端getHwServer/IceEirfu保留现有部署。

发生接收故障后，按[原采集清理与只读恢复说明](ACQUISITION_RECONCILIATION.md)完成原对象收尾、确认恢复、再手动新Prepare。原Run保持中断、原数据保留，新采集须选择新的未占用目录；恢复不自动推进编号或补封存。普通接收恢复仍拒绝独立告警或ICE源UNKNOWN；对于原接收故障之后同一原采集的独立告警，新增明确绑定原告警停止ID的确认入口。Prepare阶段告警另有[明确确认与原收尾核对入口](PREPARATION_RECONCILIATION.md#9-prepare-告警策略已保持时的明确确认)。普通已知Ready告警可明确执行[正常连接/接收封存/断电收尾](ALARM_STOP_POLICY.md#10-普通告警已经成功-stop-后怎样明确收尾)，原Run仍中断。对于提供可选恢复接口的受控主机端ICE源，新增[原源退休与同一采集明确恢复](FAILED_ICE_SOURCE_RECOVERY.md)：原线程停止后关闭原数据连接，不发送End、不补封存，原Run仍中断，之后人工新Prepare。冷对象恢复仍待，详见各自适用范围。整套重构和现场/目标平台/持续吞吐验收仍未完成。

新增[下一ECC实例的离线规划](ECC_SUCCESSOR_PLANNING.md)：`getdaq-ecc-server --config ... --plan-config` 使用与真实启动相同的编译过程计算快照和数据控制范围，不创建运行审计或取得板卡占用。规划本身不切换配置、不授权接管。

现可[保存接班身份并一次性用于启动](ECC_SUCCESSOR_IDENTITY.md)：冻结新实例的数据持有期、快照与输入，启动还须核对原交接回执。完整ECC切换与GUI待接续，保存计划不代表已交接。

配置切换所需的[数据控制权明确交接](DATA_CONTROL_HANDOFF.md)已接入原生数据服务、CLI和ECC多节点适配器：原控制者确认正常封存后指定新适配器，新适配器核对原回执才可开始新会话。原适配器永久保持门槛，丢应答不重发；完整配置版本切换、板卡主机占用交接和GUI入口仍待接续。

ECC 与操作台现已接入 [清理原失败接收会话](ECC_FAILED_RECEIVER_CLEANUP.md)：要求原保留采集对象和完整停生产审计，核对各数据节点原实例/PID/会话，明确确认后一次提交。原始文件、UNKNOWN 和未完成 Run 保留；这一步不解除硬件故障或自动重新采集。

新增 [已失败接收会话受控终止](FAILED_RECEIVER_SESSION_ABORT.md)：数据服务与原生 CLI 可按原控制者/会话明确终止自有失败接收进程，保留原始文件、未完成状态和核对门槛。同 ID 不重发信号，重启不接管旧 PID；它不代替 ECC 停生产或完整 UNKNOWN 恢复。

新增 [多文档配置工作区与 Save All](CONFIGURATION_MULTI_DOCUMENT.md)：在内存编辑多个根及共享 include，统一审阅所有已打开根的变化后保存全部；来源漂移和过期审阅拒绝，部分保存失败明确列出结果。使用 MUSIC 风格 `.h/.cpp/.ui`，Qt 仍只用于配置与控制，不进入原始数据链路。

新增 [Configure 寄存器读回证据](CONFIGURE_READBACK_EVIDENCE.md)：从原执行保留的审计摘要核对已完成主计划，分别列出读回比较、未完整读回写入、管理器应答与警告；提供认证缓存查询和无Qt离线检查。它是历史执行证据，不将文件版本或命令应答冒充实际板端加载版本。

原生ECC新增外部输入故障下的原对象停生产路径：普通请求在完整审计中失败且不执行硬件，明确负向动作仍核对原控制记录/对象/占用保护，保留UNKNOWN，不热加载或接管旧进程。操作与拒绝条件见[故障停生产说明](ECC_NETWORK_SERVICE.md#14-外部固定输入损坏后原对象明确停生产)。

配置窗口现在可保留CDATA及值前后注释/处理指令修改已有参数，准确撤销重做和全部来源/共享范围检查仍保留；详细规则见[配置编辑说明](CONFIGURATION_EDITOR.md#保留cdata和首尾标记修改值)。

中断副本可通过 **Resume original partial copy...** 明确续传：原生子进程先核对保存的来源身份和全部已有目标前缀，再追加缺失字节并完整校验，不截断文件。Qt只提交意图和查看状态，操作与拒绝条件见[副本续传说明](CATALOG_TRANSFER.md#11-中断副本的明确续传)。

数据复制窗口新增 **Verify unresolved original copy...**：服务重启后，对unknown原任务明确启动独立原生进程完整核对已有副本；成功后才解除该任务门槛，不重新复制、接管旧PID或改变Run。操作与失败处理见[复制任务人工核对](CATALOG_TRANSFER.md#10-服务重启后的原复制任务人工核对)。

操作台的 **Live hardware control bits...** 页面可人工查询原生ECC认证服务读取的CoBo/MuTaNT固定控制位，并显示节点采样时间；失联、选择改变、命令并发或六秒过期会撤回值。Qt只接收小型元数据，不承担DAQ数据传输；查询不写寄存器或清UNKNOWN，也不证明当前配置已加载。地址、凭据、字段和局限见[实时控制位操作说明](LIVE_HARDWARE_STATE.md)。

Qt 操作台的 **Configure register readbacks...** 页面可按板卡查看这些历史计数、原操作和摘要，采用 MUSIC 熟悉的 `.h/.cpp/.ui` 结构；打开时低频刷新，过期/失联撤回旧板卡表，断开前回复不进入新连接，不发送硬件或原始数据请求。

ECC 新增可选 [同机板卡占用保护](HOST_CONTROL_OWNERSHIP.md)：共用目录的新实例按板卡 IPv4 地址互斥，冲突在硬件调用前拒绝；提供认证只读查询，完整性变化保持 UNKNOWN。板端全局/跨主机控制权及配置实际加载仍待完成。

操作台可通过 **ECC host board reservations...** 只读查看同机占用、资源地址和故障；仅在页面打开时低频刷新，过期/失联不保留旧确认，旧连接回复不进入新页面。该查看不执行控制命令或经过原始数据通路。

主机端现在有[统一构建与部署入口](HOST_SUITE_BUILD.md)：`python3 tools/build-suite.py --profile full` 编译完整主机套件，`data` 构建不需要 Qt/Ice 的原生数据工具；可打包到新目录并核对文件哈希。Windows 仍可直接用 Qt Creator 打开 `GetDAQ.pro`，现已包含配置上传工具。板端 `getHwServer` / `IceEirfu` 保留现有部署，不在本次重构范围内。

原生硬件/ECC程序的拓扑与控制日志现统一使用标准C++11严格JSON，原生构建已移除yaml-cpp；操作台仍嵌入本地yaml-cpp兼容envoy YAML。已有规范日志保持，坏类型/重复键拒绝且不自动修复或重放，见[原生JSON依赖迁移](NATIVE_JSON_DEPENDENCIES.md)。

配置窗口可[查看编译好的硬件描述](HARDWARE_DESCRIPTION_VIEW.md)：设备/寄存器/位域按需展开，64位地址与主机权限精确显示。描述解析已迁到共享纯C++11/严格JSON模块，GUI和原生控制复用，不再通过yaml-cpp读取描述；查看不读寄存器或证明实际加载。

配置窗口新增[结构编辑与有效子树展开](CONFIGURATION_STRUCTURE_EDITOR.md)：十种本机操作包含新增、复制、删除、排序和类型/单位/范围编辑。保存前显示所有已加载实例的差异，并重新核对来源和保存后的快照；连接模式只读，不直接部署硬件。

已实际构建收到的旧 CompoundConfig/Spirit 文件解析实现，与新配置模型运行对照，并修正普通XML空白、CDATA和字符引用的读取与准确保存。Boost只保留为离线测试参考依赖；兼容范围、有意扩展/拒绝及逐步操作见[旧配置库对照与文本迁移](LEGACY_CONFIGURATION_RUNTIME.md)。

配置窗口和CLI现可导出[自包含XML副本或另存后编辑](CONFIGURATION_XML_EXPORT.md)：选择保留默认声明或固定当前具体有效对象，先审阅、核对来源、拒绝覆盖；旧导出方法有实际运行对照。剩余多文件工作区/来源语义等列在[命令迁移表](CONFIGURATION_COMMAND_MIGRATION.md)。

继承对象现在可[浅层覆盖](CONFIGURATION_OBJECT_OVERRIDE.md)：补齐空对象、缺失祖先及所需同名模板兄弟，保持有效值，并列出共享include的所有受影响投影。旧模型真实覆盖/撤销/重做已运行对照；仍不代表多文件工作区或整套实机验收完成。

新增[保留include的工作区副本与引用隔离](CONFIGURATION_WORKSPACE_COPY.md)：向新目录复制所有已加载XML，选择保持共享文件或逐次引用独立复制，改为副本内相对路径并核对有效值一致。可另存后打开继续编辑，原来源不改；它是可编辑副本，不能当成已加载硬件或不可变发布包。

原生接收器现已省去完整帧的重组复制，跨块帧继续有界缓存。普通及内存检查构建通过本机TCP、文件SHA与封存验证；测试每轮合计最多20GiB，生成数据自动删除。适用范围、基准解释与复现方法见[接收器性能说明](RECEIVER_PERFORMANCE.md)。

新增独立[配置仓库](CONFIGURATION_REPOSITORY.md)：操作台可上传已校验的发布包、登记和完整检查版本；另有无Qt仓库核心、低频HTTP服务和Qt上传CLI。提交回复丢失后观察同一版本，避免自动重发。它不传原始DAQ数据，也不自动加载板卡；服务器构建、Qt 5.14.2 MinGW操作、认证、恢复和资源边界见手册。

仓库窗口现可导出[审阅后的ECC配置选择](ECC_CONFIGURATION_SELECTION.md)，原生`getdaq-ecc-select`生成独立启动配置，并绑定仓库收据/清单/工件。保留模板审计根，已有历史与新绑定不符时启动仍拒绝；GET `/v1/configuration`只展示接受的缓存，实际硬件加载标记保持false。

原生操作台新增 **Accepted ECC configuration...** 只读查看；可显式启用`native_configuration_status`，在Start之前把已绑定的启动配置存入Run，收尾保存`ecc-configuration.json`，恢复时核对同一记录。界面继续标明文件健康未实时检查、硬件加载未核对；见[配置查看与Run记录](ECC_CONFIGURATION_VIEW.md)。

C++ 重构的 GET DAQ 工具集：Qt 配置与控制台、节点 sentry、联调模拟服务，以及独立的无 Qt 原生数据接收/导出与离线检查程序。控制代码采用 MUSIC 熟悉的 `.pro`、`.h/.cpp/.ui`、信号槽和中文注释；MUSIC 目录作为参考保留，未修改。

目标环境：**Ubuntu 24.04；Windows GUI 使用 Qt 5.14.2 + MinGW**。控制程序使用 C++11、Qt 5.12 及以上 API。原生接收器使用 C++11/POSIX、独立 CMake 构建，不链接 Qt；Linux 已测试，macOS 待实际验证。GUI 不转发原始事件。

本地配置页面现在解析按元素名共享的单位和范围，分别显示值/单位/范围来源，并在保存时检查共享约束及全部已加载文件。配置编译入口复用同一解析；不做单位换算或直接下发硬件。include规则、具体例子和仍缺的范围语义见[配置页面说明](CONFIGURATION_EDITOR.md)。

当前已实现并测试控制/监控/归档、原生 plain TCP MFM 接收和可选在线导出；**尚未完成全部 ECC/Ice 硬件后端重构和实机兼容验收**。可选 Ice 兼容组件已能离线编译并通过 GET 接口的本机 TCP 测试，尚不等于完整硬件后端。逐项状态见 [重构进度](REFACTOR_STATUS.md)，实际验收见 [验证记录](VALIDATION.md)。

Prepare 部分失败、尚未创建采集对象时，原生 ECC 现在可通过原准备对象明确关闭生产源。停止结果不清 UNKNOWN，不断开数据连接或注销告警，已尝试阶段不会因换操作 ID 重发。适用条件、GUI/HTTP 操作和日志逐项解释见[准备故障停止手册](PREPARATION_FAILURE_STOP.md)。

固定ECC服务另可明确启用后台告警策略：消费自有CoBo回调，持久记录一次停止，保留原连接/receiver并保持告警门槛，重复通知和重启不重放。默认未启用，不依赖外部`#STOP#`日志框架。GUI可独立启用native_alarm_status查看原告警依据/停止ID并保存到未完成Run；停止complete不会清UNKNOWN或自动封存/写CSV。配置、两个开关、逐步操作和原理见[后台告警手册](ALARM_STOP_POLICY.md)。

固定 ECC 服务还可启用接收进程故障停采：只对当前绑定会话/PID 的明确失败执行一次生产停止，保留核对门槛与未完成数据；正常退出和查询失联不冒充进程故障。配置、状态和测试边界见[接收故障停采手册](RECEIVER_FAILURE_STOP_POLICY.md)。 操作台可独立启用native_receiver_failure_status查看停止阶段/原触发依据，并保存到未完成Run；查看开关不启用后台策略。 升级ICE源的保持对象停止/join和无End组合检查见该手册第9节。

完整实验参数现可用 `getdaq-config compile-system` 组合为版本工件，由无Qt读取器检查并按实例ID绑定到原生系统Prepare/Configure，已接固定ECC服务与GUI整系统入口；已有两种编码的本机TCP端到端验证。profile、命令、参数精度、摘要、架构与未完成边界见[完整实验配置手册](SYSTEM_EXPERIMENT_CONFIGURATION.md)。实际板端加载核对、运行中切换和硬件恢复仍待完成。

原生发送器控制和整组数据连接组件已加入：DataSenderClient/DataLinkSetClient只发送板端控制命令，原始MFM直接进入独立接收器。暂停保留TCP连接，结束后还须断开、EOF、排空和正常封存；详细参数、状态、部分失败和验证见[数据发送器控制手册](DATA_SENDER_CONTROL.md)。

`SystemAcquisitionClient`进一步接续同一已配置准备对象，组合CoBo流水线/读指针、快控制、MuTaNT主从及发送器的连接/启动/暂停/恢复/停止，并接入原生ECC网络与GUI。全部生产停止确认后才刷新sender，失败后可显式请求停止生产并保留未知状态。原始数据仍不经过Qt；详细时序、库调用、状态和故障处置见[全系统采集手册](SYSTEM_ACQUISITION.md)。实际控制权/加载版本和完整硬件恢复仍待完成。

建议先读 [逐步操作手册](OPERATIONS.md)，再看 [高速数据服务](DATA_SERVICE.md)、[软件架构](ARCHITECTURE.md)、[原理](PRINCIPLES.md)、[协议](PROTOCOLS.md) 与 [依赖/离线说明](DEPENDENCIES_AND_OFFLINE_BUILD.md)。

本地参数、实例覆盖及十种结构编辑已有同一会话的审阅撤销/重做，见[操作说明](CONFIGURATION_UNDO_REDO.md)。恢复只修改本机文件，不回滚已发布配置或板端加载。

主窗口 **GET configuration files...** 可浏览本地`.xcfg`、解析include/默认值及共享单位/范围来源，保存声明或预览具体实例覆盖，导出/比较有效快照，并生成完整[本地配置发布包](CONFIGURATION_PUBLICATION.md)供固定ECC绑定。支持范围、操作步骤与未完成边界见[配置页面](CONFIGURATION_EDITOR.md)和[快照审阅](EFFECTIVE_CONFIGURATION_SNAPSHOTS.md)。窗口及`getdaq-config`还可离线导出硬件描述，见[硬件描述流程](HARDWARE_DESCRIPTIONS.md)；这些功能不发送硬件命令，不进入原始DAQ通路。

独立硬件访问层 `getdaq-hardware` 已支持设备/寄存器描述、读写和带日志的顺序执行；已迁移流水线初始化和 [AsAd 慢控制校准](ASAD_CALIBRATION.md)。新的无 Qt 状态约束核心见 [ECC 状态机](ECC_STATE_MACHINE.md)。完整 ECC 后端仍在重构，构建、操作与边界见 [硬件后端](HARDWARE_BACKEND.md)。

ECC 状态核心已增加持久化操作 ID、启动核对约束和只读历史工具 `getdaq-ecc-journal`；文件格式、命令及错误处理见 [ECC 日志手册](ECC_JOURNAL.md)。完整生产恢复检查与网络入口仍待接入。

原生`NativeEccCoordinator`已组合九个ECC命令、固定工件/拓扑/接收计划和持久审计；`ReceiverProcessManager`、无Qt的`RemoteReceiverManager`管理本机及远程receiver的就绪、EOF、退出和封存，原始数据不经控制器。输入和接法见[协调器](NATIVE_ECC_COORDINATOR.md)、[本机接收管理](RECEIVER_PROCESS_MANAGEMENT.md)、[远程ECC生命周期](REMOTE_ECC_DATA_LIFECYCLE.md)和[数据主机管理](REMOTE_RECEIVER_CONTROL.md)。固定HTTP由`getdaq-ecc-server`提供，配置见[网络服务手册](ECC_NETWORK_SERVICE.md)。GUI已有native_ecc模式，通过共用纯协议、客户端与持久意图一次控制全系统；可选native_catalog将数据主机的原生硬链接Run归档接到封存之后，成功才完成GUI记录。详细操作、原理、故障及本地Run恢复见[GUI手册](ECC_GUI_CLIENT.md)、[数据端Run归档](DATA_RUN_CATALOG.md)，模板见[原生GUI配置](../config/envoy.native.example.yml)。旧SOAP模式保留；硬件UNKNOWN核对、实际加载版本、跨文件系统/主机载荷迁移、目标平台和现场/吞吐验收仍未完成。

全系统描述现由无Qt的`getdaq-system-describe`与`SystemDescriptionClient`执行：支持只读检查、核对现有描述、创建空节点及明确替换旧描述；全部目标先检查，保留全部CoBo initDone→mutantMode顺序，未知结果不重发。逐步命令、旧拓扑绑定、原理和故障处置见[全系统描述手册](SYSTEM_DESCRIPTION.md)。它不提交生产ECC状态，也不参与原始数据传输。

结束配置和撤销准备进一步由同一原生对象的 Breakup/Unprepare 协调：按运行阶段停生产/刷新、断开自有 sender，再清全体 CoBo 就绪位；断电前读取实际供电与保护，只释放自己的告警订阅。详细状态、操作顺序、部分失败和 Reset 边界见[结束与撤销准备手册](SYSTEM_RELEASE.md)。接收器 EOF/排空/封存及生产ECC状态提交仍需独立完成。

实验参数编译已迁移 AGET/ASTRE 的 13 个配置寄存器与读出掩码；`getdaq-config compile-aget` 检查单片，`compile-cobo-aget` 检查一个 CoBo 的全部活动芯片及共用读出参数。计算核心使用纯 C++11，离线命令使用 Qt Core，不连接硬件。原生硬件库另有核对型号、逐项写入/读回的 AGET 子序列，尚未接入 CLI/GUI JSON 部署或完整 ECC Configure。两者均不参与原始数据传输；命令、位序、来源和边界见 [AGET 配置手册](AGET_CONFIGURATION.md)。

`compile-cobo-readout` 进一步编译 CoBo 的触发、帧头编号、multiplicity、零抑制及板内循环缓冲区，并检查跨芯片读出模式/位置屏蔽冲突。原生库 `CoBoReadoutConfigure` 执行本文范围的组合子序列，尚非完整 Configure；详细操作、地址与单位、执行前提和错误处理见 [CoBo 读出配置](COBO_READOUT_CONFIGURATION.md)。

`compile-cobo-configure` 再加入活动 AsAd 的 TCM、ISPM、脉冲发生器和 2p 初始化策略。原生库 `CoBoConfigure` 已组合 AGET/AsAd/Module 操作，整份预检查后顺序执行并记录结果，Configure 不启动周期脉冲或采集。完整 Prepare、生产 ECC 状态/恢复/网络入口及 JSON 部署仍待接入；命令和电压/信号解释见 [AsAd 配置手册](ASAD_CONFIGURATION.md)。

Prepare 已迁移基础触发/LEMO/缓冲区、AsAd 输入保护/槽位和 ADC/DAC 命令子流程；原生 CoBoPrepareClient进一步组合上电/时钟/校准/保护及自有告警，在最后复位后恢复ADC通道并逐步消费停止联锁。仍未提交全系统 Prepared或接生产ECC；使用与生命周期见 [单板组合手册](COBO_PREPARE_CLIENT.md)。接口、时序、旧实现的 ADC 顺序问题及后续工作见 [Prepare 说明](COBO_PREPARE.md)。

AsAd 上电、连接/供电检查、监控阈值、VDD 稳定等待和后段保护已有离线 `compile-cobo-monitoring` 及原生库子流程；明确区分上电前后 AL 含义和旧监控实参顺序。Qt 只编译参数，原生控制层执行命令，独立数据进程仍不依赖 Qt。模式策略、字段/单位、量化示例、操作顺序与恢复限制见 [AsAd 上电和监控手册](ASAD_MONITORING.md)。完整生产 Prepare/ECC 和真机验收仍未完成。

LED管理器、板端告警检查器和原生主机回调已有实现；`getdaq-hardware watch-asad-alarms`可独立检查已有服务的通知。订阅ID独立、有界队列/日志、故障请求保持，不重发结果未知的动作；观察命令不发送硬件Stop，完整ECC停机/排空协调仍待接入。参数、反向网络、状态和故障处理见 [LED与主机告警详细手册](COBO_SERVICES.md)。

另已迁移 [整板 PLL/LCM 时钟编译与原生子流程](COBO_CLOCK_CONFIGURATION.md)：离线 compile-cobo-clock 读取 xcfg、保留来源并显示上下游名义频率；原生库独立执行 FIFO/LCM 命令。JSON 没有硬件部署入口，命令应答不证明 PLL 锁定或完整 Prepared。

AGET 串行初始化、全局阈值和 SCR 自动读取延迟子流程也已迁移；`compile-cobo-calibration` 离线检查活动芯片/预期型号、校准片和明确的 global/per-asad 布局。原生计划保留 ADC 测试、64位 hit 和等待，区分“计算”和“使用”延迟，未知写入停止。校准会覆盖 reg1/reg2并临时使能固件，须在最终 Configure 前执行。字段、示例、原理与失败处理见 [AGET 校准手册](AGET_CALIBRATION.md)，仍不是完整生产 Prepare。

MuTaNT 的参数计算、`compile-mutant` 离线编译及原生 Prepare/Configure/计数初始化/时钟位子流程也已迁移。保留请求与量化时间、48位计数、对齐掩码与ATO区别，旧名称/时钟回退事前明确选择，断线不重发。完整TDC对齐、系统同步/运行和生产ECC仍待组合；命令、字段、原理和失败处理见 [MuTaNT 手册](MUTANT_CONFIGURATION.md)。

原生 `getdaq-align` 已迁移 CoBo/MuTaNT 槽位定位、机箱内测量和机箱间主从三步协调。支持明确拓扑、全节点预检查、保存对齐掩码和故障不重发/不自动恢复；不链接Qt、不接收原始数据。板端TDC算法仍依赖现场旧服务；配置示例、逐步操作、原理及架构见 [系统对齐手册](SYSTEM_ALIGNMENT.md)。

原生库`SystemPreparationClient`进一步组合全系统Prepare/Configure、PLL之后的同步及跨板告警，保留同一次对齐快照和监听器。全部目标先检查，同步期间保持initDone=0，所有CoBo设置就绪后才配置MuTaNT；后续采集接SystemAcquisitionClient。仍需生产ECC网络/GUI、实际控制权和版本、远程接收管理及完整恢复；详见[全系统操作与原理](SYSTEM_PREPARATION.md)。
