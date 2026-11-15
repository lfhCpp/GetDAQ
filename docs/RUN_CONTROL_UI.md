# Run Control 界面与可调整项

主窗口继续采用 C++11 / Qt Widgets，由 [mainwindow.ui](../apps/envoy/mainwindow.ui) 生成。Run setup 合并原配置及 Run 细节；操作顺序为 Run start、Run stop、Connect、Disconnect、Regress all、Progress all、Retry finalization、Recover journal。宽度足够时单行，较窄时四列两行；后端操作、确认框及启用条件沿用既有实现。

右上参数表与左侧设置/控制区共用上部水平 splitter。系统总览位于左侧操作区下方、完整节点表上方，利用参数面板旁的空间。下方节点表保持全宽；其下方速率曲线在左、Event log在右，默认各占一半，可拖动水平 splitter 调整左右比例，竖向 splitter 继续调整节点区与下方区域高度。1536×1024 默认展示全部 11 项参数和 12 个节点；1280×800 下允许参数或节点表局部滚动，操作按钮、曲线和日志仍可见。

Automatic Run 与 Run setup 标题在顶部对齐，Apply 与 Enable 同行，进度文字独占整行换行；Experiment 行保持紧凑，左右两组内容在 Run setup 可用高度内垂直居中，平衡上下留白。Enable 在采集中可即时开关已保存策略，关闭不停止当前采集；其余条件和 Apply 在 Run/序列期间锁定。窄窗口通过同区标题右侧按钮打开设置。Time / File size 独立选择，两者同时选用 OR 触发；可停止或收尾后续开 Run。续跑至少等待成功收尾后的3秒，并要求新鲜 Ready 状态。Event log 警告橙色、错误红色，原文字及导出不变。完整规则和 Native 服务更新要求见 [Automatic Run](AUTOMATIC_RUN.md)。

Tools → Waveform viewer… 打开共享离线波形窗口；Data file tools… 提供检查、合并、矩阵转换和可选FITS/ROOT工具。Run期间入口禁用且文字灰显；全部离线窗口关闭、索引线程和受管进程退出前，GUI和控制线程均拒绝普通/扫描点Run启动。独立程序保留，外部Run观察与范围见 [波形查看器](WAVEFORM_VIEWER.md#控制台-tools-入口)。Elapsed显示时、分、秒，超过24小时不回卷。

## 视觉与尺寸入口

| 调整内容 | 入口 |
| --- | --- |
| 结构、控件名称、原信号槽绑定 | [mainwindow.ui](../apps/envoy/mainwindow.ui) |
| 主界面/菜单颜色、字体、边框、按钮状态 | [consolepresentation.cpp](../apps/envoy/consolepresentation.cpp) 的 `StyleSheet` / `MenuStyleSheet` |
| 节点/参数行高、灯列宽度与灯色 | [consolepresentation.h](../apps/envoy/consolepresentation.h) 常量和 `HealthColor` |
| 矢量操作图标及禁用态 | `consolepresentation.cpp` 的 `Icon`，使用 Qt Painter，不新增外部图标依赖 |
| 面板边距、按钮高度、参数宽度范围、响应式排布 | [mainwindow_console.cpp](../apps/envoy/mainwindow_console.cpp) 的 `InitializeConsole` / `ArrangeConsole` |
| 节点表列宽、参数列宽与内容高度 | [mainwindow.cpp](../apps/envoy/mainwindow.cpp) 的 `SetConfig` / `ResizeStatusTableToContents` |
| 曲线颜色、图例、网格、坐标及标题 | [widgetplot2d.cpp](../apps/envoy/widgetplot2d.cpp) 的 `paintEvent` |

第二轮精修统一正文13 px、分区标题15 px粗体、主标题22 px；普通操作按钮半粗体，表格12 px、24 px行高。YAML按钮36 px，采集按钮38 px，Experiment / Run / Description输入框36 px；Run号14 px半粗体，步进箭头和节点选择箭头使用随程序编译的XPM资源，避免平台默认子控件与QSS混用后消失。日志使用系统等宽字体和12 px文字；边框、浅灰禁用态与3–4 px小圆角统一。

采集面板按实际布局内容计算高度，小窗口缩小面板边距；窗口resize后再核对一次子控件宽度，避免单行换两行时裁切。节点表按配置行数设置高度上限，重设配置时同步更新竖向splitter分配；减少节点后下方面板立即上移，仍可手动缩小节点区并滚动查看。Regress / 节点选择 / Progress连续排列，顺序提示使用剩余宽度。宽屏Run setup吸收为右侧完整参数表保留的少量高度差；1280×800允许局部滚动，不强行同时塞入全部字段和节点。

QSS 主要限定在 `centralwidget`，菜单独立应用样式，不把新主界面配色强加给维护、恢复等对话框。生产入口启用 Qt 高 DPI 和高 DPI 图标；动作图标以矢量路径绘制到带设备比例的 pixmap，步进/选择箭头资源在 `consoleicons.qrc` 中。跨平台字体仍使用系统字体。

## 状态显示口径

- 顶部保留独立的模式、阶段标签及真实计时，不放独立灯点。
- 节点表增加最左侧 28 px 灯列。底层仍使用原逻辑列索引，只移动表头视觉顺序，原 Node ID 与控制目标绑定不变。灯有实际状态 tooltip、可访问文本，ECC / Sentry 等文字字段继续保留。
- 灯色：无观察/离线为灰，已报告故障为红，有效过渡为黄，已取得所需观察且正常为绿。Native 逐板 journal 投影的 UNKNOWN 或 transitioning 也参与显示，不把健康的父级 ECC 状态冒充该板健康。
- `N CoBos ready` 仅计算固定配置的 CoBo，要求新鲜 ECC Ready（状态4）、稳定转移且没有硬件 UNKNOWN；MuTaNT 不计入。Running/Paused 是其他明确状态，不计作 Ready。
- 吞吐率为全部配置 CoBo 的 Sentry MB/s 之和；任一缺失或数值无效则未知。不再显示 `Disk max` 总览，节点表原有磁盘字段继续保留。
- 错误摘要统计有控制、Sentry、receiver、source 或逐板 UNKNOWN 故障的配置节点，包括 MuTaNT；缺少观察时不会显示 `No errors`。
- Native 当前状态协议没有同口径实时 MB/s 指标，速率继续显示未知，并保留原本 Native 模式不显示该 Sentry 速率曲线的行为；不会把累计字节或示例数值冒充速率。

## 数据与交互边界

按钮原有名称/对象名和操作槽保持；表格及新增节点选择框共用同一配置行选择，最终目标仍来自 Node 项原 ID。长 Process / Data path / Error 保留原完整 tooltip。Run 元数据连接后仍可编辑、Run 活动后锁定，状态切换由原控制层决定。

曲线只重构绘制：沿用原 500 ms 显示节流、每节点最多600点、最后300秒可视范围及已有 MB/s 数据，没有修改采样、定时器、后台通信或历史存储。日志继续使用真实消息/时间戳、原自动滚动和2000行窗口上限；连接会话自动留存，Tools → Logs 的原导出入口仍在。

回归入口为 `tests/tests.pro`。新增测试覆盖真实 Simulation 生命周期、两种尺寸下按钮无重叠、宽屏完整节点、节点选择双向绑定、动态总览与未知数据，以及独立灯列的位置。第二轮增加12→2→1节点动态收缩、上下/左右splitter调整、按钮在父面板内完整可见、单节点操作栏间隔和输入框高度检查。视觉产物和最终结果见 [验证记录](VALIDATION.md)。这些检查不代替 Windows/macOS、现场显示器或真实 DAQ 硬件验收。
