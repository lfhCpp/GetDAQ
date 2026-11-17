# Dependencies and offline build

Use [HOST_SUITE_BUILD.md](HOST_SUITE_BUILD.md) for current commands and
[ORIGINAL_BACKENDS.md](ORIGINAL_BACKENDS.md) for exact original-source adaptations.
Dependencies are bundled as source and compiled into original tools. No external
GET installation or historical `source/` directory is required during a build.
A compiler, CMake, Python and Qt development kit remain platform prerequisites.

Source verification: `python3 tools/verify-original-backends.py`.
Product verification: `python3 tools/verify-product-boundary.py`.
