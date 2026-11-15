# 接收文件命名：native 与 GET 样式、计划绑定和归档

本文对应 `apps/datareceiver/FrameStore`、receiver参数和数据管理服务的固定接收计划。原始MFM仍由无Qt的C++11/POSIX接收器保存；命名只在创建文件时计算，不把波形展开、不增加传输库。

## 1. 选择哪一种

| 参数 | 文件例子 | 编号 |
| --- | --- | --- |
| `--file-naming native` | `CoBo0_AsAd0_长时间值_进程ID_0.graw` | 当前存储实例的全局文件序号，原默认保持 |
| `--file-naming get` | `CoBo0_AsAd0_2026-10-08T12:34:56.123_0000.graw` | 每个来源独立、至少四位、从0递增 |

默认仍native，现有管理计划不加字段时规范字节保持。GET样式适合需要传统日期和四位编号形状的下游扫描程序；真实旧分析程序仍须用样例互通，不能仅凭文件后缀宣称完整兼容。

GET时间戳采用接收机器的本地时区，含毫秒，不含时区偏移。它是写盘实例建立时的会话标识，不是板端触发时间或文件第一事件的物理时间。不同机器要对时，并记录实际时区，不能用文件名代替帧内时间。

## 2. 旧源码与保留的安全行为

旧GET FrameStorage文件名包含CoBo/AsAd或MuTanT、`YYYY-MM-DDTHH:MM:SS.mmm`和至少四位编号；旧Time.cpp使用localtime及毫秒。独立原函数片段对照27组时间/三时区和21个实际新版文件名，确认命名形状相同。

这不是旧存储行为逐条照搬：

- 旧代码新来源或事件重置时重新取时间、编号归0，毫秒重复时可能打开相同名字并截断；新版固定写盘会话时间、该来源编号持续递增。新名字独占创建；若完整文件名仍碰撞，则只截断并重写安全的单硬链接普通文件。符号链接、硬链接归档、权限不安全或正在使用的目标会拒绝，避免误删归档或覆盖活跃写入。
- 事件号小于或等于上一事件号仍开始新文件，但GET编号继续增加。自然EOF后同一常驻接收器再次连接，也继续该来源编号，不重用0000。
- 大小阈值仍使用新版整帧轮转，不切帧；达到阈值后下一帧新开文件。旧代码写后超过阈值可先创建空文件的细节没有复制。
- 未知MFM类型仍保存。旧FrameStorage对不识别的非MuTaNT类型直接丢弃，不能为了命名兼容恢复这种数据丢弃。
- 新版按明确帧类型分源；不把CoBo编号≥100当成MuTaNT。这个旧保留号约定及实际固件编号范围仍需现场核对。

GET样式保留传统可识别文件名，同时继续保持新版录盘、背压、异常和封存语义；不代表完整运行旧GET框架或所有下游工具已验证。

## 3. 独立运行

```sh
cmake -S apps/datareceiver -B build-receiver -DCMAKE_BUILD_TYPE=Release
cmake --build build-receiver --parallel 2
mkdir -p /tmp/getdaq-get-names/incoming
./build-receiver/getdaq-receiver \
  --output /tmp/getdaq-get-names/incoming \
  --port 46005 --status-port 46015 \
  --file-naming get --once
```

此例只监听本机。发送端指向输入46005，状态查看46015；目录必须为空且没有其他接收器占用。正式接收地址、端口和目录按实验配置设置，操作见[数据服务](DATA_SERVICE.md)。

命名与输入协议独立，GET样式也可配独立FDT或ZBUF入口。受管理的生产计划目前仍用TCP，不能因为文件命名可选就认为ECC已支持FDT生命周期，见[输入协议](RECEIVER_TRANSPORTS.md)。

GET名字含冒号，这是POSIX文件名的一部分。原生数据与归档服务在Linux/macOS运行；Windows GUI仍处理控制。不要把文件原名直接当Windows兼容路径承诺，macOS及跨文件系统实际行为尚待验。

## 4. 在固定接收计划中选择

在已有 `receiver_plan.sources` 对象增加 `file_naming`，以下是一个完整来源对象示例，接收程序路径/摘要、配置/工件摘要和其他计划字段仍必须正确：

```json
{
  "id": "C0",
  "receiver_address": "127.0.0.1:46005",
  "bind": "127.0.0.1",
  "output_root": "/data/getdaq/cobo0",
  "data_port": 46005,
  "status_port": 46015,
  "export_port": 0,
  "queue_mib": 64,
  "max_frame_mib": 32,
  "rotate_mib": 1024,
  "export_queue_mib": 16,
  "file_naming": "get"
}
```

选择值只能是小写字符串native/get，数字/布尔/未知值拒绝。省略等于native；规范编码省略默认native字段，六组与冻结旧编码器逐字节一致。get进入计划规范字节及摘要，并作为固定参数传给receiver，不接受远端随意拼接命令行。

改变命名也会改变计划摘要。应使用新的部署/审计目录并重新审阅/绑定预期计划，不能修改已运行部署文件或恢复旧日志来绕过核对。具体配置和认证入口见[远程管理](REMOTE_RECEIVER_CONTROL.md)。此阶段没有新增GUI命名下拉框，入口是固定计划配置。

