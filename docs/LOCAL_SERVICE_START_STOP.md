# 本机启动与关闭

目标机目录：`/home/user/DAQ/GetDAQ`。

## 打开主界面

在 Ubuntu 桌面终端运行 `./start-getdaq.sh`。启动器显示 PID 和日志路径；启动失败会给出错误。`site/gui-runtime.txt` 指定当前 GUI 运行包。

启动锁仅在启动器中短暂持有，不传给 GUI 或后台服务。旧启动器将锁继承给 ECC，导致窗口退出后新启动器仍因锁占用静默退出；新启动器使用独立的启动锁文件，兼容仍持有旧锁的服务。

## 在服务窗口关闭

选择 ECC 或监管服务及其配置文件，点击 **Stop selected service**。程序按当前用户、安装目录、实际可执行文件名和 `--config` 路径查找进程，支持同一安装中的旧版本服务，不要求 PID 文件。发信号前再次核对进程启动时间，避免 PID 被复用后误杀。先发送 TERM，五秒后仍在运行才询问是否强制结束；不会自动强杀。

## 从终端关闭后台服务

```bash
cd /home/user/DAQ/GetDAQ
./stop-getdaq-services.sh all --list  # 只列出
./stop-getdaq-services.sh ecc         # 关闭此安装中的 ECC
./stop-getdaq-services.sh data        # 关闭此安装中的监管及 receiver
./stop-getdaq-services.sh all         # 关闭上述全部后台服务
```

脚本先列出范围并要求确认；正常结束后等待五秒。若未退出，明确使用 `./stop-getdaq-services.sh all --force` 才允许 KILL。范围限定为当前用户、该安装目录内的实际可执行文件和确定的服务名；不会按命令行子串广泛匹配，也不会关闭 GetDAQ 主窗口。

正常结束采集先在主界面执行 **Run stop**。关闭进程不等于关闭板端发送、完成 Run 或解除 UNKNOWN；强制结束也不保证数据完整。已有数据和审计文件会保留。
