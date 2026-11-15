# 全系统硬件描述：核对、创建与明确替换

本文介绍无Qt的SystemDescriptionClient和getdaq-system-describe独立入口。相同描述流程已由NativeEccCoordinator接入native ECC的Describe命令、持久操作日志及认证HTTP网络入口；Qt需选择native_ecc配置才能使用该路径。旧SOAP/模拟模式仍保留。现场控制权、硬件读回、真实板卡和冷启动恢复仍待验收。

这是低频控制工具。它不收取、转发、解析或保存原始 MFM 事件，也不链接 Qt。高速数据仍走[独立数据服务](DATA_SERVICE.md)。

## 1. “描述”到底做什么

硬件描述告诉板端服务：设备叫什么、访问方式是什么、设备基址在哪里、每个寄存器的偏移和字节宽度是多少、位域位于哪些位。原始 .xcfg 可由[硬件描述编译工具](HARDWARE_DESCRIPTIONS.md)导出 JSON。

描述不是实验参数。创建名为 ctrl 的设备和 pipeCtrl 的寄存器，只是建立软件访问对象；它不会把实际芯片上的所有寄存器重置为零，不会给 AsAd 上电、校准 AGET、锁定 PLL 或开启采集。

CoBo 的两个相关状态位是：

| 位域 | 含义 | Describe 的动作 |
| --- | --- | --- |
| ctrl/mutantConfig/initDone | CoBo 向 MuTaNT 报告配置是否完成 | 对全部声明的 CoBo 写 0并读回 |
| ctrl/mutantConfig/mutantMode | 是否采用 MuTaNT 驱动模式 | 对全部声明的 CoBo 写 0并读回 |

按照收到的 2021 年 SystemManager::describe，必须先完成**全部 CoBo 的 initDone=0**，再进行**全部 CoBo 的 mutantMode=0**。不是对第一块依次写两个位，再处理第二块。后续 Configure 才根据系统拓扑设置 mutantMode，并在规定的同步边界后设置 initDone。

MuTaNT 在 Describe 阶段只核对描述和 run=0，不执行 MuTaNT Prepare/Configure。系统对齐、实际时钟/校准见[准备协调](SYSTEM_PREPARATION.md)。

## 2. 三种策略必须明确选择

| POLICY | 中文含义 | 开始前要求 | 修改 |
| --- | --- | --- | --- |
| use-existing | 使用并核对已经加载的描述 | 名称、设备全集、全部元数据符合目标描述；生产停止观察通过 | 保留已有描述，清除全部 CoBo 的两个状态位 |
| create-empty | 给空软件节点创建描述 | 所有目标软件设备列表为空 | 逐节点 create，核对新描述及实际停止位，清除 CoBo 状态位 |
| replace-existing | 明确替换已知的旧描述 | 所有当前描述/名称符合提供的旧策略，所有生产停止观察通过 | 对每个节点 destroy→确认设备列表为空→create；最后再全系统核对和清除状态位 |

**use-existing 的整个 Describe 仍会写状态位。** 要只做观察，使用 check-system。离线格式检查使用 validate-topology。

replace-existing 保留旧 Describe 的重新加载职责，但不再默认销毁任何已存在的目标。用户必须明确选择策略并声明已取得控制权、停止硬件与数据进程。

旧 EccBackEnd::loadHwDescription 会先 destroy，再用临时名称 create，随后 rename。新实现直接把最终名称作为 NodeConfig.id 传给 create，省去临时名称和额外 setName RPC；收到的 HwNodeImpl::create 本来就采用该名称。每块节点最后的名称和设备结构必须匹配。

## 3. 构建

从项目根目录执行：

~~~sh
cmake -S third_party/ice-compat -B build-ice-compat -DCMAKE_BUILD_TYPE=Release
cmake --build build-ice-compat --parallel 4
build-ice-compat/getdaq-system-describe --help
~~~

依赖复用本地 Ice 3.5.1/GET 接口及 yaml-cpp；没有增加外部 JSON、日志或数据库库。实际构建平台见[验证记录](VALIDATION.md)，尚不能声称 Ubuntu 24、Windows 或 macOS 已完成运行验收。原生硬件组件当前采用 POSIX 文件持久化；Windows Qt GUI 的构建独立于此工具。

## 4. 准备拓扑文件

复用[对齐拓扑格式](SYSTEM_ALIGNMENT.md)和[示例文件](../config/hardware/alignment-topology.example.json)，无需建立第二套硬件地址目录。独立 CoBo 模式允许 mutants 为 []。

