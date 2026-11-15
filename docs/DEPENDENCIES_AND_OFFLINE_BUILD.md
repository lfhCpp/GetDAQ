# 依赖源码保存与离线构建说明

本文面向使用和维护 GetDAQ 的人，不要求先熟悉 Ice。先说明目前有什么，再解释后续怎样做到“换一台电脑，也不必重新寻找已经失效的旧库”。

**目标：项目必需的专用依赖有本地源码副本，版本固定，修改有记录，构建不临时下载。** Qt、编译器和板端工具链另外保存清单及离线环境。主机端整套构建已使用本地专用依赖通过；真实板端互通与目标平台验收仍待，不能把源码保存或合成TCP通过理解为现场硬件已验收。

## 1. 目前实际保存了什么

| 内容 | 位置 | 当前状态 |
| --- | --- | --- |
| YAML 配置解析库 | `third_party/yaml-cpp/` | 已随现有控制台源码编译，不用另装 yaml-cpp |
| GET、MDaq、CompoundConfig 等历史源码 | `source/` | 用户提供，保留作为迁移和对照基线 |
| GET 的 Ice 接口定义 | `source/` 内各 `.ice` 文件 | 已有；它们不是完整 Ice 库 |
| 官方 Ice 3.5.1 源码压缩包 | `third_party/archives/ice-v3.5.1-40aac360.tar.gz` | 已接入独立兼容构建并通过本机 TCP 测试；未实机测试 |
| Ice 原版权与许可文件 | 压缩包内，以及 `third_party/licenses/ice-3.5.1/` | 原文保留；没有改成项目自己的许可证 |
| 源码包校验值 | `third_party/archives/SHA256SUMS` | 已生成并校验本地副本 |
| 依赖状态清单 | `third_party/dependencies.json` | 区分已集成、只归档、缺失和外部工具链 |
| CEA 的 IceEirfu 板端移植版 | 尚未取得 | 官方主机端 Ice 包不能代替它 |
| Qt、编译器、完整离线系统包 | 不在本仓库中 | 当前使用开发机自己的环境；离线工具链包待制作 |

Ice 压缩包约 5.6 MiB，保存的是官方该提交的完整仓库源码快照，没有只挑几个头文件。它不包括该库外部依赖的全部源码，也不包括用户设备可能使用的 CEA 补丁或板端工具链。

选择 Ice 3.5.1 作为历史备份，是因为本地 `GetBench.../ChangeLog` 的 2013-11-06 条目明确提到了它。另一个 `mordicus/trunk/src/ECC.cpp` 标注由 Ice 3.3.1 生成。该目录本地 `.svn/wc.db` 记录工作副本在远程`ECC/trunk`路径、revision 2010；根目录最后改动revision 1439，时间为2013-11-22。`README`和`NEWS`内容仍是Sample模板，`ChangeLog`为空，`configure.ac`还依赖MDaq。当前`tools/build-suite.py`不编译此ControlCore，只选用发布版MDaq/GetBench代码所需部分；保留该快照用于历史对照，不把它当作新版或现场必需依赖。以上是随本地副本保存的SVN元数据，不代表远端服务器当前状态。因此不能推断所有组件使用同一个 Ice 版本，也不能推断现场一定安装的是3.5.1。

## 2. Ice 到底在做什么

假设软件要读取 CoBo 上某个寄存器。

1. 控制程序表达“读取这个设备的这个寄存器”。
2. Ice 把对象名称、操作名称和参数变成双方约定的网络字节。
3. 板端服务收到请求，调用硬件访问代码。
4. 板端把读到的数值或错误按同一约定返回。
5. 主机端 Ice 解码结果，交给我们的控制程序。

Ice 不负责决定哪个寄存器应该写什么值，也不自动完成 CoBo 校准。这些GET专用逻辑由新原生硬件/ECC层实现，已有旧函数对照与合成TCP验证，现场效果仍待核对。

可以把 Ice 理解为主机与板端之间共同使用的“通信语言和收发工具”。换掉界面相当于换掉操作面板；换掉 Ice 则还要保证对面的旧板卡能够听懂。

**之前说“不能直接删掉 Ice”，是指不能丢失旧协议兼容，不是说 Ice 源码不能保存、编译或修改。** 合理路线是先保存源码，再隔离它的使用位置，最后根据测试决定保留、升级还是替换。

