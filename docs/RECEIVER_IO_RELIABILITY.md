# 接收器写盘可靠性：真实ENOSPC、同步错误和封存门槛

本文对应 `apps/datareceiver` 当前错误处理和本机开发验证。真实采集错误的数据保留调查；只有测试自己生成的合成数据自动删除。原始路径继续标准C++11/POSIX，不增加Qt或其他运行库。

## 1. 本轮解决什么

已有单文件大小上限EFBIG验证，本轮增加两种实际内核ENOSPC，以及私有目录的错误注入：

| 检查 | 来源 | 可以证明 |
| --- | --- | --- |
| 空间耗尽 | 私有8MiB tmpfs实际用完数据块 | 实际write失败、前缀保存、退出1/无seal |
| inode耗尽 | 私有tmpfs仅12 inode，实际无法创建更多来源文件 | 实际open失败、此前完整帧保留、退出1/无seal |
| write零字节/EIO、fsync/close/rename错误 | 测试专用LD_PRELOAD拦截系统调用返回值 | 程序错误处理、诊断及不发布成功凭据 |
| 首次write返回EINTR | 测试拦截一次，之后调用实际write | 不丢/重复字节，重试后正常EOF/封存 |

tmpfs是受限内存文件系统，ENOSPC是内核实际产生，不是EFBIG或伪造errno；它不代表实际磁盘设备、SSD缓存或生产文件系统的全部行为。注入EIO/fsync错误也不能称为真实介质故障或断电测试。

## 2. 为什么不会填满你的数据盘

外层在自己的新证据目录中保存小日志、状态和SHA。内层用 `unshare --user --map-root-user --mount` 建立独立用户/挂载命名空间，只在自己新建的ramdisk位置挂载8MiB tmpfs。主机数据盘和其他进程看到的挂载不变。

每组最多发送12584192字节，两个场景顺序执行；每场景原始文件上限8MiB。测试先结束子进程，再核对/删除合成.graw、卸载tmpfs并删除挂载点。外层退出也关闭自有进程组，命名空间消失后私有挂载释放。

用户所有传输测试每轮全部流合计20GiB的上限继续保持。本工具不在真实数据盘、共享/dev/shm或已有Run中制造满盘，也不通过大文件轮转绕过预算。

这个方法要求Linux允许用户/挂载命名空间。不可用时应明确记录测试未执行，不把模拟故障改称真实ENOSPC；不要为了跑测试自行改系统安全设置。

## 3. 两种ENOSPC分别怎么验

空间测试发送12个1MiB合法CoBo帧，写入同一个文件。达到私有8MiB容量后，实际write报No space left on device。核对文件字节与独立生成输入的完整前缀一致，同时证明剩余数据块为0、inode还有空间。

inode测试发送20个不同Source编号的64字节通用MFM帧。tmpfs仅允许12 inode，根、锁和标记也占inode；无法再创建下一个来源文件时，实际open报ENOSPC。核对已有各来源文件是完整原始帧，证明inode剩余为0而数据块还有空间。

两者都要求最终退出码1、error明确非空、stream_sealed=false、最终seal不存在。实际EOF有时已到达，有时发送端因接收器错误停止读取而收到断链；EOF本身不能清除写盘失败。

失败前接受帧数或sealed_bytes可以非零，因为此前文件可能已写入/同步。正常完成必须同时看退出、错误、EOF、文件清单和seal，不能只看frames较大或文件存在。

## 4. 写入与同步诊断修复

原write循环遇到返回0时用strerror(errno)。若errno也为0，可能打印File write failed: Success，虽然失败退出，但现场容易误解。现在单独报告：

```text
File write returned zero bytes
Buffered file write returned zero bytes
```

EINTR仍重试，同一个缓冲区偏移不提前移动。负返回值保留原errno诊断，不把零写入当成功或无限循环。

文件和目录同步、seal同步/关闭/重命名现在保存失败的errno，避免后续close等调用覆盖它。seal各阶段提示明确区分：

```text
fsync failed: Input/output error
Directory fsync failed: Input/output error
Receiver seal fsync failed: Input/output error
Receiver seal close failed: Input/output error
Receiver seal rename failed: Operation not permitted
Receiver seal directory fsync failed: Input/output error
```

