新版统一入口见[新旧ECC简明配置](UNIFIED_ECC_SETUP.md)：连接已有ECC只填名称、IP、端口；创建新服务使用同一窗口的Create页面。

# 主控机 / 从控机：最少操作

Linux 首版，主控 / 从控窗口内全部使用英文。在 **Nodes / Paths 右边**只增加 **Master ▾ / Slave ▾** 一个英文角色按钮；Run setup、节点状态表、波形和原采集按钮布局不改。

## 只有一台电脑

1. 点 Nodes / Paths 右侧 **Master ▾**。
2. 首次设置中点 **Use this computer (automatic)**，软件自行启动并连接本机管理服务。
3. 点 **Add boards / ECC…**，填写 CoBo IP、控制和接收网卡 IP，核对端口与实际电子学设置。选择自己的实验文件，或适用的内置预设。
4. 点 **Start services and use this configuration**，软件自动保存、同步、校验并启动 ECC 和数据监管，然后回到原主界面。
5. 按原流程 Connect、Describe / Prepare / Configure，再 Run start。

首次设置完成后，平时只需选择 ECC 配置，点 **Start services and use this configuration**。不需要手工发布、复制摘要或写服务 JSON。

## 两台或多台电脑：先从控，再主控

各机先安装同一套 GetDAQ，包含节点代理和ECC/接收后台。下面不需要手写JSON、挂载共享目录或复制实验文件。

### 从控电脑做三步

1. 打开 GetDAQ，点 Nodes / Paths 右侧角色按钮，选择 **Slave**。
2. 设置两个本机目录：**Configuration cache (on this slave)** 是接收配置的缓存目录；**Data output (on this slave)** 是原始数据保存目录。默认值也可以使用。这不是选择主控上的 `.xcfg` 路径。目录在首次启用时保存；已建立节点保留原目录，避免改变已有实例绑定。
3. 点 **Enable / check slave**。出现在线、等待配对提示后，点 **Copy pairing code**。管理端口默认24260，多网卡时在 **Network and data storage settings (optional)** 确认主控能访问的本机IP。

从控此时只启用了节点管理服务，还没有开始采集，不必在从控再手动启动一遍ECC。

### 主控电脑接着做四步

1. 打开 GetDAQ，角色选 **Master**。展开 **First-time setup / edit hosts and boards**，在 **Hosts** 页粘贴连接码，点 **Add slave**（连接码15分钟内有效）。只用本机时直接点 **Use this computer (automatic)**。
2. 在 **ECC and boards** 页点 **Add boards / ECC…**。多主机时选择 **ECC host** 和 **Data receiver host**；选刚加入的从控，就会在它上面运行后台。只有一台执行主机时会跳过这一步选择。
3. 填 CoBo IP、控制网卡IP、接收网卡IP及端口。实验源文件只在主控选择：板卡窗口的 **Advanced: electronics template and service internals** 下可选已有实验模板和匹配profile，也可以复用适合实际电子学的已有预设。保存板卡组。
4. 点 **Start services and use this configuration**。软件自动保存、分发、校验缓存并在选定主机启动ECC/数据监管；成功后回到原主界面，点 **Connect**，再按原硬件准备/配置流程操作，最后 **Run start**。

主界面连接的是所选ECC实例的电脑IP和端口，不是节点管理端口24260，也不是CoBo板卡IP。管理地址、ECC地址、数据接收网卡可以不同；主控需要能访问从控管理/ECC地址，从控需要能访问主控配置服务（默认24250）。

### 配置如何统一

主控保存实验源文件与总配置，从控在自己选的缓存目录接收同一版本的文件，并自动生成本机路径。不要把主控的绝对文件路径手工填到从控，也不要在从控另外修改缓存文件。修改配置仍回主控操作；活动组换配置前先正常结束采集并停止该组。

## 如果操作长时间等待

窗口会显示当前步骤和已等待秒数。服务启动只做有总时限的就绪检查，不再重复叠加长网络超时；一次后台部署等待最长3分钟。

