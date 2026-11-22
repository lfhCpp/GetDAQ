# Qt Creator 打开此工程，构建Qt控制端和本机MCP入口；高吞吐DAQ接收保持独立。
TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS = console sentry waveformviewer cconfigeditor onlinewaveform controlmcp
console.file = apps/envoy/envoy.pro
sentry.file = apps/sentry/sentry.pro
waveformviewer.file = apps/waveformviewer/waveformviewer.pro

cconfigeditor.file = apps/cconfigeditor/cconfigeditor.pro

onlinewaveform.file = apps/onlinewaveform/onlinewaveform.pro

controlmcp.file = apps/controlmcp/controlmcp.pro

# Set GETDAQ_BUILD_BACKENDS=1 in Qt Creator's qmake arguments to also build
# native Windows ECC / DataExporter / DataRouter (requires CMake + Python 3).
win32:equals(GETDAQ_BUILD_BACKENDS, 1) {
    SUBDIRS += windowsbackends
    windowsbackends.file = apps/original-backends/windows-backends.pro
}
