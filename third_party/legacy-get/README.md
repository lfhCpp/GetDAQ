# Project-local legacy GET sources

This directory contains the curated source inputs for the original cconfiged
and original SOAP ECC / DataExporter integrations. Their build scripts do **not** read `source/`.
Keep this directory when copying the project to another computer.

| Directory | Contents |
|---|---|
| `widgets`, `compound`, `tinyxml` | Configuration UI, parser and serializers |
| `getbench`, `mdaq`, `multiframe`, `utilities` | Required client, data, frame, utility code and Slice interfaces |
| `boost_1_55_0` | Selected headers and program_options/filesystem/system source dependencies |
| `ice` | Ice runtime, Slice compiler and required Ice/IceStorm interface sources; excludes unrelated servers/tests/examples |
| `licenses` | Original copyright and license notices |

`SOURCE_MANIFEST.json` lists each extracted file's origin and SHA-256. Origins
are provenance strings, **not build input paths**. Original file contents and
notices are preserved here. The existing compatibility adjustments happen in
the build directory. Extraction follows the actual Linux build source lists,
compiler dependencies, UI resources, format dictionaries and interface includes;
it is not a copy of the entire historical tree. Clang/libc++/macOS and MinGW/Windows conditional headers are also included. Native macOS validation
is tracked separately; source presence alone does not establish cross-platform compatibility.

Run `python3 tools/verify-legacy-sources.py` from the project root to verify it.
No network or original historical source directory is used by this check.
Existing project-local bzip2 and mcpp archives remain prerequisites for compiling
Ice; normal Qt development/system tools are still required. Runtime binaries
need the corresponding Qt/system runtime, not this source directory.

This extraction does not upload the historical sources to GitHub. It does not
change the owner's earlier publication/history instructions or constitute a
blanket licensing clearance. Refer to the preserved notices before distribution.
