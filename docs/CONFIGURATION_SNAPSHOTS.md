# 每次运行的配置依赖快照

本页说明Run收尾保存的原字节依赖镜像。编辑器/CLI导出的具体对象、有效值、约束和来源审阅JSON见[有效配置快照](EFFECTIVE_CONFIGURATION_SNAPSHOTS.md)；两者都不证明远端硬件实际加载。

## 1. 为什么不能只复制三个文件

GET 配置可能按不同节点使用不同 describe/prepare/configure ID，一个文件又可以 include 其他文件。只备份实验同名的三个文件，会遗漏自定义 ID、嵌套引用或位于配置目录外的共享参数。

新的收尾备份按照每个节点实际发送的配置 ID 选择入口，再递归收集 include。SOAP 和备份共用同一个 ID 选择函数，避免“发送的是 A，备份的是 B”。默认 CoBo describe ID 为 `cobo<节点编号>`，其他默认值为实验名；显式 ID 优先。

文件查找沿用旧 ConfigFileManager 的规则：`<类型>-<ID>` 替换扩展名后先尝试 `.xcfg`，不存在再尝试 `.xml`。例如 ID `special` 对应 `configure-special.xcfg`，不能自动假定它位于 `describe-cobo/` 子目录。

## 2. 快照保存的位置

每个 Run 的控制台记录目录包含：

```text
run_NNNN/
  run.yml
  journal.json
  configuration/
    manifest.json
    files/
      config/                  配置目录内部文件，保留相对路径
      external/<路径哈希>/     配置目录外部的 include 文件
  backup.complete
  run.csv
```

目录内部的文件名、内容、注释、中文和 include 声明保持原样，不把展开后的内容重新序列化。共享文件按规范化真实路径去重；不同目录的同名文件不会互相覆盖。

外部引用按原始路径的 SHA-256 分目录，这个目录名用于区分来源。清单中另外记录文件**内容**的 SHA-256，不要把两个哈希混为一谈。

## 3. manifest.json 如何读

| 字段 | 含义 |
| --- | --- |
| `schema_version` | 清单格式版本，目前为 1 |
| `captured_utc` | 本次捕获的 UTC 时间 |
| `capture_phase` | 目前为 `finalize_local_mirror`，明确表示收尾时的本地镜像 |
| `config_directory` | 收集时的配置目录真实路径 |
| `roots` | 每个节点、类型、配置 ID 和入口文件的对应关系 |
| `files[].source` | 原文件真实路径 |
| `files[].stored` | 相对 `configuration/` 的存储位置 |
| `files[].bytes` | 字节数，以十进制字符串记录 |
| `files[].sha256` | 内容哈希 |
| `files[].external` | 是否位于主配置目录之外 |

`backup.complete` 在完整目录发布并校验之后写入，记录 manifest.json 的哈希。没有完成标记时不能把零散文件当作成功备份。旧运行中的 `backup.partial/` 和旧完成标记继续保留，不自动迁移或覆盖历史记录。

## 4. 失败时发生什么

临时目录采用 `configuration.pending-XXXXXX`。缺少入口/include、XML 无法读取、循环引用、文件变化、超过容量或写入失败，都会让本地收尾报错，不推进 Run 编号。

成功收集后再次比较所有源文件的大小和内容，尽量发现收集过程中的修改，再将临时目录发布成 `configuration/`。这不是跨进程全局锁，不能与其他编辑器同时修改而期待原子快照。

如果目录已发布而写完成标记失败，重试会校验已发布的所有文件并补写标记，不会重新读取新的源内容替换旧快照。校验失败则保留现场并报错。此前已经完成 Catalog 的节点不会因为本地备份重试而再次归档。

恢复中断记录还会核对原快照中的独立 sentry/数据主机、端口、配置 ID、SOAP 模式及配置目录；改变这些目标后不能继续旧 Run 的归档。

总计限制为 256 个文件、64 MiB；单文件解析限制 8 MiB。配置通常远小于数据文件；这些限制不会限制原始 DAQ 数据量。内部 simulation=true 不访问真实 GET 文件；独立 HTTP 模拟器提供可解析的占位 XML，但它们不是硬件配置。

## 5. 恢复时的步骤

1. 保留原 Run 目录副本，读取清单确认节点、入口和捕获时间。
2. 逐个核对备份文件大小与内容 SHA-256。
3. 配置目录内部文件可按 `files/config/` 下的相对目录恢复到新的配置根目录。
4. 外部 include 文件根据 `source` 恢复原位置，或在**工作副本**中明确调整 include 路径。原始快照保持不变。
5. 用配置页面检查所有引用，随后通过实际 GET 后端检查有效参数。
6. 确认部署目标后再下发，不能仅因为备份成功就自动恢复寄存器。

绝对路径 include 和外部文件意味着整个快照不一定能直接换个目录打开；清单保留了恢复所需的来源映射，但还没有自动重写引用或一键部署功能。

## 6. 必须明确的可靠性边界

目前在 Catalog 之后、本地收尾时捕获配置，得到的是那一刻控制台可见的本地文件。它不证明这些字节在 Describe/Prepare/Configure 时相同，也不证明远端 ECC 已加载了这些字节。正在运行时不要改动配置镜像；界面连接期间只读，但外部编辑器仍能修改文件。

后续硬件控制后端需要将“配置校验、不可变发布、实际下发和读回结果”绑定到同一个版本。完成该步骤前，不能将 `backup.complete` 当成硬件运行配置的一致性凭证。
