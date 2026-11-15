# Windows Qt 5.14.2：完整界面和网络控制检查

本页记录Windows Qt 5.14.2、MinGW 7.3 x64上的当前源码功能检查（早期历史验证见70.71）。构建与测试脚本可由WSL或Windows原生Python调用真正的Windows编译器和程序，无需升级Qt。Windows操作台只处理配置、命令、状态和Run记录，原始DAQ由Linux上的独立原生接收器处理。

**最新当前源码结果（71.105，2026-10-21）：** 用户Windows主机用Qt5.14.2/MinGW7.3从当前工作树重建并运行Widgets与ECC客户端QtTest：Widgets105通过、0失败、1项Unix专属锁测试跳过；ECC客户端63/63通过。917项构建输入经收据记录；两个测试进程退出码均为0，Windows私有TEMP已删除，raw为0。Windows七程序候选已绑定本轮QtTest收据；七程序构建和SDK-free帮助、offscreen窗口、原生窗口smoke及helper HTTP回归见[Qt5.14.2构建与交付页](WINDOWS_QT5142.md)。QtTest收据及限制见[验证记录71.105](VALIDATION.md#71105-windows-qt5142-当前源码-widgets-与-ecc-客户端-回归-2026-10-21)。这是用户当前Windows主机上的原生进程，不是独立干净机器、Linux ECC服务、macOS或硬件验收。

## 1. 已经实际验证什么

| 运行内容 | 结果与范围 |
| --- | --- |
| 完整TestGetDAQ | 71.25当前源码101通过、0失败、1跳过；正常Windows插件，Fusion风格与产品一致 |
| 完整TestEccSystemClient | 63通过、0失败、0跳过；真实Qt Network访问本机模拟HTTP服务 |
| 被跳过的一项 | 原有nativeReceiverArchiveGuard只验证Linux/macOS本地POSIX目录锁，Windows没有原生接收器支持；未新增跳过项 |
| GET头兼容修复 | 只在POST时设置Content-Type；原9条对应告警消失，既有GET/POST、认证、超时与未知结果检查仍通过 |

窗口确认、只读、配置编辑/迁移/恢复、运行控制与关闭过程都由实际Windows程序执行。它仍不是独立干净Windows机器上的人工操作、真实Linux服务地址的现场联通或真实板卡验收。

## 2. Qt Creator用户怎样检查

生产入口仍是根目录GetDAQ.pro。tests/tests.pro生成getdaq-tests.exe；tests/ecc_system_client.pro生成ecc-system-client-tests.exe。二者需要Qt Test模块，正常操作包不需要Qt Test DLL。

分别打开上述pro，选择Desktop Qt 5.14.2 MinGW 64位Kit，使用独立Release构建目录。保留完整源码及参考配置，测试用QFINDTESTDATA读取source中的例子；只复制测试exe不足以复现全部检查。

实际配方为`-std=gnu++11 -pedantic-errors`，沿用MinGW/qmake的C++11模式并把相应扩展诊断作为错误。原测试lambda的默认参数被该编译器拒绝，已改为显式传参；Qt5.14的QSKIP宏也触发空可变参数诊断，改用公开qSkip函数保持同一原有跳过原因和范围。断言未删减。

## 3. 从WSL或Windows构建完整测试

从源码根目录运行，output第一次必须不存在：

```bash
python3 tests/build_windows_qt_validation.py \
  --kit /mnt/d/Software/QT/5.14.2/mingw73_64 \
  --compiler /mnt/d/Software/QT/Tools/mingw730_64 \
  --output build-windows-functional-new
```

工具核对Qt版本、64位PE、产品/测试/参考配置摘要，调用实际qmake/moc/uic/MinGW make，再用匹配Kit部署运行库。保留Makefile、编译日志、DLL/插件摘要和build-receipt.json；status为complete才计构建成功，随后仍须运行。

Windows原生命令示例（按本机目录调整）：

```powershell
python tests/build_windows_qt_validation.py `
  --kit D:\Software\QT\5.14.2\mingw73_64 `
  --compiler D:\Software\QT\Tools\mingw730_64 `
  --output build-windows-functional

python tests/run_windows_qt_validation.py `
  --build-root build-windows-functional --only all
```

未封存且确属同一验证目录的失败构建可以显式`--resume`。独占目录锁拒绝脚本并发重建；改变编译参数时增加`--clean`，让make清理自己的对象。配方变化不能默默沿用旧对象，已封存目录保留原样。SDK文件不修改；缺库先看deploy.log，勿从其他版本拼凑DLL，部署规则见[Windows交付手册](WINDOWS_QT5142.md)。

## 4. 运行、临时目录和结果

当前源码的固定入口在测试源码树中，构建目录可以在验证后删除：

```bash
python3 tests/build_windows_qt_validation.py \
  --kit /mnt/d/Software/QT/5.14.2/mingw73_64 \
  --compiler /mnt/d/Software/QT/Tools/mingw730_64 \
  --output build-windows-qt5142-tests

python3 tests/run_windows_qt_validation.py \
  --build-root build-windows-qt5142-tests --only all
```

可将`--only`设为`widgets`或`client`单独执行。运行会短暂显示并自动关闭测试窗口，无需人工确认；GUI界面测试显式选Fusion，与产品main函数一致。GUIless ECC客户端不传`-style`，避免Qt5.14把该参数当作未知选项。

构建回执将Qt、MinGW、测试程序和所有输入摘要绑定。原生Windows runner会读取qmake Makefile的`QT_TESTCASE_BUILDDIR`，并要求它与回执中的测试构建目录一致，再启动程序；WSL分支继续检查Z盘映射和共同构建根。运行器把精简运行文件部署到Windows本机临时目录；若`Z:`已映射到WSL发行版，则保留既有映射并经其运行构建产物；若`Z:`空闲，则只在本轮临时映射期间使用它。运行环境PATH限制为测试运行库、Windows系统目录和匹配Qt插件，避免SDK PATH碰巧补库。每个进程最长240秒、Windows临时卷至少留1GiB、所有自有临时文件不超过512MiB，并监视`.graw`总量不超过20GiB；结束后保留小型结果/清理回执并删除Windows临时根。测试中断时先核对进程和本轮目录，再执行自有数据清理。

只处理本次自有目录，不删除真实Run。Qt测试多已自行删文件，采样峰值不是累计生成量或DAQ吞吐指标。所有测试流仍合计不超过用户20GiB限制；SIGKILL/断电后应先核对原进程和具体自有目录。

## 5. 为什么使用正常Windows插件

原Kit离屏插件缺默认字体；指定Windows系统字体后告警消失，但自动确认框仍在Qt内部崩溃。失败退出、调用栈和清理回执均保留，没有改写成通过。

相同程序在正常Windows插件及Fusion风格下完成全部101项和原有1项跳过。因此本轮Windows功能验证使用正常插件，不修改SDK DLL，也不宣称该Kit的offscreen确认框已经修复。71.32又以配置边界修复后的当前源码重建并运行完整套件，Widgets101通过/1项Unix锁测试跳过，ECC客户端63/63通过。Linux Qt离屏可用，不能据此推断Windows旧插件也可用。当前源码结果见[验证记录71.32](VALIDATION.md#7132-windows当前源码配置边界回归及运行候选2026-10-20)。

## 6. 两个平台的候选包

当前Windows操作端目录为`build-packages/getdaq-windows-qt5142-20261021-71-80-current-source-candidate`，内含六个当前源码操作程序、匹配Qt/MinGW运行库、示例配置与最新手册，验证回执也随包提供；manifest共268项。Widgets 103通过/1项Unix专属锁测试跳过，ECC客户端63/63通过；复制完整目录到Windows本地路径后运行`GetDAQ.exe`，先按[操作手册](OPERATIONS.md)检查默认模拟模式，再配置Linux服务地址和固定输入。

当前Ubuntu24交付候选为`build-packages/getdaq-ubuntu24-20261021-71-80-current-source-full-candidate`，包含当前源码full profile的27个程序、操作文档及验证摘要，176项manifest文件。Ubuntu24.04.5用户空间测试核心149/149、Widgets104/104、CTest22/22及receiver回归通过；用户空间运行在WSL宿主Linux内核，不能替代独立Ubuntu机器验收。

`20261019-functional-candidate`和`20261019-control-http-candidate`是较早历史包；保留它们在旧验证记录中的名称不表示它们包含70.82/70.83之后的源码或文档。不要将历史候选与上述当前源码候选混用。

升级先正常收尾、保留原Run和故障记录，再部署新目录；程序SHA绑定的固定计划需按变化重新生成和确认。新版本不授权清除硬件UNKNOWN，也不热覆盖正在运行的程序。

当前两个候选的清单复核命令分别为：

```bash
python3 tools/deploy-windows-from-wsl.py --verify \
  build-packages/getdaq-windows-qt5142-20261021-71-80-current-source-candidate
python3 tools/build-suite.py --verify-stage \
  build-packages/getdaq-ubuntu24-20261021-71-80-current-source-full-candidate
```

完整依据见[验证记录70.82](VALIDATION.md#7082-当前源码ubuntu24全量交付候选2026-10-20)与[验证记录71.08](VALIDATION.md#7108-include共享类信息双向补齐及当前源码回归2026-10-20)。项目其他缺口见[重构进度](REFACTOR_STATUS.md)；这些本机/候选包验证不能换算为总体完成百分比或现场验收。

## 7.2 当前源码 Qt5.14.2 功能回归（70.83）

2026-10-20按本页第3节从当前源码构建QtTest，再用第4节入口在用户Windows环境执行。结果：完整界面97通过、0失败、原有1项POSIX锁用例跳过；ECC HTTP客户端63通过、0失败、0跳过；两个测试进程退出码均为0。构建摘要、QtTest结果和临时数据清理回执见[验证记录70.83](VALIDATION.md#7083-当前源码windows-qt功能回归2026-10-20)。单项跳过仅因该接收器目录锁测试使用Windows当前未实现的POSIX本地目录锁；没有为取得通过新增跳过项。测试结束无残留`.graw`，Windows测试临时根已删除。

这证明当前测试二进制在这台Windows/Qt环境中完成了相应回归；不等价于独立干净Windows机器、人工逐项操作、真实Linux服务连接、真板运行或DAQ吞吐验收。

## 7.3 当前源码共享配置回归（71.08）

2026-10-20在同一Windows Qt 5.14.2/MinGW 7.3环境重建完整Widgets与ECC客户端测试程序并运行。Widgets 100通过、0失败，原有1项POSIX目录锁测试跳过；ECC客户端63通过、0失败、0跳过。新增的`configurationDisplaysImportedSharedMetadata`验证本地scalar没有unit/range时，查询仍能显示include同类成员提供的有效属性和来源；相关include/共享属性GUI用例全部通过。测试runner的20GiB raw上限未触发，结束时raw剩余0文件、0字节，Windows临时根已清理。

此外，六个产品操作端按该源码重建，部署到`build-packages/getdaq-windows-qt5142-20261020-classinfo-candidate`；265文件manifest（含验证回执）、SDK-free PATH下六个帮助入口、offscreen窗口及Windows原生窗口smoke通过。构建与QtTest日志的本地回执位置见[验证记录71.08](VALIDATION.md#7108-include共享类信息双向补齐及当前源码回归2026-10-20)。该证据不替代独立干净Windows机器、真实服务连接、板端或持续DAQ吞吐验收。

