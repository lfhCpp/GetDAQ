# MuTaNT 配置、计数器与时钟控制说明

## 1. MuTaNT 在系统里做什么

CoBo负责接收AsAd/AGET采样数据，MuTaNT负责整套系统的触发、时钟分发和事件/时间计数。可以把MuTaNT理解为各板协同工作的时间和触发控制模块。它也可以产生自己的数据帧；这些数据仍应由独立接收进程处理。

本阶段新增纯C++11计算、xcfg编译器和原生硬件子流程。Qt只读取配置并生成可检查的结果，原生硬件库使用已有本地Ice/GET接口发送控制命令。**原始事件字节不经过Qt配置工具或GUI。** 本阶段没有修改接收器的数据热路径，也没有增加新的运行库。

| 部分 | 位置 | 能证明什么 |
| --- | --- | --- |
| 参数和寄存器计算 | common/experiment/mutantconfiguration.h/.cpp | 输入是否满足已迁移算法、位宽和策略 |
| 配置读取与来源 | common/config/mutantcompiler.h/.cpp | 实际读到的文件、具体实例/默认来源、单位和声明范围 |
| 命令行 | getdaq-config compile-mutant | 离线产生新的JSON检查工件，不连接板端 |
| 原生执行子流程 | apps/hardware/mutant.cpp | 经过描述预检查、逐动作持久化后执行一次并停止于错误 |
| 系统级对齐和运行 | 完整ECC后端仍待接入 | 本阶段不证明完整Prepared/Ready、TDC相位对齐或数据进程生命周期 |

原始依据为收到的2021版GetBench里的MutantConfigParser.cpp/.h、MutantNode.cpp和hardwareDescription_Mutant.xcfg。保留原源码作为对照，不要求安装旧CCfg、Boost或log4cxx。沿用代码的CeCILL许可证保留在third_party/licenses/getbench/COPYING。

## 2. 第一次使用：先离线编译

先按 [操作手册](OPERATIONS.md) 构建Qt工程。根GetDAQ.pro会生成getdaq-config；Linux构建示例的程序位于build-suite/bin/getdaq-config，Windows在对应构建目录里找到getdaq-config.exe。编译实验配置不需要板端在线，也不需要构建Ice组件。

```bash
build-suite/bin/getdaq-config compile-mutant \
  source/get/GetBench-software-release-20210208/data/config/configure-ExternalTrigger-2CoBo_1Mutant.xcfg \
  master 1 3 3 modern strict mutant-master-review.json
```

这里的模块数和掩码只是命令示例。真正使用时必须填写经过拓扑/对齐阶段核对的值，不能因为文件名包含“1Mutant”就把示例掩码当成实测结果。最后的输出文件必须尚不存在；已有结果不会被覆盖。命令失败不会留下一个声称编译成功的JSON。

完整格式为：

```text
getdaq-config compile-mutant INPUT.xcfg MUTANT_INSTANCE MODULE_COUNT GMC_MASK WSCA_MASK REGISTER_LAYOUT CLOCK_POLICY OUTPUT.json
```

| 参数 | 填写方式 | 含义 |
| --- | --- | --- |
| INPUT.xcfg | 原始实验Setup配置 | 可以包含include和`*`默认声明 |
| MUTANT_INSTANCE | 如master，区分大小写 | 必须明确声明，不能填写`*`或不存在的实例 |
| MODULE_COUNT | 规范十进制1..4096 | 用于判断主模块是否需要PromptWSCA；4096是软件输入上限，不是可连接模块数量的硬件保证 |
| GMC_MASK | 0..4095或0x000..0xfff | 12个AMC槽位的GMC有效掩码快照 |
| WSCA_MASK | 同上 | 相同阶段的WSCA有效掩码快照 |
| REGISTER_LAYOUT | modern或legacy-names | 选择寄存器名称；两种都使用本阶段迁移的v1.5算法 |
| CLOCK_POLICY | strict或legacy-vhdci | strict拒绝未知时钟名称；后一项显式恢复旧VHDCI回退行为 |
| OUTPUT.json | 新文件路径 | 保存寄存器值、时间量化、来源和适用边界 |

配置实例的路径是`/Setup/Node[Mutant]/Instance[master]`。它没有CoBo的isActive字段，不能照搬“必须isActive=true”的检查。硬件描述的根ID则是`MuTanT`，是收到的原文件拼写；这两个名称属于不同结构，不要混用。

`*`可以提供默认参数，但不会创建真实MuTaNT实例。只有默认声明而没有具体Instance时，编译拒绝，防止生成一块不存在的模块。独立旧测试文件test/data/mutant-config.xcfg使用Mutant根，不是当前命令接受的Setup结构；回归测试在临时目录包装它，原文件保持不变。

