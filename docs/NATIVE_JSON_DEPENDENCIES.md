# 原生控制程序移除yaml-cpp：拓扑、日志和兼容边界

本阶段把剩余原生拓扑和ECC控制日志的JSON读取迁到共享标准C++11 `StrictJson`。结合已经完成的硬件描述迁移，原生硬件/ECC兼容构建不再编译或链接yaml-cpp。板端Ice/GET接口、本地Ice/bzip2兼容源码仍保留；Qt操作台读取真正的envoy YAML时仍编译本地yaml-cpp。

原始数据链路继续由无Qt接收器完成，没有因为控制程序减少依赖而改变MFM格式、落盘顺序或导出协议。

## 1. 为什么JSON不继续用YAML库读取

旧项目读取硬件描述、拓扑和JSON Lines时借用YAML解析器。这样会让一个本来只接受JSON的文件入口额外接受YAML语法、字符串数字和字符串布尔。现在这些入口按各自JSON协议核对类型、重复键、未知字段、UTF-8和预算，减少依赖，也减少字段被猜成另一种类型的可能。

这个调整不会自动转换或覆盖旧文件。由原程序按协议写出的JSON和日志保持原字节；不合协议的输入拒绝后仍保留，操作者应确认来源并人工修正配置。日志不能为“方便读取”自动改写、截断、补完成记录或重发操作。

## 2. 拓扑文件的字段类型

拓扑仍使用schema1、kind为`getdaq.alignment-topology`，原字段、角色、节点顺序、代理和相对描述路径规则保持。

| 字段 | 类型 |
| --- | --- |
| schema_version | JSON整数1 |
| kind | JSON字符串getdaq.alignment-topology |
| encoding | 字符串1.0或1.1，不能是JSON小数 |
| timeout_ms | 非负JSON整数，原60000上限保持 |
| mutants / cobos | JSON数组，原16/192上限保持 |
| id / expected_node_name / node_proxy / alignment_proxy / link_proxy / hardware_description_file | 有界非空字符串 |
| role | master或slave字符串 |
| enabled | JSON布尔true/false |

例如`"enabled": true`有效，`"enabled": "true"`拒绝；`"timeout_ms": 3000`有效，`3000.0`和`"3000"`拒绝；`"encoding": "1.0"`有效，数字`1.0`拒绝。YAML、多文档、注释、尾部内容、重复键和未知字段均拒绝。

单拓扑最多1MiB，字符串/数组/节点预算仍有界。解析和离线validate不读取硬件寄存器；实际系统对齐仍有独占、停止、真实接口/布局预检查及失败不重放门槛。此迁移没有实现远端控制权或实际相位验证。

## 3. 控制日志的字段类型

ECC日志保持UTF-8 JSON Lines，一行一个对象、LF换行、最大64MiB文件、64KiB单行和原记录/操作数量限制。`seq`是十进制字符串；schema_version、state、command、from、to是真正JSON整数。phase、id、scope、request_key、message和evidence是字符串。

这些类型就是原写入器已经产生的类型，写入器的规范字节没有改动。读取器继续检查连续序号、相同scope、合法状态转换、唯一操作ID和完成/未知记录与原意图的关系。锁、逐条fsync、文件身份/长度检查和重开核对也保留。

坏类型、重复/未知键、非法UTF-8、注释或损坏尾行直接拒绝。错误定位仍包含记录编号。任何失败都不能自动删除旧尾部或使UNKNOWN变成成功；只读CLI不会建立不存在的文件或修改旧日志。

## 4. 编译依赖现在怎样划分

| 程序/模块 | 依赖范围 |
| --- | --- |
| 原生ECC状态/日志工具 | 标准C++11、系统线程/POSIX、共享StrictJson；无Qt/Ice/yaml |
| 原生硬件/系统对齐/ECC后端 | 标准C++11/POSIX、共享描述/StrictJson、本地Ice/GET及其兼容依赖；无yaml |
| ECC低频HTTP前端 | 上述原生后端加QtCore/Network/Concurrent；不接原始帧 |
| Qt操作台 | Qt基础模块和已嵌入yaml-cpp，用于实际envoy YAML |
| 原始数据接收器 | 标准C++11/POSIX/线程，不链接Qt/Ice/yaml |

`GetDAQLocalYaml.cmake`和原yaml源码保留，供历史资料与GUI兼容，不要求用户删除它们。旧构建目录可能仍有以前编译的libgetdaq-yaml-native.a，但当前原生目标的编译/链接命令已不引用；不能仅按磁盘上存在某个.a判断产品仍依赖它。

原生插桩构建脚本也已移除yaml头路径、YAML_CPP_STATIC_DEFINE和该静态库。旧Ice/GET/bzip静态库没有因此整体插桩；新代码与旧库的验证范围必须区分。

## 5. 复查和操作

从项目根目录独立构建日志/状态组件：

```bash
python3 tests/build_ecc_json_validation.py build-ecc-json
build-ecc-json/ecc-state-tests
build-ecc-json/ecc-journal-tests
python3 tests/ecc_journal_integration.py build-ecc-json/getdaq-ecc-journal \
  build-ecc-json/ecc-journal-tests
```

这组不需要Qt或Ice SDK。正常与ASan/UBSan/LeakSanitizer构建均检查实际锁、部分写、进程退出/SIGKILL、重开门槛及只读CLI。测试生成的是临时小型日志，结束清理。

拓扑离线检查仍用`getdaq-align validate-topology FILE`。如果之前手写的是YAML，应按[系统对齐手册](SYSTEM_ALIGNMENT.md)中的JSON模板另存并审阅；不要把文件扩展名换成.json就认为内容已转换。已有JSON日志应先保留备份，用只读inspect确认，不能用配置格式转换工具处理控制日志。

## 6. 本阶段证明和限制

从既有不可变源码包提取迁移前YAML日志读取器编译，与新读取器检查同一规范历史日志：输出逐字节一致，原文件不变。原日志全套及新类型负向场景通过；拓扑类型/重复/未知键/JSON-only拒绝、固定ECC输入/恢复故障保持和普通两编码完整系统对齐通过。

当前原生CMake编译数据库没有yaml-cpp源文件，相关归档/执行链接命令不含yaml；独立日志程序的动态依赖只有系统库。日志/状态/CLI普通及插桩结果、75次严格C++11原生插桩构建和实际普通/新控制代码插桩网络流程分别保留。网络组的数据服务/receiver明确是普通程序，不能把它们计入完整插桩覆盖。

网络夹具只在同一物理主机生成小规模合成帧，校验后删除.graw，只保留计数、SHA、seal与日志；每轮总量远低于20GiB。没有连接真实板端、没有本阶段GUI全套或持续吞吐/断电/目标平台验收声明。

最后结果与独立证据入口见[验证记录](VALIDATION.md)；日志恢复原则见[持久化手册](ECC_JOURNAL.md)，依赖源码和板端缺口见[离线构建说明](DEPENDENCIES_AND_OFFLINE_BUILD.md)。整套重构仍需实际加载/控制权、完整硬件UNKNOWN恢复、未覆盖旧数据协议和目标平台验收。
