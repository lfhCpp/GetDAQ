# GetDAQ 简易说明书

- `GetDAQ-Quick-Guide.pdf`：20 页中文安装、配置与操作说明书。
- `GetDAQ-Quick-Guide-en.pdf`：20 页英文说明，覆盖相同操作与按钮。
- `GetDAQ-Quick-Guide-zh.html` / `GetDAQ-Quick-Guide-en.html`：由 LaTeX 源生成的软件内置阅读版。
- `GetDAQ-Quick-Guide-en.tex`：英文版 LaTeX 源文件。
- `GetDAQ-Quick-Guide.tex`：LaTeX 主文件。
- `GetDAQ-Button-Reference.tex`：第 9–20 页按钮与配置字段对照，编译时由主文件引入。
- `fonts/`：Noto Sans CJK SC 字体及对应许可，保证中文字体可用。
- `compile.sh`：重新编译，临时产物在系统临时目录清理，保留两种语言 PDF 及生成的 HTML。

内容包括：运行原理、Ubuntu 从源码安装步骤、macOS 编译入口、IP/端口含义、三个目录的区别、单机服务启动、主从配对与整目录同步、采集/日志/告警、在线与离线波形、3D 坐标及通道间距、当前 SOAP MCP 安装和配置。正文依据当前本机源码整理，日期为 2026-11-22；macOS 原生构建及现场硬件验收仍待完成。

## 软件内阅读

Configuration → User guide → 中文 (Chinese) / English。在独立的非模态阅读窗口切换语言、选择章节或搜索；Save PDF 可导出当前语言 PDF。阅读版、PDF 和示例图通过 Qt 资源编译进 GetDAQ，离线可读；安装目录的 docs 被移动时仍能打开。打开说明书不需要断开 ECC，采集中也可使用。内置说明字体为随项目字体的 OFL 子集，未装中文系统字体时也可阅读；没有新增 PDF/WebEngine 运行库依赖。

## 按钮说明从哪里查

第 9–10 页：主界面、菜单、采集和自动运行。第 11–18 页：Nodes / Paths、已有 ECC、主控/从控、主机/板卡组、配置字段、目录同步及原版配置编辑器。第 19–20 页：波形、日志、警告、数据工具和直连 CoBo。每项按当前英文名称说明含义、操作时机和实际影响；特别标注多选/单组范围，以及保存 YAML、同步 XML 和配置硬件的区别。

## 重新生成

安装含 XeLaTeX、fontspec、TikZ、tcolorbox、tabularx、hyperref 等组件的 TeX Live 或 MiKTeX 后，在本目录执行：

```sh
./compile.sh
```

脚本会编译两种语言，并调用 `../../tools/build-guide-html.py` 生成阅读版。只更新 HTML 时可以单独运行该 Python 脚本（不需要第三方 Python 包）。

只编译某一种 PDF 时，也可手动运行两次：

```sh
xelatex -interaction=nonstopmode -halt-on-error GetDAQ-Quick-Guide.tex
xelatex -interaction=nonstopmode -halt-on-error GetDAQ-Quick-Guide.tex
```

字体来源：Noto CJK `Sans2.004`，原始文件 `Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf`；许可见 `fonts/LICENSE`。PDF 内嵌了实际使用的字体子集，阅读时不需要另装字体。

字体子集 `fonts/GetDAQGuideSans.otf` 已提交，普通编译直接使用。增加文档字符后，开发者可在装有 fontTools 的环境运行 `python3 tools/build-guide-font.py` 再重新编译；这是文档维护工具，不是用户安装或运行依赖。许可保留在 `fonts/LICENSE`，运行包也提供 `share/licenses/user-guide/OFL.txt`。
