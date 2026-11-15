# 离线规划下一代 ECC 实例

后续接班身份保存与一次性启动接线见[身份说明](ECC_SUCCESSOR_IDENTITY.md)。本页的 `--plan-config` 仍只做确定性规划，不生成持有期；新命令和真实接班核对的验证范围另行记录。

## 1. 这一步解决什么问题

切换配置时，新旧 ECC 实例需要不同的配置身份和数据控制范围。此前必须构造新的运行协调器才能取得这些值，这会创建审计根、尝试取得板卡主机占用；旧实例还在正常工作时，这种做法会发生冲突。

现在可以先执行 `getdaq-ecc-server --plan-config`。它使用与真实启动相同的参数编译代码，计算下一实例的快照和数据控制范围，输出小型 JSON，不创建运行审计根、不取得主机板卡占用、不监听控制端口，也不发送硬件或接收器命令。

**规划是准备材料，不是切换。** 它不停止旧实例，不接管旧接收会话，不生成新适配器的持有期，也不证明硬件已经加载配置。完整配置切换仍须核对原实例状态、数据计划和控制权，再执行明确交接与激活。

## 2. 准备哪些文件

使用已经生成的固定 ECC 配置，例如配置选择工具输出的 `service.json`。它引用的实验工件、拓扑、硬件描述和可选发布记录必须完整存在，摘要正确。认证文件仍需是本机可读的私有文件；规划不会向服务发送其中的认证值，也不会把值打印到结果里。

应先把部署输入放到最终目录再规划。固定输入绑定包括文件路径、内容摘要及引用；复制到其他路径、修改文件字节或改变引用后，应重新计算，不能继续使用旧规划。

审计根和主机板卡占用目录在这个步骤可以尚未建立，也可以已有其他实例持有占用。规划只计算身份，不把这种情况当作授权。真实启动时仍会检查运行环境、目录和占用。

## 3. 怎样执行

在 ECC 主机上使用已构建的程序：

```bash
bin/getdaq-ecc-server \
  --config /home/daq/getdaq-input/next/service.json \
  --plan-config
```

需要保存规划结果时，可以由 shell 重定向到自己的审阅文件：

```bash
bin/getdaq-ecc-server \
  --config /home/daq/getdaq-input/next/service.json \
  --plan-config > /home/daq/review/next-ecc-plan.json
```

输出目录必须事先准备。shell 保存的是规划 JSON，不是运行审计或已执行配置。命令成功返回 0；非法路径、摘要不符、参数冲突或重复发送器等情况返回非零，诊断写入标准错误。

`--check-config` 仍保留原行为，适合只检查固定输入；`--plan-config` 在此基础上编译完整父快照与数据控制范围。两种模式不能同时指定，也不能与 `--generate-token` 混用。正常启动仍是不加这两种离线选项：

```bash
bin/getdaq-ecc-server --config /home/daq/getdaq-input/next/service.json
```

最后这条会真正监听服务、创建/打开运行审计并检查占用，必须在完整部署和交接条件满足后使用。

## 4. 输出怎样看

| 字段 | 含义 |
| --- | --- |
| `schema` / `kind` | 版本 1，类型 `getdaq.native-ecc-plan` |
| `service_id` | 固定配置中的服务名称，不是新进程的随机实例 ID |
| `input_binding_sha256` | 这组固定输入的文件/引用绑定 |
| `snapshot_sha256` | 与真实启动共用编译过程得到的父 ECC 快照摘要 |
| `data_lifecycle_binding_sha256` | 配置的数据服务节点、固定计划及端点等输入绑定 |
| `data_controller_scope_sha256` | 父快照和审计根路径派生的数据控制范围；不是父快照摘要本身 |
| `configuration_sha256` / `artifact_sha256` | 编译的实验配置与工件摘要，不是板端读回版本 |
| `audit_root` | 规划使用的运行审计路径，不表示该目录已经创建 |
| `audit_roots_created` | false，没有建立运行审计根 |
| `host_reservations_acquired` | false，没有取得或移交板卡主机占用 |
| `hardware_commands_sent` | false，没有发硬件命令 |
| `receiver_started` | false，没有启动接收进程 |

重复使用完全相同的输入与路径，应得到相同规划。尚未启动的新进程实例 ID、客户端持有期及新接收会话不属于这些确定性配置摘要。

## 5. 与数据控制权交接的关系

