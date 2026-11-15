# Configure 寄存器读回证据：如何查看，能证明什么

## 1. 三件事要分别判断

1. **接受文件版本**：ECC 启动时接受了哪份参数、工件、仓库选择和父快照。
2. **执行配置命令**：某次 Configure 对哪些板卡发送了哪些写入和管理器命令，哪些命令得到了应答。
3. **比较实际读回**：板端寄存器 RPC 返回的值，在该次操作中是否与计划的期望值、掩码和允许差异相符。

收到文件不证明执行过配置，写命令应答也不证明读回一致。当前板端没有可靠的“已加载工件 SHA/版本”查询；因此不能把前两项的摘要直接称为板卡实际加载版本，更不能仅把 `hardware_loaded_verified` 改成 true。

新功能把已有 Configure 执行中的读回结果整理为可查询证据，同时保留未读回写入、管理器应答和差异警告。它不增加额外硬件读写，不改变原配置顺序，不读取 DAQ 文件，也不参与原始数据传输。

## 2. 覆盖了哪些流程

捕获正常整系统 Configure 中每个活动 CoBo 的 `cobo-configure` 和每个 MuTaNT 的 `mutant-configure` 主计划。停用 CoBo 不产生主配置记录。准备、对齐、同步脉冲、运行启停等其他阶段仍保留自己的日志，不混作配置寄存器核对。

CoBo 记录包含 AGET/FPGA/AsAd 配置中的期望比较、普通写入、延时和脉冲管理器调用。MuTaNT 还包含只读观察和显式读改写：先读旧值，按掩码计算有效写入值，再检查其读回。

同名寄存器可能属于不同被选择的芯片/AsAd；记录保持完整计划顺序与选择操作，不把重复名称随意合并成一个最终值。以下计数是**执行步骤计数**，不是物理寄存器去重数量，也不证明读完后所有板卡状态永久不变。

## 3. 核对日志为什么需要执行时的摘要

硬件审计器在每条记录成功写入并 fsync 后，更新带域名的 SHA256 链。执行成功的 HardwareClient 保存该摘要；不是从待检查文件重新计算一个摘要并自我认可。

Configure 完成一个主计划后，核对日志的身份、属主、0600权限、单硬链接和大小，再读取至多8MiB。解析器必须重建出执行对象保留的摘要；任何文件字节、记录顺序或结果变化都不能得到原摘要。

随后逐步检查 begin、应答、实际值和终止记录。普通期望比较重新计算 `(actual XOR expected) AND mask` 并检查允许差异；读改写核对旧值、掩码、有效写入值和最终读回。缺失 complete、步骤错序、未确认写入、矛盾结果、未知操作或不支持的记录不能发布成已完成证据。

这证明的是本程序这次执行所写的审计结果一致，不是独立硬件认证、数字签名或物理测量。具有控制程序修改能力的人不在该机制的防护范围内。

## 4. 通过服务查看

认证只读接口：

```text
GET /v1/configure-readbacks
```

它只返回内存缓存和当前父 ECC 状态，不重新读板卡或文件。原生客户端提供 `NativeControlHttpClient::ConfigureReadbacks()`。首次启动没有成功 Configure 时也能查询，状态为 `not-observed`、entries为空。

使用已有私有认证文件查询，替换路径和 URL：

```sh
python3 - /home/daq/getdaq-input/ecc.credential http://127.0.0.1:24003/v1/configure-readbacks <<'PY'
import json, pathlib, sys, urllib.request
token = pathlib.Path(sys.argv[1]).read_text().strip()
request = urllib.request.Request(sys.argv[2], headers={"Authorization": "Bearer " + token})
with urllib.request.urlopen(request, timeout=3) as response:
    print(json.dumps(json.load(response), ensure_ascii=False, indent=2))
PY
```

Python 只是可选查询方式，不是 C++ 服务或接收器的运行依赖。认证值不作为命令行参数显示，不要复制到日志。

| 顶层字段 | 含义 |
| --- | --- |
| `instance_id` / `snapshot_sha256` | 本服务实例与固定父配置；应与预期服务核对 |
| `configuration_sha256` / `artifact_sha256` | 此对象绑定的参数/工件摘要，不是板端报告的版本 |
| `controller` | 当前父ECC状态，包括UNKNOWN和需要核对；不能只看读回记录就忽略它 |
| `readbacks.configure_operation_id` | 这些检查属于哪一次Configure |
| `readbacks.phase` | 下面解释的捕获阶段 |
| `readbacks.historical` | 始终true，检查发生在过去的Configure过程中 |
| `readbacks.live_hardware_checked` | 始终false，GET没有实时读硬件 |
| `readbacks.actual_loaded_configuration_verified` | 始终false，未实现独立完整加载版本证明 |
| `entries` | 各主配置计划的板卡ID、阶段、审计文件及证据摘要 |

`pending` 表示开始了新 Configure、尚未捕获全部主计划；`unavailable` 表示这次失败前没有完整记录；`captured` 表示硬件主计划证据已捕获。之后接收启动、sender连接或父提交仍可能失败，所以 captured不能解释为父ECC已Ready或整套DAQ成功。

