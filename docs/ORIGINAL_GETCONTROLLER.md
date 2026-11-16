# Original GetController integration

**Tools → GET Controller…** opens the original GetController window as a
separate process shipped with GetDAQ. Disconnect GetDAQ first. This does not
change the Run Control layout or replace the existing ECC/data service manager.
Closing one window does not close the other.

## Use

1. Restart GetDAQ after installing the update and choose **Tools → GET Controller…**.
2. In the original workspace wizard, open your existing GetController workspace
   or create a new workspace file. This is a GetController workspace `.xcfg`,
   not GetDAQ's `envoy.yml`.
3. The original data-router wizard selects internal versus external data
   reception. Check the receive address/port against any already running
   receiver before accepting it.
4. Use **Open test**, **New test**, and **Edit test** in the original window.
   Its test contains ECC, DAQ, target-board addresses and hardware/conditions/data
   paths. **Load Hw → Configure → Start → Stop** remain the original operations.

The frame-format dictionary and all referenced revisions are shipped together
under `share/get-bench/format/`. The menu supplies its path automatically.
No example workspace is automatically connected to a real board.

## Protocol boundary

This is the original **Ice `get::GetEcc` client**, with object identity `Ecc` by
default. Its original default ECC port is 46002, router management port 46003,
and router data-flow port 46005. Actual installations can choose other ports.
Use the address/port of the corresponding Ice service, not a SOAP-only address,
GetDAQ native HTTP API, management agent, or board-control socket.

A working legacy SOAP connection in GetDAQ does not establish that the same
port supports this client's Ice interface. The current native HTTP ECC service
does not implement the original GetEcc interface. No adapter claiming to bridge
all original register, calibration, scan and data operations has been added.
Real hardware compatibility must be tested against the actual original Ice ECC.

GetController retains its own workspace/settings and optional internal router.
The GetDAQ acquisition YAML is not silently converted to a GetController
workspace, and GetDAQ does not track a detached GetController acquisition.

## Build and dependencies

Linux build:

```sh
python3 tools/build-original-getcontroller.py \
  --build-dir /tmp/getdaq-original-controller --jobs 2
```

The standard Linux control/full suite also includes this component. Build needs
Python 3, CMake, a C/C++ compiler and Qt 5 development modules (Widgets, Network,
XML, PrintSupport, OpenGL, SVG). `--ice-build` can reuse the existing full native
compatibility build; otherwise bundled Ice interfaces/runtime are built offline.
The curated `third_party/legacy-get/` directory supplies the required sources;
`source/` is not read by this build.

CompoundConfig, ConfigWidgets, TinyXML, Utilities, MDaq, MultiFrame, the required
GetBench client/data code, Ice/IceStorm client bindings, Boost and Qwt are compiled
into the executable. Users need no separately installed legacy libraries or
Python. Qt/system runtime libraries remain required, including SVG, printing
and OpenGL modules used by the original plotting/export functions.

Qwt 6.1.4 is the [official source release](https://sourceforge.net/projects/qwt/files/qwt/6.1.4/qwt-6.1.4.tar.bz2/download),
SHA-256 `1529215329e51fc562e0009505a838f427919a18b362afff441f035b2d9b5bd9`.
Used source files are extracted under `third_party/legacy-get/qwt-6.1.4/`;
this build does not extract or download the full archive. License/copyright notices are
in `apps/getcontroller/licenses/` and installed under `share/licenses/getcontroller/`.

Preparation copies originals into the build directory without changing them,
adapts generated UI-header names, reproduces the old Ice include-selection
macros, adds explicit QPainterPath includes to affected Qwt build copies/wrappers
for Qt 5.15, and uses unique compilation wrappers to avoid object-name collisions.
It respects the original list of controller headers (unused old WorkspaceDialog
code is not added). `original/original-inputs.json` records the input hashes.

## Validation

`tests/original_getcontroller_smoke.pro` builds against the same original code,
excluding only its production `main`. The check uses temporary settings and
workspace files, loads the real frame dictionary, creates/saves/reopens a test,
checks channel masks, displays the original main window, plots a curve and
exports SVG. It queries/pings an identity-only mock Ice `Ecc` object on a
private loopback port using separate server/client communicators. That mock
cannot execute hardware operations. No physical ECC/CoBo is contacted.

These checks do not establish hardware acquisition, all test/calibration/scan
results, or full new/old ECC protocol compatibility. Windows/macOS builds are
not validated by this integration.