## 3. 如何读输出

建议先看以下字段：

| 字段 | 解释 |
| --- | --- |
| kind/schema_version | 当前为getdaq.mutant-image/1 |
| mutant_instance/is_slave/module_count | 选中的实例、主从和调用者提供的模块数 |
| register_layout/register_mapping | 名称布局及v1.5语义，不代表已经查询固件版本 |
| gmc_shelf_mask/wsca_shelf_mask/shelf_mask | 输入快照以及两者交集 |
| amc_tag_only_mask/final_*_output_mask | 仅接收同步信息的槽位，以及合入ATO后的实际输出目标 |
| timestamp_10ns/event_number | 完整计数值，使用十进制字符串，保持48/32位精度 |
| timings | 请求us、编码值、名义us和physical_verified=false |
| registers | 普通寄存器/位域值；十进制value和hex都是字符串 |
| clock_path_update | 需要读改写的clk_path_sel；mask=5，只更新位0、2 |
| parameters | 查询路径、原值、类型/单位/范围、来源文件和行号、代码默认值标记 |
| sources | 实际参与读取的文件和SHA-256 |
| warnings | 已知行为变化、布局假设和未验证条件 |

未带前缀的十六进制输入会在来源中保留原value，并记录compiled_value，如`10`转成`0x10`。参数的范围检查和最终计算使用相同解释，避免“先按10校验、后按16计算”。

hardware_deployment_supported、complete_prepare和alignment_verified目前都是false。这份JSON便于检查和保存配置，不能直接交给寄存器write命令逐行执行。它没有取代计数器命令时序、读改写、描述权限、停止检查、配置所有权及系统状态提交。

## 4. 主模块、从模块及触发

主模块读取Master/enablePortA和enablePortB。具体实例中的false会覆盖默认true；不能只查看默认块就断定端口已启用。从模块读取Slave/slavePort=A/B和slaveId=0/1，并只启用对应端口。

从模块必须关闭Level0、Level1和Level2，并且不能选择CENTRUM耦合。主模块可以选择Disabled、Scalers或CENTRUM。未知字符串直接拒绝，不采用旧解析器“没找到就按Disabled/LOW/L0L1”的静默回退。

| GLOBAL_CONF位 | 本阶段采用的实际语义 |
| --- | --- |
| 0 | 主模块=1，从模块=0 |
| 1 | 从模块编号 |
| 2/3 | VHDCI端口A/B启用 |
| 4/5/6 | L0/L1/L2启用 |
| 7 | L2启用且hitPatternModified |
| 8 | L1L0优先；L0L1为0 |
| 9..10 | LOW=0、HIGH=1、OR=2、AND=3 |
| 11 | 2p模式 |
| 12 | 2p模式下halfEventReadout |
| 14 | Scalers或CENTRUM接口启用 |
| 15 | CENTRUM模式 |

旧代码旁边的HER“Bit 13”注释与实际setField(...,12,...)不一致，本次以执行代码为依据。v1.4的L1选择表也与v1.5不同；legacy-names只更改名称，不能用它启用v1.4算法。

L1关闭时仍配置L1A参数，旧软件说明这是为了检查信号用途，因此不能跳过其单位和范围校验。2p关闭时L1B采用旧默认：低阈值0、高阈值65535、两组bucket=511、timeout编码1；不使用闲置L1B配置里的非法值。2p打开时才读取并校验L1B和halfEventReadout。

## 5. 时间单位和量化

gateDelay、gateDuration及postTriggerDelay使用us，硬件编码单位为10ns，即0.01us。新版保留旧float中间量和floor(100.0f*us)，同时先检查输入是否有限、是否越界。delay/postDelay范围0..655.35us，duration范围0.01..655.35us。

例如请求655.35us，旧float计算实际得到编码65534，名义655.34us；不能直接显示“实际655.35us”。JSON会显示请求和计算结果。名义值来自寄存器算法，physical_verified=false，仍需硬件测量。

L1B_TIMEOUT在v1.5中每档2.56us，采用旧round方式floor(float(us)/2.56+0.5)。16位最大编码65535，对应167769.6us，即约167.77ms。旧XML上限1677000us过大，旧实现又在钳制前赋值uint16，无法可靠拦截越界；新版在窄化前拒绝。0码保留旧解析器允许的行为，不代表适合你的触发窗口。

旧v1.4.0使用10ns、v1.4.1使用1us，不能把本阶段算法直接用于这些版本。需要实际固件与描述确认后单独迁移。

## 6. 时间戳、事件号和计数器