`controller` 是原生硬件父状态，不包含网络服务全部操作/存储/控制权门槛。操作前还应按原流程查询 `GET /v1/status`，检查 `control_needs_reconciliation`、当前操作和数据节点状态；不能仅凭这份历史读回结果决定继续采集。

重新Configure使用新操作ID并替换缓存，不把旧记录当成新结果。Reset、停止或后续状态变化不会把历史检查提升为当前证明。服务重启不会从旧日志自动认领已加载状态，缓存重新从not-observed开始；旧日志仍可离线检查。

## 5. 每条证据怎样读

| 字段 | 含义 |
| --- | --- |
| `audit_sha256` | 完整原始审计文件字节的SHA256 |
| `audit_chain_sha256` | 与执行对象保存值核对的记录链摘要 |
| `started_at_unix_ms` | 该硬件计划开始的主机时间，以字符串保存精确64位值 |
| `steps` | 计划步骤数量，最多4096 |
| `writes_acknowledged` | 普通写入及读改写得到应答的数量 |
| `writes_readback_verified` | 同目标、同值、完整掩码的紧邻写/期望比较，以及完整核对的读改写数量 |
| `writes_without_full_readback` | 已应答写入中未达到上述完整读回条件的数量；不能隐藏成全部验证 |
| `checks_matched` / `checks_with_warnings` | 期望比较通过数及其中允许差异的警告数；含前提观察，不都是配置参数写入后的比较 |
| `masked_updates_verified` | 已核对读旧值、计算、应答和完整有效值读回的读改写数量 |
| `read_only_observations` | 只记录实际值、没有期望比较的读取数量 |
| `manager_commands_acknowledged` | 脉冲管理器调用得到应答；不等于其全部效果得到独立读回 |
| `delays_completed` | 已完成的计划等待数量 |

允许差异有明确掩码，不把warning改写成精确一致。部分掩码比较不证明整个寄存器；暂未比较的选择/命令寄存器、脉冲、定时器及物理电气效果继续保持未验证边界。

## 6. 用无Qt工具离线检查

从可信的服务回复或受控捕获记录取得 `audit_chain_sha256`，再运行：

```sh
bin/getdaq-config-check readback-audit /home/daq/ecc/operations/prepare/prepare/hardware-15.jsonl TRUSTED_COMPLETED_CHAIN_SHA256
```

把路径和最后一个参数换成自己的文件及64位十六进制链摘要。该工具使用标准C++11，不连接硬件、不依赖Qt/Ice、不读原始DAQ流；成功打印覆盖计数和摘要，错误退出1。它检查的是这份历史文件，不能据此自动恢复ECC或启动采集。

不要用待检查文件现算出的链值作为“可信执行摘要”，那样只能检查内部自洽，丢失与原执行绑定的意义。普通文件SHA与链SHA也不能互换。

## 7. Qt 操作台查看页面

连接自己的 native ECC 服务后，点击顶部 **Configure register readbacks...**。旧 SOAP/模拟模式不显示此入口；未连接时不可操作。页面使用与 MUSIC 相同的 `.h/.cpp/.ui` 组织方式，只查看元数据，不发送配置/停止/恢复命令。

上方表格显示证据范围、原 Configure 操作ID、ECC实例、父快照、参数/工件及当前原生父状态；下方表格按板卡列出写应答、完整读回、未完整读回、比较/警告、只读观察/读改写、管理器应答/延时以及审计文件/摘要。鼠标悬停可查看精确64位计划开始时间、节点代理和原执行链摘要。计数仍是步骤数，不是物理寄存器去重数量。

页面打开且可见时每两秒读取缓存，也可点击 **Refresh historical evidence**；关闭后停止定时查询。它不会重读板卡。缓存收到超过六秒时在下一次显示更新中标为过期，并撤回下方板卡表；查询失败同样清空表格，避免旧数字被当作当前确认。

父 UNKNOWN/需要核对会明确显示，但可继续查看原历史记录；后续正常回复不会自动清客户端控制门槛。未观察、pending、unavailable分别显示相应信息，captured仍标明当前完整加载未验证，不显示“已全部加载”的绿色确认。操作前继续核对主窗口的完整网络/数据状态。

客户端核对原实例、父快照与参数/工件，严格拒绝虚假的实时/全量加载声明、矛盾覆盖计数、重复板卡、不安全审计文件名或超限内容；合法UTF-8板卡ID和空格与后端规则一致。窗口另核对所选快照和连接代次，配置切换/断开/重连前的延迟结果不会回到新页面。

## 8. 失败与后续验收

成功执行了部分硬件动作之后，审计捕获或核对失败，仍按原流程保持Failed/UNKNOWN，不重发先前写入、不自动回滚。保留原操作目录和记录用于调查。

当前记录来源于板端RPC返回与执行日志，尚无现场独立验证。真实板卡/固件、当前实际加载版本、全局控制权、完整恢复和物理同步仍待验收。流程、摘要、拒绝场景和两种历史编码的实际本机结果以 [验证文档](VALIDATION.md) 为准，不能用文件版本或模拟器代替现场结论。
