# 从 GET 配置到板端硬件描述

更新：2026-10-21。本流程实现离线编译、原生读取、空节点创建、元数据核对和本地权限策略绑定。SystemDescriptionClient已接入NativeEccCoordinator的生产Describe命令及固定HTTP入口，见[全系统描述](SYSTEM_DESCRIPTION.md)。现场硬件读回、全局控制权与冷启动恢复仍待验收。

## 1. 四种文件和操作的区别

| 名称 | 作用 |
| --- | --- |
| 原始 `hardwareDescription_*.xcfg` | GET 的设备、寄存器、位域配置；可能使用默认对象和 include |
| 编译后的 `.json` | 已展开具体实例和数值的硬件描述，便于人工检查和跨进程传递 |
| `create-description` 操作日志 | 记录将哪些描述提交给哪个板端、结果是否确定、读回是否一致 |
| 实际 `.graw` 数据 | 由独立接收器处理，与这个配置流程分开 |

编译不会连接硬件，也不会写寄存器。创建描述会调用板端 `Node::create`，改变板端软件中的设备/寄存器访问对象。板端驱动的构造行为依实现而定，不能把 CREATE 当作纯查询。创建后核对的是地址、宽度、名称等元数据，不会顺带读取所有寄存器值。

```mermaid
flowchart LR
    X[原始 XML 与 include] --> C[离线 HardwareCompiler]
    C --> J[可检查的 JSON 描述]
    J --> V[原生格式与边界校验]
    V --> A[写入创建意图日志]
    A --> N[Ice Node::create]
    N --> R[枚举并核对元数据]
    R --> L[记录完成或不确定结果]
```

## 2. 编译器放在哪里

- `common/config/hardwarecompiler.*`：转换逻辑，依赖 Qt Core，复用原文保留的 `XcfgDocument`。
- `apps/configtool/`：`getdaq-config` 离线工具，随根 `GetDAQ.pro` 构建。
- 配置窗口：**GET configuration files... → Export hardware description...**。编译在工作任务中执行，界面不接收原始事件。
- `common/hardware/hardwaredescription.*`：原生工具/Qt查看共用的严格JSON读取、校验与权限编码，标准C++11，不依赖Qt/Ice/yaml；旧apps路径保留兼容占位。
- 配置窗口新增 **View compiled hardware description...**：只读设备/寄存器/位域树、精确64位地址与本地主机权限，连接期间可查看；见[查看手册](HARDWARE_DESCRIPTION_VIEW.md)。
- `HardwareClient::Create`：调用板端并核对返回描述。

因此 Windows Qt 5.14.2 MinGW 可以用于离线编译和查看描述，无须在那里编译旧 Ice。部署工具在 Ubuntu 构建运行。实际 Windows 和 Ubuntu 24 验收仍待完成。

## 3. 命令行离线编译

从项目根目录执行：

```sh
bash tools/build-linux.sh control
build-suite/bin/getdaq-config compile-hardware \
  source/get/GetBench-software-release-20210208/data/config/hardwareDescription_fullCoBoStandAlone.xcfg \
  cobo-description.json
```

输出必须使用新文件名；已有工件不会覆盖，避免把运行时引用的旧版本悄悄换掉。编译失败不会修改输入 XML。文件输出失败时会报错，不能拿不完整文件部署；部署端还会重新解析和校验。

在 Windows 的 Qt Kit 命令提示符下，对应程序通常为构建目录中的 `bin\getdaq-config.exe`。GUI 导出的也是同一格式。GUI 使用当时已经加载的文档快照，若磁盘内容随后被外部修改，先重新打开再导出；文件清单哈希对应加载的字节。

## 4. 编译如何处理旧配置

根对象必须有具体 `id`。编译器枚举 `Device`、`Register`、`BitField` 的具体实例；`id="*"` 只提供默认声明，不生成名为 `*` 的真实设备。

查找沿用已实现的 CompoundConfig 回溯规则，递归处理本地 include。例如完整 CoBo 描述中的 `zeroSuppress/Register[aget0ch1]` 会得到默认位域 `threshold`，宽度为 12 位，寄存器偏移为 4。

位域名可能包含斜杠，例如 MuTaNT 的 `C/S`、`M/S`、`L0/L1`。路径 `Node/Device[x]/Register[y]/BitField[C/S]/width` 中，方括号内部的 `/` 属于名称，不作为路径分隔符。

基础数值规则：

- `registerWidth` 是字节数，允许 1～8；不是位数。
- 位域 offset/width 是位数，必须落在寄存器内。
- 基址和寄存器地址偏移使用明确的 `0x` 前缀，或声明 `type="hex"`。不对缺乏类型的地址文本猜进制。
- JSON 中所有 64 位地址以十进制字符串保存，避免 double 导致精度丢失。
- 拒绝重复名称、非法位域、地址加法回绕和超限描述。
- 地址重叠、位域重叠并不自动当成错误：旧配置可能用别名描述同一硬件。是否符合实际固件仍须验证。

这不是完整 CompoundConfig 编译器。共享类单位、任意表达式、数据库来源和完整校准参数语义尚未全部实现；不能把硬件描述成功编译等同于全部实验配置通过验证。

## 5. JSON 工件怎样检查

顶层结构为：

```json
{
  "schema_version": 1,
  "kind": "getdaq.hardware-description",
  "node_id": "Hardware",
  "devices": [],
  "sources": [],
  "warnings": []
}
```

