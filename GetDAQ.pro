# Qt Creator 打开此工程，构建Qt控制端和本机MCP入口；高吞吐DAQ接收保持独立。
TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS = console sentry simulator configtool configcheck configtransfer supervisorquery waveformviewer controlmcp
console.file = apps/envoy/envoy.pro
sentry.file = apps/sentry/sentry.pro
simulator.file = apps/simulator/simulator.pro
configtool.file = apps/configtool/configtool.pro
configcheck.file = apps/configcheck/configcheck.pro
configtransfer.file = apps/configtransfer/configtransfer.pro
supervisorquery.file = apps/datasupervisor/getdaq-supervisor-query.pro
waveformviewer.file = apps/waveformviewer/waveformviewer.pro
controlmcp.file = apps/ecc/mcp/mcp.pro
