# Run Setup：在界面配置板卡与数据接收地址

此入口用于 **GetDAQ、ECC、supervisor 和 receiver 都运行在同一台 Linux 电脑** 的新部署。它读取已审阅的实验参数与硬件兼容 profile，在界面填写网络地址，生成配置、凭据及绑定摘要，检查后启动本机控制服务，再把操作台配置回填。无需手写 ECC、supervisor、topology JSON 或抄写 SHA256。

需要使用包含 `Set up board and receiver addresses…` 按钮的新源码重新编译 GetDAQ。旧程序不会因换了示例配置而出现此功能。安装套件仍按 [统一构建](HOST_SUITE_BUILD.md) 的 full profile 完成，将整套程序放在同一 `bin` 目录。

Windows远程操作端继续使用原有 native ECC 连接设置；本入口不自动安装远程Linux服务。它当前生成 Ice 1.0 控制及 TCP 数据接收配置，使用板端已有描述（use-existing），不替换固件或描述。需要其他编码、数据协议、远程节点、描述创建或替换时，沿用相应专题流程。

同一电脑有独立控制/数据网卡时，启动脚本可设置 `GETDAQ_CONTROL_IPV4` 和 `GETDAQ_RECEIVER_IPV4`，分别预填板端回调地址与接收地址；这些是电脑端网卡，不是板卡IP。未设置时仍需在界面填写。

## 界面操作

1. 打开 **Run Setup → Nodes / Paths → Set up board and receiver addresses…**。
2. 选择现场实验 `.xcfg` 和配套的硬件兼容 `profile.json`。保留实验配置的全部include。profile声明板型、芯片型号、寄存器布局等，不能从IP推断，初次部署应先按 [实验工件说明](SYSTEM_EXPERIMENT_CONFIGURATION.md) 审阅。后续可复用这份已审阅文件。
3. 程序目录选择完整运行包的 `runtime-linux/bin`，其中必须有 `getdaq-ecc-server`、`getdaq-supervisor`、`getdaq-supervisor-query`、`getdaq-receiver`。部署目录使用全新的绝对路径；raw目录选择本机数据盘上的私有目录（运行用户所有、0700），不要选择已有Run或状态目录。
4. **Board callback IPv4** 填本机能被板卡回连的网卡地址。控制端口默认ECC 24000、supervisor 23100，回调起始端口默认24100，每块CoBo分配一个。按实验室网络开放实际端口。
5. 点击 **1. Read experiment boards**。界面从实验中读取真实实例ID、活动/停用状态以及MuTaNT主从角色。不要在界面用别名替换这些ID。
6. 逐行填写下表字段，为每块板选择匹配的硬件描述 `.xcfg` 或已编译 `.json`。选中行后点击 **Selected board service names…**，填写真实Node对象名、预期硬件节点名、LinkManager/AlignmentServer对象名及DAQ控制对象/端口。首次对象名使用明确占位值，未填写会拒绝生成，不能仅凭板卡IP猜测。
7. 核对旧控制程序已退出、采集和旧发送器已停止，再勾选独占控制/停采声明。该勾选是现场声明，不会自动停止旧系统，也不提供板端全局锁。
8. 点击 **2. Generate and check**。窗口自动编译实验和描述、生成本机接收计划、调用实际supervisor/ECC离线校验器，取得真实plan/snapshot/input摘要并保存 `envoy.yml`。
9. 检查成功后点击 **3. Start local services**。窗口重新核对文件及程序摘要、检查控制端口未占用，启动本次新服务并认证核对ECC和supervisor身份。它不终止或替换占用端口的旧服务。
10. 点击 **Use in Run Setup**，配置回填主窗口。随后 **Connect → Describe → Prepare → Configure → Start run → Stop run**，每步等待结果。Prepare/Configure会执行真实硬件操作，Start才开始采集。

## 每个IP填在哪里

