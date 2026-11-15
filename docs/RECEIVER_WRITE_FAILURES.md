# 接收器写盘失败：文件上限、短写和封存门槛

本文说明2026-10-08完成的单文件大小限制故障验证。原生接收器保留失败文件并报告错误；测试工具只删除自己生成的合成文件。这里的“测试后删除”不能用于清理真实异常Run。

## 1. 修复了什么问题

Linux对进程设置`RLIMIT_FSIZE`后，普通文件写到上限会触发`SIGXFSZ`。默认动作直接终止进程，可能导致应用来不及输出最终JSON错误状态。此前接收器在实际2KiB文件限制下由该信号终止，退出码为`-25`，没有成功封存。

接收器现在忽略`SIGXFSZ`的默认终止动作，使对应`write`返回`EFBIG`。已有的完整写入循环继续处理短写与`EINTR`，遇到错误则进入原有失败路径：停止接收、报告`File too large`、退出码1，`stream_sealed:false`，不创建成功seal。这不提高或解除系统文件上限，也不会把失败写入当成成功。

代码入口为`apps/datareceiver/main.cpp`；实际写入与剩余偏移处理在`framestore.cpp`；错误状态和封存门槛在`receiver.cpp`。不新增产品运行依赖，不在正常数据路径加入额外内存副本。

## 2. 为什么文件里可能只有一部分帧

`write(fd, data, size)`不保证一次写完。操作系统可以先写入2048字节，再在后续调用返回错误。接收器必须保留此前已经写入的字节，并把整次采集标记为失败；不能因为文件存在就认为完整帧或Run已保存。

小帧先进入256KiB批量缓冲，随后统一写盘。因此`accepted_file_bytes`和`frames`表示保存模块已接受的数据量，不是每个字节已经持久到磁盘。`sealed_bytes`只有完整刷新/同步通过后才推进；最终完成还需要自然EOF、无部分尾帧、正常退出和成功seal。直接写的大帧如果失败，保存接受计数不会把尚未完整写入的整帧计为成功。

测试里EOF刷新失败的例子：三帧共12288字节已被接受，但文件实际上只有2048字节，`sealed_bytes`为0、错误非空、无成功seal。不能使用已接受计数替代文件完整性或持久性判断，也不能把这个部分文件送入正常Run归档。

## 3. 已验证的四种情况

`tests/receiver_file_limit_test.py`启动真正的原生接收器，在子进程执行前设置内核文件上限。它不填满文件系统，不使用真实板卡；每个合成原始文件最多4KiB，结束后自动删除，只留小型结果与日志。

| 情况 | 发送字节 | 单文件上限 | 实际文件字节 | 退出/封存 |
| --- | --- | --- | --- | --- |
| 小帧在自然EOF时刷新 | 12288 | 2048 | 2048 | 退出1，错误可见，无seal |
| 小帧达到批量阈值后短写 | 266240 | 4096 | 4096 | 退出1，错误可见，无seal |
| 768KiB大帧直接写短写 | 786432 | 4096 | 4096 | 退出1，错误可见，无seal |
| 一帧恰好等于上限 | 4096 | 4096 | 4096 | 退出0，计数与持久seal通过 |

三种失败均核对原始文件实际字节与发送数据前缀完全相同、错误状态和无成功凭据；恰好上限检查避免误将合法边界拒绝。普通接收器与ASan/UBSan/LeakSanitizer接收器各四项通过。旧归档程序另实际复现信号直接终止，作为修复前对照；不是旧版本通过产品验收。

范围为Ubuntu20.04.6/GCC9.4、同一物理主机、合成数据。`RLIMIT_FSIZE`产生的是`EFBIG`，**不能拿它声称已验证磁盘满的`ENOSPC`、设备`EIO`、`fsync`失败或断电**。这些仍需受控故障环境；用户要求当前工作区不填满磁盘，仍严格遵守每轮总量20GiB限制。

## 4. 现场遇到类似错误如何处理

1. 保存错误日志、原接收会话和文件清单，按既有控制流程明确停生产。没有确认的硬件动作保持UNKNOWN，不能因接收器退出就假设板端已停止。
2. 核对接收器退出码、最终状态与seal。文件存在或`frames`较大不构成完成；本错误不能进入正常完成Run。
3. 检查启动接收器的shell或服务管理器是否设置单文件上限、文件系统限制以及剩余空间。调整限制只影响以后运行，不能使旧部分文件自动变完整。
4. 用`getdaq-inspect`对原文件做结构检查，另核对硬件/发送计数。部分文件可能在帧中间结束；保留异常Run，不补造seal、不自动截断或覆盖。
5. 新运行使用新的受控会话与目录，明确处理之前的未完成意图和硬件门槛。产品没有在这种故障后自动重启、重发或清UNKNOWN。

本机开发测试工具生成的是可删除合成数据，测试结束会清理；真实Run由操作者保留和调查。两者不能混用。

## 5. 复现与证据

从项目根目录构建、运行：

```bash
python3 tests/build_receiver_validation.py build-receiver
python3 tests/receiver_file_limit_test.py build-receiver/getdaq-receiver \
  --evidence build-receiver/file-limit-new
```

测试一次只保留最多4KiB原始文件，实际发送总量约1.02MiB；四项结束全部删除。`result.json`保存程序/测试脚本SHA、每项实际前缀SHA、最终状态、退出码与清理声明；每项`payload-cleanup.json`保存删除字节。运行测试需要Linux/POSIX的`resource`接口，不是Windows GUI测试。

不可变证据位于`build-receiver/validation-file-limit/`，包含修复前程序/实际信号结果、修复后普通/插桩程序及构建凭据、源码与小型故障记录，排除原始载荷。性能阶段原`validation-performance/`保持历史字节；主程序已修改，复查那一包须使用历史模式。

```bash
python3 tests/archive_receiver_file_limit_validation.py
python3 tests/archive_receiver_file_limit_validation.py --historical-only
python3 tests/archive_receiver_performance_validation.py --historical-only
```

前两条分别检查当前源码与历史归档；历史模式不能证明后来修改的工作树仍被测试。载荷已经删除，归档中的SHA为删除前实际读取校验的记录，复查记录不等于重新读取已经删除的数据。

更多状态字段、目录锁和封存条件见[数据服务手册](DATA_SERVICE.md)，复制优化与性能数字见[性能说明](RECEIVER_PERFORMANCE.md)，剩余故障与平台验收见[验证记录](VALIDATION.md)。

后续已在独立8MiB tmpfs验证真实空间/inode ENOSPC，并增加私有测试库的零写、EIO、fsync/close/rename及EINTR检查，见[写盘可靠性手册](RECEIVER_IO_RELIABILITY.md)。tmpfs是真实内核错误但不是生产介质；注入EIO/同步失败不等于真实设备/断电验证。本页EFBIG归档现只作历史复核，原字节保持。
