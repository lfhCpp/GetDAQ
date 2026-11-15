# 在操作台查看 ECC 配置并绑定 Run 记录

## 1. 这个页面能回答什么

主窗口原生模式新增 **Accepted ECC configuration...**。它显示当前已核对实例在启动时接受的参数工件、固定输入、可选仓库选择及其摘要。状态栏简要显示选定版本和“hardware load unverified”；完整摘要与服务器清单路径在只读表格中查看。

三个结论必须分开：仓库登记表示完整包已保存；ECC接受选择表示这份包进入该实例的固定输入；板端实际加载核对需要另外的硬件/固件证据。当前页面明确显示 **Hardware loading: Not verified** 和 **Live file health: Not checked by this cached response**。

这个查询不会重新解析全部文件、读取寄存器、执行 Configure/Start 或搬运 `.graw`。原始MFM依然从板端直接进入独立原生receiver。界面与Run文件只记录配置/控制/封存元数据。

## 2. 手动查看

1. 按[ECC GUI手册](ECC_GUI_CLIENT.md)配置 `native_ecc`，核对服务/客户身份、固定输入与父快照，连接原生ECC。
2. 点击 **Accepted ECC configuration...**。首次使用发送认证GET `/v1/configuration`；已经接受的同一启动记录可以直接查看。
3. 查看ECC实例、父快照、固定输入、参数与工件摘要。有审阅选择时还显示仓库身份、发布版本、原收据、有效参数/来源快照和ECC主机上的清单路径。
4. 根据完整摘要核对该版本，不要仅凭状态栏显示的前12个字符作最终确认。

未使用新选择字段的旧部署显示 **No reviewed repository selection**，仍能展示启动工件；这不表示没有参数，也不自动生成仓库收据。断开后清除显示，避免旧部署信息继续被当作当前观察。

服务不支持接口或网络失败时显示不可用；此前已核对的同实例记录可以作为历史缓存展示。它不会因此变成当前文件健康证明。实例、父快照、固定输入、参数或工件不匹配，以及同一实例中的选择变化，会保持客户端故障；恢复原回复不会解除。

## 3. 让每个 Run 保存绑定的配置

为兼容原有部署，自动观察/强制记录开关默认关闭。确认服务器支持本接口后，在控制台配置中加入：

```yaml
getdaq:
  native_configuration_status: true
  native_ecc:
    # 原有十一项固定客户字段不变，按你的部署填写。
```

这是现有配置中的补充片段，不是完整的可运行文件。详细模板见 `config/envoy.native.example.yml`。

启用后，连接时自动观察固定实例的启动记录。开始新Run时，若尚未取得有效记录，不创建Run目录、不提交Start，而先请求观察；界面就绪后再次明确Start。已取得的记录在Start持久意图之前保存到Run journal。它标识本次Run使用的启动输入，不会把实际硬件加载标记改为true。

开关关闭时保持旧模式兼容；若已手动获取有效记录，也会将其保存到之后的新Run。只读配置查看不会产生硬件POST请求。

## 4. 保存了哪些文件

`journal.json` 的 `native_configuration` 保存已核对的原JSON回复，与已有 `native_identity`、`receiver_ready`、`native_operations` 一起绑定本次Run。正常停止后仍须等待Breakup、全部receiver自然EOF/正常退出/封存，才完成Run。

最终记录增加 `ecc-configuration.json`，保存该Run开始前绑定的记录；不会在收尾时改用另一份新配置或用本机 `.xcfg` 镜像代替。既有 `ecc-identity.json`、`receiver-status.json`，以及启用归档时的 `data-catalogs.json` 保持原职责。

原始数据目录仍在数据主机。配置记录不是原始数据的备份、全局实际加载凭据或端到端零丢失证明。

## 5. 本地 Run 恢复

已有恢复继续要求相同实例/父快照以及全部接收会话、PID和数据目录。启用本开关或原Run已有配置记录时，还要核对保存的启动配置和当前接受记录一致。

新连接尚未取得记录时，恢复先请求只读观察并提示重试，不重发Start。已保存选择发生变化会拒绝本地恢复；不能通过关闭开关、改摘要或手工替换JSON把不同部署拼进同一Run。

成功恢复仍只是本地Run关系重绑，保留 `duration_incomplete:true`，不解除硬件UNKNOWN或证明完整采集时长。

## 6. 协议与实现

共享 `EccControlConfiguration` 位于 `common/control/eccprotocol.*`，纯C++11、无Qt/Ice/POSIX后端。服务端编码、Qt客户端和Run恢复共用字段/类型/大小与选择绑定检查，避免各处自行解释JSON。

schema1仅接受 `input_binding_live_verified:false` 和 `hardware_loaded_verified:false`。如果未来实现真实加载证明，需要定义有证据的协议扩展；当前不能仅将布尔值改为true。客户端核对回复与已接受identity中的实例、父快照、固定输入、参数和工件，再保持同一实例中规范化后的启动记录不变。

`EccSystemClient::QueryConfiguration`只发送GET；`NativeRunControl`管理记录/恢复；`RunControl`跨控制线程传送界面信号；`MainWindow`只读显示。开关启用不授予控制权、不启用告警停机策略，也不改变板端原始数据传输路径。

## 7. 验证范围

协议检查覆盖字段/类型、预算、选择/工件绑定和伪造硬件证明拒绝。客户端检查身份前不读、固定绑定、选择变化保持及无POST；Run夹具检查Start前记录、缺记录不预占Run、最终文件和恢复拒绝。界面用例检查只读表格、未核对状态及断开清除。

实际原生ECC/Qt控制器/两数据服务/独立receiver的合成本机联调，另外核对记录、父快照和封存一致。它仍不是现场实际加载、全局独占控制权、物理多主机、目标平台或持续DAQ吞吐证明；普通与插桩覆盖和最终结果见[验证文档](VALIDATION.md)。
