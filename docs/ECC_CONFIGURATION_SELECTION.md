# 从仓库版本到 ECC 的选定配置

保持原实验/接收计划的服务实例更替，另见[旧ECC交接与独立接班进程](ECC_SERVICE_HANDOFF.md)。它不代替本页选定版本的实际加载，也尚不支持将任意不同实验参数直接切换到原数据计划。

## 1. 三种状态分别是什么意思

| 状态 | 软件实际知道什么 | 不能据此推断什么 |
| --- | --- | --- |
| 已登记 | 配置仓库完整检查并保存了发布包，存在登记收据 | 板卡已经采用此版本 |
| 已选定 | 审阅了某个登记版本，ECC 固定输入绑定选择文件、收据及完整包 | Configure 已执行成功或现场硬件已读回 |
| 实际加载已核对 | 必须有现场硬件/固件的匹配版本及必要读回证据 | 不能用上传成功、参数摘要或状态 Ready 替代 |

当前登记和选择记录均为 `hardware_loaded_verified:false`；ECC 父快照仍为 `actual_loaded_configuration_verified:false`。正常 Configure 的流程、参数写入/局部读回已有原生实现，但全局实际加载版本证明仍在重构中。因此上述 false 不是软件报错，也不应该手工改成 true。

选择操作只生成启动输入。不会向板端发送 Prepare/Configure/Start，不切换正在运行的 ECC，不启动或停止 receiver。Qt 只传配置和查看信息；MFM 仍由独立接收器直接接收。

## 2. 推荐的机器分工

配置仓库和 ECC 当前应在同一控制主机上，或能按同一绝对路径访问同一份只读包，且运行用户满足文件属主约束。选择文件记录的是服务器路径，单独把选择 JSON 拷贝到另一台电脑，不会把整个发布包搬过去。

Windows Qt 5.14.2 MinGW 操作电脑通过网络上传、检查和导出选择文件；ECC 选择/启动工具仍属于 POSIX 原生兼容组件。数据接收主机可独立部署。当前没有实现发布包的受控跨仓库搬移/路径重定位。

## 3. 在界面选择版本

1. 按[仓库手册](CONFIGURATION_REPOSITORY.md)连接配置仓库，核对 `repository_id`。
2. 上传完整发布包，或列出已经登记的版本。
3. 选择版本并点击 **Verify selected version**。索引本身不代表完整内容已经复核。
4. 完整查询通过后，点击 **Export reviewed ECC selection...**，保存到一个新的 JSON 文件。
5. 将这份选择文件送到 ECC 控制主机的受限配置目录。它不包含认证令牌，但包含部署路径和配置摘要。

更换地址、端口、凭据路径、仓库身份会丢弃此前客户端；再次完整查询后才导出。文件保存要求新名称，已有文件拒绝覆盖。连接或校验失败不能生成有效新选择。

导出后硬件保持原状。不要把“选择文件已保存”解释为主窗口已经连接了新 ECC，或板卡已经加载。

## 4. 用命令行导出同样的选择记录

```bash
build-config-transfer/getdaq-config-upload \
  --host 127.0.0.1 --port 24002 \
  --credential /absolute/private/repository.token \
  --repository-id REVIEWED_REPOSITORY_ID \
  selection PUBLICATION_SHA256 /absolute/private/selection.json
```

将两个大写占位符替换成已核对的仓库身份与版本摘要；不是令牌值。程序先核对仓库身份，再完整查询该版本，然后写新的只读选择文件。Linux文件权限设为0400；Windows实际保护仍需ACL及Kit验证。

文件中的主要字段：

| 字段 | 含义 |
| --- | --- |
| `repository_id` | 原仓库稳定身份 |
| `publication_sha256` | 原 publication.json 的字节摘要 |
| `registration_sha256` | HTTP 返回的原登记收据的完整字节摘要 |
| `manifest_file` | 服务器上的绝对清单路径 |
| `configuration_sha256` | 已编译完整参数摘要 |
| `artifact_sha256` | experiment.json 字节摘要 |
| `effective_sha256` / `snapshot_sha256` | 有效参数树及完整来源记录的摘要 |
| `hardware_loaded_verified` | 固定为false，不表示板端加载 |

收据摘要绑定原字节。自行排版或改写服务器收据，即使语义看似相同，也不再匹配原选择；不要重新计算摘要来掩盖未经审阅的替换。

无需Qt/Ice的`getdaq-config-check selection SELECTION.json`或`registration REGISTRATION.json`可以离线检查记录格式。它返回`valid_record:true`和`publication_checked:false`：只说明记录字段有效，没有据此检查服务器完整包。完整包和部署绑定须继续执行第5节工具。

## 5. 建立新的 ECC 启动输入

本工具建立独立部署配置，避免覆盖正在使用的文件。先准备一份已有格式的 ECC 服务模板：它包含板端代理、拓扑、硬件描述、sender/数据节点目标、自有告警策略、认证文件路径和审计根。详见[ECC 服务手册](ECC_NETWORK_SERVICE.md)。模板必须能通过原有离线检查，不能只用 `.xcfg` 或仓库收据代替。

原生兼容构建中增加 `getdaq-ecc-select`：

```bash
cmake -S third_party/ice-compat -B build-ice-compat
cmake --build build-ice-compat --target getdaq-ecc-select getdaq-ecc-server --parallel 2
```

