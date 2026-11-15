# 通信与配置协议

## 1. ECC 客户端

- 传输：HTTP POST。
- 路径：`/`，默认端口 8083。
- Content-Type：`text/xml; charset=utf-8`。
- HTTP 请求头：`SOAPAction: ""`。SOAP 1.1要求HTTP客户端携带此头；空字符串表示由HTTP Request-URI表达请求意图。
- SOAP Envelope namespace：`http://schemas.xmlsoap.org/soap/envelope/`。
- ECC default namespace：`urn:ecc`。
- 命令：GetState、Describe、Prepare、Configure、Start、Stop、Pause、Resume、Undo、Breakup。

旧 SOAP 控制模式下，操作台仅在所有节点均有新鲜且一致的状态时发出 Pause/Resume；每条命令应答后重新读取 GetState，全部节点到达 Paused/Running 才完成界面状态切换。暂停期间不累计运行时长，恢复后继续累计。数据接收仍由独立接收服务处理。

GetState 示例：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
                   xmlns="urn:ecc">
  <SOAP-ENV:Body><GetState/></SOAP-ENV:Body>
</SOAP-ENV:Envelope>
```

其他命令的 body 包含 `configID` 和 `table` 字符串参数，内容分别为 ConfigId 和 DataLinkSet XML。默认按本地 `ecc.hh` 的 std::string 定义进行 XML 转义，例如 `<configID>&lt;ConfigId&gt;...&lt;/ConfigId&gt;</configID>`。参数内本来就有 `&amp;` 时，外层会再次转义；接收端先解析 SOAP，再解析参数中的 XML，最终字符应与配置一致。

三个 SubConfigId 分别使用 type=describe/prepare/configure；默认 CoBo describe 为 `cobo<ID>`，其他项为实验名称。`describe_id`、`prepare_id`、`configure_id` 可以覆盖默认值。`soap_inline_xml: true` 保留旧 envoy 的内嵌 XML 形式，只有核对现场服务后才使用。inline XML 子元素继承SOAP默认`urn:ecc`命名空间；默认字符串形式中的内层XML在SOAP字符解码后仍是无命名空间XML。旧GET源码的`ConfigId::fromXml`对字符串内容使用TinyXML并按元素名查询，因此该序列化形状与其源码路径相符，但不能代替真实服务联调。`tests/test_getdaq.cpp::soapStringAndInlineXmlParameterForms`解析两种外层请求并核对参数语义；`soapNamespacedResponseEnvelopes`覆盖规范形状的响应/故障包装，本地夹具不等于现场抓包，也不决定现场envoy应选哪种模式。

DataLink 中的发送者为 `CoBo[ID]` 或 `Mutant[master]`。默认保留原版 data<ID>:46005 和 exporter<ID>:46007 两个目标；`router_host`/`exporter_host` 可独立设置，端口为 0 时省略该目标。改为新接收器单输入、内部导出时，应禁用旧第二数据目标；新 receiver 的导出监听口不能作为板端第二个输入目标。

响应必须具有 SOAP 1.1 Envelope（namespace URI 为 `http://schemas.xmlsoap.org/soap/envelope/`）、其直接 Body，以及直接位于 Body 内的操作响应包装（本地标签名以 `Response` 结尾）或 SOAP Fault。Envelope/Body 的前缀可以不同；ECC 操作响应包装的前缀与 namespace URI不固定，以兼容旧 gSOAP 输出差异。只从该包装内部读取字段：必须有有效 ErrorCode；非零错误或 SOAP Fault 返回错误。GetState 还必须包含合法 State 和整数 Transition。裸 XML、错误 SOAP Envelope namespace、缺少 Body/操作响应包装、重复字段、DTD 均拒绝。`soapNamespacedResponseEnvelopes`覆盖这些结构规则，但使用的是本地夹具，不是现场抓包或旧gSOAP服务器进程验证。状态值见 PRINCIPLES.md。

客户端最大响应为 1 MiB；HTTP 非 2xx、重定向、超时、畸形 XML 都不作为操作成功。不会接受 DTD。真正设备的 XML 样例仍需在实机联调时与当前服务版本核对。

