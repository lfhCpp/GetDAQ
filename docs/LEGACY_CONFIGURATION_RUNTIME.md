# 旧配置库运行对照与 XML 文本迁移

更新时间：2026-10-10。

这次实际构建并运行了收到的 CompoundConfig 文件/XML 实现，包括原 Spirit Classic 字符串和范围解析器、默认值回溯、共享类信息及本地 include。参考库源码没有改动；新软件仍使用自己的 C++11 配置模型，不链接旧 CompoundConfig、TinyXml 或 Boost。

这项工作补充了过去仅阅读旧源码或提取计算函数体的验证。它还没有验证全部旧库接口、二进制格式、数据库/网络扩展、全部编辑命令、在线加载或真实硬件，不能把一组对照测试当成整套重构完成。

## 1. 两个程序分别做什么

```text
同一份小型临时 XML / include 文件
             |
             +--> legacy-configuration-probe
             |       收到的 CCfg / TinyXml / Spirit 实际代码
             |       独立进程；原 int32 / uint32 域
             |
             +--> current-configuration-probe
                     生产 XcfgDocument / configurationrange
                     独立进程；C++11；Qt Core 仅用于 XML 外围
             |
             +--> Python 对照实际类型、值、单位、范围包含结果
                     区分一致、明确扩展、严格拒绝和共同拒绝
```

旧程序没有调用新算法来生成期望值。Python 只统一展示格式：旧类型名 `integer` 对应新 `int`，旧十进制展示的无符号值对应新 `0x...`，实数按实际 double 比较。范围表达式的空格、排序和重复项不要求字节相同；比较实际范围包含结果，并另外运行共享类导入兼容场景。

两边错误发生的阶段可以不同：旧库可能在装载时抛错，新模型可能先保留来源、在解析值或建立有效索引时拒绝。测试检查行为结果，不要求旧错误文本逐字相同。

## 2. 为什么要修正字符串空白

先前新模型对所有读取文本统一 `trimmed()`。实际旧库对照表明，收到的 TinyXml 默认先处理原 XML 中的普通空白，再解码字符引用；CDATA 保留文本。这三种写法不能混为一谈：

| XML 内容 | 实际字符串值 | 适用情形 |
| --- | --- | --- |
| `  a   b  ` | `a b` | 普通文本，首尾排版空白去掉，连续空格/Tab/换行折叠 |
| `&#32;&#32;a&#32;&#32;b&#10;c&#32;` | 两个开头空格、`a`、两个空格、`b`、换行、`c`、一个结尾空格 | 字符引用明确表达的空白，保留 |
| `<![CDATA[  a   b  ]]>` | `  a   b  ` | CDATA 中的首尾及连续空白，保留 |
| 不换行空格 U+00A0 | 保留该字符 | 不能用 Qt 广义 `isSpace()` 把它当成 XML 排版空白裁剪 |

例如以下范围要求精确字符串：

```xml
<Setup>
  <label type="string" range="{&quot;  a  b &quot;}">&#32;&#32;a&#32;&#32;b&#32;</label>
</Setup>
```

有效值为 `  a  b `，属于该枚举。把它裁剪成 `a  b` 后再检查范围，会产生错误判断。新版读取、校验和有效快照现在保留这些明确空白；原文件字节仍独立保存，不会因为读取而重排 XML。

这里的折叠只针对普通 XML 文本中的空格、Tab、CR、LF。不转换单位，不把字符串的引号或 `\n` 字面写法自动当成转义。范围内的带引号字符串有自己的 C 转义语法，详见[配置手册](CONFIGURATION_EDITOR.md)。

## 3. 在界面中怎样编辑

