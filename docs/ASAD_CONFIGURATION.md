# AsAd 配置、脉冲发生器与 CoBo Configure 参数

## 1. 本次增加了什么

CoBo 读出编译已经计算 AGET、触发、零抑制和循环缓冲区。本次再加入活动 AsAd 的 TCM、检查信号、脉冲发生器参数，以及 CoBo 的 2p 初始化脉冲。

纯计算位于 common/experiment/coboasadconfiguration.h/.cpp，仅使用 C++11 标准库。Qt Core 外层读取配置并记录来源；原生硬件库组合顺序计划。原始事件直接进入独立数据接收进程，AsAd 配置不会经过该数据流，也不会让接收器依赖 Qt 或 Ice。

**这还不是可以上线的完整 ECC 服务。** 此处实现的是 CoBo Configure 的参数和组合操作；完整 Prepare、串行校准、生产状态转换、控制权独占、启动恢复和 MuTaNT 协调仍须接入。JSON 当前供离线检查，没有硬件部署 CLI/GUI。普通运行继续使用旧 ECC。

## 2. 如何离线检查自己的配置

在项目根目录构建：

    bash tools/build-linux.sh control

    # 单独构建纯计算核心，不需要 Qt/Ice。
    bash tools/build-linux.sh experiment
    (cd build-experiment && ctest --output-on-failure)

命令格式：

    getdaq-config compile-cobo-configure INPUT.xcfg COBO_INSTANCE VERSIONS OUTPUT.json

使用提供的原始 pedestal 配置：

    ./build-suite/bin/getdaq-config compile-cobo-configure \
      source/get/GetBench-software-release-20210208/data/config/configure-pedestals.xcfg \
      0 '0=0x202,1=0x202,2=0x202,3=0x202' /tmp/cobo0-configure-new.json
    python3 -m json.tool /tmp/cobo0-configure-new.json

Windows Qt Creator 打开 GetDAQ.pro，使用自己的 Qt 5.14.2 MinGW Kit；编译后使用 getdaq-config.exe，把版本列表放在双引号内。Windows 平台尚未实际验收。

CoBo instance 是实验配置中的实例 ID。版本列表编号是 4*AsAd+AGET，必须恰好覆盖活动芯片；0x201/0x202 为 AGET，0x203 为 ASTRE。手工填写版本不表示实机 reg5 已核对。

输出文件必须不存在。返回 0 表示离线编译和写出成功，1 是配置/文件错误，2 是命令格式错误。失败不生成工件，不修改原始 xcfg，也不替换调用者已有的编译结果。

## 3. 工件怎么读

kind 为 getdaq.cobo-configure-image，schema_version 为 1，hardware_deployment_supported 为 false。

| 字段 | 含义 |
| --- | --- |
| readout | CoBo 读出子工件：AGET、零抑制、Module、缓冲区与来源 |
| asad_mask | 活动 AsAd 位图，必须与 readout 的位图相同 |
| asad_boards | 每个活动 AsAd 的 TCM 模式、延时及 ISPM 值 |
| pulser | 整数 mV、DAC 参考值、外触发/周期/双脉冲/Poisson 策略 |
| initialize_2p | 保留旧 Configure 的初始化动作，即使普通模式也执行 |
| initialize_2p_delay_ms | 高电平与低电平之间等待 20ms |
| parameters / sources | 本层查询路径、选中声明、代码缺省及源文件 SHA-256 |

寄存器值及电压/周期使用字符串表示。工件是可审阅的计算结果，不是通用脚本，不能传给 create-description。来源显示实际选中的声明；共享 class 和跨默认声明的完整元数据语义仍未迁移。

## 4. TCM 是什么

TCM 是 AsAd 的测试与校准管理器，控制测试信号进入 AGET 的方式和脉冲幅度。它不负责网络传输或文件存储。

| 配置 | 硬件字段 | 解释 |
| --- | --- | --- |
| AsAd/Generator/isCalibMode | TCM0.Calib，bit7 | false：电压阶跃直接到测试输入；true：经过校准电容产生电流脉冲 |
| AsAd/Generator/injectValue | TCM0.Rg，bit6 | 1pF → 0；11pF → 1；旧名字 10pF 同样表示 11pF |
| Generator/externalTriggerMode | TCM0.Trg，bit5 | false：写 TCM2 触发阶跃；true：AsAd ISP1 外部信号触发 |
| AsAd/Generator/triggerDelay | TCM0.Delay，bit0..4 | WCK 周期数，0..31；不是 ms 或 10ns |

