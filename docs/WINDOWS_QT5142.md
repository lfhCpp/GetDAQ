# Windows / Qt Creator build

Windows builds the GetDAQ control GUI and local tools. Original SOAP ECC and
DataExporter/DataRouter can also be built as native Windows services using the
optional backend recipe. Linux retains automatic Master / Slave node management.
See [PRODUCT_SCOPE.md](PRODUCT_SCOPE.md).

## Build with Qt Creator

1. Install Qt 5.14.2 with the **MinGW 7.3.0 64-bit** kit and its matching compiler.
2. Install Python 3 and make the `python` command available to Qt Creator.
   The bundled original configuration editor uses Python during qmake to prepare
   its source tree; no separate Boost, CompoundConfig or TinyXML installation is needed.
3. Download/extract the complete GetDAQ source, keeping `third_party/legacy-get`
   and the other bundled dependency folders. Open `GetDAQ.pro` in Qt Creator.
4. Select **Desktop Qt 5.14.2 MinGW 64-bit**, run qmake, and build Release.
   The default output directory is the project's `bin` folder.

If Python is not on PATH, set the qmake argument
`GETDAQ_PYTHON=C:/path/to/python.exe` to the actual Python executable.
Use a normal Windows folder such as `C:/Work/GetDAQ` for source and shadow builds.

## Missing `boost/config/platform/win32.hpp`

This error was caused by an incomplete bundled Boost platform-header selection,
not a missing binary library or a requirement to install system Boost.
The unmodified Boost 1.55.0 Windows platform header is now included and listed
with its origin and checksum in `third_party/legacy-get/SOURCE_MANIFEST.json`.

After updating the source, use **Build → Run qmake**, then rebuild. This copies
new bundled files into the configuration editor's existing shadow-build
`apps/cconfigeditor/original` folder. If stale generated build files remain,
remove only that project's generated build folder and configure it again;
keep source, experiment configuration and acquired data.

## Verification scope

The configuration editor compiled and linked successfully with the Windows Qt 5.14.2 / MinGW 7.3.0 x86_64 toolchain. Its original UI passed opening, relative includes, editing, undo/redo, saving, reopening and metadata checks. See [the validation receipt](../validation/windows-cconfig-platform-header.json).

The regression check `python tests/test_original_cconfig_platform_inputs.py`
verifies Linux, macOS and Windows platform headers and refresh of an existing
configuration-editor shadow build. Windows compiler/linker verification uses
Qt 5.14.2 and MinGW 7.3.0 x86_64 through WSL interoperability. It does not replace
an acquisition test on an independent Windows computer and physical hardware.

## Native ECC and receiver services

Native Windows ECC, DataExporter and DataRouter can now be built separately or
with the optional Qt Creator backend target. See
[Windows backend build and configuration](WINDOWS_BACKENDS.md).
Automatic Master / Slave deployment management still uses Linux.