1. 打开本地配置，选择一个可编辑的普通参数声明。连接模式下仍然只读。
2. 字符串输入框显示解析后的实际值。普通 XML 中的多个排版空格可能已经显示成一个；CDATA/引用中的明确空白保留。
3. 输入需要保存的值。例如两端留空格、内部保留连续空格。
4. 保存时，新编辑器把空格、Tab、CR、LF编码为 `&#32;`、`&#9;`、`&#13;`、`&#10;`。`&`、`<`、`>`仍按 XML 转义。
5. 重新读取后，得到与输入一致的字符串。原 XML 中可能出现更多字符引用，这是保持实际值的写法，不是数据传输格式变化。
6. 已有具体实例的覆盖、新增参数及有效子树物化也使用相同编码。精确字符串范围检查不再先裁剪值。
7. 撤销继续恢复原文件的准确字节；重做恢复保存后的字节。来源守卫、外部文件变动拒绝、会话数量/字节预算均保持。

普通参数编辑仍拒绝整块替换带注释或 CDATA 的原内容，以免删掉用户排版和注释。可以用源文件编辑器调整后重新打开；不能以此绕过已有文件/范围/来源检查。各操作的审阅步骤见[结构编辑](CONFIGURATION_STRUCTURE_EDITOR.md)、[引用与导入](CONFIGURATION_INCLUDE_IMPORT.md)、[撤销与重做](CONFIGURATION_UNDO_REDO.md)。

## 4. 被标记为扩展的六个场景

| 场景 | 旧库实际结果 | 新版处理 |
| --- | --- | --- |
| 显式 `int` 的 `2147483648` | 超出旧 int32，装载拒绝 | int64 可表示；仍检查具体硬件字段范围 |
| 显式 `hex` 的 `100000000` | 超出旧 uint32，装载拒绝 | uint64 可表示 |
| 无类型 `2147483648` | 数字解析失败后成为字符串 | 推断为 int64 |
| 无类型 `0x100000000` | 旧 hex 溢出后成为字符串 | 推断为 uint64 hex |
| 无类型 `0XFF` | 原自动语法只识别小写 `0x`，成为字符串 | 推断为 hex |
| 显式 `bool` 的 `TRUE` | 旧显式 bool 转换要求小写，拒绝 | 接受大小写并规范为 `true` |

这些不是旧库兼容性通过。对于必须保持字符串意义的旧配置，使用明确 `type="string"`；对于必须保持旧数值域的硬件参数，应继续使用硬件编译器的位宽/范围检查，不凭宿主机整数宽度放行。

## 5. 更严格拒绝的八个场景

| 场景 | 旧运行结果 | 新版处理及原因 |
| --- | --- | --- |
| 显式 hex `-1` | 转成 `4294967295` | 拒绝负数，避免绕成无符号掩码 |
| 无类型 `1e309` | 产生无穷实数 | 拒绝非有限值 |
| 范围 `{1,2}tail` | 接受合法前缀并忽略尾部 | 拒绝未完整消费的表达式 |
| 范围 `[0,10]tail` | 同上 | 同上 |
| 范围 `{1,}` | 没形成有效范围，参数失去约束 | 明确拒绝非法分隔符 |
| 显式字符串 `a<!--c-->b` | 旧访问器最后只留下 `b` | 读取时保留旧值 `b`；原始多段标记设为只读，需显式整理 XML 后才能编辑 |
| 同对象下两个同名/同 ID 参数 | 查询取第一项 | 拒绝重复声明 |
| include 与本地同名/同 ID 参数碰撞 | 实际查询取 include 中第一项 | 拒绝冲突，要求明确整理原配置 |

多段文字、注释插在文字中间、文字加 CDATA 等写法，请整理为一个普通文本片段或一个 CDATA 片段。单段文字前后有注释仍可读取；编辑时仍保留原注释的只读规则。

新检查器不复制旧库忽略尾部、丢弃约束或丢弃文本的行为。这些差异已经由旧程序实际运行确认，测试要求旧接受且新拒绝，单独计数。

## 6. 本轮运行覆盖

192 个配置场景、384 次独立进程运行，普通新程序及新代码 ASan/UBSan/LeakSanitizer 程序分别对照同一个旧参考程序：

| 分类 | 场景数 | 判定 |
| --- | --- | --- |
| 对照一致 | 170 | 实际类型/值/单位/范围包含结果、对象/缺失状态一致 |
| 明确扩展 | 6 | 新程序接受，但旧程序拒绝或解释不同 |
| 严格拒绝 | 8 | 旧程序接受，新程序拒绝 |
| 共同拒绝 | 8 | 两边均不接受；不要求同一阶段或同一错误文本 |