上面只是结构示意，空设备数组不能部署。`devices` 中每项含名称、访问方式、基址、寄存器字节数以及寄存器/位域列表。`sources` 记录原文件路径和 SHA-256，便于追踪来源；它不会把原 XML 文件嵌入工件，原文件仍应按 Run 快照保存。

`warnings` 必须阅读，尤其权限语义。来源哈希是追踪信息，不是工件的数字签名；原生加载器不会凭本机路径重新打开或自动下载源文件。

部署前先离线校验：

```sh
bash tools/build-linux.sh ice
build-ice-compat/getdaq-hardware validate-description cobo-description.json
```

该命令不建立 Ice 连接。读取最多 8 MiB，要求单文档和完整 schema，重复/未知键、数值溢出与不合范围的结构会被拒绝。底层解析器来自已有 yaml-cpp，因此也能接受同一严格结构的 YAML；工具标准输出工件仍为 JSON。

## 6. 向空节点创建描述

取得实际板端 Hardware Node 对象地址，确认属于准备/维护流程后执行：

```sh
build-ice-compat/getdaq-hardware create-description \
  '<对象名>:tcp -h <主机> -p <端口>' \
  cobo-description.json \
  create-description-001.jsonl
```

执行次序：

1. 本机解析并验证完整工件。
2. 连接明确指定的 Node，查询已有设备。
3. 如果节点非空，拒绝创建；不隐式调用 destroy、removeAllDevices 或替换已有设备。
4. 独占创建新的日志，持久化目标地址、节点名及完整将发送的设备结构。
5. 一次调用 `Node::create`，不自动重试。
6. 成功应答后，重新取得节点名、设备列表和每个设备描述，逐项核对结构。
7. 记录 `complete`，标出 `metadata_verified=true`、`register_values_read=false`、`board_permissions_verified=false`。

这不是跨客户端锁。若另一控制器在“检查空节点”和 CREATE 之间修改板端，仍可能竞争；当前要求实验流程保证唯一控制端。新后端没有自动销毁非空节点的恢复流程。

## 7. 创建失败不能直接再点一次

旧 `HwNodeImpl::create` 是逐设备创建，后面的设备失败时，前面的设备可能已经存在。因此：

- `unknown`：远端异常或响应丢失，可能已部分创建。保留日志，查询实际设备，不自动重发。
- `acknowledged` 后 `verification_failed`：创建已返回成功，但读回结构、查询或日志完成失败。不能重新 CREATE 来“重试校验”。
- `complete`：元数据核对通过；不证明寄存器值、时钟、校准或采集就绪。

已有节点再次提交 CREATE 会在发送前被拒绝，不会清空它。日志路径也不能重复使用。

## 8. readOnly 与 writeOnly 的真实边界

旧 GET `RegisterDescription` 没有整个寄存器的权限字段。虽然 `FieldDescription` 有 `readOnly`，当前提供的旧 `NodeAtTop`/`DeviceImpl` 没有完整填充、保存和实施这些标志。部分 MuTaNT 文件还使用 `writeOnly`，接口里没有对应项，不能擅自把它翻译成 readOnly。

编译器会保留本地 register/field 的 readOnly 策略，遇到上述能力缺口写入 warnings。创建后的元数据验证有意不声称旧板端权限已验证。

要在后续独立 CLI 调用中继续执行本地策略，应绑定同一工件：

```sh
build-ice-compat/getdaq-hardware --description cobo-description.json \
  describe '<板端代理地址>' asad

build-ice-compat/getdaq-hardware --description cobo-description.json \
  read-register '<板端代理地址>' asad monitorID
```

在绑定模式下，每次目标访问前核对设备、基址、访问方式、寄存器和位域结构。结构变化会拒绝访问；写入时再执行本地只读限制。一个整个只读的寄存器即使没有位域，也会拒绝写入。`Create` 成功后的同一个 C++ 客户端自动持有该策略；换进程后需重新用 `--description` 绑定。

没有绑定工件时，只能使用远端返回的描述，不能据此声称旧板端缺失的权限已经生效。`writeOnly` 尚没有新增本地读取限制，工具也不会自动遍历读取所有值。

## 9. 当前实测覆盖

已成功编译并通过原生读取校验：

| 原始配置 | 设备数 | 寄存器数 | 位域数 | 权限提示数 |
| --- | ---: | ---: | ---: | ---: |
| fullCoBoStandAlone | 11 | 1310 | 1429 | 2 |
| Mutant | 5 | 87 | 122 | 78 |
| Zedboard | 4 | 45 | 104 | 0 |

端到端测试：

```sh
python3 tests/description_integration.py \
  build-suite/bin/getdaq-config \
  build-ice-compat/getdaq-hardware \
  build-ice-compat/hardware-client-tests
```

测试把真实 CoBo 配置编译成工件，经原生读取后提交给本机模拟板端，创建 11 个设备并核对元数据；另验证文件不覆盖、非空节点拒绝、权限策略和结构变化拒绝。C++ TCP 测试覆盖 1.0/1.1 编码、部分创建失败、丢失应答、高位地址和错误读回。

这些结果不代表已向真实板卡部署，也不代表完整新 ECC 完成。下一步继续把配置、状态机、准备和校准动作组合成有明确前置状态的硬件控制流程。
单板Create之上的SystemDescriptionClient/getdaq-system-describe已组合全系统描述与状态位次序，明确区分核对已有、创建空节点和替换已知旧布局。旧/新完整策略、逐节点destroy/create、命令和故障恢复边界见[全系统描述手册](SYSTEM_DESCRIPTION.md)；工具不提交生产ECC状态。
