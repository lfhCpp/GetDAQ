# 第三方源码与依赖

必要专用依赖随项目本地保存，构建不临时下载。Qt、编译器和操作系统开发包由对应平台提供；它们不是本目录中的源码库。板端getHwServer/IceEirfu保留现有部署，不需要为了编译主机端重新构建板端。

| 内容 | 本地位置 | 版本与使用者 |
| --- | --- | --- |
| yaml-cpp源码 | `yaml-cpp/` | 0.8.0，提交f7320141120f720aecc4c32be25586e7da9eb978；仅操作端的实际envoy YAML读取 |
| Ice源码归档 | `archives/ice-v3.5.1-40aac360.tar.gz` | 3.5.1，提交40aac3604b78513769adfa9246cc8afdb9287bc1；原生GET硬件兼容边界及可选ICE数据组件 |
| bzip2源码归档 | `archives/bzip2-1.0.8.tar.gz` | 1.0.8；本地Ice压缩支持，静态构建 |
| mcpp源码归档 | `archives/mcpp-2.7.2-4ice-671886cb.tar.gz` | 固定ZeroC提交；本地构建slice2cpp所需的预处理器 |
| gSOAP旧ECC兼容测试归档 | `archives/gsoap-2.8.124.orig.tar.gz` | 仅生成旧`ecc.hh`绑定并构建独立测试服务；GPL相关生成器/生成文件仅用于测试，不进入产品程序 |
| CoBoFrameViewer ROOT对象模型 | `coboframe-root-model/` | 随源码保存旧`GDataFrame/GDataChannel/GDataSample/GFrameHeader`与`T2KEvent`类及许可；ROOT6离线转换器写`GET::`新类名，查看器同时带有隔离的`get::`旧类名字典以只读兼容ROOT5时代文件 |
| GET/MDaq Slice接口 | `../source/`相应release目录的`.ice` | 用户提供的固定历史接口，配合本地slice2cpp生成；不能把接口文件当作运行库 |
| 原许可 | `licenses/`、各库LICENSE、`ATTPC-LICENSE` | 保留上游原文；不因整合目录而统一改为项目MIT许可 |
| 本地兼容构建 | `ice-compat/CMakeLists.txt` | 从固定归档构建Ice/生成工具/GET接口，输出到构建目录 |
| 校验清单 | `archives/SHA256SUMS`、`yaml-cpp/SHA256SUMS` | 分别绑定四个压缩包和93个已保存yaml源码/许可文件 |
| 机器可读状态 | `dependencies.json` | 库版本、来源、摘要、许可路径与组件边界；旧独立库的布尔字段不代替系统集成状态 |
| 旧解析器对照依赖 | `validation/` | Boost等仅用于验证旧CompoundConfig，不进入新版生产构建；见[说明](validation/README.md) |

## 编译前先检查

从工程根目录运行：

```sh
python3 tools/verify-dependencies.py --profile full
python3 tools/verify-dependencies.py --profile control
python3 tools/verify-dependencies.py --profile data
```

完整主机套件检查109个文件：依赖元数据、两个校验清单、93个yaml源文件、四个归档及对应许可。control检查95个文件，不要求Ice或SOAP测试归档；data没有本地第三方库依赖，检查数为零。data-host使用系统Qt构建低频管理服务，不要求yaml、Ice或SOAP测试源码。`--json`只向标准输出打印准确检查回执，不写文件。

统一构建脚本会自动执行对应检查。缺文件或摘要不一致时停止；先重新复制正确来源，不能修改校验表使损坏副本“通过”。脚本不下载、不解压、不运行硬件命令，不证明所有第一方源码完整、工具链可用或真机兼容。

## 运行时的依赖边界

| 程序或层 | 使用的专用库 |
| --- | --- |
| 普通TCP/ZBUF/FDT receiver与inspect | 无Qt/Ice/yaml；标准C++11、POSIX和线程 |
| 可选ICE数据程序 | 本地Ice/GET；不链接Qt，独立于默认receiver部署 |
| 原生硬件、ECC核心与接班CLI | 本地Ice/GET和标准C++11 StrictJson；不链接yaml/Qt |
| ECC、数据节点低频HTTP服务 | 原生核心加Qt Core/Network/Concurrent；不转发原始载荷 |
| GetDAQ操作台 | Qt Widgets/Network及本地yaml-cpp；只处理配置、控制、元数据 |
| 可选 `getdaq-root` / `getdaq-root-viewer` | 系统ROOT 6 Core/RIO/Tree；后者另需Qt 5 Widgets；离线转换/浏览，默认不构建 |
| 旧ECC SOAP互操作探针 | gSOAP 2.8.124生成器/运行时，单独的测试服务进程；不进入默认构建或运行包 |

原生描述、拓扑、工件和ECC日志已改为标准C++11严格JSON解析。`common/cmake/GetDAQLocalYaml.cmake`作为历史/可选入口保留，不能仅凭磁盘存在旧yaml静态库就认为当前原生目标仍链接它。硬件准备/采集/收尾已接生产协调器，组件本身保持上述依赖边界。

源码归档的官方上游地址用于追溯，不是日常构建条件：yaml-cpp来自jbeder/yaml-cpp，Ice和mcpp来自ZeroC，bzip2保留原归档与许可。具体URL、提交、SHA及字节大小见dependencies.json。历史GET源的CeCILL等许可仍按各来源保留。

本机已验证主机兼容构建及合成TCP；实际板端、Ubuntu24/macOS及独立Windows完整验收仍待。完整步骤、离线工具链准备及“保存源码”和“验证硬件”的区别见[详细依赖说明](../docs/DEPENDENCIES_AND_OFFLINE_BUILD.md)，主机程序交付见[统一构建](../docs/HOST_SUITE_BUILD.md)。
