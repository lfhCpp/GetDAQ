# 硬件描述查看与共享C++模型

操作台现在可以直接查看编译好的硬件描述JSON。这里显示设备基址、寄存器偏移、位域位置和主机权限策略，**不是板端当前寄存器值或实际加载证明**。窗口不创建Ice客户端、不执行寄存器读写，也不转发原始DAQ事件。

## 1. 如何打开

1. 在主窗口打开 **GET configuration files...**。
2. 若只有原始`hardwareDescription_*.xcfg`，先打开它，使用 **Export hardware description...** 生成新的JSON。导出不覆盖已有工件。
3. 点击 **View compiled hardware description...**，选择该JSON。
4. 展开设备，再展开寄存器，即可查看位域。查看按钮在连接模式也可用，因为它只读取本地描述文件；不会使正在运行的硬件改用该文件。

窗口先只建立设备行，展开设备才建立寄存器行，展开寄存器才建立位域行，避免打开时一次创建所有界面项。重复展开不增加重复行。打开另一文件或失败时先清除旧模型和树，不能把之前的布局当成这次读取成功。

## 2. 每列和两个摘要是什么意思

| 列 | 含义 |
| --- | --- |
| Device / register / field | 描述中的设备、寄存器或位域名称 |
| Address / bit offset | 设备基址、寄存器绝对地址，或位域的起始位 |
| Register width / field bits | 设备寄存器宽度；寄存器行按位显示，位域行显示位数 |
| Host policy / access | 设备访问方式字符串，或本地描述允许的只读/可写策略 |
| Register offset | 寄存器相对设备基址的偏移 |

绝对地址为“设备基址+寄存器偏移”。共享模型先拒绝64位回绕；界面用完整无符号64位数显示十六进制，不经过JSON double或32位int。例如最大地址`18446744073709551615`显示为`0xffffffffffffffff`。寄存器字节宽度1..8对应8..64位；位域范围必须落在该宽度之内。

位域的有效主机只读策略是“寄存器只读或位域只读”。旧GET板端没有完整保留/执行这些只读字段，界面的Host read-only来自本地策略，不能解释为真实板端访问保护已经核对。Host writable也不是允许在采集时随意写入的授权。

**File SHA-256**标识本次实际读取的描述文件字节；**Host policy SHA-256**标识解析后的权限/布局规范字节。文件排版或来源记录变化可能改变前者，布局/权限变化会改变后者。两者都不证明板端正在使用该描述，也不证明文件以后没有被其他程序修改。

## 3. 为什么要共享模型

原生硬件控制原先用本地yaml-cpp读取JSON工件，Qt界面则只有编译导出功能。现在`common/hardware/hardwaredescription.*`统一描述数据、严格JSON读取、校验和规范权限编码；Qt窗口与原生硬件访问直接复用它。这个模块只使用标准C++11和已有`StrictJson`，没有Qt、Ice、Boost、ROOT、yaml-cpp或curl依赖。

`apps/hardware/hardwareclient.h`继续提供原有类型和函数名称，通过共享头引用相同定义；原生硬件接口和GET的64位位模式没有改变。旧`hardwaredescription.cpp`路径保留兼容占位，新实现由共享库编译。普通CMake和独立插桩构建入口也已接入。

这是硬件描述模块的依赖减少。原生拓扑/其他旧配置读取和可选Ice兼容组件仍有各自依赖，Qt控制台也仍使用本地yaml-cpp处理envoy YAML；不能把本阶段理解为整套已完全移除yaml-cpp或Ice。

## 4. 现在接受哪些格式

支持编译器生成的schema1、kind为`getdaq.hardware-description`的JSON。字段完整性、未知/重复键、UTF-8、整数/布尔类型、来源记录格式和大小都检查。单文件最多8MiB，设备最多4096、寄存器总数最多65536、位域总数最多100000，单寄存器位域最多256；名称与总名称字节也有限额。

地址/偏移使用字符串，保留完整uint64；仍支持原有十进制或`0x`写法。schema/register_bytes/offset_bits/width_bits使用真正的非负JSON整数，read_only使用真正的JSON布尔值。

例如，`"read_only": true`有效，`"read_only": "true"`拒绝；`"register_bytes": 8`有效，`8.0`、`"8"`拒绝；地址不能作为JSON数值传入。YAML语法、多文档、尾部内容、溢出、重复名称或非法位域也拒绝。原先YAML解析器宽松接受的字符串类型，不属于当前编译工件协议；需要按JSON字段类型修正，而不是让程序猜。

来源路径和SHA字段只验证格式，读取描述不会去打开所有来源文件并重新验证它们，也不会连接板端。因此文件里的来源记录不是本次现场健康证明。

## 5. 验证了什么

共享模块的普通和严格C++11 ASan/UBSan/LeakSanitizer检查覆盖精确uint64、64位位域、地址回绕、重复键、未知字段、类型混淆、UTF-8和JSON-only边界。独立测试程序的动态依赖只有系统运行库，没有Qt/Ice/yaml等。

优化前的YAML描述解析器、JsonString/ParseUnsigned原函数从既有不可变源码包提取后编译；两份完整CoBo/MuTaNT描述，以及三份原始GET XML（fullCoBoStandAlone、Mutant、Zedboard）经当前编译器生成的工件，旧/新解析器的规范权限JSON逐字节一致。比较的是已经迁移前的项目解析器，不能说成整个原始GET软件都已对照运行。

本机另外执行三原始样本编译→新原生读取→空节点11设备Create/读回/权限绑定和漂移拒绝；完整普通硬件客户端Ice1.0/1.1回归也通过。它们使用软件板端夹具，没有真实设备或DAQ数据。新描述模块插桩检查不等于整个原生Ice/Qt/第三方库均插桩。

Qt完整普通回归65项通过，实际只读窗口验证最大地址显示、按需展开、位域继承只读、不改原文件、失败清旧显示及连接模式查看。当前操作台/CLI构建通过。Windows Qt5.14.2 MinGW、Ubuntu24.04和macOS仍需实际运行验证；当前环境为Ubuntu20.04.6、GCC9.4、Qt5.12.8。

本阶段没有大数据传输测试，不生成原始DAQ文件；既有20GiB每轮总预算和测试后删除规则保持。

## 6. 开发人员如何复查

纯C++模型不需要Qt/Ice开发环境：

```bash
python3 tests/build_hardware_description_validation.py build-hardware-description
build-hardware-description/hardware-description-tests
```

传入一个描述路径时，测试工具输出规范权限JSON，可用于开发比较；它仍不执行硬件动作。实际原生联调命令：

```bash
python3 tests/description_integration.py build-suite/bin/getdaq-config \
  build-ice-compat/getdaq-hardware build-ice-compat/hardware-client-tests
```

构建与运行结果见[验证记录](VALIDATION.md)。本阶段独立证据包保存源码、普通/描述模块插桩程序、Qt构建/生成文件、旧解析器及比较结果，不保存DAQ载荷；历史检查只证明归档字节，不能证明后来工作树。硬件创建、停止前提和实际权限约束见[硬件描述部署手册](HARDWARE_DESCRIPTIONS.md)与[硬件后端](HARDWARE_BACKEND.md)。

后续仍需要把实际板端观察接到受控服务并单独证明其时效、身份和权限。当前离线查看不授予控制权，不核对UNKNOWN，不提供波形/事件查看。
