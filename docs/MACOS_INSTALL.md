# macOS build and installation

Build on the Mac itself. The `bin/` directory copied from Ubuntu contains Linux
executables and must not be used as macOS build output. Intel and Apple Silicon
must use a Qt 5 kit matching the architecture of the Mac/build process.
Historical `source/` is not needed; keep `third_party/legacy-get/` intact.

## Build

Install Xcode Command Line Tools and a Qt 5 development kit. With an existing
Homebrew installation, the relevant dependencies can be installed with:

```sh
xcode-select --install
brew install cmake qt@5
```

Use Python 3. From the copied project directory:

```sh
cd /Users/attpc/Documents/GetDAQ
./build-macos.command --jobs 2
```

The launcher handles both a source checkout and a copied Ubuntu installation
whose sources are under `code/`. It selects an installed Qt 5 kit or Homebrew
Qt 5 automatically. An independently installed Qt 5 kit can be selected explicitly:

```sh
./build-macos.command --qmake /path/to/Qt/5.15/macos/bin/qmake --jobs 2
```

The default is the full POSIX host suite. `--profile control` builds the GUI and
control/offline tools, including original cconfiged and GetController, without
the backend processes. `--build-only` skips app packaging.

The output directory must be new. To build another package without replacing a
previous one:

```sh
./build-macos.command --output dist-macos-new --jobs 2
```

## Run

Double-click `dist-macos/GetDAQ.app` or `dist-macos/start-getdaq.command`.
The app includes Qt frameworks/plugins and the original helper executables.
The configuration editor and GET Controller use the existing menus; no main
window layout is changed. The frame dictionary and license notices are stored
inside `GetDAQ.app/Contents/share/`, matching the helper lookup paths.

Do not copy Ubuntu `site/` state or assume its absolute configuration paths are
valid on the Mac. Open a configuration with paths for this Mac. The packaged app
does not auto-connect to a board. Settings remain separate from the Ubuntu host.
Original GetController still requires an Ice GetEcc service; a SOAP-only or
native HTTP endpoint does not become compatible merely by using macOS.

The package is locally ad-hoc signed and signature-verified; it is not Apple
notarized. Use the Mac's normal Open workflow if it requests confirmation for an
app copied from another computer.

## Build adaptations and verification status

- The original controller is included in macOS control/full build plans.
- Darwin linkage omits Linux `librt`/`libdl`. The old upstream Apple exclusions
  for POSIX timer-based debug/performance counters are preserved; regular data
  storage, control, test UI and plotting sources remain included.
- Ice CPU detection is patched in the build copy using compiler byte-order and
  pointer-size macros, covering Intel and Apple Silicon without altering the
  curated original header.
- Curated Boost headers include Clang, libc++, macOS and relevant conditional
  dependencies, rather than only the previously tested GCC/Linux branch.
- Packaging uses `macdeployqt` on the main GUI and all helper programs, checks
  Mach-O architectures, rejects unchanged Linux outputs, and checks remaining
  direct dependencies for developer-machine library paths.
- The minimum macOS version is read from the Mach-O deployment target; it is not
  invented from the host OS or used to claim support for an untested old system.

Portable recipe, CPU preprocessor and packaging checks run via:

```sh
python3 tests/test_macos_original_tools.py
python3 tools/verify-legacy-sources.py
```

The macOS host-suite GitHub Actions job additionally builds/runs original-editor
and GetController workspace/plot/SVG/loopback-Ice smoke checks, then produces a
`GetDAQ-macos.zip` artifact. That workflow code is an execution path, not evidence
of success until a specific macOS run passes. Native Intel/Apple Silicon builds,
launching the deployed app and physical acquisition must be verified on the
actual target architecture; Linux checks alone do not prove them.

References: [Qt macOS deployment](https://doc.qt.io/archives/qt-5.15/macos-deployment.html),
[Homebrew Qt 5](https://formulae.brew.sh/formula/qt@5).
