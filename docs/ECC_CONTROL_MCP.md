# ECC 控制 MCP

本程序 `getdaq-control-mcp` 把现有 ECC 网络控制服务包装成一个本机 MCP stdio server，供支持 MCP 的 AI 客户端发现状态、查询操作以及（显式启用后）提交有限的生命周期命令。它不控制板卡驱动、不代替 ECC 状态机、不打开/搬运原始数据文件，也不实现 TCP DAQ receiver。高流量接收和落盘仍由独立的原生 receiver 完成。

```text
AI 客户端 ──MCP / stdin/stdout──> getdaq-control-mcp ──Bearer HTTP──> getdaq-ecc-server
                                                                    │
                                                                    └──原生 ECC FSM / 数据节点
原始数据发送器 ───────────────────────────────> 原生 receiver ──> 文件存储
```

MCP 使用标准输入/输出的 JSON-RPC 消息；程序的 stdout 只输出协议消息，启动错误发往 stderr。MCP host 通过启动本地子进程连接它，因此通常每个 host 配置一个 MCP 子进程。适配器使用 Qt Core/Network，仅承载低频控制 JSON，不把 Qt 引入高速 receiver。协议版本和消息边界按 [MCP 2025-11-25 transport](https://modelcontextprotocol.io/specification/2025-11-25/basic/transports)、[lifecycle](https://modelcontextprotocol.io/specification/2025-11-25/basic/lifecycle) 与 [tools](https://modelcontextprotocol.io/specification/2025-11-25/server/tools) 实现。

## 提供的工具

| 工具 | 行为 | 是否改变硬件/采集状态 |
| --- | --- | --- |
| `ecc_identity` | 读取服务实例、配置快照和工件绑定 | 否 |
| `ecc_status` | 读取 ECC 与数据节点缓存状态 | 否；不等待硬件，也不读取 raw |
| `ecc_cobo_lifecycles` | 读取固定活动 CoBo 集合、生命周期投影和最近单板操作凭据 | 否；来自 ECC 生命周期/journal 投影，不是硬件读回；服务重启或历史不确定时可能为 UNKNOWN |
| `ecc_receivers` | 查询 receiver 进程观察 | 否；服务端可能执行有界状态检查 |
| `ecc_operation` | 按 `operation_id` 查询一个操作 | 否 |
| `ecc_submit_operation` | 提交一个 `describe/prepare/configure/start/stop/pause/resume/breakup/reset` 操作 | 是；仅 `allow_control=true` 时显示 |
| `ecc_reprepare_cobo` | 在系统保持健康Prepared时，按原不可变计划重新Prepare一个固定活动CoBo | 是；仅 `allow_control=true` 时显示 |
| `ecc_configure_cobo` | 在健康Prepared且数据已停止时Configure一个固定活动CoBo，整机仍保持Prepared | 是；仅 `allow_control=true` 时显示 |
| `ecc_catalog_run` | 为已停止并封存的 receiver session 提交明确的 Experiment/Run 归档请求；可显式请求替换已完成Run | 是；仅 `allow_control=true` 时显示 |
| `ecc_resume_catalog_replacement` | 对同一live ECC进程中保留的原失败Run替换Publish执行一次受限续作 | 是；仅 `allow_control=true` 时显示 |

`ecc_submit_operation` 要求新操作 ID 和严格匹配的 `I_CONFIRM_<command>_<operation_id>` 字段。该字符串是调用前的显式门槛，不能替代 MCP host 面向人的确认 UI。若计划让代理自主无人值守运行，应单独设计授权策略、命令白名单和现场安全规则；不要把“提供了 confirmation 字符串”误当作人已经批准。

`ecc_configure_cobo`单独要求`cobo_id`、新`operation_id`和精确的`I_CONFIRM_configure-cobo_<cobo_id>_<operation_id>`确认字符串。MCP预检与ECC服务端都会核对同一服务实例/快照、健康Prepared、数据进程停止、固定活动目标和无在途命令；操作只复用该板固定Configure计划，不改变父ECC状态、不触碰其他板或MuTaNT，也不启动Run。只读模式不公布此工具；不确定时以原operation ID查询。

`ecc_reprepare_cobo`要求`cobo_id`、新`operation_id`和精确的`I_CONFIRM_reprepare-cobo_<cobo_id>_<operation_id>`确认字符串。MCP预检要求同一固定服务身份/配置快照、父ECC处于健康Prepared、没有活动命令或reconciliation门槛，且选定CoBo已有可信的Prepared或Configured生命周期证据；ECC服务仍会再次执行自身门控。调用只重做该CoBo的原固定Prepare计划，不推进父系统状态，不触碰其他CoBo/MuTaNT，不执行系统对齐或启动Run。若状态未知，必须先人工恢复可验证状态；只读模式不公布此工具。测试及边界见[验证71.390](VALIDATION.md#71390-mcp单cobo-reprepare工具与预检回归2026-10-24)。

`ecc_cobo_lifecycles`是无参数只读工具，读取固定配置中的活动CoBo目标、ECC投影的生命周期状态及最近单板操作证据。MCP会核对服务client/instance/snapshot绑定、目标配置摘要、schema、目标唯一性及逐板字段集合；身份不匹配或响应结构异常时拒绝返回数据。它在只读模式也可用，不读取硬件、不解除UNKNOWN、不执行恢复；服务重启后的状态仍可能是UNKNOWN。若返回了最近操作`operation_id`，可将同一ID传给只读`ecc_operation`查看该操作的请求、阶段、消息和结果；这只查询缓存状态，不重试或改变硬件。需要新鲜固定控制位采样时使用`ecc_hardware_state`，但该采样同样不等同于板端Prepare/Configure状态读回。

`ecc_operation`按精确`operation_id`读取ECC缓存的操作请求、阶段、消息和结果。提交结果超时或不确定时，用原ID查询；也可查询`ecc_cobo_lifecycles`提供的最近单板操作ID。该工具是只读观察，不会重发请求或触发硬件动作。

`ecc_catalog_run` 必须提供 `experiment`、正整数 `run_number`、布尔值 `replace_completed_run`、唯一 `operation_id` 和 `I_CONFIRM_catalog_<operation_id>`。Run替换只能在新一轮采集正常停止、receiver退出并封存后进入归档阶段；ECC的固定状态机仍负责检查运行状态和各数据节点回执。替换为真时，多节点先全部prepare并验证候选，全部通过后才提交publish请求。MCP只提交控制元数据，不创建接收session，也不读取或传送`.graw`。

成功替换会在所有固定数据节点的新catalog receipt通过核验后进入Finalize：只清除同一source内完整文件名与新Run完全相同的旧raw链接，保留所有不同名旧raw及旧catalog备份元数据。Finalize失败会停止后续节点；受限恢复先观察原Publish操作，若所有节点已发布则跳过重复Publish并续作Finalize。跨主机不是原子提交，进程重启后的cold recovery不支持；未知结果时不得盲目换新ID重发。详见[验证71.366](VALIDATION.md#71366-native-run替换finalize恢复与父审计jsonl修复2026-10-23)和[Native替换规范](NATIVE_RUN_REPLACEMENT.md)。

### 受限恢复 `ecc_resume_catalog_replacement`

此工具只用于原替换catalog请求已失败、且ECC服务进程仍保留原Publish intent/journal凭据的情况。它不能恢复普通catalog失败、Prepare失败、服务重启后的旧任务或结果仍为`unknown`的操作。工具需要三个参数：

```json
{
  "failed_operation_id": "原失败的替换catalog操作ID",
  "operation_id": "新的恢复操作ID",
  "confirmation": "I_CONFIRM_catalog_recovery_<原失败ID>_<新恢复ID>"
}
```

MCP会先读取当前服务身份、原操作与系统状态，核对原请求确实是本客户端发起的已失败替换catalog、服务实例/配置快照/client epoch一致、没有活动命令，且控制器处于无transition/error/UNKNOWN的`Prepared`状态。然后ECC服务还会独立检查原请求和恢复资格仍在本进程内、原Publish凭据可读、没有输入/存储/报警/receiver独立故障保持。任一条件不满足都会拒绝；MCP预检不能代替服务端检查。

恢复请求复用原稳定replacement ID，并为未完成的数据节点创建新的子操作ID；服务先查询原Publish操作结果，确认未尝试节点确实不存在，再续作并验证所有正式catalog回执。恢复失败后不会提供第二次自动续作。ECC或DataSupervisor进程重启后，当前实现没有cold recovery。超时、HTTP错误或操作状态不确定时，应查询原operation与恢复operation的状态，不要重新生成ID盲目重发。

该工具的confirmation是调用门槛，MCP host仍必须在触发前向人展示并取得真实批准；仅由模型自行填入字符串不代表人已确认。它也不是跨节点事务回滚：已经发布的节点不会被回滚，旧backup/session payload会被保留。正向ECC服务恢复已有本机loopback集成回归：真实ECC HTTP服务在原替换Publish失败后接受精确续作，并复核两个本机Supervisor节点的catalog receipt；测试通过且清除reconciliation门槛。MCP工具参数转发目前由假HTTP集成测试覆盖，尚未验证MCP stdio host到真实ECC服务的完整正向端到端流程。该恢复依赖ECC和DataSupervisor进程中的live凭据，不能跨进程重启恢复。完整证据和范围见[验证71.364](VALIDATION.md#71364-native-run替换正向ecc服务恢复集成回归2026-10-23)。

ECC 服务是状态机权威，MCP 不能绕过它。操作员界面和MCP可以连接到同一个ECC服务，但要使用相同的固定`client_id`与已登记`client_epoch`，这样ECC不会把另一入口误判成新的控制者。两入口共享的是服务端控制权，不是客户端本地journal：每个界面仍有自己的操作记录。

ECC接口每次只接纳一个在途命令。比如GUI的Prepare仍在运行时，MCP提交新的Start/Prepare请求会得到HTTP 409“busy/not queued”；它不会排队等待，也不会向CoBo发送第二条命令。反过来也一样。完成当前操作并刷新状态后，另一入口才可以提交下一条；并发操作由服务端拒绝，不应靠客户端睡眠或自动重试来猜测何时空闲。相同operation ID和完全相同请求只返回原操作，不会重复硬件动作；相同ID配不同请求会拒绝。不同MCP/GUI配置若持有不同epoch，后来的请求会被owner门槛拒绝，不得删除epoch文件或擅自换epoch接管。

MCP会把ECC HTTP 409响应中不超过512字符的`error`说明带入工具的`isError=true`文本（例如`busy; not queued`），便于host明确告诉操作者该命令没有被排队。它不自动重试或稍后执行该命令。

非法状态、身份/快照过期、操作冲突和恢复门槛均由 ECC 服务拒绝。每个新动作使用唯一 ID。若 HTTP 请求超时或返回结果不确定，先用 `ecc_operation` 查询同一 ID；不要盲目生成新 ID 重复执行。MCP 层不会自动重试控制 POST。相同owner epoch跨入口的busy拒绝由[验证71.368](VALIDATION.md#71368-gui与mcp共享owner时服务端拒绝并发控制命令2026-10-23)的ECC网络服务回归覆盖；它证明不会并行执行，不代表控制命令会自动排队。

## 构建

要求 Qt 5.12 或更新的 Qt 5、与 Qt Kit 匹配的 C++ 编译器和 qmake；代码为 C++11。Windows 可使用项目指定的 Qt 5.14.2 MinGW。构建整个控制套件时它会随 control/full profile 一起构建：

```sh
python3 tools/build-suite.py --profile control --plan
python3 tools/build-suite.py --profile control --jobs 4
```

也可独立构建：

```sh
mkdir -p /tmp/getdaq-control-mcp-build
cd /tmp/getdaq-control-mcp-build
qmake /path/to/GetDAQ/apps/ecc/mcp/mcp.pro
make -j4
```

项目还提供独立 CMake 入口：

```sh
cmake -S apps/ecc/mcp -B /tmp/getdaq-mcp-build -DBUILD_TESTING=ON
cmake --build /tmp/getdaq-mcp-build --parallel 4
(cd /tmp/getdaq-mcp-build && ctest --output-on-failure)
```

## 安装配置

### 1. 准备 ECC 服务凭据

先按 [ECC 网络服务手册](ECC_NETWORK_SERVICE.md) 部署并启动 `getdaq-ecc-server`。从可信的本地终端为 ECC API 生成 token 文件：

```sh
install -d -m 700 /home/daq/getdaq-credentials
getdaq-ecc-server --generate-token /home/daq/getdaq-credentials/ecc-main.token
```

将真实服务地址和 token 路径填入 [示例配置](../examples/ecc-control-mcp.example.json)，然后设置仅当前用户可读写的文件权限：

```sh
chmod 600 /home/daq/getdaq-credentials/ecc-main.token
chmod 600 /home/daq/getdaq-credentials/getdaq-control-mcp.json
```

`client_id` 必须与 ECC 固定服务配置里的 `client_id` 完全一致。ECC 部署一次只绑定一个控制 `client_epoch`：MCP在提交前读取固定服务的身份和当前状态；如果状态尚无owner，就生成并持久保存自己的epoch；如果已有owner且本地文件还不存在，就复用并保存服务报告的epoch。已有本地epoch与服务owner不一致时，MCP拒绝提交，不尝试接管。这样MCP能与已经通过GUI取得控制权的同一操作者配合，同时保留服务端不允许静默换owner的约束。`client_epoch_file`应稳定保存；不要随意删除、用另一值覆盖，或让不同操作者共用。

epoch文件父目录必须已存在且对其他用户不可访问。Unix 上程序拒绝 owner 不匹配或 group/other 有权限的 token/epoch 文件；epoch 文件以 owner-only 权限写出。token 不写入 MCP 配置正文，避免误提交到版本库。

默认 `allow_control` 为 `false`。首次集成建议保留只读状态工具，确认服务身份、网络路由和 MCP host 工具确认行为都符合预期。只有在你明确需要 AI 触发动作时才设为 `true`；命令最终仍受 ECC 服务端状态机约束。

### 2. MCP host 配置

MCP 客户端需要启动本机程序并提供配置路径。通用 JSON 形式如下（替换为真实绝对路径）：

```json
{
  "mcpServers": {
    "getdaq-ecc": {
      "command": "/absolute/path/to/getdaq-control-mcp",
      "args": ["--config", "/home/daq/getdaq-credentials/getdaq-control-mcp.json"]
    }
  }
}
```

具体配置文件位置由使用的 MCP host 决定；不要把 token 作为 `args`、环境变量或 prompt 内容传入。用 `getdaq-control-mcp --help` 检查二进制可启动，再在 MCP host 的工具列表中确认工具描述。连接只会在工具被调用时访问 ECC HTTP API。

## 网络与安全边界

- ECC Bearer token 认证不会自动提供传输加密。ECC 服务若跨主机连接，应使用受信任的隔离网络或由部署管理员配置安全隧道/HTTPS 终结；不要把明文 HTTP token 暴露到不可信网络。
- MCP stdio 本身不监听端口、不开放远程 MCP endpoint；远端 AI host 如需调用，应由 host 侧采用受控隧道或运行在控制主机上。
- 适配器会检查返回 HTTP 状态、限制单条 MCP 消息和 HTTP 回复大小，且不会在诊断中输出 token。
- `allow_control=true` 会向 MCP host 暴露控制工具；拥有本地 MCP host 配置权限的人也能调用。认证 token 应只授予可信账户，MCP host 必须对控制动作显示真实确认，不可假设工具 annotation 自动实施批准。
- 本适配器只传递命令与有限 JSON 状态，不接触 `.graw`、frame stream、receiver buffer 或大文件。它不在数据吞吐路径上，MCP/Qt 的延迟不会影响 DAQ 写盘速率。

## 测试

`tests/ecc_mcp_test.py` 使用临时目录与本机假 ECC HTTP 服务检查 stdio 消息、初始化、工具 schema、只读调用、Bearer header、生命周期/归档/恢复确认门槛、catalog替换字段、操作 ID/epoch 绑定和默认关闭控制模式。测试只交换小型 JSON，不生成 raw 文件：

```sh
python3 tests/ecc_mcp_test.py /path/to/getdaq-control-mcp
```

这项集成测试证明的是 MCP 到假 HTTP 服务的协议适配，不证明真实 ECC 状态机的正向恢复、多节点publish成功恢复、板端动作、跨机网络安全或现场授权流程。真实ECC loopback的负向门槛测试仅证明普通catalog不能借恢复命令绕过状态机。MCP的catalog工具用于明确提交封存session的归档/替换请求，不代表替代GUI的Run Start确认和Run号管理。