包含原 32 位边界、明确/自动类型、实数/布尔/字符串、枚举和开闭区间、引号/逗号/C转义、普通文本/CDATA/字符引用、多层默认回溯、按 include 文件目录解析嵌套引用、类单位/范围传播、等价/冲突导入和循环/缺失文件。临时输入在结束时清理，另核对两个探针都未改写输入。

新配置模型普通及新代码插桩各 63 项通过，覆盖准确空白的保存、具体覆盖、新增、物化和文件字节撤销；其余原编辑/引用/来源测试继续运行。系统 Qt 和旧 CCfg/Boost/TinyXml 参考库没有整体插桩。完整 GUI、快照 CLI 和离线系统编译回归的实际最终结果见[验证记录](VALIDATION.md)。

## 7. 怎样离线重现

在项目根目录执行。旧参考使用本地编译器和固定 Boost 源码包，不运行下载：

```bash
python3 tests/build_legacy_configuration_reference.py --build-dir build-legacy-config --jobs 2
python3 tests/build_qt_control_validation.py tests/current_configuration_probe.pro build-legacy-config/current-probe --jobs 2
python3 tests/legacy_configuration_integration.py build-legacy-config/legacy-configuration-probe build-legacy-config/current-probe/current-configuration-probe --evidence build-legacy-config/runtime-ordinary
```

构建旧参考时使用原 C++98 基线、XML/STL开关，按原 Makefile 的普通库和 XML源列表编译，再加入本地 TinyXml。不是重新实现一个“看起来相似”的旧解析器。新探针使用 C++11，复用生产文件读取和范围代码。

内存检查的新探针：

```bash
python3 tests/build_qt_control_validation.py tests/current_configuration_probe.pro /tmp/getdaq-legacy-config-current-probe-asan --sanitized --jobs 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 python3 tests/legacy_configuration_integration.py build-legacy-config/legacy-configuration-probe /tmp/getdaq-legacy-config-current-probe-asan/current-configuration-probe --evidence build-legacy-config/runtime-sanitized
```

如果运行环境本身对 LeakSanitizer 使用的进程检查机制有限制，错误输出会明确显示 LeakSanitizer 失败。必须调整测试运行环境并重新通过，不能把探针已经输出 JSON 当作内存检查通过。本轮完整 GUI 的模拟 HTTP 监听同样需要运行环境允许本机监听。

旧参考的 `build-receipt.json` 保存 55 次编译、588 个实际项目/Boost依赖输入、标志、程序摘要和构建前后原文件核对。新 Qt 探针/模型/GUI的凭据由通用 Qt 验证构建脚本保存。本轮对照结果和当前输入绑定见 `build-legacy-config/checkpoint.json`；这是本机工作区检查点，不能独立替代整个源码及工具链的离线分发包。

## 8. 依赖和数据通路边界

固定的 Boost 包只在[验证依赖目录](../third_party/validation/README.md)，旧 GPL参考库和 TinyXml原许可仍保留。生产 `.pro`/CMake 不增加其包含路径或链接；新配置模型没有这些旧依赖。

Qt 配置/控制和查看界面不传原始 MFM。原生接收/写盘/在线导出程序仍独立；本轮没有变更数据接收器，也没有进行大流量传输。20 GiB所有测试流总量上限和结束删除约束继续有效。本轮使用小型 XML/JSON，不以它们推断生产吞吐、设备健康或硬件加载。

## 9. 后续仍要做什么

文件/XML支持域已有运行对照，下一步仍要逐项迁移或明确替代剩余旧命令，验证更广泛的词法/转义、Text/Binary格式及数据库/网络资源使用情况、跨文件变更与迁移、完整编辑工作流。旧完整产品构建成功与全部接口行为一致是两件事。

真实硬件控制权、配置实际加载版本、UNKNOWN恢复、Ubuntu24/Qt5.14.2 MinGW/macOS、持续采集/断电验证继续在[待验收清单](VALIDATION.md)中保留。普通本地构建和模拟服务通过不覆盖这些项目。
