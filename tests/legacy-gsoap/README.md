# 旧 ECC gSOAP 进程互操作探针

该探针从保留的 `source/get/GetBench-software-release-20210208/src/get/soap/ecc.hh` 实时生成绑定，用 gSOAP 2.8.124 编译一个只监听 `127.0.0.1` 的独立测试服务，再让当前 Qt `EccClient` 作为另一个进程连接它。它直接检验客户端请求、gSOAP 解码、旧服务命名空间和 gSOAP 响应序列化，不会启动真实 GetBench 服务或访问硬件。

从项目根目录运行：

```sh
python3 tools/verify-dependencies.py --profile full
python3 tools/run-legacy-gsoap-interop.py --qmake "$(command -v qmake)" \
  --full-qt-suite --evidence build-receiver/legacy-gsoap-interop.json
```

要求本机有 GCC/G++、GNU make、Qt 5 开发工具和 localhost TCP socket 权限。`--jobs` 可设为 1–8，默认 2。探针会在系统临时目录解开固定 gSOAP 源码、生成旧 ECC SOAP 绑定、构建测试服务及带专项测试的 `getdaq-tests`，运行后删除整棵临时目录。调试时可传入一个空目录 `--work-dir /tmp/getdaq-gsoap-debug` 保留中间文件；目录须为空，不要指定含用户文件的目录。整个探针只发送少量控制 XML，不创建 DAQ `.graw` 文件。

## 当前覆盖

- 当前 `EccClient` 与 gSOAP 生成服务对 `GetState`、`Describe`、`Prepare`、`Configure`、`Start`、`Stop`、`Pause`、`Resume`、`Breakup`、`Undo` 共10个界面控制操作的真实双进程请求/响应交互。测试服务记录并核对每个文本操作解码出的 `configID` 和 `table`。
- GetBench 旧命名空间 `urn:http://localhost:8083/` 与 SOAP 1.1 `SOAPAction: ""`。
- 服务端收到的每个文本操作 `configID`、`table` 内容，包括配置 ID、数据链路 XML和端口。
- 旧绑定实际生成的响应根名 `ResponseState` 和 `ResponseText`。

该服务对没有测试需求的 ECC 操作提供固定成功响应，因此它不验证完整旧 ECC 状态机、数据库操作、长期运行/异常边界，也不代表现场服务器的确切 gSOAP 版本。项目固定 2.8.124 是为了可离线重建测试工具；旧 `configure.ac` 只表明版本达到 2.8.13 后使用 `-z1`，不能据此断言部署版本就是 2.8.124。

## 许可和产品边界

gSOAP 发行包内含不同许可组件。`soapcpp2` 和生成源码在 GPLv2 范围内；gSOAP runtime 使用其包内说明的 gSOAP Public License。原文保存在 `third_party/licenses/gsoap-2.8.124/`。本探针及所有生成物只在独立的临时测试构建中使用，绝不链接进 GetDAQ、控制服务、GUI 或数据接收程序，也不复制到候选运行包。若以后要分发生成服务/代码，必须先独立确认许可兼容性。

完整Qt Widgets套件107/107通过，最终双进程回归覆盖10个操作（9个带参数）；收据与日志由`--evidence`保存。当前回归证据见 `docs/VALIDATION.md` 的“旧 ECC gSOAP 双进程互操作”。该证据不替代真实 GetBench 主机、板端或 DAQ 吞吐验收。
