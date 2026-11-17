# Qt Creator 打开此工程，构建Qt控制端和本机MCP入口；高吞吐DAQ接收保持独立。
TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS = console sentry waveformviewer cconfigeditor
console.file = apps/envoy/envoy.pro
sentry.file = apps/sentry/sentry.pro
waveformviewer.file = apps/waveformviewer/waveformviewer.pro

cconfigeditor.file = apps/cconfigeditor/cconfigeditor.pro