isCalibMode/injectValue 优先使用该 AsAd 的声明，缺失时寻找 CoBo Generator 的旧全板声明。triggerDelay 缺省 0。未知电容名拒绝，不能把任意拼写默认为 1pF；旧误写 10nF 不作为新语义接受。

## 5. 电压为何要区分参考值与幅度

配置中的 mV 是相对 Vref=1102mV 的输出偏移。例如 defaultVoltage=0 表示输出约 1102mV；amplitudeStart=-1000 表示输出约 102mV。两者之差才是电压变化，不能把 -1000 解读为绝对负电压。

旧板端接口接收整数 mV。新编译先检查有限实数和 -1000..1000mV 范围，再向零截断：-12.9 → -12，12.9 → 12。不会四舍五入。即使截断后可能落入范围，1000.1 仍拒绝；NaN、无穷、错误单位和混入文本也拒绝。

14 位 DAC 保留旧 float 运算及整数截断：

    code = int((31.6 / 32) * 8192 * (1 - offset_mV / 1102))
    TCM1 = code 的 bit8..13
    TCM2 = code 的 bit0..7

参考值：偏移 0 → 8089，-1000 → 15430，+1000 → 748。它们是旧公式的代码值，不代替现场电压测量或器件校准。

amplitudeStart 优先位于 CoBo Generator；缺失时，旧无参函数读取 AsAd0/Generator/Amplitude[0]。新代码保留这个路径，**不是按每个 AsAd 分别读取 amplitude**，即使 AsAd0 不活动也可能作为旧参数来源。有新名字时不读取旧名字。

## 6. 周期、外触发、双脉冲和 Poisson

| CoBo Generator 参数 | 缺省与范围 |
| --- | --- |
| defaultVoltage | 0mV，-1000..1000 |
| amplitudeStart | 必须提供新声明或上述旧来源 |
| externalTriggerMode | false |
| periodicModeEnabled | false |
| period_ms | 1000ms；正的 32 位整数 |
| doublePulseEnabled | false |
| secondPulseAmplitude | 0mV；-1000..1000 |
| poissonModeEnabled | false |

周期启用时配置第一次幅度/周期、第二次幅度和 Poisson 开关，但 Configure **不启动定时器**。双脉冲关闭时向板端写第二幅度 0，以清除上一次配置；启用但第二幅度为 0 时，旧管理器也不会产生第二次脉冲。周期关闭时不调用周期设置，先前计时器仍在配置开头停止。

Poisson 表示板端计时器采用随机间隔，配置软件不承担精确脉冲时序。波形、极性和时序仍须现场测量。周期和外触发是否组合适合实验，应按硬件接线和板端行为确认。

旧 ECC 控制台的 `asad-pulser-start voltage period` 会立即覆盖幅度/周期并启动定时器；`asad-pulser-stop` 直接停定时器。新软件将这类周期输出纳入固定实验配置和采集生命周期：完整 Configure 写入参数但不启动定时器，采集 Start 在数据发送器启动后按 `periodicModeEnabled` 开启，正常 Stop/Breakup 停止。操作台没有运行外任意改周期的即时入口。

旧 `asad-pulser-trigger voltage` 是一次即时调用板端 `triggerPulser`。当前 `CoBoConfigure` 仍只在 `externalTriggerMode=true` 时，使用已编译的 `amplitudeStart` 触发一次；此外，健康 Prepared 且所有数据服务停止时，可在 `Selected CoBo maintenance...` 窗口为固定配置中的活动 CoBo 提交一次 `-1000..1000 mV` 单脉冲诊断命令。命令绑定原实例/快照、目标板和电压，默认取消确认、持久保存单次意图、不可自动重试；界面只报告硬件应答，不证明实际物理脉冲或波形。不要为手动诊断修改配置后重放 Configure。周期输出仍只由固定实验配置和 Start/Stop 生命周期管理，单次诊断不会覆盖其幅度或周期。

## 7. ISPM 检查信号怎么编码

ISPM 将两个内部信号接到 AsAd 检查线，方便示波器诊断；它不改变主机数据协议。普通设备编号在 bit4..7，ISP1 在 bit2..3，ISP2 在 bit0..1。Multiplicity 特例是 ISP1 占 bit3、ISP2 占 bit0..2，不能统一套用普通字段。

