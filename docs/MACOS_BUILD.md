# macOS build

See [macOS installation](MACOS_INSTALL.md) for the current build commands.
The full profile builds the original SOAP ECC, DataExporter, GUI tools and node agent.
The control profile builds GUI tools; the data profile builds offline inspection tools.
Qt 5, CMake, Python 3 and Xcode Command Line Tools are build prerequisites.
Bundled third-party sources are used offline. Linux binaries must not be run on macOS.

The former native receiver/supervisor build and its workflow have been retired.
The current product boundary is documented in [PRODUCT_SCOPE.md](PRODUCT_SCOPE.md).
macOS physical compilation and acquisition remain unverified; Linux test results
do not establish macOS support.
