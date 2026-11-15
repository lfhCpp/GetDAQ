# 仅供历史实现对照的依赖

这个目录不在 GetDAQ、配置工具、原生接收器或 Ice 兼容组件的生产构建路径中。保存源码包是为了以后离线重建旧实现的参考程序，避免再次依赖失效的网址；它不意味着新软件要安装 Boost。

| 文件 | 用途 |
| --- | --- |
| `boost_1_55_0.tar.bz2` | 官方 Boost 1.55.0 源码包，58,146,647 字节；原 Spirit Classic/Phoenix1 和容器头文件供旧 CompoundConfig 参考构建使用 |
| `boost-1.55.0.json` | 固定版本、官方来源、字节数和 SHA-256 |

官方版本页面：[Boost 1.55.0](https://www.boost.org/releases/1.55.0/)；官方源码与摘要：[源码包](https://archives.boost.io/release/1.55.0/source/boost_1_55_0.tar.bz2)、[校验元数据](https://archives.boost.io/release/1.55.0/source/boost_1_55_0.tar.bz2.json)。SHA-256 为：

```text
fff00023dd79486d444c8e29922f4072e1d451fc5a4d2b6075852ead7f2b7b52
```

`tests/build_legacy_configuration_reference.py` 每次先校验压缩包，只向指定的隔离构建目录提取 `boost/` 头文件和原始 `LICENSE_1_0.txt`，限制路径、成员类型及解压大小。其余历史依赖取自收到的 `source/cconfig/`，不修改这些原文件。构建前后检查参考输入摘要，保留实际编译命令、依赖和程序摘要。

Boost 的许可正文位于原包的 `boost_1_55_0/LICENSE_1_0.txt`，解压后仍保留。旧 CompoundConfig 和 TinyXml 继续适用它们各自原始源码中的许可；参考程序不是新版生产软件的一部分。

重建、运行范围、兼容与有意差异见[旧配置库运行对照](../../docs/LEGACY_CONFIGURATION_RUNTIME.md)。
