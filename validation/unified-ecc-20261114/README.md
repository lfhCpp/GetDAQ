# 统一新旧ECC配置入口（2026-11-14）

Linux首版。Nodes / Paths和Master均进入统一窗口，Connect existing ECC只需名称/IP/端口；Create ECC / data services保留已有主从部署。底层共同使用RunConfig YAML，可选ecc_name是友好名称，不改变固定服务身份或板卡编号。协议字段自动维护，不要求用户把SOAP与原生协议手工写成一样。

本机完整设置回归33通过、0失败、1个可选外部项目测试跳过（full-regression.log）；覆盖两种SOAP命名空间识别/保存、取消等待、原生授权和身份一致/不一致、未授权失败、同机多实例及同安装GUI跨运行包复用原节点/仓库PID。截图connect-existing.png使用测试服务，无凭据。

目标10.26.183.239已在Ubuntu22.04/Qt5.15.3完整重建31程序，209包文件校验通过，选用runtime-linux-unified-ecc-v23，root/bin同步，旧运行包及当前连接配置保留，未关闭用户界面或停止既有ECC/Data。见deployment.json。

生产实现只读实测192.168.40.16:46003自动识别Ice并改用同机8083 SOAP，返回Idle，见auto-probe.log。目标新版真实HTTPS仓库/两个隔离节点/ECC/Data联调7项通过（integration.log）。不等于实物采集或跨物理主从采集验收。

外部受保护原生ECC首次需已有授权连接文件或节点配对，不能凭IP/端口伪造凭据/epoch/SHA；已有授权自动复用，身份不匹配不降级或接管。新连接默认不猜数据目标。现有旧ECC此前未返回实验配置ID、数据接收端口未确认，不将Idle视为已可采集。

入口和用法：docs/UNIFIED_ECC_SETUP.md、tests/existing_ecc_auto_probe.cpp、tests/test_hardwaresetup.cpp。
