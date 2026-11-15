# 源端观察与故障停止：当前检查点

此项正在接入整套系统，**尚未完成完整验收**。普通与新代码插桩的直接源控制、同机ECC故障、实际ICE HTTP联调、传统TCP/FDT回归及Qt观察层已有验证；并发/控制连接丢失补充仍要继续。不要将本页当成整套重构已经完成。

## 已实现的行为

DataSenderClient::ObserveSource使用独立的只读Ice communicator查询升级源的describeSource，核对原source ID、实例、接收会话和返回计数。它不安装新控制代理、不改变原控制连接、不改控制状态、不发Start/Stop/End。旧TCP/FDT/ZBUF来源显示legacy，尚未建立的ICE流显示unbound；查询中或无法核对时分别显示busy/unavailable。

查询超时与原控制超时分开。旧Ice的Ice.Override.Timeout会覆盖代理ice_timeout，不能在原communicator上改一个代理参数就认为查询已有短超时。独立只读communicator保留禁止重试，使用本次短超时；实际50ms超时用例已通过。NativeEccCoordinator按整体预算查询，预算用完的后续来源不发RPC。

观察返回的available表示应答身份和结构已核对，**不表示源健康、原控制权恢复或接收器封存**。源可以明确返回unknown；失联后只读查询恢复也不能清除原控制UNKNOWN。数据计数是本地交付统计，不是硬盘持久字节。源句柄通过独立共享所有权保持，不在慢查询期间占住ECC命令锁或引用已释放的准备对象。

显式StopProductionAfterFailure现在在原系统对象中先停硬件生产，再对已连接的升级ICE源请求一次stopIceSourceAfterFailure。它停止/join源读出线程，不排空、不补End、不关闭或封存receiver，不清UNKNOWN。源停止失败或已尝试但未确认时不重复发送；另一源仍可独立停止。原TCP/FDT/ZBUF负向路径不增加这种源RPC。

源故障发生在普通命令预检查期间，也会在NativeEccCoordinator保留源UNKNOWN门槛，防止“尚未进入Apply，所以仍可以继续运行”的错误。直接提交显式故障停止时，仍通过原已绑定控制对象复查源，再记录负向意图。正常命令不能以只读观察恢复、换对象或换会话清除此门槛；完整硬件恢复尚未实现。

## 第70.1阶段已验证的范围（历史）

- 普通SDK实际ECC服务及源测试程序已重建；不称整个旧SDK为严格pedantic或已插桩。
- 15个实际源控制场景通过，包含原11场景，以及只读超时、非法返回/只读恢复不清UNKNOWN、未知父对象的一次源停止和异步源故障观察。实际MFM字节/接收结束条件及生成数据清理保持。
- 同机ECC两种编码的异步源故障测试通过：只读观察不发动作，显式停止后两个源消费线程已停/join，原receiver会话保留，父状态仍UNKNOWN，没有源End/成功seal。
- 每组所有输入/输出流保守总上限分别8MiB和2MiB，低于用户20GiB合计上限。成功/失败组生成raw均删除；SIGKILL/断电不保证清理。
- 接续实际HTTP联调的两种ICE编码及空源三种正常流程通过；Ready时两源均返回available/connected，固定实例及整集合核对通过，只读查询没有硬件动作。空源场景验证正常生命周期，本次没有在该分支额外查询源接口。证据build-source-observation/ordinary-http-final/managed-ice-ecc-ckmhxt7r，8个raw共131072逻辑字节删除。
- 可选native_source_status已接Qt客户端、配置窗口和主窗口的Source state列。普通与新代码ASan/UBSan/LeakSanitizer的客户端各21、流程模型各20通过；源unknown/unavailable、身份/会话不匹配、真实缓存年龄过期及配置保存重载，不增加生产命令。系统Qt未整体插桩。
- 普通完整窗口67项通过，含源错误文字、14列表格及故障/过期颜色检查；实际GetDAQ操作台重建。qmake输出路径改为相对项目文件定位，避免独立构建目录把bin写到项目外。不是Windows Kit验证。
- 纯C++11协议新代码ASan/UBSan/LeakSanitizer通过，保留大于2^53的精确计数，拒绝重复来源、非法结束凭据和错误字段。原生插桩程序32严格编译及47个未变依赖对象；直接源控制15、同机异步故障2、实际ICE HTTP正常/空源3、传统TCP/FDT4通过。旧vendor/系统Qt未整体插桩，数据HTTP服务使用普通程序；不能称整套旧SDK已插桩。
- 普通和插桩传统TCP/FDT各4场景通过：两编码重连、明确未使用来源及默认TCP；普通和插桩ICE各3场景通过。各组只保留小型证据，raw分别196608和131072逻辑字节已删除。新接口在传统来源返回legacy，没有假装远端状态已知。

第70.1阶段汇总入口build-source-observation/checkpoint-gui.json核对该阶段输入、实际程序和最终小型证据摘要；本轮控制验证实现及夹具变更后，它只作历史，不代表当前工作树。小型Qt/纯协议结果保存在build-source-observation/verified-test-logs/，没有复制raw或大批测试程序；它是当前工作树的部分验证检查点，不是可脱离原构建目录重放的完整软件包。