preLoadedTimestamp用low/mid/high三个16位块组成48位时间戳，每单位10ns。preLoadedEventNumber为32位。旧getHexValue是严格类型访问，依据收到的CompoundConfig Scalar.cpp/Value.cpp，而不是把十进制数强行转为hex。这些字段必须声明type=hex，或不声明类型但使用0x前缀。type=hex时10表示16；无类型10或ffff、显式type=int/string会拒绝，避免改变原有解释。来源保留原值及规范化compiled_value。

即使preLoadTimestamp=false或preLoadEventNumber=false，仍读取并校验预载数值，与旧解析器行为一致。缺这两个开关时默认true；不能把构造函数里的初值false当成解析后的默认值。

原生Configure保留如下次序：

1. EUC和EEC写0，暂停相应计数。
2. clearCounters=true时，四个CL*C依次写1，再依次写0。
3. 需要时间戳预载时，写LOW/MID/HIGH，再把LUC写1、0。
4. 声明支持l2ptc时再写1、0；缺此命令必须调用前明确选择，不从失败后继续。
5. 需要事件号预载时，写LOW/HIGH，再把LEC写1、0。
6. EUC和EEC写1，恢复计数。

命令位不强求回显。命令可能自清零，普通寄存器读回方法不能套用到加载/清零脉冲。动作应答仅确认调用返回，不能证明计数器实际同步正确。

Scalers先禁用全部ENS1..5，按策略将CS1..5分别写1、0，再启用选中的计数器。新Scalers/clearScalers优先；确实缺失时才使用旧Scalers/clearCounters。外层clearCounters缺失时跟随该清scaler策略。存在但格式错误的字段会失败，不能因此转用旧字段。

## 7. 时钟来源、AMC掩码和ATO

FireWire1/FireWireA映射0，FireWire2/FireWireB映射1，VHDCI映射2，Optical映射3。从模块强制使用外部GMC，主模块按externalClockEnabled选择。未知名称在strict下拒绝；legacy-vhdci下明确选VHDCI并产生警告，操作员仍须核对接线。

MuTaNT B的GMC来源沿用旧值0，即从MuTaNT A相位对齐路径输入。WSCA来源主模块0、从模块A为1、从模块B为2。主模块在MODULE_COUNT>=2时启用PromptWSCA，从模块始终false。这些软件选择不证明物理对齐完成。

clk_path_sel宽9位，Configure只更新位0、2；WSCA子流程只更新位3。原生MaskedWriteValue先读现值，保存实际合并值，再持久化write_begin，发送一次并读回普通目标。未更新的位必须保留。HardwareClient的本地互斥只约束同一实例，读改写不是跨客户端原子操作，完整后端仍须独占控制权。

SHELF_MASK=对齐阶段保存的GMC掩码AND保存的WSCA掩码。ATO的槽位编号1..12对应位0..11；用于BEAST等仅接收同步信息的模块。Configure末尾在mrdy检查和AMC状态观察之后，将ATO并入两路实际时钟输出；它不会回头改动先前的SHELF_MASK。

例如GMC=11、WSCA=7、ATO=128，则SHELF_MASK=3，末尾输出分别为139和135。不能用后加ATO的掩码重新计算之前的对齐结果。

## 8. 原生库子流程怎样使用

先构建 [本地Ice兼容组件](DEPENDENCIES_AND_OFFLINE_BUILD.md)。公共硬件API仍只暴露标准C++11类型；下面是后端集成示例，地址、描述及策略必须由实际部署提供：

```cpp
#include "hardwareclient.h"
#include "mutantconfiguration.h"

GetDAQ::HardwareClient client(proxy, 0, 3000); // 编码1.0；1表示1.1
client.UseDescription(GetDAQ::LoadHardwareDescription(descriptionPath));
GetDAQ::MutantSettings settings = reviewedSettings;
client.Execute(GetDAQ::MutantPrepare(settings), newPrepareAuditPath);
// 系统对齐/CoBo准备必须由完整协调器在正确的中间阶段执行。
client.Execute(GetDAQ::MutantConfigure(settings), newConfigureAuditPath);
```

这段示例只说明API连接方式，不是完整可上线的Prepare脚本。直接连续执行两个子流程不能取代中间的CoBo准备、TDC对齐、同步及掩码快照核对。

| API | 内容与前提 |
| --- | --- |
| MutantPrepare | run=0；GPIO方向、电源A/B检查、可选板ID、run清零、VCXO、来源/GLOBAL_CONF和GMC字段 |
| MutantConfigure | run=0及实际掩码等于审阅快照；时钟位更新、B寄存器、计数/scaler、检查信号、mrdy、12个AMC观察和ATO |
| MutantRunInit | run=0；按策略清scaler和接受事件计数；不预载、不启动发送器或采集 |
| MutantWscaClock | run=0；仅修改clk_path_sel位3 |
| MutantSyncPulse | run=0；start_pulse写0/1，不添加隐式回显；调用者先满足CoBo mutantMode=1、initDone=0及系统对齐前提 |
| MutantClockShelfMasks | run=0；检查12位范围，再写/读回两个时钟输出字段 |

