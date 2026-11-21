# GetDAQ 简易说明书

- `GetDAQ-Quick-Guide.pdf`：8 页中文安装、配置与操作说明书，直接阅读。
- `GetDAQ-Quick-Guide.tex`：LaTeX 源文件。
- `fonts/`：Noto Sans CJK SC 字体及对应许可，保证中文字体可用。
- `compile.sh`：重新编译，临时产物在系统临时目录清理，只保留 PDF。

内容包括：运行原理、Ubuntu 从源码安装步骤、macOS 编译入口、IP/端口含义、三个目录的区别、单机服务启动、主从配对与整目录同步、采集/日志/告警、在线与离线波形、3D 坐标及通道间距、当前 SOAP MCP 安装和配置。正文依据当前本机源码整理，日期为 2026-11-21；macOS 原生构建及现场硬件验收仍待完成。

## 重新生成

安装含 XeLaTeX、fontspec、TikZ、tcolorbox、tabularx、hyperref 等组件的 TeX Live 或 MiKTeX 后，在本目录执行：

```sh
./compile.sh
```

也可手动运行两次：

```sh
xelatex -interaction=nonstopmode -halt-on-error GetDAQ-Quick-Guide.tex
xelatex -interaction=nonstopmode -halt-on-error GetDAQ-Quick-Guide.tex
```

字体来源：Noto CJK `Sans2.004`，原始文件 `Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf`；许可见 `fonts/LICENSE`。PDF 内嵌了实际使用的字体子集，阅读时不需要另装字体。