较早源控制证据入口build-source-observation/checkpoint.json的16个源控制输入只在当时与工作树匹配；后续控制验证实现已修改，它不代表当前源码，也不涵盖GUI修改。普通直接组ordinary-direct-final-v3/source-control-61fuj4cw，同机故障组ordinary-source-failure-final/source-failure-ecc-6dis4ufe；新插桩对应sanitized-direct-final/source-control-stx78f8q和sanitized-source-failure-final/source-failure-ecc-tge14kzt。实际HTTP普通ordinary-http-final/managed-ice-ecc-ckmhxt7r、插桩sanitized-http-final/managed-ice-ecc-cvoubmjl；传统回归普通ordinary-legacy-current/managed-fdt-vhocnkwm、插桩sanitized-legacy-final/managed-fdt-b5fv4x0c。早期失败组只保留诊断，不作通过证明。

## 第70.2阶段：控制断线与观察并发

已复现并修复绑定ICE控制连接丢失后，命令内部预检查失败但本地状态没有进入UNKNOWN的问题。现在内部和显式验证使用同一失败入口；只读恢复不清门槛、不采用新控制连接。普通/新代码ASan/UBSan/LeakSanitizer各19直接源场景通过，新增Connected和Running两状态/两编码：Start或Stop在原连接验证阶段拒绝，生产调用次数不增加，没有假停止或End。

普通/新代码插桩各两编码的实际慢查询HTTP用例通过：屏障确认源RPC正在等待，`/v1/status`仍响应，第二个`/v1/sources`返回409，原查询释放后正常返回且没有硬件动作；随后完整采集和结束通过。普通/插桩各两编码的Reset生命周期通过：仅测试观察调用线程使用调试器式暂停，原查询预算不变，持有句柄跨实际Reset及采集集合释放；新查询集合为空，旧回答仍保留Breakup时的实例/session/操作ID，允许因原预算超时明确unavailable。

Reset暂停用例不证明自然调度下每个Reset必与短查询重叠，也不证明实时响应/硬件恢复。最初仅等待源RPC的组在500 ms后提前结束，未达到跨释放断言，raw已清理，只作诊断。控制断线用例最终由夹具清理有限生产线程和接收器，不是产品自动停止；旧控制断线时显式故障停止不能冒充已经成功。

本轮原生基线32严格C++11插桩编译，再分别对最终Reset及直接源夹具各重编1个对象，依赖摘要及复用对象核对；旧vendor/系统Qt未整体插桩，数据HTTP服务仍普通。Qt流程模型普通/新代码插桩各20通过，增加拒绝旧操作ID源回答的检查；没有改变GUI生产代码。

入口build-source-observation/checkpoint-concurrency.json，544输入/13实际程序/30小型证据摘要核对；普通/插桩各23原生源/HTTP/Reset场景，合计46，以及Qt模型各20通过。当前直接普通direct-concurrency-ordinary-final/source-control-sde47jd2、插桩direct-concurrency-sanitized-final/source-control-4wyq52kk；Reset普通reset-paused-ordinary-final/local-ice-ecc-ih3_jnw7、插桩reset-paused-sanitized-final/local-ice-ecc-1e9ubunc；HTTP普通overlap-ordinary-final/managed-ice-ecc-_93_zqel、插桩overlap-sanitized-final/managed-ice-ecc-3x2502tx。它仍是本机部分验证检查点，不是整套完成证明或独立重放包。

普通/插桩每组直接源131072、Reset65536、HTTP131072逻辑raw字节已删除，所有流总上界分别8/2/8MiB，保持用户20GiB合计上限。失败诊断组也清理raw；SIGKILL/断电仍可能阻止finally。

详细操作、失联含义和测试原理见[控制断线与并发说明](SOURCE_OBSERVATION_CONCURRENCY.md)。

## 尚未验证或接完

上述慢源HTTP重叠、Reset对象生命周期及Connected/Running原控制连接丢失已完成本机范围；仍不能据此证明实际硬件独占控制权、运行中完整接管/恢复或真实设备停止。

正常TCP/FDT/ICE与原生新代码插桩回归已完成上述范围。第一次普通传统回归误用build-receiver旧程序，未使用FDT结束请求被拒绝，失败raw已清理；最终改用19个输入摘要均与工作树相同的build-source-mfm/receiver重测通过，不能把失败组或同名程序作为当前版本证明。真实硬件停止/驱动、物理多主机、Ubuntu24/Windows MinGW/macOS及持续吞吐/断电验收仍待完成。

详细启用步骤、状态解释和原理见[源端状态观察手册](SOURCE_GUI_OBSERVATION.md)。

已有数据与源生命周期说明见[源控制](ICE_SOURCE_CONTROL.md)、[读出线程](SOURCE_READOUT_LIFECYCLE.md)、[MFM边界](SOURCE_MFM_BOUNDARIES.md)。本检查点之后继续完成上述工作，不替代[完整剩余验收](VALIDATION.md)。
