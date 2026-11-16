# Original configuration editor

GetDAQ's **Configuration → GET configuration files…** opens the original
`cconfiged` editor, shipped as `bin/getdaq-cconfig-editor`. Disconnect before
opening it. Its **File → Open** dialog starts in the Run Setup configuration
directory. Open the existing `.xcfg` file, edit, and use **File → Save**.
Changes are used when the configuration is next loaded/prepared; saving does not
send commands to ECC or change a running acquisition.

The original tree, attribute builder, find, column visibility, include/import,
validation and undo/redo implementations are retained. The editor runs in a
separate process so its original Quit/close-all-windows behavior cannot close
the acquisition GUI. No new button or layout change is made to the main window.

## Build and runtime dependencies

The original CompoundConfig XML/binary engine, TinyXML, ConfigWidgets, and the
Utilities logging implementation are compiled directly into the editor. Boost
1.55 headers come from the curated project-local source directory.
No separately installed legacy library, database or NFS service is needed for
local configuration files. Qt Widgets and the normal system C++ runtime remain
runtime dependencies, as they already are for GetDAQ.

Build with the normal `GetDAQ.pro` / `tools/build-suite.py` control or full build,
or build just the editor:

```sh
mkdir -p /tmp/getdaq-cconfig-build
cd /tmp/getdaq-cconfig-build
qmake /path/to/GetDAQ/apps/cconfigeditor/cconfigeditor.pro
make -j2
```

Build requires Python 3 and the curated project-local sources in
`third_party/legacy-get/`. Historical `source/` and the full Boost/Qwt source
archives are no longer read by this editor's build. `GETDAQ_PYTHON=/path/to/python`
selects the build interpreter. End users need only the resulting editor beside
GetDAQ and the existing Qt runtime.

Preparation copies originals into the isolated build directory, adapts generated
UI-header names for qmake, and gives the two `Exception.cpp` files distinct object
names. Original source files and copyright/license notices remain untouched.
`original/original-inputs.json` records their hashes. Package/source license
notices are provided under `apps/cconfigeditor/licenses/`; Boost's license is
also in its bundled archive. No licensing clearance beyond these existing
notices is implied.

## Validation and limits

`tests/original_cconfig_editor_smoke.cpp` runs the actual original window/model
against temporary files: relative include, value edit, undo/redo, save/reopen,
and retention of include paths, ranges and units. An optional second input
opens a historical hardware `.xcfg` without writing it. It never operates ECC
or receiver services. This does not establish complete equivalence for every
original feature; database support is not added (the supplied original model
already reports no database repository support).

To run the UI/model smoke check using the same qmake preparation:

```sh
mkdir -p /tmp/getdaq-cconfig-test
cd /tmp/getdaq-cconfig-test
qmake /path/to/GetDAQ/tests/original_cconfig_editor_smoke.pro GETDAQ_DESTDIR=/tmp/getdaq-cconfig-test
make -j2
QT_QPA_PLATFORM=offscreen ./getdaq-original-editor-smoke
```

Linux Qt 5.12 local validation passed, including opening the historical
`configure-pedestals.xcfg`. Windows/macOS builds and complete manual parity
of all editor operations have not been verified in this change.