| 字段 | 使用规则 |
| --- | --- |
| schema_version / kind | 1 / getdaq.alignment-topology |
| encoding | 显式 1.0 或 1.1 |
| timeout_ms | 100～60000 |
| cobos[].id / mutants[].id | 实体 ID；每个类型内不得重复 |
| expected_node_name | 板端最终名称；整个系统内不得重复 |
| node_proxy | 硬件 Node 的直接 TCP 代理 |
| hardware_description_file | 已编译的完整描述 JSON；相对路径以拓扑文件所在目录为基准 |
| cobos[].enabled | 实验是否活动；停用实验板仍在本 Describe 节点集合中 |
| link_proxy / alignment_proxy / role | 拓扑后续对齐/准备所需字段；Describe 不调用链路/TDC 服务 |

不要把未安装或不应访问的板放进节点集合。这里的 enabled=false 表示已声明、可访问但不参与实验采集的 CoBo，它也必须向系统报告未就绪。历史 Describe 配置的 isActive=false 则可能表示连节点都不加入；两者所在层级不同。系统操作台后续应明确区分“硬件是否属于拓扑”和“是否参与本次实验”。

工具将每份描述的 nodeId 绑定为该拓扑的 expected_node_name，不改写原始 JSON 文件。因此同一份 CoBo 描述可用于不同实例名称。所有本地权限和布局以本次读入的副本为准，不在执行到一半时重新读取文件。

只接受明确主机/端口的直接 TCP 端点，不接受 router/locator、间接适配器或未知端点类型。同一对象 ID/category/facet 的主机/端口重叠会被拒绝；超时或压缩参数不同不能绕过去重。DNS 别名、不同 IP 对应同一物理板等仍需现场拓扑核对，这不是远端独占锁。

## 5. 第一步：完全离线检查

~~~sh
build-ice-compat/getdaq-system-describe validate-topology topology.json use-existing
build-ice-compat/getdaq-system-describe validate-topology topology.json create-empty
~~~

成功返回 valid=true、hardware_connected=false。这只证明文件能读、格式/描述/所需状态位和代理形式符合本工具的规则；不证明任何板存在，不检查实际寄存器值，也不声明取得控制权。

此模式不接受 --exclusive-stopped。它内部启用的是用于计划检查的条件，输出仍明确 ownership_declared=false、hardware_stopped_verified=false。

错误示例：遗漏 mutantMode、状态位标为只读、位宽不是1、节点名称重复、未知键、缺少硬件描述文件。先修正文件，不要靠切换策略绕过错误。

## 6. 第二步：在线观察，不修改

在已经取得实际控制权、硬件生产停止、sender/receiver 均完成停止核对后：

~~~sh
build-ice-compat/getdaq-system-describe check-system topology.json use-existing --exclusive-stopped
~~~

此命令连接全部 Node，读取全部设备元数据，核对名称和设备全集，再观察 CoBo firmwareEnable=0、intEnable=0 或 MuTaNT run=0。它不创建操作目录，不发送 destroy/create，不清除就绪位。

--exclusive-stopped 是**调用者声明**，不是软件实施的锁或自动停止命令。旧接口没有完整 sender 所有权/运行状态查询；两个 CoBo 停止位或 MuTaNT run=0 不证明 sender 已刷新、receiver 已退出。输出仍保持 control_lock_verified=false 和 sender_stopped_verified=false。

create-empty 的 check-system 只能确认软件设备列表为空。空节点无法按尚未加载的描述读取停止位，因此 empty_node_hardware_stopped_verified=false。真正执行后还要观察新描述下的停止位，才能进行状态位写入。

## 7. 第三步：执行 Describe

按实际部署目的，选择一个策略；每次使用全新目录：

~~~sh
build-ice-compat/getdaq-system-describe describe-system topology.json use-existing audit/describe-001 --exclusive-stopped
build-ice-compat/getdaq-system-describe describe-system topology.json create-empty audit/describe-002 --exclusive-stopped
build-ice-compat/getdaq-system-describe describe-system topology.json replace-existing audit/describe-003 --exclusive-stopped
~~~

上面的三条是不同场景的示例，不是必须连续执行的步骤。audit 父目录须已存在。已有动作目录不覆盖、追加或删除，防止丢失先前失败记录。

