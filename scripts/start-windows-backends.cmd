@echo off
setlocal
if "%~3"=="" goto usage
set "getdaq_programs=%~dp0..\bin"
set "getdaq_project=%~1"
set "getdaq_control=%~2"
set "getdaq_data=%~3"
if not exist "%getdaq_programs%\getEccSoapServer.exe" goto missing
if not exist "%getdaq_programs%\DataExporter.exe" goto missing
if not exist "%getdaq_project%\CoboFormats.xcfg" (
  echo Copy the experiment XML/XCFG files and CoboFormats.xcfg into "%getdaq_project%" first.
  exit /b 2
)
if not defined GETDAQ_ECC_PORT set "GETDAQ_ECC_PORT=8083"
if not defined GETDAQ_ALARM_PORT set "GETDAQ_ALARM_PORT=46011"
if not defined GETDAQ_ROUTER_CONTROL_PORT set "GETDAQ_ROUTER_CONTROL_PORT=46003"
if not defined GETDAQ_DATA_PORT set "GETDAQ_DATA_PORT=46005"
if not defined GETDAQ_PREVIEW_PORT set "GETDAQ_PREVIEW_PORT=46491"
set "GETDAQ_COBO_FORMATS=%getdaq_project%\CoboFormats.xcfg"
start "GetDAQ DataExporter" /D "%getdaq_project%" "%getdaq_programs%\DataExporter.exe" "%getdaq_control%:%GETDAQ_ROUTER_CONTROL_PORT%" "%getdaq_data%:%GETDAQ_DATA_PORT%" --export-port "%GETDAQ_PREVIEW_PORT%"
start "GetDAQ SOAP ECC" /D "%getdaq_project%" "%getdaq_programs%\getEccSoapServer.exe" --port "%GETDAQ_ECC_PORT%" --alarm-port "%GETDAQ_ALARM_PORT%" --config-repo-url .
echo Backend consoles opened. Inspect both logs before connecting GetDAQ.
echo Stop acquisition in GetDAQ before closing services. Ctrl+Break requests graceful service exit.
exit /b 0
:missing
echo Build native Windows backends first: python tools\build-windows-backends.py --compiler-bin PATH_TO_MINGW_BIN
exit /b 2
:usage
echo Usage: start-windows-backends.cmd PROJECT_DATA_FOLDER CONTROL_IPV4 DATA_IPV4
echo Example: start-windows-backends.cmd "C:\GetDAQ-data" 192.168.40.10 192.168.50.10
exit /b 1