它复用本地原生 ECC/GET/Ice 兼容组件，运行时执行离线构造和完整绑定检查；不是只有标准库的程序。Qt GUI/上传CLI无需链接这些硬件组件。

在ECC主机上：

```bash
build-ice-compat/getdaq-ecc-select \
  /absolute/private/service-template.json \
  /absolute/private/selection.json \
  /absolute/private/new-selected-deployment
```

最后一个目录必须不存在。失败保留意图与未完成文件，不覆盖旧目录；不要把 `.pending-service.json` 当完成结果。有效完成结果为 `new-selected-deployment/service.json`，并返回固定输入摘要及选择记录。它是配置生成结果，不是后台服务已经运行。

工具保留模板的`audit_root`，只创建新的部署配置目录。已有审计根的绑定与新选择不同，ECC启动会拒绝；不能通过本工具换根清除旧UNKNOWN或启动核对门槛。已经运行过的系统还需受控停机/恢复和部署迁移流程，当前没有自动完成这些步骤。测试中的新审计根是明确的合成首次部署，不是生产恢复证明。

启动前再检查：

```bash
build-ice-compat/getdaq-ecc-server \
  --config /absolute/private/new-selected-deployment/service.json --check-config
```

离线检查不创建 ECC/receiver 审计根、不连接硬件。完整运行和原始数据的状态约束继续由已有 ECC 服务负责；本工具不代替实际控制权、停机核对、未知操作恢复或旧运行的封存。

## 6. ECC 如何检查选择

生成的服务配置增加 `configuration_selection` 文件引用，同时将 `configuration_publication` 与 `experiment_artifact` 指向同一已登记包。所有引用都带字节 SHA-256。ECC 依次执行：

1. 严格读取固定服务配置及选择记录，不接受重复/多余字段或错误类型。
2. 按选择记录核对同一只读包、工件和原登记收据。
3. 检查仓库身份、收据字节摘要、清单路径/摘要、参数/工件/有效快照摘要一致。
4. 重验完整发布包的文件、来源与结构化快照语义。
5. 按模板中的实体 ID/活动/主从/顺序核对拓扑、硬件描述、sender 与接收计划。
6. 持续保留输入文件/目录的身份及字节绑定，后续命令前继续检查；损坏恢复不解除同一对象故障。

选择文件和登记收据只读，目录及属主遵循固定输入规则。父ECC快照另含选择文件摘要、仓库身份和登记收据摘要，避免仅看到“参数相同”就把不同来源当成同一次部署。没有配置选择字段的旧服务配置保持兼容。

共享纯C++11协议位于 `common/experiment/configurationselection.*`；仓库、Qt导出及固定ECC共用。部署生成位于 `apps/ecc/native/eccconfigurationselector.*`，独立CLI入口为 `apps/ecc/select/main.cpp`。没有新增第三方库或原始数据依赖。

## 7. 查看 ECC 接受的配置

认证的 GET `/v1/configuration` 返回启动时接受的缓存：实例/父快照/固定输入摘要、参数与工件摘要、可选选择记录。未启用新字段时 `selection:null`。

它与 GET `/v1/identity` 的实例和父快照应一致。`input_binding_live_verified:false` 表示这次缓存查询没有重新读取所有文件；`hardware_loaded_verified:false` 表示没有全局硬件加载证明。缓存能说明该进程启动时接受哪份配置，不能单独作为当前输入仍健康或现场板端状态证据。原有命令入口仍执行持续输入检查。

此接口不返回原始数据或认证值，也不接受配置切换 POST。运行期间替换固定配置会触发故障保持；后续受控切换必须继续处理控制权、硬件和原数据会话的边界。

主窗口已经接入只读配置表格，另可显式启用Start前Run绑定和恢复核对；操作步骤和缓存边界见[配置查看与Run记录](ECC_CONFIGURATION_VIEW.md)。

## 8. 当前验证范围

使用纯协议测试和本机模拟绑定夹具，核对严格字段/false范围、收据字节、仓库/清单/工件一致性、离线部署、持续只读约束和故障保持。真实跨进程联调另使用仓库服务、Qt上传/选择导出、原生离线选择工具和ECC配置缓存HTTP；发布包来自合成测试，不执行板端命令、receiver或MFM。

这不是现场加载/控制权证明，也不是物理多主机、Ubuntu24/Windows/macOS、真实断电或DAQ吞吐验收。最终记录与尚未完成事项以[验证文档](VALIDATION.md)为准。


## 配置切换的控制权接续

数据服务原控制范围/持有期的明确交接已由[数据控制权交接](DATA_CONTROL_HANDOFF.md)实现；它保持数据计划不变。不同实验参数/接收计划的更替使用[ECC计划切换流程](ECC_PLAN_TRANSITION.md)及其[Qt审阅窗口](ECC_PLAN_TRANSITION_GUI.md)，包含新ECC计划激活与新数据服务换计划，但需要明确停旧服务、启动新服务并人工开始新周期，不支持运行中热加载，也不证明板端实际加载版本。连续板卡主机占用移交及真实硬件读回仍待验证。不要直接修改运行服务的固定配置，也不要用新审计根绕过旧故障。

下一实例可以先通过[离线快照规划](ECC_SUCCESSOR_PLANNING.md)取得确定的父快照和数据控制范围，不必为了查询身份先启动服务或抢占原主机占用。该步骤不生成运行实例/持有期，不替代明确交接或计划参数切换。