replace-existing 未提供旧拓扑时，要求当前布局和名称与目标拓扑描述一致。这用于明确重新加载同一描述。若旧名称或布局不同，使用：

~~~sh
build-ice-compat/getdaq-system-describe validate-topology new-topology.json replace-existing --current-topology old-topology.json
build-ice-compat/getdaq-system-describe check-system new-topology.json replace-existing --current-topology old-topology.json --exclusive-stopped
build-ice-compat/getdaq-system-describe describe-system new-topology.json replace-existing audit/replace-001 --current-topology old-topology.json --exclusive-stopped
~~~

两份拓扑必须按类型和 ID 对应，node_proxy 文本完全一致；CLI 故意要求该字段精确相同，避免把旧布局声明绑定到另一目标。旧列表重排可以接受，执行顺序仍采用新拓扑的顺序。仅替换旧描述的名称/布局，不从旧拓扑执行任何链路或 TDC 命令。

CLI 目前对整个系统采用一个策略；C++ 库允许逐节点选择，例如已存在的 CoBo 使用 use-existing、新装 CoBo 使用 create-empty、MuTaNT 使用 replace-existing。

成功输出 phase=described 是**本工具的命令阶段**，production_ecc_state_committed=false。不要据此点击新系统的 Prepare 按钮或将外部 ECC 状态手工修改为 Described。生产后端须完成拓扑/控制权、操作 ID、持久化和恢复绑定后才能提交其状态。

## 8. 真正的执行顺序

~~~mermaid
flowchart TD
    A[本地完整描述与所需位域检查] --> B[全部实际 Node 类型/设备/名称/停止观察]
    B --> C[新目录和根意图持久化]
    C --> D[按拓扑逐节点保留或明确 destroy/create]
    D --> E[全部最终元数据与生产停止观察]
    E --> F[全部 CoBo initDone=0并读回]
    F --> G[全部 CoBo mutantMode=0并读回]
    G --> H[完成记录持久化]
    H --> I[本对象 Described]
~~~

对于替换，每次 destroy 前再次观察停止位，DestroyDescription 本身再核对当前名称和全部元数据；每次 create 都要求设备列表为空。销毁只移除软件设备描述，**不会停止固件、复位硬件或断开 sender**。

所有新描述创建完后，再核对整个系统的最终名称、设备全集和全部元数据；再观察所有生产停止位、检查两个 CoBo 状态位的远端布局及本地写权限。某个空节点创建后发现实际固件仍在运行，则停止流程，保持 Failed，不写任何 CoBo 就绪位；已经创建的描述仍可能留在板端。

这不是硬件事务。整体预检查减少晚期格式错误带来的部分操作，但网络或硬件故障仍可能在前几块完成后发生。不会隐式回滚、更换策略、清理先前设备或继续下一节点。

## 9. 文件和审计如何阅读

~~~text
describe-001/
  system-description.jsonl
  step-0.jsonl
  step-1.jsonl
  ...
~~~

动作目录采用0700，文件采用0600。创建目录/文件后同步目录，每条意图及结果逐行 fsync。日志中的 uint64 地址和值使用字符串，避免被 JSON double 截断。

根日志保存 schema_version=1、编码/超时、完整节点顺序、实际代理、每节点策略和完整目标权限/布局；替换还保存完整当前描述。node_begin/node_complete 记录 stage 和子日志名。stage 为 destroy、create、initDone 或 mutantMode。

| 子日志 | 重点 |
| --- | --- |
| destroy | 意图持久化→一次调用→ACK→设备列表为空→完成 |
| create | 完整 NodeConfig 意图→一次调用→ACK→最终名称/全部设备/元数据核对→完成 |
| 状态位步骤 | write 和 expect 的计划，实际写入/读回结果，出现异常时的停止位置 |

description policy 总量最多32 MiB，根日志最多64 MiB，节点最多195。超限会拒绝，不继续发下一条命令。已有文件或后续日志创建失败，也不得越过记录步骤继续修改。

metadata_verified=true 只表示所核对的软件布局符合输入。旧 DeviceImpl 不保存完整权限标志，所以 board_permissions_verified=false；主机仍用绑定的本地策略拒绝禁写字段。它不证明电气地址正确、实验参数加载、板端实际型号/固件已验收或物理采集就绪。

## 10. 应答不明及错误后的处理

