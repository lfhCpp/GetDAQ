# Ubuntu24构建与软件验证

**最新Ubuntu24当前源码full套件与候选（71.223，2026-10-22）：** 在已验签的Ubuntu24.04.5项目用户空间中，从包含`HttpClient::Cancel()`回退防护的当前源码构建28/28程序，109项嵌入依赖核查通过；Core151/151、Widgets116/116、CTest23/23及配置迁移、配置事务、receiver传输/安全、失败handoff窗口均通过，标准runner 23条命令全部成功。四路localhost负载共1GiB，测试后删除1,073,741,824字节，handoff raw也清理完成，验证树内raw剩余0。由同次构建生成候选`build-packages/getdaq-ubuntu24-20261022-71-223-httpclient-cancel-candidate/`；28程序包内Ubuntu24 loader启动smoke通过，回执在候选`validation/`。Qt5.12.8本机Widgets116/116，`backupRetryDoesNotRecatalog`单项及其20次重复均通过。此前一次失败的触发原因仍未知；新取消逻辑是防御性改进，不宣称已证明修复旧失败根因。详见[验证记录71.223](VALIDATION.md#71223-qt-网络请求取消健壮性与-ubuntu24-全套回归2026-10-22)。该用户空间共用WSL2宿主内核，不等于独立Ubuntu机器，不代表实板、物理双机或持续DAQ吞吐验收。

此前71.222候选对应原源码版本；最新全量验证绑定71.223候选的精确28个程序SHA。71.221和71.222均保留为历史候选。

**最近的功能性候选增量验证（71.201，2026-10-22）：** 新包 `build-packages/getdaq-ubuntu24-20261022-71-200-pulser-scan-candidate/` 从71.190 full候选复制，仅替换 `getdaq-config`，其余27程序沿用旧候选；扩展后的Pulser/PeakingTime配置集成在Ubuntu24.04.5 loader下59/59进程通过，28程序包内启动smoke通过，最终186项manifest完整。它不是当前源码全量重编译。见[验证记录71.201](VALIDATION.md#71201-pulser-扫描计划刷新-ubuntu24-候选2026-10-22)。

**最近一次完整Ubuntu24套件（71.191，2026-10-22）：** 在Ubuntu24.04.5项目用户空间按当前源码构建`full` profile 28个程序，构建回执绑定951项源码/测试/工具输入；Core151/151、Widgets114/114、CTest22/22、配置迁移/事务、receiver传输/安全、四路1GiB接收和失败handoff窗口均通过，runner 23条命令全部成功。合成wire为1,073,799,755字节，handoff另有8,192字节raw；清理收据和runner最终状态均为`raw_remaining=0`。候选`build-packages/getdaq-ubuntu24-20261022-71-190-current-source-full-candidate/`由同次构建的精确二进制生成，28程序/180项完整清单验证通过，28程序经目标loader帮助/启动smoke通过。项目用户空间共享WSL2宿主内核，不能代替独立Ubuntu机器、现场硬件或持续物理DAQ验收。详细结果见[验证记录71.191](VALIDATION.md#71191-ubuntu24-当前源码-full-suite-与候选重建2026-10-22)。


用户主要部署目标是Ubuntu24。现有开发宿主是Ubuntu20.04，因此不能用旧宿主的编译成功替代Ubuntu24验证。本页提供两种执行方式：在真实Ubuntu24上直接运行，或使用项目内的Ubuntu24用户空间做兼容性检查。

**历史完整源码快照（71.122，2026-10-21）：** Ubuntu24.04.5已验签用户空间/WSL2宿主内核使用GCC13.3.0、CMake3.28.3和Qt5.15.13，重建full profile 27个程序；核心QtTest151/151、Widgets106/106、原生CTest22/22及相应合成场景通过。该轮候选、回执和平台限制仅描述71.122历史源码，不作为当前版本证据；当前源码状态以上面的71.191为准。详情见[验证记录71.122](VALIDATION.md#71122-旧-ecc-gsoap-双进程互操作与qt完整回归2026-10-21)。

**历史验证（71.85，2026-10-21）：** 在经官方签名和SHA256验证的Ubuntu24.04.5用户空间中，经项目WSL runner对当前源码执行完整`--stage all`。GCC13.3.0/CMake3.28.3/Qt5.15.13构建full profile全部27个程序；Core 149/149、Widgets105/105、原生CTest22/22、来源迁移/配置事务、接收协议/安全和新增Qt失败交接窗口端到端夹具通过。合成wire数据总计1,073,799,755字节（约1.074GB），远低于用户20GiB上限；raw清理回执为0残留。新候选`build-packages/getdaq-ubuntu24-20261021-71-84-current-source-full-candidate/`包含27程序和174项manifest，当前GetDAQ SHA=`df50dc5db337414d8d0155cc103a178b4b5456dbb8a9c4940620a0cc25124e22`；其余26程序来自同一当前源码full构建。逐程序依赖检查由该Ubuntu24构建回执完成；GetDAQ候选通过Ubuntu24 loader、帮助、offscreen窗口/ready日志、截图以及SOAP/UI专项测试。详情见[验证71.85–71.86](VALIDATION.md#7185-ubuntu24045当前源码全套验证纳入失败交接窗口-2026-10-21)。用户Qt5.14.2 Windows当前源码构建与原生QtTest也通过（见[Windows手册](WINDOWS_QT5142.md)）。Ubuntu24用户空间共用WSL2宿主内核，仍不等于独立Ubuntu24机器。macOS原生实机、双机普通TCP、真实旧gSOAP服务、板端与持续DAQ吞吐尚未验收。

71.87新增失败交接窗口对缺失原生查询helper的保护，并通过Ubuntu20.04/Qt5.12.8 Widgets 106/106和Windows Qt5.14.2本机验证；该中间检查点已由上方71.93当前源码Ubuntu24全套回归覆盖。

**历史进度（71.62，2026-10-21）：** 当前Ubuntu24组合候选为`build-packages/getdaq-ubuntu24-20261020-71-34-current-source-candidate`，27程序/484 manifest文件；它基于26程序/477文件的历史全量构建，后续增量加入`getdaq-matrix`、刷新`getdaq-sentry`并更新`getdaq-receiver`，仍未从当前源码全量重建27个程序。71.57在Ubuntu20开发宿主对当前源码重新full构建27程序，并运行核心149、Widgets103、CTest22、配置迁移/事务及当前接收器回归；还修正了平台runner过期的Widgets计数门槛。Ubuntu24本机WSL辅助入口在Windows进程启动时仍报`UtilBindVsockAnyPort: socket failed 1`，runner未启动。为继续获得目标用户空间的当前源码证据，71.62新增GitHub Actions `ubuntu24-current-source-full-suite` job，在官方`ubuntu-24.04` runner执行完整`--stage all`，并在失败后清理隔离工作目录中的`.graw`；本机只完成workflow/计划静态核验，Actions尚未运行，不能写成Ubuntu24验证通过。71.51接收器源码SHA=`c0749e333988546c66414cda63db53378e3f34a0dc0e17a76612e477b3a980a0`，delivery SHA=`8d0f35b61fe33448760ca6e5834b0f1f14de6094282eddb85ddfcd31b65f7b3d`。71.52候选内receiver慢客户端/数据落盘通过，71.56候选四路18GiB loopback轮转通过且raw已删除；二者运行在WSL宿主内核，18GiB仍低于主机内存，不是独立Ubuntu24机器或持续吞吐验收。其余macOS、真实双机/防火墙、板端和现场DAQ也未验收。详细状态见[重构状态](REFACTOR_STATUS.md)及[验证71.57–71.62](VALIDATION.md#7157-当前源码full构建套件回归与widgets门槛修正2026-10-21)。

## 1. 在Ubuntu24机器上直接验证

安装正常的系统开发工具与Qt SDK。专用旧依赖仍使用仓库中的本地源码，不从网络临时拉取Ice、yaml-cpp或GET接口：

```bash
sudo apt-get install --no-install-recommends \
  build-essential cmake python3 qtbase5-dev qt5-qmake \
  pkg-config fonts-dejavu-core ca-certificates
```

然后使用普通用户运行。输出目录选择源码目录之外的一个新目录，例如：

```bash
python3 tests/ubuntu24_platform_validation.py \
  --work "$HOME/getdaq-platform-ubuntu24" --jobs 2 --stage all
```

不要使用sudo运行此验证脚本。目录权限拒绝测试在root身份下可能失去意义。脚本先核对`/etc/os-release`确为Ubuntu24.04及当前用户非root，不符合时在创建测试输出前拒绝。

这个入口当前覆盖x86_64。它不代表其他CPU架构、独立Windows或macOS已验证。Qt SDK属于正常开发工具链；生产原生数据接收器仍不因此增加Qt依赖。

## 2. 在现有WSL中准备隔离的Ubuntu24用户空间

仓库提供：

```bash
python3 tools/validate-ubuntu24-from-wsl.py bootstrap
```

该辅助工具使用已经安装的WSL发行版作为内核和引导宿主，在项目专用目录创建根文件系统，不注册另一个WSL发行版。默认现有发行版名称为`Ubuntu`；实际名称不同可在动作前提供`--distribution NAME`。

基础镜像固定为Ubuntu Base 24.04.5 amd64，来自[Ubuntu官方镜像目录](https://cdimage.ubuntu.com/ubuntu-base/releases/24.04/release/)。工具先用已安装的Ubuntu信任公钥核对[SHA256SUMS签名](https://cdimage.ubuntu.com/ubuntu-base/releases/24.04/release/SHA256SUMS.gpg)，要求准确签名指纹，再用已认证的[校验清单](https://cdimage.ubuntu.com/ubuntu-base/releases/24.04/release/SHA256SUMS)核对压缩包。校验失败不会继续解包，也不通过关闭验证来安装。

目录如下：

```text
build-ubuntu24-validation/
  downloads/                 官方镜像、校验清单、签名及校验回执
  rootfs/                    项目专用Ubuntu24用户空间
  environment-receipt.json   已安装软件包版本和引导状态
  tasks/                     明确执行的命令及退出记录
```

引导安装仅在该根文件系统中进行。随后构建及测试使用uid1000，源目录只读挂载到`/code`，可写输出位于`/build`。每次使用私有挂载和PID命名空间，退出时释放本次挂载和命名空间中的进程；不把宿主整棵`/dev`作为硬件通道暴露进去。

已经ready的环境再次执行bootstrap不会悄悄更新工具链。若需要新的发行版镜像或SDK版本，应明确准备新环境并保留旧回执，不能用升级后的库冒充以前执行时的版本。

这是构建隔离和用户空间兼容性工具，不是安全容器产品，也不模拟Ubuntu24的独立内核、硬件、图形桌面或真实磁盘环境。

## 3. 在隔离环境执行

准备完成后：

```bash
python3 tools/validate-ubuntu24-from-wsl.py run -- \
  /usr/bin/python3 /code/tests/ubuntu24_platform_validation.py \
  --work /build/getdaq-platform --jobs 2 --stage all
```

该命令需要启动既有WSL中的受控引导入口，但实际编译与测试已经降为普通用户。输出从宿主可在下面位置查看：

```text
build-ubuntu24-validation/rootfs/build/getdaq-platform/
```

生成或更新Ubuntu24 full候选后，可再按候选清单启动所有交付程序。Ubuntu20宿主检查Ubuntu24二进制时传入已验签的rootfs；直接在Ubuntu24上运行时可省略`--rootfs`：

```bash
python3 tests/ubuntu24_package_program_smoke.py \
  --candidate build-packages/getdaq-ubuntu24-20261022-71-182-current-source-full-candidate \
  --evidence build-receiver/ubuntu24-package-smoke.json \
  --rootfs build-ubuntu24-validation/rootfs
```

该检查从full build计划核对完整程序清单，CLI使用`--help`；GetDAQ启动初始窗口、保存临时PNG后退出。程序临时HOME、缓存、运行目录和截图位于私有临时目录，退出时自动删除。它不连接硬件，也不生成DAQ raw。

执行前可只查看计划：

```bash
python3 tests/ubuntu24_platform_validation.py --stage plan
```

plan模式只输出命令，不表示命令已经执行。它允许在其他系统查看；正常build/run/all模式仍检查真正运行时的Ubuntu版本。

## 4. 验证包括哪些部分

| 部分 | 计划执行及证据 |
| --- | --- |
| 完整产品 | 统一full profile构建，核对当前计划中的29个主机程序及运行依赖（含ECC控制MCP） |
| 配置核心 | QtCore核心回归，严格C++11编译；当前runner门槛为151项通过 |
| 操作界面 | offscreen完整QWidget回归；当前runner门槛为114项通过 |
| 原生契约 | 配置参数、版本/读回协议、ECC状态机与持久日志、接收分帧/队列/存储及导出锁隔离；CTest数量从本次构建的注册表动态读取并要求全部通过（71.57为22项），不要求Qt或Ice |
| 来源并发及迁移 | 两个当前源码探针的真实进程中断、明确恢复和并发拒绝场景 |
| 原生接收器 | full产品中的实际receiver执行TCP/ZBUF/FDT等12个小型回环传输场景，逐字节校验并删除自有raw |
| 并发接收压力 | 当前full产品中的receiver启动四个真实子进程，每路256MiB/128MiB轮转，总计1GiB；校验每流/每文件SHA、EOF、封存并自动清理；与同轮其他传输合计受20GiB上限约束 |

完整构建和测试程序分别保存源码输入及程序摘要。单独运行`--stage run`时，必须仍与当前源码和已编译程序匹配；修改源码后应先重新build，不能拿上一次程序证明当前代码。

可以分两步排查失败：

```bash
python3 tests/ubuntu24_platform_validation.py --work /build/getdaq-platform --stage build
python3 tests/ubuntu24_platform_validation.py --work /build/getdaq-platform --stage run
```

隔离环境中执行时，这两条命令也应放到上一节WSL辅助工具的`run --`后面。

## 5. 怎样看结果

每次执行有独立的`platform-UUID.json`，逐条命令保存参数、工作目录、日志、退出码和耗时。状态为running或failed时不计通过；日志中某几项PASS也不能替代整组完成。

- `delivery/full/build-receipt.json`：产品构建及程序摘要。
- `test-build-receipt.json`：Qt测试和进程探针的当前源码/程序绑定。
- `core/result.txt`、`widgets/result.txt`：完整QtTest结果。
- `logs/`：编译、CTest和各运行组日志。
- `evidence/`：配置进程场景和原生传输的有界元数据证据。

`--stage build`完成只表示编译成功，不能当作`--stage run`通过。用户空间回环场景也不能当作真板、实际电气配置或持续高速采集已验收。

原生CTest从明确的构建目录启动，并先读取本轮注册的测试数，再要求运行结果覆盖全部测试，避免新增用例后沿用过期的固定计数。不能只检查返回码0：旧版CTest可能忽略不支持的目录选项，在错误目录报告零测试而仍返回0。

## 6. 数据量及清理

验证只使用自有合成数据。来源并发及文件迁移不产生DAQ数据；传输场景使用小型帧，保留准确字节数和清理回执，继续执行全部传输流合计不超过20GiB的用户限制。

原生传输脚本及四路压力脚本在正常结束、失败和可捕获中断时删除本组自有`.graw`；配置及界面临时文件按各夹具生命周期清理。持续接收脚本的并发锁放在`TMPDIR`，不写入只读源码挂载。总入口将接收传输与四路负载合计后检查不超过20GiB，并核对自有输出目录无`.graw`残留。失败后不要按模糊目录名删除其他任务数据，应先核对具体夹具归属。

Ubuntu基础镜像、SDK和编译产物不是DAQ测试文件。它们留在专用build目录供重复验证；安装后的空间以本机实际为准，不将构建工具链大小作为DAQ吞吐指标。

## 8. GitHub Actions上的Ubuntu24当前源码全套验证

仓库工作流`.github/workflows/native-data-path.yml`包含`ubuntu24-current-source-full-suite` job，运行于GitHub托管的`ubuntu-24.04` x86_64 runner。它安装发行版C++/Qt5开发工具，然后以普通runner用户执行：

```bash
python3 tests/ubuntu24_platform_validation.py \
  --work /tmp/getdaq-ubuntu24-full-suite --jobs 2 --stage all
```

该命令从当前提交源码重建full profile全部29个程序和独立Qt/native测试，再运行本文第4节列出的完整矩阵。临时输出置于源码树外；runner自身要求至少4GiB工作空间。工作流末尾的`always()`步骤检查此专用目录并删除测试失败遗留的`.graw`，发现遗留时仍将job标为失败。之后另一个always步骤会将构建/测试摘要、平台和专项回执及诊断日志保存为14天Actions artifact；选择上传的文件不包括`.graw`。

workflow和命令计划的静态核验只能证明验收入口已配置。需等GitHub Actions实际完成且job全绿，才能记录该次Ubuntu24 runner验证；该托管虚拟机仍不是用户独立部署机、真实双机网络、板卡或持续DAQ吞吐验收。查看Pull Request的Checks，或在Actions页面手动运行`Native data path`工作流。

## 7. 历史Ubuntu24回归证据及边界

本节和7.1以后记录的是各自标题/验证编号绑定的历史源码快照。它们证明对应版本在Ubuntu24用户空间中的结果，不会自动覆盖之后修改的当前工作树；当前源码最新平台状态见本页开头和[验证71.191](VALIDATION.md#71191-ubuntu24-当前源码-full-suite-与候选重建2026-10-22)。

实际环境为Ubuntu24.04.5用户空间、GCC13.3、Qt5.15.13及glibc2.39，运行身份uid1000；共用宿主的WSL2内核5.15。官方镜像签名和摘要、安装清单及实际版本分别留有记录，不用旧宿主版本冒充目标用户空间。

基础回归的完整回执位于源码目录下：

```text
build-ubuntu24-validation/rootfs/build/getdaq-platform/
  platform-dd5c0911f0eb4d2b9cf92d9cb7d50822.json
  delivery/full/build-receipt.json
  test-build-receipt.json
```

完整尝试状态为complete，绑定851项产品/测试/工具输入。配置迁移组实际执行12个进程中断及两个活写者场景；事务组验证共享读、独占写及明确恢复。12个接收传输场景合计线传输49739字节，自有raw已删，阶段残留0。默认原生数据工具的依赖检查继续拒绝Qt、Ice、yaml等进入热路径；本轮没有整体插桩系统Qt或新增持续吞吐证明。

### 7.1 首次失败及修复原因

首次核心失败使用单个65536字符XML名称，新版Qt在4KiB名称限制处就拒绝解析。该夹具本来要检查项目的聚合路径保护，因此改为40层、每层2048字节名称：必须先加载成功，然后修改被项目路径限制拒绝，最后核对源文件字节不变。实际Qt5.12.8、Windows Qt5.14.2及Ubuntu24 Qt5.15.13均验证了这一反例；没有关闭Qt限制或放宽产品保护。

第二次运行核心通过，但四项界面检查报Cannot inspect data filesystem。独立诊断发现chroot中statvfs成功，而QStorageInfo对根目录和/build无效；当时mountinfo只有proc、devpts及/code，没有可见的根挂载。在私有挂载命名空间内先将专用rootfs绑定挂载到自身，再进入chroot，Qt即可识别根文件系统，容量信息有效；完整界面随后98项全部通过。这里修复的是测试环境，没有将Sentry磁盘检查改成默认成功。

前后诊断及挂载信息保存在`build-ubuntu24-validation/storage-info-diagnostic/evidence/`，两次失败工件分别保存在`first-xml-name-limit-failure/`和`first-storage-info-failure/`。失败记录保留原样，不改成成功。曾发生的自动审批服务额度限制已经解除，本轮使用原授权入口实际执行，没有通过其他入口绕过限制。

### 7.2 历史实际服务组合与候选包（70.82）

同一完整构建中的ECC、两个数据管理服务及原生receiver已实际组合运行四个场景：normal-0、normal-1、start-ack-loss、truncated。硬件端是本机合成服务，数据直接传至原生receiver，Qt控制服务不转发raw。四项均返回0，测试目标启用前后25个产品程序的摘要保持一致；证据在`post-network/result.json`及`post-network/evidence/description-recovery-aoh1p_0d/`。该组删除14个自有raw，共245760字节，残留0；512MiB为最大测试预算，不是实际生成量或吞吐结果。这里没有执行完整GUI采集操作链。

Ubuntu24专用候选目录为：

```text
build-packages/getdaq-ubuntu24-20261020-current-source-candidate/
  bin/              25个主机程序
  docs/             中文操作、架构及专题说明
  share/            示例配置与许可证，不含现场认证或采集数据
  manifest.json     程序及交付文件摘要
```

这是使用Ubuntu24系统库的候选包，不能假定它能运行在Ubuntu20.04或任意其他Linux发行版。系统Qt、libc、libstdc++等仍由目标机提供，不能只复制一个GetDAQ文件就认定部署完成。项目专用旧依赖已使用本地源码构建；Windows仍使用原Qt5.14.2 MinGW候选，两种平台的二进制不可混用。

在源码根目录核对候选清单：

```bash
python3 tools/build-suite.py --verify-stage \
  build-packages/getdaq-ubuntu24-20261020-current-source-candidate
```

运行前在Ubuntu24机器上检查系统库，例如`ldd bin/GetDAQ`和`ldd bin/getdaq-receiver`，不能出现not found。随后按[操作手册](OPERATIONS.md)先验证模拟配置，再准备现场固定配置；勿直接用示例启动真实板卡。初始窗口检查只验证程序和Qt插件能启动，不证明全部人工操作或硬件通信。

包内验证使用独立`package-validation/result.json`记录清单、25程序运行库、七个help、初始窗口及实际配置CLI迁移/双向恢复；只在passed为true时计通过。结果和本节提到的`post-network/`均位于第7节的getdaq-platform目录。阶段只读复核入口为`python3 build-ubuntu24-validation/checkpoint.py`，必须先有完整封存的checkpoint.json；它核对来源及工件摘要，不重新运行服务，不把goal_complete改为true。

### 7.3 失败控制交接补齐查询CLI增量刷新（2026-10-20）

完整Ubuntu24候选原先没有70.94新增的`getdaq-supervisor-query build-handoff-completion`。70.95使用Ubuntu20.04.6/WSL2 GCC9.4按C++11重建查询CLI、supervisor和receiver，再使用Ubuntu24 rootfs glibc2.39加载器验证交付CLI的动态依赖和`--help`。在替换候选程序前，当前查询CLI参与本机localhost合成服务回归：原请求篡改和非规范请求拒绝、规范生成阶段没有POST、操作者显式单次提交、完成证明和接班者范围核对，以及冷重启后保持控制隔离均通过。测试最大raw预算64MiB，实际8192字节删除，剩余0。

交付工具`tools/refresh-ubuntu24-program.py`只接受已存在、路径无符号链接且清单完整的`full`候选；在临时副本替换所选程序、剥离交付副本调试信息、检查Ubuntu24 loader和`--help`，并将源/交付程序摘要与测试结果存入候选。只有完整清单再次通过后才替换候选。70.95仅重建查询CLI，其余24个沿用原全量构建；具体见[70.95记录](VALIDATION.md#7095-ubuntu24交付候选刷新失败交接查询cli2026-10-20)。

### 7.4 历史：认证 TCP 归档 CLI 增量刷新（70.97，已由普通 TCP 替代）

70.97阶段曾把认证 TCP 归档发送和监听命令纳入当时的完整候选`build-packages/getdaq-ubuntu24-20261020-current-source-candidate/`。Ubuntu20.04.6/WSL2、GCC9.4.0、C++11重建`getdaq-catalog`及两个相关测试目标；系统实验/HMAC向量和完整catalog传输测试通过。验证runner以Ubuntu24 glibc2.39动态加载器运行新CLI和测试进程，在localhost回环实测错密钥预先拒绝、第一份文件完成后中断、同一partial续传、目标独立读回以及已完成归档无载荷续传。TCP归档payload为24,576字节；本组全部catalog合成raw共生成并删除15,196,160字节，runner确认两处私有测试根目录为空、raw_remaining=0，预算上限64MiB。

当时的刷新合同要求`getdaq-catalog`提供HMAC向量和错密钥拒绝；这描述的是70.97阶段已被替代的认证TCP设计，不是当前协议。用户随后明确选择无认证普通TCP/IP，故71.20起刷新合同改为检查无认证直连、拒绝连接、中断续传、目标回读和完成归档零载荷续传；71.27又加入线程级SIGPIPE隔离检查。回归回执绑定源码构建程序SHA、回归程序SHA及剥离后的包内交付程序SHA；刷新器把manifest里程序路径规范到包内`bin/`，避免引用原构建机路径。打包器校验回执和catalog SHA。

增量替换后运行`tools/package_ubuntu24_catalog_ssh.py --verify`，25个程序全部经Ubuntu24 rootfs loader依赖检查，包内`getdaq-catalog --help`及新回归回执绑定检查通过；`tools/build-suite.py --verify-stage`通过。随后文档同步器刷新候选包文档，确认产品程序及运行依赖SHA不变，候选当前含175个manifest文件，其中新增71.04实际CLI错误凭据拒绝回执；本阶段只重建getdaq-catalog，其余24个程序复用，不能称为当前源码全量重编译。71.05再次以严格C++11当前构建刷新catalog，使其具有15秒非阻塞TCP connect总时限；包内收据`validation/program-refresh-getdaq-catalog.json`绑定新delivery SHA，更新后候选仍为175文件、其余24程序复用。验证在Ubuntu24用户空间加载器+WSL宿主Linux内核运行，不是独立Ubuntu24安装、两台机器、防火墙、macOS、真实DAQ或持续吞吐验收。


### 7.5 当前源码完整平台回归和Ubuntu24候选（71.09）

本轮以实际Ubuntu24.04.5用户空间、GCC13.3、Qt5.15.13、glibc2.39和uid1000从当前源码全量重建25个主机程序，再运行配置核心、GUI、native contracts、迁移/事务及receiver transports。第一轮运行各测试全通过，但平台守门器的GUI计数99已过期，故平台总回执标为failed；更新为实测101后完整重跑，最终状态complete。最终结果：核心147/147，Widgets 101/101，CTest 19/19；迁移、事务、TCP/ZBUF/FDT/ICE接收场景通过。两个runner attempt在共享的有界workspace累计wire字节99,478；所有测试raw均删除，最终`.graw`剩余0，未超过20GiB上限。

71.09阶段产物（后续已补入71.10至71.13验证证据）位于：

```text
build-packages/getdaq-ubuntu24-20261020-classinfo-candidate/
  bin/              当前源码构建的25个主机程序
  docs/             当前操作与架构手册
  share/            示例配置与许可证
  validation/       产品构建、测试构建、最终平台回执及测试摘要
  manifest.json     193项SHA-256清单（71.09阶段快照；当前候选已增至234项）
```

在源码根目录复核包清单：

```bash
python3 tools/build-suite.py --verify-stage \
  build-packages/getdaq-ubuntu24-20261020-classinfo-candidate
```

71.09构建/测试/平台回执在包内`validation/ubuntu24-build-receipt.json`、`ubuntu24-test-build-receipt.json`和`ubuntu24-platform-receipt.json`；核心及GUI完整QtTest输出、19项CTest日志、迁移/事务/接收传输摘要也由manifest绑定。71.09全量构建结果经71.10归档TCP、71.11四路接收、71.12只读源码runner接入和71.13锁文件安全回归证据补充；包内全量证据绑定了候选当前程序SHA。候选在专用Ubuntu24用户空间里构建，但仍共享WSL2宿主内核；不是独立Ubuntu24物理/虚拟机验收、真实硬件或持续高速采集证明。多主机传输、防火墙、实测运行时磁盘、macOS和现场操作仍待。
