# Build and install the current suite

The supported route is listed in [PRODUCT_SCOPE.md](PRODUCT_SCOPE.md). Original
backend source adaptations are documented in [ORIGINAL_BACKENDS.md](ORIGINAL_BACKENDS.md).

Install a C/C++ compiler, CMake, Python 3, Qt 5 development files (including SVG),
and OpenSSL's command-line tool for host pairing. GET, Ice, Boost, CompoundConfig,
TinyXML, DHSM, MFM, gSOAP and Asio sources are bundled in the project.

```sh
python3 tools/verify-product-boundary.py
python3 tools/verify-original-backends.py
python3 tools/build-suite.py --profile full --jobs 2 --stage /path/to/new-installation
python3 tools/build-suite.py --verify-stage /path/to/new-installation
```

Use a new staging directory. Normal builds contain the GUI, original cconfiged,
read-only CoBo information, SOAP ECC, DataExporter, Sentry, node agent, configuration service,
and offline inspect/merge/matrix programs. Retired receivers/controllers are not
built or installed. `control` builds the frontend/original tools; `data-host`
builds an independently managed host; `analysis` adds optional FITS/ROOT tools.

Runtime programs go in `bin/`, resources in `share/`. Use the documented GUI
Master / Slave flow to distribute configuration and start backend instances.
Closing the GUI leaves the independent services running. Existing external ECC
and receiver services can be connected without host pairing or installation.

Tests:

```sh
mkdir build-tests
cd build-tests
qmake ../tests/tests.pro
make -j2
QT_QPA_PLATFORM=offscreen ./getdaq-tests
```

Original process and payload checks are in
`tests/original_backends_payload_smoke.py` and
`tests/original_backends_integration.py`.
