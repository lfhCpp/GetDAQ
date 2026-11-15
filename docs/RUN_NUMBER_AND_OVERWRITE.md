# Run编号、回退与覆盖

本文说明GetDAQ的Run编号语义、用户确认的同名raw覆盖范围及各运行模式的实现状态。覆盖仅能由操作员在Start时明确确认；默认Sentry `/catalog`仍拒绝覆盖已有Run，显式替换有单独的operation ID。

## 编号规则

Run编号在操作台的Run字段中配置。操作员可手动把自动递增后的编号改回之前的值；目前没有单独的“回退”按钮。旧SOAP/模拟路径仅在本地Run文件写入成功并将journal置为`Complete`后递增；显式覆盖时，还必须完成所有legacy Sentry主机的同名raw替换/保留收尾和本机旧Run记录更新。Native路径在Run记录和journal成功完成、接收器退出与seal核验成功、且启用Native catalog时catalog核验成功后递增；用户配置文件可写且不是当前Run的`run.yml`快照时，会自动保存下一个编号。中断、超时、未知结果和未完成归档不得递增。

回退本身只选择目标编号，不能因此推定目标数据可以删除。覆盖必须由单独的明确操作授权。

**DataRouter和DataExporter都不负责Run编号或Run覆盖。** 旧`dataRouter`进程启动后直接调用`DataRouter::runStart()`；现场使用的DataExporter改进版仍继承`FrameStorage`并在存盘后增加TCP帧导出，其`runConfig()`仍为空，不把GUI Run number交给接收/文件层。旧`FrameStorage`按CoBo/AsAd、时间戳和文件序号生成文件名，并用`std::ios::trunc`打开，只有名字意外重合才可能截断同名文件；这不是经过Run身份核验的覆盖流程，不能依赖它保护或清理旧Run。Run归档与接收是不同阶段，替换只能由操作员点击Start并明确确认后进入归档/catalog事务。

## 覆盖对象与规则

Run编号可以在断开控制台后手动调回。回退编号不会删除任何原始文件；旧Run已存在时，只有操作者在点击Start后确认，才进入替换流程。未确认、旧Run不完整、身份不匹配、文件类型异常或接收目录仍有活动写入时均拒绝。

用户选择的 raw 语义是**只按完全相同的文件名覆盖**：

- 新Run产生的文件名与旧Run某个raw完全相同时，新raw胜出，旧的同名内容随旧catalog元数据清理。
- 文件名不同的旧raw保留在结果Run中。legacy Sentry通过同一数据卷内的rename迁移它们，不复制载荷，不产生额外的raw空间峰值。
- 不清理整个旧Run的raw集合，不因Run号回退而提前操作数据。
- 数据文件本身的接收/同名截断在原生接收器完成。OpenRawStorageFile()只允许截断安全的、未被占用的单硬链接普通文件；符号链接、FIFO、硬链接归档和活动写入均拒绝。

DataRouter和DataExporter都不负责Run编号。旧DataExporter基于FrameStorage按CoBo/AsAd、时间戳和序号生成文件名，std::ios::trunc仅在输出路径中恰好出现同名时截断；它不理解GetDAQ Run号，也不能清理Run目录。新接收器把数据写入受管session，因此Run级同名替换需要catalog层配合：legacy Sentry已实现，Native仍待实现。

## 当前存储布局

legacy Sentry会把incoming目录的.graw移入：

```text
<data_dir>/<experiment>/run_NNNN/
  <event>.graw
  catalog.json
  receiver-seal.json        # 仅在适用的接收路径存在
```

Native接收器使用独立session目录，Native catalog通过硬链接发布文件：

```text
receiver session: <outputRoot>/getdaq-<session>-source-<index>/
Native catalog:   <catalogRoot>/<experiment>/run_NNNN/<deployment>/
```

Native目录布局有意保留可验证的session/catalog关系。不能只删除Native catalog目录，因为原session链接仍占空间；也不能直接复用旧session来伪造本次Run。

## 覆盖时的处理顺序

1. 回退Run号只改变配置字段。开始Run之前不得改动旧raw。
2. 点击Start且操作者确认后，先校验旧Run journal、experiment/run身份和数据主机旧catalog；旧Run不完整、身份不符、活动接收锁、符号链接或额外异常文件都拒绝。
3. 新Run正常采集和封存后，数据主机生成新catalog。相同名字由新raw取代；旧catalog中没有同名新raw的文件迁入新catalog，使用同卷rename而非复制数据。
4. 每次迁移都把文件名写入新catalog的replacement进度字段。同一operation ID重试时，可识别已迁移文件并继续；已完成的新Run验证通过后才清理旧catalog元数据及同名旧文件。
5. 所有主机catalog和本机Run journal都完成后，才递增Run号。出现部分失败或结果未知时，保留operation ID和可恢复文件，不递增Run号。

这不是多主机文件系统原子事务。某个节点完成而另一个节点失败时，需要用同一operation ID恢复；不能声称所有节点已整体回滚。以上简化规则目前只在legacy Sentry路径实现。Native ReceiverCatalog与每Run独立session还不支持已有Run替换，不能简单删除旧catalog或复用旧session。

## 当前实现与待完成项

- Run spinbox可手动调号。legacy路径在Start时确认替换，RunControl固定operation ID；本机Run元数据目录仅在旧journal完整且身份匹配时备份替换。
- legacy Sentry的POST /catalog默认拒绝既有Run；显式替换要求replace_completed_run=true和operation ID。Finalize在新Run完成后将旧catalog中不同名的raw同卷迁入新catalog，保留新raw的同名胜出，并删除备份中的旧同名raw及旧catalog元数据。manifest记录迁移进度，Finalize重复调用及rename后回执落盘前的中断可用原operation ID恢复。
- RunControl等待各Sentry catalog成功和本机Run journal完成后才请求Finalize；清理回执写入journal，所有节点完成才递增Run号。跨节点不是原子提交，未决操作须保留journal并使用原operation ID恢复。
- Native目前仍拒绝已有Run/catalog替换。Native接收session是唯一目录，旧catalog使用硬链接关联session；满足同名替换、不同名raw保留仍需为Native catalog/session设计和实现配套逻辑。
- Native目标事务、catalog保留集格式、跨节点提交/恢复门槛和分阶段验收清单见[Native Run同名raw替换事务](NATIVE_RUN_REPLACEMENT.md)。该文档是目标规范，不表示实现已完成。
- Qt 5.12.8/GCC 9.3/C++11构建的完整QtTest目标中，RunRecord替换和Sentry catalog替换相关单元测试通过；HTTP端点在允许本机loopback后通过。回归覆盖同名新文件胜出、不同名旧raw保留、Finalize重复调用和rename后回执前恢复。未连接真实数据主机、ECC、CoBo或现场采集，Windows Qt 5.14.2/macOS及Native替换未验收。
- Run元数据目录会按Run编号重建；这里的raw保留/覆盖规则针对数据主机的.graw载荷，不会把旧Run记录或恢复journal带入新Run。

## 验收

每次改动后用隔离临时目录和小型合成文件验证：Start前调回Run号不改旧文件；未确认时旧文件哈希不变；同名新raw胜出、不同名旧raw保留；Finalize重试幂等；旧Run不完整/身份不符、符号链接、活动接收锁和未知journal状态拒绝；迁移中断后原operation ID可恢复。测试raw总量不得超过20GiB，结束必须删除并确认无残留。还需分别验证Qt GUI、legacy Sentry和Native路径；Sentry测试不能代替Native、双机、跨平台或现场验收。