选择空 `SOAPAction` 的旧端兼容依据：归档 `ecc.hh` 未声明 `method-action`，旧 `Makefile.am` 调用`soapcpp2 $(SOAP_CPP2_FLAGS) -L -x -pecc -n`，`configure.ac`只会在新版本添加`-z1`兼容选项，没有使用`-A`/`-a`。按 gSOAP 官方文档，`-A`才强制服务根据SOAPAction分派，`-a`才让存在的SOAPAction参与分派；因此**推断**该旧服务仍按SOAP Body操作名识别请求，空SOAPAction不会取代Body分派。SOAP 1.1头规则见[W3C SOAP 1.1第6.1.1节](https://www.w3.org/TR/SOAP)，gSOAP选项语义见[Genivia gSOAP用户指南](https://www.genivia.com/doc/guide/html/index.html)。本地TCP mock已断言客户端头值，但未运行旧生成服务，现场兼容仍需真实服务验证。

## 2. sentry GET /status

```json
{
  "disk": "DAQ storage",
  "process": "dataRouter",
  "data_path": "/data/incoming",
  "data_written_gb": 1.25,
  "data_path_files": 3,
  "disk_avail_gb": 900.0,
  "disk_total_gb": 1000.0
}
```

字符串字段必须为字符串；数字必须为有限非负数；文件数必须为整数。写入量和磁盘量都是十进制 GB。只统计 incoming 根目录 `.graw` 普通文件，不包含已归档子目录。

新 sentry 的进程检查失败返回 503；旧 sentry 通常返回 500。控制台将任何非 2xx 判为失败，兼容两种错误码。

## 3. sentry POST /catalog

请求 Content-Type 为 application/json：

```json
{"experiment":"Exp","run_number":42}
```

实验名称限制为 1–80 个字母、数字、下划线和连字符，首字符为字母或数字。Run 编号为非负整数。

新服务执行：校验请求 → 保持归档忙状态 → 等待 settle_ms → 取得目录锁 → 检查进程/磁盘 → 独占创建目标目录 → 写计划清单 → 逐文件重命名并更新清单 → 再次查询 → 写完成标记 → 返回 `/status` 格式。

若根目录有 `.getdaq-native-receiver` 标记，则必须在接收机器本地取得与 receiver 相同的 flock 锁，并验证 `.getdaq-receiver-seal.json` 的 schema=1、complete/durable=true、文件清单及大小。接收器存活、缺少凭据或文件变化均返回 409。凭据成功后随 Run 归档，相关细节见 [DATA_SERVICE.md](DATA_SERVICE.md)。

响应：

| HTTP 状态 | 含义 |
| --- | --- |
| 200 | 操作成功，附状态对象 |
| 400 | 参数、JSON 或 HTTP 请求无效 |
| 404 / 405 | 路径或方法不支持 |
| 409 | 已有 Run、另一归档正在进行、仍有 incoming 数据等冲突 |
| 500 | 文件、目录、清单或磁盘操作失败；可能已部分完成 |
| 503 | 进程不存在/查询失败，或连接数量达到上限 |

错误 JSON 包含 `error`。控制台即使收到 HTTP 错误，也不推断所有文件都没有移动。

## 4. 最小 HTTP 服务约束

服务只支持短连接 HTTP/1.0/1.1 的 GET/POST；每个连接处理一个请求后关闭。Header 最大 16 KiB，body 最大 64 KiB，同时连接最多 64，sentry 后台任务最多 8。

重复 Header、负数/异常 Content-Length、chunked 请求和 pipeline 请求被拒绝。客户端 QNetworkAccessManager 的正常 JSON/XML POST 使用 Content-Length，可以直接访问。

默认监听 127.0.0.1。实际节点部署时应在配置中明确指定实验网卡 IP。协议与原版一样没有认证/TLS，仅用于受控 DAQ 网络，不是公共互联网接口。此次为兼容重构，没有悄悄改变原服务器的鉴权要求。

## 5. 控制台 YAML 扩展

保留旧字段，新增 getdaq：

```yaml
experiment: Exp
run_number: 0
description: Run note
fields:
  Beam: '12C'
  Energy (MeV/U): '20'
getdaq:
  simulation: true
  config_dir: /data/get-configs
  output_dir: /data/run-records
  poll_ms: 2000
  timeout_ms: 10000
  catalog_timeout_ms: 120000
  step_timeout_ms: 120000
  settle_ms: 2000
  nodes:
    - id: 0
      role: cobo
      name: CoBo 0
      host: 192.168.41.60
      sentry_host: 192.168.41.100
      router_host: 192.168.41.100
      ecc_port: 8083
      sentry_port: 8080
      router_port: 46005
      exporter_port: 46007
    - id: 11
      role: mutant
      name: MuTaNT
      host: 192.168.41.1
      ecc_port: 8083
```

节点 ID 不要求连续，但不能重复；至多一个 MuTaNT，至少一个 CoBo；ECC endpoint 不能重复。允许纯 CoBo 配置。省略 getdaq 时使用原版拓扑默认值和安全的模拟模式。sentry_host/router_host/exporter_host 留空分别沿用 host。

## 6. sentry JSON 配置

```json
{
  "data_dir": "/data/incoming",
  "process_name": "getdaq-receiver",
  "disk_name": "DAQ storage",
  "bind": "192.168.41.60",
  "port": 8080,
  "settle_ms": 30000,
  "check_process": true
}
```

data_dir 必须已经存在。当前 `node.example.json` 使用 `getdaq-receiver`，启用进程检查时应按目标系统上的真实进程名填写；仍部署旧版 dataRouter 时填 `dataRouter`，Windows 旧版 DataExporter 部署则可填 `DataExporter.exe`。disk_name 是展示标签，实际磁盘数值从 data_dir 所在文件系统获取。原生 receiver 必须退出后才能归档，因此使用单独的 `native-receiver.example.json`：`check_process=false`，以目录锁与封存凭据确认；也可用于明确的测试场景。JSON 采用 Qt 原生解析，不依赖 yaml-cpp。

## 7. 原生接收器协议与未支持部分

`getdaq-receiver`默认接受无额外包装的MFM TCP字节流，ZBUF同属该路径；独立`--input-protocol FDT`只在帧边界过滤4字节心跳/GOODBYE，仍等待实际EOF。帧头依据本地MultiFrame/GetBench源码；下游导出保持little-endian uint64长度+原始帧，不发送FDT控制字。只读GET /status使用独立端口，字段见[DATA_SERVICE.md](DATA_SERVICE.md)和[输入协议手册](RECEIVER_TRANSPORTS.md)。`getdaq-receiver`不直接接受Ice RPC；需要兼容旧GET/MDaq的`DataProcessor.processData(ByteSeq)`时，使用单独的`getdaq-ice-receiver`入口；其结束凭据及受管理范围见[ICE数据入口](ICE_DATA_INGRESS.md)。固定FDT/ICE接收计划与ECC生命周期已有本机集成验证，但实际旧板固件、物理主机和持续高速采集仍需现场验收。没有重新定义硬件寄存器协议。

独立 Qt 模拟器的测试文件不是合法 MFM；`tests/receiver_integration.py` 独立构造合法头部的合成帧用于分段、分帧和写盘验证。合成帧不能替代实际固件帧样例和端到端事件完整性验收。

下游导出为单向读取订阅，客户端保留写方向打开、不发送额外字节；写方向EOF/额外输入结束连接。空闲时也回收名额，重连只得到之后发布的完整消息，不提供可靠处理确认或重放。详细步骤和兼容边界见[导出重连手册](EXPORTER_RECONNECTION.md)。
## 冷数据交接补齐的协议位置

`getdaq-ecc-handoff cold-complete-submit`及`cold-complete-inspect`是原ECC主机上的独立CLI，没有增加一个允许冷实例接管的旧ECC HTTP命令。原`complete-data-handoff`仍要求保留的原活实例。冷CLI核对原network策略、准确父请求、正常前状态和后端上下文，独占只读原生日志，再使用已有节点HTTP协议查询原操作；只有明确未受理且当前原周期匹配的请求才可本次补交。

原父操作仍为原failed或原已有状态，旧控制器不重新获得权利。新目录中的`cold-request.json`类型是`getdaq.cold-data-handoff-completion`，用于这一次工具操作；`receipt.json`沿用`getdaq.remote-control-handoff`或`getdaq.remote-plan-retirement`，以供已经冻结的接班实例核对。只读inspect从本次持久完成记录及节点证明重建相同字节、核对回执SHA，不POST或联网；报告型`getdaq.cold-handoff-review`不具有这份回执的用途。命令参数、文件位置和失败处理见[完整步骤](COLD_DATA_HANDOFF_COMPLETION.md)。


## 独立冷数据恢复任务

`getdaq-handoff-service`提供`GET /v1/cold-handoff/identity`、`POST /v1/cold-handoff/tasks`和`GET /v1/cold-handoff/tasks/TASK_ID`。全部要求固定Bearer认证。JSON类型分别为`getdaq.cold-handoff-identity`、`getdaq.cold-handoff-request`和`getdaq.cold-handoff-operation`，编码在`common/control/coldhandoffprotocol.*`。

请求只包含task_id、instance_id、policy_sha256、action和attempt_id及schema/kind，不允许路径或命令注入。submit的task等于attempt；inspect引用同服务历史中原submit。同ID准确重复返回原记录；不同意图同ID拒绝。新task绑定当前服务实例，历史task可跨服务重开只读查看。重开未完成任务保持unknown并阻止新submit，直到明确inspect证明准确原attempt持久完成；原unknown不被改写。任务GET不会启动检查工具。

完成操作中receipt以字符串保存原字节，另附SHA-256，不从展示JSON重新排版构造凭据。原控制失败/冻结以及节点协议保持。窗口确认、持久目录和异常处理见[详细手册](COLD_DATA_HANDOFF_WINDOW.md)。