[数据控制权交接](DATA_CONTROL_HANDOFF.md)要求明确指定接班者的数据控制范围与持有期。这里提前解决范围和快照的计算，避免为取得范围而抢占原实例的资源。

规划不替代交接回执。新适配器仍需自己的明确持有期、原控制者提交的交接、所有数据节点的完整完成结果，以及新对象对原回执/原实例/正常封存周期的核对。只把规划摘要填进 GUI 或请求文件，不能让新对象绕过这些门槛。

接收计划参数发生改变时，还要处理数据服务计划的受控切换；本命令不改变原节点计划。板卡主机占用的连续交接、ECC 服务代次持久激活和完整 GUI 切换入口仍未接完。

## 6. 软件原理与接口

原生库新增纯输入接口：

```cpp
NativeEccPlan PlanNativeEccConfiguration(
    const NativeEccOptions &options,
    const std::string &dataLifecycleBindingSha256);
```

返回值包含 `snapshotJson`、`snapshotSha256` 和 `dataControllerScopeSha256`。它与 `NativeEccCoordinator` 真实构造共用同一编译函数：校验完整工件、绑定并规范化系统参数、对齐描述/发送器集合，然后生成原有规范快照。

真实构造随后才检查主机占用、打开审计目录、保存输入和绑定数据适配器；离线入口在这些动作之前返回。没有新增一套简化的快照算法，也没有添加新的可执行程序或运行库依赖。

数据控制范围的定义仍为：

```text
SHA256("getdaq.data-controller:1\n" + snapshot_sha256 + "\n" + audit_root)
```

CLI 只输出摘要和少量说明字段，原始事件数据不经过这个入口。Qt 只用于现有服务程序的命令行外壳；原生规划接口使用标准 C++11 类型。

## 7. 本机验证与限制

普通构建已覆盖两种编码各七项：与先前实际启动保存的快照比对、独立重算固定输入文件清单摘要与数据控制范围、重复计算一致、缺失新审计/占用目录、已被持有的占用、非法相对路径、重复发送器和冲突命令模式。重复计算归于独立场景，快照/输入/范围检查归于同一正向场景。

测试给程序加载了拒绝并记录 `connect` / `bind` / `listen` / `sendto` / `sendmsg` 的专用拦截库，未记录网络尝试。拦截库仅供测试，不加入交付程序。全部只处理配置元数据，没有测试载荷或 `.graw` 文件。

这证明离线规划与原有启动快照的规范一致性和本轮观察到的无网络/运行根创建行为，不代替新版本联网启动、硬件配置、控制权交接、真实板卡或目标平台验收。自动审批额度限制期间没有执行需要该审批的补充联网测试，也没有用离线结果冒充联网结果。

结果和进一步验证范围见[验证记录](VALIDATION.md)，当前未完成项见[重构进度](REFACTOR_STATUS.md)。


## 8. 当前结果入口

当前普通构建的两组记录为：

- `build-ecc-successor-planning/ordinary-encoding0-evidence/ecc-successor-plan-qcmmdv6t`。
- `build-ecc-successor-planning/ordinary-encoding1-evidence/ecc-successor-plan-tndjxyjh`。

新代码ASan/UBSan的两组记录为：

- `build-ecc-successor-planning/sanitized-encoding0-evidence/ecc-successor-plan-w3kdk6rr`。
- `build-ecc-successor-planning/sanitized-encoding1-evidence/ecc-successor-plan-a53bts1i`。

四组各七项，共28个离线场景通过。插桩版本20条严格C++11编译，62个对象按实际仓库依赖核对复用，374项依赖记录；旧GET/Ice和系统Qt未整体插桩。离线运行启用ASan/UBSan，泄漏检查未启用，不将其写成LSan通过。两个改动的C++编译单元另以严格C++11做了语法检查。

Linux全套23程序已重建。当前Linux候选包为 `build-packages/getdaq-host-20261015-ecc-successor-planning-candidate`，仅保存与上述普通测试相同字节的程序及更新文档；联网启动回归未执行。Windows操作台代码本轮未改，仍可使用上一阶段候选包；全套来源收据尚未刷新到本轮后端快照，不冒充Windows本轮重新验收。

阶段证据索引为 `build-ecc-successor-planning/checkpoint.json`。本机是Ubuntu20.04.6/WSL、GCC9.4、Qt5.12.8，不代表Ubuntu24/macOS或真实板卡验收。整套重构仍未完成。