## 3. 四个容易混淆的东西

| 名称 | 通俗解释 | 要保存什么 |
| --- | --- | --- |
| `.ice` 接口文件 | 双方约定有哪些操作、参数和返回值 | GET / MDaq 原始文件及版本 |
| `slice2cpp` 等生成工具 | 把接口翻译成 C++ 代码的工具 | 工具源码、版本、生成命令 |
| 生成的 `.h/.cpp` | 生成工具输出的通信代码 | 可随项目保存，也应知道如何重新生成 |
| Ice 运行库 | 程序收发和解码消息时使用的实现 | 对应源码、必要依赖、补丁及构建方法 |

只保存 `.ice`，就像只保存了图纸，没有保存制造工具和实现。只保存生成代码，也可能因为工具与库的版本不一致而编译失败。本地旧 `ECC.cpp` 就包含 Ice 版本检查；不能为了绕过报错而简单删除这些检查。

还要区分三种“版本”：

- **库版本**：例如 Ice 3.5.1，表示使用哪一套实现。
- **网络协议/编码版本**：表示消息和参数如何组织。
- **GET 接口版本**：表示具体有哪些操作、参数顺序和类型。

ZeroC 文档说明，Ice 3.5 引入 1.1 编码，同时可以使用 1.0 编码与更早服务互通。但这不自动证明某个新库与现场 GET/IceE 接口完全兼容，需要核对接口和实际请求。见 [官方兼容说明](https://archive.zeroc.com/ice/3.7/release-notes/backward-compatibility-of-ice-versions) 与 [协议及编码版本](https://archive.zeroc.com/ice/3.7/ice-protocol-and-encoding/protocol-and-encoding-versions)。

## 4. “源码放进项目”具体怎样做

### 4.1 先归档，确保资料不会丢失

归档保存原始源码包，不更改里面的文件，同时记录来源、提交号和 SHA-256。SHA-256 是文件内容的校验值：复制过程中内容改变，校验值通常就会改变。它检查副本是否与记录一致，本身不是硬件兼容证明，也不是独立的上游签名。

当前 Ice 已完成归档，并进一步完成最小 C++ 核心的独立构建。源码在项目中，即使将来的网址失效，本地包仍可读取。

### 4.2 再集成，让工程真正使用本地源码

选定并验证版本后，建立工作源码目录，由构建脚本明确引用它。若为支持新编译器修改旧代码，保留原始包，另外记录修改补丁、原因和验证方法。

当前已实现 `third_party/ice-compat/CMakeLists.txt`，从本地归档构建并把工作源码及生成文件放在 `build-ice-compat/`。所需 bzip2/mcpp 也已归档。步骤、范围与验证见 [Ice 兼容组件](ICE_COMPATIBILITY.md)。

新 AsAd TCM/ISPM/DAC 参数计算仅用 C++11 标准库；Configure 的脉冲动作使用已经本地生成的 GET AsAdPulserMgr 接口，未增加新库。现场板端管理器仍负责计时，不要求 Qt 原始数据接收。其兼容边界和操作见 [AsAd 配置](ASAD_CONFIGURATION.md)，不能据此认为缺失的板端 IceEirfu 已被替代。

当前实际目录为：

```text
third_party/
  dependencies.json        固定版本、来源、摘要与许可路径
  yaml-cpp/                当前直接编译的GUI YAML源码与SHA256SUMS
  archives/                Ice/bzip2/mcpp原始包与SHA256SUMS
  licenses/                原始许可副本
  ice-compat/              本地解压、适配与接口生成的CMake入口
  validation/              仅供旧库对照的依赖

tools/
  verify-dependencies.py   对应构建profile的离线源码检查
  build-suite.py           统一主机构建与运行目录生成
```

不另设一个未实际使用的third_party/ice副本。可重建的工作源码与生成头/实现位于构建目录vendor/generated中；删除构建目录后从固定归档和收到的Slice接口重建。补丁/适配写在实际ice-compat构建入口，原压缩包保持不变。

这不要求把全部源码塞进一个 `.pro`。可以在同一项目中分模块构建，最后形成几个职责明确的程序。上游网址用于追溯，不作为每次编译时必须访问的地址。

不会只交付指向远程仓库的 submodule，也不会让普通构建自动下载“最新版”。若使用构建缓存，删除缓存后仍应能依靠交付包重建。

## 5. 为什么把 Ice 放在后台服务

```mermaid
flowchart LR
    UI[Qt 配置界面] -->|配置命令与状态| Control[硬件控制服务]
    Control --> Ice[项目内保存并构建的 Ice 兼容模块]
    Ice --> Board[现有板端 getHwServer]
    Board -->|原始事件数据| Router[独立数据接收和保存程序]
```

该边界已经落地：原生ECC/硬件准备、采集与正常收尾已接Qt控制入口，部分/冷恢复和现场验收仍在继续。这样安排后，在 Windows 检查界面时只需配置 Qt 和 MinGW，不必先解决旧 Ice 与 MinGW 的全部兼容问题。Ubuntu 主机运行硬件控制服务，由它连接板卡。

这是部署和维护上的分离，不代表源码或许可证问题因此自动消失。若以后需要让硬件控制服务也运行在 Windows 上，应另外验证它的工具链；“GUI 编译成功”不代表硬件后端通过。

## 6. 依赖还依赖其他库怎么办

当前最小 Ice 构建已按下述方式补齐本地 bzip2/mcpp，并验证不需要 Berkeley DB、OpenSSL、Boost、Bison/Flex 等额外开发包。这个结论仅针对这里选取的非 SSL C++ 核心，不适用于完整 Ice 安装或板端移植。

一个库有源码，但编译时还要求另一个旧库。如果只保存第一个库，问题仍然没有解决。这种“依赖的依赖”也要检查。

正式接入 Ice 前，应列出实际启用功能对应的全部依赖：哪些来自标准工具链，哪些还需保存源码，哪些属于不用的可选组件。不能声称离线构建，却在脚本内部临时联网安装其他东西。

1. 先明确需要的通信和平台功能。
2. 不把 Java、Python、数据库、管理服务等不用的部分全部加入 C++ 应用构建。
3. 根据实际构建确定需要补齐的库及生成工具。
4. 固定版本和本地路径。
5. 在没有旧 GET 安装、没有个人缓存的环境中验证。

保存完整源码包与只编译必需模块可以同时做到：前者方便长期恢复，后者控制日常构建复杂度。

## 7. 主机软件与板端软件为什么分别保存

主机运行 Ubuntu、Windows 或 macOS；板端可能运行 VxWorks 或特定嵌入式 Linux。两边的编译器、处理器架构和硬件驱动可能完全不同。

GET 原文档引用 CEA 的 **IceEirfu** 移植版，它不等于一般主机端 Ice。若以后重编译板端，还应保存匹配的 IceEirfu、GET 板端源码、补丁、板卡支持包、编译器和构建参数。目前资料没有全部提供，不能写成“板端已可离线重建”。

近期保持已有板端可用，通过兼容接口重构主机端。同时建立板端资料清单，逐项补齐，避免故障时才发现缺工具。

## 8. 离线交付需要几种包

| 交付物 | 解决什么问题 | 不代表什么 |
| --- | --- | --- |
| 源码包 | 能检查和修改软件，不必到旧网站找专用依赖 | 不能代替 Qt 和编译器 |
| 已编译运行包 | 实验人员按步骤启动软件 | 不能代替源码和构建方法 |
| 离线构建环境包 | 在指定平台重建程序 | 不保证所有系统版本都直接可用 |
| 现场配置备份 | 恢复地址、实验参数、启动方式和匹配版本 | 示例不能代替真实实验配置 |

第一层目标是：电脑已有匹配的 Qt 和编译器时，构建专用软件无需联网。

更完整的目标是：为确认过的 Ubuntu 环境保存安装包或构建环境镜像；Windows 保存准确的 Qt/MinGW 版本和可用离线安装材料。环境镜像本体也要留在本地，只有一个容器下载地址仍不能保证将来可用。

“从空电脑恢复”与“已有 Qt 的电脑编译项目”是不同验收项目，会分别写步骤、记录结果。完整离线环境包目前尚未制作。

## 9. 编译前检查本地依赖

在源码根目录运行统一检查：

```sh
python3 tools/verify-dependencies.py --profile full
```

预期为PASS及109个已核对文件。control检查95个文件且不要求Ice；data和analysis均为零个本地第三方文件，数据面无需`third_party/`目录；data-host只核对1个依赖元数据文件。数字来自当前`tools/verify-dependencies.py`及`third_party/dependencies.json`，源码或清单调整后应以脚本实际回执为准。追加`--json`输出每个实际文件的SHA-256，可保存为自己的交付回执。全套构建入口自动执行对应profile检查，在SDK查询/编译和创建构建目录之前拒绝缺失或损坏的本地依赖。

yaml-cpp的93个文件有固定完整清单，新增未审阅源码、改动、缺失、越界路径或用符号链接替换均拒绝。三个归档同时核对大小、dependencies.json摘要与archives/SHA256SUMS，并检查声明的许可文件存在。校验只证明当前副本与本地固定记录一致，不提供外部真实性认证，也不检查全部第一方源码、工具链、硬件或未来断电行为。

Windows若安装Python3，可在项目根目录用同样命令；只编译操作端选择control。Qt Creator本身不会执行Python预检查，使用其独立构建时可先手工运行；该脚本不是C++程序的运行依赖。

### 单独复查归档


以下只检查文件，没有编译或安装 Ice，也不会连接硬件。

### Ubuntu / Linux归档检查

打开终端，从项目根目录进入源码包目录：

```bash
cd third_party/archives
sha256sum -c SHA256SUMS
```

预期输出：

```text
ice-v3.5.1-40aac360.tar.gz: OK
```

显示 `FAILED` 说明内容与清单不一致。先检查是否复制完整、是否拿错包，不要直接改清单让它“通过”。显示找不到文件时，先确认当前目录。

查看压缩包中的文件：

```bash
tar -tzf ice-v3.5.1-40aac360.tar.gz
```

这只列出内容，不解压。源码顶层目录包含完整提交号，C++ 代码位于其 `cpp/` 下。

### Windows PowerShell

在项目根目录打开 PowerShell，执行：

```powershell
Get-FileHash -Algorithm SHA256 .\third_party\archives\ice-v3.5.1-40aac360.tar.gz
```

把 `Hash` 一列与下面值比较，大小写不影响结果：

```text
fb119b51c62f9efcff70705aa60e4fa8aad2e8ea1265d4558825e6896e298b24
```

Windows 命令本轮未实测；本轮已在 Linux 验证实际归档的校验值和清单。

## 10. 怎样才算“依赖整理好了”

完成标准不是“源码文件夹里多了一个 Ice 目录”，而是全部满足：

1. 每个必要依赖有确定版本、本地源码、来源、许可文件和状态。
2. 编译只用交付包和列出的工具链，不偷偷使用 `/usr/local` 中的旧 GET 库。
3. 断网、全新构建目录和空缓存条件下能构建目标程序。
4. 通信代码生成工具与运行库匹配，生成命令可以重复执行。
5. 与实际板端完成只读查询、参数设置、错误返回、超时等兼容验证。
6. 初始化、配置、启停和单板维护有旧版对照记录。
7. 下一位使用者仅看文档也能完成安装、配置和基础检查。

源码校验、编译通过、模拟测试通过、实机通过是四个不同结果，文档分别标注。Ice主机兼容核心、生成工具、GET接口和新ECC已本地构建并完成合成TCP；默认原生数据程序不依赖Ice/Qt。实际板端和持续吞吐仍待，结果见[验证记录](VALIDATION.md)。

## 11. 文档应该详细到什么程度

不要求使用者从几个命令猜测整个过程。每个步骤按下面格式编写：

| 项目 | 要写清楚的内容 |
| --- | --- |
| 目的 | 为什么要做，完成后解决什么问题 |
| 操作位置 | Windows 主机、Ubuntu 主机还是某个采集节点；在哪个目录 |
| 前提 | 先启动什么，设备应处于什么状态，需要哪些文件 |
| 输入 | 完整命令或按钮路径；哪些文字需要替换 |
| 预期结果 | 应看到的状态、文件、日志或截图 |
| 失败排查 | 常见报错含义、先检查什么、如何回到可继续的状态 |
| 影响范围 | 是否改配置、写硬件或移动数据，影响单板还是全系统 |
| 原理关联 | 对应哪个服务、状态或协议，链接到原理说明 |

例如，不只写“启动 ECC”，还要说明在哪台机器启动、配置目录是什么、监听哪个地址和端口、怎样确认成功、GUI 如何连接、失败时去哪里找日志。

最终文档分为：面向操作者的上手和操作手册、面向排障的维护手册、帮助理解系统的原理手册、面向开发者的架构和构建手册。术语第一次出现时用中文解释；图中的每条连接说明传递的是命令、状态还是原始数据。

## 12. 本轮确定记录

- 官方仓库：<https://github.com/zeroc-ice/ice>。
- 标签：`v3.5.1`，提交：`40aac3604b78513769adfa9246cc8afdb9287bc1`。
- 保存日期：2026-10-03。
- 压缩包：5,849,186 字节，9,413 个归档条目。
- 原始版权/许可文件随包保留并另行复制；当前可选 `ice-compat` 目标从本地包编译并静态链接所需模块，Qt GUI 和原生数据接收器不链接 Ice。
- [方案入口](SOURCE_REVIEW_AND_PLAN.md)。
- [机器可读状态](../third_party/dependencies.json)，明确标记 `complete_offline_hardware_build: false`。

## 独立 ECC 日志组件的依赖

`bash tools/build-linux.sh ecc`构建状态核心、POSIX持久化日志和只读查看工具。状态/日志只需标准C++11、线程/POSIX及共享StrictJson；描述、拓扑和日志均已移除yaml读取与链接，见[原生JSON迁移](NATIVE_JSON_DEPENDENCIES.md)。原生Ice/ECC构建也不再编译yaml-cpp，Qt操作台仍用本地源码处理真正envoy YAML；不需另装JSON库。日志不依赖Qt或Ice，不成为数据接收器的链接依赖。

只构建纯状态核心，可在独立 CMake 构建时设置 `-DGETDAQ_ECC_JOURNAL=OFF`。日志组件当前只在 UNIX 构建，实际验证环境仍是 Ubuntu 20.04.6。构建、文件格式及当前未完成的真实恢复/服务入口见 [ECC 日志](ECC_JOURNAL.md)。

## 实验参数计算核心的依赖

`bash tools/build-linux.sh experiment` 单独构建 `getdaq-experiment-core` 和单片/整板 AGET 测试，只需 C++11 编译器/CMake/系统运行库，无 Qt/Ice/YAML。Qt 工程直接编译同一份计算源码，`getdaq-config compile-aget/compile-cobo-aget` 的文档读取/来源/JSON 由现有 Qt Core 完成。可选 Ice 工程链接同一个原生计算目标以生成 AGET 写入子序列；此链接不会引入 Qt，也不改变数据接收器依赖。没有增加外部库安装或网络下载。映射来源的 CeCILL 许可副本见 `third_party/licenses/getbench/COPYING`，操作见 [AGET 配置手册](AGET_CONFIGURATION.md)。

同一目标现在也计算 CoBo Module/触发/零抑制/循环缓冲区、AsAd TCM/ISPM/pulser、Prepare 基础值/输入槽位、整板 PLL/LCM 时钟、AsAd 上电/监控保护及 AGET/延迟校准策略；独立核心测试为八项 CTest。`compile-cobo-readout`、`compile-cobo-configure`、`compile-cobo-clock`、`compile-cobo-monitoring`、`compile-cobo-calibration` 使用共用 Qt Core 标量检查和来源适配，原生 Configure/Prepare/ClockInit/校准子流程复用纯计算结果，不新增依赖。原算法对照脚本仅在测试时需要 Python/编译器，不成为运行依赖。详细边界见 [CoBo 读出配置](COBO_READOUT_CONFIGURATION.md)、[AsAd 配置](ASAD_CONFIGURATION.md)、[Prepare](COBO_PREPARE.md)、[时钟说明](COBO_CLOCK_CONFIGURATION.md)、[上电监控](ASAD_MONITORING.md)、[AGET 校准](AGET_CALIBRATION.md)。这些模块不会进入原生接收器依赖，也不代替尚缺的板端 IceEirfu/工具链。

LED、板端告警检查器和主机回调也复用本地Ice/GET及POSIX/标准库，不引入Qt、Boost、log4cxx或新的运行库。getdaq-hardware提供watch-asad-alarms观察入口；Python仅用于可选CLI测试。回调日志与原始数据写入分属不同进程/职责，逐条控制记录fsync不会加入MFM接收热路径。构建、监听端点、资源预算及仍缺的生产协调见 [服务详细说明](COBO_SERVICES.md)。

MuTaNT参数计算和原生控制也复用相同三层，没有新增运行库；compile-mutant仍只依赖现有Qt Core，MuTaNT硬件库使用本地Ice/GET。原源码函数对照只在测试时需要Python/C++编译器。布局、时钟策略、计数位宽和系统对齐边界见 [MuTaNT详细说明](MUTANT_CONFIGURATION.md)。

系统对齐主机层复用纯C++11 StrictJson、本地Ice/GET和POSIX，getdaq-align不链接Qt/yaml。板端AlignmentServer接口来自保留的原始.ice，这不补齐缺失的板端TDC算法或IceEirfu。独立旧主机函数对照只在测试时使用Python/标准编译器。原始接收器依赖不变，详见[系统对齐](SYSTEM_ALIGNMENT.md)。

整系统实验参数现已有`compile-system`离线工件、纯C++11严格读取/摘要和原生ID/角色绑定，接续同一SystemPreparationClient；详细profile、命令、数据精度、配置输入审计与运行前提见[完整实验配置手册](SYSTEM_EXPERIMENT_CONFIGURATION.md)。这不新增接收器依赖，仍未接生产ECC发布/加载版本和网络状态。

DataSenderClient/DataLinkSetClient复用相同本地Ice/GET与POSIX日志，没有新运行库。可通过GETDAQ_RECEIVER_TEST_BINARY显式选择独立receiver来登记跨进程测试；Python仅为测试依赖，接收器仍不链接Qt/Ice/yaml。构建命令、参数和EOF边界见[发送器控制手册](DATA_SENDER_CONTROL.md)。

系统采集协调SystemAcquisitionClient同样复用现有原生硬件库，没有Qt或新运行库。实际顺序、失败后的生产停止、构建和独立旧源码对照见[系统采集手册](SYSTEM_ACQUISITION.md)。Python/C++参考编译器只用于测试，板端缺失的IceEirfu/TDC并没有因此补齐。
getdaq-system-describe/SystemDescriptionClient复用本地Ice/GET和标准C++11/StrictJson/POSIX，不再需要yaml，未增加Qt依赖；逐条控制审计不进入MFM接收路径。详见[全系统描述手册](SYSTEM_DESCRIPTION.md)。

系统Breakup/Unprepare复用getdaq-hardware-core和本地Ice/GET、严格JSON描述解析，不需要yaml，不给原生receiver引入这些依赖。库协调及恢复边界见[结束与撤销准备](SYSTEM_RELEASE.md)。

## 旧 ECC SOAP 服务的依赖状态

旧 GetBench 源码仍包含 SOAP 客户端/服务端实现和 `ecc.hh`。现在项目固定了 gSOAP 2.8.124 源码包及原始许可证文件，并提供**仅供测试**的旧ECC服务兼容构建路径。它用于按旧接口生成绑定并启动隔离服务进程，不属于默认生产程序、GUI、receiver或控制服务的链接依赖。

2.8.124 是本项目为重建 `soapcpp2` 与运行时夹具而固定的兼容验证版本；旧项目的 `configure.ac` 只证明其遇到 gSOAP >= 2.8.13 时会启用 `-z1`，没有证明现场原软件实际使用的精确版本。因此本地互操作结果能证明“旧 `ecc.hh` + 2.8.124 生成绑定/服务”与当前客户端的线协议兼容，不证明现场部署中 gSOAP 二进制版本相同，也不证明板卡业务行为相同。

许可边界需要保留：gSOAP 包中各组件分属 gSOAP Public License 或 GPLv2；`soapcpp2` 和其生成的源文件属于 GPL 范围。兼容探针必须保持在单独的测试进程和测试构建目录，不得把生成文件或 gSOAP runtime 链接进交付程序。随包保留的 `LICENSE.txt`、`GPLv2_license.txt` 和 `license.pdf` 是许可审阅依据；若未来要随正式产品分发生成服务或生成代码，先进行独立许可审查。

源码包还保存了 Visual Studio 构建目录中的预生成 Flex 扫描器和 Bison 解析器。无 flex/yacc 的离线环境可以用它们编译生成器；生成扫描器时不能定义 `WITH_LEX`，否则其 EOF 分支不返回结束标记，导致 `soapcpp2` 在输入结束后循环。该细节由本地生成器构建与真实旧 `ecc.hh` 绑定生成复现。项目级进程间互操作测试与具体命令见 `tests/legacy-gsoap/README.md`。该探针仍不代替现场旧服务抓包、完整旧服务启动、板卡或持续DAQ吞吐验收。