启动后管理器核对真实状态的file_naming；缺字段按历史native处理，要求get却运行native时明确Failed并拒绝新Start，不自动重启/清理。Failed状态本身是禁止重试门槛，不要求所有启动失败的needs_reconciliation布尔都为true；已接入的数据进程不能随意kill来恢复。

## 5. 分源和编号

| 帧来源 | GET文件前缀 |
| --- | --- |
| 已识别CoBo | CoBo编号 + AsAd编号 |
| 已识别MuTaNT | MuTanT |
| 其他通用MFM | MFM类型 + Source编号 |

每个来源第一次创建0000，下一个0001。其他来源第一次也从0000开始；全局files状态仍是整个进程创建文件数。达到10000时不截断，编号宽度会扩展；拼接时按数字编号排序，不能把文件名当事件排序或重排帧。

GET模式的来源编号映射跨输入连接保留，最多1024个生命周期来源。自然EOF后释放文件/缓存，下一连接继续编号。文件名完全相同时，原生接收写入会截断并重写该文件；为了避免误伤归档，已有文件必须是当前用户所有、单硬链接、普通文件且可独占锁定，符号链接、硬链接归档和正在使用的文件都拒绝。管理器为每个接收周期使用新的session目录，且同一周期的时间戳/编号保持唯一，因此此规则只作用于实际文件名碰撞，不会把Run号映射成文件名，也不保证删除旧Run的其它文件。

## 6. 状态、封存及归档

GET模式状态额外有 `"file_naming":"get"`；native保持原状态键。received/accepted/sealed字节含义和EOF门槛不变，命名不影响数据完整性证明。

seal仍使用原schema和文件清单；不增加破坏严格读者的字段。文件名字、大小、PID/会话/配置身份、EOF/退出/锁和fsync核对全部保留。

原生归档允许GET时间中的冒号；斜线、反斜线、NUL和其他不支持字符仍拒绝，要求单一文件组件，并继续用目录句柄、禁止符号链接、设备/inode/大小及链接计数检查。同文件系统归档仍是硬链接，元数据服务不读或复制原始载荷。

实际两接收器Get计划已通过两完整采集周期及硬链接归档、真实认证HTTP服务的normal/catalog流程。下游必须按各Source子目录读取，不把两台板的同名前缀当成同一个文件。

## 7. 开发复查与磁盘清理

```sh
python3 tests/build_receiver_validation.py build-receiver
./build-receiver/receiver-core-tests
python3 tests/receiver_file_naming_integration.py \
  build-receiver/getdaq-receiver build-receiver/get-name-new-check
```

实际TCP每组13676684线上字节，包含混合来源/未知类型、十三个1MiB轮转文件、两连接编号及FDT组合；校验完整文件字节/哈希、计数、EOF/退出和seal后删除合成.graw。成功、失败和可处理中断都先结束自有子进程再清理，保留小型删除清单；SIGKILL/断电仍可能阻止清理。

管理专项普通/插桩各17场景顺序执行，包含命名不符拒绝、原始生命周期和归档，每组固定发送上界低于4MiB。认证服务normal/catalog、八HTTP故障、codec/credentials也分别通过。系统Qt/系统库未整体插桩，Qt只在低频管理服务。

归档文件系统单元测试曾遗留稀疏合成数据。本轮根据固定测试PID/摘要/会话等签名确认43个旧私有测试目录，删除221个.graw路径、逻辑5369806911字节，保留元数据/清理凭据，没有处理真实Run或历史参考材料。新Fixture使用独占目录RAII，成功/拒绝/异常均清理，不跟随符号链接；普通/插桩27项检查后无新目录遗留。

所有传输测试每轮全部流合计最多20GiB，结束删除生成数据。不要拆文件绕过上限；没有在本机填满磁盘。稀疏文件逻辑大小不等于物理写入或吞吐量。

## 8. 验证范围与证据

当前普通/ASan/UBSan/LeakSanitizer receiver及管理核心/低频服务构建有同版来源/程序/命令凭据。原native TCP五组、FDT/ZBUF十二场景及导出重连回归均通过，已有sentry是普通Qt程序。

旧参考验证只提取实际时间/命名/规范计划函数片段并保留原来源SHA；时间对象以微秒存根替换、计划入口改名，不调用新算法充当旧参考，不链接旧框架。它证明给定输入的格式，不证明旧全程存储行为一致。旧格式对照与当前原生ECC/管理工具重建记录均保留。

证据入口为各构建的results-get-naming-*.txt、results-managed-file-naming-final-v2.txt、results-file-naming-http-current.txt和reference-final/。独立`build-file-naming-supervisor/validation-file-naming/`排除.graw及实际认证值，保留小型证据和同版程序；`tests/archive_file_naming_validation.py`默认检查当前SHA，`--historical-only`只证明归档。

环境Ubuntu20.04.6/GCC9.4/Qt5.12.8，同一物理主机、合成发送器。没有真实固件/旧分析客户端、Windows/Ubuntu24/macOS、跨主机、断电或持续吞吐结论；完整板端所有权/加载/UNKNOWN恢复、ICE及受管理FDT等仍待完成。
