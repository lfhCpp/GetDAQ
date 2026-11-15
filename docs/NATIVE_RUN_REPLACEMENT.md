# Native Run同名raw替换事务

本文规定Native ECC/receiver模式在Run号回退并重新采集时，如何按用户确认的规则替换已有数据。这里只描述控制面与归档文件的低频元数据操作；receiver仍直接接收并写入raw，Qt和MCP不进入数据传输热路径。

## 必须保持的用户语义

- 改Run号不触碰旧数据。只有在操作员点击Start并确认已有Run替换后，才建立本次替换意图。
- 新Run必须正常结束，所有receiver退出、seal有效且各数据节点新catalog完整之后，替换才可提交。
- 只有同一source下完整文件名完全相同的旧`.graw`由新raw取代。旧catalog中没有同名新文件的raw必须保留，并在新Run归档中仍可检索。
- 失败、超时或跨主机结果未知时，不前进Run号、不自动重放、不提前删除任何旧raw；保留旧catalog、操作ID和可恢复证据。
- raw不复制、不读入控制进程；同一文件系统用硬链接/rename及目录元数据操作完成。一次操作不得因为保留旧raw产生第二份payload副本。

## 当前实现边界

已实现本机候选准备和catalog目录发布工具。`getdaq-catalog --prepare-catalog-replacement OLD NEW CANDIDATE`验证两份完整输入catalog后，在一个新的同文件系统目录建立独立schema 2候选。候选对同一source按完整文件名合并，精确同名由NEW胜出，其余OLD raw以硬链接保留；候选记录来源catalog receipt摘要、source目录和inode/大小身份，随后由`InspectReceiverCatalog`独立校验。schema 2 completion receipt绑定正式Run catalog路径，manifest还记录正式root/target和按operation ID派生的旧catalog backup路径；因此候选可在暂存位置检查，并可在同卷改名到正式路径后重开检查。

`getdaq-catalog --publish-catalog-replacement OLD NEW CANDIDATE`实现单机目录发布：在目标父目录创建operation绑定journal和独占lock，将原catalog改名到operation专属backup，再将候选以不覆盖目标的rename发布到正式路径，并写入阶段标记、同步相关目录、重开检查receipt。同一operation ID重试会核对意图、receipt摘要和目录身份，可从“旧catalog已移入backup但标记未写”及“候选已改名到target但published标记未写”两个中断窗口恢复；无法解释的状态会停止并保留全部路径。Linux使用`renameat2(RENAME_NOREPLACE)`，macOS使用`renameatx_np(RENAME_EXCL)`；其他平台失败关闭。发布后保留完整旧catalog backup；新catalog逻辑集合中，新raw胜出同名项，旧catalog中不同名raw仍保留。

