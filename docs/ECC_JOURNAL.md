# ECC 持久化日志、启动核对与只读查看

## 1. 为什么还需要这一份日志

电子学控制命令可能执行很久，也可能在改变了部分硬件后才报错。仅把状态保存在内存里，程序退出后就会丢失“正在执行什么、是否收到完成应答”的信息；重新创建一个 Idle 对象不能代表板卡真的回到了 Idle。

现在增加 `EccJournal`，可选接入 `EccStateMachine`。它在执行前记录操作意图，后端完整成功后才记录完成。两条记录都需要落盘成功，不能把“发出命令”当成状态转换完成。重启时从文件恢复历史，并要求重新核对硬件。

这份日志记录控制操作，不记录原始事件，不读取 `.graw`，不进入数据接收热路径。它与下面两份记录各有职责：

| 记录 | 证明的内容 | 不能证明的内容 |
| --- | --- | --- |
| ECC 状态日志 | 哪个操作 ID 开始/完成/未知，最后确认的状态及恢复证据 | 板卡在进程重启后没有变化、全部实验参数正确 |
| HardwareClient 操作日志 | 寄存器/设备调用的顺序、应答及明确的读回 | 整个多板 ECC 流程一定完整成功 |
| receiver 封存凭据 | 接收器正常退出时的数据文件清单、字节数和持久化标志 | 电子学准备正确、物理事件绝对零丢失 |

完整生产 ECC 网络服务和具体硬件恢复检查仍在迁移。当前新增的是持久化组件、状态核心接入和只读查看工具，GUI 继续连接旧 ECC。不能用本文的测试后端替代真实 Prepare/Configure。

## 2. 状态字段怎么理解

| 字段 | 含义 |
| --- | --- |
| `state` | 最近一次完整确认的状态，编号见 [ECC 状态机](ECC_STATE_MACHINE.md) |
| `transition` | 当前内存里正在执行的操作；普通命令 1～9，恢复核对为本地扩展 10，空闲为 0 |
| `error` | 最近出错的操作；普通命令 1～9，恢复核对为本地扩展 10 |
| `outcome_unknown` | 存在可能部分执行、尚无确认完成的硬件动作 |
| `needs_reconciliation` | 重启后尚未核对硬件，或日志持久化不可用 |
| `operation_id` | 最近登记/确认的操作 ID，用来查询对应历史 |
| `message` | 错误或解释文本；不是自动恢复指令 |

例如日志最后有 Start 完成，历史 state=5。重新打开日志时 `outcome_unknown=false`，但 `needs_reconciliation=true`：Start 曾经确认完成，**现在的板卡是否仍运行尚未检查**。

如果 Pause 写了部分硬件后响应丢失，state 仍是 5，`outcome_unknown=true`。它不表示板卡肯定继续运行；可能已经暂停，只是主机没有确认。不能盲目重发 Pause，也不能为了显示 Paused 改掉日志。

只读工具从历史推导这些字段，不是板卡实时状态探测。运行中的控制器应使用 `EccStateMachine::Status()`；`EccJournal::Load()` 用于初始化和历史核对，有未确认意图时会保守地报告 unknown。新字段和本地恢复编号尚未接入旧 SOAP 协议。

## 3. 执行与落盘的顺序

持久化模式要求调用 `Execute(command, operationId, requestKey)`，不允许只传 command：

1. 检查状态约束、没有其他转换在进行、没有 unknown 或待核对标志。
2. 检查操作 ID 是否全新、文本是否有效；检查后端完整流程和配置。
3. 追加 `intent`，写入完整一行并 `fsync`。这一步失败，不调用硬件 Apply。
4. 执行后端 Apply；后端继续使用独立硬件日志记录具体操作。
5. 后端正常返回后追加 `complete` 并 `fsync`。
6. 最后才把内存 state 改为目标状态。

Apply 报错时追加 `unknown`；原 state 保留，后续普通命令被阻止。Complete 落盘失败时，即使硬件已经返回成功，也保留最后确认的旧 state 并阻止继续控制。不会再次调用 Apply 来“补一次”。