假设本机数据网卡是 `192.168.10.20`，CoBo是 `192.168.10.60`：

| 界面字段 | 示例 | 指向哪里 |
| --- | --- | --- |
| Board IPv4 | `192.168.10.60` | CoBo/MuTaNT板卡 |
| Board port | 按板端实际部署，如46000 | 板端Node及Link/Alignment服务监听口 |
| Receiver IPv4 | `192.168.10.20` | 本机用于接收该来源数据的网卡 |
| Data port | 23001 | 板卡原始数据进入receiver的端口 |
| Status port | 23002 | 接收管理器观察receiver的状态端口 |
| Board callback IPv4 | `192.168.10.20` | 板卡告警反向连接的本机网卡 |
| DAQ control port（服务名称窗口） | 板端实际端口 | 板端DaqCtrlNode控制服务，允许与Board port不同 |

接收IP必须属于当前Linux电脑。不要把远端数据电脑的IP填进本机部署入口，也不要把数据端口当作ECC HTTP端口。停用CoBo仍需要实际板卡拓扑及硬件描述，但不分配接收来源。开启readout的MuTaNT会得到独立接收源和端口，并在操作台显示其接收状态。

## 独立启动已有服务

已经生成部署配置后，无需重新生成来启动程序。可打开 **Tools → Start independent ECC / data services…**，分别选择ECC或数据服务及其配置。启动后的服务独立运行，关闭窗口不会停止它们；独立receiver快捷启动也在该窗口提供，需与受管理模式区分。详见 [独立服务启动](LOCAL_SERVICES.md)。

## 保存、失败与重新打开

每次生成使用全新部署目录，保留实验工件、硬件描述、拓扑、服务配置、私有凭据、检查结果和 `envoy.yml`。生成失败时保留 `BUILD_INCOMPLETE`；目录不能作为完成部署运行。修正后选择另一个新部署目录重建，不覆盖原记录。

检查通过后改了界面字段，需要重新生成；启动前还会重新校验文件、程序及绑定。源实验和描述会编译为本次部署的固定副本，不在运行中追随外部文件修改。

服务日志在部署目录 `logs/`，启动PID在 `service-processes.json`。服务以独立进程运行，关闭配置窗口或操作台都不会替代停采或自动停止服务。某个服务启动失败时，已启动的本次服务会保留，先看日志及PID，不能直接另建部署接管。

下次启动GetDAQ，用 **Load configuration** 打开生成的 `envoy.yml` 即可恢复连接设置、节点绑定和持久client_epoch。服务重新打开已有审计目录时仍遵循后端核对规则，不能通过删目录清除UNKNOWN。首次真实Run的EOF、退出、封存与数据质量核对，参照 [原生操作台](ECC_GUI_CLIENT.md)。

## 已验证范围

本功能的专项测试使用已有实验样本、实际配置编译器和实际本机服务解析器，检查多板/停用板/MuTaNT数据源、非本机地址和端口冲突拒绝、新目录保护、凭据权限、窗口入口、生成配置、服务启动及认证身份。原始数据和硬件命令不经过该设置窗口。

本机回环和离屏GUI测试不能代表Ubuntu24独立电脑、Windows人工操作、真实板卡、校准或持续吞吐验收。首次上机仍先做小Run，核对帧、来源、字节数及波形后再扩展。

专项测试入口（Linux，`GETDAQ_SETUP_BIN` 指向本次完整运行包的bin）：

```bash
mkdir -p /tmp/getdaq-setup-check
cd /tmp/getdaq-setup-check
qmake /path/to/GetDAQ/tests/hardwaresetup.pro
make -j2
QT_QPA_PLATFORM=offscreen GETDAQ_SETUP_BIN=/path/to/runtime-linux/bin ./hardware-setup-tests
```

测试会启动并停止自己的本机ECC/supervisor，只执行离线检查和身份查询，不连接板卡，不执行Describe/Prepare/Start，不生成DAQ raw。