- 点 **Stop waiting**：停止本机等待和后续提交，保留已经受理的远端操作，不当成已停采，不自动重放。
- 等待时点关闭：先停止本机等待，完成退出后关闭窗口；独立后台继续保持原状态。
- 超时/停止后，点 **Refresh / check original operation** 核对原操作。一次配对未拿到结果时，原连接码会从私有记录恢复，避免丢失配对线索。
- 真正停止ECC/数据后台使用 **Stop selected group**，不能把停止等待当成停止服务。

如果仍不能连接，日志会包含具体IP、端口和失败步骤。先区分主控配置服务、从控管理服务、ECC服务三个端点，再检查相应网络；无需先连接CoBo才能启动这些后台服务。

## 配置与关闭

- 源配置只在主控选择和维护。从控保存运行所需的受管理缓存副本，不是零文件运行；只传配置，不把原始采集流绕经主控。
- 从控已启用且角色保存为从控时，再次打开 GetDAQ 会检查并恢复节点管理进程。切换角色或关闭界面不会自动停采，也不会终止独立后台。
- 主控窗口里的 **Stop selected group** 只停止所选 ECC / 数据监管，需已结束采集。关闭配置仓库与停止 ECC 是不同操作。
- 从控窗口显示代理在线和匹配进程，不把进程存在当作 ECC 已初始化或正在采集。
- 内置 pedestals 预设假设单 CoBo、硬件 v5、AGET 2.2、AsAd 槽 0。必须按实物核对；启动后台不自动写板卡。
- 旧 SOAP ECC 本身按配置名读取目录/可选数据库，不自动同步实验文件。这里的自动缓存由 GetDAQ 节点管理服务处理。

### 终止后无法编辑或删除板卡组

`terminated-needs-inspection` 表示后台进程已终止，尚未确认旧硬件/数据会话已释放，不等于正常停止。选择这组，点 **Review terminated group…**。只有在你已经确认板卡停止采集（或已复位）且原始数据已保留后，才确认继续。

软件会检查原启用操作不再进行、后台和receiver子进程均不存在，再保存独立的核对释放收据。状态变为 **Released after inspection**，原失败、UNKNOWN、Run和数据不删除，也不伪装成正常停止。随后可删除草稿组，或保存修正后的板卡设置，再点 **Start services and use this configuration** 发布、校验并启用新配置。

**Board group added to the project draft** 仅表示保存草稿；`Select an existing prepared group` 或 `edited recipe has not been activated` 表示尚未发布/启用所选版本。已有草稿不应直接当成正在运行的ECC选用。GUI和执行主机的node-agent都需要更新到包含核对入口的版本。

当前验证范围是 Linux/Qt 本机回环多进程，包括真实 ECC 和监管服务；物理多机网络、实板 Prepare/采集及持续吞吐仍需现场联调。未安装系统开机服务，未实现跨 ECC 同步 Run start。

## ECC 与 Data 的关系

ECC负责板卡命令、状态和采集协调；Data监管负责接收进程生命周期，receiver接收板卡数据并落盘。ECC不转发原始数据。它们可以放在同一台电脑，也可以分开放在不同电脑；一台电脑可以同时运行多套ECC与Data实例，共用一个节点管理进程，不需要另开多个主界面。

日常配置填写板卡IP、控制/接收IP、端口及数据目录即可。服务端口自动建议，按主机避开项目中已有实例和节点管理端口；删除组再新增也不会因组数变化建议重复端口。用户仍可改成现场需要的端口。只有一台执行主机时，添加板卡直接进入板卡设置，不再重复选择ECC和Data主机。未知的其他进程占用端口仍在实际启动时报告，软件不会按名字终止它们。

实验模板和实际电子学接线需要一致；已选模板会保存在主控项目中，日常不重复填写profile、硬件描述、凭据、摘要、缓存目录或内部服务JSON。主控/从控只决定配置管理角色，不限制本机可以运行多少ECC或Data实例。