以下中断情况已经用真实子进程测试：

| 中断点 | 重启看到的历史 | 是否自动再发命令 |
| --- | --- | --- |
| intent 已落盘，尚未执行硬件 | 有 intent，没有 complete；按 unknown 处理 | 否，因为重启者无法仅凭文件确定中断精确发生在调用前 |
| 硬件动作之后，complete 之前 | 有 intent，没有 complete；按 unknown 处理 | 否 |
| complete 已落盘，尚未正常退出 | 能恢复已确认目标 state，仍需核对当前硬件 | 否 |
| intent 写到一半失败 | 文件残尾保留，打开时拒绝；本次硬件未被调用 | 否 |

这些测试验证正常进程终止、强制终止和文件写错误的控制顺序，不替代整机掉电、存储控制器缓存或实际文件系统的验收。

## 4. 操作 ID、request_key 和 scope

操作 ID 合法字符为 ASCII 字母、数字、下划线和连字符，长度 1～128。推荐使用一次请求生成的 UUID。同一个 ID 一旦登记，无论后来 complete、unknown 还是恢复，都不能再次用于新动作。应查询它的历史结果。

预检查未通过时尚未登记 intent，因此修正配置后可以继续使用该请求 ID。intent 已登记或执行结果未知时，换一个 ID 也不能绕过状态约束；unknown 标志仍会阻止执行。

`request_key` 是调用者提供的不可变请求依据，例如已发布配置的内容哈希、硬件描述版本和 Run 标识。日志把原文本保存下来，**不会自己计算哈希、发布配置或验证文件内容**。未来完整后端需要把它关联到实际加载的不可变配置，不能只填一个会被覆盖的文件名。

`scope` 在创建日志时写入 header，用于标识目标设备集合和部署版本。每次打开都必须传入完全相同的文本，大小写、空格和 UTF-8 字节均需相同。不匹配时拒绝且不追加 opened 记录。

scope 的真实目标身份需要由后端从设备地址/身份、部署配置等生成；库不会根据这段文本自动连接板卡核实身份。不能把同一个简短标签随意用于不同设备。

scope 应是这套设备和基础部署的稳定身份；每次实验的物理参数版本放入 request_key。不要每次 Configure 都换一个 scope 或新建空日志，丢掉已有状态和操作 ID。

scope、request_key 和恢复 evidence 均为 1～4096 字节有效 UTF-8，不能含 NUL。非法输入在硬件执行前拒绝。错误信息超过限制会截短；非 UTF-8 错误字节以有界十六进制文本保存，避免错误信息本身破坏 unknown 记录。详细硬件日志应同时保留。

## 5. 文件格式和每种记录

日志是UTF-8 JSON Lines，一行一个对象，文件用LF换行。seq用十进制字符串保存，header为0，其余逐条加1；schema_version/state/command/from/to使用真正JSON整数。读取现复用标准C++11 StrictJson，拒绝重复/未知键、注释、非法UTF-8、数字/字符串/布尔类型混淆和YAML。既有程序产生的规范日志字节保持，损坏或非规范日志拒绝后仍不改原件；不自动修复未知schema或残尾。

| phase | 关键内容 | 用途 |
| --- | --- | --- |
| `header` | schema_version=1、kind、scope、初始 state=1 | 标识格式与目标范围 |
| `opened` | 当前最后确认的 state | 已有日志被新的写入会话打开，要求启动核对 |
| `intent` | id、command、from、to、request_key | 调用硬件前的持久化意图 |
| `complete` | id | 后端完整成功后的确认 |
| `unknown` | id、message | 执行结果不确定；不改成目标 state |
| `reconciled` | 新 id、from、to、evidence | 后端核对通过后的恢复记录 |

恢复只增加新记录，原 unknown 或 intent 保持原样，不伪造原操作成功。查询原操作时仍会看到它当时没有完整确认。

写入者独占文件锁；离线查看者使用共享只读锁。已有写入者持锁时，查看工具会明确拒绝。该锁保护同一个日志文件，不是跨不同日志目录/不同控制软件的全局板卡锁。

