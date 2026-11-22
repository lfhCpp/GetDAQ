# MCP：连接当前原版 SOAP ECC

`getdaq-control-mcp` 是独立 stdio 程序，随 control / full 构建和安装。复用 GUI 的 YAML、原版 SOAP 请求和 Sentry 状态解析，无需 Python、Node.js 或额外 MCP SDK。

旧 MCP 曾连接已移除的原生 HTTP ECC（`base_url`、epoch、snapshot、原生维护/扫描接口）；那套配置不能用在 SOAP ECC 上。当前适配器不启动后端、不直连板卡，不替代 GUI Run 记录、自动采集和 Sentry catalog。

## 配置

1. 在 GetDAQ 完成 Nodes / Paths、Master / Slave 配置，Save YAML，保存实际使用的 `envoy.yml`。
2. 复制 `examples/ecc-control-mcp.example.json`（安装包中为 `share/config/ecc-control-mcp.example.json`），修改 `run_config` 指向该 YAML。相对路径以 MCP JSON 所在目录为准。
3. 在 MCP 客户端中设置本机可执行文件及参数。MCP 客户端负责启动此 stdio 子进程，不要把 SOAP URL 当作 MCP URL。

MCP JSON 示例：

```json
{
  "schema": 2,
  "run_config": "/home/user/DAQ/GetDAQ/site/my-experiment/envoy.yml",
  "allow_control": false,
  "request_timeout_ms": 30000
}
```

常见客户端配置格式（具体外层格式以对应客户端为准）：

```json
{
  "mcpServers": {
    "getdaq": {
      "command": "/home/user/DAQ/GetDAQ/bin/getdaq-control-mcp",
      "args": ["--config", "/home/user/DAQ/GetDAQ/mcp.json"]
    }
  }
}
```

macOS 使用 `GetDAQ.app/Contents/MacOS/getdaq-control-mcp` 的绝对路径。后台 ECC 可在任意可达主机；连接地址由 YAML 决定。改完 YAML 后重新启动 MCP 子进程加载，不会自动重启 ECC。

## 功能

| 工具 | 内容 |
| --- | --- |
| `ecc_configuration` | 当前 YAML 的节点、IP、端口、Describe / Prepare / Configure ID 和目录含义 |
| `ecc_status` | 实时 SOAP GetState，支持 `node_id` 或全部节点 |
| `ecc_receivers` | 实时 Sentry 状态，兼容 DataExporter / DataRouter，不读取原始数据 |
| `ecc_operation` | 按 `operation_id` 查询本适配器保存的命令回执 |
| `ecc_submit_operation` | 启用控制后，对一个指定节点发送一条原版 SOAP 命令 |

默认只开放查询。需要控制时，把本地 JSON 的 `allow_control` 改为 `true` 并重启 MCP。每条控制命令要求 `node_id`、`command`、唯一 `operation_id` 及确认字符串：

```json
{
  "node_id": 0,
  "command": "stop",
  "operation_id": "pedestals-stop-001",
  "confirmation": "I_CONFIRM_stop_pedestals-stop-001"
}
```

支持 describe / prepare / configure / start / stop / pause / resume / undo / breakup。确认文字必须由已获操作者同意的客户端提供。多节点准备、MuTaNT 顺序、完整 Run 创建/覆盖/归档仍建议使用主界面；这里是单节点 SOAP 命令接口。

## 回执、超时与并发

- 新回执默认位于 `~/GetDAQ/operations/<配置路径摘要>/`；已存在的旧回执目录继续复用，以保留命令去重记录。`ecc_configuration` 返回实际 `operation_directory`；可在 MCP JSON 指定独立目录。
- 先原子保存 accepted 意图，再发送 SOAP。回执和目录限本用户访问；同目录跨进程命令锁防止重复下发。
- 收到响应标记 acknowledged，**不等于已经达到目标状态**；调用 `ecc_status` 核对 Ready / Running。
- 超时或进程中断后结果标记 unknown，可能已在远端生效。重用相同 ID 只返回回执，不重发。不同内容使用旧 ID 被拒绝；必须先检查真实状态再决定新操作。
- GUI 和独立 MCP 都能发送原版 SOAP 命令，彼此没有跨客户端硬件事务锁。避免同时操作同一板卡；关闭 MCP 不停止采集。
- 观察有总时间预算；某节点耗尽预算时，可单独查询该节点。stdout 仅输出 JSON-RPC，诊断写 stderr。

支持 MCP stdio 协议版本 2024-11-05、2025-03-26、2025-06-18、2025-11-25，通过 initialize 协商；不宣称支持后续新协议或 HTTP MCP。实现依据 [官方 stdio 规范](https://modelcontextprotocol.io/specification/2025-11-25/basic/transports)、[生命周期](https://modelcontextprotocol.io/specification/2025-11-25/basic/lifecycle)和[工具调用规范](https://modelcontextprotocol.io/specification/2025-11-25/server/tools)。

测试：`python3 tests/ecc_mcp_test.py /path/to/bin/getdaq-control-mcp`，仅使用本地测试服务，不访问实板。