| 现象 | 含义与当前行为 |
| --- | --- |
| 最后一个节点类型、名称或元数据不符 | 首次修改前拒绝；本对象仍 Fresh |
| 新动作目录已存在 | 不发送修改；保留原目录 |
| destroy 已清空设备但 ACK 丢失 | UNKNOWN；不重发 destroy，不 create，不处理后续节点 |
| destroy ACK 后仍有设备 | UNKNOWN；不靠再次 destroy 修补 |
| create 生效但 ACK 丢失 | UNKNOWN；不重发、不补 setName、不处理后续节点 |
| create 后元数据不符 | 保留实际创建结果与失败记录，停止后续动作 |
| 状态位写入生效后断线 | Failed；不继续其余状态位，不按 Described 完成 |
| 第一块完成后下一子日志不可创建 | Failed；不发送下一条修改，不清理已完成节点 |

旧 Slice 将 destroy 声明为 idempotent。这里仍关闭 Ice 自动重试，单对象只允许一次 destroy 尝试；结果不明后，即使实际列表已空，也禁止该 HardwareClient 继续 create。成功替换后也不能在同一描述操作对象上再开始下一次销毁。

**重新启动 CLI、构造新对象或选新日志目录不是核对恢复。** 手动工具尚没有跨进程的生产 ECC 全局恢复门禁；任何 UNKNOWN 都必须保留全部日志，检查实际目标、数据进程和先前动作，由完整控制器实施恢复。不要按错误信息自动循环调用这些命令。

本对象状态为 Fresh→Describing→Described，运行中异常进入 Failed。Described/Failed 不能再次 Describe。析构只释放本地主机资源，不向板端 destroy/Stop，也不关闭 sender。

退出码：0 表示命令在说明的范围内成功；2 表示基本调用形式错误；1 表示选项、文件、前提或执行失败。诊断在 stderr，成功结果在 stdout。不能只解析 stdout 某个字段而忽略退出码。

## 11. 如何接入生产后端

C++ 库入口：

~~~cpp
// topology必须已经由控制器核对实际控制权/全部数据停止条件。
// 此示例不实现那些前提，不是可直接复制的生产服务main。
GetDAQ::SystemDescriptionOptions options =
    GetDAQ::BuildSystemDescriptionOptions(topology,
        GetDAQ::DescriptionPolicy::UseExisting);
GetDAQ::SystemDescriptionClient description(options);
description.Validate();                   // 只读预检查
description.Describe(newAuditDirectory);   // 一次完整动作
~~~

生产服务还必须串行化所有 Describe/Prepare/Configure/采集/维护命令。此类的互斥仅保护本对象，不能阻止其他工具或旧 ECC 控制同一板。

Describe 完成后，原生后端应使用同一拓扑和已固定实验工件建立 SystemPreparationClient，再保留该准备对象接续 SystemAcquisitionClient。配置输入 SHA 不是实际加载版本，目录锁不是远端控制权；不能跳过[持久化核对约束](ECC_JOURNAL.md)。Reset 的旧 UnDescribe 仅移除主机节点表，不能无条件解释为远端硬件 reset/destroy。

NativeEccCoordinator已将SystemDescriptionClient接入生产ECC操作执行路径：先保存意图，再调用描述流程，成功后提交Described状态；未知结果保持UNKNOWN并要求核对，重启不会重放硬件动作。本机测试覆盖模拟服务与持久日志，不代表真实控制权、实际配置加载版本或现场恢复已验收。

## 12. 验证范围

实际使用收到的 CoBo 11设备/1310寄存器和 MuTaNT 5设备/87寄存器描述，通过本机真实 Ice TCP 两种编码测试三个策略、全部节点预检查、混合旧/新布局、状态位顺序、无关位保留、销毁/创建/状态写入 ACK 丢失、日志失败、持久化和快速状态快照。CLI另由独立进程执行同一服务器，检查离线/在线、三策略、旧拓扑绑定与失败行为。

独立参考脚本提取收到的 SystemManager::describe/setCoBoInitDone/setCoBoMutantMode 和 CoBoNode::setInitDone/setMutantMode 五个真实函数体，严格C++11编译；只替换外围对象/日志/RPC记录环境，不调用新计划生成期望。六组成功TCP审计的状态位写入顺序一致。

旧 setter 会吞掉部分异常；新实现不会将失败视为成功，并增加完整元数据检查、停止观察、读回和持久化。参考对照只比较成功状态位写入，不证明替换事务、现场停止、物理时序或实机效果。确切结果、回归及内存检查记录见[验证记录](VALIDATION.md)第25节。