一个日志对象由一个状态机负责控制；后端和日志的生命周期必须长于状态机，执行线程结束后才销毁相关对象。状态机内部的并发拒绝不能替代不同控制软件之间的协调。

每次追加前后检查文件身份和长度；文件被替换、删除或长度改变时停止控制。它不能提供密码学防篡改，也不能证明同长度编辑未发生。不要在控制进程运行时手工编辑、删除或移动它的日志。

## 6. 启动恢复为什么不能自动清除错误

已有日志每次被写入者重新打开，都会进入待核对状态，包括最后确认的状态为 Idle。即使退出时很干净，期间也可能有板卡重启或其他控制端操作。

恢复入口是 `Reconcile(newOperationId, verifiedState, evidence)`：后端必须实现 `VerifyRecovery`，实际核对允许恢复的状态、原操作与现场观察。基类默认实现直接拒绝，防止仅凭说明文字清掉 unknown。当前 `NativeEccCoordinator` 为若干绑定同一存活实例的 Describe、Prepare、采集故障路径提供了专用只读核对证明；该能力不等于日志组件本身可以恢复，也不允许通用状态跳转或冷启动接管，详见[原生ECC协调器](NATIVE_ECC_COORDINATOR.md#8-失败重启与显式停生产)及各故障恢复手册。

核对函数必须没有硬件写操作。需要修复、重新准备、停止或单板维护时，应由对应独立、带日志的流程执行，再核对结果。不能把恢复入口当作直接跳转状态的接口。

VerifyRecovery 失败时，原 unknown/待核对标志保留。核对通过但恢复记录落盘失败，也不能恢复普通控制。没有通用的“清除错误”命令，查看工具也没有修改状态功能。

当前仍没有通用的生产硬件 `VerifyRecovery`、冷启动接管或任意故障恢复；已实现的原生路径只覆盖特定原对象的停止、清理和只读核对条件。新核心在 unknown 状态下拒绝普通 Stop/Reset，专用失败停产流程也必须匹配对应的保留对象和操作ID。实际实验仍需按现场部署验收，不能用测试后端替代现场控制。

## 7. 构建方法

Linux 根目录：

```sh
bash tools/build-linux.sh ecc
```

或独立 CMake：

```sh
cmake -S apps/ecc -B build-ecc -DCMAKE_BUILD_TYPE=Release
cmake --build build-ecc --parallel 4
cd build-ecc
ctest --output-on-failure
```

Linux/macOS配置得到状态静态库、日志静态库、getdaq-ecc-journal和测试程序。日志读取共享标准C++11 StrictJson，不编译或链接yaml-cpp，不需要另外安装JSON库。状态核心仍只依赖C++运行库和线程；POSIX日志写入使用本机文件系统操作。

只需要状态核心、不要 POSIX 日志组件时：

```sh
cmake -S apps/ecc -B build-ecc-core -DGETDAQ_ECC_JOURNAL=OFF
cmake --build build-ecc-core --parallel 4
```

可选 Ice 工程也会构建POSIX日志工具，输出在 `build-ice-compat/`。Windows Qt GUI 从 `GetDAQ.pro` 构建；Windows另有使用原生Win32文件API的扫描执行journal存储，下面单独说明。POSIX日志目前在Linux验证，macOS文件系统行为仍待验。

### 7.1 Windows Qt/MinGW 扫描journal

Windows配置工具使用原生扫描journal后端，不把Qt放进持久化或DAQ数据路径。它只保存最多1 MiB的控制JSON元数据，不接收、缓存或转发事件数据。构建需要CMake和用户安装的Qt配套MinGW工具链；CMake本身不需要Qt：

```powershell
cmake -S apps/ecc -B build-platform/windows-scan-journal -G 'MinGW Makefiles' `
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DGETDAQ_ECC_JOURNAL=ON `
  -DCMAKE_CXX_COMPILER='D:/Software/QT/Tools/mingw730_64/bin/g++.exe' `
  -DCMAKE_MAKE_PROGRAM='D:/Software/QT/Tools/mingw730_64/bin/mingw32-make.exe'
cmake --build build-platform/windows-scan-journal --parallel 2 `
  --target scan-journal-store-tests getdaq-scan-journal-tool
Push-Location build-platform/windows-scan-journal
ctest --output-on-failure -R '^scan-journal-store$'
$testExitCode = $LASTEXITCODE
Pop-Location
if ($testExitCode -ne 0) { exit $testExitCode }
```

请按实际安装位置修改编译器路径。`getdaq-scan-journal inspect FILE`输出已有journal的状态摘要；打开时会取得独占锁，因此不能与写入者并行查看。此工具不创建或修改journal。

Windows journal必须放在本地盘目录中，该目录及其中journal/lock文件的owner必须是当前用户，DACL中的允许访问ACE只能授予当前用户；拒绝ACE可以保留。UNC网络路径、设备路径、reparse point、硬链接journal、非规范文件和宽松ACL均拒绝。程序不会自动更改目录ACL。应使用专用私有目录，不要放入多人共享目录或网络盘。每次更新写同目录临时文件、刷新后用Win32原子替换，并校验锁、目录和journal身份。掉电语义仍须在部署文件系统上验收。

Windows专用测试和CI构建步骤已加入，但本地WSL启动Windows进程曾在qmake探测时报`UtilBindVsockAnyPort: socket failed 1`。Windows CI实际通过前，不能把该后端标记为已编译验收。

## 8. 只读查看命令

```sh
build-ecc/getdaq-ecc-journal inspect '/path/to/ecc-state.jsonl' '准确的scope'
build-ecc/getdaq-ecc-journal inspect '/path/to/ecc-state.jsonl' '准确的scope' \
  --operation '一次请求的操作ID'
```

第一条输出最后确认状态、unknown/待核对标志和操作历史；第二条只输出指定 ID 的记录。工具不连接板卡、不创建不存在的日志、不追加 opened、不截断文件。文件损坏、锁冲突、scope 不符或 ID 不存在时退出码为 1，参数错误为 2。

Linux/macOS POSIX扫描执行journal可通过同一工具查看：

```sh
build-ecc/getdaq-ecc-journal inspect-scan '/absolute/private/path/scan-execution.json'
```

输出是`getdaq.scan-execution-status` JSON摘要，含execution ID、源计划与快照摘要、当前点/状态、当前点运行期身份观察摘要、事件数及pending operation ID/是否未知。journal schema v3的新扫描点在`start-ecc`完成后，必须先持久化调用方验证过的ECC运行身份摘要，才能写入`describe`及后续操作intent；推进下一点后必须重新绑定。Qt-free `ObserveScanRuntimeIdentity`复用ECC `/identity`、`/status`严格解码器并核对调用方冻结的逐点期望、Idle控制器、data controller身份及fresh receiver计划，然后生成稳定摘要。journal本身仍只验证摘要格式和事件顺序；该协议观察不认证HTTP服务或证明板端硬件读回，调用方期望的安全来源仍由未来逐点部署适配器负责。schema v1/v2仍按旧格式兼容读取。查看工具不显示执行请求正文，不连接ECC或硬件，也不推进journal状态。为避免读取时与写入者竞争，命令会取得扫描journal的独占非阻塞本机锁；扫描服务持有journal时命令会拒绝。POSIX路径必须绝对且规范，并位于当前用户权限为0700的目录中。首次检查缺少锁文件的journal时，会安全创建0600的`.lock`边车文件；只读介质或权限不足时无法检查。

Windows Qt/MinGW版本使用独立程序：

```powershell
build-platform/windows-scan-journal/getdaq-scan-journal.exe inspect 'C:\Users\me\AppData\Local\GetDAQ\scan\scan.json'
```

该命令输出相同摘要并持有独占锁。Windows使用当前用户私有ACL；macOS仍使用POSIX后端，但原生macOS回归尚未执行。

Envoy的“Scan plan and Run control”窗口会在完整校验计划后启用“Confirm plan reviewed and save journal”。只有操作员在确认框选择Yes后，才会把计划从`Planned`推进为`Reviewed`。计划SHA和点数在弹框之后重新校验；若计划文件已变化，拒绝记录。review journal位于`QStandardPaths::AppLocalDataLocation/scan-journals/`，POSIX要求专用0700目录，Windows新目录以当前用户专属ACL创建。文件名由计划摘要派生，所以同一份计划重复确认会回报已审阅，不会创建重复审阅记录。

该按钮只证明本机用户确认了应用中的review提示，不保存操作者身份、外部证据或签名，也不代表硬件读取了配置。它不发送ECC、Configure、Run/Stop或receiver命令；journal仍需后续完整执行器接入。跨设备复制计划时，各主机的本地review记录互不共享。

下面可以在没有硬件时看完整样例；它使用测试后端，状态和错误都是模拟的：

```sh
getdaq_journal_demo="$(mktemp -d /tmp/getdaq-journal-demo-XXXXXX)"
build-ecc/ecc-journal-tests --fixture "$getdaq_journal_demo/ecc-state.jsonl"
build-ecc/getdaq-ecc-journal inspect "$getdaq_journal_demo/ecc-state.jsonl" \
  'test-CoBo-0-configuration-v1'
build-ecc/getdaq-ecc-journal inspect "$getdaq_journal_demo/ecc-state.jsonl" \
  'test-CoBo-0-configuration-v1' --operation pause-1
```

样例的 Describe、Prepare、Configure、Start 完成，Pause 执行后模拟响应丢失。因此最后确认 state 为 5，error 为 6，unknown 和待核对均为 true；pause-1 的历史 phase 为 unknown。不能据此认定某块真实板卡正在运行。

## 9. 限制和故障处理

单个日志最大 64 MiB、200000 条记录、100000 个操作 ID，单行最大 64 KiB。达到限制会拒绝继续记录，不能自动删除历史或换一份空日志来恢复控制。完整生产服务还需要受控归档/轮转和跨归档的操作 ID 查询。

空的已有文件、截断残尾、非法转换、重复 ID/字段、序号不连续和未来 schema 都拒绝打开。保留原文件与硬件操作日志，核对最后执行的动作。当前没有自动修复工具，不要通过删掉残尾或把状态改成 Ready 来解除阻止。

持久化依赖本机文件系统：POSIX使用`flock`/`fsync`/原子rename，Windows使用Win32独占句柄、`FlushFileBuffers`和`MoveFileExW`。不保证网络文件系统、硬件断电或磁盘控制器缓存的全部行为；Windows原生测试、macOS及实机目标平台验收仍在[验证记录](VALIDATION.md)中列为待完成。

原始数据的结束仍独立处理：控制恢复到 Ready，不证明 receiver 已收到 EOF 或已封存数据，继续按 [数据服务手册](DATA_SERVICE.md) 检查接收器和归档条件。

原生`NativeEccCoordinator`将此日志连接到硬件流程；固定部署/接收计划快照与原工件绑定操作ID和子审计。Breakup须等待本机或各远程节点receiver的EOF、排空、退出和封存后才提交Prepared，重开仍保持核对门槛。[远程服务](REMOTE_RECEIVER_CONTROL.md)、[ECC多节点适配](REMOTE_ECC_DATA_LIFECYCLE.md)和固定HTTP网络入口已实现，并有本机真实进程回归。真实硬件、独立多主机、冷启动/未知结果恢复仍有限制；详见[原生ECC协调器](NATIVE_ECC_COORDINATOR.md)及[本机管理](RECEIVER_PROCESS_MANAGEMENT.md)。

原生后端现实现限定于同一活跃实例原 Describe 失败的 VerifyRecovery；内部绑定原日志与新只读观察凭据后，才可写 reconciled 并提交 Described。原失败记录不改，普通说明字符串仍被拒绝。它不等于完整未知硬件恢复；详见[恢复手册](DESCRIPTION_RECONCILIATION.md)。