`getdaq-catalog --inspect-catalog-replacement OLD NEW CANDIDATE OPERATION_ID`提供只读阶段观察，检查operation绑定的journal、marker及目标/backup/candidate receipt身份，可报告prepared、old-preserved、published、finalize-incomplete和finalized等稳定或可识别阶段。该命令不读取raw payload（`raw_payload_bytes_read=0`），且固定返回`recovery_authorized=false`；缺失/被占用/不安全的事务锁、路径身份冲突或无法解释的文件状态会失败关闭。观察结果本身不能授权修复或跨Supervisor/ECC重启续作；现有冷启动恢复仍未实现。Linux filesystem回归覆盖两个rename后、marker尚未写入的中断状态，并确认由同一operation的既有Publish逻辑续作；另验证事务锁占用时观察被拒绝且不改动replacement路径。它不是断电实测或多节点恢复验收。详见[验证71.372](VALIDATION.md#71372-native-run替换只读检查中断阶段2026-10-23)。

schema 1旧catalog继续可检验，schema 2候选可以作为后续替换的旧catalog或新catalog输入。计划条目区分逻辑来源是否为新Run与文件是否属于该catalog的current sealed session；因此下一次candidate只把新输入的current session文件写入`files`，而历史`retained_files`继续作为保留项，并不会被伪装成本次接收数据。准备和发布均不复制或读取raw payload。单机准备/发布API不清理raw；生产控制在所有数据节点Publish receipt核验成功后才接入Finalizer，精确删除同一source、完整文件名碰撞的旧raw链接，非同名旧raw保留。全节点Prepare屏障已实现；Publish或Finalize阶段遇到首个节点失败或结果不确定时会停止后续操作并把未尝试节点写入父操作审计。失败留下的stage、candidate、backup和journal必须先检查，不能盲目清除。

### 当前源码的生产控制接线状态

Envoy Native Start现可在操作员确认后把替换标志交给RunControl/NativeRunControl；该意图保存在本机Run journal，并沿ECC请求和每个数据主机的Supervisor catalog请求传递。旧请求省略新字段时保持原普通catalog编码，显式替换标志改变请求身份。Native Run只有完成全部原有完成门槛（receiver正常退出/seal、catalog和本机Run记录完成）后才会走Run号递增路径。替换的本机Run记录备份由`RunRecorder::FinalizeReplacement`清理；receiver catalog旧目录backup元数据保留，但其中与新Run同名的旧payload链接由Finalize清理，不同名payload保留。

每个数据主机收到`catalog_phase=prepare`后，会建立该节点的stage并准备独立candidate；ECC远端接收管理器逐个等待全部固定节点prepare成功并校验回执后，才写入第二阶段`catalog_phase=publish`意图，逐节点发布正式Run catalog。所有Publish receipt验证成功后才进入`finalize`阶段，逐节点清理同名旧raw。各阶段使用独立journal operation ID，但以同一稳定replacement ID绑定stage、candidate和receipt。prepare失败时不会进入publish；Publish或Finalize遇到首个节点失败或结果不确定，会停止后续节点操作并将余下节点记为`node_not_attempted`。父操作保留reconciliation，不自动回滚或盲目重试。显式恢复会先只读观察原Publish operation：若发布尚未全部完成，则依相同replacement ID恢复Publish；若所有节点已完成Publish，则跳过重复Publish，只恢复Finalize。JSONL父审计将嵌套JSON规范化为单行，供恢复解析。双逻辑数据主机fixture已验证正常替换、Prepare失败屏障、Publish首失败停止及Finalize失败后的不重复Publish恢复；这是同机loopback，不是物理双机验收。

父级现提供显式`ResumeCatalogReplacement(newAudit, originalPublishAudit, experiment, run)`恢复入口。它只接受同一个仍在运行的ECC/RemoteReceiverManager所保留的原Publish intent/journal，先按原operation ID查询所有已提交节点：只接受已终止的`complete`或`failed`；原审计标记为`node_not_attempted`的节点必须由远端返回operation不存在。请求、节点、session、Run、controller身份和稳定replacement ID都必须与原intent一致。只有所有已提交请求都已得到终态证据且未尝试节点确认不存在，才会写恢复审计并提交同一个replacement ID；各Supervisor用新的子operation ID处理恢复请求。恢复成功后逐节点重新核验正式catalog receipt，再清除父级需对账标志。

子节点只在当前DataSupervisor进程仍持有完全相同的失败replacement ID、Experiment、Run和session，原receiver进程已退出且seal完整、存储/目录身份检查通过时，接受一次Publish恢复。首次恢复再次失败后不再允许自动恢复。原ECC或数据主机Supervisor重启会丢失这组仅存在内存中的失败证明，因此冷启动恢复仍未支持；远端operation查询超时、状态仍为`unknown`、已提交operation意外不存在或任何身份不符时均失败关闭，不得换新replacement ID或盲目重发。此能力是有证据门槛的显式恢复，不是跨主机原子提交，也不回滚已发布节点。

数据Supervisor现在提供认证只读GET `/v1/catalog-replacement-observations/{publish-operation-id}`，并由`NativeControlHttpClient::CatalogReplacementObservation()`调用。它只根据本服务持久化的原replacement Publish/Finalize操作推导Run目标、stage与candidate，再调用filesystem观察器；响应绑定原request key、节点/plan/controller身份、原请求实例和当前Supervisor实例。Prepare操作、未认证请求和正在占用事务锁均拒绝。观察不读raw payload、不改变catalog/journal，也固定返回`recovery_authorized=false`。`catalog-restart`集成回归已验证服务重启前后用原Publish ID观察相同已发布事务成功；这只是重建远端证据的子节点基础，ECC父操作尚未把重启后观察结果与持久Run审计结合，因此不能据此恢复Publish或Finalize。

ECC HTTP控制面已接收`resume-catalog-replacement`，请求必须使用新的恢复operation ID并引用原失败replacement operation ID。服务只允许原catalog请求明确带有`catalog_replace_existing=true`、原操作已持久化为`failed`、同一live ECC进程仍保留对应Publish凭据、硬件控制器仍为健康`Prepared`且没有其他独立故障保持时进入恢复。恢复请求与原请求必须匹配ECC实例、配置快照、client ID和client epoch；接纳后仍由上述`RemoteReceiverManager::ResumeCatalogReplacement()`执行远端核对和续作。Envoy主窗口已有独立恢复按钮，只有本机Run journal、ECC client store、原失败操作、实时身份和健康Prepared状态全部匹配时启用；弹框展示原失败ID和新恢复ID并要求操作员再次确认。该UI通过假ECC RunControl集成回归，不能替代真实GUI、真实ECC及MCP host批准体验验收。ECC服务或数据主机重启后的cold recovery仍不支持。

`getdaq-control-mcp`在显式`allow_control=true`时公布`ecc_resume_catalog_replacement`。工具要求原失败ID、新恢复ID和绑定两者的确认字符串；它先从ECC读取identity、原operation和status作预检，随后仍由ECC服务端重复执行权威校验。MCP confirmation只构成调用参数门槛，不能替代host提供给操作员的批准UI，也不应作为无人值守授权。当前已验证协议/MCP绑定及真实ECC对不合格恢复请求的拒绝；正向ECC服务跨进程恢复回归仍待补齐，不能把Manager loopback成功用例等同于此服务入口的完整验收。

Finalize会保留旧catalog backup目录及旧session的其他文件，但现在会逐项物理删除“同一source、完整文件名完全相同”的旧raw硬链接：先持久化包含旧/新receipt和文件身份的cleanup intent，再核对目录、session排他锁、设备号/inode/大小及新raw身份，分别删除backup和旧session链接，并为每项写稳定完成标记。断电或进程在两个unlink之间退出时，同一operation ID可按remaining link恢复；重复Finalize幂等。不同名旧raw和其他source文件保持不变。Linux filesystem回归通过同名真实fixture验证只清理同名项、保留不同名项与重复Finalize。这个单节点清理闭环不等于父ECC在重启后可自动恢复：父/子服务重启后的cold recovery仍不支持，多节点Publish/Finalize仍可能部分完成，必须显式对账与恢复；不得换新operation ID盲目重试。Native GUI确认/journal和Run成功后的编号递增有定向QtTest。71.357新增的MCP `ecc_catalog_run` 可明确提交归档/替换请求，但不启动Run、不替代Start授权/编号管理，也未用真实MCP host确认UI验收。Windows Qt 5.14.2测试仅覆盖legacy Sentry/GUI，Native Run物理双机、macOS、独立Ubuntu24与CoBo现场仍未验收。

在隔离副本上检查候选的命令形式如下。候选路径必须尚不存在，并且OLD、NEW和候选父目录必须位于同一文件系统：

```sh
getdaq-catalog --prepare-catalog-replacement /archive/old/run_0042/node \
  /archive/new/run_0042/node /archive/staging/run_0042-node-candidate
getdaq-catalog --inspect-catalog /archive/staging/run_0042-node-candidate
```

准备命令只增加硬链接和候选元数据，不截断、重命名或删除输入文件。单机发布命令将旧catalog目录整体保留为backup，不单独清理raw；Native多节点流程只有全部Publish receipt通过后才运行Finalize清除精确同名旧链接。若生产替换在Publish或Finalize跨节点执行期间失败/超时，父Run会保持未决/需对账状态；先按原operation检查父审计、节点journal、正式catalog和backup，不要自动递归删除或改用新operation ID重试。

正式`ReceiverCatalog`仍将新session归档到原目标路径，要求目标不存在；普通schema 1 catalog source的current file set必须精确对应本次Stopped session seal。单机发布API可交换catalog目录，但未接入正式Run生命周期。

Native接收session使用独立目录，catalog里的raw是指向session文件的硬链接。Finalizer按旧catalog receipt和替换计划核对source、完整文件名及文件身份，再删除精确同名旧链接；不同名旧raw仍从旧session/catalog backup保留。新session始终独立接收，不会直接截断旧session文件；旧`std::ios::trunc`行为不能代表Native跨session覆盖机制。

## 目标事务顺序

1. **Start前校验与确认。** UI读出目标Run是否存在并展示experiment、Run、deployment和覆盖范围。只有完整、身份匹配且状态可检查的已完成Run可以进入替换。用户取消、Run不完整、身份冲突、符号链接或活动receiver锁均拒绝。此步骤不得删/改raw。
2. **持久化统一操作身份。** Start请求前，本机Run journal记录唯一operation ID、旧Run/catalog摘要、节点清单和替换阶段。ECC为所有固定数据节点建立同一父操作绑定；每个节点的子操作ID必须稳定派生，超时后只能观察或恢复同一操作，不能生成新ID重试。
3. **按新session采集。** 新raw写入新的唯一session目录。旧catalog和旧session仍完整保留。若Start或采集失败，旧Run仍可用，未完成的新session按现有失败恢复流程处理，Run号不递增。
4. **验证并准备新catalog。** 所有节点完成真实EOF/ICE结束、receiver正常退出、seal和raw身份校验后，各节点先构建独立的新catalog候选。候选必须绑定本次session、部署/source映射和operation ID；任何节点未准备完成都不能进入发布/旧raw清理阶段。
5. **构建保留集。** 对每个source分别读取旧完整catalog与新完整session的文件名集合。新文件名集合优先；旧集合中没有新同名文件的raw以硬链接方式纳入新Run归档，并记录其来源catalog/session、设备号、inode及大小。旧集合中同名raw不纳入新Run。source不同即视为不同命名空间，不能跨source误判碰撞。
6. **发布新catalog。** 在同一catalog父目录中完成候选与旧catalog的可恢复切换，先保证新完整catalog可独立校验，再将新catalog发布为Run的正式目标。保留旧catalog备份和操作意图，直至所有节点都返回并经父ECC验证新catalog receipt。
7. **按精确名字清理旧冲突项。** ECC确认全部节点的新catalog receipt后才发送Finalize；节点按source、完整文件名及文件身份核对后，从旧catalog和旧session中移除同名旧raw链接。保留非冲突旧raw及session审计元数据；清理进度逐文件持久化并按原replacement ID幂等恢复。任何身份、inode或路径校验不符时停止清理并保留旧副本。
8. **完成并递增。** 所有节点的精确同名旧raw清理完成、保留集可再次独立验证、本机Run journal写入Complete后，才删除可安全清理的旧catalog元数据/事务临时目录并递增Run号。用户配置可写时持久化下一个编号；任何未决节点都保持旧Run号并显示需要恢复。

多节点文件系统之间没有全局原子rename。这里的安全性质来自“所有新catalog先完整发布、后清理同名旧raw”、每节点持久化意图和回执、父ECC按节点持久化进度，以及未知状态时保留旧raw。系统不得声称跨主机整体回滚；恢复报告必须区分已发布、已清理、未开始和结果未知的节点。

## 归档格式与兼容要求

schema 1 manifest把source文件清单与单一session seal严格绑定。新schema 2候选已将`files`（当前session）与`retained_files`（旧catalog保留项）分开，并记录旧文件的source ID、原catalog路径/receipt摘要、source目录及文件身份。它还在manifest记录`publication_root`、`publication_target`和operation ID派生的`publication_backup`，completion receipt中的directory固定指向publication target。`InspectReceiverCatalog`按新request验证current seal与current集合，并按当前检查路径实际`lstat`核对所有current/retained硬链接的名字、device/inode/大小及总集合；允许schema 2 candidate位于暂存目录或正式target，但必须与声明的正式Run身份相符。旧schema 1仍要求receipt path与当前检查路径相同。当前检查器校验retained来源凭据字段和摘要格式，但不会递归打开来源catalog重建整条receipt lineage；该来源链进一步验证和格式演进仍是后续工作。候选准备不修改输入，所以扫描绑定仍可引用旧catalog。正式发布/修订时仍须保留旧catalog快照，不能原地改写已有绑定引用对象。

正式Run catalog的逻辑文件集合应只对同一source保留一份同名raw；同名采用新文件，非同名旧raw保留。用户要求“同名才覆盖”，因此当前阶段不得主动清理其他旧raw；生产payload清理仍需单独实现并验证后，才可声称物理覆盖完成。实现不得靠复制raw构建修订，也不得在旧catalog回执尚未保存时破坏其目录。

## 分阶段实现与验收门槛

1. **判定器和准备API已实现。** `PlanReceiverCatalogReplacement()`先验证完整输入catalog并在返回前重验快照；每个获选文件带来源catalog/source目录、completion receipt摘要和大小/device/inode。`PrepareReceiverCatalogReplacement()`在独立候选中硬链接逻辑合并集，schema 2凭据绑定未来正式路径。
2. **新schema及检查器已完成首版。** schema 2将current文件与retained旧文件分开记录来源；检查器验证current seal与新Run绑定、每个硬链接文件的实际身份/文件集合、retained来源字段和完成凭据。连续合并时仍按新catalog中current session与retained集合区分，两次候选fixture验证通过。旧schema 1仍由原路径检查。仍需增加source映射变化、来源receipt链及跨平台fixture验证。
3. **单节点Prepare/Publish/Finalize及同名清理已接入Native Run流程。** ECC先对所有固定数据节点Prepare并核验，再Publish；所有Publish receipt成功后才Finalize。Finalizer按同一source和完整文件名只删除旧catalog及旧session中的同名旧raw链接，保留其他旧raw；清理有持久化意图，可按同一replacement身份幂等续作。跨节点不是全局原子事务，进程重启后的cold recovery尚不支持。
4. **跨节点失败恢复已有限度接通。** Prepare失败不会创建Publish意图；Publish首个失败或未知结果后停止后续节点；Finalize失败后同样停止。显式恢复逐节点观察原Publish操作，所有节点已发布时不重复Publish，只对Finalize续作；若Publish未全部完成则先安全恢复Publish，再Finalize。父级审计每条JSONL记录严格保持单行。ECC服务正向Publish失败恢复已有真实本机HTTP loopback集成；Finalize失败恢复和6项相关CTest覆盖同机双Supervisor，不是物理双机。cold recovery、真实MCP host正向端到端、GUI恢复入口及Run完成后号码推进完整串联仍需继续审查。
5. 继续验证实际崩溃/断电恢复边界、Windows Qt 5.14.2和macOS当前源码构建、独立Ubuntu24及目标网络现场；最后安排受控物理双机/CoBo验收。所有合成raw始终低于20 GiB并在测试后删除。

在以上各层通过之前，Native已有Run仍应拒绝替换并保留旧catalog。legacy Sentry现有的同名替换实现不能作为Native事务已完成的证据。