| 设备 | 编号 | ISP1 按编码顺序 | ISP2 按编码顺序 |
| --- | --- | --- | --- |
| ADC | 0 | CKS, SPI_CS, MOSI, CKR | SPI_CK, CAS, ACK, RCK |
| DAC | 1 | CKS, SPI_CS, MOSI, MISO | SPI_CK, CAS, ACS, RESET_FAST |
| TCM | 2 | CKS, SPI_CS, MOSI, MISO | SPI_CK, CAS, ACS, CKO |
| MONITOR / ID / IM | 3 / 4 / 5 | CKS, SPI_CS, MOSI, MISO | SPI_CK, CAS, ACS, IDLE |
| LCM | 6 | CKS, SPI_CS2, MOSI2, SPI_CK1 | SPI_CK2, CAS, SPI_CS1, MOSI1 |
| SCA0..3 | 7..10 | CKW, SCW_AGET, CKR, SCR_AGET | WCK, SCW, RCK, SCR |
| AGET0..3 | 11..14 | CKP, SC_CS, DIN, DOUT | SC_CK, CA编号, A编号C, LAST_INIT |
| Multiplicity | 15 | TRGint, CKO | CKO, CKW, SCW, TRGext, DISCT0..3 |

读取 AsAd/InspectionLines/deviceToInspect，再查询对应设备分组的 ISP1/ISP2；未知名字和信号拒绝。整个选择缺失时使用 ADC/SPI_CS/CAS，值为 0x05。已明确选择设备但缺少信号时拒绝，不沿用旧 try/catch 中意外将整组恢复成 ADC 的行为。

## 8. 原生组合流程与失败

CoBoConfigure(const CoBoConfigureSettings&) 是 C++11 库接口，位于 apps/hardware/coboconfigure.cpp。先编译全部参数，再由 HardwareClient::Execute 对整份计划检查远端结构、位宽、本地权限和管理器接口，之后才执行寄存器或脉冲动作。

1. 检查停止位，停止旧周期 pulser。
2. 清除 triggerMask，进入慢控制并核对全部 reg5。
3. 设置全部 AGET 配置和 FPGA 零抑制。
4. 每块活动 AsAd 设置前，保留旧三脉冲慢控制切换及 1000ms 等待；再设置 TCM、默认电压、外触发模式及 ISPM，明确读回。
5. 恢复 AsAd mask，保存外触发策略。
6. 设置 Module、缓冲区、触发及共用读出参数。
7. 保存周期策略，执行 2p 的 1 → 等待20ms → 0 脉冲。

控制层必须先绑定硬件描述。管理器在板端间接操作 triggerMask、TCM1/2，启动周期模式还访问 TCM0.Trg；新客户端检查这些目标的本地权限，不能用管理器 RPC 绕过只读策略。TCM/ISPM 和 DAC 数据有显式读回；DAC 只检查已知 6/8 位，ISPM 检查低 8 位。

aget_2p_initNow 是命令脉冲：保留 1、20ms、0，不强加未经确认的读回。Configure 不复位时间戳/事件号，不启动采集，也不读取原始事件。

本流程会改变寄存器和缓冲区指针；调用者必须已完成 Prepare/串行校准，停止采集及相关读取，并独占控制权。两个 firmwareEnable/intEnable 检查不能证明完整前提；当前尚无生产联锁或真实恢复检查。

读回不符立即停止；管理器动作后断线或远端异常记 unknown，不重发、回滚或执行后面的步骤。缺失旧管理器接口在预检查时拒绝；但旧固件即使声明同一接口，也可能缺少较新的具体方法，这只能在调用时发现，不能宣称每个方法已提前探测。此时保留日志并核对状态，不能忽略异常或把 ECC 改为 Ready。

现场 AsAdPulserMgr 保留在板端兼容边界；本次没有重建 getHwServer/IceEirfu，也没有把定时器移到 Qt GUI。控制动作和原始数据传输职责仍分开。

## 9. 验证与后续

纯核心、两份原始 GET 配置的 Qt/CLI、两种 Ice 编码及故障注入结果见 [验证记录](VALIDATION.md)。这些都是本机模拟，尚无实机、Ubuntu 24、Windows MinGW、macOS 或持续吞吐验收。

下一步接完整 Prepare、生产 ECC 状态/恢复与网络入口，再接 Start/Stop/周期 pulser 和独立数据进程生命周期。Configure 参数工件不能代替这些步骤。

依据为收到的 CoBoNode.cpp、CoBoConfigParser.cpp、AsAdPulserMgrI.cpp、asad/utl.cpp 及完整 CoBo 硬件描述；CEA GET 来源和 CeCILL 许可保留在 source/ 与 third_party/licenses/getbench/COPYING。