所有子流程要求绑定描述。整份计划的设备/寄存器/位域结构、宽度和本地权限先检查，后段元数据错误不能留下前面的写入。run=0只是一个寄存器观察，不能证明发送器已停、接收器已排空、TDC已对齐或所有控制端都已退出。

功率状态A要求高7位均就绪；B严格要求低8位0xff。旧原型0xdf只能通过allowPrototypePowerDf=true明确接受，并保留matched_with_warning。verifyBoardId开启时按24位expectedBoardId核对；仅ReadValue记录ID不会自动验证身份。

默认使用现代名称SHELF_MASK、AMC_TAG_ONLY、USER_REG。旧名称策略依次为MASTER_MASK、USER_REG1、USER_REG2。不论哪种策略，都先绑定匹配描述；禁止在写失败后换名称重试。twoProtonCounterCommand=false是明确声明缺l2ptc命令，默认true；缺失但未声明时整份计划预检查失败。

SOURCE_SYSTEM/source限8位；FIFO_FRAME_TYPE/ft宽2位，旧说明把3标为保留，因此新版拒绝3。0..2仍须符合配置声明范围，收到的实验配置仅声明0；1/2需要实际固件及数据格式验收。USER_REG为16位，四个Inspection为8位。mrdy必须1，12个AMC的4位状态只记录，不擅自发明“某值就是已对齐”的判据。

## 9. 出错时怎样处理

| 失败阶段 | 当前结论 | 下一步 |
| --- | --- | --- |
| 离线单位/范围/实例错误 | 没有硬件动作，工件不生成 | 修正具体来源，使用新输出文件重新编译 |
| 整份描述/权限预检查 | 没有执行计划里的写入 | 核对真实描述和事先选定的布局 |
| run或掩码前置检查 | 没有开始配置写入 | 由系统控制者确认停止与对齐快照 |
| Prepare后段电源/ID失败 | GPIO方向可能已改变 | 保留日志，检查实际硬件；不能称零副作用 |
| Configure后段mrdy/读回失败 | 前面的寄存器可能已改变 | 停止后续动作，不自动回滚；由完整恢复核对 |
| 写入应答丢失 | UNKNOWN；可能已生效 | 禁止自动重发、换名称、补写清理或继续步骤 |
| 写已应答但日志/读回失败 | 已知应答，验证/持久化未完成 | 同样不重复该动作，按证据恢复 |

读改写在写前读失败时不发送这次写入，但前面的计划动作仍可能已经执行。审计路径每次必须新建，intent、write_begin、acknowledged、observed/matched和complete逐阶段持久化；“complete”表示该子流程完成，不能解释为ECC已Prepared。

## 10. 已验证和接下来要做什么

本机测试使用从收到硬件描述编译出的5设备/87寄存器夹具，不由新算法重新编造一套布局；CLI回归另重新编译原描述并比较夹具及源SHA。TCP测试同时使用Ice编码1.0和1.1，命令读取会直接失败，用来检查不强求回显的规则。

独立对照测试在临时目录提取旧MutantNode实际函数体及旧解析器的量化表达式，使用记录存根替换外围依赖，不拿新函数充当旧参考。覆盖6144种GLOBAL_CONF组合、65536组时间码和64组Configure/RunInit/WSCA/同步命令次序。新增停止/权限/读回/UNKNOWN规则另用TCP测试覆盖。

复现：

```bash
python3 tests/mutant_compiler_integration.py build-suite/bin/getdaq-config
python3 tests/mutant_original_reference.py
cmake --build build-ice-compat --parallel 4
(cd build-ice-compat && ctest -R hardware-mutant-tcp --output-on-failure)
```

实际结果和插桩范围见 [验证记录](VALIDATION.md)。本机模拟和旧算法一致不能证明真实电源、PLL/TDC锁定、相位精度、实际AMC状态或目标平台兼容。

系统拓扑与AlignmentServer主机协调已有独立原生实现，见 [系统对齐手册](SYSTEM_ALIGNMENT.md)。仍需MuTaNT/CoBo完整Prepare次序、同步联锁、发送器Start/Stop、生产ECC故障恢复和网络入口。旧AlignmentServer接口的存在不证明原始板端实现和移植依赖已齐全。当前GUI真实采集仍按 [操作手册](OPERATIONS.md) 使用已验证的现场旧ECC及独立接收流程。