这些是测试示例，不表示所有现场错误都会是EIO。程序保留首个接收/写盘/封存错误，不自动重启或重放硬件动作。

## 5. seal在哪些情况下不能发布

```text
完整MFM → 写盘缓存排空 → 文件fsync/关闭 → 数据目录fsync
  → seal临时文件写完 → seal文件fsync/关闭 → rename最终seal
  → seal目录fsync → 正常退出
```

任一步失败就不能把采集当正常完成。seal临时文件可能留下，它不是最终凭据。rename后目录同步失败会尝试删除最终seal，并保持退出1/error；本机实际测试确认最终路径不存在。

这不是断电持久性保证：删除也可能无法持久化，存储设备可能有其他缓存。管理服务仍需要实际退出码和持久门槛，不能因为重启后看到某个seal就接管旧硬件或清UNKNOWN。真实部分数据不自动删除或截断。

## 6. 十项受控注入测试

测试共享库只在测试子进程的LD_PRELOAD中出现，根据明确私有输出目录和fd路径选择目标，不影响日志、stdout、其他目录或实际采集服务。产品CMake及运行依赖不包含它。

九项失败：缓冲零写、大帧直接零写、缓冲EIO、原始文件fsync、数据目录fsync、seal文件fsync、seal关闭、seal重命名、发布后目录fsync。各项核对退出1、指定错误、无最终seal，以及实际文件字节/完整SHA或应为空文件。

另一项首次write返回EINTR，再调用真正的内核write；要求正常退出0、完整字节/计数/EOF/seal。所有场景合计897024发送字节，每组不足1MiB，结束删除合成数据。插桩运行先加载ASan再加载测试库，系统库未整体插桩。

## 7. 开发复查

```sh
python3 tests/build_receiver_validation.py build-receiver
python3 tests/receiver_enospc_test.py \
  build-receiver/getdaq-receiver build-receiver/enospc-new-check
python3 tests/receiver_io_fault_test.py \
  build-receiver/getdaq-receiver build-receiver/io-fault-new-check
```

这是开发测试，需要Linux的unshare/mount/umount及C编译器。测试依赖不变成采集程序依赖；实际接收器仍只需原C++11/POSIX构建环境。不要把测试库加入真实服务的环境变量。

每组创建新证据目录，result.json绑定程序和脚本SHA；payload-cleanup.json保存删除前字节/哈希，挂载信息保存容量及失败时空间/inode计数。原始载荷已经删除，复查SHA凭据不等于重新读取原文件。

## 8. 验证结果和现场处理

最后普通与ASan/UBSan/LeakSanitizer分别通过真实空间/inode两场景及十项注入，完整核心、原TCP/慢导出/错误尾帧/锁/真实普通sentry归档和真实EFBIG回归也通过。sentry和系统库未整体插桩。

本机Ubuntu20.04.6/GCC9.4，Linux用户命名空间；没有真机、真实设备EIO、生产文件系统fsync、断电、目标平台或持续多CoBo吞吐结论。

现场失败先保存会话、错误和文件清单，按控制流程明确停生产，再核对发送器、硬件溢出、接收文件和不完整Run。接收器失败不证明板端已停止。换新受控会话前要处理旧未知意图；不要恢复旧日志、编造seal或自动重发未知结果。

当前源码入口framestore.cpp、receiver.cpp和tests/receiver_enospc_test.py、receiver_io_fault_test.py、receiver_io_faults.c。证据日志results-enospc-current.txt、results-io-fault-current.txt及results-io-*-regression.txt。独立`build-receiver/validation-io-reliability/`保留同版来源/普通插桩程序/小型凭据，不含.graw；`tests/archive_receiver_io_validation.py`默认核对当前输入，`--historical-only`只证明归档。

此前EFBIG详见[文件上限故障](RECEIVER_WRITE_FAILURES.md)，一般操作见[数据服务](DATA_SERVICE.md)。真实可靠性仍须生产文件系统、物理网络、电子学计数、长期压力与断电验证。
