# Offline MFM event merger

`getdaq-merge` replaces the old `CoBoFrameViewer` command-line `cobo-frame-merge` job for its event-grouping function. It is an offline file tool under `apps/datareceiver`; it does not listen on TCP, participate in acquisition, or pass waveform bytes through Qt. It uses the same standalone C++11 build as the receiver and has no ROOT, CFITSIO, Qt, Boost, Ice, or yaml-cpp dependency.

The program scans MFM primary headers and records only each supported frame's event ID, source file, offset, and byte length. It then sorts those references by event ID, input order, and file offset and copies selected frame bytes in bounded 1 MiB chunks. Waveform payloads are not expanded into channel objects and a complete `.graw` file is never loaded into RAM. Index memory grows with the number of supported frames; the program refuses to index more than 20 million frames (the reference index is roughly 480 MiB at that limit). Sorting is in place and does not allocate a second reference array. Each individual source frame is limited to 1 GiB.

## Build and run

```sh
cmake -S apps/datareceiver -B build-receiver -DCMAKE_BUILD_TYPE=Release
cmake --build build-receiver --parallel 4 --target getdaq-merge
./build-receiver/getdaq-merge -o run.gmer run-a.graw run-b.graw
```

Useful options:

| Option | Meaning |
| --- | --- |
| `-o FILE` | Required output base filename. |
| `--min-event ID` / `--max-event ID` | Inclusive uint32 event-ID range. |
| `--max-events N` | Stop after N selected event groups; zero means no count limit. |
| `--max-file-mib MIB` | Start a numbered output part before an event would exceed this size; zero disables rotation. Default is 1024 MiB. |

Outputs use `.gmer` by convention, but the extension is not required. If a destination already exists, the program preserves it and chooses a numbered part (`run.1.gmer`, `run.2.gmer`, ...). It does not overwrite an input file. One event group is kept intact in one output file, so a single unusually large event may exceed the configured part size. An error or interrupted process can leave an incomplete output file; verify completed files with `getdaq-inspect` before analysis.

The merger includes supported CoBo MFM frames (type `0x01`, revisions 0–5; type `0x02`, revision 5) and RIBF frames (type `0x30`, event ID at header offset 14). Other MFM types are skipped. Events are written in ascending numeric event-ID order; frames with the same event ID keep their input-file and within-file order. The output uses the legacy `MergedByEventId` layered format, frame type `0xFF01`, revision 1, a 20-byte logical header, and the event ID at bytes 16–19. Embedded frames remain byte-for-byte unchanged.

This command reproduces the useful data-merging job, not the old CoBoFrameViewer plotting or full ROOT analysis. The optional `getdaq-root` converter preserves the legacy `tree/GDataFrame` and `TT2K/T2KEvent` object-tree paths, while `getdaq-root-viewer` browses events and displays channel waveforms or a bucket-by-channel heatmap. Legacy statistical plots, 3D analysis, and compatibility with real downstream ROOT consumers still need verification. A separate optional `getdaq-fits` converts supported CoBo type-1 frames to FITS when CFITSIO is installed. Neither ROOT nor CFITSIO is required to receive, preserve, or merge raw MFM.

## Safety and validation

Use the tool on closed, archived `.graw` files. Do not merge files while a receiver is still writing them. It rechecks every selected frame while copying and aborts if the source has become truncated. Keep enough disk space for the output, which can approach the total size of selected input frames plus event headers and alignment padding. Synthetic regression fixtures are small and are cleaned after the test; production input files are never deleted by this program.

The native `mfm-merge` test checks event ordering, grouping, supported-type filtering, unchanged embedded bytes, output framing, and preservation of an existing destination. This validates the implemented byte layout using the documented MFM fields and local frame parser; it is not a physical-event or downstream ROOT/legacy-analysis application acceptance test.
