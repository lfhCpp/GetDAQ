# 71.774 Offline converter CLI argument parsing shared (2026-11-10)

Moved duplicate command-line value parsing from `matrix_main.cpp`, `fits_main.cpp` and `root_main.cpp` into `apps/datareceiver/cli_options.h`. `ParseUnsignedOption` owns decimal unsigned conversion and bounds/error handling; `OptionValue` supports both separate values and `--option=value`. Each CLI keeps its own accepted options, usage text, conversion call and result output.

Built the default matrix and optional FITS targets, then configured and built the optional ROOT converter:

```bash
cmake --build /tmp/getdaq-fits-refactor-build --target getdaq-matrix getdaq-fits --parallel 4
cmake -S apps/datareceiver -B /tmp/getdaq-offline-tools-refactor \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/home/lfh/work/root6.26 \
  -DGETDAQ_BUILD_OPTIONAL_FITS_TOOLS=ON -DGETDAQ_BUILD_OPTIONAL_ROOT_TOOLS=ON
cmake --build /tmp/getdaq-offline-tools-refactor --target getdaq-matrix getdaq-fits getdaq-root --parallel 4
```

The optional build used CFITSIO 3.47 and ROOT 6.26/16. The current Linux full profile also rebuilt successfully:

```bash
python3 tools/build-suite.py --profile full --build-root /tmp/getdaq-full-refactor-774 --jobs 4
```

The full receipt reports **30/30 programs**, 1,054 input records and **109/109 embedded dependency files** verified; status is `complete` and `target_platform_and_hardware_accepted=false`. No tests, CLI commands, conversions, runtime services, GUI flows, DAQ data or hardware were used. Receipts: `/tmp/getdaq-full-refactor-774/full/build-receipt.json`; optional tools in `/tmp/getdaq-offline-tools-refactor`.

# 71.773 Current-source Windows Qt 5.14.2 control-suite build (2026-11-10)

Rebuilt the current Windows control profile through the installed Qt 5.14.2 / MinGW 7.3 x64 Kit:

```bash
python3 tools/build-windows-from-wsl.py \
  --kit /mnt/d/Software/QT/5.14.2/mingw73_64 \
  --compiler /mnt/d/Software/QT/Tools/mingw730_64 \
  --build-root build-windows-refactor-772 --jobs 2
```

The build completed successfully with **9 programs**, all verified by `file` as x86_64 PE, and **1,054 input records** in the receipt. Embedded dependency verification passed offline; the build log has no warning/error lines. This includes the current standalone `getdaq-waveform-viewer` CLI split. Receipt: `build-windows-refactor-772/windows-build-receipt.json`; log: `build-windows-refactor-772/control-windows.log`.

This is a build-only result from the user's installed Windows Kit invoked through WSL interop. No tests, launches, GUI interaction, deployment smoke or hardware flow were run. Receipt fields keep `hardware_accepted=false` and `independent_windows_machine_accepted=false`; it does not establish macOS, clean Windows deployment or field acceptance.

# 71.772 Current-source Linux full host-suite build after Waveform Viewer CLI split (2026-11-10)

Built the current full host profile after adding the standalone Waveform Viewer CLI module:

```bash
python3 tools/build-suite.py --profile full --build-root /tmp/getdaq-full-refactor-772 --jobs 4
```

The build completed with status `complete`: **30/30 host programs** built, **109/109 embedded dependency files** passed offline checksum verification, and the receipt binds 1,053 build-input records. The built `getdaq-waveform-viewer` receipt SHA-256 is `37645eadef30e14e78a15902a3d27ded6671506504c9823f3d540e3a48e29c95`. Receipt: `/tmp/getdaq-full-refactor-772/full/build-receipt.json`.

This was a Linux/WSL2 build-only pass with CMake 3.16.3, Qt 5.12.8 and GNU Make 4.2.1. No tests, launch smoke, GUI interaction, service flow, hardware access or DAQ data flow were run. The receipt sets `target_platform_and_hardware_accepted=false`; this does not establish Windows/macOS, independent-host, board or field acceptance.

# 71.771 Waveform viewer CLI separated from GUI entry point (2026-11-10)

Moved the standalone waveform viewer's `--analyze-peaking-scan`, `--analyze-pulser-response`, and `--help` handling from `main.cpp` into `waveformviewer_cli.cpp/.h`. The GUI entry retains Qt startup and `--surface-smoke`; the standalone qmake project registers the new module, while Envoy and the aggregate Qt tests continue to include only `waveformwindow.pri`.

Built in a fresh shadow directory with Linux Qt 5.12.8/C++11:

```bash
qmake /home/lfh/Work/Codex/GetDAQ/apps/waveformviewer/waveformviewer.pro -o Makefile
make -j4
```

The standalone `getdaq-waveform-viewer` compiled and linked successfully. No CLI commands, GUI smoke flow or tests were run. Other platform builds and runtime behavior remain unverified.

# 71.770 Current-source optional ICE ingress build (2026-11-10)

Rebuilt the optional production targets from current source in `/tmp/getdaq-native-717-clean`, an ICE-enabled CMake tree:

```bash
cmake --build /tmp/getdaq-native-717-clean --target getdaq-ice-receiver getdaq-ice-ingress-core --parallel 4
```

Both `getdaq-ice-receiver` and `getdaq-ice-ingress-core` built successfully, including the shared `apps/datareceiver/diskreserve.cpp` implementation. No test targets or tests were run; no receiver process was started and no network/data/hardware flow was exercised. The strict ICE validation helper also executes an options-test binary, so it was not used in this build-only check. This confirms compilation only, not ICE runtime compatibility or field acceptance.

# 71.769 Scan-plan command implementations separated by scan type (2026-11-10)

Moved the PeakingTime, Pulser, Threshold and AutoGain plan-generation branches from `scanplancommands_generation.cpp` into dedicated command implementation units. The remaining source dispatches by the existing command name. Argument-count and field parsing, preflight order, artifact naming/order, manifest keys, status text and error messages remain in their corresponding command implementation. Registered all four sources in the standalone ConfigTool and strict direct-source probe qmake projects.

Built `apps/configtool/configtool.pro` with Linux Qt 5.12.8 and separately built `tests/configtool_probe.pro` in a fresh directory using `CONFIG+=strict_c++` (`-std=c++11 -pedantic-errors`). Both compiled and linked successfully. No tests or scan-plan commands were run; no hardware or DAQ data were used. Runtime behavior and other platform builds remain unverified.

# 71.768 Current-source Linux full host-suite build (2026-11-10)

`python3 tools/build-suite.py --profile full --build-root /tmp/getdaq-full-refactor-767 --jobs 4` completed successfully. The receipt reports 30 built programs and 109 verified embedded dependency files, with no network use. The Linux environment is WSL2 (`Linux-5.15.167.4-microsoft-standard-WSL2`, glibc 2.31), CMake 3.16.3, and Qt 5.12.8; receipt: `/tmp/getdaq-full-refactor-767/full/build-receipt.json`. The receipt's `target_platform_and_hardware_accepted` field is `false`.

This was a build only. No test suites, help/startup smoke, GUI interaction, real service, board, or DAQ-data flow were run. It confirms all registered current-source Linux full-profile targets compile together, but does not establish application behavior, a clean independent platform install, macOS/Windows acceptance, hardware compatibility or field readiness.

## 71.767 Generated scan-plan publication shared (2026-11-10)

Extracted generated scan-plan target validation and staged directory publication to `apps/configtool/scanplancommands_publication.cpp`. PeakingTime, Pulser, Threshold and AutoGain commands retain their command-specific parsing, point generation, point-file bytes/order, manifest fields and success/error messages; the shared helper handles parent/target preflight, a private same-parent staging directory, the manifest write and final no-replace rename. Registered the module in `apps/configtool/configtool.pro` and `tests/configtool_probe.pro`.

Built the production ConfigTool with Linux Qt 5.12.8/C++11 using an isolated `/tmp` qmake build directory; compilation and linking completed. Separately rebuilt `tests/configtool_probe.pro` in a fresh `/tmp` directory with `CONFIG+=strict_c++` (`-std=c++11 -pedantic-errors`); it also compiled and linked with the new publication unit registered. No tests or generated-plan commands were run. No hardware or DAQ data were used. Broader CLI regression and platform verification remain open.

## 71.766 Failure-only system unprepare boundary (2026-11-10)

Moved the failure-only `UnprepareAfterFailure`, `VerifyUnpreparedAfterFailure`, `FailureCleanupAuditChains` and `FailureVerificationAuditChains` methods into `apps/hardware/systempreparation_failureunprepare.cpp`. Normal `Unprepare` and `UnprepareAuditChains` stay in `systempreparation_unprepare.cpp`. This is a source responsibility split; preconditions, operation order, audit evidence, failure latching, owned alarm handling and no-replay semantics were not intentionally changed. Registered the new source in the hardware CMake library.

`cmake --build build-platform/aget-system-hardware-71-102 --target getdaq-hardware-core --parallel 2` reconfigured and rebuilt the hardware core successfully on Linux. No tests were run and no hardware calls or physical data were used. Broader lifecycle, platform and field verification remain open.

## 71.765 Hardware plan structural validation separation (2026-11-10)

Moved the local, deterministic `HardwareStep` structure checks to `apps/hardware/hardwareclient_planvalidation.cpp`; the coordinator still performs step-count checks, clears the execution audit chain at the same point, supplies the current bound-description state, and then performs all remote metadata/width validation and plan execution. The extracted checks preserve their original order and exact error messages. Registered the source in `apps/hardware/CMakeLists.txt`.

`cmake --build build-platform/aget-system-hardware-71-102 --target getdaq-hardware-core --parallel 2` reconfigured and rebuilt the hardware core successfully with GCC on Linux. No tests were run. No runtime hardware calls, board firmware or physical DAQ data were involved. Full responsibility, platform and field validation remain open.

# 71.764 Native ECC receiver failure cleanup separated (2026-11-10)

Moved `AbortFailedReceivers` and `RetireFailedReceivers` from the general Native ECC recovery coordinator into `nativeecccoordinator_receiverrecovery.cpp`. Their original stop-proof, failure-connection cleanup, prior Abort proof, journal binding, immutable audit, and UNKNOWN-preservation checks are unchanged. Registered the source in the native ECC CMake library; `getdaq-ecc-native` compiled and linked on Linux. No tests or runtime failure-recovery flow were run; hardware/platform and full ECC audit remain open.

# 71.763 Configuration value save coordination separated (2026-11-10)

Moved scalar-save coordination, the multiline value editor and override-preview cleanup from `ConfigWindow` into `configwindow_valueediting.cpp`; confirmation of wildcard inheritance changes, connected read-only gating, background effective-value validation, atomic save/history behavior and line-ending confirmation remain intact. Registered the source in production Envoy and aggregate Qt tests. The Linux control profile rebuilt all nine programs and its receipt includes the new file. No tests or GUI interaction were run; Windows, macOS and hardware acceptance remain open.

# 71.762 Shared receiver disk-reserve policy separated (2026-11-10)

Moved checked disk-space reserve arithmetic and the one-MiB check-window policy from `FrameStore` into `apps/datareceiver/diskreserve.{h,cpp}`. `FrameStore` and the optional ICE ingress target now compile the same shared implementation; CMake registers it in both production targets. Updated the strict ICE build manifest to account for the additional compile input. The Linux `getdaq-receiver` and `getdaq-ice-receiver` targets both compiled and linked; the strict standalone build and tests were not run. Runtime acquisition, hardware and platform acceptance remain open.

# 71.761 Configuration value editor delegate separated (2026-11-10)

Moved type- and range-aware editor creation, editor value transfer, and edit commit dispatch from `ConfigWindow` into the private `ConfigValueDelegate` module. Kept the existing boolean/enumeration selectors, bounded integer editor, numeric validators, and asynchronous-save callback behavior unchanged. Registered the source in the production Envoy and aggregate Qt test qmake projects. `python3 tools/build-suite.py --profile control --build-root /tmp/getdaq-control-760 --jobs 4` completed with nine programs; the build receipt includes the new source/header. No tests or runtime GUI flow were run; Windows, macOS and hardware acceptance remain open.

# 71.760 Receiver identity validation separated (2026-11-10)

Moved the existing `ValidateReceiverIdentity` policy into `apps/datareceiver/receiver_identity.cpp` without changing its rules or error text, and registered it in the standalone DataReceiver CMake target. The affected `getdaq-receiver` target compiled and linked successfully from `/tmp/getdaq-host-full-748/full/receiver`. No tests, runtime sender, receiver hardware, or production data were used; the full DataReceiver and platform audits remain open.

# 71.759 Sentry HTTP request validation separated (2026-11-10)

Extracted route/method resolution and catalog mutation JSON validation from `SentryService::Handle`, retaining the original check order and service state/admission behavior. Registered the new source and private header in Sentry, Simulator and `tests/tests.pro`. `python3 tools/build-suite.py --profile control --build-root /tmp/getdaq-control-759 --jobs 4` completed with status `complete`; the receipt lists nine built programs and includes the new validator source/header. The aggregate Qt test binary was not part of this profile; no tests, service runtime, hardware or production data were exercised. Receipt: `/tmp/getdaq-control-759/control/build-receipt.json`.

# 71.758 ICE ingress status projection separated (2026-11-10)

Moved `IceIngressRecorder::Status()` serialization to a dedicated translation unit, preserving the JSON field names/order, mutex, and atomic reads. Registered the source in the optional ICE core and updated the strict ICE compile-count assertion from 19 to 20. The current CMake `getdaq-ice-receiver` target built successfully from `/tmp/getdaq-host-full-748/full/native`, which reconfigured against this source tree; the build compiled and linked both 71.757 and 71.758 units. No tests, strict standalone build receipt, runtime protocol, or hardware flow was run.

# 71.757 ICE ingress output preflight boundary (2026-11-10)

Extracted storage-mode, output-directory occupancy, and storage-path preflight from `IceIngressRecorder` construction into a private ICE ingress unit, preserving the directory-lease-before-preflight-before-writer order and existing error paths. Registered the new production translation unit and changed the strict ICE compile-count assertion from 18 to 19. The current CMake `getdaq-ice-receiver` target subsequently compiled and linked this unit together with the 71.758 status unit; no tests or runtime flow were run. The previous 71.741 build evidence predates these source changes.

# 71.756 Configuration value save and effective review moved off the UI thread (2026-11-10)

Moved scalar save/history execution, including its before/after effective-configuration snapshots, to a QtConcurrent worker. A window-modal progress indicator keeps the configuration window from accepting overlapping edits while the worker runs; the tree and history state refresh on completion. Saving a `[*]` template now asks for confirmation and states the precise inheritance behavior: instances lacking their own value declaration inherit the change; explicit values remain as declared. Production Envoy rebuilt and linked with Qt 5.12.8. No GUI interaction or automated test was run, so UI responsiveness on the user's large configuration still needs confirmation.

# 71.755 Configuration values edit inline with type-aware controls (2026-11-10)

The value column now edits in place on double-click/F2: declared enumerations and booleans use combo boxes, bounded integers use a spin box with inclusive/exclusive endpoints, and other scalar types use typed text input. Commits go through `XcfgEditHistory::SetValue` and the existing source/value validation. The old ConfigWidgets source confirms these editor types and inline-cell editing. Its multiselection menu only provides copy, cut, and delete; it has no batch-value-edit action, so this change does not claim or add that unverified behavior. Production Envoy rebuilt and linked successfully with Qt 5.12.8 using `/tmp/getdaq-config-editor-20261110`. No GUI interaction or automated test was run.

# 71.754 Configuration editor workspace and value editing improved (2026-11-10)

Restructured the GET configuration window into separate edit and tools tabs so utility actions no longer consume the main source-tree area. Added explicit selected-object/scalar guidance, made Find value source select and scroll to the exact source declaration, and added a large modal editor for multiline values. Multiline edits reuse `XcfgEditHistory::SetValue`; the UI explains LF line endings and asks before normalizing CR-bearing values. The blank instance-override preview is hidden until a preview exists. Production Envoy rebuilt and linked to `bin/GetDAQ` using Qt 5.12.8. No GUI launch, automated tests, source-file edit flow, or hardware interaction was run.

# 71.753 Receiver catalog source-session validation separated (2026-11-04)

Moved source receiver lease acquisition, seal/configuration binding checks, raw-file identity/size checks, and repeated source snapshot validation from `ReceiverCatalog::Impl` into `receivercatalog_sources.cpp`. The catalog coordinator still validates the target/replacement state, retains validated source leases, writes intents, publishes hard links and the completion receipt. Registered the unit in the production DataSupervisor CMake library. The production `getdaq-data-supervisor-core` library and `getdaq-catalog` executable rebuilt and linked in `/tmp/getdaq-datasupervisor-724`. No tests or catalog/runtime/hardware flow was run; this is a bounded DataSupervisor refactor, not an application-completion audit.

# 71.752 Simulator configuration-workspace generation separated (2026-11-04)

Moved simulator-only XCFG generation and initial `envoy.yml` creation into `apps/simulator/simulator_configuration.cpp`. Moved recursive accounting of existing `.graw` bytes, including symlink/type checks and the aggregate byte-budget gate, into the simulator data unit beside timed output writing. `Simulator::Start` retains listener/service creation and startup coordination. Existing generated file contents/defaults and the rule preserving an existing `envoy.yml` are unchanged. Registered the unit in standalone Simulator and aggregate Qt qmake targets. The standalone Simulator built and linked from a clean Qt 5.12.8 shadow directory; the aggregate `getdaq-tests` executable also rebuilt and linked. No tests, simulator runtime, GUI, receiver, or hardware flow was run.

# 71.751 Native Windows Qt 5.14.2 control build with scan confirmation split (2026-11-04)

Rebuilt the current `control` profile through the installed Windows Qt 5.14.2 / MinGW 7.3 x64 toolchain. All nine programs completed and the receipt marks the build `complete`; every output is x86_64 PE. The build compiled the new scan lifecycle- and Run-confirmation units. All 1,031 source/dependency inputs still match the receipt hashes, and the embedded-dependency check passed offline. The compiler log has no warning/error lines. This is build and architecture evidence only: no tests, GUI launch, deployment smoke or hardware flow was run. Receipt: `build-windows-refactor-751/windows-build-receipt.json`; log: `build-windows-refactor-751/control-windows.log`.

# 71.750 Native scan confirmation responsibilities separated (2026-11-04)

Separated fresh-controller confirmation of scan-point lifecycle operations (Describe/Prepare/Configure, including original-operation recovery observation) from scan Run Start/Stop confirmation and event-target receiver finalization in `nativeruncontrol_scanlifecycleconfirmation.cpp` and `nativeruncontrol_scanrunconfirmation.cpp`. The shared confirmation router retains freshness/state/reconciliation gates and failure quarantine. Registered both units in shared `apps/envoy/core.pri`, used by production Envoy and the aggregate Qt test project. A clean Qt 5.12.8 qmake production Envoy build compiled and linked successfully. Tests, GUI/runtime operation, receiver access and hardware were not run.

# 71.749 Current-source Linux full host build (2026-11-04)

Ran `python3 tools/build-suite.py --profile full --build-root /tmp/getdaq-host-full-748 --jobs 4` from a clean build directory. The receipt status is `complete`; all 30 host programs built and the embedded-dependency manifest verified 109 files. This build includes the Sentry receiver-seal boundary from 71.748 and all current Qt control applications. No test suite, launch smoke, GUI interaction or hardware flow was run. The receipt explicitly does not mark target platform/hardware acceptance: `/tmp/getdaq-host-full-748/full/build-receipt.json`.

# 71.748 Sentry receiver-seal validation boundary (2026-11-04)

Moved Native receiver-seal JSON decoding, exact file-set/size reconciliation, and byte-total verification from `SentryFiles::Catalog` into `sentrycatalog_seal.cpp`; the accumulator now rejects `quint64` overflow. Registered the unit in standalone Sentry, Simulator, and the Qt test project. Fresh Linux Qt 5.12.8 qmake builds of `getdaq-sentry` and `getdaq-simulator` compiled and linked successfully. The native Windows Qt 5.14.2 / MinGW 7.3 control build also rebuilt all nine x86_64 PE programs successfully with no warning/error lines. No tests or runtime/archive operations were run; no receiver or production data was accessed. Receipt and log: `build-windows-refactor-745/windows-build-receipt.json` and `build-windows-refactor-745/control-windows.log`.

# 71.747 Windows Qt warning cleanup and control-suite rebuild (2026-11-04)

Clarified misleading single-line control flow in the Xcfg binary conversion and Envoy transfer, workspace recovery/structure, plan transition, cold handoff, and failed handoff UI code. Removed an unused Xcfg codec helper and replaced deprecated `QSet::fromList` with explicit key insertion compatible with Qt 5.12.8. The Linux production Envoy shadow build at `/tmp/getdaq-envoy-audit-745` incrementally compiled and linked successfully. Rebuilt the native Windows Qt 5.14.2 / MinGW 7.3 `control` profile with `tools/build-windows-from-wsl.py`; all nine x86_64 PE executables succeeded. The new Windows build log contains no compiler warning or error lines. This is build evidence only: no tests, runtime/GUI launch, deployment, macOS or hardware flow was run. Receipt and log: `build-windows-refactor-745/windows-build-receipt.json` and `build-windows-refactor-745/control-windows.log`.

# 71.746 Native Windows Qt 5.14.2 control-suite build (2026-11-04)

Ran `tools/build-windows-from-wsl.py` with the installed Qt 5.14.2 / MinGW 7.3 x64 kit from a fresh `build-windows-refactor-745` directory. The script verified source hashes remained unchanged during the build, then confirmed all nine control executables are x86_64 PE files: GetDAQ, Sentry, Simulator, Config, Config Check, Config Upload, Supervisor Query, Waveform Viewer and Control MCP. This includes the scan-binding index validation from 71.745. Receipt: `build-windows-refactor-745/windows-build-receipt.json`; compiler log: `build-windows-refactor-745/control-windows.log`. Build completed successfully with 27 warning instances emitted across repeated shared-source compilations (misleading indentation, deprecated Qt APIs, and one unused helper); these were corrected and rebuilt at 71.747. No QtTest, launch smoke, manual GUI operation or hardware flow was run; those remain separate gates.

# 71.745 Envoy standalone Qt build and binding-index validation (2026-11-04)

Reviewed the Envoy production qmake source list and rebuilt the standalone `GetDAQ` target from a fresh shadow directory at `/tmp/getdaq-envoy-audit-745`. This compiled the GUI, shared configuration/ECC clients, and integrated waveform viewer, then linked the production executable. The build surfaced a signed/unsigned comparison in scan binding point-index validation; the check now accepts only a non-negative integral JSON number within `unsigned int` range and compares it without signed conversion. The updated translation unit rebuilt and the executable relinked successfully. No tests or runtime flows were run. This establishes the Linux Qt 5.12.8 production build only; Windows, macOS and hardware acceptance remain open.

# 71.744 Configuration repository filesystem adapter separated (2026-11-04)

Moved POSIX repository directory/file identity checks, bounded stable reads, root-path validation, exclusive synced file publication, directory enumeration, and descriptor RAII into private `configurationrepository_files_p.h/.cpp`. The repository implementation retains content-object/version semantics, validation, fault latching, and commit ordering. Registered the adapter in the standalone repository CMake target. Rebuilt `getdaq-configuration-repository` in the Linux full-profile repository tree. No repository regression tests or service scenarios were run; this backend remains POSIX-only. See [configuration repository architecture](CONFIGURATION_REPOSITORY.md) and [coverage inventory](REFACTOR_COVERAGE.md).

# 71.743 SystemPreparation immutable plan compilation separated (2026-11-04)

Moved copied-option normalization, CoBo/MuTaNT topology and cross-device validation, and hardware step-plan compilation out of `SystemPreparationClient` construction into private `systempreparationplan_p.h/.cpp`. The compiler performs no hardware RPC; the coordinator still creates the retained preparation/alignment clients and owns lifecycle ordering. Registered the new unit in `getdaq-hardware-core`. The target rebuilt in the existing Linux full native CMake tree. No simulator or lifecycle tests were run, and this does not establish physical hardware or cross-platform acceptance. See [system preparation architecture](SYSTEM_PREPARATION.md) and [coverage inventory](REFACTOR_COVERAGE.md).

# 71.742 DataReceiver merge output transaction separated (2026-11-04)

Moved MFM merged-event framing, output-part rotation, exclusive output creation, durable close, exact input-frame copying, and rollback cleanup into private `merge_output_p.h/.cpp`. `MergeMfmFiles` retains input indexing, event sorting/filtering, event limits, and result accounting. Registered the module in both production `getdaq-merge` and `merge-tests` source lists. A fresh standalone Linux C++11 build of `getdaq-merge` passed in `/tmp/getdaq-merge-refactor-711`. No merge regression tests or runtime scenarios were run in this step; Windows and macOS are outside this native DataReceiver build.

# 71.741 ICE ingress writer strategies separated (2026-11-04)

Split the optional ICE recorder's persistence strategies into `iceingress_writer_bytes.cpp` and `iceingress_writer_frames.cpp`. The coordinator retains bounded queue, stream-end/failure lifecycle, status and receipt coordination; byte mode writes original payload bytes, while frame mode owns MFM parsing, complete-frame storage and optional export. Updated the strict ICE build source-count contract from 16 to 18 and the application coverage inventory from six to eight production translation units. `cmake --build /tmp/getdaq-host-full-739/full/native --target getdaq-ice-receiver --parallel 4` passed, compiling the affected recorder and receiver target. The strict standalone ICE compile/archive script and its 15 synthetic scenarios were not run. This is a focused Linux build, not a full application, platform or hardware audit. See [ICE ingress architecture](ICE_DATA_INGRESS.md) and [coverage inventory](REFACTOR_COVERAGE.md).

# 71.740 Current-source Linux full host build (2026-11-04)

Configured a fresh `/tmp/getdaq-host-full-739` build and ran `python3 tools/build-suite.py --profile full --build-root /tmp/getdaq-host-full-739 --jobs 4` against the current source, including the two ECC recovery service boundary extractions. The build completed with receipt status `complete`: all 30 host programs built, including `getdaq-handoff-service` and `getdaq-config-upload`; the 109 embedded dependency files passed offline checksum verification. Receipt: `/tmp/getdaq-host-full-739/full/build-receipt.json`. This is a Linux/WSL2 Qt 5.12.8 and GCC 9 toolchain build; it does not verify Windows/macOS native builds or physical hardware. No test suite was run in this milestone; the clean single-invocation 91/91 CTest result remains outstanding.

# 71.739 ECC recovery immutable service policy extracted (2026-11-04)

Moved recovery service configuration decoding, fixed-input pinning/digest binding, canonical task-root and exclusive service-lock acquisition, immutable policy creation, and policy/lease revalidation into private `recoveryservicepolicy_p.h/.cpp`. `RecoveryService::Impl` now keeps task history, authenticated HTTP routing, request admission, and native child orchestration; task-file persistence remains in `recoverytaskstore.cpp`. Rebuilt `getdaq-handoff-service`; the localhost synthetic `cold-handoff-service-0` integration passed against the actual ECC service, two data services, and native receivers. Receipt: `/tmp/getdaq-recovery-policy-738/description-recovery-uhtqmbn4/result.json`, `passed=true`, `raw_remaining=0`. Sanitizers were off and no physical hardware was involved. This is one focused recovery path, not complete ECC/project or platform acceptance.

# 71.738 ECC recovery task storage boundary extracted (2026-11-04)

Moved recovery-task directory loading, saved request/state integrity checks, request creation, and atomic operation-state persistence from `recoveryservice.cpp` into private `recoverytaskstore_p.h/.cpp`. Bounded pinned-file reads and recovery metadata path checks now live in `recoveryfiles_p.h/.cpp`. The HTTP service still owns policy/configuration checks, credentials, lease ownership, request validation, and process orchestration. Rebuilt `getdaq-handoff-service`; the synthetic localhost `cold-handoff-service-0` integration passed with the actual ECC service process, two data services and native receivers. Runner receipt reports `passed=true` and `raw_remaining=0` at `/tmp/getdaq-recovery-store-737-evidence/description-recovery-l2ta7xx2/result.json`. This validates the selected cold-handoff recovery path and cleanup only; it is not hardware, production-data, cross-platform, or full-suite acceptance. Broader ECC and project refactoring remain open.

# 71.737 Remote handoff delivery-history reducer separated (2026-11-04)

Extracted the handoff journal delivery-history state reducer from `RemoteHandoffEvidence::Impl` into private `remotehandoffevidence_delivery_p.h/.cpp`. The filesystem evidence owner still binds and rechecks files/directories, while the reducer validates phase order, submission/rejection/acceptance history, explicit completion receipts and final per-node delivery classification. The reducer remains historical classification only and does not authorize request replay. Registered the new unit in the ECC native CMake library. The rebuilt `getdaq-ecc-native` and `remote-handoff-evidence-tests` targets passed; `remote-cold-audit-evidence` passed 1/1 (0.74 s) with bounded synthetic metadata. No receiver, hardware, production data or non-Linux platform was exercised.

# 71.736 Catalog replacement observation query split (2026-11-04)

Moved the `catalog-replacement-observation` query workflow into private `queryrunner_catalogreplacement_p.h/.cpp`. The helper verifies the original operation is a catalog Publish, fetches its replacement observation, and binds service/plan/instance/controller, operation phase, request identities, allowed replacement phase, zero raw-byte reads, non-authorizing status, and validated paths before returning the response for CLI output. Registered in both CMake query branches and qmake. Linux CMake and strict C++11/pedantic qmake query targets rebuilt; `supervisor-control-service` passed 1/1 (8.65 s), covering accepted/rejected and restart observations. Windows source-registration contracts passed 20/20; no Windows-native build was run. Synthetic local services only; no hardware or production receiver data.

# 71.735 Supervisor query plan-transition proof validation separated (2026-11-04)

Moved validation of plan-retirement and predecessor responses from the query command coordinator into private `queryrunner_plantransition_p.h/.cpp`. The unit binds proof schema, service agent/instance, controller, plan transition target, request key, successor plan digest, and stopped-cycle evidence; the CLI still performs the same identity request, selected GET, error reporting, and output. Registered the unit in both Windows/POSIX CMake query targets and the standalone qmake query project. Linux CMake query CLI and strict C++11/pedantic qmake CLI builds passed. `supervisor-control-service` and `data-plan-transition-staging` passed 2/2 (21.65 s) with local synthetic services. Windows build-entrypoint contract tests passed 20/20; this verifies source registration only, not a Windows compile or runtime. No hardware or production receiver data was used.

# 71.734 DataSupervisor control journal storage boundary (2026-11-04)

Moved durable control-journal filesystem responsibilities out of `controljobs_impl.cpp` into the private `controljobs_journal_p.h` / `controljobs_journal.cpp` component. It now owns exclusive directory bootstrap, immutable policy identity, journal open/read bounds, append/fsync, newline-integrity validation, and directory/file identity and content checks. `SupervisorControl::Impl` retains policy construction, replay into operation state, sequence/event meaning, reconciliation gates and command execution. Journal JSON rows, limits, error behavior, and startup recovery policy remain unchanged. Registered the source in the DataSupervisor CMake core; the Windows query-only branch does not compile this POSIX service component.

Configured a fresh complete native CMake tree with ICE receiver and services enabled. Rebuilt `getdaq-data-supervisor-core`, the control/process test executables, `getdaq-supervisor`, `getdaq-supervisor-query`, and `getdaq-catalog` successfully. The focused `supervisor-control-codec`, `supervisor-control-service`, and `data-supervisor-process-lifecycle` CTests passed **3/3** with local loopback enabled. The service integration exercised journal-fault and restart/reopen scenarios; evidence is synthetic/local and does not establish physical hardware behavior. Full native, Qt Widgets, Windows/macOS and target-hardware suites were not rerun. Overall application refactoring remains open.

# 71.733 Full native CMake build and 91-case regression (2026-10-27)

A fresh complete native CMake profile enabled the ICE receiver and both services. Its first build exposed that `getdaq-config-check` did not compile the newly extracted `apps/configcheck/artifactcommands.cpp`; the source was added to `common/experiment/CMakeLists.txt`, after which the complete native build passed. The tree registers 91 CTest cases. With loopback enabled and four workers, 90/91 passed in one run (19m 14s). `fixed-ecc-configuration-and-scan-deployment` initially failed because its fixed evidence directory already contained synthetic output from an earlier run; after verifying the directory belonged to this isolated build and removing it, the case passed separately (1/1, 2.17s). All 91 cases therefore have passing evidence, but not from one clean 91/91 invocation. Several system lifecycle/fault scenarios take 1–8 minutes, so this is a slow acceptance suite rather than a focused edit-level test. The independent Qt Widgets suite, Windows/macOS builds, and hardware acceptance were not run here. No physical hardware or production DAQ data was used.

# 71.732 Clean native CMake build and 85-case CTest audit (2026-10-27)

Configured and built the native aggregation from current source in a fresh `/tmp/getdaq-native-730-current` tree. The full native build succeeded and registered 85 CTest cases. With loopback permitted and four workers, 84/85 passed in the full invocation. The remaining fixed-ECC case refused to create its evidence directory because an earlier interrupted attempt had left an empty directory behind; after inspecting and removing that directory owned by the isolated build, the case passed separately (1/1, 2.15 s). The complete 85-case suite was not rerun after cleanup. This was a receiver-disabled native profile; the expanded complete profile and its 91-case results are recorded at 71.733. In a separate clean Linux full-profile build from the same current source, all 30 programs were built and runtime-inspected; 109 embedded dependency inputs were verified, and the receipt reports `complete`. No physical hardware or production DAQ data was used.

# 71.731 Configuration artifact verification commands separated (2026-10-27)

Moved the `getdaq-config-check` configuration snapshot, publication, experiment, selection, registration, and readback-audit command handling into `apps/configcheck/artifactcommands.{h,cpp}`. `main.cpp` now coordinates the artifact and already-separated scan command families and owns the shared usage text. Command forms, JSON output, error messages, and exit-code behavior are preserved. Registered the new implementation in the standalone qmake project.

Built the production CLI from a fresh `/tmp/getdaq-configcheck-731` directory with Qt 5.12.8/GCC in strict C++11 mode. `--help` returned 0 with all command forms, no arguments returned 2, and a missing snapshot returned 1 with the existing diagnostic. Positive CLI invocations passed for all six artifact families: snapshot, publication, experiment, selection, registration, and readback-audit. Extended `tests/system_experiment_integration.py` to exercise `getdaq-config-check experiment` against the current generated system artifact and check its configuration digest, artifact digest and hardware-verification flag; the current configtool/system-config/checker integration passed all 95 process cases. No hardware, network service, or DAQ data was used. Full application/platform audit remains open.

# 71.730 Envoy log persistence separated from presentation (2026-10-27)

Moved application-log and per-connection session-file ownership into `apps/envoy/consolelogstore.{h,cpp}`. The store now owns the existing user-data → cache → process-temporary fallback order, unique connection filenames, append/flush, and session close; `MainWindow` retains UI formatting, severity coloring, connection transition decisions, and visible-log export. Registered the new implementation in the production Envoy and aggregate Qt test qmake projects. Rebuilt the full `getdaq-tests` target and the standalone production Envoy from isolated `/tmp` build directories. The focused operation-log and severity tests passed **5/5** including QtTest setup/cleanup: separate connection files, session close markers, visible export, original warning/error text and color, and bounded-log severity metadata. The standalone Envoy launched offscreen and created an application log containing its ready message. A separate launch with `XDG_DATA_HOME=/dev/null` verified fallback to the cache log directory and recorded the fallback notice. No hardware or production DAQ data was accessed. The full Widgets suite, Windows/macOS builds and target-hardware acceptance were not rerun.

# 71.729 CoBo LED plan generation separated (2026-10-27)

Moved CoBo LED plan construction from `cobomonitoring.cpp` to `coboled.cpp`, with a private append helper shared by the AsAd power-cycle plan. The monitoring unit now keeps only AsAd power, protection, sample-read and alarm-monitor plan generation. Existing LED command fields, validation and annotations are preserved; the hardware core source registration includes the new unit. Rebuilt `getdaq-hardware`, `getdaq-system-config`, and the monitoring/services test targets.

Added direct assertions for set-all version binding, per-AsAd modify fields, blink, single/all flip, and pulse/stop payloads. The localhost mock-service CTests `hardware-monitoring-tcp` and `hardware-services-tcp` passed **2/2** (14.20 s and 10.96 s); they use synthetic service state and no physical hardware. Test-owned temporary directories were removed. Full native/Qt suites and Windows/macOS acceptance were not rerun.

# 71.728 Hardware description payload conversion separated (2026-10-27)

Extracted conversion from `HardwareDescription` to the Ice `NodeConfig` plus its canonical creation-audit JSON into private `hardwareclient_description.cpp`. `HardwareClient::Create` retains validation, locking, audit publication, UNKNOWN fencing, the remote create call, and post-create metadata verification. The configured native CMake build rebuilt the hardware core, hardware client/system-description test targets, and all four production hardware CLIs (`getdaq-hardware`, `getdaq-align`, `getdaq-system-config`, `getdaq-system-describe`).

The localhost mock-service CTests `hardware-system-description-tcp` and `hardware-client-tcp` passed **2/2** (5.86 s and 217.60 s). The first sandboxed attempt could not bind local sockets; the same tests passed with loopback permission. These tests use synthetic service descriptions only; no physical hardware or production data was accessed. Full 151-test native/Qt suites and Windows/macOS acceptance were not rerun.

# 71.727 Configtool direct-source probe rebuilt (2026-10-27)

The strict qmake `tests/configtool_probe.pro` target was built from a fresh temporary directory with Qt 5.12.8/GCC and `-pedantic-errors`; the resulting `getdaq-config-probe --help` returned 0. This closes the earlier 71.708 gap where the probe Makefile listed the new parser but its executable was not rebuilt. The build surfaced a misleading-indentation warning in `XcfgSaveJournal::Load`; the loop and return were separated without changing behavior, then the probe was rebuilt successfully.

Using that direct-source probe binary and an isolated `XDG_CACHE_HOME`, AGET, MuTaNT (50 contracts), CoBo Configure, readout, clock, monitoring (31 contracts), and calibration (33 contracts) CLI integration suites all passed. These are offline configuration tests; they did not access hardware, external services or production DAQ data. The complete Qt Widgets and native suites, Windows/macOS builds and hardware acceptance were not rerun.

# 71.726 Self-contained configuration repository core CTest (2026-10-27)

Extended the standalone repository core test to generate a temporary, valid synthetic publication package and run the existing full filesystem core scenarios without depending on a prior frozen build artifact. CTest `configuration-repository-core` now covers object validation, duplicate commit, publication verification, reopen/list/version, same-plan interrupted commit recovery, foreign intent rejection, corruption/fault latching, and credential-file integrity. The fixture, including its generated credential, is removed by the owned temporary-directory guard.

The strict C++11/pedantic repository build passed **51 compile commands over 121 tracked inputs**; the complete core CTest passed **1/1**. The network/Qt service integration was not run in this check. No network, hardware, or DAQ data were used.

# 71.725 Configuration repository object-store CTest registration (2026-10-27)

The standalone `apps/configrepository` CMake project built `configuration-repository-tests` but did not register any CTest case. Added a self-contained `--basic-smoke` mode using an exclusively owned temporary directory, and registered it as `configuration-repository-object-store`. It verifies repository identity, exclusive writer locking, duplicate/hash/size/path guards, object readback, reopen persistence, and empty-version listing; cleanup removes its private fixture.

`python3 tests/build_configuration_repository_validation.py /tmp/getdaq-configrepo-ctest-725` passed **51 strict C++11/pedantic compiles over 118 inputs**. The new CTest passed **1/1** and left no fixture directory. This smoke does not replace the existing full publication/commit/recovery runner, which requires a validated publication manifest; no current-source full repository/network integration was run. No network, hardware, credentials, or DAQ data were used.

# 71.724 getdaq-catalog scan and transfer CLI responsibility split (2026-10-27)

Moved the scan-point binding, scan summary, threshold/AutoGain analysis, and Pulser/PeakingTime analysis dispatch and numeric argument validation from the monolithic `catalogmain.cpp` into `catalogscancli.cpp` with a narrow `HandleScanCommand` entry point. Also moved catalog copy/resume, verification, TCP/SSH/stream transport dispatch, port parsing, and progress-file reporting into `catalogtransfercli.cpp`. The supported argument forms, numeric parsing rules, progress output, and existing validation diagnostics were retained. The main entry point now delegates those two coherent command groups and retains request/replacement orchestration.

Rebuilt the selected native targets in `/tmp/getdaq-native-717-clean`; `receiver-catalog-filesystem` passed **1/1** there. Then `tests/build_data_supervisor_validation.py /tmp/getdaq-datasupervisor-724` passed the standalone strict C++11/pedantic build: **154 compile commands and 264 tracked inputs**. In that build, `receiver-catalog-filesystem` and `catalog-transfer-filesystem` both passed (**2/2**) with localhost loopback enabled. These are the affected catalog/transfer regressions; the 151-test native suite was not rerun end to end. Windows/macOS/hardware gates remain open.

# 71.723 Current-source Linux full host profile after DataReceiver and ICE test changes (2026-10-27)

Rebuilt `python3 tools/build-suite.py --profile full --build-root /tmp/getdaq-host-full-716 --jobs 4` from the current source. The receipt reports `status=complete`, all **30 programs** built, and 109 embedded dependency inputs checked. The `getdaq-inspect` receipt confirms only system C/C++ runtime libraries and no Qt/ROOT/Ice data-tool dependency.

Ran the current full-profile `getdaq-inspect` binary through `tests/run_hitpattern_scan_package_test.py`; all **9 CLI scenarios** passed, including the generic structure-inspection path and both hit-pattern modes. The runner generated and deleted 280 bytes of synthetic frame fixtures; a scan found zero `.graw` files remaining in the full build tree. No physical hardware or production acquisition data was used. This is Linux build and CLI evidence, not platform or hardware acceptance. The expanded 151-test CTest registry has targeted changed-area results but has not yet had a complete run.

# 71.722 ICE source-control regression registration and small-app audit (2026-10-27)

Reviewed the remaining review-only ICE-related app boundaries. `getdaq-ice-check` is a compact read-only ping/status CLI; `ControlledIceSource` owns producer/stream lifecycle while `SourceControlServant` adapts the generated Ice RPC surface; `SourceRingReader`, `SourceRingReadout`, and `SourceMfmRing` each own distinct ring-consumption, worker-lifecycle, and MFM-boundary responsibilities. No additional source split was warranted in these small units.

Registered `source-fault-commit-tests` as `controlled-ice-fault-commit` and registered the existing `run_ice_source_control_tests.py` harness as `ice-source-control-lifecycle` when the optional native ICE receiver and test targets are enabled. This exercises real local RPC/receiver boundaries with bounded synthetic frames and automatic raw cleanup. Rebuilt the selected programs and passed **6/6** targeted CTest cases: ICE check help, the 19-case source-control/receiver integration, source-fault commit, and all three source-ring tests. The 19-case receipt reports `raw_deleted=true`; a follow-up scan found zero `.graw` files in the isolated build tree. No physical hardware or production DAQ data was used. The previous full native run remains 149/149 at 71.718; a full run of the expanded 151-test registration has not been performed. Windows/macOS and hardware gates remain open.

# 71.721 DataReceiver inspect CLI responsibility split (2026-10-27)

Moved the CoBo hit-pattern and threshold-scan command handling out of `inspect.cpp` into `inspect_hitpattern.cpp`; `inspect.cpp` now owns the generic streaming MFM structure-inspection command and dispatches the two hit-pattern modes through a narrow handler. CLI arguments, output schema, error handling, and counting behavior are unchanged. Registered the new unit in the standalone DataReceiver CMake target.

Added the existing synthetic hit-pattern package regression as the `hitpattern-cli` CTest, so help, generic structural inspection, per-file CSV, multi-file aggregation, mixed endianness, mapping disclaimer, invalid ordering/input, and temporary fixture cleanup are checked on each standalone build. GCC 9.4/C++11 built all default DataReceiver programs and test targets. The complete standalone suite passed **8/8** with localhost loopback permitted. Its first sandboxed attempt passed 6/8 because the two socket tests could not create local sockets; the same suite then passed 8/8 without source changes when run with loopback access. The hit-pattern CLI test created and removed only small synthetic frames. The aggregate Linux `data` host profile then built all 6 programs, including the separated inspector, and the revised CLI integration test passed again. Project Markdown link validation passed 2,188 links. No physical hardware or production DAQ data was used. Windows/macOS DataReceiver validation and the broader application responsibility audit remain open.

# 71.720 Windows Qt 5.14.2 test finding and pending verification (2026-10-27)

The user's Qt 5.14.2/MinGW 7.3 full control profile built 9 x86_64 programs from the source before the current selection-publication guard. The native Windows Widgets run then reported **168 passed, 1 failed, 11 skipped**. The failure was `configurationSelectionFilePublicationIsAtomicAndNoReplace`: attempting to publish to an existing directory left an extra temporary file in the directory's parent. The test runner removed its owned temporary root and reported zero remaining `.graw` files/bytes.

`apps/configtransfer/selectionfile.cpp` now rejects an existing destination (including a symlink) before creating its temporary file; the Linux Qt 5.12.8 aggregate target rebuilt and the focused test passed 3/3. Windows has not verified this fix: qmake could not start because WSL-to-Windows interop returned `UtilBindVsockAnyPort: socket failed 1` before the build ran. Retry a current-source Windows build and targeted Widgets test only after interop is available; run the full Windows suites when the targeted test passes or a broader source change warrants it. This remains a platform gate, not a Windows pass.

# 71.719 Full current-source Qt Widgets suite 180/180 (2026-10-27)

Rebuilt `tests/tests.pro` with Qt 5.12.8/GCC 9.3 in `/tmp/getdaq-qt-718`, then ran the complete `getdaq-tests` target with `QT_QPA_PLATFORM=offscreen`, an isolated HOME/XDG profile, and localhost loopback. Set `GETDAQ_TEST_OFFLINE_TOOLS_DIR=/tmp/getdaq-host-full-716/full/receiver` so tests invoked the current `getdaq-inspect`, `getdaq-matrix` and `getdaq-merge` builds. QtTest completed **180 passed, 0 failed, 0 skipped** in **112.729 seconds**. The suite exercises the Run UI, automatic Run policy, event-log coloring, connection-scoped operation logs, export, waveform viewer gating, offline data-tool execution/window closing, configuration workspaces and Simulator integration. The isolated QtTest fixture root was removed with no `.graw` leftovers. No physical hardware or production DAQ data was used; Windows/macOS and target hardware remain unverified.

# 71.718 Current-source native regression suite 149/149 (2026-10-27)

Configured and built the current source in a new `/tmp/getdaq-native-717-clean` tree using `cmake -S third_party/ice-compat -B /tmp/getdaq-native-717-clean -DCMAKE_BUILD_TYPE=Release -DGETDAQ_BUILD_ICE_RECEIVER=ON -DGETDAQ_RECEIVER_EXECUTABLE=/tmp/getdaq-host-full-716/full/receiver/getdaq-receiver -DCMAKE_PREFIX_PATH=/usr`, then `cmake --build /tmp/getdaq-native-717-clean --parallel 4`. CTest registered **149 tests**. With localhost loopback permitted, `ctest --output-on-failure -j1` completed **149/149 passed, 0 failed**, in **5,537.69 seconds**. The suite includes hardware service lifecycle, acquisition failure recovery, supervisor plan transitions, ECC network/alarms, remote receiver and ICE source-ring contracts. The previously failing retirement, handoff and plan-transition cases passed in the full run.

After completion, the isolated build tree contained 71 synthetic `.graw` test artifacts (1,777,669 bytes); these were deleted and a follow-up scan confirmed zero remaining `.graw` files. No production DAQ data or physical hardware was used. This validates Linux host tests only, not Qt Widgets, Windows/macOS, or target hardware.

# 71.717 Full Linux host profile and supervisor qmake registration (2026-10-27)

The clean `full` host profile initially stopped at `getdaq-supervisor-query` because `getdaq-supervisor-query.pro` omitted `controlprotocol_request_validation.cpp`; the CMake library already included that implementation. Registered the source in the qmake target, then reran `python3 tools/build-suite.py --profile full --build-root /tmp/getdaq-host-full-716 --jobs 4`. The receipt reports `status: complete`, verifies 109 embedded dependency files and records all **30 programs** built with runtime dependency inspection.

This verifies Linux host-profile compilation/linking only. The build receipt explicitly marks target-platform and hardware acceptance false. It does not run the full CTest/QtTest suites; the 149-test native rerun after the retirement-state fix, per-application regression suites, and Windows/macOS acceptance remain outstanding. No physical hardware or production DAQ data was used.

# 71.712 Plan retirement state and full-suite audit (2026-10-27)

The first post-71.707 complete native CTest run executed all **149 tests** in 5,451.24 seconds: **143 passed and 6 failed**. `data-plan-transition-staging` found that a normally successful plan retirement left `control_needs_reconciliation` set because the implementation coupled that status to `planRetired`. Normal retirement now clears that status while keeping `planRetired` latched, so the independent plan-retirement command gate still refuses every new command for that instance. Failed-retirement recovery remains distinct and retains its reconciliation hold. Rebuilt the full native tree and reran `failed-retirement-completion-service`, `supervisor-control-service`, `data-plan-transition-staging`, and `data-supervisor-process-lifecycle`; all **4/4** passed.

The other five failures were exclusive-create collisions in evidence paths retained by earlier runs in this reused build tree. `fixed-ecc-configuration-tests` passed with a fresh evidence directory. `native-ecc-receiver-tests` passed all four `normal-0`, `normal-1`, `truncated`, and `disconnect-ack-loss` scenarios using unique audit paths. Thus all six failed cases have passed targeted checks on the corrected source. The full 149-test suite has **not** been rerun after the final retirement-state distinction; do not report the suite as fully green. The first CTest tree used receiver and native binaries refreshed at 71.707; the standalone DataReceiver 7/7 and ICE options 1/1 checks remain the 71.709 evidence. Tests use synthetic data and loopback services only; no real hardware, production DAQ data, optional ROOT/FITS in this run, macOS, or Windows was exercised.

# 71.711 Simulator ECC control protocol separated (2026-10-27)

Moved the simulator's SOAP response builder and ECC operation parsing/state-transition policy to `apps/simulator/simulator_control.cpp`. `simulator.cpp` retains service startup, local ports, configuration directory creation and Sentry setup; generated `.graw` output remains in its dedicated writer. Registered the new source in `apps/simulator/simulator.pro` and `tests/tests.pro`.

Qt 5.12.8/GCC 9.3 rebuilt the standalone Simulator and the full Qt test executable. `standaloneSimulatorIntegration` passed through Describe, Prepare, Configure, Start, Stop and Sentry catalog/Run finalization; `simulatorGeneratedDataBudgetIsHard` passed its fixed-budget and restart-refusal checks. Both tests used local loopback and synthetic temporary files, with no physical hardware or production DAQ data. The full Widgets suite and Windows/macOS Simulator builds remain unverified in this milestone.

# 71.710 Sentry configuration loading separated (2026-10-27)

Moved Sentry configuration-file loading, JSON field/type/range checks, path resolution and `SentryConfig::Validate` into `apps/sentry/sentryconfig.cpp`. The service unit now retains construction and listen startup. Registered the source in standalone Sentry and Simulator qmake targets and in the complete Qt test target.

Qt 5.12.8/GCC 9.3 rebuilt the Linux Sentry and Simulator production targets and the complete `getdaq-tests` executable. Six focused Sentry tests passed: strict configuration fields, catalog files, explicit replacement, incomplete-run rejection, replacement resume and HTTP endpoints. HTTP used a local loopback fixture; catalog tests used synthetic temporary files. No real DAQ data or hardware. The full Qt test suite, Windows/macOS targets and physical Sentry hosts were not tested in this milestone.

# 71.709 Receiver CLI parsing separated and MiB scaling corrected (2026-10-27)

Extracted `getdaq-receiver` command-line parsing and usage printing into `receiver_options.{h,cpp}`, leaving identity policy and service startup in `main.cpp`. Added parser coverage for numeric bounds/conversions, managed identity fields, help short-circuit behavior and invalid diagnostics. The audit also found that both native and ICE receiver parsers scaled `--max-frame-mib` and `--export-queue-mib` as GiB despite the documented MiB units; corrected both and retained the intended MiB caps.

Linux CMake rebuilt the production native receiver and ICE receiver. The complete default DataReceiver suite passed **7/7** (`receiver-options`, merge, matrix conversion, CoBo hit pattern, receiver core, exporter lock and byte-storage integration); ICE ingress option tests passed **1/1**, including 32 MiB max-frame and 16 MiB export budgets. The loopback tests first failed under the sandbox's socket restriction and passed when run with local-network permission. Tests used synthetic data only; optional ROOT/FITS targets, macOS and physical hardware were not exercised in this milestone. Full host-suite and platform refactoring remains open.

# 71.708 Register compiler CLI option parsing separated (2026-10-27)

Moved command recognition and validation of AGET IDs/versions, CoBo version maps, clock versions, monitoring mappings, calibration layouts, and MuTaNT masks/layout/clock policy into a dedicated private parser. The command executor now performs usage, XCFG loading, and compiler dispatch from the parsed options. Production and direct-source probe qmake projects both register the new unit.

The Linux Qt 5.12.8/GCC production `getdaq-config` target rebuilt successfully. The AGET compiler integration passed (golden register images, whole-CoBo/mixed versions, provenance and failure/no-output contracts); the MuTaNT (50 contracts), CoBo Configure, readout, clock, monitoring (31 contracts), and calibration (33 contracts) CLI integration suites also passed using an isolated temporary configuration transaction cache. `--help` retained its successful usage output, and a malformed AGET identifier returned the original validation diagnostic and status 2. An initial AGET test attempt hit a busy default shared transaction cache; rerunning with a fresh temporary cache passed. The direct-source probe qmake project generated a Makefile containing the new parser source/object, but that probe executable was not rebuilt in this milestone. No physical hardware or production acquisition data was used.

# 71.707 Linux full-build audit and plan-retirement recovery fixes (2026-10-27)

Built the three independent Linux CMake trees used by this delivery layout (`full/native`, `full/receiver`, and `full/repository`). A 149-test run from `full/native` completed in 3,948.98 seconds with 146 passing and three failures. Two observation failures came from testing against a stale `full/receiver/getdaq-receiver` binary because only the native tree had been rebuilt. The retirement-completion failure exposed that a successfully repaired retirement incorrectly cleared the old plan's reconciliation gate. The retirement transaction also lacked a post-publication check of the reviewed successor-plan source and could write one metadata file before discovering a conflicting later file.

The supervisor now keeps the retired plan fenced after completion, preflights all existing retirement metadata before publishing any of it, and revalidates the successor-plan source after metadata publication. After rebuilding the receiver and native trees, all three previously failing tests passed in isolation: `failed-retirement-completion-service`, `supervisor-control-service`, and `data-supervisor-process-lifecycle` (3/3). The full 149-test suite has not yet been rerun after these changes; platform-wide and per-application completion claims remain open. No physical hardware or production DAQ data was used.

# 71.706 T2K ROOT event decoding separated (2026-10-27)

Moved `TT2K` / `T2KEvent` branch support detection, binding, 272×512 shape validation, 12-bit sample validation and event conversion into `apps/rootviewer/t2keventadapter.{h,cpp}`. `RootTreeReader` now handles file/tree selection and delegates T2K and ROOT5-era data to format-specific adapters while retaining current ROOT6 `GET::GDataFrame` decoding. Registered the adapter in the optional GUI and both reader-based test executables.

With ROOT 6.26 and Qt 5.12.8, the optional `getdaq-root-viewer`, `root-viewer-tests`, `root-statistics-tests` and `root-statistics-scan-tests` targets built. CTest `root-viewer-reader`, `root-statistics` and `root-statistics-scan` passed (3/3), including current ROOT6, ROOT5-era, T2K matrix, malformed-dimension and histogram-scan fixtures. The `getdaq-receiver` production target and MFM merge, matrix conversion, CoBo hit-pattern, receiver-core and exporter-lock tests also built; all five tests passed. The first exporter-lock attempt hit the sandbox's socket restriction and passed on retry with local test socket permission. The tests use temporary synthetic files only; no hardware or production DAQ data was accessed. Documentation links passed **2,169** checks.

# 71.705 Transfer worker process isolated (2026-10-27)

Moved `TransferJobs::Impl::Work` to `apps/datasupervisor/transferjobs_worker.cpp` and introduced a private declaration header so worker process code builds independently from service state and API coordination. `transferjobs.cpp` retains service initialization, fixed policy/lease checks, job persistence, submissions and explicit recovery coordination. Registered the new translation unit in `apps/datasupervisor/CMakeLists.txt`.

The strict Linux C++11 data-supervisor core and `transfer-jobs-tests` rebuilt. The native transfer-jobs service integration runner passed: actual child process, authenticated HTTP, no duplicate/restart replay, unfinished restart quarantine, explicit verification and continuation, lost-response observation, unchanged receiver state, and preserved prefix inode. The runner reports a 64 MiB maximum aggregate synthetic payload; cleanup reported no remaining raw files. The first sandboxed runner attempt was denied loopback socket creation; the same synthetic integration suite passed when run with local-network permission. No physical hardware or production data was used.

# 71.704 Windows scan journal file I/O separated (2026-10-27)

Moved Win32 journal-file reads, temporary-file validation/writes, and create/atomic-replace publication from `scanexecutionjournalstore_windows.cpp` into `scanexecutionjournalstore_windows_io.cpp` and its private header. The store retains secure directory and lock ownership, active writer checks, journal transition validation and fault latching. Registered the helper in the Windows ECC CMake target and Envoy qmake project. The extraction preserves the existing ACL, reparse-point, file-identity, complete-read/write and atomic-publication checks.

The Linux ECC journal regression executable passed, covering journal ordering, private atomic storage, restart quarantine, tamper and link refusal; the ECC state matrix executable also passed. Documentation links passed **2,169** checks. The current environment has neither a MinGW cross-compiler nor a usable native Windows Qt build, so the changed Win32 source and native Windows journal safety suite remain unverified in this milestone. No hardware or production acquisition data was used.

# 71.703 Xcfg workspace edit history separated (2026-10-27)

Moved `StageSource`, `StageSources`, `StageSourcesImpl`, `ApplyHistory`, `Undo` and `Redo` into `common/config/xcfgworkspace_edits.cpp`. The unit now contains staged byte validation, detached include-dependency checks, atomic in-memory state update, bounded undo/redo history and history-step revalidation. Registered it in `common/config/xcfg.pri` and `tests/xcfg_shared.pro`.

Qt 5.12.8 strict C++11/pedantic shared Xcfg build passed and all **151/151** tests passed. The Linux production Envoy target rebuilt and linked with the new shared qmake source. No hardware or production acquisition data was used.

# 71.702 Xcfg workspace graph building separated (2026-10-27)

Moved `XcfgWorkspace::Build` and its shared workspace source-budget check into `common/config/xcfgworkspace_build.cpp`. It owns multi-root graph reads, declaration validation (including templates), effective-snapshot capture, shared-source consistency and the 256-file/8 MiB-per-file/64 MiB-total limits. The workspace coordinator retains source opening, staging/history and preview flows. Registered the implementation in `common/config/xcfg.pri` and `tests/xcfg_shared.pro`.

Qt 5.12.8 strict C++11/pedantic shared Xcfg build passed; all **151/151** tests passed using an isolated cache. The direct-source configuration-transaction probe rebuilt and its cooperating-process runner passed. The Linux production Envoy target rebuilt and linked with the new shared qmake source. Documentation links passed **2,167** checks. No hardware or production acquisition data was used.

# 71.701 Remote receiver mutation intent persistence separated (2026-10-27)

Extracted `RemoteReceiverManager::Impl::PersistMutationIntent` into `apps/ecc/native/remotereceivermanager_mutationintent.cpp`. It persists the accepted request list, optional original handoff context and audit proof identities. The `Mutation` coordinator still performs authorization and live state/plan/source validation before it creates requests; `ExecuteMutation` still owns sequential POSTs, audit progression, result verification and failure reconciliation. Registered the new source in `apps/ecc/native/CMakeLists.txt`.

The strict C++11 native ECC library rebuilt successfully. Seven local `native-ecc-remote-receiver` integration scenarios passed: normal-0, normal-1, partial-start, catalog-replace, plan-mismatch, audit-fault and ownership. The failed-handoff-parent-completion integration test also passed, confirming explicit node repair, successor adoption and no parent repair POST; its cleanup receipt records eight synthetic raw files deleted and zero remaining. No physical hardware or production acquisition data was used.

# 71.700 Workspace Save All transaction separated (2026-10-27)

Moved the full `XcfgWorkspace::SaveAll` member implementation to `common/config/xcfgworkspace_save.cpp`; the workspace core retains opening, source checks, staging/history and preview creation. Registered the source in `common/config/xcfg.pri` and the direct-source `tests/xcfg_shared.pro`. The moved member continues to own the same prepare-before-replace, journal creation, file identity/source rechecks, synchronization and incomplete-save recovery behavior.

Qt 5.12.8 strict C++11/pedantic builds passed for the shared Xcfg suite and dedicated configuration-transaction probe. The shared suite passed **151/151** tests with an isolated temporary HOME/cache. The process-level transaction runner passed cooperating source transaction processes without DAQ payload. The Linux production Envoy target also rebuilt successfully through `apps/envoy/envoy.pro`, exercising the shared `common/config/xcfg.pri` source registration. Documentation-link validation passed **2,166** local links. The initial suite attempt against the user default cache saw an existing reader reservation and refused access; rerunning against an isolated cache passed, and the user cache was left untouched. A Windows Qt 5.14.2/MinGW build attempt could not start because the current WSL-to-Windows socket bridge failed before `qmake.exe` ran. No hardware or production acquisition data was used.

# 71.699 Supervisor control command dispatch separated (2026-10-27)

Moved the `SupervisorControl::Impl::Run` command switch into `controljobs_dispatch.cpp` as `Impl::Dispatch`; `Run` keeps the operation mutex, request-file identity/content check before and after dispatch, operation journal transitions, failure recovery eligibility and reconciliation/result-state updates. The source block was compared with its pre-extraction snapshot and matched after normalizing indentation and the boundary newline. Registered the new source in `apps/datasupervisor/CMakeLists.txt`.

`tests/build_data_supervisor_validation.py /tmp/getdaq-71-699-supervisor` passed the repository's strict C++11/pedantic data-supervisor build: 151 compile commands, 258 tracked inputs, and dependency checks. The local service integration runner passed its `normal` and `catalog` scenarios, all eight native HTTP fault cases, codec and credential checks; it observed 147,456 bytes in six synthetic `.graw` files and removed all six (zero remained). The plan-transition `success` case passed with a changed successor plan and removed its owned synthetic payload. The controller-handoff `success` case passed while preserving the original sealed raw data during the test and cleaning its own fixtures. The fault-injected failed-handoff completion case passed explicit completion, successor-owner use and cold-restart fencing, then removed its synthetic raw. The `supervisor-control-codec` CTest passed (1/1), and documentation-link validation passed 2,165 local links. No physical hardware or production acquisition data was used.

# 71.698 RunControl monitoring and Sentry observation callbacks separated (2026-10-27)

Moved legacy node freshness/state queries, status publication, periodic ECC/Sentry polling, Sentry status replies and replacement-finalization acknowledgements from `apps/envoy/runcontrol.cpp` to `runcontrol_observation.cpp`; the original controller retains connect/disconnect/shutdown setup and journal writing. Registered the new source through `apps/envoy/core.pri`, which is shared by production Envoy and the GUI test projects.

Reconfigured and rebuilt the complete Linux Qt 5.12.8/C++11 `getdaq-tests` Widgets target and production Envoy. The focused loopback-backed legacy control/finalization cases—independent node progress/regress, SOAP request compatibility, catalog timeout handling and backup retry without recatalog—passed **6/6 QtTest results** including setup/cleanup. The complete Windows Qt 5.14.2/MinGW strict C++11 Widgets and ECC-client test targets were then reconfigured and rebuilt with the extracted source. The native Windows suite had passed immediately before this source-only relocation (Widgets 168 passed, 0 failed, 11 skipped; ECC client 70 passed, 0 failed, 0 skipped), but those Windows runtime tests were not rerun for 71.698. No hardware or production acquisition data was used.

# 71.697 ECC client store responsibilities separated (2026-10-27)

Split the `EccClientStore` private implementation declaration and durable replay/state checks, query APIs, retirement APIs and write/observation mutations into `eccclientstore_impl_p.h`, `eccclientstore_state.cpp`, `eccclientstore_queries.cpp`, `eccclientstore_retirement.cpp` and `eccclientstore_mutations.cpp`. The façade retains construction/destruction. Registered the new units in shared `apps/envoy/core.pri` and both direct-source ECC test projects (`tests/ecc_system_client.pro` and `tests/input_fault_client_probe.pro`). The move preserved existing behavior and introduced no policy change.

Linux Qt 5.12.8 strict C++11 builds succeeded for production Envoy, the ECC system-client suite, the input-fault probe and the complete Widgets test target. The ECC-client suite passed **70/70**; `nativeCatalogRecoveryPolicyRequiresExactLivePreparedReplacement` passed **3/3** including setup/cleanup. The strict Qt 5.14.2/MinGW 7.3 Windows build completed for Widgets and ECC-client test programs. A fresh native Windows run reported Widgets **168 passed, 0 failed, 11 skipped** and ECC client **70 passed, 0 failed, 0 skipped**; both reported zero remaining raw files/bytes, and the isolated Windows test root was removed. No physical hardware or production acquisition data was used.

# 71.696 Windows scan journal filesystem support split and native validation (2026-10-27)

Moved the Win32-only support primitives for `ScanExecutionJournalStore` into `scanexecutionjournalstore_windows_support.cpp` with private declarations in `scanexecutionjournalstore_windows_support_p.h`. The main store still owns durable journal transitions, lock scope, mutation detection and atomic replacement. Both Windows CMake and Envoy qmake targets register the helper. The Windows Qt 5.14.2/MinGW strict C++11 build completed and deployed the complete Widgets and ECC-client test programs. Native Windows QtTest reported Widgets **168 passed, 0 failed, 11 skipped** and ECC client **70 passed, 0 failed, 0 skipped**; the skips are existing Unix-only fixtures or tests requiring separately built offline tools. Both test processes reported zero remaining raw files/bytes, and the isolated Windows test root was removed.

The dedicated Windows journal-store regression was also compiled with the same MinGW 7.3 compiler against the CMake target's experiment/control sources and the production Win32 store files, then run as a native Windows process. It passed the private ACL, exclusive-writer, restart quarantine, hard-link rejection and external-tamper checks. Local Windows CMake was unavailable, so this run verifies the dedicated CMake test source and production files directly rather than the CMake generator/build step. Separately, Linux Qt 5.12.8/GCC 9.3 rebuilt the full Widgets test target; the focused operation-log export and offline-tool close-control group passed **6/6** QtTest results including initialization/cleanup. The moved filesystem helper function bodies were checked against the pre-split source and remained identical. Documentation-link validation passed **2,162** local links. No hardware or production DAQ data was accessed.

During Windows validation, strict flags exposed Qt 5.14 `QSKIP` variadic-macro diagnostics and a C++14 default argument on a lambda. The tests now call the public `QTest::qSkip` function for explicit platform skips, and the status-label helper receives explicit boolean values; Windows and Linux builds pass with strict C++11. The Windows visible-log export test now disables native dialogs locally so its existing QFileDialog automation behaves consistently on Windows; the product still uses the platform's normal dialog. This milestone does not certify all applications, Windows CMake packaging, macOS, physical hardware or production network behavior; overall refactoring remains open.

# 71.695 Remote receiver mutation execution and cold evidence separated (2026-10-27)

Moved the remote mutation execution phase into `apps/ecc/native/remotereceivermanager_mutationexecution.cpp`: ordered per-peer POST execution, action/audit records, operation-result binding, command-specific result checks, UNKNOWN handling, catalog publication recording, cleanup/retirement completion and explicit control-handoff receipts. `remotereceivermanager_mutation.cpp` retains command authorization and recovery gates, receiver-state refresh, plan/source binding, intent request construction, and durable intent/handoff-context publication. Also moved cold parent-history validation and cold completion receipt assembly from `apps/ecc/handoff/coldhandoff.cpp` into `coldhandoff_evidence.cpp`. The CLI coordinator retains fixed-input verification, operation authorization and file persistence. The helper now validates and preserves optional per-node completion request metadata; this keeps a cold re-export bound to the exact completed child request, including the empty-pair representation used when no child repair was needed. Registered both implementation files in CMake and exposed the existing two cold-completion scenarios through CTest with mandatory handoff-tool binding.

Linux GCC/C++11 rebuilt `getdaq-ecc-native`, `getdaq-ecc-handoff`, `native-ecc-remote-receiver-tests`, `ecc-network-service-tests`, the ECC server and supervisor fixtures. The complete `native-ecc-remote-receiver-*` CTest matrix passed **16/16** (517.87 s), covering normal ICE encodings, truncated input, start/finish acknowledgement loss, partial start, service restart (running and paused), data-only mode, catalog replacement and prepare/publish/finalize failures, plan mismatch, audit fault and ownership. The added cold-completion CTest cases passed **2/2** (293.74 s), covering both configured node-order encodings, explicit cold completion, receipt inspection/re-export and retention of the original controller fence. Their runners removed all 12 synthetic raw files (163,840 bytes total); no raw files remained. Documentation-link validation passed 2,161 local links. The localhost tests use actual service processes and synthetic data; they do not establish physical hardware, production-network or sustained DAQ throughput behavior. No physical hardware or production acquisition data was used.

## 71.694 ECC client recovery policy separated (2026-10-27)

Qt 5.12.8/GCC 9.3 rebuilt production Envoy, `ecc-system-client-tests`, the direct-source `input-fault-client-probe` and the full `getdaq-tests` target with `eccclientstore_recoverypolicy.cpp` registered. The input-fault probe initially exposed a pre-existing direct-source omission of `eccclientstore_storage.cpp`; its qmake project now registers the storage implementation and private header, and the probe links. All 70 ECC system-client QtTest results passed, including persisted retirement-context replay, completion/receipt guards, the new catalog-replacement eligibility test and setup/cleanup. `getdaq-tests nativeCatalogRecoveryPolicyRequiresExactLivePreparedReplacement` passed (3/3 including setup/cleanup). A full RunControl failed-catalog-replacement GUI recovery scenario was not run. Fixtures used localhost and temporary metadata only; no hardware or production acquisition data was accessed.

## 71.693 Host application coverage audit and config-transfer build repair (2026-10-27)

Created `docs/REFACTOR_COVERAGE.md` to track the 17 current application directories and distinguish recorded work from completion. Auditing `configtransfer` exposed that `apps/configtransfer/configtransfer.pro` did not register `configurationrepositoryclient_upload.cpp`, leaving `ConfigurationRepositoryClient::Upload` undefined at link time; adding that existing implementation restored the standalone qmake build of `getdaq-config-upload` with Qt 5.12.8. Built `getdaq-ice-check`, all three source-ring test targets, and `ice-source-control-tests`. CTest `ice-check-help`, `source-ring-reader`, `source-ring-readout` and `source-mfm-ring` passed (4/4); the synthetic ICE retirement/owner-loss integration passed 11 cases with raw payload cleanup. Documentation links passed 2,160 checks. This audit does not certify every application, full suite or platform. No hardware or production acquisition data was used.

## 71.692 Supervisor control-request validation separated (2026-10-27)

Moved supervisor command string mapping/classification, command-specific request size limits, operation-ID validation and complete request-field semantic checks into `controlprotocol_request_validation.cpp`; JSON request encode/decode and request-key derivation remain in `controlprotocol.cpp`. Registered the unit in `getdaq-supervisor-protocol`. The protocol library, `getdaq-supervisor-query` and `supervisor-control-tests` rebuilt; CTest `supervisor-control-codec` passed (1/1). This validates the local codec and its rejection rules, not the complete supervisor service. No hardware, network or acquisition data was used.

## 71.691 ICE data-sender failure recovery separated (2026-10-27)

Moved source observation, failure-stop and failed-source retirement operations, together with retirement receipt binding checks, into `datasenderclient_ice.cpp`; registered it in `getdaq-hardware-core`. The hardware core, `hardware-data-sender-tests`, and `ice-source-control-tests` rebuilt. CTest `hardware-data-sender-tcp` passed (1/1). The synthetic ICE source-control runner passed 7 failed-retirement cases and 4 owner-loss/observation cases; it reported raw payload cleanup. Documentation links passed 2,156 checks. All fixtures used local services and synthetic bounded streams; no hardware or production DAQ data was used.

## 71.690 Transfer job startup validation separated (2026-10-27)

Moved transfer-agent component/deployment validation, fixed-root normalization and overlap checks, root directory verification, and native executable ownership/mode/digest binding from `transferjobs.cpp` into `transferjobs_options.cpp`. Registered the new source in the CMake data-supervisor core. The CMake core library, `getdaq-supervisor-query`, `transfer-jobs-tests`, and `getdaq-catalog` rebuilt successfully. The native transfer controller integration test passed, covering source receipt binding, a separate catalog child, progress, idempotent operation IDs, changed-request rejection, completed history, policy corruption rejection, and owned cleanup. First invocation with a relative catalog executable path was rejected by the existing absolute-path policy; rerunning with its absolute path passed. No hardware, external network, or production raw data was used.

## 71.689 ICE receiver receipt publication separated (2026-10-27)

The optional Ice 3.5 receiver rebuilt with `iceingress_receipt.cpp`; the parser CTest passed (1/1). Ordinary and ASan/UBSan strict C++11 builds each compiled 16 commands over 32 source inputs. Both the regular CMake receiver and strict receiver passed all 15 synthetic ICE ingress scenarios, including successful receipt publish, byte-storage receipt, rejected/missing end markers, wrong session/connection/count, malformed/truncated frames and message limits. The runner removed generated raw payloads. Existing Ice/GET/bzip vendor libraries were not sanitizer-instrumented; LSan remained disabled only for the isolated parser test because of the runner's fatal LSan runtime error. No hardware or external network was used.

## 71.688 ICE ingress startup option parsing separated (2026-10-27)

The optional Ice 3.5 CMake build rebuilt `getdaq-ice-receiver` and the new `ice-ingress-options-tests` target. CTest `ice-ingress-options` passed (1/1), covering normalized output paths, managed identity, defaults, numeric bounds, port/mode conflicts, unknown/missing values and help short-circuiting. Both ordinary and ASan/UBSan strict C++11 receipts compiled 15 commands and ran the options test; existing old Ice/GET/bzip vendor libraries were not sanitizer-instrumented. LeakSanitizer itself terminated fatally in this runner, so it was disabled only for the isolated options test; ASan and UBSan remained enabled. The strict ordinary receiver then passed all 15 actual ICE ingress protocol scenarios; raw payloads were deleted. No real hardware or external network was used.

## 71.687 Configuration checker scan commands separated (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the standalone qmake `getdaq-config-check`; GCC 9.4 rebuilt the CMake checker and `system-experiment-tests`. CTest `system-experiment` passed (1/1). Against an existing 16-point offline scan-plan fixture, both checker builds passed scan-plan, execution-policy and execution-snapshot CLI checks, rejected a policy bound to the wrong plan, and refused a symlinked plan directory. No fixture files were changed; policy/snapshot test outputs were written under a temporary directory. No hardware, network or DAQ raw data was used. Documentation links passed 2,151 checks.

## 71.686 Simulator generated-data writer separated (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the standalone `getdaq-simulator` and the complete `getdaq-tests` target with `simulator_data.cpp` registered. The first build exposed a missing function-closing brace in the extracted unit and an extra closing brace in the original source; both were corrected before successful rebuilds. Focused `standaloneSimulatorIntegration` and `simulatorGeneratedDataBudgetIsHard` tests passed (4/4 QtTest results including setup/cleanup) after enabling local loopback for the synthetic fixture. They covered the simulated Describe/Prepare/Configure/Start/Stop path and the hard generated-byte limit. No physical hardware, external network, or real GET/MFM data was used.

## 71.685 Configuration repository upload-plan validation separated (2026-10-26)

Linux GCC rebuilt `getdaq-configuration-repository`, `configuration-repository-tests`, the repository CLI and HTTP service with `configurationrepository_plan.cpp` registered. The full repository core regression passed against a generated synthetic publication package, covering object hashes/sizes, duplicate commit idempotency, complete version verification and reopen, same-plan interrupted resume, foreign-intent rejection, content-corruption fault latching and credential fault latching. No network, hardware or DAQ data was used.

## 71.684 CoBo lifecycle transition policy separated (2026-10-26)

Linux GCC rebuilt `getdaq-hardware-core`, `hardware-cobo-lifecycle-tests` and `hardware-cobo-lifecycle-journal-tests` with `cobolifecycle_policy.cpp` registered in CMake. `hardware-cobo-lifecycle` and `hardware-cobo-lifecycle-journal` passed (1/1 each), covering transition legality and durable lifecycle journal behavior. No physical hardware or DAQ data was used.

## 71.683 Scan plan review decision recording separated (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` executable and the production Envoy application with `scanplanreview_record.cpp` registered in both qmake projects. `offlineScanPlanBuilderIsAvailableFromConsole` passed (3/3 QtTest results including setup and cleanup) with offscreen Qt and isolated XDG data/cache/config directories. No hardware or DAQ data was used.

## 71.682 Receiver catalog copy/resume orchestration separated (2026-10-26)

Linux GCC 9.4 rebuilt `getdaq-data-supervisor-core`, `getdaq-catalog` and `catalog-transfer-tests` with `catalogtransfer_copy.cpp` registered in CMake. The `catalog-transfer-filesystem` integration regression passed (1/1, 3.34 seconds), covering plain-TCP new transfer, interruption/resume, independent readback, complete-transfer resume, bounded connection refusal and synthetic receiver lifecycle checks. The first sandboxed attempt was blocked from binding its local test socket (`errno=1`); rerunning the same synthetic-fixture test with local-network permission passed. No physical hardware or production receiver data was used.

## 71.681 ECC client operation transport separated (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt production Envoy, complete `getdaq-tests`, and standalone `ecc-system-client-tests` with the new unit registered. Two focused in-process transport scan-recovery cases passed (4/4 QtTest results including setup/cleanup), covering clearing only the operation's own transient hold and preserving a concurrent alarm hold. Socket-backed cases could not run in this sandbox (`Cannot bind fixture`); the full ECC client suite was not run. No hardware or DAQ data was used.

## 71.680 Configuration range parser separated (2026-10-26)

Linux GCC 9.4 rebuilt the CMake configuration-range library/test and experiment core; CTest `configuration-range` and `scan-execution-state` passed (1/1 each). The old implementation comparison passed 9,972 checks. Qt 5.12.8/GCC 9.3 rebuilt production Envoy, complete `getdaq-tests`, and `xcfg-shared-tests`; the focused GUI range check passed (3/3 including setup/cleanup), and all 151 shared Xcfg tests passed. Documentation link validation passed 2,144 links. Synthetic configuration only; no hardware or DAQ data.

## 71.679 Scan journal reconciliation separated (2026-10-26)

Linux GCC 9.4 rebuilt `getdaq-experiment-core` and the Qt 5.12.8/GCC 9.3 qmake build rebuilt production Envoy and the complete `getdaq-tests` executable with the new reconciliation unit registered. CTest `scan-execution-state` passed (1/1), covering unknown-operation quarantine and both applied/not-sent reconciliation paths, including retirement evidence refusal. No hardware or DAQ data was used.

## 71.678 Envoy responsive layout separated (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the production Envoy application and the complete `getdaq-tests` executable with the responsive layout unit registered in both qmake projects. Three focused GUI checks passed (5/5 QtTest results including setup/cleanup): compact parameter/node visibility, node-table reflow after configured nodes shrink, and configured-row table height. Tests used offscreen Qt and synthetic configuration only; no hardware or DAQ data was used.

## 71.677 ECC request semantic validation separated (2026-10-26)

Linux GCC 9.4 rebuilt `getdaq-control-protocol` and `ecc-protocol-tests`; focused CTest `ecc-protocol` passed (1/1). The validation code is now in a dedicated source registered in both CMake and qmake. This validates the protocol target, not the complete Envoy application or hardware behavior.

## 71.676 Structural edit persistence separated (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and standalone `xcfg-shared-tests` with the new Apply unit registered through the shared qmake fragment. All 151 shared Xcfg tests passed (0 failed), including structural preview/apply, stale review and source-change refusal, include materialization and override handling. Tests used isolated XDG data/cache/config directories; no hardware or DAQ data was used.

## 71.675 CompoundConfig binary codec separated (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the production Envoy application and standalone `xcfg-shared-tests` with the codec registered through the shared qmake fragment. All 151 shared Xcfg tests passed (0 failed), including version-1 type/range round trips, truncation/trailing-byte rejection, malformed/overflow rejection, import/export review, and save-time source/output checks. Tests used isolated XDG data/cache/config directories; no hardware or DAQ data was used.

## 71.674 Fixed ECC input file binding separated (2026-10-26)

Linux GCC 9.4 rebuilt `getdaq-ecc-native`, `getdaq-scan-deploy` and `fixed-ecc-configuration-tests` with the extracted input-set component. The focused fixed ECC configuration and scan deployment regression passed (1/1, 6.48 seconds), covering the existing offline configuration, publication, file tamper/identity and deployment checks. Validation used synthetic local fixtures and a localhost test service; no hardware or production DAQ data was accessed.

## 71.673 POSIX scan journal file reading separated (2026-10-26)

Linux GCC rebuilt the ECC journal library, CLI and integration test with the new POSIX read unit; the ECC journal integration passed, covering private atomic storage, intent ordering, restart quarantine, tamper detection and link refusal. Qt 5.12.8/GCC 9.3 also rebuilt production Envoy and the complete `getdaq-tests` executable through the shared qmake registration. No hardware or DAQ payload was used; the full Qt suite was not run.

## 71.672 FITS event serialization and CFITSIO support separated (2026-10-26)

CMake rebuilt the optional `getdaq-fits` and `fits-tests` targets with the event serializer and CFITSIO support units. The FITS conversion integration test passed against synthetic type-1 and compact type-2 frames, checking grouping rows, event/channel metadata, bucket/sample values, checksums, frame count, no-replace publication, output bounds and truncated-input rejection. It removed its synthetic raw input (279,808 bytes). No physical data or hardware was used. The full repository suite was not run.

## 71.671 Native Run Control scan event baseline flow separated (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the complete `getdaq-tests` executable with the baseline flow registered in `core.pri`. The focused `scanEventBaselineRequiresConnectedNativeController` gate passed (3/3 QtTest results including setup/cleanup), verifying a disconnected controller rejects the query without creating or binding a journal. No full Qt suite or hardware test was run.

## 71.670 Xcfg scalar XML text codec separated (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt production Envoy, the complete `getdaq-tests` executable, the standalone `xcfg-shared-tests`, and `current-configuration-probe` with the new scalar codec registered. All 151 Xcfg shared tests passed, including legacy literal/reference whitespace, CDATA and Unicode decoding, split markup read-only handling, and edits preserving comments/instructions. Focused tests also run through the Envoy test executable as part of its full rebuild; no full Qt suite or hardware testing was performed.

## 71.669 Envoy Run command handlers separated (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the production Envoy binary and complete `getdaq-tests` executable with `mainwindow_runcommands.cpp` registered in both qmake projects. Six focused GUI checks passed (8/8 QtTest results including setup/cleanup): manual Run-number rollback, automatic-run settings and placement/locks, and integrated waveform-viewer Run gating/closure. Tests used an isolated HOME and offscreen Qt runtime. Two additional lifecycle checks that require local sockets could not start under the restricted sandbox; no full Qt suite or hardware testing was performed.

## 71.668 ECC service request routing separated (2026-10-26)

Linux GCC 9.4 rebuilt the production `getdaq-ecc-server` and `ecc-network-service-tests` targets after moving GET and POST endpoint handling into separate production translation units. The focused `normal-0` localhost network-service scenario passed in 84.8 seconds, exercising authenticated status/identity/receiver observations, control operations and shutdown through the real HTTP service. Its runner removed all four synthetic raw files (65,536 bytes); no raw files remained. The sandbox initially denied localhost sockets, so the same focused test was rerun with reviewed local-network permission. No hardware or production acquisition data was used; the full network-service matrix and repository suite were not run.

# 71.667 Xcfg source migration marker and completion lifecycle separated (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt production Envoy, the complete `getdaq-tests` executable, standalone `xcfg-shared-tests` and the source-migration process probe with the shared record-state unit registered. All 151 Xcfg shared tests passed, including both recovery directions, corrupt payload and third-party change refusal. The migration process harness passed 12 crash/recovery cases and two active-writer cases. Three focused migration GUI tests passed (5/5 QtTest results including setup/cleanup) using isolated HOME/XDG paths. Documentation link validation passed 2,131 local links. The full Qt suite was not rerun; no hardware or production acquisition data was used.

# 71.666 Xcfg source migration recovery-record decoding separated (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt production Envoy, the complete `getdaq-tests` executable, the standalone `xcfg-shared-tests` target and the source-migration process probe with the new decoder registered. All 151 Xcfg shared tests passed, including migration recovery in both directions, payload tampering and third-party source changes. The source-migration integration runner passed 12 crash/recovery cases and two active-writer cases. Three focused offscreen GUI cases passed (5/5 QtTest results including setup/cleanup) using isolated HOME/XDG paths. Documentation link validation passed 2,129 local links. The full Qt suite was not rerun; all fixtures were temporary and no hardware or production acquisition data was used.

# 71.665 Waveform viewer 3D surface renderer separated (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the standalone waveform viewer, production Envoy and complete `getdaq-tests` executable with `waveformcanvas_surface.cpp` registered. Eight focused waveform viewer tests passed (10/10 QtTest results including setup and cleanup), covering MFM index/decode, malformed input, waveform metrics, scan summaries, navigation and CSV export. A separate interactive 3D surface render/rotation smoke was not confirmed in this run. No hardware or production acquisition data was used; the full Qt suite was not rerun.

## 71.664 Sentry catalog replacement finalization separated (2026-10-26)

Moved `SentryFiles::FinalizeReplacement` to `apps/sentry/sentryservice_replacement.cpp`; qmake registration was updated for the Sentry service, Simulator and `getdaq-tests`. Qt 5.12.8/GCC 9.3 rebuilt both production apps and the complete Qt test executable. Four catalog/replacement journal tests passed, including incomplete-old-Run refusal and restart recovery after merged-manifest publication; the HTTP endpoint integration passed separately with localhost binding allowed. This is 5 focused test slots passed across the two runs. Fixtures were temporary/local; no hardware or acquisition data was used.

## 71.663 DataReceiver matrix codec separated (2026-10-26)

Moved matrix frame decoding and `.gmat` serialization into `apps/datareceiver/matrix_codec.cpp`, with private API and geometry declarations in `matrix_codec_p.h`; the public `MatrixOptions`, `MatrixResult` and converter contract are unchanged. A clean GCC 9.4 Release/C++11 CMake configuration with optional FITS/ROOT tools disabled rebuilt `getdaq-matrix` and `matrix-tests`; CTest `gmat-conversion` passed (1/1), and `getdaq-matrix --help` passed. The conversion regression uses synthetic type-1 frames and covers revision/header layouts, per-channel sample arrival ordering, frame selection, output collision, size limit and malformed/incomplete frames. No hardware or production raw files were used.

## 71.662 ROOT Viewer window responsibilities separated (2026-10-26)

Split the offline ROOT Viewer window implementation into UI construction (`rootviewerwindow.cpp`), file/event navigation (`rootviewerwindow_navigation.cpp`) and statistics scanning/progress (`rootviewerwindow_statistics.cpp`), using the private declaration in `rootviewerwindow_p.h`. The optional ROOT 6.26/Qt 5.12.8 `getdaq-root-viewer` target rebuilt successfully. Reader, statistics and full-tree statistics-scan CTests passed (3/3); offscreen application startup remained healthy for the 3-second smoke window. Waveform/heatmap render smoke remained green (107,294 / 275,439 changed pixels), as did 3D rotation (`262,458`). All viewer inputs were synthetic or existing local tests; no hardware or production acquisition data was used.

## 71.661 ROOT event plot rendering separated (2026-10-26)

Moved `EventCanvas::DrawAxes`, `DrawWaveforms` and `DrawHeatmap` into `apps/rootviewer/rootcanvas_eventplots.cpp` and added it to the optional `getdaq-root-viewer` CMake target. The ROOT 6.26/Qt 5.12.8 build of `getdaq-root-viewer` and `root-viewer-tests` passed. The ROOT reader CTest passed (1/1), the existing offscreen surface rotation smoke passed (`changed_pixels=262458`), and a temporary Qt offscreen smoke rendered synthetic waveform and heatmap events (`107294` and `275439` changed pixels). No real ROOT event file, DAQ hardware or production acquisition data was used.

## 71.660 ConfigWindow export workflows separated (2026-10-26)

Moved `ConfigWindow::OpenXmlExport` and `ConfigWindow::OpenWorkspaceCopy` into `apps/envoy/configwindow_xml_export.cpp` and `apps/envoy/configwindow_workspacecopy.cpp`. Each translation unit now declares its JSON/Qt widget dependencies directly; both production Envoy and the full Qt test project register the two sources. Qt 5.12.8/GCC 9.3 rebuilt production Envoy and `getdaq-tests`. Four focused offscreen tests passed (6/6 results including setup and cleanup), covering both dialogs' preview/save flows and read-only/stale-review gates. The restricted run could not create the Qt test-mode transaction lock under `~/.qttest`; rerunning with normal local cache access passed. No hardware or acquisition data was used.

## 71.659 Supervisor query response validation separated (2026-10-26)

Linux GCC 9.4 rebuilt `getdaq-supervisor-query` through both CMake and the qmake project after moving strict HTTP response decoding, scope checks and operation response validation into `queryrunner_validation.cpp`. The CMake target's `--help` smoke check passed. Supervisor loopback integration passed the normal control and catalog-restart scenarios plus all eight bounded native-transport fault cases, codec checks and credential checks; 14 synthetic raw files (344,064 bytes) were removed and none remained. The failed-handoff completion integration also passed through original-failure preservation, exact explicit completion, successor ownership and cold-restart fencing after fixing the empty inactive-operation-ID gate. The Windows build-entrypoint checks passed (20/20), including CMake and qmake source registration. Tests used synthetic local services only; no physical hardware or production acquisition data was used. Windows CMake/qmake source registration was updated but no Windows build was run.

## 71.658 ConfigWindow tree projection separated (2026-10-26)

Moved `ConfigWindow::Rebuild`, recursive `AddNode` and `SelectionChanged` into `apps/envoy/configwindow_tree.cpp`; source-tree values, include/source-line tooltips, read-only legacy typed-scalar behavior and selected-value edit gates are unchanged. Registered the production unit in `apps/envoy/envoy.pro` and the Qt test link target. Qt 5.12.8/GCC 9.3 clean qmake builds of production Envoy and `getdaq-tests` succeeded. Focused include/lossless-edit, legacy split-scalar, history/stale-window and imported-metadata tests passed (6/6 including setup and cleanup). The test run required the writable local cache because the sandbox cannot create the shared configuration lock; no hardware or acquisition data was accessed. The full Qt suite was not rerun.

## 71.657 Scan analysis path checks and report publication shared (2026-10-26)

The DataSupervisor core and `getdaq-catalog` were rebuilt with shared scan-point input validation, path-boundary validation and report publication. The scan-point loader validates the per-point binding, catalog path, decoded artifact and target CoBo once for threshold and waveform analyzers. The receiver-catalog regression executable passed all 62 native checks, including threshold hit-pattern analysis and bound AutoGain, Pulser and PeakingTime waveform reports over synthetic standard, compact type-2 and layered MFM fixtures. `getdaq-catalog --help` also passed. Publication continues to reject existing outputs and uses an exclusive link from a pending file followed by parent-directory synchronization. Fixtures were synthetic and cleaned up; no receiver, hardware, network or production DAQ data was used. The full Qt suite and non-Linux builds were not rerun.

## 71.655 RunRecorder persistence test separation (2026-10-26)

Moved the six Run reservation, explicit replacement, native reference publication, durable completion and interrupted-recovery test bodies from `tests/test_getdaq.cpp` into `tests/runrecorder_tests.cpp`; the original QtTest slot names remain delegates. The aggregator decreased from 1,941 to 1,832 lines. Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target. The full suite then completed with **177 passed, 0 failed, 2 skipped** in 113.6 seconds. Both skips require the separately built offline data-tool directory (`GETDAQ_TEST_OFFLINE_TOOLS_DIR`); the other offline-tool lifecycle cases passed. All six standalone window-probe makefiles exclude `runrecorder_tests.o`. The suite used temporary directories and local mock services; no hardware or production acquisition data was accessed.

## 71.654 Sentry service and catalog test separation (2026-10-26)

Moved Sentry catalog enumeration, completed-Run replacement/cleanup recovery, strict configuration parsing and HTTP endpoint tests into `tests/sentry_service_tests.cpp`; the original QtTest slots remain delegates. `tests/test_getdaq.cpp` decreased from 2,108 to 1,941 lines. Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target. The five file/config regressions passed in the sandbox; the HTTP endpoint regression passed when rerun with the previously authorized temporary loopback allowance (3/3 including setup and cleanup). All six standalone window probes exclude `sentry_service_tests.o`. Synthetic local data only; the full QtTest suite was not run.

## 71.653 SOAP protocol and payload test separation (2026-10-26)

Moved the legacy SOAP topology/configuration, request/response namespace and payload-form, and protocol-validation tests into `tests/soap_protocol_tests.cpp`; the original QtTest slots remain delegates. `tests/test_getdaq.cpp` decreased from 2,324 to 2,108 lines. Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target, and the four unguarded focused tests passed (6/6 including setup and cleanup). The optional `GETDAQ_GSOAP_INTEROP` process test was moved with its compile guard but was not built or run in this configuration. All six standalone window probes exclude `soap_protocol_tests.o`. The full QtTest suite was not run.

## 71.652 Operation log and console control test separation (2026-10-26)

Moved the independent connection-session log, visible-log export and severity-preserving event-log tests into `tests/operation_log_tests.cpp`; their original QtTest slots remain delegates. The manual Run-number editing/layout gate and elapsed-time formatting tests now live in `tests/consolelayout_tests.cpp`. `tests/test_getdaq.cpp` decreased from 2,503 to 2,324 lines. Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target, and all five focused offscreen tests passed (7/7 results including setup and cleanup). The checks cover per-connection log isolation, export, diagnostic color/text retention, console layout and run-state input locking, and elapsed formatting. The full QtTest suite was not run.

## 71.651 Native status projection and cache test separation (2026-10-26)

Moved the Native receiver-evidence projection, system-status ownership/state gates, and decoded-observation cache lifecycle tests from `native_gui_tests.cpp` into `tests/native_status_tests.cpp`; their original QtTest slots remain delegates. `tests/test_getdaq.cpp` decreased from 2,683 to 2,503 lines, and the GUI test module now contains only window-facing cases. Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target; all five Native status and GUI regression slots passed (7/7 results including setup and cleanup). All six standalone window probe makefiles exclude `native_status_tests.o`. The full QtTest suite was not run.

## 71.650 Scan-plan output validation test separation (2026-10-26)

Moved `scanPlanOutputInspectionRejectsMalformedPlans` into `tests/scanplan_output_tests.cpp`; its original QtTest slot remains as a delegate. `tests/test_getdaq.cpp` decreased from 2,788 to 2,683 lines. Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target, and the focused offscreen test passed (3/3 results including setup and cleanup). The regression still checks malformed plan metadata, artifact digests, unsafe and missing paths, duplicate points, and symlink rejection on Unix. The full QtTest suite was not run.

## 71.649 Native GUI and whole-system control test separation (2026-10-26)

Moved `nativeGuiMetadataAndWholeSystemControls` into `tests/native_gui_tests.cpp`; the original QtTest slot remains as a delegate. `tests/test_getdaq.cpp` decreased from 3,007 to 2,788 lines. Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target, and the focused offscreen test passed (3/3 results including setup and cleanup). All six standalone window probe makefiles were regenerated and checked to exclude `native_gui_tests.o`. The GUI regression used synthetic Native ECC state and temporary settings only; no hardware or acquisition data was accessed. The full QtTest suite was not run.

## 71.648 Offline scan-plan and native scan integration test separation (2026-10-26)

Moved the 544-line offline scan-plan builder GUI test into `tests/scanplan_builder_gui_tests.cpp` and the 238-line Native scan Run-to-catalog integration test into `tests/native_scan_run_tests.cpp`; the original QtTest slot names remain delegates. `tests/test_getdaq.cpp` decreased from 3,779 to 3,007 lines. Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target. With current `getdaq-config` and `getdaq-config-check` builds placed beside the test executable, both extracted tests passed in one focused run (4/4 QtTest results including setup/cleanup). The Native scan fixture used synthetic local loopback ECC metadata only; no receiver process, hardware or production acquisition data was used. Six standalone window probe makefiles were regenerated and verified to exclude both test-only objects. The full QtTest suite was not run.

## 71.647 Legacy Run Control integration test separation (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target with 11 local HTTP/Run Control test bodies moved into `legacy_runcontrol_tests.cpp`; all 11 extracted QtTest slots passed, each including successful setup and cleanup. Six standalone window probes reconfigured successfully, and their generated makefiles exclude all 11 test-only objects, including the previously retained waveform-viewer test and fixture objects. The loopback regressions used synthetic local services and isolated temporary state only; no hardware or acquisition data was accessed.

## 71.646 Data file tools close path made explicit (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target with idle close using explicit deferred destruction. Four offscreen close/reopen and child-process lifecycle groups passed (19/19 QtTest cases including setup/cleanup): title-bar/Close/Escape while idle, menu reopen, close with responsive and TERM-ignoring children, and menu/Run gating until a closing child exits. The child-process cases use temporary scripts; no hardware or acquisition data was accessed.

## 71.645 Configuration compiler test suite separation (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target with 23 configuration compiler tests moved into `configuration_compiler_tests.cpp`; all 23 passed (69/69 including QtTest setup/cleanup). Coverage includes Mutant, CoBo AGET/readout/configure/clock/monitoring/calibration, AGET configuration and hardware-description compilation. qmake parsed all six standalone window probes, and each generated makefile excludes eight test-only object files. Documentation link validation passed 2,111 local Markdown links. Tests used repository fixtures, temporary files and in-memory compiler inputs only; no hardware or acquisition data was used.

## 71.644 Legacy HTTP/SOAP test server fixture separation (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target with the shared test-only `MockNode` server in its own source unit. All 11 dependent loopback integration tests passed (13/13 including QtTest setup/cleanup), covering SOAP headers, lifecycle ordering, delayed/failing states, catalog timeout/retry and Run replacement finalization. Six standalone window probes exclude the fixture and other test-only units. Documentation link validation passed 2,109 local links. Tests used local synthetic services only; no physical hardware or production acquisition data was accessed.

## 71.643 Native recovery and control policy test separation (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` qmake target with seven Native policy tests compiled from a separate unit. All seven passed (9/9 including QtTest setup/cleanup), covering receiver finalization evidence, deployment identity, recovery evidence and receiver sessions, exact prepared catalog replacement, alarm operation/outcome binding, CoBo maintenance gates, and plan-retirement projection. The six standalone window probes reconfigured and excluded the test-only units. Documentation link validation passed 2,108 local links. Tests used in-memory synthetic control metadata only; no hardware or acquisition data was accessed.

## 71.642 Offline data-tool and integrated viewer test separation (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the full `getdaq-tests` target with offline Data file tools and integrated waveform-viewer lifecycle tests in a separate source unit. Offscreen regressions passed for viewer Run gating and close behavior, external Run closure, all Data file tools close controls, close while child processes run, menu reopen, pending-process Run locking, existing-program inspection/conversion/merge, and multiple-window Run locking. The first focused run skipped the two existing-program-dependent cases because no tool directory was set; rerunning with the repository's inspect, matrix, and merge executables passed both (4/4 including setup/cleanup). qmake parsed all six standalone window probes, whose makefiles exclude the test suite and shared test units. Documentation link validation passed 2,107 local links. Temporary synthetic inputs only; no hardware or production acquisition data was used.

## 71.641 Console status and responsive layout test separation (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` qmake target with the five Console summary/layout tests in `consolelayout_tests.cpp`. All five passed: four offscreen status/geometry/reflow checks and one synthetic localhost lifecycle/layout integration. The six standalone window probe projects reconfigured successfully and their generated makefiles exclude the test suite and shared test helpers. Documentation links passed (2,106 local links). Temporary simulated fixtures only; no physical hardware or production acquisition data was used.

## 71.640 Envoy automatic-run test suite separation (2026-10-26)

Qt 5.12.8/GCC 9.3 regenerated and rebuilt the complete `getdaq-tests` qmake target with the nine automatic-run test implementations and shared Run Control test helpers in separate translation units. All nine extracted slots passed, covering saved settings, either-threshold timing, pause/fresh-byte accounting, completion/readiness gating, simulator cycles, UI settings locks and layout. Three existing lifecycle/HTTP/layout cases using the delegated shared fixtures also passed (12 focused cases total). qmake parsed all six standalone window probes, and their generated makefiles exclude these test-only units. Synthetic localhost services and temporary files only; no physical hardware or production acquisition data was used. Documentation links passed (2,105 local links).

## 71.639 ECC handoff recovery CLI separation (2026-10-26)

Linux GCC 9.4 rebuilt `getdaq-ecc-handoff` and `ecc-network-service-tests` after moving shared handoff receipt verification, explicit failed-handoff completion (`complete-submit`/`complete-inspect`), and read-only cold evidence review (`review-cold`) into `handoffrecoverycommands.{p.h,cpp}`. The original `submit`/`inspect` route still calls the shared receipt validator. Added the registered `plan-transition-complete-1` network case to exercise the real CLI. The focused localhost run passed; the runner removed all 6 synthetic raw files (81,920 bytes), with none remaining. `--help` and the documentation link check passed (2,104 links). No hardware or production acquisition data was used.

## 71.638 Waveform viewer test suite separation and qmake integration (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the complete production Envoy target and full `getdaq-tests` qmake target with the POSIX scan-journal publisher registered through `core.pri`; the test suite now compiles the moved waveform tests and shared MFM fixtures as separate units. A focused Qt run passed 21/21 cases: all eight waveform tests, offline data inspection/matrix conversion/merge with temporary synthetic input, and Data file tools close, close-while-running and reopen paths. Documentation link validation passed 2,103 local links. No hardware or production acquisition data was used.

## 71.637 POSIX scan journal atomic publication separation (2026-10-26)

Linux GCC 9.4 rebuilt the strict C++11 POSIX ECC journal library, CLI and integration test after registering `scanexecutionjournalstore_publish.cpp`. Focused `ecc-journal` and `ecc-state` CTests passed (2/2); the journal integration covers private file/lock permissions, exclusive writer ownership, create/update/reopen, atomic state persistence, external drift refusal and symlink rejection. Documentation link validation passed for 2,102 local Markdown links. Fixtures were temporary; no hardware, network service or DAQ acquisition data was used.

## 71.636 ECC journal replay and durable storage separation (2026-10-26)

Linux GCC 9.4 rebuilt the strict C++11 POSIX ECC journal library, CLI, `ecc-journal-tests` and `ecc-state-tests` with record replay moved into `eccjournal_replay.cpp`. Focused CTest `ecc-journal` and `ecc-state` passed (2/2), covering recovery-state reconstruction, pending/unknown operations, reconciliation, malformed/truncated input, locking and journal identity/drift checks. Documentation link validation passed for 2,101 local Markdown links. Test journals were temporary; no hardware, network service or DAQ acquisition data was used.

## 71.635 Waveform raw indexing and sample decoding separation (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` qmake target with both waveform reader modules. `waveformViewerIndexesRawAndLayeredFrames`, `waveformViewerDecodesCompactCoboFrames` and `waveformViewerRejectsTruncatedMfmInput` passed (5/5 including init/cleanup), covering regular and nested MFM indexing, selected sample readback, compact type-2 decoding and malformed-input rejection. Fixtures were temporary; no acquisition files or hardware were used.

## 71.634 Receiver seal verification and publication separation (2026-10-26)

The GCC 9.4 native receiver rebuilt with seal generation moved into `receiver_seal.cpp`. A synthetic TCP loopback sent one 4,096-byte MFM frame and half-closed the sender; the receiver wrote one matching `.graw`, recorded 4,096 received bytes, and published a complete seal after EOF. The fixture and raw file were removed; no physical hardware or production data was used.

## 71.633 Receiver storage writer separation (2026-10-26)

The GCC 9.4 `getdaq-receiver` and `receiver-core-tests` targets rebuilt after splitting writer dispatch, ByteStorage and FrameStorage units. CTest `receiver-core`, `exporter-lock` and `byte-storage-integration` passed (3/3). The integration tests used temporary directories and loopback only; no hardware or production data was used.

## 71.632 Supervisor query entry point separation (2026-10-26)

The Linux CMake target `getdaq-supervisor-query` rebuilt with query command handling moved into `queryrunner.cpp`; invoking `--help` produced the same command and credential guidance. The Windows build registration and qmake project both include the shared runner source; a Windows compiler/runtime was not available in this environment.

## 71.631 FITS MFM event decoding separation (2026-10-26)

GCC 9.4 and system CFITSIO 3.47 rebuilt the optional converter and FITS test target with CoBo frame decoding moved into `fits_decode.cpp`. CTest `fits-conversion` passed (1/1), exercising type-1 and compact type-2 event conversion, sample ordering, bounds rejection, output limits and cleanup. The test uses synthetic frames and removes its temporary fixtures; no DAQ hardware or raw acquisition files were used.

## 71.630 FITS event writing and index management separation (2026-10-26)

GCC 9.4 and system CFITSIO 3.47 rebuilt the optional `getdaq-fits` converter and `fits-tests` after moving FITS serialization behind the internal writer component. CTest `fits-conversion` passed (1/1), covering generated event/grouping tables, compact type-2 samples, checksum/index validation, output limits and rejection cleanup. The test uses synthetic frames and removes its temporary fixtures; no DAQ hardware or raw acquisition files were used.

## 71.629 ROOT viewer UI and application bootstrap separation (2026-10-26)

The optional ROOT 6/Qt 5 viewer target rebuilt after moving `RootViewerWindow` and the statistics worker into `rootviewerwindow.cpp`; the `root-viewer-tests` target rebuilt and CTest `root-viewer-reader` passed (1/1). Offscreen `getdaq-root-viewer --surface-smoke` passed synthetic 3D painting and pointer rotation (`changed_pixels=262458`). The smoke uses synthetic samples; no real event file, DAQ hardware or raw acquisition data was used.

## 71.628 ROOT viewer 3D surface rendering separation (2026-10-26)

The optional ROOT 6/Qt 5 viewer target rebuilt with `rootcanvas_surface.cpp`; the `root-viewer-tests` target also rebuilt. CTest `root-viewer-reader` passed (1/1) from `/tmp/getdaq-rootviewer-build`, covering ROOT6/ROOT5/T2K input and invalid-entry rejection. The offscreen `getdaq-root-viewer --surface-smoke` synthetic-event render/rotation check passed (`changed_pixels=262458`), exercising 3D painting and pointer rotation. This does not replace visual review with real event files or a target desktop. No DAQ hardware or raw acquisition data was used.

## 71.627 Scan-plan subprocess result dispatch separation (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and `getdaq-tests` after registering `scanplanwindow_processoperations.cpp` in both qmake projects. `scanPlanOutputInspectionRejectsMalformedPlans` and `offlineScanPlanBuilderIsAvailableFromConsole` passed (3/3 each including setup/cleanup); the Data file tools close-button/Escape/reopen and pending-process Run-gate regressions passed (5/5, 5/5 and 3/3). No physical hardware or DAQ payloads were accessed.

## 71.626 Scan-plan subprocess failure and close lifecycle separation (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the production Envoy target and `getdaq-tests` after registering `scanplanwindow_processlifecycle.cpp` in both qmake projects. The offscreen `offlineScanPlanBuilderIsAvailableFromConsole` test passed (3/3 including setup/cleanup). Full-suite status remains limited by this sandbox's denied localhost fixture listeners; its earlier run aborted while creating a synthetic Native scan fixture. No hardware or acquisition data was accessed.

## 71.625 Native scan pending-lifecycle observation separation (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the production Envoy target and `getdaq-tests` with `nativeruncontrol_scanrecovery.cpp` registered through `apps/envoy/core.pri`. The focused offscreen `offlineScanPlanBuilderIsAvailableFromConsole` test passed (3/3 including setup/cleanup). A full-suite attempt was not a valid regression signal in this restricted sandbox: localhost listener creation failed across hardware fixtures, and the suite aborted while creating a Native scan fixture (59 passed before abort). No physical hardware or production acquisition data was accessed.

## 71.624 Scan-plan policy-state refresh separation (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the production Envoy target and `getdaq-tests` with `scanplanwindow_policystate.cpp` registered in both qmake projects. The focused `offlineScanPlanBuilderIsAvailableFromConsole` Qt test passed (3/3 including setup/cleanup). The extracted method retains its existing journal validation, runtime identity gates, lifecycle-action labels, and policy-creation availability checks. Test ran offscreen with local temporary state; no hardware or acquisition data was accessed.

## 71.623 SystemPreparation selected-output maintenance separation (2026-10-26)

Linux GCC/C++11 rebuilt `getdaq-hardware-core` and `native-ecc-coordinator-tests` with `systempreparation_cobooutputs.cpp` registered. The complete `maintenance-0` integration passed against synthetic localhost Ice/GET fixtures, including reprepare/configure, selected LED actions, blink/flip/pulse, AsAd pulser, journal reopen and cold-hold checks. Receiver lifecycle remained an explicit test double. No physical hardware or production DAQ data was accessed.

## 71.622 Native ECC read-only observations separation (2026-10-26)

Linux GCC/C++11 rebuilt `getdaq-ecc-native` and `native-ecc-coordinator-tests` with `nativeecccoordinator_observations.cpp` registered. The focused `hardware-observation-0` scenario and full `normal-0` Native ECC lifecycle both passed against synthetic localhost Ice/GET fixtures, covering bounded per-node observation, race sample withdrawal, controller status, ownership/configuration readback and the standard command lifecycle. Receiver data lifecycle was an explicit test double; no physical hardware or production acquisition data was accessed.

## 71.621 SystemPreparation Unprepare release separation (2026-10-26)

Linux GCC/C++11 rebuilt `getdaq-hardware-core`, `hardware-system-release-tests`, `broken-acquisition-stop-tests` and `native-ecc-coordinator-tests` with `systempreparation_unprepare.cpp` registered. The full `hardware-system-release normal-0` scenario passed; Native ECC `failure-unprepare-0` and `acquisition-unprepare-0` passed, covering both failure cleanup APIs and final retained-audit verification. All fixtures were synthetic localhost Ice/GET endpoints; receiver lifecycle used the explicit test double. No physical hardware or production DAQ data was accessed.

## 71.620 Native ECC failure cleanup separation (2026-10-26)

Linux GCC/C++11 rebuilt `getdaq-ecc-native` and `native-ecc-coordinator-tests` with `nativeecccoordinator_failurecleanup.cpp` registered. Focused `failure-unprepare-0`, `failure-breakup-0`, `acquisition-unprepare-0` and `acquisition-unprepare-power-loss` scenarios passed against synthetic localhost Ice hardware fixtures. The acquisition-unprepare success case verifies the retained lifecycle audit across the final read-only observation; the power-loss case confirms incomplete cleanup remains rejected. Receiver data lifecycle was an explicit test double, not production receiver/seal proof. No physical hardware or production DAQ data was used.

## 71.619 Data file tools 关闭完成与标题栏关闭按钮 (2026-10-26)

Qt 5.12.8/GCC 9.3 从当前源码重建 Envoy 和 Widgets 测试目标。Data file tools 关闭回归 **13/13通过**：标题栏关闭提示位、空闲时的叉号/Close/Esc、运行中隐藏并停止子进程、忽略终止信号后的强杀、关闭后菜单重开以及销毁前持续锁定 Run。子进程退出后窗口使用显式 `deleteLater()` 收尾，避免依赖再次触发已隐藏窗口的关闭事件。测试只使用本机临时脚本和文件；未访问硬件或采集数据。

## 71.618 ECC journal JSON codec separation (2026-10-26)

Linux GCC/C++11 rebuilt the POSIX ECC journal library, journal CLI and `ecc-journal-tests` with `eccjournal_codec.cpp` registered. The complete `ecc-journal` CTest passed (1/1, 0.84 seconds), exercising strict record replay, recovery state, locking, persistence, identity/drift checks and bounded malformed input. All test journals were temporary; no hardware or DAQ data was used.

## 71.617 Xcfg source migration durable record and file transaction separation (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the 151-case `xcfg-shared-tests` and production-source `getdaq-config-probe` with `xcfgsourcemigration_store.cpp` registered in both qmake paths. All 151 shared tests passed, including original/candidate migration recovery, stale and forged preview rejection, third-party source-change refusal, and corrupt payload refusal. Tests used temporary local files and an isolated Qt cache; no hardware or DAQ data was used.

## 71.616 SystemAlignment binding, preflight and command execution separation (2026-10-26)

Linux GCC/C++11 rebuilt `getdaq-hardware-core`, `getdaq-align`, `hardware-alignment-tests` and `topology-json-tests` with the two extracted units. `topology-json` passed (1/1); synthetic-localhost `hardware-alignment-tcp` and `hardware-alignment-cli` passed (2/2), covering both Ice encodings, command/audit order, validation gates, stopped-DAQ/exclusive-owner checks, UNKNOWN outcomes and CLI behavior. Loopback tests ran with local-network permission and synthetic devices only; no physical hardware or production acquisition data was accessed.

## 71.615 Xcfg source-path migration recovery workflow separation (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt `xcfg-shared-tests` and the production-source `getdaq-config-probe` with `xcfgsourcemigration_recovery.cpp` registered through both qmake entry points. All 151 Xcfg shared tests passed, including candidate/original recovery in both directions, stale/forged review rejection, third-party source change refusal and corrupt-payload refusal. Documentation link validation passed after this entry was added. Tests used temporary local files and an isolated Qt cache; no hardware or DAQ data was used.

## 71.614 System-description option validation separation (2026-10-26)

CMake rebuilt `getdaq-hardware-core`, `hardware-system-description-tests` and `getdaq-system-describe` with `systemdescription_options.cpp` registered. `hardware-system-description-tcp` passed (1/1, 7.23 seconds) and `hardware-system-description-cli` passed (1/1, 5.19 seconds); the CLI regression covers all three policies, both Ice encodings, offline validation, replacement binding, Describe order and failure audit behavior against synthetic local servants. The sandbox denied loopback on the first TCP test attempt; the same test passed with local loopback access. No physical hardware or production acquisition data was used.

## 71.613 Config-tool register compiler command separation (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the production `getdaq-config` qmake target and direct-source configtool probe with `configtool_registercommands.cpp`. Compiler CLI integrations passed: AGET, MuTaNT (50 contracts), CoBo Configure, readout, clock, monitoring (31 contracts), calibration (33 contracts), and system experiment (94 process cases). The hardware-description pipeline also passed for all three repository samples, strict loader validation, synthetic 11-device CREATE/readback, no-overwrite, local write policy and configuration drift rejection; the loopback fixture ran with permission and an isolated cache. Configure/clock fixture inputs were aligned with declared XCFG types and ranges. The Envoy production target rebuilt with the asynchronous Data file tools close fix, and its title-bar, Close, Escape, running-child, menu-reopen and Run-gate Qt regressions passed (13/13). Tests used temporary files and synthetic local processes only; no physical hardware or production DAQ data was used.

## 71.612 Scan-point deployment preparation and sealed bundle publication separation (2026-10-26)

The compatibility CMake build rebuilt `getdaq-ecc-native`, `getdaq-scan-deploy` and `fixed-ecc-configuration-tests` with the publication unit registered. `fixed-ecc-configuration-and-scan-deployment` passed (1/1, 6.60 seconds), covering the generated per-point service/receiver plans, sealed bundle manifest, read-only verification, tamper rejection and no-overwrite behavior using temporary files and local synthetic fixtures. No physical hardware or production acquisition data was used.

## 71.611 Scan-plan CLI dispatch and implementation separation (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the full production `getdaq-config` qmake target with the dispatch, metadata, plan-generation and shared-support units registered. The `system_experiment_integration.py` suite passed all 94 process cases with the native system-configuration inspector and Qt-free scan-plan checker, covering all four offline plan generators, manifest/artifact digests, execution policy/snapshot commands, invalid inputs, no-overwrite behavior and unchanged source configuration. All files were temporary; no hardware or DAQ raw data was used.

## 71.610 Receiver plan transition staging and predecessor retirement separation (2026-10-26)

Linux GCC/C++11 rebuilt the data supervisor core, service, query/catalog tools and focused test executables. `supervisor-control-service`, `data-plan-transition-staging` and `data-supervisor-process-lifecycle` passed (3/3); the lifecycle suite ran serially and passed all 22 synthetic scenarios to avoid loopback fixture overlap. Python syntax checks passed for the adjusted test harnesses. These tests use local synthetic services/files only; no physical hardware or production acquisition data was used.

## 71.609 XcfgWorkspace structural editing and cross-file move separation (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt `xcfg-shared-tests` with the structural editing and cross-file move units registered; the production GetDAQ qmake project parsed with the shared source list. All 151 Qt test cases passed, including workspace structural edit, cross-file move, history, stale-review and no-write checks. Documentation-link validation passed 2,074 local links. Tests used temporary files and an isolated Qt test HOME/cache. No hardware or DAQ data was used.

## 71.608 Offline AGET configuration compiler stage separation (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target with the four AGET compiler translation units registered; the production Envoy qmake project parsed with the shared source list. Nine focused Qt cases passed (11/11 including init/cleanup), covering single-chip, whole-CoBo, readout and Configure images, rejection paths, source includes and no-overwrite file saving. Tests ran with an isolated Qt test HOME/cache. Documentation-link validation passed 2,073 local links. No hardware or DAQ data was used.

## 71.607 Waveform viewer window responsibility separation (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt `getdaq-waveform-viewer` with separate window construction, file/index lifecycle, controls/navigation and presentation/export translation units; the production Envoy qmake project parsed with the shared source list. The offline package smoke passed (CLI help and offscreen window startup), and `--surface-smoke` passed its render/drag-rotation check (96,353 changed pixels). Documentation-link validation passed 2,072 local links. No waveform input, DAQ raw, hardware or network connection was used.

## 71.606 ECC service process shutdown separation (2026-10-26)

CMake rebuilt `getdaq-ecc-native`, `ecc-service-process-supervisor-tests` and `getdaq-ecc-server` with the shutdown implementation registered. `ecc-service-process-supervisor` passed (1/1, 0.31 seconds) using a synthetic child process and local authenticated HTTP endpoint. No hardware or production acquisition data was used.

## 71.605 Scan runtime fresh identity observation separation (2026-10-26)

CMake rebuilt the control protocol, Native ECC and scan identity/execution/system experiment test targets. `system-experiment`, `scan-execution-state` and `scan-runtime-identity` passed (3/3). Qt 5.12.8/GCC 9.3 qmake rebuilt `ecc-system-client-tests`; `scanRuntimeIdentityRequiresFullFrozenDeploymentExpectation` passed (3/3 including init/cleanup). The test project now also registers the existing extracted `eccclientstore_storage.cpp` required for linking. No hardware or DAQ data was used.

## 71.604 Remote receiver cold recovery separation and catalog recovery gate repair (2026-10-26)

CMake rebuilt `getdaq-ecc-native`, `getdaq-supervisor`, the remote receiver fixture and focused test executables with `remotereceivermanager_coldrecovery.cpp` registered. Synthetic tests passed (10/10): cold audit evidence, catalog replacement, publish-failure and finalize-failure cold continuation, normal receiver control, catalog prepare failure, plan mismatch, ownership, supervisor control service and receiver process lifecycle. These tests exposed and now cover the service recovery gate that publishes exact failed replacement eligibility and clears reconciliation only after successful continuation. All integrations used local synthetic Ice/TCP fixtures; no physical hardware or production receiver data was used.

## 71.603 Catalog transfer wire protocol separation (2026-10-26)

Linux GCC/C++11 rebuilt `getdaq-data-supervisor-core`, `catalog-transfer-tests` and `getdaq-catalog`. The `catalog-transfer-filesystem` CTest passed (1/1, 4.20 seconds), including synthetic local TCP new-transfer, interrupted-resume, readback, and separate-process listener/send paths. Documentation-link validation passed 2068 local links. No external host, physical hardware or production DAQ data was used.

## 71.602 XcfgDocument IO and query separation (2026-10-26)

Qt 5.12.8/GCC 9.3 rebuilt `xcfg-shared-tests`; all 151 tests passed, including XML include loading, legacy scalar decoding/editing, source-drift rejection and recovery/history cases. The production Envoy qmake project regenerated with `xcfgdocument_io.cpp` registered and compiled that object successfully. Documentation-link validation passed 2068 local links. No hardware or DAQ data was used.

## 71.601 Bound scan waveform metric extraction (2026-10-26)

Linux GCC/C++11 rebuilt `getdaq-data-supervisor-core`, `receiver-catalog-tests` and `getdaq-catalog`. The `receiver-catalog-filesystem` CTest passed (1/1, 20.68 seconds), including bound AutoGain, Pulser and PeakingTime waveform analysis over standard, layered and compact synthetic MFM frames. Documentation-link validation passed 2068 local links. No hardware or production DAQ data was used.

## 71.600 Catalog replacement directory-sync helper separation (2026-10-26)

CMake rebuilt `getdaq-data-supervisor-core`, `catalog-replacement-tests` and `native-ecc-remote-receiver-tests`. `catalog-replacement-file-set` passed, and the synthetic localhost `native-ecc-remote-receiver-catalog-replace` integration passed (1/1, 4.37 seconds), covering the normal prepare/publish/finalize path with directory syncs. No physical hardware or production acquisition data was used.

## 71.599 Remote handoff evidence filesystem support separation (2026-10-26)

CMake rebuilt `getdaq-ecc-native`, `remote-handoff-evidence-tests` and `native-ecc-remote-receiver-tests` with the extracted support unit. `remote-cold-audit-evidence` passed (1/1, 0.85 seconds), covering locally generated evidence files, bounded journal parsing and tamper/identity checks. No hardware or production acquisition data was used.

## 71.598 HardwareClient device description and helper separation (2026-10-26)

CMake rebuilt `getdaq-hardware-core`, `hardware-client-tests`, `hardware-system-description-tests`, `hardware-services-tests` and `getdaq-system-describe`. Synthetic tests `hardware-system-description-tcp`, `hardware-client-tcp`, `hardware-services-tcp` and `hardware-system-description-cli` passed (4/4). The client TCP integration took 227.49 seconds; the other scenarios took 7.57, 11.74 and 5.44 seconds respectively. These use local Ice/service fixtures only; no physical hardware or production acquisition data was used.

## 71.597 ECC control-job validation and alarm observation separation (2026-10-26)

CMake rebuilt `getdaq-ecc-native` and `ecc-network-service-tests` with the extracted validation unit. Synthetic localhost scenarios `ecc-network-service-normal-0` and `ecc-network-service-alarm-ready` passed (2/2, 140.69 seconds), covering fixed startup and request validation, a normal lifecycle, alarm evidence serialization and the alarm-held command gate. No physical hardware or production acquisition data was used.

## 71.596 RemoteReceiverManager state and lifecycle validation separation (2026-10-26)

CMake rebuilt `getdaq-ecc-native` and `native-ecc-remote-receiver-tests` with the extracted state, validation and support units. Synthetic localhost scenarios `native-ecc-remote-receiver-normal-0` and `native-ecc-remote-receiver-normal-1` passed against the final code (2/2, 172.55 seconds), exercising both supported Ice protocol encodings through remote identity binding, readiness, stop/seal verification and operation handling. No physical hardware or production acquisition data was used.

## 71.595 SystemPreparation runtime state and preflight separation (2026-10-26)

CMake rebuilt the hardware release, preparation, description, acquisition, failure-stop and product targets with the extracted units. All nine `hardware-system-release` scenarios passed (2 normal protocol variants and 7 release-fault cases; 698.37 seconds total). The main `hardware-system-preparation-tcp` integration also passed (1/1, 222.91 seconds), covering preparation, configuration, endpoint uniqueness and interlock failure paths. These tests use synthetic local Ice fixtures and loopback; no physical hardware or production acquisition data was used.

## 71.594 SystemAcquisition internal execution and journal separation (2026-10-26)

CMake rebuilt `hardware-system-acquisition-tests`, `broken-acquisition-stop-tests`, `hardware-data-sender-tests`, `getdaq-hardware` and `getdaq-supervisor`. Four synthetic loopback integration tests passed (4/4, 666.07 seconds): normal system acquisition, broken-acquisition stop, and configuration invalidation failure/success. No physical hardware or production acquisition data was used.

## 71.593 DataSenderClient remote validation and transition separation (2026-10-26)

CMake rebuilt `hardware-data-sender-tests`, `getdaq-hardware` and `getdaq-supervisor` with the new validation and transition units registered. The `hardware-data-sender-tcp` CTest passed (1/1, 4.03 seconds) using local Ice/TCP fixtures over loopback. This verifies the extracted sender runtime path without physical hardware or production acquisition data.

## 71.592 GetDAQ control MCP server implementation separation (2026-10-26)

GCC 9.4 built the MCP target through both standalone CMake and the production qmake project, with qmake output isolated under `/tmp`. The existing `ecc-control-mcp-stdio` authenticated localhost HTTP/stdio integration test passed (1/1, 0.66 seconds), exercising JSON-RPC initialization, read tools and confirmed control operations. No hardware or production data was used.

## 71.591 Native ECC coordinator preflight and initialization separation (2026-10-26)

The GCC 9.4/C++11 compatibility build rebuilt Native ECC, coordinator tests and the ECC server with the extracted validation unit. `native-ecc-host-ownership` and `native-ecc-normal-0` passed (2/2, 94.70 seconds), covering ownership reservation, fixed-plan setup, command path binding and a complete synthetic lifecycle over local Ice. No physical hardware or production data was used.

## 71.590 Native ECC recovery state verification separation (2026-10-26)

The GCC 9.4/C++11 compatibility build rebuilt Native ECC, coordinator tests and the ECC server. Synthetic `native-ecc-fault-start` and `native-ecc-fault-prepare-0` scenarios passed (2/2, 70.17 seconds), exercising failure recovery and original-operation proof checks through local Ice fixtures. No physical hardware or production data was used.

## 71.589 Native ECC hardware command application separation (2026-10-26)

The GCC 9.4/C++11 compatibility build rebuilt Native ECC, coordinator tests and the ECC server with `nativeecccoordinator_apply.cpp` registered. Synthetic local `native-ecc-normal-0` and `native-ecc-fault-start` scenarios passed (2/2, 149.26 seconds), covering Slice 1.0 lifecycle execution and failure-audit handling over loopback. No hardware or production data was used.

## 71.588 Native ECC recovery proof validation separation (2026-10-25)

The GCC 9.4/C++11 compatibility build rebuilt Native ECC, coordinator, ECC server and fixed-configuration targets. Synthetic local scenarios `native-ecc-fault-start`, `native-ecc-fault-stop-receiver` and `native-ecc-fault-prepare-0` passed (3/3, 129.46 seconds), covering stop, receiver-abort and preparation-cleanup proof paths. No physical hardware or production data was used.

## 71.587 ECC network observation projection separation (2026-10-25)

The GCC 9.4/C++11 compatibility build rebuilt the Native ECC library, coordinator and network-service tests, CoBo projection test and ECC server. `cobo-lifecycle-projection`, `native-ecc-host-ownership`, `native-ecc-normal-0` and `ecc-network-service-ecc-restart` passed (4/4, 144.89 seconds). This covers projected lifecycle gates, coordinator operation results and persisted service status using synthetic local fixtures and loopback. No hardware or production data was used.

## 71.586 ECC network journal integrity and append separation (2026-10-25)

The GCC 9.4/C++11 compatibility build rebuilt `getdaq-ecc-native`, `ecc-network-service-tests`, `native-ecc-coordinator-tests` and `getdaq-ecc-server`. `ecc-network-service-ecc-restart` and `ecc-network-service-ecc-interrupted` passed (2/2, 53.78 seconds), exercising journal reopen, storage/input checks, durable append and unfinished-intent handling through synthetic local services over loopback. No physical hardware or production data was used.

## 71.585 ECC network recovery binding checks separation (2026-10-25)

The GCC 9.4/C++11 compatibility build rebuilt `getdaq-ecc-native`, `ecc-network-service-tests` and `getdaq-supervisor` with `ecccontroljobs_recoverychecks.cpp` registered. `ecc-network-service-ecc-restart` and `ecc-network-service-ecc-interrupted` passed (2/2, 53.63 seconds), covering persisted recovery bindings and unfinished-intent handling through synthetic local services over loopback. No physical hardware or production data was used.

## 71.584 ECC network request acceptance separation (2026-10-25)

The GCC 9.4/C++11 compatibility build rebuilt `getdaq-ecc-native`, `native-ecc-coordinator-tests` and `getdaq-ecc-server` with `ecccontroljobs_acceptance.cpp` registered. Four synthetic Native ECC scenarios passed: host ownership, normal lifecycle under Slice encodings 1.0 and 1.1, and fault-start handling (4/4; 243.47 seconds). These tests exercise request acceptance and local fake Ice services over loopback; they do not validate physical hardware or production data.

## 71.583 ECC network journal recovery separation (2026-10-25)

The GCC 9.4/C++11 build rebuilt `getdaq-ecc-native`, `native-ecc-coordinator-tests`, and `getdaq-ecc-server`; the service integration test also built with the synthetic receiver and supervisor dependencies. `ecc-network-service-ecc-restart` and `ecc-network-service-ecc-interrupted` passed (2/2, 52.94 seconds), verifying service journal reopen and interrupted-intent handling. These tests use synthetic local services and loopback; no physical hardware or acquired DAQ data was used.

## 71.582 ECC network command execution separation (2026-10-25)

The GCC 9.4/C++11 compatibility build rebuilt `getdaq-ecc-native`, `native-ecc-coordinator-tests` and `getdaq-ecc-server` with `ecccontroljobs_run.cpp` registered. Four synthetic Native ECC scenarios passed: host ownership, normal lifecycle under Slice encodings 1.0 and 1.1, and fault-start handling (4/4; 245.89 seconds). The tests use local fake Ice servants and loopback; they do not validate physical hardware or production data.

## 71.581 Catalog transfer verification separation (2026-10-25)

The C++11 build rebuilt `getdaq-data-supervisor-core`, `catalog-transfer-tests` and `getdaq-catalog` with `catalogtransfer_verify.cpp` registered. The catalog transfer executable passed all 12 native scenarios plus streaming SHA vectors, SIGPIPE isolation, oversized metadata rejection, TCP interruption/resume/readback and CLI separate-process checks. Cases include distinct filesystems, same-size content corruption, file-limit failure, preservation of source data and refusal to overwrite. Fixtures contained 15,269,888 synthetic raw bytes and were cleaned; this is not hardware or production-data validation.

## 71.580 DataSupervisor status protocol separation (2026-10-25)

The C++11 build rebuilt `getdaq-supervisor-protocol`, `getdaq-data-supervisor-core`, `supervisor-control-tests`, `data-supervisor-tests` and `getdaq-supervisor` with `controlprotocol_status.cpp` registered. `supervisor-control-tests codec` passed, covering strict plan/request scope and exact 64-bit status encoding. The moved functions preserve the existing JSON schemas and validation rules. This codec test uses synthetic data only and makes no hardware or receiver-process claim.

## 71.579 Data sender local validation separation (2026-10-25)

The C++11 build rebuilt `getdaq-hardware-core`, `hardware-data-sender-tests` and the `getdaq-hardware`, `getdaq-align`, `getdaq-system-config` and `getdaq-system-describe` executables with the new validation unit. The synthetic sender test executable passed all four groups: lifecycle/UNKNOWN/checkpoint behavior in Slice encodings 1.0 and 1.1, global-set preflight and partial-operation guards, bounded option/address checks, and direct endpoint/unique-set validation. These tests use local synthetic Ice services and loopback; no hardware or acquired DAQ data was used. This change only separates translation-unit ownership; validation and runtime behavior remain unchanged.

## 71.578 Native Run Control observation and status coordination separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target with `nativeruncontrol_observation.cpp` registered through shared Envoy `core.pri`. `nativeStatusProjectionRequiresMatchingLiveReceiverEvidence`, `nativeStatusObservationCacheOwnsDecodeFreshnessAndReset`, `nativeGuiMetadataAndWholeSystemControls`, `nativeSingleCoBoLifecycleUsesWholeEccTransitions`, `scanRunStartRequiresConnectedNativeController` and `nativeScanRunFinishesReceiverAndCatalogAfterEventTarget` passed (12/12 including init/cleanup). The synthetic scan/receiver/catalog integration ran with local-network permission and no hardware or DAQ raw. The split only changes translation-unit ownership; polling priority, freshness, stale-response rejection, evidence gates and lifecycle advancement remain unchanged.

## 71.577 DataSupervisor receiver lifecycle and seal verification separation (2026-10-25)

Reconfigured and built `getdaq-data-supervisor-core`, `data-supervisor-tests`, `supervisor-control-tests` and `managed-ice-supervisor-tests` under the project C++11 build. The actual receiver process lifecycle suite passed all 22 scenarios, covering startup/abort, receiver runtime identity, status, stop/EOF, final seals, output lock, catalog, disk reserve, failed observations and byte storage. `receiver-catalog-filesystem` and `supervisor-control-codec` passed (2/2). The child process tests used synthetic localhost frames; no hardware or sustained-throughput claim is made. The change moves existing validation methods between translation units without changing the receiver contract or finalization gates.

## 71.576 Xcfg source migration preview separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt the full `getdaq-tests` target and the standalone `xcfg-shared-tests` target with the new preview unit registered; production and test qmake source lists include the unit and private helper header. All eight source-migration/referrer test functions passed (15/15 including data rows and init/cleanup), covering transitive roots, shared include rewrites, root/dependency migration, collision/stale/forged preview rejection, recovery in both directions and third-party/payload corruption refusal. The split changes translation-unit ownership only; preview digest/effective configuration checks and durable recovery semantics are unchanged. Tests used temporary local configuration files; no hardware or DAQ data was used.

## 71.575 Receiver status endpoint separation (2026-10-25)

GCC 9.4/C++11 configured and built `getdaq-receiver` and the complete data-receiver test targets with `receiver_status.cpp` registered in CMake. All six receiver CTest cases passed: MFM merge, waveform matrix conversion, CoBo hit pattern, receiver core, exporter lock and byte-storage integration. The final loopback tests were run with local-network permission after the restricted run demonstrated socket creation is blocked in the sandbox. The split relocates status JSON and the HTTP `/status` handler; no protocol, JSON fields, request bounds, deadlines, response codes or sealing behavior changed. Tests used temporary local fixtures; no hardware or DAQ acquisition data was used.

## 71.574 Offline data tools window close lifecycle verification (2026-10-25)

Reviewed the existing `OfflineDataToolsWindow` lifecycle and ran `offlineDataToolsAllCloseControls`, `offlineDataToolsMenuWindowClosesAndCanReopen`, `offlineDataToolsMissingProgramsAndCloseDuringProcess` and `offlineDataToolsClosePendingLocksMenuUntilProcessExits`: 13/13 passed, including title-bar close, the in-window Close button, Escape, and a child that ignores termination. Closing hides the window immediately; deletion and the MainWindow Run gate release follow process exit, then the tool can reopen. The current implementation already provides these paths, so no source change was required for this report. Only known offscreen Qt size-hint warnings appeared. No hardware or DAQ data was used.

## 71.573 Console presentation icon and style separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target with `consolepresentation_icons.cpp` and `consolepresentation_styles.cpp` registered in production and test qmake projects; the production Envoy qmake project parsed successfully. `consoleOverviewUsesConfiguredFreshCoBos`, `elapsedShowsHoursMinutesSeconds`, `consoleReferenceLayoutAndLiveSimulation`, `compactConsoleLayoutKeepsParametersAndNodeRowsVisible` and `consoleReflowsWhenConfiguredNodesShrink` passed (7/7 including init/cleanup). The split only relocates existing icon painting and stylesheet definitions; no runtime behavior changes. Only the known offscreen Qt runtime-directory warning appeared. No hardware or DAQ data was used.

## 71.572 ConfigWindow edit-history review separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target with `configwindow_history.cpp` registered in production and test qmake projects; the production Envoy qmake project parsed successfully. `configurationCdataValueEditPreservesCommentsAndReviewedUndo`, `configurationHistoryReviewsUndoRedoAndRejectsStaleWindow` and `configurationStructureEditsEnterTheSameUndoHistory` passed (5/5 including init/cleanup). Only known offscreen Qt size-hint warnings appeared. No hardware or DAQ data was used.

## 71.571 Plan transition partial-retirement completion separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target with `plantransitionwindow_completion.cpp` registered in production and test qmake projects; the production Envoy qmake project parsed successfully. `planTransitionReviewRequiresConfirmationAndRejectsChangedFiles`, `partialRetirementConfirmationBindsSavedOriginalAndKeepsOldControlFenced`, `partialRetirementRejectsStateChangeWhileConfirmationIsOpen` and `planTransitionExportsOnlyMatchedOriginalReceipt` passed (6/6 including init/cleanup). Only known offscreen Qt size-hint warnings appeared. No hardware or DAQ data was used.

## 71.570 Native scan operation confirmation separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target with `nativeruncontrol_scanconfirmation.cpp` registered through shared Envoy `core.pri`; the production Envoy qmake project parsed successfully. `scanEventBaselineRequiresConnectedNativeController` and `scanRunStartRequiresConnectedNativeController` passed in the restricted environment. The full synthetic localhost `nativeScanRunFinishesReceiverAndCatalogAfterEventTarget` test passed under expanded local-network permission (3/3 including init/cleanup), covering event-target Stop, fresh ECC confirmation, receiver Finish/seal, catalog and Run finalization. The fixture used control metadata only; no hardware or DAQ data.

## 71.569 ECC client secure file storage primitive separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target with the secure storage primitive unit registered through shared Envoy `core.pri`; the production Envoy qmake project parsed successfully. `nativeGuiMetadataAndWholeSystemControls` and `nativeSingleCoBoLifecycleUsesWholeEccTransitions` passed (4/4 including init/cleanup), exercising client-backed Native control lifecycle and GUI gates. No hardware or DAQ data was used.

## 71.568 MainWindow control-state update separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target with `mainwindow_control_state.cpp` registered in the production and test qmake projects; the production Envoy qmake project parsed successfully. `nativeStatusProjectionRequiresMatchingLiveReceiverEvidence`, `nativeSystemStatusRowsPreserveReceiverOwnershipAndStateGates` and `nativeGuiMetadataAndWholeSystemControls` passed (5/5 including init/cleanup). Only known offscreen Qt sizing/raise warnings appeared. No hardware or DAQ data was used.

## 71.567 Native receiver failure status projection separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target with the receiver-failure projection unit registered through shared Envoy `core.pri`; the production Envoy qmake project parsed successfully. `nativeStatusProjectionRequiresMatchingLiveReceiverEvidence`, `nativeSystemStatusRowsPreserveReceiverOwnershipAndStateGates` and `nativeStatusObservationCacheOwnsDecodeFreshnessAndReset` passed (5/5 including init/cleanup). No hardware or DAQ data was used.

## 71.566 CoBo maintenance window action separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target with `cobomaintenancewindow_actions.cpp` registered through shared Envoy `core.pri`; the production Envoy qmake project parsed successfully. `nativeCoBoMaintenancePolicySharesPreparedIdentityGate`, `coboMaintenanceRequiresDefaultCancelAndBindsOriginalTarget`, `coboMaintenanceFlipRequiresCancelAndBindsExactGpioBit`, `coboMaintenancePulseRequiresCancelAndBindsExactPeriod` and `coboMaintenanceWithdrawsWrongStateAndForeignMetadata` passed (7/7 including init/cleanup). Only known offscreen Qt size-hint warnings appeared. No hardware or DAQ data was used.

## 71.565 Configuration repository upload workflow separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target with the upload workflow source registered through shared Envoy `core.pri`; the production Envoy qmake project parsed successfully. With an isolated writable Qt test home, `configurationRepositorySamples`, `configurationPublicationBuildsReviewedLocalPackageWhileConnected` and `configurationRepositoryWindowIsSeparateAndRequiresEndpointCredential` passed (5/5 including init/cleanup). The offscreen GUI test emitted known sizing and restricted AF_NETLINK warnings. This covers publication package identity and UI entry, but no live remote repository transfer was exercised. No hardware or DAQ data was used.

## 71.564 Native Run Control recovery action separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target with `nativeruncontrol_recoveryactions.cpp` registered through shared Envoy `core.pri`; the production Envoy qmake project parsed successfully. Native recovery snapshot, recovery evidence, alarm recovery, GUI control and whole-ECC lifecycle tests passed (7/7 including init/cleanup). Only known offscreen Qt sizing/raise warnings appeared. No hardware or DAQ data was used.

## 71.563 Native Run Control inspection separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target with `nativeruncontrol_inspection.cpp` registered through shared Envoy `core.pri`; the production Envoy qmake project parsed successfully. `nativeConfigurationViewSeparatesSelectionAndHardwareProof`, `nativeStatusObservationCacheOwnsDecodeFreshnessAndReset`, `nativeSystemStatusRowsPreserveReceiverOwnershipAndStateGates` and `nativeGuiMetadataAndWholeSystemControls` passed (6/6 including init/cleanup). The offscreen Qt platform emitted only its known sizing/raise warnings. No hardware or DAQ data was used.

## 71.491 DataSupervisor implementation and API responsibility separation (2026-10-25)

Linux GCC/C++11 rebuilt `getdaq-data-supervisor-core`, `data-supervisor-tests`, `managed-ice-supervisor-tests`, `supervisor-control-tests` and `getdaq-supervisor`. CTest `supervisor-control-service` and `data-supervisor-process-lifecycle` passed (2/2; 14.57 seconds total) after moving private implementation methods out of the header and separating run and observation APIs. Synthetic local fixtures cover start/finish/seal, process observation, catalog and recovery behavior. Permitted loopback socket access was required; no physical hardware, production receiver deployment or acquired DAQ raw was involved.

## 71.490 DataSupervisor catalog transaction separation (2026-10-25)

Linux GCC/C++11 rebuilt `getdaq-data-supervisor-core`, `data-supervisor-tests`, `supervisor-control-tests` and `getdaq-supervisor`. CTest `supervisor-control-service` and `data-supervisor-process-lifecycle` passed (2/2; 14.51 seconds total) after moving catalog execution and replacement resume checks. The process lifecycle fixture covers normal catalog creation, staged replacement, failure recovery and finalize verification with synthetic local receiver data. Permitted loopback sockets were required. No physical hardware, production receiver deployment or acquired DAQ raw was involved.

## 71.489 DataSupervisor recovery lifecycle separation (2026-10-25)

Linux GCC/C++11 rebuilt `getdaq-data-supervisor-core`, `data-supervisor-tests`, `managed-ice-supervisor-tests`, `supervisor-control-tests` and `getdaq-supervisor`. CTest `supervisor-control-service` and `data-supervisor-process-lifecycle` passed (2/2; 14.77 seconds total). Lifecycle cases include failed startup abort, failed-session abort/retirement, child signal/exit observation, timeout and receiver seal/file-set checks. Tests use synthetic local processes and receivers; permitted loopback socket access was required because the default sandbox denies socket creation. Documentation-link validation passed 2,002 local links. No hardware, production receiver deployment or acquired DAQ raw was used.

## 71.488 DataSupervisor POSIX receiver process adapter separation (2026-10-25)

Linux GCC/C++11 built `getdaq-data-supervisor-core`, `data-supervisor-tests`, `managed-ice-supervisor-tests`, `supervisor-control-tests`, `getdaq-supervisor` and `getdaq-supervisor-query`. CTest `supervisor-control-service` and `data-supervisor-process-lifecycle` passed (2/2; 14.69 seconds total). The process lifecycle runner covered normal and abnormal child exits, signals, timeout, partial start, receiver observations and storage-related cases with synthetic local receiver fixtures. Tests needed permitted loopback socket access; default sandbox execution was blocked at socket creation. No physical hardware, production receiver deployment or acquired DAQ raw was involved.

## 71.487 ECC network control observation separation (2026-10-25)

Moved cached status, identity, alarms/failure, control handoff, receiver/source/hardware observations and configuration evidence JSON adapter methods from `ecccontroljobs.cpp` into `ecccontroljobs_observation.cpp`. `EccControlJobs::Impl` and its shared internal helpers are defined in `ecccontroljobs_p.h`, allowing both translation units to use the same state representation. Command acceptance, journal mutation, workers and shutdown validation remain in the control implementation; response schemas and lifecycle behavior are unchanged. The primary source decreased from 855 to 275 lines. CMake includes the new unit.

Linux GCC/C++11 rebuilt the Native ECC library, Qt `getdaq-ecc-server`, and `ecc-network-service-tests`. `ecc-network-service-normal-0` passed (1/1, 89.07 seconds): the runner launched local service/supervisor/receiver fixtures, exercised the control/status API and synthetic data flow, then verified clean handoff and shutdown. This does not constitute physical hardware, production receiver deployment or acquired DAQ raw validation.

## 71.486 Native ECC recovery and reconciliation separation (2026-10-25)

`NativeEccCoordinator::Impl` now has one shared private definition in `nativeecccoordinator_p.h`, used by the normal coordinator and new `nativeecccoordinator_recovery.cpp`. The latter owns `Reconcile*`, UNKNOWN/input-failure holds, production-stop and retained failure cleanup, receiver/source retirement and acquisition Unprepare after failure. Public API, operation IDs, journal sequencing and proof gates are unchanged. The coordinator source decreased from 1,060 to 261 lines; CMake registers the recovery unit.

Linux GCC/C++11 rebuilt `getdaq-ecc-native`, `native-ecc-coordinator-tests`, `native-ecc-filesystem-proof-tests` and `cobo-lifecycle-projection-tests`. Filesystem proof, lifecycle projection and host-control ownership tests passed (3/3). Fake-Ice `fault-start` CTest passed; direct `description-recovery-0` and `failure-breakup-unknown` scenarios passed. The latter scenarios use local synthetic Ice servants and a test-double data lifecycle. They do not prove physical hardware behavior, production receiver recovery, DAQ sealing or raw-data correctness.

## 71.485 Waveform canvas rendering component separation (2026-10-25)

The plot widget was split into `apps/waveformviewer/waveformcanvas.{h,cpp}` and registered in the shared `waveformwindow.pri`, used by the standalone viewer, Envoy and Widgets tests. It retains the same 2D plot, rotatable 3D surface and PNG-rendering behavior; file/index/frame/channel handling, analysis and export coordination remain in `waveformwindow.cpp`.

Qt 5.12.8/GCC 9.3/C++11 rebuilt the complete `tests/tests.pro` target and the standalone `apps/waveformviewer/waveformviewer.pro` target. The waveform indexing, compact-frame decode, truncation rejection, pulse metrics, scan summaries, frame navigation and CSV export QtTest functions passed (8 tests; 10/10 including init/cleanup). `getdaq-waveform-viewer --surface-smoke` passed offscreen with 96,353 changed pixels after drag rotation. No hardware or acquired DAQ raw was used.

## 71.484 ScanExecutionJournal operation lifecycle separation (2026-10-25)

Moved `WithOperationIntent`, completion/observation/quarantine/reconciliation transitions, pending-operation target lookup and `IsImmediateSuccessorOf` from `scanexecutionjournal.cpp` into the new `scanexecutionjournal_operations.cpp`. These methods remain members of the immutable journal class and continue to apply the same private validation helpers. Point transitions and snapshot/runtime/event-baseline bindings remain in the state implementation; JSON schema encoding/decoding remains in `scanexecutionjournal_codec.cpp`. No journal schema, event bytes or transition gate changed. The state implementation decreased from 781 to 407 lines; the operation unit is 392 lines.

Linux GCC/C++11 rebuilt `getdaq-experiment-core`, `scan-execution-state-tests`, `system-experiment-tests` and `ecc-journal-tests`. CTest `scan-execution-state` and `system-experiment` passed (2/2); `ecc-journal` passed (1/1). CMake and qmake source lists both include the new unit. Documentation-link validation passed 1,996 local links. Tests use control metadata only; no hardware or DAQ raw was accessed.

## 71.481 Remote acquisition-event query adapter separation (2026-10-25)

Linux GCC/C++11 rebuilt the Native ECC library and remote receiver integration fixture. `native-ecc-remote-receiver-normal-0` passed (1/1, 84.80 seconds); its scenario queries both acquisition-event counts and accepted file bytes from the original Ready receiver session, checking source, session, PID, service instance and fixed plan binding. The test uses local supervisor/receiver processes and synthetic data, not real hardware. Documentation link validation passed 1,993 links.

## 71.480 Remote receiver endpoint identity separation (2026-10-25)

The Linux GCC/C++11 build passed for `getdaq-ecc-native`, `native-ecc-coordinator-tests` and `native-ecc-remote-receiver-tests`. `native-ecc-remote-receiver-normal-0` passed (1/1, 86.91 seconds) with two local supervisor service processes and synthetic receiver data flows. This exercises the normalized fixed endpoint binding through remote lifecycle control; it is not a physical-host or hardware test. Documentation link validation passed 1,992 links.

## 71.479 Receiver catalog inspection separation (2026-10-25)

The GCC/C++11 build passed for `receiver-catalog-tests`, `catalog-transfer-tests` and `getdaq-catalog`. The registered catalog and transfer filesystem tests passed (2/2; 27.05 seconds). They exercise catalog schema/receipt/source seal checks, raw file identity, replacement phase recovery and finalization, plus local transfer resume/readback and source preservation. Fixtures are synthetic and local; no production data host or hardware was used.

## 71.478 Receiver catalog replacement transaction separation (2026-10-25)

The Linux GCC/C++11 build completed for `receiver-catalog-tests`, `catalog-transfer-tests` and `getdaq-catalog`. `receiver-catalog-filesystem` and `catalog-transfer-filesystem` passed (2/2; 27.58 seconds), covering catalog creation/inspection, same-Run candidate planning, replacement publication and recoverable journal phases, finalization, transfer resume/readback and source preservation. Documentation links passed (1,990). Fixtures and transport endpoints were local and synthetic; no real data host, receiver process or DAQ hardware was used.

## 71.477 Receiver catalog replacement plan separation (2026-10-25)

Rebuilt `receiver-catalog-tests`, `catalog-transfer-tests` and `getdaq-catalog` with Linux GCC/C++11. Both registered filesystem regressions passed (2/2; 26.24 seconds): receiver catalog creation/inspection/replacement and transaction recovery, plus transfer identity/resume/readback and source-preservation scenarios. Documentation link validation passed 1,989 local links. All data came from synthetic local fixtures; no real receiver, remote host or hardware was involved.

## 71.476 Receiver catalog request identity separation (2026-10-25)

Linux GCC/C++11 rebuilt `receiver-catalog-tests`, `catalog-transfer-tests` and `getdaq-catalog`. The registered `receiver-catalog-filesystem` and `catalog-transfer-filesystem` tests both passed (2/2; 27.78 seconds total). These cover canonical request encoding/decoding and schemas, catalog inspection/replace transaction evidence, transfer request identity, explicit resume/readback, corruption refusal and source preservation. Local documentation link validation passed 1,989 links. Tests used synthetic filesystem fixtures and local transport endpoints only; no real receiver, remote host or hardware was used.

## 71.475 OpenSSH catalog transfer process boundary separation (2026-10-25)

Rebuilt `catalog-transfer-tests` and `getdaq-catalog` with Linux GCC/C++11. The registered `catalog-transfer-filesystem` test passed 1/1 in 4.29 seconds. Its required output confirms standard/chunked SHA vectors, thread-local SIGPIPE isolation, four oversized TCP metadata rejections before staging, plain TCP new-transfer/interruption-resume/readback/completed-resume/CLI scenarios and 12 native transfer cases, including fake SSH endpoint lifecycle, prefix validation, corruption refusal and source preservation. It generated and cleaned 15,269,888 synthetic raw bytes. The sandbox denied the first loopback socket attempt; rerunning the same local test with loopback access passed. This did not contact a remote SSH host or real hardware.

## 71.474 Native ECC filesystem proof primitives separation (2026-10-25)

Linux GCC 9.4/C++11 rebuilt the Native ECC library and filesystem/coordinator test targets. Three local tests passed: the new filesystem proof test (permissions, symlinks, exclusive create, same-size tampering, retained-prefix append and audit-chain binding), host-control ownership, and CoBo lifecycle projection. `native-ecc-host-ownership` could not start its Ice TCP fixture because the environment denies local sockets (`Operation not permitted`). No hardware was accessed.

## 71.473 Remote receiver observation query separation (2026-10-25)

Linux GCC 9.4/C++11 rebuilt `getdaq-ecc-native`, `native-ecc-remote-receiver-tests` and `supervisor-control-tests`. Added `observe-events` integration assertions for the extracted endpoint observer, including fixed identity/plan binding, available lifecycle data and no ECC/hardware actions. The fixture stopped at Ice adapter setup because local TCP sockets are denied in this environment (`Operation not permitted`), so these runtime assertions remain unverified here. Local Markdown link validation passed 1,986 links. No hardware was accessed.

## 71.472 Configure audit capture separation (2026-10-25)

Linux GCC 9.4/C++11 rebuilt `getdaq-hardware-core` and `hardware-system-release-tests`. A temporary standalone check verified that a valid owner-owned 0600 audit file is captured and a symlink is rejected. Local Markdown link validation passed 1,985 links. The hardware-system release integration executable was not run because its Ice TCP fixture is unavailable under the sandbox's socket restriction. No hardware was accessed.

## 71.471 ECC successor-plan transition protocol separation (2026-10-25)

Separated successor identity, reviewed transition and retirement receipt protocol handling into `eccplantransitionprotocol.cpp` with a shared private strict-validation helper. Rebuilt `getdaq-control-protocol`, Native ECC production and coordinator tests using GCC 9.4/C++11; the ECC protocol and scan runtime identity tests passed. Rebuilt the Qt 5.12.8/GCC 9.3 Envoy target. No hardware or network was accessed.

## 71.470 Scan deployment capture and point binding separation (2026-10-25)

Separated fixed ECC/receiver deployment capture and frozen scan-point binding into `scanruntimeidentity_deployment.cpp`, with shared validation helpers and unchanged public results. Built `getdaq-control-protocol`, `getdaq-ecc-native`, `native-ecc-coordinator-tests` and the Qt Widgets test target. `scan-runtime-identity-tests` passed the identity/receiver freshness/scope/epoch/plan checks. No hardware or network was accessed.

## 71.469 Supervisor handoff and retirement file proofs (2026-10-25)

The data Supervisor's persistent handoff proof and retirement file checks now compile in a dedicated private implementation unit. Built `getdaq-data-supervisor-core`, `native-ecc-remote-receiver-tests` and `supervisor-control-tests` with Linux GCC 9.4/C++11. Ran `supervisor-control-tests codec`, which passed strict plan/request scope and exact status checks. Network-backed handoff/retirement integration was not run because local TCP sockets are denied by the sandbox. No hardware was accessed.

## 71.468 Native ECC accepted-request proof separation (2026-10-25)

Extracted the accepted-operation filesystem proof into a private class and rebuilt `getdaq-ecc-native` plus `native-ecc-coordinator-tests` with Linux GCC 9.4/C++11. The proof continues to enforce request and reviewed-plan directory/file identity, owner/mode and exact content. Runtime coordinator scenarios were not run because local Ice TCP sockets are denied by the sandbox (`Operation not permitted`). No hardware was accessed.

## 71.467 Scan runtime identity protocol codec separation (2026-10-25)

The scan runtime identity wire encoders and decoders now compile from `scanruntimeidentity_codec.cpp`; both codec and observation logic share the private strict-validation helpers. Built `getdaq-control-protocol`, `getdaq-ecc-native`, `native-ecc-coordinator-tests` and `fixed-ecc-configuration-tests` with Linux GCC 9.4/C++11. Ran `scan-runtime-identity-tests`: it passed the validated identity, fresh receiver set, idle-state, data-scope/epoch and exact-plan checks. Local Markdown link validation passed 1979 links. No hardware or network was accessed.

## 71.466 System preparation release-audit writer separation (2026-10-25)

Moved the bounded chained writer used for SystemPreparation release operations into a private implementation unit. The call sites retain the original operation order and audit payloads. CMake rebuilt `getdaq-hardware-core` and `hardware-system-release-tests` with GCC 9.4/C++11. Running the release integration executable was blocked when its local Ice TCP fixture received `Operation not permitted` from the environment; this was a sandbox limitation, not a passing runtime test. No hardware was accessed.

## 71.465 ScanPlanWindow asynchronous process lifecycle separation (2026-10-25)

Moved `ProcessFinished`, `ProcessError` and `closeEvent` from `scanplanwindow.cpp` into `scanplanwindow_process.cpp`. This unit now owns the external metadata tool lifecycle: completion dispatch, catalog/journal outcome handling, policy validation follow-up, plan output verification, start failures, and the gate that prevents closing while an operation runs. Behavior and operation order are unchanged. Both production Envoy and Widgets test qmake projects include the new source. The plan/control implementation decreased from 1,224 to 958 lines; the callback unit is 282 lines.

Qt 5.12.8/GCC 9.3/C++11 rebuilt the production Envoy and complete Widgets target. Rebuilt `getdaq-config` and `getdaq-config-check` from current source and ran the scan-plan GUI integration through offline plan generation, policy creation plus independent checking, duplicate-output rejection and plan review. Along with malformed-plan tamper rejection, 4 QtTest cases passed, 0 failed, 0 skipped (including init/cleanup). The offscreen plugin printed its `propagateSizeHints` notices; no hardware or DAQ raw was used.

## 71.464 Native ECC pure plan compilation and runtime coordination separation (2026-10-25)

Moved validation and compilation from `NativeEccOptions` into canonical preparation options, ordered senders, the immutable ECC snapshot and data-controller scope hash into `nativeeccplancompiler.cpp`. A private `nativeeccplancompiler_p.h` carries the compiled result shared by the pure public planning API and the runtime coordinator. The coordinator still initializes from the same complete compiled result. Input path and digest checks, experiment artifact binding, description/topology validation, sender policy validation and strict JSON snapshot serialization are unchanged. Added the implementation to the Native ECC CMake target.

Linux GCC/C++11 built `getdaq-ecc-native`, `native-ecc-coordinator-tests` and `fixed-ecc-configuration-tests`. The fixed configuration/publication/selection binding and offline scan-deployment integration executable passed and reported `no hardware executed`. The build still reports existing misleading-indentation warnings in `nativeecccoordinator.cpp`; this extraction did not change those unrelated lines. No physical hardware or DAQ data was used.

## 71.463 ScanPlanWindow UI assembly separation (2026-10-25)

Moved the `ScanPlanWindow` UI construction and signal wiring into the private `BuildUi()` method in `scanplanwindow_ui.cpp`. The constructor now creates the asynchronous process and calls that builder; widget configuration, styles, layout, signal connections and construction order are unchanged. The plan/control implementation decreased from 1,546 to 1,224 lines; the UI builder unit is 347 lines. Added the unit to both the production Envoy and Widgets test qmake projects.

Qt 5.12.8/GCC 9.3/C++11 rebuilt the production Envoy and complete Widgets test target successfully. The targeted scan-plan output tamper-rejection and console-entry QtTests passed: 4 passed, 0 failed, 0 skipped (including init/cleanup). No hardware, remote service or DAQ raw was used.

## 71.462 Native ECC remote operation protocol decoder separation (2026-10-25)

Moved canonical decoding of parent control requests and strict remote-operation responses from `remotereceivermanager.cpp` into the private `remoteoperationcodec.{h,cpp}` unit. The manager retains HTTP transport, polling and lifecycle coordination. The codec owns exact field-set checks, request identity binding, phase and status decoding, handoff proof validation, and catalog completion receipt checks. Added the implementation to the Native ECC CMake target; wire schema and validation behavior are unchanged. The manager implementation decreased from 1,508 lines before extraction to 1,445 lines; the codec implementation is 78 lines.

Linux GCC/C++11 built the Native ECC, supervisor and receiver targets. The generated remote receiver integration executable passed `normal-0` using two actual local service processes, a synthetic localhost receiver, and direct raw TCP. The test runner reports that this is not hardware or throughput validation. Documentation link checking passed 1,974 local Markdown links. No real hardware or DAQ data was used.

## 71.461 Scan execution journal model and codec separation (2026-10-25)

Moved `ScanExecutionJournal::Encode/Decode` and versioned event JSON persistence into `scanexecutionjournal_codec.cpp`. `scanexecutionjournal.cpp` retains lifecycle transitions, operation intent handling and successor/invariant checks. Field parsing, state mapping, identity and event validation remain single-source shared helpers in private `scanexecutionjournalinternal.h`. The state implementation decreased from 1,613 to 781 lines; codec is 671 lines. Public API, schema compatibility and runtime decisions are unchanged. Added the codec to CMake and `experiment.pri`.

Linux GCC/C++11 built `getdaq-experiment-core` and `scan-execution-state-tests`; the state lifecycle/restart-quarantine/tamper-rejection executable passed. Qt 5.12.8/GCC 9.3 regenerated and fully built the production Envoy and Widgets test targets with the new codec unit. `scanExecutionJournalRoundTripQuarantineAndReconciliation` passed (3 QtTest cases including init/cleanup). The full Widgets suite, hardware and live DAQ were not run.

## 71.460 Receiver catalog and scan archive analysis separation (2026-10-25)

Moved scan-point/catalog binding, binding-index summarization, Threshold hit-pattern analysis and PeakingTime/Pulser/AutoGain waveform analysis from `receivercatalog.cpp` into `receivercatalog_scananalysis.cpp`. Strict catalog JSON parsing, canonical signed-integer parsing and file metadata identity comparison are shared through private `receivercataloginternal.h`; the security-sensitive metadata comparison is not duplicated. The catalog implementation shrank from 1,747 to 1,120 lines; the new analysis unit is 660 lines. Public APIs and validation order are unchanged.

Linux GCC/C++11 rebuilt `getdaq-data-supervisor-core`, `getdaq-catalog` and `receiver-catalog-tests`. Direct execution of the generated catalog test binary passed all 68 checks, including catalog filesystem handling, scan binding, Threshold aggregation and streamed PeakingTime/Pulser/AutoGain analyses. This build tree exposed no CTest cases, so the generated test executable was run directly. No real hardware, receiver process, network or DAQ data was used.

## 71.459 MainWindow menu assembly and controller initialization order (2026-10-25)

Moved Configuration, Tools, Run and Recovery menu/action assembly into `mainwindow_menus.cpp` and GUI-to-RunControl signal wiring into `mainwindow_connections.cpp`. `RunControl` is now constructed with `MainWindow`, before menu signal wiring; this removes the previous ordering where the scan-plan menu connected through `fRunControl` before that pointer was assigned. Menu actions, control gates, and signal behavior are unchanged. The remaining `mainwindow.cpp` is 547 lines, down from 1,234 before the status/log/menu/connection extractions.

Qt 5.12.8/GCC 9.3/C++11 production Envoy and full Widgets target rebuilt from current source. Nine targeted QtTests passed for the scan-plan menu/window, Native and Legacy control gates, ECC observation views, logging, automatic-run GUI locking/cycles, and manual Run-number rollback. `make -q` reports both targets up to date; documentation link check passed 1,972 local links. Full Widgets and localhost-mock suites were not run. No hardware or DAQ raw was used.

## 71.458 MainWindow status projection and connection logging extraction (2026-10-25)

Moved `ShowStatusSlot`, `ShowControlSlot`, Native ownership/configuration observations and their renderers from `mainwindow.cpp` to `mainwindow_status.cpp`. Moved severity-preserving visible-log output, per-connection log creation/closure and visible-log export to `mainwindow_log.cpp`. Added both units to the production `envoy.pro` and test `tests/tests.pro`; no slot signatures or control rules changed. The main implementation file decreased from 1,234 to 902 lines.

Qt 5.12.8/GCC 9.3/C++11 production Envoy and full Widgets test targets rebuilt successfully from current source. Nine targeted QtTests passed, covering status projection and whole-system gates, single-CoBo scope, stale ownership/configuration views, per-connection logs, visible-log export and severity coloring, and dynamic status-table height. Eight more offline-tool behavior cases passed for all close gestures, close-pending menu lock, simultaneous-window Run lock, and unlock after both windows close; the multi-window case was run with the repository-built viewer and receiver tools on `GETDAQ_TEST_OFFLINE_TOOLS_DIR`. There were no skips in the final runs. `make -q` reports both targets up to date; documentation link check passed 1,971 local links. The full Widgets suite and localhost-mock cases were not run in this extraction check. No hardware, receiver or DAQ raw was used.

# 验证记录与待验收项

## 71.457 RunControl dispatch and Run finalization unit extraction (2026-10-25)

Moved 37 Native recovery, maintenance, scan and observation slots from `runcontrol.cpp` to `runcontrol_native.cpp`. The extracted unit retains the mixed Legacy/Native Pause and Resume dispatch; Legacy state checks and every Native call remain unchanged. Moved six finalization methods—catalog, local file completion, replacement cleanup, completion publication and Finalize entry—to `runcontrol_finalization.cpp`; Legacy lifecycle reply handling remains in `runcontrol.cpp`.

Qt 5.12.8/GCC 9.3/C++11 rebuilt the production Envoy and full Widgets test targets from current source. Targeted QtTest covered Legacy whole-system Describe/Prepare/Configure/Start/Pause/Resume/Stop ordering, Run reservation failure, replacement cleanup sequencing and no-recatalog retry, plus Data file tools menu close/reopen: 8 passed, 0 failed, 0 skipped. The full Widgets suite was not counted as passing in the restricted sandbox because localhost mock listeners and the user cache directory were unavailable. No hardware, receiver or DAQ raw was used. Windows current-source rebuild was blocked by WSL interop (`UtilBindVsockAnyPort: socket failed 1`); Windows behavior remains unverified.

## 71.448 冻结快照生成逐点固定部署包（2026-10-25）

新增离线`CreateScanPointDeployments`及POSIX native入口`getdaq-scan-deploy create PLAN_DIRECTORY POLICY.json SNAPSHOT.json ECC_TEMPLATE.json NEW_BUNDLE_DIRECTORY RECEIVER_PLAN.json TARGET_DIRECTORY [...]`：从已验证的`ScanExecutionSnapshot`逐点读取并重核artifact字节，为每点生成sealed artifact、ECC fixed service JSON和每个数据agent的原生receiver plan。每个receiver plan须附其远端目标目录并按ECC模板endpoint顺序传入。每点配置绑定独立的deployment ID、ECC/receiver审计root、扫描点配置与artifact SHA；ECC endpoint引用新receiver plan摘要。manifest列出本地文件、每个agent的目标plan路径及目标审计root所需父目录。输入receiver模板必须规范、摘要与固定ECC endpoint相符，agent顺序精确，receiver source ID集合与父ECC sender集合完全一致。现有configuration publication/selection模板目前明确拒绝，因为需要为每个点重新生成publication及effective snapshot。CLI通过现有两点Native模拟输入完成实际包生成，并拒绝覆盖已存在的输出目录。

Linux GCC/C++11增量构建`getdaq-scan-deploy`及`fixed-ecc-configuration-tests`通过；允许localhost Ice mock后完整固定配置集成测试退出码0，新增两点扫描回归验证每个生成的ECC配置可重新解析、artifact摘要与冻结快照一致、receiver plan及endpoint摘要相同、receiver来源集合合法、各点ECC绑定与receiver审计root不同、目标计划路径及审计父目录正确。相同fixture还调用实际CLI并验证两点manifest及输出目录独占拒绝。测试明确输出`no hardware executed`。该包没有安装远端plan、创建数据主机目录、退休当前ECC/receiver实例、启动successor或推进scan journal；它不构成多点自动执行完成。当前验证没有真实板卡、接收器、远端主机或DAQ raw。

## 71.447 离线工具可用性与自动Run测试等待上限（2026-10-24）

Run tool与真正启动共用输入/输出准备函数，编辑/增删文件/切模式/任务状态变化时刷新；未选择输入、单输入转换选多个文件、未填或已有输出时按钮灰显，tooltip解释原因，未选项时Remove selected灰显。实际QProcess启动前再次核对，任务结束后已有输出会保持拒绝覆盖；不新增后台文件轮询。

Linux Qt5.12.8/GCC9.3/C++11生产Envoy和Widgets目标构建成功，最终增量无告警。真实临时合成文件inspect/matrix/merge、保留已有输出、移除项后禁用、叉号/Close/Esc的空闲/有子进程关闭及Run互锁专项13/13通过（3.635秒）。首次完整Widgets为173通过/1失败，自动Run仿真在约5.15秒满足两轮完成条件，超过QtTest默认5秒上限；保留失败日志，将该测试的等待上限设为15秒，未改产品策略或≥3000ms单调时间断言。Time/FileSize/Either补测通过；最终完整Widgets **174/174**通过（105.805秒，无跳过/失败）。截图、源码/程序摘要、前后日志见[工具可用性证据](../build-ui/offline-tools-readiness-20261024/README.md)。无实板、现场DAQ或Windows/macOS验收。

## 71.446 CoBo日志终点与审计镜像（2026-10-24）

Linux GCC/C++11重建生命周期、准备、采集、释放和Native协调器测试目标及getdaq-hardware、getdaq-system-config、getdaq-ecc-server生产目标。日志单元目标通过：检查点严格codec、初始/最终终点、整行尾部丢失、过期检查点、同长度同记录数但不同合法链，以及原有权限/替换/部分写入/进程中断等路径。依赖Native源码重建有既有misleading-indentation告警，未声称全工程无告警。

共36项localhost Ice mock后端CTest最终全部通过（真实墙钟1235.97秒）；[产物入口](../build-cobo-lifecycle-checkpoint-20261024/README.md)保留构建与单元证据。新增集成断言从真实准备/采集操作审计提取初始及各阶段检查点，逐一比对历史前缀，并以最终独立终点拒绝完整尾行丢失；故障负向停止要求UNKNOWN/null终点。新增检查点集成3种场景现已通过（104.87秒），上述终点/尾部拒绝和UNKNOWN/null负向停止断言均实际执行。准备331.80秒、采集352.05秒、故障停止/Breakup505.44秒、Native正常编码0/1也已通过；最终36/36通过，ctest-final.log和completed-tests-final.log为完整证据；早期progress快照仅用于追溯。无真实板卡、现场DAQ或跨平台验收，不恢复冷启动控制权。

## 71.445 Tools离线数据组件、计时与关闭回归（2026-10-24）

生产Envoy、独立waveform viewer、Widgets和Native控制目标均从当前相关源码重新构建；离线inspect/matrix/merge目标构建并复制至bin。未引入主程序ROOT/CFITSIO链接依赖。

- 专项QtTest **13/13**：Elapsed进位/负值/25小时、共享波形菜单、普通/扫描点启动互锁、两个离线窗口的组合互锁、外部活动状态关闭窗口；真实CLI检查/矩阵/合并、输入不变、含空格和分号的路径、已有输出拒绝、稀疏帧转换失败；未安装选项禁用；空闲和后台任务运行时的叉号/Close/Esc共6种关闭场景。
- Native控制完整QtTest **42/42**（42.643秒），使用localhost mock服务，无实板。
- 独立waveform viewer包测试：help/离屏窗口启动通过；3D绘制/拖动通过，changed_pixels=96353。独立程序和共享窗口使用同一实现。
- 生产GetDAQ离屏初始截图、Ready/Running宽窄窗口、实际菜单灰字和工具完成窗口已检查，截图及日志见[验证产物](../build-ui/offline-tools-20261024/README.md)。

最终完整Widgets **174/174**（105.662秒，无跳过/失败），完整日志保存在同一验证目录。程序发现缓存不依赖状态轮询重复扫描PATH；工具通过独立argv启动，受管进程停止后才销毁窗口及解除Run互锁。工具输出显示上限256KiB/2000行。可选ROOT/FITS只验证缺失时禁用及模拟查看器进程管理，未运行真实ROOT/FITS转换。未访问真实DAQ硬件，不代表Windows/macOS/现场验收。MCP/外部活动仅在GUI观察后触发关闭，存在观察延迟。

## 71.444 自动Run、诊断着色与运行中Enable开关（2026-10-24）

Run setup顶部右侧加入Automatic Run，Apply与标题/Enable同行，状态文字独占整行并可换行；左侧YAML与Experiment/Run行向上收紧。Time和raw File size独立选择，同时选择为OR，任一阈值到达即Stop或开始收尾后续跑；不使用文件数量。轮数包含第一轮。自动续跑必须等当前Run的封存、归档及journal成功Complete，之后至少再等3000 ms，并确认新鲜Ready/空接收会话；失败、未知、指标失效、session/PID变化或下个Run号冲突均取消续跑。

阈值、动作、轮数和Apply在Run及两轮间隔中锁定，控制线程也拒绝绕过GUI的参数修改。Enable使用独立开关：运行中关闭只取消自动触发/续跑，不停止当前采集或撤销已开始的收尾；重新开启只使用已保存条件，从当前确认的Running/Paused重新建立窗口。Pause/Resume、手动停止和外部/MCP状态变化继续走实际控制状态；设置可保存但程序重启/恢复不会自动开始采集。Event log为警告橙色、错误红色，原始消息/多行/时间戳、纯文本导出和会话日志保持。

Legacy Sentry增加精确目录字节字符串，兼容旧GB报告；Native新增认证只读receiver-file-bytes观察，经固定plan/instance/source/session/PID绑定读取accepted_file_bytes。旧事件数端点保持原格式；写入计数不是fsync落盘证明，最终完成仍取决于原封存路径。新接口需要同步部署ECC/supervisor/receiver；控制侧不读取raw内容，没有改动接收数据热路径。

Qt5.12.8/GCC9.4/C++11生产Envoy、Sentry、Native ECC/supervisor/receiver及测试目标构建成功。最终完整Widgets **163 passed、0 failed、0 skipped**（100.531秒），Native控制 **42 passed、0 failed、0 skipped**（42.196秒）。专项UI **7/7**、Native自动Run与Enable **4/4**；Simulation及Native三种条件两轮测试均以单调时钟断言成功收尾到下一Start至少3000 ms。覆盖慢收尾、Ready不足、手动停止、轮数上限、归档失败、Run号冲突、大小会话变化/失效、外部Pause/Resume/Stop、运行中参数拒绝和Enable关开后计数重建。日志测试核对逐块颜色、原多行文本及2000行裁剪；GUI测试核对双条件文字换行、顶部对齐、运行/间隔锁定、12→2→1节点收缩及窄屏入口。

另通过纯C++11 ECC协议和supervisor协议测试；真实本地receiver在两个supervisor服务进程下接收测试帧，核对raw字节数、原session/PID与实际写入一致，新只读查询没有新增硬件动作。1536×1024/1280×800实际Qt截图检查通过。产物与完整日志见[验证目录](../build-ui/automatic-run-20261024/README.md)，使用/部署规则见[Automatic Run](AUTOMATIC_RUN.md)。全部为Simulation、localhost模拟硬件或测试帧；没有现场部署、实板、Windows/macOS或吞吐验收，完整项目重构继续。

## 71.443 Qt第二轮视觉精修与动态节点区收缩（2026-10-24）

保持现有Qt Widgets布局、右上参数表、全宽节点表及下方左曲线/右日志。统一13 px正文、标题/按钮字重、白灰背景和小圆角；输入框36 px、采集按钮38 px、节点行高24 px，Run号14 px半粗体。内置XPM箭头解决QSS自定义步进/下拉子控件后箭头消失的问题，无新增第三方UI依赖。采集面板按内容定高，小窗口收紧边距并在resize事件后复核子控件布局，避免两行按钮重叠/裁切。单节点Regress、选择框和Progress连续排列，右侧顺序提示使用剩余宽度。

节点表及面板按真实行数设高度上限，重设配置时显式更新竖向splitter分配；12→2→1节点后曲线/日志随即上移。保留竖向分隔线调整及节点表局部滚动。下方水平分隔线默认50/50，也可独立调整比例。图例在标题下按可用宽度换行；日志使用等宽字体。CoBos ready仍为真实数量，Disk max继续不显示，节点磁盘字段保留；Native既有无同口径速率时隐藏曲线的行为不变。后台、协议、FSM、采集流程、真实数据与确认/启用条件未修改。

Linux Qt5.12.8/GCC9.4/C++11生产Envoy及Widgets测试目标增量构建成功，最终构建日志无编译警告。最终完整QtTest **153 passed、0 failed、0 skipped**（62.846秒），含新增动态节点收缩、上下/左右分隔调整、输入框高度及控件边界检查。首轮完整回归有1项首帧尺寸断言在Qt异步布局稳定前读取；改为等待完成后仍要求宽屏无纵向滚动，最终全套通过，没有放宽验收条件。

实际离屏Qt截图已逐项检查：[1536×1024 Ready](../build-ui/run-control-polish-20261024/ready-1536x1024.png)、[1536×1024 Running](../build-ui/run-control-polish-20261024/running-1536x1024.png)、[1280×800 Running](../build-ui/run-control-polish-20261024/running-1280x800.png)、[减少至2节点](../build-ui/run-control-polish-20261024/compact-2-node-1536x1024.png)、[减少至1节点](../build-ui/run-control-polish-20261024/compact-1-node-1536x1024.png)、[生产入口1.5倍DPI](../build-ui/run-control-polish-20261024/production-hidpi.png)。1536×1024展示全部11项参数和12节点；1280×800允许表格局部滚动。宽屏Run setup保留为右侧完整参数表所需的高度差。全部使用Simulation/localhost mock，未进行实板、现场显示器或Windows/macOS验收。

文件清单、源码差异、采样/动作方法逐字核对、日志与散列在[本轮验收目录](../build-ui/run-control-polish-20261024/README.md)。完整项目持续重构及平台交付未由本轮GUI精修完成。

## 71.442 速率曲线与日志左右等宽及总览简化（2026-10-24）

按用户新要求，节点表下方改为左侧速率曲线、右侧 Event log，横向布局等比例分配宽度；原竖向 splitter 继续调整节点区与下方并排区域的高度。节点表高度预算相应按下方共同高度计算。CoBo 就绪数改为 `11 CoBos ready`，移除 `Disk max` 总览，节点表既有磁盘字段不变。采样、日志及控制命令未改。

Qt5.12.8/GCC9.4/C++11生产Envoy和Widgets测试目标构建通过，无编译警告。现有布局、Simulation生命周期、Run号编辑、Native状态/维护以及摘要六个用例共 **8 passed、0 failed**（含init/cleanup，2.786秒）。新增布局断言核对1536×1024与1280×800下两面板左右次序、顶端对齐、宽度差不超过1px，DiskOverview不存在。实际生产初始窗口及两种尺寸Running截图检查通过：[1536×1024](../build-ui/run-control-side-by-side-20261024/running-1536x1024.png)、[1280×800](../build-ui/run-control-side-by-side-20261024/running-1280x800.png)。只用Simulation/localhost mock；本次未重复完整Widgets套件或实板/目标平台验收。

## 71.441 参考图驱动的Qt浅色Run控制台重构（2026-10-24）

按用户提供的完整界面任务和效果图，保留 C++11 / Qt Widgets，将原 Configuration/Run details 合并为 Run setup；顶部四个菜单独立放在标题与状态标签之间，删除顶栏独立 LED。采集按钮按 Run start、Run stop、Connect、Disconnect、Regress all、Progress all、Retry finalization、Recover journal 排列，宽屏单行、窄屏四列两行。右上参数面板保留全部字段和编辑/增删，下方设备表、曲线及日志全宽；总览利用参数表旁的左侧空间，位于设备表上方。

新增独立灯列以表头视觉顺序放在最左侧，原数据逻辑列与 Node ID 保持不变；灯与原状态文字分开，提供 tooltip 和可访问文本。总览从固定配置及实际 `NodeStatus` 计算 CoBo Ready 数、Sentry 总 MB/s、最大磁盘占用和节点错误数，缺失数据不报健康。MuTaNT 不进入 CoBo 数/速率/磁盘统计；Native 逐板 UNKNOWN 投影参与灯色及总览。Native 协议没有同口径 MB/s/磁盘观察，仍显示未知，并保持原本隐藏该 Sentry 曲线的行为。

主界面 QSS、矢量操作图标、健康色和摘要计算集中到 `consolepresentation`，布局与响应式排布归入 `mainwindow_console`；主要样式限定在 centralwidget，维护/恢复等对话框不承受新主界面主题。曲线只改变绘制，增加清晰坐标、11色稳定图例和浅色网格；原采样、节流及有界历史方法逐字相同。16个既有操作/配置方法与修改前源代码逐字比较相同，Add parameter操作仅将20 px行高字面量改用同值展示常量，原确认流程、后台线程、协议与采集命令未由本次UI重构改动。上一节逐板日志属于此前持续重构的独立工作，已单独验明。

Qt 5.12.8 / GCC 9.4.0 / C++11 生产 Envoy 与完整 Widgets 测试目标构建通过，最终构建无警告。专项 **7/7通过**；最终完整 Widgets QtTest **152 passed、0 failed、0 skipped**（62.103秒），包括 YAML/配置、Run元数据、维护/恢复确认、单节点、日志和真实 Simulation 的 Prepare/Run/Stop。新增布局用例验证1536×1024与1280×800的8个按钮无重叠、宽屏单行、完整12节点、表格/选择框双向绑定及编辑锁定；新增摘要用例覆盖MuTaNT排除、缺失观察、无效数字和Native未知指标。

人工检查实际Qt窗口截图：1536×1024 Ready/Running、1280×800 Running；另生产入口以1.5倍高DPI离屏启动，生成2304×1536像素截图。截图来自实际 Simulation 数据和控件，无硬编码截图状态或重画界面。产物：[1536×1024 Ready](../build-ui/run-control-20261024/ready-1536x1024.png)、[1536×1024 Running](../build-ui/run-control-20261024/running-1536x1024.png)、[1280×800 Running](../build-ui/run-control-20261024/running-1280x800.png)、[高DPI](../build-ui/run-control-20261024/getdaq-console-hidpi.png)；测试日志在同目录，调整入口与统计口径见[界面说明](RUN_CONTROL_UI.md)。未连接实板或现场receiver，没有 Windows/macOS 或实体显示器人工验收；全套重构与平台交付继续。

## 71.440 CoBo连续状态持久记录与日志故障停止回归（2026-10-24）

准备与采集对象共用的 `CoBoLifecycleLedger` 现于新的 Prepare 中、首个硬件步骤前建立独占 `cobo-lifecycle.jsonl`。scope 绑定原准备目录和配置/拓扑审计前缀，逐行绑定序号、固定 ID/索引、前后态及 SHA256 链；完整追加和 fsync 后才发布状态。文件身份、0600 权限、大小和原字节前缀发生变化时锁存失败，正向硬件联锁拒绝继续。转移或原操作审计镜像写入失败时，UNKNOWN 不会被旧状态掩盖。

独立负向停止路径允许在生命周期日志坏掉后继续原先可审计的各个生产停止步骤，保留逐步骤去重证明；目标保持 UNKNOWN，新操作日志标明 `lifecycle_persisted=false`。`ReadHistory` 仅核对已经关闭写入者的历史，不能恢复硬件对象或授予控制权。原 Native 重启 UNKNOWN 与禁止自动重放保持；完整单板 FSM 和跨服务恢复仍未完成。文件位置、预算和散列链的适用边界见 [持久记录说明](COBO_LIFECYCLE_JOURNAL.md)。

GCC 9.4.0/C++11 重建硬件模块、Native ECC 服务及测试目标成功。第一批 CTest **5/5通过**（355.50秒）：生命周期矩阵、日志故障/部分写入/进程中断、三场景持久化集成（102.81秒）、完整 Prepare（334.03秒）及采集（355.50秒）。第二批 **7/7通过**（443.10秒）：Breakup 停止、配置失效失败/成功、Native 正常两种编码、启动失败及负向审计失败。新增集成直接验证 Prepare→Configure→Connect→Start→Pause→Resume→Stop→Disconnect 的同一日志，以及损坏日志后拒绝 Start、实际硬件停止仍执行且不重复。原生增量构建无新增文件编译警告；首次重建既有 Native 文件仍有其原有 misleading-indentation 警告。

全部仅用本机 localhost Ice 模拟硬件和测试 sender，无实板、现场 receiver 或 DAQ raw；没有 Windows/macOS、当前全套运行包或吞吐验收。

## 71.439 Native操作结果按工作流分流及连接状态徽标整理（2026-10-24）

`NativeRunControl::Operation()`由235行收为38行，保留公共的原意图观察持久化、operation ID关联、未知结果处理和终态解码，再按原顺序分派给catalog续作、扫描、单CoBo维护、计划退役、故障恢复及普通生命周期处理方法。各结果方法归入对应的既有编译单元；新鲜状态驱动的普通Run收尾与扫描receiver/catalog证据推进也分开。扫描的deadline和未知结果记录统一归入扫描模块，`ScanState`的构造/析构都在该模块定义，主控制单元不再包含扫描journal、runner与协调器的实现头文件。原请求、状态门槛、信号、journal写入及后续GET顺序保持原样；重连观察旧意图仍不提交新命令。

新增本机HTTP集成用例覆盖七种单CoBo维护命令，以及明确失败、没有保持Prepared、丢失非幂等flip应答三种情况；独立计数每次收到的HTTP POST，核验每条命令只提交一次、原意图可读、没有Run完成信号或Run目录。另覆盖启动后读取旧flip意图的终态，服务端没有收到任何新POST。在本机Qt 5.12.8/GCC 9.4.0/C++11下，完整Native控制集成 **38 passed、0 failed、0 skipped**（16.109秒）；控制器拆分后完整Widgets回归 **150 passed、0 failed、0 skipped**（59.715秒），包含单扫描点事件数Stop、receiver封存与catalog归档流程。

随后将连接灯与Phase文字归入同一连接状态徽标，灯固定于徽标右端，不再悬在模式与状态文字之间；文字和状态背景仍随状态更新。该GUI改动后，状态映射、徽标内几何位置、紧凑参数布局及节点表高度三个定向用例共 **5 passed、0 failed**（含init/cleanup，0.160秒）；生产Envoy重新构建无编译警告，1500×1000离屏截图检查通过，Markdown本地文件链接1899项通过。全部使用本机窗口、localhost mock或合成数据，新增维护用例仅交换控制元数据；未连接实板或现场receiver。全项目重构、逐CoBo持久FSM对齐和跨服务恢复仍未完成。

## 71.438 Envoy白底控制台菜单定位与Native计划转换职责拆分（2026-10-24）

主界面采用中性白灰画布、白色分区卡片和克制的青绿色主操作色，隐藏系统菜单栏；Configuration、Tools、Run、Recovery四个菜单入口作为一组位于标题与右侧状态标识之间。顶栏状态灯在断开时为红色、连接时为绿色、操作忙碌时为琥珀色、控制故障时为红色，并保留文字状态。配置操作改为三个等宽横排按钮，Run细节保持单行；采集按钮按连接、阶段转换与Run/恢复两行分组，行间留出间距且卡片高度保证按钮完整显示。Connect/Run start为绿色主操作，Run stop为红色，Regress和Progress使用低饱和红/绿区分方向。Run记录参数保留在右上方，设备状态表保持全宽，曲线和日志仍在下方全宽显示。


`NativeRunControl`中四个计划转换/退役方法移至独立`nativeruncontrol_plantransition.cpp`编译单元，原状态门槛、receipt检查、提交顺序及错误语义不变。Linux Qt 5.12.8/GCC 9.3/C++11完整Widgets QtTest **150 passed、0 failed、0 skipped**（59.784秒）；GUI专项测试验证状态灯状态映射、菜单居中、配置按钮均分及Run控制按钮未被裁切。`apps/envoy/envoy.pro`生产目标构建成功，1500×1000离屏截图检查确认白灰配色、青绿/红色语义强调与参数表布局；Markdown本地链接检查1898项通过。测试未连接实板、真实receiver或DAQ raw；Windows、macOS和现场显示仍待验证。

## 71.437 Envoy色彩与设备表留白修整（2026-10-24）

配置窗口采用中性炭灰色，状态表不再以浅色整格底纹标记状态，改用低饱和前景色；曲线背景、网格和数据线改为与控制台协调的色彩。Run记录参数表隐藏行号并缩窄参数列，保留更宽的值列。设备状态表高度现在根据配置节点的标题及数据行计算，关闭无用的纵向滚动条；状态操作按钮紧邻表格，下方曲线和日志在主窗口全宽范围内使用剩余空间。

新增QtTest验证2节点紧凑窗口下参数值列仍可见、12节点状态表逐行显示且面板不再拉伸出大块空白。完整Widgets QtTest **150 passed、0 failed、0 skipped**（59.545秒）；`apps/envoy/envoy.pro`生产目标重建成功。测试使用本地窗口、合成状态和localhost mock，不连接实板、真实receiver或DAQ raw；未在Windows、macOS或现场显示器验收。

## 71.436 Envoy控制台布局收紧与Native单CoBo维护单元拆分（2026-10-24）

Envoy保留上半区左侧控制、右侧参数表以及下半区全宽设备/磁盘状态、曲线、日志的结构。移除标题副标题和日志面板顶部重复提示；YAML配置与Run细节保持单行，连接/阶段控制和Run/恢复控制分成两行。Run参数表每行固定22 px，参数表宽度保持约380 px。整机及选中节点生命周期按钮仍为Regress在左、Progress在右；正式Run按钮名称和位置仍为Run start在左、Run stop在右。应用深蓝灰背景、青绿高亮及明确的绿/红Run按钮配色。日志会话继续自动留存，手动导出入口保持在Tools > Logs。

将`NativeRunControl`的单CoBo目标查询、维护准入适配及重准备、配置、LED、pulser等命令实现迁到`apps/envoy/nativeruncontrol_maintenance.cpp`，并加入Envoy构建清单；控制器仍拥有异步操作、phase、deadline、journal和结果路由，维护准入、operation ID及失败语义不变。

Qt 5.12.8/GCC 9.3/C++11完整`tests/tests.pro`从当前源码构建成功；完整offscreen QtTest **148 passed、0 failed、0 skipped**（59.006秒）。布局专项断言验证连接/阶段与Run/恢复按钮分行、参数行高22 px、上层参数区位于右侧，以及设备/磁盘状态表、曲线、日志均与下层Splitter同宽。`apps/envoy/envoy.pro`生产目标从当前源码独立重建成功。测试使用本地窗口和合成/localhost控制mock，不连接实板、真实receiver或真实DAQ raw；未在Windows、macOS或现场显示器验收。

## 71.434 Native计划退役状态生命周期归组（2026-10-24）

Native计划退役请求、原始receipt状态快照、退役完成标志及“新鲜状态读回后检查receipt”标志现归入`PlanRetirementState`；Connect统一调用`Reset()`清除整组会话状态。提交前状态保存、原operation引用、异步状态确认和receipt查询的触发顺序保持不变，其他GUI信号和store/client职责不迁移。

计划退役确认、确认期间状态变化拒绝、失败data-handoff补齐界面及缺失查询帮助器降级四个定向QtTest **6 passed、0 failed**（含init/cleanup）；完整Widgets QtTest **147 passed、0 failed、0 skipped**（59.669秒），生产`apps/envoy/envoy.pro`独立构建成功。验证使用本地窗口/合成元数据，不连接实板、真实receiver或DAQ raw。

## 71.433 Native CoBo维护准入与命令分类策略独立化（2026-10-24）

七个CoBo维护入口共用`CanSubmitNativeCoBoMaintenance()`，统一要求控制层可用、无活动Run、controller处于健康Prepared，且当前ECC instance/snapshot与维护窗口原始目标完全匹配；控制器仍负责调用`Available()`及各入口专属提示、参数校验和客户端提交。成功/未知operation回调共用`IsNativeCoBoMaintenanceCommand()`识别维护命令，集中白名单，避免结果信号分支与命令集合漂移。

新增策略回归覆盖可提交及不可用、活动Run、非Prepared、controller error、instance/snapshot空值或变化，并验证七种维护命令及非维护命令分类。定向QtTest **5 passed、0 failed**（含init/cleanup），完整Widgets QtTest **147 passed、0 failed、0 skipped**（59.933秒），生产`apps/envoy/envoy.pro`独立构建成功。验证使用合成状态和localhost mock，无实板、真实receiver或DAQ raw。

## 71.432 Native告警观察缓存和freshness生命周期归属拆分（2026-10-24）

Native ECC告警响应的解码、最近成功结构缓存、原始响应字节和freshness计时器现由`NativeStatusObservations`统一持有；`NativeRunControl`继续处理操作日志、Run journal、恢复门槛和控制决策，三条告警恢复入口复用同一解码观察及freshness。连接/HTTP传输错误会撤销freshness但保留最后成功观察；JSON解码失败也保留最后成功缓存且不刷新freshness，保持既有错误处理语义。

新增`nativeStatusObservationCacheOwnsDecodeFreshnessAndReset`回归覆盖成功/重复观察、解析失败、传输失败、重新观察及Reset。包括该用例和四个Native恢复策略用例的定向QtTest **7 passed、0 failed**（含init/cleanup）；完整Widgets QtTest **146 passed、0 failed、0 skipped**（62.465秒）；生产`apps/envoy/envoy.pro`独立构建成功。验证使用合成控制元数据及localhost mock，不连接实板、不启动真实receiver或生成DAQ raw。

## 71.431 Native告警恢复operation匹配策略独立化（2026-10-24）

将UNKNOWN acquisition/preparation告警确认和已知stopped Run告警释放的告警身份判定抽到`MatchesNativeAlarmRecovery()`。两种模式都绑定原ECC instance/snapshot及已启用、latched的告警；UNKNOWN确认要求`complete`、有告警operation ID、原controller为UNKNOWN且controller operation一致。已知停止释放允许`complete`/`held-no-run`、拒绝UNKNOWN，并按原规则优先匹配非空alarm operation ID、否则匹配controller operation ID。调用方保留原有状态、freshness门槛、命令与错误消息。

新增策略回归覆盖两种合法模式，以及phase/outcome、operation、instance、snapshot、latched状态不匹配的拒绝。四个恢复策略定向QtTest **6 passed、0 failed**（含init/cleanup）；完整Widgets QtTest **146 passed、0 failed、0 skipped**（59.344秒），生产`apps/envoy/envoy.pro`独立干净构建成功。测试用合成控制元数据和已有localhost mock，不连接实板、不启动真实receiver或生成DAQ raw。

## 71.430 Native失败catalog续作资格策略独立化（2026-10-24）

将`ResumeCatalogReplacement()`中的纯资格条件抽到`CanResumeNativeCatalogReplacement()`：续作必须绑定journal中同一失败replacement operation、不同且非空的新operation ID、明确replacement意图、新鲜状态、无活动操作或reconciliation hold、controller已知Prepared，以及存储层确认原catalog替换仍可续作并允许提交命令。控制器仍负责查询本地存储/client能力、发起原续作请求及处理失败；不增加重试或自动续作。

新增策略回归覆盖正常Prepared续作及失败operation不匹配、operation ID重用、replacement意图缺失、上下文/状态过期、存储/命令拒绝、controller transition/error/UNKNOWN/reconciliation/活动操作和非Prepared状态。三个恢复策略定向QtTest **5 passed、0 failed**（含init/cleanup）；完整Widgets QtTest **145 passed、0 failed、0 skipped**（59.475秒），生产`apps/envoy/envoy.pro`独立干净构建成功。使用localhost mock与合成控制元数据，无实板、真实receiver或DAQ raw验收。

## 71.429 Native恢复运行证据策略独立化（2026-10-24）

将`NativeRunControl::Recover()`中的当前运行证据匹配抽到只读`NativeRecoveryEvidenceError()`：核对原ECC实例和snapshot、保存receiver覆盖，以及每个receiver的session ID、PID、输出目录，并限制控制器阶段为3至6。调用方仍负责journal、配置观察、Run恢复状态和失败处理；策略不做网络、文件或硬件I/O，匹配失败继续沿原路径拒绝恢复。

新增Qt回归覆盖精确匹配、实例/snapshot变化、session/PID/输出目录变化、非法控制器阶段及receiver缺失。策略与快照身份两个定向用例QtTest **4 passed、0 failed**；当前源码完整Widgets QtTest **144 passed、0 failed、0 skipped**（59.661秒）；生产`apps/envoy/envoy.pro`独立干净构建成功。验证使用合成控制元数据和既有localhost mock，不连接实板、不启动真实receiver或产生DAQ raw；Windows、macOS和现场恢复仍未验收。

## 71.428 Native恢复Run快照身份策略独立化（2026-10-24）

将`NativeRunControl::Recover()`中保存Run与当前部署配置的一致性比较抽为只读`MatchesNativeRecoverySnapshot()`。恢复仅接受完全相同的Native模式、catalog/experiment/Run编号、输出与记录目录、ECC地址/端口/service/client/epoch、snapshot/input-binding摘要、配置观察开关，以及按原顺序对应的节点ID、mutant标志、sender和agent ID；任一差异或非Native配置均拒绝。本策略不判断journal完成态、服务freshness、ECC当前实例/receiver读回或恢复资格，其余运行期门槛仍由Recover执行。

新增回归验证一致快照接受，Run编号、client epoch、input-binding摘要、节点sender/agent变化及Native模式关闭时拒绝。策略专项QtTest 3/3通过，完整Widgets QtTest **143 passed、0 failed、0 skipped**（60.887秒），生产Envoy独立构建成功。mock仅用于已有本机集成fixture，不连接实板、不启动真实receiver或产生DAQ raw。

## 71.427 ScanState资源释放入口集中化（2026-10-24）

将扫描运行期间identity/event-baseline coordinators与stores、runtime snapshot/expectation、operation runner、Run journal store、event monitor及身份响应缓冲的释放收拢至`NativeRunControl::ScanState::ReleaseResources()`。Disconnect与Shutdown共用该入口；Shutdown先写活动Run的Interrupted记录，再释放扫描资源，然后关闭ECC client/store。该方法只释放资源并清除原有disconnect/shutdown时清除的recovery control hold，不重置其余扫描逻辑状态。

Qt 5.12.8/GCC 9.3/C++11完整`tests/tests.pro`重建成功；完整Widgets QtTest **142 passed、0 failed、0 skipped**（59.602秒），生产Envoy独立构建成功。测试用localhost mock/模拟器，不访问实板、真实receiver或DAQ raw。

## 71.426 Native Run完成持久化顺序移入RunRecorder（2026-10-24）

新增`RunRecorder::CompleteNativeRun()`统一执行已通过控制器证据门槛后的Native持久化顺序：原子写receiver/ECC及可选catalog引用、写Run CSV、将Run journal记为Complete，最后才清理显式replacement backup。replacement清理失败仍保留Complete结果并返回单独警告；写引用/CSV/journal失败则返回错误，由控制器沿原失败处理路径保留现场。FinishRun仍负责扫描journal前置门槛、receiver策略校验、活动状态、Run编号和GUI信号。

新增持久化回归验证Complete journal、Run CSV、catalog引用结果；专项RunRecorder QtTest 3/3通过，完整Widgets QtTest **142 passed、0 failed、0 skipped**（59.891秒），生产`apps/envoy/envoy.pro`独立构建成功。mock集成使用localhost，Run持久化测试使用临时目录，无实板、真实receiver或DAQ raw。

## 71.425 Native Run引用文件写入归属RunRecorder（2026-10-24）

将Native Run完成时对`receiver-status.json`、`ecc-identity.json`、可选`ecc-configuration.json`和catalog引用文件的原子写入从`NativeRunControl::FinishRun()`迁入既有`RunRecorder::WriteNativeReferences()`。FinishRun仍在全部receiver/catalog证据校验后调用该持久化入口，并按原顺序完成Run CSV、Complete journal、替换备份清理及Run编号推进。可选配置和catalog规则保持不变。

新增RunRecorder回归核对必需文件内容、配置引用、catalog镜像开启及关闭。Qt 5.12.8/GCC 9.3/C++11定向测试3/3通过，完整Widgets QtTest **141 passed、0 failed、0 skipped**（59.797秒），生产Envoy独立构建成功。验证使用本地临时Run目录，不连接硬件、不生成DAQ raw。

## 71.424 Native接收器封存与归档门槛策略独立化（2026-10-24）

将`NativeRunControl`内重复的接收器最终核验和正常计划退役判断抽为`nativereceiverfinalizationpolicy`只读策略模块。Run完成、扫描receiver-finish/catalog完成以及计划退役前置门槛共用同一校验逻辑：固定活动源覆盖、receiver stopped/reaped/正常退出/EOF seal证据、可选catalog linked-file数量匹配及溢出拒绝。调用方仍决定何时查询/写journal/发控制命令；策略无I/O和副作用。

新增策略回归覆盖缺receiver、receiver完整但catalog缺失、catalog文件数吻合、正常退役控制器阶段及reconciliation拒绝。Qt 5.12.8/GCC 9.3/C++11策略专项3/3通过，完整Widgets QtTest **140 passed、0 failed、0 skipped**（59.870秒），生产`apps/envoy/envoy.pro`独立构建成功。集成fixture只用localhost和合成元数据，不连接硬件、不启动真实receiver或产生DAQ raw。

## 71.423 Native恢复等待状态归组（2026-10-24）

将Native恢复期间等待配置元数据的门控标志从通用`NativeRunControl`字段归入专属`RecoveryState`。连接重置、恢复元数据查询失败、恢复期清理及GUI busy投影仍使用同一标志和原有条件；本次只调整状态所有权，没有改变恢复资格、重试或失败关闭规则。

Qt 5.12.8/GCC 9.3/C++11完整`tests/tests.pro`构建成功；完整Widgets QtTest **139 passed、0 failed、0 skipped**（59.604秒），生产`apps/envoy/envoy.pro`独立重建链接成功。Native扫描集成fixture需localhost监听；受限运行拒绝创建fixture，在允许loopback的环境完整通过。测试不连接实板、不启动真实receiver或生成DAQ raw。

## 71.422 采集控制按钮区分与底部布局修正（2026-10-24）

上方整机控制与下方节点控制保留原有Progress/Regress名称，只把每组内部顺序改为Regress在左、Progress在右；正式采集按钮单独命名为Run start/Run stop，并保持Run start在左。底部参数和操作日志面板改为1:2初始宽度比例，参数面板不再占据大半区域。界面回归同时检查按钮名称、顺序以及分割器比例，避免将控制阶段按钮误标为采集启停。

Linux Qt 5.12.8、GCC 9.3、C++11下完整Widgets QtTest从当前源码重建并通过，**139 passed、0 failed、0 skipped**；生产`apps/envoy/envoy.pro`独立重建链接成功。文档链接检查通过。未在Windows Qt 5.14.2、macOS或现场硬件上验收。

## 71.421 Native恢复入口独立编译单元（2026-10-24）

将`NativeRunControl`的本地Run恢复、失败catalog精确续作，以及Describe/Prepare/采集UNKNOWN的清理与对账入口迁至`apps/envoy/nativeruncontrol_recovery.cpp`，并登记到共享qmake工程。主控制器文件减少246行。保留原有新鲜状态、原operation ID、同一ECC实例/snapshot、receiver会话/PID/目录、告警身份、Run journal和reconciliation门槛；仅调整实现归属，不改变控制顺序或自动重试行为。

Qt 5.12.8/GCC 9.3/C++11下完整`tests/tests.pro`从当前源码重建成功；在允许localhost mock监听的环境运行完整Widgets QtTest：**139 passed、0 failed、0 skipped**（59.790秒）。生产`apps/envoy/envoy.pro`独立重建链接成功。第一次受限运行中本机监听被系统拒绝，不作为回归结论；复跑只访问短时localhost mock与合成控制元数据，无实板、真实receiver或DAQ raw。

这是恢复实现的编译边界拆分，方法仍操作Native控制器共享状态；独立恢复协调器、跨ECC服务重启恢复、Windows Qt 5.14.2、macOS及现场DAQ仍未完成。

## 71.420 Native整机状态到CoBo行模型投影拆分（2026-10-24）

将`NativeRunControl::Status()`内将解码后的整机ECC状态映射为固定CoBo表格行的逻辑移入只读`BuildNativeSystemStatusRows()`。该投影保留ECC连接年龄、控制器state/transition/UNKNOWN、缓存接收器/session/PID/计数和catalog目录；receiver必须映射到配置中的agent，且无整机操作时Run/运行前的receiver集合门槛保持不变。Status回调仍负责响应代际/丢弃处理、缓存状态、identity/operation推进及告警查询。

新增`nativeSystemStatusRowsPreserveReceiverOwnershipAndStateGates`验证在线状态及字节/frame/file计数投影、UNKNOWN和reconciliation行、mutant不应出现Sentry状态、错agent拒绝、Running缺receiver拒绝，以及整机操作进行时允许尚无receiver快照。Qt 5.12.8/GCC 9.3/C++11完整Widgets测试从当前源码重建后 **139 passed、0 failed、0 skipped**（60.026秒），含Native扫描/receiver/catalog收尾集成；生产Envoy独立目标重建成功。仅localhost mock和合成控制元数据，无实板、真实receiver或DAQ raw。

该改动只拆行模型投影，不改变Native状态门控或控制顺序；Windows Qt 5.14.2、macOS、服务重启恢复与现场DAQ仍未验收。

## 71.419 Native状态观察缓存生命周期独立化（2026-10-24）

将receiver、source及receiver failure三类Native观察的协议解码、固定agent集合校验、缓存内容、错误变化判定、freshness计时和重置生命周期收拢到`NativeStatusObservations`。`NativeRunControl`仍处理错误日志、接收失败告警/Run journal副作用和控制流；扫描代码通过缓存访问器读取同一份receiver观察。保持旧行为：观察错误会撤销freshness，receiver错误同时清空原始响应字节，source/failure错误保留最近成功解码的结构值但不会被视作fresh。

新增`nativeStatusObservationCacheOwnsDecodeFreshnessAndReset`覆盖有效接收观察、agent集合不匹配、错误变化去重、错误导致过期、source解析/过期、receiver failure响应去重及整体重置。Qt 5.12.8/GCC 9.3/C++11生产Envoy目标重建成功；完整`tests/tests.pro` Widgets测试重建后 **138 passed、0 failed、0 skipped**（59.789秒），含Native扫描Run收尾集成。只使用localhost mock和合成控制元数据，不连接实板、不启动真实receiver、不产生DAQ raw。此次构建于`/tmp`隔离，验证后已清理。

该改动隔离的是Native观察缓存职责，不改变控制/安全门槛；Windows Qt 5.14.2、macOS、跨服务重启恢复及现场DAQ仍未验收。

## 71.418 Native硬件观察到GUI状态行投影独立化（2026-10-24）

`NativeRunControl::EmitStatus()`现仅整理receiver/source/failure观察的有效期及缓存错误，再调用Envoy独立状态投影函数生成展示行。原安全门槛保持：receiver必须匹配配置的数据agent/CoBo、ECC当前节点的plan/instance、所选PID和session；source需匹配当前controller operation及receiver session；失败证据只有与当前ECC状态、数据节点和保留PID/session均相符时才标注为当前影响。投影纯展示，不发控制命令。

新增`nativeStatusProjectionRequiresMatchingLiveReceiverEvidence`验证新鲜、完整匹配的receiver显示为运行中；PID/session错配不显示运行证据；过期receiver观察显示stale并撤回运行状态。Qt 5.12.8/GCC 9.3/C++11生产Envoy重建及完整`tests/tests.pro`重建成功；新增用例单独3/3通过（含init/cleanup），完整Widgets QtTest **137 passed、0 failed、0 skipped**（59.838秒），既有Native GUI、receiver失败投影及扫描收尾集成均通过。使用localhost mock/元数据，不连接实板、不启动真实receiver或产生DAQ raw。

该项只隔离GUI状态投影逻辑，不提供控制服务重启恢复或硬件读回；Windows Qt 5.14.2、macOS、双机和现场DAQ仍未验收。

## 71.417 Native扫描事件样本绑定逻辑独立化（2026-10-24）

将扫描运行中对receiver事件样本的安全绑定抽成无I/O纯函数：要求请求发送者、目标CoBo、冻结Run identity和数据部署绑定一致；系统状态必须是新鲜且未处于转移、未知或待reconciliation的Running；receiver总览必须新鲜且同属该部署；receiver agent、实例、plan、ready session和目标CoBo进程PID必须一一对应并处于running。Native GUI协调器继续负责异步查询、刷新两类状态、事件数决策及Stop，不把协议校验和控制编排混在一起。

新增`scan-event-count-observation-tests`，成功样例确认绑定身份及事件计数完整保留；拒绝过期系统/receiver观察、错误发送者、非Running状态、系统snapshot错配、receiver数据绑定错配和PID不匹配。独立CMake/C++11目标构建及测试通过。完整Qt Widgets工程由当前源码重建成功；允许localhost测试服务后全套 **136 passed、0 failed、0 skipped**（59.644秒），Native事件目标扫描及receiver/catalog/journal收尾用例通过。mock只交换测试元数据，不连接实板、不启动真实receiver进程或产生DAQ raw。

这是证据绑定和职责拆分，不提供ECC/Supervisor服务重启恢复，也不代表Windows Qt 5.14.2、macOS、双机或现场DAQ验收。受限sandbox中无法创建本机扫描fixture服务，故集成回归在允许localhost的环境重跑。

## 71.416 Native扫描运行状态集中收拢（2026-10-24）

在71.415拆分编译单元后，将扫描专属的journal stores/coordinators、runtime snapshot/expectation、event monitor、点级事件/超时策略、正在观察的生命周期operation和封存阶段状态集中到`NativeRunControl::ScanState`。主控制器以一个私有成员持有此状态；非完整类型的智能指针在扫描实现单元中的out-of-line析构处释放，通用Run/连接字段仍独立保留。该调整不改变扫描操作顺序和持久化字段。

Qt 5.12.8/GCC 9.3/C++11完整`tests/tests.pro`从当前源码重建和链接成功。启用localhost mock服务后完整Widgets QtTest为**136 passed、0 failed、0 skipped**（59.351秒），包含Native事件计数扫描成功收尾、生命周期控制、Run替换、GUI和模拟器回归。mock/模拟测试不连接实板或物理DAQ链路。

此项只把扫描状态按职责归组，NativeRunControl仍负责调度和跨域协调；独立扫描生命周期对象及跨ECC服务重启恢复仍待实现，平台原生与现场验证范围不变。

## 71.415 Native扫描控制实现拆分（2026-10-24）

将`NativeRunControl`中查询扫描运行身份、建立事件基线、启动扫描点、观察生命周期操作、按事件数停止以及确认收尾的实现拆至`apps/envoy/nativeruncontrol_scan.cpp`；扫描工件读取校验随实现一并迁移，qmake共享入口`apps/envoy/core.pri`已登记新源文件。原控制类仍持有连接、定时器、journal和客户端状态，扫描动作接口及运行语义保持不变。

Qt 5.12.8/GCC 9.3/C++11完整`tests/tests.pro`从当前源码重建成功。localhost metadata-only扫描集成用例`nativeScanRunFinishesReceiverAndCatalogAfterEventTarget`通过（3 passed、0 failed、0 skipped，含初始化和清理），覆盖事件目标、Run Stop、receiver收尾、catalog和journal封存。本地mock仅交换元数据，不启动receiver进程、不生成DAQ raw，也不连接实板。首次受限sandbox运行无法绑定loopback；在允许本机loopback的环境以同一二进制复跑通过。

本次仅验证Linux Qt 5.12.8构建，不代表Windows Qt 5.14.2、macOS、现场ECC或真实DAQ验收；NativeRunControl其余状态推进、运行恢复和生命周期责任仍需继续解耦。

## 71.414 新采集客户端保留未知CoBo状态（2026-10-24）

新建`SystemAcquisitionClient`前会对共享CoBo账本执行只读门控：全部活动CoBo必须已确认`Configured`，禁用CoBo必须保持`Disabled`。原先的`BeginAcquisitionSession()`会无条件覆盖活动板状态，现已移除；因此`Unknown`、`Running`、`Ready`等非起始状态不能被创建新对象静默抹平。单测确认门控不改动状态，采集ACK丢失集成场景确认新Run构造被拒绝且原`Unknown`证据不变。

在`build-platform/aget-system-hardware-71-102`重建`hardware-cobo-lifecycle-tests`及`hardware-system-acquisition-tests`成功；生命周期单测通过。localhost Ice模拟采集回归两种编码1.0和1.1通过，含正常运行、暂停/恢复、未知结果拒绝重建、显式失败停机、Standalone CoBo及禁用主触发用例。模拟使用回环TCP，不连接实板、外部数据接收器或DAQ存储。

该门控只保护当前进程内存中的逐CoBo生命周期状态；它没有提供跨进程持久化或硬件读回，复位/恢复仍必须由显式、可核实的恢复路径完成。

## 71.413 Prepare/Run共用固定拓扑CoBo状态账本（2026-10-24）

逐板生命周期状态现在由准备对象创建的一份进程内账本持有，采集对象通过同一账本读取和转移状态。账本固定CoBo ID及配置顺序，校验ID/索引绑定、启用/禁用边界和合法状态边；线程互斥保护状态快照及转移。新采集协调器要求账本处于`configured/disabled`，准备端和采集端的JSON状态查询读取同一个实时值。Breakup在配置失效后仍把状态镜像写入本次Run审计；Breakup-after-failure同样保留该镜像记录。

`hardware-cobo-lifecycle-tests`通过，覆盖固定顺序、初态、有效/非法转移、ID索引错配、禁用板边界和快照不变性。GCC/C++11下重建生命周期模块、采集集成、准备集成、Breakup停止回归及硬件主程序成功。localhost采集回归两种ICE编码通过，并覆盖准备/采集JSON视图同步、Connect/Start/Pause/Resume/Stop/Disconnect、UNKNOWN及明确停机；独立CoBo与禁用主触发模式通过。Breakup配置失效正常和部分失败测试均通过，确认两视图分别反映Prepared和UNKNOWN。文档链接检查1873项通过。所有集成测试使用模拟板和loopback，无实板、真实receiver、DAQ raw或现场验收。

该账本只在当前进程内存中有效；没有持久化或进程重启恢复，也不构成实时硬件读回。

## 71.412 Prepare/Configure与Run共享强类型CoBo生命周期FSM（2026-10-24）

将采集阶段原有的字符串状态和局部转移表并入CoBo生命周期核心；逐板状态统一为强类型枚举，准备及采集协调器共用稳定状态名和合法转移校验。Connect、Start、Pause/Resume、Stop、Disconnect、Breakup及Breakup失败状态同步均通过同一转移入口，不再以字符串比较，Breakup也不再直接覆盖采集状态向量。对外生命周期JSON与既有审计字段保持兼容。

GCC/C++11重建生命周期模块、`hardware-system-acquisition-tests`、`hardware-system-preparation-tests`、`hardware-system-release-tests`、`broken-acquisition-stop-tests`、`native-ecc-coordinator-tests`及硬件主程序成功。14状态全矩阵/状态名/无效枚举测试通过；localhost mock采集流程两种ICE编码通过，覆盖Connect、Start、Pause/Resume、Stop和UNKNOWN；Breakup成功及部分失效状态一致性两项回归通过。无实板、receiver或DAQ raw验收。编译输出仍含其他既有`-Wmisleading-indentation`警告；本次修正了`systemacquisition.cpp`中发现的两处同类警告。

该FSM仍是当前协调器对象内存状态，不是独立逐CoBo持久账本；服务重启后的逐板恢复、旧版全部单节点失败语义迁移和现场验收仍未完成。

## 71.411 CoBo生命周期状态机核心与转移门控（2026-10-24）

将CoBo生命周期枚举、稳定对外状态名、启用/禁用初态和转移矩阵从`SystemPreparationClient`抽为独立硬件模块。每次逐板状态更新现在先校验启用边界及合法转移，再写入原有生命周期审计行；JSON状态名和审计格式不变。矩阵保留当前已实现路径，包括Fresh→Preparing→Prepared、Prepared→Configuring→Configured、板级重准备及取消前的旧阶段恢复、全局配置覆盖、配置失效回Prepared，以及确认释放后的Fresh和不确定结果的Unknown。

新增`hardware-cobo-lifecycle-tests`穷举7×7状态组合、状态名和初态，PASS。Linux GCC/C++11重建CoBo状态模块、`hardware-system-preparation-tests`、`hardware-system-release-tests`和`native-ecc-coordinator-tests`成功；localhost ICE模拟回归中，完整三MuTaNT/双CoBo Prepare+Configure两种编码均通过，跨板告警/Unknown保护及Standalone通过，Release `normal-0`和`fault-2`通过。无实板、receiver或DAQ raw验证。

这一步把现有同一准备对象内的逐板转移规则显式化并拒绝未定义转移，尚未实现逐板FSM独立持久化、服务重启恢复或旧版全部`prepareNode/configureNode`失败语义迁移。其不代表完整Native CoBo状态机已完成。

## 71.407 当前源码Linux full交付构建与程序启动复验（2026-10-24）

从当前工作树重新构建Linux `full` profile：**29/29程序成功**，嵌入依赖检查 **109/109通过**。独立暂存包 verifier 校验 **192个交付文件**；包内启动帮助入口 smoke **29/29通过**，无硬件访问且无raw payload。构建、暂存和smoke均在专用`/tmp/getdaq-current-full-*`目录完成。

这是Ubuntu/WSL Linux用户空间的构建、依赖和安全启动检查，不代表独立Ubuntu 24、Windows Qt 5.14.2、macOS、物理双机、CoBo或真实DAQ吞吐验收，也不替代完整Widgets测试之外的全部原生CTest及现场运行验证。Native Run替换跨ECC/Supervisor服务重启后的授权恢复与逐CoBo持久FSM仍未完成。

## 71.406 当前源码完整Qt Widgets回归复验（2026-10-24）

从当前工作树重新生成并构建完整 `tests/tests.pro`（Qt 5.12.8、GCC 9.3、C++11）成功。完整QtTest在允许本机loopback的环境通过：**136 passed、0 failed、0 skipped**（59.6秒），覆盖配置工作区、legacy/Native生命周期、单点扫描Run→事件阈值Stop→receiver finish/seal/catalog、操作日志独立会话与导出、Run号回退及Run start左右顺序。当前Widgets源码构建有若干既存 `-Wmisleading-indentation` 警告，没有编译错误。

首次在loopback受限sandbox执行时，本机HTTP/ICE服务无法监听并导致网络用例失败；该次不作为代码回归结论。放行同一测试程序后全部通过。测试使用mock服务和临时元数据，不代表CoBo实板、receiver真实数据吞吐、独立Ubuntu24、Windows Qt 5.14.2或macOS验收。临时构建树在验证后删除。文档本地链接检查1865项通过。

## 71.389 Native单CoBo Configure 写入隔离断言使用操作增量（2026-10-24）

重新从当前源码构建 `native-ecc-coordinator-tests` 后，运行 `maintenance-0` 发现原测试断言把禁用C2的历史累计写入绝对值固定为0；在该测试前置流程已有2次写入时误报单板Configure越界。审计记录确认Configure operation `configure-cobo-one`（命令17）已完成，C0写入计数676→1137，另一活动C1保持339，禁用C2历史计数为2。测试改为捕获操作前C2计数并断言操作后不变，同时保留C0必须增加、C1不变的门槛。

在允许localhost Ice/TCP的环境重跑同一 `maintenance-0` 场景通过；父ECC仍为Prepared。测试使用合成硬件与数据生命周期替身，不连接实板、不运行真实receiver、不产生DAQ raw。首次在受限sandbox运行因socket权限失败，未作为测试结果；未修改硬件执行逻辑。

## 71.388 Native逐CoBo运行阶段远端观察与MCP查询（2026-10-24）

`SystemAcquisitionClient`为活动CoBo增加本次运行对象内的逐板状态：Connect后`ready`、启动中`starting/resuming`、成功运行`running`、Pause/Stop分别为`paused/stopped`、Disconnect回`configured`、失败命令结果不确定时`unknown`。硬件观察schema新增固定来源标签`live-controller-cache`和`cobo_lifecycle_is_hardware_readback=false`，严格解码器限制状态集合、板ID唯一性/与CoBo节点一一对应、busy/inconsistent时撤回缓存；GUI增加缓存阶段列。MCP新增只读`ecc_hardware_state`，经既有认证HTTP发起有界GET `/v1/hardware-state`。

Linux GCC 9.4/C++11重建采集、准备、Native协调器目标通过。模拟板TCP测试`hardware-system-acquisition-tcp`与两种ICE编码的`native-ecc-normal-{0,1}`共**3/3通过**（415.31秒）；Native `hardware-observation-{0,1}`两种编码手动回归通过，覆盖初始`fresh/disabled`阶段、来源非硬件读回、busy时不返回缓存以及固定硬件位读取不写硬件。Qt 5.12.8完整测试程序目标重建成功，硬件状态协议/窗口三项定向测试**5/5通过**（含init/cleanup），覆盖生命周期列、认证GET、过期撤回；MCP独立目标重建，`ecc-control-mcp-stdio` **1/1通过**，确认工具只走认证GET。测试期间第一次窗口用例发现新列后采样时间索引未调整，修正为新列索引后通过。所有测试使用localhost模拟硬件，不启动真实接收器、不生成DAQ raw。

本状态只在当前控制器进程/采集对象内缓存；服务重启后不恢复，也不构成硬件读回、独立持久CoBo FSM或旧版全部失败恢复语义。Linux Qt 5.12.8验证不代表用户提供的Windows Qt 5.14.2、macOS、实体板卡、跨主机部署或现场DAQ验收。Loopback测试需要在允许本地TCP的环境运行。临时构建树和qmake产物验证后清理。

## 71.387 Hardware准备协调器逐CoBo阶段状态回归（2026-10-24）

Linux GCC/C++11下从当前源码配置并构建 `hardware-system-preparation-tests` 成功。`hardware-system-preparation-tcp` 注册集成测试通过 **1/1**（225.07秒），覆盖整机Prepare/Configure、配置前校验失败时保留Prepared板状态，以及同步脉冲结果未知时将已进入Configure阶段的板标为unknown。随后选中CoBo maintenance用例通过，两种ICE编码各3个场景，共 **6 cases**；新增断言确认单板Configure成功后重复调用被拒绝，写入计数不变，缓存状态保持Configured。测试仅使用localhost与合成硬件服务，没有真实板、receiver或DAQ raw；alignment的物理效果未验证。

这只验证 `SystemPreparationClient`存活期间的逐板命令阶段跟踪与审计投影，不证明ECC重启恢复、独立逐板持久FSM、板端实时读回、Run/Stop逐板状态或现场硬件。临时构建目录位于 `/tmp/getdaq-refactor-build`，验证后清除。

## 71.386 Native逐CoBo命令转移门控回归（2026-10-24）

重建 `ecc-network-service-tests` 与 ECC server；测试夹具及ECC服务使用同一当前源码构建。`cobo-maintenance-window-0` 在允许localhost loopback的环境运行 **1/1通过**（124.246秒）。本机ECC HTTP服务、两个Supervisor、receiver和生产GUI维护窗口完成既有全流程；在ECC HTTP已验证单板 Configure 后，以新operation ID再次Configure同一CoBo，服务返回HTTP 412，断言确认没有新增ECC network journal记录或硬件写入。最终维护证据校验器通过，并确认结构化结果`configured_board_repeat_rejected=true`、`duplicate_configure_no_mutation=true`。

runner清除4个合成`.graw`、共65,536字节，剩余0；这是localhost假硬件测试，不是实板、真实跨机网络或持续DAQ吞吐验证。该改动把journal投影状态用于命令门控，但仍不是独立CoBo状态账本、板端读回或完整旧版故障恢复FSM。完整范围与限制见[重构进度71.386](REFACTOR_STATUS.md#71386-native逐cobo生命周期状态开始参与命令门控)。

## 71.385 Native逐CoBo生命周期状态投影及GUI网络回归（2026-10-24）

ECC维护目标元数据升级到schema 2：每个固定活动CoBo带有`lifecycle_state`、最后操作ID/命令和操作阶段。状态由当前ECC系统状态及持久操作journal投影；单板重准备、单板Configure只更新目标CoBo，LED/Pulser命令保留该板生命周期并更新最后操作证据，全局生命周期命令按系统结果更新整组目标。Native主状态表显示逐板生命周期，维护窗口显示选中板的状态和最后操作；完成后显式重新读取目标元数据。连接/服务重启后尚不能从硬件恢复可信逐板状态，状态保守显示`UNKNOWN`，该投影不是板端实时读回，也不是独立的单CoBo FSM迁移。

Qt 5.12.8/C++11 `getdaq-tests`完整目标和生产维护窗口探针构建成功；完整Qt Widgets套件 **135/135通过**（含init/cleanup），维护窗口状态/刷新断言包含在内。`ecc-protocol-tests`通过schema 2编解码与非法状态/阶段拒绝。真实ECC HTTP服务、两个Native数据Supervisor、接收器和生产主窗口探针运行`cobo-maintenance-window-0` **1/1通过**（约120秒）：覆盖Cancel/Yes、单板重准备和后续LED/Pulser命令、busy exclusion、原operation ID终态观察及每CoBo最近操作投影；selected CoBo仍为Prepared，另一块CoBo未被命令且状态保持Prepared。测试生成4个合成raw共65,536字节，清理4/4，剩余0；只使用localhost模拟板，不连接实体硬件或外部主机。Markdown本地链接检查1843项通过。

这次Linux/WSL Qt 5.12.8验证不等同Windows Qt 5.14.2、macOS或现场验收。该改动只是journal派生的保守生命周期视图；旧系统`prepareNode/configureNode`的逐板中间状态、失败/恢复语义和多板协调仍待迁移，不能据此宣称独立Native CoBo FSM完成。

## 71.384 Envoy连接日志回归走真实模拟连接按钮路径（2026-10-24）

Qt 5.12.8/C++11完整Widgets测试目标从当前源码重建成功。`eachConnectionGetsAnIndependentPersistentOperationLog`现在通过主窗口Connect/Disconnect按钮建立两次模拟会话，不再直接伪造connected状态；它验证日志落入用户数据目录`logs/connections/`、包含连接请求和断开标记、每次会话文件唯一且内容不串写。该用例与Run编号/按钮位置回归定向运行，**4/4通过**（含init/cleanup）。模拟模式不访问ECC、CoBo、真实网络或DAQ；这加强了GUI连接生命周期证据，不替代硬件连接验收。

## 71.383 单板 Configure 持久ECC/HTTP/GUI维护网络回归（2026-10-24）

重建 `ecc-network-service-tests` 和 Qt 5.12.8/C++11 生产维护窗口探针后，运行 `cobo-maintenance-window-0` 本机集成 **1/1通过**，耗时122秒。场景启动真实ECC控制服务、两个数据Supervisor和Native接收器，并经生产MainWindow/RunControl维护窗口执行Cancel/Yes、重准备、LED、Pulser、blink、flip和周期LED操作；另通过ECC HTTP提交单板 Configure，等待原operation ID进入终态后核对请求完全一致、硬件审计只写所选C0、其他CoBo计数不变且整机保持Prepared。此前测试曾在HTTP 202后立即读取异步operation，误把未终态当成失败；改为轮询同一operation至complete/failed/unknown终态后通过。

测试生成4个合成`.graw`共65,536字节，runner删除4/4并报告 `raw_remaining=0`；使用localhost模拟CoBo，不连接实板或外部主机，不代表现场硬件/吞吐验收。独立UI门控和每连接日志QtTest共4项（含init/cleanup）通过；文档链接检查1840项通过。完整Qt套件135/135和MCP stdio/确认/只读模式另见71.379及对应MCP回归；Windows Qt 5.14.2、macOS仍需原生验证。临时联调目录在复核cleanup.json后清理。

## 71.382 单板 Configure 硬件层隔离回归（2026-10-24）

严格C++11目标 `hardware-system-preparation-tests` 在Qt/ICE兼容构建隔离目录成功构建。用本机loopback模拟硬件运行selected-CoBo maintenance/preparation场景，两种ICE编码均通过；单板Configure只写选中板，数据未停止/停用目标在任何写入前拒绝，其他CoBo及MuTaNT不变、`initDone`保持0、生产停止，父对象仍可显式进入整机Configure。跨层网络和GUI维护窗口回归见[71.383](#71383-单板-configure-持久ecchttpgui维护网络回归2026-10-24)。此处模拟测试不连接实板，不能代替现场验收。

## 71.381 Native catalog失败续作重建父管理器上下文（2026-10-24）

新增一次性`ResumeColdCatalogReplacement()`：重新校验原Publish审计、固定controller/data绑定、计划和配置工件身份，并核对每个数据Supervisor中原operation、Run/session、receivers退出与sealed catalog证据；通过后沿用原replacement身份和已有Observe/Publish/Finalize路径续作。调用方必须在每次观察或修改前提供硬件仍处于健康Prepared且独立控制hold仍成立的边界检查。该帮助函数尚未接入ECC service，也不返回可长期控制的manager。

同机loopback回归先销毁ECC父`RemoteReceiverManager`，保留两个数据Supervisor service进程，再分别验证Publish失败和Finalize失败后的单次续作；Publish失败恢复会完成原Publish/Finalize，Finalize失败恢复只补Finalize、不重复Publish。两项定向CTest **2/2通过**，fixture仅有小型合成raw（每帧8 KiB），均小于20 GiB；未访问真实硬件。验证只证明父manager可在数据Supervisor实例持续存活时重建一次性续作上下文。数据Supervisor重启会更换service instance并丢失当前catalog-recovery授权状态，因此会失败关闭；ECC service/MCP/GUI集成后的完整cold recovery、进程崩溃窗口和物理双机仍未验证。

## 71.380 Native catalog审计保留原始逐节点Publish请求（2026-10-24）

`RemoteCatalogPublishEvidence`在完成固定节点、operation ID、request key、controller scope/epoch和replacement ID校验后，将解码的完整`SupervisorControlRequest`随每节点证据保留。冷恢复层可据此核对原session、远端实例、计划和原operation，而不必从摘要重造请求；该证据对象仍只读，不发网络、不恢复owner、不授权重试。

Linux GCC 9.4/C++11完整重建`getdaq-ecc-native`及`remote-handoff-evidence-tests`成功；冷审计证据CTest **1/1通过**，25个元数据场景通过，含中断和成功Publish的每节点原请求精确序列化对照及request-key复核。测试无网络、硬件或raw数据。此改动只为后续重启恢复提供已校验的请求材料；ECC父级冷恢复、硬件重核后续作仍未实现。专用构建位于`/tmp/getdaq-catalog-publish-request-build`，本轮清理后再结束本节。

## 71.379 完整Widgets套件在允许loopback的环境通过（2026-10-24）

针对71.378之后的当前源码，重新构建完整`tests/tests.pro`目标，并在允许本机loopback监听的环境运行：**135 passed、0 failed、0 skipped**，耗时约59秒。首次受限运行中，依赖本机TCP监听的69项测试失败并在Native scan fixture处终止；获准允许loopback后，HTTP排序、单CoBo状态机、Native扫描、每次连接独立操作日志、Run编号回退及两组Run start/Run stop按钮顺序等回归全部通过。此前全量Linux full profile的29程序构建、192文件交付包和29/29启动smoke证据仍见71.378。

套件包含受硬预算保护的模拟DAQ数据测试；本次没有运行物理数据传输、连接DAQ硬件或产生大文件。验证使用Qt 5.12.8/GCC 9.3/C++11和WSL用户空间，不代表Windows Qt 5.14.2、macOS、独立Ubuntu部署、物理双机或现场DAQ验收。临时构建与运行时目录在测试后清理。

## 71.378 当前源码full交付包及Envoy连接日志/UI回归（2026-10-24）

从当前工作树在Ubuntu 20.04用户空间/WSL2共享内核中构建Linux `full` profile：receiver、native、repository、control和MCP共 **29/29程序**成功，嵌入依赖清单 **109项**通过。暂存为新目录后，package verifier确认 **192个交付文件**完整；逐个运行程序帮助入口的package launch smoke **29/29通过**，没有连接硬件或启动DAQ接收。

同轮按Qt 5.12.8/GCC 9.3/C++11从当前源码重建完整 `tests/tests.pro` Widgets测试程序；构建回执绑定 **131个源文件、305项输入**。定向执行每次Connect独立日志、Run编号回退及Run start/Run stop左右顺序和Connect后编辑、GUI生命周期用例：连同QtTest init/cleanup **5 passed、0 failed、0 skipped**。另外本地Markdown链接检查 **1836项通过**。这些用例不生成DAQ raw。

本轮构建、暂存包、smoke证据与Qt测试目录均位于专用`/tmp/getdaq-goal-*`并在验证后删除；raw检查为 **0个、0字节**。环境只证明WSL中的Linux用户空间构建/启动，不是独立Ubuntu24、Windows或macOS部署，也不证明物理双机、CoBo或现场DAQ吞吐。尝试从WSL调用用户提供的Windows Qt 5.14.2 qmake时，版本查询在Windows进程启动阶段报`UtilBindVsockAnyPort: socket failed 1`，因此本轮没有Windows构建结论。Native Run替换ECC父级服务重启后的跨节点状态重建和安全续作仍不支持。

## 71.377 Native Run替换父级Publish审计只读解析强化（2026-10-23）

新增`RemoteCatalogPublishEvidence`，以只读、有限元数据方式检查父controller快照和scope、权限/文件身份、Publish intent中的固定节点顺序、原始Supervisor请求、节点级replacement ID及JSONL投递顺序。拒绝重排/重复节点、错误operation/request key、失败后继续POST、伪造`post_sent=false`、不匹配的409/412拒绝、缺少接受回执或catalog receipt的完成标记、未终结/截断日志和内容身份漂移。证据对象只分类原始node outcome，不恢复管理器状态、不发网络请求、不授权重试。

`ResumeCatalogReplacement()`现先解析该持久审计，再将原epoch、Experiment/Run、intent摘要、节点实例与plan、配置/工件摘要和当前固定远端绑定逐项比对，然后才开始按原operation ID进行只读远端核验。解析器本身不实现cold recovery；当前显式恢复仍需原ECC及RemoteReceiverManager保持存活、原验证cycle和状态门槛成立。

Qt-free Linux/GCC 9.4/C++11目标构建成功。`remote-cold-audit-evidence` CTest **1/1通过**（25项元数据场景）；本机两个数据Supervisor + ECC控制管理器的`catalog-publish-failure`、`catalog-finalize-failure` loopback恢复测试 **2/2通过**。测试只生成小型合成raw，严格小于20 GiB，所有测试raw与临时构建树已删除并复核无残留；不代表物理双机、ECC/receiver进程重启后的cold recovery、现场硬件或DAQ吞吐验收。

## 71.374 Native catalog替换重启观察增加原生查询CLI（2026-10-23）

`getdaq-supervisor-query catalog-replacement-observation --operation-id ...`现在可经认证HTTP client读取数据Supervisor持久化的replacement Publish/Finalize操作及其只读文件系统观察。CLI先核对当前服务身份，再将观察响应与历史operation的request key、原实例、控制器snapshot/epoch、plan和当前实例绑定；要求观察是指定replacement ID，且不读取raw payload、不授权恢复。集成覆盖Prepare ID拒绝、无认证拒绝、Publish执行后CLI查询，以及Supervisor停止/重启后再查询同一原Publish ID。

Qt 5.12.8/GCC 9.4/C++11下相关Supervisor、CLI、catalog及测试目标构建成功；真实本机service/receiver loopback `catalog-restart`通过，8项HTTP传输故障用例、codec与credential检查通过。`catalog-replacement-file-set`与`receiver-catalog-filesystem` CTest **2/2通过**。合成raw为12个、294,912字节，runner清理回执为12/12删除、`raw_remaining=0`；未访问硬件或DAQ现场。此CLI仅供数据节点侧取证，ECC父端仍未把重启后的多节点证据重建到Run审计，也没有因此获得cold recovery能力。随后修正文档示例的Bash续行符，抽取命令通过`bash -n`；本地文档链接检查1835项通过。临时构建/测试目录位于`/tmp/getdaq-catalog-query-build`，测试结束后清理并复核。

## 71.373 Native catalog替换增加认证只读HTTP观察并覆盖数据服务重启

数据主机Supervisor新增认证GET `/v1/catalog-replacement-observations/{publish-operation-id}`，只接受其持久操作记录中原始的replacement Publish/Finalize operation。响应绑定当前服务实例、原请求实例、agent、plan、controller身份、request key及稳定replacement ID，并调用catalog filesystem只读观察器核对当前target、stage、candidate和事务journal。观察器不会读取raw payload（`raw_payload_bytes_read=0`），也不授权修复、重命名、删除或恢复（`recovery_authorized=false`）；非Publish operation返回412、未知operation返回404、锁/操作忙返回409。原生HTTP client增加对应单次GET入口。

使用Qt 5.12.8/GCC 9.4/C++11重建Supervisor service、原生query client、catalog CLI和相关测试目标成功。真实HTTP service + receiver合成集成场景`catalog-restart`通过：覆盖原Publish观察、拒绝Prepare operation观察、无Bearer认证返回401，以及停止并重启数据服务后用原Publish operation ID重新观察已发布catalog；同时运行HTTP传输fault cases 8项、codec与credential测试。`catalog-replacement-file-set`和`receiver-catalog-filesystem` CTest **2/2通过**。fixture检查12个合成raw共294,912字节（20 GiB硬上限），测试后12个文件均删除，`raw_remaining=0`。验证未访问硬件或DAQ现场，也没有在ECC父协调器中实现cold recovery；该API只提供重启后证据，不能单独授权自动续作。临时build/evidence位于`/tmp/getdaq-catalog-observation-build`，本轮完成后删除。

## 71.372 Native Run替换只读检查中断阶段（2026-10-23）

新增`InspectReceiverCatalogReplacement()`及`getdaq-catalog --inspect-catalog-replacement OLD NEW CANDIDATE OPERATION_ID`，只读核对replacement请求、operation journal/marker与catalog receipt。返回的`raw_payload_bytes_read`为`0`且`recovery_authorized`为`false`；CLI回归会解析输出JSON并核对kind、operation、finalized阶段和这两个只读属性。它不修复journal、不重命名或删除文件，也不允许服务重启后的cold recovery。filesystem回归覆盖prepared、旧catalog rename到backup后marker尚未落盘、candidate rename到target后published marker尚未落盘、published和finalized状态，并验证同一operation现有Publish逻辑可续作两个rename中断夹具；事务锁被占用时观察必须失败关闭且不得改变目录。严格C++11重建`getdaq-catalog`和`receiver-catalog-tests`成功，`receiver-catalog-filesystem` CTest **1/1通过**。测试raw使用受管临时目录并在测试后清理；未访问硬件、未做多节点/ECC/MCP/GUI端到端或物理数据主机验证。该观察器只是后续冷恢复审计的证据读取基础，不能据此宣称跨服务重启恢复已实现。

## 71.371 当前源码Linux full构建、29程序启动与完整QtTest（2026-10-23）

在Ubuntu 20.04.6/WSL2、GCC 9.4.0、CMake 3.16.3、Qt 5.12.8上从当前源码干净构建`full` profile：29/29程序成功，receipt绑定556项构建输入，嵌入依赖检查109项通过。将29程序暂存到独立候选后，包内启动smoke 29/29通过；GetDAQ离屏截图成功、其余程序按帮助入口启动，receipt为`hardware_accessed=false`、`raw_files_remaining=0`、私有临时目录已清理。

另用同一工作树的`tests/tests.pro`重建完整Qt Widgets测试程序（131个源文件、305个跟踪输入），运行得到 **134通过、0失败、0跳过**。模拟器数据测试设置128 KiB硬上限；测试使用临时目录，进程结束后已核实临时根目录不存在。QtTest日志、构建receipt及包smoke receipt保存在`build-receiver/linux-host-current-71-371/`（约164 KiB）；273 MiB full构建树、92 MiB候选包和26 MiB Qt测试构建树随后删除，无大型交付副本保留。

这里的“Linux full”是WSL2当前Ubuntu主机用户空间/共享Linux内核；包内smoke没有使用Ubuntu24 rootfs。因此它不是独立Ubuntu24部署、macOS或Windows构建证据，也不代表真实CoBo、物理双机、真实实验文件差分或连续DAQ吞吐验收。Windows Qt 5.14.2单独原生QtTest结果见[验证71.370](#71370-windows原生qttest修复sentry-run替换文件句柄边界2026-10-23)。整体重构仍未完成。

## 71.370 Windows原生QtTest修复Sentry Run替换文件句柄边界（2026-10-23）

用户提供的Windows Qt 5.14.2/MinGW 7.3 x86_64环境中，完整Widgets QtTest **133通过、0失败、1项Unix专属锁测试跳过**，ECC客户端QtTest **69通过、0失败、0跳过**。此前Windows回归暴露legacy Sentry同名Run替换Finalize在合并/重写`catalog.json`时返回500：Windows不允许`QSaveFile`原子替换仍有打开读句柄的目标文件，也不允许删除仍持有打开manifest句柄的旧Run目录。修复后Sentry在每次读取后及时关闭当前及备份manifest；QtTest也在Finalize/幂等复查前关闭自己打开的manifest。现有`QSaveFile`原子写、同名raw由新Run胜出、不同名旧raw保留的事务行为不变。

Windows原生运行器的最终清理回执为`temporary_root_removed=true`、`raw_files_removed=0`、`raw_bytes_removed=0`，raw硬上限20GiB未触发。两个QtTest构建目录（各约110MiB）已删除；保留当前36MiB Windows控制程序构建。此结果是Windows Qt 5.14.2原生QtTest，不代表独立Windows主机、真实CoBo/数据主机、现场Run替换或持续DAQ吞吐验收。证据：`apps/sentry/sentryservice.cpp`、`tests/test_getdaq.cpp`、`tests/run_windows_qt_validation.py`。

## 71.369 MCP清晰报告ECC并发busy拒绝（2026-10-23）

MCP 的 HTTP 客户端现会在 HTTP 409 时解析有界JSON错误响应，并将服务端`error`文本作为MCP工具的`isError=true`内容返回；其他HTTP错误仍保留原网络错误说明。这样同一ECC owner下的并发提交会明确显示`ECC submission busy; not queued`，调用方知道服务没有排队或执行该命令，不会误以为稍后会自动发生。

从 `apps/ecc/mcp` 严格 C++11 构建 `getdaq-control-mcp` 成功。增强后的 `ecc-control-mcp-stdio` 回归 **1/1通过**：stdio调用控制工具、假ECC返回HTTP 409 busy/not queued，MCP错误内容保留状态和服务端原因，服务操作列表没有增加新operation。测试只开本机loopback假服务、无硬件访问、无DAQ raw；尚未对真实MCP host或真实ECC服务做并发端到端验收。

## 71.368 GUI与MCP共享owner时服务端拒绝并发控制命令（2026-10-23）

GUI和MCP属于两个独立客户端，但可以使用相同ECC固定`client_id`与现有`client_epoch`。新增ECC network service回归，在Prepare请求已被接纳且operation仍为`running`时，用相同owner identity、不同operation ID提交第二个Describe请求。ECC服务拒绝该请求，返回HTTP **409**（busy/not queued），并且硬件fake write计数没有增加；它不会缓存等待第二条命令，也没有发送额外板端动作。已有断言同时确认相同operation ID与相同request key会返回原操作、不重放硬件命令，另一个epoch不能借机成为新owner。

当前源码重新构建`ecc-network-service-tests`成功；`ctest -R '^ecc-network-service-normal-0$' --output-on-failure`在本机loopback运行通过，耗时 **88.74秒**。此为ECC HTTP服务和fake硬件控制器测试，不是GUI和真实MCP host同时连接物理ECC的验收。实际使用约束是“两个入口可同时连接、共享同一owner；一次只接受一个新命令”。另一入口收到busy后不会自动排队或重试，操作员应等待当前操作完成并刷新状态，再明确提交后续动作。不同epoch的入口会被拒绝而不是静默接管。

## 71.367 Native Run替换恢复接入GUI并通过完整Qt回归（2026-10-23）

Envoy主窗口现提供独立的失败catalog replacement恢复按钮和禁用原因提示。按钮只在原Native ECC仍在线、Run journal精确绑定同一个未完成replacement、原失败catalog operation仍可由本机ECC client store验证、Run仍active、状态新鲜且控制器在健康Prepared状态时启用；任何本机控制故障或其他待处理操作均禁止提交。操作员确认对话框显示原失败操作ID和新恢复操作ID，并说明这是一次性恢复以及不会启动新Run或删除不同名旧raw。提交仍由ECC服务端再次执行最终门槛校验；成功响应后GUI查询新状态并核验数据主机catalog receipt，确认后才完成本地Run记录和递增Run号。

新增`failedNativeReplacementCanBeExplicitlyResumedThroughRunControl`本机假ECC集成回归，先让已确认Run的替换catalog明确失败，核验恢复状态信号只携带原失败operation ID并允许精确恢复，再以全新恢复operation ID绑定原operation；恢复成功后重新读取catalog状态、完成Run，并写下恢复完成journal。该场景通过。完整`native-gui-tests` QtTest **27/27通过**，包括原有Native lifecycle、Run完成门槛、日志恢复和新增replacement恢复流程。另从当前源码构建`apps/envoy/envoy.pro`成功，确认Qt Designer表单、主窗口信号槽和Native控制层在Linux Qt 5.12.8/GCC 9.3/C++11下能共同链接。GUI状态机集成测试的本机假ECC loopback只产生小型Run journal/配置文件，没有生成raw。

验证同时修正了恢复请求原先无法被本地客户端提交的问题：`CanSubmit`对历史recovery命令要求已锁存本地控制故障，但失败catalog replacement要求控制器保持健康Prepared。catalog恢复现独立要求新鲜、匹配客户端身份、无本地控制故障、无正在运行operation且Prepared状态健康；数据主机reconciliation由已绑定失败operation和ECC服务端最终门槛决定，不把GUI本地门槛降为无条件提交。

本轮约36 MiB临时qmake测试/app构建树已删除，检查这两个目录未发现`.graw`、`.raw`或`.gmat`。没有真实DAQ、ECC硬件、真实MCP host或物理双机验收；Windows Qt 5.14.2、macOS、独立Ubuntu24及持续高吞吐也未由此验证。此次界面回归验证RunControl和Network mock，不自动点击需操作员确认的真实GUI弹框。整套重构仍未完成。

## 71.366 Native Run替换Finalize恢复与父审计JSONL修复（2026-10-23）

跨节点Run替换现在在所有固定节点Publish回执确认后执行Finalize：每个数据主机只清除旧catalog/旧session中与新归档同一source、完整raw文件名精确相同的旧链接，保留不同名旧raw。若Finalize失败，父协调停止后续节点并保留reconciliation；显式恢复会检查原Publish结果，在所有节点已Publish时跳过重复Publish，再按原replacement身份续作Finalize。进程重启后的cold recovery仍不支持。

本轮修复了父级`remote.jsonl`把带末尾换行的catalog receipt和Supervisor status直接嵌入对象，导致一条记录跨行、之后恢复解析失败的问题。现在嵌入前必须检查编码器输出只有对象加一条行终止符，并严格解析对象。GCC 9.4/C++11相关目标重建成功；`receiver-catalog-filesystem`、`supervisor-control-codec`及Native ECC双节点正常替换、Prepare失败、Publish失败和Finalize失败后恢复六项CTest全部通过（6/6）。Finalize失败恢复用例验证不重复Publish，并最终逐节点Finalize。

测试仅创建微型合成`.graw`（每个fixture文件8192字节），检查合计约1.4MiB，远低于20GiB。执行后删除本轮新建的10个`/tmp/getdaq-native-remote-*` fixture根，并确认这些根中没有剩余raw；没有接触真实DAQ、板卡或物理双机。MCP代码位置为`apps/ecc/mcp/`，其中`main.cpp`实现本机stdio适配器，网络服务仍由独立`getdaq-ecc-server`提供；本轮未做MCP host到真实ECC的正向端到端或GUI与MCP同时发命令的验收。见[Native Run替换规范](NATIVE_RUN_REPLACEMENT.md)及[ECC控制MCP手册](ECC_CONTROL_MCP.md)。整体重构仍未完成。

## 71.364 Native Run替换正向ECC服务恢复集成回归（2026-10-23）

`qt-catalog-recovery`本机loopback场景现已从真实ECC服务接收替换Publish失败，经显式`resume-catalog-replacement`请求完成同一replacement的受限续作，并逐个独立重开两个数据节点catalog复核receipt。结果文件确认原失败已观察、同一替换已恢复、所有节点receipt有效且control/data reconciliation门槛均清除。测试服务使用两个本机Supervisor/receiver进程，测试通过，场景exit code **0**，耗时109.062秒。

此场景生成12个合成raw，共147,456字节（远低于20GiB）；runner已删除12个、剩余0个、残留字节0、清理错误0。证据位于本机临时运行目录`/tmp/getdaq-ecc-catalog-recovery-positive/ecc-network-system-o_le510t/`；关键回执为`qt-catalog-recovery/catalog-recovery-service-result.json`，runner清理账本为`cleanup.json`。该测试证明本机ECC服务HTTP恢复和两个逻辑数据节点的正向闭环；MCP自身仍以假HTTP集成测试验证参数转发，尚无MCP stdio host到真实ECC服务的完整正向调用验收。没有真实ECC/CoBo、物理双机、Windows Qt 5.14.2、macOS、独立Ubuntu24或持续DAQ吞吐验收。

同轮补跑关联回归后，ECC协议、catalog filesystem、Supervisor control service/process lifecycle、Native receiver正常替换/Prepare失败/Publish失败停止测试共 **7/7通过**；先前执行中的测试二进制曾落后于源码，重建后复跑结果全绿。Markdown本地链接检查 **1820项通过**。这些CTest夹具共生成126个合成raw、3,162,122字节，均远低于20GiB；通过后清除了两个生成型Supervisor evidence目录，并确认本轮构建/集成测试树内剩余raw为 **0个/0字节**。正向ECC集成runner自身也报告剩余raw为0。

整体Run替换仍不是跨主机原子事务，进程重启后的cold recovery不支持；旧session/catalog backup内被同名替换的旧raw尚未物理回收。详见[Native Run替换规范](NATIVE_RUN_REPLACEMENT.md)与[MCP手册](ECC_CONTROL_MCP.md)。整套重构未完成。

## 71.363 Native Run替换恢复接入ECC协议与MCP（2026-10-23）

新增ECC控制请求`resume-catalog-replacement`，要求新恢复operation ID绑定原失败replacement operation ID。服务端只允许原live ECC进程保留Publish proof、原操作为已失败替换catalog、ECC实例/配置快照/client/epoch完全匹配、硬件仍是健康Prepared且无其他独立hold时接纳；重启后不能cold recover。`getdaq-control-mcp`新增`ecc_resume_catalog_replacement`，只在显式`allow_control=true`时显示；它以原失败ID、新恢复ID和两者绑定确认字符串预检身份/操作/状态，再调用ECC权威门槛。确认字符串不等于MCP host已向人取得批准。

严格C++11重建ECC协议测试、ECC network service、ECC server和MCP目标成功；ECC协议CTest **1/1通过**，`tests/ecc_mcp_test.py`假HTTP集成通过，覆盖工具显示/只读限制、确认失败不发HTTP、有效恢复请求绑定原失败ID/新ID和client epoch。真实本机ECC loopback沿用`qt-catalog-failure`场景，尝试将普通非替换catalog operation作为恢复目标，服务返回 **HTTP 409**，确认没有新增远端catalog调用。该用例验证拒绝门槛，不验证正向ECC服务恢复闭环。`results.json`记录场景exit code **0**；合成raw共3个、49,152字节，runner删除3个、剩余0、无清理错误。无真实硬件或物理双机。

该节原记录的正向服务回归缺口已由[验证71.364](VALIDATION.md#71364-native-run替换正向ecc服务恢复集成回归2026-10-23)补上；GUI恢复按钮已由[验证71.367](VALIDATION.md#71367-native-run替换恢复接入gui并通过完整qt回归2026-10-23)补齐。MCP stdio host到真实ECC服务的正向端到端路径、Windows Qt 5.14.2、macOS、独立Ubuntu24、CoBo和连续DAQ吞吐仍未覆盖。Native旧catalog backup/session内的同名旧raw也尚未物理回收，整体重构未完成。

## 71.362 Native跨节点Publish失败后的显式恢复（2026-10-23）

为71.361的首失败停止策略补上了有边界的续作入口。`ResumeCatalogReplacement()`先校验同一ECC进程内保留的原Publish intent、固定节点列表、实验/Run/session、controller绑定和稳定replacement ID，再只读查询每个已提交节点的原operation ID。已提交节点必须观察到`complete`或`failed`终态；`node_not_attempted`节点必须确认operation不存在。证据不完整、仍为unknown、operation意外缺失或身份变化都会停止，不发恢复请求。证据齐全后写独立Observe/Resume审计，沿用原replacement ID但生成新的子operation ID；各节点只允许同一live DataSupervisor内存中记录的精确Publish失败恢复一次，并要求原receiver已退出、seal有效、session与存储身份不变。成功后逐节点重验正式catalog receipt。

严格C++11重建`getdaq-ecc-native`、`getdaq-supervisor`、`native-ecc-remote-receiver-tests`、`supervisor-control-tests`、`data-supervisor-tests`、`getdaq-supervisor-query`及`getdaq-catalog`成功。Selected CTest最终 **18/18通过**：Supervisor codec/control-service/process-lifecycle各1项，Native ECC remote receiver全15项；其中新增Publish失败恢复fixture验证首节点原operation终态失败、后续节点原operation不存在后，受控续作同一replacement ID并验证全部正式receipt。首次control-service启动因该临时构建树缺少query/catalog辅助程序未运行；补建后完整重跑通过。恢复测试及其他loopback均为同机多个独立Supervisor/receiver服务，不是物理双机或CoBo验收。

全程生成的`.graw/.raw/.gmat`共227个路径、合计3,207,173字节，最大49,152字节，远低于20GiB；所有本轮loopback临时raw目录和`/tmp/getdaq-catalog-resume-build`已验证后删除。此恢复证明依赖ECC与各数据主机Supervisor进程未重启，恢复资格只在内存中；不支持cold recovery，远端状态unknown时不重发。旧catalog backup与旧session中被同名新raw取代的物理载荷仍未清理；本轮未验证Windows Qt 5.14.2、macOS、独立Ubuntu24、真实ECC/CoBo或持续DAQ吞吐。

## 71.361 Native跨节点Publish首失败停止后续提交（2026-10-23）

`RemoteReceiverManager`的替换Publish批次现在在第一个节点命令失败或结果不确定时立即停止，不再向后续节点发送Publish；父审计把剩余节点记录为`node_not_attempted`、`post_sent:false`并保持需要对账状态。新增显式Prepare/Publish方法，使两个逻辑数据节点的测试可以在Prepare完成后注入单节点发布故障。正常替换、Prepare屏障失败和Publish首失败停止三个测试 **3/3通过**。严格C++11 Native ECC/Supervisor构建成功。

测试使用本机loopback服务和合成raw，不是物理双机或CoBo验收。测试产生33个`.graw`，合计245,760字节，单个最大8,192字节；全部测试raw目录和`/tmp/getdaq-catalog-publish-failure`临时构建目录已删除并核实不存在。当前没有Publish部分成功后的父级Observe/Resume恢复，也没有从旧session/catalog backup物理删除精确同名旧raw；失败状态仍须人工对账。未验证独立Ubuntu24、Windows Qt 5.14.2、macOS、真实ECC/CoBo或持续DAQ吞吐。

## 71.360 当前源码full profile全量构建及29程序启动smoke（2026-10-23）

从当前源码以Linux `full` profile重新构建 **29/29程序**，构建receipt记录Ubuntu 20.04用户空间、Qt 5.12.8、109项嵌入依赖检查通过；这是WSL2共享Linux内核环境。候选包共192个文件，`tools/build-suite.py --verify-stage`通过。启动回归逐个校验包内程序摘要并执行安全入口：28个CLI/服务调用`--help`、GetDAQ GUI以offscreen模式只截取初始窗口，最终 **29/29退出码为0**，无硬件访问、无raw文件残留。

此smoke第一次发现`getdaq-ice-check --help`被当成无效子命令而退出2。已在`apps/icecheck/main.cpp`增加`-h/--help`成功路径，并将完整候选包smoke中的该程序改为常规帮助检查；新增CTest `ice-check-help`，C++11编译及CTest **1/1通过**。同一profile重新构建并打包、29程序smoke全数通过。帮助行为失败时仍保留真实错误参数的退出码2。

验证命令和输入均只使用小型参数/窗口截图，无DAQ接收负载。临时构建与候选包随后清理。本轮不验证独立Ubuntu24、Windows Qt 5.14.2、macOS、物理双机、CoBo硬件或持续吞吐。整体重构仍未完成。

## 71.359 当前数据主机profile构建与Supervisor完整CTest（2026-10-23）

从当前工作树以Linux `data-host` profile构建数据接收/管理交付，7/7程序成功，1项嵌入依赖检查通过。另以`BUILD_TESTING=ON`构建当前`apps/datasupervisor`并运行完整CTest：**27/27通过**，覆盖catalog同名替换与文件集合、receiver catalog文件系统、普通TCP归档传输、Supervisor控制编解码、失败退役/交接、Native传输任务、Supervisor控制服务、接收进程生命周期，以及实验配置/协议/运行时核心。测试在本机允许的loopback环境执行，不是两台物理主机互通或目标网卡吞吐。

测试生成的`.graw`/`.raw`夹具共63个、总计1,581,061字节（最大49,152字节），远低于用户规定的20GiB上限。测试完成后已删除`/tmp/getdaq-data-host-rebuild`和`/tmp/getdaq-data-tests`两个本轮临时目录，并确认路径不存在；临时文件系统仍有615GiB可用空间。未使用现场DAQ数据或真实CoBo。Qt Windows 5.14.2、macOS、独立Ubuntu部署、真实双机以及连续DAQ吞吐未在本轮验证。

这轮验证只证明数据主机当前profile能构建且其软件测试通过，不代表Run替换生命周期已完全闭合：跨节点publish部分失败后的父级恢复仍待实现；同名旧文件在catalog逻辑集合中被新文件优先取代，但旧session/catalog backup中的旧载荷尚未物理回收。整套重构仍未完成。

## 71.358 当前源码control交付构建及完整Qt Widgets回归（2026-10-23）

从当前工作树在Ubuntu 20.04/WSL2用户空间以Qt 5.12.8、GCC 9.3/C++11构建`control` profile：Envoy控制台、Sentry、模拟器、配置/检查/上传工具、Supervisor查询、波形查看器及MCP共9/9成功；另有95项嵌入依赖检查通过。随后从同一源码完整重建`tests/tests.pro`并运行offscreen QtTest，结果 **134 passed、0 failed、0 skipped**，耗时约60秒。覆盖当前Run编号回退、legacy Sentry同名替换与两种恢复点、Native CoBo/全系统控制边界、MCP旁边的GUI控制流程及模拟器生成数据硬预算。loopback测试在获准环境运行；Qt offscreen关于窗口尺寸提示/raise的warning不影响通过结果。

测试过程没有连接真实ECC/CoBo，也没有做真实DAQ收发或吞吐验收；模拟器/GUI fixture使用项目私有临时目录并在测试结束时清理。control profile只包含控制侧9个程序，不代表data/data-host/full profile本轮重建。Qt 5.12.8/Ubuntu 20用户空间、WSL2宿主内核的证据不能证明用户Windows Qt 5.14.2原生运行或macOS。Native Run替换publish部分失败后的父级恢复、同名旧session/backup raw物理回收、真实双机及硬件验收仍未完成，整套重构仍在继续。文档链接检查1804项通过。

## 71.357 ECC控制MCP新增Run归档/替换工具（2026-10-23）

`getdaq-control-mcp`在`allow_control=true`时新增`ecc_catalog_run`，要求显式Experiment、正整数Run号、replacement布尔值、operation ID和精确匹配的确认字段。请求仍绑定固定ECC的client identity、snapshot和client epoch，并由ECC服务执行归档门槛及71.356的全节点Prepare屏障；`allow_control=false`时该工具不出现在tool list，隐藏调用也被拒绝。工具仅发送低频JSON，不访问raw数据通路。同步更新了[ECC MCP手册](ECC_CONTROL_MCP.md)及[Native Run替换规范](NATIVE_RUN_REPLACEMENT.md)。

严格C++11下`getdaq-control-mcp` CMake构建通过。本机loopback假ECC集成测试 **1/1通过**，覆盖工具schema、无效确认在HTTP请求前拒绝、归档替换参数绑定、身份/snapshot/epoch、只读模式对两个控制工具的隐藏与拒绝。测试只创建临时凭据和小型JSON，没有生成raw。文档链接检查在本次修改后待重跑。

此测试没有调用真实ECC，也不证明多节点publish部分成功后的父级恢复、物理旧同名raw回收、MCP host的人类审批界面、Windows Qt5.14.2、macOS、实板或持续DAQ吞吐。MCP接口提交的是显式归档操作，不能代替GUI的Run Start授权及Run号递增流程；整体重构仍未完成。

## 71.356 Native Run替换增加跨节点Prepare屏障（2026-10-23）

修改Supervisor catalog请求，给显式Run替换增加`catalog_phase=prepare|publish`和稳定`catalog_replacement_id`。每节点Supervisor仍以自身operation ID记录两个可区分的请求；两个请求引用同一个replacement ID，因此stage目录、candidate目录和其schema 2 completion receipt保持同一Run替换身份。普通catalog和旧的显式单阶段替换请求仍采用`single`兼容路径。DataSupervisor的prepare只创建/独立检查stage与candidate，不触碰正式旧catalog；publish要求此前candidate已存在，否则失败关闭。

`RemoteReceiverManager::CatalogReceivers(..., true)`现在先在`<action>_prepare`目录记录完整节点列表并逐个等待prepare完成。仅当所有节点都完成、结果receipt通过operation/Run/target校验后，才写入`<action>_publish`意图并向节点提交发布；prepare任一节点失败时不会生成publish意图。publish阶段仍没有跨主机原子rename：某节点发布失败后管理器仍会尝试其余节点，父ECC记录失败并保留所有节点本地journal/备份，尚无父级resume/retry闭环。

Linux GCC 9.4/C++11完整ECC/Supervisor树编译到100%。定向CTest **5/5通过**：`catalog-replacement-file-set`、`receiver-catalog-filesystem`、`supervisor-control-codec`、`native-ecc-remote-receiver-catalog-replace`、`native-ecc-remote-receiver-catalog-prepare-failure`。`supervisor-control-service`在loopback允许的隔离环境另行通过 **1/1**。双节点正常fixture确认两阶段请求使用相同stable replacement ID、两节点正式schema 2 catalog完成发布且旧非同名raw仍在retained集合。失败fixture有两个真实Supervisor和receiver进程：Run 1原catalog完成后，为Run替换的新session创建微型raw；将第一节点Run目录父级设为不可写后发起替换，另一节点prepare成功、被阻塞节点失败，fixture核对所有旧正式catalog receipt不变且`_publish/intent.json`不存在。网络在同机loopback，非物理双机。合成raw极小且远低于20 GiB；所有`/tmp`测试raw目录已在验证后删除。

验证不覆盖publish中途的进程崩溃、父级恢复、部分发布后的续作、物理同名raw清理、真实ECC上的MCP归档调用、Windows Qt 5.14.2、macOS、Ubuntu24独立系统、CoBo硬件或DAQ持续吞吐。完整重构仍未完成。


## 71.355 Native Run替换授权接入控制链并验证逐节点归档（2026-10-23）

Run替换确认位已由Envoy Start动作送入RunControl/NativeRunControl，在本机Run journal持久化，并通过ECC catalog控制请求与ECC远端接收管理器传入Supervisor catalog请求及DataSupervisor。协议字段可缺省以保持原普通catalog编码；显式`catalog_replace_existing=true`改变request key，避免把普通catalog和替换catalog误认为同一操作。每个数据主机执行stage、candidate准备和本机发布；GUI只有用户确认后才发送此标记。

验证：严格C++11下`getdaq-ecc-native`、`getdaq-supervisor`、`native-ecc-remote-receiver-tests`编译成功；`catalog-replacement-file-set`、`receiver-catalog-filesystem`和`supervisor-control-codec` CTest **3/3通过**。使用既有receiver产品二进制的普通远端receiver ECC loopback回归`native-ecc-remote-receiver-normal-0`在允许127.0.0.1 bind后 **1/1通过**（83.31秒）。新增`native-ecc-remote-receiver-catalog-replace`在两台逻辑数据主机（同机两个独立服务、每host一个receiver计划）上，先为Run 1建立旧catalog，再采集新session并调用`CatalogReceivers(...,true)`；测试检查父intent两项均含显式替换位、新receipt各自重新Inspect成功、schema 2 retained集合包含旧非同名raw、旧catalog backup receipt仍在。该case **1/1通过**；无跨物理主机通信。Qt 5.12.8/GCC 9.3下`tests/native_gui.pro`构建成功；`dataHostCatalogIsRequiredBeforeRunComplete`、`nativeConfirmedReplacementIsJournaledAndUsesCatalogReplacementIntent`、`nativeRunCompletionPersistsNextRunNumber`三个定向QtTest合计 **5 passed、0 failed**（含init/cleanup）。

新增`data_supervisor_test.cpp catalog-replace`以真实receiver进程完成旧session/catalog与新session/catalog，再把新归档发布到同一Run目标；独立检查schema 2合并catalog并确认旧Run的不同名raw仍在retained集合及正式目录。四个测试raw每个48 KiB（共192 KiB有效载荷，多处为同inode硬链接），整个fixture在验证后已删除。独立判定器fixture继续覆盖同source精确同名由新raw胜出；本新增真实receiver fixture本身没有制造同名碰撞。未连接真实硬件、CoBo或外部主机。

替换fixture实际生成的raw为8 KiB小文件，每项测试不超过20 GiB；fixture在回归后已从`/tmp/getdaq-native-remote-*`删除。限制：跨节点调用仍是逐节点stage+publish，没有全节点Prepare屏障；部分节点成功后的父级重试/观察/恢复未验证。发布后整份旧catalog backup与旧session保留，所以同名旧raw目前只从正式catalog逻辑集合剔除，尚未物理清理；非同名旧raw保留已验证。Run级操作无法宣称已完成，MCP/现场Run替换端到端、Windows Qt5.14.2、macOS、独立Ubuntu24及真实硬件均未验收。临时构建树约60 MiB、ECC原生树约161 MiB、Qt测试树约16 MiB；合成raw fixture已删除。

## 71.351 schema 2候选正式路径绑定与改名后重开检查（2026-10-23）

schema 2 replacement manifest现记录`publication_root`、`publication_target`和由本次operation ID确定的`publication_backup`；completion receipt中的`directory`指向正式Run catalog路径，而不是临时candidate路径。检查器核对正式root/experiment/Run/deployment组合及backup命名，因此候选在暂存路径可独立检查，之后同卷改名到正式路径仍可用同一completion receipt重新检查。保留旧文件的provenance指向将被保留的旧catalog backup位置。旧schema 1 receipt仍要求其物理路径与原directory字段相同，传输代码未改变。

新增filesystem回归在小型合成fixture上先检查candidate，再把旧catalog改名到候选声明的backup路径、把candidate改名到正式target，并在target重开检查；未删除旧backup或session raw。GCC 9.4/C++11下`catalog-replacement-file-set`与`receiver-catalog-filesystem` CTest **2/2通过**；schema 1 catalog TCP/文件系统传输CTest在受限环境首次因mock loopback `EPERM`未到协议断言，改在只允许127.0.0.1的执行中通过 **1/1**，相关测试最终3/3通过。构建目录约20 MiB，验证后清理；raw均为小型fixture并自动清理，远低于20 GiB。文档链接检查见本条目后的最终计数。

没有实现/执行生产路径发布命令、operation journal、故障恢复或旧同名session raw清理；没有ECC/CoBo/GUI/MCP接线、Windows/macOS/独立Ubuntu24或真实DAQ验收。该结果仅证明候选元数据能跨目录改名保持可检查，不能据此声称Run替换已完成。142 MiB专用构建目录已删除，fixture raw已自动清理；本轮Markdown链接检查1791项通过。

## 71.350 候选输入快照绑定完成凭据并保持旧归档传输兼容（2026-10-23）

候选准备现在在使用input manifest/request构建schema 2目录前，先把读取的manifest SHA-256与其原catalog completion receipt绑定，并核对新Run request中的operation ID/request key与完成凭据一致；完成后再对两个输入做全量复检并要求receipt未变化。避免候选使用从已验证receipt之外读到的短暂/错配元数据。

GCC 9.4/C++11下新构建catalog CLI与替换测试后，替换计划CTest及完整catalog filesystem/CLI测试 **2/2通过**；既有schema 1归档复制与TCP文件系统传输测试 **1/1通过**。传输测试首轮在默认沙箱因mock loopback `EPERM`于协议断言前失败，放宽本机测试环境后单项通过；只连127.0.0.1临时端口，不连接外部主机。测试fixture按默认预算运行、合成raw远低于20 GiB并自动清理；20 MiB专用build tree已删除。文档链接检查1789项通过。

这验证本机candidate Prepare和既有catalog传输兼容，仍未验证candidate被正式发布/交换、崩溃恢复、ECC多节点协调、Native GUI/MCP覆盖流程、Windows/macOS或真实DAQ。Native已有Run依旧拒绝覆盖。

## 71.349 生成并独立验证Native schema 2替换候选（2026-10-23）

新增`PrepareReceiverCatalogReplacement(old,new,candidate)`和CLI `getdaq-catalog --prepare-catalog-replacement OLD NEW CANDIDATE`。它要求两个完整catalog身份匹配、候选路径不存在，且旧/新catalog与候选父目录同一文件系统；随后按source把新Run文件作为current集合，把只有旧catalog包含的文件作为retained集合，硬链接到独立schema 2候选目录。相同source的精确同名只链接新Run版本；不同source与旧Run独有source保留。候选记录输入catalog路径/receipt摘要、文件size/device/inode和source目录；完成后调用catalog检查器独立验证候选。schema 1检查与读取保持兼容，schema 2候选可再次作为合并输入。输入catalog和raw均不修改，候选不发布为正式Run目录。

GCC 9.4/C++11重新构建`getdaq-catalog`和相关测试目标成功；`receiver-catalog-filesystem`及`catalog-replacement-file-set` CTest **2/2通过**。filesystem集成回归构造完整旧/新catalog，其中新Run撤掉一个旧source，验证同名新文件的device/inode与新源一致，旧非同名与retired-source文件的device/inode与旧源一致；验证候选独立检查、v2再规划，以及CLI单独进程成功。重复候选路径被拒绝，原候选收据不变。所有合成raw均为小型fixture、远低于20 GiB，fixture目录自动清理；18 MiB专用build目录已删除。无网络传输、真实receiver、ECC、CoBo或DAQ访问。

这是单机候选物化，不是可用于现场的Run覆盖事务：失败候选没有operation journal/自动恢复；尚无正式catalog目标交换、旧同名raw清理、ECC多节点提交、GUI/MCP Start接线或Run号递增。Native已有Run仍拒绝替换。使用边界见[Native Run替换事务规范](NATIVE_RUN_REPLACEMENT.md)。

## 71.348 替换计划携带已验证raw文件身份与来源（2026-10-23）

`PlanReceiverCatalogReplacement()`返回的每项现在包含被选中的源catalog路径、source存储目录、完成凭据SHA-256，以及raw大小/device/inode；纯文件名集合判定器仍只处理名字，身份字段由两个经过完整检查的catalog manifest提供。新增断言逐条用`lstat()`核对计划中的来源与文件身份，覆盖新catalog优先的同名raw、旧catalog独有raw及其他source。结合71.347的返回前快照复核，后续准备硬链接候选时可以依据明确的已验证源身份，而无需扫描或读取raw载荷。

GCC 9.4/C++11成功构建catalog CLI及两个测试程序；从构建目录运行既有catalog与替换计划CTest **2/2通过**。最大测试raw为1 GiB逻辑稀疏文件（低于20 GiB），测试fixture自动清理，18 MiB专用构建目录已删除；没有真实DAQ/ECC/CoBo访问。计划身份尚未被执行为新catalog硬链接，也没有接入ReceiverCatalog发布、ECC多节点协调或Native RunControl；已有Run仍拒绝替换。

## 71.347 替换计划返回前重新验证catalog快照（2026-10-23）

`PlanReceiverCatalogReplacement()`现在在生成source/file保留计划后，会重新读取并比较两份catalog的`request.json`、`manifest.json`，并再次完整调用catalog检查器，要求返回的完成凭据与最初验证结果逐字节相同。若计划计算期间catalog被替换或修改，则拒绝返回计划。此举封闭“先验catalog、后读文件集合”的并发变化窗口；仍然只读，不改变catalog，也不实现Run替换提交。

GCC 9.4/C++11下重建`getdaq-catalog`、`receiver-catalog-tests`和`catalog-replacement-tests`成功；从构建目录运行CTest，`receiver-catalog-filesystem`与`catalog-replacement-file-set` **2/2通过**。本机CTest不支持`--test-dir`：从源码目录调用时会报告`No tests were found`，故按实际支持方式先切换到构建目录运行；该零测试调用没有作为通过证据。测试最大逻辑稀疏raw为1 GiB（低于20 GiB），fixture自动清理，18 MiB专用构建树已删除；无真实DAQ/ECC/CoBo访问。详情见[Native Run替换事务规范](NATIVE_RUN_REPLACEMENT.md)。

## 71.346 从完整ReceiverCatalog生成只读Run替换计划（2026-10-23）

新增`PlanReceiverCatalogReplacement(oldCatalogDirectory, newCatalogDirectory)`：它先分别调用既有catalog检查器验证两份完整目录，再要求experiment、Run号和deployment一致、session与operation身份不同，之后按source读取catalog文件名元数据并生成替换计划。测试构造两份身份独立且完整的catalog，验证同source同名raw由新catalog胜出、旧catalog独有raw保留、另一个source的文件独立保留；相同目录和Run号不匹配均被拒绝。调用前后重新检查catalog，确认该计划阶段只读，不移动/删除文件、不打开raw载荷。

GCC 9.4/C++11构建`getdaq-catalog`及相关测试后，CTest中的`receiver-catalog-filesystem`和`catalog-replacement-file-set` **2/2通过**。测试使用的最大逻辑稀疏raw为1 GiB，低于20 GiB上限；测试fixture根已由测试清理，专用`/tmp/getdaq-native-replacement-71-346`构建目录已确认不存在。无真实ECC、receiver、CoBo或DAQ访问。该API目前只生成并分类计划，没有接入catalog发布/Finalize、session文件整理、ECC多节点事务、本机Run journal或GUI；Native已有Run仍拒绝替换。详见[Native Run替换事务规范](NATIVE_RUN_REPLACEMENT.md)。

## 71.345 Native替换判定器与现有catalog文件系统回归（2026-10-23）

把新`catalog-replacement-file-set`测试与现有`receiver-catalog-filesystem`放入同一GCC 9.4/C++11构建树，重新构建Supervisor核心库、catalog CLI和两个测试程序。CTest **2/2通过**：新判定器覆盖同名新文件优先、旧非同名文件保留及source隔离；原catalog套件继续验证catalog seal、receiver session绑定、硬链接不复制raw、元数据更改拒绝及catalog完成凭据。

catalog套件含1 GiB逻辑容量的稀疏raw fixture（低于20 GiB），其临时fixture根在测试后消失；`/tmp`测试构建树约51 MiB，已删除。无真实receiver、ECC、硬件或DAQ网络。该测试只证明判定器及既有不可变catalog仍可用；Native替换逻辑尚未接入catalog发布、多节点ECC事务、GUI或Run journal，已有Run仍被拒绝替换。

## 71.344 Native Run替换的按source文件集合判定器（2026-10-23）

新增C++11 `BuildCatalogReplacementPlan()`，只处理旧/新catalog的source ID和raw文件名元数据：同一source中相同完整文件名由新Run胜出；不同名旧raw保留；source不同则不构成碰撞；source不存在于新Run时，其旧文件仍进入逻辑保留集。拒绝空/越界source ID、非`.graw`名称、路径分隔符/异常字符以及超过65,536项的集合。该函数不打开或读取任何raw payload。

GCC 9.4/C++11下完整`getdaq-data-supervisor-core`静态库成功构建；`catalog-replacement-file-set` CTest **1/1通过**，覆盖同名优先、旧文件保留、新旧source隔离及非法元数据拒绝。无receiver进程、loopback、硬件或raw文件操作。测试构建树在验证后清理。这个纯判定器是Native替换第一阶段，尚未接到`ReceiverCatalog`、ECC多节点事务、GUI或Run journal；当前Native仍拒绝已有Run替换。

## 71.343 当前源码完整QtTest回归（2026-10-23）

从当前源码构建`tests/tests.pro`完整QtTest目标，使用Qt 5.12.8、GCC 9.3及C++11。首轮受限沙箱无法完成localhost监听和私有fixture访问；在允许这些本机mock测试资源的隔离环境中重跑，完整套件134/134通过，0失败、0跳过，用时58.94秒。覆盖配置/控制GUI、legacy Run替换与Sentry恢复、Native GUI、HTTP mock及模拟器集成等回归。

模拟器用例各自把生成数据预算限制在1 MiB和128 KiB，总上限约1.125 MiB；20 GiB上限校验和达到预算后停止写入的回归通过。测试使用私有临时目录，QtTest结束自动清理；专用25 MiB构建目录和qmake在`/tmp`产生的临时Makefile/stash随后删除，未留下`.graw`。未连接真实ECC、CoBo、数据主机或DAQ，因此这不是硬件/现场传输或持续吞吐验收。Native已有Run的receiver session/catalog同名替换仍未实现；Windows Qt 5.14.2 MinGW与macOS原生构建也未验证。

## 71.342 Run覆盖修改后的当前Linux full重建与29程序启动检查（2026-10-23）

## 71.342 Run覆盖修改后的当前Linux full重建与29程序启动检查（2026-10-23）

在legacy Sentry同名Run替换实现（71.341）之后，使用当前源码重新执行Linux `full` profile。构建回执状态为`complete`，覆盖29个主机程序并验证109项嵌入依赖。对构建树生成Linux候选包并通过包清单检查后，使用隔离临时HOME逐个启动全部29个程序；GetDAQ以Qt offscreen模式启动并保存临时截图，其余程序运行帮助或无输入校验。启动smoke receipt为`complete`，29/29，`hardware_accessed=false`、`raw_files_remaining=0`、`private_temp_cleaned=true`。

构建和候选包仅在`/tmp`生成，验证后删除；没有访问真实ECC/receiver/CoBo或DAQ，也没有产生`.graw`。full profile关闭CMake测试目标，因此本项是最新源码全产品编译、依赖和启动检查，不替代C++/QtTest。当前只验证Linux host工具链，不代表独立Ubuntu24、Windows Qt5.14.2/MinGW、macOS或现场验收；整套重构仍未完成。

## 71.341 Legacy Sentry Run回退仅覆盖同名raw（2026-10-23）

明确Run号回退后的raw语义：回退不提前动数据；在Start确认并完成新Run后，文件名完全相同的旧raw被新raw替代，文件名不同的旧raw保留在结果Run。legacy Sentry Finalize在同一数据卷内rename迁移旧Run独有raw，不复制载荷；manifest持久化每文件进度，重复Finalize可继续处理；如果rename已完成但进度记录尚未落盘，可用“旧路径缺失+新路径为普通文件”识别这次迁移。

Qt5.12.8/GCC9.3/C++11完整QtTest目标从当前源码构建成功。Sentry同名替换/两种Finalize中断恢复、RunRecorder替换/拒绝、RunControl多节点清理顺序、手动回退GUI及HTTP endpoint共9个用例，连同init/cleanup为11/11通过。覆盖同名新文件胜出、非同名旧raw保留、重复Finalize、rename后回执前恢复、已合并manifest但旧备份未删除时恢复，以及RunControl等待所有节点再清理并递增。loopback用例在允许本机localhost监听后执行。仅使用数十字节的小型文本fixture，无硬件/真实receiver/DAQ传输，无大文件raw；临时构建树约25 MiB，测试结束删除。

本轮没有重建full profile或刷新bin程序。Native receiver session/catalog回退替换、Windows Qt5.14.2、macOS、真实数据主机与现场CoBo仍未验证。整套重构仍未完成。

## 71.340 断开控制台后可手动回退Run编号（2026-10-23）

增加界面回归确认Run编号字段遵循设备操作安全门槛：控制台已连接时禁用编辑；断开后重新启用，用户可将自动递增后的编号手动调回。该测试只验证GUI配置入口，不触发Start、不对Run目录或数据主机做覆盖。

Qt5.12.8/GCC9.3/C++11重建`tests/tests.pro`完整QtTest目标成功；离屏定向用例`manualRunNumberRollbackRequiresDisconnectedConsole`为3/3通过（含初始化和清理）。没有网络监听、ECC/receiver/硬件访问或raw生成。临时构建目录在测试后清除。此项只验证GUI编辑门槛；legacy同名替换实现见71.341，Native仍拒绝已有Run/catalog替换。

## 71.339 Ubuntu24候选校验器兼容新增MCP程序（2026-10-23）

full profile新增`getdaq-control-mcp`后，当前完整程序清单为29项。Ubuntu24候选完整性检查器从`build-suite.py`动态取清单，但其回归测试仍将数量固定为28，且历史候选支持集合未覆盖“不含MCP”的旧版本。本轮将当前集合扩至29项，并显式保留MCP加入前26/27/28项的历史库存变体；其它未知清单仍拒绝。构建手册和Ubuntu24 CI验收说明已更新为当前29项，同时保留71.319及旧候选作为带时间标注的历史证据。GitHub Actions本身未在本轮运行。

`python3 tests/package_ubuntu24_catalog_ssh_test.py` **16/16**通过；`python3 tests/test_build_suite.py`通过；本地Markdown链接检查**1776项**通过。测试创建的临时包目录清理完成；无`.graw`或DAQ载荷生成。本轮没有Ubuntu24 rootfs实际加载/打包候选、没有重新构建程序，也没有访问硬件。该改动只修正包清单兼容门槛，不代表Ubuntu24、macOS或现场验收。

## 71.338 Linux full 构建修复qmake参数污染（2026-10-23）

整套`full`构建此前在MCP组件失败：构建脚本把`-DCMAKE_PREFIX_PATH=<Qt目录>`追加到所有非control步骤，其中qmake MCP项目不接受CMake的`-D`选项。现在脚本只给形如CMake配置命令（含`-S`和`-B`）的步骤设置Qt前缀；qmake仍使用自身选定的Qt SDK。新增无SDK构建计划回归，确保CMake步骤收到前缀且MCP qmake命令不含`-D`参数。

`python3 tests/test_build_suite.py`通过。修复后在Ubuntu20.04用户空间/WSL2、GCC、CMake3.16.3、Qt5.12.8/C++11从当前源码运行`python3 tools/build-suite.py --profile full --build-root /tmp/getdaq-full-current-3 --jobs 8`成功，receipt状态`complete`；29/29程序产物存在，Linux动态库依赖扫描通过，109个嵌入依赖文件核验通过。覆盖控制GUI、`getdaq-control-mcp`、receiver、Native ECC/监督器和配置仓库程序。该构建关闭CMake测试目标；这是全产品编译与依赖验证，不是全量CTest。receipt检查完成后，本轮和前两次失败尝试的临时构建树均已删除；未打包、未连接真实ECC/板卡/DAQ、未生成raw；不代表独立Ubuntu24、Windows、macOS或现场验收。

## 71.337 旧版CTest可用的配置核心验证命令（2026-10-23）

更新操作手册中的 CTest 示例，统一改为先切换到构建目录再运行，避免 Ubuntu 20.04 常见的 CMake/CTest 3.16 把`--test-dir`当作无效选项、转而在源码目录报告“没有测试”。经实际复现确认：在项目根目录执行`ctest --test-dir /tmp/getdaq-raw-collision-lock-build`会错误地查询当前目录；必须在目标构建目录执行`ctest`。

使用GCC 9.4/CMake 3.16.3/C++11从当前`common/experiment`源码新建临时构建，完整构建成功；进入构建目录运行CTest，实际发现15项并全部通过，包括配置读回/选择、CoBo/AGET/AsAd配置、扫描策略与状态机。未启用Qt、未访问硬件/ECC/receiver、未生成raw。临时构建树验证后删除；Markdown本地链接检查1773项通过。更新了`AGET_CONFIGURATION.md`、`COBO_READOUT_CONFIGURATION.md`、`EXPORTER_LOCK_ISOLATION.md`、`ECC_JOURNAL.md`和本记录中可复现的命令，历史中记录当时无效命令的证据保留。

## 71.336 同名Raw活动文件拒绝覆盖（2026-10-23）

补充精确文件名碰撞保护：当旧Raw文件仍由一个打开的接收句柄持有排他锁时，第二次`OpenRawStorageFile()`必须失败，且原文件字节保持不变；关闭旧句柄后才允许同名文件按既有规则截断写入。`receiver-core`新增该回归，覆盖“新Run开始尝试同名写入，但旧文件仍活跃”的保护边界。

从当前源码以GCC 9.4/C++11在`/tmp/getdaq-raw-collision-lock-build`完成receiver全目标构建；CTest共6/6通过，包括receiver-core、exporter-lock及ByteStorage进程级回归。后两项仅绑定本机127.0.0.1，默认沙箱拒绝socket后在获准的loopback权限下重跑通过。ByteStorage合成载荷393,353字节，脚本逐字节核对后删除raw并报告`raw_remaining=0`；全程远低于20 GiB，未连接真实DAQ/硬件。临时3.7 MiB构建目录随后删除。本验证只保护精确同名活动文件，不实现Native Run/catalog/session级替换。

## 71.335 Native Run完成后保存递增编号（2026-10-23）

Native模式此前在ECC/receiver/catalog完成后仅递增内存中的Run字段；退出并重新打开操作台会再次读取旧配置，可能重复使用已完成的编号。现在`NativeRunControl::FinishRun()`在Run目录记录与`Complete` journal成功后递增，并在`RunConfig::filePath`指向独立用户配置文件时保存新编号。若用户打开的文件本身就是当前Run目录的`run.yml`历史快照，则保留它不改写；自动保存失败只报告错误，不把已经完成的Run降为失败。

在Qt5.12.8/GCC9.3/C++11下重建`tests/native_gui.pro`和完整`apps/envoy/envoy.pro`目标，后者更新`bin/GetDAQ`。新增`nativeRunCompletionPersistsNextRunNumber`运行真实Native控制路径对接本机假ECC：先保存编号0配置，经Describe/Prepare/Configure/Start/Stop及receiver结束确认完成，再重开同一配置文件核对编号为1。该用例及`dataHostCatalogIsRequiredBeforeRunComplete`、`startupConfigurationIsBoundBeforeStartAndSavedWithRun`合计5/5通过；文档链接1771项通过。没有连接真实ECC、receiver、硬件或DAQ；仅使用临时YAML/JSON/日志，没有生成raw。此改动没有实现Native Run/catalog覆盖；手动回退到已存在Native Run仍会在Start前被拒绝。

## 71.334 ECC控制MCP适配器与本机假服务联调（2026-10-23）

新增独立的`getdaq-control-mcp` stdio server，复用固定ECC服务的认证HTTP API，不驱动Qt窗口，也不读取/转发DAQ原始数据。工具列表提供identity、cached status、receiver和operation查询；控制命令默认关闭，只有本地配置显式`allow_control=true`后才公布有限的ECC lifecycle submit工具。提交要求独立operation ID、持久client epoch与精确确认字段；MCP自身不自动重试POST，未知应答应按原ID查询。详细配置、AI host接入、网络边界和命令语义见[ECC控制MCP手册](ECC_CONTROL_MCP.md)。

Ubuntu20.04.6/WSL2 host使用Qt5.12.8、GCC9.4、CMake3.16.3对独立CMake target构建成功；qmake/C++11产品入口也构建成功，`ldd`显示应用链接Qt5Core/Qt5Network而不链接QtGui。注册CTest的本机loopback fake-ECC集成测试1/1通过，覆盖MCP初始化/stdio framing、工具schema、状态和操作读取、无效确认不访问ECC、Bearer认证、成功POST的instance/snapshot/client/epoch绑定、已有ECC owner epoch复用、read-only隐藏控制及超1MiB消息拒绝。Windows构建入口合同回归19/19通过；build-suite计划显示control包含原8程序及独立MCP程序、full计划29程序且MCP只登记一次；文档链接1770项通过。

fake服务只收发小型JSON，未创建任何raw，也未访问真实ECC、receiver、板卡或DAQ。没有在本轮用用户Windows Qt5.14.2 MinGW实际构建此新程序，未运行full profile编译；macOS、真实MCP host审批交互、跨机TLS/隧道和现场控制均未验证。此结果是适配器协议联调，不替代MCP host对人的控制确认，也不证明硬件操作安全性。

## 71.333 同名Raw碰撞拒绝FIFO且不阻塞（2026-10-23）

同名Raw打开现在对既存路径加`O_NONBLOCK`，避免FIFO在文件类型检查前挂住接收线程；随后仍必须通过当前用户所有、私有权限、单硬链接普通文件、路径身份一致和非阻塞独占锁检查，才允许截断。`receiver-core`新增FIFO碰撞回归，确认打开被拒绝且FIFO原样保留；CTest在30秒超时约束下1/1通过，当前运行约0.18秒。修改后完整receiver构建树6项CTest最终6/6通过：`mfm-merge`、`gmat-conversion`、`cobo-hit-pattern`、`receiver-core`、`exporter-lock`、`byte-storage-integration`。后两项首次因沙箱禁止创建localhost socket失败，随后获准的本机loopback重跑2/2通过；ByteStorage只收发393,353字节合成数据，脚本校验并删除全部raw，`raw_remaining=0`。所有测试远低于20 GiB，无真实DAQ或硬件连接。该修复不改变精确文件名碰撞规则，也不实现Native Run目录/session/catalog替换。

复现：`cmake --build build-receiver/linux-host-71-189 --target receiver-core-tests -j2`，随后从`build-receiver/linux-host-71-189`目录运行`ctest -R '^receiver-core$' --output-on-failure`。若使用旧版CTest，需先切换到构建目录再执行，以免忽略`--test-dir`参数。

## 71.332 Envoy应用日志只读目录回退（2026-10-23）

只读环境启动Envoy时，应用日志现在依次尝试Qt用户数据目录、缓存目录和按进程隔离的系统临时目录；成功使用回退目录时，主界面日志显示实际日志文件路径。此前失败只会留下屏幕错误且丢失文件日志。使用Qt 5.12.8、GCC/C++11从当前源码完整qmake构建`apps/envoy/envoy.pro`成功，更新了`bin/GetDAQ`；通过将`XDG_DATA_HOME`和`XDG_CACHE_HOME`指向不可写的`/proc`路径进行离屏启动，窗口截图和临时日志均确认回退成功，日志包含准确回退路径。本次只验证Linux/WSL当前Qt，不代表Windows/macOS相同目录权限行为。

该smoke未连接ECC、CoBo、DataSupervisor或数据主机，也没有生成raw。用于检查的截图和临时日志在验证后删除；保留更新的产品`bin/GetDAQ`。复现：`XDG_DATA_HOME=/proc/getdaq-readonly XDG_CACHE_HOME=/proc/getdaq-cache QT_QPA_PLATFORM=offscreen bin/GetDAQ --screenshot /tmp/getdaq-ui.png`。

## 71.331 ICE入口集成回归与精确同名覆盖代码路径构建（2026-10-23）

使用`third_party/ice-compat`提供的仓库集成构建目录，从当前源码构建`getdaq-ice-ingress-core`、`getdaq-ice-receiver`、`getdaq-catalog`及`getdaq-data-supervisor-core`，并启用`BUILD_TESTING`后构建`ice-ingress-fixture`和`receiver-catalog-tests`。另在原生receiver构建目录重新构建并运行`receiver-core` CTest，1/1通过；集成目录运行`receiver-catalog-filesystem` CTest，1/1通过。`tests/run_ice_ingress_tests.py`以实际ICE入口程序执行15个loopback场景，全部符合预期，覆盖完整/空输入、byte/frame storage、导出、legacy结束以及错误会话/连接/计数、截断、重连和超限拒绝。ICE入口实际编译包含`apps/datareceiver/rawfile.cpp`，所以精确同名文件覆盖实现也进入此构建路径；本次场景不是专门的重复文件名碰撞用例。

该回归生成11个合成`.graw`，总计147,457字节；`payload-cleanup.json`记录11个文件均已删除、`remaining_raw=0`。另一次受限启动只留下空测试目录且没有raw，已删除。无大文件传输，远低于20 GiB限制。测试只在Ubuntu/WSL本机loopback运行，不代表Windows/macOS、双机网络、真实ICE/CoBo/ECC、现场DAQ或持续高吞吐验收。此项也不实现Run号回退后的整次Run替换；`std::ios::trunc`式同名覆盖只在数据接收端实际产生完全相同文件名时生效。整套重构仍未完成。

复现入口：配置集成树`cmake -S third_party/ice-compat -B <ice-build> -DBUILD_TESTING=ON -DGETDAQ_BUILD_ICE_RECEIVER=ON`，构建`getdaq-ice-ingress-core getdaq-ice-receiver getdaq-catalog getdaq-data-supervisor-core ice-ingress-fixture receiver-catalog-tests`，然后分别从`<ice-build>`和`build-receiver/linux-host-71-189`构建目录运行`ctest -R '^receiver-catalog-filesystem$' --output-on-failure`与`ctest -R '^receiver-core$' --output-on-failure`。ICE场景命令为`python3 tests/run_ice_ingress_tests.py <ice-build>/getdaq-ice-receiver <ice-build>/ice-ingress-fixture <ice-build>/ice-ingress-validation`。首次默认沙箱运行无法绑定loopback；本次回归在获准的本机loopback环境完成。

## 71.330 精确同名Raw覆盖回归（2026-10-23）

DataReceiver写盘现在保留“只有完整文件名精确相同才覆盖”的简单语义：新文件仍独占创建；同名目标仅在确认是当前用户所有、私有权限、单硬链接普通文件且可取得非阻塞排他锁后才截断重写。符号链接、多硬链接归档、权限不安全目标和正在写入的文件均拒绝。此规则不把Run号映射到文件名，也不执行Run目录/旧session清理；Managed Native仍使用独立session目录，Native catalog已有Run目标仍由归档层拒绝。

在Ubuntu 20.04.6/WSL2、GCC 9.4.0、CMake 3.16.3下重建`getdaq-receiver`与`receiver-core-tests`成功；receiver-core单测通过，覆盖同名截断重写及硬链接/符号链接保护。随后GET文件命名TCP集成测试四组通过：混合来源6文件、大小轮转13文件、重连2文件、FDT 1文件；合成线上字节共13,676,684，所有生成`.graw`在测试后删除，测试根内无遗留raw。默认沙箱禁止本机socket bind，集成回归在获准的localhost测试环境重跑通过。无真实DAQ/CoBo/ECC/数据主机访问，不代表Native Run回退覆盖、Windows/macOS或现场验收。

复现核心检查：`cmake --build build-receiver/linux-host-71-189 --target getdaq-receiver receiver-core-tests --parallel 2`，然后从构建目录运行`ctest -R '^receiver-core$' --output-on-failure`。GET TCP集成入口见`tests/receiver_file_naming_integration.py`。

## 71.329 DataReceiver 六项原生回归（2026-10-23）

在 Ubuntu 20.04.6/WSL2、GCC 9.4.0、CMake 3.16.3 环境，对现有 `build-receiver/linux-host-71-189` 执行增量构建，补齐缺失的 `hitpattern-tests`，随后从构建目录运行完整 CTest。`mfm-merge`、`gmat-conversion`、`cobo-hit-pattern`、`receiver-core`、`exporter-lock` 与 `byte-storage-integration` 共 **6/6 通过**。其中 exporter-lock 验证发布线程在客户端发送被阻塞时仍可完成、保序发送、慢客户端超预算退出、worker故障关闭连接及TCP reset不会触发进程级SIGPIPE；ByteStorage进程测试验证384 KiB+137字节任意二进制载荷原样保存、EOF seal和FDT拒绝。

ByteStorage测试只生成393,353字节合成`.graw`，在隔离临时目录自动清理；测试后确认没有残余临时运行目录或`.graw`。构建树约3.8 MiB。两个需要本机loopback的测试仅连接本机临时端口；未连接真实CoBo/ECC/数据主机，也未测试持续高吞吐。此次只覆盖原生DataReceiver/离线数据工具核心，不验证Qt控制台、Native Run覆盖、跨主机、Windows/macOS或整套重构完成度。

复现命令（从构建目录运行，兼容当前CMake/CTest版本）：

```sh
cmake --build build-receiver/linux-host-71-189 -j2
cd build-receiver/linux-host-71-189
ctest --output-on-failure
```

## 71.328 Run编号回退覆盖legacy流程接入（2026-10-23）

Run编号spinbox可以手动回退。若legacy SOAP/Sentry模式下本机Run目录已存在，Start先要求操作员明确授权；RunRecorder仅接受身份匹配且journal为`Complete`的旧Run，并在同一文件系统保留旧Run副本。显式Sentry `/catalog` 使用持久化operation ID：旧完成catalog移入隐藏备份后发布新catalog；所有新catalog及本机Run完成后，RunControl再逐节点请求清理旧catalog，最后清理本机备份并递增Run编号。普通Reserve和普通`/catalog`继续拒绝覆盖。失败、超时或未知结果时不递增编号，恢复沿用原operation ID。

Linux/WSL Qt5.12.8、GCC 9.3/C++11下，Run Control和Sentry专项QtTest共13项通过，包含旧Run完整性拒绝、Sentry显式替换和幂等收尾、普通状态机回归及“所有新catalog完成后才清理并递增”。独立qmake构建的Envoy与Sentry可执行目标均成功。文档链接检查1754项通过。完整`getdaq-tests`从当前源码重新构建后，在允许本地loopback mock的隔离测试执行中**132通过、0失败、0跳过**。先前默认受限环境的47通过/69失败由测试监听和临时配置夹具不可用导致，不是这次完整回归的结果。

测试运行器只在独立`/tmp`私有树工作，实时监控其中全部`.graw`总量，达到20 GiB即会终止测试；实际测试结束时0个`.graw`、0字节残留。无真实ECC/receiver/数据主机/DAQ访问。测试和应用构建树在验证后删除。生产目标`bin/GetDAQ`和`bin/getdaq-sentry`已由本轮源码构建更新。当前只支持legacy Sentry：Native receiver session/catalog替换未实现。跨主机不是原子提交；单个节点可能已发布新catalog而其他节点失败，旧数据仍保留在各自隐藏备份，Run号不递增，需恢复或人工检查。无Windows、macOS、现场硬件或持续DAQ吞吐验证。详细行为与限制见[Run编号与覆盖](RUN_NUMBER_AND_OVERWRITE.md)。

## 71.326 内存状态机拒绝无证据退休并隔离旧退休授权（2026-10-23）

进一步检查发现，journal虽已拒绝新退休操作，但可独立使用的`ScanExecutionState::Transition()`仍允许`Bound → Retired`；而旧退休journal若解码为Retired，也会满足原来的`WithNextPoint()`前置条件。现将普通状态转换也限制在`Bound`：新退休必须等待未来带有ECC退出、receiver/catalog终态和控制权租约证据的专用journal操作。解码旧operation-complete或旧confirmed-applied retirement时仍重放原状态以保持审计可读，但对象会记录历史退休标记，拒绝用其推进新点。未决旧退休的确认未发送仍回到Bound。

Qt-free C++11 `scan-execution-state-tests`和集成`ecc-journal-tests`均从当前源码构建并直接运行通过。新增断言覆盖直接状态转换拒绝、旧成功退休和旧人工确认退休仍可解码、两类旧退休均不能调用`WithNextPoint()`；旧未决退休的拒绝及not-sent恢复回归继续通过。Markdown本地链接检查在文档更新后通过。本轮没有完整QtTest、GUI、真实ECC、receiver、硬件、DAQ或raw文件活动。

## 71.325 扫描journal退休历史兼容性回归（2026-10-23）

在71.324的禁止无证据退休护栏上补齐旧journal兼容性测试：解码含未决旧`retire` intent的记录后，journal保持`NeedsReconciliation`；通用人工对账不能将其确认成已执行，确认未发送则回到原`Bound`状态。历史上已经完成并记录为`Bound → Retired`的journal仍可读取，避免安全修正破坏既有审计数据。

Qt-free C++11目标`scan-execution-state-tests`从现有`build-ecc`构建并运行通过（PASS）；更新本记录后，Markdown本地链接检查1751项通过。该测试只使用小型内存JSON控制元数据，无GUI、ECC、receiver、硬件、DAQ或raw文件活动。本轮没有重跑完整QtTest。

## 71.324 扫描journal阻止无证据退休与跨点推进（2026-10-23）

审查单点 `Bound → Retired` 边界时发现：虽然GUI没有退休入口，journal核心仍允许用通用 `retire` intent 和任意成功响应推进到 `Retired`，继而调用 `WithNextPoint()`。这会把归档绑定误当作ECC实例已关闭、控制权已释放。核心现拒绝新建通用 `retire` intent，拒绝通用完成及人工“确认已执行”对账推进该操作；既有退休日志仍保留解码映射。未提供退休证据前，`Bound`不能推进至下一点。架构文档说明未来必须核对ECC退出/状态、receiver与归档终态及控制权租约释放，并将证据绑定当前点身份。

在已有 `build-ecc` 树仅重建C++11 `scan-execution-state-tests`，运行通过（1/1）；测试覆盖 `Bound` 状态拒绝普通退休intent且拒绝进入下一点。Markdown本地链接检查1749项通过。无GUI/ECC/receiver/硬件/DAQ访问，也未生成raw；使用已有小型控制元数据测试，不创建大型传输文件。

## 71.323 扫描窗口的Run作用范围与中断状态提示（2026-10-23）

扫描计划窗口标题与说明已更新为“计划与Run控制”：计划生成仍为离线操作，Native运行区明确会连接ECC进行身份核对并提交整系统生命周期动作；原始DAQ数据仍由receiver服务接收。之前说明中把整个窗口描述成“不连接ECC、不启动采集”已与现有功能不符。运行journal停留在`RunStarted`或`RunStopped`时，状态卡片现在使用琥珀色待处理样式并指出应核查实时ECC状态、查看/对账journal，避免把未收尾点显示成可直接继续的普通状态。

Qt5.12.8/GCC9.3/C++11下从当前源码构建`tests/tests.pro`完整QtTest目标；`offlineScanPlanBuilderIsAvailableFromConsole`与`nativeSingleCoBoLifecycleUsesWholeEccTransitions`两项定向测试均通过，各3/3（含init/cleanup）。测试只构建并显示离屏控制GUI，不连接ECC/receiver/硬件，不生成或传输DAQ/raw。临时构建目录已在测试后删除。完整QtTest套件本轮没有重跑。

状态机源码对照结论：旧GetBench `BackEnd::createEngine()`是单个ECC整系统状态机，`onPrepare`/`onConfigure`委托`SystemManager`；旧GetController GUI则以单个`TestController`呈现选定CoBo工作流。Native仅在配置确为单CoBo、数据源目标匹配且无MuTaNT时，把行内快捷动作转发给整套ECC状态机；多CoBo下仍没有独立逐板生命周期FSM，单板重准备维持Prepared-only语义。没有访问真实设备或执行状态转移。

## 71.322 单扫描点Run至归档绑定的fake ECC集成回归（2026-10-23）

从当前源码重建完整QtTest目标后，运行 `nativeScanRunFinishesReceiverAndCatalogAfterEventTarget`。测试创建两点Pulser计划，但只执行第0点；通过本机fake ECC HTTP服务验证身份接纳、逐次身份重验、整系统Describe/Prepare/Configure、事件基线查询、Run启动、receiver事件轮询、事件数达标后只提交一次Stop、状态读回、Breakup、receiver finish/seal、catalog验证、Run记录完成及scan journal绑定。最终状态为 `Bound`，pending intent清空，Run编号从1推进至2。该覆盖直接证明当前Linux fake服务上的单点控制与元数据归档成功链已接通；不证明真实ECC、远端receiver子进程、真实文件seal、板卡或DAQ吞吐。

Qt5.12.8/GCC9.3/C++11 `tests/tests.pro`完整测试目标构建成功；该集成用例3/3通过（含init/cleanup，约2.8秒）。随后完整`getdaq-tests`套件127/127通过，耗时约57秒。回环fake扫描测试仅交换小型控制/生命周期JSON，不写raw；完整套件中的独立模拟器回归最多生成1 MiB和128 KiB的标记测试`.graw`，均位于`QTemporaryDir`并在用例后自动清理。没有真实receiver、板卡或DAQ服务访问，也没有传输采集载荷；测试生成物远低于20GiB，本轮临时测试目录已确认清除。本轮qmake构建树已用`make distclean`清除。Windows Qt5.14.2/MinGW、macOS、独立Ubuntu24及现场服务未在本轮验证。此用例只执行计划第0点；下一点的工件装载、ECC部署切换/重新启动、自动继续及跨点崩溃恢复仍未实现。

## 71.321 旧SOAP模式逐节点Progress/Regress回归（2026-10-23）

对照旧GetBench源码确认：SOAP兼容模式的 `ProgressSlot(id)` / `RegressSlot(id)` 针对被选中的ECC节点发出动作；它与Native模式的整机ECC FSM快捷入口不同。新增双mock节点回归，覆盖CoBo与MuTaNT分别Describe/Prepare、CoBo在MuTaNT尚未Prepare时被全局顺序约束阻止、CoBo单独Configure，以及只对CoBo执行Breakup/Undo且MuTaNT状态保持不变。该测试补上了此前只测全局快捷入口、未直接锁定逐节点路由的缺口。

Qt5.12.8/GCC9.3/C++11测试目标构建成功；`legacySelectedNodeProgressAndRegressRemainIndependent`定向QtTest为3/3通过（含init/cleanup，约0.85秒）。默认沙箱禁用本机TCP监听；在仅对该测试进程开放本机回环后，两个mock ECC服务和控制器回归通过。没有连接真实ECC、硬件、receiver或生成DAQ/raw文件。完整QtTest、Windows Qt5.14.2、macOS及现场SOAP服务本轮未验证。实现测试位于 `tests/test_getdaq.cpp`。

## 71.320 状态表Transition提示及旧单板操作语义核对（2026-10-23）

主界面状态表的Transition单元格现在按稳定、正在转换、不可用/未知分别使用中性、琥珀、红色提示，并在悬停时明确作用范围：Native显示“整个ECC系统的转换，在每个配置数据源行重复”，旧SOAP兼容模式显示“当前单个ECC节点的转换”。原有ECC状态列仍在Transition期间显示琥珀色，避免把上次确认状态误看成新状态已完成。

对照随仓库保存的GetBench 2021-02-08源码：`BackEnd::createEngine()`定义ECC全局Describe/Prepare/Configure/Start/Stop/Pause/Resume/Breakup/Undo状态机；`BackEnd::onPrepare()`与`onConfigure()`调用SystemManager处理全系统；同时`BackEnd::prepareNode()`/`configureNode()`确实提供选中节点单独准备/配置入口。主程序旧SOAP模式保留逐节点Progress/Regress；Native模式的“single-CoBo system”快捷键只有一个受控CoBo、数据源ID匹配且无MuTaNT时出现，并转发到全局ECC状态机。多CoBo Native模式不声称存在独立的单板全生命周期FSM，仍使用有明确Prepared门槛的单板维护流程。源代码复核路径见`source/get/GetBench-software-release-20210208/src/get/rc/BackEnd.cpp`及`apps/envoy/runcontrol.cpp`。

验证：Qt5.12.8/GCC9.3/C++11完整QtTest目标编译成功；`nativeGuiMetadataAndWholeSystemControls`与`nativeSingleCoBoLifecycleUsesWholeEccTransitions`专项用例通过（4/4，含init/cleanup）。离屏测试覆盖转换中颜色、Native整机范围tooltip、单CoBo整机按钮以及多CoBo/含MuTaNT/目标不匹配时只显示维护入口。此次只验证GUI元数据和路由，不连接旧/新ECC服务，不执行单板硬件操作、Run或DAQ/raw传输。完整QtTest未在本轮重跑；编译对象与临时构建文件已清理。

## 71.319 Linux full 当前源码重建与程序启动smoke（2026-10-23）

71.318修正Transition表头列宽后，在Ubuntu20.04.6/WSL2、Qt5.12.8主机从同一当前源码重新构建`full` profile。构建receipt状态`complete`，绑定549项源码输入；28个程序全部重建，SHA256/`ldd`依赖检查通过，嵌入依赖109个文件核验通过。用该次实际二进制生成188文件临时运行目录，包manifest校验和28程序启动smoke均通过，GetDAQ以offscreen模式无连接打开初始窗口并截图退出；未访问硬件、未生成raw，测试用户目录、截图及每次进程的数据均在临时目录清理。构建输入摘要在smoke之后与当前C++/构建入口输入再次比较一致。

这条`full`发布构建显式关闭CMake `BUILD_TESTING`；native目录的`ctest -N`结果为0，因此本项是全程序构建、依赖和启动烟测，不是CTest/单元/集成测试。平台仅是Ubuntu20.04.6/WSL2，不替代独立Ubuntu24、macOS、Windows、硬件或持续吞吐验收。未执行DAQ/raw传输负载，遵守20GiB上限。临时构建、运行包和smoke回执已删除。证据：GetDAQ `tools/build-suite.py`、`tests/ubuntu24_package_program_smoke.py`、`docs/VALIDATION.md` 71.319。

## 71.318 主界面Transition列标题宽度修正（2026-10-23）

检查已交付的1440×960主窗口截图时发现，系统状态表的`Transition`列只有72 px，英文表头被压缩为残缺文字，用户难以快速确认单CoBo/整套ECC状态机当前的迁移动作。本轮把Native和旧兼容表的该列统一加宽到104 px，仍保持交互调整，长状态值继续可滚动并可通过单元格提示查看。新增UI回归要求列宽不少于100 px。

Qt 5.12.8/GCC9.3/C++11 `tests/tests.pro`完整目标构建成功；`nativeGuiMetadataAndWholeSystemControls`专项QtTest 3/3通过（包含init/cleanup）。从`apps/envoy/envoy.pro`重建并更新`bin/GetDAQ`，在隔离HOME及offscreen环境启动成功，1440×960截图人工复核确认Transition完整显示。仅启动初始界面，没有ECC/receiver/硬件/DAQ访问或raw生成；临时测试构建、截图及用户目录已删除。

本轮只变更状态表视觉宽度及回归断言，没有重跑full profile、Ubuntu24/Windows/macOS或真实系统状态迁移；上一条71.317 full套件烟测发生在本项UI源修改之前。证据：`apps/envoy/mainwindow.cpp`、`tests/test_getdaq.cpp`、当前`bin/GetDAQ`。

## 71.317 Linux full 套件当前源码启动检查（2026-10-23）

在 Ubuntu 20.04.6/WSL2、GCC及Qt 5.12.8主机上，`tools/build-suite.py --profile full` 的完整构建收据与构建时549项源码输入摘要一致，报告28个程序构建完成，109个嵌入依赖文件核对通过。对28个构建产物逐项重新计算SHA256，并确认收据记录的`ldd`无缺失库；使用同一收据生成临时188文件运行目录后，`tests/ubuntu24_package_program_smoke.py` 对28个程序全部启动检查通过。Qt GUI在offscreen模式只打开初始窗口并截图退出；其它程序只执行帮助或无参数输入校验。smoke确认未访问硬件、未留下`.graw`，临时用户数据和截图自动清理；文档链接检查1737项通过。随后71.318只修改Envoy状态表列宽，完整套件没有按当前最新源码重新构建。

本机full profile发布构建关闭了`BUILD_TESTING`，该build目录`ctest -N`列出0项；因此本节是编译、依赖与程序启动验收，不是单元/集成测试。当前执行环境是Ubuntu 20.04.6/WSL2，不等于独立Ubuntu 24、macOS或现场验收。Windows重建尝试因WSL interop返回`UtilBindVsockAnyPort: socket failed 1`而未能启动Windows qmake，不能据此判断Windows源码构建失败；Windows Qt5.14.2/MinGW当前源码构建仍待可用的原生Windows环境重跑。无DAQ/raw传输测试，未触碰板端。

临时构建 `/tmp/getdaq-full-current-71-316`、smoke运行包 `/tmp/getdaq-full-smoke-stage-71-317` 和回执均需在验证后删除。证据：该build的`full/build-receipt.json`、smoke回执 `/tmp/getdaq-full-smoke-evidence-71-317.json`。这项结果不更新候选包，也不改变28个程序的目标平台/硬件验收标志。

## 71.316 扫描窗口显示journal当前点进度（2026-10-23）

扫描窗口在运行控制区增加当前点状态卡片，显示计划尚未创建、计划已验证但尚未确认、journal当前点及状态，未决/未知操作时以错误态提示必须先对账。`Bound`只显示为归档已绑定，并明确说明硬件退休及下一点配置不会自动发生；不会把catalog绑定误呈现为设备已释放。卡片只读取本地冻结计划/journal，不轮询ECC、不发送控制命令、不访问receiver数据。确认计划Review后立即刷新状态。

GUI离屏回归`offlineScanPlanBuilderIsAvailableFromConsole`验证无计划、已验证待Review、`Reviewed`和未决操作需对账四种状态及错误视觉态；Qt5.12.8/GCC9.3/C++11测试目标和Envoy目标从当前源码构建，完整QtTest 126/126通过。文档链接检查1737项通过。没有生成DAQ/raw数据；临时构建目录测试后删除。该卡片是journal进度显示，不是ECC/硬件状态证据，也不实现`Bound→Retired`或下一点配置切换。

## 71.315 扫描绑定自动采用journal当前点（2026-10-23）

Run绑定控件明确显示“绑定到当前点”，并提供说明；绑定流程不再要求操作员手工输入点号。窗口先读取经过校验的当前扫描journal，只有无待决/未知操作且状态为`CatalogVerified`时才继续；catalog工具参数和输出文件名都由journal的当前点索引生成。这样避免让操作员选中一个计划中存在、但当前journal并未到达的点，再等到后续校验才拒绝。该改动只消除错误点选择，不推进`Bound`之后的退休或下一点配置切换。

验证：Qt5.12.8/GCC9.3/C++11完整QtTest目标从当前源码构建成功；扫描Run收尾及绑定链集成`nativeScanRunFinishesReceiverAndCatalogAfterEventTarget`通过（3/3，含初始化/清理），Native主界面、单CoBo系统状态语义和journal重放回归合计5/5通过；完整QtTest 126/126通过。完整套件在当前用户权限下因loopback监听/临时目录限制出现环境失败，获准在本机测试权限下对相同二进制重跑后全绿。测试只使用小型fake ECC控制元数据和绑定JSON，没有启动receiver或生成DAQ/raw数据。当前仍没有真实catalog CLI/硬件绑定验收、Windows Qt5.14.2/MinGW、macOS或现场验收。

## 71.314 扫描归档绑定写回journal（2026-10-23）

之前GUI的 `getdaq-catalog --bind-scan-point` 只生成独立绑定文件，成功后没有推进对应执行journal。现在绑定入口仅接受当前 `CatalogVerified` 点：启动独立catalog工具前，先持久化包含计划、点索引、catalog目录和输出路径摘要的 `bind` intent；异步操作期间保持journal独占锁。工具返回后必须确认计划仍为原摘要、绑定文件为安全的有界普通文件、stdout与发布文件逐字节相同，且JSON kind/manifest摘要/点索引匹配，才记录intent完成并推进到 `Bound`。失败或证据不匹配会保留原operation ID为未知并阻止再次绑定；进程异常退出后重开journal也会隔离未完成intent。

Native scan fake HTTP回归扩展到 `CatalogVerified -> bind intent -> Bound`，检查归档绑定输出、成功journal完成以及重复绑定拒绝；定向测试3/3通过，完整QtTest 126/126通过。完整 Envoy 与测试目标均以Qt5.12.8/GCC9.3/C++11从当前源码构建成功，`bin/GetDAQ`已更新。文档链接检查1735项通过。测试只使用小型合成控制元数据和临时绑定JSON，没有启动receiver或生成DAQ/raw数据。此处不自动写`Retired`，不自动执行下一点，也不证明绑定中报告的硬件实际加载；真实catalog CLI/硬件流程、Windows Qt5.14.2、macOS及现场仍待验收。

## 71.313 扫描点收尾阶段的journal推进（2026-10-23）

完成71.312后，逐点journal此前在Run Stop后一直停留于`RunStopped`。现在事件目标Stop得到回执和新鲜ECC状态确认后，系统先保存绑定已确认Stop operation ID的`receiver-finish`意图，再发送整系统Breakup；新鲜状态必须确认所有接收进程正常退出且seal有效，才把journal推进到`ReceiverFinished`。之后先保存与即将提交的catalog ECC请求字节、operation ID完全一致的`catalog-verify`意图，再提交唯一catalog操作。只有新鲜ECC状态带有catalog receipt、且receipt链接文件数与receiver seal统计一致时，journal才推进到`CatalogVerified`。Breakup/catalog失败或超时、receipt缺失或不一致都会保留待对账intent，不生成新的operation ID重发。

成功路径完成Run记录后释放journal独占锁；71.314随后将当前 `CatalogVerified` 点的catalog绑定意图与结果写回journal并推进到`Bound`。仍不会自动退休ECC或开始下一扫描点。单CoBo fake HTTP集成覆盖`describe, prepare, configure, start, stop, breakup, catalog`，并断言最终journal为`CatalogVerified`且没有待处理操作。Qt5.12.8/GCC9.3/C++11 Envoy完整目标和完整测试目标构建成功；定向扫描集成3/3、完整QtTest 126/126通过。本地假服务只交换小型状态/receipt元数据，没有生成或传输DAQ/raw数据。Windows Qt5.14.2/MinGW、macOS、真实ECC与现场数据吞吐尚未验证。

## 71.312 扫描事件目标后的receiver封存与ECC状态显示（2026-10-23）

本节记录71.313之前的中间实现；其中关于journal停留在`RunStopped`的描述已由71.313取代。

单点扫描Run达到必填事件目标并且整套ECC Stop经回执及新鲜状态确认后，NativeRunControl现在继续走普通Run收尾：Breakup、等待receiver退出/EOF与seal证据、在`native_catalog=true`时核验并归档catalog，最后完成Run记录。扫描Run按钮仅在启用`native_catalog`时开放，避免成功Stop后缺少本点归档。若Run记录完成，界面提示接收端封存/catalog和Run归档已经完成；归档凭据仍未写回扫描journal，journal保持`RunStopped`，不自动推进下一点。真实ECC/receiver、配置工件切换、跨进程Run恢复及多点编排仍未实现。

集成夹具严格模拟单CoBo控制器Idle→Described→Prepared→Ready→Running→Ready→Prepared，并在Configure时才产生已绑定的receiver会话。事件目标后命令顺序断言为`describe, prepare, configure, start, stop, breakup, catalog`；测试同时确认receiver退出/封存、catalog receipt、Run记录`Complete`及扫描journal仍为`RunStopped`。`nativeScanRunFinishesReceiverAndCatalogAfterEventTarget`在Qt5.12.8/GCC9.3/C++11完整`tests/tests.pro`目标中通过，3/3（含init/cleanup）。该测试仅交换小型控制/receiver元数据，不启动receiver、不生成raw或传输数据。完整`apps/envoy/envoy.pro`同版Qt构建成功，`bin/GetDAQ`已更新。

GUI的Transition列从数字改为`Describing`、`Preparing`、`Configuring`、`Starting`、`Stopping`、`Pausing`、`Resuming`、`Breaking up`及`Resetting`等状态机动作名称。对应关系核对旧GetBench `src/get/rc/SM.h`的1–9枚举及`BackEnd.cpp::createEngine()`；原程序维护的是整套ECC状态机，Active内含Ready/Running/Paused。当前单CoBo快捷入口同样只适用于唯一活动CoBo且没有MuTaNT的固定部署，并转发到整套ECC状态机，不代表多板逐板状态机。未连接实机；Windows Qt5.14.2、macOS及现场验收未在本轮执行。

## 71.311 扫描原ID查询故障的有限联锁恢复（2026-10-23）

## 71.311 扫描原ID查询故障的有限联锁恢复（2026-10-23）

服务对生命周期operation有效返回`phase=unknown`时，Native客户端会建立扫描观察联锁；普通HTTP网络错误不直接设置此客户端标志，pending/reconciliation仍由ECC客户端记录库保持。恢复只允许同一Describe/Prepare/Configure operation ID得到与请求、目标状态完全相符的complete结果，并由新鲜status再次确认同一ID/目标状态；告警健康且联锁代次未变化后，才分别清除客户端短暂保持和该ID在`EccClientStore.heldOperations`中的记录。并发告警或其他保持不会被清除。

验证：新增默认关闭的进程内HTTP Transport，使回归不需要loopback监听。Qt5.12.8/GCC9.3/C++11下完整`apps/envoy/envoy.pro`、完整`tests/tests.pro`与独立`tests/ecc_system_client.pro`从源码构建成功。三项ECC联锁回归共5 passed、0 failed；`nativeGuiMetadataAndWholeSystemControls`、`offlineScanPlanBuilderIsAvailableFromConsole`与`nativeSingleCoBoLifecycleUsesWholeEccTransitions`共5 passed、0 failed；文档链接检查1728项通过。使用`QT_QPA_PLATFORM=offscreen`运行GUI测试；离屏插件关于raise/size-hint的提示不影响断言。未访问ECC、receiver、板卡、DAQ或raw数据；临时构建目录已清理。

## 71.310 扫描生命周期intent重启后的原ID只读恢复（2026-10-23）

扫描journal现在公开未决原请求SHA-256；ECC本机客户端记录存储增加按operation ID读取原始请求的只读接口。Envoy恢复入口仅允许Describe/Prepare/Configure。它校验journal的operation ID/命令/request SHA与ECC原请求完全匹配，再GET原operation ID；只接受精确原请求、`complete`终态、预期whole-system目标状态及原operation ID。随后必须新发状态查询并确认状态仍是目标态、operation ID仍匹配，才写入schema 6的`operation-observed-complete`事件并恢复扫描journal。该路径不重发控制命令。重启后结果未知、GET失败、返回failed、状态变化、其他Native intent未决或身份范围不一致都保持隔离；Run/Stop不使用该恢复方式。

在POSIX C++11 `ecc-journal-tests`中新增回归：journal含未完成Describe intent时重开进入NeedsReconciliation，同时保留原request SHA；`CompleteObserved`把精确pending操作通过schema-6事件恢复到Described并能再次重开；非Describe/Prepare/Configure命令拒绝该自动恢复。测试通过。Qt 5.12.8/GCC 9.3下完整`apps/envoy/envoy.pro`和`tests/tests.pro`均重新编译链接；`nativeGuiMetadataAndWholeSystemControls`、`offlineScanPlanBuilderIsAvailableFromConsole`各通过，含init/cleanup共4/4个QtTest槽。Linux `bin/GetDAQ`已由本次源码重新生成并保留供检查；本轮CMake/qmake构建目录位于`/tmp`且已删除。

本轮没有运行fake HTTP恢复链，因此目前验证覆盖journal逻辑及Qt编译/离线GUI，不证明GET响应在服务端如何表示已完成操作，也不证明新鲜状态读回在现场可用。没有连接ECC、receiver、硬件或DAQ，也没有创建raw/传输测试文件。Windows Qt5.14.2/MinGW、macOS和现场均未验证。

## 71.309 主窗口单CoBo快捷入口视觉回归（2026-10-23）

为真正唯一CoBo且无MuTaNT的Native部署，行内Progress/Regress继续驱动整套ECC状态机，但现在使用区分明显的主操作/回退色，并提供辅助技术范围描述。多板、目标不匹配或有MuTaNT时，入口仍是Prepared-only维护样式，逐板Regress保持隐藏。新增回归断言覆盖快捷入口的scope属性和无障碍描述。

Qt5.12.8/GCC9.3/C++11下，完整`tests/tests.pro` Widgets目标于`/tmp/getdaq-gui-visual-tests`编译链接成功；`nativeSingleCoBoLifecycleUsesWholeEccTransitions`与`nativeGuiMetadataAndWholeSystemControls`均通过，2个定向用例槽及各自init/cleanup通过。完整`apps/envoy/envoy.pro`也在独立目录重新编译，生成程序更新到`bin/GetDAQ`；隔离数据目录下启动该程序的1440×960无连接截图成功。离屏Qt插件报告不支持raise和size-hint，该提示不影响测试。构建、测试、截图和用户数据临时目录已清理，无数据传输或DAQ文件生成；未连接ECC、receiver、板卡。Windows Qt5.14.2/MinGW、macOS及硬件未验证。

## 71.308 Native扫描点单步生命周期操作（2026-10-23）

Envoy扫描窗口新增单次全系统Describe/Prepare/Configure入口。动作只在journal状态与当前操作匹配时启用；每次操作前必须重验ECC身份，持久化操作意图后提交命令，并在取得成功结果后请求新ECC状态。只有回读状态与操作结果一致才将journal推进到下一扫描阶段。异常结果要求检查journal，禁止自动重发。操作作用于整个Native ECC部署；Configure使用已经固定在ECC中的工件，不会读取或切换到点JSON，因此操作员仍须事先确认ECC工件就是当前扫描点。

新增`nativeScanRunStopsAtEventTargetWithoutFinalizingReceiverSession`覆盖路径被扩展为从Idle开始依次验证`adopt`、Describe、Prepare、Configure、Run和事件目标Stop，并检查五个系统命令的顺序与最终`RunStopped` journal；完整QtTest目标编译成功。Linux Qt5.12.8/GCC9.3严格C++11 Envoy完整目标及`tests/tests.pro`测试目标均构建成功；可在无TCP server的单元测试中运行的扫描journal、离线计划、单CoBo维护及断连Run/基线门槛专项9个测试槽通过，连同QtTest init/cleanup共11/11。

新fake HTTP端到端用例未能执行：运行环境禁止创建loopback监听socket，独立Python bind探针返回`PermissionError: [Errno 1] Operation not permitted`，Qt fixture也因此无法启动。未绕过沙箱。没有访问ECC/硬件/receiver或传输raw；未生成扫描DAQ数据。构建/测试中间目录在验证后清理，保留的`bin/GetDAQ`为更新后的产品构建。Windows Qt5.14.2/MinGW、macOS、真实ECC和现场硬件仍待验证。

## 71.307 Envoy主窗口轻量视觉统一（2026-10-23）

主窗口普通标签统一为蓝灰色；当Connect/Start run/Stop run被禁用时统一采用中性禁用底色、边框和文字，避免禁用操作仍显示蓝/绿/红强调色。该修改只涉及QSS，不改变按钮门槛或Native/Legacy状态机行为。

Linux Qt5.12.8/GCC9.3/C++11完整`apps/envoy/envoy.pro`目标编译链接成功。使用独立`XDG_DATA_HOME`启动`bin/GetDAQ --screenshot`捕获1440×960离屏界面，并人工检查字段标签与禁用按钮的结果；临时截图和应用数据已删除。此运行不连接ECC/receiver/板卡、不产生DAQ文件。构建显示既存的`-Wmisleading-indentation`警告。Windows Qt5.14.2/MinGW与macOS未在此处验证。

## 71.306 Native扫描Run事件Stop fake HTTP成功链（2026-10-23）

新增本机HTTP fake ECC服务端到端测试：为已绑定身份和事件基线的`Configured`点发出单次Run，读取当前CoBo receiver事件数，在达到策略目标后提交单次Stop，并验证新的控制器状态读回和journal推进。该测试还捕获并修复了扫描Stop成功后错误落入普通Run结束分支、继而发送Breakup的控制流问题；现在扫描Stop结束于`RunStopped`，receiver生命周期会话保持原状，不会执行普通Run的Breakup/Finish/seal/catalog。

Qt5.12.8/GCC9.3严格C++11下，`tests/tests.pro`完整Qt Widgets测试目标重新构建成功。成功链及扫描journal、离线扫描GUI入口、Native系统控制、单CoBo生命周期、断连事件基线/Run门槛的7个定向测试通过；计入QtTest init/cleanup共9/9。fake服务只在localhost提供小型JSON控制元数据，没有启动ECC或receiver进程、连接硬件、写DAQ文件或传输raw。测试构建目录位于`/tmp/getdaq-native-scan-e2e`，验证后删除。

此回归证明当前Linux fake服务控制链和Stop后不误发Breakup，不证明真实ECC API、板端状态、receiver进程实际生命周期、数据吞吐、Windows Qt5.14.2/MinGW、macOS或现场运行。完整逐点Configure、多点推进、receiver Finish/seal/catalog和故障恢复仍待实现与验证。

## 71.305 扫描Run入口范围提示与文档对齐（2026-10-23）

扫描窗口将受限入口标为“Run the currently configured point”，新增突出提示：当前点必须已在ECC完整Configure，实时身份需精确匹配该点工件；动作控制整套Native ECC系统，不加载点配置、不推进下一点、不完成receiver Finish/seal/catalog。确认框同步说明该边界。Threshold仍禁用自动Run。回归断言提示、按钮标题和tooltip均保留这些关键信息。

文档复核发现架构第5节仍写事件查询未接Run，而71.304已修复事件轮询；已改为当前实现并写明查询身份、receiver session/PID和每轮新鲜观察要求。扫描操作手册同步区分单点Run与尚未完成的多点执行。

Linux Qt5.12.8/GCC9.3严格C++11下，`apps/envoy/envoy.pro`主程序、`apps/configtool/configtool.pro`和完整`tests/tests.pro`目标均从干净的`/tmp`影子构建目录编译链接成功。定向运行`offlineScanPlanBuilderIsAvailableFromConsole`为3/3通过（含init/cleanup），覆盖新的范围说明、按钮和tooltip。Markdown本地链接检查1726项通过。构建和测试目录均位于`/tmp/getdaq-gui-scope-71-305`，总计约51 MiB；本轮使用小型GUI/配置测试数据，没有连接ECC、receiver或板卡，也没有生成DAQ/raw传输文件。Windows Qt5.14.2/MinGW、macOS、物理设备和完整逐点扫描流程仍未验证。

## 71.304 Native扫描事件计数轮询优先级修复（2026-10-23）

复核NativeRunControl::Poll发现常规`QueryStatus()`先于扫描`QueryReceiverEvents()`启动。由于ECC客户端将状态读取也纳入`Busy()`，扫描查询分支在每轮都被跳过。调整后，只有在Native仍为Running、状态和receiver观察都新鲜、监测器已绑定且客户端空闲时，先发receiver事件请求并结束本轮；事件未达目标后才启动下一组状态/receiver刷新，形成事件样本之间的状态边界。事件达标/超时/身份不符仍复用原Stop/未知journal处理。

Linux Qt5.12.8/GCC9.3/C++11完整`apps/envoy/envoy.pro`构建成功，完整`tests/tests.pro`构建链接成功；QtTest定向执行`scanExecutionJournalRoundTripQuarantineAndReconciliation`、`nativeGuiMetadataAndWholeSystemControls`、`nativeSingleCoBoLifecycleUsesWholeEccTransitions`、`scanEventBaselineRequiresConnectedNativeController`及`scanRunStartRequiresConnectedNativeController`，7 passed、0 failed、0 skipped（含init/cleanup）。本轮没有假ECC网络Run→事件→Stop成功路径测试，所以轮询修复只获得编译和相邻控制/journal回归支持，不能视为集成成功证据。无真实服务、receiver、硬件、DAQ或raw文件。

## 71.303 扫描点Run入口门槛和C++11构建（2026-10-23）

检查并实现Envoy扫描点的单次整系统Run控制：GUI在点已Configured、事件策略/运行身份/基线完整绑定且journal没有未决操作时才开放Run入口；Threshold计划不开放自动Run。NativeRunControl先持久化Run意图，再提交命令，等Native操作完成和新鲜状态读回后才开始轮询当前CoBo的receiver事件计数。事件身份还要与receiver生命周期session和运行PID一致；事件数达标后持久化Stop意图、只提交一次整系统Stop，只有成功结果及状态读回通过才推进`RunStopped`。超时或观察异常属于中止并保留未决操作，不记为扫描点成功。

Linux Qt5.12.8/GCC 9.3/C++11完整`apps/envoy/envoy.pro`目标构建链接成功；完整`tests/tests.pro` QtTest目标构建链接成功。新增及相邻定向测试运行：离线扫描窗口入口门槛、断连Run拒绝、断连事件基线拒绝、扫描journal恢复共6 passed、0 failed、0 skipped（含init/cleanup）。构建中有既有`-Wmisleading-indentation`警告。一个首次测试调用使用了错误的QtTest函数名，未运行测试；之后按`-functions`列出的真实名称重跑成功。

独立CMake C++11实验核心全量CTest 15/15通过，包含`scan-event-count-monitor`和`scan-execution-state`。此构建不访问DAQ或硬件。

该轮只运行本机Qt Widgets和小型元数据测试，没有网络ECC/receiver/硬件/DAQ访问，没有生成或传输raw；临时测试构建树位于`/tmp/getdaq-scan-test-build`，验证后清理。没有执行真实跨主机传输。完整逐点ECC准备、receiver Finish/seal/catalog、失败/崩溃后恢复及多点推进仍未完成；Windows Qt5.14.2、macOS、独立Ubuntu24和现场均未验收。

## 71.302 已绑定扫描点跨生命周期重验与状态机对照（2026-10-23）

此前身份响应编码器对每次查询都套用首次接纳的Idle/fresh条件，而journal协调器已允许在Described、Prepared、Configured恢复重验，二者契约不一致。请求/响应协议现区分`adopt`和`revalidate`，明确携带期望/观察到的ECC控制状态；旧schema 1仍解释为Idle adoption。恢复重验验证冻结部署身份与journal生命周期状态相同；首次接纳依旧要求干净Idle。运行身份摘要排除动态控制状态、观测时间、session及PID，避免正常状态迁移导致摘要变化；Configured阶段仍逐一验证Ready receiver session及对应CoBo PID，进程清单按照ID匹配。

`ecc-journal-tests`加入实际的Idle接纳、EccStarted重验和Configured/Ready重验响应，包含session/PID与receiver集合校验并确认重验不改写journal；`scan-runtime-identity-tests`覆盖稳定摘要、状态不符拒绝和协议往返。Linux GCC严格C++11构建及两项测试通过。本次没有连接ECC/receiver、发送网络请求、运行板卡或DAQ，也没有创建raw文件。

源码对照结论见[71.291](#71291-envoy主界面分区与单cobo生命周期回归2026-10-23)和[ECC状态归属说明](ECC_STATE_OWNERSHIP.md)：旧`RunControl::ProgressSlot(id)`的单CoBo进度路径属于多ECC连接的兼容模式；当前Native ECC是一个绑定固定部署的系统级状态机，单CoBo动作是Prepared下的维护操作。Native模式不应把系统整体Prepare/Configure/Run伪装成可独立改变单板状态。扫描ECC命令执行、逐点Run/自动Stop及receiver/catalog闭环仍未实现，Threshold继续手动。

## 71.299 扫描运行身份绑定后的只读恢复重验（2026-10-23）

之前 NativeRunControl 只允许扫描journal首次写入ECC身份摘要；应用在绑定后重启时，无法重建内存中的point deployment期望，后续即使journal处于Configured也不能安全重新查询receiver事件基线。`ScanRuntimeIdentityCoordinator`现在提供显式revalidation模式：只允许当前点处于EccStarted、Described、Prepared或Configured、无待处理/未知操作且已有身份摘要时查询；返回响应必须通过原冻结点期望验证，重新计算的运行身份摘要必须与journal完全相同。成功只返回journal原值，不追加事件、不覆盖摘要；不匹配的ECC身份拒绝。

`ecc-journal-tests`覆盖首次身份绑定、EccStarted状态重启重验、错误实例拒绝且journal字节不变，以及点已Configured后重验成功且journal逐字节不变。Linux CMake C++11目标构建并运行通过；Qt5.12.8 qmake单独编译NativeRunControl与两个扫描协调器源文件成功。只验证了底层协调器和编译接线，未模拟NativeRunControl HTTP回包/GUI信号的完整恢复流程；没有ECC、receiver、硬件或DAQ raw操作。NativeRunControl仍有既存`-Wmisleading-indentation`警告。

边界：GUI仍未提供身份重验/基线查询操作入口；逐点服务部署、自动Run/事件计数Stop、receiver Finish/seal/catalog及故障恢复尚未闭环。Threshold仍不自动Run。

## 71.298 事件基线绑定当前receiver生命周期（2026-10-23）

`ScanEventCountBaselineCoordinator::BindResponse`现在同时接收只读事件计数响应和当前receiver观察，并在写journal前集中校验固定ECC实例/snapshot/data binding、唯一匹配的agent/receiver实例/plan、节点可用且空闲、带时间戳的`ready`生命周期及一致session，以及选中CoBo仍处于running并具有同一PID。NativeRunControl继续负责查询前后的ECC Ready状态及观察新鲜度，重复的response身份/session/PID判断从Qt控制层移入Qt-free协调器，避免两处实现逐渐不一致。

`ecc-journal-tests`新增有效生命周期正例和错误session、PID不符、CoBo已停止、部署data binding不符等拒绝用例；拒绝后逐字节确认journal没有变化，正例仍精确保留`9007199254740993`。Linux CMake C++11 journal目标构建成功并运行通过；qmake/Qt5.12.8也单独编译了`nativeruncontrol.cpp`与协调器源文件。编译没有错误，NativeRunControl既有代码仍报告多处`-Wmisleading-indentation`警告。测试不连接真实ECC/receiver，不启动DAQ，不产生raw。

边界：没有测试完整fake ECC HTTP到NativeRunControl信号再到GUI/journal的集成成功路径；离线扫描窗口尚未接入自动逐点Run/Stop、事件监测、receiver Finish/seal/catalog或失败恢复闭环。Threshold仍不得自动Run。

## 71.297 ECC客户端receiver事件查询合同测试（2026-10-23）

为补上71.296 Native事件基线桥未测试HTTP成功响应的缺口，`tests/test_ecc_system_client.cpp`新增fake ECC endpoint回归：客户端必须先观察匹配身份；随后将source `C0`编码为只读路径`/v1/receiver-events/4330`，阻止并发重复查询，解码并原样保留`9007199254740993`事件计数及PID；响应source与请求不同时，撤回响应body并返回错误。测试还确认此操作不发送任何控制POST。

Linux Qt5.12.8/GCC9.3/C++11的独立ECC system client QtTest目标构建成功；新增fake endpoint用例3/3（含init/cleanup）通过，完整ECC system client回归66/66通过。默认沙箱运行不能绑定loopback，首次只在fixture bind阶段失败；随后在本机回环权限下运行，测试通过。构建仅用小型JSON元数据和本机TCP，不接外网、真实ECC、receiver、硬件或DAQ raw；临时构建目录和日志已清理。本记录不代表NativeRunControl成功路径已集成联调。

边界：事件查询响应到达NativeRunControl后的journal绑定只覆盖断连拒绝，成功交叉核验仍需fake ECC集成测试；GUI也尚未调用逐点执行路径。自动逐点Run/Stop、receiver Finish/seal/catalog和恢复闭环未完成。

## 71.296 NativeRunControl只读查询并绑定扫描事件基线（2026-10-23）

在71.295的Qt-free journal协调器之上，`NativeRunControl::QueryScanEventCountBaseline`现在会从控制线程调用`EccSystemClient::QueryReceiverEvents`，并由`RunControl::QueryScanEventCountBaselineSlot`转发。调用要求该扫描journal/点已成功绑定ECC运行身份、控制器空闲且处于新鲜匹配的Ready状态， receiver观察同属当前ECC部署；并验证冻结快照中的目标CoBo只映射到一个固定receiver agent，receiver plan/instance、session以及正在运行的CoBo PID均匹配。查询返回时再次检查身份、状态和receiver观察仍新鲜且Ready，再校验response中的source/agent/plan/session/PID；只有全部通过才让`ScanEventCountBaselineCoordinator`追加持久基线。断连、状态迁移、身份变化、receiver重启或任何查询失败都不产生基线。该接口只读，不改变ECC FSM，也不传输DAQ raw。

Linux Qt5.12.8/GCC9.3/C++11的完整`tests/tests.pro`目标重编译链接成功；专项离屏QtTest`scanEventBaselineRequiresConnectedNativeController`、`nativeSingleCoBoLifecycleUsesWholeEccTransitions`和`nativeGuiMetadataAndWholeSystemControls`共9 passed（含init/cleanup），0 failed。新增拒绝用例确认未连接时不打开/创建调用方journal。一次全量QtTest运行47 passed、76 failed：Native单CoBo状态与GUI控制用例通过；另外有本地监听被运行环境拒绝，以及配置工作区/文档打开用例失败，本轮没有查明后者的独立原因，不能将其计为全量通过或都归因于沙箱。测试只产生约24 MiB临时编译产物，构建目录与日志已删除；未启动receiver、硬件服务、网络DAQ或生成raw文件。

边界：专项用例只覆盖断连拒绝；本轮没有连接真实/模拟ECC HTTP服务去验证receiver事件查询的成功响应路径。MainWindow/离线`ScanPlanWindow`尚未调用该查询接口；扫描Run入口、每点事件轮询、阈值达成后持久Stop意图与ECC整系统Stop、receiver Finish/seal/catalog及恢复闭环仍未实现。Threshold仍不得自动Run。Windows Qt5.14.2、macOS、Ubuntu24独立主机和实板本轮未验证。

## 71.295 receiver事件基线协调器与主表交互层（2026-10-23）

新增`ScanEventCountBaselineCoordinator`，将receiver事件计数查询的journal边界收紧到：当前扫描点已完成Configure、没有待处理/未知操作、冻结执行快照与当前point一致、运行期ECC身份已绑定、且尚无事件基线。请求来源由冻结快照的目标CoBo决定。响应必须与ECC实例/部署snapshot/data binding、source以及固定agent/receiver实例/plan一致，并且事件计数可用；通过后才把receiver session、PID、uint64事件数和观察时间写入schema-4 journal。错来源时测试确认journal字节不变；正向用例保留大于2^53的uint64计数并确认基线持久化后禁止重复查询。

对照旧代码：`BackEnd::createEngine()`定义的是GET ECC整套部署生命周期FSM；`SystemManager`按MuTaNT/CoBo依赖执行节点步骤，`CoBoNode::impl_prepare/configure/start/stop`实施单板硬件流程，Configure阶段切换慢控、Start切换快控。不是每块CoBo各有独立ECC服务生命周期。Envoy Native ECC继续显示整系统状态；唯一CoBo按钮仍调用一板部署的整系统FSM；多板单独Run状态未引入。

界面额外启用状态表交替行底色、悬停和选中态，维持既有颜色状态标记与窄屏列宽策略，不影响状态刷新和命令。`apps/ecc/CMakeLists.txt`及Envoy qmake文件均已纳入协调器源码。

验证：Qt-free C++11配置与`ecc-journal-tests`构建成功，直接运行输出`PASS ECC and scan journals`；qmake完整`tests/tests.pro`目标构建链接成功；Qt离屏定向测试`nativeGuiMetadataAndWholeSystemControls`、`nativeSingleCoBoLifecycleUsesWholeEccTransitions`、`scanExecutionJournalRoundTripQuarantineAndReconciliation`及`guiLifecycleAndClose`共6 passed（含init/cleanup），0 failed、0 skipped。CMake生成的`ctest --test-dir`在当前环境错误定位到源码树且显示0项，因此直接执行生成的测试程序验证。未启动硬件服务、receiver、网络DAQ或生成raw文件。

边界：该协调器尚未接入NativeRunControl的`QueryReceiverEvents`调用，也没有逐点Start/Stop调用、停止后journal状态确认、receiver Finish/seal/catalog闭环。此提交不能启动自动扫描。Linux Qt验证版本为5.12.8；本轮未在Windows Qt5.14.2/macOS重建。

## 71.294 主状态表紧凑布局与旧版单节点控制语义复核（2026-10-23）

主窗口`StatusTable`采用明确列宽、交互式调整、像素滚动和不换行呈现；15列Native元数据不再按内容自动撑开整张表。长内容保存在原单元格并提供悬停全文，不改变状态采样、命令或接收数据通路。

源码核对：旧`BackEnd::createEngine()`的`SM_ECC`是系统FSM。`SystemManager::prepare/configure/start/stop()`通过`Node::List::doLoop_*`逐组组织板卡操作；`Node::List::doLoop(InParallel)`会并行调用每个Node对象并汇总`CmdException`，没有自动回滚成功的其它板。`CoBoNode::impl_start()`清FIFO/读指针、先启动data sender、再启用固件与中断并切换Fast Control；`impl_stop()`停周期pulser、读温度、禁用固件和中断、要求sender停止/flush。SystemManager在有MuTaNT时先启动slave后master；Stop按源码注释先停master及slaves，再停CoBos。旧GetTest单CoBo窗口则对固定目标调用自己的load/configure/start/stop流程。这些是三种不同范围，不能只凭“逐行按钮”认定旧GetBench有并行的逐CoBo ECC状态机。

当前Native ECC仍由一个ECC服务管理整套固定部署。Native表的`System ECC`值会在数据源行重复显示，它是同一系统状态的视图；唯一CoBo快捷按钮仍调用整机FSM，多板快捷入口只打开已有Prepared-only维护。多板逐CoBo Configure/Start/Stop需要新的服务端状态、互斥、失败部分执行和数据接收生命周期协议，当前不提供。高吞吐raw数据不经过GUI。

验证：Linux/WSL Qt 5.12.8、GCC 9.3、C++11完整`tests/tests.pro`以qmake/make构建链接成功。离屏定向QtTest运行`nativeGuiMetadataAndWholeSystemControls`、`nativeSingleCoBoLifecycleUsesWholeEccTransitions`、`scanExecutionJournalRoundTripQuarantineAndReconciliation`，共5 passed（含init/cleanup），0 failed、0 skipped；新增断言确认列宽策略为Interactive、首列固定宽度低于200 px、长路径/错误原文保存在tooltip。`python3 tools/check-documentation-links.py`报告1722个本地Markdown链接通过。临时构建目录位于`/tmp/getdaq-gui-compact-71-294`，清理后再结束本节；未启动硬件服务或生成DAQ文件。

## 71.293 扫描journal持久化逐点事件基线（2026-10-23）

扫描执行journal新增schema 4的`event-count-baseline-bound`事件。该事件只能在点已Configure并且Runtime ECC identity完成绑定后追加，字段严格固定，包含ECC instance、deployment snapshot和data binding、agent/receiver plan、receiver实例、source、session/PID、计数可用性、累计事件数及观察时间；两个uint64字段使用规范十进制JSON字符串保存，保留超过2^53的精度。重放拒绝不可用计数、非法身份、重复绑定、错误生命周期位置、非规范/越界计数，以及没有事件基线的schema-4 `run-start` intent。点退休并进入下一点时runtime identity及当前receiver事件基线一起清除。schema 1–3既有日志保持兼容。

验证：`scan-execution-state-tests`覆盖schema-4绑定、十进制uint64回读、重复/不可用基线拒绝、没有基线的Run intent拒绝、Run intent后重启unknown quarantine、点间清除以及之前版本日志兼容。Qt-free实验核心完整构建，CTest **15/15 passed**；Qt5.12.8 qmake完整`tests/tests.pro`目标构建成功，定向`scanExecutionJournalRoundTripQuarantineAndReconciliation` **3/3 passed**。无ECC/receiver/DAQ数据访问或网络测试。

这使停止计数的运行依据可持久恢复，但尚未在`NativeRunControl`建立该绑定，也未接入`QueryReceiverEvents`轮询、一次性整系统Stop、停止结果复核及receiver结束/归档流程。不能把本条视为自动扫描闭环完成。

## 71.292 扫描事件数停止判定核心（2026-10-23）

新增纯C++11 `ScanEventCountMonitor`，输入是开始某点Run前记录的receiver事件累计值及每次新观察。比较前先核对ECC instance、deployment snapshot、data binding、agent、plan、receiver instance、source、session和PID；之后要求事件计数可用、`observedAtUnixMs`严格递增且累计事件不回退，再计算相对基线的增量。目标计数达到后返回`StopAtEventCount`；超出最大运行时间先返回不同的`StopAtMaximumDuration`并说明这是中止而非成功；身份、计数或新鲜度异常都返回`InspectRequired`。判定器自身不执行网络、硬件命令、journal写入或状态迁移。

验证：`scan-event-count-monitor-tests`独立覆盖目标数、超时中止、receiver session更换、不可用计数、累计计数回退、过期观察及终态重复观察；实验核心CMake目标构建、`system-experiment-tests`通过；Qt 5.12.8 qmake完整测试目标构建通过，扫描journal/计划校验/主界面/单CoBo整系统状态专项QtTest 6/6通过。此测试不启动ECC或receiver，不传输DAQ数据，也不生成raw数据。

尚未完成自动扫描的停止闭环。现有调用方没有在扫描点Run开始时捕获并持久化事件基线，因此不能安全地让该组件发出Stop。后续需要实现扫描点控制入口，将冻结snapshot/policy、journal intent、运行身份及基线绑定，并将达标决定接入一次性整系统Stop及远端结果观察；超时应保留为中断点，不能自动前进。Threshold继续手动采集。

## 71.291 Envoy主界面分区与单CoBo生命周期回归（2026-10-23）

主窗口把原本铺满顶层菜单栏的低频配置、工具、Run和故障恢复入口收纳到四个菜单；顶部增加系统控制说明、模式/连接状态卡片，并把配置、Run元数据和采集操作分成独立区域。布局在1440×960下进行Qt离屏截图检查，状态表、运行图和日志/参数区均可见；未更改数据接收线程或原始数据路径。

对照旧源码确认：旧GetController的GetTest窗口驱动一个TestController/目标CoBo，旧GetBench的`SM_ECC`管理由SystemManager执行的整套ECC生命周期。Native Envoy继续按整系统状态驱动；只有服务端固定目标恰好一块CoBo、无MuTaNT且与配置来源匹配时，行内快捷键才调用整套ECC状态机。多CoBo行内操作仍为受Prepared前提限制的单板维护，不把重复的系统状态伪装成独立CoBo状态。

Linux Qt5.12.8/GCC 9.3/C++11完整Envoy Widgets和完整QtTest目标均从当前源码构建成功。离屏执行`nativeGuiMetadataAndWholeSystemControls`与`nativeSingleCoBoLifecycleUsesWholeEccTransitions`共 **4 passed、0 failed、0 skipped**，覆盖四组菜单、信息卡片、Native整系统提示及唯一CoBo/多CoBo/MuTaNT/目标不匹配分支。Qt offscreen的`raise`/`propagateSizeHints`警告不影响结果；默认启动截图产生的测试日志与图像及临时构建位于`/tmp/getdaq-envoy-gui-refresh-20261023`和`/tmp/getdaq-envoy-gui-tests-20261023`，均在本条验证结束后删除。没有连接硬件/远端ECC、启动receiver或生成DAQ数据。

本轮未重验Windows Qt5.14.2、macOS和独立Ubuntu24，也未改动状态机协议。逐点事件数自动停止仍未实现；见71.290后续待办。

## 71.290 事件数查询向 ECC 与 Envoy 转发（2026-10-23）

单源事件计数现在从 `RemoteReceiverManager` 转发至ECC控制服务。父端要求当前remote plan中source唯一映射到一个agent和监督器索引，并将结果绑定当前ECC instance/snapshot/data binding、agent/plan、receiver instance/session/PID；失配、服务错误或busy均失败关闭。ECC服务仅接受规范小写hex UTF-8 source ID，通过认证GET和后台线程执行查询。Envoy `EccSystemClient::QueryReceiverEvents`增加相同认证路径，并核对ECC部署身份及请求来源。共享控制协议将计数和观察时间编码为十进制字符串，避免JSON double丢失64位精度。

严格C++11构建成功：重建`native-ecc-remote-receiver-tests`、ECC Qt服务、Supervisor Qt服务及receiver；完整Envoy工程通过Qt5.12.8/GCC构建。`ecc-protocol-tests`通过，覆盖大于2^53的事件数精度、十进制编码强制、不可用状态约束及schema拒绝。授权本机loopback的`observe-events`远程receiver场景通过：两个真实Supervisor service进程对应两个接收器，C0事件数从0增至2且仍绑定原session/PID；查询在发送数据前验证不改变ECC状态或板端动作。原始合成数据共约16 KiB，小于20 GiB。本轮失败尝试与成功测试的`/tmp/getdaq-observe-events-20261023`、Envoy Qt构建目录均在验证后清理。

旧`get_software_doc_201312(1).pdf`说明`CoBo.Module.nbEvent`为GetController达到后停止的事件目标；receiver计数按旧GetTest对MFM type >2拓扑帧的过滤计数。当前只完成计数读取链，不会自动停Run：逐点策略/journal、基线捕获、定时轮询、Stop去重和停止后状态验证还没接入NativeRunControl。也没有真实ECC设备、现场DAQ负载或持续吞吐验收；Windows Qt5.14.2、macOS和Ubuntu24本轮未重验。

## 71.289 单接收器事件数沿本机监督控制口观察（2026-10-23）

在71.288的receiver计数基础上，`DataSupervisor::ObserveAcquisitionEvents(sourceIndex)`增加按固定计划索引查询单个接收器的有界路径：只在监督器操作锁可立即取得、该子进程仍被确认运行时读取其本机 `/status`，并复用现有PID/session/source/config/artifact/目录/运行参数身份校验。frame-storage返回旧循环兼容的`acquisition_events`；byte-storage明确返回`available=false`。该查询不会轮询所有CoBo、读取raw文件、持有等待全计划状态端口或改变receiver/ECC生命周期。

监督服务新增认证的`GET /v1/receiver-events/SOURCE_INDEX`，通过QtConcurrent后台执行一次单源查询，固定响应绑定agent/controller/instance/plan及source/session/PID和观察时间；非规范索引返回400、计划外索引404、未认证请求401。它是数据监督服务的一跳控制元数据接口，尚未穿过ECC RemoteReceiverManager/ECC服务/Envoy，也未接入逐点扫描Stop协调器，不能据此称事件数自动停Run已完成。

严格C++11/pedantic-errors下重建receiver、监督核心、查询客户端和Qt监督服务成功。经授权本机loopback执行的`data-supervisor-process-lifecycle`和`supervisor-control-service`均通过；前者覆盖21个生命周期场景，其中新增observe-clean/error/signal/lost/byte-storage，后者覆盖认证HTTP服务、4个receiver观察场景和新事件端点。接口回归验证运行初值0、发送完整type-1帧后观察到1、身份对应当前session/PID、ByteStorage不可用标志及越界拒绝。测试最大合成raw总量分别约0.62 MiB和0.33 MiB，均低于20 GiB；测试输出及raw随后已从临时目录删除。无真实硬件或吞吐结论；Windows Qt5.14.2、macOS和Ubuntu24本轮未重验。

## 71.288 接收端逐点事件计数观察基础（2026-10-23）

旧`GetTest::onDataFrameReceived`跳过MFM frame type大于2的拓扑帧，再增加`nbEvent`。新`getdaq-receiver`现保留原始`frames`总帧数，并在frame-storage status新增`acquisition_event_count_available`和`acquisition_events`；只把type <= 2计为兼容旧循环的acquisition event。ByteStorage不解析MFM，status明确报告计数不可用。receiver集成夹具以type 1、2、3混合输入验证总帧数与可计事件数不同；ByteStorage集成验证不可用标记。此数据仍只是观察值，不会自动停止ECC Run；没有改变传输、文件字节或封存语义。

验证使用严格C++11独立构建和本机loopback receiver集成；集成最大合成raw数据32 MiB（低于20 GiB），由测试临时目录自动删除。该结果不验证真实板卡事件语义、自动Run停止、Windows/macOS/Ubuntu24部署或持续吞吐。

## 71.287 单CoBo固定部署的生命周期快捷入口（2026-10-23）

对照旧`GetController-release-20210208`的`GetTest::loadHw/configure/startDaq/stopDaq/Reset`及`Ctrl::State`，确认它是在一个`TestController`/目标端点上驱动`Off/Idle/Loaded/Configured/Running`，`TestController::configure()`以CoBo索引0配置所选板内活动AsAd/AGET。旧GetBench `BackEnd::createEngine()`的ECC状态则是系统级FSM；`onConfigure/onStart/onStop/onBreakup`都委托`SystemManager`。所以不能在多板native部署中为某行伪造独立ECC状态。

Envoy连接native ECC后现在并行发起认证的只读固定目标查询。仅当服务端活动目标数量为1、该ID与唯一配置数据源`nativeSenderId`匹配且Run配置不含MuTaNT时，选中行显示“Progress single-CoBo system / Regress single-CoBo system”，将动作路由至现有整系统Progress/Regress状态机。系统仍使用原有鲜活状态、忙碌、Run与UNKNOWN门槛；菜单的全系统按钮和服务端状态机不变。其余部署仍显示Prepared-only单板维护入口，不增加多板逐板独立生命周期。

Qt5.12.8/GCC9.3严格C++11下，从当前源码全量构建`tests/tests.pro`的`getdaq-tests`成功。定向运行`nativeSingleCoBoLifecycleUsesWholeEccTransitions`和`nativeGuiMetadataAndWholeSystemControls`为4 passed、0 failed、0 skipped；用例覆盖单一目标按钮到整系统信号的映射，以及多目标、存在MuTaNT、配置数据源不匹配时回退原单板维护入口。没有启动ECC服务、发送网络请求或访问硬件；没有生成DAQ/raw文件。完整QtTest、Windows Qt5.14.2和macOS本轮未重验。

此GUI路径将真实服务端固定目标查询作为开关依据，仍需Windows/macOS构建与真实单CoBo部署的人工操作验收；它不是多CoBo并行独立FSM，也不改变多板控制语义。

## 71.286 ECC与扫描Qt-free核心全量合同回归（2026-10-23）

使用当前`apps/ecc` CMake构建目录完整增量构建ECC状态/journal、运行身份协调、单次操作runner、实验配置和控制协议核心目标；CTest共19/19通过。`ecc-journal`包含持久intent、外部动作单次调用、未知结果重启隔离、文件篡改与链接拒绝；扫描身份/状态协议及全部硬件编译子核心均通过。无ECC服务、板端或receiver启动，无DAQ raw或数据传输。测试目录为既有`build-ecc`，本轮未创建大型构建/数据目录。

边界：此回归只证明核心元数据和状态转换契约。`ScanOperationRunner`尚未接到真实逐点部署器；每点固定ECC配置/远程receiver计划生成、服务启动、事件数控制、Stop/Finish/seal与catalog绑定闭环仍缺失。Windows/macOS及独立Ubuntu24本轮未重验。

## 71.285 CoBo单板维护窗口视觉整理（2026-10-23）

Linux Qt5.12.8/GCC9.3/C++11完整Envoy Widgets目标编译成功；完整`tests/tests.pro`目标在`/tmp`独立构建成功。离屏运行4个CoBo维护GUI用例，6/6通过（含init/cleanup），覆盖选定目标、默认取消、单次提交、LED命令参数、Prepared前提及外来身份拒绝。Qt offscreen插件的`propagateSizeHints`提示不影响断言。临时构建目录及输出已清理；未连接网络/硬件、未产生raw或传输测试文件。Windows Qt5.14.2、macOS及Ubuntu24独立构建未在本轮重验。

## 71.284 扫描计划窗口视觉与窄屏适配（2026-10-23）

`ScanPlanWindow`采用统一浅色样式，加入页面标题和离线行为说明、清晰的控件焦点/主要操作层级、结果状态卡片，以及可滚动内容页；窗口最小尺寸设为760×600。既有离线语义不变：事件数必须由操作者填写，Threshold不开放自动Run。

Linux Qt5.12.8/GCC9.3/C++11完整Envoy Widgets目标重新构建并链接成功。完整`tests/tests.pro`目标也在独立临时目录构建成功；离屏运行`offlineScanPlanBuilderIsAvailableFromConsole`通过3/3（含init/cleanup），新增断言验证滚动区域、标题、说明和最小尺寸。首次测试误用了`bin/getdaq-config-check`旧二进制，缺少`scan-execution-policy`命令而失败；随后将`bin/getdaq-config`和`bin/getdaq-config-check`更新为已验证的71.276 Linux host候选包同版本工具，再运行专项测试通过。临时Qt构建目录及测试数据已删除；没有网络/硬件访问或DAQ/raw文件。

此轮未在Windows Qt5.14.2、macOS或Ubuntu24独立环境重新构建。

## 71.283 Envoy逐点ECC身份查询接入运行控制线程（2026-10-23）

`NativeRunControl::QueryScanRuntimeIdentity`和`RunControl::QueryScanRuntimeIdentitySlot`增加只读异步路径：限制于空闲native客户端；要求身份匹配且identity/status/receiver缓存均新鲜；重新读取并严格验证本地manifest、event-count策略和所有点工件；从配置客户端锚点及当前观察构造完整部署期望；打开调用方journal并通过`ScanRuntimeIdentityCoordinator`确认当前点已durably completed `start-ecc`后才查询。ECC返回的完整identity/status/receiver包由客户端期望核对，再由协调器重验journal并持久化身份摘要。成功或失败都通过结果signal返回；没有提交硬件命令或DAQ数据。

Linux Qt5.12.8/GCC9.3/C++11完整`tests/tests.pro` Widgets目标重建成功；`nativeGuiMetadataAndWholeSystemControls`与`scanExecutionJournalRoundTripQuarantineAndReconciliation`均通过，合计4/4（含init/cleanup）。严格C++11 `scan-runtime-identity-tests`与`ecc-journal-tests`二进制均通过。构建只在`/tmp`，执行后自动清理；没有网络、receiver、板卡或raw文件操作。`python3 tools/check-documentation-links.py`通过1710项本地Markdown链接检查。

边界：这是可调用的运行控制线程接口，当前`ScanPlanWindow`没有调用它；现有流程也无法建立实际ECC启动intent/完成事件。因此还没有能从GUI到达的逐点扫描执行器，也没有自动Describe/Prepare/Configure/Run/Stop、receiver封存、catalog绑定或完整恢复流程。Qt5.14.2、macOS、真实ECC和实机仍未验证。

## 71.282 旧GetController单CoBo状态语义对照（2026-10-23）

源码检查范围为旧`GetController-release-20210208`的`GetTest.cpp`及`TestController.cpp`。`Ctrl::State`属于窗口级控制状态；一个`TestController`使用一个`targetEndpoint`，并在配置中以CoBo索引0遍历该板AsAd/AGET。由此确认旧程序支持选定目标CoBo控制，但不能据此声称它实现了多板并行独立FSM。旧启动路径先显示Running再启动线程，停止路径在GUI线程等待结束；新系统保留服务端确认后更新状态的行为。

GUI测试`nativeGuiMetadataAndWholeSystemControls`新增ECC状态表头提示断言，确保native模式标明状态覆盖整套ECC。Linux Qt5.12.8/C++11完整Widgets目标重建成功，定向该用例3/3通过；`python3 tools/check-documentation-links.py`链接检查通过。测试只构造临时界面与内存状态，无网络/硬件操作、无raw文件或大文件输出；Windows Qt5.14.2和macOS未在本轮构建。

## 71.281 扫描运行身份协调入口

新增`ScanRuntimeIdentityCoordinator`，连接ECC运行身份查询与逐点journal：创建协调器及`PrepareQuery()`时，要求当前点状态为已持久完成`start-ecc`、没有pending/unknown操作、尚无身份绑定；并重新比较执行ID、快照摘要、计划摘要、点数、点索引和该点配置/工件摘要。查询请求从当前journal和冻结快照生成，不采用界面临时拼写的哈希。拿到完整ECC响应后，`BindResponse()`再次检查当前journal并复用严格响应包验证器持久化观察摘要。已绑定点不能重复查询/绑定。

严格C++11 `build-ecc`增量构建成功，`ctest -R '^ecc-journal$'`为1/1通过；测试覆盖未完成`start-ecc`时在构造协调器阶段拒绝请求、匹配快照构造正确查询、完整响应持久绑定及重复绑定拒绝。Linux Qt5.12.8/GCC9.3/C++11完整Widgets测试目标重建成功，`scanExecutionJournalRoundTripQuarantineAndReconciliation`为3 passed、0 failed（含init/cleanup）。验证仅用小型JSON元数据，没有DAQ raw、真实ECC/receiver网络或硬件操作。

该类是Envoy接线的可复用门槛，还没有由`NativeRunControl`调用；它也没有自动提交`start-ecc`、Describe/Prepare/Configure/Run/Stop、receiver Finish/seal或catalog校验。ECC部署期望的安全供给、按事件数停止以及崩溃后自动只读对账仍未完成。Windows Qt5.14.2和macOS本轮未测。

## 71.280 主窗口ECC状态视觉反馈（2026-10-23）

主窗口把ECC状态单元格呈现为可区分的状态标签：Ready/Running为绿色，Paused为蓝色，正在转移为琥珀色，离线/陈旧或未知状态为红色；悬停说明显示整系统状态及转移值。顶部阶段标签根据运行、忙碌和需要检查阶段切换颜色。按钮样式统一并加强Start/Stop层级。状态来源和命令作用范围未变：native ECC生命周期命令仍作用于整个系统，选中CoBo入口仍是既有Prepared-only维护流程。

从当前源码用Linux Qt5.12.8/GCC9.3/C++11重新构建完整`tests/tests.pro`目标成功；定向`nativeGuiMetadataAndWholeSystemControls`通过（QtTest 3 passed、0 failed，含init/cleanup），并断言Ready状态颜色/悬停文本及需要检查阶段颜色状态属性。离屏插件的`raise`和`propagateSizeHints`提示不影响用例。23 MiB临时构建目录及日志已删除；未产生DAQ raw、未连接硬件或数据接收端。Windows Qt5.14.2和macOS本轮未测。

## 71.278 完整扫描身份响应包持久化入口（2026-10-23）

`PersistScanRuntimeIdentityBinding`新增完整响应包重载：它先严格解码ECC `/v1/scan-runtime-identity`返回的identity/status/receivers与服务端expectation，再将expectation的规范编码同调用方基于冻结快照和部署输入建立的期望比较；完全一致后才继续执行既有快照/journal/receiver观察验证并持久化。这为Envoy接入异步客户端提供单一响应入口，避免调用方拆分响应字段后遗漏身份锚点比较；原观察字段重载继续保留给低层调用。

ECC journal回归以现有两点冻结快照构造完整协议响应包；对调用方期望中的客户端锚点不匹配予以拒绝且journal字节不变，匹配时身份摘要写入并经重开保留。严格C++11 `build-ecc` 下`ecc-journal-tests`增量构建成功，`ctest -R '^ecc-journal$'` 1/1通过。只生成小型JSON控制元数据，无DAQ raw、网络或硬件操作。Envoy尚未将其网络响应接入journal，也尚未实现逐点Configure/Run/receiver/catalog闭环；Windows、macOS和实机未测。

## 71.279 ECC客户端逐点身份期望回环验证（2026-10-23）

`EccSystemClient::QueryScanRuntimeIdentity`现在要求调用方同时提供从冻结快照/部署输入建立的完整期望。发送前核对请求的执行快照、点号、配置/工件摘要及固定ECC客户端身份；收到认证HTTP响应后严格解码完整ECC identity/status/receivers响应，并将服务端期望与调用方完整期望规范编码逐字段比较，任一身份、data scope/epoch或receiver agent/instance/plan不符均拒绝并锁定客户端。

独立Qt5.12.8/GCC9.3/C++11 `ecc-system-client-tests`新增本机HTTP回环用例：完整匹配响应通过；请求工件摘要与期望不符时在发出POST前拒绝；响应 internally consistent但data scope不等于客户端冻结期望时拒绝。完整ECC客户端网络回归 **65 passed、0 failed**。受限沙箱首次运行无法绑定loopback；随后在本机127.0.0.1回环权限下完成定向及全套测试。临时构建目录约23 MiB已删除，未触及raw数据、外部网络或硬件。

这仍只证明身份观察客户端门槛，未把成功响应写入执行journal，也未提供ECC实例启动、逐点Describe/Prepare/Configure/Run/Stop、receiver Finish或catalog封存执行器。Windows Qt5.14.2、macOS及真实ECC/receiver和实机扫描尚未验收。

## 71.277 GUI状态归属与运行身份HTTP观察（2026-10-23）

Envoy主窗口样式和选中CoBo入口完成当前源码Linux Qt 5.12.8/GCC9.3 C++11构建。离屏QtTest `nativeGuiMetadataAndWholeSystemControls`通过，新增检查整系统状态说明、样式应用、单板入口文案和native模式隐藏逐板Regress；CoBo维护窗口目标预选/默认Cancel和错误状态撤销测试通过，`singleCoboLifecycle`通过。运行输出为目标测试3/3 cases通过，无功能失败（offscreen插件报告不支持`propagateSizeHints`仅为平台提示）。

旧`BackEnd::createEngine()`的Idle/Described/Prepared/Active/Ready/Running/Paused转移及`onConfigure/onStart/onStop/onBreakup`委托范围已对照写入[状态归属说明](ECC_STATE_OWNERSHIP.md)。native菜单/行按钮均进入同一个Prepared-only单板维护流程；没有新造每板ECC生命周期。

扫描身份与ECC journal CTest **2/2通过**；journal绑定桥现在必须验证实时receiver observation，另加stale receiver拒绝和journal字节保持断言。完整Qt测试程序编译成功；`nativeGuiMetadataAndWholeSystemControls`、CoBo预选/取消/错误状态测试和`singleCoboLifecycle`均3/3 cases通过。固定ECC配置测试及实际`getdaq-ecc-server` HTTP进程边界脚本在本机回环模拟环境通过，覆盖认证、strict request、操作互斥及实例恢复边界。测试进程产生约3.9 MiB小型临时JSON/日志目录，结束后整体删除；没有`.graw`/raw文件，没有真实硬件或外部主机访问。运行回环进程需在网络沙箱外执行。`python3 tools/check-documentation-links.py`检查1704项Markdown链接通过。

本次没有刷新Windows/macOS运行包或独立Ubuntu24构建；样式及快捷入口仍需在用户Windows Qt5.14.2上人工查看。也没有新增设备吞吐或硬件验证结论。

## 71.276 扫描点身份跨进程契约与 Linux full 回归（2026-10-22）

扫描身份核心新增严格C++11 JSON契约：请求只带执行快照SHA、点索引、配置SHA和工件SHA；服务端返回的固定部署期望包含ECC service/client/instance、配置绑定、data scope/epoch及receiver节点计划。解码器拒绝未知字段、非法摘要、越界点号、重复或超限节点。Unix固定ECC身份构造器新增服务侧请求重载：仍从受保护的活动`FixedEccConfiguration`推导锚点，并拒绝请求点的配置/工件摘要与实际部署不符；旧快照入口转调同一逻辑。C++11身份协议测试与回环Ice模拟固定ECC集成均通过，覆盖请求/期望往返、错误摘要拒绝、错点/错客户端/错data scope及offline配置拒绝。

当前源码 Linux host full profile重建 **28/28程序**，109项嵌入依赖检查通过；严格C++11配置核心151/151、完整Widgets122/122、native CTest25/25，回环固定ECC集成通过。候选`build-packages/getdaq-linux-host-20261022-71-276-scan-identity-candidate/`含28程序，187项manifest，约88.2 MiB；包内28程序启动/help smoke通过。测试结束后无`.graw`或`.part`残留，无真实硬件访问。

本阶段只是HTTP/journal接线前的消息与固定部署校验核心：**尚未新增ECC HTTP端点、尚未接入Envoy的`EccSystemClient`，也尚未把响应持久化进扫描journal**；不能把它称为逐点执行器。Windows和macOS尚未重建这次契约改动；Ubuntu24用户空间runner本轮仍在WSL启动阶段因`UtilBindVsockAnyPort: socket failed 1`失败，未形成新Ubuntu24结论。

证据在`build-validation/linux-host-71-276/`：包含suite构建回执、C++11 core/QtTest结果、CTest日志、固定ECC集成输出、包启动回执及summary中的候选与测试程序摘要。Linux host使用Ubuntu20.04.6/WSL2共享宿主内核，不代表独立Ubuntu、现场板卡或持续吞吐验收。

## 71.275 Linux host 当前源码 full profile 与全套回归（2026-10-22）

在 Ubuntu 20.04.6/WSL2、GCC 9.3.0、Qt 5.12.8 环境，用当前源码构建 Linux host `full` profile **28/28 程序**，通过 **109 项嵌入依赖检查**；stage 后候选 `build-packages/getdaq-linux-host-20261022-71-274-current-source-candidate/` 含28程序及 **187 项 manifest 文件**，包校验通过。候选包内28个程序逐项启动/help smoke 全部通过。

当前源码测试结果：严格 C++11 配置核心 **151/151**、完整 Widgets **122/122**、native CTest **25/25**。Widgets 与配置事务测试使用 `/tmp` 私有缓存，loopback 测试在允许本机套接字的环境执行。测试目录没有遗留 `.graw` 或 `.part`，无真实硬件访问。此前 Ubuntu24 隔离用户空间 runner 在 WSL 启动阶段返回 `UtilBindVsockAnyPort: socket failed 1`，因此本节结果只证明 Ubuntu20 host，不更新 Ubuntu24 的验证结论；Ubuntu24 最新完整候选仍是71.223。

证据保存在`build-validation/linux-host-71-275/`：summary记录候选manifest摘要和QtTest二进制SHA，另含full构建回执、Core/Widgets结果、完整CTest日志及包内程序smoke回执。这不是独立Ubuntu主机、Windows/macOS、现场板卡或持续高速吞吐验收。

## 71.274 Windows Qt 5.14.2 MinGW 全套当前源码构建与扫描 journal 回归（2026-10-22）

使用用户指定的 `D:\Software\QT\5.14.2\mingw73_64` 和配套 MinGW 7.3，从本轮当前源码构建 Windows control profile **8/8 程序**。这次真实构建补出了先前 Linux 构建不能发现的 Windows 问题：journal存储调用Vista及以上API，需在所有系统头前设定`_WIN32_WINNT=0x0600`；MinGW已预定义`NOMINMAX`，Envoy和ACL测试源需条件定义。专测运行又发现`GetSecurityInfo`所用句柄必须申请`READ_CONTROL`；Windows临时目录返回的8.3短路径需用`GetLongPathNameW`规范化后再与`GetFinalPathNameByHandleW`结果比较；本机NTFS上`MoveFileExW(REPLACE_EXISTING)`返回Win32错误5，故更新现有journal改用`ReplaceFileW`，而创建新journal仍用不覆盖的原子发布。上述修改均由最终Windows构建和原生ACL存储测试复核。

随后以严格 C++11（`-pedantic-errors`）构建并运行本机 Windows QtTest：Widgets **121通过、0失败、1跳过**（跳过项为 POSIX 专属锁测试），ECC client **64通过、0失败、0跳过**。独立 Windows scan-journal-store 专测也通过，覆盖当前用户私有 ACL、排他写者、重启后未知操作隔离、硬链接拒绝和外部篡改检测。QtTest 清理回执确认私有临时目录已删除、raw 文件 0 个/0 字节、20 GiB 限额未触发。八程序运行包完成 Qt/MinGW DLL 部署、移除 SDK PATH 后帮助启动、offscreen 初始窗口、Windows 原生窗口 smoke，以及完整 manifest/PE 架构校验；候选 `build-packages/getdaq-windows-qt5142-20261022-current-source-goal-final-candidate/` 含 **8 程序、287 文件，81 MiB**。构建、测试和原生窗口运行均在用户当前 Windows 主机经 WSL 调用完成。

证据：`build-windows-qt5142-20261022-current-source-goal/windows-build-receipt.json`、`qt-tests/native-windows-results/summary.json`、`cleanup-receipt.json`、`native-scan-journal-test/build-and-run.log`、`build-packages/getdaq-windows-qt5142-20261022-current-source-goal-final-candidate/manifest.json`。这不是独立干净 Windows、现场 ECC/板卡或 DAQ 传输验收；Windows GUI 的实际人工配置流程仍需用户检查。

## 71.273 从固定ECC部署生成逐点扫描身份期望（2026-10-22）

新增 Unix 原生 `BuildFixedScanRuntimeIdentityExpectation`。函数重新检查 `FixedEccConfiguration`，调用与服务启动相同的 `PlanNativeEccConfiguration` 及 `RemoteReceiverBindingSha256`，将固定 service/client ID、ECC snapshot/input binding、点配置/工件 SHA、data binding/scope 和 receiver agent/plan 集合与受保护部署及冻结扫描点绑定。只有服务 instance ID、data epoch 和 receiver 实例号从运行状态读取；其余静态部署锚点不能由观察响应自我声明。最后仍调用 `ObserveScanRuntimeIdentity`，要求 ECC controller 已知 Idle、完整 receiver 集合 fresh 且无 session/进程。配置未通过控制凭据校验、点工件不匹配、实例/快照/配置/数据计划不符或响应客户端错配都会拒绝。

从 `build-delivery/split-scalar-full/full/native` 对当前源码增量构建 `fixed-ecc-configuration-tests` 成功；在允许 localhost loopback 的模拟 Ice fixture 环境运行该完整测试通过，输出“PASS fixed input/publication/selection binding, offline deployment, cached metadata, strict transition/dedup/owner, restoration latch and reopen gate; no hardware executed”。测试扩展生成两点 Pulser 计划、冻结 snapshot 和合成 Idle/fresh identity/status，验证固定配置的正确点身份被绑定；错扫描点、篡改 client ID、错data scope和未经控制校验的offline配置都拒绝。原输入变更测试同时确认服务以 failed 结果记录“not executed”并保持 reconciliation 门槛。测试证据目录3.2 MiB已删除；无DAQ raw、物理板卡或真实跨主机访问。

新构造器目前仅用于 Unix 原生 ECC 库，尚未接入 Envoy 异步 ECC 客户端或 `PersistScanRuntimeIdentityBinding`，也没有 receiver/Run 启动与停止、finish/catalog 闭环。Windows Qt5.14.2、macOS和实机扫描未验收；这不是自动扫描完成证明。

## 71.272 扫描运行身份增加ECC客户端ID核对（2026-10-22）

`ScanRuntimeIdentityExpectation`和逐点部署输入增加必填`clientId`。协议验证器现要求ECC `/v1/identity`响应中的`client_id`与期望一致，并把该值写入规范化观察摘要；伪造/误配为另一控制客户端时拒绝。`ecc_journal_test`为新增部署字段补值，保留原来的快照、点索引、接收节点、状态和持久化回归。

Ubuntu host GCC/C++11 下，`common/control`的`scan-runtime-identity-tests`直接运行通过；复用`build-ecc`增量构建`ecc-journal-tests`并直接运行通过，结果为“PASS ECC and scan journals: intent ordering, private atomic store, restart quarantine, tamper and link refusal”。本仓库的该CMake配置没有在所用测试目录注册CTest条目，因此按实际测试可执行程序直接运行验证。临时构建目录`/tmp/getdaq-scanidentity-cxx11`已删除；没有生成raw、访问网络或连接DAQ。

这只加强了协议身份交叉核对，没有解决调用方如何从固定部署配置安全推导期望值，也没有把观察器接到Envoy真实ECC客户端；receiver/Run生命周期及自动逐点采集仍未完成。Windows Qt5.14.2、macOS和实机扫描本轮未验证。

## 71.271 Threshold自动Run策略关闭并改为人工逐点（2026-10-22）

按用户确认，自动逐点扫描必须为每点显式填写事件数；Threshold暂不自动Run。`ScanExecutionPolicy`现只支持PeakingTime、Pulser、AutoGain的事件数+最长运行时长策略。Threshold不能创建事件数策略；旧`timed_windows` JSON也被解码器拒绝。CLI对`timed-windows`直接报错且不发布sidecar。Envoy对Threshold计划显示manual-only说明并禁用策略按钮；离线Threshold点计划及人工Run关联保留。

测试新增核心拒绝断言、Envoy Threshold计划界面状态检查、真实CLI Threshold策略拒绝和无sidecar断言。GCC9.4严格C++11实验核心`system-experiment` CTest 1/1通过；Qt5.12.8/GCC9.3从当前源码重建`getdaq-config`和完整`tests/tests.pro`目标，离屏`offlineScanPlanBuilderIsAvailableFromConsole` 3 passed、0 failed、0 skipped；`tests/system_experiment_integration.py`对当前CLI、原生系统检查器和当前策略checker运行 **94个进程场景通过**，其中Threshold策略拒绝且没有发布sidecar；Markdown链接检查1692项通过。测试只生成临时小型配置元数据，无DAQ raw、硬件或网络操作。Windows Qt5.14.2和macOS本轮未测。

## 71.270 从执行快照生成逐点静态身份期望（2026-10-22）

新增Qt-free `BindScanRuntimeIdentityExpectation`工厂。它接收经过完整计划/策略校验的不可变执行快照、点索引和部署适配器提供的运行期字段；执行快照SHA由规范快照字节计算，当前点的配置SHA和工件SHA直接读取快照，调用方不能在deployment结构中另行填写这三项静态身份。ECC journal集成测试现在通过该工厂创建期望，并验证摘要与点0一致、点1静态身份相应变化、越界索引拒绝；身份持久化桥继续拒绝其他执行快照对象及快照/期望错配，错误时journal不变。

严格C++11 GCC 9.4构建ECC journal、工具和身份协议目标成功，CTest 2/2通过；Qt 5.12.8/GCC 9.3完整`tests/tests.pro`测试程序构建成功，`scanExecutionJournalRoundTripQuarantineAndReconciliation`及`offlineScanPlanBuilderIsAvailableFromConsole`离屏QtTest各3 passed、0 failed。验证只用小型JSON控制元数据，无DAQ raw、网络、receiver或硬件；临时CMake/Qt构建目录约60 MiB已清理。Windows Qt5.14.2/MinGW及macOS未原生验证。工厂没有验证service/instance或数据部署字段来源，因此还不能替代安全的部署期望生成器或真实ECC/receiver观察适配器。

## 71.269 运行身份桥直接核对冻结点工件（2026-10-22）

`PersistScanRuntimeIdentityBinding`现要求调用方传入经过校验的`ScanExecutionSnapshot`对象。写journal前，桥接会用规范快照字节重新计算SHA，并同时核对执行ID、计划SHA、点数、当前点索引以及该点配置/工件SHA；从而避免仅由调用方分别填写正确格式但相互无关的摘要。ECC journal回归使用真实的两点测试快照与其第0点配置/工件SHA，传入其他执行ID的快照对象、错误点工件期望、错执行SHA、错点索引均须拒绝，失败前后journal字节相同；有效绑定经重开仍保留。

严格C++11 GCC 9.4构建ECC journal、工具和运行身份测试目标成功，CTest 2/2通过；Qt 5.12.8/GCC 9.3完整`tests/tests.pro`目标构建成功，`scanExecutionJournalRoundTripQuarantineAndReconciliation`及`offlineScanPlanBuilderIsAvailableFromConsole`离屏QtTest各3 passed、0 failed。Markdown链接检查通过1690项。仅使用小型JSON配置和journal元数据，无DAQ raw、网络、receiver或硬件访问；临时CMake与Qt构建目录（约60 MiB）已清理；Windows Qt 5.14.2/MinGW与macOS未原生构建。该桥仍不生成可信期望、不接真实ECC HTTP客户端、不观察Run/receiver，也不执行扫描。

## 71.268 运行身份摘要绑定具体执行快照和扫描点（2026-10-22）

运行身份观察契约现要求调用方同时提供扫描执行快照SHA-256和点索引；观察规范字节升级到版本2，并把这两个字段纳入摘要，因此即使两个点的ECC/接收节点配置完全相同，产生的绑定摘要也不同。持久化桥在解析ECC响应前先核对期望快照/点索引与当前journal绑定快照/点索引；错执行、错点会在写盘前拒绝。协议测试覆盖摘要跨执行/跨点不同、缺少/越界字段拒绝；ECC journal测试确认错误工件、错误执行SHA、错误点索引都拒绝且journal字节完全不变。

GCC 9.4严格C++11配置下`ecc-journal-tests`和`scan-runtime-identity-tests`均构建成功，CTest 2/2通过；Qt5.12.8/GCC9.3完整`tests/tests.pro`目标干净构建成功，journal和离线扫描GUI专项QtTest各3 passed、0 failed。Markdown链接检查通过1688项。测试只用小型JSON控制元数据，无DAQ raw、网络或硬件；CMake与Qt临时目录（约56 MiB）已清理。Windows Qt5.14.2与macOS未原生验证。

这为已冻结快照提供了执行/点级防串用约束，但调用方仍须从正确、受保护的快照获取期望SHA和点索引；该改动不生成可信ECC/receiver期望、不连接ECC客户端、不启动采集，完整执行器仍未完成。

## 71.267 经协议验证的运行身份观察持久绑定（2026-10-22）

新增`PersistScanRuntimeIdentityBinding`作为ECC journal存储桥接：调用方提供冻结的逐点期望值及`/v1/identity`、`/v1/status`响应字节，桥接先调用Qt-free `ObserveScanRuntimeIdentity`完成严格交叉核验，再将规范观察SHA写入当前点journal。验证失败时不会写journal；存储仍执行既有单写者租约和状态后继校验。ECC journal集成测试现以完整小型JSON响应走这条路径，核验有效身份持久化、重开后摘要保留，并确认错误工件期望被拒绝且原journal字节不变。

严格C++11 POSIX ECC journal/CLI构建及CTest 1/1通过；Qt5.12.8/GCC9.3/C++11 `tests/tests.pro`完整目标构建成功，`scanExecutionJournalRoundTripQuarantineAndReconciliation`及`offlineScanPlanBuilderIsAvailableFromConsole`各3 passed、0 failed。测试只使用小型JSON控制元数据，无DAQ raw、网络或硬件；本轮临时构建目录在记录完成后清理。Windows Qt5.14.2与macOS未原生构建验证。

该桥接不生成可信逐点期望、不连接ECC客户端，也不采集receiver/Run身份；实际Envoy流程尚未调用它。它只证明传入响应与调用方期望一致并保存摘要，不认证服务、不证明板卡状态、不启动数据采集。因此完整执行器及真实联调仍未完成。

## 71.265 每点运行期ECC身份摘要门槛（2026-10-22）

扫描journal新增运行期身份绑定事件并升级为schema v3。新点只能先提交并完成`start-ecc`，再持久化该点ECC身份观察摘要；只有绑定后才能为`describe`及后续生命周期操作写入intent。推进到下一点会清除当前运行身份，要求新点重新绑定。journal只校验摘要格式与事件顺序；调用方仍须依据正式ECC身份/状态解码器核对service instance、snapshot、点配置/工件摘要、数据绑定和节点身份，再计算该摘要。该变更本身不验证观察内容、不做认证，也没有实际ECC适配器。旧schema v1/v2仍按原格式读取，v1已有操作历史不可追溯补绑；新审阅journal写v3。

严格C++11扫描执行核心CTest 1/1通过，覆盖新点先start-ECC、缺少身份绑定时拒绝describe、绑定后允许、重复绑定拒绝、跨点清除和v2兼容；POSIX journal存储/inspect工具CTest 1/1通过，覆盖运行身份摘要持久化、重开和CLI展示。Qt 5.12.8/GCC 9.3完整Widgets测试目标编译成功；离屏`scanExecutionJournalRoundTripQuarantineAndReconciliation`及`offlineScanPlanBuilderIsAvailableFromConsole`专项QtTest各3 passed、0 failed（包含QtTest初始化与清理）。测试只用小型journal JSON/配置元数据，无DAQ raw/网络/硬件。Windows schema后端及Qt5.14.2、macOS尚未验证。

## 71.266 ECC运行身份观察的协议交叉校验（2026-10-22）

新增Qt-free C++11 `ObserveScanRuntimeIdentity`，只接受调用方已经冻结的逐点期望值及ECC `/v1/identity`、`/v1/status`只读响应。函数复用现有严格协议解码器，拒绝service/instance/snapshot/input/configuration/artifact/data binding不匹配、identity与status的实例/快照不一致、ECC控制器非空闲/忙碌/UNKNOWN/需对账、数据controller scope/epoch不同、receiver节点集合缺失/多余，以及agent instance或receiver-plan SHA不符、生命周期非fresh或已有session/process。通过后按字段转义和agent ID排序生成版本化规范字节与SHA-256；输入的节点排列顺序不影响摘要。

GCC 9.4严格C++11 `scan-runtime-identity-tests`构建成功，CTest 1/1通过；覆盖有效观察、稳定摘要、期望工件/receiver计划篡改、身份不一致、活跃ECC操作及非fresh receiver拒绝。测试只构造小型内存JSON，无网络、硬件或raw。该验证证明协议响应符合调用方给定期望，不认证HTTP端点、不证明板端寄存器或物理状态；调用方期望值如何安全地产生及部署到逐点ECC服务仍待执行器实现。Windows/macOS原生构建尚未验证。

## 71.264 旧journal操作历史禁止事后补绑（2026-10-22）

新增回归覆盖v1兼容边界：带已完成ECC操作历史的旧journal仍可按原身份解码并恢复到历史状态，但不能再绑定新生成的执行快照。这样避免把快照引入前已提交的外部操作错误归属到新输入。严格C++11扫描执行journal CTest 1/1通过；POSIX ECC journal存储CTest 1/1通过；Qt5.12.8/GCC9.3离屏扫描审阅端到端专项QtTest 3 passed、0 failed。Markdown本地链接1681项通过。测试只用小型控制元数据，无DAQ raw、网络或硬件访问；相关临时构建目录已清理。Windows Qt5.14.2与macOS未原生验证。详见71.263所述journal v2绑定及迁移边界。

## 71.263 Envoy审阅journal绑定完整执行快照（2026-10-22）

Envoy现在只有在计划旁存在有限停止策略时才允许记录执行输入审阅。人工审阅前，核心会重新验证计划、策略和每个点工件，在当前用户私有的`scan-journals/`目录独占创建或重验执行快照；journal v2记录这份快照文件字节SHA-256，并在创建时先持久化`SnapshotBound`事件。每个后续外部操作intent都要求journal已绑定快照。旧journal v1仍可只读重开；若还没有任何外部操作，可在追加审计绑定事件后迁移，已有操作历史禁止追溯补绑。

扫描journal/执行核心严格C++11 CTest 2/2通过；POSIX ECC journal存储集成CTest 1/1通过；Qt5.12.8/GCC9.3/C++11离屏`offlineScanPlanBuilderIsAvailableFromConsole` 3 passed、0 failed，验证策略创建、snapshot CLI与checker、审阅时的本机快照落盘、journal摘要绑定、重开和对账流程。测试只生成小型JSON/XML配置元数据；无raw、网络、receiver或硬件操作。测试使用的checker已从当前源码独立重建，避免旧可执行文件缺少`scan-execution-policy`命令。Windows Qt5.14.2和macOS未原生验证。

本节71.263验收时journal只绑定离线输入；运行期身份摘要门槛随后由71.265加入，但仍没有协议身份交叉校验适配、ECC/receiver命令适配或自动扫描。journal摘要不是命令授权或硬件证明。证据：`common/experiment/scanexecutionjournal.{h,cpp}`、`apps/envoy/scanplanreview.cpp`、`apps/ecc/scanexecutionjournalstore{,_windows}.cpp`及本节测试结果。last_verified: 2026-10-22。

## 71.262 计划与停止策略的不可变执行快照（2026-10-22）

新增Qt-free C++11 `ScanExecutionSnapshot`，以一次调用方执行ID绑定已完整验证的计划SHA、策略文件字节SHA、扫描类型/CoBo/点数及每个点的配置/工件摘要。`getdaq-config create-scan-snapshot`只允许在真实父目录中独占新建；`getdaq-config-check scan-execution-snapshot`重新检查计划的全部点工件，并逐字段核对快照、传入计划和策略的一致性。修改点摘要、替换策略或改动计划都会拒绝。命令不连接硬件，也不表示完整执行清单或已启动采集；快照尚无ECC实例、receiver周期、Run身份或人工单点操作确认。

GCC9.4 Release/C++11实验核心构建成功，系统实验测试1/1通过，覆盖事件策略与Threshold窗口策略快照创建/重开、计划/策略摘要、逐点摘要、非法ID、点摘要篡改和替换策略拒绝。Qt5.12.8/GCC9.3 qmake完整Widgets测试目标及配置CLI/只读checker构建成功；离屏GUI→CLI→checker专项QtTest 3 passed，包含策略创建后快照生成/复核、重复路径拒绝且原快照字节不变。未接网络、硬件或DAQ raw；本节临时41 MiB构建目录已清理。Windows Qt5.14.2和macOS尚未原生验证。

## 71.261 Envoy扫描窗口创建有限停止策略（2026-10-22）

扫描计划窗口在完整校验计划后显示与扫描类型对应的停止条件输入。PeakingTime、Pulser和AutoGain要求事件数及最大时长；Threshold要求定时窗口数及单窗时长。GUI拒绝未填写条件，展示包含计划身份和参数的二次确认，取消为默认选项；确认后使用`getdaq-config create-scan-policy`创建固定名称`scan-execution-policy.json`，不覆盖已有文件，再调用`getdaq-config-check`独立复核。创建及复核期间锁定冲突按钮；操作结果明确说明只创建策略元数据，不发送Configure/Run。

Qt5.12.8/GCC9.3/C++11 qmake测试目标构建成功；离屏真实窗口专项QtTest `offlineScanPlanBuilderIsAvailableFromConsole`通过（3 passed, 0 failed），覆盖空策略拒绝、确认创建、sidecar内容与manifest摘要绑定、独立检查通过及GUI不发硬件命令。没有网络、硬件或raw数据操作。Windows Qt5.14.2和macOS尚未原生构建验证。策略消费、自动多点执行和Threshold物理触发/结果语义仍未完成或确认。

## 71.260 离线创建并只读核验扫描执行策略（2026-10-22）

`getdaq-config create-scan-policy PLAN_DIR event-count EVENTS MAX_DURATION_MS NEW_POLICY.json`和`timed-windows WINDOWS DURATION_MS`现在提供显式策略创建流程。CLI会拒绝符号链接计划目录/点工件、先完整校验原计划及全部点工件、验证策略类型和有界停止条件，再以Qt `QFile::NewOnly`独占创建sidecar；目标已存在时拒绝覆盖。随后`getdaq-config-check scan-execution-policy PLAN_DIR POLICY.json`重验计划并核对原manifest摘要、计划类型、CoBo和点数。两个CLI均不发送硬件命令。

完整实验核心CTest 14/14通过；真实旧配置→当前源码配置CLI生成四种扫描计划→策略创建→Qt-free检查的进程集成 **100 cases通过**。四种计划均有合法策略创建/验证及manifest摘要篡改拒绝，重复创建保留原策略文件；旧配置源字节保持不变。此前71.259的严格C++11警告检查和文档链接检查仍通过；本次修改后文档链接检查见当前运行结果。测试只产生小型配置/JSON临时文件，没有raw、网络或硬件操作。Envoy尚无策略编辑UI或自动执行器；Threshold触发/Pulser/结果解释未确认，Windows Qt5.14.2和macOS未验证。

## 71.259 扫描停止策略的独立有限策略模型（2026-10-22）

新增Qt-free C++11 `ScanExecutionPolicy`，作为原扫描计划之外的JSON sidecar，不改变计划和点配置。PeakingTime、Pulser、AutoGain策略必须显式设置每点事件数（1..1,000,000,000）及最大运行时长（1..24小时）；Threshold策略必须显式设置每点定时窗口数（1..3600）和单窗时长（1..60秒），并限制单点总时长不超过24小时。两类策略互斥且不可互换。策略严格绑定原manifest字节SHA-256、扫描类型、CoBo和点数；调用方需先完整验证扫描计划，Decode再核对绑定与精确JSON字段。Qt-free `getdaq-config-check scan-execution-policy PLAN_DIRECTORY POLICY.json`对策略和原计划做只读检查，明确不会发送硬件命令。

GCC9.4 C++11实验核心、`getdaq-config-check`和`system-experiment-tests`重新构建成功，完整实验配置/四类扫描计划回归1/1通过；新断言覆盖两类策略往返、类型互斥、0值/超上限拒绝、超24小时拒绝、计划字节变化和summary身份不匹配。用当前源码`getdaq-config`从真实旧配置生成PeakingTime及Threshold计划，再由当前检查器接受有效策略并拒绝错误计划摘要；输出确认没有硬件命令。一次复用Ubuntu24候选进行全流程测试因直接宿主运行缺少glibc/Qt ABI而失败，随后候选没有包含当前扫描生成子命令，故未把它计入通过证据。仅创建`/tmp/getdaq-scan-policy-build`约14 MiB构建目录，未生成DAQ raw，未访问网络或硬件。策略模型仍不含Envoy编辑入口或自动执行；Threshold的触发源/Pulser动作与结果解释仍未定义，因此不能据此启动实机扫描。Windows Qt5.14.2/macOS本轮未验证。

## 71.257 Envoy扫描计划窗口增加Run归档关联入口（2026-10-22）

Envoy离线扫描计划窗口在计划成功打开或创建并通过摘要/点数校验后，新增“Bind a completed Run catalog to one scan point”和“Build the complete scan-to-Run index”入口。前者由操作员选择0起始点号、已封存receiver catalog和现有凭据目录，调用同版本`getdaq-catalog --bind-scan-point`创建`point-NNN.json`；后者调用`--summarize-scan`检查完整连续凭据并生成新JSON索引。启动子进程前窗口重新校验扫描计划摘要；运行期间禁用冲突操作。Envoy仅负责低速交互，catalog核心继续负责路径隔离、归档完整性、摘要精确匹配与禁止覆盖。工具可从Envoy同目录、`PATH`或操作员在文件选择框指定；控制profile不因此捆绑数据接收/传输程序。

本次没有控制硬件、启动/停止Run、连接receiver或读取/传输DAQ raw。失败提示要求先检查现存文件再决定重试。当前新增Qt回归检查按钮在无有效计划时禁用、有效计划生成后启用；扫描创建、GUI入口、journal reconciliation专项测试使用Qt5.12.8/GCC9.3/C++11离屏构建后 **4/4通过**。完整Widgets测试二进制成功链接。真实封存catalog经GUI操作、Windows Qt5.14.2重建及现场流程仍未在本轮验证。临时构建目录`/tmp/getdaq-scan-binding-ui-build`在回归结束后删除；本轮未生成或读取raw文件。

## 71.258 控制端可定位独立catalog元数据工具（2026-10-22）

控制profile刻意不包含`getdaq-catalog`，以保持Envoy控制包与接收/传输程序分离。扫描归档UI先查同目录和`PATH`；未找到时提示操作员定位已单独安装、与catalog生成端版本一致的`getdaq-catalog`，并在当前窗口会话复用路径。Envoy仍仅通过`QProcess`传递元数据命令，没有把catalog或数据传输模块链接进Qt程序。

Qt5.12.8/GCC9.3/C++11完整Widgets测试目标重建成功；扫描窗口、计划CLI和journal专项回归4/4通过，Markdown链接检查1669项通过。此次没有模拟手动选择程序的文件对话框，也未操作真实catalog；Windows Qt5.14.2、macOS和现场流程未验证。没有DAQ raw或网络访问。

## 71.255 重启后重新打开并审阅离线扫描计划（2026-10-22）

Envoy离线扫描窗口新增“Open an existing offline plan for review”入口。操作员可以在后续启动的窗口中选择以前生成的计划；程序重新运行完整manifest、网格、点工件、SHA-256和逐点配置语义检查。只有全部通过才恢复审阅按钮；无效计划清除旧身份并保持审阅禁用。该入口只读计划目录，不发送ECC/receiver命令，不读写DAQ/raw数据。此前已审阅的同摘要计划仍由本机journal幂等识别。

测试在一个新建`ScanPlanWindow`中打开上一阶段刚创建并确认的16点PeakingTime计划，使用非原生Qt文件对话框选择目录，确认状态显示验证后的摘要且审阅按钮可用；随后选择缺少manifest的空目录，确认窗口拒绝打开并禁用审阅。计划创建子进程运行期间，界面代码禁用创建/打开入口，结束后恢复；测试主要核验完整操作流程，不以极短进程上的瞬时状态断言作为门槛。Linux/WSL Qt5.12.8/GCC9.3按C++11重新构建当前`getdaq-config`和完整`tests/tests.pro`成功；包含上述按钮状态实现的最终源码下，定向`offlineScanPlanBuilderIsAvailableFromConsole` 3项（含初始化/清理）通过，0失败、0跳过。约20 MiB临时构建树已删除；没有生成raw、访问网络或硬件。

本轮Windows Qt5.14.2 MinGW与macOS未原生构建/运行；WSL启动Windows qmake仍被`UtilBindVsockAnyPort: socket failed 1`阻断。该功能仍只支持离线审阅，不构成逐点Configure/Run执行器。实现：`apps/envoy/scanplanwindow.{h,cpp}`；回归：`tests/test_getdaq.cpp`；操作：见[离线扫描计划窗口](SCAN_PLAN_GUI.md#4-审核输出与后续采集)。

## 71.254 Envoy扫描计划人工审阅持久化（2026-10-22）

扫描计划摘要现包含原始manifest字节的SHA-256。Envoy在原生CLI完成全套计划检查、manifest及所有点工件校验后，才允许操作员点击人工审阅按钮。确认框阻断式等待用户明确Yes；随后重新读取并验证计划，要求SHA和点数仍与当前对话框持有的身份一致，避免用户确认期间清单/工件发生替换。首次确认在专用本机私有目录创建`ScanExecutionJournal`并提交`Planned→Reviewed`；相同计划摘要之后重复确认只报告已审阅。POSIX目录必须是当前用户所有的0700真实目录；Windows新目录使用当前用户专属、禁止继承的DACL，存在的目录不会被静默改权限。

测试将现有Envoy菜单→计划窗口→实际`getdaq-config` CLI链路跑通，生成16点PeakingTime计划，经Qt窗口Yes确认后从磁盘重开journal，确认state为`Reviewed`/16点，并再次确认保持幂等。Qt5.12.8/GCC9.3按C++11完整Widgets测试程序构建成功，定向QtTest 1/1通过。计划工件仅是小型JSON/XML配置；无raw、网络、receiver、ECC或硬件访问。

此记录不包括操作者身份、可信时间戳、外部证据签名，也不表示实际Configure/Run或硬件加载。Windows后端代码虽已接入qmake的MinGW分支，本机WSL仍在启动Windows工具时报告`UtilBindVsockAnyPort: socket failed 1`，故Windows编译/运行和macOS原生回归尚未验证。下一步应先由目标平台构建验证私有目录与Win32 ACL行为，再继续接入安全的ECC单点流程；自动Stop/Run策略仍不能假定。源码：`apps/envoy/scanplan{window,output,review}.{h,cpp}`、`apps/envoy/core.pri`、`apps/ecc/scanexecutionjournalstore*.cpp`、`tests/test_getdaq.cpp`。

## 71.253 Windows扫描journal原生持久化后端（2026-10-22）

新增`scanexecutionjournalstore_windows.cpp`，在Windows控制端提供与POSIX相同的`ScanExecutionJournalStore`接口。构造路径拒绝相对路径、UNC和设备路径；通过Win32文件句柄核对本地目录身份、reparse属性、当前用户owner与DACL允许项。journal和lock须为当前用户拥有、单硬链接普通文件；同一journal的lock以无共享模式打开。持久化先在同目录以独占方式创建临时文件、写完整JSON并`FlushFileBuffers`，随后创建时不覆盖既有目标，更新时确认目标文件身份后使用`MoveFileExW`写穿替换。文件内容限定1 MiB。独立`getdaq-scan-journal inspect FILE`只输出状态摘要，不接硬件。

新增Windows测试覆盖私有ACL目录中的创建/重开、第二writer拒绝、pending intent重启隔离、人工确认未发送的持久化、硬链接拒绝与外部追加篡改检测。Windows GitHub Actions新增使用MinGW构建CLI及测试并运行专用CTest的步骤。操作说明已加入[Windows扫描journal构建与ACL要求](ECC_JOURNAL.md#71-windows-qtmingw-扫描journal)。

本轮Linux重新配置并构建POSIX journal与CLI，ECC/实验核心CTest 16/16通过；这只验证POSIX分支。Windows工作流尚未执行，本机从WSL启动Windows进程在qmake探测阶段报`UtilBindVsockAnyPort: socket failed 1`，所以Windows代码还没有真实编译器诊断或运行结果。未访问硬件、网络或DAQ数据，没有产生raw。部署Windows文件系统的掉电行为、ACL继承差异以及Windows CI执行仍待验证；不能据本轮宣称Windows或整套重构完成。

## 71.252 Qt Widgets跨平台扫描journal模型回归（2026-10-22）

在完整Widgets QtTest中新增纯模型回归，覆盖扫描journal intent保持原生命周期状态、待完成intent在重开解码时进入`NeedsReconciliation`、确认未发送后恢复原状态但不允许复用旧operation ID，以及已完成事件的编码/解码恢复。测试只使用内存JSON，不依赖POSIX store，因此进入现有Windows Qt 5.14.2 MinGW Widgets测试套件。

Linux Qt 5.12.8/GCC 9.3 qmake生成`tests/tests.pro` Makefile后，`make test_getdaq.o`编译新增完整Widgets测试源码成功；ECC/实验核心CTest 16/16通过，项目Markdown链接1656项通过。尝试从WSL启动Windows Qt 5.14.2 qmake时，在版本探测阶段因`UtilBindVsockAnyPort: socket failed 1`失败，尚无Windows本轮构建/运行结果；失败发生在构建脚本写入或改动候选前。未访问网络/硬件、未生成raw，临时Linux对象目录已删除。源码：`tests/test_getdaq.cpp`、`common/experiment/scanexecutionjournal.{h,cpp}`。last_verified: 2026-10-22。

## 71.251 扫描协调器异步阶段接口（2026-10-22）

为衔接现有异步 Qt ECC HTTP 客户端，`ScanOperationRunner`新增`PersistIntent`、`Complete`和`MarkUnknown`三个分阶段接口：调用方必须先持久化intent，再发起一次异步请求；得到明确响应后记录完成，超时/断连等不确定结果记录unknown。同步`Execute`保留并复用这些阶段。分阶段接口避免等待网络响应，但journal文件同步/`fsync`仍是同步操作；若在Qt主线程直接调用，磁盘延迟仍可能短暂阻塞界面。

ECC journal测试直接执行分阶段接口，确认网络提交前journal中已存在匹配的pending operation ID，成功响应可推进到`EccStarted`且关闭重开后保留。同步路径仍覆盖回复丢失、完成落盘失败不重放等故障。GCC/C++11 ECC/实验核心完整CTest 16/16通过。该测试没有把Qt客户端接入扫描runner，未访问网络、硬件或DAQ/raw；接口本身不判定某个Qt错误究竟发生在发送前还是发送后，调用方须保守地将不确定回调映射到`MarkUnknown`。若需完全避免界面线程等待本地同步持久化，应由专用worker持有journal store，再通过Qt线程队列投递网络请求和结果。源码：`apps/ecc/scanoperationrunner.{h,cpp}`、`tests/ecc_journal_test.cpp`。last_verified: 2026-10-22。

## 71.250 扫描单操作调用顺序协调器（2026-10-22）

新增Qt-free C++11 `ScanOperationRunner`，在调用外部操作回调前先将operation ID和请求SHA写入扫描journal；回调成功后记录响应SHA并按固定操作名推进状态。回调抛出异常时将提交结果标记为未知，不自动重试，并要求后续人工对账。若unknown记录无法落盘，残留intent会在重开时进入隔离状态。fake callback测试确认调用前intent已可见、正常成功仅调用一次、“外部可能已受理但回复丢失”时调用一次并在重开后进入`NeedsReconciliation`，以及外部动作已返回成功但完成记录因journal被并发篡改而无法落盘时不会重放操作。ECC/实验核心CTest 16/16通过。

该类仅约束单次操作的记录/调用顺序，没有ECC HTTP客户端、receiver/catalog或硬件适配器，也没有执行扫描、网络或DAQ/raw测试。它不能证明远端是否受理，也不能替代操作员对账UI。源码：`apps/ecc/scanoperationrunner.{h,cpp}`、`tests/ecc_journal_test.cpp`。last_verified: 2026-10-22。

## 71.249 扫描执行journal离线CLI检查（2026-10-22）

`getdaq-ecc-journal inspect-scan FILE`现在通过`ScanExecutionJournalStore`读取扫描执行journal，并输出schema固定的状态JSON摘要：执行ID、计划摘要、点数、当前点和状态、事件数、pending operation ID及unknown标志。CLI不发送硬件命令、不修改journal JSON或推进状态；打开时会获取store的独占非阻塞锁，缺少sidecar lock时会按权限合同创建0600 `.lock`文件。持锁中的服务、非规范路径、非私有目录或损坏journal均会使CLI失败。

ECC journal集成回归在重启后得到`NeedsReconciliation`状态时调用真实构建的CLI，并检查kind、unknown标志和原operation ID；`ecc-journal`及全部ECC/实验核心CTest 16/16通过。未生成DAQ/raw、没有网络或硬件访问。该命令仅POSIX构建，不能提供Windows检查器，也不是操作者UI或硬件状态查询。源码：`apps/ecc/journalmain.cpp`、`apps/ecc/CMakeLists.txt`、`tests/ecc_journal_test.cpp`及[CLI手册](ECC_JOURNAL.md#8-只读查看命令)。last_verified: 2026-10-22。

## 71.248 对账事件记录声明操作者（2026-10-22）

在71.247 journal对账事件中加入必填`operator_id`声明字段；创建API拒绝空/不安全ID，严格JSON重放拒绝字段缺失、额外字段和空值。事件保存操作者声明、原operation ID、对账结论及证据摘要，供未来审计界面关联。该字符串不是登录认证、电子签名或防抵赖证明，当前仍没有身份服务或操作员UI。

严格C++11 ECC/实验核心完整CTest 16/16通过；Qt5.12.8 qmake共享`experiment.pri`的`getdaq-config-check`以`-Wall -Wextra -Wpedantic -Werror`在临时输出目录构建和help smoke通过。文档链接检查1648项通过。临时qmake目录自动删除，未生成raw、网络或硬件访问。源码：`common/experiment/scanexecutionjournal.{h,cpp}`、`tests/scan_execution_state_test.cpp`、`tests/ecc_journal_test.cpp`。last_verified: 2026-10-22。

## 71.247 扫描操作结果人工对账事件（2026-10-22）

`ScanExecutionJournal`新增不可变`operation-reconciled`事件，要求明确原pending operation ID、声明操作者ID、对账结果和外部观察材料SHA-256。`confirmed-applied`只接受原命令映射的下一状态；`confirmed-not-sent`恢复到原命令前状态，但旧operation ID继续保留为已使用，下一次尝试必须使用新ID。结果不明期间仍不能走普通状态迁移；重启时留下intent但无unknown事件的情况，也可通过同一显式对账事件收敛并重放。操作者ID仅供审计追踪，不作身份认证；观察摘要只用于关联证据，不能验证证据真实性、硬件读回或命令确未发送。

回归覆盖错误operation ID/摘要拒绝、错误目标状态篡改拒绝、已执行与确认未发送两条路径、旧ID重用拒绝、新ID重试、重启后未完成intent对账，以及POSIX store持久写入后关闭并重开。GCC9.4严格C++11 ECC/实验核心完整CTest 16/16通过。测试只使用小型临时JSON journal，没有raw、网络或硬件访问；沿用工作区既有build-ecc目录。

限制：这是Qt-free journal API，没有操作员UI、外部观察/证据验证服务、ECC operation-status查询适配或硬件验证。任何调用方都必须确保只有具备现场核对依据的人工流程能写入对账决定。源码：`common/experiment/scanexecutionjournal.{h,cpp}`、`common/experiment/scanexecutionstate.{h,cpp}`、`tests/scan_execution_state_test.cpp`、`tests/ecc_journal_test.cpp`。last_verified: 2026-10-22。

## 71.246 扫描journal POSIX持久化与未完成intent隔离（2026-10-22）

为扫描控制元数据增加`ScanExecutionJournalStore`（POSIX/Linux/macOS）：仅接受规范绝对路径和当前用户拥有、权限严格为0700的目录；针对journal文件固定0600/regular/单硬链接，针对lock文件采用0600、固定文件身份和独占非阻塞`flock`。读写均使用`openat`/`O_NOFOLLOW`、目录fd相对检查和限制1 MiB的完整读回；Create使用私有同目录临时文件、`fsync`后`linkat`不覆盖发布，Persist要求完整journal仅前进一步，使用同目录临时文件、`fsync`、原身份复核、`renameat`替换并同步目录。外部替换/内容变化导致fail-closed，不覆盖未知文件。该服务只存控制JSON，不存放DAQ/raw内容。

ECC journal集成测试中新增扫描存储覆盖：验证私有文件mode/硬链接计数、同一 journal 第二writer被拒绝、正常Review和操作intent/complete写入后重开状态恢复；仅intent而无完成记录时重开进入`NeedsReconciliation`并保留原operation ID，不能作为未发送而自动补发；对活动journal外部追加字节后，Persist拒绝并保留改动；symlink journal拒绝。GCC9.4 Release/C++11 `ecc-journal-tests`构建成功，CTest `ecc-journal` 1/1通过；测试目录为mode0700的`/tmp`临时目录，journal和lock在fixture结束时删除，没有生成raw/访问网络或硬件。

同一阶段强化journal操作边：只有Planned→Reviewed可无操作完成；固定的start-ecc/describe/prepare/configure/run-start/run-stop/receiver-finish/catalog-verify/bind/retire命令分别只允许对应状态边，并必须先写带request SHA的唯一operation intent，再由带response SHA的completion推进。Decode拒绝映射不匹配。该边仍是模型级记录，不把摘要本身当作硬件读回证明。

限制：POSIX服务未实现Windows版；没有操作员reconciliation API/UI、停止策略、计划摘要与实际点工件目录的加载绑定、ECC控制API调用、receiver/catalog集成、或真实硬件验证。intent/complete数据格式不能替代这些适配。代码：`apps/ecc/scanexecutionjournalstore.{h,cpp}`、`common/experiment/scanexecutionjournal.{h,cpp}`、`tests/ecc_journal_test.cpp`、`apps/ecc/CMakeLists.txt`。last_verified: 2026-10-22。

## 71.245 扫描执行记录严格JSON模型（2026-10-22）

新增Qt-free C++11 `ScanExecutionJournal`不可变值模型：顶层固定schema/kind、执行ID、扫描计划SHA-256、点数和事件数组；每个状态转换和下一点事件均使用精确字段集合。解码以1 MiB总长、4096事件/数组项、1–256点、64位小写SHA、受限ASCII ID为边界，并重放全部状态事件，拒绝错误点号、跳步、错误前态、未知状态/事件、额外字段和越界点数。调用方使用`WithTransition`/`WithNextPoint`得到独立候选对象，可先编码持久化后替换当前值。

GCC9.4严格C++11下完整实验核心CTest 14/14通过；专项状态/journal用例覆盖两点完整日志回放、候选值不修改原journal、错误状态跳转、伪造状态和schema额外字段拒绝、异常身份与终态Unknown拒绝。Qt5.12.8/Linux `getdaq-config`通过共享`experiment.pri`重新构建链接，help入口正常。临时CMake/qmake目录和程序已删除；测试没有创建DAQ/raw数据、网络或硬件访问。

限制：这只是受限日志数据模型，没有磁盘读写、原子替换/`fsync`、writer锁、权限和symlink防护，也未把journal绑定到实际扫描manifest文件或ECC操作ID。进程若在硬件动作与记录间退出，模型本身不能判断操作是否发出/受理；不得据此自动重试、继续扫描或声称崩溃恢复已经实现。后续需在Qt-free执行宿主加入安全目录/锁、原子持久化、操作意图/回执拆分和人工对账。源码：`common/experiment/scanexecutionjournal.{h,cpp}`、`scanexecutionstate.{h,cpp}`、`tests/scan_execution_state_test.cpp`。last_verified: 2026-10-22。

## 71.244 扫描执行状态核心第一步（2026-10-22）

新增Qt-free `ScanExecutionState` 单点状态校验核心：仅接受 `Planned → Reviewed → ECCStarted → Described → Prepared → Configured → RunStarted → RunStopped → ReceiverFinished → CatalogVerified → Bound → Retired` 顺序；支持任一未退休阶段转入终态 `NeedsReconciliation`，但不允许自动从该状态恢复。下一点只能在上一点Retired后开始；点数限制为1–256。expected-state参数可拒绝过期调用方推进，但类本身不是并发同步原语。

Ubuntu20.04/WSL GCC9.4下以C++11 Release构建 `getdaq-experiment-core` 和 `scan-execution-state-tests` 成功；目标CTest 1/1通过，覆盖完整两点前进、跳步、陈旧expected、未退休推进、越过末点和UNKNOWN不可恢复。Qt5.12.8/Linux下，使用`experiment.pri`的`getdaq-config`也重新qmake并成功链接该核心。两个临时构建目录及程序均已删除。本项不访问硬件、网络或DAQ数据。

范围严格限于内存转换规则：无执行清单、磁盘journal/崩溃恢复、ECC API、receiver/Run/catalog操作或策略映射；不证明任何扫描动作能安全实机运行。停止策略问题仍待确认。代码：`common/experiment/scanexecutionstate.{h,cpp}`、`tests/scan_execution_state_test.cpp`。last_verified: 2026-10-22。

## 71.243 模拟器生成数据设置累计上限（2026-10-22）

独立`getdaq-simulator`此前会以每个运行CoBo每200 ms写64 KiB的速度无限追加测试`.graw`，忘记Stop可能持续占用磁盘。现在整个模拟根目录默认累计预算1 GiB，命令行`--max-generated-bytes`允许64 KiB至20 GiB；启动时递归计入每个模拟CoBo incoming及封存子目录中已有`.graw`，达到预算时停止生成并明确要求操作员Stop。它不自动停止模拟ECC、不删除文件；输出仍是标记字节，不是有效MFM帧。

Qt5.12.8/GCC9.3严格C++11测试构建成功。`simulatorGeneratedDataBudgetIsHard`验证128 KiB预算恰好停止且延迟后不增长，并重启同root以64 KiB预算时因已封存数据超额而拒绝；既有独立模拟Run生命周期回归也通过。两个定向QtTest槽：4 passed、0 failed、0 skipped；每个fixture生成不超过1 MiB，总测试数据低于20 GiB并由`QTemporaryDir`自动删除。独立模拟器qmake生产目标编译成功；CLI help包含预算参数，超过20 GiB时返回命令行错误，且不创建root。临时测试构建/二进制已删除。

此测试只证明模拟器的合成数据预算和localhost联调流程，不涉及真实DAQ、硬件、帧解码、跨主机传输或持续吞吐。

## 71.242 扫描逐点执行器架构核对（2026-10-22）

根据当前固定工件ECC和接收器绑定契约，新增[扫描执行器架构说明](SCAN_EXECUTOR_ARCHITECTURE.md)：逐点扫描需要独立ECC快照、Run、receiver周期和归档绑定；Qt只管理低频控制，不接收或转发raw。说明也核对了旧`TestController`对`nbEvent`的不同用法：PeakingTime/Gain/AutoGain计事件，而Threshold以周期Pulser和采集窗口推进；所以新执行器必须明确每种停止策略，不能默认自动Run。

本项为源码与现有文档审阅，没有新增执行器代码，也没有声称扫描自动化已实现。当前仍是操作员逐点执行与归档绑定；后续实现需要先确定停止/事件窗口策略，再增加持久执行清单、单点控制适配和明确失败恢复。文档链接检查1637项通过；未连接硬件或生成raw。

## 71.241 修正离线依赖操作手册的文件数量（2026-10-22）

依赖操作手册原写完整`full` profile核验105个文件，与当前依赖清单及脚本回执不一致。重新运行`tools/verify-dependencies.py`确认：`full`检查109个文件，`control`检查95个，`data`和`analysis`检查0个，`data-host`检查1个依赖元数据文件。已修正文档，并说明profile计数应以脚本回执为准，避免未来依赖清单变化后继续照抄旧数值。

`python3 tests/test_dependency_verification.py` 12/12通过；`full`、`control`、`data`、`data-host`和`analysis`五种profile均实际执行并通过。本项为离线文件清单核验，不编译程序、不访问网络或硬件。`--json`输出和更详细使用步骤见[依赖与离线构建说明](DEPENDENCIES_AND_OFFLINE_BUILD.md#9-编译前检查本地依赖)。

## 71.240 修正macOS扫描集成检查器路径（2026-10-22）

复查`tools/build-suite.py`发现：`getdaq-config-check`在`CONTROL`列表中，由qmake输出到`full/bin`；`getdaq-system-config`属于原生CMake的`NATIVE`列表，输出到`full/native`。macOS工作流此前把两个路径都指向`full/native`。现已将checker参数改为`build-platform/macos-full-suite/full/bin/getdaq-config-check`，并在`tests/test_windows_build_entrypoints.py`中断言程序清单归属及两条stage路径，防止后续变更再引入路径错配。

Windows/Linux平台入口合同套件19/19通过。为确认集成测试行为，在本机按当前源码重建实验核心、配置CLI、系统配置checker和Qt-free配置检查器；真实ExternalTrigger示例的四种扫描计划有效验收、manifest/点未知字段拒绝及现有配置检查共84个子进程案例通过。构建计划和本机full候选布局均显示checker在`bin`、system-config在`native`。Markdown链接检查1634项通过。测试在临时目录执行且原始输入保持不变，没有raw、硬件或DAQ网络访问。

本机没有原生macOS runner；这项修复证明工作流路径符合构建计划，尚未证明macOS工具链、动态库或完整job实际运行成功。

## 71.239 扫描计划协议字段精确校验（2026-10-22）

扫描manifest与点对象此前检查了所需字段，但没有拒绝未识别字段。现`ValidateSystemScanPlan`按PeakingTime、Pulser、Threshold和AutoGain定义精确字段集合，缺少、拼错或额外字段都会拒绝；未知计划类型也在读取点文件前失败。这样协议扩展必须显式改变schema版本/允许字段，而不会被旧消费者默默忽略。

核心`system-experiment-tests`现对四种真实计划构建器工件逐个验证合法manifest成功、错误配置摘要拒绝、manifest额外字段拒绝及点额外字段拒绝。GCC9.4/C++11完整实验核心及13个CTest目标构建通过，CTest 13/13。Qt5.12.8/GCC9.3构建当前`getdaq-config`，GCC9.4构建当前`getdaq-system-config`和`getdaq-config-check`；`tests/system_experiment_integration.py`针对真实ExternalTrigger示例编译配置、生成四类计划、由Qt-free checker验收并测试上述反例，共84个子进程案例通过。输入示例原文件逐字节保持。Windows构建入口合同19/19通过，确认CI将checker交给Windows/macOS集成脚本；本轮没有运行Windows/macOS原生流水线。Markdown本地链接检查1633项通过。

完整CMake临时构建目录`/tmp/getdaq-scanplan-strict-20261022`约十余MiB，随后删除。Qt配置工具根据其既有`common/build-output.pri`写入了仓库`bin/getdaq-config`，该文件是当前源码重新构建的可执行程序（不是扫描数据）；集成计划在脚本私有临时目录内自动清理。没有生成raw、连接DAQ、访问硬件或执行传输。验证只覆盖Linux/WSL本机C++11与配置协议，不证明Windows Qt5.14.2/macOS可构建、manifest来源可信、配置已写入板卡或实机扫描正确；整体重构未完成。

## 71.238 Qt-free扫描计划核心回归（2026-10-22）

扩展`system-experiment-tests`，直接调用Qt-free `ValidateSystemScanPlan`。回归分别用现有PeakingTime、Pulser、Threshold和AutoGain计划构建器产生的完整点工件，组合匹配的manifest字段、连续索引和SHA-256，再由内存loader提供文件内容；四类计划均接受并返回正确类型/目标/点数。每种计划还会将一个配置摘要替换为错误摘要并确认拒绝。核心测试无需Qt、计划目录、临时DAQ文件、硬件或网络。

在GCC 9.4下以C++11配置`common/experiment`并构建`system-experiment-tests`成功；`ctest -R '^system-experiment$' --output-on-failure`结果1/1通过，测试程序直接运行亦通过。临时构建树`/tmp/getdaq-scanplan-core-20261022`约7 MiB，验证后删除。此次只运行该核心CTest，没有复跑全体核心/Widgets测试，也未验证Windows Qt5.14.2、macOS、硬件或DAQ传输；摘要检查仍不是来源签名或硬件加载证明。

## 71.237 Qt-free扫描计划检查命令

把扫描计划判定逻辑从Envoy UI移入C++11实验核心 `ValidateSystemScanPlan`。Qt GUI及无Qt的`getdaq-config-check scan-plan PLAN_DIRECTORY`共用同一解析、摘要与逐点参数一致性规则；CLI要求扫描目录、manifest和每个点文件都是非链接普通文件，并限制manifest≤1 MiB、点工件≤64 MiB。它只读manifest和配置JSON，不依赖Qt/Ice，不打开raw数据，也不声称硬件加载。

使用GCC9.4/CMake构建完整Qt-free实验核心和`getdaq-config-check`成功；13项核心CTest全部通过。另使用Qt5.12.8/GCC9.3/C++11构建完整Widgets QtTest程序，并与CLI及`getdaq-config`并排运行`scanPlanOutputInspectionRejectsMalformedPlans`和`offlineScanPlanBuilderIsAvailableFromConsole`：两槽各含init/cleanup，共6项PASS。窗口实际生成PeakingTime 16点、Pulser 3点、Threshold 3点、AutoGain 6点；四个目录均由新Qt-free CLI再验证通过。篡改Pulser点电压后GUI和CLI都拒绝；恢复manifest后再次通过。另用临时夹具验证CLI拒绝符号链接扫描目录及符号链接点文件。Markdown链接检查1631项通过。约几十MiB的CMake/qmake构建目录和链接测试副本随后删除；没有生成DAQ raw、进行网络传输或访问硬件。

本轮未复跑完整Widgets QtTest，也没有在用户Windows Qt5.14.2 MinGW或macOS编译该核心。CLI验证内部结构、摘要和参数一致性，不证明manifest来源可信、硬件寄存器已加载或实机采集成功。

## 71.236 Envoy扫描计划逐点语义闭环（2026-10-22）

`InspectScanPlanOutput`在已做文件安全、manifest安全声明、流式SHA-256和连续点序检查后，现在还将每个点文件交给Qt-free核心`DecodeSystemExperimentArtifact`完整解码。它重算文件SHA-256与`configuration_sha256`，要求二者都与manifest相同，并确认扫描点值确实编码在对应CoBo/AGET配置中：PeakingTime每点对照芯片版本的16值表；Pulser逐点对照闭区间mV网格和`Generator/amplitudeStart`；Threshold对照完整码网格、`Reg1/GlobalThresholdValue`及全部通道细阈值；AutoGain对照完整通道×电压点数、旧Reg3/Reg4枚举顺序、活动且唯一被选中的测试通道及每点Pulser幅值。点数上限分别保持PeakingTime 16、Threshold 128及Pulser/AutoGain 256。该功能属于目录内部一致性验证，不提供数字签名、可信来源或硬件加载证明。

回归使用Qt5.12.8/GCC9.3严格C++11构建完整Widgets QtTest程序及同目录`getdaq-config`。`scanPlanOutputInspectionRejectsMalformedPlans`和`offlineScanPlanBuilderIsAvailableFromConsole`定向执行通过（各含init/cleanup，共6项PASS）；GUI测试真实生成PeakingTime 16点、Pulser 3点、Threshold 3点和AutoGain 6点计划，逐点语义检查全部通过；将Pulser第二点manifest电压由-950改为-940后检查器拒绝，恢复manifest后通过。当前测试没有生成raw、连接DAQ或访问硬件。构建及CLI位于`/tmp/getdaq-scanverify-20261022`，随后清理。

此轮没有复跑完整120项Widgets套件或Windows Qt5.14.2/macOS构建。检查器仍不能证明计划目录来自可信编译源；界面不执行自动逐点Configure/Run，也不证明硬件寄存器读回。板端和真实DAQ验收仍未完成。

## 71.235 离线扫描计划完整性验收（2026-10-22）

Envoy的`InspectScanPlanOutput`现在强制核验扫描计划边界声明：`hardware_loaded_verified=false`、`requires_full_configure_per_point=true`、`acquisition_event_count_unspecified=true`；AutoGain还必须保持`gain_result_interpretation_verified=false`。逐点文件限定为64 MiB以内并通过流式Qt SHA-256与manifest `artifact_sha256`比对，配置摘要须是小写64位SHA-256格式。它只核验点文件字节与manifest，不代替`getdaq-config-check experiment`对工件结构和配置摘要的完整解码。

回归覆盖错误摘要、点内容篡改、声称硬件已加载、删除逐点完整Configure要求、声称AutoGain解释已验证等均被拒绝；同一个GUI端到端用例真实生成PeakingTime 16点、Pulser 3点、Threshold 3点和AutoGain 6点计划并全部接受。Qt5.12.8/GCC9.3严格C++11完整构建后全量Widgets QtTest为120通过、0失败、0跳过（约52秒）。受限沙箱初次运行仅45通过/75失败，原因是它拒绝loopback监听和测试目录写入；相同二进制在允许这些测试前提的环境中全量通过。运行期间没有访问硬件或raw数据。私有构建树`/tmp/getdaq-scanverify`约22 MiB，测试完成后删除。Markdown本地链接检查见本节后的复核记录。

此验证限Linux Qt5.12.8和GUI/fixture，不证明Windows Qt5.14.2、macOS、真实硬件或现场扫描。摘要匹配不等同于签名/来源可信，也不验证实际寄存器加载；完整工件语义仍由独立`getdaq-config-check`负责。

## 71.233 Run预留事务失败清理（2026-10-22）

`RunRecorder::Reserve`现于最终Run目录的同一父目录创建私有暂存目录，先保存`run.yml`和初始`journal.json`，随后重命名发布。新增`runReservationFailureDoesNotPublishPartialDirectory`，用无效Run号触发配置保存拒绝，核验正式Run目录不存在，并在Run目录的实际父目录检查没有遗留`.pending`暂存项。另回归`simulatedLifecycle`和`configurationSnapshotClosureAndRetry`，确保正常Reservation、完成、已有Run防覆盖、配置闭包快照与源变化后的快照重试保持工作。

使用Ubuntu/WSL Qt5.12.8、GCC9.3、C++11在`tests/tests.pro`下构建完整QtTest程序，并执行上述3个测试槽：5项通过、0失败、0跳过（含init/cleanup）。运行测试时启用QtTest的`QStandardPaths::setTestModeEnabled(true)`，因此锁文件测试需要写入用户级`~/.qttest/cache`；受限沙箱运行时该缓存不可写，允许测试所需访问后通过。测试未启动DAQ、访问硬件或产生raw数据；临时构建位于`/tmp/getdaq-runreserve-atomic`，验证后清理。

此验证针对Run目录发布及已有配置快照回归，不覆盖跨进程并发竞争的全平台语义，也不是扫描多点Run编排、真实硬件或完整重构验收。

## 71.232 Threshold扫描网格一致性校验（2026-10-22）

阈值归档分析现在核验manifest的`start_code`、`stop_code`、正`step_code`、整除关系和完整点数，并逐点要求`threshold_code == start + index × step`；随后仍与完整配置工件中的粗/细阈值寄存器比对。重新生成Run/catalog绑定不能绕过这层计划语义检查。

新增反例将单点有效工件及重新生成的有效Run/catalog绑定置于声明为0..8的错误计划范围，分析拒绝且不发布报告。GCC9.4严格C++11构建`receiver-catalog-tests`和`getdaq-catalog`成功；`receiver-catalog-filesystem` CTest **1/1通过**。无网络传输或硬件访问；最大单个raw夹具512MiB稀疏文件、低于20GiB，测试fixture自动清理，临时构建目录随后删除。此项仅核验合成计划字段及寄存器一致性，不解释hit-pattern的物理含义。

## 71.231 扫描分析拒绝自相矛盾的电压计划（2026-10-22）

归档波形分析器现在在读取点绑定前核验Pulser/AutoGain manifest的起止电压、步长、闭区间点数和逐点电压。AutoGain还按旧Reg3/Reg4通道顺序核验完整通道×电压网格，避免一份内部字段互相矛盾、但重新生成绑定后仍可分析的计划被标成完整。PeakingTime的固定16点表校验保持原样。

新增Pulser和AutoGain反例回归：分别将单点工件与有效Run/catalog绑定到不一致的点电压/范围，分析必须在发布报告前失败。GCC 9.4严格C++11构建`receiver-catalog-tests`和`getdaq-catalog`成功；`receiver-catalog-filesystem` CTest **1/1通过**，含完整原生catalog fixture及CLI回归。测试不发送数据、不访问硬件；该测试套件最大单个raw夹具为512 MiB稀疏文件（低于用户要求的20 GiB上限），所有夹具由测试目录自动清理。测试只证明合成计划完整性和既有波形路径，不证明现场计划或硬件读回。

## 71.230 PeakingTime分析命令行端到端回归（2026-10-22）

`receiver-catalog-filesystem` CTest现在接收同一build tree的`getdaq-catalog`可执行路径，并在已封存的合成PeakingTime Run fixture上调用真实CLI。回归核对命令输出中的芯片配置时间和波形指标，验证重复运行不会覆盖报告，并确认通道越界时返回错误且不发布文件。该路径证明命令参数解析、库调用、绑定核验、报告写出和失败边界共同工作，不只验证分析API。

GCC9.4严格C++11构建成功；完整`receiver-catalog-tests` **49项通过**，其中含type-1、compact type-2、分层MFM、Pulser、PeakingTime、AutoGain和阈值catalog分析；CTest定向`receiver-catalog-filesystem` **1/1通过**。Peaking CLI使用的合成type-1 raw共208字节，整个测试fixture自动删除。没有DAQ网络传输或真实硬件访问；无认证TCP传输、Windows/macOS以及现场扫描不在本回归范围。详见[PeakingTime绑定分析](SCAN_RUN_BINDING.md#9-peakingtime-扫描归档波形响应)。

## 71.229 PeakingTime 扫描 Run 绑定波形响应（2026-10-22）

新增原生Qt-free `getdaq-catalog --analyze-peaking-scan SCAN_DIRECTORY BINDING_DIRECTORY MFM_COBO_NUMBER ASAD AGET CHANNEL OUTPUT_JSON`。它逐点重验PeakingTime计划与Run/catalog绑定、点工件摘要和所选AGET配置；因PeakingTime manifest不重复寄存器值，报告值取自点工件中的`Reg1/peackingTime`，并按该芯片版本和点号核对16项旧配置表。通道不由此计划选择，必须由操作员显式提供并在每个工件中验证为活动通道。报告给出配置PeakingTime和封存Run的波形计数/峰值/peaking/FWHM统计，不证明板端读回或科学判据。

GCC9.4严格C++11、Qt-free `getdaq-catalog`及catalog测试构建通过；`receiver-catalog-tests` **46项通过**。208字节合成type-1 raw用实际`BuildPeakingTimeScan`工件验证Run绑定、工件中的PeakingTime数值和100 ADC/2 peaking/3 FWHM输出，并验证非法通道拒绝；夹具自动清理。Pulser、AutoGain type-1/compact type-2/分层测试同时通过。Windows Qt5.14.2、macOS、真实采集/硬件和候选包未验收。见[扫描Run绑定](SCAN_RUN_BINDING.md#9-peakingtime-扫描归档波形响应)。

## 71.228 Pulser 幅值扫描 Run 绑定波形响应（2026-10-22）

新增Qt-free原生API `AnalyzeBoundPulserWaveforms` 和命令`getdaq-catalog --analyze-pulser-scan SCAN_DIRECTORY BINDING_DIRECTORY MFM_COBO_NUMBER ASAD AGET CHANNEL OUTPUT_JSON`。它重验每个Pulser幅值计划点与Run/catalog绑定、完整配置工件的`amplitudeStart`以及操作者明确指定通道的活动状态，然后复用AutoGain归档波形分析路径流式输出每点帧计数、缺失/无效/有效计数、平均峰值ADC、peaking/FWHM buckets。Pulser计划没有通道选择字段，所以通道由操作者给定；程序不推断扫描点对应通道，也不计算物理增益或质量判据。

GCC9.4严格C++11、Qt-free `getdaq-catalog`和`receiver-catalog-tests`构建成功；原生catalog套件 **44项通过**。208字节合成type-1 raw验证Run绑定输出100 ADC、2 peaking buckets、3 FWHM buckets，并覆盖非法通道拒绝；fixture在测试后自动清理。AutoGain type-1、compact type-2及分层容器原回归仍通过。未生成大体积传输文件，未访问真实DAQ网络或硬件；Windows Qt5.14.2、macOS、真实Pulser扫描和平台候选未验证。见[扫描绑定文档](SCAN_RUN_BINDING.md#8-pulser-幅值扫描归档波形响应)。

## 71.227 AutoGain 绑定归档波形响应分析（2026-10-22）

新增原生C++11命令`getdaq-catalog --analyze-autogain-waveforms SCAN_DIRECTORY BINDING_DIRECTORY MFM_COBO_NUMBER OUTPUT_JSON`。它逐点重算AutoGain Run绑定，核对manifest通道/幅值与对应完整配置工件相符，再从已封存catalog只读流式读取目标源raw；报告每点帧、通道缺失/无效/有效计数及平均峰值ADC、peaking/FWHM buckets。输出只反映波形响应，不计算物理增益、不作质量判定，也不证明硬件寄存器或Pulser实际读回。

Qt-free `getdaq-catalog` 从当前源码以GCC9.4/C++11成功构建。`receiver-catalog-tests` **42项通过**，分别用普通type-1、完整compact type-2 rev5及`0xFF01` rev1分层容器中的type-1子帧验证绑定分析，均输出100 ADC峰值、2 peaking buckets及3 FWHM buckets；type-1用例还验证manifest通道与配置工件不符时拒绝。合成raw分别为208字节、2,228,336字节和272字节，运行后fixture自动删除。测试不访问硬件、没有网络传输，也不证明现场采集。命令行帮助已更新。

当前分析支持CoBo type-1 revision 0..5、完整compact type-2 revision 5及`0xFF01` revision 1分层容器；分层帧最大32MiB、最多8层。遇到所选目标的其他type/revision或不完整布局会失败关闭。聚合器检查catalog文件身份/大小、分析期间文件元数据稳定和完整catalog/计划凭据，但catalog schema没有逐个raw载荷SHA，因此不能将这些元数据检查说成防恶意原位篡改证明。Windows/MinGW、macOS、真实MFM归档与物理增益判据本轮均未验证；尚未刷新平台候选包。见[AutoGain计划](AUTOGAIN_SCAN_PLAN.md)、[扫描Run绑定](SCAN_RUN_BINDING.md)。整体重构未完成。

## 71.226 Gain/AutoGain 归档波形响应表回归（2026-10-22）

`getdaq-waveform-viewer --analyze-pulser-response`新增按显式通道/Pulser mV/文件组汇总波形指标的CLI。核心测试覆盖相同通道/幅值多分段文件聚合、同一AGET缺失通道计数、无效通道/顺序拒绝。Qt5.12.8/GCC9.3严格C++11构建成功，两个离线波形扫描定向用例 **4 passed**；在允许本机loopback的条件下，完整Widgets套件 **119 passed，0 failed，0 skipped**，52.242秒。`tests/test_windows_build_entrypoints.py` **19/19**，Markdown本地链接1608项通过。

查看器单独构建并以2,304字节MFM type-1合成帧运行CLI，按通道7、-1000/-950 mV输出各1帧、1个有效波形、100 ADC、3 peaking buckets和5 FWHM buckets；通道8报告1帧但通道缺失。临时合成`.graw`小于20GiB并已删除；本轮完整`/tmp`构建树和日志在验证完成后清理。

随后使用同一当前源码执行Linux host `full` profile：28/28程序构建成功，109项嵌入依赖检查通过，生成71.226 full候选，186项候选清单完整性复核通过。包内`getdaq-waveform-viewer --help`列出新命令，Linux动态依赖检查无缺失库。受时间/范围限制，候选只完成完整性与新CLI smoke，没有在包内重跑全套网络/硬件测试；源码全套Widgets 119/119和单次小型CLI回归如上。该历史候选目录已于2026-10-23清理；本段保留历史验证结论，不再提供候选文件。

该命令只产生操作员提供映射的波形响应表，不根据ADC/mV自行计算物理增益，不作自动增益判定，也不核对扫描计划、Run/catalog凭据、实际Pulser输出或板端读回。没有真实采集数据或硬件访问。Linux/Ubuntu24用户空间共享WSL2内核；71.226候选不是独立Ubuntu主机验收。Windows Qt5.14.2 MinGW当前源码构建、macOS原生构建及现场Gain/AutoGain工作流仍未验证；重构仍未完成。

## 71.225 PeakingTime 离线波形汇总与完整 Widgets 回归（2026-10-22）

在 Qt 5.12.8/GCC 9.3、C++11 环境下，`tests/tests.pro` 构建成功。新定向用例`waveformViewerSummarizesPeakingTimeScan`为 **3 passed，0 failed**；开放本机回环后完整 Widgets 套件 **118 passed，0 failed，0 skipped**，52.431 秒。第一次在受限沙箱运行只有43 passed、75 failed，失败涉及socket监听和临时配置窗口/文件系统用例；相同二进制在获得回环访问后全部通过，因此不把受限运行结果视为源代码回归。

另构建`apps/waveformviewer/waveformviewer.pro`，通过 CLI `--help`及小型合成端到端试验。试验使用单个2,304字节 `.graw`帧，映射到点3和点4；CSV两行均为1帧/1个有效波形、峰值100 ADC、peaking time 3 buckets、FWHM 5 buckets。合成文件与仓库`bin/`中的临时查看器可执行文件均已删除。该试验证明本地文件读取、CLI解析和汇总输出，不证明真实CoBo数据、采集点与文件映射、Run/catalog绑定或硬件读回。

此功能只在采集归档后分析操作者指定的文件；它不连接ECC/DAQ、不参与数据传输，也不自动验证扫描manifest和Run凭据。它不能替代自动扫描控制、逐点事件统计或现场科学结果验证。Windows Qt5.14.2/MinGW本轮仍受WSL互操作错误阻断，macOS未执行；完整平台和硬件边界见[重构状态](REFACTOR_STATUS.md)。

## 71.224 Qt HttpClient取消用例与平台测试门槛更新（2026-10-22）

为`HttpClient::Cancel()`增加`httpClientCancelIsIdempotent`回归：在本机TCP服务器保持连接但不返回响应时取消请求两次，确认用户回调不会在取消后执行。测试无DAQ文件生成。Qt5.12.8/GCC9.3、严格C++11配置的完整Widgets测试程序从当前源码构建成功；定向取消用例通过，连续20次通过，Widgets套件117/117通过。普通沙箱禁用loopback，测试只在获准的本机loopback环境中运行。

因Widgets用例数由116增至117，Ubuntu24平台验证器、其入口回归以及macOS原生数据面工作流的显式总数门槛已同步更新，避免新代码在CI中被旧计数误拒。Windows Qt5.14.2当前源码构建尝试在启动用户`qmake.exe`前被WSL互操作层报`UtilBindVsockAnyPort: socket failed 1`阻断；没有产生Windows编译结论。Windows构建入口19/19、host suite交付17/17和1609项本地Markdown链接检查通过。

这轮只改Qt请求取消回归及跨平台QtTest计数门槛，没有改原生DAQ接收/传输路径，也没有生成raw。Windows原生构建、macOS原生CI实际运行、独立主机/物理双机/实板和持续DAQ吞吐仍未验收。完整重构仍未完成。last_verified: 2026-10-22。

## 71.223 Qt 网络请求取消健壮性与 Ubuntu24 全套回归（2026-10-22）

`HttpClient::Cancel()`现在先从活动reply集合摘除本轮对象、断开回调，并仅对尚未完成的`QNetworkReply`调用`abort()`；已完成但其`finished()`清理回调尚未运行的对象只延迟销毁。这样避免取消路径重复终止已经结束的网络请求。该修改针对此前观察到的Qt网络reply重复错误警告作防御性处理；此前单次`backupRetryDoesNotRecatalog`失败没有稳定复现，不能据此声称已证明是它导致的原失败。

Qt5.12.8本机严格C++11构建完整Widgets测试程序成功；`backupRetryDoesNotRecatalog`专项通过，对应Stop后归档重试路径连续20次通过；Widgets QtTest为116通过、0失败、0跳过。Ubuntu24.04.5用户空间（WSL2共享宿主内核，GCC13.3.0/CMake3.28.3/Qt5.15.13）对当前源码重新运行完整`--stage all`：full profile 28/28程序，Core151/151、Widgets116/116、CTest23/23，平台回执`build-ubuntu24-validation/rootfs/build/getdaq-platform/platform-77721e2b26d74f6a8f9dee7526222bee.json`中的23条命令全为0退出码。四路合成接收负载1GiB并清理1GiB，失败handoff另产生8,192字节并清理；回执`raw_remaining=0`。

由本轮同一Ubuntu24构建生成候选`build-packages/getdaq-ubuntu24-20261022-71-223-httpclient-cancel-candidate/`，28个程序SHA与被测构建匹配，Ubuntu24 loader下28程序启动/help合同及GetDAQ离屏窗口smoke通过，候选完整性校验通过。之前一次网络回退测试失败的精确触发条件仍未知；本轮重跑只能证明没有复现。Ubuntu24用户空间共用WSL2宿主内核，不代表独立机器、真实双机、实板或持续DAQ吞吐；Windows当前源码本轮未重建，macOS原生CI和现场验收仍待完成。整套重构未完成。last_verified: 2026-10-22。

## 71.222 Ubuntu24 当前源码全量回归与完整候选（2026-10-22）

在项目专用、已验签的Ubuntu24.04.5用户空间中，对当前工作树执行标准`--stage all`。GCC13.3.0、CMake3.28.3、Qt5.15.13；full profile从当前源码构建28/28程序，109项嵌入依赖检查通过。平台验证回执记录23条命令全部退出码0：Core QtTest 151/151，Widgets QtTest 116/116，原生CTest 23/23；配置来源迁移、事务、receiver传输/安全、失败数据handoff窗口及四路接收负载均通过。此前一次Widgets运行中`backupRetryDoesNotRecatalog`出现单次失败和Qt网络回复警告；在有loopback权限的同一Ubuntu24用户空间中，隔离重复20/20通过，随后完整Widgets与全套回归也通过。先前单次失败的触发原因没有被稳定复现，故将它保留为历史失败，不宣称已定位根因。

从这次验证所用的同一28程序构建产出Ubuntu24 full候选`build-packages/getdaq-ubuntu24-20261022-71-222-current-source-full-candidate/`，候选manifest与构建回执中的程序SHA一致；包内28程序的Ubuntu24 loader启动/help合同及GetDAQ离屏首窗截图smoke通过。完整平台回执及逐程序smoke回执保存在候选`validation/`目录。

四路localhost合成接收负载为每路256MiB、合计1GiB；测试结束自动删除1,073,741,824字节，清理回执`remaining_generated_payload_files=0`。失败handoff场景产生的8,192字节raw亦全部删除；验证工作树中未发现残留`.graw`。测试数据没有超过用户要求的20GiB上限。此结果证明WSL2上的Ubuntu24用户空间兼容、同机回归和有限时长合成接收路径；Ubuntu24用户空间与WSL2宿主共用内核，不证明独立Ubuntu机器、物理双机、真实板卡或持续DAQ吞吐。Windows本轮未重建，macOS原生CI及现场硬件仍待验收，整体重构仍未完成。平台回执：`build-ubuntu24-validation/rootfs/build/getdaq-platform/platform-28fe794b13ff474da7b5f2ed28876687.json`。last_verified: 2026-10-22。

## 71.221 Ubuntu24 当前源码 full profile 候选构建（2026-10-22）

在项目预先配置并验签的Ubuntu24.04.5用户空间内，从当前只读源码挂载重新构建`full` profile全部28个程序；清单记录Qt5.15.13、CMake3.28.3及109项嵌入依赖检查。生成候选`build-packages/getdaq-ubuntu24-20261022-71-221-full-current-source-candidate/`，并逐项验证候选内28个程序的Ubuntu24动态加载器启动/帮助合同；GetDAQ离屏主窗口截图启动正常。候选manifest含187个文件，其中包内启动回执绑定各程序SHA。包清单校验通过。

构建/测试临时目录位于专用Ubuntu24用户空间，复制候选后已删除。启动smoke无硬件访问、无DAQ传输、无`.graw`；私有Home/Qt缓存/截图目录清理完成。回执在候选`validation/ubuntu24-package-program-smoke.json`及`build-delivery/ubuntu24-full-71-221-package-smoke.json`。本轮只证明Ubuntu24用户空间编译和程序启动，不等于完整QtTest/CTest/集成矩阵重新通过，也不等于独立Ubuntu24机器、macOS、Windows本轮构建、现场硬件或持续吞吐验收；用户空间使用WSL2宿主内核。整体重构仍未完成。last_verified: 2026-10-22。

## 71.220 Full 候选普通 TCP 归档传输回归（2026-10-22）

针对Linux host full候选包中的`getdaq-catalog`，以当前源码重新编译`system-experiment-tests`与`catalog-transfer-tests`后，直接运行系统实验回归和普通TCP传输回归。验证覆盖无认证TCP新传输、连接中断后续传、接收端回读、已完成续传不重发载荷、独立CLI监听/发送进程、连接拒绝有界退出、四种超限元数据在暂存前拒绝，以及SIGPIPE线程隔离。传输结束回执与候选包内`getdaq-catalog` SHA绑定。

本轮合成raw生成15,269,888字节，回归预设上限33,554,432字节（32 MiB），测试脚本核实传输回读及结束后raw剩余0字节、raw文件0个、两个私有测试根目录为空，并清除临时构建目录。小型JSON证据在`build-delivery/catalog-plain-tcp-current-full-71-220.json`。运行环境是WSL2 Linux用户空间/Qt5.12.8/glibc2.31；这验证同一主机上的loopback普通TCP，不是物理双机、真实网卡/磁盘、Ubuntu24独立主机、Windows/macOS或板端吞吐验收。TCP按用户要求无认证且不加密。

首次在沙箱内运行时，环境以`EPERM`拒绝测试用的`setsockopt(SO_RCVTIMEO)`；在获准的本机执行环境下，同一回归完整通过。测试源码现会在该调用失败时报告errno，便于区分系统策略和产品故障。last_verified: 2026-10-22。

## 71.219 Linux 当前源码 full profile 候选（2026-10-22）

以 `tools/build-suite.py --profile full --jobs 2` 从当前工作树干净构建完整Linux host suite，生成 `build-packages/getdaq-linux-host-20261022-71-219-full-current-source-candidate/`。接收/离线工具4项、原生硬件/ECC/ICE和supervisor工具14项、配置仓库2项以及控制工具8项，共28程序；109个嵌入依赖文件检查通过。候选完整manifest共186项并经 `--verify-stage` 校验。随后按各程序实际CLI约定验证包内28个入口：27项 `--help` 返回0，`getdaq-ice-check --help` 按设计打印该命令的Usage并返回2。包内GetDAQ以隔离HOME/XDG和Qt offscreen启动，通过 `--screenshot` 正常退出并生成1280×860 PNG，截图已删除。

当前完整候选约87MiB，临时构建树峰值261MiB并已删除；候选内raw文件数为0。没有连接ECC、硬件或启动receiver服务，没有生成DAQ传输负载。这是Qt5.12.8/Linux WSL2 host full profile，不等同Ubuntu24独立rootfs、Windows Qt5.14.2、macOS或实机/持续吞吐验收。详情见候选manifest及[重构状态](REFACTOR_STATUS.md)。last_verified: 2026-10-22。

## 71.218 Linux 当前源码 control profile 候选（2026-10-22）

用 `tools/build-suite.py --profile control` 从当前工作树干净构建 Linux Qt/C++11控制工具组，生成 `build-packages/getdaq-linux-host-20261022-71-218-control-current-source-candidate/`。候选含8个程序：`GetDAQ`、`getdaq-sentry`、`getdaq-simulator`、`getdaq-config`、`getdaq-config-check`、`getdaq-config-upload`、`getdaq-supervisor-query`、`getdaq-waveform-viewer`；95个嵌入依赖文件核查通过。stage后包校验通过，共166项清单文件。8个包内程序分别以 `--help` 启动成功；包内GetDAQ用隔离HOME/XDG和Qt offscreen运行 `--screenshot`，主窗口正常退出并输出1280×860 PNG，截图随后删除。未连接ECC/硬件、receiver或DAQ路径，没有生成raw。

临时控制构建树约37MiB，完成后删除；候选包约14MiB。该证据来自Qt5.12.8/WSL2宿主，不代表Ubuntu24单独rootfs、Windows Qt5.14.2、macOS、现场硬件或持续DAQ验收；这是control profile，不包含数据接收/传输程序。详见候选manifest及[重构状态](REFACTOR_STATUS.md)。last_verified: 2026-10-22。

## 71.217 扫描计划输出检查器独立回归（2026-10-22）

将扫描 manifest 与点工件检查提取到 Qt Core 组件，供操作台在 CLI 返回成功后复用。检查器只接受四种已支持扫描类型，校验schema、CoBo、1..256点、manifest常规文件、点对象及精确连续索引；工件名必须是唯一的单层文件名，并确认对应对象是非空普通文件且不是符号链接。遇到错误即返回具体原因，GUI 不再将损坏的部分输出显示成完整计划摘要。它不重算工件SHA，完整内容仍由原生检查器校验。

Linux Qt 5.12.8、严格C++11完整 Widgets 测试程序与完整操作台重建通过。新增用例覆盖合法计划、未知 kind、浮点索引、路径穿越、重复文件名、缺失/空文件及Unix符号链接清单；与四种计划真实窗口/CLI回归合跑为4 passed、0 failed、0 skipped。配置CLI单独重建成功。测试不访问硬件或DAQ raw。

这验证的是Qt5.12.8本机结构检查及离屏流程，不代表Windows Qt5.14.2、macOS、SHA完整性或现场硬件验收。操作台和配置CLI写入项目标准 `bin/` 构建目录；QtTest及隔离用户目录在 `/tmp` 构建/运行并于测试后清理。last_verified: 2026-10-22。

## 71.216 离线扫描计划完成摘要和输出检查（2026-10-22）

操作台离线扫描窗口在同目录`getdaq-config`退出成功后读取生成的manifest，核对schema、扫描类型、CoBo、点数及连续索引，并确认每个列出的点文件都是非空普通文件、不是符号链接。界面显示扫描类型、目标CoBo、配置点数和文件在场数；manifest无效或点文件缺失时提示先检查生成目录。此检查仅确认计划结构和文件在场，不重算工件SHA；实际完整参数/摘要校验仍由原生检查器承担。

Linux/WSL Qt 5.12.8下用严格C++11重建配置CLI和Widgets QtTest程序。离屏真实窗口测试`offlineScanPlanBuilderIsAvailableFromConsole`通过（3 passed、0 failed、0 skipped），实际调用相邻CLI，依次生成PeakingTime 16点、Pulser 3点、Threshold 3点和AutoGain 6点计划，并核对窗口摘要、文件数量及已有非法范围拒绝。测试还修正了一个场景错误：原用例没有从AutoGain切回Threshold就断言Threshold非法范围提示。用户目录/XDG隔离；没有硬件、DAQ或raw文件。

这是Linux Qt 5.12.8本机回归，不代表Windows Qt5.14.2、macOS或发布包验收。临时CLI/QtTest构建目录已清理。

## 71.215 GetController `hittedChannel` 分支范围核对（2026-10-22）

静态复核 `source/get/GetController-release-20210208/src/get/WorkspaceManager.cpp` 的 `ALLOWED_TEST_TYPES` 与 `TestController.cpp` 工作线程分支。默认允许类型是 `Acq2pMode`、`Default`、`PeakingTime`、`StdAcquisition`，不含 `hittedChannel`；不能仅据线程分支认定该测试仍是现场可选入口。旧 `hittedChannel` 分支在当前幅值下注入并等待事件，达到 `statNb` 后停发送、切慢控制、左移选择掩码、设置 Reg3/Reg4 后恢复发送；分支没有自己的命中判据、结果表或文件写出，首个选择值也未在该分支初始化。AutoGain计划起止幅值相同可表达相近的逐通道配置点，但不包含旧事件循环和状态切换，不能视作完整替代。

本项为对收到的旧源码静态核对，没有硬件、DAQ或编译测试。使用说明和迁移边界见[GetController扫描计划](GETCONTROLLER_SCAN_PLAN.md#hittedchannel-分支的迁移边界)及[源码审阅](SOURCE_REVIEW_AND_PLAN.md)。现场是否使用该分支仍待确认。

## 71.214 AutoGain 离线配置计划核心与界面验证（2026-10-22）

新增纯C++11 `BuildAutoGainScan`、`getdaq-config plan-autogain-scan` 和操作台 AutoGain 模式。计划要求显式启用CoBo、活动AsAd/AGET及全部活动通道的0..67闭区间；整数Pulser幅值必须为升序闭区间、步长整除终点，起点与基准 `Generator/amplitudeStart` 一致；通道数 × 幅值档数上限256。每点只在目标芯片选择一个测试通道，并设置目标CoBo幅值，形成完整系统工件且重算配置/工件SHA。遍历顺序按旧 `TestController` 对 Reg3/Reg4 单热位递增的顺序：Reg3 bit0..33映射通道33..0，再到Reg4 bit0..33映射34..67；幅值内层升序。manifest逐点标明通道/电压，声明需各自完整Configure、事件数未指定、硬件未验证且不作增益解释。

验证结果：严格C++11实验核心测试通过，额外展开全部68通道，并逐点核验通道顺序及编译后Reg3/Reg4 one-hot位。Linux Qt5.12.8/C++11 `getdaq-config`和完整 `GetDAQ`主界面均重建成功；主界面在隔离用户目录的offscreen环境保持运行3秒后按测试时限结束，没有崩溃（WSLg runtime目录只读警告不影响启动）。`tests/system_experiment_integration.py` **68个CLI/native-checker进程场景通过**，逐点比较完整系统参数，验证工件SHA与配置摘要、原有输出保护和manifest元数据。离屏真实操作台测试 `offlineScanPlanBuilderIsAvailableFromConsole` **3 passed、0 failed、0 skipped**，验证窗口实际调用同目录CLI生成两通道×三幅值计划和非法步长拒绝。Windows构建入口合同 **19/19通过**。`receiver-catalog-tests` **38项原生检查通过**，包含AutoGain点Run绑定及整套索引汇总；AutoGain专用合成`.graw`为320字节，夹具已清理；套件另有最大512MiB稀疏文件系统fixture，不读写DAQ载荷且测试后清理。没有硬件命令或DAQ接收。

本轮没有运行完整Widgets全量套件；既有115/115结果属于71.209/71.207记录。没有刷新Linux full、Ubuntu24、Windows或macOS候选包。Windows Qt5.14.2/MinGW本机qmake仍受WSL互操作 `UtilBindVsockAnyPort: socket failed 1`阻断，GitHub原生工作流尚无本轮结果。AutoGain旧事件采集、停止/异常恢复、增益计算/判定、事件策略和结果格式并未迁移；本项只是对配置点展开和Run绑定元数据的本机回归，不证明现场工作流或硬件行为。见[AutoGain计划手册](AUTOGAIN_SCAN_PLAN.md)、[扫描计划GUI手册](SCAN_PLAN_GUI.md)及[重构状态](REFACTOR_STATUS.md)。

## 71.213 阈值扫描分析的 Ubuntu24 候选交付（2026-10-22）

Ubuntu24 full 候选 `build-packages/getdaq-ubuntu24-20261022-71-211-hitpattern-scan-candidate/` 已增量更新当前源码构建的 `getdaq-catalog`。新增候选收据类型 `catalog_threshold_analysis`，与 `catalog_plain_tcp` 分开记录；替换catalog程序时会失效并清除旧catalog回归回执，避免把阈值分析误标为TCP传输测试。该缺陷曾令文档刷新器误读继承/覆盖后的收据，本次修复并补入回归覆盖。

验证：Ubuntu24 rootfs loader对源码构建CLI回归通过，之后刷新器在候选中以Ubuntu24 loader运行依赖检查和 `--help`，完成增量替换；包文档刷新和完整包校验通过，193项清单。最后直接对包内剥离版 `getdaq-catalog` 经同一loader重跑 CLI 回归，320字节合成raw计数正确、拒绝覆盖既有报告，所有fixture目录及raw清理，`raw_remaining=0`。交付二进制SHA-256为 `af915d89d18770fc8ff259ba94a7db08136f2a0b7590f77c4df86bf44c7b10be`。测试驱动的15项Ubuntu24收据测试和19项Windows构建入口合同均通过。

此Ubuntu24用户空间运行在WSL2共享Linux内核，不是独立Ubuntu机器验收；未验证Windows/macOS、实板、现场Run及物理阈值判据。全套重构仍未完成。

## 71.212 阈值扫描绑定归档后的自动 hit-pattern 汇总（2026-10-22）

`getdaq-catalog --analyze-threshold-scan SCAN BINDINGS MFM_COBO_NUMBER OUTPUT_JSON`为已停止并封存的 AGET Threshold 扫描新增只读离线分析。程序重验每点 binding、完整系统工件、配置/工件摘要和封存catalog；阈值码须与点工件中 `Reg1/GlobalThresholdValue` 及所选AGET通道细分阈值一致。通过后仅打开计划目标来源下已由catalog清单绑定的 `.graw` 文件，拒绝符号链接，读取前后核对device/inode/size/mtime/ctime；按CoBo数值ID、AsAd、AGET流式复用原MFM parser累计68通道头hit位，并以不可覆盖方式发布完整JSON。不同文件内没有目标帧时允许跳过，但整点没有目标帧则失败。

计划中的 `target_cobo`（例如`C0`）是来源标识，不等同MFM帧头中的数值CoBo ID，因此命令要求操作者明确给出后者，且不推断映射。报告标记 `mapping_verified=true`、`hardware_loaded_verified=false`；它只证明计划工件与Run归档绑定完整，并报告头位计数，不证明板端读回配置，也不作物理阈值判断。当前输入仅支持直接 CoBo type-1 revision 0..5，不支持layered/compact输入。

GCC 9.4严格C++11构建 `getdaq-catalog` 和 `receiver-catalog-tests`通过。`receiver-catalog-filesystem`回归包含1个已绑定的threshold点、两份80字节合成MFM帧、逐点通道计数、错误CoBo号及输出覆盖拒绝；另验证把manifest标签改成错误阈值并重建有效绑定仍被点工件检查拒绝，截断catalog原始文件时分析失败且不发布报告。目录测试1/1通过，用时约22秒。分析器测试raw夹具总计320字节，由私有测试目录退出时删除；测试运行期间既有catalog文件系统套件另有512MiB稀疏元数据夹具，它没有通过网络传输，套件全部私有目录退出时清理。以上不是实板/真实运行的物理采集验收。目标服务增加的编译依赖只有仓库内已有MFM流解析器及标准库，Qt/第三方依赖未增加。

随后以当前源码在GCC9.4/Qt5.12.8 host full profile构建28/28程序并生成`build-packages/getdaq-linux-host-20261022-71-213-threshold-catalog-full-candidate/`（185项清单）；构建器包校验通过。候选内 `getdaq-catalog` 的CLI单项烟测通过同一320字节夹具，核对报告通道数、重复输出拒绝和测试根清理；证据由`tests/run_threshold_catalog_cli_test.py`写入`build-receiver/threshold-catalog-71-213/linux-host-cli-regression-final.json`，程序SHA-256为`ec4032990f6cfe6730af8f972a652bfe61cc949577c38db08c3c5ef3057046d0`。之后Ubuntu24候选已增量刷新，并在Ubuntu24 loader下通过源码构建版和包内交付版的CLI回归及193项清单校验，详见[验证71.213](#71213-阈值扫描分析的-ubuntu24-候选交付2026-10-22)；不是Ubuntu24全套当前源码重编译。Windows/macOS候选尚未更新。

详细流程和边界见[扫描点与Run归档关联](SCAN_RUN_BINDING.md#6-aget-threshold-离线-hit-pattern-分析)。

## 71.211 Threshold 扫描文件的 hit-pattern 多点汇总（2026-10-22）

`getdaq-inspect --hit-pattern-scan COBO ASAD AGET CODE FILE [CODE FILE ...]`在现有流式头字段计数之上，按操作者显式提供的0..127升序阈值码聚合多个raw文件；同码可重复以汇总同一点的多份分段文件。CSV按阈值码和通道输出命中帧、匹配帧及比例。整轮输入均成功解析后才输出结果；乱序码、坏文件或无匹配目标帧会失败且不产生部分CSV。聚合使用uint64溢出检查。

这是旧`TestController`每个阈值点按通道累计`hitPat_0`结果的离线数据整理迁移，不执行硬件控制、自动逐点Configure/Run或物理阈值判定。命令不读计划manifest、catalog或绑定凭据，所以文件到阈值点的关联仍由操作者确认；输出元数据明确标记该关联未经核验。旧代码本身也保留hit-pattern物理意义待确认注释。

GCC9.4/CMake3.16严格C++11完整数据构建成功；数据套件CTest **6/6通过**（两项loopback测试在许可本机socket后通过）。专项CLI以大小端混合的合成raw验证同码两文件累加、两个点136行输出、具体通道计数及降序输入拒绝；直接 smoke 总量240字节、自动清理。新增核心合并测试验证帧/通道计数以及uint64溢出和空目标拒绝。可复用回归脚本`tests/run_hitpattern_scan_package_test.py`检查单文件、重复阈值码聚合、大小端混合、明确标示人工映射、坏帧/乱序拒绝和临时raw清理；生成并删除280字节合成raw，runner SHA为`44e7bdad5579ed26d217c11fe87091709c33912ab8e6036fc70d323b3cf4fd07`。Ubuntu24刷新入口合同及Windows构建入口测试现为**17/17通过**。

随后从当前源码完整构建Linux host full profile，28/28程序构建成功，109项本地嵌入依赖核对通过；候选`build-packages/getdaq-linux-host-20261022-71-211-threshold-summary-full-candidate/`共185项，程序帮助/启动和package verifier通过。刷新候选文档后复验仍为185项，包内新CLI以小型合成夹具验证通过。包内`getdaq-inspect` SHA-256为`52d2d3506c859b28b2d7b4741a4cbddb462793db7813cde22e51208e1e15e0ee`；运行依赖只有系统C/C++运行库，无Qt、ROOT、Ice或新增第三方库。

将该原生CLI经既有安全刷新器替换进Ubuntu24 full候选后，`build-packages/getdaq-ubuntu24-20261022-71-211-hitpattern-scan-candidate/`共188项并通过manifest/包校验。刷新器在Ubuntu24 rootfs loader下通过依赖闭包和`--help`，另外用该loader对两份共160字节raw执行多文件汇总，核对68行及通道0/67计数；raw由临时目录自动删除。Ubuntu24包中交付二进制SHA-256为`92ca421be9474c0e48e391899442f448149d5da373ebee97edbd4600512a27d2`。它基于71.200候选只增量更新`getdaq-inspect`、复用其余27程序，不是Ubuntu24全量当前源码重编译。

这不验证Ubuntu24专用loader、Windows/macOS、现场raw/计划/catalog点位对应或硬件采集。也不证明hit位的阈值物理意义。项目本地Markdown文件链接检查1574项通过（该检查不校验锚点）。详见[离线检查说明](DATA_SERVICE.md#9-离线检查与异常恢复)。

## 71.210 原生流式 hit-pattern 离线计数（2026-10-22）

`getdaq-inspect --hit-pattern FILE COBO_ID ASAD_ID AGET_ID`现可在无Qt原生数据工具中统计指定CoBo/AsAd/AGET的MFM帧头命中位。仅支持直接type-1 revision 0..5帧；输入按完整MFM帧顺序流读取，最大单帧32MiB、块缓冲64KiB，只读取头内数据，不展开波形、不修改文件。输出CSV列出68个通道各自命中的匹配帧数和帧比例。该模式与DAQ接收面无调用关系，不读取嵌套`0xFF01`、compact type-2帧。

GCC9.4/CMake3.16严格C++11构建 `hitpattern-tests` 与 `getdaq-inspect` 成功；新原生测试以合成大小端帧核对CoBo/AsAd/AGET选择、通道0/7/8/31/67边界、缺帧/截断/type-2及非法ID拒绝。当前数据套件CTest共 **6/6通过**，包含原有MFM合并、GMAT、receiver核心、导出锁与ByteStorage集成。受限环境第一次有两个socket测试被系统拒绝（`socket failed`/`Operation not permitted`）；同一构建在允许loopback后复跑6/6通过。

命令行另以实际候选`getdaq-inspect`处理了一个80字节的临时合成CoBo帧，断言通道0与67各计数一次、通道66为零；原始fixture由私有临时目录自动删除，未留下`.graw`。使用当前源码新构建full Linux host profile **28/28程序**，完整候选`build-packages/getdaq-linux-host-20261022-71-210-hitpattern-full-candidate/`有185项文件清单且验证通过；其中`getdaq-inspect` SHA-256为`11dc7bd5a69562fbe6a0e10985a5e6cac297c2b64f1b02994599f1684b7aee02`，与实际构建产物一致。包内二进制帮助和相同80字节CLI烟测通过；Linux依赖仅系统C/C++运行库，无Qt/Ice/ROOT/CFITSIO数据依赖。完整候选基于Linux/WSL环境，不证明独立Ubuntu部署、macOS、现场数据解析或hit位的阈值物理意义。

源码依据：[`CoBoEvent`旧模型](../source/get/CoBoFrameViewer-release-20210208/src/get/CoBoEvent.h)将72位hit-pattern称为68个实际通道；对应旧Threshold处仍保留字段物理含义待确认注释。因此本命令只报告MFM头位出现次数，不能命名为阈值响应率/噪声率，也不自动关联扫描点或Run。实现与用法见[数据服务手册](DATA_SERVICE.md#9-离线检查与异常恢复)。

## 71.209 Threshold 离线计划生成与完整 Widgets 回归（2026-10-22）

在71.200阈值配置计划器基础上，操作台新增单颗 AGET Threshold 模式：可显式选择 AsAd/AGET，按0..127阈值码区间生成完整系统配置工件。系统实验核心测试按目标芯片ID比较完整参数，覆盖全局3位/通道4位边界、摘要重算及缺失/禁用/非活动芯片和非法范围拒绝。严格C++11的 `system-experiment-tests` 构建及执行通过；qmake CLI按严格C++11选项构建通过。

当前 `getdaq-config` 运行 `tests/system_experiment_integration.py` 共 **64个进程场景通过**，包括三点阈值配置、manifest与逐点SHA、完整配置差异、已有输出拒绝；同时调用现有 `getdaq-system-config` 检查生成工件。该checker来自既有host构建树，本轮未重编checker，故这里只证明本轮CLI生成物通过该现有checker的兼容检查。

使用 `tests/build_qt_control_validation.py` 从当前源码以严格C++11重建完整 `tests/tests.pro` Widgets QtTest（Linux/WSL、Qt5.12.8），**115 passed、0 failed、0 skipped**，耗时52.863秒。新增窗口用例实际调用同目录CLI生成阈值码0/4/8计划，并检查目标信息、manifest及非法步长不产生输出；PeakingTime与Pulser旧用例也随完整套件通过。第一次受限执行的网络错误不作为产品回归失败：只绑定隔离测试运行时允许loopback后，在同一构建上完成全套回归。被监控临时构建/运行目录最大约24MiB，低于512MiB临时目录上限；全程没有生成 `.graw`，低于20GiB限制。完成后已删除临时构建树和集成收据。

构建入口/Windows工作流合同测试 `tests/test_windows_build_entrypoints.py` 为 **16/16通过**。本机从WSL启动Windows Qt5.14.2 `qmake.exe`仍因 `UtilBindVsockAnyPort: socket failed 1` 在构建前失败，因此没有Windows本轮源码构建证据；macOS workflow也尚未在Apple runner执行。Threshold迁移仅生成离线配置，不做旧 `hitPat_0`事件统计、自动阈值判定、板端加载或逐点Configure/Run。同期CoBoEvent源码已解释72位容器和68个实际通道的宽度关系；hit-pattern的阈值物理含义和现场结果格式仍待确认。详情见[阈值计划手册](THRESHOLD_SCAN_PLAN.md)、[扫描计划GUI手册](SCAN_PLAN_GUI.md)和[重构状态](REFACTOR_STATUS.md)。

## 71.208 离线扫描计划窗口完整 Widgets 回归（2026-10-22）

针对71.207新接入的扫描计划窗口，使用当前源码在Linux/WSL宿主的Qt 5.12.8/GCC环境中，以项目`tests/build_qt_control_validation.py`入口、严格C++11重新构建完整`tests/tests.pro` Widgets QtTest。构建收据记录114个编译源文件、277项跟踪输入；测试实际通过 **115 passed、0 failed、0 skipped**，用时52.049秒，覆盖完整窗口套件而非仅扫描计划定向用例。扫描计划窗口通过真实同目录CLI分别生成16点PeakingTime计划和`-1000、-950、-900 mV`三点Pulser计划；manifest值和最后一个工件存在性均核对通过。步长60 mV不能精确到达-900 mV时，窗口显示错误且未创建目标目录。

第一次受限沙箱运行102通过、13失败；失败全部在启动本机服务/回环socket的集成用例，系统报告`Could not create AF_NETLINK socket (Operation not permitted)`。保持同一测试二进制，仅在隔离临时目录中获准访问本机回环后完整复跑通过。监控测试树最高约22MiB（远低于512MiB临时空间上限），没有`.graw`文件，远低于20GiB raw上限；验证结束后整个临时构建/运行树删除。此结果证明Linux Qt5.12.8构建和完整Widgets回归，不代表Windows Qt5.14.2/MinGW、macOS、Ubuntu24原生构建或人工现场操作已验证。

本轮尝试从WSL启动用户Windows Qt5.14.2的`qmake.exe`，仍在启动阶段因`UtilBindVsockAnyPort: socket failed 1`失败，没有产生Windows构建结果。该限制不影响上面的Linux回归。平台及现场未验收范围见[重构状态](REFACTOR_STATUS.md)。

## 71.207 操作台接入离线扫描计划窗口（2026-10-22）

GetDAQ 操作台新增 **Offline scan plan builder...** 菜单入口，可填写完整系统实验工件、显式 CoBo ID、PeakingTime 或 Pulser 参数和新输出目录。窗口通过 Qt `QProcess` 异步调用同目录的 `getdaq-config`，计划生成仍由既有 CLI/核心执行；启动失败会恢复按钮并显示错误。`apps/envoy/envoy.pro` 和 `tests/tests.pro` 已加入新窗口源码，项目不存在独立 `apps/envoy/CMakeLists.txt`。

在 Ubuntu/WSL 的 Qt 5.12.8/GCC 9.3 环境中，操作台及 `getdaq-config` 使用 qmake 构建成功，QtTest 程序构建成功。离屏定向执行 `offlineScanPlanBuilderIsAvailableFromConsole`：**3 passed，0 failed，0 skipped**。测试打开新菜单窗口，确认两类计划选项/按钮可用，保持操作台 `Disconnected`；用完整合成系统工件及带空格的路径从窗口异步调用同目录 `getdaq-config`，成功生成 manifest 和16个 PeakingTime点。随后再次选择相同目标，CLI拒绝覆盖，测试确认原 manifest 字节保持不变。另在测试程序与CLI分离的目录运行同一QtTest，缺失CLI提示分支通过；可执行但失败的子进程错误状态也通过。所有输出都位于临时目录并在验证后删除；只生成小型JSON，不产生DAQ `.graw`。

71.207当轮临时 qmake 构建树已清理；之后的完整 Widgets 回归见[71.208](#71208-离线扫描计划窗口完整-widgets-回归-2026-10-22)。仍未在用户 Windows Qt 5.14.2/MinGW、macOS或平台候选包验证新窗口；合成点计划生成和覆盖保护不能替代人工操作、现场多点扫描或 DAQ 性能验收。功能限制和使用步骤见[操作台离线扫描计划窗口](SCAN_PLAN_GUI.md)。

## 71.206 扫描Run归档整套完整性索引（2026-10-22）

`getdaq-catalog`新增`--summarize-scan SCAN_DIRECTORY BINDING_DIRECTORY OUTPUT_JSON`。它按扫描manifest的连续索引读取`point-NNN.json`绑定，重算绑定内容并重新调用catalog完整性检查；要求每个点的配置/工件摘要、计划摘要、Experiment/Run、session和归档回执仍匹配，Run号及session不得重复。仅全部通过时才独占发布`getdaq.scan-run-index` JSON。发布前重新复验计划、绑定和封存catalog；扫描计划、凭据、catalog及输出目录互相隔离，输出不可覆盖。

Ubuntu20.04/WSL2宿主GCC9.4严格C++11从源码重新配置并构建`receiver-catalog-tests`与`getdaq-catalog`成功。`receiver-catalog-filesystem` CTest **1/1通过**，测试程序报告36项原生检查通过：包括绑定、完整单点汇总、重复输出拒绝、过期凭据拒绝、catalog回执前后不变及原有文件系统保护；CLI帮助显示新入口。集成测试使用一个点对应已封存合成catalog的小型汇总计划；原套件含最多单个512MiB的稀疏`.graw`元数据夹具，不是网络传输负载，按fixture析构清理。临时14MiB构建树在验证后删除；夹具创建的文件均在作用域结束时清理，没有DAQ数据遗留。

这只验证离线归档索引，不是实际完整扫描、多点Run、板端配置读回或自动分析；Gain/AutoGain/Threshold、逐点执行协调和恢复仍未完成。本轮未更新平台候选包。范围及示例见[扫描点与Run归档关联](SCAN_RUN_BINDING.md)。

## 71.204 扫描点绑定已完成 Run 归档凭据（2026-10-22）

在原生 `getdaq-catalog` 增加 `--bind-scan-point SCAN_DIRECTORY POINT_INDEX COMPLETED_CATALOG OUTPUT_JSON`。它先完整调用catalog检查器核验receiver退出、session、配置/工件摘要、封存清单和完成回执，再核验PeakingTime或Pulser计划中的点号、局部工件文件SHA和工件内摘要；只有两类摘要完全相同才写出独立凭据。凭据记录扫描点、CoBo、catalog请求/manifest/完成回执摘要以及catalog请求中的Experiment、Run number和session ID，并明确 `hardware_loaded_verified=false`。输出独占创建且必须在扫描计划及catalog目录外，不改动已封存数据。

严格C++11 GCC 9.4从源重新配置、编译`receiver-catalog-tests`和`getdaq-catalog`成功。`receiver-catalog-filesystem` CTest **1/1通过**，其中包含扫描点正确绑定、绑定错点拒绝、已有输出/根目录目标拒绝，以及绑定前后catalog完成回执保持不变；`getdaq-catalog --help`列出新用法。测试使用既有稀疏catalog文件系统夹具（最大单文件512 MiB），未由绑定逻辑读取DAQ载荷；测试专用目录已清理，未留下自有`.graw`。之后从当前源码构建Linux host `full` profile全部28个程序，嵌入依赖核验109项通过；新候选`build-packages/getdaq-linux-host-20261022-71-205-scan-binding-candidate/`含183项manifest，`--verify-stage`完整性检查和包内新命令help通过。成功构建临时树已删除。

本轮只证明工具对合成元数据/封存夹具的校验和独立凭据写入。没有执行真实扫描采集、板端加载读回、GUI逐点Run协调、失败恢复或物理DAQ；摘要匹配不等于硬件实际装载证明。操作及字段解释见[扫描点与Run归档关联](SCAN_RUN_BINDING.md)。

## 71.203 macOS full-suite 纳入配置扫描 CLI 回归（2026-10-22）

macOS full-suite workflow现在在完整28程序构建及Mach-O架构回执核验后，使用本轮构建产物中的 `getdaq-config` 和 `getdaq-system-config` 执行 `tests/system_experiment_integration.py`，覆盖PeakingTime和Pulser幅值配置计划，并将结果回执上传。该顺序保证集成测试针对同一轮macOS构建，而非依赖Linux/Windows二进制。`tests/test_windows_build_entrypoints.py` 工作流合同16/16通过，workflow YAML成功解析，Markdown本地链接1531项通过。当前运行环境是Linux/WSL，本轮没有执行Apple runner；只能证明CI接线和静态合同有效，macOS原生编译/运行仍需Actions结果。

## 71.202 Linux host full 候选纳入 Pulser 扫描计划（2026-10-22）

Linux host Ubuntu20.04/WSL2环境从当前工作树以 `tools/build-suite.py --profile full` 顺序构建28/28主机程序，嵌入依赖检查109项通过。新候选 `build-packages/getdaq-linux-host-20261022-71-201-pulser-scan-full-candidate/` 按当前源码生成，构建回执完整；候选28个程序全部通过隔离启动/help smoke，`getdaq-config` 与 `getdaq-system-config` 另通过59进程扫描集成，覆盖PeakingTime/Pulser点位、完整配置及摘要和拒绝覆盖。扫描和包smoke回执进入完整184文件manifest，中文文档刷新后候选校验通过。所有测试均无硬件访问、无DAQ raw生成。

第一轮交付尝试的构建回执为failed，日志在ECC服务链接阶段出现未解析析构符号；随后从同一工作树在干净build root单次重建，28程序成功。第一轮失败根因没有单独隔离，不据它声称源代码存在或不存在链接缺陷；第二次完整构建结果是当前候选依据。Ubuntu24另有71.201增量候选，只替换 `getdaq-config`、复用27个旧二进制；两者均不是独立物理机器验收。Windows Pulser集成虽已加入CI，但尚无Actions结果；macOS、旧gSOAP服务/板端、真实双机和持续DAQ吞吐仍未验证。

## 71.201 Pulser 扫描计划刷新 Ubuntu24 候选（2026-10-22）

在Ubuntu24.04.5项目用户空间loader下，对Linux Qt5.12.8/C++11当前源码 `getdaq-config` 与候选内系统工件检查器运行扩展后的 `tests/system_experiment_integration.py`，59个进程场景全部通过，包括三点Pulser幅值计划、完整工件/配置摘要、非目标配置保持、native checker校验及已存在计划拒绝覆盖；回执 `raw_remaining=0`，未访问硬件。随后从71.190完整候选复制出新候选 `build-packages/getdaq-ubuntu24-20261022-71-200-pulser-scan-candidate/`，只替换 `getdaq-config`，另外27程序逐字节沿用基线；刷新回执标记不是全套重新编译。新配置程序在Ubuntu24 loader下通过help及集成运行，完整28程序包smoke通过；59场景回执、28程序smoke收据及更新后的中文手册均进入最终186文件manifest，包完整性验证通过。旧71.190候选保持不变。

候选基于Ubuntu24.04.5用户空间但共用WSL2内核；除本轮替换的配置工具外，其余27个程序来自71.190旧全量构建。它不能证明当前所有源码在Ubuntu24全量重编译通过，也不是独立Ubuntu、Windows、macOS、实体板卡、物理双机或持续DAQ吞吐验收。Windows集成已接入CI但尚未取得新版本运行结果。详细限制见[Ubuntu24验证说明](UBUNTU24_VALIDATION.md)。

## 71.199 PeakingTime 单CoBo选择按ID覆盖回归（2026-10-22）

增强 `tests/system_experiment_test.cpp`：将启用目标 `C0` 放在系统CoBo列表第二位，列表第一位保留另一块有完整参数的禁用CoBo。每个扫描点都与完整预期系统配置比较，只允许目标CoBo活动芯片的AGET/ASTRE peaking-time值变化；未选CoBo及其它全部系统字段必须一致。未知/禁用目标拒绝测试也随目标移动到第二项，避免仅依赖列表顺序的假阳性。

执行 `cmake --build build-ecc/experiment --target system-experiment-tests -j2` 并运行 `build-ecc/experiment/system-experiment-tests`，构建和完整核心测试通过。此次仅为测试覆盖增强，没有硬件访问或raw数据；Windows qmake仍受WSL进程桥错误影响，本记录不表示Windows/Actions平台通过。GetDAQ整体重构未完成。

## 71.198 PeakingTime 计划接入 Ubuntu 24 包与 Windows CI（2026-10-22）

对71.197 Linux host候选中的新 `getdaq-config` 与71.190 Ubuntu24候选内的 `getdaq-system-config` 运行 `tests/system_experiment_integration.py`，通过Ubuntu24.04用户空间loader执行54个真实子进程场景。回归逐点验证16个完整系统配置工件的SHA-256/配置摘要、仅目标CoBo发生PeakingTime变化、其他CoBo保持不变、已有计划拒绝覆盖、输入配置未改变，且不发送硬件命令。测试使用私有临时配置目录，没有生成raw。

`tools/refresh-ubuntu24-program.py`现在要求该测试回执绑定当前runner、待替换配置编译器及候选内原有系统工件检查器的精确SHA，并要求54场景、零数据和Ubuntu24 loader标记，才允许增量替换。候选 `build-packages/getdaq-ubuntu24-20261022-71-190-current-source-full-candidate/` 已基于71.190只替换 `getdaq-config`，其余27个程序原样复用；manifest从180增至182项，刷新回执明确记录 `full_suite_recompiled=false`。候选完整性和28程序Ubuntu24 loader启动smoke通过。

`.github/workflows/native-data-path.yml` 的Windows Qt5.14.2/MinGW原生构建job现于运行库部署后执行相同54场景，并上传PeakingTime回归JSON。工作流合同测试16/16通过；本轮因WSL启动Windows进程报 `UtilBindVsockAnyPort: socket failed 1`，未能在本机运行Windows qmake或该Windows CI，也未刷新Windows程序包。新增工作流需Actions实际运行后才能算Windows原生验收。

该计划仍只生成离线配置，不执行完整逐点ECC Configure/Run；真实实验板卡、Windows Actions、macOS原生及DAQ长时吞吐仍未验收，整体重构未完成。

## 71.197 GetController PeakingTime 单CoBo计划（2026-10-22）

扫描计划CLI现要求明确的 `COBO_ID`，从完整系统配置中只修改该CoBo活动AGET/ASTRE芯片的PeakingTime；其他所有CoBo字段保持原样。manifest增加 `target_cobo`。纯C++11系统实验测试覆盖选择和拒绝未知/禁用板卡、基准合法性、芯片型号各自的16点表、配置摘要变化和工件往返。`tests/system_experiment_integration.py`逐点比较目标CoBo除peaking-time以外的完整对象及未选CoBo完整对象，核对16个工件SHA与配置摘要，并由原生 `getdaq-system-config inspect` 重新检查；54进程场景通过。

当前源码Linux host full候选含28程序/181项清单，包内28程序help/offscreen启动smoke及扫描CLI集成均通过，包校验和文档刷新通过。运行时发现并修正了一处命令行参数索引错误：错误输出目录在自动删除的临时回归目录中被发现，修正后对完整临时目录创建/manifest/目标板绑定执行正向集成测试；没有触碰用户配置或原始数据。测试结束删除了私有配置缓存、构建目录和临时扫描工件，未生成raw。

逐点ECC完整Configure/Run协调及恢复策略仍未实现：现有ECC Run固定绑定系统配置摘要，不得在采集期间替换。事件数与分析输出策略、Gain/AutoGain/Threshold流程、Ubuntu24/Windows/macOS新版本构建、实板和持续DAQ吞吐验收仍待。整体重构未完成。

## 71.195 GetController PeakingTime 离线扫描计划（2026-10-22）

新纯C++11 `BuildPeakingTimeScan` 从完整系统实验工件生成16个PeakingTime配置变体；按每颗芯片的明确型号分别采用 AGET/ASTRE 表，验证每个修改后的芯片参数及完整系统配置，并为每一点重算配置工件与摘要。严格 C++11/GCC9.4 `system-experiment-tests` 通过，覆盖16点顺序、两种芯片表、来源摘要保留、相邻执行摘要不同、编码/解码往返、无启用芯片拒绝及非法基线拒绝。

Qt5.12.8/C++11 `getdaq-config` 构建通过。以已有完整实验工件运行CLI实测生成16个JSON和manifest；逐一核对文件SHA-256及配置摘要，重复使用已存在目标目录被拒绝。`tests/system_experiment_integration.py`已扩展为逐点读取16个工件、核对两种芯片表、使用原生系统工件检查器重解码，并确认已有计划未被修改；完整54个进程场景通过。新Linux host full候选含28个程序/181项清单，全部28程序启动smoke及候选包配置工件集成测试通过。输出由私有临时目录准备后发布；没有发送硬件命令或启动采集，测试无raw数据，临时输出已删除。手册：[GETCONTROLLER_SCAN_PLAN.md](GETCONTROLLER_SCAN_PLAN.md)。

这是旧扫描功能的离线配置计划基础，不是GetController完整替代：逐点完整Configure/Run编排、事件数策略、波形/FWHM结果输出、Gain/Threshold/AutoGain流程、Ubuntu24/Windows Qt5.14.2更新包运行、macOS原生、真实板卡和跨平台验收仍未完成。整体重构继续进行。

## 71.194 GetController自动扫参工作流迁移盘点（2026-10-22）

对照`GetController-release-20210208/src/get/WorkspaceManager.cpp`和`TestController.cpp`梳理旧单板自动测试：工作区默认类型为`Default`、`StdAcquisition`、`Acq2pMode`、`PeakingTime`；测试线程代码另有`Gain`、`AutoGain`、`Threshold`、`InjectTest`、`hittedChannel`等分支，未因源码分支存在就假定它们属于可配置/仍受支持入口。当前GetDAQ已迁入AGET配置与校准子流程及波形离线FWHM查看，但**尚无旧GetController完整扫参工作流的对应替代**。

关键兼容差异：旧PeakingTime循环每收够事件就停止DAQ、慢控制下直接改AGET reg1时间索引、清显示缓存后重启，16个索引；源码中的FWHM计算和结果写盘只是注释。Threshold通过`hitPat_0`计数，源码仍有待确认注释，且数组72列而读出计数循环68通道。Gain/AutoGain也会在运行中改脉冲幅度和AGET通道寄存器。新实现不应照搬这些在线写寄存器/清缓存行为。

当时记录为旧源码静态核对，没有实现扫描计划；之后71.195新增了有限离线计划生成器。现场实际使用的测试工作区、阈值/增益结果消费格式及是否继续使用仍需确认。整体重构未完成。

## 71.193 隔离 macOS QtTest 用户目录与缓存（2026-10-22）

审查71.192加入的Core QtTest运行环境时发现，仅设置`TMPDIR`不足以隔离`QStandardPaths`使用的HOME缓存以及配置/数据目录。macOS工作流现将`HOME`、`XDG_CACHE_HOME`、`XDG_CONFIG_HOME`、`XDG_DATA_HOME`、`XDG_RUNTIME_DIR`和`TMPDIR`全部设在`build-platform/macos-full-suite/qt-test-temp`下，测试启动时建立私有目录并把runtime目录设为0700。原有`if: always()`清理helper仍对这一整棵树统计`.graw`数量/字节、删除目录并写回执；因此测试失败时也不会留下runner用户缓存。

`tests/test_windows_build_entrypoints.py`确认所有路径在受清理根内且runtime权限命令存在；Windows/macOS入口合同16/16、macOS清理helper4/4、host delivery17/17通过，workflow YAML解析通过。随后使用本机Qt5.12.8严格C++11重建`xcfg-shared-tests`，在HOME/XDG/TMPDIR全部隔离的环境执行Core 151/151；调用同一macOS清理helper清除模拟临时树，回执为`status=complete`、0个raw、0字节、根目录已删除。该验证仅覆盖Qt对隔离路径的使用和清理helper在Linux上的行为；没有启动macOS Actions，不证明Apple runner已执行。本轮无DAQ raw和新增大文件。

## 71.192 macOS CI 补入配置核心 QtTest（2026-10-22）

复核原生macOS host-suite后发现，已有Widgets和ECC客户端QtTest，但缺少Ubuntu24 full runner执行的XCFG配置核心测试。现`.github/workflows/native-data-path.yml`的macOS job新增 `tests/xcfg_shared.pro` 严格C++11构建及运行，将Core结果要求固定为151通过、0失败、0跳过，并把构建收据、源码摘要、构建/部署日志和测试结果上传；保留已有Widgets 114项固定门槛、ECC客户端零失败门槛、临时目录清理和receiver/Sentry联调。`tests/test_windows_build_entrypoints.py`同步核查三套QtTest均有build/run/证据顺序。

本机Linux Qt5.12.8按相同严格C++11 runner构建Core测试并运行，151/151通过；运行时为防沙箱禁止写入用户缓存，使用 `/tmp/getdaq-macos-core-qttest/home` 隔离HOME，专属tmp和构建目录在测试后清理。Windows/macOS入口合同16/16、macOS清理4/4、host delivery17/17通过，workflow YAML解析通过。本机不是Apple主机，本轮没有运行GitHub Actions，因此新增门槛尚无macOS原生执行证据。没有生成DAQ `.graw`，整体重构未完成。

## 71.189 Linux host 当前源码 full 候选与接收导出回归（2026-10-22）

在Ubuntu 20.04/WSL2 host使用GCC 9.4、Qt 5.12.8、CMake 3.16.3，从当前工作树执行`python3 tools/build-suite.py --profile full --build-root build-delivery/linux-host-71-189 --jobs 2 --stage build-packages/getdaq-linux-host-20261022-71-189-current-source-full-candidate`。构建收据绑定503项输入并完成28/28程序；新候选180项文件清单由`tools/build-suite.py --verify-stage`验证通过。候选包28个程序的隔离启动smoke通过：`build-receiver/linux-host-71-189/package-smoke.json`，无硬件访问、无raw payload。

针对最新`apps/datareceiver/exporter.cpp`，使用GCC 9.4/C++11另建`build-receiver/linux-host-71-189/`并执行CTest，5/5通过（mfm-merge、gmat-conversion、receiver-core、exporter-lock、byte-storage-integration）。普通沙箱两项loopback测试因socket创建被拒而在断言前失败；相同构建在允许本机回环的执行中通过。

随后使用**候选包内的`getdaq-receiver`与`getdaq-sentry`**运行`tests/receiver_integration.py`，覆盖TCP分片/两种端序、多观察端精确帧、落盘与seal、同目录互斥、32MiB慢观察者隔离、截断/非法长度失败、Sentry活进程及退出后状态、离线归档和更改后文件拒绝。集成输出及哈希绑定回执：`build-receiver/linux-host-71-189/receiver-integration.log`、`receipt.json`。最高合成raw 33,554,432字节，低于20GiB限制；fixture使用临时目录，成功退出后自动清理。本轮没有硬件访问，也未宣称物理网络或吞吐结果。

历史候选目录`getdaq-linux-host-20261022-71-189-current-source-full-candidate`已于2026-10-23清理；WSL2 host构建和loopback只证明该工具链/宿主内核下当时源码的构建、启动与有限数据面语义，不替代独立Ubuntu24/macOS、物理双机、真实板卡或长时间持续DAQ验收；整体重构仍未完成。

Ubuntu24当前源码全套本轮没有产生新结果：常规入口的Windows WSL桥在启动时报告`UtilBindVsockAnyPort: socket failed 1`；对已准备rootfs直接启动私有mount namespace则因`unshare: Operation not permitted`在挂载/构建之前退出。未改动rootfs、未启动编译，未生成数据。Ubuntu24最近可用的当前源码整套证据仍为71.183/71.182候选；71.186之后的receiver导出锁改动尚未取得Ubuntu24新候选结果。不要将旧套件/包冒充为71.189当前源码验收。

## 71.190 Linux host 71.189 候选四路 16 GiB 接收/写盘/封存回归（2026-10-22）

对71.189 full候选中的实际`bin/getdaq-receiver`运行`tests/receiver_sustained_load.py`，四个独立localhost接收进程各接收4 GiB，合计 **17,179,869,184字节（16 GiB）**，每路1,048,576个4KiB合成帧，并按每1GiB轮转，共16个`.graw`。每路发送端摘要与轮转文件逐字节读回的完整SHA一致；字节/帧数、自然EOF、正常退出和持久seal均通过。被测候选接收器SHA256为`90144a4ca0ad64121ac7a66fbd58fc49e73120abd174401183b903032170c8ba`。总墙钟28.20秒包含合成发送及全部文件SHA读回；单进程峰值RSS为20,884KiB。

测试回执在`build-receiver/linux-host-71-189-current-16gib/receiver-load-gobm3e4v/`。清理回执记录删除17,179,869,184字节、16个文件、剩余0个；随后又核查本次证据目录及整个工作区没有`.graw`残留，接收器进程已退出。第一次受限沙箱尝试在创建监听socket时即失败、尚未写入payload；其0字节私有失败目录已删除。正式回归只在允许本机loopback的权限下执行，合计payload仍低于用户20GiB上限，磁盘启动前有1GiB保留空间。

该负载在单一WSL2/Linux host上用Python合成帧并通过localhost运行，结果回执显示数据量未超过主机物理内存；28.20秒不能换算成生产吞吐，也不证明真实网卡/磁盘、物理双机、实板、多CoBo或长期零丢失能力。它加强的是71.189精确候选二进制的四路大容量接收、轮转、完整读回、封存与清理证据。整套重构仍未完成。

## 71.191 Ubuntu24 当前源码 full suite 与候选重建（2026-10-22）

在经官方签名/SHA校验的 Ubuntu24.04.5 项目用户空间中，通过仓库标准 WSL runner 对本轮工作树执行`tests/ubuntu24_platform_validation.py --stage all --work /build/getdaq-platform-71-190 --jobs 2`。使用 GCC 13.3.0、CMake 3.28.3、Qt 5.15.13；full profile **28/28程序**完成，构建回执记录951项源码/测试/工具输入。Core QtTest **151/151**、Widgets QtTest **114/114**、原生 CTest **22/22**，以及来源迁移、配置事务、receiver transport、安全、失败handoff窗口场景均通过；平台runner共23条构建/测试命令，全部退出码0。

接收与交接测试合计记录的synthetic wire为**1,073,799,755字节**，其中四路接收1GiB、transport夹具49,739字节；失败handoff窗口另产生8,192字节raw。runner、逐场景清理收据及复查确认`raw_remaining=0`，工作区回执中没有遗留`.graw`，低于用户20GiB限制。

候选包`build-packages/getdaq-ubuntu24-20261022-71-190-current-source-full-candidate/`由这次Ubuntu24 full构建的精确二进制生成，28个程序SHA与构建回执核对，180项完整文件清单通过。包内28个程序使用项目Ubuntu24 loader执行帮助/启动smoke通过，未访问硬件、未生成raw。精简构建/平台收据、QtTest/CTest输出、传输/迁移/事务/交接场景回执、日志摘要及包smoke见`build-receiver/ubuntu24-71-190-evidence/`。

此Ubuntu24用户空间仍与WSL2共用内核，不是独立Ubuntu主机验收；smoke与合成回归也不代表macOS、物理双机、真实现场旧服务/板卡或生产DAQ持续吞吐。整体重构未完成。

## 71.188 Windows 配置发布器文件提交（2026-10-22）

审计发现 `SystemConfigurationPublisher::WriteNew` 在Windows上只有`QFile::flush()`，而Unix分支还会对文件内容及只读权限执行`fsync`。现在Windows分支在写入/flush后及权限设置后调用CRT `_commit(file.handle())`；调用失败会停止发布并报告错误。目录项提交与真实断电持久性仍不作保证。

使用用户Qt 5.14.2/MinGW 7.3 x86_64原生Windows工具链，以严格C++11构建现有配置发布器QtTest并执行3个发布场景：真实旧XCFG发布/全包读回/拒绝覆盖、来源变化拒绝发布及无效profile不留目录，结果 **5通过、0失败、0跳过**（含初始化/清理）。临时测试源仅把`QFINDTESTDATA`查找替换为同一份本地XCFG与profile的绝对路径，测试断言不变。证据位于`build-windows-qt5142-20261022-source-migration/windows-publication/`，receipt绑定生产源和测试程序摘要。测试只写小型配置包，未生成raw，Qt临时目录由`QTemporaryDir`清理。

随后使用同一Qt/MinGW套件从本轮当前源码构建完整Windows control profile **8/8程序**及Widgets/ECC客户端QtTest。原生Windows QtTest为Widgets **113通过、0失败、1项POSIX目录锁用例跳过**，ECC客户端 **64通过、0失败、0跳过**。runner记录 raw 残留0个/0字节、20GiB限制未触发，并已删除Windows私有临时根；摘要和清理回执在`build-windows-qt5142-20261022-publication-sync/qt-tests/native-windows-results/`。

部署新候选`build-packages/getdaq-windows-qt5142-20261022-71-188-publication-commit-candidate/`后，SDK-free PATH帮助、离屏启动、Windows原生GetDAQ窗口及包清单校验通过，候选共280个清单文件；`python3 tools/deploy-windows-from-wsl.py --verify <候选目录>`通过。候选内用户手册同步到当前状态并通过刷新前/后包校验。Windows完整构建、测试及部署收据由`build-windows-qt5142-20261022-publication-sync/`和上述source-migration目录提供。

这证明当前Windows编译/运行路径可用及发布结果可读回，不证明掉电后的文件/目录持久性、ACL继承、独立干净Windows或macOS验收。接收器/DAQ数据路径未修改；整体重构仍未完成。

## 71.187 Windows 配置路径迁移核心与中断恢复回归（2026-10-22）

使用用户提供的 Windows Qt 5.14.2/MinGW 7.3 x86_64 工具链，从当前源码按严格 C++11 构建配置核心 QtTest 和原生进程探针。核心 QtTest 为 **150通过、0失败、1跳过**；唯一跳过项是 POSIX 目录权限夹具，Windows 需独立 ACL 测试，本轮没有把它计作通过。完整结果保存在 `build-windows-qt5142-20261022-source-migration/windows-core-qt-test.txt`，程序/输入SHA和Qt版本见同目录构建回执。

Windows 原生 NTFS 进程回归另通过12个中断后的原始/候选方向恢复场景，以及两个活跃写者场景；结果为 `build-windows-qt5142-20261022-source-migration/migration-evidence/migration-pprumvua/result.json`。测试只用私有小型 XML 夹具，无 `.graw`。QtTest启动脚本从WSL访问工程时输出UNC当前目录提示，但通过`pushd`映射目录后正常执行；QtTest结果文件完整，Windows进程退出码为0。

此项只补充配置核心在用户当前Windows/Qt工具链上的回归，不等同独立干净Windows安装、macOS、旧现场服务、实体板卡、物理双机或持续DAQ吞吐验收，也没有运行完整Windows Widgets/ECC套件。整体重构未完成。

## 71.186 在线导出帧复制移出发布锁（2026-10-22）

检查 `apps/datareceiver/exporter.cpp` 发现，之前虽已把 `send()` 移到发布锁外，`Publish()` 仍在持锁状态创建共享消息、分配 `size+8` 缓冲并复制整帧。现在完整消息副本只分配/复制一次且发生在锁外；锁内只遍历当前连接、应用既有每客户端字节和消息队列限制、入队共享指针或请求断开。若单帧连一位客户端的配置预算都装不下，则锁内标记观察客户端断开并跳过副本创建。原小端uint64长度头、最多八个观察者、每客户端4096排队消息和慢观察者隔离策略未改变，接收器本身没有新增依赖或Qt路径。

使用GCC 9.4、严格项目C++11设置构建 `exporter-lock-tests` 与 `getdaq-receiver` 成功。普通受限环境执行 `exporter-lock` 时在测试开始阶段因不能创建socket而失败；随后对同一构建执行只绑定 `127.0.0.1` 的loopback测试，`exporter-lock` 1/1通过。它覆盖send调用被人为暂停时第二次Publish能并发完成、旧帧长度头/顺序/字节保持、超预算帧断开观察者但不触发worker错误、工作线程accept故障关闭客户端、重置客户端不触发SIGPIPE。再以当前接收器运行 `tests/receiver_integration.py`：TCP分片/合并、多观察者逐帧精确导出、原始文件和seal核对、32MiB慢客户端自动断开及raw SHA一致、截断和非法尾帧错误状态均通过；runner自动清理各自私有临时目录。最高单场景32MiB，远低于20GiB；该证据无吞吐性能对比，也不能证明真实DAQ性能。临时构建曾位于`/tmp/getdaq-exporter-publish-lock`，复核通过后已删除，测试payload也已清理。

## 71.185 复核旧 MuTaNT 对齐命令范围（2026-10-22）

对收到的GetBench源码做定向静态核对：`GetEccCmd.cpp`中的`mutant-align`调用当前MuTaNT `AlignmentServer::alignDelayTDC`并列出槽位结果；`mutant-align-multishelve`调用当前MuTaNT三步跨机箱API。`AmcSlotManager::findSlotOfCoBo`虽然传入单个CoBo ID，但会切换该CoBo反馈链路、遍历全部MuTaNT AlignmentServer结果、要求唯一槽位并恢复Normal；异常路径不保证执行恢复操作。

据此澄清新旧功能边界：新`SystemAlignmentClient`按完整拓扑执行并由System Prepare协调；独立`getdaq-align`仍是受预检/审计保护的工具。单板维护重准备是为保留其他板和已完成对齐而执行的完整CoBo Prepare，不等于TDC或跨机箱realignment。当前未实现可证明只影响选定CoBo的TDC入口，因此没有新增误导性的单板“对齐”按钮。同步修订[旧 ECC 命令迁移表](LEGACY_ECC_COMMAND_COVERAGE.md#对齐命令的范围源码复核-2026-10-22)、[系统对齐手册](SYSTEM_ALIGNMENT.md)和[剩余验收清单](REFACTOR_STATUS.md)。

这是源码/文档范围复核，不是新编译或测试；没有执行硬件命令、传输数据或生成raw，也不增加板端算法/物理对齐证据。

## 71.184 Windows Qt5.14.2/MinGW 当前源码构建与三维查看器验证（2026-10-22）

使用用户实际安装的`D:\Software\QT\5.14.2\mingw73_64`和MinGW7.3 x86_64，从当前源码严格C++11构建8个control程序。Windows原生QtTest：Widgets **113通过、0失败、1项POSIX目录锁用例跳过**，ECC客户端**64/64通过**；runner清理收据为0个/0字节raw，20GiB硬上限未触发。构建和测试的精简输入/二进制/结果摘要保存在`build-receiver/windows-71-184-evidence/`。

候选`build-packages/getdaq-windows-qt5142-20261022-71-184-current-source-candidate/`部署对应Qt/MinGW运行库，初始280项清单通过；八个程序SDK-free PATH帮助、Qt离屏启动和GetDAQ Windows原生窗口检查通过。查看器在该Windows运行包内实际执行`--surface-smoke`，鼠标拖动前后图像96,353像素不同。三维曲面验证不访问设备、不传输DAQ数据、不生成raw。

这是用户当前Windows机器和本机工具链验证，不等同独立干净Windows安装、macOS、现场服务/板卡、双机网络或持续DAQ吞吐验收。Windows目标只包含control程序；DAQ receiver仍为独立原生程序。整体重构未完成。

## 71.183 Ubuntu24 当前源码 full suite、平台候选与清理（2026-10-22）

在项目隔离的 Ubuntu24.04.5 用户空间（GCC13.3、Qt5.15.13、CMake3.28.3）使用当前源码执行 `--stage all`：full profile 28/28程序构建成功，配置Core QtTest **151/151**、Widgets QtTest **114/114**、原生CTest **22/22**；配置迁移/事务、失败handoff窗口、receiver传输和负载安全、四路接收回归全部成功。四路负载为1GiB，整轮合成wire共1,073,799,755字节，低于20GiB；平台回执记录`raw_remaining=0`。过程绑定当前源码/测试输入和实际程序摘要；精简回执与必要日志位于`build-receiver/ubuntu24-71-182-evidence/`。

候选`build-packages/getdaq-ubuntu24-20261022-71-182-current-source-full-candidate/`由本次Ubuntu24精确构建的28程序组成，180项manifest完整性通过；包内28程序经Ubuntu24 loader help/启动检查通过。包内查看器还在Ubuntu24 Qt5.15.13下通过离屏合成68×512曲面绘制和鼠标拖动检查，画面变化96,353像素。该烟测未生成raw。Ubuntu用户空间与本机共享WSL2内核，不能代替独立Ubuntu机器验收。

全量临时构建树（约335MiB，含对象文件）和测试运行目录已删除，仅保留约2.8MiB的精简证据。与Linux host 71.182候选一起，最新 `.graw/.gmat` 查看器已进入当前Linux/Ubuntu24候选；Windows Qt5.14.2候选还未包含71.181曲面更新。macOS原生、独立部署机、物理双机、旧现场服务/板卡和持续物理吞吐仍待验收，整体重构未完成。

## 71.182 Linux host full profile 刷新离线查看器候选（2026-10-22）

在当前Linux/WSL2 host、GCC9.4、Qt5.12.8、CMake3.16.3下，使用当前源码构建器从头构建full profile，28/28程序成功，构建回执绑定498项输入，精简回执保存在`build-receiver/linux-host-71-182-build-receipt.json`。新候选`build-packages/getdaq-linux-host-20261022-71-182-current-source-full-candidate/`从本轮精确二进制生成，初始180项完整manifest校验通过；之后包内28程序隔离help/启动smoke全部通过，执行`getdaq-waveform-viewer`时使用Qt offscreen，最终没有生成raw。receiver、inspect、merge、matrix和catalog五个程序的`ldd`均无Qt/Ice/yaml/Boost/curl动态依赖。Windows构建入口契约测试16/16通过；临时255MiB构建树归档回执后删除。

此为共享WSL2 Linux host的构建和包内启动证据，不是独立Ubuntu24机器、macOS、Windows、实体板卡、物理双机或持续DAQ吞吐验收；full profile此次未重跑CTest/完整Widgets套件，Ubuntu24/Windows/macOS候选尚未包含71.181查看器改动。整体重构未完成。

## 71.181 `.graw`/`.gmat` 查看器补回可旋转三维曲面（2026-10-22）

旧 CoBoFrameViewer 的可选 QwtPlot3D 曲面模式现在可在离线波形查看器中查看当前 AGET 的桶号×通道×信号值。曲面使用当前选中的通道；未选通道按零值绘制，以保留旧通道掩码语义。鼠标左键拖动可旋转视角，PNG导出会渲染当前曲面。实现只在已有 Qt Widgets 查看器中用 QPainter 栅格绘制，不使用QwtPlot3D、Qt OpenGL绘制API或ROOT库，也不新增这些库的直接链接依赖；Qt自身Gui平台依赖随现有Qt Kit提供。功能不进入DAQ接收程序。

Linux/WSL2、Qt5.12.8、GCC9.3、严格C++11下，从当前源码重建查看器成功；`--help`和离屏`--surface-smoke`通过。烟测用内存合成的68×512波形渲染并注入鼠标拖动事件，旋转前后有96,353个像素变化。未生成`.graw`/`.gmat`。文档本地链接、Windows构建入口契约测试通过。此次针对GUI绘图的smoke不等于完整Widgets套件重跑；前一版多通道数据读取和导出测试见71.180。真实实验文件、人工视觉布局检查、Ubuntu24/Windows新候选及macOS原生构建仍待验收，整体重构未完成。

## 71.180 波形查看器恢复 AGET 多通道叠加浏览（2026-10-22）

根据旧 `CoBoFrameViewer` 的 AGET 通道叠加用法，查看器现在默认同时显示所选 AGET 的68条通道，可用多选窗口选择子集；颜色图例随窗口宽度调整列数。`GrawFrameReader::ReadChannels` 对同一 `.graw` CoBo帧只进行一次磁盘读取和解码，稀疏type-1帧省略没有样本的通道；type-2保持四个AGET完整性检查。`.gmat`按所选通道逐条读取4KiB样本。多通道PNG显示全部曲线，CSV按通道区分每行数据；脉冲指标限定单通道。

在Linux/WSL2、Qt5.12.8、GCC9.3、严格C++11下，波形查看器独立构建、`--help`和offscreen窗口启动通过。`getdaq-tests`最终完整Widgets回归 **114通过、0失败、0跳过**；启用localhost后全量测试完成。普通沙箱下同一套件有13项因拒绝本机TCP监听失败，放行loopback后这些用例通过。新增断言覆盖type-1两条通道和稀疏缺失通道、type-2多通道解码、重复/空选择时输出清空、多通道CSV每条记录的通道字段。测试夹具均为QTemporaryDir中的小型MFM文件，未创建大体积DAQ数据，也没有访问真实设备。

尚未基于此改动刷新Ubuntu24/Windows/macOS交付候选，也没有使用真实实验文件作差异比对；启动smoke只验证窗口可运行，不代替人工检查全部68条曲线的绘图布局。此项仅恢复旧GUI的多通道浏览能力，不代表ROOT/FITS/3D/统计功能或整个重构已完成。

## 71.175 当前源码 Qt Widgets 全量回归（2026-10-22）

在 Linux/WSL2、Qt 5.12.8、GCC 9.3 环境下，从当前源码重建 `tests/tests.pro` 的完整 `getdaq-tests`（严格 C++11）。以独立临时 `HOME`/Qt 缓存目录运行，避免访问受限沙箱中的用户缓存；从 `tests/` 目录按项目约定启动，并在允许本机 loopback 的环境运行完整套件。结果为 **112通过、0失败、0跳过**，退出码0，包含新增 `waveformViewerComputesLegacyPulseMetrics`。

此前普通沙箱中配置事务无法在用户HOME缓存目录创建私有锁目录，且本机TCP监听受限；将缓存重定向到临时HOME后，除13项需本机服务监听的场景外其余99项通过；同一测试程序在允许loopback环境完整通过。全套测试使用的合成夹具由临时目录管理；检查本次独立构建目录没有 `.graw`，测试结束后相关自动临时目录已消失。没有执行大文件传输或硬件访问。临时构建与日志已删除。

这是 Linux/WSL2 当前工作树的 Widgets 回归，不是 Ubuntu24 全量候选重建、Windows Qt5.14.2、macOS、实板、物理双机或持续DAQ吞吐验收；整体重构仍未完成。

## 71.174 离线波形查看器迁移旧版 FWHM 与 peaking time（2026-10-22）

对照归档 CoBoFrameViewer 的 `Channel::fwhm(0.1, 0.5, 0)` 和 `Channel::peakingTime(0.1, 0.05, 0)`，将旧版已启用的脉冲宽度/峰值时间计算接入当前 `getdaq-waveform-viewer`。结果按bucket离散差计算，界面报告桶数和按旧采样频率0.1 GHz换算的纳秒值。实现保留旧 `uint16_t` 比较阈值截断行为，同时对乱序稀疏样本按桶排序；重复bucket、无正向峰值或缺阈值交叉明确报不可计算，不复现旧代码对`find_if`缺失交叉点的无效迭代器处理。

Qt5.12.8/GCC9.3/C++11下，波形查看器独立目标构建、`--help`及offscreen窗口启动smoke通过；临时构建目录删除。当前Qt Widgets测试二进制定向运行`waveformViewerComputesLegacyPulseMetrics`为3/3（含fixture初始化/清理），覆盖旧稠密值、奇数峰值下阈值截断、乱序稀疏bucket、重复bucket、无正脉冲和无效阈值；临时测试根`.graw`为0。平台门槛将Widgets用例总数从111调整到112，runner合同16/16通过。尝试在普通沙箱跑全Widgets套件时为99通过、13失败；失败场景在启动本机网络测试服务时报错，未把该次当作全套通过，需在允许loopback的环境重跑。此轮尚未重建完整Linux/Ubuntu24/Windows候选，Windows Qt5.14.2与macOS仍待构建验证。无真实DAQ输入、硬件访问或raw生成。

## 71.173 新交付包严格校验完整文件清单（2026-10-22）

通用 `build-suite.py --stage` 现在在新包的manifest写入`complete_file_inventory: true`；`--verify-stage`除了核对列出的摘要，也要求包内每个普通文件都有清单摘要，并拒绝符号链接和非普通manifest。这样后续新增的测试回执若没有补入清单，会直接导致校验失败；旧manifest没有该标志时保留原有逐项校验兼容行为。

回归`tests/host_suite_delivery_test.py` **17/17**通过，覆盖新包未登记文件和符号链接拒绝；Python语法检查通过。Ubuntu24当前候选此前实际有一份包内启动smoke回执未计入清单，此问题已补入manifest并将候选升级为完整文件清单模式；`build-suite.py --verify-stage`确认候选190项文件全部与manifest一一对应，没有未登记文件或清单中的缺失文件。此修改只加强包清单校验，不改动候选程序、运行库或DAQ数据路径。

## 71.172 Ubuntu 24.04 当前源码全套重建、回归与候选包（2026-10-22）

在项目专用 Ubuntu 24.04.5 用户空间中，用当前工作树从源码重新构建 `full` profile 的28个程序；构建收据绑定498项输入。当前 Core QtTest **151/151**、Widgets QtTest **111/111**、原生 CTest **22/22** 全部通过；配置来源迁移、配置事务、接收协议/传输边界、接收器清理安全、四路接收和失败交接窗口验证均通过。四路接收使用四份各256 MiB的确定性合成数据，总计1,073,741,824字节；整轮回执累计合成wire数据1,073,799,755字节。runner为测试raw设置20 GiB硬上限；收尾`raw_remaining=0`，检查构建工作目录也没有残留`.graw`。

候选 `build-packages/getdaq-ubuntu24-20261022-71-172-current-source-full-candidate/` 由本轮完整构建收据中的28个精确二进制生成，程序摘要与构建收据绑定。28个包内程序经Ubuntu24加载器运行隔离帮助/启动检查，全部通过；GetDAQ以offscreen平台生成并核验启动截图。验证包记录位于候选的`validation/ubuntu24-71-172/`。此处Ubuntu24.04.5是项目专用用户空间，Linux内核仍是WSL2宿主共享内核；结果不能替代独立Ubuntu主机安装，也不代表实体硬件、两台物理主机网络或持续DAQ吞吐验收。

本轮失败的第一次尝试仅因runner仍要求旧Widgets总数106而停止；检查原生QtTest文本结果确认当前源码实际为111/111。更新门槛后重跑完整验证并通过，不把旧失败回执当作成功证据。

## 71.171 当前源码 Windows Qt 5.14.2/MinGW 重建与原生回归（2026-10-22）

使用用户提供的 `D:\Software\QT\5.14.2\mingw73_64` 和匹配的 MinGW 7.3 x86_64，从当前源码严格 C++11 构建8个操作/配置程序；构建回执绑定499项输入。Windows原生 QtTest：Widgets 110通过、0失败、1项POSIX目录锁用例跳过；ECC客户端64/64通过。QtTest私有临时根已删除，清理回执为0个残留`.graw`、0字节，20GiB限额未触发。

候选 `build-packages/getdaq-windows-qt5142-20261022-71-171-current-source-candidate/` 从这8个程序部署匹配运行库；SDK-free PATH帮助、Qt离屏启动和Windows原生窗口smoke通过。另以Windows原生Python运行当前`getdaq-supervisor-query.exe`：受ACL保护的凭据下401身份拒绝只请求一次；正常路径为一次身份GET加一次经范围校验的POST并接受202；POST后断连和500均没有重试且不报告成功；凭据未写入证据，临时目录已删除。候选随包收录构建/QtTest/回环收据和日志，最终297项manifest校验通过。此证据来自用户当前Windows主机及本机工具链，不代表独立干净Windows部署、现场服务/板卡、macOS、真实双机或持续DAQ吞吐验收；Qt仅用于低频控制，DAQ raw仍由独立原生接收程序处理。

## 71.170 当前构建器的Linux host full profile 全量重建（2026-10-22）

使用当前源码和当前`tools/build-suite.py`重新构建`full` profile，GCC 9.4、Qt 5.12.8、CMake 3.16.3；回执绑定498项输入，28/28程序构建成功。Linux host候选`build-packages/getdaq-linux-host-20261022-71-170-current-source-full-candidate/`含28个程序，附回执后manifest 184项；包完整性校验通过。候选内28程序的隔离help/启动smoke通过，没有访问硬件或生成raw；接收、inspect、merge、matrix、catalog五程序无Qt动态依赖。与71.169分析profile相比，共用的receiver、inspect、merge、matrix四个交付二进制SHA逐一相同；同一应用源码版本的CTest10/10已通过。构建器交付/profile测试16/16通过。

该构建绑定了当前构建器修订，也确认full profile不误启用可选ROOT/FITS路径。当前 Linux host 仍运行于共享WSL2内核；Ubuntu24当前源码runner另在chroot前因WSL启动桥错误退出，不能用历史71.143结果替代。此候选不证明独立Ubuntu24/macOS、实板、双机或持续DAQ吞吐验收；整套重构仍未完成。完整回执和smoke证据位于候选`validation/linux-host-71-170/`，本轮临时构建目录在归档后删除。


## 71.169 独立 ROOT/FITS analysis profile 构建、回归与候选包（2026-10-22）

`tools/build-suite.py`新增独立`analysis` profile，显式启用`GETDAQ_BUILD_OPTIONAL_FITS_TOOLS`、`GETDAQ_BUILD_OPTIONAL_ROOT_TOOLS`和`GETDAQ_BUILD_OPTIONAL_ROOT_VIEWER`，一次构建7个程序：receiver、inspect、merge、matrix、FITS导出、ROOT转换和ROOT查看器。该profile只配置`apps/datareceiver`，不构建ECC、Ice或控制台组件；常规`data`和`full`数据接收仍默认关闭ROOT/FITS可选项。构建回执记录Qt、ROOT和CFITSIO版本。ROOT 6.26使用`root-config --prefix`并设置`ROOT_DIR=<prefix>/cmake`，因为该版本没有可用的`root-config --cmakedir`选项。

在Linux/WSL2、GCC 9.4、CMake 3.16.3、Qt 5.12.8、ROOT 6.26/16、CFITSIO 3.47环境中，analysis profile 7/7程序构建成功。数据接收/格式转换/ROOT reader/统计扫描CTest共10/10通过；`tests/host_suite_delivery_test.py`为16/16，依赖隔离检查通过。候选包为`build-packages/getdaq-analysis-linux-20261022-71-169-candidate/`，含7个程序和159项初始文件；附加8项验证记录后清单共167项。包内7个程序隔离启动检查7/7通过，其中ROOT viewer的`--surface-smoke`检查到鼠标旋转前后262,458个像素变化；receiver、inspect、merge、matrix、FITS和ROOT转换器六个程序均通过无Qt动态依赖检查；ROOT viewer按设计链接Qt Widgets。验证私有目录最终没有`.graw`或`.root`残留。

验证收据位于候选包的`validation/linux-host-71-169/`。这个分析包需要目标机自行安装匹配的ROOT、CFITSIO、Qt及其系统运行库；它们未被伪装成自包含依赖，也不应因此加进默认DAQ接收安装。验证主机共享WSL2内核，不是独立Ubuntu 24或macOS运行证据；本轮未找到真实实验文件，合成测试不能证明对历史ROOT/FITS文件及下游消费者的完整兼容，也没有持续DAQ吞吐或实板证据。整套重构仍未完成。

同日尝试用项目现有签名Ubuntu24用户空间runner对当前完整源码执行`--stage all`，但Windows WSL启动器在进入chroot前返回`UtilBindVsockAnyPort: socket failed 1`；该次没有启动构建/测试，没有新Ubuntu24结果，测试预算和raw均为0。最近一次Ubuntu24全套成功结果仍为历史候选71.143，不能代表本轮源码。

## 71.168 当前源码主机套件全量构建与数据面回归（2026-10-22）

以当前源码在Linux/WSL2环境使用Qt 5.12.8、GCC 9.4、CMake 3.16.3从空的`/tmp/getdaq-full-20261022-71-168/`构建目录完成`full` profile，28/28程序构建成功，构建回执状态`complete`并绑定498项源码输入。该套件含4个独立DAQ数据程序、14个原生服务/控制程序、2个配置仓库程序和8个Qt操作/配置程序。生成的构建树保持在`/tmp`，没有把对象文件留入源码树。

从该次构建的精确程序核查`getdaq-receiver`、`getdaq-inspect`、`getdaq-merge`、`getdaq-matrix`及`getdaq-catalog`的动态依赖，5/5均无Qt运行库依赖，验证了DAQ热路径与低频GUI的二进制边界。另从同一当前源码独立开启`apps/datareceiver`测试构建，`mfm-merge`、`gmat-conversion`、`receiver-core`、`exporter-lock`与`byte-storage-integration`共5项CTest全部通过。ByteStorage使用393,353字节确定性合成流，逐字节核对接收文件、字节计数和complete seal，测试自行删除输出；测试后工作目录内`.graw`为0，数据预算低于20GiB。沙箱内初次运行有两项因本机TCP socket权限而未能启动；在允许本机测试网络的环境中重新运行完整CTest后5/5通过。新候选中的28个Linux程序均以隔离临时目录启动检查通过，GetDAQ初始离屏窗口截图通过后删除；没有访问硬件或生成raw。此启动检查运行在本机WSL2 Linux，不视为Ubuntu24验收。

此构建主机共享WSL2内核，并非独立Ubuntu24机器；全量构建和小型数据面回归不证明macOS构建、真实板卡/旧服务互通、物理双机网络或持续DAQ磁盘吞吐。ROOT/FITS为显式可选工具，此full profile未启用，因此它也不验证可选ROOT/FITS目标。完整运行候选目录和manifest见`build-packages/getdaq-linux-host-20261022-71-168-current-source-full-candidate/`；该包的`validation/linux-host-71-168/`保存源码输入/程序摘要构建收据、原生CTest输出、Qt依赖检查和28程序启动smoke回执，方便检查。临时对象构建树位于`/tmp`，验收收据归档进候选后会删除临时构建树，不保留编译对象。

## 71.167 任意周期 CoBo LED 脉冲 Windows 原生回归与当前源码候选（2026-10-22）

使用用户提供的 Qt 5.14.2/MinGW 7.3 工具链从包含任意周期 LED 脉冲入口的当前源码严格 C++11 构建8个 x86_64 Windows控制程序。Windows原生QtTest Widgets为110通过、0失败、1项POSIX目录锁测试跳过；ECC客户端64/64通过。Widgets记录明确包含`coboMaintenancePulseRequiresCancelAndBindsExactPeriod`通过。两测试进程退出码均为0，runner收据均为`raw_files_remaining=0`、`raw_bytes_remaining=0`。

新候选目录为`build-packages/getdaq-windows-qt5142-20261022-ledpulse-candidate/`，manifest记录8个程序、280个文件，Qt版本5.14.2、x86_64，SDK-free PATH帮助命令、Qt离屏初始窗口和Windows原生窗口smoke成功；候选manifest完整性复核通过。实际构建收据和Windows QtTest日志分别在`build-windows-qt5142-20261022-ledpulse/`及`build-windows-qt-validation-20261022-ledpulse/native-windows-results/`，候选供Windows环境直接检查。

这是用户当前Windows工具链、本机运行包及原生QtTest证据，不是独立干净Windows机器安装、完整人工控制流程、远程Linux ECC/真实硬件互通或持续DAQ吞吐验收。LED物理灯态未读回；macOS原生构建、独立Ubuntu24、双物理主机和实板仍未验收。GUI仍只负责低速控制，不读取DAQ raw。整套重构仍未完成。

## 71.166 旧 ECC 任意周期 CoBo LED 脉冲迁入及网络联调（2026-10-22）

将旧控制台`cobo-led-pulse <ledId> <period_ms>`接入Qt单板维护窗口、ECC HTTP协议、NativeEccCoordinator、持久journal和`SystemPreparationClient`。只接受固定活动CoBo、健康Prepared、所有数据服务已停止且互锁通过；LED选择为GPIO 0..15或全板16，周期为半周期毫秒0..`INT_MAX`，0传给旧板端方法停止所选pulser。操作键绑定CoBo、LED和周期，确认框默认Cancel，确认后至多执行一次，不自动重试；板端应答不作为物理灯态验证。新增命令持久编号16，journal冷重开继续执行既有硬件核对保持。

验证：协议、状态机、ECC journal三个核心二进制通过；journal新增Prepared脉冲操作写入、完成和重开回归。Native ECC coordinator双场景runner通过。Linux Qt 5.12.8/C++11生产窗口探针经ECC HTTP服务、两个Supervisor与真实receiver跑完两个硬件API变体；证据检查器确认取消无请求、确认只提交一次、C0/GPIO10/725ms绑定、改周期同ID拒绝、receiver正常停止/封存，GUI没有读raw。两场景合计生成并由runner删除8个合成`.graw`、131,072字节，20GiB预算未超，`raw_remaining=0`；`tests/check_cobo_maintenance_window_evidence.py`通过，文档链接检查通过1465项。证据在`build-delivery/led-pulse-maintenance-coordinator/coordinator-yqima4sz/`和`build-delivery/led-pulse-gui-network/ecc-network-system-gv7jwr_b/`。

验证只覆盖本机Linux模拟硬件、真实ECC/receiver进程和合成数据，不是板卡物理灯态、实机或持续吞吐验收；Windows源码构建与运行包另见[验证71.167](#71167-任意周期-cobo-led-脉冲-windows-原生回归与当前源码候选2026-10-22)。macOS及双物理主机仍未验收。整套重构仍在进行。

## 71.165 旧 ECC LED 脉冲功能映射边界复核（2026-10-22）

对照旧 `GetEccCmd.cpp`、`LedManager.ice`、`LedManagerI.cpp` 与当前 `CoBoLedModify`/`CoBoMaintenanceWindow`：旧`cobo-led-pulse`接受LED编号0..15或16（全部），周期参数是翻转半周期毫秒；0会移除所选pulser，正值创建/替换周期任务。旧`modifyLED`的Slow/Fast分别固定使用1000/500 ms，Off/On最终进入`setLedEnabled`并先停止同一LED的pulser。当前主机维护界面的Off/On/Slow/Fast因此已覆盖旧`modifyLED`的状态语义；仍没有让操作者指定任意`period_ms`的独立入口，不能再笼统写成“周期LED pulse全部未迁移”。

本轮为旧源码与当前源码的静态语义对照，没有调用板卡或新增运行测试。当前状态/差异见[旧 ECC 命令迁移核对表](LEGACY_ECC_COMMAND_COVERAGE.md)和[重构进度](REFACTOR_STATUS.md)；真实GPIO/灯态与任意周期入口仍待处理，板端服务不在主机重写范围。

## 71.164 ECC 网络测试按场景清理合成 raw 并记录预算（2026-10-22）

复核维护窗口网络测试证据时发现，旧 runner 的清理回执没有覆盖 receiver 场景输出目录，网络集成虽通过，目录中仍可能留有小型 `.graw`。`tests/run_ecc_network_service_tests.py` 现会在每个场景进程结束后，只删除自己新建场景目录下的普通 `.graw` 文件；清理拒绝非普通路径，记录文件数/字节数/残留/错误，并在累计超过20GiB、有残留或清理失败时将测试标记为失败。每次运行同时写出 `cleanup.json` 汇总。三字节隔离夹具验证删除行为和非raw旁文件保留通过。

修正后重新运行 `ecc-network-service-cobo-maintenance-window-0/1`，耗时112.00秒与113.44秒，均通过。每场景各删除4个合成文件、65,536字节，两个场景合计131,072字节；两个 `cleanup.json` 都报告remaining=0且未超预算。增强版 `check_cobo_maintenance_window_evidence.py` 对两场景的Qt窗口证据、operation key、ECC/硬件审计、原始请求和完整网络结果一致性复核通过。测试的 `.graw` 全数删除，保留的审计目录没有 `.graw`。

此证据仅证明本机localhost模拟器场景和测试runner自有目录清理，不证明真实硬件、物理双机或持续DAQ吞吐。详情见[维护窗口说明](COBO_MAINTENANCE.md#10-gpio-flip-回归测试)及[flip实现验证71.162](VALIDATION.md#71162-旧版-cobo-led-flip-迁入受控维护链2026-10-22)。

## 71.163 用户 Windows Qt 5.14.2 当前源码构建与 LED flip 原生测试（2026-10-22）

使用用户提供的 `D:\\Software\\QT\\5.14.2\\mingw73_64` 和 MinGW 7.3，经受控 WSL/Windows 原生互操作从当前源码严格 C++11 构建8个 x86_64 PE 控制程序，构建收据状态为`complete`。新运行候选 `build-packages/getdaq-windows-qt5142-20261022-ledflip-candidate/` 初始含8程序和280个文件；所有程序 SDK-free PATH `--help`、Qt 离屏初始窗口及 Windows 原生窗口 smoke 通过，运行库/清单校验通过。

同一 Kit 重建并运行 Widgets 与 ECC 客户端 QtTest：Widgets 109通过、0失败、1项Unix专属目录锁测试跳过；ECC客户端64/64通过，两个测试进程退出码均为0。新增 `coboMaintenanceFlipRequiresCancelAndBindsExactGpioBit`，验证默认Cancel取消无提交、GPIO 10 与“All LEDs” 16精确映射、确认后只发一次信号、重复点击被禁用，以及返回的GPIO位与原提交不符时被拒绝。runner清理回执为临时目录移除、raw剩余0个/0字节、20GiB限制未触发。构建和测试收据分别保存在工作区 `build-windows-qt5142-ledflip-20261022/`、`build-windows-qt5142-ledflip-tests-20261022/`，并随候选一同交付于 `build-packages/getdaq-windows-qt5142-20261022-ledflip-candidate/validation/`。

此结果证明用户当前 Windows 工具链上的编译、Qt窗口 smoke 和本机 QtTest，不代表独立干净 Windows 安装、完整人工控制流程、真实 Linux ECC 网络服务或板卡物理LED验证；也不替代 macOS、独立Ubuntu24、真实双机和持续DAQ吞吐验收。运行包文档/manifest已由本记录关联的候选刷新工具更新后另行校验。

## 71.162 旧版 CoBo LED flip 迁入受控维护链（2026-10-22）

旧`cobo-led-flip`按当前 GPIO 值取反，属于非幂等维护动作。新协议`flip-cobo-led`绑定固定活动CoBo、原ECC实例、快照、唯一operation ID和`led_id`；合法选择为单个位0–15或全灯16。ECC调度器将请求显式送至`NativeEccCoordinator::FlipCoBoLed`，状态只允许Prepared→Prepared；原生协调器/`SystemPreparationClient`在确认数据接收停止、Prepared、活动目标和硬件管理器后，只构造并执行一次旧板端`flipLED(id)`或`flipLEDs()`调用。硬件与ECC审计保存目标、GPIO位、原生操作号和无重试边界。板端RPC应答不等于物理灯状态已读回。

使用GCC 9.4、Qt 5.12.8构建ECC服务、原生网络测试与生产Qt维护窗口探针成功。严格C++11状态机、journal、ECC协议和硬件描述TCP测试4/4通过；原生协调器`normal-0`通过。生产Qt窗口→ECC HTTP服务→两个Supervisor→receiver→模拟CoBo的维护测试，在硬件API版本0和1均通过，分别耗时110.863秒与112.504秒。覆盖默认Cancel且取消无intent、确认只提交一次GPIO 10、同operation ID改成GPIO 11拒绝、操作后known Prepared、其他CoBo/告警订阅/系统对齐未改变、GUI不读取或转发raw，以及之后两轮Configure/Start/Stop/Breakup。

两次网络测试各生成65,536字节合成`.graw`，总计131,072字节（远低于20GiB）。测试结束后仅删除本轮两个`/tmp/getdaq-ledflip-build/full/native/ecc-network-evidence/ecc-network-system-*`目录内的`.graw`，核验剩余raw文件为0；JSONL维护审计、GUI结果和原生操作证据保留在相应临时证据树。可复查[选定CoBo维护说明](COBO_MAINTENANCE.md#10-gpio-flip-回归测试)。

限制：这是localhost模拟器，不是实体CoBo、现场DAQ、灯态读回、连续吞吐或物理双机验收；本轮Linux构建不代表用户Windows Qt 5.14.2/MinGW及macOS编译通过。非幂等flip超时/UNKNOWN必须观察原operation ID并按人工硬件检查流程处理，禁止自动重发。整套重构仍未完成。

## 71.161 旧版全板 LED 一次闪烁迁入受控维护链（2026-10-22）

将旧GetBench `cobo-led-blink-all` 语义迁入新的单板维护流程。实现从旧源码确认入口最终调用`LedManager::blinkLEDs()`，旧`CtrlNodeI::blinkLEDs`会顺序闪烁全板LED。新`blink-cobo-leds`仅允许固定活动CoBo，在健康Prepared且数据服务已停止时由GUI默认Cancel确认一次；请求绑定实例、快照、目标与唯一操作ID，原生ECC在硬件调用前重新检查停止状态和管理器接口。持久审计记载`sequential-once`、硬件应答与`physical_lamp_state_verified=false`；不自动重发。协议回归拒绝错目标、清空目标、LED无关字段和注入额外JSON字段；状态机仅允许Prepared→Prepared。原单灯版本2/5边界保持，旧flip和任意周期pulse仍未迁移。

使用GCC 9.4及Qt 5.12.8完成`GetDAQ`主控台和`cobo-maintenance-window-probe`完整编译；另构建ECC网络测试、数据Supervisor/ECC服务及精简原生接收器。协议、状态机、journal、硬件客户端回归通过；原生协调器`maintenance-0/1`均通过。随后使用真实Qt主窗口、原生RunControl、ECC HTTP服务、两个Supervisor和模拟CoBo，在两种硬件API兼容版本上执行本机网络集成，2/2通过：取消确认不提交/不持久化，确认后重准备、单灯设置、单次pulser和全灯顺序闪烁分别恰好提交一次；UI忙碌时禁用冲突命令，目标绑定正确，整个路径保持Prepared，原始数据读保护自检通过。测试只使用合成localhost设备，不连接实体硬件或外网。共生成8个合成`.graw`，总计131,072字节，单文件最大24,576字节，均已删除；构建/联调临时目录也已清理。`tools/check-documentation-links.py`检查1453个本地Markdown链接通过。Qt应用和联调探针的Linux构建通过不代表Windows Qt 5.14.2、macOS或现场板卡验收；旧flip与任意周期pulse仍未迁移，整体重构未完成。

## 71.160 默认DAQ数据套件独立构建与依赖复核（2026-10-21）

从干净目录配置`apps/datareceiver`，未启用可选FITS/ROOT工具或ROOT查看器。GCC 9.4 Release构建成功，包含`getdaq-receiver`、`getdaq-inspect`、`getdaq-merge`、`getdaq-matrix`、`mfm-stream-benchmark`及接收核心/导出锁/格式测试目标。CTest五项`mfm-merge`、`gmat-conversion`、`receiver-core`、`exporter-lock`、`byte-storage-integration`全部通过。需要loopback socket的测试在沙箱许可临时本地socket后运行；没有连接板卡、外网或物理DAQ。

对receiver、inspect、merge、matrix运行`ldd`，动态依赖只见系统`libpthread`（receiver）、`libstdc++`、`libgcc_s`、`libc`、`libm`和加载器；没有Qt、Ice、Boost、ROOT或CFITSIO。CMake缓存确认`GETDAQ_BUILD_OPTIONAL_FITS_TOOLS`、`GETDAQ_BUILD_OPTIONAL_ROOT_TOOLS`及`GETDAQ_BUILD_OPTIONAL_ROOT_VIEWER`均为OFF。字节存储集成用393,353字节确定性合成payload；矩阵转换夹具原始输入3,343,168字节；合并和存储单元夹具分别为112及1,024字节。进程测试私有目录自动删除。测试raw总量低于20GiB且没有大文件压力测试。临时构建约3.5MiB，完成后删除。此验证证明Linux/GCC 9.4默认数据套件的可构建性、回归和依赖边界，不等于Ubuntu24独立机器、macOS、真实NIC/磁盘或持续DAQ吞吐验收。整体重构未完成。

## 71.159 FITS导出支持compact type-2帧（2026-10-21）

依据本地旧`CoBoFrameViewer`的`cobo-frame-graw2fits`调用通用`CoBoEvent::fromFrame`以及`CoBoFormats-Rev-5-Compact.xcfg`，将可选`getdaq-fits`扩展为识别CoBo type-2 revision 5。MFM schema规定每项2字节、AGET索引2 bit、样本12 bit；旧数据模型按每个AGET分别递增通道再递增桶号。新解码器要求完整4×68×512样本，按每个AGET的顺序重建通道桶号，并拒绝不完整读出及越界桶数。type-1路径保持不变。此实现针对schema驱动的`fromFrame`路径；旧源码中另一段硬编码解码辅助函数并非本次参考路径。

使用GCC 9.4、CFITSIO 3.47构建`getdaq-fits`与`fits-tests`成功。`fits-conversion`专项1/1通过：一份279,808字节混合raw包含两个type-1事件和一个compact type-2事件；输出校验276条`GROUPING`索引记录、type-2事件272张通道表、AGET/channel扩展元数据、每通道512行、首末桶号/ADC值和FITS checksum，并检查不完整type-2拒绝、输出容量限制、截断输入、已有输出不覆盖及失败不发布。测试在退出时删除全部raw及FITS产物。另以AddressSanitizer和UndefinedBehaviorSanitizer构建并重跑`fits-conversion`，1/1通过（当前容器关闭LeakSanitizer）。同一构建目录下的六个数据接收器/格式/存储集成测试全部通过；其中两个需回环TCP的测试在允许本地socket后通过。该数据小于用户规定的20GiB测试预算。

限制：样本由新测试夹具生成，没有实验组提供的真实compact type-2 raw，也没有和旧CCfits输出逐字段比对；因此只验证实现与仓库所载schema/decoder语义一致，不声明旧版全兼容。Windows/macOS CFITSIO构建、下游FITS消费者和长时转换仍待现场验收。详见[FITS离线导出说明](DATA_FITS.md)。

## 71.158 ROOT查看器兼容旧ROOT5时代对象类名（2026-10-21）

历史CoBoFrameViewer在ROOT5版本中用小写`get::GDataFrame`等类名保存对象树；ROOT6版本更名为`GET::GDataFrame`。ROOT查看器现带有旧lowercase类的独立ROOT字典和reader adapter，按旧字段模型将事件映射到当前内存结构。读入仅限显式启用的ROOT查看器，不影响默认程序、接收器或转换器输出的新`GET::`树。

使用ROOT 6.26/16、GCC 9.4、Qt 5.12.8重新配置`GETDAQ_BUILD_OPTIONAL_ROOT_VIEWER=ON`，`getdaq-root-viewer`、`root-viewer-tests`、`root-statistics-tests`及`root-statistics-scan-tests`构建成功。小型reader夹具分别写入`GET::GDataFrame`、由兼容字典写入的`get::GDataFrame`和`T2KEvent`矩阵树；回归核对lowercase树的事件编号/时间/CoBo/AsAd、两个AGET通道与样本引用，并拒绝越界AGET；T2K维度/零填充及缺失文件/entry边界也继续覆盖。测试只生成自动清理的小型ROOT文件，没有DAQ raw。ROOT转换、reader、统计公式和全树扫描共4/4通过；viewer曲面离屏烟测通过，旋转前后262,458像素变化；另将FITS/ROOT选项全部关闭，receiver、inspect、merge、matrix四个默认数据目标均构建通过。

限制：lowercase fixture由ROOT6运行时与相同类名/版本字段的隔离字典生成，证明新程序可加载该命名空间和结构；本机没有ROOT5运行时，不能据此宣称已读取ROOT5真实文件。ROOT自动Schema演进差异、真实实验样本、macOS/Windows ROOT构建仍待验证。构建位于`/tmp/getdaq-root5-compat-build`，reader夹具自动清理；普通Qt控制和DAQ接收链未被修改。

## 71.157 离线ROOT查看器增加可旋转曲面视图（2026-10-21）

使用ROOT 6.26/16、GCC 9.4、Qt 5.12.8，单独开启`GETDAQ_BUILD_OPTIONAL_ROOT_VIEWER=ON`并关闭ROOT转换器，`getdaq-root-viewer`和三项reader/统计测试目标构建成功；`root-viewer-reader`、`root-statistics`、`root-statistics-scan`为3/3通过。离屏`getdaq-root-viewer --surface-smoke`在内存合成完整4×68×512 ADC矩阵，实际渲染四个曲面并检查非背景像素；随后发送鼠标按下/拖动/释放事件，再渲染并核对画面变化，262,458像素不同，退出码0。进程耗时约0.33秒，峰值RSS 257,144KiB（含ROOT/Qt运行时，非曲面增量内存）。该用例不写ROOT文件、不生成DAQ `.graw`。曲面显示由Qt Widgets/QPainter完成，无QwtPlot3D/Qt OpenGL新增依赖；真实ROOT大文件渲染及Windows/macOS ROOT支持尚未验收。细节见[ROOT工具手册](DATA_ROOT.md)。

## 71.156 旧ROOT显示模式范围核对（2026-10-21）

只读核对`source/get/CoBoFrameViewer-release-20210208/src/root2disp/cobo-frame-root2disp.cpp`、`CoBoFrameBrowser.cpp`和同项目`get/CoBoFrameViewer.cpp`、`CoBoSurfDisplay.cpp`及QwtPlot3D Autoconf探测宏。确认旧root2disp的`data`选项打开四个AGET波形浏览器；浏览器方法`DrawFrame3D()`实际填充`TH2F`并调用`Draw("COLZ")`，功能是二维桶×通道色图。其拟合代码处于注释中。真正的QwtPlot3D表面图属于CoBoFrameViewer主界面的独立可选显示，由构建时检测到QwtPlot3D依赖才启用。71.156审阅时软件曲面尚未迁入；该状态由[71.157](VALIDATION.md#71157-离线root查看器增加可旋转曲面视图2026-10-21)的Qt软件绘制模式更新，仍不复制旧QwtPlot3D外观。本条是静态源码范围审阅，没有修改程序或运行测试。细节见[ROOT工具说明](DATA_ROOT.md)。

## 71.155 ROOT查看器支持T2K矩阵树（2026-10-21）

在ROOT 6.26/16（`/home/lfh/work/root6.26`）、GCC 9.4、Qt 5.12.8下单独配置`GETDAQ_BUILD_OPTIONAL_ROOT_VIEWER=ON`并关闭可选转换器，`getdaq-root-viewer`和reader/统计测试目标构建成功。扩展的`root-viewer-reader`测试分别创建并重开`tree/GDataFrame`与`TT2K/T2KEvent`文件，核对旧事件头/样本引用、T2K 272×512矩阵映射、零填充、末桶边界及错误维度拒绝；与`root-statistics`、`root-statistics-scan`一起运行3/3通过。`QT_QPA_PLATFORM=offscreen`下viewer持续运行到3秒smoke超时（退出码124），无崩溃；Qt提示当前WSL runtime目录只读。所有ROOT夹具由测试清理，无DAQ `.graw`生成。Linux本机测试不等于Windows/macOS ROOT构建、真实文件或旧消费者验收；统计仅对GDataFrame提供。实现细节和使用限制见[ROOT工具说明](DATA_ROOT.md)。

## 71.154 ROOT统计扫描移入可取消工作线程（2026-10-21）

ROOT全树统计改由专用Qt`QThread` worker执行，worker独立持有/打开ROOT reader；GUI事件循环每50ms读取原子permille进度并处理取消。调用`ROOT::EnableThreadSafety()`后，真实三entry小型ROOT夹具在独立`std::thread`中成功扫描。测试按entry重建事件数据：event time三值、相邻frequency/period两值（覆盖0 delta）、桶号四个边界/散布值、每事件3/1/0非空通道分布均精确核对；预设取消立即退出且不报读取错误。`root-viewer-reader`、`root-statistics`公式/边界、`root-statistics-scan`五字段及线程/取消回归3/3通过；`getdaq-root-viewer`独立配置构建通过，离屏GUI启动持续到3秒测试超时、无崩溃。临时ROOT测试目录自动删除，没有生成`.graw`。没有进行大/真实ROOT文件性能测试；entry导航仍由GUI线程读单个事件。

## 71.153 ROOT旧统计字段的Qt直方图迁移（2026-10-21）

`getdaq-root-viewer`增加五种ROOT全树统计选项，迁移旧 `root2disp` 的 `time/freq/period/bucket/nchannels` 数据定义，并通过Qt QPainter显示线性直方图。事件时间用1e8 ticks/s转换；相邻时间差转换为Hz及ms，差为0时频率按旧逻辑记0；bucket聚合全树所有样本；channel count从每事件非空channel对象数得到。连续字段使用两遍可取消扫描和1000 bins；固定字段分别用512/273 bins。计数内存与事件数无关。统计值/边界专项测试 `root-statistics` 通过，ROOT对象树reader `root-viewer-reader` 通过（2/2）；viewer独立配置构建通过，offscreen启动存活至3秒测试超时、无崩溃。另以ROOT工具和viewer均关闭的默认配置从当前源码单独构建`getdaq-receiver`；`ldd`无Qt、ROOT Core/RIO/Tree依赖。测试无DAQ raw。统计全树扫描现由可取消QThread执行；没有大规模ROOT文件耗时验证，单个复杂entry在逐事件导航时仍可能短暂影响响应。旧ROOT Canvas的对数轴样式、CoBoFrameViewer 3D/拟合和积分直方图仍未迁移。

## 71.152 可选 ROOT 离线事件查看器（2026-10-21）

使用 ROOT 6.26/16（`/home/lfh/work/root6.26`）、GCC 9.4 和 Qt 5.12.8 构建 `getdaq-root-viewer`、`root-viewer-tests`、`getdaq-root` 与 `root-tests`。`root-viewer-reader` 和既有 `root-conversion` 两项 CTest 均通过。reader测试在临时目录写入很小的 `tree/GDataFrame` ROOT 文件，重开后核对事件头、通道/样本引用和值，并检查entry越界、文件缺失拒绝；临时ROOT文件自动删除，没有生成DAQ raw。查看器使用 `QT_QPA_PLATFORM=offscreen` 启动并持续运行3秒后由测试超时结束，未崩溃。

另检查只启用 `GETDAQ_BUILD_OPTIONAL_ROOT_VIEWER=ON`、关闭 `GETDAQ_BUILD_OPTIONAL_ROOT_TOOLS` 的单独配置/构建，确认查看器不要求转换器target。此为Linux主机合成ROOT数据验证，不是Windows/macOS构建、真实实验ROOT文件、旧 `root2disp` 端到端或真实下游分析兼容验证。功能范围、构建方式及未迁移旧统计绘图/3D说明见[ROOT工具手册](DATA_ROOT.md)。

## 71.150 可选 `.graw` 到 FITS 离线导出器（2026-10-21）

本机系统提供CFITSIO 3.47。使用CMake选项`GETDAQ_BUILD_OPTIONAL_FITS_TOOLS=ON`构建`getdaq-fits`和`fits-tests`成功；定向运行`ctest -R '^fits-conversion$'`为1/1通过。测试生成1024字节合成raw（两个type-1 CoBo帧），转换后检查4行GROUPING索引、索引中的事件文件名、事件文件3个HDU、通道桶号与ADC样本读回，以及primary和channel HDU checksum。并覆盖最多一事件、20GiB上限参数（测试用1字节触发拒绝）、既有目标目录拒绝、截断输入拒绝和失败时不发布目录。夹具清理整棵临时目录；raw生成总量1024字节，删除后为0。

默认构建选项仍为OFF，CFITSIO不会作为主接收器/控制程序/Qt界面的依赖。当前只支持CoBo type-1 revision 0–5；旧`cobo-frame-graw2fits`实际数据差分、旧分析软件端到端读取、Windows/macOS构建尚未验证。一次完整CTest没有作为该功能的证据：该构建目录只编译了`getdaq-fits`和`fits-tests`，其他测试可执行文件未构建；其中现有接收网络测试又受沙箱创建socket权限阻断。专项FITS测试自身已独立通过。详细接口、表结构、构建和使用限制见[FITS导出器手册](DATA_FITS.md)。

## 71.151 可选 `.graw` 到 ROOT 离线转换（2026-10-21）

在 ROOT 6.26/16（`/home/lfh/work/root6.26`）与 GCC 9.4 下配置 `GETDAQ_BUILD_OPTIONAL_ROOT_TOOLS=ON`，成功生成 `getdaq-root` 和 `root-tests`。ROOT字典在构建时由rootcling生成；独立转换器target用C++17，复用的DAQ数据核心仍是C++11。运行 `ctest -R '^root-conversion$'` 为1/1通过。

测试重新打开并读回输出ROOT文件，验证默认 `tree`、`GDataFrame` 分支及其类名为 `GET::GDataFrame`，核对事件号/时间、hit-pattern位映射、嵌入的配置文本、通道和样本对象、桶号/ADC值；另验证 `--t2k` 的 `TT2K/T2KEvent` 名称、272通道×512桶布局及未命中桶为0。还覆盖拒绝覆盖、1字节输出上限拒绝、截断输入拒绝且失败不发布。合成raw为512字节有效帧及13字节截断帧，共525字节，测试清理临时根后raw为0。

ROOT仍是显式可选系统依赖，不进入默认接收器、GUI或控制程序；本轮只在Linux主机完成构建与合成结构检查。未在macOS/Windows构建，未使用现场真实raw与旧graw2root逐字段差分，也未让实验室旧root2disp打开结果。因此只证明兼容的树/对象接口主路径，不能宣称历史ROOT格式及整个分析链完整兼容。完整依赖、命令和限制见[ROOT转换手册](DATA_ROOT.md)。整体重构仍未完成。

## 71.149 归档器普通 TCP 跨网络命名空间回归（2026-10-21）

新增 `tests/run_catalog_netns_tcp_test.py`，将两个隔离Linux网络命名空间用临时veth互联。发送端/接收端均使用Linux host候选包中的同一份真实 `getdaq-catalog`（SHA-256 `c7cec6ee7b8ee5b17f61c43db9fe910038f3dd7299c257a2a2a19535eaae9aaa`），通过 `--send-catalog-tcp` 与 `--listen-catalog-tcp` 传输。源归档由当前源码构建的 `catalog-transfer-tests --prepare-netns-catalog` 生成，包含两个receiver source、四个合成 `.graw` 文件及封存元数据；源端 `--inspect-catalog` 检查通过。接收端发布完成后，发送回执与独立 `--verify-transfer` 全量校验回执逐字段一致，且源请求键与传输回执绑定。

合成raw按路径累计25,190,400字节（源receiver文件、源catalog硬链接和目标副本），低于20GiB限制；12个raw路径均已删除，临时根移除，`raw_remaining=0`。回执为 `build-receiver/catalog-netns-tcp-71-149.json`，SHA绑定候选归档程序、fixture helper与runner。包内保留副本 `validation/catalog-netns-tcp.json`；候选manifest更新后共182文件。运行步骤及证据限制见[接收传输手册](RECEIVER_TRANSPORTS.md#归档器普通-tcp-跨命名空间回归)。

此验证经过veth/IP路由，但仍在同一Linux主机和内核，不是两台物理机器；也不验证实际NIC、防火墙、现场目标盘、长时间吞吐或断线恢复现场流程。跨命名空间发送使用普通未认证、未加密TCP。真实双机验收仍待完成，整体重构未完成。

## 71.145 旧 MuTaNT BRAM 检查命令的写入范围复核（2026-10-21）

复核`source/get/GetBench-software-release-20210208/src/get/GetEccCmd.cpp`的`MutantCheckBram::exec`及`source/mordicus/MDaq-release-20210208/src/mdaq/EccBackEnd.cpp`的`EccBackEnd::checkDevice`后确认：命令用固定设备名`CARD_B_BRAM`、访问模式`MemBus16`、寄存器宽度2字节、基址`0x80F10000`，再由操作者提供value、regCount、firstOffset，逐项按`firstOffset + i*2`写入相同值并读回。旧函数在测试后仅移除临时设备描述，不保存或恢复原始BRAM内容；该命令路径也未验证可写安全范围。源代码没有给出BRAM的有效深度、保留区或恢复策略，因此不能直接迁移成任意地址写入的C++/Qt入口。

已在[旧ECC命令迁移核对表](LEGACY_ECC_COMMAND_COVERAGE.md)记录准确行为和证据边界；Markdown本地链接检查通过。本次为只读旧源码审阅，没有生成或传输DAQ数据。只有在取得实际MuTaNT内存映射、现场允许测试区域、设备停机门槛及维护恢复流程后，才可设计固定范围的安全诊断；现有板端服务与硬件未修改。

## 71.144 三平台71.143当前源码候选与完整回归（2026-10-21）

为使71.143选定CoBo AsAd单次pulser维护入口进入可检查交付，本轮从当前源码分别构建并刷新Linux host、Ubuntu24和Windows候选包。Linux host full suite为27个程序；现有候选`build-packages/getdaq-linux-host-20261021-71-143-current-source-full-candidate/`完整性复核通过，包内27程序smoke通过。Ubuntu24.04.5已验签用户空间以uid1000/GCC13.3.0/CMake3.28.3/Qt5.15.13从当前源码重新构建full suite 27个程序；Core QtTest 151/151、Widgets QtTest 106/106、native CTest 22/22，以及来源迁移、配置事务、接收传输/安全、四路接收和失败handoff窗口测试均通过。合成wire总量1,073,799,755字节（约1.0GiB），低于20GiB；raw清理后剩余0。新候选`build-packages/getdaq-ubuntu24-20261021-71-143-current-source-full-candidate/`含27程序及验证回执，共190个清单文件；Ubuntu24加载器下27程序启动smoke通过。平台回执记录Ubuntu24用户空间运行在WSL2共享内核，不作为独立机器验收。

Windows使用用户提供的Qt 5.14.2/MinGW 7.3 x64重建7个控制端程序。原生Windows QtTest Widgets为105通过、0失败、1项Unix本地锁测试跳过；ECC客户端64/64通过，临时目录清理收据为raw 0个/0字节。候选`build-packages/getdaq-windows-qt5142-20261021-71-143-current-source-candidate/`含7程序、Qt/MinGW运行库和272项manifest；SDK-free PATH帮助检查、Qt离屏窗口和Windows原生窗口smoke通过，候选完整性复核通过。此结果来自用户当前Windows主机，不代表独立干净安装验收。Linux host候选27程序、Ubuntu24候选27程序、Windows候选7程序均已同步当前操作/验证文档。

核心pulser命令的硬件维护和生产Qt窗口至本机ECC HTTP服务/模拟receiver端到端证据见上一节71.143。此次Ubuntu24四路合成接收是有限时长协议/落盘测试，不能证明真实CoBo/MuTaNT源、独立双机无认证TCP、macOS原生、现场旧gSOAP部署、实板物理脉冲或持续DAQ吞吐。板端`getHwServer`/`IceEirfu`按范围保持未重写；现场板卡和独立目标主机验收仍未完成，整体重构继续进行。


## 71.143 AsAd 单次 pulser 维护命令及 Qt 端到端验证（2026-10-21）

在既有选定CoBo维护入口中新增`trigger-asad-pulser`单次诊断操作。请求将固定活动CoBo和`-1000..1000 mV`绑定到规范协议键及持久操作；原生路径重新核对Prepared、数据服务停止和互锁，执行且只执行一个`TriggerPulser`步骤。硬件调用不确定时保持UNKNOWN并禁止自动重试。审计明确标记没有物理脉冲/波形读回。周期pulser仍由实验配置与采集生命周期控制。

验证：Linux控制端7程序重建通过；原生维护相关硬件/协调器源码完成严格C++11 ordinary build。ECC协议、状态机和journal测试通过。硬件系统采集完整CTest 1/1通过（215.01秒，mock Ice服务）。随后将当前Qt维护窗口、ECC服务、原生ECC协调器、两数据服务和合成receiver连成实际本机HTTP/进程场景；Qt在offscreen下按真实窗口执行取消/确认，准确提交C0、321mV单脉冲，校验busy exclusion、持久请求、同ID改电压拒绝、原请求观察和最终Prepared状态。首次场景102.636秒通过；清理临时测试构建物后，用归档中SHA一致的生产Qt探针从持久构建路径复跑，102.435秒通过。更新后的[窗口证据检查器](../tests/check_cobo_maintenance_window_evidence.py)对归档证据通过。最终元数据证据位于`build-delivery/pulser-maintenance-window-e2e-71-143/`，其中没有凭据或raw载荷。

该场景曾生成4个合计65,536字节的`.graw`合成文件；总量远低于用户20GiB上限，测试后4个文件全部删除，回执中`raw_remaining=0`，归档本身也为0个raw。第二次复跑后检查仍为0个raw。71.144已用当前源码刷新Linux host、Ubuntu24和Windows Qt5.14.2候选，其中Linux host/Ubuntu24各27个程序，Windows 7个控制程序；平台测试和候选范围见71.144。Ubuntu24用户空间运行于共享WSL2内核。不是实板测试，不验证电气脉冲、实际波形、独立双机或持续DAQ吞吐；macOS本机也未运行。整体重构仍未完成。

本轮另修正ECC请求验证允许表遗漏新命令的问题，并把控制器状态转移上界/回归断言更新为13；协议codec全测通过。原生插桩构建辅助脚本现会排除另一平台专用的监督器源文件，Linux ordinary严格C++11构建通过。边界：本机loopback/模拟硬件与实际两机、旧gSOAP服务、板卡及独立目标机验收不同。

## 71.120 当前源码 Windows Qt 5.14.2 构建、测试与运行候选（2026-10-21）

在用户指定的Windows Qt 5.14.2/MinGW 7.3 x64工具链下，通过原生Windows进程从当前源码严格C++11构建七个控制端程序。构建receipt `build-receiver/windows-current-source-20261021-71-120/windows-build-receipt.json`为`complete`，所有程序均为x86_64 PE。重新构建Widgets和ECC客户端QtTest后，在Windows原生进程运行：Widgets 105通过、0失败、1项既有Unix本地锁测试跳过；ECC客户端63/63通过。两测试退出码均为0，清理回执确认私有Windows临时目录已移除、raw文件和字节均为0；本轮DAQ合成载荷为0。

由同一组构建程序生成的当前候选为`build-packages/getdaq-windows-qt5142-20261021-71-119-current-source-candidate/`，包含7程序/268项manifest。包完整性复核通过；清除SDK PATH后七程序帮助、GetDAQ离屏启动截图和Windows原生窗口截图通过。可查看候选中的`native-window.png`，Windows QtTest构建/运行证据位于`build-receiver/windows-current-source-20261021-71-120/qt-tests/`。另外，候选内`getdaq-config.exe`在Windows原生进程对完整引用扫描和外部include不完整扫描实测通过，退出码0/3符合合同。以Windows Qt/MinGW编译的独立迁移探针随后运行`tests/run_source_migration_delivery.py --windows`，通过实际迁移、原始/候选两方向崩溃恢复、快照校验和包内CLI验证；修正了测试runner对Qt返回的`//wsl.localhost/...` Windows路径与WSL路径的比较，实际完整用例通过。测试夹具已清理、raw为0，回执在`build-receiver/windows-current-source-20261021-71-120/source-migration-delivery/`，探针编译日志同目录。入口测试`tests/test_windows_build_entrypoints.py` 16/16通过，Markdown本地链接检查1366项通过。

这证明用户当前Windows主机上的指定Qt/MinGW工具链能构建并运行这些软件，不代表独立干净Windows部署、现场人工操作、真实Linux服务/旧gSOAP进程、板卡或DAQ数据流程已经验收。整套重构仍有独立Ubuntu24、macOS、双机TCP、现场硬件和持续吞吐等未完成项。Windows运行包用途及复制步骤见[Windows Qt5.14.2操作手册](WINDOWS_QT5142.md)。

## 71.119 配置路径迁移增加有界只读引用发现（2026-10-21）

路径迁移原来要求操作者先知道并打开所有相关配置根，但没有工程内查找工具。本轮在`getdaq-config`增加`discover-project-referrers SCAN_DIRECTORY OLD_SOURCE`，只读扫描用户明确指定目录中的常规`.xcfg`/`.xml`，建立include反向关系并列出传递引用者及候选顶层根。扫描受入口数100,000、配置文件数4,096、单文件8MiB、总量256MiB、XML token数100,000和深度128约束；检查未完成保存标记、DTD、坏XML、include越界/缺失、循环、符号链接及扫描期间变化。遇到不完整证据时仍输出JSON并返回退出码3；完整返回码0。任何扫描均不写配置。

验证：GCC9.3/Qt5.12.8、严格C++11 `-pedantic-errors`重建配置核心，QtTest 151/151通过，其中新增传递引用顶层根、只读不变、外部include和损坏XML不完整用例各通过。独立重建`getdaq-config`及migration probe后，`tests/run_source_migration_delivery.py`在隔离临时包通过：实际CLI对完整扫描找到3个引用者/最外层wrapper根，对损坏清单返回3且`complete=false`；原迁移、两种中断恢复方向、包内快照核验也全通过。

随后在Ubuntu20.04.6/WSL2 Linux 5.15宿主内核、GCC9.4.0、CMake3.16.3、Qt5.12.8下从同一源码执行`tools/build-suite.py --profile full`，27/27个程序构建成功。新Linux host候选`build-packages/getdaq-linux-host-20261021-71-119-current-source-full-candidate/`包含27个程序和172项manifest，`--verify-stage`通过；同一个候选内`getdaq-config`完成完整扫描与不完整扫描、路径迁移、原始/候选两向中断恢复及配置快照核验。六个原生数据工具逐个通过无Qt动态依赖检查。完整构建回执复制到`build-receiver/host-current-source-full-qt-20261021-71-119/build-receipt.json`，包内迁移交付证据位于同目录`source-migration-delivery/`。构建树254MiB及临时probe已删除，未生成DAQ raw；包候选和小型收据保留。文档本地链接检查1359项通过。

扫描完整性严格限定于所选目录的`.xcfg`/`.xml`，不能证明目录之外、其他格式或其他主机没有引用；它是辅助发现，不会自动把结果加入迁移工作区。Linux host与Ubuntu24候选现已更新到71.119。Ubuntu24.04.5用户空间使用GCC13.3.0/Qt5.15.13从当前源码full构建27/27程序；核心QtTest 151/151、Widgets106/106、CTest22/22、配置迁移/事务、接收传输与安全、四路接收和失败交接窗口均通过。平台回执`build-receiver/ubuntu24-current-source-20261021-71-119/platform-4b638ac3a0fc4a39a339230025814403.json`记录总wire 1,073,799,755字节（约1.074GB），测试预算20GiB、失败交接窗口raw 8192字节，最终`raw_remaining=0`。Ubuntu24候选含27程序/172项清单，经Ubuntu24 loader对全部程序启动检查；逐程序结果在`build-receiver/ubuntu24-current-source-20261021-71-119-package-smoke`。此套件使用Ubuntu24.04.5用户空间但共用WSL2内核，四路1GiB是有限合成接收而非真实连续DAQ吞吐。Windows仍需刷新到71.119，macOS runner与现场迁移仍未验证。

## 71.118 明确监督服务的 Qt 构建依赖并验证无 Qt 数据 profile（2026-10-21）

此前 `data-host`/`full` 请求构建 `getdaq-supervisor` 时，服务子目录对 Qt 使用静默 `QUIET` 探测；如果机器没有 Qt，配置阶段仍成功，随后才因目标不存在而构建失败。现改为服务目标存在时明确要求 Qt 5.12 Core、Network、Concurrent。纯 `data` profile 仍关闭服务目标，不查找 Qt，也不把 Qt 引入接收/归档工具。

从当前源码运行 `tools/build-suite.py --profile data --jobs 4` 成功，构建6个程序。之后以相同源码执行Linux host `full` profile，27/27程序全部构建成功，完整回执保留在`build-platform/linux-full-current-source-7118/build-receipt.json`。再次对`getdaq-receiver`、`getdaq-inspect`、`getdaq-merge`、`getdaq-matrix`、`getdaq-supervisor-query`、`getdaq-catalog`逐一执行`tools/check-no-qt-runtime.py`，全部显示`PASS: no Qt runtime dependency`。以`-DGETDAQ_BUILD_SUPERVISOR_SERVICE=ON -DCMAKE_DISABLE_FIND_PACKAGE_Qt5=TRUE`配置时，CMake在配置阶段按预期失败，明确指出禁用的Qt5为`REQUIRED`包；启用当前系统Qt 5.12.8正常配置，并从源码构建`getdaq-supervisor`及两个测试程序。`receiver-catalog-filesystem`和`supervisor-control-codec`共2/2通过。`tests/host_suite_delivery_test.py`现分别真实配置Qt禁用的原生数据核心（成功）及Qt禁用的服务profile（按预期配置失败），全套共7/7通过；此回归已加入Linux/macOS native-data-path workflow，但GitHub Actions本轮未运行。

测试使用的临时目录均已删除；完整构建树约253MiB，删除后只保留约100KB构建回执。本轮没有遗留`.graw`文件，也没有生成大型传输载荷。该改动澄清的是构建依赖和失败时机，没有改变控制服务、HTTP协议或数据接收实现。Qt服务仍只处理低频管理请求；DAQ raw仍由原生receiver独立接收。

## 71.117 旧 SOAP 控制模式补齐 Pause/Resume（2026-10-21）

旧 ECC SOAP 接口已定义 Pause/Resume，状态编号6为 Paused，但重构后的控制台此前只在 Native ECC 模式显示这两个动作。现将 Pause/Resume 暴露给旧 SOAP 模式：Pause 要求所有 ECC 状态新鲜且为 Running；Resume 要求所有 ECC 状态新鲜且为 Paused。命令发出后沿用现有 SOAP 命令应答与后续 GetState 确认链。运行日志在 Paused 时结算当前运行片段，Resume 后从累计时长继续；Stop 也允许从 Paused 状态结束。Native ECC 路径保持原有实现。更新后的 SOAP mock 覆盖多节点 Start → Pause → Resume → Stop 和 Paused → Stop。

在 Ubuntu20.04 WSL host 使用 Qt5.12.8/GCC9.3/C++11通过`tests/tests.pro`构建完整 GetDAQ QtTest；默认受限执行中，13项监听测试被 localhost socket 权限阻止。随后在允许本机loopback的执行环境下，完整套件106/106通过（0失败、0跳过）；其中本轮多节点真实HTTP/SOAP mock暂停恢复用例和暂停态Stop定向测试均通过。测试未连接现场服务或硬件，也没有进行DAQ吞吐测试。临时Linux QtTest构建、HOME、XDG runtime及测试日志已删除。

随后使用用户提供的Qt5.14.2/MinGW7.3在原生Windows进程中从当前源码构建七个控制程序与完整Widgets/ECC客户端QtTest；Widgets105通过、0失败、1项Unix锁测试跳过，ECC客户端63/63通过，raw清理余量0。新运行候选`build-packages/getdaq-windows-qt5142-20261021-71-117-pause-resume-candidate/`含7程序/268项manifest；SDK-free帮助、离屏初始窗口及原生Windows窗口 smoke通过，native截图`native-window.png`。构建和Windows QtTest回执位于`build-windows-qt5142-20261021-71-117/`。

同一源码状态随后在Ubuntu24.04.5已验签用户空间/WSL2宿主内核以GCC13.3.0、CMake3.28.3、Qt5.15.13完成full profile构建和`--stage all`验证：27/27程序、Qt Core/Widgets测试、CTest22项、配置迁移/事务、接收传输/安全、四路1GiB接收及失败交接窗口场景通过。平台回执`build-ubuntu24-validation/rootfs/build/getdaq-platform-20261021-71-117/platform-2400c64f42f2483b978306ae734f1f1d.json`记录总wire 1,073,799,755字节，合成raw上限20GiB，失败交接夹具raw 8192字节，`raw_remaining=0`。候选`build-packages/getdaq-ubuntu24-20261021-71-117-current-source-full-candidate/`含27程序/176项清单，逐程序Ubuntu24 loader启动检查及候选清单校验通过；测试证据随包存入`validation/`。四路1GiB是有限时长接收/落盘测试，不是持续吞吐或真实DAQ验收。Ubuntu24用户空间共用WSL2宿主内核；macOS、真实gSOAP进程、现场硬件、独立双机网络仍未验证。

同一源码再以Ubuntu20.04.6/WSL2宿主GCC9.4.0、CMake3.16.3、Qt5.12.8执行`tools/build-suite.py --profile full`，27/27程序构建成功，嵌入依赖检查105项通过。候选`build-packages/getdaq-linux-host-20261021-71-117-current-source-full-candidate/`由该构建回执直接封装，随后27程序包内启动/帮助检查、GetDAQ离屏窗口截图和`build-suite.py --verify-stage`通过；不会将Qt GUI测试误当成DAQ接收性能测试。此Linux host构建与Ubuntu24全量测试来自相同工作树状态，但操作系统/编译器版本不同。三个平台候选均已包含旧SOAP Pause/Resume源码；其余未验收边界如上。

## 71.116 Ubuntu24当前源码全套复验及27程序运行候选（2026-10-21）

在已验签Ubuntu24.04.5用户空间中，通过专用WSL入口运行当前源码`tests/ubuntu24_platform_validation.py --stage all --jobs 4`。首次调用在沙箱内因WSL互操作层`UtilBindVsockAnyPort: socket failed 1`而未启动发行版；在允许本机WSL启动的执行权限下，同一命令完整完成。工具链为GCC13.3.0、CMake3.28.3、Qt5.15.13，full profile 27/27程序构建成功；Core 149/149、Widgets 106/106、CTest22/22、配置迁移/事务、12项receiver传输/安全、四路接收及失败交接窗口均通过。平台回执`build-ubuntu24-validation/rootfs/build/getdaq-platform-20261021-71-116/platform-18b4fb2d62324c24a1ef44d16369670b.json`记录四路接收1GiB、其他传输49,739字节，总合成wire 1,073,799,755字节，测试预算20GiB、`raw_remaining=0`。QtTest的私有HOME/XDG/TMP清理逻辑也随本次全套runner实际执行。

由本次Ubuntu24 full build的确切27个二进制建立新候选`build-packages/getdaq-ubuntu24-20261021-71-116-current-source-full-candidate/`。27个程序均逐项通过Ubuntu24 loader/动态依赖检查；`tests/ubuntu24_package_program_smoke.py`已从过期的26程序数更新为从当前full build计划核对清单，支持用目标rootfs loader运行。候选包内27个程序的CLI/启动smoke通过，GetDAQ通过offscreen初始窗口截图检查；测试没有硬件访问，也没有raw。候选现为27程序/202项清单，包含构建、平台、测试、包启动与截图摘要回执；包完整性验证通过。证据见包内`validation/`及`build-receiver/host-current-source-full-qt-20261021-71-115/ubuntu24-package-smoke.json`。

本轮验证仍运行于WSL2内核，不是独立Ubuntu24主机；没有覆盖双机物理网络/防火墙、macOS原生运行、旧gSOAP服务、真实板卡、独立安装环境或持续物理DAQ磁盘吞吐。该候选不代表这些验收已完成；整体重构继续进行。

## 71.115 当前源码 Linux host 全量构建与 QtTest 回归（2026-10-21）

在 Ubuntu 20.04.6/WSL2 Linux 5.15.167.4 宿主内核、GCC 9.4、CMake 3.16.3、Qt 5.12.8 下，使用当前工作树运行 `tools/build-suite.py --profile full --jobs 4`。27/27 个主机程序构建成功，105 个嵌入依赖文件校验通过；构建回执逐个绑定程序 SHA-256。回执保存在 [`build-receipt.json`](../build-receiver/host-current-source-full-qt-20261021-71-115/build-receipt.json)，摘要见 [`summary.json`](../build-receiver/host-current-source-full-qt-20261021-71-115/summary.json)。

随后从同一源码按 C++11 重建 Qt 操作台完整测试程序，并在本机回环可用的执行权限下运行完整 QtTest：106 passed、0 failed、0 skipped，耗时 51.299 秒。第一次受限执行有 13 项因不能绑定 localhost 端口失败；逐项均为本机网络夹具启动失败，随后同一二进制完整复跑通过。QtTest 使用本轮私有 HOME、XDG cache/runtime 与 TMP；`~/.qttest`已清理，未生成 raw。完整输出见 [`qt-tests.txt`](../build-receiver/host-current-source-full-qt-20261021-71-115/qt-tests.txt)。

尝试使用专用Ubuntu24用户空间入口复验71.114测试目录隔离时，WSL在启动发行版前报告 `UtilBindVsockAnyPort: socket failed 1`；这不是Ubuntu24构建/测试结果，也没有重试或留下测试数据。当前记录只证明Linux host完整编译及本机Qt回归，不能替代Ubuntu24全套复跑、独立机器、macOS、双机网络、旧gSOAP服务、板卡或持续DAQ验收。

## 71.114 QtTest测试模式数据目录隔离（2026-10-21）

Qt Widgets测试调用`QStandardPaths::setTestModeEnabled(true)`。在Linux/WSL中，Qt会把测试模式数据放到`~/.qttest`，即使设置了`XDG_CACHE_HOME`也可能继续访问用户HOME；Windows则可能使用用户的APPDATA/LOCALAPPDATA。现在Ubuntu24全套runner把HOME、XDG缓存、运行目录和临时目录限定在本轮私有工作目录；Windows QtTest PowerShell runner把APPDATA/LOCALAPPDATA也限制在自有临时根，该根会由既有清理流程删除。

`tests/test_windows_build_entrypoints.py` 16/16通过，三个Python文件语法检查通过。Ubuntu24 runner会在core/Widgets QtTest结束后仅清理本轮私有HOME中的`.qttest`目录，并拒绝符号链接或非目录路径；Windows runner的APPDATA/LOCALAPPDATA随自有临时根一起删除。该回归检查路径隔离配置和清理边界，没有重跑Ubuntu24平台全套、用户Windows QtTest或应用二进制构建；GitHub Actions尚未因此次修改运行。没有生成DAQ raw。

## 71.113 配置来源路径迁移当前源码复验（2026-10-21）

在当前工作树使用 Ubuntu20.04/GCC 9.3、Qt 5.12.8 对配置核心、来源迁移进程探针及 GetDAQ 控制界面测试按严格 C++11 参数构建。配置核心 QtTest 149/149 通过；`run_source_migration_tests.py` 的12种进程中断/原始或候选恢复组合及2种活跃写者场景通过；界面侧路径迁移取消默认、只读拒绝、确认保存、成功后重开新来源/清空旧历史，以及缺失路径恢复预览和确认共3项通过。测试只使用临时XML；确认本轮 `.graw` 数量为0，编译/测试临时目录已删除。

本次复验解决了旧70.68检查点目录已不存在的问题。路径迁移代码及GUI/CLI功能仍在当前源码；此结果不是Windows、macOS、Ubuntu24独立机器、真实断电、未打开引用扫描或外部非合作读者原子可见性的证明。仍未实现任意外部读者可见的跨文件原子事务。此前70.68中的候选包和Windows运行数据属于历史候选，不作为当前包状态。当前迁移操作和边界见[路径迁移手册](CONFIGURATION_PATH_MIGRATION.md)。

## 71.112 ECC/配置核心严格C++11构建与测试清理（2026-10-21）

在临时目录 `/tmp/getdaq-ecc-core-71-111` 以 GCC 9.4、C++11、`-Wall -Wextra -Wpedantic -Werror` 配置并构建 `apps/ecc` 及其配置核心。首次全目标构建发现 `tests/effective_configuration_record_test.cpp` 的 `Array()` 未使用；删除该死代码后，全目标构建成功。ECC状态机、持久化日志及实验配置共15项CTest全部通过。构建目录随后删除。此变更只改测试辅助代码，不改变产品行为；没有重跑全量27程序构建，也没有刷新候选二进制，71.110平台证据仍是最近的全量当前源码验证。该验证不涉及硬件或DAQ数据。

## 71.110 Ubuntu24当前源码整套回归与全量候选（2026-10-21）

在已验签Ubuntu24.04.5用户空间/WSL2宿主内核上，通过`tools/validate-ubuntu24-from-wsl.py run`运行`tests/ubuntu24_platform_validation.py --work /build/getdaq-platform-71-110 --jobs 2 --stage all`。Ubuntu24 runner明确以非root uid 1000编译和测试。当前源码full profile 27/27程序构建成功；QtCore 149/149、Widgets 106/106、原生CTest 22/22通过。配置来源迁移与并发事务、12项receiver传输协议、四路并发各256MiB合计1GiB真实receiver子进程写盘/轮转/逐流逐文件SHA/EOF/seal、失败handoff窗口生产服务故障注入均通过。

平台回执：`build-ubuntu24-validation/rootfs/build/getdaq-platform-71-110/platform-1778324386d34fffbf903fa4c2cc1139.json`，SHA-256=`16a50000f3c850da8b149c08a2f13a684ecf73b072e81b4ba06164f37904898b`。Qt测试输出在同目录`core/result.txt`和`widgets/result.txt`；其余关键测试摘要位于`logs/`。aggregate test wire为1,073,799,755字节；receiver四流为1,073,741,824字节，12项协议测试为49,739字节，其他控制/交接场景也计入aggregate。整轮低于20GiB。失败交接测试短暂生成8,192字节raw并在夹具结束时删除，最终`raw_remaining=0`，测试工作树无`.graw`。

由相同 full build receipt 中的27个程序制作`build-packages/getdaq-ubuntu24-20261021-71-110-current-source-full-candidate/`，候选含27个程序/185项文件，并把Ubuntu24平台回执、完整软件构建输入/程序SHA回执、QtTest构建摘要和核心集成结果纳入SHA清单；`tools/build-suite.py --verify-stage`通过。候选`getdaq-align` SHA-256=`7a15db2d150b2529aa5a2b18775a3a24f42ffe2cf2efcafdf9eaaa784c3177aa`，包含71.109的零活动CoBo拒绝检查。再使用Ubuntu24 rootfs动态加载器直接运行包内该二进制：有效启用拓扑返回成功；全部CoBo禁用时返回1，并输出`Alignment topology requires at least one active CoBo`。此命令只执行拓扑校验，没有连接硬件；绑定回执为候选包`validation/active-cobo-topology.json`，raw文件生成数为0。

此证据证明已签名Ubuntu24.04.5用户空间在WSL2宿主内核上的当前源码构建与合成集成回归；不证明独立Ubuntu24主机、两台独立主机互通、macOS、旧SOAP服务、真实板卡、实际实验数据或持续DAQ吞吐。测试载荷已清理，测试raw剩余0。

候选包和收据确认后，通过同一Ubuntu24受控runner删除本轮delivery、QtTest、probe、native测试程序及编译临时目录，约释放330MiB；平台/测试回执、关键stdout日志和约1.9MiB集成证据保留在专用work目录，根目录剩约2.6MiB，`.graw`为0。删除命令只作用于`/build/getdaq-platform-71-110/`内列明的本轮生成目录，清理命令回执退出码0；候选包中的27个程序不受影响，清理后包完整性及Ubuntu24候选复核再次通过。

## 71.107 监督器共享规范 IPv4 解析和 Windows 当前源码候选（2026-10-21）

将监督器计划校验和 Windows HTTP 客户端中的重复 IPv4 字符串解析合并为纯 C++11 `SupervisorIO::ParseCanonicalIPv4`，同时让 POSIX HTTP 客户端也使用同一校验。该解析器仅接受四段十进制、无前导零且每段为0–255的规范地址；计划校验继续拒绝0.0.0.0和首段大于等于224的目标，Windows 与 Linux 地址规则因此共用同一实现。`supervisor-control-tests codec`直运行通过，覆盖正确host-order结果、非规范文本以及224/4、240/4和广播地址拒绝；当前Linux CMake构建的测试与查询程序目标均成功。

使用用户Windows Qt5.14.2/MinGW7.3从当前工作树重建七个控制程序，Windows原生QtTest为Widgets 105通过、0失败、1项Unix锁测试跳过，ECC客户端63/63通过；两测试raw均剩余0。新Windows候选`build-packages/getdaq-windows-qt5142-20261021-71-107-shared-ipv4-candidate`通过SDK-free帮助、offscreen和原生窗口smoke及PE/运行依赖完整性验证，7程序；绑定构建/QtTest/清理回执后共272项清单，`getdaq-supervisor-query.exe` SHA-256=`9ae3279fd6fde71f3e65af3ad0d63567bd2df6d263146150df4ab11d622b15fe`。本机原生Windows验证不代表独立干净机器、真实远端服务或硬件验收。

## 71.106 gSOAP 兼容边界说明同步到三个交付候选（2026-10-21）

复核当前源码后，确认旧 GetBench SOAP 客户端/服务端和 `ecc.hh` 仍在，但生成绑定、`soapcpp2`、`stdsoap2.h`、gSOAP 库及本机安装归档均不可用。因此现有 mock HTTP 测试只能证明请求格式，不能代表真实 gSOAP 服务进程互通。将这一依赖缺口和后续补齐条件写入 `docs/DEPENDENCIES_AND_OFFLINE_BUILD.md` 与 `docs/REFACTOR_STATUS.md`，并使用 `tools/refresh-package-docs.py` 同步到 Linux host、Ubuntu24、Windows Qt5.14.2 三个候选。

三个候选刷新前后原生完整性验证均通过：Linux host 173项、Ubuntu24 27程序/174项、Windows 7程序/270项；刷新器额外核对程序、运行库和既有验证收据摘要未变化。源码本地 Markdown 链接1339项通过，`third_party/dependencies.json`有效JSON。本轮没有改代码或增加平台/互操作证据；真实旧SOAP服务、独立主机、macOS、板端和持续DAQ吞吐仍未验收，整体重构未完成。

## 71.83 Ubuntu24当前交付接收器四路16GiB负载回归（2026-10-21）

使用当前Ubuntu24全源码候选中的确切交付二进制`getdaq-receiver`，经Ubuntu24.04.5 rootfs的glibc loader，在WSL2 Linux宿主内核运行四路localhost接收。每路4GiB、4KiB合成CoBo帧、每文件1GiB轮转，共16GiB（17,179,869,184字节），低于用户规定的单轮20GiB上限。四路各1,048,576帧、每路4个文件；发送端整流SHA与逐个落盘文件SHA、字节数、帧数、自然EOF、接收进程退出和持久seal均一致。接收器交付SHA-256为`7a246c4ec2e05ed70077331343a282a33b288927a82c196442f7bf801e8af16c`，测试脚本SHA为`ae554bf84be6e9c42b75e5e91b26c913a7878c44b45d5ed882ac53f39cd5a1c0`。

完整发送及全文件读回校验墙钟为27.62秒；每路接收器峰值RSS为21,116KiB，CPU约0.86–1.19秒用户态、4.85–5.85秒内核态。上述时间受Python合成/哈希、localhost、WSL虚拟磁盘及页缓存影响；系统约52.6GB内存，大于测试载荷，故不证明超过内存缓存的持续磁盘速度，也不代表DAQ吞吐。测试回执与逐流SHA保存在`build-receiver/current-ubuntu24-16gib-71-83/receiver-load-1uigfvgg/result.json`；清理回执确认自动删除全部17,179,869,184字节，剩余生成raw为0，证据目录约76KiB。该证据是当前交付二进制的同机并发/文件完整性检查，不是独立双机、真实板卡、目标网卡、macOS或现场持续零丢失验收。

本轮随后使用`tools/refresh-package-docs.py`将该记录和接收性能说明同步到Ubuntu24、Windows候选包；刷新器在刷新前后校验程序/运行库并重算文档清单。Ubuntu24候选保持27程序/178项清单，Windows候选保持6程序/268项清单，刷新后包完整性均通过。`python3 tools/check-documentation-links.py`通过1322条本地Markdown链接检查；链接检查不验证锚点、独立安装环境或程序运行。

补充复核：`python3 tools/verify-dependencies.py --profile full`通过，完整主机套件构建输入中105个嵌入依赖文件均存在且摘要一致；该检查不下载依赖，也不验证系统工具链或硬件。

## 71.82 Ubuntu24当前交付包普通TCP归档专项回归（2026-10-21）

对当前Ubuntu24全源码候选的交付目录内`getdaq-catalog`确切二进制执行普通TCP传输回归。候选程序SHA-256=`180f0960a093aa41f355dbc5e3dfea2c9e0182629c461b5cc67bdb813202163d`；测试收据绑定同一SHA。测试在Ubuntu24.04.5用户空间/rootfs内运行，宿主内核为WSL2，使用当前`tests/run_catalog_plain_tcp_tests.py`及当前源码构建的传输测试程序。

无认证普通TCP新传输、中断后由操作者显式续传、目标读回、完成后零载荷续传、独立listener/sender CLI进程、连接拒绝有界退出、SIGPIPE隔离及原生归档回归全部通过。本轮合成raw生成/删除各15,245,312字节，预设聚合上限32MiB（33,554,432字节），私有测试根为空、`raw_remaining=0`。回执`build-ubuntu24-validation/rootfs/build/getdaq-platform-71-79/evidence/catalog-plain-tcp-71-82-final.json`，SHA-256=`9a97ec21e073a3153a5d2158b943a6dbf723309153c9e98648c9d94e09baafc7`；当前候选包同时包含`validation/catalog-plain-tcp.json`及其增量刷新回执，27程序/178项清单完整性通过。

这是同一内核上的独立进程loopback，不是两台物理/虚拟主机互通、真实板卡数据、Windows/macOS传输或持续DAQ吞吐证明。协议没有身份认证与加密；SHA-256只用于内容校验。用户要求测试数据不超过20GiB，本轮上限为32MiB且raw已清理。

## 71.81 Windows Qt5.14.2/MinGW当前源码构建、测试和运行包（2026-10-21）

使用用户安装的Windows Qt5.14.2/MinGW7.3 x86_64工具链从当前源码原生构建六个操作端程序；构建回执状态`complete`，Qt版本为5.14.2，六个PE输出均为x86_64。随后使用同一Kit在Windows原生进程运行完整Qt测试：Widgets 103通过、0失败、1项Unix专属活跃目录锁测试跳过；ECC客户端63/63通过。测试清理回执显示私有TEMP已删除、raw剩余0、未触及20GiB限额。

新运行候选（71.80历史快照）包含六个程序、匹配Qt/MinGW DLL和插件、操作文档、示例配置、offscreen及Windows原生窗口截图，268项manifest文件。去掉Qt SDK的PATH后六个程序帮助检查通过，GetDAQ两个初始窗口smoke通过。GetDAQ SHA-256=`bd547cfbc5382126f6099930b1c247aa7a571d1fa446a908804d44c15e758a53`；包完整性检查通过。候选包、验证摘要、构建回执与清理回执目录已于2026-10-23清理；这里保留历史摘要，不代表当前源码Windows验收。

这是用户本机Windows上的原生Windows程序和窗口测试，由WSL工作树调用已安装的Windows工具；不是独立干净Windows主机、真实Linux ECC服务连接或板卡验收。候选未连接硬件，也不包含DAQ传输。完整主机和交付证据边界见[重构状态](REFACTOR_STATUS.md)及[Windows构建手册](WINDOWS_QT5142.md)。

## 71.80 Ubuntu24.04.5当前源码全套验证与27程序候选包（2026-10-21）

在已验签的Ubuntu24.04.5用户空间中，以专用普通用户`uid=1000`通过项目WSL2 runner执行`tests/ubuntu24_platform_validation.py --work /build/getdaq-platform-71-79 --jobs 4 --stage all`。底层内核仍为WSL2的5.15.167.4，因此本记录是Ubuntu24用户空间兼容性与全套回归证据，不是独立Ubuntu24主机验收。工具链为GCC13.3.0、CMake3.28.3、Qt5.15.13。

当前源码full profile 27/27程序构建及105项嵌入依赖校验通过。配置核心149/149、Widgets104/104、原生CTest22/22、配置源迁移12种崩溃恢复情景、事务runner、接收器传输、安全和四流接收回归全部通过。接收器四路各256MiB，共1GiB；连同传输协议夹具，本轮合成wire总量1,073,791,563字节，小于20GiB上限。平台回执记录`raw_remaining=0`，工作目录内没有`.graw`。Ubuntu24平台回执SHA-256=`98abc23891ad48c4f28c069310e6fac2733b1f5d6d992213ff82ac76bee98161`；应用构建回执SHA-256=`07c55f076f43417765549ff3c7dd72c2331f41f2f0c98ed1f6a0b216bb9ca962`；当前构建输入摘要=`0498a9a739a8096db6981bf4e515383bc2eaa12bca7a90432ddee4f5b97e74ea`。

基于这次27程序full build生成当前Ubuntu24候选`build-packages/getdaq-ubuntu24-20261021-71-80-current-source-full-candidate/`，包含27个程序和176项manifest文件，总大小约91MB。逐个使用Ubuntu24 rootfs动态加载器检查27个包内程序，依赖均能在目标用户空间解析；包内GetDAQ的`--help`、offscreen主窗口启动、ready日志与PNG截图检查通过，临时截图已删除。GetDAQ SHA-256=`b9b7800443b486bb45f935b142bbe7fa4cae52a9a0e3aff7efac01b27d8be609`。轻量摘要在[Ubuntu24回执](../build-delivery/current-source-ubuntu24-full-suite-71-80.json)，包内还包含逐程序loader回执和平台runner原始JSON回执。

该结果没有覆盖独立Ubuntu24机器、macOS/Windows原生构建、两台主机普通TCP归档、真实旧gSOAP服务、板端或现场持续吞吐。候选包依赖目标Ubuntu24系统提供的运行库，不是静态自包含二进制；它也不能替代现场配置、网络和硬件验收。

## 71.79 当前源码全量重建、平台门槛修正与有界接收回归（2026-10-21）

在Ubuntu20.04.6/WSL2、GCC9.4、Qt5.12.8下，用`tools/build-suite.py --profile full`在独立`/tmp`目录重新构建当前应用源码的27个程序；构建回执完成，105项嵌入依赖检查通过。之后独立构建配置核心、Widgets、迁移探针、事务探针和严格C++11/`-pedantic-errors`原生测试。配置核心QtTest 149/149、Widgets QtTest 104/104、原生CTest 22/22通过；迁移和事务runner返回0。完整Widgets套件使用模拟TCP服务；沙箱首次运行因禁止本机loopback监听而产生级联失败，在获准的本机loopback权限下重跑后104/104通过。

接收器回归使用本轮全量构建的`getdaq-receiver`：协议传输runner返回0，记录合成wire 49,739字节；四路接收/轮转/哈希/EOF/seal运行各256MiB，合计1,073,741,824字节。负载安全清理测试9/9、Windows/WSL入口合同测试13/13、配置示例刷新测试2/2及Sentry示例检查通过。全部合成测试wire合计1,073,791,563字节；逐项清理回执成功，临时测试树无`.graw`。构建和测试源码树位于`/tmp/getdaq-full-current-71-79`，属于临时验证资料，不作为交付包保留；摘要见[当前源码回归回执](../build-delivery/current-source-full-suite-71-79.json)。

本次复核发现`tests/ubuntu24_platform_validation.py`在SOAPAction Qt回归加入后仍要求Widgets恰好103项，而实际完整测试结果是104项；该错误门槛会让Ubuntu24 `--stage all`在通过测试后仍失败。现在期望数集中为`core=149`、`widgets=104`，新入口合同用例会拒绝过期的103门槛，并检查完整结果确实达到预期。此修正只补齐验收runner，没有改变产品协议或应用二进制。

上述结果是Ubuntu20开发宿主上的当前源码构建和单机loopback回归，不是Ubuntu24执行。Ubuntu24平台runner仍须在真实Ubuntu24/GitHub Actions运行；双机普通TCP传输、Windows原生Actions、macOS、真实旧服务进程、板端和现场持续数据吞吐也仍需独立证据。全流程合成数据低于用户设定20GiB上限且测试后删除，未访问硬件。

更新：2026-10-21（71.81）。本文记录各阶段实际运行的检查，不把目标平台、方案或源码归档当作测试结果。章节及检查点绑定当时的源码和程序；后续变更后旧摘要只作历史，不代表当前整套工作树。71.62新增Ubuntu24当前源码全套GitHub Actions验收入口，71.63新增Windows Qt5.14.2/MinGW7.3产品构建入口，71.64将Windows完整Widgets/ECC客户端QtTest接入该job，71.65补强测试runner在目录/夹具初始化失败时清理自有临时内容；71.66核对原生Windows QtTest内嵌构建目录与回执路径一致；71.67在节点设置窗口提示核对未实机验证的默认CoBo/MuTaNT地址，并说明Native ECC拓扑来源；71.68将更新后的GetDAQ控制台增量装入Ubuntu24候选并在该候选运行时启动主窗口；71.69让Windows Qt 5.14.2 job在完整构建/QtTest后部署六程序运行包、刷新详细文档、校验运行清单并上传artifact。71.70补全Ubuntu24候选GetDAQ启动 smoke 的可写XDG用户数据目录和应用日志合同；71.71从当前工作树在Ubuntu20.04.6/GCC9.4/Qt5.12.8重新全量构建27程序，并以隔离HOME/XDG和获准的loopback重跑配置核心149/149、Widgets103/103、CTest22/22；71.72补齐Ubuntu24/Windows候选中落后的Sentry配置示例，刷新器只同步`share/config/`并校验其他包文件保持不变；71.74把配置示例刷新器2项回归接入Ubuntu/macOS原生数据工作流；71.75在入口回归中固定刷新器回归的工作流顺序，本机入口测试11/11通过；71.76为旧SOAP 1.1客户端补充必需的空SOAPAction头，并在本机Qt完整界面/控制测试中104/104通过。71.76仅是当前Ubuntu20/Qt5.12.8的本机回归；不代表真实旧gSOAP服务进程、Windows、macOS或硬件验收。GitHub Actions尚未实际运行。71.77进一步将SOAPAction回归纳入Ubuntu24候选GetDAQ刷新门槛，并在候选用户空间验证窗口/启动日志；仅刷新GetDAQ，其他26个程序未变。

## 71.78 旧 GetBench SOAPAction 分派依据复核（2026-10-21）

为判断`SOAPAction: ""`是否会与旧 GetBench ECC 服务冲突，重新检查归档服务的生成接口及构建规则：`ecc.hh`没有`method-action`声明，`Makefile.am`调用`soapcpp2 $(SOAP_CPP2_FLAGS) -L -x -pecc -n`，`configure.ac`只会在较新 gSOAP 添加`-z1`，未启用`-a`或`-A`。W3C SOAP 1.1规定HTTP SOAP请求必须带SOAPAction，空字符串表示意图由HTTP Request-URI提供；Genivia文档说明`-A`要求并按SOAPAction分派，`-a`在有该头时使用它分派，否则按SOAP/XML请求方法名分派。

据此，**基于旧接口/构建源码和gSOAP官方选项文档推断**，未启用`-a`/`-A`的旧服务器应按SOAP Body操作名分派，空SOAPAction不会改变其分派依据。该源码检查进一步支持71.76的空头选择，但仍不是旧生成服务二进制或现场服务实测。通信手册已记录HTTP头合同、推断依据与现场验证边界；候选包尚未因本轮文档更改而刷新。链接：[W3C SOAP 1.1](https://www.w3.org/TR/SOAP)、[Genivia gSOAP用户指南](https://www.genivia.com/doc/guide/html/index.html)。

## 71.77 Ubuntu24候选纳入旧 SOAP 请求头回归（2026-10-21）

71.76的SOAPAction修复原先已通过当前源码本机Qt完整测试，但Ubuntu24候选刷新门槛只检查UI地址/拓扑提示，可能导致候选更新不包含这项兼容性证据。本轮扩展`tests/run_envoy_ui_package_test.py`：对同一新构建的Widgets测试二进制分别执行`nativeGuiMetadataAndWholeSystemControls`和`legacySoapHttpRequestSendsQuotedEmptySoapAction`；刷新器要求两项scenario都为true，并校验runner及主程序SHA。新增入口合同测试，防止以后回归器与刷新策略脱节。

Ubuntu20.04.6/WSL2、GCC9.4/Qt5.12.8/C++11当前源码重建GetDAQ和QtTest程序。两个定向Qt场景均3/3通过；`tests/test_windows_build_entrypoints.py` 12/12通过。刷新后的GetDAQ交付SHA-256为`b9adc00a4d5ccc8f720c45aec5ea44757c539063416da14aa319d1c6744ede23`，源码构建SHA为`515650c3821d108a066203edffd2b9eebe82472910d8a0d37a356f9347e96b52`。Ubuntu24.04用户空间rootfs loader、Qt offscreen插件、帮助入口、真实主窗口截图及`GetDAQ ready`启动日志检查通过；测试截图已删除。候选`getdaq-ubuntu24-20261020-71-34-current-source-candidate`为27程序/486文件，本轮仅重建GetDAQ，其余26程序复用；包刷新前后完整清单校验通过。回执`validation/program-refresh-GetDAQ.json`和`validation/envoy-ui-package.json`均已入包，源码回归收据见[envoy-soapaction-package-71-77.json](../build-delivery/envoy-soapaction-package-71-77.json)。没有生成raw或访问真实硬件。

此结果验证本机mock TCP服务收到正确SOAP 1.1 HTTP头，并验证候选GetDAQ在Ubuntu24用户空间可启动；不是gSOAP生成服务互通、独立Ubuntu24主机、Windows、macOS或现场硬件验收。候选仍是组合增量包，不是27程序当前源码全量构建。71.78对照旧GetBench的soapcpp2实际构建命令，确认未启用按SOAPAction分派的`-a`/`-A`选项；gSOAP默认Body分派是基于源码与官方文档的推断，不是旧服务进程实测。

## 71.76 旧 ECC SOAP 1.1 请求补充 SOAPAction（2026-10-21）

源码中的旧GetBench gSOAP接口声明SOAP 1.1 HTTP传输，但`apps/envoy/eccclient.cpp`此前只发POST和XML，没有`SOAPAction`头。SOAP 1.1规定HTTP SOAP请求必须携带该头；当没有定义具体action URI时，空值表示操作意图由请求URI给出。[SOAP 1.1第6.1.1节](https://www.w3.org/TR/SOAP)与本地`source/get/GetBench-software-release-20210208/src/get/soap/ecc.hh`（未声明method-action）据此采用精确HTTP值`SOAPAction: ""`。

`EccClient`现在给每个旧SOAP操作添加带引号的空SOAPAction。真实本机TCP测试服务记录请求头；`legacySoapHttpRequestSendsQuotedEmptySoapAction`通过，确认实际`QNetworkAccessManager`请求确实携带该值并且响应解析成功。完整当前Qt测试程序按Qt5.12.8/GCC9.3/C++11重建，完整测试104/104通过。哈希绑定小型回执为[soapaction-legacy-http-71-76.json](../build-delivery/soapaction-legacy-http-71-76.json)；临时构建目录已删除，没有生成raw。

本机测试端是模拟ECC SOAP服务，不是gSOAP生成服务或现场旧服务进程。因此这证明请求符合SOAP 1.1 HTTP头合同，不证明旧服务器进程互通；真实gSOAP兼容回归仍需相应运行库/夹具和目标服务环境。

## 71.75 固化候选示例刷新回归的工作流顺序（2026-10-21）

在`tests/test_windows_build_entrypoints.py`加入工作流合同：要求`.github/workflows/native-data-path.yml`先运行当前/归档Sentry示例检查，再运行`tests/test_refresh_package_examples.py`，之后才验证离线数据依赖。这样配置候选刷新器的2项路径保护、manifest更新与程序保留测试不会只依赖人工检查。

`python3 tests/test_windows_build_entrypoints.py` 11/11通过，`python3 tests/test_refresh_package_examples.py` 2/2通过，Markdown文件链接1299项通过。无`.graw`生成。Windows原生Qt构建与GitHub Actions尚未运行，因此没有新增平台验收证据。

## 71.74 配置示例候选刷新回归纳入CI（2026-10-21）

`tools/refresh-package-examples.py`负责把当前源码中10份配置示例安全同步到已有Ubuntu24/Windows候选包；它在暂存副本重算manifest并验证后才替换候选，且要求程序和其他包文件保持不变。新增的`tests/test_refresh_package_examples.py`原先只在人工检查时运行，未接入自动工作流。现将它加入`.github/workflows/native-data-path.yml`的`native-data-path` job，Ubuntu 24与macOS矩阵都会执行。

本机`python3 tests/test_refresh_package_examples.py` 2/2通过，`tests/test_sentry_examples.py`通过；PyYAML解析workflow并确认刷新器回归位于Sentry示例与依赖检查之间。Markdown文件链接检查1299项通过。测试只使用小型临时候选目录，自动清理；没有生成DAQ raw。此项增加CI覆盖，GitHub Actions尚未运行，不新增平台验收证据。

## 71.72 修复候选包中的Sentry进程名示例（2026-10-21）

复核已部署的Ubuntu24与Windows候选时，发现包内`share/config/sentry/node.example.json`仍将旧`dataRouter`作为受监控进程名，而当前源码示例使用`getdaq-receiver`。旧值会让照抄示例的Sentry在新接收器运行时误报进程缺失。

新增`tools/refresh-package-examples.py`，从`tools/build-suite.py`的实际打包清单同步全部10个`share/config/`示例。工具限定在`build-packages/`内的有效候选，先验证源包与模板清单，再在候选旁构建完整暂存副本；它重算manifest、验证暂存包，并逐文件确保程序、运行库、文档及既有收据摘要不变，最后再替换候选。测试夹具也覆盖了路径拒绝、模板更新和程序摘要保留。

当前Ubuntu24候选`getdaq-ubuntu24-20261020-71-34-current-source-candidate`刷新后仍为27程序/486文件；Windows Qt5.14.2候选`getdaq-windows-qt5142-20261020-legacy-attr-boundary-candidate`仍为6程序/273文件。两个包中的10份配置示例现与源码SHA一致；Ubuntu24 manifest校验、Windows运行包校验、`tests/test_refresh_package_examples.py` 2/2、`tests/test_sentry_examples.py`及Windows入口10/10通过。文档链接检查1298项通过。本轮未生成或传输raw。

这只修复了随包分发的配置示例，没有重建Windows或Ubuntu24程序。Windows候选依然不含71.67后的界面二进制；Ubuntu24仍是增量组合包。独立操作系统、Actions、板端、双机和持续DAQ验收状态未变化。详细候选刷新步骤见[整套构建手册](HOST_SUITE_BUILD.md)。

## 71.71 当前工作树完整Linux构建及三组回归（2026-10-21）

从当前源码运行`python3 tools/build-suite.py --profile full --build-root /tmp/getdaq-current-full-20261021 --jobs 2`，成功生成27个主机程序，嵌入依赖检查105项通过。环境为Ubuntu20.04.6/WSL2 Linux 5.15.167.4、GCC 9.4.0、Qt 5.12.8、CMake 3.16.3；精简构建及测试摘要、27个程序摘要和468项源码输入摘要保存在[验证回执](../build-delivery/ubuntu20-current-source-71-71.json)，没有保留整套临时二进制。它不是可移交的Ubuntu24候选。

当前测试源重建后，配置核心149/149、完整Widgets 103/103、原生CTest 22/22通过。Qt测试使用私有`HOME`和XDG目录；需要本机loopback的Qt/CTest场景在本机授权运行后通过。ByteStorage集成生成393,353字节合成raw，测试报告`raw_remaining=0`；本轮无大文件传输，完整suite无硬件访问。入口runner测试10/10通过，文档链接检查1297项通过，Ubuntu24候选manifest仍是27程序/486文件且完整性检查通过。

首轮未隔离Qt用户目录时，配置测试尝试写入默认用户缓存并失败；普通受限shell下loopback绑定也被拒绝。这些是运行环境限制，不作为产品通过/失败证据；修正隔离目录并用本机loopback权限重跑后的结果才计入上述通过数字。临时构建与测试目录在记录后删除。该结果不代表独立Ubuntu24、Windows Qt5.14.2、macOS、真实板卡/双机、Conduit现场互通或持续DAQ磁盘吞吐验收。

## 71.70 Ubuntu24候选GetDAQ首启日志合同（2026-10-21）

对71.68更新后的候选GetDAQ做实际Ubuntu24 Qt 5.15.13 offscreen窗口截图并目视检查时，截图初次显示`Cannot open application log: No such file or directory`。原因是手动 smoke 环境没有把`XDG_DATA_HOME`指向可写目录，Qt使用沙箱外的默认用户目录；主程序本身已有`QDir().mkpath(logDir)`。修正候选启动环境，将XDG runtime/config/cache/data都隔离到权限0700的临时目录，并要求启动后生成含`GetDAQ ready`且不含日志错误的日志，再确认截图PNG有效并删除截图与临时目录。

当前源码`GetDAQ`/Widgets测试程序按Qt5.12.8/C++11重建；`nativeGuiMetadataAndWholeSystemControls` 3/3通过。Ubuntu24候选实际加载Qt5.15.13并完成offscreen`--help`、主窗口启动、PNG截图及115字节应用日志检查，正常退出；截图126,137字节已删。候选`build-packages/getdaq-ubuntu24-20261020-71-34-current-source-candidate`完整性通过，27程序/486文件；本轮只刷新GetDAQ，其余26程序沿用原候选，交付SHA仍为`c4b99d9721de50d6c620946637fa04e0b591e8b7afa4afc0ca8c904dc464d4a8`。`tests/test_windows_build_entrypoints.py`增加日志检查器缺失/错误/成功三种用例后10/10通过；无`.graw`生成，临时构建与截图已清理。

边界：目视截图只用于发现初次启动日志环境问题；通过后验证的是隔离用户目录下的单机窗口/日志启动，不是Windows/macOS、ECC/硬件、现场用户权限、真实拓扑或运行控制验收。Ubuntu候选仍是组合增量包，不是当前源码27程序全量重建。证据：`validation/program-refresh-GetDAQ.json`、`validation/envoy-ui-package.json`、`tools/refresh-ubuntu24-program.py`、`tests/test_windows_build_entrypoints.py`。

## 71.69 Windows Qt5.14.2 CI生成可检查运行包（2026-10-21）

Windows job此前只上传构建与QtTest日志，无法直接取得可运行的当前源码包。现在`windows-qt5142-control-build`在六程序原生构建、Widgets/ECC完整QtTest及程序回执检查后，调用既有`tools/deploy-windows-from-wsl.py`使用同一Qt 5.14.2/MinGW 7.3 kit收集Qt插件和编译器运行库，并以不含Qt SDK路径的`PATH`运行六程序帮助检查和GetDAQ初始窗口offscreen截图。随后调用`tools/refresh-package-docs.py --kind windows`复制当前 README/操作手册，最后再次校验运行目录中的程序、运行库与清单SHA；完整`runtime/`随构建回执和测试结果上传为14天artifact。

本机验证：`tests/test_windows_build_entrypoints.py` 9/9通过，包含native Windows路径选择及workflow中测试→打包→验证→上传的顺序断言；PyYAML解析workflow并核对job步骤通过。当前旧Windows候选通过`tools/deploy-windows-from-wsl.py --verify`，六程序/263文件完整；控制profile嵌入依赖95项、Markdown文件链接1296项通过。没有构建Windows程序或生成DAQ数据。

边界：GitHub Actions尚未运行，因此新的Windows运行包尚未实际产出；现有Windows候选仍是旧二进制，仍未包含71.67后的UI提示。此提交建立可重复CI交付路径，不构成Qt5.14.2真实构建、Windows运行、GUI人工验收或独立机器硬件验收。工作流与回归位置：`.github/workflows/native-data-path.yml`、`tests/test_windows_build_entrypoints.py`、`tools/deploy-windows-from-wsl.py`。

## 71.68 Ubuntu24候选更新GetDAQ控制台并验证Qt启动（2026-10-21）

为避免将71.67后的窗口提示只留在源码，本轮扩展`tools/refresh-ubuntu24-program.py`，让它支持`GetDAQ`专用单程序刷新。该路径要求测试收据绑定主程序SHA、当前runner SHA和定向GUI回归；随后对候选程序检查Ubuntu24动态依赖、offscreen帮助、Qt平台插件、真实主窗口启动及PNG截图签名，成功后删除截图并原子替换候选。它不改动或重建其余程序。

从当前源码以本机Ubuntu20.04.6/GCC9.4/Qt5.12.8构建`GetDAQ`与Widgets测试程序，`nativeGuiMetadataAndWholeSystemControls`为3/3通过。更新后的候选`build-packages/getdaq-ubuntu24-20261020-71-34-current-source-candidate`内，GetDAQ SHA-256为`c4b99d9721de50d6c620946637fa04e0b591e8b7afa4afc0ca8c904dc464d4a8`；Ubuntu24 rootfs Qt5.15.13 loader及offscreen插件下，`--help`和实际窗口截图/正常退出通过，PNG在验收后删除。候选包完整性检查通过，manifest为27个程序/486个文件，增量回执为`validation/program-refresh-GetDAQ.json`，GUI回归为`validation/envoy-ui-package.json`。

边界：这是Ubuntu24.04用户空间运行在本机WSL/Linux宿主内核，不是独立Ubuntu24机器验收。仅GetDAQ从当前源码重建，另外26个程序复用候选原有字节；因此候选仍是组合增量包，不是27程序当前源码全量包。Windows候选还未包含71.67后的控制台，Windows Qt5.14.2和macOS本轮未运行；没有连接ECC/硬件、启动receiver或生成`.graw`，不是现场地址/拓扑验收。临时构建和截图均已清理。刷新实现及可复用runner见`tools/refresh-ubuntu24-program.py`、`tests/run_envoy_ui_package_test.py`。

## 71.64 Windows QtTest构建与运行器支持原生Windows入口（2026-10-21）

用户Qt路径在当前WSL工作区可见，但通过`tools/build-windows-from-wsl.py`启动实际`qmake.exe -query QT_VERSION`仍失败：WSL返回`UtilBindVsockAnyPort: socket failed 1`。检查确认失败发生在Qt编译前，未创建构建目录，不是代码编译失败或Qt版本不符。

为让用户的Windows Qt Kit和Windows CI不依赖WSL互操作，`tests/build_windows_qt_validation.py`现在使用原生`cmd.exe`命令选择、仅在POSIX设置可执行位，并用跨平台独占目录锁代替`fcntl`文件锁。`tests/run_windows_qt_validation.py`新增原生Windows路径：由Windows本地Python/PowerShell启动QtTest，以产品一致的`windows`插件和Fusion风格运行，参考配置复制到本次专用构建根，并按原逻辑将runner私有TEMP限制为512MiB、监视测试`.graw`聚合不超过20GiB、结束后清理临时目录。原WSL Z:映射/临时本机目录逻辑保留。

Windows `windows-qt5142-control-build` job现在依次构建六个产品程序、完整Widgets与ECC客户端QtTest目标，并运行完整QtTest；always上传14天构建日志、结果摘要和清理回执，不上传`.exe`。本机运行`tests/test_windows_build_entrypoints.py`为6/6，覆盖原生路径选择及运行库回执完整性/篡改拒绝；Python语法检查、workflow YAML及步骤/证据静态合同通过；文档链接1296条、嵌入依赖105项通过。当前执行环境不能启动该Windows Kit，GitHub Actions也尚未运行，所以本轮没有Windows二进制或QtTest结果；不能据此声称Windows构建/测试已通过。

## 71.63 Windows Qt 5.14.2 MinGW 当前源码构建CI入口（2026-10-21）

在71.63检查点，`.github/workflows/native-data-path.yml`新增`windows-qt5142-control-build` job，在`windows-2022`安装Qt 5.14.2 `win64_mingw73`桌面套件和MinGW 7.3.0工具，调用`tools/build-windows-from-wsl.py`的原生Windows入口，从当前检出源码构建六个控制程序：`GetDAQ`、`getdaq-sentry`、`getdaq-simulator`、`getdaq-config`、`getdaq-config-check`、`getdaq-config-upload`。完成回执检查Qt版本、程序集合和x86_64 PE架构；CI保留14天的JSON构建回执和编译日志，不上传可执行文件。当时该job只编译；71.64随后增加了当前源码QtTest构建和运行步骤，详见下一节。

## 71.67 节点设置窗口提示拓扑配置边界（2026-10-21）

操作台的默认配置预填11个CoBo和一个MuTaNT网络地址。它们尚无现场联调证据；此前“Nodes and paths”窗口没有提示用户先核对地址，Native ECC模式又使用单个ECC控制端点和固定服务输入提供的板端拓扑，容易让人混淆两套连接方式。现在传统逐节点模式显示醒目的核对提示；Native ECC模式显示单一控制端点/固定拓扑来源说明，并隐藏传统逐节点地址字段。默认地址和通信行为均未改变。

新增`nativeGuiMetadataAndWholeSystemControls`界面断言覆盖传统提示、原生提示与模式切换。在Ubuntu20.04用户空间、Qt 5.12.8/GCC 9.3/C++11重新构建`tests/tests.pro`，offscreen定向回归3/3通过（含init/cleanup）；没有连接ECC/板卡、启动receiver或生成`.graw`。完整Widgets suite、Windows 5.14.2和macOS本轮未重跑；现场地址仍须按实际拓扑确认。Ubuntu24与两个Windows候选只刷新了文档并通过完整性检查，程序/运行库未重建，因此71.67的界面行为只属于当前源码，不能视为已进入这些候选二进制。

## 71.66 Windows QtTest核对内嵌构建目录（2026-10-21）

原生Windows runner此前校验QtTest程序和运行库哈希，却没有读取该构建目录`Makefile.Release`中的`QT_TESTCASE_BUILDDIR`。这一路径供`QFINDTESTDATA`查找构建树以外的配置和源码夹具；若它指向别处，即使程序哈希有效，测试也可能读错目录。现在runner在启动QtTest前解析Makefile并要求内嵌路径与当前回执绑定的`<build-root>/<test-name>`完全一致，路径不匹配时在创建夹具/启动测试前失败关闭。原WSL Z:映射分支已有独立路径检查，保持不变。

新增入口回归覆盖合法路径和另一个盘符/目录的偏移路径。`python3 tests/test_windows_build_entrypoints.py`为8/8，runner及测试文件Python语法检查通过。当前保留的Windows构建目录没有Makefile（已清理），且WSL启动Windows进程仍在编译前报`UtilBindVsockAnyPort: socket failed 1`，所以本轮尚无来自真实Qt 5.14.2 Makefile的运行结果；GitHub Actions尚未触发。没有生成DAQ载荷。

## 71.65 Windows QtTest夹具初始化失败清理（2026-10-21）

复核原生Windows分支时发现，runner先创建Windows临时根再创建结果目录，而结果目录创建失败发生在清理`try/finally`之前；此外，夹具`copytree`若只复制一部分后抛错，所有权路径尚未登记，失败残留不会被清理。现在结果目录先准备，再在清理保护范围内创建私有TEMP；夹具路径在复制前登记为本次自有目录，失败时由统一清理函数删除普通目录，遇到符号链接则不跟随、不删除其目标。WSL已有的Z盘与本机TEMP隔离路径保留。

新增隔离回归：注入夹具复制进行一半后失败，确认路径已登记并清除；同时确认指向外部目录的符号链接及其目标内容保持不变。`tests/test_windows_build_entrypoints.py`现为7/7；Python语法检查、Windows workflow顺序/证据合同、Sentry示例、1296条Markdown链接和105项嵌入依赖检查通过。测试未创建DAQ载荷。Windows原生执行与GitHub Actions尚未运行，因此没有新增Windows编译/QtTest通过证据。

本机静态核验：PyYAML解析workflow成功；确认Windows runner、Qt版本/架构/工具包配置、构建入口与现有脚本的原生Windows分支一致；将CI要求的六程序集合与`tools/build-suite.py`控制程序计划比对一致；回执检查字段、总是执行的构建证据上传及14天保留配置存在。`tests/test_windows_build_entrypoints.py`、`tests/test_sentry_examples.py`、文档链接和依赖清单回归通过。没有启动Windows编译器，也没有执行GitHub Actions，所以这只证明CI入口配置与静态约束通过；Windows编译、QtTest、干净机部署和完整GUI人工验收仍待真实runner/用户机器完成。此轮未生成DAQ载荷。

## 71.44 Sentry配置拒绝类型错误与非整数参数（2026-10-21）

`getdaq-sentry`配置解析现在要求可选`process_name`、`disk_name`和`bind`字段为字符串；`port`、`settle_ms`必须是有限、整数值且能安全表示为C++ `int`，随后继续执行端口0–65535及settle 0–120000的原有范围校验。错误字段类型、浮点小数、整数范围外数值均在监听服务启动前拒绝。未改HTTP协议、监控/归档行为或DAQ数据面，也没有新增依赖。

在Qt 5.12.8/C++11下从`tests/tests.pro`重建完整`getdaq-tests`成功；offscreen定向执行`sentryConfigStrictFields`为3/3（含QtTest init/cleanup，目标用例通过）。另外从当前源码独立构建`getdaq-sentry`，真实CLI对小数port、字符串settle和非字符串process_name均返回配置错误并在监听启动前退出。覆盖默认值及合法字段、port/settle小数和字符串、process/disk/bind错误类型、port/settle越界。临时测试构建和配置随后删除；常规忽略的`bin/getdaq-sentry`更新为当前源码构建。Ubuntu24及Windows交付候选本轮没有刷新，仍需单独按当前源码重建/验证。该定向测试不等于重跑完整Widgets回归或独立平台验收。

## 71.43 DataSupervisor失败退出清理自有合成载荷（2026-10-20）

修正`tests/run_supervisor_control_tests.py --cleanup-raw`的退出清理边界：runner在任何场景启动前注册进程退出回调；异常退出会由回调清理，CLI入口另显式捕获Ctrl+C并在以130退出前清理。清理只删除runner私有证据根下名为`.graw`的普通文件和符号链接，不触碰目录外目标、其他证据或`.graw`目录。完整结果路径仍在正常流程中执行20GiB聚合预算核验并写清理回执；早期失败时退出回调只负责清理，不伪造完整回归结果。SIGKILL、断电或文件系统失效无法执行Python退出回调，不能保证这些情况下即时清理。

新增`tests/test_supervisor_control_cleanup.py`两个测试：直接检查目录边界与符号链接目标保护，并分别通过未捕获异常及显式处理的`KeyboardInterrupt`终止子进程，验证清理合成载荷且保留普通证据。曾验证未捕获`KeyboardInterrupt`会绕过`atexit`，因此由CLI入口在异常传播前执行清理。Linux/macOS共用CI在运行完整服务矩阵前执行这项检查。当前GCC9.4/C++11/Qt5.12.8源码构建成功；三项archive CTest已由前述71.42同代码复核3/3。本轮在当前源码上重跑默认服务矩阵19/19、8项HTTP故障、codec和credential；预算回执仍为23个`.graw`共339,968字节，已全部删除、余量0。临时构建/证据根由命令退出清理。该验证不是GitHub Actions实际运行、目标平台或硬件验收。

## 71.42 DataSupervisor完整服务矩阵接入Linux CI及测试载荷清理（2026-10-20）

将Linux CI从单一`preconditions`场景扩展为`run_supervisor_control_tests.py`默认19个真实服务/接收器场景，覆盖正常Run收尾、断连/超时/UNKNOWN、控制归属、请求/日志故障、服务重启、凭据故障、部分启动、四种catalog场景及receiver观察恢复；同时包含8项HTTP响应故障、codec和credential检查。macOS仍只跑Qt-free数据面，不引入桌面控制服务。

CI调用runner时增加`--cleanup-raw`。runner先递归统计其私有结果目录中的所有`.graw`路径字节，任何路径合计超过20GiB或遇到非规则文件都会让回归失败；然后仅在该私有目录内删除合成raw并检查无残留，结果JSON写入已记录上限、观察字节、删除路径数和残余数。默认不传该选项的历史离线归档脚本仍可保留它们所需的测试证据；CI明确要求清理。

按当前workflow顺序在本机Ubuntu20用户空间/GCC9.4/Qt5.12.8复跑：native receiver构建成功，归档与协议CTest 3/3通过；同一DataSupervisor CMake目录由服务OFF重配置为ON并构建成功。完整runner的19个服务场景全部通过，另8项HTTP故障、codec和credential通过。raw预算回执：23个`.graw`路径共339,968字节（约0.33MiB），低于20GiB上限；23条路径均在私有测试目录删除，`remaining_paths=0`；测试后未留下raw。Workflow SHA-256=`1ed97b143becee76016e127f3579ba1ce5896a4f49a2ec8f6f6678f430bfd6f9`，runner SHA-256=`d4aba12af8798b1bcce39a519f29c7a57c8066ecf442ae03a6f5a52ed46e371b`。临时构建及结果目录随后已清除。

这证明Linux本机集成场景及清理预算行为，不是GitHub Actions已运行、独立Ubuntu24、macOS、真实双机、板端或DAQ吞吐验收。

## 71.41 Linux CI纳入DataSupervisor真实服务计划合同（2026-10-20）

将71.40的配置/规范计划断言接入`.github/workflows/native-data-path.yml`。Linux CI现在在先构建Qt-free接收器与归档核心后，重新配置DataSupervisor启用可选Qt服务，构建`getdaq-supervisor`，再运行真实服务`preconditions`场景及8项原生HTTP故障、codec和credential fixture；macOS job继续只构建/测无Qt数据路径，不额外引入Qt服务依赖。workflow YAML可解析。

按workflow顺序在本机Ubuntu20用户空间/GCC9.4/Qt5.12.8复跑：数据receiver全目标构建成功；归档/协议CTest准确发现3项且`receiver-catalog-filesystem`、`catalog-transfer-filesystem`、`supervisor-control-codec` 3/3通过；随后重新配置同一个CMake build tree从服务OFF切换到ON，`getdaq-supervisor`与fixture构建成功；计划合同、preconditions服务场景、8项HTTP故障、codec、credential全部退出码0。workflow SHA-256=`b1d1d69be6ab6f58b550841522f81849193860632ec47b0442be6ebdb6d034c4`，测试runner SHA与71.40相同。此轮在测试目录中未发现`.graw`；`/tmp`构建/集成证据已删除。

本机重放确认CI命令顺序和当前Linux工具链可行，不等于GitHub Actions已触发，也不代表macOS或现场硬件验收。

## 71.40 DataSupervisor检查/计划输出回归合同（2026-10-20）

将`--plan-config`加入`tests/run_supervisor_control_tests.py`的真实服务夹具初始化验证，避免离线检查只测到`--check-config`。每次服务集成场景创建配置时，runner现在会核对：`--check-config`给出的`plan_sha256`存在；`--plan-config`成功输出的JSON与配置中的receiver计划相同；规范输出原始字节的SHA-256与前一命令检查的摘要完全一致；两条离线路径都不输出服务凭据、不创建audit根或数据文件。验证发生在服务启动之前。

Ubuntu20用户空间/GCC9.4/Qt5.12.8严格C++11构建当前源码的`getdaq-supervisor`、`getdaq-supervisor-query`、`supervisor-control-tests`和`getdaq-receiver`成功。修改后的runner使用`--scenarios preconditions --jobs 1`运行：真实服务配置检查/计划生成、服务启动后的preconditions HTTP场景、8项原生HTTP格式/截止故障场景、codec和credential fixture全部退出码0。运行输出的`results.json`记录四项通过；合成`.graw`生成量为0。重要SHA-256：runner=`d9308e857eafd2e371a9b39382c1604e71190152dd63623c7988b03d1caac92b`，service=`55ba66d80c9dd34b560b522607a423c0837207a64f78eb351f1c46a8ef042f0e`，fixture=`8971c17c7d559f11ac63ce3809fceeb87c944a5d11a25d94b4c15deeec939381`，receiver=`3ecdf4fdb3dc5ebd2d0a5213b7ae0bd8bd1c8e2dccffe06a13e333ef279f3fb1`。临时构建、凭据和运行证据已清理。

这只覆盖本机服务夹具及离线计划合同，不代表真实ECC请求、DAQ硬件、双主机、macOS或持续吞吐验收。

## 71.39 DataSupervisor离线配置检查与计划输出（2026-10-20）

此前`getdaq-supervisor --check-config`与`--plan-config`未能得到成功证据，原因是执行沙箱禁止`getifaddrs`枚举本机IPv4接口。现从当前工作树以GCC9.4、C++11和Qt5.12.8重新构建`getdaq-supervisor`，并针对Ubuntu24候选中的`getdaq-receiver`创建独立临时配置和0600凭据。获准只读枚举本机接口后，`--check-config`成功，输出`plan_sha256=2d3796f686006a33cffb043d36288e058479937dd4052dac958a5bd460cba424`；`--plan-config`成功输出规范计划，receiver程序SHA与候选二进制一致。两项均未启动服务、打开端口、执行receiver，也未创建audit/control根或数据输出目录。服务二进制SHA-256=`55ba66d80c9dd34b560b522607a423c0837207a64f78eb351f1c46a8ef042f0e`。随后已删除临时构建、临时服务配置和凭据；没有生成raw数据。

这只验证离线配置编译/计划序列化和本机接口检查路径，不是DataSupervisor实际服务启停、HTTP生命周期、板卡控制或真实配置部署验收。

## 71.38 当前源码无认证普通TCP归档传输回归（2026-10-20）

为核对跨主机归档遵循普通TCP/IP、无应用层身份认证的要求，从当前工作树以GCC 9.4、严格C++11构建`getdaq-catalog`和`catalog-transfer-tests`。构建成功；`getdaq-catalog --help`明确列出TCP发送、续传和监听入口，并说明不提供认证或加密。测试程序先在默认受限环境遇到loopback bind拒绝，随后在获准的私有本机loopback端口完整运行通过。

回归覆盖普通TCP新归档、连接拒绝、发送后中断、相同来源显式续传、完整目标读回、完整归档零载荷复续传，以及独立CLI监听/发送进程。最终报告为`PASS ... plain TCP new-transfer/interruption-resume/readback/completed-no-payload-resume/CLI bounded connection refusal/listener-send separate-process/readback ... 12 native catalog transfer scenarios`。合成`.graw`共生成并删除15,245,312字节，低于64 MiB测试预算与用户20 GiB上限；测试私有根目录清理后为空，未访问硬件。构建产物SHA-256：`getdaq-catalog`=`c452b82f37884d3ab6a24c55d31d095f83615f06cd2c424ec26f0145925ad569`，`catalog-transfer-tests`=`aa1de3fa0e2c937ad0157432f5dd8e571af6695da21168c1f485cb2dbcc13d98`。

这证明当前源码的本机TCP协议回归，不证明两台独立主机的防火墙/DNS/网卡/磁盘互通、macOS、现场DAQ或持续吞吐。普通TCP无认证、无加密，只有受控可信网络可按此部署；SHA-256校验不能替代认证或防篡改。

## 71.37 CoBoFrameViewer `.gmat`波形矩阵转换（2026-10-20）

源码复核发现同一旧CoBoFrameViewer源码树有真实`.gmat`消费者：`CoBoFrameViewer::setMatrix()`按`4*68*512*sizeof(double)`定位帧，`CoBoEvent::fromMatrix()`按该布局读样本；旧`graw2gmat`每通道却写512个`uint16_t`，每帧278,528字节，Viewer要求1,114,112字节，且旧写入不检查通道向量是否有512项。依据这个本地消费者而不是把旧写入当格式规范，新增独立离线原生`getdaq-matrix`。

新工具流式解析CoBo type 1 revision 0–5，按各revision对应块长度解释MFM总帧长度、头长度与末尾块填充；按AGET/channel布局验证全部4*68*512个bucket各自唯一存在，并保持旧`Channel::data()`的每通道样本到达顺序，精确输出IEEE-754 binary64 little-endian；无法提供四个完整AGET的type 2 compact帧、短/重复/越界样本、坏item布局和超限输出均拒绝。目标采用同目录临时文件、fsync和hard-link不覆盖发布；失败清理临时文件。默认输出上限20GiB，可用`--max-output-bytes`收紧。`--start`按识别的CoBo帧跳过，`--count`限制输出矩阵数。原始数据不经过Qt或DAQ在线接收路径。

GCC9.4/严格C++11构建目标`getdaq-matrix`和`matrix-tests`通过，`gmat-conversion`、`mfm-merge`、`receiver-core`专项CTest 3/3通过；Sanitizer版矩阵回归在ASan/UBSan插桩下通过（当前ptrace沙箱需关闭LSan）。合成CoBo type 1 revision 0–5逐版本使用对应32/64/256字节MFM块长度与头长度，覆盖binary64小端、非顺序bucket下每通道样本到达顺序、帧选择、不覆盖、20GiB上限和坏帧拒绝；SHA绑定CLI回归也覆盖help/块编码/不覆盖/输出限额。累计生成并删除3,900,480字节合成`.graw`，输出合计最大5,014,592字节，远低于20GiB；`raw_remaining=0`且临时测试根目录为空。`ldd`显示只有系统C/C++/数学/加载器库，无Qt、Ice、Boost、yaml、curl、ROOT或CFITSIO。

当前Ubuntu24候选`build-packages/getdaq-ubuntu24-20261020-71-34-current-source-candidate`已通过单程序增量刷新加入`getdaq-matrix`，共27个程序、481个manifest文件。新二进制SHA-256=`ba6f83d5587076b7654df019130ea55df97d2b85e97dd7b84768a26c79426c52`，来源构建SHA-256=`0f14ef1d5f369a2323f5e0bb9b4b8dc6619c15b373cc3ad10f911a32cf228673`；Ubuntu24 rootfs loader、动态库解析和CLI帮助通过，包内`validation/gmat-conversion-regression.json`绑定该来源二进制，文档包刷新前后完整性校验均通过。此为增量加一个程序，不是27程序整套重编译；底层26程序全量平台证据继承自71.35，`full_suite_recompiled=false`。

完整receiver CTest首次执行中`mfm-merge`和`receiver-core`通过；`exporter-lock`及`byte-storage-integration`因当前沙箱禁止创建loopback socket失败，这是环境限制，不作为矩阵转换失败。专门重新运行的`gmat-conversion`通过。尚未用真实DAQ文件、旧Viewer程序或独立Ubuntu24/macOS做联合验收；合成结构回归不证明现场物理数据兼容。操作与格式约束见[矩阵转换手册](DATA_MATRIX.md)。

## 1. 实际开发环境

| 项目 | 实际值 |
| --- | --- |
| 系统 | Ubuntu 20.04.6 LTS，x86_64 |
| 编译器 | GCC 9 系列 |
| Qt | 5.12.8，qmake 3.1 |
| CMake | 3.16.3 |
| C++ 标准 | C++11 |
| GUI 测试 | Qt offscreen 插件 |
| 网络测试 | 127.0.0.1 临时端口，无真实 DAQ 连接 |

Windows Qt 5.14.2 MinGW 已有本机实际构建、测试和运行目录记录，见70.71及70.90。Ubuntu 24.04用户空间已有rootfs内构建与运行验证，但仍没有独立Ubuntu 24主机验收；macOS尚无成功构建/运行记录。以上均未经过真实GET板卡/完整硬件验收。

## 2. 已通过的构建

Qt 根工程 `GetDAQ.pro` 当前生成GetDAQ、getdaq-sentry、getdaq-simulator、getdaq-config和getdaq-config-check，统一输出在源码根目录`bin/`。早期章节的`build-suite/bin/`是当时路径，保留作历史；当前运行步骤见操作手册。

独立 `apps/datareceiver/CMakeLists.txt` 生成 getdaq-receiver、getdaq-inspect、getdaq-merge、getdaq-matrix 与对应原生测试程序。输出在 `build-receiver/`。64 位检查和 UNIX 平台要求在配置阶段执行。

检查接收器动态依赖仅有系统 C/C++、线程/数学/加载器库，没有 Qt、Boost、ROOT、Ice。未声称它是一个完全静态的可执行文件，也未声称任意 Linux 发行版均可直接复用二进制。

2026-10-20从当前工作树执行`python3 tools/build-suite.py --profile full --jobs 2`，25个操作台、原生服务、配置仓库、receiver及工具全部构建完成。构建主机实测为Ubuntu20.04.6 LTS / WSL2 x86_64、GCC9.4、Qt5.12.8、CMake3.16.3；构建前离线核对的105个嵌入依赖文件通过，25/25个程序的动态依赖均被检查。24个非GUI程序的`--help`或专用无参数用法检查通过；GetDAQ使用Qt offscreen启动初始窗口并按截图选项正常退出，临时截图在验证后删除。当前receiver构建还补跑`receiver-core`与`exporter-lock` CTest，2/2通过。整套轻量回执`build-delivery/full/full-suite-smoke-result.json` SHA256=`6600106984bb620643a8a2be45192c94f7c507b779c2baaedee5db99768b6513`，构建回执`build-receipt.json` SHA256=`4ecb0296cdccee504c68a7057f03a012cd6d30fbe79017cc54fc4992c96315ce`，窗口回执`gui-smoke-result.json` SHA256=`5c3c58eac853e5b13d4f5c86070a905c6bb072dba24f990ffde2b5a78eb87238`。沙箱内首次exporter-lock运行因禁止本机回环socket而在socket创建处失败；获准在本机回环环境重新执行后与receiver-core均通过。没有运行真实DAQ或生成`.graw`。这是当前源码在Ubuntu20.04上的整套构建/轻量冒烟证据，不替代Ubuntu24实际机器、macOS、Windows全功能或硬件验收。

## 3. Qt 自动测试

```sh
mkdir -p build-suite-tests
cd build-suite-tests
qmake ../tests/tests.pro
make -j4
QT_QPA_PLATFORM=offscreen ./getdaq-tests -o results-description.txt,txt
```

实际结果：**32 passed，0 failed，0 skipped**，约 18.5 秒。这包含初始化与清理两个 QtTest 条目，功能用例为 30 个。

覆盖：

- 硬件描述具体实例/default 展开、`C/S` 位域路径、64 位地址、失败不替换结果和三份真实 GET 样本。

- 原配置 include、中文/未知属性原字节保留、范围检查、外部修改保护和真实 GET 配置加载。
- 与旧 CompoundConfig 多层回溯案例一致的默认值来源查询。
- 自定义配置 ID、`.xml` 回退、外部 include 快照；已发布快照重试不换源；损坏和缺失引用拒绝。
- 恢复旧 Run 时拒绝已改变的 sentry/数据地址、配置 ID、SOAP 模式或配置目录。

- Paused 的设备可以正常 Stop，未跟踪的外部运行不会擅自 Catalog。
- 原生接收器活跃目录锁、缺失凭据、变化文件大小会阻止归档。
- SOAP 字符串中 XML 的两层解析、特殊字符、独立主机地址、禁用第二数据目标、自定义配置 ID。
- 无 MuTaNT 的单 CoBo 生命周期。
- State=Ready 但 Transition 非零时不能开始采集。
- 旧 YAML、多行/中文字段、路径/实验名校验、错误加载不破坏现有配置。
- SOAP/JSON 错误和畸形响应。
- 连续模拟运行、Run 目录不覆盖。
- 真实 HTTP 命令顺序、CoBo Start 失败时不启动 MuTaNT。
- 操作成功但状态未到位的总体超时。
- Catalog 超时不重复发送。
- Stop 等待 MuTaNT 实际状态确认。
- 中断恢复、备份重试不重复归档。
- GUI 生命周期和关闭线程。
- sentry 文件与 HTTP 行为。
- 独立模拟器到正式控制/归档实现的联调。

早期 `results-protocol.txt` 有一个旧 SOAP 字节串断言失败；修复后采用“解析外层 SOAP，再解析内层 ConfigId”检查实际语义。最终结果以 `results-description.txt` 为准，未隐藏早期失败记录。

## 4. 原生数据核心测试

```sh
cmake -S apps/datareceiver -B build-receiver -DCMAKE_BUILD_TYPE=Release
cmake --build build-receiver --parallel 4
cd build-receiver
ctest --output-on-failure
```

实际结果：**1/1 CTest 可执行项通过**，内含两种字节序、255 个分段位置、多帧合并、长度/item 越界拒绝、半帧拒绝、AsAd 分文件、事件号回退轮转、原始内容逐字节比较、固定内存池 10000 次交接。

不同 CMake 版本对 `ctest --test-dir` 的支持不同，以上使用进入构建目录的写法，避免旧版本在错误目录显示“没有测试”。

## 5. 真实本机 TCP 与跨进程归档

在项目根目录执行：

```sh
python3 tests/receiver_integration.py \
  build-receiver/getdaq-receiver \
  build-suite/bin/getdaq-sentry
```

Python 3 仅用于开发验证，脚本只使用标准库，接收器运行不依赖 Python。需要环境允许监听 loopback socket。第二个参数省略时明确跳过跨进程 sentry 测试，而不是把它当作通过。

实际通过：

1. 一个帧按单字节发送、后续多个帧合并发送；大小端混合；两个在线消费者得到完整长度前缀和相同帧内容。
2. 文件内容与输入逐字节相同；第二个接收器不能使用同一数据目录。
3. 卡住的导出客户端被断开；32 MiB 主文件内容 SHA-256 与发送端一致。
4. 合法帧后跟半帧：程序非零退出，先前完整帧仍被保存，不生成成功凭据。
5. 合法帧后跟非法超大长度：同样明确失败，保存先前完整帧。
6. 接收器仍存活时正式 sentry 拒绝归档；正常退出后具备封存凭据。
7. 离线 inspect 能读回文件并统计帧数；封存后修改文件大小，归档被拒绝。
8. 文件恢复到凭据对应大小后归档成功；归档内容完全一致；重复归档被拒绝。

一次 32 MiB 测试耗时约 0.08 秒，这是开发环境中的功能检查数值，可能主要受缓存影响，**不是正式磁盘吞吐量基准**。

### 内存与未定义行为检查

原生接收器另以 Debug、`-fsanitize=address,undefined -fno-omit-frame-pointer` 构建到 `/tmp/getdaq-receiver-sanitize`。核心 CTest 和包含正式 sentry 的完整 TCP 集成测试均通过，ASan/UBSan/LeakSanitizer 未报告错误。首次沙箱内 LeakSanitizer 因 ptrace 环境无法运行，之后在允许该检查的环境重新执行通过；没有将工具无法运行当成成功。

## 6. 第三方源码保存

yaml-cpp 0.8.0 已直接随 Qt 控制台编译。Ice 3.5.1、bzip2、mcpp 源码、来源、SHA-256 和原始许可已保存。`third_party/ice-compat` 已通过本地编译，重新生成 GET 接口并运行真实 TCP 回环：1.0/1.1 编码的状态、中文和类型化异常都通过，原有 Ice 接口测试通过；加上硬件访问层后，兼容工程 CTest 为 2/2（详见下节）。生成的 getdaq-ice-check 不链接 Qt；尚未连接实际硬件，完整 ECC 逻辑仍待迁移。IceEirfu 仍缺失；“完整离线硬件构建”标志保持 false。详见 [Ice 兼容组件](ICE_COMPATIBILITY.md)。

检查 Ice 文件完整性：

```sh
cd third_party/archives
sha256sum -c SHA256SUMS
```

这个校验只证明本地归档与记录一致，不证明编译成功或与板端兼容。

## 7. 新硬件访问层的本机验证

`getdaq-hardware`、`getdaq-hardware-core` 及 `hardware-client-tests` 已构建。`ldd` 显示只依赖系统运行库，不链接 Qt、Boost、ROOT 或独立安装的 Ice 动态库。

```sh
cd build-ice-compat
ctest --output-on-failure
cd ..
python3 tests/hardware_cli_integration.py \
  build-ice-compat/getdaq-hardware \
  build-ice-compat/hardware-client-tests
```

硬件访问层初版实际结果为 **2/2 CTest 通过**，其中 `hardware-client-tcp` 使用两种 Ice 编码，验证：

- 设备发现、中文、32 位寄存器和 4 位位域的宽度单位。
- 64 位全 1 值无精度损失；位域写入保留模拟寄存器其他位。
- 未知/只读/越界目标拒绝；前置校验失败时写调用次数不增加。
- 写操作不隐式预读；已有日志路径不会引发第二次写入。
- 远端先写后报错、先写后断开连接：只发送一次，日志标记 unknown。
- 多步骤计划整体验证；中间写结果不确定或期望值不匹配时，后续步骤不执行。
- 流水线初始化的 15 次写入及复位/中断控制顺序。

Python CLI 集成测试也通过，验证真实子进程、JSON、中文、uint64 输出、完整日志、拒绝重用日志与流水线入口。这些测试只连接 loopback 模拟板端。没有验证实际寄存器电气行为、校准效果或采集吞吐，原生数据程序本轮未修改；配置与 GUI 的回归另见第 3 节，不把这些结果当成真实硬件验收。

新 `hardwareclient.cpp`、`cobomaintenance.cpp` 与测试对象另以 ASan/UBSan 编译并启用 LeakSanitizer，测试通过且未报告错误。本次只对新硬件层及测试对象插桩；链接的历史 Ice 静态库没有插桩，因此不声称完成了全部 Ice 源码的内存检查。

### 硬件描述编译到部署

新增 `tests/description_integration.py` 已通过：三份真实硬件描述离线编译、原生 schema 校验、已有工件不覆盖、重复/未知键及数值溢出拒绝、模拟板端 11 设备创建/元数据读回、非空节点拒绝、只读策略和结构变化拒绝。原 C++ `hardware-client-tcp` 增加了两种编码下部分 CREATE 失败、响应丢失、不重试、高位地址及本地权限绑定测试，仍为 2/2 CTest 可执行项通过。既有 hardware CLI 集成也已回归通过。

真实样本展开规模：fullCoBoStandAlone 为 11/1310/1429（设备/寄存器/位域），Mutant 为 5/87/122，Zedboard 为 4/45/104。原始源码未改动。当前只验证元数据和模拟板端行为，未连接实机。

最终硬件描述版本另对新硬件访问层、描述读取层、CLI 与测试对象进行 ASan/UBSan 插桩，并启用 LeakSanitizer。C++ TCP 测试及完整 description_integration 跨进程测试均通过，未报告错误；历史 Ice、yaml-cpp 静态库及 Qt 编译器程序未插桩，不把结果扩大为这些第三方库的完整审计。共享导出入口调整后，另运行配置编译与 GUI 关闭相关的 4 个用例，QtTest 含初始化/清理为 6 passed、0 failed（results-description-final.txt）。

### ECC 状态约束与 AsAd 慢控制校准

本次新增 `apps/ecc/eccstatemachine.h/.cpp`，并扩展硬件计划的显式读取记录及 `CoBoAsAdCalibrate`。实际环境仍为 Ubuntu 20.04.6 / GCC 9.4，没有连接实机。

该阶段状态核心独立构建及 `bash tools/build-linux.sh ecc` 通过，`build-ecc` 内 CTest 为 **1/1 通过**。当时可选 Ice 工程 CTest 为 **3/3 通过**：`ice-get-tcp`、`ecc-state`、`hardware-client-tcp`。下面的持久化阶段已增加第四项测试。

状态测试覆盖 7 个协议状态与 9 个命令的允许/拒绝关系、完整运行周期、Reset/Breakup 的来源差异、预检查失败、标准/非标准执行异常、unknown 后拒绝重发及并发命令不排队。执行期间查询保留最后确认状态及当前 transition，不等待慢后端。

新增状态核心及测试、维护序列及硬件 TCP 测试，以及硬件访问/描述读取/CLI 均另用 `g++ -std=c++11 -pedantic-errors -fsyntax-only` 检查通过，避免依赖 GCC 的较新语言扩展。这个结果仅证明当前 GCC 下的 C++11 语法检查；Windows MinGW 的实际 Kit 验收仍待完成。

硬件 TCP 测试增加实际 CREATE 与新状态核心的联调：创建成功才提交 Described，部分创建、响应丢失和读回结构错误均设置 unknown；缺失 Prepare 流程时在 Validate 中拒绝，不报告 Prepared。这里是测试后端，不代表完整生产 ECC 已接好。

AsAd 校准测试分别使用 Ice 1.0/1.1，覆盖 mask 1/5/10/15、只访问启用板卡、AIM0 用户标签、AIM0～AIM3 顺序、低八位标签检查、正常结束恢复选择范围、monitorID/标签不符和标识读取错误后立即停止。计划最后一个目标不合法时，整体验证拒绝且不发生寄存器操作。

以下跨进程测试全部通过：

```sh
python3 tests/calibration_integration.py \
  build-ice-compat/getdaq-hardware build-ice-compat/hardware-client-tests
python3 tests/hardware_cli_integration.py \
  build-ice-compat/getdaq-hardware build-ice-compat/hardware-client-tests
python3 tests/description_integration.py \
  build-suite/bin/getdaq-config \
  build-ice-compat/getdaq-hardware build-ice-compat/hardware-client-tests
```

校准 CLI 测试同时验证描述绑定、两种编码、24 步稀疏板卡计划、显式 observed 记录、非法 mask 在连接前拒绝、描述不符预检查拒绝及已有日志不覆盖。第三方依赖清单 JSON 和文档本地链接检查通过；`ldd getdaq-hardware` 仍仅显示系统运行库，无 Qt 动态库。

最新状态核心、新硬件层、校准序列、CLI 和测试对象另进行 ASan/UBSan 插桩并启用 LeakSanitizer。独立状态测试、C++ 硬件 TCP 测试及校准跨进程测试全部通过，没有内存、未定义行为或泄漏报告。工件在 `/tmp/getdaq-ecc-calibration-asan`；历史 Ice/yaml 静态库未插桩，不能扩大为第三方代码完整审计。

该阶段没有修改 Qt 或数据接收实现，因此没有重复其既有完整测试。当时尚无持久化；下一节已经补入。网络服务和完整生产后端仍未完成；校准尚无实机结果，也无工具层自动采集状态联锁。操作和原理分别见 [ECC 状态机](ECC_STATE_MACHINE.md)、[AsAd 校准](ASAD_CALIBRATION.md)。

### ECC 持久化、启动核对与历史查看

新增 `EccPersistence`、`EccJournal`、`getdaq-ecc-journal` 和共用的 `getdaq-yaml-native`，没有增加外部安装依赖。`build-ecc` 完整构建及 **2/2 CTest 通过**；关闭日志的 `build-ecc-core-only` 构建及 **1/1 通过**。可选 Ice 工程整体 **4/4 通过**，新项目为 `ecc-journal`。

最终四项回归输出保存在 `build-ice-compat/results-journal.txt`；测试详细记录在同一构建目录的 `Testing/Temporary/LastTest.log`。

实际文件系统测试覆盖 scope 不匹配拒绝且不修改文件、进程锁、操作 ID 不重用、非法 UTF-8/参数拒绝、预检查失败不消耗 ID、正常完成后的历史查询、重启仍要求硬件核对、恢复检查失败不清除标志，以及恢复成功后保留原 unknown 记录。

三种真实子进程退出分别发生在 intent 后/动作后/complete 后，重开日志均按定义保留历史，未自动重放。另以子进程 `RLIMIT_FSIZE` 产生真实部分写失败，验证硬件未被调用、残尾不自动修复。文件在动作前/后被替换时，日志身份检查停止控制，分别保留“尚未执行”和“结果未知”的区别。截断、非法转换、序号跳跃、重复 ID/字段、未知字段及未来 schema 均拒绝且不修改原文件。

控制器通过可控同步门验证 Begin/Complete 存储等待期间状态查询响应和重复命令立即拒绝。空错误、非 UTF-8 错误和超长中文错误会转换为有界可读记录，重开文件仍能读取 unknown。

`hardware-client-tcp` 另把持久化控制器接到真实 GET TCP 模拟对象，分别使用 Ice 1.0/1.1：CREATE 成功记录 complete，响应丢失记录 unknown；重启不再次 CREATE，缺少实际 VerifyRecovery 实现时不能清除阻止。这仍是模拟板端和 Describe 子流程，不代表完整生产硬件恢复已经实现。

跨进程 `tests/ecc_journal_integration.py` 通过，覆盖 JSON/中文/按 ID 查询、只读不创建/不修改文件、锁冲突、scope 不符、截断拒绝和 SIGKILL 后读取未确认 intent。原硬件/校准/描述 CLI 回归继续使用本机模拟板端，不访问实验设备。

新增状态核心、日志、只读工具及测试另使用严格 C++11 编译和 ASan/UBSan 插桩，开启 LeakSanitizer；状态、日志和跨进程查看测试均通过，没有报告错误。工件在 `/tmp/getdaq-ecc-journal-asan`。yaml-cpp 静态库未插桩；实际 Ice 联调本轮使用普通构建，不把该结果扩大为全部第三方或全部硬件层的内存审计。`ldd getdaq-ecc-journal` 仅显示系统运行库，无 Qt/Ice/YAML 动态库。

这次没有修改 Qt 控制台或原生接收器源码。真实硬件恢复、完整 ECC 网络服务、日志受控归档/轮转及目标平台/掉电验收仍待完成。详见 [ECC 日志手册](ECC_JOURNAL.md)。

## 8. AGET 实验参数计算与离线编译

2026-10-04，Ubuntu 20.04.6、GCC 9.4、Qt 5.12.8；没有连接真实 CoBo/AsAd/AGET。新的 `common/experiment` 独立 CMake 采用标准 C++11，CTest **1/1 通过**。Qt 四程序重新构建成功；QtTest 完整回归 **35 passed、0 failed**，日志在 `build-suite-tests/results-aget-full.txt`。新编译用例的单独记录为 `results-aget-targeted.txt`，含初始化/清理 5 passed、0 failed。

纯核心测试对照原始映射，验证 68 个通道逐个位序和 FPN 排除规则、全部 32 个型号相关峰值时间档、每个全局布尔字段及枚举编码、最高 64 位值、13 个寄存器的顺序/有效位数、读出掩码边界与通道顺序无关性。另覆盖阈值/增益/编号越界、重复/缺少通道、错误型号、2p/偏移/深度冲突、非法字符串及旧别名。

Qt/CLI 使用收到的 `configure-pedestals.xcfg` 和 `configure-ExternalTrigger-2CoBo_1Mutant.xcfg`，检查示例全局/通道寄存器的固定参考值、继承来源、显式覆盖、真实文件 include、完整 SHA-256 来源、新文件保护及失败时不生成工件。`tests/aget_compiler_integration.py` 跨进程通过；包含非活动板/芯片、型号表不匹配、额外/重复通道、unit/type/range/class 拒绝和可审计代码缺省。测试中曾错误假定所有 AsAd 活动，已按原配置只开启 AsAd 0 的实际声明修正测试；没有修改源配置或跳过活动检查。

新增纯核心、Qt 文档/编译层和 CLI 另按 `-std=c++11 -pedantic-errors` 构建 ASan/UBSan 版本，启用 LeakSanitizer。纯核心及上述完整 CLI 测试均通过，无 ASan/UBSan/泄漏错误；工件在 `/tmp/getdaq-aget-asan`。系统 Qt 库未插桩，不能把结果扩大为 Qt 的完整审计。普通 qmake/核心构建无新增警告；直接编译检查的系统 Qt 5.12 头文件存在 GCC deprecated-copy 提示。

`ldd build-experiment/aget-configuration-tests` 只有系统 C/C++ 运行库，没有 Qt/Ice/YAML；离线 `getdaq-config` 仅直接使用 Qt Core，由本机 Qt 安装引入其系统依赖。原生接收器、Ice 硬件层与 ECC 状态/日志实现本阶段没有改动，没有重复其已有验证。

这一阶段当时只计算单片芯片的寄存器和读出掩码，没有整板一致性检查；下一节已扩展到整板编译和原生配置子序列。JSON 部署入口、实机 reg5 核对、完整 Configure、完整共享类及跨默认声明属性语义仍未完成。手册和依据见 [AGET 配置](AGET_CONFIGURATION.md)。

## 9. 整板 AGET 编译、原生写入和回归

2026-10-05，在上述本机环境完成 `getdaq-experiment-core`、Qt 四程序、Ice 兼容工程和测试的重新构建。新增计算、Qt 适配及硬件子序列保持 C++11；没有真实 DAQ 连接，也没有新增第三方安装依赖。

最终结果：

| 检查 | 实际结果与证据 |
| --- | --- |
| 独立实验计算核心 | 1/1 CTest 通过，包含单片映射及整板用例 |
| Ice 工程完整原生回归 | 5/5 CTest 通过，36.70 秒；`build-ice-compat/results-cobo-aget.txt` |
| Qt 完整回归 | 37 passed、0 failed、0 skipped，约 19.6 秒；`build-suite-tests/results-cobo-aget-full-final.txt`，包含初始化/清理 |
| 离线编译 CLI | `tests/aget_compiler_integration.py` 单片及整板全部通过 |
| 既有硬件 CLI | hardware_cli、calibration、description 跨进程回归全部通过 |
| ASan/UBSan/LeakSanitizer | 新计算核心、Qt 编译适配/CLI、硬件层/维护子序列及测试对象插桩后通过；详见下面的范围限制 |

整板纯计算用例覆盖全部 16 片及稀疏活动范围、排序、混合型号、FPN 逻辑或、活动 AsAd 掩码、重复/非法芯片、无活动芯片及共用 offset/depth/2p 冲突。Qt/CLI 使用收到的原始 pedestal 配置检查全部活动芯片、继承来源、混合 ASTRE 显式参数及共用偏移冲突；版本表缺项、额外项、重复项、非规范编号或未来型号均拒绝，失败不会生成新工件。

原生 `hardware-client-tcp` 使用实际 GET Node/Device 接口和本机 TCP，分别运行 Ice 编码 1.0/1.1。三个活动芯片编号为 0、7、15，其中包含 ASTRE；正常流程写 39 个 AGET 配置寄存器，验证高 64 位值、每次写入及读回的芯片选择、两组掩码银行、选择范围恢复和共用参数。所有芯片 reg5 校验均发生在第一个 AGET 配置寄存器写入之前。

故障用例覆盖最后一片型号错误/版本读取异常时零 AGET 配置写入、初始 firmwareEnable 不符时零写入、寄存器/掩码读回不符后停止、默认严格的 reg2 bit21 差异、显式兼容时 `matched_with_warning`、bit21 加其他差异仍拒绝，以及 reg6 已应用后断线只发送一次并保留 unknown。计划最后的 readOffset 元数据缺失、超出位宽的允许差异策略、在 Delay 上设置允许差异均在任何寄存器操作前拒绝。

首次 Qt 全回归为 36 passed/1 failed（`results-cobo-aget-full.txt`）：独立模拟器用例在探测后释放基准端口，未检查相邻 MuTaNT 端口，存在端口竞争。修正为由实际 ECC/sentry 服务各自绑定端口 0，再写入实际监听端口；显式端口的旧用法保持不变。完整模拟运行/归档回归随后通过，最终结果以 `results-cobo-aget-full-final.txt` 为准。新生成配置仍遵循已有文件不覆盖规则，使用自动端口重启时要换根目录或更新旧配置。

另以 `-std=c++11 -pedantic-errors` 和 ASan/UBSan 构建纯核心、Qt 文档/编译/CLI，以及新硬件层/维护序列/测试对象，开启 LeakSanitizer。纯计算、单片/整板 CLI 和两种编码的完整硬件 TCP 测试均通过，无报告错误。纯核心工件在 `/tmp/getdaq-aget-asan`，Qt/硬件工件和结果在 `/tmp/getdaq-cobo-aget-asan`。系统 Qt、历史 Ice、yaml-cpp 及已链接的 ECC 静态库本轮未插桩，不把结果扩大为它们的完整内存审计；ECC 的独立插桩记录见第 7 节。普通构建无新增警告，直接编译使用的 Qt 5.12 头文件仍有 deprecated-copy 提示。

`ldd build-ice-compat/getdaq-hardware` 仍仅显示系统运行库，没有 Qt 动态依赖。原生数据接收代码本轮未修改，没有重复其既有吞吐/归档验证。当前仍只有类型化库执行入口，JSON 工件没有硬件部署入口；上述回环验证不代表完整 ECC Configure、实机慢控制可靠性、校准效果或长时间高速采集。操作与原理见 [AGET 配置](AGET_CONFIGURATION.md)。

## 10. CoBo 触发、零抑制、缓冲区及组合配置

同一实际开发环境下，新增纯 C++11 `coboreadoutconfiguration` 和原生 `CoBoReadoutConfigure`，并加入离线 `compile-cobo-readout`。Qt 四程序和可选 Ice 工程全部构建成功；原生接收器/导出器代码本阶段未修改，也没有增加 Qt 或第三方依赖。

| 检查 | 实际结果 |
| --- | --- |
| 独立实验计算核心 | 2/2 CTest 通过，新增 cobo-readout |
| Ice 工程整套原生回归 | 6/6 CTest 通过，107.21 秒；`build-ice-compat/results-readout.txt` |
| 最终 Qt 完整回归 | 39 passed、0 failed、0 skipped，约 19.8 秒；`build-suite-tests/results-readout-hex-final.txt`，含初始化/清理 |
| 离线 CoBo/AGET CLI | cobo_readout_integration 和 aget_compiler_integration 全部通过 |
| 严格标准及内存检查 | 新计算/Qt 编译/原生硬件层和测试对象以 C++11 pedantic 构建；ASan/UBSan/LeakSanitizer 均通过 |

纯核心用例覆盖全部 15 个活动 AsAd 掩码的自动缓冲区分配（含三板按四份分配）、显式/共享布局及边界/重叠/部分范围拒绝；25 种主次触发组合与其 2p 主触发形式、noTrigger、旧别名/缺省和非法模式；位置 multiplicity 一致性、全板 full/partial 冲突、全部 68 个零抑制阈值及数值/乘法边界。未知类型化参数、重复/遗漏/不活动读出芯片也会拒绝。

Qt/跨进程 CLI 用两份收到的实际 GET 配置检查固定寄存器值和来源摘要。pedestal 的实际声明是 full readout，AsAd0 明确占 0x08000000..0x0A000000，并不是自动占用整个全局范围；新测试按这些源声明核对。验证新文件保护、未使用旧别名不求值、显式 hex 无前缀文本按十六进制处理并保存 compiled_value、继承来源、FPN/不活动通道零抑制阈值、缓冲区/触发/数值/共享类拒绝和失败不写工件。原始配置字节未修改。

原生 TCP 用 Ice 1.0/1.1、AsAd mask=11（0/1/3），每轮正常配置写 39 个 AGET 寄存器及 204 个零抑制阈值。核对触发/帧头字段、32 位最高 triggerPeriod、缺省位置屏蔽、FPN/不活动通道阈值及 per-asad/shared 两种布局；三活动板的最后一块自动范围仍符合旧四分之一规则。对 16 片全活动计划另验证不超过 4096 步。

故障注入包括零抑制读回不符时停止、通道阈值已生效后断线仅写一次且 unknown、最后一片版本不符时零 AGET/零抑制配置写入、后段 Module 读回不符时不执行后续步骤，以及末尾 aEmptyLevel 元数据缺失时整个组合计划零寄存器操作。这些只连接本机 GET 模拟对象，不证明真实固件行为。

新增纯核心、Qt 文档/编译适配/CLI、硬件访问层/维护序列和硬件测试对象另用 `-std=c++11 -pedantic-errors`、ASan/UBSan 插桩，开启 LeakSanitizer。纯核心、两种编码的完整硬件测试、最终两项 CLI 集成全部通过，没有错误报告。纯核心工件为 `/tmp/getdaq-readout-core-asan`；Qt/硬件工件及结果在 `/tmp/getdaq-readout-asan`（最终 Qt 为 getdaq-config-hex，硬件结果为 results-hardware.txt）。系统 Qt、历史 Ice、yaml-cpp 和本轮链接的 ECC 静态库没有插桩，不能把结果扩大为它们的完整审计。普通构建无新增警告；直接编译时仍有系统 Qt 5.12 头文件的 deprecated-copy 提示。

首次手工硬件插桩构建漏传本地 yaml-cpp 头文件路径，未构建成功；补齐本工程提供的 include 后成功并完成上述测试，没有安装外部 YAML 库。早期新增 CLI 测试曾误设 pedestal 的读出模式/AsAd0 范围预期，已根据原始源声明修正，没有改动源配置或放宽编译检查。

复现命令：

```bash
(cd build-experiment && ctest --output-on-failure)
(cd build-ice-compat && ctest --output-on-failure)
python3 tests/cobo_readout_integration.py build-suite/bin/getdaq-config
python3 tests/aget_compiler_integration.py build-suite/bin/getdaq-config
```

`ldd build-ice-compat/getdaq-hardware` 仍只显示系统运行库、无 Qt。该阶段当时尚缺 AsAd TCM/inspection/pulser 与 2p 初始化，下一节已补入；完整生产 ECC Configure、采集/读取停止、控制权联锁和 JSON 部署入口仍未接入。测试时长和回环成功不能当作数据接收吞吐基准、真机验收或目标平台完成。操作、寄存器及兼容差异见 [CoBo 读出配置](COBO_READOUT_CONFIGURATION.md)。

## 11. AsAd 参数、脉冲管理器与 CoBo Configure 组合

同一实际开发环境下完成 C++11 AsAd TCM/ISPM、电压/DAC、pulser 参数以及离线 `compile-cobo-configure`。原生 `CoBoConfigure` 组合 AGET/AsAd/Module 和 20ms 2p 脉冲，Configure 停止旧周期计时器、保存新策略，不启动周期 pulser 或采集。未增加外部安装依赖，未连接真实 DAQ。

| 检查 | 实际结果/证据 |
| --- | --- |
| 当时的独立计算核心 | 3/3 CTest 通过，新增 cobo-asad；随后 Prepare/clock 已扩展到五项 |
| 最终慢控制时序原生回归 | 7/7 通过，219.58 秒；build-ice-compat/results-configure-slow-final.txt |
| Qt 完整回归 | 41 passed、0 failed、0 skipped，约 20.9 秒；build-suite-tests/results-configure-full.txt，含初始化/清理 |
| 原配置跨进程编译 | cobo_configure_integration、cobo_readout_integration、aget_compiler_integration 全部通过 |
| 内存及未定义行为 | 新核心、Qt 编译适配/CLI、原生硬件/测试对象严格 C++11 插桩后通过；Prepare 阶段再次覆盖最终慢控制时序，范围见下一节 |

核心用例覆盖全部活动 AsAd 掩码、16 种 inspection 设备及 ISP1/ISP2 编码（包括 Multiplicity 特例）、14 位 DAC 的三个固定参考值和全部整数电压的单调性、有限实数/mV 向零截断、周期/外触发/双脉冲/Poisson 策略、TCM 电容/延迟与跨层掩码。非法单位、NaN、无穷、1000.1mV、缺失/重复/不活动板、未知键及信号均拒绝。

Qt/CLI 使用收到的 pedestal 与 ExternalTrigger 配置验证参数来源、源 SHA-256、真实类型/单位、fractional voltage、全局与旧 AsAd0/Amplitude[0] 优先关系、继承、ISPM 与 TCM 参考值、新文件保护和失败不生成工件。原配置字节不修改。新 CLI 的插桩集成测试也通过，系统 Qt 没有插桩。

原生 GET TCP 分别使用 Ice 1.0/1.1、AsAd mask=11，验证 TCM/DAC/ISPM 板选择和读回、外触发及周期策略、初始化脉冲与 mask 恢复。直接对照旧 impl_configure 后保留每块 AsAd 设置 TCM 前的三脉冲慢控制切换及 1000ms 等待；用操作顺序确认没有省略。对全 16 片计划核对不超过 4096 步。

故障注入覆盖默认 DAC 已生效后断线仅发送一次且 unknown、DAC 读回错误后不再设置后续板、缺少较新管理器方法时停止、最后芯片型号错误时不写 AGET 配置/零抑制/AsAd 电压。前期停止计时器及清除触发等控制动作可能已经执行，不能把“未写配置寄存器”说成“没有硬件影响”。

预检查分别拒绝未绑定描述、缺失 AsAdPulserMgr 接口、末尾 ISPM 元数据不符及管理器间接访问 TCM1 的本地只读权限；均无寄存器或管理器动作。显式 StartPeriodic/StopPeriodic 原语也只在本机模拟中验证，Configure 没有调用 StartPeriodic。接口 checkedCast 不能提前证明每个具体方法均存在；OperationNotExist 作为未确认动作停止并留日志。

JSON 没有部署入口，完整 Prepare/生产状态与恢复/所有权/服务仍待接入。详细电压、信号与失败语义见 [AsAd 配置](ASAD_CONFIGURATION.md)。

## 12. Prepare 基础配置、输入保护/槽位、ADC 与 DAC

新增 coboprepareconfiguration 与原生 coboprepare，复用标准库/已有本地兼容层。独立核心此时 4/4 通过；下一节时钟加入后为 5/5。Qt 四程序重新构建成功，无新增警告；原生库/工具构建成功。最终带 clock 的原生完整结果为 **9/9 CTest 通过，235.94 秒**，build-ice-compat/results-prepare-clock-final.txt。

核心验证 13 种 LEMO 功能、全部触发/读出默认值、四块初始 32MiB 半开范围和显式共享布局、可选旧寄存器与无版本 R-CoBo 策略。输入槽位覆盖全部 15 个活动板掩码，以及每个物理位置全部 active/enabled/ZAP/protection 组合，核对 ADC 关闭位与 IM 开启位的极性和 AGET 逻辑活动范围；重复/缺失/非法板或布局拒绝。

两种 Ice 编码的本机 TCP 用例确认型号错误不会进入流水线/LEMO 初始化、基础触发和缓冲区初值、显式旧布局可在没有新寄存器的模拟板上完成；旧布局通过独立客户端读取并绑定，保持已有描述不匹配时拒绝的约束。末尾元数据缺失时不执行前面的复位。

输入保护/槽位用 mask=11 检查 ADC0/IM0/IM1 的固定值、关闭位置、抗放电/ZAP 极性、不活动板不写及结束恢复选择范围。ADC 同时验证 DDR/SDR、自清零复位、deskew 与 bit-slip 顺序、两次 1000ms 等待和最后的普通数据模式。模拟 ADC 读取直接报“只写”，正常序列仍完成，避免用虚假的寄存器回显替代真实协议。DAC 测试检查 0x81、DAC6=0、始终不写只读 DAC2；手动延迟检查 255 边界及 256 拒绝。

ADC 写入生效后丢失应答只发送一次，日志 unknown、后续校准脉冲/DAC 不执行；末尾 DAC7 元数据不符和 ADC0 本地只读在整份预检查中拒绝。firmwareEnable=1 时也不会复位 ADC。命令完成不等于电气校准成功，也不提交 Prepared。

严格 `-std=c++11 -pedantic-errors` 构建新核心、Qt 编译适配和原生硬件/维护子流程/测试对象，ASan/UBSan 并开启 LeakSanitizer。Prepare 与 AsAd 核心、含最终慢控制顺序的完整原生 TCP 测试全部通过，没有错误报告；工件在 /tmp/getdaq-asad-asan，原生记录 results-prepare-native-final.txt。系统 Qt、原先 Ice/bzip/yaml 与所链接 ECC 静态库未插桩，不扩大为这些依赖的完整审计。

普通完整回归与插桩回归并发时曾触及原来的 300 秒总测试上限；保留所有硬件等待和客户端超时，仅将完整测试的 CTest 上限调整为 600 秒后单独重跑，得到上述 235.94 秒通过结果。测试逐步 fsync，耗时不表示原生接收器吞吐；数据接收代码没有修改，也未重复其旧压力测试。

旧 Prepare 的 ADC 复位会清除先前的 ADC 槽位设置，文档已明确说明，未偷偷改变完整顺序。时钟部署、上电监控、AGET/自动延迟校准、告警及生产协调器仍待接入。操作及原理见 [Prepare](COBO_PREPARE.md)。

## 13. PLL/LCM 纯计算及原始源码对照

新增纯 C++11 coboclockconfiguration，无 Qt/Ice/YAML/Boost。最终独立核心 **5/5 通过**，build-experiment/results-clock-core-final.txt；Qt 四程序重新构建通过，原生工程也构建成功。最终增加频率偏差/范围标志后，原生工程的 cobo-clock 单独重建并 **1/1 通过**，results-clock-targeted-final.txt，不重复未改变的硬件 TCP 流程。

固定参考值和边界测试覆盖六种标准 LCM1/R-CoBo 分频、两种 1MHz 特例、通用搜索、全部专家字段的每个可表示值与反位序、25/12.5/6.25 的 LCM2 编码、四种相位和 -5335..5335 每个整数延迟。验证未知输出源不会猜测频率，以及旧通用搜索的 N 钳制可能产生低于目标/AGET 范围的名义频率；新工件明确给出 false 标志。

PLL 测试核对默认 CKR 包、R3/R4/R5 整数分频及地址、高 32 位覆盖、R0..16/R24..31 索引映射、R13.EN_CLKin1 导致的第二次 R11/SYNC_POL_INV 清除、非法地址/范围拒绝。123MHz 请求的名义默认输出为 125MHz，没有将请求值伪装为实际频率。

独立 `tests/cobo_clock_original_reference.py` 提取收到的 CEA GET LCM1/LCM2 原函数、PLL 默认数组及 setCKWFrequency/setRegisters/loadPLLConfig，用标准工具外围运行，并直接比较新算法。**4743 个 LCM1、128052 个 LCM2、829 个 PLL 用例全部一致**。没有调用新实现作为旧参考、修改原始源码或安装 CCfg/Boost。复现：

    python3 tests/cobo_clock_original_reference.py

新 clock 核心和测试另以严格 C++11/ASan/UBSan 插桩，启用 LeakSanitizer，通过且无报告；工件 /tmp/getdaq-asad-asan/cobo-clock-tests。旧参考程序为普通严格 C++11 编译，不把它的源码对照说成旧代码完整内存审计。ldd cobo-clock-tests 只有系统运行库。

本纯计算阶段当时还没有 xcfg 适配、硬件部署和真实锁定/相位核对；后续整板编译与命令子流程见第 14 节。计算与旧算法一致不证明任意目标可达。上下游 CKR、专家参数及名义范围必须在完整编译/部署中验证，不能据此提交 Prepared。详见 [时钟计算](COBO_CLOCK_CONFIGURATION.md)。

## 14. 整板时钟编译、来源适配与原生 ClockInit

在同一实际开发环境下新增 CoBoClockSettings/CompileCoBoClock、ClockCompiler、compile-cobo-clock 和原生 CoBoClockInit。Qt 的标量/单位/范围/来源检查抽成共用 ExperimentReader，既有 AGET/读出/AsAd 编译复用它；未引入运行依赖，没有修改原生数据进程或把数据送进 Qt。

| 检查 | 实际结果 |
| --- | --- |
| 独立核心完整 CTest | 5/5，通过；build-experiment/results-clock-whole-core.txt |
| 可选 Ice/原生完整 CTest | 10/10，通过，239.87 秒；build-ice-compat/results-clock-deployment-final.txt |
| 其中新增硬件时钟 TCP | 通过 Ice 1.0/1.1，7.63 秒；测试是控制命令模拟，不是 DAQ 吞吐测试 |
| Qt 完整回归 | 43 passed、0 failed、0 skipped，21.021 秒；build-suite-tests/results-clock-full.txt，含初始化/清理 |
| Qt 四程序、原生库/工具 | 重新构建通过 |
| 离线 CLI | AGET、CoBo 读出、CoBo Configure 三项原有集成及新时钟集成均通过 |
| 时钟原始算法对照 | 4743 LCM1、128052 LCM2、829 PLL 及 175 命名 PLL 原解析器用例通过 |
| 严格 C++11 和内存检查 | 新核心、配置适配/CLI、原生执行器/ClockInit/描述与 TCP 测试的 ASan/UBSan/LeakSanitizer 通过，无报告 |

新增核心覆盖全部 15 个活动 AsAd 掩码、R/M-CoBo 六档频率、18 个命名 PLL 参数的宽度/布尔拒绝、R11 双同步包，以及硬件版本/覆盖/重复 ID/晚期参数错误。名义模型核对 123→125MHz、R-CoBo CKR12.5 请求而上游仍25、skipPLL/专家 MODE 和未知 LCM 输出，区分已知偏差与未知；十进制实数先在 double 范围检查后缩窄，避免范围外值舍入后被接受。

新 CLI 集成使用两份收到的原配置和无 AGET 的最小 include 配置，验证 SHA-256/来源行/默认值、64 位字符串、完整专家模式、停用字段不读取、标准 PLL 拼写优先与旧拼写回退、闲置 R14 不擅用、类型/单位/范围拒绝、非法板型和失败不产生工件/不覆盖。QTest 另验证失败不会改调用者原有 JSON 或 typed settings。

原生 TCP 模拟 LCM/FIFO 的普通读操作直接报错，因此正常流程确实没有隐式命令读回。Full=1 仍只读观察。晚期 LCM2 缺失/只读、LCM32位宽度不足、FIFO只读和参数错误都不会让前段 FIFO 提前写入；未绑定时连远端元数据也不访问。运行中控制位/实际板型不符时在首个时钟写入前停止。第三个 FIFO 包生效后断线只发送一次，LCM2 生效后异常不重发、不补写恢复掩码，审计保存 UNKNOWN。

计划新增 requireBoundDescription 和有界 ASCII annotation，仅用于执行前提/intent，不是新硬件操作。后段非法注释在元数据访问前拒绝；4096 个零延迟步骤也受 4MiB 总体限制，不能靠 Delay 分支绕过。已有大量 Prepare/Configure 子流程在完整原生回归中再次通过。

复现主要新增检查：

    (cd build-experiment && ctest --output-on-failure)
    (cd build-ice-compat && ctest --output-on-failure)
    QT_QPA_PLATFORM=offscreen build-suite-tests/getdaq-tests
    python3 tests/cobo_clock_integration.py build-suite/bin/getdaq-config
    python3 tests/cobo_clock_original_reference.py

插桩工件位于 /tmp/getdaq-clock-deployment-asan/，包括 cobo-clock-tests、getdaq-config、hardware-clock-tests 及 results-native-clock.txt。使用 -std=c++11 -pedantic-errors -fsanitize=address,undefined 和 detect_leaks=1；系统 Qt/Ice/GET/bzip/yaml 静态库未整体插桩，不能宣称这些库已全部内存审计。检查中发现测试使用了 C++14 才允许的 lambda 默认参数，已改为普通 C++11 辅助函数并重新通过严格构建/核心测试。

ldd 的核心和原生时钟测试仅包含系统运行库，无 Qt 动态依赖；Ice 等兼容代码是本地静态链接。原码对照独立编译现在链接共用整数解析所需的现有核心源文件，并提取实际 CoBoConfigParser::getPllRegValue，使用标准 CConfig 存根读取收到的默认 XML，逐个改变命名字段的全部可表示值，与新的整板 PLL 包比较（175 例）。参考位位置来自原函数，默认值来自原 XML，均不由新表代替；仍不调用新函数充当旧参考、不安装 CCfg/Boost、不改收到的原始源码。原函数实际使用标准 CLKin_SELECT_MODE 拼写，已核对并在文档中区分执行代码与注释。

**限制：** ClockInit 的完成只表示已列命令应答/期望字段一致，尚无真实 PLL 锁定、频率/相位、电气校准、完整 Prepare/生产 ECC 提交或 JSON 硬件部署入口。平台与吞吐验收仍见第33节。详细操作、参数、模型与失败处理见 [时钟说明](COBO_CLOCK_CONFIGURATION.md)。

## 15. AsAd 上电、监控、保护与有界执行

本轮新增纯 C++11 cobomonitoringconfiguration、Qt Core MonitoringCompiler、compile-cobo-monitoring 及原生上电/监控/保护/一次观察子流程。没有新增运行依赖或修改数据接收代码，Qt 没有进入原始事件传输路径。

| 检查 | 最终实际结果 |
| --- | --- |
| 独立纯核心完整 CTest | 6/6，通过，0.02 秒；build-experiment/results-monitoring-core-final.txt |
| 可选 Ice/原生完整 CTest | 12/12，通过，270.28 秒；build-ice-compat/results-monitoring-native-final.txt |
| 其中监控 TCP | Ice 1.0/1.1 均通过，16.33 秒；这是控制模拟测试，不是 DAQ 吞吐指标 |
| Qt 完整回归 | 46 passed、0 failed、0 skipped，21.117 秒；build-suite-tests/results-monitoring-full-final.txt，含初始化/清理 |
| Qt 四程序及原生库/工具 | 最终重建通过；新增源码无编译警告 |
| 新监控 CLI | 26 组合同检查通过；build-suite-tests/results-monitoring-cli.txt |
| 既有 CLI 集成 | AGET、CoBo 读出、Configure、时钟均再次通过 |
| 原始监控函数对照 | 160 模式、64 保护掩码、2258 阈值、256 原始测量用例通过；build-experiment/results-monitoring-original-reference.txt |
| 严格 C++11/内存检查 | 新核心、配置/CLI、原生硬件源码及监控 TCP 的 ASan/UBSan/LeakSanitizer 均通过，无运行时报告 |

纯核心检查全部 15 个活动 AsAd 掩码和完整覆盖/重复 ID 拒绝，监控 item 的五种合法码、两种实参映射和全部模式布尔组合、32 种保护关闭位组合、全部 256 个测量/有符号温度码、VDD 稳定码的每个精确及相邻浮点边界。固定参照 VDD3.0..3.5→204..238、IDD0..2→0..169；VDD3.0 稳定最低码是205，而不是保护下限204。拒绝非有限、负值、限值顺序错误、8位溢出、温度范围错误和超额等待。映射枚举采用明确的 unsigned 基类型，使非法数值能在校验中拒绝，避免依赖未指定枚举取值范围。

原码对照 `tests/cobo_monitoring_original_reference.py` 在临时目录提取收到的 CoBoNode 监控模式、实际配置调用、保护掩码、阈值及测量函数，以及 CoBoConfigParser 的 item 函数。用标准测试外围替代 CCfg/设备/日志，没有修改原文件或安装 CCfg/Boost。legacy 与实际旧调用比较，declared 与按形参含义传值比较；没有把修正行为说成所有旧调用一致。阈值只比较可表示的合法值，不运行旧越界浮点到 uint8 转换作参考。温度返回与旧代码的一致性限于本机编译器，新计算使用明确的有符号解码。

CLI 用两份原配置和无 AGET 的最小 include 配置，检查源文件 SHA-256/行号、默认值、负温度、模式差异、现代/旧式 VDD 字段和旧等待的独立3.2V默认、停用告警字段不读取。单位/类型/声明范围、部分字段对、共享标量类、非法实例/模式、无活动板及失败不产生工件/不覆盖均有检查。两份原始配置哈希保持不变；Qt 测试另核对失败不改已有 JSON/typed settings。

原生 TCP 模拟器拒绝命令寄存器回显；复位自清零仍能正常初始化。稀疏活动板、已连接非活动板断电、未连接板条件跳过、片段索引平移、两种模式实际写入、后段保护和原始有符号样本均验证。缺绑定零元数据访问；后段 VDD 缺失、阈值只读或被跳过分支的目标非法均在首个上电动作前拒绝。非法条件引用、轮询参数/读取总额/等待总额不生成可执行意图或发送写入。上电前检查明确放宽时保留 warning，上电后告警仍失败；后面板连接失败可能留下前面板已上电，测试确认不回滚/继续/补写选择。

上电生效后断线只执行一次、记录 UNKNOWN 并停止后续板。原始零码重读、非零低电压的外层次数、全部低值耗尽及读异常立刻停止均经过 TCP；读取失败不重发之前写命令。观察读到零只记录零，不当作已达到非零稳定电压。轮询逐次 poll_begin/observed 记录，条件跳过写 skipped；共享执行器的既有 Prepare/Configure/ClockInit 在完整回归中通过。

复现：

```bash
(cd build-experiment && ctest --output-on-failure)
(cd build-ice-compat && ctest --output-on-failure)
QT_QPA_PLATFORM=offscreen build-suite-tests/getdaq-tests
python3 tests/cobo_monitoring_integration.py build-suite/bin/getdaq-config
python3 tests/cobo_monitoring_original_reference.py
```

插桩工件在 /tmp/getdaq-monitoring-asan/：cobo-monitoring-tests、getdaq-config、hardware-monitoring-tests 及 results-native-monitoring-final.txt。以 -std=c++11 -pedantic-errors -fsanitize=address,undefined 编译，detect_leaks=1、halt_on_error=1；本轮独立实验核心源文件、配置适配源文件和原生硬件库源文件整体插桩。系统 Qt、旧 Ice/GET/bzip 和本地 yaml 静态库未整体插桩；编译时系统 Qt5.12 的头文件有既有 deprecated-copy 警告，不是新增源文件警告。原函数对照为独立普通严格 C++11 编译，不等于旧代码整体内存审计。

ldd 的监控核心和原生 TCP 测试仅包含系统运行库，无 Qt 动态依赖；兼容接口静态链接。最后本轮检查本地 Markdown 链接、依赖 JSON 以及 GET 许可证副本保持一致。数据接收器没有修改，没有借此重复旧压力测试或宣称持续采集性能。

**本阶段限制：** 当时 LED 管理 RPC、主机持续告警订阅、AGET/自动延迟校准、MuTaNT、完整生产 Prepare/ECC 和 JSON 部署入口仍待接入；后续AGET/延迟子流程的落地见第16节。模拟电源/保护状态不证明真实电源时序、保护切断、电气/温度解释或平台验收；详见 [上电监控手册](ASAD_MONITORING.md)。

## 16. AGET 串行初始化、阈值和自动读取延迟

新增纯C++11校准策略核心、CalibrationCompiler、compile-cobo-calibration，以及原生CoBoAgetCalibrate/Init、CoBoAgetGlobalThreshold、CoBoAutomaticReadDelayCalibrate和CoBoSelectReadDataDelay。没有新增运行库或修改数据接收热路径，公共硬件接口仍不暴露Qt/Ice类型。

| 检查 | 最终实际结果 |
| --- | --- |
| 独立纯核心完整CTest | 7/7通过，0.02秒；build-experiment/results-calibration-core-final.txt |
| 可选Ice/原生完整CTest | 14/14通过，275.74秒；build-ice-compat/results-calibration-native-final.txt |
| 新增校准TCP | Ice1.0/1.1均通过，18.79秒；控制命令测试，不是事件吞吐指标 |
| Qt完整回归 | 49 passed、0 failed、0 skipped，21.375秒；build-suite-tests/results-calibration-full-final.txt，含初始化/清理 |
| Qt四程序和原生库/工具 | 最终重建通过，新增源码无构建警告 |
| 新校准CLI | 33组合同检查通过；build-suite-tests/results-calibration-cli.txt |
| 既有CLI | AGET、读出、Configure、时钟及上电监控集成再次通过 |
| 提取旧函数体对照 | 36串行和20自动延迟动作/读取观察/等待顺序一致；build-experiment/results-calibration-original-reference.txt |
| 严格C++11/内存检查 | 纯核心、全部配置/CLI源文件、全部原生硬件源文件和新增TCP夹具的ASan/UBSan/LeakSanitizer构建/运行通过，无运行时报告 |

纯核心覆盖15种活动板掩码、单板全部活动芯片子集/请求校准片组合、16种最终延迟策略、空芯片板与缺失/重复/非活动覆盖、未知型号、旧寄存器缺失的显式限制。0表示自动找第一片活动芯片，非零请求必须活动；共同字段的混合策略直接拒绝，不以最后一次写入覆盖前板策略。

CLI使用两份收到的原配置及无Global/68通道参数的最小include配置，验证来源/行号/SHA-256、代码默认、板级优先于Module、计算开关与使用策略独立、显式校准片、空芯片板null输出、单位/范围/共享标量类/非法ID拒绝、失败不产生工件和输出不覆盖。原配置哈希保持不变；新增3个Qt测试另检查JSON/typed settings失败保持不变及旧寄存器显式选择。

TCP夹具模拟稀疏活动板0/1/3和芯片2/5/15，ADC0软件复位自清零、命令不允许普通回显、AGET广播写与读取选择器分离。串行校准确实写两次64位0xa000000000000005，执行三次reg5观察和校准后逐活动片低16位预期型号检查；已知但错误型号同样停止，不继续SCA初始化。阈值测试设置第22位极性为1并逐芯片保存其他reg1位，验证使用3位DACthreshold，不误用4位DAC字段。

自动延迟按旧时序广播reg1/reg2、临时使能固件、两次200ms及显式末尾检查；计算后仍可选择手动。关闭计算不会写校准寄存器；逐板选择使用实际编号而非旧固定0，停用板不写。未绑定零元数据访问，后段初始化命令缺失、hit只读和自动字段缺失在首个复位/寄存器覆盖前拒绝，不进行异常后的布局回退。临时firmwareEnable=1生效后丢失应答只发一次、停止、保留校准/使能实际状态并记录UNKNOWN，没有自动清理写入。旧calibrationAget缺失变体在模拟远端实际移除该寄存器后显式执行，只支持校准片0。

原码对照脚本仅在临时目录提取实际agetCal及无参数calibrateAutoReadDataDelay函数体，使用标准记录设备/等待存根，与原生计划的原动作投影逐项比较。校准片和末尾型号/描述检查在其他测试中验证，参考存根不假装执行完整旧后端，也不把新实现充当旧函数。编译为严格C++11，不安装CCfg/Boost、不修改原文件。

复现：

```bash
(cd build-experiment && ctest --output-on-failure)
(cd build-ice-compat && ctest --output-on-failure)
QT_QPA_PLATFORM=offscreen build-suite-tests/getdaq-tests
python3 tests/cobo_calibration_integration.py build-suite/bin/getdaq-config
python3 tests/cobo_calibration_original_reference.py
```

插桩工件在/tmp/getdaq-calibration-asan/，包括cobo-calibration-tests、getdaq-config、hardware-calibration-tests及results-native-calibration.txt。使用-std=c++11 -pedantic-errors -fsanitize=address,undefined，运行detect_leaks=1、halt_on_error=1。本轮纯核心、配置适配/CLI和原生硬件库源文件整体插桩；系统Qt、旧Ice/GET/bzip及本地yaml静态库未整体插桩，不能宣称全部依赖已内存审计。系统Qt5.12头文件有既有deprecated-copy警告；新源码普通构建无警告。ldd核心和原生TCP测试只有系统运行库，无Qt动态依赖。

**本阶段限制：** 逐板SCR字段只有模拟验证，收到描述仅证明共同字段布局；型号正常不证明电气时序、PLL锁定或ADC最终通道配置。AGET校准会清除ADC设置，自动延迟会覆盖reg1/reg2并临时使能固件，必须按完整Prepare次序、最终Configure前组合。此时LED/持续主机告警仍未迁移，后续落地见第17节；生产ECC/恢复/网络/独占控制、MuTaNT、JSON部署及平台/真机验收仍待接。详细操作与原理见 [AGET 校准](AGET_CALIBRATION.md)。

## 17. LED、板端告警检查器与原生主机回调

新增HardwareAudit共用持久化实现、LED/checker计划、ExpectObservedValue、CoBoAlarmClient和watch-asad-alarms观察命令。公共API只有标准C++11类型，复用现有Ice/GET与标准/POSIX接口，无新增运行库；数据接收热路径本阶段没有修改。

| 检查 | 最终结果与证据 |
| --- | --- |
| 独立纯核心完整CTest | 7/7通过，0.02秒；build-experiment/results-led-alarm-core-final.txt |
| 可选Ice/原生完整CTest | 16/16通过，285.95秒；build-ice-compat/results-led-alarm-native-final.txt |
| 最终出队分配改进后的相关回归 | 2/2通过，21.68秒；build-ice-compat/results-led-alarm-services-final.txt |
| Qt完整回归 | 50 passed、0 failed、0 skipped，21.038秒；build-suite-tests/results-led-alarm-full-final.txt |
| 监控/告警策略CLI | 31组合同通过；build-suite-tests/results-led-alarm-cobo_monitoring-cli.txt |
| 既有配置CLI | AGET、readout、Configure、clock、calibration全部通过；同目录results-led-alarm-*-cli.txt，校准仍33组 |
| 新服务与独立观察CLI | 两种编码通过；tests/hardware_services_test.cpp、tests/hardware_alarm_cli_integration.py |
| 既有硬件CLI | 描述、uint64、读写日志、不覆盖/不重发与流水线顺序通过；build-ice-compat/results-led-alarm-original-cli.txt |
| 普通构建 | Qt四程序、纯核心、原生库/CLI成功，无新源码警告 |
| 严格C++11/内存检查 | 新核心、全部配置适配/CLI、原生硬件库及观察CLI插桩构建；核心/服务TCP/观察CLI/六组配置CLI通过，无ASan/UBSan/LeakSanitizer报告 |

纯核心新增显式LED策略与板型2/5校验，NoLedManager允许忽略闲置板型。MonitoringCompiler解析Alarms/checkAsAd和Alarms/subscribe，默认true，保存来源；Qt新增策略检查，非法布尔/单位失败保留原JSON和typed settings。监控CLI增加默认、覆盖、非法类型/单位及不生成工件检查，收到的原配置SHA-256保持不变。native_*_available只声明可选原生API存在，JSON自身仍不部署LED或订阅。

服务TCP验证稀疏板0/1/3上电清灯和P灯、故障B慢闪、管理器修改实际连接后仍按原观察失败，以及前置片段加入后两类引用正确平移。缺LED方法停止；P灯动作生效后应答丢失记UNKNOWN，仅调用一次。板端checker的间接initDone本地只读在首个RPC前拒绝，启停与应答丢失均覆盖。显式NoLedManager不调用LED接口。

回调测试保留其他客户端，两份自有订阅ID不同，从未调用reset，仅注销自身。覆盖订阅应答前回调、无回调锁死、事件顺序、有界队列溢出、持久化成功但未能入队的丢失计数、非法节点/零/负位图、事件预算、Drain不清故障标志及注销应答已知Closed但事件记录失败。用RLIMIT_FSIZE和SIGXFSZ设置真实部分文件写失败，随后恢复进程限制；并非只用返回值存根模拟磁盘错误。订阅/注销已应用后断线，即使Slice声明idempotent也各只发送一次，Unknown禁止再次调用；析构不隐式注销。

CLI另起模拟板端和观察进程，实际监听反向TCP。两种编码分别验证安静到期/Closed退出0但健康未知、收到AsAd3告警/落盘/退出3、硬件Stop仍false、其他订阅者保留及非法时长/重复路径/错误身份。Python仅为测试依赖，CMake无Python时不登记该CLI测试。

复现普通检查：

```bash
cmake --build build-ice-compat --parallel 4
(cd build-experiment && ctest --output-on-failure)
(cd build-ice-compat && ctest --output-on-failure)
QT_QPA_PLATFORM=offscreen build-suite-tests/getdaq-tests
python3 tests/cobo_monitoring_integration.py build-suite/bin/getdaq-config
python3 tests/hardware_alarm_cli_integration.py build-ice-compat/getdaq-hardware build-ice-compat/hardware-services-tests
```

插桩工件位于/tmp/getdaq-services-asan/：cobo-monitoring-tests、getdaq-config、hardware-services-tests、getdaq-hardware及results-core/native/observer-cli/各配置-cli.txt。以-std=c++11 -pedantic-errors -fsanitize=address,undefined编译，detect_leaks=1、halt_on_error=1运行。全部新原生硬件库/CLI源文件插桩；系统Qt、旧Ice/GET/bzip及本地yaml静态库未整体插桩，不能据此声称全部依赖已内存审计。ldd纯核心、原生服务测试及getdaq-hardware无Qt动态依赖。

文档/清单检查确认本地Markdown链接、依赖归档SHA-256和许可证有效；getbench COPYING与收到原件逐字节一致。详见 [LED/告警手册](COBO_SERVICES.md)。

**限制：** 模拟GPIO/回调不证明实际灯、现场反向网络或30秒检查器时效；订阅应答/安静无事件不能证明持续健康。新观察器只保持停机请求，不自动控制硬件、关闭数据发送或封存接收器。完整Prepare、MuTaNT、生产ECC告警消费/Stop/排空/恢复、JSON部署及下述目标平台/实机验收仍未完成。

## 18. MuTaNT参数编译及原生配置/计数/时钟子流程

新增纯MutantSettings/CompileMutant、共用ExperimentReader的明确实例及MuTaNT严格hex类型、MutantCompiler/compile-mutant、原生MutantPrepare/Configure/RunInit及WSCA/同步/掩码API。MaskedWriteValue用于普通字段的有审计读改写，保留其他位并读回；计数与同步命令不添加隐式回显。没有新增运行库，也没有修改原始数据接收热路径。

| 检查 | 结果与证据 |
| --- | --- |
| 纯核心完整CTest | 8/8通过，0.03秒；build-experiment/results-mutant-core-final.txt |
| 原生完整CTest | 18/18通过，298.31秒；build-ice-compat/results-mutant-native-final.txt |
| 最终保留帧拒绝/工件字段增量后的原生相关回归 | 2/2通过，26.23秒；build-ice-compat/results-mutant-related-final.txt |
| Qt完整回归 | 52 passed、0 failed、0 skipped，21.031秒；build-suite-tests/results-mutant-full-final.txt |
| 工件字段增量后的Qt相关回归 | 两项MuTaNT用例及init/cleanup共4 passed、0 failed；同目录results-mutant-related-final.txt；随后严格hex修正再通过上述完整Qt回归 |
| MuTaNT跨进程CLI | 49组合同通过；build-suite-tests/results-mutant-mutant_compiler-cli.txt |
| 既有六组配置CLI | AGET、readout、Configure、clock、monitoring、calibration全部通过；同目录results-mutant-*-cli.txt |
| 既有硬件CLI | TCP、64位数、顺序日志、不覆盖/不重发通过；build-ice-compat/results-mutant-original-cli.txt |
| 独立旧源码对照 | 6144个GLOBAL_CONF组合、65536组时间码、64组Configure/RunInit/WSCA/同步写入次序通过；build-experiment/results-mutant-original-reference.txt |
| 普通构建 | Qt四程序、独立纯核心、原生库/CLI/TCP测试构建成功，无新源码警告 |
| 严格C++11/内存检查 | 新核心、全部配置适配/CLI及原生硬件库整体插桩构建；MuTaNT核心/TCP及七组离线CLI通过，无ASan/UBSan/LeakSanitizer报告 |

原生TCP夹具从收到的hardwareDescription_Mutant.xcfg编译出tests/data/mutant-hardware.json，保留5个设备、87个寄存器及源SHA；CLI测试重新编译原描述，逐项比较设备/寄存器/位域，避免由新算法重新编造布局。原始实验、旧单独测试配置和硬件描述的SHA在跨进程回归后不变。

纯计算覆盖主从规则、v1.5 L1/HER实际位、12槽位掩码、48/32位精度、两种时钟名称族、显式旧回退、float时间量化及有限/范围检查。L1B必须窄化前检查，最大167769.6us；收到XML的大上限不能绕过硬件范围。时间655.35us沿用旧float后得到65534码，输出请求与名义值分开。保留帧码3拒绝；该拒绝与clock_path_update字段、CompoundConfig严格hex语义是在首次完整回归后加入，随后重新构建并通过上述相关回归/CLI/插桩，不把先前二进制当作最终验证。

Qt/CLI测试覆盖具体实例和默认块、真实include来源、缺preload/readout开关时的旧默认、显式false、Scalers旧clearCounters与外层clearCounters缺省、从模块外时钟、2p后段超大timeout、关闭L1仍检查L1A、显式hex一致范围及无类型0x前缀/模糊hex拒绝、单位/布尔/非法槽位以及失败无工件和禁止覆盖。JSON仍只作检查资料，不部署硬件。 严格hex依据收到的source/cconfig/CompoundConfig-release-20210208/src/CCfg/Scalar.cpp、Value.cpp及Grammar/ValueGrammar.hpp：getHexValue/getHex要求HEXADECIMAL，自动语法中的无前缀数字是INTEGER，并非默认hex。明确type=hex和无类型0x前缀分别验证；不把解析器方法名称当作自动转换依据。

两种Ice编码覆盖实际9位clock path、两路12位输出及无关位保留、ATO不改变旧SHELF_MASK、完整预载/计数命令顺序、自清零命令禁止读取、read_mode反向极性和2p、WSCA/同步、GPIO/供电/板ID、mrdy失败不执行后段ATO、运行/旧掩码不符零写、缺绑定零元数据、后段描述/只读错误零写、明确旧名称/缺l2ptc。MaskedWrite还覆盖实际写已生效后断线只写一次且UNKNOWN、已应答而读回失败不重发，以及另一个合成64位普通目标的符号位与其他位保留。

独立对照脚本提取MutantNode实际25个函数体及MutantConfigParser的四个量化表达式，标准记录存根只替换外围，不调用新计算生成旧参考，不安装CCfg/Boost/Ice，也不修改收到的原文件。它验证可定义输入的算法和写入顺序；新停止/权限/读回/故障检查另由TCP验证。旧输入窄化溢出及真实对齐/电气行为不由该对照证明。

复现：

```bash
cmake --build build-ice-compat --parallel 4
(cd build-experiment && ctest --output-on-failure)
(cd build-ice-compat && ctest --output-on-failure)
QT_QPA_PLATFORM=offscreen build-suite-tests/getdaq-tests
python3 tests/mutant_compiler_integration.py build-suite/bin/getdaq-config
python3 tests/mutant_original_reference.py
```

严格插桩工件在/tmp/getdaq-mutant-asan/：mutant-configuration-tests、getdaq-config、hardware-mutant-tests及results-core/native/各配置-cli.txt。以-std=c++11 -pedantic-errors -fsanitize=address,undefined编译，detect_leaks=1和halt_on_error=1运行；原函数对照本身不整体插桩。系统Qt、旧Ice/GET/bzip及本地yaml静态库未整体插桩，不能声称所有依赖已经内存审计。核心、原生MuTaNT测试及硬件CLI仍无Qt动态依赖。

**限制：** run=0不证明独占/发送器停止，软件掩码不证明TDC相位，GPIO/电源检查和命令应答不证明实物行为。完整AlignmentServer与MuTaNT/CoBo拓扑、Prepare/同步/Start/Stop和接收器生命周期、生产ECC/恢复/网络/JSON部署及目标平台/真机高速采集仍待接。详细字段、示例、旧语义差异与错误处理见 [MuTaNT手册](MUTANT_CONFIGURATION.md)。

## 19. CoBo/MuTaNT拓扑与系统对齐主机协调

新增纯alignmentconfiguration结果检查、原生SystemAlignmentClient、拓扑读取和独立getdaq-align。迁移旧AmcSlotManager的两路槽位定位/机箱内测量以及SystemManager的机箱间主从三步与最终WSCA；不接收原始数据。HardwareClient::Validate复用Execute完整软件/元数据检查，不读寄存器值或写入；执行仍重新检查。

| 检查 | 结果与证据 |
| --- | --- |
| 纯核心完整CTest | 9/9通过，0.03秒；build-experiment/results-alignment-core-final.txt |
| 原生完整CTest | 21/21通过，322.14秒；build-ice-compat/results-alignment-native-final.txt |
| Qt完整回归 | 52 passed、0 failed、0 skipped，20.956秒；build-suite-tests/results-alignment-full-final.txt |
| 独立对齐CLI | 25组合同通过；build-ice-compat/results-alignment-cli-final.txt；完整CTest亦执行该脚本 |
| 独立旧函数对照 | 提取10个实际旧主机函数，14组单/双/三机箱与活动板组合的TCP命令顺序、AMC定位和保存掩码一致；build-experiment/results-alignment-original-final.txt |
| 文档/依赖/示例 | 214个本地Markdown链接、源码包SHA/大小/许可、两份夹具来源SHA、示例离线校验通过；原生对齐/硬件/核心/接收器均无Qt动态依赖，接收器无Ice/yaml；build-experiment/results-alignment-document-checks.txt |
| 普通构建 | 原生硬件库/两个CLI、Qt四程序及独立纯核心构建通过，无新源码警告 |
| 严格C++11/内存检查 | 新核心、全部配置适配/CLI和原生硬件源以-std=c++11 -pedantic-errors及ASan/UBSan插桩构建；纯对齐、原生TCP和对齐CLI运行通过，detect_leaks=1，无ASan/UBSan/LeakSanitizer报告 |

MuTaNT夹具仍来自收到的原始5设备/87寄存器描述；新CoBo夹具tests/data/cobo-hardware.json由原始hardwareDescription_fullCoBoStandAlone.xcfg经getdaq-config编译，包含11设备/1310寄存器及来源SHA。模拟设备采用实际字段位置/宽度，没有重新猜测firmwareEnable或时钟寄存器布局。原始源码/描述未改动。

纯检查验证全部12个板端索引/AMC编号、10000ps边界、INT_MIN/INT_MAX安全差值、两路键/存在性、重复板/槽位和掩码。新版额外要求存在集合与独立定位和保存掩码一致；只证明报告一致性，不证明真实电气时间。机箱间接口未声明时间单位，保留原始有符号返回值。

真实本机TCP两种Ice编码覆盖三机箱AMC1/12/6、两个slave的master→slave三步、停用CoBo与最终Normal、单机箱跳过multi、机箱间改写实际掩码后保存快照仍不变、WSCA无关位保留。先核对全部描述/接口/名称再读停止与实际主从/时钟前提；后段名称/管理器/只读错误、停用板firmwareEnable非零、实际slave角色错误均零链路/对齐写入。Validate夹具确认仅元数据、零寄存器读写。

故障覆盖实际链路/TDC/multi执行后应答丢失只调用一次、AlignmentAlarm不假设没有生效、非唯一/两路不同位置、非法字典键/超过12项、延迟/输出掩码矛盾和过大注释。所有失败均停止、锁定对象单次使用，不自动Normal或WSCA恢复；析构不恢复。通过RLIMIT_FSIZE在真实ACK记录制造部分写入，已应答操作不重发。完整失败记录和原生子日志保留UNKNOWN及禁止自动补偿。

跨进程CLI验证相对描述路径、严格必填/重复/未知字段与单文档、角色/数量/布尔/编码/超时、缺exclusive-stopped参数不创建目录、不覆盖旧目录、完整策略对象快照与日志可解析，以及三种未知故障的实际调用计数。声明owner/stopped不实现锁或停发送器；当前GUI仍接旧ECC SOAP。

独立脚本直接提取AmcSlotManager.cpp的7个函数体和SystemManager.cpp的3个函数体，标准存根仅替换外围与模拟报告；参考程序不调用新协调/纯检查函数，不安装CCfg/Boost/Ice。新后端通过真实TCP执行，14组动作/位置/保存掩码逐项比较；原始文件SHA前后不变。新停止/权限/故障行为有意不同于旧自动恢复分支，另由TCP与CLI验证。

复现：

```bash
cmake --build build-ice-compat --parallel 4
(cd build-experiment && ctest --output-on-failure)
(cd build-ice-compat && ctest --output-on-failure)
QT_QPA_PLATFORM=offscreen build-suite-tests/getdaq-tests
python3 tests/alignment_cli_integration.py \
  build-ice-compat/getdaq-align build-ice-compat/hardware-alignment-tests
python3 tests/alignment_original_reference.py build-ice-compat/hardware-alignment-tests
```

严格插桩工件/日志在/tmp/getdaq-alignment-asan/，包括alignment-configuration-tests、hardware-alignment-tests、getdaq-align及results-core/native/cli.txt。系统Qt、旧Ice/GET/bzip及本地yaml静态库未整体插桩；旧参考程序本身仅严格C++11构建，不声称所有依赖内存审计。getdaq-align/原生硬件库仍没有Qt动态依赖，也未改变接收器热路径。

**限制：** 只迁移主机协调；收到的板端AlignmentServer仍只有接口，现场TDC算法/XGPIO行为/IceEirfu未重建或验收。完整Prepare/Configure、CoBo PLL之后的系统SyncPulse/启动停止、实际控制权/加载配置绑定、恢复与网络入口、接收器排空/封存和目标平台/高速实机仍待完成。详细操作、配置模板、架构和故障处理见 [系统对齐手册](SYSTEM_ALIGNMENT.md)。

## 20. 单板CoBo Prepare组合与告警联锁

本阶段增加纯核心Prepare片段活动/板型策略一致性、BuildCoBoPreparePlan、CoBoPrepareClient和HardwareClient的可选本地停止检查器。单板前后两段整体元数据检查，实际板型/停止值及AlarmService远端类型检查先于首次写入；最终ADC0在最后ADC复位后重应用，自有订阅先建立再开启板端检查器。没有新增Qt依赖或改变原始接收热路径。

| 证据 | 实际结果与范围 |
| --- | --- |
| 纯C++11核心 | 9/9通过，build-experiment/results-prepare-core-final.txt；Prepare测试加入跨片段活动/版本/手动延迟一致性 |
| 新组合TCP测试 | Ice1.0/1.1通过，143.08秒；build-ice-compat/results-prepare-composed-final.txt |
| 其余原生完整回归 | 21/21通过，361.51秒；build-ice-compat/results-prepare-native-regression.txt；与新组合测试合计覆盖22个注册项，分两组执行 |
| Qt完整回归 | 52 passed/0 failed/0 skipped，20.850秒；build-suite-tests/results-prepare-qt-final.txt |
| 旧函数独立对照 | 实际旧impl_prepare函数体、12组活动掩码/计算/订阅策略阶段比较通过；build-experiment/results-prepare-original-final.txt |

新增tests/hardware_prepare_test.cpp使用从收到源码编译出的真实CoBo硬件描述：11设备/1310寄存器，字段位置/宽度沿用原描述。模拟ADC明确禁止读回，复位只清ADC，不误清独立IM/监控/LCM；检查稀疏和关闭槽位的最终ADC0、DDR/SDR格式、IM策略、不触碰不活动板/只读DAC2、校准覆盖reg1/reg2和计算/选择分离。模拟器不是电气测量。

故障覆盖：晚期自动延迟字段缺失、错误实际名称/板型、AlarmService为空或将普通Device包装成该类型，在任何写入前拒绝；独占停止声明及全新审计目录前提失败不开始动作。Subscribe应答丢失只发送一次；Subscribe期间已回调则不开启后段板端检查器。临时firmwareEnable=1生效后断线保留UNKNOWN；应答期间收到告警则阻止后续SCW/清理，不伪装成硬件Stop。异步告警可中止1000ms等待中的后段；普通读改写在观察后出现本地停止信号不写入。成功后监听器持续接收通知，显式注销只操作本ID；失败/析构不reset、Stop、补偿或注销。

预检查暴露了旧Ice映射的实质区别：typed getter返回代理后checkedCast可按本地代理类型直接返回，先赋给ObjectPrx仍可能命中动态代理类型。实际代码改为显式远端ice_isA，错误代理TCP用例证明首次写入前拒绝。源码依据为本地Ice3.5.1的cpp/include/Ice/ProxyHandle.h及Proxy.h；不是根据函数名推断有远端检查。

独立对照tests/cobo_prepare_original_reference.py临时提取收到的impl_prepare，仅外围配置/服务替换记录存根，不调用新函数生成旧参考、不安装CCfg/Boost、不改变旧源码。前六个基础调用严格核对后按foundation阶段折叠；明确排除新增最终ADC通道修正，并把原告警封装与新自有订阅/检查器边界作阶段对照。它只证明主机组合次序，不证明旧所有子流程/异常恢复、板端时序或物理效果；那些子流程和故障另有既有/新增TCP测试。

严格C++11（-std=c++11 -pedantic-errors）和ASan/UBSan构建覆盖全部新纯核心、配置适配/配置CLI及原生硬件源、组合TCP测试与既有对齐CLI；本阶段运行纯Prepare测试和组合TCP两种编码，detect_leaks=1/halt_on_error=1及UBSan halt_on_error均通过。证据已归档到build-ice-compat/validation-prepare-sanitizers/：results-core.txt、results-native.txt、各对象compile日志及本机build-snapshot.py；实际运行目录为/tmp/getdaq-prepare-asan。本阶段没有把已编译但未重跑的其他插桩CLI算作运行验证；系统Qt、旧Ice/GET/bzip及yaml依赖未整体插桩。

230个本地文档链接、第三方归档SHA/大小/许可证及原GET许可证字节一致性通过；原生测试/对齐不链接Qt，接收器也不链接Ice/yaml。证据为build-experiment/results-prepare-document-checks.txt。构建日志仅有系统Qt头文件既有deprecated-copy警告，新源无编译错误。

可操作库接口、审计文件和故障处置见 [单板组合手册](COBO_PREPARE_CLIENT.md)。完整系统Prepare/Configure/同步、控制权/实际加载版本、生产ECC/网络服务、告警驱动硬件Stop/排空和真实恢复仍未完成；没有新增Prepare GUI/CLI或JSON部署入口。所有目标平台和实机验收仍待进行。

## 21. 全系统Prepare/Configure、同步与跨板告警

本阶段增加无Qt的SystemPreparationClient，先全局检查所有节点/服务/描述/实际名称、单板Prepare和后续Configure目标及停止观察，再组合MuTaNT Prepare、系统对齐、CoBo Prepare和Configure。单板Preflight与执行分开但保留同一监听器；Configure开始前再次全节点核对名称/元数据/停止位。保持机箱内掩码快照、跨板保持告警，以及PLL/AsAd之后的Level2/mutantMode/WSCA/同步/initDone/MuTaNT顺序。不接原始DAQ数据，没有新增第三方依赖。

| 证据 | 本次实际结果与范围 |
| --- | --- |
| 新全系统TCP回归 | 1/1通过，335.40秒；三MuTaNT/两活动CoBo/一停用CoBo，两种编码，另有独立CoBo及故障场景；build-ice-compat/results-system-preparation-tcp.txt |
| 单板/对齐/MuTaNT回归 | 3/3通过，223.92秒；build-ice-compat/results-system-preparation-components.txt |
| 其余原生完整回归 | 19/19通过，341.63秒，-j2；build-ice-compat/results-system-preparation-native-regression.txt。三组总计覆盖23个当前注册项；不是单个23项CTest运行 |
| 纯核心 | 9/9通过，0.03秒；build-experiment/results-system-preparation-core.txt |
| Qt完整回归 | 52 passed/0 failed/0 skipped，21.362秒；build-suite-tests/results-system-preparation-qt.txt |
| 真实旧SystemManager对照 | 提取7个原函数体，与3个成功TCP审计的完整主机阶段/同步顺序一致；build-experiment/results-system-preparation-original.txt |
| 既有单板旧函数对照 | 原impl_prepare的12组活动/计算/订阅阶段组合通过；build-experiment/results-system-preparation-cobo-original.txt |

共享夹具从既有单板Prepare与MuTaNT测试中提取到tests/cobo_prepare_fixture.h和mutant_fixture.h，旧测试继续使用；收到的两份描述不变，CoBo11设备/1310寄存器、MuTaNT5设备/87寄存器。模拟器仍明确禁止ADC读回，AGET型号/广播和寄存器实际位宽不通过精简假描述绕过。

全系统覆盖：所有MuTaNT先准备才对齐、最终对齐快照接续同步和Configure、全部CoBo/AsAd配置完才设置mutantMode、所有同步高脉冲先于任何同步低脉冲、全部CoBo initDone先于MuTaNT Configure。验证稀疏槽位、停用板不初始化/订阅、tag-only输出不混入保存的SHELF_MASK、外部修改原settings不改变已编译配置、0 MuTaNT独立模式不伪造对齐结果。监听器跨Prepare/Configure持续存活，完成后仍可收到保持告警。

故障覆盖：第二活动板的AlarmService实际类型错误、晚期系统字段缺失、重复节点及停用板仍运行，首次写入/订阅/对齐前拒绝；非法从模块ID/端口或Prepare/Configure集合/型号在构造时拒绝。第一块已准备板在第二块第一次写入时告警，第二块后段和订阅不继续。Prepare后第二节点名称改变会在Configure首次写入前拒绝全部配置。Configure服务应答期间告警阻止后续寄存器写入。同步高脉冲实际生效后断线，只调用一次；不继续别的模块脉冲、置低、恢复CLK2或设置initDone，失败不自动Stop/注销/清理。实际硬件Stop与恢复仍未接入。

独立对照tests/system_preparation_original_reference.py提取真实SystemManager的prepare、configure和五个系统位/同步函数，仅用外围节点/配置/数据链接记录存根，严格C++11编译；不调用新计划生成旧参考。比较本次实际成功的两编码三机箱及独立CoBo根审计。对齐内部和单板内部已有独立参考；本次阶段对照明确排除尚未迁移的数据连接、旧一致性检查环境和物理效果，不能据此宣称完整旧ECC或实机兼容。

严格C++11/ASan/UBSan构建覆盖全部新纯核心、配置适配/配置CLI、原生硬件源、新全系统TCP测试及既有对齐CLI。实际运行Prepare核心和全系统TCP全部正常/故障场景，ASAN detect_leaks=1/halt_on_error=1、UBSAN halt_on_error=1通过。证据归档build-ice-compat/validation-system-preparation-sanitizers/，含results-core/native.txt、各对象compile日志和本机build-snapshot.py；运行工件在/tmp/getdaq-system-preparation-asan。旧Ice/GET/bzip、系统Qt及yaml静态库未整体插桩；已编译但未重跑的其他插桩CLI不计作运行验证。

249个本地Markdown文件链接（正确处理文件名括号）、本地归档SHA/大小/许可及原CeCILL字节一致性通过；两份描述SHA不变。新系统测试/对齐不链接Qt，接收器不链接Ice/yaml。证据build-experiment/results-system-preparation-document-checks.txt。完整原生构建和新原生源无编译警告；插桩配置构建仅有系统Qt头文件既有deprecated-copy警告。

详细使用/状态/同步原理/部分失败处置见[全系统手册](SYSTEM_PREPARATION.md)。原生对象Configured明确不提交生产ECC Ready、不连接发送器；实际控制权与加载版本绑定、完整配置部署、ECC网络、Start/Stop/故障排空和启动恢复仍待接。实际环境仍为Ubuntu20.04.6/GCC9.4/Qt5.12.8，目标平台及真机验收均未完成。

## 22. 整系统实验参数工件与原生绑定

本阶段组合已有类型化子编译器，补齐Prepare的LEMO/手动读取延迟/四位置输入设置；新增纯C++11整系统参数一致性、严格有界JSON编码/读取、SHA-256参数版本及文件摘要，`compile-system`和无Qt的`getdaq-system-config`检查/绑定入口。原生BindSystemExperiment按实例ID/活动/主从配对端点，并恢复工件节点顺序；运行根审计记录两种输入摘要，明确actual_loaded_configuration_verified=false和production_ecc_state_committed=false。数据接收热路径与链接依赖未修改。

| 证据 | 本阶段实际结果与范围 |
| --- | --- |
| 纯C++11完整核心 | 10/10通过，0.04秒；build-experiment/results-system-experiment-core.txt；新增system-experiment测试涵盖全部模型编码/解码、精度、策略、摘要及非法输入 |
| Qt完整回归 | 52 passed/0 failed/0 skipped，20.998秒；build-suite-tests/results-system-experiment-qt.txt；统一四程序qmake构建也通过 |
| 原生完整回归 | 24/24通过，354.52秒，-j3；build-ice-compat/results-system-experiment-native.txt；含新核心、原有硬件/ECC、单板与全系统TCP，执行于最后绑定顺序补充之前，原协调路径未改 |
| 最终配置端到端回归 | 37个进程用例通过；build-ice-compat/results-system-experiment-tcp-final.txt；最后顺序补充后重新构建并运行工件到完整Prepare/Configure，编码1.0/1.1 |
| 既有七套子编译CLI回归 | AGET/读出/Configure/时钟/监控/校准/MuTaNT通过；build-suite-tests/results-system-experiment-*.txt对应各子编译器；其中监控31、校准33、MuTaNT49合同 |
| 严格C++11和内存检查 | 全部新纯核心、配置适配/CLI、原生硬件源及新的工件检查CLI/TCP夹具严格C++11插桩构建；实际新纯核心、37用例编译/读取/绑定及两编码完整TCP运行通过ASan/UBSan/LeakSanitizer |

真实旧配置是收到的configure-ExternalTrigger-2CoBo_1Mutant.xcfg，运行时fixture保持CoBo11设备/1310寄存器、MuTaNT5设备/87寄存器及真实字段位置。测试先离线编译，严格加载后绑定实际模拟代理，再完整Prepare/Configure；检查活动AGET/读出深度、停用板不上电、订阅生命周期、参数/工件摘要进入审计且不声称实际加载版本。拓扑数组反序仍恢复工件顺序与对应代理。模拟reg5按测试profile的预期型号准备，不作为现场型号验证。

跨进程用例还检查：Python hashlib独立计算参数域摘要、源码字节与工件字节摘要；最后补强profile字节摘要断言后，普通/插桩离线36用例分别重跑通过（results-system-experiment-cli-final.txt、results-cli-final.txt）。已有输出不覆盖，缺/多实例/芯片、错误活动/角色/策略/板型、重复/未知/缺失JSON字段、数字替代字符串、摘要改变和schema不支持均拒绝；重排键/缩进保持参数摘要但改变文件摘要；无MuTaNT配置/拓扑可离线绑定，align工具仍不接受虚拟独立对齐。原文件/profile字节不变。

输入默认值修正有专门测试：缺少旧enableAsAdPosition时，仍保留明确ground=true和antiSpark=false，默认值分别计算，不复制旧同一try块跳过后段的问题。只对当前支持的具体配置参数组合；共享标量class依旧拒绝，不是完整CompoundConfig语义证明。

纯解析/编码测试包含UTF-8/代理对、转义后重复键、尾逗号、多值、数值溢出/非法格式、缺字段/未知字段、固定数组长度、非有限实数、深度/字符串界限，以及Prepare/Configure活动/型号/掩码一致性。SHA-256以空串、abc和一百万a的已知向量核对，工件再与Python独立实现交叉检查；没有声称密码模块认证。

插桩证据归档build-ice-compat/validation-system-experiment-sanitizers/：results-core.txt、results-integration.txt、各对象compile日志、build-snapshot.py/build-result.txt和本次source-sha256.json。参数ASAN_OPTIONS=detect_leaks=1:halt_on_error=1、UBSAN_OPTIONS=halt_on_error=1；运行目录/tmp/getdaq-system-experiment-asan。最后绑定顺序修改后重编译对应源/夹具再链接运行；旧Ice/GET/bzip、系统Qt/yaml没有整体插桩。其他已构建但未运行的插桩入口不计入运行验证。

270个本地Markdown文件链接、3份固定归档SHA/大小、5处许可路径及CeCILL原文、两份原始硬件描述SHA及依赖清单JSON通过；工件检查/TCP程序无Qt链接，接收器无Qt/Ice/yaml链接。证据build-experiment/results-system-experiment-document-checks.txt；统一qmake和原生构建无新增源警告，严格插桩仅有系统Qt头文件既有deprecated-copy警告。

详细操作、profile全部字段、精度/版本原理及架构见[完整实验配置手册](SYSTEM_EXPERIMENT_CONFIGURATION.md)。本机NewOnly工件创建不等于远端原子/持久发布；没有生产ECC网络/状态/加载提交、真实控制权或数据发送器连接/Start/Stop/排空/恢复。实际环境仍为Ubuntu20.04.6/GCC9.4/Qt5.12.8；目标平台、实机和持续高速吞吐仍待验收。

## 23. 原生发送器控制与接收器生命周期边界

新增DataSenderClient/DataLinkSetClient，构造只解析本地代理，实际远端ice_isA先检查整个集合，Connect/缓冲区选择/刷新策略及Start/Stop/Disconnect逐条持久化意图和应答，未知结果不重发/补偿。组件没有原始帧API，也不提交ECC运行状态。

| 证据 | 实际结果与范围 |
| --- | --- |
| 完整原生回归 | 26/26通过，350.04秒，-j3；build-ice-compat/results-data-sender-regression.txt；包含原有24项及新增控制TCP/独立接收器两项，执行于最后代理去重与根日志失败补充之前 |
| 最终相关回归 | 最后代理去重/根日志失败修改后重新构建，2/2通过，5.15秒；build-ice-compat/results-data-sender-final.txt |
| 跨进程原始数据 | 两种Ice编码各含正常暂停恢复、截断尾帧、断开应用后应答丢失，共6组；前段记录build-ice-compat/results-data-sender-receiver.txt，最终运行也包含于上述相关回归 |
| 严格C++11/内存检查 | 全部纯实验核心、原生硬件库、新控制夹具及独立接收器严格C++11/ASan/UBSan构建；最终控制正常/故障及6组receiver运行通过ASan/UBSan/LeakSanitizer，实际范围与归档见下文 |

单sender两种编码均验证：原生连接策略/启停次序、各动作实际应用后断线只调用一次（含Slice声明idempotent的disconnect）、typed异常/方法缺失/超时保持UNKNOWN、实际类型在下一动作重查、已有审计不覆盖、checkpoint在第一条调用前及部分连接后中止，以及全部应答完成后日志失败不提交本地状态。多sender集合使用编码1.1验证第二接口错误在首Connect前拒绝、部分Connect失败不清理、Running拒绝全集合断开、后段接口错误在首Disconnect前拒绝、断开应用后应答丢失不继续后续，以及根断开意图无法写入时零调用并保持Failed。规范身份忽略超时/压缩，并拒绝同对象ID/facet下的重叠目标端点；DNS/不同IP是否指向同一物理设备不由本地检查证明。

接收联调由测试板端替身另开plain TCP，原始MFM不经过控制客户端。独立期望帧与实际graw逐字节相同；Stop后仍connected且不封存，Resume仍只有一次连接。Disconnect后原生receiver的--once正常退出、fsync封存、字节/文件数一致并释放目录锁。尾帧截断时控制操作全部成功仍返回数据错误/无seal，前面完整帧保留；反过来断开ACK丢失时控制UNKNOWN仍可独立形成完整数据seal。两份证据不能互相代替。

严格插桩目录/tmp/getdaq-data-sender-asan，初次完整重编译全部上述源，最后修改按源/全部相关头文件SHA检查后只复用未变化对象，并重编译变化源后重新链接。归档build-ice-compat/validation-data-sender-sanitizers/包含编译日志、commands.json、source-sha256.json及构建脚本/运行结果。参数ASAN_OPTIONS=detect_leaks=1:halt_on_error=1、UBSAN_OPTIONS=halt_on_error=1；实际运行新sender正常/故障TCP与六组独立receiver案例。系统Qt、旧Ice/GET/bzip及yaml静态库未整体插桩，其他已经编译的硬件子流程不等于本次重新运行其内存检查。

288个本地Markdown文件链接、3份固定归档SHA/大小、5处许可路径及原CeCILL一致性、清单JSON和动态链接依赖通过；73个插桩来源/头文件当前SHA与归档一致。证据build-experiment/results-data-sender-document-checks.txt。最终原生构建和上述插桩源没有编译警告。

详细控制地址/策略、状态/日志、旧Stop不产生EOF的源码依据、构建测试命令和后端接法见[发送器控制与封存手册](DATA_SENDER_CONTROL.md)。没有完整生产ECC/硬件运行StartStop、真实控制权/已加载版本、告警应急Stop、接收进程自动管理或恢复；没有真机、目标平台或持续高速性能结论。本阶段没有新增运行依赖，receiver仍无Qt/Ice/yaml。

## 24. 全系统采集顺序和失败后的生产停止

新增无Qt的SystemAcquisitionClient，接续同一已Configured准备对象与私有sender集合；按ID/活动/掩码/编码绑定并恢复配置顺序。组合运行前全部名称/描述/服务/停止/内存范围检查、CoBo RunInit及流水线/读指针/快控制、MuTaNT从→主运行、Pause/Resume、先停止全部生产再刷新sender和显式Disconnect。失败后可显式请求停止生产，不重发此前已尝试的停止阶段、不清UNKNOWN或关闭未知sender；库不提交生产ECC、不管理receiver。

| 证据 | 实际结果与范围 |
| --- | --- |
| 既有原生26项 | 原始27项完整运行中的既有26项全部通过，478.72秒，-j3；build-ice-compat/results-system-acquisition-native.txt。新增项当时暴露共享范围混用，该日志保留失败，不冒充27/27成功 |
| 修正后新增完整TCP | 1/1通过，281.06秒；build-ice-compat/results-system-acquisition-final.txt。与前项合计覆盖27注册项，分两次执行；新增项完整重跑，两编码全部正常/故障与独立/共享/关闭主触发模式 |
| 旧采集函数独立对照 | 提取真实SystemManager四个运行函数及CoBo六个函数/重载，12个成功审计阶段与板端Start/Resume合计28组比较通过；build-experiment/results-system-acquisition-original.txt |
| 原准备同步对照 | 既有真实SystemManager七函数与本阶段共享夹具产生的三个成功审计仍一致；build-experiment/results-system-acquisition-preparation-original.txt |
| 严格C++11/内存检查 | 全部新纯实验核心、原生硬件源、新采集TCP夹具及独立receiver严格C++11/ASan/UBSan构建；实际新采集TCP全部场景通过ASan/UBSan/LeakSanitizer，287.60秒，旧依赖未整体插桩 |

共享夹具从hardware_system_preparation_test.cpp抽到tests/system_preparation_fixture.h，原测试执行路径保留并重跑通过。收到的CoBo11设备/1310寄存器和MuTaNT5设备/87寄存器不变；模拟器仍禁止ADC隐式读回并使用真实字段位置。先完整原生Prepare/Configure，再构造运行对象；不是只伪造Configured阶段。

正常用例覆盖反序sender输入恢复配置节点顺序，活动CoBo先于MuTaNT从/主，CoBo reset/readPtr先于sender启动、sender启动先于固件开放、快控制先于周期pulser。首次全板RunInit先于逐板启动，外部时间复位策略保留；Resume不清CoBo时间或MuTaNT计数，但仍复位CoBo流水线与读指针。同一连接跨Pause/Resume，未使用的MuTaNT从sender不启动但最终断开；Stopped和Closed均不声称receiver_sealed。

故障用例覆盖：本地未Configured/ID/活动/掩码/编码错误；后段sender实际接口错误、晚期远端寄存器描述改变或实际memStart改变，在首次RunInit写入前拒绝；已有动作目录不发送命令且保留Ready。sender Start、firmwareEnable=1、周期pulser启动实际生效后丢失应答，后续节点不继续、不重发。失败后显式生产停止保持Failed和未知sender；再次调用不重发生产停止。

停止用例验证全部生产停止先于任一sender Stop；某个固件停止ACK丢失或pulser停止异常时，仍尝试其余独立生产停止，但不刷新/断开sender。未知的负向停止即使服务恢复，也不重发。某sender Stop应答丢失仍停止其他独立sender，但不Disconnect；断开ACK丢失不继续后段连接。保持告警不阻断显式Stop/Disconnect；从成功Paused结束只观察停止前提，不重复停止。

独立模式采用shared全局缓冲区，另测主模块所有触发、两类计数清除关闭和周期pulser关闭。shared计划最初错误地把逐AsAd分配摘要作为全局memEnd；实际完整TCP发现不相等。修正只从已编译的memStart/memEnd硬件写入像取共享范围，不修改已有读出参数计算或receiver；最终正常与插桩完整重跑均通过。计数清除关闭时保留RunInit的停止观察，没有计数命令。

tests/system_acquisition_original_reference.py直接提取收到的旧函数体，只替换外围对象/日志/数值字符串转换/寄存器记录环境，独立严格C++11编译，不调用新计划产生期望。主机阶段与CoBo Start/Resume写入顺序比较实际根审计及夹具动作；物理等待、温度转换、旧失败布局回退、新不可变内存核对、先停全部生产再刷新、停用sender连接策略及真实数据效果明确排除。两份收到的源字节SHA不变。

插桩证据在build-ice-compat/validation-system-acquisition-sanitizers/，包括results-native.txt、所有compile日志、commands.json、source-sha256.json、build-snapshot.py和validated-binary-sha256.json。ASAN_OPTIONS=detect_leaks=1:halt_on_error=1，UBSAN_OPTIONS=halt_on_error=1；实际运行新采集TCP正常/故障与两个额外模式。构建从完整编译开始，最后仅复用SHA完全一致的源/头文件对象；最后刷新独立参考脚本摘要后，两份已构建程序字节与已验证二进制相同。旧Ice/GET/bzip、yaml和系统Qt未整体插桩；receiver本阶段插桩构建不等于重新运行它的内存测试，普通跨进程receiver用例包含于既有26项。

normal-audit-index.jsonl与sanitized-audit-index.jsonl使用归档内相对路径，保存两次运行各12个成功阶段及子日志；两份results-reference-*.txt各28组对照通过。可在未来/tmp清理后重新检查归档，无须依赖原临时目录。详细操作、每个状态、时序、旧差异、日志、显式失败停止及后端接法见[全系统采集手册](SYSTEM_ACQUISITION.md)。

最终完整原生构建及新插桩源无编译警告；注入pulser异常时的Ice服务警告是预期故障证据。85个插桩来源/头文件SHA及已测试程序字节一致，309个本地Markdown文件链接、3份固定归档SHA/大小、5处许可路径和原CeCILL一致性、依赖清单JSON及动态依赖边界通过。记录build-experiment/results-system-acquisition-document-checks.txt；新采集测试不链接Qt，receiver不链接Qt/Ice/yaml。

本阶段未修改原始接收热路径或引入新运行库。没有生产ECC/GUI网络启停、实际控制权/加载版本、后台告警自动调度、接收进程自动管理、完整UNKNOWN恢复或实机/目标平台/持续高速结论。

## 25. 全系统Describe、明确替换与原生命令入口

新增无Qt的SystemDescriptionClient/getdaq-system-describe，复用完整拓扑与已编译描述，组合全部目标检查、明确现有核对/空节点创建/旧描述替换、全部CoBo initDone清零后再全部mutantMode清零及读回。ReplaceExisting持有显式当前布局/名称；HardwareClient::DestroyDescription只尝试一次，包含旧idempotent RPC也禁自动重发，UNKNOWN后不CREATE。创建本身不重置物理寄存器，空软件节点不能充当实际停止证据。

| 证据 | 实际结果与范围 |
| --- | --- |
| 原生完整回归 | 28/28通过，534.68秒，-j3；build-ice-compat/results-system-description-native.txt。包含既有采集/准备、持久化、发送器与独立receiver联调 |
| 最终新增入口相关回归 | 2/2通过，15.45秒；build-ice-compat/results-system-description-entry-tests.txt。TCP与新CLI；CLI登记后当前共29注册项，与前项合计覆盖29，分两次执行，不称单次29/29 |
| 命令入口 | 普通及插桩独立进程CLI均通过；插桩明确记录71个契约用例，三策略/两编码、离线/在线、当前拓扑绑定与实际ACK丢失；results-cli.txt |
| 真实旧源码顺序 | 提取SystemManager三个与CoBoNode两个真实函数体，6组成功TCP Describe阶段的状态写入一致；build-experiment/results-system-description-original.txt及归档results-reference-*.txt |
| 严格C++11/内存检查 | 全部新纯实验核心/原生硬件及ECC状态/日志源，两个TCP夹具和CLI插桩构建；ASan/UBSan/LeakSanitizer实际描述全流程9.59秒、既有HardwareClient完整266.49秒、CLI71用例15.44秒通过 |

初次新TCP运行因测试对重启错误文本的期望过窄而失败，2.60秒；results-system-description-initial.txt保留原结果。改为共同reconcil前缀后全流程通过，不将初次失败冒充成功。新增CLI目标先被调用时尚未重新配置CMake，初次入口构建日志记录No rule；重新配置和最终构建通过，见results-system-description-entry-configure.txt/results-system-description-entry-build-final.txt。

使用收到的CoBo11设备/1310寄存器、MuTaNT5设备/87寄存器真实布局。正常测试覆盖使用已有/空节点创建/明确替换、混合策略/不同旧名称和基址、停用实验板状态、无关位保留、全部新描述完成后才第一条就绪位、对象配置副本不随外部修改，以及执行中快速状态快照。全节点实际类型/完整元数据/名称/停止观察失败在首次修改前拒绝；空节点创建后发现生产运行，不写任一就绪位。

故障测试实际让destroy/create/状态写入生效后关闭连接，证明不自动重发或继续后段；destroy ACK后仍残留设备也阻止Create。根/子日志路径失败保留已完成结果，不清理。完整协调另接测试ECC后端/持久日志，验证成功后提交测试Described、部分创建后UNKNOWN、重启只读核对门禁；该适配器不是生产服务。CLI重新启动或新目录没有生产全局恢复能力，文档明确不能据此清除UNKNOWN。

独立参考脚本提取SystemManager::describe/setCoBoInitDone/setCoBoMutantMode和CoBoNode::setInitDone/setMutantMode，不调用新计划产生期望。对照只覆盖成功状态位命令及全部initDone→全部mutantMode次序；新读回/前提/替换策略、旧setter吞异常改为失败、真实停止及物理效果明确排除。

插桩证据归档在build-ice-compat/validation-system-description-sanitizers/：完整首次编译后，CLI/serve入口增补只复用SHA相同的来源与全部相关头文件对象，再编译变化夹具与新CLI。commands.json/source-sha256.json/build-snapshot.py及已测试三个程序SHA保留。ASAN_OPTIONS=detect_leaks=1:halt_on_error=1，UBSAN_OPTIONS=halt_on_error=1；旧Ice/GET/bzip/yaml未整体插桩，receiver/Qt本阶段未重新插桩。编译无错误或警告；旧HardwareClient大夹具的GCC调试变量跟踪超限提示是note，日志原样保留。

普通/插桩各6阶段的可移植索引及根/子日志随归档保存，独立参考可不依赖原/tmp目录再次执行。最终81个相关来源/头文件/脚本SHA、3个实际验证程序SHA、329个文档文件链接、3份固定归档SHA/大小、5处许可及原CeCILL一致性、清单/动态依赖和29项登记核对通过。检查记录见build-experiment/results-system-description-document-checks.txt；新命令/测试无Qt，receiver仍无Qt/Ice/yaml。本阶段不证明平台、现场板端、电气行为或DAQ持续吞吐。逐步命令、原理、架构和故障处置见[全系统描述手册](SYSTEM_DESCRIPTION.md)。

## 26. Breakup、撤销准备与自有资源释放

新增SystemAcquisitionClient::Breakup、SystemPreparationClient::Unprepare、实际供电/保护观察及CoBoAlarmClient::ValidateUnsubscribe。复用原对象、配置与保存掩码，只有低频控制，无Qt、新运行依赖或原始帧路径。当前仍是库接口，不是生产ECC网络/状态/Reset服务，也不管理receiver或实现UNKNOWN恢复。

| 证据 | 当前实际结果与范围 |
| --- | --- |
| 既有原生回归 | 29/29通过，533.20秒，-j3；build-ice-compat/results-system-release-regressions.txt。最终控制库和共用夹具下的准备/采集/描述/日志及独立receiver联调 |
| 新释放正常/故障组 | 9/9通过，438.03秒，-j3；build-ice-compat/results-system-release-groups.txt。两编码正常组各8成功阶段，七故障组独立实际Prepare/必要Configure；与前项合计覆盖当前38注册项，分两次执行，不称单次38/38 |
| 新旧函数独立对照 | 提取SystemManager四个、CoBoNode四个及Node一个真实函数体，16成功TCP阶段一致；build-experiment/results-system-release-original.txt |
| 原Prepare/采集对照回归 | 原系统Prepare七函数/三个成功阶段及采集十函数/十二阶段共28比较仍通过；build-experiment/results-system-release-preparation-original.txt、results-system-release-acquisition-original.txt |
| 严格C++11/内存检查 | 新核心/原生硬件/ECC状态日志源及释放/采集夹具插桩构建通过；现有采集实际ASan/UBSan/LeakSanitizer运行384.57秒通过。释放两编码正常组各410.78秒、七故障组各约31～71秒全部通过 |

初次编译因测试中两个auto声明混用int和size_t失败，随后修复。第一次TCP运行90.19秒失败，是共用Stop断言错误要求Breakup仍保留连接；修正为仍保留完整停生产/刷新顺序，再观察Disconnect。整项第二次运行在900.12秒超时，正常16阶段已完成，故障场景持续推进；首次合并插桩运行也触发释放900秒超时，已有采集384.57秒成功结果保留。上述失败/超时记录没有作为通过证据。

最终测试拆为两编码正常组和七故障组，各自拥有真实回环服务、完整准备对象、目录与索引；保留所有断言，不伪造Prepared/Configured、不跳过旧硬件等待。正常组最大900秒，单个故障组300秒，可并行执行并定位失败。原生完整九组均通过，正常两组约438秒，各故障约34～79秒。记录见results-system-release-initial.txt、results-system-release-final.txt、results-system-release-tests-build.txt及最终构建日志。

实际收到的CoBo11设备/1310寄存器、MuTaNT5设备/87寄存器用于全部元数据和命令。覆盖Ready/Running/Paused/Stopped/Closed/保持告警，六轮同一准备对象重新Configure、同一对象Breakup→Unprepare、自有订阅/机箱内掩码保留、全体initDone→全体mutantMode、实际供电11/0/2、不活动板仍断电、非零原始保护状态和外部订阅保留。缺本次data-stopped声明、后段节点名称/缓存告警服务错误及已有目录均在释放写入前拒绝。

故障夹具实际执行生产禁止、Disconnect、清状态、断电或注销后丢失应答，核对一次调用、停止后段及对象失败门禁。观察异常或某板断电未知仍独立尝试其他板断电，但不注销订阅；注销未知不继续其他订阅。不把负向阶段继续当重试，不自动清故障、关闭未知sender或丢弃监听器。未启用订阅时不虚构owner。

九旧函数参考不调用新计划生成期望，比较成功的自有Disconnect→全部状态位清除，以及每板实际供电/保护读取→断电。明确排除旧未自有sender的断开、旧总重复Stop、新增读回/前提、旧吞异常改为失败、固定ECC订阅改为独占本对象ID及全系统断电确认后注销。非零保护仅记录，不将ADC/保护状态或RPC ACK当作真实物理证明。

插桩证据归档在build-ice-compat/validation-system-release-sanitizers/，保存初次与最终构建记录、逐源编译日志/命令、83个来源/头文件/独立参考脚本SHA、两个实际验证程序SHA、首次合并运行超时及最终九组成功结果。最终按来源及全部相关头文件SHA只重新编译变化的释放夹具；现有采集实际成功运行384.57秒的程序字节及关联来源/头文件完全未变，复用其结果，不冒充又执行了一次。ASAN_OPTIONS=detect_leaks=1:halt_on_error=1、UBSAN_OPTIONS=halt_on_error=1；旧Ice/GET/bzip/yaml未整体插桩，receiver/Qt本阶段未重新插桩。

normal-audit-index.jsonl和sanitized-audit-index.jsonl使用归档内相对路径，各保存16个成功阶段及原数据链日志；results-reference-normal.txt/results-reference-sanitized.txt各九旧函数/16阶段通过。另保存既有采集12阶段及可移植索引，results-acquisition-reference.txt的28比较通过；不依赖原/tmp目录即可再次对照。归档源码摘要和两个已测试程序一致、349本地Markdown文件链接、三固定归档SHA/大小、五处许可与原CeCILL一致性、依赖清单/动态链接边界及38项登记核对通过；记录build-experiment/results-system-release-document-checks.txt。新控制命令/释放测试不链接Qt，receiver仍不链接Qt/Ice/yaml。

详细状态、逐步接口、审计、原理、架构、部分失败及完整生产Reset边界见[结束与撤销准备手册](SYSTEM_RELEASE.md)。实际平台仍为Ubuntu20.04.6/GCC9.4；目标平台、实机、持续吞吐及生产网络/控制权/加载版本/自动数据生命周期仍待验收。

## 27. 原生ECC九命令与持久操作绑定

2026-10-06在本机加入`getdaq-ecc-native/NativeEccCoordinator`。它实际调用已实现的描述、准备、配置、采集和释放库，并通过EccJournal提交命令状态；不是空成功后端。收到的完整工件重新解码、核对摘要并逐字节保存；部署/目标/完整描述策略/回调预算/sender配置形成不可变快照。请求键、操作ID、父子审计和保留的准备目录绑定，同一对象/监听器/对齐掩码贯穿命令周期。

**实际验证范围：** 本机Ice TCP模拟板端，不是现场硬件。数据生命周期使用明确的TestData替身，日志写`test_double:true/receiver_executed:false`；没有将替身成功解释为真实receiver启动、EOF或seal。网络服务器/GUI入口、真实进程管理器、全局控制权、实际加载版本、准备故障停生产和完整恢复尚未实现。Qt和原始数据接收热路径本阶段未修改。

普通构建通过；新增九个独立组最终9/9通过284.62秒。初次运行8/9通过，存储组只匹配“内容不同”，而改变部署名先触发正确的快照长度拒绝；修正断言并补充完成根索引后重新执行全部九组，最终均通过。原始失败记录保留`build-ice-compat/results-native-ecc-tests-initial.txt`，最终记录`results-native-ecc-tests-final.txt`。

| 分组 | 实际覆盖 |
| --- | --- |
| normal-0 / normal-1 | 全部九命令、严格工件/拓扑、全节点只读失败不消耗ID、复用失败预检查ID、并发拒绝与即时状态、Pause/Resume、Stop后Start保留原连接重新RunInit、再次Configure、两层Reset、自有订阅释放及重开核对 |
| fault-start | sender Start已应用后丢应答，后段不继续；ECC保持UNKNOWN，显式只停生产不改未知sender，第二次负向操作不重发生产停止 |
| fault-seal | 原生Breakup完成后数据封存条件失败，不提交Prepared，不重发Disconnect/Breakup |
| fault-reset | 断电应用后应答丢失，其余板仍独立断电，自有订阅保留，ECC不提交Described |
| fault-receiver-start | Configure硬件阶段完成后接收启动失败，不连接sender、不提交Ready、不重复启动 |
| fault-stop-receiver | 接收端故障不会通过只读预检查阻止运行中的Stop；原生停止后数据就绪失败保持UNKNOWN，显式负向入口不重发已确认停止 |
| fault-operation-audit | 最后Run命令应用后父操作日志丢失，ECC保持UNKNOWN；子对象Running也能显式只停生产，不碰sender |
| storage | 无效拓扑/sender构造不创建根目录、同日志锁冲突、部署快照不匹配、同inode工件篡改/改回仍保持门槛、意图之后操作目录替换不写硬件且保持UNKNOWN/待核对 |

受采集接口变化影响的既有六项回归6/6通过382.07秒：系统采集、系统准备、系统描述、数据发送器、ECC状态及日志。记录`build-ice-compat/results-native-ecc-regressions.txt`。当前总登记47项，本阶段是九项新测试和六项选定回归，**没有声称一次47/47或全部旧测试本轮均重跑**。准备/释放/描述的其他既有阶段证据继续保留在前节。

独立参考仍直接提取收到的十个SystemManager/CoBoNode函数体，不调用新计划生成旧预期。协调器普通与插桩各十二成功采集阶段、Stop后的第二次Start均通过，每组30项比较；记录`results-native-ecc-original.txt`和`results-native-ecc-sanitized-original.txt`。原温度、实际等待/物理效果、运行时布局回退和新停止/刷新边界明确排除；不能据此声称逐指令/实机/数据传输完全等价。

`tests/build_native_ecc_sanitizers.py`用`-std=c++11 -pedantic-errors`构建全部新参数、原生硬件、ECC核心/日志/协调器和夹具，启用ASan/UBSan。首次严格构建通过，随后只修正测试断言/根索引；依据记录的来源、所列头文件与精确编译命令复用45个未变对象，重新编译变化的夹具，并重新链接两个程序。最终来源/所列头文件/参考及构建脚本SHA共86项，旧Ice/GET/bzip/yaml静态库未整体插桩。

`tests/run_native_ecc_sanitizers.py --include-acquisition`实际执行九个新组和既有完整采集夹具，全部退出0：normal-0/1各160.99秒，故障组40.42～93.16秒，存储1.47秒，既有采集295.37秒。ASAN_OPTIONS=detect_leaks=1:halt_on_error=1，UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1；结果`build-ice-compat/results-native-ecc-sanitizers.txt`及归档逐组日志。这里既有完整采集本轮重新执行，不复用上一阶段运行结果。

归档`build-ice-compat/validation-native-ecc-sanitizers/`保留原始构建/失败/最终测试结果、精确编译命令/日志、来源和所列头文件副本/SHA、现有依赖SHA、两个实际验证程序SHA、普通/插桩十八个完整ECC夹具根目录及两份替换目录。原始日志不改写，只新增便携索引。normal/sanitized各十二阶段独立旧函数对照通过，既有采集另十二阶段/28项对照通过，共36个便携阶段；不依赖原/tmp目录。归档是只读证据，不应作为新的现场部署目录。

本阶段文档/归档/依赖核对以`build-ice-compat/results-native-ecc-document-checks.txt`为准；复现脚本`tests/check_native_ecc_validation.py`检查来源副本/SHA、已验证程序/依赖、文档文件链接、固定依赖归档/许可、清单边界、无Qt的新协调测试及无Qt/Ice/yaml的原生receiver。详细输入、目录、原理、接口、状态、故障和恢复门槛见[原生ECC协调器手册](NATIVE_ECC_COORDINATOR.md)。实际环境仍为Ubuntu20.04.6/GCC9.4，目标平台/实机/吞吐未验收。

## 28. 真实本机接收进程管理与原生ECC组合

2026-10-06增加纯C++11/POSIX的`getdaq-data-supervisor-core`和原生ECC侧`ReceiverProcessManager`。实际启动独立receiver，不传原始帧、不链接Qt；管理核心也不链接Ice/yaml。receiver新增小份PID/会话/sender/参数摘要/工件摘要/目标身份及实际EOF标记，已有TCP热路径只在EOF时增加一次计数。完整接收计划SHA加入父ECC不可变快照；数据接口的Validate同时核对父参数/工件摘要和全体发送目标。

**范围：** 真实本机进程、TCP、原始文件和fsync/seal；板端仍为收到的完整描述与Ice回环夹具。新ECC联调使用两个活动独立CoBo源、一个不活动CoBo、无MuTaNT的实际库流程；不将它当成MuTaNT原始帧/实机或持续吞吐证明。Qt没有接原始数据，GUI仍使用旧SOAP。远程接收管理、生产网络/GUI、全局控制权、实际加载版本、后台告警、准备故障停止及完整联合恢复仍待接。

| 验证 | 已完成记录与实际范围 |
| --- | --- |
| 普通构建 | 真实receiver、独立管理核心和完整Ice/ECC构建通过；最后测试补强单独重建真实receiver夹具 |
| 独立管理核心 | `build-data-supervisor/results-final-tests.txt`：11/11通过2.67秒，包含10项纯参数回归和一项14组真实进程联调；不是仅用状态替身 |
| 管理核心14组 | 正常两轮采集、真实连接后空EOF、截断、未连接TERM不算EOF、Finish超时不发信号、并发拒绝、部分启动失败/显式空取消、已接数据拒绝取消、seal身份/文件集、同长度计划/日志篡改、部署锁替换、重开门槛及无效总预算/端口/远端地址不创建部署 |
| 既有ECC九组 | `results-data-supervisor-ecc-tests.txt`中九组均通过；同一次13项执行的新四组因测试入口含`..`被正确拒绝，整次运行9/13，不能称13/13通过 |
| 新ECC/真实receiver四组 | 修正测试路径后4/4通过168.43秒；随后补强ACK丢失后的独立完整seal检查，最终三组3/3通过138.78秒，ACK组1/1通过54.28秒，分别在`results-data-supervisor-ecc-receiver-final-three.txt`和`results-data-supervisor-ecc-receiver-strengthened.txt`；最终四组分两次验证，不称一次4/4 |
| 接收器/sentry回归 | `build-data-supervisor/results-existing-receiver-integration.txt`：分片/大小端、导出、32MiB精确文件、截断/非法长度失败、目录锁、seal检查及归档仅一次通过；elapsed数值不是硬件吞吐指标；`build-receiver/results-supervisor-core.txt`原始核心通过 |
| 管理核心/receiver内存检查 | 全部新纯核心与两个进程严格`-std=c++11 -pedantic-errors`、ASan/UBSan/LeakSanitizer；14组最终全部退出0，`results-process-lifecycle-sanitizers-final.txt` |
| ECC/真实receiver内存检查 | 全部新参数/原生硬件/ECC/管理代码及真实receiver插桩构建；四组最终全部退出0：normal两组149.912/149.911秒、truncated93.159秒、disconnect-ack-loss77.410秒；`results-data-supervisor-ecc-receiver-sanitizers-final.txt`。旧Ice/GET/bzip/yaml静态库未整体插桩 |
| 既有ECC插桩回归 | 九组全部退出0；`results-data-supervisor-ecc-sanitizer-regressions.txt`：normal两组154.86秒，故障40.87～91.36秒、storage1.47秒；旧完整采集夹具本阶段未重新运行 |

正常ECC组分别使用Ice1.0/1.1，真正Prepare/Configure后才spawn两路receiver、核对全部Ready、连接sender。Start/Resume/Stop后再次Start实际送入已知MFM；Pause/Stop保留同一个PID/会话/原始连接。Breakup真正关闭sender，等EOF/队列排空、exit0、锁释放、完整seal及小份文件逐字节比较，才提交Prepared。再次Configure生成新会话/目录并完成另一轮，最后两层Reset回Idle。

截断组真正送入完整帧加100字节尾帧，一路异常退出且没有完整seal，另一独立源仍封存；父ECC保持Ready的最后确认值和UNKNOWN，不重发Breakup。Disconnect应答丢失组先真实关闭C0原始socket再丢Ice应答，不继续断开C1，也不自动调用整组Finish。夹具独立检查C0自然exit0、真实EOF、两个摘要/会话和精确文件，但ECC仍UNKNOWN、管理生命周期没有假装完成；完整的数据不能代替失去的控制确认。

首次14组插桩运行13/14通过，`file-set`在启动阶段遇到临时端口已占用；不是封存门槛失败。检查stderr确认端口冲突后，测试在本机默认临时客户端端口范围之外选择有界测试端口，保留全部断言重新执行14组。普通最初正常组因测试缺标准头文件编译失败，随后补齐；完整ECC四组首次因CMake默认receiver路径含`..`被管理器的规范路径检查拒绝，修正测试入口为真实规范路径后重跑。初次构建/失败记录均保留，未作为通过依据。

最终补强只改变真实receiver夹具的ACK丢失断言，普通四组与插桩四组按最终程序重新验证。插桩构建脚本依据来源、所列头文件和准确命令复用未变对象；另外逐字节核对已验证的既有ECC/采集两个程序在测试重链接后完全未变，保存`data-supervisor-unchanged-sanitizer-binaries.json`。不把未执行的采集程序称为本轮通过。

可复现构建/运行工具是`tests/build_native_ecc_sanitizers.py`、`tests/run_data_supervisor_tests.py`和`tests/run_native_ecc_receiver_tests.py`。接收器及纯管理器有独立CMake插桩构建；实际flags.make保留在新归档。ASAN_OPTIONS=detect_leaks=1:halt_on_error=1，UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1；不是旧依赖整体插桩，也不是磁盘断电/满盘验收。

新归档为`build-ice-compat/validation-data-supervisor/`，由`tests/archive_data_supervisor_validation.py`核对成功结果及实际编译输入后创建。保存114个来源/头文件/参考与辅助脚本的副本和SHA、八个已测试程序SHA、已有依赖SHA、真实编译参数/命令、初次失败和最终结果、普通/插桩各14个进程场景以及各四个完整ECC/receiver根目录。原始日志不改写，另外新增归档内相对路径的十二阶段`acquisition-index.jsonl`。

使用原有独立参考直接提取收到的十个SystemManager/CoBoNode真实函数体，普通与插桩十二阶段共48项比较通过；`results-data-supervisor-original-reference-final.txt`。未减少固定十二阶段门槛，组合的是四次真实正常运行各三个阶段；最初直接传单次三个阶段被正确拒绝的记录保留。排除真实等待、电气效果、运行时布局回退、新生产停止/刷新及原始传输，不能用寄存器/控制对照证明板端MFM完全等价。

`tests/check_data_supervisor_validation.py`通过114项当前/归档输入、归档完整文件摘要、八个程序/已有依赖SHA、386个本地Markdown文件链接、本地依赖归档/许可证原文、无Qt/Ice/yaml的独立管理测试和receiver，以及52项测试登记核对。结果`results-data-supervisor-document-checks.txt`。这只是证据/依赖边界检查，不代替未运行的52项整套回归、实机或目标平台。

详细字段、操作顺序、为什么状态查询不能探测数据端口、seal语义、库接法和失败排查见[接收进程管理手册](RECEIVER_PROCESS_MANAGEMENT.md)。当前Ice兼容构建总登记52项；本阶段分组验证，没有一次52/52或全套目标平台通过的声明。历史第27节及更早的来源/结果继续作为对应当时版本证据，不能用其当前源码一致性检查覆盖新实现。

## 29. 数据主机远程控制服务与原生客户端

新增独立 `getdaq-supervisor` 服务、纯 C++11 的 `SupervisorControl` 持久操作核心、`SupervisorHttpClient` socket 客户端与 `getdaq-supervisor-query` 命令行。数据主机绑定本地固定接收计划及程序/配置/工件摘要，所有控制请求要求认证和当前实例前提；首次进程动作持久绑定控制器快照/持有期。Qt Core/Network/Concurrent 仅用于低频服务入口，原生核心、客户端/CLI和receiver无Qt/Ice/yaml/curl依赖，MFM仍直接进入独立receiver。

**范围：** 一个Linux主机上的实际控制服务、客户端/CLI和receiver进程、回环TCP及合成已知帧。已有本机ECC适配仍保留；远程ECC生命周期适配、多物理主机组合、生产ECC网络/GUI和完整恢复尚未接入。公开身份/持有期不等于全局硬件锁或真实加载版本，HTTP没有内建TLS。本机通过不等于Ubuntu24/Windows/macOS、实机、持续吞吐或断电/满盘验收。

| 检查 | 实际结果与证据 |
| --- | --- |
| 普通纯核心与进程/控制回归 | 13/13 CTest通过，5.97秒；`build-data-supervisor/results-remote-control-cli-ctest-final.txt`。含既有14组真实进程夹具；服务夹具当时10组，下面最终11组普通/插桩独立重跑 |
| 最终真实远程控制联调 | 11组全部退出0；`results-remote-control-service-confirmed.txt`。证据目录`supervisor-control-evidence/remote-receiver-2p4o5ehg/`，另有8组原生HTTP用例、严格codec及凭据检查 |
| 最终远程控制内存检查 | 同样11组全部退出0，正常两轮7.076秒；`results-remote-control-service-sanitizers-confirmed.txt`，目录`supervisor-control-sanitized-evidence/remote-receiver-xls5fdzt/`。8组HTTP/codec/凭据同样通过 |
| 既有进程管理插桩回归 | 14组全部退出0；`results-remote-control-process-sanitizer-regression.txt`，目录`supervisor-control-sanitized-regressions/receiver-process-jvrq3prg/` |
| 共享HTTP的Qt完整回归 | 52 passed、0 failed、0 skipped，21.011秒；`build-suite-tests/results-remote-control-qt-final.txt`；统一四程序qmake重新构建通过，`results-remote-control-qmake-build.txt` |
| 已有原生ECC相关回归 | 存储门槛及真实receiver正常Ice1.1两组2/2通过，89.33秒；`build-ice-compat/results-remote-control-native-regression.txt`。不称本阶段全部54项执行通过 |
| 独立无Qt构建 | `/tmp/getdaq-supervisor-control-noqt/`从干净CMake目录关闭Qt服务、关闭开发测试构建原生核心与CLI，通过；`results-remote-control-noqt-config.txt`和`results-remote-control-noqt-build.txt` |

服务正常组真正启动两个独立receiver，管理器状态探测不消耗原始数据连接；两个sender分别送入分片的大/小端4096字节帧。自然EOF之后验证实际进程退出、封存身份、最终十进制字符串计数和原始文件逐字节相等，再启动第二轮生成新会话/目录。原生生产CLI实际完成身份/状态/历史查询与一次就绪请求提交；错误节点、计划或控制器被拒绝，已知旧实例请求不发送POST。

调用者提前关闭连接后，通过原操作ID查得完成记录，只有一次接受/启动意图。Finish慢操作期间查询缓存可及时响应；不同操作立即busy，重复同键返回已知记录。超时不kill、不重启receiver，不将后续EOF当成自动恢复。控制日志或认证文件遭修改后保持故障，恢复原文件不清门槛；完成前请求损坏会保留实际封存结果，但远程操作记录失败，不能发布控制成功。

部分启动组占住第二源的数据端口，保留第一源和失败记录；重复原start不重放，只有全部尚未连接的明确abort-startup可终止这些自有子进程，之后仍保持核对要求。正常服务退出不kill已就绪receiver，旧控制端口真正释放；新实例拒绝旧请求和自动接管。执行中SIGKILL故障仅针对测试服务本身，receiver存活；重开后未完成操作转UNKNOWN，没有启动旧worker或重新执行Finish。测试额外保留一个已接受但未完整读入的控制连接，实际核对receiver的`/proc/PID/fd`没有继承服务监听/accepted socket。

原生HTTP用例检查分片响应、大小/长度界限、重复Content-Length、chunked、截断、重定向、POST回复丢失和总截止时间；每例服务端实际只接受一次连接，不重发POST或跟随重定向。codec实际检查完整规范计划/请求键往返、未知/重复字段拒绝和2^53+1/UINT64_MAX的精确计数；认证检查0600、文件身份、symlink拒绝和恢复后仍拒绝。

插桩范围为全部新纯参数/严格JSON/SHA/进程管理/控制协议/客户端、Qt服务外围和独立receiver，严格`-std=c++11 -pedantic-errors`、ASan/UBSan/LeakSanitizer。使用`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`、`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`。系统Qt库本身未插桩；本阶段没有硬件库全量插桩回归。receiver复用之前的严格插桩程序，接收热路径源码本轮未改；真实执行和程序摘要重新记录。故意SIGKILL的服务不能进行退出泄漏检查，其正常运行兄弟场景完成正常退出检查。

最初原生客户端夹具缺少`EncodeJsonString`声明头而编译失败，补齐后通过；新增CLI目标过程中一次多目标build读取了旧的目标列表，重新生成后构建通过。初次失败及最终构建日志均保留。最终socket继承断言要求实际观察到accepted连接后再启动receiver，不依赖固定sleep；这项补强后，普通及插桩11组全部重跑，没有删减原断言。

最终证据归档为`build-data-supervisor/validation-supervisor-control-final/`，由`tests/archive_supervisor_control_validation.py`创建；保存当前源码/头文件/构建入口、文档、准确编译/链接参数、已测试程序摘要、最终普通/插桩服务根目录、14组插桩进程回归和实际ECC/receiver回归根目录。认证值及其文件不进入归档，复制时扫描排除泄露，不记录认证值的摘要。早期归档`validation-supervisor-control/`保留，对应最后文档一致性修订前的版本；不覆盖历史证据，也不用历史源码一致性检查支持本次新代码。

`tests/check_supervisor_control_validation.py`默认检查当前来源/已测试程序与新归档一致、完整归档文件集/摘要、11+8组实际覆盖、14进程回归、严格插桩参数、文档链接、依赖范围和无Qt客户端/receiver。`--historical-only`仅检查历史归档，不声称当前代码仍与历史相同。完整操作、字段、认证、状态含义、线程划分与故障处理见[远程接收管理手册](REMOTE_RECEIVER_CONTROL.md)。

## 30. 原生ECC远程数据生命周期与多节点范围

新增无Qt/curl的`RemoteReceiverManager`，将多个独立数据主机服务接到同一`NativeEccCoordinator`。数据接口新增必需的父快照/根目录绑定和缓存核对门槛；所有源ID/发送目标/参数及工件摘要整体先匹配，各节点Ready/Stopped结果绑定其自己的PID/会话/输出目录。进程动作先保存全节点意图，再一次POST并只用GET查结果；部分Start不继续后段、不回滚，Finish独立保留其他已EOF节点的退出/seal结果。父/子审计持续核对，恢复原文件不清门槛。

**范围：** 两个实际控制服务进程、独立原生receiver与原生ECC/硬件库，在同一个Linux主机的回环网络联调。板端由完整收到的CoBo/MuTaNT描述及Ice夹具模拟，使用两个活动CoBo源、一个不活动CoBo、无MuTaNT的流程。不是两台物理主机、现场板、目标平台或持续高速证明；接收热路径源码本阶段未改，没有新第三方依赖。生产ECC网络/GUI、真实硬件控制权/加载版本、后台告警调度、准备故障停止和完整恢复仍待接。

| 检查 | 实际结果和记录 |
| --- | --- |
| 初次完整普通远程ECC | 12/12通过，257.32秒；`build-ice-compat/results-remote-ecc-all-initial.txt`。之后独立参考发现Resume夹具索引混入Pause动作，保留该版原证据 |
| 初次完整插桩远程ECC | 12组全部退出0；`results-remote-ecc-all-sanitizers-initial.txt`，目录`remote-ecc-sanitized-evidence/native-remote-receivers-1_99m4fd/`。同样保留后续发现的索引记录边界问题 |
| 最终普通远程ECC | 修正记录边界后12/12通过，281.74秒；`results-remote-ecc-all-final.txt`，两种编码正常两轮以及全部故障/范围组重跑 |
| 最终插桩远程ECC | 12组全部退出0；正常两组154.945/154.944秒、截断100.715秒、Start应答丢失76.026秒、Finish应答丢失98.798秒；`results-remote-ecc-all-sanitizers-final.txt`，目录`remote-ecc-sanitized-evidence/native-remote-receivers-1s6rzg0n/`；其余逐组时长同该记录 |
| 既有原生相关回归 | 五组已有协调器测试与一组本机真实receiver正常Ice1.1，分两次命令共六项通过：`results-remote-ecc-native-regressions.txt`三组3/3、117.36秒，`results-remote-ecc-coordinator-regressions.txt`三组3/3、90.30秒；不是全部66项执行 |
| 严格插桩构建 | 新参数/JSON/SHA/原生硬件/ECC/本机与远程管理代码，57对象及四个测试程序严格C++11/ASan/UBSan构建通过；`results-remote-ecc-sanitizer-build-initial.txt`。最后只改变远程夹具的记录边界，按来源/所列头文件/准确命令复用56个相同对象、重编译1个，`results-remote-ecc-sanitizer-build-final.txt` |

完整正常组真实执行Describe/Prepare/Configure、全部receiver启动/Ready后才ConnectData、Start/Pause/Resume/Stop后再次Start。Pause/Stop保留两个节点各自的PID/会话/原始连接；Breakup实际关闭sender、发送自然EOF，核对两节点exit0/seal及与独立sender字节相同的文件后才提交Prepared。第二轮Configure产生两个新的会话/目录并完成另一轮，最后Reset到Idle。

Start应答丢失代理先把请求完整交给真实服务，读到202，再丢客户端回复；只出现一次Start接受，后续节点未Start，producer未ConnectData。独立观察第一节点实际Ready和同ID complete，仍不能清父Prepared/UNKNOWN和数据核对门槛。部分启动组真实占用第二节点数据端口，第一个receiver保留，第二节点失败，父未提交Ready，不重发、不kill已启动节点。

截断组实际向C0发送尾部100字节，保留异常退出/缺seal，另一节点完整EOF仍单独Finish/seal；整体保持Ready/UNKNOWN。Finish应答丢失组两条原始连接都已确认断开，丢第一个节点的服务接受回复，不重发它，继续一次核对第二节点。独立查得第一个最终complete/seal也不能让整体提交Prepared或清UNKNOWN。

Running及Paused状态重启第一服务的组真实SIGKILL并重新打开旧节点根。Stop不被这个数据失效挡在预检查，保留的硬件采集对象先执行/保留已确认生产停止，随后实例核对失败使父保持最后确认值和UNKNOWN；没有提前发出不确认的sender EOF。已停止/仍在运行的receiver不被产品服务或适配器自动接管、重启或kill。

短组分别验证数据直连/EOF/seal但不虚构父ECC状态转换、错误计划/目标拒绝、已完成父意图文件损坏及恢复后保持门槛，以及新对象新持有期不能接管旧服务。父Status叠加数据侧缓存核对标志，仍不等待网络。旧本机接口及明确测试替身加入必需Bind/Needs接口，替身不是数据证据。

内存检查运行`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`、`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`，原生ECC/硬件/接收适配、服务外围和实际receiver均插桩执行。旧Ice/GET/bzip/yaml及系统Qt库未整体插桩；服务/receiver复用上一阶段的插桩程序，本轮源码未变、实际再次执行。故意SIGKILL的服务不能进行退出泄漏检查，正常服务场景执行正常退出检查。新框架仍不证明磁盘满/断电等现场条件。

独立参考仍直接提取收到的十个SystemManager/CoBoNode真实函数体，不更改原源码。最初参考正确拒绝将Pause动作混在Resume的索引；修正夹具在Resume前单独清记录，不改实际控制动作或对照断言，普通及插桩12组全部重跑。最终组合四次真实正常运行各三个阶段，保存十二阶段的完整审计及可移植索引；结果见`results-remote-ecc-original-reference-final.txt`，不减少原有十二阶段门槛。

新证据归档为`build-ice-compat/validation-remote-ecc/`，由`tests/archive_remote_ecc_validation.py`核对最终成功结果、编译输入和实际程序后创建，保留普通/插桩共24个真实根目录、程序/来源/已有依赖SHA、准确命令、初次失败与最终结果及十二阶段索引。认证文件和认证值排除，不修改原始运行日志或旧阶段归档。`tests/check_remote_ecc_validation.py`检查当前来源/程序一致性、完整归档文件集合、实际覆盖、原生/接收器依赖与文档链接；historical-only仅检查归档。

当前Ice兼容CMake总登记66项。新增12组与已有六组分组验证，不称本轮全套66/66；Qt代码本轮未改，不把前阶段的52项通过当成这轮重跑。接口、逐字段、生命周期、作用域/持有期、审计、故障和操作示例见[详细远程ECC手册](REMOTE_ECC_DATA_LIFECYCLE.md)。

## 31. 固定 ECC 网络入口与完整跨进程命令

新增 `FixedEccConfiguration`、`EccControlJobs` 和可选 Qt 前端 `getdaq-ecc-server`。服务读取完整固定工件/拓扑/描述/回调/sender/数据节点配置，绑定输入身份和字节摘要；认证 HTTP 接受持久操作 ID，单后台线程复用同一个原生 ECC 和远程接收适配器。Qt 不进入原始 MFM 通道，数据仍由模拟板端 sender 直接送实际独立 receiver。

最终普通构建由 `results-ecc-network-final-build-fixed.txt` 记录，严格 C++11/ASan/UBSan 构建由 `results-ecc-network-sanitizer-build-final-fixed.txt` 记录。新增原生配置、网络执行、共用HTTP传输和 Qt ECC 前端源码均插桩；旧 Ice/GET/bzip/yaml 和系统 Qt 库未整体插桩。数据节点服务/receiver 使用前阶段的插桩程序再次实际运行，其对应生产流程未改。故意 SIGKILL 的服务不执行正常退出泄漏检查；正常服务场景包含退出检查。

| 已实际完成的检查 | 证据/边界 |
| --- | --- |
| 固定输入普通/插桩 | `results-ecc-fixed-input-strengthened.txt`、`results-ecc-fixed-input-sanitizers.txt`；离线不创建根，不发硬件命令；输入修改/恢复仍保持门槛，重复/持有期/非法状态和重开历史通过 |
| HTTP普通/插桩 | `results-ecc-network-http-smoke-strengthened.txt`、`results-ecc-network-http-smoke-sanitizers.txt`；实际服务进程、认证、严格字段/范围、重复请求和实例变更；明确未执行完整硬件或接收流程 |
| 最终完整普通七场景 | `results-ecc-network-system-final.txt`；全部实际退出0；每组实际ECC服务、两个数据节点服务，接收器只在进入Configure的组启动 |
| 相关旧路径六注册项 | `results-ecc-network-regressions.txt`；共用HTTP/旧服务、ECC存储、远程data-only/ownership/audit-fault，6/6通过，7.59秒 |

完整网络场景为：`normal-0`、`normal-1`、`start-ack-loss`、`truncated`、`ecc-restart`、`ecc-interrupted`、`request-fault`。普通正常两编码实际约128.80/119.28秒。正常和Start应答丢失组覆盖全部九命令、两次Configure/封存周期、Stop→Start同PID/会话、Pause/Resume保留连接、真实graw逐字节比较和自然EOF/退出/封存；最终普通新证据根为 `build-ice-compat/ecc-network-evidence/ecc-network-system-95tywnln/`。

应答丢失夹具先确认实际ECC服务接受Start，再丢客户端应答；只GET原操作，证明服务未因客户端断连取消、重放或换ID启动。此时ECC服务仍可明确记录成功，客户端传输不确定不能被混成板端UNKNOWN。截断尾帧组保持父ECC未知结果，并保留独立第二节点正常封存；新Breakup请求被门槛拒绝。运行后ECC重启保留原receiver、不重放硬件、生成新实例、拒绝旧实例和自动接管；Prepare中实际已发硬件动作后SIGKILL，重开操作显示unknown、仍为最近确认Described，不重放Prepare、不启动receiver。历史请求损坏/恢复组在Configure首次修改前拒绝，保持故障门槛。

最初六场景测试均通过，但初版 `scope.json` 在请求故障组错误标记了实际receiver启动；修正场景范围、补强接受次数和Prepare中断后全部普通七组重跑。保留初始结果，不把初始scope当最终覆盖证据。首次夹具编译中的字符串/枚举问题已修正并保留失败构建记录，不改变流程检查门槛。

`tests/check_ecc_network_validation.py` 独立核对最终普通/插桩七场景的退出、程序摘要、输入/快照/原请求/接受次数、实际seal/文件大小/字节总量、凭据泄漏、当前编译输入和无Qt的原生/数据依赖边界。十二个实际Start/Resume阶段另与收到的十个旧函数体对照，使用既有脚本原有十二阶段门槛，不减少覆盖。最终插桩结果和独立核对的实际结果补充在本节末，不用构建通过代替运行通过。

完整操作、19字段配置、七字段请求、状态/摘要含义、目录、故障和性能分工见 [ECC网络服务手册](ECC_NETWORK_SERVICE.md)，示例见 [服务配置](../config/ecc/service.example.json)。本机夹具为独立CoBo模式、两活动一停用CoBo、两来源和同一物理主机，Ice1.0/1.1不是不同真实固件/网络主机验收。GUI仍接旧SOAP，当前生产控制权、实际加载版本、准备故障停机、完整恢复/发布和Run归档组合未完成；Qt GUI本轮未修改，未声称本轮重跑Qt全套。

最终插桩七场景均实际退出0，正常两编码约143.82/145.93秒，Prepare中断6.83秒；证据为 `results-ecc-network-system-sanitizers-final.txt` 和 `build-ice-compat/ecc-network-sanitized-evidence/ecc-network-system-kfaojc0s/`。最终普通/插桩的两个正常场景合并保留十二个成功阶段，旧十个函数体的48项独立对照通过，见 `results-ecc-network-original-reference-final.txt`。独立十四场景/程序/来源/摘要/封存/依赖/文档与凭据检查通过，结果见 `results-ecc-network-validation-check-final.txt`。

新证据包为 `build-ice-compat/validation-ecc-network/`。归档器在当前输入复核通过后复制十四个实际根、八份实际执行程序、编译源/Qt生成源/命令/依赖摘要及结果，排除认证文件和认证值；四个正常场景的十二阶段索引改为相对归档路径，以便在临时编译目录清理后继续做旧函数对照。归档不修改旧阶段证明，不可当成可直接接管现场的部署状态。`check_ecc_network_validation.py` 的 `--historical-only` 只复核归档本身，不能用它证明当前工作树仍匹配。

## 32. GUI整系统协议与客户端基础

新共用协议位于 `common/control/eccprotocol.*`，从原生网络入口拆出请求/操作编码，并补齐严格身份、控制器状态及结果解码。保留规范请求字节、域前缀和SHA；不包含Qt、Ice或POSIX后端。Qt `EccSystemClient`已加入根qmake，提供认证、预期服务/快照/实例核对、必需意图写入器、单个待确认操作、同ID只读观察和保持门槛；HTTP头扩展保留旧调用默认行为。

| 实际验证 | 结果与证据 |
| --- | --- |
| 独立纯协议/既有参数核心 | 11/11，0.04秒；`build-control-protocol/results-protocol-core.txt` |
| 真实历史请求/结果契约 | 原版及协议/测试插桩程序读取上一阶段十四根的262条实际记录通过；`results-actual-archived-messages.txt`、`results-actual-messages-sanitizers.txt`。后者的参数静态库未插桩；全新原生/参数源的完整插桩构建另见后续行 |
| Qt新客户端专项 | 7功能加初始化/清理共9 passed、0 failed，约1.12秒；`build-ecc-system-client/results-client-boundaries-initial.txt` |
| Qt新客户端插桩 | 9 passed、0 failed，约1.12秒；`build-ecc-system-client-asan/results-client-sanitizers.txt`，C++11/pedantic/ASan/UBSan/LeakSanitizer；系统Qt库未整体插桩 |
| Qt旧完整回归 | 52 passed、0 failed，21.219秒；`build-suite-tests/results-ecc-system-client-full-qt-regression.txt` |
| 统一四程序qmake | 构建退出0；`build-suite/results-ecc-system-client-unified-build.txt` |
| 实际Qt客户端→实际ECC服务 | 认证/固定范围/保存原请求/同ID观察及非法Idle Start拒绝；`results-ecc-gui-actual-service-probe.txt`。不执行硬件或receiver，不能称完整Qt采集测试 |
| 实际Qt客户端→插桩ECC服务 | 相同边界通过；`results-ecc-gui-actual-sanitized-service-probe.txt`。此探针客户端本身为普通程序，客户端插桩覆盖来自上列独立专项 |
| 拆分后原生完整网络普通回归 | 7/7，正常两编码约119.09/130.31秒；`results-ecc-gui-protocol-system-regression.txt`，新根 `ecc-gui-protocol-evidence/ecc-network-system-8l6bc85f/` |
| 更新后的全新原生/参数/ECC前端插桩构建 | `results-ecc-gui-protocol-native-sanitizer-build.txt`退出0；新增纯协议源纳入构建，旧Ice/GET/bzip/yaml和系统Qt未整体插桩。实际七组运行结果另补在本节末，不用构建成功代替运行成功 |

专项验证意图先保存、同ID观察、应答丢失不换ID重发、错误范围/无写入器不POST、实例变化后只读历史、UNKNOWN不被身份查询清除、明确负向生产停止和认证值改变/恢复的门槛。网络夹具以操作ID统计独立命令效果，不将网络调用次数当成硬件副作用次数证明。Qt专用测试使用本机HTTP夹具，无真实硬件；实际服务探针故意使用非法状态，原生根没有硬件intent。

独立纯协议程序动态依赖仅系统C/C++运行库；Qt客户端测试仅增加既有Qt Core/Network/Test等SDK运行库，没有Ice/yaml/curl硬件后端依赖。原始receiver源/链路本轮未改。主窗口与RunControl仍走旧SOAP，新YAML全局模式、当前状态查询、节点映射、生产Run意图存储与接收封存/归档/恢复尚未接入，不能称新GUI整体完成。Windows Qt5.14.2 MinGW、Ubuntu24、macOS与现场验收仍待实际执行。

原生整体网络阶段的不可变归档 `validation-ecc-network/`保持原样；它包含当时的编译输入，当前源码已改变时只可历史检查。新的客户端/纯协议结果放在各独立构建目录和 `ecc-gui-protocol-*`证据目录，不覆盖旧证明。API、行为、构建与后续接入见[客户端详细说明](ECC_GUI_CLIENT.md)。

最终拆分后的全新插桩七场景实际全部退出0，正常两编码约147.01/146.95秒，Prepare中断6.78秒；见 `results-ecc-gui-protocol-system-sanitizers.txt` 和 `ecc-gui-protocol-sanitized-evidence/ecc-network-system-62ygjumb/`。十四场景/程序/当前编译来源/请求/快照/封存/依赖/文档和凭据检查通过，见 `results-ecc-gui-protocol-validation.txt`；收到的十函数体与十二实际阶段的48项独立对照通过，见 `results-ecc-gui-protocol-original-reference.txt`。新网络证据包单独保存在 `validation-ecc-gui-protocol/`，保留当前共用协议与GUI客户端源，不覆盖旧网络归档；Qt客户端专项结果仍以各自构建目录的实际输出为准。

客户端明确负向停止处理加入后，以最终同版源码重新构建Qt完整测试，最终仍为52 passed、0 failed，28.011秒；见 `build-suite-tests/results-ecc-system-client-build-final.txt` 和 `results-ecc-system-client-full-qt-final.txt`。先前21.219秒结果保留，不把它当成最终版本的唯一证明。

## 33. 全局GUI、当前状态门槛与Run记录集成（2026-10-06）

主窗口/RunControl增加明确native_ecc分支：只建一个整系统客户端，不执行旧逐板SOAP或sentry Catalog循环。YAML恰好11个固定客户端字段、sender/agent映射、缓存状态表、Pause/Resume和人工故障生产停止均已接入。原生Run先保留本地意图；Stop后观察新状态，再Breakup等待实际receiver封存，全部成功才Complete/推进Run号。数据文件保留在数据主机，GUI只保存引用；本地Run恢复要求原实例/快照和全部receiver相同会话/PID/dataset，不实现硬件UNKNOWN核对。

| 检查 | 最终同版结果与证据 |
| --- | --- |
| 类型化全系统协议 | 状态协议CTest1/1及276既有实际记录读取通过；build-control-protocol/results-status-core-initial.txt、results-actual-system-status-initial.txt。uint64最大值和9007199254740993精确解析，溢出/数值形式拒绝 |
| Qt客户端与存储 | 普通14 passed/0 failed，2.441秒；严格C++11 ASan/UBSan/LeakSanitizer14 passed/0 failed，2.512秒。build-ecc-system-client/results-native-gui-final.txt、/tmp/getdaq-ecc-client-store-asan/results-native-gui-final.txt（含初始化/清理） |
| 全局Run工作流 | 普通9 passed/0 failed，6.349秒；严格C++11插桩9 passed/0 failed，7.732秒。build-native-gui/results-native-gui-final.txt、/tmp/getdaq-native-gui-asan/results-native-gui-final.txt（含初始化/清理） |
| GUI与旧模式完整回归 | 最终同版53 passed/0 failed，20.855秒；build-suite-tests/results-native-gui-full-qt-final.txt。新增全局控制禁用逐板/Simulation、加载配置立即显示正确模式、缓存列、最大计数精度、查询忙时禁用与可滚动配置；旧模拟/SOAP/归档回归保留 |
| 根qmake四程序 | build-suite/results-native-gui-unified-build-final.txt，实际构建退出0 |
| 实际Qt完整采集普通两编码 | qt-gui-0/1均退出0，各87.179秒；build-ice-compat/results-native-gui-actual-final.txt，ecc-native-gui-evidence/ecc-network-system-dx5vt_bb/ |
| 实际Qt完整采集插桩两编码 | 两组均退出0；results-native-gui-actual-sanitizers-final.txt，ecc-native-gui-sanitized-evidence/ecc-network-system-n1o_50wc/。精确耗时见该结果文件 |
| 独立GUI证明复核 | results-native-gui-validation-final.txt；四实际流程/十程序记录、编译源SHA及Run请求/实例/会话/EOF/seal/数据大小和凭据边界通过 |

专用模拟组覆盖一次全局命令、应答丢失后原ID观察、封存失败不Complete/不推进Run号、明确负向生产停止仍保持UNKNOWN、延迟状态回复的按钮门槛、uint64字节/帧/文件计数、GUI退出后相同Run恢复而不重放Start，以及receiver会话/ECC实例变化时拒绝恢复。此组的PID/seal是模拟元数据；实际进程证明来自后面四组，不能混称。

实际Qt探针通过RunControl公开槽调用九种命令，Reset执行两次，共十个被接受的全系统意图。两活动一停用CoBo、两实际数据节点、两独立receiver，Start/Resume各产生两帧；每个receiver最终4帧/16384字节、正常退出、封存且PID/会话从Ready至结束不变。客户端原始意图、服务接受日志及Run五个活动期操作相互核对。全部在同一物理主机使用模拟板端，独立CoBo模式，不证明MuTaNT或现场固件/多物理主机/吞吐。

首次模拟组曾在Start应答丢失后无法继续Stop；诊断单项加等待通过没有作为最终证据。修正操作完成后新GET尚未完成却提前显示可操作、以及POST未返回时提前GET操作导致404的时序，随后重跑最终完整组通过。首次真实qt-gui-1在数据节点Identity→Observe间收到空生命周期计划摘要并保持门槛；修正原生缓存初始化，使中间发布也保留已绑定计划，重新跑两编码及插桩流程。初版失败与中间结果保留，最终证明只引用上述最终根。

tests/build_qt_control_validation.py在构建前后核对源码/本地头文件字节，记录实际程序SHA、Qt版本、Makefile/命令及moc输入；首次或构建配方改变时重建对象。三套新Qt普通/插桩程序及完整GUI的build-receipt/source-sha256证明当前版输入；系统Qt库未插桩。新Qt目标的嵌入yaml源码也使用其插桩标志；ECC后端旧Ice/GET/bzip/yaml静态依赖没有整体插桩。SIGKILL故障不能做被杀进程退出泄漏检查。

协议/GUI不增加第三方库。receiver动态依赖保持无Qt/Ice/yaml/curl，原生ECC网络夹具无Qt，新QtRun探针无Ice/curl。GUI数据计数不经double/int截断，不显示旧实时速率图；ECC的缓存计数不是持续健康/实时速率证明。Run QSaveFile原子替换、本地意图fsync和receiver数据同步是不同层，不由这次测试推断真实断电恢复。

缓存发布修正后，本轮后端七种普通和七种插桩场景全部重跑退出0；普通正常两编码130.394/129.893秒，插桩148.430/148.330秒。见results-native-gui-backend-final.txt、results-native-gui-backend-sanitizers-final.txt及对应ecc-native-gui-backend-*证据目录。独立复核十四场景、八程序记录、124当前编译输入、请求/快照/seal/依赖/凭据与文档链接通过；源码收到的十旧函数体对十二实际成功阶段48项对照通过，见results-native-gui-backend-validation-final.txt、results-native-gui-original-reference-final.txt。Prepare中断/请求损坏场景没有启动receiver，范围按scope.json分别保存。

本阶段分别归档至build-ice-compat/validation-native-gui/（四实际Qt流程、Qt专项/GUI完整结果与源）和validation-native-gui-backend/（十四实际网络流程及旧函数对照），原有validation-ecc-network/、validation-ecc-gui-protocol/保持原样。历史模式只核对归档字节，不证明后来修改过的工作树。不要把前一GUI客户端基础阶段的七组或52项直接算作当前集成证明。配置、操作、记录与故障流程见[GUI手册](ECC_GUI_CLIENT.md)，最终现场边界见下节。

## 34. 原生数据节点Run归档与GUI/ECC组合（2026-10-06）

新增ReceiverCatalog/getdaq-catalog：核对完整Stopped会话、PID/退出/seal/文件集合，保留源锁，在固定同文件系统根按实验/Run建立硬链接，原会话/seal保持可查。源意图先落盘，已有目标不覆盖，完成凭据最后发布；失败保留部分结果，不自动换ID重放。数据服务catalog请求绑定原11字段加实验/Run/会话，ECC只提交一次全系统元数据请求，GUI启用native_catalog后须全部节点归档确认才Complete。原始载荷不经Qt/ECC/数据控制服务。

| 检查 | 已核实结果与证据 |
| --- | --- |
| 原生文件系统及协议 | receiver-catalog-tests27组通过，supervisor-control-codec通过；普通CTest2/2、严格C++11插桩2/2。build-native-catalog/results-catalog-core-current.txt及/tmp/getdaq-native-catalog-asan/results-catalog-native-current.txt |
| 系统调用范围 | strace跟踪27组，未发现read/pread64/readv/mmap/copy_file_range/sendfile/splice读取或复制.graw载荷；results-catalog-syscall-trace.txt。512MiB为稀疏文件元数据/相同inode/blocks检查，不能作为512MiB流式吞吐测试 |
| 实际receiver生命周期 | 新catalog场景完成两实际receiver的EOF/封存、归档、保留原路径和下一独立周期；普通及插桩各15场景通过。results-catalog-process-lifecycle-current.txt、/tmp/getdaq-native-catalog-asan/results-catalog-native-current.txt及对应receiver-process-*根 |
| 实际数据服务 | 普通/插桩各15个服务场景、8个原生HTTP故障、codec/credential通过。新增正常/空数据catalog、错误会话拒绝、服务重启保留原结果而不重放。build-native-catalog-service/results-catalog-control-current.txt、results-catalog-control-sanitizers-current.txt |
| GUI模拟Run控制 | 普通10 passed/0 failed，6.587秒；插桩10 passed/0 failed，6.951秒。build-native-gui/results-catalog-tests-final.txt、/tmp/getdaq-native-gui-asan/results-catalog-tests-current.txt |
| Qt客户端与存储 | 普通14 passed/0 failed、插桩14 passed/0 failed；build-ecc-system-client/results-catalog-tests-final.txt、/tmp/getdaq-ecc-client-store-asan/results-catalog-tests-current.txt |
| 全部GUI及旧模式 | 最终同版53 passed/0 failed，21.153秒；build-suite-tests/results-catalog-full-qt-final.txt。根qmake四程序构建通过，build-suite/results-catalog-unified-build-final.txt |
| 实际Qt/ECC两编码归档 | 普通两编码完成，每receiver4帧/16384字节，原/归档文件device+inode一致；GUI data-catalogs.json与节点完成凭据一致。最初通过根ecc-catalog-gui-evidence/ecc-network-system-b1l5nd8z/；最终统一版本以results-catalog-gui-actual-unified-final.txt为准 |
| 实际部分归档失败 | 固定第二节点已有目标，第一节点归档成功/保留，第二节点旧marker不覆盖；GUI不Complete/不写CSV/不推进Run号，ECC Prepared保留全局与数据核对门槛。修正断言后专组退出0，59.116秒；results-catalog-gui-failure-fixed.txt及ecc-network-system-656tmh0l/ |

实际部分失败初版复核误调用只适用于健康状态的State/CheckFiles断言，因此流程已经正确保持门槛而测试仍退出1。改为明确要求Prepared、全局及数据needs_reconciliation=true，保留全部实际receiver EOF/文件字节核对；健康场景仍要求门槛为false。初版失败/中间输出保留，不能算作最终通过。

最终统一同版普通三场景全部退出0，正常两编码82.841/83.943秒、部分失败81.638秒；插桩三场景全部退出0，97.086/97.135/93.376秒。普通根ecc-catalog-gui-evidence/ecc-network-system-1seon5dk/，插桩根ecc-catalog-gui-sanitized-evidence/ecc-network-system-cvdyk8xb/，见results-catalog-gui-actual-unified-final.txt和results-catalog-gui-actual-unified-sanitizers-final.txt。check_catalog_validation.py核对六个真实Qt流程、十个执行程序记录、原请求/摘要/Run/节点状态/旧目标及源码SHA、相同inode和凭据边界通过；results-catalog-validation-final.txt为入口。

本阶段的新GUI/原生归档证据包为build-ice-compat/validation-data-run-catalog/，保留内部硬链接关系并排除认证文件和值；历史检查只证明建包时的内容，不代表后来修改的工作树。数据节点普通/插桩十五场景及核心/系统调用证明仍以各构建目录实际输出为准，不把模拟稀疏文件当成真实receiver跑数。

数据主机CLI/核心/服务CMake普通/插桩的输入与六程序SHA现由capture_catalog_build.py记录，Qt四目标由build_qt_control_validation.py记录；新ECC/硬件/归档/协议及前端使用严格C++11 ASan/UBSan，系统Qt、ECC旧Ice/GET/bzip/yaml静态依赖没有整体插桩。原receiver源和接收热路径未改，仍独立使用原插桩receiver。

后端普通七场景及插桩七场景通过（results-catalog-backend-final.txt/results-catalog-backend-sanitizers-final.txt），运行发生在最后归档故障夹具健康断言调整前；不把它们的执行程序SHA误称为后改夹具的SHA。新的最终归档证明以六组实际Qt案例为入口，旧validation-native-gui/和validation-native-gui-backend/保持历史不可变。

范围为同一物理主机、独立CoBo和模拟板端，未连接真实MuTaNT/固件；硬链接仅同文件系统，不复制跨主机载荷。工作原理、配置、CLI、文件结构和失败说明见[数据Run归档](DATA_RUN_CATALOG.md)。目标平台、断电、受控原会话清理、真实控制权/加载版本/硬件UNKNOWN核对和持续吞吐仍未完成。

## 35. Prepare故障的明确生产停止（2026-10-06）

SystemPreparationClient增加显式StopProductionAfterFailure；原生ECC UNKNOWN且没有采集对象时转给同一保留准备对象，不要求成功对齐/Configure。共用CoBo生产停止计划，先MuTaNT主/从，再活动CoBo的pulser/固件/中断；独立失败继续其他目标，每阶段尝试记录保留，不因换ID或恢复响应而重发。原失败操作UNKNOWN、准备Failed和告警订阅保留，不触碰sender/供电/receiver，不提交Ready，也不解除恢复门槛。手册见[准备故障停止](PREPARATION_FAILURE_STOP.md)。

| 验证 | 实际结果与证据 |
| --- | --- |
| 新准备故障/尚无采集对象 | 普通CTest5/5，67.17秒：两编码部分Prepare、停止写入后ACK丢失、节点名称变化、Configure接收启动失败。results-preparation-negative-native-current.txt |
| 已有原生协调器 | 普通CTest另8/8，242.93秒：两编码九命令/两周期，Start/Stop/Breakup/Reset/父日志/存储故障；与上组合计13场景，分两次执行，不称单次13/13。results-preparation-negative-native-regression.txt |
| 严格C++11插桩 | 新核心/硬件/协调器及固定Qt前端构建通过；原生13场景全部退出0，ASan/UBSan/LeakSanitizer通过。results-preparation-negative-sanitizers-build.txt、results-preparation-negative-native-sanitizers-current.txt；旧Ice/GET/bzip/yaml和系统Qt未整体插桩 |
| 实际ECC网络 | 普通两编码故障及停止ACK丢失3场景通过，19.556/19.456/19.555秒；实际ECC进程、两个数据服务、模拟板端。results-preparation-negative-network-current.txt，根ecc-preparation-negative-evidence/ecc-network-system-aklnr87e/ |
| 实际网络插桩 | 同3场景全部退出0，21.662/21.662/21.762秒；原生ECC/网络前端和数据服务使用插桩程序。results-preparation-negative-network-sanitizers-current.txt，根ecc-preparation-negative-sanitized-evidence/ecc-network-system-t55br_hl/ |
| 独立证据核对 | check_preparation_stop_validation.py复核六实际场景的原规范请求/摘要/实例、只接受一次原ID、原Prepare UNKNOWN、每板三停止阶段、第二ID仅stop_not_repeated、既有未确认不清除、父/网络终态、数据节点Fresh且没有receiver，以及源码/编译标志/凭据边界。results-preparation-negative-validation-current.txt |

真实网络故障发生在Configure之前，两数据服务只执行validate/verify-stopped和缓存查询，receiver没有启动。运行器的通用成功行提到native receivers，不能据此推断本组三场景执行过receiver；以scope.json的real_receivers=false和实际生命周期记录为准。原生13场景的数据生命周期为明确测试替身，虽然通过本机Ice TCP调用新硬件流程，也不提供真实接收封存证明。

六实际网络场景独立证据包使用新的validation-preparation-stop/，保存程序、构建输入/命令、源码和故障审计，排除认证文件和值；旧GUI/归档/网络证据包保持不可变。历史复核只证明建包时的归档，不证明后来工作树。GUI本轮只使用原有协议/明确负向动作，未执行新的实际GUI点击用例；原始接收热路径未改，不需要把本阶段称为新的DAQ跑数或性能验收。

本节为准备失败停止阶段的历史边界；当时尚未支持的Configured/Ready采集对象父UNKNOWN，现由第36节接续。后台告警自动调度、生产控制权、加载版本/完整硬件恢复、平台、真实断电、MuTaNT/现场固件和持续大流量仍未完成。

## 36. 运行前UNKNOWN的原对象停止与父意图绑定（2026-10-06）

SystemAcquisitionClient在父ECC明确UNKNOWN时，允许原Configured/Ready对象停生产，不退回准备对象或重建sender。父停止意图在执行期间绑定目录、inode、权限、长度和完整字节，提交backend_complete前再次核对；失败保持storageFault，恢复原目录/文件也不解除。正常Stop、Closed/Broken及完整恢复边界不因此放宽。

| 验证 | 实际结果与证据 |
| --- | --- |
| 新原生故障边界 | 普通CTest6/6，153.80秒：Configured/Ready各两编码、父停止意图文件/目录替换及恢复后门槛。results-pre-run-negative-native-current.txt |
| 原有原生协调器 | 普通另13/13，273.44秒；与上组合计19场景，分两次执行。results-pre-run-negative-native-regression.txt |
| 严格C++11插桩 | 构建通过；全部19原生场景退出0，ASan/UBSan/LeakSanitizer通过，旧Ice/GET/bzip/yaml与系统Qt未整体插桩。results-pre-run-negative-sanitizers-build.txt、results-pre-run-negative-native-sanitizers-current.txt |
| 实际运行前网络 | 普通Configured/Ready各两编码四场景全部退出0，70.668/69.816/70.266/69.058秒；根ecc-pre-run-negative-evidence/ecc-network-system-dob89rtv/，results-pre-run-negative-network-current.txt |
| 实际网络插桩 | 同四场景全部退出0，78.294/78.293/79.245/79.291秒；根ecc-pre-run-negative-sanitized-evidence/ecc-network-system-b639cfjb/，results-pre-run-negative-network-sanitizers-current.txt |
| 独立复核 | check_pre_run_stop_validation.py核对八实际场景、原请求/摘要/实例、父Configure UNKNOWN、原采集对象阶段、独立停止和新ID无硬件重放、相同receiver PID/会话/原目录、无Finish/Catalog/Start采集、源码/严格标志/程序依赖与凭据。results-pre-run-negative-validation-current.txt |

Configured网络场景在实际数据节点verify-ready应答期间通过板端告警回调触发联锁，采集对象已创建但尚未ConnectData。Ready场景在最后一条原始TCP连接建立后替换父Configure审计，采集对象连接完成但父ECC不能提交Ready。两场景实际启动两独立receiver并保留同PID/会话/目录；Configured没有sender连接，Ready保留原TCP连接，负向停止都不修改sender。没有发送MFM帧，不称作跑数、封存或DAQ吞吐；产品没有因此实现kill/restart/UNKNOWN恢复。测试退出时的夹具清理另行处理，不作为产品停止或封存证据。

本轮没有新实际GUI点击流程，复用既有协议和GUI明确生产停止入口。原生19场景使用数据生命周期替身；父停止意图文件/目录故障由这组本机Ice TCP夹具验证，不能拿八网络成功场景替代其故障范围。新的validation-pre-run-stop/保存八实际网络根、执行程序、源码/编译输入和汇总结果，排除认证值；此前validation-preparation-stop/及其他旧证据包不覆盖。历史复核只证明归档内容，不能证明后来源码未改。

网络运行器成功行现明确由各scope.json声明receiver/GUI执行范围，避免通用文案错误暗示所有场景都运行过receiver。操作、内部/父状态区别、原对象及意图文件说明见[故障停止手册](PREPARATION_FAILURE_STOP.md)。

## 37. 明确启用的后台CoBo告警策略（2026-10-06）

固定输入增加可选alarm_stop_policy，默认关闭，启用要求所有活动CoBo自有订阅和25..5000ms轮询间隔。原生缓存观察在命令执行期间不争用准备对象；同一入口锁内重新观察父状态，保存alarm-held依据/摘要和唯一内部请求，再执行一次正常Stop或既有UNKNOWN负向入口。重复通知和重开不重放，告警保持后不允许正向命令；普通Stop仍保留连接/receiver，不自动Breakup/完成Run。新增认证GET /v1/alarms返回缓存，已有请求/状态规范不增加字段。旧AlarmLogger仅写#STOP#请求日志，不把它冒充已停生产。详细原理见[告警策略](ALARM_STOP_POLICY.md)。

| 检查 | 实际结果与证据 |
| --- | --- |
| 固定配置普通/插桩 | 非法布尔类型/周期/额外字段、缺失活动订阅、默认关闭、保留内部ID前缀与重开门槛通过，未执行板端动作。results-alarm-policy-fixed-current.txt、results-alarm-policy-fixed-sanitizers-current.txt |
| 初步实际联调 | 普通运行编码0与Ready保持两场景通过；初始根ecc-alarm-initial-evidence/ecc-network-system-he5xoy2y/。最终证据以下列八场景为准 |
| 最终普通告警 | 八场景全部退出0：运行两编码、Paused、停止ACK丢失、Ready、关闭策略、服务重启、手动Stop期间告警。66.360/71.471/73.675/69.413/69.011/72.619/66.006/64.002秒；results-alarm-stop-network-current.txt，根ecc-alarm-stop-evidence/ecc-network-system-u8j0o46z/ |
| 最终插桩告警 | 同八场景全部退出0，81.241/81.341/82.242/80.092/76.084/80.894/77.733/76.030秒。results-alarm-stop-network-sanitizers-current.txt，根ecc-alarm-stop-sanitized-evidence/ecc-network-system-6o8vnoqx/ |
| 既有普通网络 | 未启用策略时七实际正常/故障场景全部退出0，独立复核通过；results-alarm-stop-network-regression.txt，根ecc-alarm-regression-evidence/ecc-network-system-fazjgon1/。本轮没有重复执行该七场景插桩组，不称新增14后端回归 |
| 编译与独立证明 | 新纯核心/硬件/原生ECC/固定Qt前端严格C++11 ASan/UBSan构建通过；实际告警执行与固定配置含LeakSanitizer，系统Qt/旧静态依赖未整体插桩。check_alarm_stop_validation.py复核16场景的原请求/摘要/实例/client_epoch、一次alarm-held与内部Stop、原receiver生命周期、失败/重开门槛及源码/标志/凭据；results-alarm-stop-validation-current.txt |

测试使用同一物理主机、模拟独立CoBo、实际ECC/两数据服务/两receiver。除Ready外发送器发两合成MFM帧，每receiver预期8192字节；Ready无帧。普通成功Stop是生产停止/刷新，receiver仍保持原PID/会话/TCP，不把它当EOF或seal。ACK丢失后独立生产停止继续，但没有确认全部生产停止就不刷新sender；重复/第二板告警与服务重开没有再次发送Stop。重开后的ECC数据缓存可为fresh/空，不能冒充重新核对了实际旧接收会话。测试退出清理不是产品恢复。

新validation-alarm-stop/保留16实际告警根、执行程序/源码/编译输入与汇总结果并排除认证值；七普通旧网络根另留工作目录，不声称已经进入这个告警包。旧validation-pre-run-stop/及其他阶段证据包保留。历史复核只证明建包时内容，不证明后来工作树。

本轮未新增实际GUI告警提示/Run完成联调，没有验证自动策略在已UNKNOWN准备/采集对象上的网络分支、监控意图落盘期间崩溃、TSan或真机保护效果；这些后续范围不能由八种场景推断。原生UNKNOWN负向停止的独立阶段证据保留于35/36节。实际加载版本/控制权、完整硬件恢复、物理多主机/MuTaNT、平台、断电和持续大流量仍未完成。

## 38. UNKNOWN与实际Qt告警缓存联调（2026-10-07）

新增纯C++11 EccControlAlarmStatus严格解码及可选GUI native_alarm_status。启用后，身份/摘要及有效告警缓存加入正向动作门槛；恢复watching不清保持，身份变化不能通过负向操作接管。NativeRunControl显示原保持原因/停止ID与精确通知计数，把原JSON保存在未完成Run，不自动Breakup、生成CSV或推进编号。服务端策略和GUI查看是独立开关；原请求/状态字段及原始接收热路径未增加依赖。逐项操作见[告警手册第9节](ALARM_STOP_POLICY.md#9-在gui里启用查看逐步理解结果)。

| 检查 | 最终同版结果与入口 |
| --- | --- |
| 纯协议/参数核心 | 11/11通过，含告警计数uint64边界、溢出/错误类型、订阅范围及phase/latched矛盾拒绝。build-control-protocol/results-alarm-gui-codec-current.txt |
| Qt客户端/存储 | 普通与严格C++11插桩各16 passed/0 failed（含init/cleanup）；两构建目录results-alarm-gui-tests-current.txt |
| Qt Run模拟工作流 | 普通/插桩各11 passed/0 failed（含init/cleanup），9.206/10.887秒；启用告警缓存的正常Pause/Resume/Stop→Breakup，以及告警保持未完成Run通过。归档时序用例还确定触发旧GET晚返回，Catalog只提交一次 |
| 主窗口/旧模式 | 最终同版53 passed/0 failed，21.357秒；build-suite-tests/results-alarm-gui-tests-current.txt。根qmake四程序构建通过，build-suite/results-alarm-gui-unified-build-current.txt |
| 实际普通网络/Qt | 五场景退出0：Prepare UNKNOWN两编码、Start应答丢失后告警、实际Qt告警两编码。19.305/19.405/72.309/75.172/75.072秒；results-alarm-gui-network-current.txt，根ecc-alarm-gui-evidence/ecc-network-system-698v9m7r/ |
| 实际插桩网络/Qt | 相同五场景退出0，22.664/22.764/79.685/82.036/81.936秒；results-alarm-gui-network-sanitizers-current.txt，根ecc-alarm-gui-sanitized-evidence/ecc-network-system-265soz31/ |
| 独立复核 | check_alarm_gui_validation.py核对十实际场景、十执行程序记录、七Qt构建输入/生成源/程序/Makefile、原请求/摘要/属主、唯一alarm-held/停止、原失败/保留对象、原receiver会话、GUI原告警缓存/未完成Run与凭据边界；results-alarm-gui-validation-current.txt |
| CTest入口 | 新增三UNKNOWN场景已注册；results-alarm-gui-cmake-registration-current.txt。实际执行证明来自上述五场景运行器，不把-N列举当作测试执行 |

Prepare两个场景在Configure前失败，没有receiver；自动负向停止沿用原准备对象。Start应答丢失时沿用原采集对象，仅C0 sender生成两帧，C1未Start。所有UNKNOWN场景保留原Prepare/Start ID、原控制状态/outcome_unknown和全局门槛；内部停止complete不提交Ready或恢复完成，不触碰未知sender、不调用Finish/Catalog。普通早期三场景及修复前普通/插桩五场景仍留原根，最终证明以上述最终根为准。

实际Qt探针运行RunControl/NativeRunControl，在Running后由测试夹具从模拟板端注入告警；经过真实Ice回调、ECC后台策略和认证HTTP告警读取。探针不是人工操作的现场窗口；它核对显示信号/日志、原JSON和未完成Run，完整主窗口另由53项验证。健康自动Stop结束生产并保留两实际receiver PID/会话/TCP，final-status.json不报告reaped/seal。Qt场景每sender生成两合成帧；状态计数是旧缓存，可以仍为0，不能拿它画实时速率。退出清理可能随后关闭socket并产生seal文件，这不代表产品已Breakup、GUI已确认封存或Run已完成。

补测正常流程时，首次插桩组发现重复Catalog（10 passed/1 failed）。新增确定时序夹具：Catalog尚在执行时GET保存旧状态，Catalog结果先返回，旧GET后返回；修复前普通专项明确出现两次catalog。修复让GUI在操作终态后丢弃已在等待的状态回复，再明确GET；不放宽状态、摘要或Run门槛。随后重建七Qt目标、重跑完整11/16/53项和上述十实际场景。失败/复现日志保留于Qt构建目录results-alarm-gui-tests-before-catalog-check.txt及results-alarm-gui-catalog-reproducer.txt，不能算最终通过。

严格C++11 ASan/UBSan覆盖新协议/参数/硬件/ECC/低频前端及Qt控制源码，Qt专项和实际正常退出含LeakSanitizer；系统Qt及旧Ice/GET/bzip/yaml静态库未整体插桩。没有执行新的TSan、告警意图写入崩溃或完整硬件恢复。无Qt原始receiver及无Ice Qt探针的动态依赖边界通过；不是物理多主机、MuTaNT/固件、目标平台、断电或持续吞吐证明。

本阶段独立证据包使用build-ice-compat/validation-alarm-gui/，保存十实际根、执行程序、严格构建输入/命令、七Qt构建/生成源及最终结果，排除认证文件和值；旧validation-alarm-stop/2898文件及其他包不覆盖。archive_preparation_stop_validation.py的可选Qt输入支持复用打包入口，未改变旧包契约。历史检查只证明归档内容，不证明后来工作树。

## 39. 配置共享类的单位/范围与来源投影（2026-10-07）

XcfgDocument新增按元素名形成的共享类视图，忽略每层ID，共享unit/range而保留独立值/type。按旧XML入口顺序先载入独立include、导入类信息，再处理本地子元素；本地非空setUnit/setRange更新整类，导入的不同ClassInfo拒绝加入现有类。一个来源用于多个投影类时，编辑须满足所有共享范围，并在保存前核对整个已加载来源集合；源原字节仍只替换选中值。投影有次数、深度和字节上限，超限不形成部分结果或写文件。

ExperimentReader及各配置编译器使用同一有效属性，参数shared_metadata记录单位/范围的来源；GUI仍显示声明树，查询另给出有效单位/范围及各自文件、行号、路径。状态文字采用纯文本，不将配置值当成HTML。没有增加TinyXML/Boost或其他运行库，也不将配置处理引入原始MFM通道。操作、例子、单位不换算和冲突说明见[配置手册](CONFIGURATION_EDITOR.md)。

| 检查 | 当前已核实证据 |
| --- | --- |
| 原始语义定位 | 收到的Scalar::updateClassInfo、Attribute::getClassPath、ScalarClassInfo::equals、XmlToConfigVisitor::VisitEnter、Document::parseInclude及ScalarTest的单位/范围传播。属于源码/原测试预期对照，没有声称实际运行了完整旧CompoundConfig库 |
| 专用模型普通/插桩 | xcfg-shared-tests各7 passed/0 failed（init/cleanup加5场景）：值/type独立、跨include多个投影的共同范围、导入信息一致/冲突、其他元数据文件变化拒绝保存、超限不写。build-xcfg-shared/results-shared-class-tests-current.txt和/tmp/getdaq-xcfg-shared-asan/同名结果 |
| Qt/配置完整回归 | 55 passed/0 failed，22.976秒；新增两场景验证本地属性更新、include/来源/保存，以及实际点击Find value source后的单位和范围显示。build-suite-tests/results-shared-class-tests-current.txt |
| 原生GUI流程插桩 | 11 passed/0 failed，Run/告警流程没有新增行为；/tmp/getdaq-native-gui-asan/results-shared-class-tests-current.txt。该组不替代新配置模型的专用检查 |
| 整系统离线工件 | 36个实际CLI编译/读取/绑定场景通过；settings域SHA、profile/源字节、绑定拒绝、失败不发布和已有目标不覆盖。results-shared-class-system-cli-current.txt；没有传入硬件执行夹具，不是TCP/实机验证 |
| 参数CLI | AGET单片/整板、读出及MuTaNT49合同场景通过，含实际旧配置、单位/来源、默认/include与不覆盖。results-shared-class-aget-cli-current.txt、results-shared-class-readout-cli-current.txt、results-shared-class-mutant-cli-current.txt |
| 构建 | 根qmake四程序通过，build-suite/results-shared-class-unified-build-current.txt；专用模型和相关GUI使用同版源/程序/生成输入收据，插桩模型严格C++11 ASan/UBSan/LeakSanitizer通过，系统Qt未插桩 |

测试证明当前支持的共享属性与编辑/编译行为；导入范围比较暂时要求相同表达式字节，不能把格式不同但语义等价的范围误称为旧库冲突。复杂枚举、宽整数/严格类型范围、Range::equivalent完整语义、对象物化/覆盖创建、旧库全量运行对照、远端不可变发布和实际加载版本仍待完成。本阶段没有改硬件/ECC命令或receiver，没有重跑实际板端流程；前38节的封存/告警证据继续作为独立历史证明。

build-suite-tests/validation-shared-class/保存254个文件，包含四个Qt构建/程序/生成源与结果、已核对源码、CLI汇总及上述原库源码参考；manifest重新核对通过，见results-shared-class-archive-current.txt。包内旧库文件是语义定位材料，不是旧库运行结果；归档也不提供实机/数据吞吐证明。旧告警和其他阶段包保持不变，历史证据不能证明后来工作树。

## 40. 类型化配置范围和共享定义语义比较（2026-10-07）

新增纯标准C++11 configurationrange核心，不依赖Qt、Boost、Ice或XML库。int/hex分别按int64/uint64精确比较，real采用固定locale有限double，bool/string保留类型；范围包含与等价不经隐式数值转换。解析支持同类型集合、双引号/C转义字符串、数值/布尔/字符串区间与无界端点，去重/排序后比较枚举，规范字面量后比较端点；完整消费输入并检查长度/项数。GUI CheckValue、共享import与ExperimentReader均复用它，布尔编译输出规范化但保留原值追溯。

| 验证 | 实际结果与证据 |
| --- | --- |
| 无Qt核心 | 普通CTest1/1及严格C++11 ASan/UBSan/LeakSanitizer1/1通过；build-configuration-range和/tmp/getdaq-configuration-range-asan的results-configuration-range-tests-current.txt。测试覆盖64位边界、2^53相邻值、严格域、空集合/空字符串、开/无界区间、quoted逗号/转义、坏输入/项数/字节预算 |
| 收到的旧函数体 | configuration_range_original_reference.py直接提取七个Value比较、Interval/Enum包含/等价及同质性函数体，原32位域9972组比较通过；build-configuration-range/results-original-reference-current.txt和original-reference/。脚手架仅补原类型/对象容器与比较运算外围，原函数体未调用新算法充当旧参考；没有运行旧Spirit字符串解析器或整个旧库 |
| GUI/配置 | 完整Qt55 passed/0 failed，22.663秒；build-suite-tests/results-configuration-range-tests-current.txt。专用共享流程插桩9项（init/cleanup加7），含语义相同hex范围导入、最大uint64保存、quoted字符/布尔规范与real/int域不混同；/tmp/getdaq-xcfg-shared-asan/results-configuration-range-tests-current.txt |
| 离线工件/参数 | 整系统36场景、AGET与读出CLI通过。MuTaNT50合同通过，新增hex值不得进入继承int范围的拒绝，再用hex范围验证成功；build-suite-tests/results-configuration-range-*-cli-current.txt。原49合同中正向hex案例曾保留int范围，旧宽松校验掩盖了不一致，本轮按旧contains类型规则修正该夹具，没有放宽解析或改用户原配置 |
| 工程 | 根qmake四程序构建通过；build-suite/results-configuration-range-unified-build-current.txt。新核心独立CMake、编辑器/各编译器qmake及专用Core/Test均使用相同源码；系统Qt和旧库未整体插桩 |

收到的旧Int/Hex是32位，新int64/uint64是明确扩展，9972对照仅覆盖原域的比较/成员/等价规则，不证明旧库接受64位字面量。新解析器拒绝尾部垃圾、坏分隔/转义、溢出和非有限real，不继承旧fromString未检查parse.full的宽松行为。集合不同类型不混合，interval的有限端点必须与值同类型；两个端点无界时沿用旧类型无关contains逻辑。原生接收器和硬件/ECC命令本轮未改，未执行新的板端流程或吞吐测试。

配置页面的范围语法和例子见[配置手册](CONFIGURATION_EDITOR.md)。旧Spirit词法/转义的全量运行对照、完整旧库、对象物化/覆盖、不可变发布/加载版本及目标平台仍待完成，不能把核心函数体参考称为全量解析兼容。

独立证据包build-configuration-range/validation-range/保存257文件，包括三Qt构建/源/程序/生成输入与结果、两无Qt核心程序/标志、七旧函数体参考的生成程序/执行文件/源摘要和CLI汇总；manifest逐文件复核通过，见results-range-validation-current.txt。旧validation-shared-class/254文件及其他包保持不变。历史包只证明当时程序和源码，不证明后来工作树。

## 41. 已有具体实例的覆盖预览与保存（2026-10-07）

XcfgDocument新增PreviewOverride/ApplyOverride。目标必须通过原Lookup/共享约束确认且不含通配ID；在主文件的已有本地对象下建立缺少的具体结构和显式type scalar，可扩展自闭合对象，不改模板或include。先用全来源快照在内存重新解析候选，再枚举前后有效scalar集合及其他值/type/共享属性；无法证明只有目标变化的层级或include碰撞拒绝，不写文件。应用重新生成预览并核对字节、所有来源后才QSaveFile保存并重载。

GUI增加Preview instance override、候选XML只读框和Apply previewed override。预览不写文件；路径/值改变、Open/成功修改会使旧预览失效；连接期间控件禁用。保存只处理本机配置，不调用ECC/板卡、不进入原始MFM通道；没有新增库。明确操作步骤、include限制及单文件原子替换边界见[配置编辑器](CONFIGURATION_EDITOR.md#为已有实例创建覆盖)。

| 检查 | 当前结果 |
| --- | --- |
| 核心模型普通/插桩 | 各13 passed/0 failed（init/cleanup加11场景），31/73ms；新增四场景核对具体层级生成/其他实例不变、伪造预览拒绝、既有直接值不重复、不存在路径/范围/include冲突拒绝、来源变化和空字符串scalar保留。build-xcfg-shared和/tmp/getdaq-xcfg-shared-asan的results-override-tests-current.txt |
| 完整Qt | 56 passed/0 failed，22.804秒；真实点击预览/应用，核对预览未写、输入变化撤销、仅Board[0]变化和只读禁用。build-suite-tests/results-override-tests-current.txt |
| 编译入口回归 | 原系统工件36场景、AGET单片/整板、MuTaNT50合同通过；results-override-system-cli-current.txt、results-override-aget-cli-current.txt、results-override-mutant-cli-current.txt。未传硬件执行夹具，不是板端测试 |
| 工程/源 | 根qmake四程序通过；build-suite/results-override-unified-build-current.txt。三Qt构建/源码/程序摘要核对，专用模型严格C++11 ASan/UBSan/LeakSanitizer通过，系统Qt未插桩 |

此处是受约束的单值覆盖，不是完整CompoundConfig对象物化/结构编辑。当前已加载树之外的引用者、跨文件写锁/断电恢复、旧库/Spirit全量对照、有效快照持久变更审阅、不可变发布/实际加载版本及平台/实机仍待完成。候选拒绝时不删除原声明、断开include或展开反写来换取成功；旧接收/告警/范围归档保持独立历史证明。

## 42. 持久有效配置快照与变更审阅（2026-10-07）

新增EffectiveConfigurationSnapshot，保存已加载树的具体对象、有效标量、类型/规范值、共享unit/range、各自来源和原文件摘要。effective_sha256与snapshot_sha256分开，前者按确定长度字段/规范范围编码，后者还包含原文本、声明类型、来源路径/行号与字节摘要；不依赖JSON缩进/键顺序。空对象也进入有效摘要，不把默认模板生成虚拟实例。读取严格拒绝重复键/未知字段/坏UTF-8、类型/范围/来源/父对象/排序及摘要不一致；普通控制JSON的4096预算保持，快照显式扩展有界预算。

GUI连接期间允许Export effective snapshot/Compare with saved snapshot，按钮任务在后台执行；配置切换后旧比较失效，已完成的旧导出明确标记先前配置。getdaq-config增加snapshot和diff-snapshot，保存新文件，不覆盖原件。XcfgDocument的Lookup/Children/有效树枚举共用有界子项索引缓存，保持默认回溯/include重复拒绝；大配置重复查询不再每次建立整个兄弟表。详细操作、报告字段与摘要协议见[有效快照手册](EFFECTIVE_CONFIGURATION_SNAPSHOTS.md)。

| 检查 | 当前结果 |
| --- | --- |
| 专用模型 | 普通和严格C++11 ASan/UBSan/LeakSanitizer各18 passed/0 failed（init/cleanup加16场景）；新增5场景覆盖64位hex/空字符串/布尔/共享来源、持久往返、语义相同但来源/写法不同、多实例受影响、空对象、篡改/重复键/坏UTF-8/旧来源/非法范围及4101标量/5000字节字符串。build-xcfg-shared及/tmp/getdaq-xcfg-shared-asan的results-effective-snapshot-tests-current.txt |
| 完整Qt | 58 passed/0 failed；只读连接模式导出/审阅、旧来源拒绝，以及实际导出按钮/文件对话框/后台完成与打开其他文档的确定次序通过。build-suite-tests/results-effective-snapshot-tests-current.txt；offscreen模拟平台，无Windows Kit/真实窗口验收 |
| 新CLI | 12实际进程场景通过；Python手工给定有效树/类型/值/规范范围，独立计算SHA-256，与程序输出一致。包含仅写法/来源变化、具体默认实例变化、篡改/未知格式/重复键/非法范围失败不发布、已有快照/报告不覆盖。results-effective-snapshot-cli-current.txt与effective-snapshot-cli-current-evidence/；没有硬件或原始数据 |
| 旧入口回归 | 系统工件36进程、AGET单片/整板、CoBo读出和MuTaNT50合同通过；results-effective-snapshot-{system,aget,readout,mutant}-cli-current.txt。原生loader因严格JSON源码变化已重建；未传硬件夹具，非板端测试 |
| 工程/解析核心 | 根qmake四程序通过，三Qt构建/来源/程序凭据核对；纯标准C++11系统工件/默认严格JSON既有CTest1/1通过。系统Qt未插桩，无新增运行库；配置工具只需既有QtCore，原始receiver仍不进入这条路径 |

本阶段独立归档为build-xcfg-shared/validation-effective-snapshot/，打包入口archive_effective_snapshot_validation.py要求同版源/生成输入/程序与结果、实际CLI根及摘要核对后新建，拒绝覆盖旧包；files-sha256.json用于历史核对。此前override/range/shared-class归档保持历史字节，不以旧摘要证明后来的工作树。

捕获前后来源核对不是跨进程锁；快照和报告摘要是内容核对，不是签名、授权或实际加载凭据。本阶段未新增硬件/ECC命令或MFM传输，未重跑接收/告警板端流程或DAQ吞吐。旧Spirit/完整CCfg运行对照、完整对象物化/结构编辑、远端不可变发布/实际加载绑定、目标平台/实机/断电仍待完成。

## 43. 本地不可覆盖配置发布包与固定ECC绑定（2026-10-07）

新增纯标准C++11 ConfigurationPublication清单/字节验证和Qt低频SystemConfigurationPublisher。后者核对已加载源与profile，生成有效快照/完整system工件，再核对源字节并保存新目录；原source/include/profile不重写，清单最后写。Unix文件0400、目录0700及文件/权限/目录fsync；失败保留现场，已有目录不续写/覆盖。大配置只计算一次有效树，编译后仍逐字节核对来源，不用重复快照计算代替来源检查。生成和读取清单均限1MiB；超长来源集合在创建目录前拒绝。

ConfigWindow增加后台Build configuration publication入口，连接期间可生成本地包；配置切换后明确旧任务的归属。getdaq-config增加publish-system，原生getdaq-system-config增加inspect-publication。FixedEccConfiguration可选configuration_publication：核对发布目录身份/0700、全部普通只读文件、固定路径/内容和工件/profile/快照来源表，要求原experiment_artifact恰为包内工件。变化保持故障，恢复原字节/权限/目录不清同一对象门槛；父NativeEcc快照加入发布清单摘要，实际加载标记仍false。

| 检查 | 当前结果 |
| --- | --- |
| 纯工件/发布核心 | 普通及严格C++11 ASan/UBSan/LeakSanitizer CTest各1/1；清单codec/角色文件名/源字节/参数/profile/快照来源绑定、重复键、越界来源/清单总大小、丢文件/变更拒绝。build-experiment/results-publication-core-tests-final.txt与/tmp/getdaq-publication-core-asan/results-publication-tests-final.txt |
| 发布器QtCore | 普通/严格C++11 ASan/UBSan/LeakSanitizer各5 passed/0 failed（init/cleanup加3场景）；实际GET配置生成、有效快照重新读校验、不覆盖、原文变化拒绝和坏profile不建目录。build-publication-qtcore及/tmp/getdaq-publication-qtcore-asan的results-publication-tests-final-current.txt |
| 完整GUI | 59 passed/0 failed；连接只读模式下实际配置本地包生成、已有目录拒绝和旧入口回归。build-suite-tests/results-publication-tests-final-current.txt。此处发布用例调用页面的公共方法；后台快照/配置切换实际按钮测试保留，未新增人工部署/实机窗口验收 |
| 实际离线进程链 | 普通ECC与插桩ECC各14场景，共28进程；真实配置/profile/include、包内来源字节、无Qt读取、ECC --check-config接受和权限/内容/外部同内容工件拒绝、不覆盖/无清单/格式失败、原文件保持通过。results-publication-cli-final-current.txt、results-publication-cli-final-sanitized.txt与各final证据根。插桩组仅ECC插桩，生成/读取工具为普通程序；没有创建ECC/receiver根或连接板端 |
| 固定后端与协调器 | 普通/严格C++11 ASan/UBSan/LeakSanitizer固定输入专项通过；源权限/原地内容/目录替换及恢复保持、发布摘要进入父快照和actual_loaded_configuration_verified仍false。最终插桩normal-0九命令正常流程回归通过；数据生命周期是明确替身，不是实际receiver/封存/生产网络证明 |
| 旧CLI与构建 | 系统工件36、AGET单片/整板、CoBo读出、MuTaNT50合同和模型18项回归通过；根qmake四程序、四Qt构建/来源/程序凭据与69次新原生严格插桩编译核对。旧Ice/GET/bzip/yaml静态库及系统Qt未整体插桩，无新增第三方库或receiver依赖 |

详细操作、字段、路径/权限、失败与真实加载边界见[配置发布包](CONFIGURATION_PUBLICATION.md)。本阶段独立证据入口archive_configuration_publication_validation.py保存四Qt构建/输入/结果、新原生程序/编译命令、两实际CLI根及固定输入/九命令范围，排除凭据；归档在build-suite-tests/validation-configuration-publication/，manifest只能证明保存的历史字节，不能证明后来工作树。旧264/231/257/254包保持历史内容。

无Qt读者核对工件和发布文件字节、参数/profile/来源表，明确effective_snapshot_semantics_recomputed:false，没有独立重算Qt有效快照算法；生产者已校验有效快照。发布摘要不是签名/授权、跨主机上传完成或板卡实际读回。远端受控上传/版本选择、实际控制权/加载、旧Spirit/完整库、结构物化/编辑、目标平台、断电和持续DAQ仍待验收；本阶段未改原始数据通路。

## 44. 无Qt有效快照语义核心与独立检查工具（2026-10-07）

新增EffectiveConfigurationRecord共享标准C++11核心，检查结构/类型/规范值、严格范围成员、来源/行号、具体树父对象、排序/重复及预算，并重算schema1有效/完整记录摘要。NormalizeConfigurationValue/EncodeConfigurationRange也归入已有无Qt范围核心；Qt编辑器和原生发布/ECC共用它们，删除Qt适配中的第二套摘要/校验实现。schema1沿用UTF-16码元路径排序，补充平面与私用区的顺序有专项检查，不能以UTF-8排序替换而声称旧版本相同。

VerifyConfigurationPublication现在重新校验快照语义，原生inspect-publication输出effective_snapshot_semantics_recomputed:true；同时增加inspect-snapshot。新getdaq-config-check提供snapshot/publication/experiment三个离线命令，CMake或根qmake的configcheck子工程可构建，仅链接系统C++运行库，无Qt/Ice/yaml/curl/Boost。根工程当前五程序；原始receiver和硬件执行命令未改。语义检查不重新解析原XML/旧CompoundConfig，也不证明硬件实际加载。

| 检查 | 当前结果 |
| --- | --- |
| 纯核心 | 普通/严格C++11 ASan/UBSan/LeakSanitizer CTest各2/2（effective-configuration-record、system-experiment）；独立Python空树摘要常量、64位/2^53、hex/real/bool/string、同义范围/原文版本区别、Unicode UTF-16顺序、未知来源/行号/父对象/重复/坏UTF-8及内部摘要拒绝。build-experiment和/tmp/getdaq-publication-core-asan的results-effective-record-tests-current.txt |
| 范围回归 | 普通/严格C++11 ASan/UBSan/LeakSanitizer各1/1，build-configuration-range及/tmp/getdaq-configuration-range-asan的results-effective-record-range-tests.txt；旧原函数体参考属于此前独立历史证据，本轮未重跑旧Spirit/全库 |
| Qt模型/发布器/GUI | 模型普通/严格插桩各19项、发布器普通/严格插桩各5项（均含init/cleanup）、完整GUI59项通过；五Qt同版输入/程序核对。results-effective-record-tests-current.txt分别在相关构建目录；系统Qt未插桩，GUI为普通offscreen回归 |
| 无Qt检查器 | 普通/严格插桩各16实际进程，共32；读取此前Qt冻结schema1快照和两个历史发布包，保持摘要，拒绝规范值/来源/行号/父对象/范围/重复/摘要/坏UTF-8，既有工件检查通过。results-effective-record-cli-{current,sanitized}.txt及两个CLI根；ldd确认无Qt/Ice/yaml/curl/Boost动态依赖 |
| 实际发布链 | 普通ECC/插桩ECC各20进程，共40；保留原14场景，再增加3种快照语义篡改分别经无Qt读者与ECC拒绝。外层文件/清单SHA及固定ECC期望SHA已按篡改后的字节更新，内部规范值错误、错误声称摘要、范围不含值仍因Snapshot语义校验拒绝；不是仅靠外层字节变化拒绝。results-effective-record-semantic-publication{,-sanitized}.txt。后一组仅ECC插桩，生成/读取工具为普通程序；不连接板卡或创建ECC/receiver根 |
| 固定后端与旧入口 | 普通/严格C++11 ASan/UBSan/LeakSanitizer固定输入/文件身份/权限/内容恢复保持通过；本机Ice夹具未执行硬件动作。原生新源71次严格插桩编译/当前SHA核对；旧系统36/AGET/读出/MuTaNT50合同、五程序qmake构建通过，旧静态依赖未整体插桩 |

代码/操作/摘要协议见[有效快照](EFFECTIVE_CONFIGURATION_SNAPSHOTS.md)与[发布包](CONFIGURATION_PUBLICATION.md)。archive_effective_record_validation.py新建build-suite-tests/validation-effective-record/，保存五Qt输入/程序/生成源、纯/原生程序/编译和两类CLI根及固定故障保持范围，排除凭据。此前504发布包与264/231/257/254配置证据包保持原字节，只作历史核对，不能证明后来的工作树。

本阶段完成“结构化快照的支持语义重算”，未完成原XML/旧Spirit/全库独立重新解析、完整结构物化/编辑、远端受控上传/版本选择、硬件控制权与真实加载核对；平台/实机/断电/持续大流量仍待验。普通与插桩都未因此宣称receiver或生产网络流程新验收，actual_loaded_configuration_verified/hardware_loaded_verified保持false。

## 45. 独立配置仓库、分块上传与未知提交观察（2026-10-07）

新增ConfigurationRepository标准C++11/POSIX核心，专门存放配置发布包，独占写者锁、稳定仓库身份、32KiB内容摘要块和只读版本。提交先完整重验文件/工件/profile/来源及有效快照语义，绑定原计划意图，登记收据最后发布；重复同包返回原收据，同计划中断允许核对已有文件后继续。来源/对象/凭据损坏恢复仍保持同一对象故障，但该保持不等于跨重启永久门槛。版本列表只作索引（content_verified:false），单版本查询完整重验；hardware_loaded_verified始终false。

getdaq-config-service只用Qt Core/Network/Concurrent提供低频认证HTTP，后台一次执行，不排队；getdaq-config-upload及独立GUI仓库窗口上传/列举/检查发布包。它们不操作板端、不执行receiver或原始MFM、不自动选择ECC固定版本，无新增第三方库。仓库核心/CLI无需Qt/Ice/yaml；GUI保持C++11、Qt5.12及以上API，Windows Qt5.14.2 MinGW仍需真实Kit验收。

本阶段的真实连接故障暴露Qt默认QByteArray POST能够由网络层重发。共享HttpClient改用禁止缓冲、拒绝reset/seek的单遍顺序上传并设Content-Length，避免提交成功但回复丢失时重复发送；HTTP错误保留数字状态，代理改写reason phrase不把404误判为网络未知。仓库只在明确404后继续原计划，其他失败要求观察原版本；上传块回复也检查stored:true。ECC既有丢回复测试加强为实际POST数恰为1，保留服务端原ID去重要求。

验证：普通及严格C++11 ASan/UBSan/LeakSanitizer核心测试分别通过写者锁、块摘要/路径/长度、重复块/登记、版本重验/重开、同计划中断恢复、错误意图以及对象/版本/凭据故障保持。普通及插桩实际HTTP独立服务/Qt CLI测试通过完整冻结包上传、登记回复实际丢失后的原版本观察、重复上传/收据不变、索引/完整查询、认证/身份/64KiB请求边界、根写者锁和内容损坏拒绝。故障代理在第一次commit实际返回200后断开，两次客户端运行合计只有一次POST commit；同一物理主机，无硬件/receiver/原始DAQ执行。

共享HTTP改动后的完整GUI60项、ECC客户端普通/插桩各16项、Run工作流普通/插桩各11项通过；GUI仓库独立入口及缺少凭据拒绝有实际窗口用例。上次完整GUI出现一次simulatedLifecycle未结束，保持失败记录；同版单项复测及最终完整60项通过，不把失败记录改成成功。仓库CMake普通/插桩各26次严格C++11编译有输入/程序/生成文件及命令凭据；系统Qt未插桩。普通/插桩服务使用原生配置核心，不引入DAQ载荷通道。

证据入口：tests/configuration_repository_core_test.cpp、configuration_repository_core_runner.py、configuration_repository_network_test.py、build_configuration_repository_validation.py与archive_configuration_repository_validation.py；build-config-repository/results-repository-core-final.txt、results-repository-core-sanitized-final.txt、results-repository-network-final.txt、results-repository-network-sanitized-final.txt，以及各Qt构建目录results-repository-tests-final.txt。本阶段validation-configuration-repository/独立保留程序、源码、构建凭据和实际根并排除认证文件，归档检查命令见[仓库手册](CONFIGURATION_REPOSITORY.md)。既有validation-effective-record/等包保持历史字节，不能拿旧证据当作后来工作树证明。

尚未实现ECC受控版本选择/切换、实际板端加载核对、真实控制权、仓库受控清理/迁移和总磁盘硬限额；版本文件和失败临时文件不属于约1GiB完整块预算。尚未验证真实断电、物理多主机、目标平台或持续大流量。用户全面重构目标仍在进行，不能将本阶段完成等同整套完成。

## 46. 审阅后的ECC配置选择与启动绑定（2026-10-07）

ConfigurationSelection为标准C++11严格协议，仓库登记编码、Qt导出和固定ECC复用。完整版本查询后，GUI或上传CLI可将原仓库身份/登记收据字节摘要、清单路径/摘要和参数/工件/快照摘要保存为新的只读选择文件。getdaq-ecc-select复用本地原生兼容组件，只离线核对并建立新部署配置；选择/收据/包持续进入FixedEccConfiguration的身份/权限/字节检查，父快照加入选择/仓库/登记摘要。保留模板audit_root，旧历史与新绑定不符时启动仍拒绝，不自动换根清门槛。没有旧可选字段时保留原配置兼容。

认证GET /v1/configuration展示该进程启动时接受的实例/快照/固定输入及选择缓存；input_binding_live_verified:false和hardware_loaded_verified:false明确不是当前文件健康或板端加载证明。原命令入口继续完整输入检查。标准检查器另可selection/registration检查记录，valid_record:true同时publication_checked:false，不把记录格式当完整包通过。

纯协议普通CTest1/1和严格C++11 ASan/UBSan/LeakSanitizer通过字段/类型/false范围、原收据字节、仓库身份、清单路径/摘要和工件绑定拒绝。原生固定配置普通/插桩通过生成新部署输入、缓存、完整父绑定、重复目录拒绝、只读约束和恢复故障保持；保留已使用模板审计根时拒绝不匹配历史，原快照保持，不算硬件UNKNOWN恢复。

实际仓库→Qt上传/选择导出→原生离线生成→ECC --check-config→真实ECC配置缓存HTTP，普通及插桩两组通过。核对原收据字节、选择文件0400/不覆盖、未登记版本拒绝、离线不建控制根、HTTP实例/快照/固定输入绑定与所有加载false。场景为同一物理主机、合成首次部署发布包，无硬件命令/receiver/MFM；当前不推广成真实运行后切换或物理多主机验收。普通/插桩真实旧冻结发布包仓库回归另通过提交ACK实际丢失后观察、一次commit、重复登记和边界。

普通仓库回归曾在发送完整超限体后读413时收到TCP RST：服务器按声明长度提前拒绝并关闭，未读数据会影响返回可见性。当前用只发送超限Content-Length头、发送体之前读取413，明确证明声明长度拒绝；不是放宽接受上限或把RST当成功。原生块大小拒绝和真实32KiB配置上传保持。

完整GUI60项和当前操作台构建通过；Qt上传普通/插桩及原生仓库普通/插桩有同版凭据（仓库各27严格编译）。新原生ECC/选择工具/服务及夹具共74次严格C++11插桩编译，系统Qt和旧Ice/GET/bzip/yaml静态库未整体插桩。插桩运行覆盖固定选择、纯协议、实际选择全链和仓库回归；没有本阶段整个历史DAQ系统完整回归或实机加载证明。

证据：源码docs/ECC_CONFIGURATION_SELECTION.md、tests/configuration_selection_test.cpp、fixed_ecc_configuration_test.cpp、ecc_configuration_selection_integration.py；build-ice-compat/results-selection-system-final.txt、results-selection-system-sanitized-final.txt、results-selection-core-current.txt、results-selection-core-sanitized-current.txt及build-config-repository/results-selection-regression-final.txt、results-selection-regression-sanitized-final.txt。本阶段独立validation-configuration-selection/保留当前源码/程序/编译命令/实际根，排除认证文件和值；此前3490/563/504包保持历史字节。

已完成的是审阅选择、完整固定启动输入及缓存证明；在线受控切换、真实控制权/实际加载、旧UNKNOWN或运行后部署迁移/恢复仍未完成。仓库GC/总资源硬限额、完整旧配置库/结构编辑、目标平台/断电/持续DAQ验收亦未完成；原始数据依赖和热路径不变。

## 47. ECC配置查看、Start前记录与Run恢复绑定（2026-10-07）

新增共享纯C++11 EccControlConfiguration协议，原生服务编码、Qt客户端和Run恢复共用；严格核对字段、类型、64KiB预算、实例/父快照/固定输入/参数/工件及可选选择一致。schema1拒绝把input_binding_live_verified或hardware_loaded_verified伪造成true。同一已核对实例中启动选择发生变化后保持客户端故障，恢复原值不清门槛；只读GET不授予控制权或清UNKNOWN。

主窗口原生模式新增Accepted ECC configuration只读表格与状态栏。显示接受的版本/工件，明确实时文件健康未检查、硬件加载未核对；断开后清除旧显示。native_configuration_status默认false保留旧模式兼容，启用后Start前必须有绑定启动记录；缺记录不预占Run、不POST Start。记录在请求意图前进入journal.native_configuration，最终保存ecc-configuration.json；本地恢复继续核对相同实例/快照/会话/PID/dataset，并核对同一启动配置，不重放Start，不解释为硬件恢复。

普通纯协议CTest1/1及严格C++11 ASan/UBSan/LeakSanitizer通过；客户端普通/插桩各18项、Run流程普通/插桩各14项（包括服务收到Start时检查配置已持久化）、完整普通GUI61项（包含实际只读表格/false提示/断开清除）和当前操作台/探针构建通过。GUI完整组未整体插桩；系统Qt和旧Ice/GET/bzip/yaml静态库也未整体插桩。

实际Qt→ECC→两数据服务→两独立receiver的Ice1.0/1.1，两普通及两新控制代码插桩流程通过并独立复核。合成审阅包/收据/选择进入固定服务，GUI开始前记录并正常完成九命令、Pause/Resume、Stop/Breakup/Reset；核对实例/输入/参数/工件、选择/清单/收据SHA、旧单系统接受计数、两receiver每个4帧16384字节及EOF/退出/seal。审阅包使用合成来源和空有效树夹具，不是真实XML编译或现场板端加载证明。新ECC/原生夹具/Qt探针/参数协议执行插桩，数据服务和receiver在插桩联调组明确使用普通程序，不宣称所有子进程均插桩。

证据入口：源码docs/ECC_CONFIGURATION_VIEW.md，tests/ecc_protocol_test.cpp、test_ecc_system_client.cpp、test_native_gui.cpp、test_getdaq.cpp、native_gui_probe.cpp、ecc_network_service_test.cpp及check_configuration_view_network.py。build-ice-compat/results-configuration-view-network-current.txt、results-configuration-view-network-sanitized-final.txt、两份network-check-final记录，各Qt目录results-configuration-view-current/final.txt和纯协议普通/插桩记录。新原生源码74严格C++11插桩编译及同版Qt输入/程序凭据核对；独立validation-configuration-view/保存四实际根、源码/程序/命令并排除认证文件和值，旧1455/3490/563/504包保持历史字节。

范围仍为同一物理主机、合成独立CoBo模式；原始帧由板端替身直接送独立receiver，未加入Qt载荷通道。运行中版本切换、真实控制权/实际加载、硬件UNKNOWN/运行后部署迁移及平台/断电/持续大流量验收仍未完成；本阶段配置缓存不是当前文件健康或完整DAQ系统实机验收。

### 47.1 传输测试磁盘预算与强制清理（2026-10-08）

根据用户的磁盘容量约束，本工作区每轮传输测试所有流合计最多20GiB，测试结束删除合成数据，仅保留小型日志和校验凭据。已经删除此前四个原生接收器负载测试目录中的128个合成`.graw`文件，共136541372416字节（约127.16GiB）；没有删除真实Run或历史源码，四个目录保留独立清理清单。此前超限/未完成负载不算成功吞吐验收。

`receiver_sustained_load.py`现在启动前拒绝超限总量、可用空间不足及同一工作区并发测试，默认总量1GiB；成功、失败、Ctrl+C或SIGTERM后均停止测试子进程并清理本轮生成的原始数据。成功结果在清理完成后才写入。SIGKILL、断电或文件系统错误仍可能留下文件，不能宣称这种异常下也保证自动删除。清理只针对本次新建的私有目录，不能用于清理真实采集。

`receiver_load_safety_test.py`三项测试通过：超限在建目录/启动接收器之前拒绝，接收器启动失败后删除4KiB部分文件，以及中断时清理并拒绝并发第二轮。最终版本另用实际原生接收器经本机TCP传输两流各8MiB，合计16MiB；发送/保存完整SHA、计数、EOF、正常退出及封存核对通过，随后确认16MiB数据全部删除。程序/脚本SHA、命令及结果入口为`build-receiver/results-load-safety-current.json`，日志为`results-load-safety-safety.txt`和`results-load-safety-tcp.txt`。测试没有真实硬件，不提供生产吞吐证明。使用方法与预算说明见[数据服务手册](DATA_SERVICE.md)。

## 48. 原生接收器完整帧复制优化与有界负载验证（2026-10-08）

`MfmStream`新增完整帧同步读取路径：无前一帧残留且当前块包含整帧时，完整校验后直接交给同步Sink；跨块帧继续原有有界重组。Sink指针只在回调内有效，不保留/修改/跨线程使用或重入Feed。小帧批量写盘和在线导出副本仍存在，不称整链零复制；Qt/Ice等没有进入原始数据通道。

| 检查 | 结果与范围 |
| --- | --- |
| 普通/插桩构建 | 各8次严格C++11编译、18构建输入；程序/命令/依赖SHA凭据，原生程序没有Qt/Ice/yaml/Boost/ROOT/curl动态依赖。系统库和联调用的普通sentry不属于整体插桩 |
| 核心检查 | 普通CTest1/1和ASan/UBSan/LeakSanitizer通过；两字节序/每个分割点/多帧、同步借用指针、混合重组、40组畸形头两路径拒绝、合法前缀保留、精确保存/轮转/队列 |
| 实际TCP集成 | 普通及插桩接收器各五组检查通过：分段/字节序/完整文件/导出、慢导出隔离32MiB完整SHA、截断和错误长度失败、目录锁与封存归档。归档使用普通Qt sentry，临时数据已删除 |
| 有界并行写盘 | 两接收进程各512MiB、131072帧、8文件，合计1GiB；发送/文件完整SHA、字节/帧数、自然EOF/正常退出/fsync/seal全部核对，每进程峰值RSS20724KiB（约20.24MiB）。这是同主机合成4KiB帧，未连接硬件 |
| 编号轮转 | 单流12MiB生成十二文件，按数字文件编号拼接SHA，避免字符串排序将10/11放在2之前；完整计数/封存核对通过 |
| 强制清理 | 并行1GiB及轮转12MiB全部合成`.graw`删除，有独立清理凭据；当前脚本三项容量/失败/中断及并发检查通过，每轮合计20GiB硬上限保留 |
| 内存分帧比较 | 相同基准、原归档源码SHA/严格C++11 Release、四输入每组交替三次；完整4KiB/64KiB帧中位耗时比7.70/89.87，两个跨块输入约1.01/0.99。只检查头和一个载荷字节，不经过网络/磁盘，不按生产吞吐解释 |

构建器`tests/build_receiver_validation.py`保留当前源码前后SHA、CMake编译命令和四执行程序依赖；比较器`tests/receiver_framer_comparison.py`使用`validation-remote-ecc/verified-sources/`中优化前两个原文件，原/新基准和输入SHA保留。此前首条CTest命令用了本机旧CTest不支持的`--test-dir`，仅输出No tests were found；随后在构建目录运行，真正1/1通过，最终日志以实际执行为准。

证据入口为`build-receiver/results-core-performance-current.txt`、`results-integration-performance-current.txt`、`results-performance-safety-current.txt`、两份`results-load-performance-*.txt`、`framer-comparison-current/result.json`及各负载私有根的`result.json`/`payload-cleanup.json`；插桩核心/联调日志原位于`/tmp/getdaq-receiver-performance-asan/`。独立`build-receiver/validation-performance/`保留普通/插桩程序、原/新输入、实际小型元数据与结果，不保存原始载荷或认证值。归档检查同时核对源码SHA和记录间的计数/封存/删除关系；历史模式只证明归档，不证明后来工作树。

范围为Ubuntu20.04.6/GCC9.4、同一物理主机、合成帧。约49GiB主机内存大于用户20GiB预算，未完成超过缓存规模的持续磁盘验收。RSS不是全部固件/输入下的上界，发送时间包含生成/SHA/背压，父进程观察尾部不等于精确fsync时间。真实多CoBo/固件样例、磁盘与网卡长期性能、断电和目标平台仍未验收；没有新GUI功能或整套历史回归声明。原理、逐步命令及容量说明见[接收器性能手册](RECEIVER_PERFORMANCE.md)。

## 49. 接收器单文件上限、实际短写与错误封存门槛（2026-10-08）

从第48节不可变包取出原接收器，实际设置RLIMIT_FSIZE=2048后发送三帧12288字节，观察内核SIGXFSZ终止、退出-25、仅2048字节前缀且没有seal。新main忽略SIGXFSZ默认终止，使write返回EFBIG并进入已有错误路径；不提高系统限制、不新增数据复制或依赖。

普通与严格C++11 ASan/UBSan/LeakSanitizer接收器各四项通过：EOF批量刷新2KiB上限、达到批量阈值的4KiB短写、768KiB大帧直接写4KiB短写、单4KiB帧恰好上限成功。三失败最终退出1、明确File too large、stream_sealed:false、无seal；文件实际前缀与发送字节完全一致。成功退出0、接受/封存计数与durable seal核对。测试每个原始文件最多4KiB，全部删除；没有填满磁盘，没有以模拟故障伪称实际ENOSPC/EIO/fsync/断电验证。

源码入口`apps/datareceiver/main.cpp`、`tests/receiver_file_limit_test.py`；普通/插桩构建各8次严格C++11编译、18输入SHA和程序凭据，实际结果为`build-receiver/file-limit-before/`、`file-limit-ordinary/`、`file-limit-sanitized/`私有根及两份`results-file-limit-current.txt`。独立`validation-file-limit/`保留前后程序、源码/构建命令及元数据，不存.graw或认证值；旧`validation-performance/`历史复核保持。只验证这一主程序改动和四内核故障/边界场景，不声称本阶段重跑完整GUI或所有旧DAQ回归。

范围为Ubuntu20.04.6/GCC9.4、同一物理主机、合成数据。accepted_file_bytes/frames可包含写盘前批量缓存，此时sealed_bytes仍0；不能将接受计数当持久保存或成功Run。错误文件在产品中保留，测试删除只针对合成数据；详细原理、操作和未验边界见[写盘失败手册](RECEIVER_WRITE_FAILURES.md)。

## 50. 配置结构编辑、完整审阅与当前有效子树物化（2026-10-08）

新增`XcfgStructureEditor`与独立Qt审阅窗口，十种操作包含新增对象/参数、删除来源声明、改ID/名称、深复制、同父前后移动、标量类型/单位/范围以及主文件具体对象的有效子树展开。源文件保持原文，XML属性按引号边界扫描；不因另一属性值里含id/include文字而误改。候选复用XcfgDocument、类型/共享范围和EffectiveConfigurationSnapshot语义；普通公开捕获仍检查磁盘，只有内部已核对来源的虚拟候选可共用私有捕获入口。

所有已加载来源、默认声明类型/范围及对象/include碰撞先检查。预览保存原请求、原/候选字节、前后快照和完整差异；Apply重新生成并逐项核对，QSaveFile单文件替换后重载，再核对实际快照与审阅结果。保存后核对失败明确报告已保存，不谎称未写；不提供跨文件锁/目录fsync/断电保证。物化要求完整有效摘要不变，保留未选中的模板及当前已有实例，不自动生成未来默认实例或自动部署硬件。

| 检查 | 最终结果及范围 |
| --- | --- |
| 结构/既有模型普通 | 29 passed、0 failed、0 skipped，约0.96秒；含初始化/清理。十种操作、未知属性/注释/CDATA保留、精确XML转义、排序/属性的全共享影响、根/碰撞/跨父移动拒绝、预览篡改/来源改变及默认/include/自闭合/空值/空对象物化 |
| 模型插桩 | 严格C++11 ASan/UBSan/LeakSanitizer29 passed、0 failed、0 skipped，约4.71秒；系统Qt未整体插桩 |
| 完整普通GUI | 64 passed、0 failed、0 skipped，约27.13秒；含三个实际结构窗口场景，预览前不写、差异表、成功本地化/共享属性、输入改变撤销Apply、重开门槛和连接模式只读 |
| 当前构建 | 模型普通/插桩、完整GUI、实际操作台、配置CLI五构建有同版源码/程序/Qt生成源/Makefile/命令凭据；合并206构建输入SHA核对，QtAPI仍为5.12+、C++11 |
| 快照CLI | 12真实进程检查，独立手写类型化SHA参考、来源/默认变化、篡改及不覆盖；最后版本单独证据根snapshot-cli-final-evidence/ |
| 系统工件CLI | 36真实编译/原生读取/绑定检查，原始用户GET配置字节保持；调用既有普通getdaq-system-config，没有板端执行/receiver/DAQ载荷 |

最初一个物化模型用例调用ExperimentReader时没有填写原参数的mV期望单位，触发Unsupported unit；修正测试的期望单位后继续验证，未放宽读取器。后续属性/复制/排序和保存后检查均在最终源码重建后执行，最后29/64/12/36结果与源码SHA绑定；不把中途结果或旧二进制当最后版本证明。

源码入口`common/config/xcfgstructureeditor.*`、`effectiveconfigurationsnapshot.*`、`apps/envoy/configwindow.*`和两Qt测试；日志`build-xcfg-structure/results-structure-current.txt`、`/tmp/getdaq-xcfg-structure-asan/results-structure-current.txt`、`build-suite-tests/results-structure-gui-current.txt`及两份results-structure-*-cli.txt。本阶段独立`build-xcfg-structure/validation-structure/`保存五构建/程序、既有原生读取器、编译输入与生成文件、当前CLI记录和三份旧结构源码参考，不保存DAQ原始数据或凭据；历史模式不检查后来工作树。此前配置/数据验证包保持历史字节。

范围为Ubuntu20.04.6、Qt5.12.8、GCC9.4，本机配置与模拟GUI。没有本阶段实机/ECC部署/原始数据或持续吞吐验证；Windows Qt5.14.2 MinGW、Ubuntu24和macOS仍待验。旧Spirit全库运行、数据库/网络include、全部撤销命令、跨文件结构迁移、在线受控切换及实际加载仍未完成；十种操作不是全部旧CompoundConfig替换。详细逐步操作、共享属性例子、物化边界与错误说明见[结构编辑手册](CONFIGURATION_STRUCTURE_EDITOR.md)。

## 51. 硬件描述共享纯C++模型、严格JSON迁移与GUI只读查看（2026-10-08）

硬件描述类型、读取、校验与HardwarePolicyJson移至common/hardware，原生hardwareclient引用相同共享类型，旧apps/hardware/hardwaredescription.cpp保留兼容占位。共享模块只用标准C++11和既有StrictJson，移除这一描述读取路径的yaml-cpp依赖；其他原生拓扑/日志及Ice兼容依赖没有因此全部移除。整数/布尔要求真正JSON类型，地址仍为精确字符串uint64，拒绝YAML/多文档、重复键、未知字段、溢出、UTF-8与边界错误；合法编译工件的规范权限字节保持。

HardwareDescriptionWindow接配置窗口，连接模式也能只读查看本地工件。设备/寄存器/位域按展开创建，显示精确基址+偏移、位宽和寄存器/位域合成的主机只读策略。两个摘要分别标识文件字节和规范权限，不证明板端布局/实际加载或当前文件健康。没有Ice客户端/寄存器读写/原始帧API，失败先清旧模型。

| 检查 | 结果与范围 |
| --- | --- |
| 纯描述模型 | 普通与ASan/UBSan/LeakSanitizer通过精确uint64/64位域、地址回绕、严格类型、未知/重复键、溢出、UTF-8及JSON-only。各21严格C++11编译有输入前后SHA/程序/依赖凭据；独立程序没有Qt/Ice/yaml/Boost/ROOT/curl动态依赖 |
| 原解析器对照 | 从validation-configuration-view的原源码/头文件及原JsonString/ParseUnsigned函数核对SHA后编译YAML描述解析器；两完整CoBo/MuTaNT JSON及三原GET XML当前编译工件的规范权限JSON逐字节相同。是项目迁移前解析器对照，不是全GET原软件运行 |
| 描述实际联调 | 三原样本编译/严格读取、11设备本机空节点Create、读回/权限绑定、漂移拒绝和不覆盖通过；使用普通原生工具和软件Ice夹具，不接真实板卡 |
| 原生客户端 | 完整普通Ice1.0/1.1各八组PASS，包含元数据/寄存器/字段I/O、AsAd/AGET/读出/Configure/Prepare子序列、Create/持久门槛和未知不重放。不是全原生代码插桩运行声明 |
| QtGUI | 当前普通完整65 passed、0 failed、0 skipped，约22.98秒；新增实际只读窗口检查最大地址、展开不重复、位域继承只读、不改源、失败清旧显示及连接模式查看。当前操作台/配置CLI同版构建通过；系统Qt未整体插桩 |

证据为build-hardware-description/results-core-current.txt、/tmp/getdaq-hardware-description-asan/results-core-current.txt、results-legacy-comparison.json、compiled-reference/results.json、results-description-integration-current.txt及build-ice-compat/results-hardware-model-current.txt；Qt完整结果build-suite-tests/results-hardware-view-current.txt。既有NativeEcc/工具四程序重新构建，源码/程序记录native-current-receipt.json只声明普通构建；插桩范围限新共享描述模块及其标准依赖，不扩大为旧Ice/yaml/Qt库。

本阶段没有DAQ传输文件或真实硬件；本机Ubuntu20.04.6/GCC9.4/Qt5.12.8。源码输入、程序、旧解析器、比较/实际联调记录将独立归档，不改旧299/79/81证据字节；历史检查只证明归档，不能证明后来工作树。GUI现场元数据、实际加载/控制权、目标平台/断电/持续性能及其他旧数据模式仍未完成，完整操作与限制见[查看手册](HARDWARE_DESCRIPTION_VIEW.md)。

## 52. 原生拓扑/日志严格JSON迁移与yaml依赖移除（2026-10-08）

alignmenttopology和eccjournal改用共享标准C++11 StrictJson；加上第51节描述迁移，原生硬件/ECC可选构建不再编译/链接yaml-cpp。Qtenvoy真实YAML读取保留本地库，旧yaml源码/历史CMake入口保留；不把移除JSON入口依赖误称整个工具集无yaml。拓扑只接受真正JSON整数/布尔/字符串，日志seq仍为字符串，状态/命令等为真正整数；原写入器字节、锁、逐条fsync、scope/连续序号/转换/UNKNOWN及只读门槛保持。

| 检查 | 最终结果与范围 |
| --- | --- |
| 独立日志/状态普通及插桩 | 两组各24严格C++11编译、61构建输入前后SHA/程序/依赖核对；日志、状态矩阵、只读CLI各通过。包含SIGKILL、重开需核对、实际短写/锁/文件替换/坏类型/重复键且原件不修改；独立程序无Qt/Ice/yaml动态依赖 |
| 旧日志读取器对照 | 从第51节不可变原源码编译YAML日志读者，同一规范历史文件的inspect输出逐字节一致，文件SHA不变；最终程序SHA重新匹配。不是任意手写YAML/错误类型兼容 |
| 拓扑边界 | 普通/原生插桩均通过：准确绑定、编码/计时/ID/布尔类型、重复/未知键、YAML/多文档/注释拒绝；负向夹具在描述读取前拒绝 |
| 固定配置 | 普通/原生插桩均通过发布/选择/输入绑定、离线部署/缓存、请求去重/owner、文件恢复故障保持与重开门槛；没有硬件执行 |
| 普通系统对齐 | 完整Ice1.0/1.1各成功/故障组通过，三机箱AMC/掩码/主从顺序及预检查/UNKNOWN不恢复保持；本机软件板端 |
| 实际网络 | 普通normal-0/1与新控制代码插桩normal-0/1四完整ECC/两数据服务/两receiver流程通过。两种编码都是同一物理主机，未接真机；插桩组数据服务/receiver明确普通程序，不夸大覆盖 |
| 载荷清理 | 普通和插桩网络组各生成131072字节，校验后全部.graw删除，保留逐文件SHA/字节清单与原seal。每轮远低于20GiB，归档不存原始载荷 |
| 编译/链接依赖 | 当前可选原生CMake数据库305编译配方无yaml-cpp源；硬件/日志/ECC相关链接配方和执行依赖无yaml。新原生插桩75次严格C++11编译，四个旧Ice/GET/bzip静态依赖SHA及命令保留，无yaml头/宏/库 |

最初重新配置暴露ECC子目录先引入共享实验核心、兼容根又无条件引入导致CMake同名目标；兼容根改为已存在则复用后，独立/整合构建通过。新topology-json-tests目标首次请求前需要显式重新配置，之后成功构建。两次固定/对齐夹具调用初始参数错误在入口拒绝，正确参数最终重跑；未把未启动或入口错误当回归成功。

日志/状态最后严格C++11构建之后又执行最终普通/插桩三组，证据为build-ecc-json及/tmp/getdaq-ecc-json-asan的results-json-*-current.txt。原生固定/拓扑/对齐在build-ice-compat的results-native-json-*-current.txt；新原生插桩程序/源码/命令位于/tmp/getdaq-native-json-asan，75构建输入与四依赖记录；新拓扑专项另有两条严格编译/链接命令。实际四网络根在native-json-network-evidence/与native-json-network-sanitized-evidence/，两目录的payload-cleanup.json确认删除。

独立build-ecc-json/validation-native-json/保存当前源码、普通/插桩日志与新控制程序、旧日志阅读器、实际网络及固定配置小型证据，排除认证文件/实际认证值和.graw；旧376/299/79/81包保持历史字节。历史模式只复查归档，不证明后来工作树。

范围仍为Ubuntu20.04.6/GCC9.4、本机软件夹具与原生流程；没有本阶段GUI全套、真实板端控制权/加载、完整硬件恢复、目标平台、断电或持续吞吐验收。新代码插桩不代表旧Ice/GET/bzip/系统Qt整体插桩。字段类型、依赖分工、逐步复查和日志不修复原则见[原生JSON迁移手册](NATIVE_JSON_DEPENDENCIES.md)。

## 53. 原生FDT输入与ZBUF线上TCP兼容（2026-10-08）

核对收到的DataRouter/FdtDataReceiver/FdtDataSender/ZBufConnection源码后，接收器加入明确的`--input-protocol TCP|ZBUF|FDT`，不自动探测。ZBUF线上原始字节与TCP相同；FDT只在MFM边界处理4字节0心跳/全FF GOODBYE，不写盘、不导出，不过滤帧内相同字节。原生数据程序没有新库依赖。受管理身份及ECC固定计划保持TCP，旧FDT stop断开与普通TCP stop保留连接的差异仍须单独迁移。

| 检查 | 结果与范围 |
| --- | --- |
| 当前构建 | 普通Release和ASan/UBSan/LeakSanitizer构建各8次严格C++11编译、18输入前后SHA/程序/命令/ldd凭据，均无Qt/Ice/yaml/Boost/curl动态依赖 |
| 分帧核心 | 普通/插桩通过原TCP全部检查及FDT两字节序、所有1..524字节分块大小、帧内控制字相同字节保留、下一连接重置、部分控制字EOF和GOODBYE后拒绝；无新增磁盘大文件 |
| 实际TCP专项 | 普通/插桩各12场景：分段/导出、无GOODBYE自然EOF、两连接、只有控制字、GOODBYE后字节、部分字1/2/3、尾帧截断、超限头、原TCP拒绝FDT、ZBUF原始流；非法协议/大小写及受管理FDT在启动前拒绝 |
| 真实结束门槛 | 一个实际FDT sender发GOODBYE后保持socket打开，HTTP证据显示connected=true、sender_eof_confirmed=false、stream_sealed=false且seal不存在；关闭socket后才正常封存。不是用控制字冒充EOF |
| 字节/清理 | 专项每组全部流合计49739线上字节，文件逐字节核对及SHA、计数/seal/退出码检查；每个成功/失败目录finally删除.graw，保留payload-cleanup.json。两组均无原始载荷遗留 |
| 原TCP回归 | 普通/新接收器插桩各五组PASS：两字节序/逐字节分段、多客户端导出、32MiB慢导出隔离完整SHA、错误尾帧、目录锁/离线检查/文件改变拒绝/封存归档。sentry是已有普通Qt程序，未整体插桩；两组顺序运行且临时原始文件全部删除 |

第一轮原TCP回归的前四组通过，最后归档入口因误用不存在的build-sentry/attpc-sentry失败；使用实际build-suite/bin/getdaq-sentry后，普通/插桩分别完整重跑五组通过。保留最初失败日志，不把未启动的归档阶段当通过。归档脚本初版一处Python布尔表达式语法错误经py_compile发现并改正，不影响已执行的接收测试。

日志为build-receiver/results-transports-core-current.txt、results-transports-integration-current.txt、results-transports-raw-regression-final.txt及/tmp/getdaq-receiver-transports-asan对应三日志。实际根在build-receiver/transports-ordinary/与transports-sanitized/。独立validation-transports/保存同版源码/程序/旧传输参考/小型证据；不保存.graw或认证文件，旧性能/文件上限/配置包保持历史字节。

范围仍为Ubuntu20.04.6/GCC9.4、本机合成sender和实际TCP进程；没有真实VxWorks ZBUF、Narval、所有固件或生产吞吐证明。FDT无GOODBYE但帧完整时EOF允许完成，不代表板端未发送事件无丢失。ECC FDT生命周期、ICE入口、真实下游客户端格式、平台/断电/持续采集仍待完成。详细字节排列、计数、停止/重连差异和逐步操作见[输入协议手册](RECEIVER_TRANSPORTS.md)。测试每轮总预算20GiB和生成后删除要求继续保持。

## 54. 导出空闲断连回收与分析客户端重连（2026-10-08）

旧Exporter只有发送排队消息时发现断连，空闲时已关闭的连接仍占名额。当前独立导出线程在接纳新客户端前非阻塞检查/回收结束的单向读取订阅，释放未发送队列；保持八客户端/字节/消息上限，没有改变输入EOF、MFM/长度格式、写盘或Qt边界。客户端写方向EOF或额外输入结束订阅，这个连接规则须向实际分析客户端核对，不宣称旧半关闭客户端兼容。

| 检查 | 结果与范围 |
| --- | --- |
| 原问题复现 | 使用第53节冻结普通receiver，连续关闭八个空闲分析客户端后export_clients仍为8、原始输入为0；第九个真实连接被拒绝。不是以新代码模拟旧错误 |
| 当前重连 | 普通/ASan/UBSan/LeakSanitizer各通过连续二十次空闲退出/重连、八客户端上限/全部释放恢复、写方向EOF和非法输入结束；恢复后一个4096字节完整帧导出4104线上字节、录盘/计数/真实EOF/seal均一致 |
| 当前构建 | 普通/插桩各8严格C++11编译/18输入前后SHA、命令/程序/ldd凭据，原生receiver不增加Qt/Ice/yaml/Boost/curl依赖 |
| 回归 | 两组完整核心、原TCP五组和FDT/ZBUF十二实际场景均通过。原TCP包含32MiB慢客户端隔离全SHA及真实普通Qt sentry归档；新receiver插桩不代表sentry/系统Qt插桩 |
| 清理 | 新重连专项两组各4096原始字节，旧复现0字节；FDT专项两组各49739线上字节；原TCP回归顺序每组约32MiB。自身生成.graw均删除并保存清理清单，各轮总量低于20GiB |

新测试初版将拒绝非法客户端输入只判定为正常EOF，实际内核因未读输入返回RST；测试修正为接受EOF或ConnectionResetError，再用同版测试重新复现旧问题及普通/插桩验证。生产拒绝行为没有为测试放宽，初始观察日志保留。

源码`apps/datareceiver/exporter.cpp`及`tests/exporter_reconnect_integration.py`；普通/插桩构建日志results-export-reconnect-final.txt、results-export-core-final.txt、results-export-raw-regression-final.txt、results-export-transport-final.txt。旧复现results-export-reconnect-before-final.txt。独立build-receiver/validation-export-reconnect/保存同版源码/程序、旧程序/导出参考及小型证据，排除原始载荷/凭据；第53节194文件包现在只作历史复核，原字节保留。

没有真实旧分析客户端、跨物理主机、跨大小端、可靠双写或持续吞吐证明；重连只拿后续完整消息，不重放旧帧。ICE数据入口仍未实现：收到的DataProcessor.ice只有单向processData，没有结束方法，不能把该调用或适配进程退出当作板端自然EOF。详细连接规则、半关闭要求、重连丢弃半消息和复查见[在线导出手册](EXPORTER_RECONNECTION.md)。整套仍未完成。

## 55. 本地配置会话审阅撤销/重做（2026-10-08）

XcfgEditHistory接scalar保存、具体实例覆盖及十种结构保存入口，记录单文件前后准确字节、快照两端摘要和变更前后所有来源守卫。物化后不可达的旧include仍核对；不格式化XML恢复，不把数值相同当来源相同。候选复用原模型、对象/类型/共享范围和快照语义；Apply重算并核对，QSaveFile单文件提交后重新加载实际完整快照。普通scalar/override也加入保存后准确来源字节核对，已保存但失效时清模型/历史、要求重开，不自动回滚。

| 检查 | 最终结果与范围 |
| --- | --- |
| 模型普通 | 44 passed、0 failed、0 skipped，约1.635秒，包含十种结构逐字节往返、混合include值/具体覆盖、不可达include漂移、篡改/过期/外部变更拒绝、失败保留历史和新分支 |
| 模型插桩 | 严格C++11 ASan/UBSan/LeakSanitizer44 passed、0 failed、0 skipped，约3.481秒；系统Qt未整体插桩 |
| 资源预算 | 默认32条/64MiB保留字节，最早步骤裁剪；条数2专项及4MiB保留XML/32MiB历史预算实际检查。预算不是整个RSS，当前模型/候选/Qt容器另占内存 |
| 完整普通GUI | 最后67 passed、0 failed、0 skipped，约23.184秒；两个新增实际窗口场景验证预览不写、差异表、Undo/Redo、结构入口同一历史、Open清历史/拒绝旧弹窗及只读禁用 |
| 五当前构建 | 模型普通/插桩各10源/131输入，GUI86源/207输入、实际操作台83源/204输入、配置CLI34源/155输入；同版输入前后SHA/程序/Qt生成文件/Makefile/命令凭据 |
| 独立CLI | 12实际进程快照检查，手写类型化SHA参考、来源/默认实例变化、篡改及不覆盖；只读本地配置，没有ECC/硬件/receiver |

最初混合用例直接在具体对象include上请求覆盖，被既有碰撞规则拒绝；测试改为通过默认Board的include继承、再建立具体覆盖，未放宽解析器。最初43通过/1失败日志保留；最后普通/插桩44项和GUI67项由当前构建执行。最后窗口还在保存后模型失效时清旧树，完整GUI重新核对通过。

当前日志build-xcfg-history/results-history-current.txt、/tmp/getdaq-xcfg-history-asan对应日志、build-suite-tests/results-history-gui-current.txt以及results-history-snapshot-cli.txt。独立build-xcfg-history/validation-history/保存五构建/源码/程序与小型CLI证据，排除凭据/DAQ载荷；旧299/194/220等归档保持原字节且只作历史复核。

此阶段只改本地配置文件；没有DAQ传输或真实板端命令，临时XML自动清理，最大用例约4MiB。用户后续传输测试每轮所有流合计20GiB并结束清理的约束保持。平台仍Ubuntu20.04.6/GCC9.4/Qt5.12.8。

Open/窗口关闭不保留历史，保存新分支丢弃Redo；已导出/发布/绑定和真实硬件不随文件撤销回滚。没有跨文件事务/独占编辑器锁、所有并发保存后失败时序或断电验收。旧Spirit全库、全部旧命令、数据库/网络include、跨文件迁移、在线切换及目标平台/真实加载仍未完成，操作和错误解释见[撤销手册](CONFIGURATION_UNDO_REDO.md)。整套仍在进行。

## 56. GET命名样式、固定计划绑定与测试清理（2026-10-08）

receiver/FrameStore加入--file-naming native|get，默认原文件名及规范计划字节保持。get采用本地毫秒会话时间和每来源至少四位递增编号，跨EOF连接不重用0000，仍O_EXCL、完整帧/大小/事件<=轮转、不覆盖和严格封存。固定计划可选file_naming真正字符串，非默认get进入摘要/启动参数，并核对实际状态；缺字段按历史native处理，要求get实际native保持Failed及禁止新Start。

这只是命名样式兼容：旧版重置编号/取时间的同毫秒截断风险、写后阈值可创建空文件和丢弃未知类型行为没有恢复。未知MFM继续保存。旧源码CoBo编号>=100归MuTaNT的约定不作为新类型判断；真实固件和下游仍需样例验证。

| 检查 | 最终结果与范围 |
| --- | --- |
| 原源码参考 | 收到Time.cpp/FrameStorage命名片段及第52节冻结EncodeOptions实际函数；只换外围时间对象/入口名，27组微秒/UTC、上海、纽约时区相同，6组默认计划逐字节相同，21个真实GET文件名相同。不是旧框架运行或全程行为相同 |
| 原生receiver | 普通/ASan/UBSan/LeakSanitizer各8严格C++11编译、18输入，完整核心通过分源/四位编号、事件重置、跨连接递增和已有文件碰撞原字节不变 |
| 实际TCP | 两组各4场景：混合CoBo/AsAd/MuTaNT/未知类型、十三个1MiB文件轮转、两连接和FDT组合；每组13676684线上字节，22文件逐字节/SHA/计数/EOF/退出/seal核对后全部删除 |
| 管理构建/codec | 普通/插桩各49严格C++11编译、81输入，原生核心/CLI无Qt/Ice/yaml/Boost/curl，只有低频服务用系统Qt。省略native字节保持、get摘要变化、类型/大小写/未知值拒绝及64位状态通过 |
| 实际进程/归档 | 普通/插桩各17场景顺序通过，含两GET来源两周期/真实EOF/硬链接归档、运行模式不符拒绝及原15生命周期故障；每组固定线上上界3342336字节，所有.graw删除，未接真机 |
| 实际HTTP管理 | 普通/新接收器及新管理代码插桩分别通过GET固定计划normal/catalog、8 HTTP故障、codec/credentials。测试原始数据直接进入receiver，Qt只处理控制/元数据；实际认证值排除归档，所有生成载荷删除 |
| 原模式回归 | 两组原TCP五组（32MiB慢导出/SHA、错误尾帧/目录锁/真实普通Qt sentry归档）、FDT/ZBUF十二及空闲导出重连均通过，合成数据清理；sentry/系统Qt未整体插桩 |
| 测试目录修复 | 旧归档Fixture遗留43目录/221.graw路径，经固定假PID2411/2412、a/b/d摘要/会话签名确认，终止状态核对后删除5369806911逻辑字节，仅保留小型元数据/清理凭据。新OwnedTestDirectory在构造失败/成功/拒绝时析构清理、不跟随符号链接；普通/插桩各27检查后无新目录 |
| 正常集成构建 | 当前可选本地兼容构建的ECC服务器、数据管理服务、catalog和系统工件工具重新编译，新接收计划类型一致；只声明普通构建，没有把旧Ice库/整个ECC运行称为插桩或实机验收 |

首轮GET生命周期暴露另一处seal文件名字符集合仍拒绝冒号，补齐POSIX单组件许可后通过，未允许斜线/反斜线/NUL或放宽设备/inode/大小检查。命名不符测试初版错误要求needs_reconciliation布尔为true，既有Failed状态已经阻止Start；改为验证Failed及实际新Start拒绝，没有修改旧状态语义。旧格式参考首次缺外围EncodeJsonString声明，补实际头后编译，对照通过。初始失败日志/小型证据保留，不算通过。

当前日志build-receiver及/tmp/getdaq-file-naming-receiver-asan的results-get-naming-*；管理build-file-naming-supervisor及/tmp/getdaq-file-naming-supervisor-asan的results-managed-file-naming-final-v2.txt、results-file-naming-http-current.txt、results-file-naming-codec/catalog-final.txt；旧参考reference-final/。正常集成日志build-ice-compat/results-file-naming-current-build.txt。独立validation-file-naming/保存同版源码/四构建/普通集成记录、原参考及小型证据，排除实际认证值和.graw；旧308/220/194等包保持历史字节。

平台Ubuntu20.04.6/GCC9.4/Qt5.12.8、本机合成发送器；没有真实下游/固件、物理多主机、目标平台、断电或持续吞吐验证。所有流每轮总预算20GiB并结束清理；稀疏逻辑大小不等于物理吞吐。完整时区/计划/命名边界和操作见[命名手册](RECEIVER_FILE_NAMING.md)。整套仍未完成。

## 57. 私有tmpfs真实ENOSPC与写入/封存错误处理（2026-10-08）

Linux独立用户/挂载命名空间允许私有8MiB tmpfs，实际内核ENOSPC已验证，不占满数据盘/共享/dev/shm。与此同时测试专用LD_PRELOAD只对私有输出目录返回指定write/fsync/close/rename错误，产品不链接或加载该库。二者验证范围区分，不把注入EIO当介质故障。

修复零写原strerror(0)可能报failed: Success的问题，缓冲/直接路径明确returned zero bytes；EINTR继续重试且不提前移动偏移。数据目录及seal同步/关闭/rename分阶段保存errno，再执行后续调用，错误原因不因close覆盖。发布后目录fsync失败尝试移除最终seal，保持退出1，原始数据不自动删除。

| 检查 | 最后结果与范围 |
| --- | --- |
| 真实空间ENOSPC | 普通/ASan/UBSan/LeakSanitizer各在8MiB tmpfs发送12个1MiB帧，实际write ENOSPC；证明剩余数据块0、inode仍有、实际文件与独立输入前缀一致、退出1/error/无正常seal。数据删除并卸载 |
| 真实inode ENOSPC | 两组各限制12 inode、发送20个不同来源64字节MFM，实际open ENOSPC；证明inode0但数据空间仍有、此前完整帧原字节保留、退出1/无seal。每组两场景线上上限12584192字节 |
| 私有故障注入 | 两组各十场景：缓冲/大帧零写、缓冲EIO、原始文件/数据目录/临时seal/发布后目录fsync、seal close/rename失败，以及首次write EINTR后成功。失败退出1指定诊断/无最终seal，成功完整SHA/计数/EOF/seal；每组897024字节、原始载荷全部删除 |
| 当前构建 | 普通/插桩各8严格C++11编译、18输入前后SHA/程序/命令/ldd凭据，无Qt/Ice/yaml/Boost/curl；测试库独立C11编译，不属于产品运行依赖，系统库未整体插桩 |
| 回归 | 两组完整核心、原TCP五组（32MiB慢导出全SHA/错误尾帧/目录锁/普通Qt sentry归档）、真实EFBIG四场景均通过。sentry是已有普通Qt程序，合成原始数据清理 |

先只探测unshare能力，再在测试自建目录内挂载；内层核对挂载命名空间与外层不同、容量/inode限制准确。小型日志和SHA在挂载外，finally先结束子进程再删.graw/umount/删除挂载点；外层只结束自有进程组。没有挂载或填满真实数据盘，没有接真实硬件。最终普通/插桩证据来自修复后同版程序；更早普通真实ENOSPC成功记录仅保留为前期观察。

日志build-receiver及/tmp/getdaq-receiver-io-asan的results-enospc-current.txt、results-io-fault-current.txt、results-io-core-current.txt、results-io-native-regression.txt、results-io-file-limit-regression.txt。各测试根有result/payload-cleanup；独立validation-io-reliability/保留程序、源码、私有测试库及小型证据，无.graw/凭据，旧1377/308/81包保持历史字节。

范围Ubuntu20.04.6/GCC9.4/Linux命名空间，本机模拟数据。tmpfs ENOSPC是真实内核空间/创建失败，但不替代生产磁盘/网卡/设备EIO/fsync/断电/目标平台/长时事件计数。注入只证明程序处理指定返回值，不能称真实介质故障；删除最终seal也不能由此宣称断电后不会复现旧目录项。错误Run保持、硬件UNKNOWN不因receiver退出清除。详细操作及现场解释见[写盘可靠性手册](RECEIVER_IO_RELIABILITY.md)。本轮各组远低于20GiB总预算，原始合成文件结束删除。

## 58. 自有receiver即时观察及正常EOF区分（2026-10-08）

DataSupervisor增加只读ObserveReceivers，经认证服务GET /v1/receivers和原生getdaq-supervisor-query receivers提供。原/v1/status字段/缓存语义保持；新接口使用操作锁try_lock和waitid(WEXITED|WNOHANG|WNOWAIT)，忙时返回busy/空观察，不发信号、不回收、不写日志、不读MFM或seal、不锁存故障。正常EOF后退出0仍须Finish；服务重开不接管历史PID。每个查询最多三次EINTR重试，子项绑定生命周期id/pid，未知所有权不猜测PID。

| 检查 | 最终结果与范围 |
| --- | --- |
| 严格构建 | 普通/ASan/UBSan各49严格C++11编译、81输入前后SHA；核心/CLI无Qt/Ice/yaml/Boost/curl，低频服务使用系统Qt，Qt/系统库未整体插桩 |
| 原生实际进程 | 两组各21场景，包括原17生命周期/GET命名/归档和新增运行→正常EOF、截断帧退出1、SIGKILL、夹具外部回收产生ECHILD；重复观察不改变生命周期/日志，正常Finish仍可回收并验证seal，Finish忙时快速返回 |
| 真实认证HTTP/CLI | 两组各19场景通过，含新增4个运行/EOF/错误/信号/服务重开查询；错误agent/controller/plan被CLI拒绝，无认证401。正常EOF尚未Finish时仍ready但观察exited，后续Finish进入stopped并核对原始字节；异常进入failed。超时Finish期间查询busy且不等待操作完成 |
| 协议/传输回归 | 两组各codec及credential检查通过；64位时间字符串精确往返、错类型/身份/状态/数量/退出码/信号拒绝。既有8个HTTP传输错误/分片/超时用例按各自预期通过，不把预期错误退出算作成功请求 |
| 接收路径 | 两组使用第57节同版普通/插桩原生receiver；本阶段没有修改接收/写盘热路径。每组两runner的固定线上总上界8257536字节，全部生成.graw结束删除，小型SHA/状态/日志保留 |
| 普通集成构建 | 现有本地Ice兼容构建的getdaq-supervisor、getdaq-supervisor-query和getdaq-ecc-server重新构建；这里只证明普通编译，不声明整套ECC/旧Ice插桩运行或真机验收 |

普通根build-receiver-observation/ordinary-evidence/receiver-observation-80b5p9r9，插桩根build-receiver-observation/sanitized-evidence/receiver-observation-fky5wizj；各有result和payload-cleanup。两管理构建的results-observation-current.txt及build/source/compile/program/ldd凭据保留，普通集成日志build-ice-compat/results-receiver-observation-current-build.txt。独立validation-observation/冻结输入、程序、生成Qt源码和小型证据，排除实际凭据、符号链接和.graw；此前所有归档保持历史字节。

范围仍本机Ubuntu20.04.6/GCC9.4/Qt5.12.8、合成回环TCP；退出观察不证明网络/磁盘健康、进程没卡住、硬件实际加载或当前操作独占控制权。新观察时间来自服务端墙钟，不是跨主机原子时间；操作busy或重开空列表不能证明停止。ECC/GUI现有缓存尚未接此观察，不存在自动告警或硬件Stop/恢复。操作与原理详见[进程观察手册](RECEIVER_PROCESS_OBSERVATION.md)。整套重构和现场验收仍未完成。

## 59. ECC后台进程观察与GUI异常/失联/过期提示（2026-10-08）

ECC经认证GET /v1/receivers在Qt后台调用原生RemoteReceiverManager的独立只读HTTP客户端。每节点先核对identity/controller/规范计划及已绑定实例，再读真实进程；整体网络预算1500ms，最多一个任务，另一请求409、不排队。失败节点available=false/null，其他已成功节点保留；预算耗尽时剩余节点明确未确认。错误信息限制512字节/合法UTF-8/无控制字符，总节点及源195，响应有字节预算。查询不改变ECC/数据缓存、控制权/UNKNOWN，不读事件或写操作日志。

跨平台纯协议复用原生命周期校验，严格解析新增聚合/进程字段与64位时间。GUI新增可选native_receiver_status，默认旧配置关闭、新模板开启；每客户端一份待处理读取，本地单调计时器检查时效，身份/计划/PID/session相同才叠加观察。异常退出提示退出码，正常退出提示等待Finish/seal，忙碌/失联/缺失/过期撤下旧正常显示；不自动发硬件命令，不增加故障解除或控制命令联锁，原字节/帧数/文件数仍是缓存。

| 检查 | 同版最终结果与范围 |
| --- | --- |
| Qt控制流程模型 | 普通/ASan/UBSan/LeakSanitizer各78源码/199输入，16 passed/0 failed，含异常退出、正常EOF等待、busy、节点不可用、恢复显示和等待中的观察过期；核对未额外发送命令。这里使用真实Qt客户端/流程和本机HTTP模拟服务，不是完整主窗口/实机 |
| Qt客户端 | 两构建各25源码/146输入，20 passed/0 failed；读取前必须有身份、单份pending、数据绑定不符/坏JSON不展示/不POST，原意图/重开/告警/启动选择等回归通过。观察失败不清除旧控制门槛，也不阻断明确信息核对后的既有停止入口 |
| 完整界面/实际程序 | 普通完整getdaq-tests各86源码/207输入、67 passed/0 failed；GetDAQ实际GUI各83源码/204输入严格C++11构建通过。只声明普通完整GUI，系统Qt和窗口插件未整体插桩；Qt5.14.2 MinGW/macOS/Ubuntu24仍待验 |
| 新原生代码 | 新实验/硬件/ECC/数据管理/纯协议及低频ECC前端75严格C++11 ASan/UBSan编译、140源/头输入，两个生成Qt源码及四个旧兼容依赖SHA保留；旧Ice/GET/bzip及系统Qt库未整体插桩，不把插桩构建等同所有库插桩 |
| 纯协议 | 普通及严格C++11插桩程序运行通过；原状态解码提取为共用检查，原缓存/大计数合同回归；新增观察的64位最大时间、错身份/PID/state/exit/signal/busy/数量/类型/溢出/过大输入拒绝。独立程序不链接Qt/Ice/yaml/Boost/curl |
| 实际跨进程 | 普通/新代码及数据服务/receiver插桩各normal-0、normal-1、truncated三场景通过：真实ECC→两数据服务→原生接收器；fresh不虚构进程，Ready实际running，观察不发模拟硬件动作，原两周期/封存及错误尾帧保持。硬件是本地模拟Ice，不是实机 |
| 失联/慢节点 | 两组normal-0额外停止一个已结束周期的数据服务，证明该节点不可用而另一节点可观察；自有假控制端点接受连接但不应答，整轮返回未确认，另一路ECC状态在500ms内完成且缓存原字节不变，重复观察409。实际总等待小于2500ms，只属本机验证，不是硬实时承诺 |
| 清理/预算 | 两组各3145728字节固定线上上界；各10个生成.graw/180224逻辑字节已删除，保留小型清理SHA/计数；测试不会填生产磁盘或删除真实Run。所有流合计20GiB上限继续有效 |

普通最终根build-receiver-gui/ordinary-final-evidence/receiver-gui-service-vqi58h6d，插桩最终根build-receiver-gui/sanitized-final-evidence/receiver-gui-service-5wpoekkc。Qt各构建results-current.txt、source/program/generated/recipe凭据；原生/tmp/getdaq-ecc-receiver-gui-asan/commands.json、source/generated/dependency-sha256及独立协议构建/运行凭据保留。普通集成最后日志build-ice-compat/results-receiver-gui-final-build.txt，四程序及合并308输入记录于build-receiver-gui/integrated-normal-receipt.json。

早期普通三场景在补充慢节点/错误文本预算前通过，只作为过程记录；最终普通/插桩均采用补充后同版源码。独立validation-gui-observation/冻结最终源码、实际程序及小型证据，排除凭据目录/符号链接/.graw，旧2145/228等历史包保留原字节。

范围Ubuntu20.04.6/GCC9.4/Qt5.12.8，所有流是本机合成数据。新观察没有实时事件计数、磁盘/网卡健康、卡死或持续吞吐证明；正常exited/0仍须实际EOF、Finish、seal。尚无自动故障联锁/完整硬件恢复，读取失联不新增未知硬件动作或释放控制权。详细配置、各显示含义和现场排障见[操作台观察手册](RECEIVER_GUI_OBSERVATION.md)。整套仍未完成。

## 60. 固定计划与原生ECC受管理ZBUF（2026-10-08）

DataSenderOptions新增protocol，受管理接受TCP/ZBUF，真实connect使用选定字符串且审计记录；非默认ZBUF进入ECC规范快照。接收源可选input_protocol，非默认进入计划规范字节/摘要和启动参数；发送/接收ID、目标及协议必须一致，本机和远程管理均校验。receiver受管理身份支持ZBUF，状态明确报告input_protocol；要求ZBUF实际TCP或参数被忽略则Failed、禁止新Start。TCP默认规范字段/字节保持，seal格式保持，原始数据路径没有新增Qt/Ice依赖。

ZBUF仍是旧VxWorks发送器选项对应的原始TCP字节流；主机不实现zbufSockBufSend，不证明板端零复制/性能。旧FDT stop关闭连接，与当前同一Run Pause/Resume保留会话不同；FDT受管理仍拒绝，必须继续补显式周期结束/重连/数据归属机制，不能以新协议字符串冒充完成。

| 检查 | 最终结果与范围 |
| --- | --- |
| receiver构建/回归 | 普通/ASan/UBSan各8严格C++11编译18输入，完整核心和TCP/FDT/ZBUF十二场景各通过，49739线上字节/各组载荷清理；只是身份/状态/计划接法扩展，分帧/写盘热路径未新增协议包装 |
| 数据管理构建 | 普通/插桩各49严格C++11编译81输入；纯核心/CLI无Qt/Ice/yaml/Boost/curl，低频服务用系统Qt。新ZBUF规范往返/摘要变化、默认省略、FDT/ICE/小写/空/布尔拒绝通过 |
| 实际管理进程 | 两组各23生命周期场景，新增两ZBUF来源两周期/同inode归档和包装器忽略模式后拒绝；原退出观察/GET名/故障/归档继续通过。两组各19HTTP场景、8传输故障及codec/凭据回归通过；每组总上界9043968字节，全部合成.graw删除 |
| 板端控制接口 | 普通真实Ice模拟节点，两种编码各ZBUF connect/缓冲策略/Start/Stop/再Start/Stop/Disconnect八调用和审计protocol核对，非法模式无调用；原未知/失败前置/整组连接等回归通过。不是真实VxWorks或板端实现运行 |
| 实际ECC链路 | 普通/新代码及数据服务/receiver插桩各zbuf-0、zbuf-1、normal-0三场景通过。模拟板端严格要求ZBUF参数，固定输入/父快照/两数据节点计划一致；Pause/Resume和Stop/Start保持同PID/session，Breakup实际EOF/退出/seal，新Configure另周期，原字节核对。各组三场景上界3145728字节，原始文件删除 |
| 本地管理绑定 | 普通/插桩各normal-0完整本地ECC/receiver流程通过，新增同地址却协议不符在硬件/进程动作前拒绝；默认同周期/两周期/原始字节/seal保持。各上界1MiB，按本次标记/索引核对后删除载荷 |
| 默认旧字节对照 | 实际第52节冻结EncodeOptions函数与当前实现六组默认计划字节相同；实际原Time/FrameStorage参考27时间及21文件名匹配，后者使用保留的历史命名凭据，不冒充本阶段新GET传输。原代码未放宽，首轮仅传错payload-evidence父目录，修正参数后通过 |
| 原生构建与依赖 | 新原生核心/ECC及低频前端75严格C++11 ASan/UBSan编译/140输入，marker修正后增量只重编变化的本地夹具、其余按同源/头/命令复用。旧Ice/GET/bzip及系统Qt未整体插桩；普通集成五程序及合并182输入另有摘要 |

本地旧夹具在/tmp自己创建根，包装器不能仅凭同名新目录签名删除：先记录基线，数据启动前由夹具在本次索引旁独占写fixture-root.json，finally先结束自有测试组，再核对本次标记/索引、固定部署/实际receiver/输出根，删除.graw。当前普通/插桩采用标记修正后的同版程序和包装器；早期成功记录只作过程证据。SIGKILL/断电仍不保证finally，不能套用到真实Run。

普通管理证据build-managed-zbuf/ordinary-supervisor-evidence/receiver-observation-9hxmildd，插桩sanitized-supervisor-evidence/receiver-observation-cs6rmrni；实际ECC普通ordinary-ecc-evidence/receiver-gui-service-khldqegz，插桩sanitized-ecc-evidence/receiver-gui-service-tkkkwrvn。本地最终普通local-marker-final-evidence/local-receiver-check-1dzu1exd，插桩sanitized-local-evidence/local-receiver-check-e5fotix8；原参考reference-final-evidence/file-naming-reference-xhp32zed。日志各build目录results-managed-zbuf-*、results-supervisor/ecc/local-manager-*及编译/程序/依赖凭据保留。

独立validation-managed-zbuf/冻结最终源码/实际运行程序/小型证据，排除.graw、凭据目录和符号链接；旧2745/2145/228等包保持历史字节。源输入/计划/实际程序以SHA绑定，不按同名程序替代。GUI本轮未变，未重跑或新宣称完整GUI/Windows Kit；本机Ubuntu20.04.6/GCC9.4/Qt5.12.8、模拟Ice/回环数据，没有真实固件/物理多主机/目标平台/长期吞吐。整套未完成，操作详见[受管理ZBUF](MANAGED_ZBUF.md)。

## 61. FDT数据主机多连接接收周期基础（2026-10-09）

数据主机固定计划现支持input_protocol=FDT；DataSupervisor为每源创建匿名AF_UNIX socketpair，以受控继承stdin传给receiver。FDT管理模式有完整四身份、durable、once=false及实际cycle_control核对，同PID/session可接多次自然EOF，累计文件/字节/连接；原TCP/ZBUF仍一次连接，不改变其默认规范计划字节。原始接收、分帧和写盘保持无Qt/Ice，控制通道仅传单字节。

私有F先持久化意图，再对所有FDT源请求结束，当前活动连接仍等真实发送器EOF；GOODBYE、控制EOF、信号和Finish超时均不能代替真实EOF。无连接F拒绝正常seal。私有A仅在从未接入数据的失败启动中止，并在receiver读取时再次核对，已有连接拒绝且继续录盘。父控制端在F前消失也继续接收，不正常封存、不自动接管。F已接受之后的控制器崩溃仍需全系统持久意图/Run/硬件核对。

| 检查 | 最终结果与范围 |
| --- | --- |
| 构建与依赖 | 普通/新代码ASan/UBSan各receiver 8严格C++11编译18输入、数据管理49编译81输入；实际程序/源码/编译命令和动态依赖核对。核心/CLI及原始数据路径无Qt/Ice/yaml/Boost/curl，低频数据服务用系统Qt；系统Qt和库未整体插桩 |
| 私有控制实际进程 | 两组各六场景：活动连接F等真实EOF，父端失联继续接两段且无正常seal，已连接A拒绝/继续录盘，未连接A确认，空会话F拒绝，非法控制字拒绝。各16416线上字节/完整原字节核对，清理四个.graw |
| 实际管理进程 | 两组各27生命周期/归档场景；新增三段同PID/session/累计连接3及完整原字节/归档、活动Finish等待真实EOF、空会话无成功seal、部分启动失败以A中止。原ZBUF/模式核对/命名/退出观察等23场景继续通过 |
| 认证HTTP/CLI | 两组既有19场景和八传输故障、codec/凭据检查；新增三真实FDT场景：三段同会话Finish/正常seal，活动连接Finish超时保持子进程，空周期失败。新超时测试先核对两receiver实际接入/解析帧，三段分别核对实际EOF，避免用TCP连接完成或进程尚在运行替代接收证明 |
| 输入回归 | 两组核心与十二TCP/FDT/ZBUF传输回归通过；各49739实际线上字节，载荷中控制字保持、GOODBYE后实际EOF、半帧/非法流/在线导出等原规则保持 |
| 清理与上界 | 各管理+既有HTTP总线上上界10637568字节；新增HTTP上界1048576、实际32768字节，清理六个.graw。测试顺序执行，全部流总20GiB预算保持；build-fdt-cycle已核对无.graw，保留小型SHA/计数/日志。SIGKILL/断电不保证finally，真实Run不按测试清理 |

普通最终证据：ordinary-control/receiver-cycle-guyy9yfd、ordinary-supervisor-final-evidence/receiver-observation-p_tx3xmy、ordinary-http-final/receiver-cycle-http-7i4t8agp；插桩：sanitized-control/receiver-cycle-asfz_hwd、sanitized-supervisor-evidence/receiver-observation-_szph6qz、sanitized-http-final/receiver-cycle-http-rzl00oij，均在build-fdt-cycle。构建收据分别在build-receiver、build-fdt-cycle和/tmp/getdaq-fdt-cycle-{receiver,supervisor}-asan；运行程序与结果SHA对应。早期三HTTP场景仅作过程证据，最终普通/插桩均用补强接入/EOF检查后的同版runner。

测试曾因seal目录枚举次序而错误拼接三段；最终独立参考按文件后缀原生数字编号排序，核对各文件和完整流，未放宽生产seal/归档。FDT多连接seal要求connections至少1且与最终实际状态一致，TCP/ZBUF仍严格1。

独立validation-fdt-cycles/冻结同版输入/实际普通及插桩程序/小型证据，排除符号链接、.graw和实际凭据目录；旧5066/2745/2145/228等包保留历史字节。这里只完成receiver/DataSupervisor基础，完整ECC sender仍限TCP/ZBUF；FDT板端start/stop、Pause/Resume/Stop/Start重连、从未Start的空周期与Run/GUI归属仍须接续。未新跑完整GUI、未证明真实固件/Narval、多主机、目标平台、长期吞吐或断电持久性。详见[FDT接收周期手册](FDT_RECEIVER_CYCLES.md)。整套重构仍未完成。

## 62. 原生ECC受管理FDT、显式未使用来源及GUI状态（2026-10-09）

FDT现接入固定ECC发送选项、对应接收计划、普通Ice控制调用和实际运行核对；不是仅增加协议枚举。Pause/Stop按旧sender行为关闭物理TCP连接，Resume/再次Start重新连接，同一receiver PID/session/文件编号/累计计数保留到Breakup。父ECC确认sender Disconnect后请求私有F，仍以实际EOF/排空/同步/退出/目录与凭据完成管理周期。

FDT构造/Connect可以不建立数据连接。从未发出daqStart且同一对象确认Disconnect的来源，父ECC明确列入unused_source_ids，按节点写入持久Finish请求/摘要，再请求私有E。manager与receiver各核对零连接/数据，receiver还拒绝待接纳连接；已有连接拒绝E、继续录盘。应答丢失不清startIssued，发过Start但没有事件/连接不准冒充未使用。未使用凭据保持sender_eof_confirmed=false、connections=0、字节/帧/文件0并显式source_unused_confirmed=true；不是数据EOF或采集完成证明。

管理状态、归档请求/复制的seal和跨平台GUI协议保留未使用字段，拒绝非零计数/未退出的假凭据。空来源归档只有元数据，有数据来源仍核对原字节/文件身份/同文件系统硬链接。GUI状态显示not used for acquisition，不自动发Stop/restart/恢复；普通TCP默认规范字段/请求/状态仍省略新字段。详细步骤、字段解释与现场边界见[受管理FDT](MANAGED_FDT.md)。

| 检查 | 最终结果与明确范围 |
| --- | --- |
| receiver / 数据管理 | 普通及新代码ASan/UBSan各receiver8严格C++11编译18输入、管理49/81；纯核心/CLI和原始数据路径无Qt/Ice/yaml/Boost/curl，数据HTTP前端用系统Qt。新原生ECC/硬件/参数/数据管理/协议及低频ECC前端75严格C++11插桩编译配方140输入；本地夹具补充后仅1测试源码增量，其余按同源/头/命令复用，旧Ice/GET/bzip和系统Qt未整体插桩 |
| 真实私有控制 | 两组各八场景通过；新增零连接E结束且EOF=false/stream_sealed=false、已有连接E拒绝/继续接收，再合法F与真实EOF正常结束。原六F/A/控制失联/错误请求保持；各20520实际线上字节，生成.graw全部删除 |
| 管理生命周期/归档 | 两组各29实际进程场景；新增全空与一来源有数据/另一未使用，未使用声明的未知ID/重复ID/已有数据拒绝，失败前置不伪造周期结束。归档保留显式未使用/false EOF和空清单；原27多段/错误/命名/观察场景继续通过。两组既有认证HTTP19及八传输故障/codec/凭据检查通过，各组合上界11425536字节，raw已删除 |
| 新认证HTTP/CLI | 两组各五实际数据服务/原生查询场景通过，包含原三多段/活动超时/空F失败及新增全空E、混合F/E结束；原生客户端解码保留未使用字段。每组实际36864线上字节，上界1MiB，raw删除 |
| 实际ECC→数据服务 | 普通/新代码及数据服务/receiver插桩各fdt-0、fdt-1、fdt-empty-0、normal-0四场景通过。FDT两编码的Pause/Resume、Stop/Start物理重连3段、同PID/session及两周期原字节/封存；Ready未Start没有数据连接，以E结束且EOF=false；默认TCP正常流程/观察回归保持。每组总上界4202496字节，20个.graw/196608逻辑字节全部删除；硬件为本地模拟Ice，不是实际板端 |
| 本地ECC / receiver | 同机FDT两周期及Ready未Start场景普通/新代码插桩各两场景通过；数据主机代码及同一父对象均为实际实现。各上界1MiB，依据数据启动前的marker、固定计划/实际receiver/输出根核对归属后删除raw，不按同名临时目录清其他会话 |
| 控制接口及未知历史 | 普通真实Ice模拟接口两种编码通过，包括FDT八调用、Connect不伪造Start历史、Stop/Disconnect不擦历史，既有应答丢失/前置/部分集合/禁止重发回归通过；失去Start应答仍保留startIssued。未运行真实VxWorks发送线程 |
| GUI纯协议 | 普通/严格C++11 ASan/UBSan/LeakSanitizer通过；未使用来源真实布尔字段保留，非零数据或未退出状态拒绝。默认规范请求、64位大计数、身份/枚举/界限及未知历史原合同保持；独立协议程序无Qt/Ice/POSIX后端 |
| GUI模型 / 实际程序 | Qt模型普通及新代码插桩各78源码199输入，17 passed/0 failed，新增真实Qt HTTP客户端→模型未使用字段映射、零计数和无自动命令。普通完整GUI67（86/207）与实际GetDAQ构建83/204通过；系统Qt未整体插桩，未新跑完整主窗口FDT板端或目标Windows Kit |
| 普通集成范围 | 实际运行的SDK ECC/夹具、接收器、独立数据服务与GUI等9程序/合并348输入有摘要。旧兼容CMake的普通原生配方包含C++11/GNU++11，不能标成全套普通pedantic构建；新代码严格C++11证明来自独立75插桩配方。receiver/管理/GUI各自普通严格构建独立成立 |

普通组件根build-unused-source-supervisor/ordinary-control/receiver-cycle-26xwyxv7、ordinary-observation-final/receiver-observation-ovj4kfy3、ordinary-http/receiver-cycle-http-ambnzk7q；插桩根build-managed-fdt/sanitized-control/receiver-cycle-yk4i682l、sanitized-observation/receiver-observation-dl_im1wf、sanitized-http/receiver-cycle-http-5jadd6b4。实际ECC普通ordinary-ecc/managed-fdt-bw_8bceh、插桩sanitized-ecc/managed-fdt-7zwc10ad；各程序SHA与实际运行程序绑定，不以同名构建替代。

本地最终普通ordinary-local/local-receiver-check-hvbak29b及ordinary-local-unused/local-receiver-check-68esoste，插桩sanitized-local/local-receiver-check-msw6jakx及sanitized-local-unused/local-receiver-check-odepygvb，均在build-managed-fdt；数据启动前私有根marker和归属核对/清理凭据保留。各组fdt-0实际65536原始字节已删除，fdt-empty-0无原始文件。

首轮新增管理场景已通过，但旧signal-no-eof回归因诊断前缀改变失败；恢复既有EOF/drain/seal前缀后最终29/HTTP全部通过，不放宽错误判断。整个Run字节按原生文件数字后缀排序，不能靠目录枚举次序或未补零字符串排序。FDT旧控制语义取自收到的实际FdtDataSender/DaqCtrlNodeI，原source/和历史包保留。

独立validation-managed-fdt/冻结同版输入、实际程序、Qt生成输入与小型证据，排除.graw、实际凭据、符号链接；旧2777/5066/2745等包仅保留历史字节。当前所有已结束组raw均已清理，总20GiB限制保持；SIGKILL/断电不保证finally。本机Ubuntu20.04.6/GCC9.4/Qt5.12.8，无真实FDT/Narval、物理多主机、目标平台、持续吞吐、端到端零丢失或断电持久性验收；整套重构仍未完成。

## 63. 可选原生ICE输入、显式源结束及在线导出（2026-10-09）

新增apps/iceingress，以独立getdaq-ice-receiver兼容原mdaq::daq::DataProcessor.processData，原DataProcessor身份/checkedCast/单向ByteSeq保持。任意RPC块不当帧边界，借用字节同步复制进固定块池，单写盘线程复用MfmStream/FrameStore及可选原生Exporter，不含Qt。旧ICE sender没有结束方法、析构不关闭其长期communicator数据连接；旧无结束、peer关闭或信号只能保留已确认完整帧，不制造成功EOF/凭据。

新增GetDAQ::DataIngress继承原接口，finishStream(sessionId, expectedBytes)由数据源在同一个有序数据连接发送；固定单线程同步dispatch/连接序列化，session与累计接受字节必须精确相同，拒绝其他控制连接的假屏障。总字节核对覆盖首条单向大消息丢失后重连、不能冒充零数据完成。实际标记、完整帧、队列排空/同步及文件集核对后，仅发布独立getdaq.ice-receiver-seal类型，sender_end_confirmed=true但sender_eof_confirmed=false；不被现有TCP/FDT归档接法误用。

| 检查 | 最终结果与范围 |
| --- | --- |
| 可选构建 | GETDAQ_BUILD_ICE_RECEIVER默认OFF，仅历史兼容构建显式开启；默认TCP/ZBUF/FDT CMake/GUI不新增Ice链接。新源、MFM/store/export与生成新接口共7严格C++11编译、19源/头输入，普通/ASan/UBSan各完成；4生成文件和4本地旧GET/Ice/IceUtil/bzip静态库另有SHA，旧库未整体插桩 |
| 实际数据协议 | 普通/新代码插桩各14本机真实Ice客户端/服务场景，两种1.0/1.1编码、混合MFM字节序、317字节RPC分片跨帧、帧内全0/FF原样保存，原DataProcessor checkedCast/oneway兼容通过。升级源正常/零数据结束、错误session/字节数/其他连接拒绝且之后合法同连接结束通过 |
| 缺结束与错误 | 两组各旧单向无结束、强制peer关闭仍持续等待/未发布完成，测试清理信号后退出1；半帧保留此前8192完整字节但不成功，超大MFM头/第二来源拒绝，超Ice消息限额和丢首消息后另一连接End声明非零总数拒绝，未将空目录封存成功 |
| 在线导出 | 两组均真实分析TCP连接核对小端uint64长度+两帧原字节，共8208导出字节；复用已有最多8客户端/有界预算/慢客户端断开逻辑，不引入可靠双写或历史重放承诺。观察用100ms只在测试fixture，生产结束靠源标记和字节/文件核对，不靠等待时间 |
| 清理与容量 | 每组14场景保守线上总上界32MiB（输入/协议/输出余量），10个.graw/81920逻辑字节均删除，结束后无raw遗留。用户全部流合计20GiB限制保持；SIGKILL/断电不保证finally，生产Run不按此清理 |
| 来源与平台 | 旧DataProcessor.ice/IceDataSender.cpp/IceDataReceiver.cpp在本地原source/核对且未修改，使用本地Ice3.5.1/原GET生成接口；程序无Qt/yaml/Boost/curl动态依赖。Ubuntu20.04.6/GCC9.4本机模拟，未证明真实固件、VxWorks源升级、Narval、目标平台、持续吞吐或端到端事件完整 |

最终普通build-ice-ingress/ordinary-build及ordinary-export-final/ice-ingress-750vla77，插桩/tmp/getdaq-ice-ingress-asan及build-ice-ingress/sanitized-final/ice-ingress-fglii5bo；各7配方/19输入、生成/依赖/实际程序摘要和小型清理证据。早期6编译/13场景只作过程记录，不当最终有导出版本。Slice首次使用保留Ice前缀被编译器拒绝，接口改为DataIngress后构建通过；独立MFM参考字段采用原布局和两种字节序，不依赖新解析器生成期待值。

独立validation-ice-ingress/冻结同版源码、生成接口、实际普通/插桩程序与小型证据，排除.graw和真实凭据。当前阶段仅独立输入，原getdaq-receiver和固定ECC/DataSupervisor仍只管理TCP/ZBUF/FDT；原ICE单向接口不会自动产生新结束方法。后续接升级发送端、固定ICE计划/身份/专属结束与归档及GUI管理，不能以控制Stop或另一个连接关闭输入冒充源结束。详细操作/原理/依赖与使用边界见[ICE入口手册](ICE_DATA_INGRESS.md)。整套重构仍未完成。

## 64. 原生ICE源库/发送工具、固定连接与UNKNOWN（2026-10-09）

新增IceSourceClient数据源库，公共头仅C++11类型，Ice实现单独隔离；它不接ECC/Qt控制路径的原始事件。Connect实际核对原DataProcessor及新增DataIngress，并经只读streamSession发现/核对会话，再通过实际Connection.createProxy固定数据/结束代理；不是保持同一地址后自动找新服务。单向return仅本地handoff，issuedBytes/Chunks在调用前保守计数，locallySentBytes在返回后累计。Finish同连接声明session/总数，sourceEndAcknowledged只是源标记应答，不证明receiver退出/文件seal或Run归档。

重试关闭，连接/发送/结束错误保持UNKNOWN；原对象拒绝Connect/Send/Finish重放，析构不发End。读文件/stdin的getdaq-ice-send用有界缓冲，只有自然EOF无中断/读取/发送错误才显式Finish；文件在连接前可按max-bytes拒绝，管道超过限制不发结束、不删除生产输入。

| 检查 | 最终结果与范围 |
| --- | --- |
| 构建与依赖 | 普通/新代码ASan/UBSan各10严格C++11编译23源/头输入，4生成接口和4本地旧静态库另有SHA；实际receiver、源库fixture、旧接口fixture及getdaq-ice-send共4程序绑定。新逻辑/新生成接口插桩，旧GET/Ice/IceUtil/bzip未整体插桩，无Qt/yaml/Boost/curl动态链接 |
| 实际源→接收 | 两组各13实际本机场景：编码1.0/1.1、自动会话发现、零流、错预期会话在Send前拒绝、析构不发End，真正DataProcessor-only服务缺结束扩展时零数据拒绝，文件/stdin自然EOF成功、连接前限额0字节/绑定拒绝、保持stdin打开时中断不发End |
| 丢连接/替换 | 每组实际接受/解析源8192字节后SIGKILL原receiver，在同端口启动另session的新进程，通过replacement-ready标记再允许源Send/Finish；固定连接不转到替换服务，新服务received/bound均0，源UNKNOWN拒绝再次Send/Finish。测试不将SIGKILL前内存缓存当持久性或丢失字节证明 |
| 接收/旧协议回归 | 两组各原14真实ICE入口/两编码/错session/总数/其他连接/无结束/peer关闭/半帧/超大头/第二源/消息限额/丢首消息重连/导出通过；新增streamSession为只读，不绑定数据源或请求完成。原DataProcessor单向接口不修改，原代码/source保留 |
| 清理与预算 | 源13场景每组保守线上总上界8MiB、11个合成输入/输出.graw共73728逻辑字节删除；接收14场景每组上界32MiB、10.graw/81920字节删除，当前根无raw。总20GiB用户限制保持，生产输入不被工具删除，SIGKILL/断电不保证一般finally |

普通最终build-ice-source/ordinary-build、ordinary-final/ice-source-6tk5sd4a及ordinary-ingress/ice-ingress-64z6svyw；插桩/tmp/getdaq-ice-source-asan、build-ice-source/sanitized-final/ice-source-jwlgek6h及sanitized-ingress/ice-ingress-5_23r0z4。输入/实际程序/生成/旧库SHA与普通/插桩日志、错误/替换和小型清理凭据保留。独立validation-ice-source/冻结同版输入/实际程序/小型证据，旧203文件ICE入口及6844/2777/5066等包保留历史字节，不以旧版本冒充本轮源。

范围仍Ubuntu20.04.6/GCC9.4/本机模拟；未验证真实VxWorks源库接入、旧板端升级、物理多主机/目标平台、持续吞吐及端到端事件完整。固定ECC ICE身份/生命周期、专属凭据Run归档和GUI管理尚未接，不能因发送工具成功就宣称整套完成。详细API/状态/文件与管道/UNKNOWN步骤见[ICE源手册](ICE_DATA_SOURCE.md)，接收器原理见[ICE入口](ICE_DATA_INGRESS.md)。

## 65. 数据主机混合ICE计划、独立源结束与专属归档（2026-10-09）

DataSupervisor新增可选ice_executable/ice_executable_sha256，按每来源协议选择接收程序；同一计划支持ICE和原生TCP/ZBUF/FDT。程序inode/权限/所有者/内容摘要进入固定身份核对；默认计划仍省略新增字段和默认TCP字段。ICE运行身份带PID/session/source/configuration/artifact/目录、实际端口/命名和来源绑定数，不用普通TCP readiness连接消耗数据来源。

管理Finish仅等待ICE源同连接finishStream和接收器退出，不由控制端发送End、不发送信号、不把空目录或peer关闭当EOF。专属ICE凭据保留false sender_eof_confirmed和true sender_end_confirmed，正常退出/队列/文件/字节核对后管理状态新增source_end_confirmed。严格控制协议、原生HTTP查询和Qt映射保留字段并拒绝未正常退出或与unused混用。混合Run归档按来源读取不同凭据，严格核对身份/结束种类，保留原凭据字节并以同文件系统硬链接归档，不读取帧。

| 检查 | 本阶段结果及边界 |
| --- | --- |
| 普通构建 | 数据管理50严格C++11编译82输入；可选ICE10/23（4生成接口和4本地旧静态库另记SHA），当前原生receiver8/18。默认receiver无Qt/Ice/yaml/Boost/curl，管理核心/CLI无Qt/Ice，认证数据HTTP前端用系统Qt |
| 内存检查构建 | 同版新管理、ICE入口/源库及原生receiver分别ASan/UBSan严格C++11构建；旧Ice/GET/IceUtil/bzip及系统Qt未整体插桩。插桩实际运行结果以下述最终小型证据为准 |
| 实际自有进程 | 普通/新代码插桩各10实际场景通过：两编码正常、混合TCP/ICE、合法空源、缺源结束、半帧尾、专属凭据/归档篡改、失败空启动中止和错程序摘要；逐源PID/会话/源结束字段及原始字节对照，全部生成raw删除。新ICE不接受FDT unused声明，缺End的Finish不杀接收器 |
| 认证HTTP/原生查询 | 普通/新代码插桩各5场景通过：两编码正常、混合来源、合法空源和缺End超时；实际认证数据服务→接收子进程→源→Finish→查询→原生Catalog/Inspect。未认证观察拒绝，专属凭据/false EOF和查询新字段保持；raw及本轮专属认证文件清理 |
| 旧协议/源回归 | 普通/新代码插桩各原14真实ICE接收/导出和13源库/CLI场景通过；原生29管理场景及19HTTP/八故障、原文件命名/观察回归通过。所有组使用本阶段实际程序SHA，不用旧同名二进制冒充当前源 |
| 核心/纯协议 | 普通/插桩receiver核心、管理codec和文件系统catalog通过；独立ECC协议普通及严格C++11 ASan/UBSan/LeakSanitizer通过，新增真实布尔源结束及非法未退出/unused组合拒绝 |
| GUI及ECC程序 | 实际Qt操作台83源码204输入重建；GUI模型78/199、18 passed/0 failed，新增实际Qt HTTP客户端→模型ICE结束/封存/非unused映射且无自动命令。普通SDK getdaq-ecc-server重建通过，但没有新版完整ECC ICE源端联调或目标Windows Kit证明 |

普通最终根build-managed-ice/ordinary-final-v3、ordinary-http-final-v3、ordinary-ingress-v3、ordinary-source-v3、ordinary-native-regression-v3。插桩最终根sanitized-final-v3、sanitized-http-final-v3、sanitized-ingress-v3、sanitized-source-v3、sanitized-native-regression-v3；两套最终结果及所有组清理已按result.json及日志核对；阶段目录核对无.graw遗留。每组管理总上界8MiB、HTTP4MiB、ICE接收32MiB、源8MiB和原生/HTTP回归11425536字节，全部流合计远低于用户20GiB上限；finally按本轮私有目录删除raw及归档链接，不删除生产输入/参考材料。

首次认证HTTP混合用例显式填默认TCP字段，而规范编码省略该字段，固定计划字节比较失败；夹具改用已有默认省略合同，未放宽比较。首轮旧FDT回归误用build-receiver旧程序，真实E请求返回Invalid private cycle control request；改用当前源码新构建程序并重跑。早期通过记录不冒充最终同版证明。

独立build-managed-ice/validation-managed-ice/冻结同版输入/实际程序/小型证据，创建后由tests/archive_managed_ice_validation.py核对。旧383/203/6844等包仅保持历史冻结字节。最新操作/原理/异常说明见[管理ICE手册](MANAGED_ICE.md)。本机Ubuntu20.04.6/GCC9.4/Qt5.12.8/Ice3.5.1；无真实板端/VxWorks升级、物理多主机、目标平台、持续吞吐、端到端事件完整性或断电结论。完整ECC源端/旧板端升级仍待接，整套重构未完成。

## 66. ICE源端生产/暂停/最终排空模块及无Qt数据构建（2026-10-09）

新增apps/icecontrol/ControlledIceSource，公共头仅C++11/标准库，源端生产线程通过Feed调用原生IceSourceClient；ECC/Qt不搬运原始波形。四个明确回调verifyIdle/start/stop/drain由真实源应用实现，缺少回调或非固定预期会话在连接前拒绝。控制锁/数据锁分开，stop/join和drain不占数据锁，避免退出线程/尾缓冲Feed死锁；Pause/Stop不发End，同一Run内Start/Stop多周期保留原连接及总数。最终Disconnect再验证生产者静止并排空，只有源自身通过同连接Finish声明结束。

任一生产/排空/空闲检查/发送/结束异常，包括非标准C++异常和停止后迟到Feed，保持UNKNOWN。原对象禁止普通控制重放，析构不发Stop/End。一次显式StopProductionAfterFailure仅stop/join+verifyIdle，不排空、补字节或补End，成功应答仍保留UNKNOWN，尝试失败也禁止再执行该停止。此库本身不实现真实板端控制权或持久恢复。

| 检查 | 已验证结果与范围 |
| --- | --- |
| 新模块严格构建 | 普通/ASan/UBSan各2严格C++11编译、28源/头输入；复用同版已核对的源库/新协议对象SHA，配对入口各10严格编译23输入、4生成接口/4本地旧静态库另记SHA。新逻辑插桩，旧Ice/GET/IceUtil/bzip未整体插桩，无Qt/yaml/Boost/curl链接 |
| 真实线程与数据 | 普通/新代码ASan/UBSan/LeakSanitizer各12实际场景通过：两编码下三次Start/Stop、线程Feed+drain尾帧、同连接/session总16384字节及正常结束；合法空源、错会话、连接前非空闲、缺回调、排空std/非标准异常、停止后非空闲、迟到Feed、析构无End、结束丢连接/同端口新会话替换。原始字节、计数、真实源标记和独立接收凭据对照；UNKNOWN中的一次停止不清门槛且不能重放 |
| 纯数据构建 | 新GETDAQ_ICE_DATA_ONLY默认OFF；开启需GETDAQ_BUILD_ICE_RECEIVER=ON。独立构建禁用Qt查找，跳过全部Qt/ECC/无关硬件目标，仅生成原DataProcessor这1份GET接口。数据程序和源模块11仓库编译；实际receiver/send/fixture不链接Qt/yaml/Boost/curl。旧vendor普通CMake可含GNU++11，严格新代码证明来自独立配方，不称整个旧库pedantic/插桩 |
| 无Qt实际运行 | 纯数据构建另12实际源→receiver线程/网络场景通过，按独立实际程序SHA绑定；不是仅检查target或ldd。正常/异常/空流/替换和字节核对全部通过 |
| 依赖/旧证据 | 普通和插桩重新构建的4个ICE依赖程序摘要与第65节实际程序逐个相同，因此复用该阶段14入口/13源/数据主机等冻结运行证据；未把旧二进制当当前源码重构。SDK controlled-source正常目标重建通过；第65节3893文件296输入归档历史模式复核通过 |
| 容量与清理 | 三组各总预算8MiB，所有流合计远小于20GiB；成功、失败和捕获中断删除本轮raw，仅保留小型日志/计数/SHA。stage build-controlled-ice核对无raw；SIGKILL/断电仍不保证finally，不清生产输入 |

普通最终build-controlled-ice/ordinary-final/controlled-ice-w_b8rhng，插桩sanitized-final/controlled-ice-bkb7rp2p，纯数据data-only-evidence下唯一controlled-ice目录；普通源码对象在source-ordinary，插桩/tmp/getdaq-controlled-ice-source-asan，配对入口ice-ordinary和/tmp/getdaq-controlled-ice-ingress-asan。各build-receipt记录实际program/源/命令及复用对象摘要；纯数据data-only-receipt和data-only-source-sha256单独记录配置/编译图。

首轮异常流在accepted_file_bytes/frames达到预期后直接读取原文件，因256KiB写盘批量缓冲尚未关闭而失败。夹具改为先证明无End/凭据且receiver仍活着，再用明确测试清理信号等待失败退出/关闭缓冲，核对真实原字节；信号不制造源结束或正常凭据。最终三组全部通过，不将接受计数当成实时已持久写盘。

独立build-controlled-ice/validation-controlled-ice/冻结当前输入、实际普通/插桩/纯数据程序与小型结果，排除raw和实际凭据，旧3893/383/203/6844包保持历史字节。API、状态、构建、线程锁/回调约束、UNKNOWN和接入示例见[源端生命周期手册](CONTROLLED_ICE_SOURCE.md)。范围仅本机Ubuntu20.04.6/GCC9.4/Ice3.5.1；完整源端低频控制接口/ECC固定会话传递、真实CircularBuffer/设备中断/策略回调和VxWorks工具链接入仍待完成。原DataSenderClient尚未放开ICE，不用字符串替代真实源端能力核对；非平台/硬件/持续吞吐/端到端完整性/断电验收。整套重构仍未完成。

## 67. 源端控制扩展、固定控制实例与完整ECC ICE会话（2026-10-09）

新增GetDAQ::DataSourceControl继承原DaqCtrlNode，SourceInfo只包含能力/来源/实例/固定目标/会话/状态/计数，不含原始帧。SourceControlServant在源进程持有ControlledIceSource和真实生产/策略回调；旧无会话connect/idempotent disconnect和分散策略调用拒绝，新connectIceStream、configureSourcePolicy、closeIceStream、stopIceSourceAfterFailure为明确动作。绑定原TCP控制连接、SourceID/每对象实例、固定目标和实际receiverSession，正常新会话仅在此前明确Disconnected后允许，旧会话/超256历史拒绝，不清UNKNOWN。

DataSenderClient仅在实际原/新接口和源身份核对后接受ICE。控制代理通过真实Connection.createProxy固定，后续实例/会话变化或源故障保持UNKNOWN；数据源另固定自己的数据连接，源结束不由ECC转发波形。EccDataLifecycle本地/远程从已核对Ready周期获取ICE来源会话；DataLinkSet整集合先查缺失/未知/重复/格式，再本地绑定，Configure在首次源连接前完成绑定。原TCP/ZBUF/FDT规范字段及调用路径保持。

| 检查 | 结果和证明范围 |
| --- | --- |
| 源控制实际RPC | 普通/新代码ASan/UBSan/LeakSanitizer各11场景通过：两编码三次Start/Stop、空源、另一控制连接拒绝、未绑定会话、真正旧接口、错误SourceID/receiverSession、策略故障与一次失败停止、同对象身份实例替换、真实数据End后丢控制ACK。原始MFM字节/计数/独立凭据逐项核对；末项控制UNKNOWN但独立数据seal存在，不据此清控制门槛 |
| 完整远程ECC | 普通/新控制与源侧代码插桩各ice-0、ice-1、ice-empty-0三流程通过；真实ECC进程→两个认证数据服务→实际receiver、源端控制/线程/原始ICE流。正常Describe/Prepare/Configure/Start/Pause/Resume/Stop/再Start、Breakup、第二接收会话与Reset；同Run PID/session保持，新Configure不复用旧会话。未Start空源由真实同连接End完成，EOF=false/source_end_confirmed=true，不伪造FDT unused |
| 插桩构建 | 原生新实验/硬件/数据管理/ECC/源端/新生成控制与数据协议及ECC低频Qt前端共81严格C++11 ASan/UBSan编译、153输入；源对象/数据客户端/回调逻辑均插桩。旧Ice/GET/IceUtil/bzip和系统Qt未整体插桩；远程插桩组数据服务与receiver明确使用已有普通/独立插桩程序，不宣称整套子进程全插桩 |
| 普通构建与依赖 | SDK实际ECC、数据服务、源控制夹具等程序重建并绑定6程序/147输入，旧兼容配方可含GNU++11。源servant/控制协议分层，控制主机只链接低频协议，数据源才链接原始发送库；默认原生receiver无Qt/Ice依赖 |
| 原控制回归 | 实际hardware-data-sender-tcp通过，原TCP/ZBUF/FDT计数/停止/重连/UNKNOWN行为保持。当前完整旧FDT/TCP网络回归另以专属结果核对，不将一次单元通过称完整旧DAQ验收 |
| 纯数据构建 | 无Qt配置/链接模式按新增能力生成3份必要原GET接口（DataProcessor、DaqControl、CmdException），13仓库编译/31输入，source-control-servant目标实际构建。该模式另12原线程/数据/UNKNOWN用例通过，raw清理。第66节1接口/11编译是历史结构，不改成当前证明 |

竞态修正后的最终普通/插桩控制、远程和本机组分别在build-source-control/ordinary-direct-final、ordinary-ecc-final、ordinary-local-final和sanitized-direct-final、sanitized-ecc-final、sanitized-local-final；各组按唯一result.json及实际程序SHA核对并通过，全部raw删除。早期通过记录仅为修正前过程，不冒充最终同版。各组三流程8 raw/131072逻辑字节已清理。源控制各8MiB保守总上限，远程组8MiB，纯数据12用例8MiB；各组输入/输出所有流合计远低于用户20GiB上限，捕获中断/失败按本轮私有目录清理，不清生产输入/原source。SIGKILL/断电不保证finally。

本地同机ReceiverProcessManager普通/新代码插桩各双编码真实生命周期及源会话绑定通过，专属夹具另1严格插桩编译并复用核对的原生对象。普通旧完整FDT/TCP四场景回归通过，数据全部清理；这些与网络流程分开记录，不把网络证明当本机适配器直接运行。最终重跑三类普通/插桩组全部通过并核对当前源码/程序SHA。生成协议/source实例标签不是认证证书或真实硬件锁，源进程重开也不提供持久恢复。

源码复核发现ReportFailure可在最终状态检查与提交之间异步发生；原State赋值可能覆盖UNKNOWN。状态赋值与Start尝试标记现都在同一状态锁内拒绝覆盖未知。普通/插桩/无Qt构建的专属实际延迟End RPC测试通过：发送源End期间独立报告设备故障，释放真实RPC应答后仍UNKNOWN，数据End ACK保留，显式一次失败停止不补End/清门槛；用例不生成raw，不把注入设备故障当真实板端验证。

详细能力、配置、固定连接、新会话、正常与UNKNOWN步骤见[源控制手册](ICE_SOURCE_CONTROL.md)，源线程职责见[源生命周期](CONTROLLED_ICE_SOURCE.md)。独立build-source-control/validation-ice-source-control由tests/archive_ice_source_control_validation.py冻结当前源码/实际程序与小型证据，排除raw/实际凭据；第65/66节3893/279等旧包按历史冻结字节保留。本机Ubuntu20.04.6/GCC9.4/Ice3.5.1；未验证真实板端CircularBuffer/设备策略/中断、VxWorks工具链/源服务部署、硬件控制权/加载版本与完整恢复、目标平台、物理多主机、长期吞吐、端到端事件完整性或断电。整套重构仍未完成。

## 68. 源端环形缓冲读出及读指针失败门槛（2026-10-09）

新增独立apps/sourcereader/SourceRingReader，标准C++11/线程库，无Qt/Ice/yaml/Boost/curl。应用提供借用内存、累计CPU可见生产位置和设备读指针发布回调；按缓冲轮流发送有界连续片段，发送明确返回后才发布读指针。发送不确定不推进；发送已返回而读指针发布丢ACK时分别保留handed/published位置，UNKNOWN禁止重发/再写指针。掩码更改前核对所有缓冲为空，普通批量门槛及强制尾部刷新明确区分。独立构建可复用同步发送器，ICE选择接到既有源生命周期，不把Qt放进原始字节路径。

旧CircularBuffer/发送器/MemRead/SyncedCircularBuffer/daqStop已作源码对照。旧地址寄存器不能判别完整绕圈，新增LegacyRingWriteTracker只有在应用显式保证每观察不足一圈/无满空歧义/不覆盖未读数据时才允许；范围或溢出异常锁存UNKNOWN。不凭该参数声称获得真实固件保证。SourceControlServant增加本地ReportFailure入口，设备/读出线程只锁存同一已连接源故障，不增加远程raw接口。

| 检查 | 已验证结果与边界 |
| --- | --- |
| 核心 | 普通/ASan/UBSan/LeakSanitizer各12用例通过：回绕/预算、延后尾部与强制排空、双缓冲公平性/禁用数据保留、部分发送不确定、设备已写但ACK丢失、越界覆盖、计数复位、非标准异常/报告失败、恰好满圈、无效掩码及旧地址边界/计数溢出；不生成payload文件 |
| 严格构建 | 普通/插桩各6严格C++11编译、19源/头/生成输入，涵盖新reader、ControlledIceSource、IceSourceClient、新生成数据协议及两fixture；实际程序SHA/命令/运行依赖保存。新逻辑插桩，旧Ice/GET/IceUtil/bzip静态库未整体插桩 |
| 实际双线程 | 普通严格/插桩/无Qt CMake各4场景通过；两线程分别生产和消费6144字节环形内存，701字节生产批次、317发送片段、1537读出预算。两编码各三Start/Stop周期、九MFM帧36864字节，强制排尾和原始文件逐字节相同；合法空流End；发布读指针丢ACK后reader/source双UNKNOWN、只交付317字节、接收器无End/成功seal |
| 无Qt构建 | 数据模式16仓库编译/36输入，三份必要原GET接口；source-control-servant实际重建，配置禁Qt查找、数据程序无Qt/yaml/Boost/curl链接。不是仅声明target，另实际运行四环形读出场景 |
| 容量与清理 | 每组所有流合计保守总上界2MiB，远低于20GiB；三组各73728输出逻辑字节已删除，故障partial frame未写成完整raw。finally删除本轮合成raw，保留计数/SHA/小日志；SIGKILL/断电不保证清理，不清原source/生产数据 |

最终普通证据build-source-ring/strict-ordinary-evidence/ring-ice-4w3kksrg，插桩sanitized-evidence/ring-ice-cckmf_ep，纯数据ordinary/ring-ice-sr4qw_a0。独立普通build-source-ring/strict-ordinary，插桩/tmp/getdaq-source-ring-asan；配对receiver来自同版既有普通/独立插桩ICE程序，按实际SHA绑定。首次编译fixture误用End字段名已修正，后续最终组通过；宿主旧ctest不支持--test-dir，改在构建目录执行，独立CTest实际通过。

详细操作、计数/指针原理、驱动前提、线程锁、正常/失败结束与构建见[源端环形缓冲手册](SOURCE_RING_READOUT.md)。build-source-ring/validation-source-ring由tests/archive_source_ring_validation.py冻结当前输入/实际程序/证据，旧4606/279/3893等包保持历史冻结字节。本机Ubuntu20.04.6/GCC9.4/Ice3.5.1；仍无真实DMA/MMIO/缓存一致性/设备背压/中断或VxWorks、硬件所有权/恢复、目标平台、长期吞吐及断电证明。完整ECC既有源控制已验证，但本阶段环形读出尚未放进完整硬件ECC实机流程。整套重构继续。

## 69. 可复用源读出线程、完整帧切换与ECC联调（2026-10-09）

新增apps/sourcereader/SourceRingReadout，标准C++11/线程库，不依赖Qt/Ice。组合driver idle/start/stop/有界只读wait与SourceRingReader，启动前确认已静止/无旧尾部，设备stop先于消费线程join，尾部Drain不发送End。设备stop尝试在调用前记录；即使已生效后丢ACK也join本地线程并保持UNKNOWN、不重复设备动作或排空。Drain按全部环形容量给静态上界，伪空闲连续生产不会导致无限刷新；析构只取消/join本地消费线程，不制造硬件停止/End。

完整ECC集成复核发现两处此前单缓冲专项未覆盖的问题：实验sender掩码实际为9（缓冲0/3），原夹具仅提供0，Configure按真实策略拒绝并保留UNKNOWN；此前逐片段轮换会把不同缓冲的半帧交错，不能用单缓冲流正确证明多来源MFM正确。修正为多缓冲必须明确保证观察写位置与初始读位置在完整消息边界；选定完整批次后，即使预算在半帧处结束也跨调用保留该缓冲，到完整批次结束才轮换。不解析/排序跨缓冲事件，不把driver保证布尔当真实固件证明。新反向用例拒绝缺边界保证，跨调用预算用例证明AAA与A的续接不会插入另一消息。

| 检查 | 最终结果与边界 |
| --- | --- |
| 核心与严格构建 | 普通/新代码ASan/UBSan/LeakSanitizer各13环形用例及9读出用例通过；含跨调用半消息接续、缺完整边界拒绝、三周期回绕/尾部、启动不确定、停止生效后丢ACK不重发、wait非标准异常、有界drain和析构只本地退出。各8严格C++11编译22输入；无payload文件 |
| 完整远程ECC | 普通/新代码插桩各ice-0、ice-1、ice-empty-0通过；实际ECC进程→两认证数据服务→receiver/SourceControlServant→实际SourceRingReadout消费线程与模拟设备生产线程。缓冲0/3各一完整MFM帧，确定性合成中断批次；Pause/Resume、Stop/Start、两接收会话及Breakup/Reset，原始文件/计数/源End/独立凭据对照。不是实时连续生产吞吐 |
| 同机ECC | 普通/新代码插桩各两编码通过，实际ReceiverProcessManager/源会话/源控制/读出线程，四帧每源、同一PID/session及最终源End/退出/seal。不是拿远程HTTP结果代替本机适配器运行 |
| 插桩ECC构建 | 五严格C++11新编译（reader/readout/servant和两实际ECC夹具），根据当前实际依赖核对后复用72旧插桩对象，144仓库依赖；实际重新链接ECC服务/远程/本地夹具并绑定SHA。新读出和控制代码插桩；旧Ice/GET/IceUtil/bzip及系统Qt未整体插桩，数据HTTP服务明确为原普通程序，接收器是配对独立插桩程序 |
| 无Qt及旧单缓冲回归 | 当前纯数据配置18仓库编译39输入、三份必要GET接口，禁Qt查找及数据程序无Qt/yaml/Boost/curl链接。修正后普通/插桩各原4单缓冲异步ICE/空源/读指针失败场景通过，raw删除；不把纯构建称真实板端驱动验收 |
| 容量与清理 | 每远程组全流预算8MiB、每本地组2MiB、单缓冲回归2MiB，均远低于20GiB；远程各8raw/131072逻辑字节、本机各4raw/65536、回归各73728字节全部删除。失败组也finally清理本轮raw；不清生产输入，SIGKILL/断电不保证finally |

最终普通build-source-readout/ordinary-ecc-final/managed-ice-ecc-ykko9m7_及ordinary-local-final/local-ice-ecc-klik5h6p；插桩对应sanitized-ecc-final及sanitized-local-final下唯一结果目录，按实际程序SHA核对。严格核心普通strict-ordinary-final，插桩/tmp/getdaq-source-readout-core-final-asan；ECC新模块插桩/tmp/getdaq-source-readout-ecc-overlay-asan。早期掩码错误的普通失败组、被明确终止的未完成全量构建均不当最终通过证据；配置/构建应顺序等待明确终止，首次未等待配置结束的普通构建失败已在配置完成后重建。

本阶段只把合成设备线程和已实现源读出接到实际ECC流程，不执行真实DMA/MMIO/中断。源端跨Run累计读出位置保留，接收流使用独立会话计数；生产者保证、驱动wait有界、真实停止/背压/缓存一致性仍待实机确认。具体操作、线程锁与UNKNOWN说明见[读出生命周期手册](SOURCE_READOUT_LIFECYCLE.md)和[环形手册](SOURCE_RING_READOUT.md)。

build-source-readout/validation-source-readout-ecc由tests/archive_source_readout_ecc_validation.py冻结当前输入/实际程序/小型证据，排除raw/实际凭据。第68节172/56与第67节4606/207等包仅作历史字节复核，不代表修正后多缓冲当前实现。本机Ubuntu20.04.6/GCC9.4/Qt5.12.8/Ice3.5.1；目标平台、真实板端源码/工具链/所有权/加载/恢复、连续吞吐、端到端事件完整性和断电仍未证明，整套重构继续。

## 70. 源端原始MFM前缀检查、半帧停止与共享头规则（2026-10-09）

新增SourceMfmRing将任意CPU可见原始写前缀转换为经过结构检查的完整MFM边界，FramedView供现有SourceRingReader/SourceRingReadout使用。只复制最多28头字节，载荷继续借用，不分配整帧副本；每观察1..65536新帧预算，默认1024，最大帧8B..min(ring容量,1GiB)。检测原始越界/计数复位而不只看完整位置，消息切换仍保留前一批次到完整边界。

新增共享mfmheader.h，接收MfmStream::Inspect/Feed和源扫描复用大小/字节序/standard header-item范围/CoBo与MuTaNT字段。旧完整帧Inspect接口保持，头前缀与整帧长度分开传入。SourceRing新增可选verifyDrained，reader的Start前/最终Drain后及掩码改变前核对原始尾部；完整集合Empty不等于没有半帧，停止于半头/半体保持UNKNOWN、不补End或跳坏字节。

| 检查 | 最终结果与范围 |
| --- | --- |
| 源核心 | 普通/新代码ASan/UBSan/LeakSanitizer各15前缀、13环形、9读出用例通过；逐字节大/小端、头/体回绕、另一完整源可先发而不混半帧、半头/体、坏头提前失败、超长、raw越界/复位、读指针ACK丢失、扫描预算、旧topology blob和Start前旧半帧拒绝。各10严格C++11编译26输入，无payload文件 |
| 旧/新共享头对照 | 从第69节不可变包提取旧Integer/FrameSize/Inspect函数，普通/ASan/UBSan/LeakSanitizer各20006有效/非法/小blob样本：13197接受、6809拒绝，字段与拒绝一致。完整旧库/所有固件未运行，不把函数对照称完整MFM协议验收 |
| 实际任意前缀ICE | 普通/插桩各6双线程真实场景通过；701字节任意写前缀、317发送片段、6144环形、正常两编码三周期/合法空流/读指针丢ACK，以及第三帧只写7头字节或2048体前缀后停止。后两项只送前两完整帧8192，raw位置8199/10240、完整/发布8192、三层UNKNOWN且无End/成功seal。两组各4raw/90112字节删除 |
| 完整ECC | 普通/插桩各远程3及本机2编码通过，双缓冲0/3的原始位置按任意写批次更新，SourceMfmRing给真实消费线程提供边界。实际ECC/认证数据服务/源控制/receiver、Pause/Resume、多Run与新会话、空源、原字节及独立End保持。合成完整中断批次，不是实时生产吞吐 |
| 默认接收与旧ICE回归 | 默认receiver普通/插桩各8严格C++11编译19输入，核心及12实际TCP/FDT/ZBUF/导出场景通过，49739线上字节/组并清理raw，无Qt/Ice/yaml/Boost/curl。旧ICE入口/导出14场景各通过，旧无结束源仍不假End。插桩ICE入口10/24、旧vendor普通库另记SHA，非全库插桩 |
| ECC插桩/依赖 | 6严格新编译（reader/readout/MFM/servant与两ECC夹具），按实际依赖核对复用72插桩对象、147仓库依赖；实际重链ECC/fixture，配对新插桩receiver。旧Ice/GET/IceUtil/bzip及系统Qt未整体插桩，数据HTTP服务明确普通程序。纯数据禁Qt查找，20仓库编译43输入、三必要GET接口，无Qt/yaml/Boost/curl链接 |
| 总量/清理 | 每源前缀组全流2MiB、每远程8MiB、本机2MiB、旧ICE32MiB、默认传输49739线上字节，全部远低于20GiB；普通/插桩各远程131072 raw逻辑字节、本机65536、源前缀90112和所有回归载荷已删除。finally只清本轮合成raw，SIGKILL/断电不保证，不删除生产输入/旧参考 |

最终普通源前缀ordinary-ring-final/ring-ice-ogkbo4a8、ECC ordinary-ecc-final/managed-ice-ecc-_2j_h2bd与ordinary-local-final/local-ice-ecc-53b0dswz；插桩源前缀sanitized-ring-final/ring-ice-wev7_8lu、ECC sanitized-ecc-final/managed-ice-ecc-qfzzvveh及sanitized-local-final/local-ice-ecc-12b1whrp，均在build-source-mfm。普通旧ICE最终ordinary-ice-ingress-regression-final/ice-ingress-l85geqwn，确保fixture构建已明确结束后再运行；早期重叠构建组不作为最终证明。头对照脚手架首次随机数类型影响std::max编译已显式转unsigned，最终两组通过，不改比较规则。

详细易读操作、位置/边界原理、扫描预算、驱动前提和半帧UNKNOWN见[源MFM边界手册](SOURCE_MFM_BOUNDARIES.md)。build-source-mfm/validation-source-mfm由tests/archive_source_mfm_validation.py冻结当前输入/实际程序/小型证据，排除raw/真实凭据；旧第69节2902/193、第68节172/56等仅历史字节复核。驱动仍须证明CPU可见性、未读内存不覆盖和初始帧起点；结构检查不是CRC、实际硬件身份/触发计数或无漏事件证明。

本机Ubuntu20.04.6/GCC9.4/Qt5.12.8/Ice3.5.1，尚无真实DMA/MMIO/中断/VxWorks部署、目标平台、硬件所有权/实际加载/完整恢复、长期吞吐或断电验收。整套重构继续。

## 70.1. 源端只读观察、显式故障停止及Qt显示

本阶段增加认证`GET /v1/sources`、纯C++11严格解码及可选`native_source_status`。按固定ECC身份/摘要、源ID、操作和receiver session显示源状态，unknown、不可观察和缓存过期都撤下正常显示。源查询使用独立短超时Ice communicator，不替换原控制代理；慢调用在ECC命令锁外执行。原系统显式故障停止会停止硬件生产，并尝试一次已绑定ICE源停止/join，不排空、补End或清UNKNOWN。启用步骤和原理见[操作手册](SOURCE_GUI_OBSERVATION.md)。

| 检查 | 最终本机结果 |
| --- | --- |
| 直接源控制 | 普通/新代码ASan/UBSan/LeakSanitizer各15；含只读超时/非法回答、恢复不清UNKNOWN、未知父对象停止及异步故障 |
| 同机ECC异步源故障 | 普通/插桩各两编码；只读查询无动作，显式停止消费线程，原receiver会话保留、UNKNOWN/no-End/无成功seal |
| 实际跨进程ICE HTTP及正常生命周期 | 普通/插桩各3：两编码及空源；前两项在Ready验证源HTTP响应、固定实例、两源available/connected和无硬件动作，空源分支验证生命周期 |
| 传统TCP/FDT正常回归 | 普通/插桩各4：FDT两编码重连/未使用源、默认TCP；传统源HTTP状态为legacy |
| Qt客户端及流程模型 | 普通/新代码插桩各21/20；身份/会话拒绝、精确大整数、unknown/unavailable、实际缓存过期及配置往返，不自动提交生产动作 |
| 完整窗口及操作台 | 普通窗口67；14列/源故障与过期文字和颜色、现有流程通过；实际GetDAQ重建，qmake输出目录按项目文件定位 |
| 纯协议及依赖 | 新代码插桩协议通过；默认receiver及可选ICE/源程序无Qt/yaml/Boost/curl动态依赖 |

原生插桩构建32个严格C++11编译、47个实际依赖未变的旧对象；系统Qt、旧Ice/GET/bzip未整体插桩，数据HTTP服务使用已绑定普通程序，不称整套旧SDK或所有进程均插桩。两类receiver当前19/24输入摘要均与工作树相同；此前第70节归档保持历史字节，不代表后续GUI源码。

测试顺序运行，所有输入输出合计上界按组2或8MiB，保持用户20GiB总上限。普通/插桩直接源各114688、故障各32768、ICE正常各131072及传统回归各196608逻辑raw字节均已删除。第一次传统普通组误用旧receiver并失败，raw亦已清理，只保留诊断，不计入通过结果。SIGKILL/断电仍不保证finally清理。

证据入口build-source-observation/checkpoint-gui.json及[检查点说明](SOURCE_OBSERVATION_CHECKPOINT.md)，绑定当前源码/程序/结果摘要；小型Qt日志在verified-test-logs。它不是完整独立重放包，不复制大批程序/raw。文档工具`python3 tools/check-documentation-links.py`排除代码示例后核对本地文件链接，未检查页内锚点。

尚需慢源HTTP重叠查询409、查询/Reset与对象释放及原控制连接丢失补充。物理多主机、真实板端/驱动/停止、目标平台、持续吞吐和断电边界不由本机模拟证明；整套剩余仍见下节。

## 70.2. 原控制断线、慢查询409与Reset释放生命周期

第70.1之后，实际关闭源端原控制TCP发现：独立只读查询仍有效，但直接Start内部验证失败未标记本地UNKNOWN。DataSenderClient现统一内部/显式验证的错误入口，绑定后的ICE验证失败保留UNKNOWN和最后确认状态，不增加生产调用次数、不用观察连接接管。Fresh零流预检查和传统非ICE行为保持。

普通/新代码ASan/UBSan/LeakSanitizer各19直接源场景通过，含原15及Connected/Running两状态的原控制丢失两编码。Running各已交付/接收8192字节，Stop验证失败后仍无停止确认/End，接收器没有正常结束凭据；夹具最终清理不当产品停止。

实际HTTP慢源普通/插桩各两编码通过，源RPC同步屏障建立并发；状态响应和第二份读取409在第一份源观察完成前得到，生产动作数0，释放后原响应及正常采集/结束通过。Reset普通/插桩各两编码通过：调试器式SIGUSR1仅暂停测试观察调用线程，处理器只用read/write，查询预算不变；实际Reset完成释放采集集合后，旧句柄仍存活，新集合为空，旧响应绑定旧操作/实例/session。允许原预算超时unavailable，不把暂停用例当实时性或自然调度证明。

最初仅靠远端RPC等待的Reset组因500 ms预算先完成而不满足跨释放断言，raw已删除，只作诊断。原生基线32严格C++11插桩编译，最终两夹具分别1对象增量并复核其他输入/对象；旧vendor和系统Qt未整体插桩，数据HTTP服务普通。Qt模型普通/新代码插桩各20通过，补查旧操作ID不能覆盖当前显示，不自动发送生产命令。

所有组顺序运行，所有输入输出上界分别8/2/8MiB；普通/插桩各直接131072、Reset65536、HTTP131072逻辑raw字节删除，本阶段根已无raw。保留小型结果及摘要，入口build-source-observation/checkpoint-concurrency.json与[说明](SOURCE_OBSERVATION_CONCURRENCY.md)；旧checkpoint/checkpoint-gui和第70.1对应历史源码，不能当当前摘要或重放包。

## 70.3. 本地配置引用、改引用地址及导入

从本地ConfigWidgets的IncludeConfig、SetConfigNodeIncludeUrl、ImportConfig，以及CompoundConfig的Document::include/import和Object::import读取职责：引用/导入替换目标子内容，改地址保留已有本地内容，相对资源按所选来源目录解析。新XcfgStructureEditor追加三个操作，保留原十项编号；导入递归展开外部include，保留默认/具体ID及可保留注释和未知属性，移除引用，不额外嵌套外部根标签。这里是本地文件迁移，没有运行整个旧Spirit库或接入数据库/HTTP资源。

Preview新增外部来源字节守卫，覆盖新引用以及导入后已不在有效树中的来源。Apply重算并核对所有来源/候选；会话历史保留导入来源摘要，外部文件即使只改注释也拒绝旧预览或Undo/Redo。仅写一个选中来源，不写导入文件。GUI提供独立路径输入及Input dependencies列表，显示文件/大小/摘要，沿用原差异、输入变更撤下Apply、只读/窗口版本和会话撤销门槛。

普通/新代码ASan/UBSan/LeakSanitizer模型各51通过（10仓库源/131跟踪输入），新增引用/地址/导入历史、嵌套/default/未知属性/注释、来源变化拒绝、循环/冲突/缺文件/非法根/远端地址、移除引用保留本地及跨文件组合深度保护。合法单文件叠加仍可能形成过深导入树，展开前显式限制128深度、200000访问和8MiB输出；没有放宽原加载预算。系统Qt未整体插桩。

普通完整窗口70通过（86仓库源/207输入），三种新动作都验证预览不写、依赖列表、输入变更失效、保存及Undo。实际GetDAQ、sentry、模拟器、离线配置编译器及配置检查器从根GetDAQ.pro构建，所有五目标按各自项目文件定位到源码根bin，修复独立/嵌套构建路径混用。目标平台未验。

新bin/getdaq-config的有效快照CLI12组、系统实验编译/原生离线读取与绑定36组通过，原用户参考源字节保持；未传入执行夹具、未连ECC/实机，不把编译绑定当实际硬件加载。最终程序/同源摘要和小型日志在build-config-includes/checkpoint.json；较早配置/GUI归档保持历史，不代表本轮结构/历史类布局。

本轮测试生成小型临时XML/JSON并由夹具收尾，未运行大流量传输。本工作区所有传输流合计20GiB上限/结束清理要求保持。详细操作、替换范围、导入来源为何继续受历史守卫及旧代码依据见[引用与导入手册](CONFIGURATION_INCLUDE_IMPORT.md)。数据库/网络资源、跨文件写入/迁移、全部旧命令/Spirit运行对照、平台和实际加载继续见下节。

## 70.4 旧CompoundConfig实际运行对照与XML文本空白（2026-10-10）

此前旧配置语义主要依据收到的源码、原测试预期及提取函数体。这次在隔离目录实际构建未改动的CompoundConfig普通库/XML及TinyXml，以固定官方Boost1.55.0提供原Spirit Classic/Phoenix1头文件，运行两个独立进程逐项比较。没有为参考程序改写旧函数体，也没有给生产Qt/接收器引入旧库。

实际对照发现普通XML、CDATA和字符引用不能统一trim。XcfgDocument现在先折叠普通XML字面的排版空白，再解码引用，保留CDATA/引用/非XML空白；准确原文件另行保存。CheckValue不裁剪精确字符串，新值保存/覆盖/新增/物化使用空白字符引用保持重载结果。被注释/标记拆成多个文字片段拒绝，避免复制旧显式type访问器只留下最后片段的丢值行为。

| 验证 | 实际结果与证据 |
| --- | --- |
| 旧参考构建 | 55次C++98原库/TinyXml/探针编译，588个实际依赖输入；原输入构建前后核对，官方包SHA固定。build-legacy-config/build-receipt.json；文件/XML及Spirit实际运行，不含Binary/数据库/网络扩展验收 |
| 普通实际运行对照 | 192场景/384进程：170一致、6明确扩展、8严格拒绝、8共同拒绝；runtime-ordinary-final/processes.json及scope.json。32位原域、范围/引号/C转义、文本空白、多层回溯、嵌套include/共享类/冲突与循环覆盖 |
| 新代码内存检查对照 | 相同192/384、相同四类计数；runtime-sanitized-final。新探针3源/124输入，严格C++11 ASan/UBSan/LeakSanitizer；旧参考库和系统Qt未整体插桩 |
| 配置模型普通/插桩 | 各63通过；10源/131输入。普通文字/引用/CDATA/非XML空白、多个片段拒绝，以及准确字符串保存/覆盖/新增/物化/范围与原字节Undo；原51引用/历史/模型场景继续运行 |
| 完整GUI | 最终71通过、0失败；86源/207输入。新增实际输入框保存连续/首尾空格及审阅撤销；results-gui-current-final.txt。首次沙箱60/10因模拟监听受限；移出沙箱首次69/1出现旧Run收尾计数超时，单项3/0后最终同版完整71/0。保留失败和单项日志，不用早期失败运行替代最终结果 |
| 离线CLI回归 | 新getdaq-config快照12进程通过，独立语义SHA/默认与篡改/不覆盖；cli-snapshot-final。系统编译/无Qt读取及绑定36通过且原参考文件保持；results-system-final.txt。本轮未传第三执行夹具或发送硬件命令 |
| 根五程序 | 根qmake工程增量构建全部成功，仍输出源码bin/；GetDAQ、sentry、模拟器、配置编译及无Qt配置检查程序摘要进入检查点。普通根工程使用现有C++11/GNU构建标志，不称整套严格插桩 |
| 本地检查点 | tests/archive_legacy_configuration_validation.py复核旧输入、5个Qt构建凭据、实际程序及小型结果，输出build-legacy-config/checkpoint.json。是当前工作区索引，不是完整可移植重放包；第70.3配置/GUI摘要现在只保留历史 |

固定Boost包位于third_party/validation/，只供参考测试；源码包/许可和官方摘要保留，新默认接收器及可选ICE生产构建不包含其路径。本轮参考解压约117MiB，没有生成原始传输载荷；对照临时XML由夹具清理，工作区所有传输流合计20GiB上限与结束删除要求不变。

本机环境仍是Ubuntu20.04.6/GCC9.4/Qt5.12.8。沙箱ptrace限制导致首次LeakSanitizer自检退出，调整运行环境后全部192对照及模型63通过；不能把已输出JSON视为插桩通过。GUI的实际Windows Qt5.14.2 MinGW/Ubuntu24/macOS、硬件加载/控制权/恢复、全旧命令和数据库/网络/跨文件迁移、断电/持续吞吐未完成。更广泛旧词法、Text/Binary格式及全部接口仍需独立验收；完整步骤和各项差异见[旧库运行对照与文本迁移](LEGACY_CONFIGURATION_RUNTIME.md)。

## 70.5 自包含XML导出、另存后编辑及旧命令对应（2026-10-10）

核对旧ConfigWidgets十个CommandId和文件动作后，新增XcfgExporter与GUI **Review XML export / Save As...**、CLI `export-xml`。声明模式展开include、保留内部默认/注释/CDATA及可保留属性，固定上下文共享单位/范围；独立模板根去掉自身`*` ID。有效模式导出既有具体子树、明确实际ID/type，精确保留字符串空白，并逐项核对原/导出对象、值、type、单位与语义范围。两模式区别明确，不用独立声明副本冒充外层继承全部物化。

预览通过自动清理的临时候选重新加载；原文件/所选输出不写入。保存重算预览和全部来源，使用NewOnly拒绝覆盖；Linux文件fsync、输出重载字节/摘要/来源再次核对。部分写入/写后检查失败明确报告已经创建/保存，不自动删除或回滚。GUI后台预览/保存与窗口版本守卫、只读导出、输入改变撤审、可选打开副本清历史已接；没有跨进程锁/目录fsync或断电原子发布保证，不参与DAQ热路径。

| 验证 | 最终结果与证据 |
| --- | --- |
| 配置模型普通/新代码插桩 | 各67通过；11源/133输入，普通build-xml-export/model及/tmp/getdaq-xml-export-model-asan。两模式整根语义SHA、共享属性/精确CDATA空白、模板根/具体子树、原来源不改、预览篡改/来源漂移/已有目标/非法对象拒绝，以及此前全部配置历史回归 |
| 完整GUI | 最终73通过，0失败；87源/209输入，results-gui-final-current.txt。实际后台预览不写目标、输入改变撤Save、来源表、另存后打开/历史清空、只读导出和窗口版本变更拒绝。两处编译缩进警告修正后同版重新构建/全组通过，无新硬件命令 |
| XML CLI普通/插桩 | 各30进程，cli-ordinary-final和cli-sanitized-final；后者35源/157输入，严格C++11 ASan/UBSan/LeakSanitizer，系统Qt未整体插桩。完整副本语义、模板/具体模式区别、精确空白、不覆盖和错误无输出通过，全部临时XML/JSON由夹具清理 |
| 旧真实导出 | 两组CLI各含3个实际原CCfg::Io::Document::exportSubConfig调用，对整根/两个具体声明的输出有效语义比较通过。独立reference/构建55源/588依赖、原源码前后守卫；旧库/系统库未整体插桩。不是新函数生成旧期望，不声称所有旧导出输入或旧多文件SaveAs均兼容 |
| 旧CLI回归 | 快照12进程/独立SHA等通过，snapshot-cli-final；系统编译/无Qt读取绑定36通过且原文件保持，results-system-final.txt。本轮未提供第三执行夹具或连硬件 |
| 根五程序 | 根qmake增量构建成功，最终日志control-build-final.txt，源码bin/输出和实际程序摘要进入检查点；普通既有GNU/C++11标志，不称完整根套件插桩 |
| 当前绑定 | tests/archive_xml_export_validation.py核对原参考输入/对象、4份Qt构建/生成输入、实际程序和小型结果，build-xml-export/checkpoint.json。第70.4相关Qt/GUI输入已经变化，旧818/12/43检查点保留历史；当前索引不是独立可移植重放包 |

十个原命令逐项列在[配置命令对应表](CONFIGURATION_COMMAND_MIGRATION.md)。旧Override还包含对象浅层/兄弟处理，当前scalar覆盖及已有具体对象Materialize不能代替其完成；保留引用的多文件SaveAs/SaveAll也未完整接续。收到的三个Import/Export/Include数据库UI处理函数体为空，不把原空菜单当成已实现数据库功能；扩展或现场用途另行核对。操作、失败解释与原理见[XML导出手册](CONFIGURATION_XML_EXPORT.md)。

本轮只小型离线XML/JSON、GUI模拟服务，没有大体积原始传输文件，数据接收器未改。每轮所有传输流合计20GiB上限/生成数据结束删除保持。本机Ubuntu20.04.6/GCC9.4/Qt5.12.8；真实板端/控制权/加载/完整UNKNOWN恢复、目标平台、多文件迁移与持续采集/断电仍待完成。

## 70.6 继承对象浅层覆盖、模板兄弟与旧真实模型（2026-10-10）

新增OverrideInheritedObject（第14项，原0..12编号保持）及CLI preview-object-override/override-object。按原视图来源顺序浅层补对象，选`*`时补同名所需具体兄弟，递归补缺的本地祖先；新层级清include，参数仍继承。Lookup内部另记录是否经过默认回溯，直接include与本地直接对象不是继承对象，不能重复插入。候选整体有效对象/value/type/unit/语义range摘要必须保持，复用预览/依赖/保存后快照与准确字节历史。

真实来源编辑可能写include文件；多处引用同文件时，列出所有由继承变成本地的具体投影，不能只显示请求路径。创建路径/投影列表同样进入保存前重算守卫。按物理文件编辑与旧多个内存include副本的声明投影不保证完全等价；该边界明确，初始有效内容保持。

| 验证 | 最终结果与证据 |
| --- | --- |
| 实际旧模型/命令 | 未改ConfigNode、ConfigModelAccessor、OverrideConfigNode和基类/错误/实际日志共13次C++11编译，moc原QObject代码；54个旧CCfg/TinyXml对象SHA核对复用，681实际输入。reference/build-receipt.json，不构建整个旧界面；没有提取重写旧函数或用新值作oracle，旧库未整体插桩 |
| 运行对照普通/新代码插桩 | 各16场景、151进程；13实际覆盖、3直接/根无动作。真实旧redo/undo/redo的有效摘要与新候选/保存一致，含默认声明的对象树及顺序一致；缺祖先、模板兄弟、任意来源顺序、已有本地兄弟、注释、继承include/清新URL与外部父对象。cli-ordinary-final、cli-sanitized-final；全部临时XML/JSON删除 |
| 配置模型普通/插桩 | 各74通过，11源/133输入；共享include双投影、投影清单篡改拒绝、源漂移/未知实例、主/foreign文件准确Undo/Redo及所有旧回归。/tmp/getdaq-object-override-model-asan，严格C++11 ASan/UBSan/LeakSanitizer；Qt系统库未整体插桩 |
| 最终完整GUI | 74通过，0失败；87源/209输入。新操作路径/预览不写、相关兄弟3条/具体投影2条、输入撤审、实际空对象保存、准确撤销和只读门槛。首次完整运行73/1在旧realHttpOrdering的Run收尾超时；同版单项3/0、最终完整74/0，失败日志保留，不推断其原因 |
| 配置CLI插桩 | 35源/157输入，strict C++11 ASan/UBSan/LeakSanitizer；151进程对照真正执行新preview/apply与Snapshot。旧Qt/CCfg仍普通，不能称整条参考链完全插桩 |
| 原导出/快照/系统回归 | XML导出30进程含3旧导出、快照12独立SHA/篡改等、离线系统编译/无Qt读取绑定36全部通过；export-regression-final、snapshot-regression-final、results-system-final.txt。原参考文件保持，本轮未提供硬件执行夹具 |
| 构建/绑定 | 根qmake五程序最终构建成功，control-build-final.txt；tests/archive_object_override_validation.py核对真实旧依赖/对象及4个新Qt构建/程序/小型结果，build-object-override/checkpoint.json。前70.5的819/11/38及旧检查点仅作历史，本轮索引不是独立可移植重放包 |

详细操作、为何浅层覆盖不是物化、外部来源如何影响其他投影及失败处理见[对象覆盖手册](CONFIGURATION_OBJECT_OVERRIDE.md)与[命令对应表](CONFIGURATION_COMMAND_MIGRATION.md)。没有新增生产依赖、改变receiver或大体积传输，20GiB所有流总量上限/结束清理继续保持。仍为Ubuntu20.04.6/GCC9.4/Qt5.12.8；目标平台/真机、保留引用的多文件SaveAs/SaveAll、跨文件迁移与逻辑include隔离、真实加载/控制权/恢复、持续采集/断电仍未完成。

## 70.7 保留include的工作区副本与逐引用隔离（2026-10-10）

新增XcfgWorkspaceCopy、GUI **Review source workspace copy...**和CLI preview-workspace-copy/copy-workspace。向新目录复制主XML及全部加载include，shared每源一份、isolated逐引用复制其后续链；只改include属性为内部相对路径，保留其他原文。临时重解析和最终副本重载都要求有效对象/value/type/unit/range摘要一致、所有引用落在复制集合。源字节/图/目标/清单重算守卫，不覆盖目录，Linux文件/目录fsync，初始workspace.json最后写入，失败保留目录；可编辑初始清单不是不可变发布或加载凭据。

| 验证 | 最终同版结果与证据 |
| --- | --- |
| 模型普通/新代码插桩 | 各78通过，12源/135输入。两共享策略、嵌套相对引用/3与5文件、复制后浅层覆盖1/2投影、全部原来源不改、来源/输出/目标清单篡改拒绝、已存在目录和隔离扩展256文件边界在目标创建前拒绝；既有74配置场景继续运行 |
| 完整GUI | 76通过，0失败；88源/211输入，results-gui-final.txt。后台审阅不写目标、模式切换撤Save、来源映射3/2行、隔离复制后打开/历史清空、只读副本/版本变更拒绝及全部旧回归 |
| 工作区CLI普通/插桩 | 各19进程，cli-ordinary-final和cli-sanitized-final；程序36源/159输入。清单各文件大小/SHA、有效摘要、全部引用约束在副本内、不覆盖、错误不创建、移动原目录后重新加载、隔离/共享1/2投影和编辑后初始清单自然失配、原文件不改；全部自有XML/JSON/目录清理 |
| 插桩范围 | 模型/CLIstrict C++11 ASan/UBSan/LeakSanitizer，系统Qt未整体插桩；GUI普通。私有临时候选文件会同步，计为配置验证I/O，不称DAQ吞吐 |
| 对象/导出/快照/系统回归 | 原真实模型对象redo/undo/redo16场景/151进程、XML导出30含3旧导出、快照12独立SHA/默认/篡改、离线系统编译/无Qt读取绑定36全部通过。object-regression-final、export-regression-final、snapshot-regression-final、results-system-final.txt；旧参考Qt/CCfg普通，无硬件执行夹具 |
| 构建/检查点 | 根qmake五程序构建成功，control-build-final.txt；tests/archive_workspace_copy_validation.py核对5个Qt构建及实际程序/结果，build-workspace-copy/checkpoint.json。70.6的908/12/42已因新输入变化历史保留；当前索引不是完整可移植重放包 |

当前复制限制每XML8MiB、复制总量64MiB、最多256文件/32引用深度/10000边/16MiB清单，isolated按实际复制展开计数，不绕过预算或自动退成shared。复制范围不包括profile/硬件描述/固件/二进制/YAML/数据或scalar字符串内的其他路径。允许另存后继续编辑，初始清单不绑定后续字节，也不自动复用原工件/ECC接受版本；effective一致仍可能有不同来源/snapshot/编译工件摘要。

此阶段接续多文件新目录副本及主动来源隔离，不等于原文件就地搬迁/多文档SaveAll/跨文件事务或全部旧内存副本接口等价。详细操作、完整/失败标志、清单与发布区别及复制范围见[工作区副本手册](CONFIGURATION_WORKSPACE_COPY.md)。原来源/source/MUSIC保留，数据接收器未改，本轮无大体积原始传输，所有流20GiB总上限/结束删除保持。仍为Ubuntu20.04.6/GCC9.4/Qt5.12.8，本轮未验证实机/目标平台/实际加载/控制权恢复/生产持续吞吐或断电，整套未完成。

## 70.8. 接收进程明确失败后的后台生产停止（2026-10-10）

本轮补固定 receiver_stop_policy、同实例/计划/会话/输出目录/PID 的失败匹配、一次 receiver-held 意图及保留采集对象的负向停止。后台只读取元数据；普通 sender 不刷新/断开，不自动重启、回收、封存 receiver 或完成 GUI Run。父 ECC 保留原状态/操作 ID 并保持 UNKNOWN 与网络核对门槛。详细配置、状态解释及故障边界见[手册](RECEIVER_FAILURE_STOP_POLICY.md)。

普通本机实际进程验证已完成九个场景，分两组顺序执行：

- 两种 Ice 编码的 Running 接收进程信号退出。
- Paused、Ready、生产停止应答丢失、策略关闭。
- 正常 EOF/退出码 0、数据服务失联、ECC 服务重开。

第一组证据 build-receiver-failure/ordinary/receiver-failure-c_3wfbwe（1项）；第二组 receiver-failure-ebhro2_3（8项）。各组 scope.json、results.json、原始触发状态/观察及操作记录通过；成功/失败负向结果、原状态/会话/PID、无刷新/断开/Finish、禁止继续 Start、重复轮询和重开不重放均由实际进程夹具核对。正常退出、策略关闭和节点失联没有触发硬件写入。

纯协议普通及严格 C++11 ASan/UBSan/LeakSanitizer测试通过，覆盖非零/信号退出以及 ECC 实例/父快照/数据绑定、节点实例/计划、会话/目录/PID、busy/并发操作等不匹配；配置检查覆盖默认关闭、周期100..5000、错误类型和多余字段拒绝。配置检查使用本机模拟服务，没有原始传输或现场板端操作。

完整触发依据超过普通字符串预算时，编码必须显式声明元数据上限；普通 experiment 标量仍为4096字节。持久 receiver-held 的依据最多1 MiB字符串，重开重新解析、匹配原绑定并核对域摘要；预算错误或写入/接受失败保持门槛而不盲目发送停止。

最终原生 ASan/UBSan/LeakSanitizer 组三项（receiver-running-1、receiver-stop-loss、receiver-restart）通过，证据 build-receiver-failure/sanitized/receiver-failure-tt_wcu0u。新控制/夹具43次严格C++11编译，36个复用对象经原输入核对，381实际来源/头文件/生成输入摘要在构建前后及验证结束复核；记录 /tmp/getdaq-receiver-failure-native-asan/build-receipt.json 与 input-sha256.json。旧 Ice/GET/bzip 和系统 Qt 未整体插桩，receiver/数据服务明确沿用普通程序，不称整个DAQ或SDK插桩。普通兼容 CMake 含 GNU++11，不能称全库严格标准构建。

原有完整Run（normal-1）和后台告警停止（alarm-running-1）回归各1项通过，证据 build-receiver-failure/regression/receiver-failure-regression-02ahwxxx；全部6个 .graw/81920字节删除。当前重建的 fixed-ecc-configuration-tests 检查通过，记录 build-receiver-failure/fixed-final-vikzh323/test.log。纯协议插桩输出 /tmp/getdaq-receiver-failure-protocol-test.txt；普通输出 build-receiver-failure/protocol-ordinary.log。

两组九项保守总上界18 MiB，远低于每轮所有流合计20 GiB；实际删除16个 .graw/81920字节，remaining_raw=0。 三项插桩组上界6 MiB、删除6个 .graw/24576字节；两项正常/告警回归上界4 MiB。本轮正式传输组保守总上界28 MiB，全部组 remaining_raw=0。失败的早期调试组也通过 finally 清理。脚本只删除自身新建测试目录的载荷，保留小型日志/计数/SHA，不删除真实 Run 或历史原件。不是生产吞吐、实机、目标平台、跨物理主机或断电恢复验收。

本轮本地证据索引由 tests/archive_receiver_failure_validation.py 生成于 build-receiver-failure/checkpoint.json，逐份复核当前来源/程序/日志摘要、实际场景结果及清理证明。它是当前工作区索引，不是可移植部署包。此前工作区复制的230输入/13程序/44证据及旧原生整包仍保留作历史，不拿其中的旧输入摘要冒充本轮全部代码。

当时的GUI策略查看待办由下节接续；尚待观察与人工命令/告警的确定性并发、意图持久化中断、升级 ICE 源的接收故障组合，以及现场恢复与全局控制权/实际加载版本。新鲜健康退出和网络失联不能被解释为“数据完整”或“硬件健康”。

## 70.9. 操作台接收故障结果、原依据与未完成 Run（2026-10-10）

本轮接续70.8的后台停生产，增加纯C++11严格缓存解码、Qt认证异步查询、YAML/节点设置查看开关、状态表结果列和活跃Run的原缓存记录。GUI开关只查询，不启用后台策略；不发送自动Stop、重启、End或封存。详细步骤见[故障停采手册第8节](RECEIVER_FAILURE_STOP_POLICY.md)。

原生模式状态表15列，新增 Receiver stop result；提示包含原操作ID、触发接收器/PID/退出/信号/会话。当前触发标记要求父实例、数据节点实例/计划、会话/PID及当前状态时效同时匹配；过期、失联、错误绑定撤下旧正常结果，旧依据只作历史显示。确认过的控制故障不因下一份watching或查询失败解除。

活跃Run保存 native_receiver_failure_cache，标记 duration_incomplete=true，保留未完成阶段，不写run.csv或前进编号。新缓存解码核对字段、阶段、内部操作前缀、原Ready/Running/Paused对象和明确失败退出；正常/未确认退出不能伪造成已完成停止。重开可以展示原实例依据，但不将它当新实例的当前进程。

普通客户端23项/控制模型22项通过，含重复查询拒绝、错误数据绑定/坏JSON、保持门槛、无新增POST、失联/年龄撤下结果、换数据实例及换会话不标当前故障、Run记录和配置往返。客户端与控制模型新代码严格C++11 ASan/UBSan/LeakSanitizer分别23/22项通过；完整普通QWidget窗口76项通过，含实际列数/文本/停止ID提示/保持红色及过期撤下。系统Qt未整体插桩；不要把模型测试称为目标平台或人工GUI验收。

最终实际Qt RunControl进程→认证ECC→两个数据服务→原生receiver，两种Ice编码顺序联调均通过，记录 build-receiver-failure-gui/integration/receiver-failure-qt-v6s_8cf2：在GUI真实Running后夹具终止原PID，后台只停生产，回传完整依据/唯一停止ID；GUI保存未完成Run和不完整时长。初版夹具已收到有效结果，但读取较大Run缓存时仍用4KiB普通解析预算导致后置检查失败；失败组raw已清理。测试读取现在显式采用有界元数据预算，没有扩大普通配置字段或绕过验证。

后端接收热路径未修改，未新增数据进程Qt/Ice/yaml等依赖。root五实际程序重建；普通兼容CMake含GNU++11，严格标准/插桩范围是明确记录的新纯协议及Qt构建，不称旧SDK或整个DAQ插桩。此前70.8的392输入/7程序/109索引及70.7工作区索引随本轮公共协议/GUI变化只作历史；本轮索引是当前工作区校验，不是可移植部署包。

本轮六份Qt凭据：普通/插桩客户端各25来源/149输入，普通/插桩模型各80/204，实际Qt探针79/203，普通完整窗口88/212；根五实际程序最终重建。源码/程序/生成输入、最终日志和联调/清理的本地索引为 build-receiver-failure-gui/checkpoint.json，由 tests/archive_receiver_failure_gui_validation.py复核。

用户每轮全部流20GiB/结束清理约束保持。实际Qt联调每组保守4MiB，窗口回归使用私有TMPDIR、offscreen和240s期限，按唯一模拟器64KiB/200ms/单CoBo及小型mock文件计算256MiB组上界；全程只删除本轮私有目录的生成载荷，不删除真实Run/原件。 最终联调4个.graw/16384字节删除，窗口QTemporaryDir析构和外层私有目录核对无剩余raw；早期失败组和被后续代码替代的成功组同样清理，不作为最终同版证明。

剩余：观察与人工命令/告警的确定性竞争、接收故障意图持久化中断、升级ICE源组合及现场控制权/实际加载/硬件恢复、实机/目标平台/跨物理主机/持续吞吐和断电。整套重构未完成。

## 70.10. 接收故障与升级 ICE 源线程停止组合（2026-10-10）

本轮接续70.8/70.9，使用现有生产源控制/环形读出实现，验证接收进程退出→一次负向停止→原板端与原ICE源线程分别处理。没有修改接收热路径或新增运行依赖；Qt仍只作低频前端。详细语义及命令见[接收故障手册第9节](RECEIVER_FAILURE_STOP_POLICY.md)。

普通实际过程四项通过：ice-receiver-running-0、ice-receiver-running-1、ice-receiver-stop-loss、ice-receiver-restart，分1+3项两组顺序执行。证据build-ice-receiver-failure/ordinary/receiver-failure-1foo4a2q与receiver-failure-243vlapx。实际ECC/两数据服务/两个ICE receiver独立进程，源服务/SourceRingReadout/SourceMfmRing真实实现嵌入夹具，不称独立现场源进程或板端运行。

每来源两个6144字节环形缓冲各写一个4096字节MFM帧；读指针发布完成且消费线程仍运行后才终止原receiver PID。负向结果核对：

- 原父状态/操作、源实例/receiver_session及接收PID/session保持，正向Start拒绝。
- 两路源state=unknown/local_state=9、failure_stop_issued/acknowledged=true，消费线程退出并join、设备停止回调确认。
- 每来源issued_bytes/locally_sent_bytes均8192且前后不变；不Drain/补数据/Finish/End，不回收或封存receiver。
- 板端停止应答丢失时，负向操作保持failed，但两路源仍分别停止；重复轮询和重开不重发板端写入/源停止。
- 健康另一来源也保留UNKNOWN核对状态，不能因线程已停止就解除父故障或把数据判完整。

最终严格C++11 ASan/UBSan/LeakSanitizer原生控制/源实现/夹具组同四场景通过，记录build-ice-receiver-failure/sanitized/receiver-failure-kp42yggr。43次新严格编译、36个依赖核对复用对象、381实际输入在构建前后和验证结束SHA核对；凭据/tmp/getdaq-ice-receiver-failure-native-asan/。ICE receiver使用已有真实插桩程序/tmp/getdaq-source-mfm-ice-asan/getdaq-ice-receiver，运行依赖含asan/ubsan而无Qt；数据服务普通、系统Qt/旧Ice/GET/bzip未整体插桩。受害receiver由SIGKILL终止，没有该进程自身退出期LeakSanitizer证明，不称全部旧SDK或所有进程泄漏验收。现有源实现的callback顺序/失败保持此前已有单元及本机证明，这里补组合真实进程边界；不把它称为大流量/DMA/真实设备停止或持续生产中丢失字节量的证明。

普通与插桩各四项、各8MiB保守总上界，正式两组总上界16MiB；各8.graw/32768字节全部删除，所有组remaining_raw=0；状态/计数/SHA/线程及无End/封存的小型凭据保留。用户每轮所有流20GiB/结束清理约束继续有效；只清理自身私有测试目录，不删除真实Run/参考原件。

本轮本地索引build-ice-receiver-failure/checkpoint.json由tests/archive_ice_receiver_failure_validation.py复核普通/插桩实际程序SHA、来源/源实例/会话/字节计数及线程标志、部分失败/重开/清理记录。此前GUI228输入/16程序/49索引的夹具/文档随本轮变更只作历史，原程序与已验证逻辑不冒充本轮全代码；当前索引不等于可移植部署包。

后续继续接收故障与人工命令/告警的确定性并发、意图持久化中断、持续源背压/半帧在途组合、真实设备与源进程部署、目标平台/多物理主机/断电/持续吞吐及控制权/加载核对。整套重构仍未完成。

## 70.12 主机端统一构建与目录交付（2026-10-11）

本阶段按用户澄清保留板端 getHwServer/IceEirfu，完善主机软件交付入口；没有改动原始数据接收算法或板端程序。70.11 对应接收观察与手动停止的并发检查尚未形成完整最终记录，不能由本阶段替代。

- 根 `GetDAQ.pro` 新增原先遗漏的 `getdaq-config-upload`，六个 qmake 子工程共用 `GETDAQ_DESTDIR`；Qt Creator 默认仍输出工程 `bin/`，统一构建另选独立目录。
- `tools/build-suite.py` 为 Python 标准库构建工具，提供 control/data/data-host/full 四种 profile；读取指定 Qt 5 Kit，Qt 相关 CMake 接同一安装根。构建时不下载依赖、不安装系统软件、不启动服务。`tools/build-linux.sh all/control/data/supervisor` 已接同一入口；独立 ecc/experiment/ice 构建保留。
- 本机新的 `build-delivery/data/` 完成 receiver、inspect、supervisor-query、catalog 四程序，实际动态依赖无 Qt/Ice/yaml/Boost/curl；新的 `build-delivery/full/` 完成全部 23 程序，含六操作工具、主机 ECC/硬件控制、数据管理、配置仓库及可选 ICE 数据工具。实际默认 receiver 不链接 Qt/Ice，可选 ICE receiver/send 不链接 Qt。各程序 SHA、构建命令、输入 SHA 与 Linux ldd 在对应 `build-receipt.json`，状态 complete；旧 build 目录没有用于复用这些编译结果。
- 程序目录包 `build-packages/getdaq-data-20261011/`（106 个清单文件）及 `build-packages/getdaq-host-20261011/`（125 个清单文件）完成新目录复制与文件哈希核对。清单包含实际程序、文档、示例和许可证，不包含现用认证/Run/原始载荷；系统 Qt/libc/libstdc++ 没有打包，源码与 SDK 仍须另外保存。程序目录中的源码跳转需要原源码工程，不宣称包内具有所有源码。
- `tests/host_suite_delivery_test.py` 三项通过：原生 data 构建计划不引入 Qt/Ice 配置；实际模板与许可证复制、已有目标拒绝、程序篡改识别；不完整目录拒绝。复制检查使用小文件夹具，不能据此替代上述真实编译。
- 包内 13 个程序 `--help` 正常退出，catalog 缺参数按接口返回 2 并给出使用方法。GetDAQ 在 offscreen 下成功打开初始窗口、保存临时截图并退出；临时目录和截图已删除。凭据在 `build-delivery/full/package-smoke.json`。最初错误地对 catalog 要求 `--help` 返回 0，已按该 CLI 的既有接口修正检查；没有把参数拒绝误算作编译失败。
- 本轮没有传输 DAQ 数据、创建 `.graw` 或执行硬件控制；20GiB 每轮合计限制未消耗。没有新增平台、真机、长时间吞吐或内存插桩验收结论。实际环境仍为 Ubuntu 20.04.6/WSL2、GCC 9.4、Qt 5.12.8；Ubuntu 24、Qt 5.14.2 MinGW、macOS 待验证。

逐步构建、程序作用、输出目录、Windows Qt Creator/MinGW、部署限制和错误处理见 [主机套件构建部署](HOST_SUITE_BUILD.md)。编译成功不表示整套重构完成或现场可直接上线。

## 70.13 同机新 ECC 的板卡占用保护（2026-10-11）

- `HostControlOwnership` 为无Qt/Ice的严格C++11/POSIX核心：在既有0700目录取得按规范IPv4地址去重的flock，锁文件0600、无符号链接/硬链接、CLOEXEC，持有期覆盖ECC所有硬件客户端。部分获取失败释放已取得的占用，锁文件不删除；内容/身份/权限变化保持故障。该机制只协调共用目录的同机新ECC，不宣称板端、旧软件或跨主机全局排他。
- 固定输入增加可选 `host_control_ownership.directory`，进入输入/父快照绑定；省略保持原快照行为。启用时从硬件与sender直连TCP代理取得IPv4资源，取得占用后才创建审计根和ECC日志。固定输入离线检查不创建占用目录/审计根，也不访问硬件。
- ECC命令绑定检查核对占用完整性，发现变化拒绝硬件动作、保留UNKNOWN/需要核对；恢复文件不清故障。认证只读 `GET /v1/ownership` 和原生HTTP客户端方法返回缓存/实例/父快照/显式scope；`board_global_lock_verified`与`cross_host_lock_verified`始终false。尚未新增Qt专用查看页面，没有实现实际加载核对或完整硬件恢复。
- 核心普通CMake构建与独立 `-std=c++11 -pedantic-errors` ASan/UBSan/LeakSanitizer运行通过，结果在 `build-host-ownership/core/result-final.txt` / `result-sanitized.txt`：同地址不同部署拒绝、不相交资源、部分获取回退、独立进程拒绝、析构重新取得、子进程SIGKILL后内核释放、非法地址/符号链接拒绝、篡改与恢复后故障保持。插桩覆盖新占用核心/父测试；被SIGKILL子进程没有自身退出时LSan证明。
- 最终 `native-ecc-coordinator-tests --scenario host-ownership` 实际父协调器验证占用冲突不创建新ECC审计根、不调用硬件，内容变化在Describe前拒绝并保持父UNKNOWN；原对象释放后另实例取得占用。`normal-host-ownership`实际执行完整正常模拟控制流程，测试替身数据生命周期，没有真实receiver/封存或DAQ。结果分别为 `build-host-ownership/native-ecc-result-final.txt` / `native-ecc-normal-result.txt`，同名proof/root记录保留；通用结果文案已区分无硬件调用和模拟硬件流程。
- `run_host_control_ownership_tests.py`最终根 `build-host-ownership/service-final/host-ownership-bzjn37ng/`：固定输入/发布/选择回归通过，实际独立ECC服务认证缓存查询通过，错误凭据401，第二实例因占用拒绝且未创建其审计根，第一实例终止后第三实例取得占用；`proof.json`保留两实例、scope和零DAQ/无硬件命令证据。启动服务使用旧fixture的已验证静态输入，未执行Describe/Prepare；不能把取得新锁称为恢复原硬件。
- 本轮不传输DAQ数据、不启动raw receiver、不创建.graw；进程均已终止，仅保留小型锁/日志/元数据，20GiB合计预算未消耗。普通父ECC/网络服务实际链接验证通过；没有对系统Qt或旧Ice SDK整体插桩，也没有新增Ubuntu24/Windows MinGW/macOS、真机、跨主机或长期性能验收结论。

逐步部署、共用目录、固定配置、认证查询、锁释放与UNKNOWN边界见 [主机占用手册](HOST_CONTROL_OWNERSHIP.md)。第70.12构建包与384输入收据在本轮修改后属于当时历史版本；不拿它们代表新增占用代码已经编译。

本轮最新完整主机套件已重新构建23程序：`build-delivery/full/build-receipt.json`的386输入与23程序当前SHA核对通过；`build-packages/getdaq-host-20261011-ownership/`的126清单文件核对通过。包内Release ECC程序再次通过实际localhost服务检查，证据为 `build-host-ownership/release-service/host-ownership-j13oyqxs/proof.json`与`release-service-result.txt`，同样无硬件命令和DAQ；原生receiver没有新增Qt/Ice链接。该包冻结本轮打包时的文档，源码此处另补最终证据位置，不反写已有程序包清单。

## 70.14 操作台占用查看、连接代次与真实只读客户端（2026-10-11）

- 新增可在Windows/Qt客户端复用的纯C++11 `DecodeEccControlOwnership`：64KiB以内严格字段、实例/父快照、同机scope、缓存/非全局声明、规范目录和最多256个排序去重单播IPv4资源；错误类型、重复/非法地址、伪全局/实时声明、矛盾占用状态及超限拒绝。`ecc-protocol-tests`通过，结果为 `build-ownership-gui/protocol-result.txt`；没有Qt/Ice/POSIX头进入此解码器。
- `EccSystemClient::QueryOwnership`复用认证异步GET，核对原实例/父快照，同实例启用状态/目录/资源必须保持；仅故障状态变化可接受，已知故障保持客户端控制门槛，后续正常缓存不解除。网络失败只撤回观察，不伪造全局控制权或自动提交Stop/解锁；查询单独在途，不阻塞命令busy状态。
- native RunControl与主窗口转发只读查询并携带连接代次。native菜单 `ECC host board reservations...` 提供只读表格、手工刷新、打开时两秒刷新、关闭后停止定时查询；显示最后检查时占用/未启用/故障/查询失败，收到结果超过六秒在显示更新时标为过期。数据地址与当前确认在失联时撤回，断开/重连/配置切换前的延迟回复拒绝；所选父快照另作核对。未引入新的YAML开关，不调用原始数据接口。
- 同版严格C++11客户端普通及ASan/UBSan/LeakSanitizer均 **27 passed/0 failed**，分别25编译源/149输入，程序及输入收据在 `build-ownership-gui/client/` 和 `client-sanitized/`。新增用例检查读前身份、单在途、零POST、资源/peer/伪全局拒绝和故障后正常状态不清门槛；系统Qt未整体插桩。
- 完整offscreen QWidget回归 **77 passed/0 failed**，88编译源/212输入。新页面检查同机/非全局文案、只读、目标地址、过期、故障、失败撤回和断开后旧代次回复拒绝；结果根为 `build-ownership-gui/widget-evidence/receiver-policy-widgets-xwl5pkp3/`。运行器合计预算256MiB，正常Qt临时目录析构与失败清理保护生效，生成raw清理且剩余零；不能推广为真实平台/人工完整操作验收。
- 实际QCore客户端探针增加 `--ownership-only`，不改变原Start探针默认行为；同版23编译源/147输入构建通过。实际Qt客户端→独立ECC服务的根 `build-ownership-gui/integration/host-ownership-o8wt8k4r/` 中 `qt-ownership.json` / `qt-probe.log` 核对原实例与父快照，网络日志无accepted/complete/failed控制请求。服务冲突/释放与认证回归通过，proof标记qt_readonly_client_checked=true、硬件命令false、DAQ零；该真实客户端为QCore探针，不冒充实际窗口全链路或板卡验收。
- 第70.13的18输入检查点和23程序包在本轮协议/前端修改后仅为历史版本；本轮新增源码不以旧程序/旧SHA宣称已编译。核心占用/硬件算法及默认原始receiver数据通路未修改；没有新增板端全局或跨主机控制权、实际加载版本、完整恢复、目标三平台或持续吞吐证明。

完整主机23程序已按当前代码重新构建，386输入和23程序SHA在 `build-delivery/full/build-receipt.json`；当前新目录包为 `build-packages/getdaq-host-20261011-ownership-gui-final/`，126清单文件。先前ownership-gui包保持原字节；final包使用同一批程序并加入最终说明文档，没有重复编译或反写旧包。操作步骤与显示含义见 [占用查看手册](HOST_CONTROL_OWNERSHIP.md#6-qt-操作台怎样查看)。整套重构仍未完成。

## 70.15 Configure 历史寄存器读回证据（2026-10-11）

- 新纯C++11 `configurationreadback` 核对完整硬件日志与成功执行对象保留的SHA256记录链，8MiB/4096步骤/有界记录数；并检查原计划、begin/结果顺序、期望/掩码/允许差异、写应答、延时、只读观察及读改写旧值/计算/应答/读回。`HardwareAudit`在记录成功写入/fsync后更新摘要，`HardwareClient`只在成功完整Execute后保存摘要；Validate或部分执行不提供当前完成摘要。原日志字节和硬件RPC顺序未因此增加伪读回。
- SystemPreparation捕获活动CoBo `cobo-configure` 和MuTaNT `mutant-configure`，核对文件身份/权限/单硬链接和原执行摘要，记录每板覆盖计数与两种摘要。普通写紧邻同目标/值/完整掩码零差异比较、显式读改写完整比较才计入writes_readback_verified；其余应答写入、管理器应答、单纯观察和差异警告分别保留，不提升成全部加载验证。
- Native ECC缓存绑定新Configure操作ID；启动not-observed、新配置pending、硬件证据captured或失败前无完整证据unavailable。认证 `/v1/configure-readbacks` 返回原实例/父快照/参数/工件和当前父状态，仅读缓存、不读硬件/原始文件。captured不保证receiver启动/连接或父Ready成功；历史/live=false/actual_loaded_configuration_verified=false保持。重启不自动接管旧加载状态，完整恢复和独立板端版本仍未实现。
- `getdaq-config-check readback-audit AUDIT_FILE TRUSTED_COMPLETED_CHAIN_SHA256` 为标准C++11无Qt/Ice离线检查，按原执行可信摘要核对文件并输出计数；与实际已捕获CoBo记录字段完全一致，结果 `build-configure-readback/offline-check.json`。自己从待检查文件现算摘要不能代替原执行绑定。
- 独立核心普通及严格C++11 ASan/UBSan/LeakSanitizer通过，结果 `build-configure-readback/core/result-final.txt` / `core-sanitized/result.txt`。夹具检查保留摘要绑定、完整/未完整写入、读改写与只读观察、顺序、精确64位、缺尾/未知操作/溢出/差异拒绝；没有硬件或DAQ。插桩为标准库实验核心与测试，不是系统Qt或旧Ice整体插桩。
- native协调器两种Ice编码的完整正常模拟流程通过：`native-1-final.log` / `native-0-final.log`，对应proof/root记录给出原测试根。每次Configure捕获2 CoBo/3 MuTaNT主计划，首次保存 `controller/configure-readbacks-test.json`，再次Configure ID更新。样例CoBo461写应答/296完整读回/351期望比较，165写入仍未完整读回；MuTaNT69应答/41完整读回/44比较/1读改写，包含另外的只读观察。数据生命周期明确为替身，没有真实receiver或现场板卡证明。
- 初次完整测试发现MuTaNT具有多记录的显式读改写及只读读取，不能使用“每步骤固定begin/result两行”的错误假设；按真实记录补齐核对后，两编码最终流程均通过。失败旧根保持诊断，不能算最终成功证明。
- 实际网络根 `build-configure-readback/network/receiver-failure-regression-_q6o6prn/normal-1/`：独立ECC/两个数据服务/实际receiver完整正常小帧流程通过，认证readbacks identity/参数/工件/操作ID核对，`configure-readbacks-configure.json` / `configure-readbacks-configure-second.json`显示对应两次主计划；历史/不全量加载声明保持。运行器本轮合计上限2MiB，成功/失败清理保护，最终.graw已删除、剩余零；不是持续吞吐、目标平台、真机或独立物理测量验收。

完整主机23程序按本轮代码重新构建；当前full输入与程序SHA见 `build-delivery/full/build-receipt.json`。当前目录包 `build-packages/getdaq-host-20261011-readback-final/`包含23程序、127清单文件，使用同一批程序并加入最终说明文档；早期readback包保留原字节，不反写。操作、范围、接口、计数字段、可信摘要与失败处理见 [读回证据手册](CONFIGURE_READBACK_EVIDENCE.md)。此前前端398/27/24检查点在共享实验核心和后台代码修改后仅为历史；不能以旧程序代表本轮代码。整套重构仍未完成，尤其是实际当前加载版本、全局控制权、完整恢复与现场验收。

## 70.16 MUSIC 风格的 Qt Configure 读回查看（2026-10-11）

- 新 `configurationreadbackwindow.h/.cpp/.ui` 提供 native 菜单 `Configure register readbacks...`，展示原操作/实例/参数绑定、当前原生父状态，以及每板写应答/完整读回/未完整读回、比较/警告、只读/读改写、管理器/延时、文件/摘要。精确64位开始时间和原执行摘要在提示中保留；表格只读，不显示“当前全部加载”的绿色确认。
- 页面可见时两秒认证GET、关闭停止定时查询，接收缓存超过六秒后在显示更新撤回板卡表；失联、无效回复、断开/切换配置/重连撤回并丢弃旧代次。历史检查仍可在父UNKNOWN时查看，但不清控制门槛、不Stop/恢复/接管，也不经过原始数据通路。完整网络/数据门槛仍按主窗口status检查。
- 共享纯C++11 `DecodeEccControlConfigureReadbacks` 严格限制1MiB/195硬件条目/4096步骤，核对实例/父快照/参数、历史/非实时/非全量加载声明、阶段/原操作、唯一板卡和安全审计文件、精确64位及覆盖计数一致性。硬件ID与实验后端规则一致，接受合法UTF-8/空格；操作ID仍使用原ASCII规则，不能混用。便携协议回归通过，`build-readback-gui/protocol-result-final.txt`保留结果。
- 最终Qt客户端普通与严格C++11 ASan/UBSan/LeakSanitizer各 **31 passed/0 failed**（各26编译源/154输入），结果 `client/test-final.log` / `client-sanitized/test-final.log`。检查身份先于查询、单在途、零控制POST、参数/错误声明/矛盾计数拒绝及父UNKNOWN被后续正常缓存不清除；系统Qt未整体插桩。
- 最终完整offscreen QWidget回归 **78 passed/0 failed**（90编译源/218输入）。新窗口确认历史范围、未完整写入、精确时间、只读表格、过期撤回、父UNKNOWN、失联、旧代次和所选快照不符。根 `build-readback-gui/widget-final-evidence/receiver-policy-widgets-ox0jhoj9/`，整轮预算256MiB，生成raw已清理、剩余零；旧先验编译/结果不当本次最终证据。
- QCore探针新增可选 `--readbacks-only`，保留原Start和ownership默认模式。当前24源/152输入探针→实际独立ECC服务根 `build-readback-gui/integration-final/host-ownership-thmibtre/`：`qt-readbacks.json`核对固定身份/参数及初始not-observed缓存，服务网络日志无控制接受/完成事件；proof标记qt_readback_client_checked=true、硬件命令false、DAQ零。此为真实客户端初始缓存联调，不声称实际窗口→已完成硬件Configure全链路或真机证明；实际非空读回记录的网络证据在70.15。
- 此轮未改变硬件执行算法/摘要或receiver数据路径；没有新增三目标平台、板端当前加载、全局控制权、完整恢复或长期性能验收。70.15的395/31/16检查点在前端/共享协议更新后仅为历史，不能以旧SHA冒充最新程序。

完整23主机程序按当前代码重新构建，当前输入/程序摘要在 `build-delivery/full/build-receipt.json`；最新目录包为 `build-packages/getdaq-host-20261011-readback-gui-final/`，127清单文件。final使用同一批程序并加入最终文档，不反写先前程序包或重复编译。操作步骤及显示边界见 [读回页面手册](CONFIGURE_READBACK_EVIDENCE.md#7-qt-操作台查看页面)。整套重构仍未完成。

## 70.17 Windows Qt 5.14.2 MinGW 实际构建与运行目录（2026-10-11）

- 实际工具为用户 `D:\Software\QT\5.14.2\mingw73_64` 和匹配 `Tools\mingw730_64`，经 WSL 调用真实 Windows qmake/moc/uic/g++/make，六个操作端 Release 程序构建通过；`build-windows-qt5142/windows-build-receipt.json` 为complete，392项输入和六个PE x86_64输出SHA核对一致。不是 Linux Qt 编译结果推断 Windows 兼容。
- UNC绝对源码路径在生成的GCC参数中丢失前导斜线；入口以pushd临时映射构建目录，qmake传相对工程路径，SUBDIRS输出使用映射盘绝对路径。未修改用户SDK、注册表或板端软件。
- 本 Kit 的 windeployqt `--release` 过滤全部平台插件：其PE检测将MinGW插件判为debug，插件实际导入Qt5Core.dll/Qt5Gui.dll。自带Qt5.14.2源码说明MinGW debug检测不可靠；使用自动模式后部署成功，保存SDK/编译器许可证并检查匹配运行库与主要非系统DLL导入。未从其他Qt安装混入插件。
- 运行包将DLL/插件/模板/文档纳入SHA清单，末尾才撤下BUILD_INCOMPLETE；`--verify`无需执行Windows工具，核对全部已列文件、六程序和PE架构，并拒绝未列文件。最终交付目录为 `build-packages/getdaq-windows-qt5142-20261011-final/`，其具体文件数/摘要以manifest.json和本轮checkpoint为准。
- 实际帮助命令覆盖六工具，PATH只含Windows系统目录、清空Qt插件变量；GetDAQ通过包内offscreen插件生成初始截图。离屏截图在此Kit未绘制文字，不能声称其文字渲染通过；另以正常qwindows插件短暂启动/截图/退出，实际截图文字与初始Simulation/Disconnected状态已查看。全部未连接硬件或执行控制命令。
- 实际Windows getdaq-config对两个Board共享include、Module继承、整数范围及中文字符串值生成快照，独立getdaq-config-check重新核对语义摘要并保持hardware_loaded_verified=false。夹具与JSON在 `build-windows-qt5142/offline-configuration/`，文件路径为ASCII；中文值通过不代表原生Unicode路径已验。
- 没有DAQ传输或.graw，用户20GiB总预算未消耗。构建/初始窗口和小型离线配置证明不覆盖完整人工GUI、Qt自动交互回归、独立干净Windows、真实网络/板端、Windows持久性/Unicode路径、Ubuntu24/macOS或持续性能。操作与故障诊断见 [Windows手册](WINDOWS_QT5142.md)。整套重构仍未完成。

## 70.18 多文档配置工作区与 Save All（2026-10-12）

- 核对原ConfigWidgets `Control/Workspace.cpp`的saveAll：遍历dirty模型逐个save，并不是跨文件事务。新增XcfgWorkspace和MUSIC风格configurationworkspacewindow.h/.cpp/.ui，接入配置窗口；最多16根，共享规范路径来源只留一份，内存编辑、组合暂存、全根有效差异审阅，再Save All。
- 对全部根与声明核对XML/include/继承/类型/范围，改动include后脱离根树的脏来源也单独验证；不为每个已经可达的共享来源保留重复完整树。输入预算256来源、单文件8MiB、集合64MiB，不宣称进程RSS上限。原始receiver/事件导出与板端代码未改，没有新增第三方依赖。
- 保存绑定工作区代次、完整待写字节和各根报告；所有临时QSaveFile先准备，禁用direct-write fallback，逐次commit前核对磁盘原字节。拒绝外部变化、旧/篡改审阅和无效组合；部分成功列出saved/remaining并保持需重新打开，不自动重试或恢复硬件。没有跨文件原子性、持久化事务日志或断电恢复证明；本轮没有实际注入中间commit失败，不能把预备失败测试推广为所有部分失败场景已验。
- 普通与严格C++11 ASan/UBSan/LeakSanitizer核心各84项通过（13编译源/144核对输入），最终完整QWidget80项通过（92源/223输入）。包括多根共享单次写、编辑未提前写盘、两来源值/范围组合、无效组合不留部分暂存、脱离根树来源验证、旧/篡改审阅、外部改动拒绝以及第二目录不可写时第一个文件也不替换。GUI另检查共享根报告、CRLF未编辑不变、再编辑撤回保存、非法XML不写和连接模式只读。
- 当前沙箱下完整GUI的10个旧localhost夹具因socket禁用失败，LeakSanitizer因ptrace退出；原句柄全部终止后，在允许本机环回/LSan的环境运行最终版本通过。失败输出只作诊断，不计通过。系统Qt未整体插桩，完整GUI为普通构建。
- 实际Qt5.14.2 MinGW严格C++11构建并运行同一配置核心：83项通过、1项Unix目录权限测试跳过，0失败；TEMP/TMP固定工作区，程序和144输入已绑定。Qt5.14 QSKIP宏在严格MinGW模式出现空变参错误，改为同库qSkip函数调用，不放宽语言检查。该Windows测试使用SDK PATH，只证明核心测试；最终运行包另外执行移除SDK PATH的启动检查。Windows完整新工作区人工交互与ACL权限故障仍需验收。
- 完整Linux23程序已按本轮396输入重新构建，Windows六操作程序按397输入重新构建，分别有程序SHA/架构/依赖收据。新目录包为 `build-packages/getdaq-host-20261012-workspace/` 和 `build-packages/getdaq-windows-qt5142-20261012-workspace/`；具体文件数和哈希以最终清单为准，证据入口 `build-config-workspace/checkpoint.json`。70.17的392输入检查点及此前Linux405输入检查点因本轮源码变化仅为历史，旧包字节不反写。
- 新Linux包129清单文件，Windows包224清单文件（含本轮逐程序help/screenshot输出）。初次无控制台启动GetDAQ --help出现Qt系统MessageBox而超时；根据SDK qcommandlineparser.cpp核对原因，已确认并终止该自有测试进程，失败目录清理后重做。最终使用PowerShell显式重定向句柄、每个自有子进程10秒等待与超时清理；六程序help及offscreen/qwindows初始截图在无SDK PATH环境通过，运行包完整性核对通过。该初始截图不覆盖Windows新工作区人工交互。
- 核心和Windows专项只用小型XML，窗口回归合计预算256MiB、最终自有.graw已清理；无新增硬件或高速DAQ验收。操作步骤、保存失败边界见 [多文档手册](CONFIGURATION_MULTI_DOCUMENT.md)。统一树命令/跨文件历史、结构迁移、持久化事务、更广泛旧库接口、完整UNKNOWN恢复及目标平台/真机仍需继续完成。

## 70.19 已失败接收会话受控终止（2026-10-12）

- 新增无Qt/Ice管理核心AbortFailedSession(path,expectedSession)，及schema1 supervisor新命令abort-failed-session；expected_session_id必需且进入request_key，其他命令不得携带。服务在接受前核对会话，不匹配返回412且无新操作意图；最终HTTP场景另通过真实失败Finish建立控制门槛，再执行原会话负向清理。原有规范请求不新增字段；现有认证/实例/计划/控制者/快照/epoch/一次操作绑定保持。
- 核对原Ready/Failed、完成启动、原会话及完整子进程/source映射；必须已有非零异常退出或生命周期Failed。waitid WNOWAIT观察仍自有的进程，存活者须保持原输出锁；审计意图落盘后对全部存活自有进程一次SIGTERM，再有界等待并回收。健康/正常EOF会话、未完成启动、错误会话、无控制者归属、储存漂移、丢失子进程或旧进程接管拒绝。没有SIGKILL回退/信号重发、Finish/source End、删raw或解除UNKNOWN。
- cleanup成功仍Aborted/needs_reconciliation，服务控制门槛保持；complete仅是该负向操作完成。此前seal_verified只保留已核对的值，不使整套数据complete；未封存计数仍是原生命周期缓存。重开服务不认领旧PID，原服务同ID同请求只返回记录，新ID也不能重放本会话的已尝试动作。
- tests/run_failed_session_abort_tests.py普通与管理核心/控制服务ASan/UBSan/LeakSanitizer各通过6个实际进程场景：新TCP/FDT/ZBUF失败会话终止，以及原正常TCP/FDT/ZBUF封存/目录流程；另有规范协议测试和真实认证服务/生产getdaq-supervisor-query联调。HTTP覆盖未归属/错误控制者/错误会话拒绝、允许原所有者在故障门槛下负向清理、原PID/会话回收、帧字节保留、无封存凭据、同ID无重发、禁止新Start及重启不接管。
- 每轮保守总输入/输出预算16MiB，两轮分别删除557056逻辑raw字节（包括正常归档硬链接名称计数），剩余.graw零；测试凭据删除，小型元数据和日志保留。测试先证明产品没有删raw，再由夹具清理；不能把夹具删除当产品清理策略。当前测试均localhost合成帧，receiver是普通已验证原生程序；插桩范围是本轮管理核心/查询CLI/控制服务与便携实验核心，系统Qt不整体插桩，SIGKILL故障receiver不提供退出LSan证明。
- 完整23主机程序已按当前396输入重新构建，默认原生receiver和query仍无Qt/Ice/yaml/Boost/curl动态依赖；原始接收/写盘/导出、板端及Windows GUI代码未改。本轮Linux包 `build-packages/getdaq-host-20261012-failed-session/`，最终清单/证据以manifest.json及 `build-failed-session/checkpoint.json`为准。70.18整组输入检查点因后台/协议修改只作历史，归档证据与旧程序包保持。
- ECC/Qt专用清理入口、实际ICE失败会话、卡死/真实EIO/断电、多物理主机及全系统UNKNOWN核对仍待完成；NativeEcc VerifyRecovery继续拒绝，GUI Run Recover仍仅恢复原Run关系。本步骤是恢复前的自有接收进程清理，不扩张成全部故障恢复或现场验收。操作与失败处理见 [受控终止手册](FAILED_RECEIVER_SESSION_ABORT.md)。

## 70.20 ECC 与操作台失败接收器清理（2026-10-12）

- 新增ECC命令abort-failed-receivers，仍是原客户端实例/快照/epoch的一次操作，不属于普通九个状态迁移。NativeEcc保留原成功负向停生产的失败操作/停生产操作及inode/权限/完整字节审计证明；必须同一保留采集对象、UNKNOWN、匹配原失败操作，才允许清理。审计失败保持storageFault，恢复原字节不重开门槛；清理尝试后不换ID重放，不调用Reconcile或恢复Ready。
- RemoteReceiverManager在首次POST前核对全部节点原实例、scope/epoch、成功Start时保存的source/PID/session/output_directory，拒绝新会话/冷重启/部分启动接管。父意图全部持久化后逐节点最多一次POST、原ID GET观察；已确认正常封存的原会话记录跳过，不改变seal，其他必须原会话Aborted/待核对/全reaped并以新鲜状态再次核对。独立节点失败保留其他结果，存储故障停止后续请求。
- supervisor新增可选parent_failure_operation_id，进入request_key并仅允许原失败会话命令，present空串/非法ID拒绝；原没有此字段的请求规范编码保持。原认证所有者可声明全系统失败，清理尚健康的兄弟接收器；下层保留parent_failure_independently_verified=false/hardware_stopped_verified=false，不把声明当独立硬件证明。ECC入口负责上述原停生产审计守卫。
- 操作台新增NativeReceiverAbort确认菜单，typed nativeHardwareOutcomeUnknown控制健康/断开/忙碌禁用。NativeRunControl与客户端额外要求新鲜原UNKNOWN/已保持故障、无pending/换身份/凭据故障；取消不提交，确认一次提交，原ID观察。存在Run journal时保存native_receiver_cleanup_operation及duration_incomplete，不Complete/CSV/递增Run、重启或解除UNKNOWN。
- 实际独立ECC→两数据服务→普通原生receiver→模拟板端，两Ice编码receiver-cleanup-0/1通过；核对原实例/PID/会话、全部回收、无seal、无新增硬件动作、父/data未知门槛保持及同ID仅观察。实际receiver-cleanup-stop-loss与receiver-cleanup-audit通过：停生产失败/证据被改时数据服务清理POST零，恢复证据后仍零POST并保持失败。每两场景整轮保守4MiB，生成.graw最终清理。
- Qt客户端普通与严格C++11 ASan/UBSan/LeakSanitizer各31项通过（26源/157输入），最终完整普通QWidget80项通过（92源/223输入），覆盖原身份/UNKNOWN、新鲜状态与凭据拒绝，健康禁用及实际取消/确认按钮。最初夹具用QMessageBox::done未设置clickedButton，确认按取消返回；改为真实标准按钮click后完整回归通过，不把失败组计为通过。窗口轮保守256MiB、raw清理，系统Qt未整体插桩。
- 本轮重新编译的数据管理核心/服务/CLI另验证6个原实际TCP/FDT/ZBUF场景与真实认证HTTP/原生CLI，并增原父声明清理健康兄弟接收器及不提升为硬件证明的检查；receiver普通，整轮16MiB，raw与测试凭据删除。当前Native/backend网络专项普通，未新增其整体插桩结论。没有真实窗口→现场硬件/完整Run journal的全链路人工验收。
- Linux完整23程序与实际Qt5.14.2 MinGW六程序已按当前源码构建；新交付目录为build-packages/getdaq-host-20261012-ecc-cleanup与getdaq-windows-qt5142-20261012-ecc-cleanup，最终文件数/源程序及证据SHA以manifest和build-ecc-cleanup/checkpoint.json为准。默认receiver/query仍无Qt/Ice/yaml/Boost/curl动态依赖；原始接收/写盘/导出和板端源码未改，Qt只传元数据。70.19及70.18旧全组检查点因后台/协议/前端更新仅作历史，归档证据和旧包保持字节。
- 原停生产证据匹配只是本对象负向流程核对，不是板端全局控制权/当前全量加载或恢复完成。部分启动、冷重启孤儿接管、ICE原始接收专项、物理多主机、卡死/EIO/断电和完整UNKNOWN核对仍待接续，Native VerifyRecovery继续拒绝。操作、声明边界与结果说明见 [ECC/Qt清理手册](ECC_FAILED_RECEIVER_CLEANUP.md)。整套重构仍未完成。

## 70.21 生产窗口清理全链路与已封存节点保留（2026-10-12）

- `tests/native_window_cleanup_probe.pro`复用完整生产窗口、UI和控制源，92个C++源/223项追踪输入。Linux Qt5.12.8 offscreen运行真实MainWindow，点击Connect、状态推进和StartRun进入Running；测试监视器只查询ECC状态，实际控制请求由生产窗口信号/工作线程发出。两种Ice编码`qt-window-cleanup-0/1`均通过。
- 独立ECC、两个真实数据服务与原生receiver保持原实例/会话/PID/输出目录。夹具仅杀死自己的一个测试receiver，自动策略完成负向停生产后，真实QMessageBox取消按钮产生零请求，确认按钮只产生一次请求；服务器接受日志核对准确命令次数，不仅计数Qt信号。所有原receiver随后reaped且不制造seal，无新增硬件控制动作。
- 实际Run journal保存`native_receiver_cleanup_operation`的complete结果，但采集仍Interrupted、duration_incomplete；保留原Run编号、无完成CSV、父ECC和数据待核对保持。此处complete仅是清理操作结束，不是Run完整或UNKNOWN恢复。证据`build-window-cleanup/integration/real-window-cleanup-jbgl7_fv/`。整轮输入/输出保守预算4MiB，实际删除16384逻辑raw字节，raw残留零。
- `native-ecc-remote-receiver-tests`另增加`failed-cleanup-sealed-0/1`：直接调用原远程管理器，真实TCP截断导致一个节点Finish失败，同时独立另一节点正常封存；清理只向失败节点发送一次，封存节点零次，原PID/session、seal与已封存raw字节保持，审计明确记录node_already_sealed/no_post_sent，再次清理拒绝。此专项未经过ECC清理入口、未独立核实硬件停生产，不扩展Breakup后ECC恢复承诺。证据`build-window-cleanup/sealed-peer/sealed-peer-cleanup-n1nnq5m1/`；整轮4MiB保守预算，32768逻辑raw字节删除，raw残留零。
- 两个新runner按自有进程组停止后清理，仅删除唯一测试目录中非symlink的.graw，成功/失败/可捕获SIGINT/SIGTERM均走finally。不能保证SIGKILL或掉电运行finally；产品代码仍保留生产原始数据。fixture新增可选NEW_ROOT，以便失败前即能定位并清理自己的数据目录。
- 本轮生产程序未改：现有Linux396项构建输入、Windows397项输入及六程序映射仍核对一致，不重复构建或复制交付包。新增源码与当前六个测试/服务程序、独立证明的SHA入口为`build-window-cleanup/checkpoint.json`。70.20旧检查点中的测试fixture输入和程序已变化，整组仅为历史；原归档证据/交付包不修改。
- 本轮普通测试，无新增整体ASan/UBSan/LSan/TSan、Qt系统库插桩或性能结论。没有Windows完整窗口联调、Ubuntu24/macOS、现场板端/物理多主机、冷重启/部分启动、卡死/EIO/断电验收；完整UNKNOWN核对仍未实现。原始数据传输不进入Qt，整套重构仍未完成。

可复现命令（从源码根目录运行；仅用于本机合成服务测试）：

```bash
python3 tests/build_qt_control_validation.py tests/native_window_cleanup_probe.pro build-window-cleanup/probe --jobs 2
# 避免与正在运行的构建/测试共用输出。
cmake --build build-delivery/full/native --target ecc-network-service-tests native-ecc-remote-receiver-tests --parallel 2
python3 tests/run_native_window_cleanup_tests.py build-delivery/full/native/ecc-network-service-tests build-delivery/full/receiver/getdaq-receiver build-delivery/full/native/getdaq-supervisor build-delivery/full/native/getdaq-ecc-server build-window-cleanup/probe/native-window-cleanup-probe build-window-cleanup/integration
python3 tests/run_remote_sealed_cleanup_tests.py build-delivery/full/native/native-ecc-remote-receiver-tests build-delivery/full/receiver/getdaq-receiver build-delivery/full/native/getdaq-supervisor build-window-cleanup/sealed-peer
```

## 70.22 Breakup 后封存失败的清理接续（2026-10-12）

- 修复原采集对象已Broken、同一准备对象已Prepared，但独立数据节点Finish失败时无法继续负向清理的缺口。此时stop-production-after-failure仅验证原全部生产停止结果、原本地发送器断开结果及新鲜板身份/停止寄存器，不重复Stop/Disconnect/Breakup，不退回普通Stop，不清UNKNOWN。
- 只读核对全部CoBo firmwareEnable/intEnable/initDone/mutantMode为零及MuTaNT run为零；活动CoBo pulser/firmware/interrupts及各MuTaNT必须已有原成功停止结果。没有原停止历史即拒绝；任一读回失败保持本对象故障，恢复数值后仍拒绝。历史pulser结果与寄存器读回不是独立全局所有权或实际全量加载证明。
- 普通底层六场景已通过，两Ice编码各覆盖正常核对、寄存器不符/恢复后保持失败、没有原Stop历史；包含CoBo/MuTaNT，核对没有新写入或发送器动作。既有hardware-system-acquisition-tests的两编码、独立共享缓冲CoBo及禁用主触发初始化回归通过；夹具故意产生的stopPeriodicPulser异常属于既有拒绝测试。
- 普通实际ECC→两数据服务→原生receiver链路broken-cleanup-0/1及broken-cleanup-readback通过：一来源截断导致封存失败，另一来源独立封存；正常清理只向失败节点发一次、已封存节点零次，原会话/PID/seal/raw保持、原partial-finish与UNKNOWN保持，硬件/发送器动作不重放。读回不符及恢复数值后均拒绝，数据清理POST零。证据build-broken-cleanup/network/receiver-failure-7yxig70q；整轮6MiB预算，49152逻辑raw字节已删除、残留零。
- 最终版本另覆盖broken-cleanup-readback-again：先取得成功停止依据，再制造读回矛盾；后一次核对失败即撤销前一次依据，恢复寄存器仍失败、数据清理POST零。新核对前必须验证旧停止审计，不能用新ID替换被修改的旧依据；已有receiver-cleanup-audit拒绝回归同时通过。最终普通整组五场景证据build-broken-cleanup/network-final/receiver-failure-20y0xj34，10MiB保守预算，73728逻辑raw字节删除，残留零。
- 严格C++11 ASan/UBSan/LeakSanitizer底层六场景实际运行通过；真实插桩ECC与插桩夹具的broken-cleanup-readback-again网络反例也通过。旧Ice/GET库与系统Qt未整体插桩；网络中的data service/receiver使用普通构建，不把本轮提升为全栈插桩或吞吐证明。插桩网络证据build-broken-cleanup/network-sanitized/receiver-failure-1qvk6f06，2MiB预算，16384逻辑raw字节删除、残留零。三轮网络共删除139264字节，每轮远小于20GiB。
- Linux完整23程序已按当前396输入构建；最终普通联调实际使用的fixture/receiver/data service/ECC程序SHA与构建后的原路径仍一致。Windows用户实际Qt5.14.2 MinGW六程序按当前397输入构建通过，六程序字节与此前完整运行目录逐个一致，因此沿用getdaq-windows-qt5142-20261012-ecc-cleanup的DLL/初始运行证据，不重复复制或声称新增Windows全功能验证。新Linux交付目录为build-packages/getdaq-host-20261012-broken-cleanup。
- 当前源码、普通及插桩程序、实际运行结果和小型归档证据入口build-broken-cleanup/checkpoint.json。70.21/70.20旧整组检查点因后台输入/程序变化仅历史，原归档/旧包保持。完整UNKNOWN恢复、冷重启/部分启动、ICE原始数据接续、全局控制权/实际全量加载、断电和现场及目标平台验收未实现/未验证；整套未完成。

操作和条件见[ECC失败接收器清理](ECC_FAILED_RECEIVER_CLEANUP.md#21-breakup-已完成但数据封存失败)。

## 70.23 多文档来源组的统一撤销与重做（2026-10-12）

- `XcfgWorkspace`新增跨来源Undo/Redo。每次成功StageSources只记录实际改变文件的修改前后完整UTF-8字节；值与共享范围可作为同一组修改，不按单文件顺序留下非法中间态。恢复前核对当前字节，再通过既有全根/全部声明/include/继承/范围校验；失败不移动历史位置或保留部分暂存。
- Save All成功后保留历史，撤销只使旧字节成为新的内存待保存内容，不自动写盘；必须重新审阅并Save All。Undo/Redo增加审阅版本，字节恢复相同也不能复用旧审阅；无实际变化的审阅不增加历史、不删除Redo，合法新分支才截断Redo，非法分支保持原记录。
- 已打开新根也参加恢复校验；如果旧组合使新根非法，撤销拒绝，根不会因撤销被关闭。外部磁盘变化保持拒绝覆盖；部分Save All/不确定保存仍冻结继续编辑和历史。当前是多文档来源文本历史，与声明树编辑命令统一、跨文件结构迁移和持久化磁盘事务仍待完成。
- 历史限制32组/128MiB修改前后字节，按记录保守计数、整组淘汰最早历史；源集合仍64MiB，单文件8MiB。这不是Qt进程RSS预算；没有新第三方依赖。预算测试实际使用2MiB配置注释并进行36次修改及全部可用Undo/Redo，不生成DAQ载荷。
- MUSIC风格`.h/.cpp/.ui`窗口增加Undo/Redo按钮，未审阅标签文本时禁用整组历史，防止丢掉编辑器内容；历史切换用QSignalBlocker同步全部相关标签，撤回旧Save All审阅。连接模式只读。优化为每次敲键只更新当前来源的待审阅集合，避免扫描全部256个标签。
- 最终Linux Qt5.12.8核心普通与严格C++11 ASan/UBSan/LeakSanitizer各88 passed/0 failed（13源/144输入）；完整普通QWidget81 passed/0 failed（92源/223输入）。新增回归覆盖组合值/范围、保存后不写盘的撤销、旧审阅ABA拒绝、新根非法恢复、外部漂移、非法分支不丢Redo、预算淘汰、实际窗口按钮与本标签文本撤销。系统Qt未整体插桩。结果位于build-workspace-history/core、core-sanitized和widget-final-evidence/receiver-policy-widgets-rb_h7a4_。
- 实际Windows用户Qt5.14.2 MinGW严格C++11构建并运行同版核心，87 passed/0 failed/1 skipped；跳过的是Unix目录权限用例，不替代Windows ACL验收。初次cmd零返回且stdout空不算通过；UNC TEMP实际造成85项创建临时文件失败，该组不计通过。改为维持临时盘符到进程退出、私有TEMP/TMP、显式Process句柄等待及QtTest独立文件报告后最终通过；证据build-workspace-history/windows-core/results-mapped.txt、mapped-result.json及run-mapped脚本。Qt DLL/SDK未改。
- Linux完整23程序和Windows六操作程序分别按最终396/397输入构建；当前Linux包build-packages/getdaq-host-20261012-workspace-history，Windows包getdaq-windows-qt5142-20261012-workspace-history，Windows匹配DLL、移除SDK PATH的帮助/初始离屏与qwindows窗口、清单SHA通过。初始主窗口证明不覆盖Windows多文档窗口的完整人工操作或现场服务联调。
- QWidget两轮各256MiB保守输入/输出预算、临时.graw均最终清理；核心与Windows核心仅生成临时配置文件，无DAQ传输。原始receiver/写盘/导出、板端和ECC故障逻辑未改。当前源码/程序与小型归档证据入口build-workspace-history/checkpoint.json；70.22及旧工作区检查点整组因操作端/配置输入变化仅历史，原归档/旧包不改。Ubuntu24/macOS、真实板端/吞吐、完整UNKNOWN恢复、冷重启和跨文件事务继续未完成，整套未完成。

操作、保存后撤销的含义及容量边界见[多文档手册](CONFIGURATION_MULTI_DOCUMENT.md#21-标签内撤销与整组撤销有什么区别)。

## 70.24 原生归档副本与跨文件系统复制（2026-10-12）

- 独立无Qt `getdaq-catalog`新增--copy-catalog SOURCE NEW_DESTINATION及--verify-transfer DESTINATION。只处理原已完成/可核对硬链接归档，逐文件64KiB分块复制到新独立inode，源数据/原seal/原Run/ECC不改；复制不进入Qt或ECC线程，不新增网络传输协议或GUI任务按钮。
- 源归档及原receiver目录锁、请求/完成凭据/清单/文件集合/身份先核对；拒绝已有目标、目标位于源归档/原接收会话中或锁冲突。先写意图和历史元数据副本，保存本次产生的目标清单/文件身份，fsync后完整读回目标SHA-256，再核对原来源和本次产生的元数据未改，最后独占发布transfer-complete.json。中断/读写/同步/身份故障保留部分结果，不自动删源/覆盖/重试；不是断电原子目录事务。
- 新Sha256Stream保留一个64字节摘要块，旧ConfigurationSha256函数及规范输出不改，无OpenSSL等新依赖。32个标准/块边界组合覆盖空串、abc、55/56/63/64/65字节和百万a，并与旧实现及abc/百万a已知摘要对照；finished后追加/重复Finish拒绝。原归档reader显式采用65536项数组预算，修正默认4096限制与文件数预算矛盾；控制协议默认JSON限额不变。
- 实际两个不同st_dev（普通文件系统与/dev/shm）的原生CLI复制及独立校验通过；原目录临时不可访问仍可核对副本，目标inode独立/nlink=1、字节完全相同。七个普通及严格C++11 ASan/UBSan/LeakSanitizer场景覆盖同大小目标损坏、源变化、中断、配对修改目标/清单摘要、锁冲突及隔离子进程RLIMIT_FSIZE导致真实写失败。限额子进程用_exit只证明写失败/无完成、不提供该子进程自身LSan退出结论。
- 旧归档27项回归及4097个额外空文件的清单容量检查普通/插桩通过。真实DataSupervisor和两个原生receiver接收合成TCP帧、EOF/排空/reap/seal/硬链接归档后复制并校验，通过且仍可启动新独立接收周期，原归档保留。初次实际锁兼容失败发现receiver锁为0640而非夹具0600；改为沿用“不允许group/other写入”的原锁约束，新传输锁仍0600，最终重跑通过。管理核心/复制CLI插桩，receiver普通，未提升全栈插桩或实机证明。
- 最终普通证据build-catalog-transfer/final-ordinary/catalog-transfer-bvj3vnnp，插桩证据final-sanitized/catalog-transfer-_3qw_jzu。每轮保守输入/输出/硬链接名称计数4GiB、小于20GiB，私有目录及catchable SIGINT/SIGTERM finally清理raw；原生RAII夹具正常/异常自行清理，写失败子进程现场由其父夹具处理。首轮失败也清理raw，不计通过。SIGKILL/断电不保证finally；产品保留真实数据。
- 配置核心当前同版普通88项回归通过，确认既有配置摘要契约未变；Linux完整23程序/398输入、实际用户Windows Qt5.14.2 MinGW六程序/399输入构建。新包Linux getdaq-host-20261012-catalog-transfer及Windows getdaq-windows-qt5142-20261012-catalog-transfer使用当前程序与文档；Windows只验证操作端构建/匹配DLL和初始运行，不运行POSIX bulk-copy、不宣称本轮完整Windows交互验收。默认raw receiver/query/copy CLI无Qt/Ice/yaml/Boost/curl依赖。
- 原生复制/归档core与CLI、实际普通/插桩运行及小型归档SHA入口build-catalog-transfer/checkpoint.json。70.23旧整组检查点因新增原生源/公共摘要声明变化仅历史，原归档/旧包保留。这里的SHA核对不是签名、seal以来无人改源、实际物理事件正确或硬件恢复证明；Ubuntu24/macOS/现场磁盘/NFS/持续吞吐/断电、跨主机传输、GUI/ECC复制任务与完整UNKNOWN恢复仍未完成，整套未完成。

操作、空间要求、完成证据及失败处理见[复制手册](CATALOG_TRANSFER.md)。

## 70.25 数据主机原生复制任务、认证接口与操作台窗口（2026-10-12）

本轮在70.24独立跨盘复制上接入任务管理，Qt只发送和接收元数据。纯C++11 `TransferJobs` 固定来源/目标/部署/原生程序摘要，保存意图后使用posix_spawn启动getdaq-catalog子进程；复制子进程负责raw及内容校验。新窗口采用MUSIC的.h/.cpp/.ui组织，从主窗口菜单打开，不修改ECC/Run状态。

已执行的验证：

- 普通原生控制器及实际复制子进程：来源摘要/实例拒绝、互斥根、同ID重复观察不重复启动、改请求拒绝、独立目标内容核对、完成历史加载、未完成历史unknown且不重放、原请求损坏拒绝及策略故障保持。
- 严格C++11 ASan/UBSan/LeakSanitizer执行同一原生控制器与实际插桩复制子进程通过；范围为新原生核心/子进程，系统Qt和旧依赖不作整体插桩声明。
- 实际localhost supervisor认证HTTP：无凭据401、错误结构400、错实例409、正常202及原ID GET、重复ID不新增子进程、原接收控制状态保持。故意不读取接受响应后断开，再以原ID观察完成并核对没有重放；服务重启保留完成历史，未完成历史unknown且禁止新任务。
- 生产TransferWindow→真实supervisor→真实原生复制子进程：真正QMessageBox取消不生成ID/不创建任务，Yes只提交一任务并按原ID看到完成；错误agent禁止新复制。最后完整offscreen QWidget回归81通过/0失败。窗口构建94个源/228输入；初始失败的夹具/探针构建不能作为通过证据，最终结果使用下述final-network-evidence。
- 实际用户Windows Qt5.14.2 MinGW x86_64六个操作程序编译通过，Qt任务窗口已包含在GetDAQ；Windows不提供POSIX复制子进程，复制在数据主机运行。本轮不是Windows任务窗口人工操作或远程现场验收。

证据入口：`build-transfer-jobs/final-network-evidence/transfer-jobs-fx7p8n1t/result.json`与window-result.json、cleanup.json；插桩原生为 `build-transfer-jobs/sanitized-evidence/transfer-jobs-12c3y2h0/`；完整窗口为 `build-transfer-jobs/widget-evidence/receiver-policy-widgets-0trisq89/`。每轮传输夹具保守64MiB、窗口256MiB，均小于20GiB，最终自有raw为零。传输来源是合成、已seal的文件系统夹具，本轮没有真实板端或真实receiver启动证明；实际receiver采集到副本的证明属于70.24，不混为本轮新增证明。

独立原生程序ldd只有系统线程/标准C++/libgcc/libc/libm，无Qt/Ice/OpenSSL/curl。常规进度更新约每秒一次；这不是吞吐、RSS峰值或实时带宽验收。关闭窗口不停止复制，普通服务退出等待自有子进程，强杀/断电没有正常退出持久性结论。最多256任务、一任务在执行、无排队/自动重试/续传/删源/冷启动PID认领，未完成重启需人工核对。跨主机数据协议、完整UNKNOWN硬件恢复、全局控制权/实际加载及Ubuntu24/macOS/真机长期性能仍待。

详细步骤与原理见 [跨盘复制任务](CATALOG_TRANSFER.md)第9节。统一完整主机与Windows构建/交付收据另外记录最终输入和文件摘要；旧70.24整组收据只表示当时源码，不当作本轮同版证明。

## 70.26 多文档工作区的结构编辑与统一历史（2026-10-12）

新WorkspaceStructureWindow按MUSIC的.h/.cpp/.ui组织，工作区选择根/来源声明并预览十四种结构操作；使用已经审阅、尚未保存的内存来源，核对所有打开根后接受到原StageSources历史。文本/结构共用Undo/Redo及Save All，不另外直接写盘。原单文件结构编辑器仍保留实际磁盘来源检查，私有内存入口只有工作区可以调用。

- 普通配置核心105通过；严格C++11 ASan/UBSan/LeakSanitizer同一核心105通过。包括十四操作各自内存预览/暂存/准确字节Undo/Redo且磁盘不变，混合文本与共享结构历史、另一根范围拒绝、过期预览及Undo恢复相同字节后仍拒绝旧代次、导入暂存版本、外部输入漂移拒绝。早期对象覆盖因旧磁盘检查失败，修正后完整105组通过；早期失败不计完成。
- 实际生产工作区模态窗口选择声明、预览、暂存、文本/结构连续Undo/Redo以及重新审阅Save All通过；完整offscreen窗口回归82通过/0失败。窗口构建95源/231输入，普通核心13源/149输入；系统Qt未整体插桩。
- 用户实际Windows Qt5.14.2 MinGW六操作程序构建通过，包含新窗口及共享算法。新的独立Windows核心目录保留pushd映射到子进程退出、私有TEMP/TMP、显式进程句柄等待和QtTest独立输出；104通过/0失败/1既有Unix权限跳过。不是Windows新窗口人工交互、独立干净目标机或macOS/Ubuntu24验收。
- Linux完整23程序构建通过，后续新包绑定构建输入及文件摘要。测试核心只有自动清理的临时XML；完整窗口轮上限256MiB，自有raw最终零。没有接板、硬件命令或原始数据传输路径变化，不增加第三方依赖。

证据入口：build-workspace-structure/core与sanitized的build-receipt.json/source-sha256.json/tests.log；窗口为build-workspace-structure/widget-evidence/receiver-policy-widgets-03samkrc；Windows核心为build-workspace-structure/windows-core/results-mapped.txt及mapped-result.json；实际Windows构建收据仍在build-windows-qt5142，Linux统一构建收据仍在build-delivery/full。70.25整组检查点已因配置/窗口源码变化成为历史，不延用为当前SHA证明。

功能范围见[多文档操作与原理](CONFIGURATION_MULTI_DOCUMENT.md)第8/9节及[命令迁移表](CONFIGURATION_COMMAND_MIGRATION.md)。统一仅在一个工作区内，不共享独立单文件窗口的全局历史；跨文件声明搬移、跨文件持久化事务、数据库/网络include及更广旧接口、实际硬件加载/完整UNKNOWN恢复仍未完成。Save All部分失败仍冻结，恢复内存不是磁盘事务回滚或硬件回滚。

## 70.27 工作区跨文件声明移动（2026-10-12）

在70.26的统一结构窗口增加现有对象/标量跨文件移动。PreviewMove生成删除后的源和插入后的目标，literal include按目标目录重写但仍绑定原依赖文件，候选组合验证所有已打开根；StageMove重算准确字节/报告/代次后，通过同一StageSources把两文件记成一组历史。没有自动写盘、硬件调用或原始数据处理。

- 普通配置核心109通过，严格C++11 ASan/UBSan/LeakSanitizer同一核心109通过（13源/149输入）。新增覆盖两目录移动、双嵌套include逆序重写、中文/特殊字符依赖及误匹配属性保护、已暂存依赖版本、第三根共享影响、保存后三文件实际写入与仅内存Undo。还覆盖非空对象插入、字符引用空白/原注释/原子项保持、重复声明/标量目标/文件根/同来源拒绝、篡改候选/ABA旧代次/外部漂移拒绝、新include环不部分暂存。
- 实际生产工作区模态窗口选择源/目标、双文件完整审阅、暂存/整组Undo/Redo/重新审阅Save All通过；完整offscreen窗口83通过/0失败，95源/231输入。系统Qt未整体插桩。
- 实际Windows Qt5.14.2 MinGW六操作程序编译通过；独立核心目录使用映射TEMP与真实进程等待，QtTest实际108通过/0失败/1既有Unix权限跳过。包含上述新中文路径/跨目录include及移动测试；不宣称所有Unicode路径或新窗口Windows人工交互已验。
- 每轮窗口上限256MiB，自有raw最终零；核心只有临时XML，没有接板、DAQ持续吞吐或Qt原始数据链路变化。功能不增加依赖。

证据入口：build-workspace-move/core与sanitized的构建收据/输入SHA/tests.log；窗口build-workspace-move/widget-evidence/receiver-policy-widgets-sglte0tf；Windows核心build-workspace-move/windows-core/results-mapped.txt和mapped-result.json。Linux完整主机与Windows运行包另由统一构建收据及清单绑定；70.26整组检查点因源码更新只作历史。

范围见[移动操作与原理](CONFIGURATION_MULTI_DOCUMENT.md)第10节。只移动现有声明，不移动文档/include根，不生成新文件或自动合并/改ID，不进行文件路径重命名。移动改变父级继承/共享属性时必须审阅实际有效差异，不能把保持依赖引用当作全语义等价。双文件仅内存成组原子接受，Save All仍逐文件提交；中途失败/强制退出可留下混合磁盘版本，需先备份并按已保存/剩余结果人工核对。跨文件持久化事务、断电恢复、全局历史、更广旧接口、完整硬件UNKNOWN恢复、跨主机/目标平台/真机性能仍未完成。

## 70.28 配置Save All持久化记录与人工恢复（2026-10-12）

Save All在准备全部来源临时文件后，先创建独立原始/候选副本、摘要manifest和涉及来源的待核对标记，再逐文件提交和同步。全部实际字节核对成功后发布完成记录，保留原始/候选证据并清理自有标记。工作区和原单文件来源检查阻止绕过未完成标记。恢复窗口只对原始/候选二者中的当前来源进行审阅，明确选择完成候选或回退原始，默认取消，不自动重放。

- 普通配置核心117通过，严格C++11 ASan/UBSan/LeakSanitizer同一核心117通过，14源/154输入。覆盖完成记录与原始/候选副本、混合提交后两个恢复方向、活写者拒绝、已完成记录不重放、第三种内容/候选篡改/损坏载荷拒绝、观察器异常后第一文件已同步/剩余文件及冻结/恢复、256记录上限在源替换前拒绝、单文件编辑器不能绕过待核对来源，原109项完整回归保留。
- 实际独立保存进程在第一文件commit+同步之后立即_exit(77)，不调用析构，观察到a为候选、b为原始且无完成记录。另一个进程使用同一记录取得失效写者锁后完成候选、发布完成并移除来源标记。只临时XML且上限4MiB，不声称真实断电或立即退出子进程的LSan清理。
- 生产恢复窗口展示准确当前/目标来源文本及摘要，真正QMessageBox取消不写源、不完成；Yes才写选定方向。只读模式允许审阅、禁止恢复。最终完整offscreen窗口84通过/0失败，97源/237输入，系统Qt未整体插桩。
- 实际Windows Qt5.14.2 MinGW六操作程序编译通过；新的独立核心目录QtTest实际116通过/0失败/1既有Unix权限跳过，包含上述恢复/写者门槛及保存数据检查。实际进程句柄等待与映射TEMP保持；新窗口Windows人工交互、Windows ACL及目录断电持久性未验。
- 当前Linux完整23主机程序414输入、Windows六操作程序415输入构建通过；对应运行包另绑定文件清单与摘要。窗口轮上限256MiB，最终raw零；原始DAQ/板端代码不变，无新第三方依赖。

最终证据：build-workspace-recovery/core与sanitized的构建收据/来源SHA/tests.log；final-crash-evidence/save-crash-lkdozqo0的result.json和实际进程日志；final-widget-evidence/receiver-policy-widgets-a5qzvp8j的窗口回归与raw清理；windows-core/results-mapped.txt/mapped-result.json。70.27与70.26整组检查点因配置代码更新只作历史，旧包保持。

详见[Save All恢复操作与原理](CONFIGURATION_MULTI_DOCUMENT.md)第11节。每个恢复根最多256记录、保留载荷2GiB；记录原始与候选逻辑合计128MiB，无自动删除。文件/目录POSIX同步，Windows采用文件_commit；不宣称实际断电、EIO或所有文件系统持久性。流程提供可核对的继续/回退，外部读取者仍可能看到中间混合版本，不是全旧/全新跨文件原子可见性，也不提供跨机器路径重映射、被外部删除文件恢复或恶意同用户篡改防护。完整硬件UNKNOWN恢复、实际加载/全局控制权、跨主机数据及目标平台/现场验收仍未完成。

## 71. 仍需完成的验收

| 范围 | 尚未验证/尚未实现 |
| --- | --- |
| Windows GUI | Qt 5.14.2 MinGW六程序实际构建、含DLL/插件运行目录、无SDK PATH初始窗口及离线配置已通过（70.17）；仍需完整人工/交互功能、独立干净机器、Unicode文件路径和现场服务联调 |
| Ubuntu 24.04 | 新工具链的干净构建、正式文件系统与网卡测试 |
| macOS | GUI/原生程序构建，目录 fsync/flock 与网络行为 |
| ECC/SOAP | 两种请求XML参数序列化形式已由本地Qt回归解析并核对语义（70.86）；实际现场服务样例、服务端操作顺序与读回仍待 |
| ECC/Ice 后端 | 固定HTTP、九命令原生协调库、工件/接收计划/操作绑定、本机/远程数据生命周期、GUI入口、原对象明确停止，以及可选后台告警普通运行/暂停/Ready/故障/重开/UNKNOWN和实际Qt缓存提示已有本机验证。告警意图崩溃/TSan、控制权/发布/实际加载版本、完整硬件恢复及受控跨归档查询仍待接或待验证；物理多主机、板端互通/校准与Ubuntu24工具链待验 |
| 源状态观察/故障停止 | 独立只读查询、短超时、身份/会话、UNKNOWN及一次源停止和Qt显示已有本机证明；慢源HTTP409/状态响应、调试器式暂停跨实际Reset释放、Connected/Running原控制连接丢失已验证。实际硬件控制权/完整接管恢复、真实设备停止与接收持久性仍不能由观察证明，见[检查点](SOURCE_OBSERVATION_CHECKPOINT.md) |
| 配置编辑 | 共享属性/来源、范围/等价、scalar及继承对象浅层/相关兄弟覆盖、原十种结构操作/物化及本地引用/改URL/导入、会话撤销/重做、XML副本/具体有效子树导出、保留include的工作区新目录复制/逐引用隔离/明确另存后打开、快照/发布/仓库/ECC选择已有本机验证；旧文件/XML与Spirit192场景、原导出3组及实际旧模型对象覆盖redo/undo/redo16场景对照。多根来源文本SaveAll已有70.18验证，工作区树/文本共用历史已由70.26接续，现有声明跨文件移动已由70.27接续；文件路径迁移已于70.68实现并由71.113在当前源码复验；任意外部读者可见的跨文件持久化原子事务、更广泛词法/Text/Binary/全部接口、数据库/网络资源、运行切换和实际加载仍待完成；见[命令表](CONFIGURATION_COMMAND_MIGRATION.md)。新工作副本隔离不等于全部旧内存副本接口等价；64位/大小写扩展及严格拒绝单列，不称全旧库兼容 |
| 数据兼容 | 独立FDT、受管理TCP/ZBUF、ECC受管理FDT重连/明确未使用来源及GET文件名样式/固定计划/归档已有本机验证；独立ICE接口/显式结束已有本机验证；仍须真实各固件MFM、VxWorks ZBUF/Narval互通、FDT真实固件/旧Narval与下游互通、ICE真实板端源/服务部署、设备回调与现场互通、真实旧命名下游用法 |
| Run 边界 | 原生采集/接收生命周期/父ECC、GUI意图/本地同实例恢复和数据节点同文件系统Run归档已有本机验证；仍须实际固件/物理多主机、加载版本、硬件UNKNOWN核对、跨主机载荷传输/复制任务冷启动核对、受控原会话清理的更广场景（数据服务/CLI已有70.19，ECC/Qt保留对象入口已有70.20）和断电持久性 |
| 数据可靠性 | 私有tmpfs真实空间/inode ENOSPC、EFBIG/短写及指定I/O/封存错误处理、自有进程即时观察、ECC/GUI可选退出/失联/过期提示已有本机验证；自动故障联锁、卡死/连续健康判断，生产文件系统/真实设备EIO与fsync、断电、长期压力和端到端事件计数仍待验 |
| 性能 | 持续吞吐、压力下内存和队列峰值、多 CoBo 并发。本工作区每轮合计上限20GiB；内存大于该预算时不能声称完成超过页缓存规模的验收，需另行安排合适测试环境 |
| 在线分析 | 空闲连接回收/重连/队列上限及单向连接规则已有本机验证；真实下游长度/半关闭用法、跨主机恢复行为及是否需要可靠输出仍待验或待接 |

不能用模拟器通过替代上述项目；也不能因为接收器使用原生 socket 就宣称零丢失或达到某个 Gb/s 指标。

## 70.29 固定硬件控制位的认证只读观察（2026-10-12）

新增纯C++11有界观察协议、原生ECC固定控制位读取、认证GET `/v1/hardware-state` 和MUSIC形式的 `.h/.cpp/.ui` 操作窗口。读取之前核对Node名字、完整设备清单、实际描述和本地权限；只读CoBo的firmwareEnable/intEnable/initDone/mutantMode及MuTaNT的run。节点字段全部为0/1才显示，失败清空该节点所有值。Qt不参与原始数据传输。

- 普通原生和实际独立ECC HTTP两种Ice编码共四场景通过，证据为 `build-hardware-state/final-network-evidence/hardware-state-2em6gf4j/result.json`。认证HTTP核对身份/摘要、错误凭据401、名字不符撤回，以及查询不改变父缓存或硬件写入计数。
- 两种编码新增受控并发：实际Ice服务端字段RPC在测试夹具中等待，负向请求取得协调器命令锁，然后按原有UNKNOWN门槛拒绝。父状态/操作ID未改变，仍因动作代次变化撤回整批值；结果保存在native-0-race.json和native-1-race.json。这证明慢观察不占住协调器长期命令锁与过期撤回，不证明真实停采成功。
- 原生代码不跨硬件RPC或完整文件核对持命令锁；默认2000ms预算在调用间检查，连接/调用最多300ms。元数据高层方法可包含多次RPC，不能宣称两秒硬截止。`consistent`仅覆盖本机协调器代次/状态，不能当多节点物理原子采样、跨主机排他或硬件自身稳定证明。
- 最终完整offscreen窗口87通过/0失败，99源码/243构建输入，见 `build-hardware-state/final-widget-evidence/receiver-policy-widgets-5kqkz0_2/result.json`、widgets.log和payload-cleanup.json。新增精确采样时间显示、六秒自然到期撤回并保持只有两次认证GET；原错误/配置变化/UNKNOWN保持测试保留。窗口测试使用本地HTTP夹具；系统Qt未整体插桩，Windows新窗口人工交互仍未验。
- 同版Linux完整23主机程序/419输入和实际Windows Qt5.14.2 MinGW六操作程序/420输入构建通过，对应build-delivery/full/build-receipt.json与build-windows-qt5142/windows-build-receipt.json。构建成功不代表Ubuntu24/macOS/现场固件验收。
- 最终严格C++11 ASan/UBSan/LeakSanitizer原生/实际HTTP两编码四场景通过，见 `build-hardware-state/final-sanitized-evidence/hardware-state-dfgbc_xr/result.json`，包含上述受控并发。插桩构建161输入SHA已核对；旧Ice/GET静态库和系统Qt未整体插桩，HTTP联调中的两数据服务为普通程序，本轮没有启动任何receiver。先前w4ngd026只作未补并发/固定外部输入门槛时的历史证据。
- 两编码普通/插桩各验证外部固定配置来源修改后返回503；恢复原字节仍503，控制位读取和硬件写入均为0。每场景的hardware-state-input-fault.json保存该证据。服务在实际观察前后检查固定输入，不访问并发变化的操作日志容器。
- 两轮后端只涉及元数据，4MiB测试预算，无receiver/raw。固定观察范围不执行Stop/Reset/写寄存器；并发测试中的负向请求被状态门槛拒绝，无硬件写入。不是实机、板端改造或大流量吞吐验收。

操作窗口先查询身份，再查询同实例/快照/参数/工件观察，只有人工刷新，无自动硬件轮询；表格包含精确Unix毫秒采样时间。客户端收到六秒后撤回，错误或端点选择改变也撤回，UNKNOWN/需要核对保持。窗口协议夹具与实际后端测试分开，不声称窗口到现场硬件完整链路。

加载、全局控制权和恢复声明始终false；普通TCP/FDT旧发送器无远端状态getter，本地Fresh不能认定旧连接停止。完整VerifyRecovery仍默认拒绝。详细使用、字段与限制见[实际控制位观察](LIVE_HARDWARE_STATE.md)。整套重构、现场和目标平台验收仍未完成。

## 70.30 原复制任务冷启动后的独立原生完整核对（2026-10-12）

新增有界C++11核对请求、认证POST `/v1/transfers/reconciliations`、TransferJobs独立校验子进程，以及复制窗口 **Verify unresolved original copy...**。保留原复制request/实例/child记录，新的核对意图在启动前保存；只运行原生 `--verify-transfer 固定原目标`，不重新复制、接管或kill旧PID。全部内容/凭据通过并可靠保存后才将原任务核对为complete；其他unknown任务仍阻止新复制。

- 最终普通实际原生/HTTP及生产窗口通过：`build-transfer-reconciliation/final-network-evidence/transfer-jobs-kzeont8s/result.json`。覆盖旧实例/错误原请求key/未认证/畸形请求拒绝、同verification ID不启动第二个校验、原复制child.json不变、来源归档目录移走时目标独立校验、核对完成后恢复新复制，以及多个unknown任务须逐一完成。
- 普通原生实际子进程覆盖目标占用锁拒绝、同大小一字节损坏拒绝且损坏字节不被修复、失败保持unknown、明确新校验成功及旧请求归属保持。合成原来源/seal是明确测试夹具，不是实际receiver/板端采集证明。
- 生产窗口经实际HTTP连接后台与真实原生校验子进程：真实QMessageBox取消保留unknown、确认后完成；远端仅一份核对意图，原ID不变，其他unknown在核对前继续阻止新复制。verification-window-result.json记录该窗口证据。不是模拟一个GUI成功状态，也不代表Windows新窗口人工验收。
- 严格C++11 ASan/UBSan/LeakSanitizer原生控制器、复制/完整校验子进程及HTTP服务联调通过：`build-transfer-reconciliation/final-sanitized-evidence/transfer-jobs-3axfescw/result.json`。该组不执行生产窗口探针；系统Qt未整体插桩，没有使用实际receiver。
- 完整offscreen界面87通过/0失败，构建99源码/243输入，见 `build-transfer-reconciliation/widget-evidence/receiver-policy-widgets-urnbntsh/`；Windows Qt5.14.2 MinGW六操作程序及Linux完整23主机程序重新构建通过。
- 每个原任务最多64个核对记录，全服务单活动、不排队；原请求/固定路径/核对意图在执行与完成前检查。保存/意图故障保持门槛。同ID的当前观察与每次校验结果不是同一概念，不自动续传或重发POST。

测试有一次因重启前后服务instance_id不同而误判“不改变控制状态”；最终断言改为同一当前实例在核对前后的完整状态相同，保留失败日志、不放宽接收/硬件门槛。最终普通及插桩组均完整重跑，窗口组同时覆盖两个未知任务。

每轮普通/插桩复制与校验保守合计64MiB，完整界面256MiB，低于20GiB；所有自有.graw在成功/失败/可捕获中断路径清理，传输组cleanup.json和窗口组payload-cleanup.json记录边界。数据校验不增加raw副本，来源及已有目标在生产操作中不删除。

完整副本可以不依赖原归档目录，但服务的固定source_root/backup_root/audit_root仍需存在且身份不变，不能推广为整块来源盘卸载后的服务接管。部分副本续传/废弃、旧子进程接管、跨主机、实际断电、持续吞吐、Ubuntu24/macOS/现场以及完整硬件UNKNOWN恢复仍未完成。详细操作与失败处理见[复制手册第10节](CATALOG_TRANSFER.md#10-服务重启后的原复制任务人工核对)。70.29整组源码凭据因本阶段控制/传输源码更新仅作历史，旧包保留。

## 70.31 原生副本前缀核对、明确续传与操作台入口（2026-10-12）

新增ResumeReceiverCatalog、原生CLI `--resume-catalog`、严格恢复请求及认证POST `/v1/transfers/resumptions`，操作台MUSIC形式窗口增加明确续传按钮。新复制在raw写入前保存来源身份；续传匹配原意图/根/来源记录，先逐块核对全部已存在前缀，再从已有长度追加。既有文件不使用O_TRUNC，缺失文件独占新建；最终完整目标SHA通过后才发布完成凭据。Qt和服务不读取或转发raw载荷，仍由独立无Qt原生进程执行。

- 最终普通核心通过：`build-catalog-resume/final-core-evidence/catalog-resume-h850qe8l/result.json`；严格C++11 ASan/UBSan/LeakSanitizer同组通过：`build-catalog-resume/final-sanitized-core-evidence/catalog-resume-imemlgme/result.json`。11个复制/续传场景与流式SHA已知向量/块边界覆盖保留旧回归；1MiB前缀及真实RLIMIT_FSIZE短写留下的16KiB非块对齐前缀续传后inode不变、完整内容核对通过。
- 来源同大小改动、目标前缀不同拒绝，目标原inode/长度不变；额外未登记内容拒绝，不清理它。缺少来源身份记录的旧副本不能续传；旧完整副本仍能独立验证。来源与目标合作锁、原身份/元数据及最终全字节核对保留。立即_exit的文件限制测试子进程不作为自身退出泄漏证明，主测试正常退出仍插桩核对。
- 普通真实子进程/认证HTTP/生产窗口通过：`build-catalog-resume/final-network-evidence/transfer-jobs-6mw7uqnw/result.json`。实际复制子进程继承16KiB文件上限，确实短写失败并留下部分目标；服务重启后明确续传，旧原任务/实例/child.json保持，目标inode保持并长至完整大小。未认证/畸形/旧实例拒绝、同恢复ID不启动第二子进程、当前receiver控制状态不变均核对。
- 普通生产窗口使用实际HTTP和原生子进程：真正QMessageBox取消无恢复提交，确认后只一份resume意图，原任务完成、原文件inode和旧PID记录不变；resume-window-result.json记录结果。第70.30完整副本核对、多个unknown门槛和原复制确认/失联观察同组回归保留。
- 插桩真实控制器、HTTP服务、复制与续传子进程通过：`build-catalog-resume/final-sanitized-network-evidence/transfer-jobs-j64nwxzy/result.json`，不执行窗口探针。系统Qt未整体插桩，本轮没有启动真实receiver；来源数据/seal为明确合成夹具，不冒充板端采集。
- 完整offscreen界面87通过/0失败，99源码/243输入，见 `build-catalog-resume/widget-evidence/receiver-policy-widgets-51iv3twz/`。Windows Qt5.14.2 MinGW六操作程序、Linux完整23主机程序重新构建通过；新窗口Windows人工交互、Ubuntu24/macOS及现场未验。

核心每轮256MiB保守预算，后台/窗口联调每轮64MiB，完整界面256MiB，均低于20GiB；核心/传输组cleanup.json与窗口组payload-cleanup.json确认自有raw零。失败/可捕获中断也清理自有测试raw，生产来源/副本与旧证据包不删。

恢复入口只接受同策略的unknown/failed原任务、当前实例与原request_key。每个原任务的核对及续传合计最多64个记录，全服务单活动、不排队、不自动重发；保存/意图故障保持门槛。前缀核对需要重新读已有数据，copied_bytes包含复用前缀而非本次新增写量，不能当吞吐验收。

来源仍须在线、原文件/路径/目录身份未改变。新来源记录是合作软件的身份核对，不是签名、事件物理正确性、断电或磁盘重新映射保证。跨主机续传、改变原路径/磁盘身份后的接管、失败任务废弃/删除、旧PID接管、长期大流量、目标平台及完整硬件UNKNOWN恢复/加载/全局控制权仍未完成。详见[续传操作与原理](CATALOG_TRANSFER.md#11-中断副本的明确续传)。70.30整组源码SHA因本阶段更新仅作历史，旧包保持。

## 70.32 单值片段的CDATA/注释保留编辑（2026-10-12）

XcfgDocument共享单值解析器现在同时定位可编辑片段，只改该片段。保留首尾注释/处理指令、XML排版、属性与其他节点；CDATA能表示时保留，终止符或回车输入转单个字符引用文本，预先重算候选值并要求与请求一致。原共享类型/范围/全部来源与Save All待核对门槛、最小保存和准确字节撤销保留。NBSP不能被当成排版空白；无type空值仍拒绝改变scalar角色。

- 普通与严格C++11 ASan/UBSan/LeakSanitizer核心各126通过/0失败，14源码/158输入：CDATA+首尾注释、普通文本注释、处理指令、空评论/空CDATA、终止符/回车的准确表示、NBSP值边界及每组Undo/Redo准确字节；include范围不符和其他来源变化不写原文件；引用空白保持scalar，旧分裂文本拒绝回归保留。
- 完整offscreen界面88通过/0失败，99源码/243输入。生产ConfigWindow实际选中CDATA数值保存，原评论/处理指令和属性保持，真实审阅Undo窗口显示准确原文、Apply还原，读取模式Save禁用。证据 `build-scalar-tokens/widget-evidence/receiver-policy-widgets-82vt3f94/`。
- 实际Windows Qt5.14.2 MinGW核心125通过/0失败/1既有Unix权限跳过，包含新增用例与旧完整回归；真实Process句柄等待、独立QtTest报告及程序SHA记录在windows-core/results-mapped.txt和mapped-result.json。最初从WSL映射执行出现系统0xC0000006分页读取异常，没有完整结果，不计通过；失败文件以wsl-image-error保留。随后将同版exe/匹配DLL复制到唯一Windows本地临时目录运行并清理，该实际完整结果才计通过，不称Windows系统Qt整体插桩。
- 同版Linux23主机程序与Windows六操作程序已构建，419/420输入；本轮不改原生数据通路/硬件后端，无新依赖。GUI轮256MiB保守预算、自有raw清理；核心仅小型临时XML，不涉及DAQ传输。

核心证据为build-scalar-tokens/core与sanitized的tests.log/build-receipt/source-sha256；Windows实际新核心报告和初始失败保留，界面结果另有payload-cleanup.json。核心支持换行/回车不等于单行GUI支持多行输入；CDATA表示转换不是保持外壳，多个有效文本片段/自闭合值仍未放开。单文件编辑不证明全局事务、在线配置加载或硬件恢复；目标平台现场及更广旧接口仍待完成。详细操作见[配置编辑手册](CONFIGURATION_EDITOR.md#保留cdata和首尾标记修改值)。70.31整组源码SHA因本轮配置代码更新仅作历史，旧包保持。

## 70.33 外部固定输入故障与原对象明确停生产（2026-10-13）

原网络Check将外部输入和控制审计故障一起锁死，普通停机被拒绝后，客户端意图也可能一直待核对。本轮将外部固定输入故障单独保持；普通命令在完整控制审计中持久记录为failed但不执行硬件，生产客户端按绑定失败结果闭合原意图，仍禁止Start。明确故障停生产/既有原失败接收器清理可以沿用原对象，同时检查网络请求/日志、原生工件/快照/原操作及占用保护，绝不重读新输入构造目标或清UNKNOWN。

- 普通实际ECC网络八场景通过：`build-input-fault-stop/final-network-evidence/input-fault-stop-dqzrxlj_/result.json`，两Ice编码各准备后、运行中、网络审计损坏拒绝和完整正常九命令流程。准备后无receiver启动；运行/审计及正常场景执行实际两个receiver与数据服务，板端为明确Ice回环夹具。
- 外部来源变化时普通请求持久failed、硬件不变；原对象明确停生产后CoBo firmwareEnable/intEnable均零，父状态和原操作ID保持、UNKNOWN/需核对不清。恢复原字节不解除门槛；持久input-fault-held绑定原操作和启动输入SHA。重启观察原结果、不重放停止，新进程无原对象时负向请求失败、无硬件接管。
- 修改网络operations.jsonl后，明确停生产也被拒绝，实际硬件计数/动作不变。仅放开外部输入故障，不跳过控制审计/内部绑定/所有权检查，不实现无审计紧急执行或自动恢复。
- 生产Qt EccSystemClient与EccClientStore实际HTTP两编码通过：`build-input-fault-stop/qt-evidence/input-fault-stop-qhrgy0d2/result.json`。普通Stop未执行而得到绑定failed结果，客户端和持久存储Pending为空；随后明确负向停止complete，Start仍不可提交。27源码/171输入的Qt探针使用生产组件，不是整个MainWindow/RunControl按钮链路。最初探针遗漏必需input_binding_sha256而被构造器拒绝，仅属夹具配置错误，修正为核对保存的启动身份后完整重跑；不放宽客户端摘要检查。
- 严格C++11 ASan/UBSan/LeakSanitizer同版八场景通过：`build-input-fault-stop/sanitized-evidence/input-fault-stop-1qg14uq7/result.json`；新控制/硬件/ECC及服务代码插桩，旧GET/Ice静态库、系统Qt未整体插桩，数据服务/receiver为普通程序。新Qt客户端探针也为普通程序，不归入插桩覆盖。
- 完整offscreen界面88通过/0失败（99源码/243输入），见 `build-input-fault-stop/widget-evidence/receiver-policy-widgets-5r98w48o/`。Linux23主机程序/419输入、Windows Qt5.14.2 MinGW六操作程序/420输入重新构建通过；Windows新故障按钮完整交互及Ubuntu24/macOS/现场未验。

每组网络/Qt联调512MiB保守总预算，完整界面256MiB，成功/失败/可捕获中断清理自有.graw及测试凭据；cleanup.json/payload-cleanup.json确认raw零。接收器生产操作不因本轮改动被自动kill、断开或封存；只读状态不访问原始载荷。

HTTP 202只表示原请求接受记录，必须读取phase；failed不是硬件成功。磁盘不能可靠记录、请求/审计/占用锁或内部绑定损坏时仍拒绝，不能将此当作已停止。未增加配置变动自动停机策略，未解决未知sender闭合、完整硬件恢复、真实加载/全局控制权、跨主机、持续吞吐或现场验收。详细目的/步骤/失败处理见[网络手册第14节](ECC_NETWORK_SERVICE.md#14-外部固定输入损坏后原对象明确停生产)。70.32整组源码SHA因本轮控制代码更新仅作历史，旧包保持。

## 70.34 旧二进制配置转换与实际旧库对照（2026-10-13）

新增XcfgBinary、生产CLI import-binary/export-binary和MUSIC形式.h/.cpp/.ui转换窗口。导入旧版本1到可编辑XML，导出复用声明展开/具体有效对象语义，编码后解码比较有效摘要；全部来源及候选重算、独占新文件，不覆盖原配置，不参与raw或硬件加载。

- 最终普通和严格C++11 ASan/UBSan/LeakSanitizer核心各135通过/0失败，见`build-binary-config/core/tests.log`、`sanitized/tests.log`和各自result.json。包含五种类型/范围/空白、每个截断前缀、尾随/版本、int32/hex32及范围溢出、非法bool/非有限real/表数量、512确定性变异、审阅漂移/篡改/重复保存及缺失来源。系统Qt未整体插桩。
- 实际旧库参考增加可选--binary，59编译/625跟踪输入；Boost仅供独立参考，原CompoundConfig/TinyXml未修改。参考直接以Write|Binary打开FileStorage再调用原BinaryFormat，避免Document::exportSubConfig路径未打开流但返回成功。见`build-binary-config/legacy/build-receipt.json`、`binary-export-build-receipt.json`。
- 真实旧库七组交叉对照：原daq/daq2样例各8/66参数，合成五类型5项、整数/空串边界3项、默认/具体对象2项、字符串/实数范围2项，以及原daq2_ill拒绝。旧二进制→新XML、新二进制→旧库读取、旧实际编码→新导入核对类型/值/单位/范围及有效性；本阶段有效输入共86个标量查询。最终生产Release CLI证据见`build-binary-config/final-runtime-evidence/`；过程配置均由私有临时目录退出删除，无DAQ原始文件。
- 最终完整offscreen窗口89通过/0失败；生产ConfigWindow按钮打开新窗口，真实点击预览/保存、改变目标撤销审阅、导入/导出新文件并核对原来源保留，见`build-binary-config/final-widget-evidence/receiver-policy-widgets-kzcluh3a/`。窗口组其他既有模拟采集按256MiB保守总预算，自有raw清理记录零。
- 实际Windows Qt5.14.2 MinGW核心134通过/0失败/1既有Unix权限跳过，见`build-binary-config/windows-core/results-mapped.txt`、mapped-result.json；同版exe/匹配Kit DLL复制到唯一Windows本地目录，实际Process句柄等待、报告及exeSHA核对，临时目录清理。一次编译器向WSL映射TEMP写汇编文件Invalid argument失败不计通过，改用唯一Windows本地编译TEMP后重跑成功；原失败log保留。初版单个测试向要求非空的ResolveScalar属性输出传nullptr导致测试崩溃，修正夹具输出对象后最终整组重跑，不放宽解析器。
- Linux完整23主机程序/424输入、Windows六操作程序/425输入构建通过，见各full/windows-build-receipt.json。新配置转换生产端只用既有Qt Core/本地范围模型，没有Boost/旧CompoundConfig依赖；原生接收器/导出器不链接模块。运行包和最终源/程序/证据SHA入口为`build-binary-config/checkpoint.json`；旧70.33/70.32包保留，不能代表本轮修改后的源码。

范围为收到的旧版本1配置格式及明确支持域。hex线上8字节但旧主机类型unsigned32，高位非零严格拒绝；字符串用字符引用保留空白。XML注释/PI/排版、原include路径/来源分布不会存入二进制，额外属性/命名空间/其他版本拒绝。详见[操作与原理](CONFIGURATION_BINARY_FORMAT.md)。这些不是现场板端加载、全量旧ABI/数据库接口、硬件UNKNOWN恢复、跨主机控制、目标平台或持续吞吐验收；整套重构仍未完成。

## 70.35 原 Describe 失败的绑定只读恢复（2026-10-13）

新增SystemDescriptionClient::VerifyExistingDescription、原生ReconcileDescription及限定内部凭据的VerifyRecovery、认证reconcile-description请求、生产客户端/存储和主窗口明确确认入口。只允许同一原活跃实例、Idle/UNKNOWN、原Describe失败且无准备/采集对象；实际核对全部描述、CoBo四停止/未就绪位、MuTaNT run及原数据停止，再记录新观察与reconciled后恢复到Described。原失败历史不改，不重发CREATE、不修补布局、不执行寄存器写入；其他阶段和冷接管拒绝。

- 普通及严格C++11 ASan/UBSan/LSan原生各六场景通过：`build-description-recovery/final-native-evidence/description-recovery-oxoxfho7/`与`sanitized-native-evidence/description-recovery-qdl0mrdr/`。两Ice编码，实际RPC的原Describe状态位写入已应用后丢ACK、只读恢复后Prepare、未就绪/数据未停/错误原ID拒绝、原审计损坏保持UNKNOWN/存储门槛，以及冷实例拒绝。数据生命周期明确为替身，无receiver或raw，不冒充生产数据证明。
- 最终普通实际HTTP十场景：`build-description-recovery/final-closed-network-evidence/description-recovery-h9d4joli/`。两编码各恢复成功后Prepare/Configure/Start/Pause/Resume/Stop/Breakup/两次Reset及实际两个receiver正常EOF/封存；两编码各状态/原ID拒绝、审计拒绝、生产Qt客户端/存储恢复；另两场独立正常九命令基线。恢复周期与正常九命令分别在scope.json标记，不把原Describe失败写成正常Describe完成。
- 最终新代码插桩HTTP八场景：`build-description-recovery/final-closed-sanitized-network-evidence/description-recovery-xq6j77zk/`，六恢复及两正常基线；161跟踪输入当前SHA匹配。新控制/硬件/ECC/服务插桩，旧GET/Ice静态库和系统Qt未整体插桩，receiver/data-service及新Qt探针普通。原生和网络各自退出检查；不推广为旧依赖整体插桩或物理硬件恢复。
- 生产Qt客户端/存储两编码核对绑定恢复、持久意图闭合、刷新状态后才恢复Prepare资格。另客户端34项全部通过，包括读取历史恢复记录不能清后来故障门槛、错误目标回复保留Pending。客户端消息绑定/凭据/实例故障不因恢复而清除；原Pending先观察，不能换ID重发。本地存储按原操作保持故障，只随绑定的成功Describe凭据解除相应原故障及其已失败只读核对尝试；后来或其他故障保留，重开记录重新推导。最初组件探针未包含GUI自身原意图，复核发现本地门槛仍保持，补齐生产存储和真实窗口闭环后才计通过。
- 完整offscreen界面89通过/0失败，`build-description-recovery/finished-widget-evidence/receiver-policy-widgets-of8h2qgp/`；生产菜单默认Cancel零请求，Yes一条信号。新增两编码实际生产MainWindow→工作线程RunControl→ECC→两数据服务联调，见`build-description-recovery/window-evidence/description-recovery-5fdip8ed/`：GUI提交并持久记录原Describe失败，Cancel零请求/Yes一次恢复，新状态及本地门槛解除后实际Prepare成功。板端仍为明确Ice夹具；未启动receiver/Run，不冒充现场。
- 纯C++11协议用例通过，原普通请求规范字节保持；新命令必须包含原失败ID且进入摘要，普通命令不能夹带该字段。恢复过渡10已支持；有界诊断消息允许Ice换行/回车/tab，ID和策略文本仍拒绝控制字符。首次实际Describe丢ACK的多行错误曾使GET operation返回400，修正诊断边界后同版联调重跑；原错误内容和UNKNOWN保留。
- 原父Describe审计在失败时绑定实际写入链、原目录/文件身份及字节；新的每份观察审计也核对实际写入链，并绑定文件SHA/身份。读取后、提交前检查，恢复意图和proof是新文件，不覆盖旧记录。原审计损坏仍拒绝；无外部输入故障放行、无无审计紧急执行。真实窗口首次还暴露忙碌解除后恢复菜单未重新启用，修正ShowControl状态更新并补回归后重跑两编码闭环。原生状态提交与网络最终结果保存为两层，后者失败不能当整次恢复成功。

本轮网络每组512MiB、窗口256MiB保守总预算，cleanup.json/payload-cleanup.json确认自有raw零；小型客户端元数据测试不创建raw。早期完整采集测试误按一次Start的两帧计数，遗漏Resume产生的另两帧，实际每源16384字节/四帧已封存；修正夹具预期后重跑，不修改接收器或削掉Resume。Qt夹具漏给存储根时被正式存储守卫拒绝，补私有绝对目录后重跑，保护不放宽。

当前Linux23主机程序/424输入、Windows Qt5.14.2 MinGW六操作程序/425输入构建通过，运行包/执行程序及证据入口`build-description-recovery/checkpoint.json`。数据通路不改，Qt只传低频元数据，未新增依赖。Windows新恢复功能人工交互、独立目标机、Ubuntu24/macOS、全局控制权/实际加载、Prepare/Run失败及冷接管完整恢复、跨主机持续性能仍未验。操作、原理及拒绝处理见[Describe核对恢复](DESCRIPTION_RECONCILIATION.md)；整套重构仍未完成。


## 70.36. 原 Prepare 故障后的断电与自有订阅收尾（2026-10-13）

本轮新增 `unprepare-after-failure`，沿用同一失败准备对象和原停生产凭据，先核对完整目标及停止状态，再监测实际已上电 AsAd、独立断电，全部确认后关闭自有已知订阅。每步保存首次尝试/确认，断电丢应答不重发；父 ECC 的 UNKNOWN 和原失败历史保持。没有采集对象才能使用，Configure/Run 故障、冷服务接管和完整正向恢复未在本轮实现。

- 普通原生五场景通过：`build-failure-unprepare/confirmed-native-evidence/description-recovery-mo6_zxe8/`。Ice 1.0/1.1 成功、原停生产丢应答拒绝、断电丢应答后不重发、监测失败仍尝试其他独立板断电。原生数据生命周期为明确替身，不是实际 receiver。
- ASan/UBSan/LSan 原生同五场景通过：`build-failure-unprepare/sanitized-native-evidence/description-recovery-7mm_dyek/`。本次复核其程序 SHA 与最后插桩构建一致；161 项编译输入当前 SHA 匹配。旧 GET/Ice 静态库和系统 Qt 未整体插桩。
- 普通实际 ECC HTTP 三场景通过：`build-failure-unprepare/corrected-network-evidence/description-recovery-auzagj46/`。两种 Ice 编码真正断电收尾及断电丢应答；每项均有 `failure-cleanup-result.json`，证明父 UNKNOWN 保留且未启动 receiver。两个实际数据服务保持 Fresh，不传 raw。
- 插桩 ECC 后端及网络前端同三场景通过：`build-failure-unprepare/sanitized-network-evidence/description-recovery-6rhkvwok/`。板端为本机 Ice 夹具，数据服务/receiver 程序普通；不代表现场硬件、全依赖插桩或持续吞吐验证。
- 最终Linux包内Release ECC及两个数据服务同三场景通过：`build-failure-unprepare/package-network-evidence/description-recovery-k2qqwr9x/`。专属收尾凭据和scope均核对，全部自有raw清理；不是完整窗口或实际receiver采集测试。
- 完整 offscreen 窗口89通过/0失败：`build-failure-unprepare/confirmed-widget-evidence/receiver-policy-widgets-w1_p3a41/`，覆盖新菜单默认取消零信号、明确确认一次信号及既有界面用例。这是窗口确认测试，不是新收尾按钮到服务的完整 MainWindow/RunControl 网络验收。Windows 新交互仍待人工检查。

证据纠正：早期网络组 `network-evidence/description-recovery-e38v2szb/` 中 `prepare-cleanup-0` 错误进入 hardwareObservation 分支，exit0只证明只读查询，不能计断电收尾。现已移除错误分流，运行器要求专属收尾凭据及scope中的两个实际数据服务、零receiver、未完成完整工作流；修正后普通/插桩各三场景重跑通过。另一次误用 full 构建目录内旧测试文件收到 Unknown scenario，未计通过；最终普通夹具来自 build-ice-compat，ECC 使用 full 交付程序。

网络每组512MiB、窗口256MiB保守总预算；原生组无raw，所有cleanup/payload-cleanup记录确认自有raw零，继续保持用户20GiB总上限、测后删除要求。不删除生产或参考数据。当前Linux23程序/424输入、Windows Qt5.14.2 MinGW六程序/425输入源码摘要匹配；本轮只增加控制命令，Qt不参与原始事件接收、存储或复制，未增加第三方依赖。

交付目录为 `build-packages/getdaq-host-20261013-failed-prepare-unprepare/` 与 `build-packages/getdaq-windows-qt5142-20261013-failed-prepare-unprepare/`；最终清单/程序/证据引用在 `build-failure-unprepare/checkpoint.json`。Windows匹配DLL、无SDK PATH初始窗口及原生平台初始窗口检查不代表独立目标机或新命令人工验收。Ubuntu24/macOS、现场固件、实际加载/全局控制权、Prepare/Run完整恢复、冷接管和跨主机持续采集仍待，整套目标未完成。


## 70.37. 原 Prepare 收尾后的明确核对恢复（2026-10-13）

新增 `reconcile-preparation`，绑定原失败Prepare ID；仅同一服务保留已确认Unprepared对象、原停止/断电/自有订阅证明、有效原父审计且无采集对象时只读核对，提交Described后才允许随后明确的新Prepare。原失败历史unknown保留；不是Run或冷服务完整恢复。Qt只发送小型请求/状态，原始数据由独立原生程序接收、存储和复制。

- 最终普通原生八场景通过：`build-prepare-reconciliation/closed-core-evidence/description-recovery-nmz8v195/`。两编码核对后新Prepare、供电变化/原收尾审计修改拒绝、新观察清单在数据RPC前/返回前两个时间点被改动均拒绝，另回归两编码Describe恢复。数据生命周期为明确替身，不是receiver或封存证明。
- 严格C++11 ASan/UBSan/LSan原生同八场景通过：`closed-sanitized-core-evidence/description-recovery-_dqkilrb/`。新原生实验/硬件/ECC及网络前端161编译输入SHA匹配；旧GET/Ice库和系统Qt未整体插桩。
- 最终普通实际ECC HTTP九场景通过：`closed-network-evidence/description-recovery-tf_6_v0z/`。四项准备恢复成功/供电和审计拒绝，三项故障收尾，以及两编码正常九命令采集基线。每场景两个实际数据服务；准备/恢复场景未启动receiver，正常组有两个实际receiver，经过自然EOF、正常退出和封存核对。
- 插桩ECC及网络前端同九场景通过：`closed-sanitized-network-evidence/description-recovery-7pilo3ln/`。receiver/数据服务普通，板端为本机Ice夹具；不代表全依赖插桩、现场固件或持续吞吐。
- 最终生产Qt客户端/持久存储两编码HTTP通过：`closed-qt-evidence/description-recovery-ksnh8qux/`。实际记录并观察原Prepare失败、原停止及原收尾意图，不重发；恢复成功按原操作解除相关持久门槛，但仍要求新状态才接受Prepare。其他独立故障保持。这是生产组件探针，不是完整MainWindow/RunControl新按钮到网络链路。
- 最终包内Release ECC四项恢复成功/供电和审计拒绝通过：`package-network-evidence/description-recovery-jobr39sk/`；包内服务与生产Qt客户端/存储两编码通过：`package-qt-evidence/description-recovery-bol6qkdt/`。各自专属证明及scope核对，无receiver/raw；不是完整窗口链路。
- 客户端34项回归通过：`build-prepare-reconciliation/client-tests/tests.log`。完整offscreen窗口89通过：`widget-evidence/receiver-policy-widgets-2pzga519/`，含新菜单默认Cancel零信号、Yes一次信号、忙碌/断开门槛；窗口确认不冒充完整服务流程。

恢复必须保留同一原对象：原Prepare父失败审计在失败时绑定文件身份/实际写入链；原故障断电的所有硬件/注销/系统审计均按实际链及字节保存守卫。新观察清单单独保存 `observation-manifest.json`（预算1MiB），父 `recovery-proof.json` 只绑定清单SHA、数量及原ID/快照等，明确小于等于4096字节。这样板卡数量增长不会把完整清单挤入父ECC日志的4096字节单证据字段。原/新证明在最后数据停止RPC前后都重新检查；任何修改保持UNKNOWN和存储故障，不清日志、重放步骤或重建对象猜测订阅。

覆盖再纠正：70.36的prepare-cleanup-0移除错误查询分流后仍遗漏夹具编码选择，实际使用编码1，旧网络证据不能证明两编码。70.37已把全部preparationCleanup场景纳入编码选择，每项结果记录实际encoding_minor，运行器强制匹配0/1；上述普通/插桩九场景和Qt两场景才证明正确编码。场景名和exit0不能代替专属证明。早期70.37普通六/七及插桩七场景只是历史，后续清单/最后RPC核对修改以这里最终八/九组为准。

各网络组512MiB、窗口256MiB保守总预算。普通正常组按全部raw文件名统计删除8项/131072字节，准备/恢复组无raw；所有最终cleanup/payload-cleanup确认自有raw零，用户20GiB总上限及测后删除保持。无需删除生产或参考数据。

当前Linux23程序/424生产输入、Windows Qt5.14.2 MinGW六程序/425输入构建通过；运行包为 `build-packages/getdaq-host-20261013-preparation-reconciliation/` 与 `build-packages/getdaq-windows-qt5142-20261013-preparation-reconciliation/`。最终程序/清单/来源和测试证据入口 `build-prepare-reconciliation/checkpoint.json`，goal_complete=false。Windows匹配DLL、无SDK PATH及原生/offscreen初始窗口已验，不能冒充新功能人工交互或独立目标机验收。

完整MainWindow新恢复链路、Run/冷服务完整恢复、Ubuntu24/macOS/独立目标机、现场固件、实际加载配置/全局控制权和跨主机持续采集仍待，整套目标未完成。完整操作/原理/架构关系见[准备核对恢复手册](PREPARATION_RECONCILIATION.md)。


## 70.38. 完整主窗口的原Prepare故障收尾与核对恢复（2026-10-13）

本轮闭合70.36/37的完整操作台验收缺口，生产代码保持不变。新增真实MainWindow探针，从窗口Progress触发Describe及Prepare，在原自有告警建立后注入告警，实际GUI记录原Prepare失败，再明确停生产、Cancel/Yes收尾、Cancel/Yes核对恢复，刷新状态后手动新Prepare。Qt工作线程RunControl、生产客户端/存储、ECC及两个数据服务全部实际运行，板端为回环Ice夹具。

- 普通后端两编码通过：`build-preparation-window/closed-window-evidence/description-recovery-pgud9kun/`。
- ASan/UBSan/LSan ECC后端两编码通过：`sanitized-backend-window-evidence/description-recovery-2iec5zrv/`。GUI/网络夹具、数据服务及系统Qt普通，旧GET/Ice未整体插桩。沿用70.37的当前插桩服务，134项非测试来源及嵌入依赖SHA已匹配；旧SAN测试夹具不用于本组，不将其旧全161来源记录当当前新夹具构建证明。
- 包内Release后端两编码通过：`package-window-evidence/description-recovery-3sn6ful3/`，ECC/数据服务来自已核对Linux23程序包，窗口为实际生产源码探针。

每个窗口均确切六份后端接受：describe一、prepare二、stop-production-after-failure一、unprepare-after-failure一、reconcile-preparation一。原失败Prepare必须终态failed且确为注入的alarm interlock；两次Cancel各零信号/请求，两个Yes各一次。负向请求complete后父Described/UNKNOWN及原ID保持；核对complete绑定原GUI失败ID并回到已知Described；新Preparecomplete为Prepared。数据服务始终Fresh，零receiver、零raw，没有Start或Run完成。

独立 `tests/check_preparation_window_evidence.py` 对上述三组全部通过：每个场景六份实际GUI请求文件、规范请求SHA、本地持久intent、六份终态observation逐项匹配ECC接受/终态记录；新恢复和新Prepare的状态/ID、原失败及负向UNKNOWN分别核对。`window-store-binding-result.json` 保留本地/后端日志及验证器SHA，不使用窗口自报布尔值替代实际记录。普通窗口探针包含原请求存储，不再仅观察服务端已存在的失败。

新增测试只改变探针、夹具和运行器。早期日志使用通用“native receivers”文本，不符合本场景；最终已改为明确no receivers/raw，并取消误用description-recovery结果文件。终态proof、实际encoding_minor及独立存储检查为覆盖依据。70.37旧整组检查点随验证输入更新属历史；生产424输入/Windows425及原生后端未改，旧包保留。

每组512MiB保守预算，全部cleanup.json确认自有raw零，用户20GiB上限/测后删除保持。新交付文档和证据入口`build-preparation-window/checkpoint.json`；对应Linux/Windows阶段包命名preparation-window。Windows仍为六操作程序匹配运行库和初始窗口验证，不将Linux offscreen全链路冒称Windows新交互或独立目标机。

Run/冷服务完整恢复、现场板端固件、实际加载配置/全局控制权、Ubuntu24/macOS、独立目标机和跨主机持续性能仍待，整套重构未完成。操作与ID解释见[准备核对恢复手册](PREPARATION_RECONCILIATION.md)。


## 70.39. 原失败采集的发送器连接明确收尾（最终核对2026-10-14）

新增breakup-after-failure，要求同一原失败采集对象及全部确认的原生产停止记录。全目标先核对，只接续已知发送器的Stop/Disconnect；独立目标失败仍处理兄弟目标，全部确认后撤销CoBo就绪配置。采集对象为Broken、同一准备对象Prepared，但父状态/原ID/UNKNOWN保持，不调用数据服务Finish/Abort，不完成Run。

- 最终普通原生六场景通过：build-failure-breakup/core-evidence/description-recovery-340mfsgs。两编码成功、Stop/Disconnect丢ACK、未知预检查拒绝、原Broken只读不重发；含MuTaNT，数据生命周期为明确替身。第一次漏场景许可导致Unknown scenario，未计通过。
- ASan/UBSan/LSan原生同六场景通过：sanitized-core-evidence/description-recovery-4vykru38；实际ECC→两数据服务→receiver故障HTTP五场景普通通过于final-network-evidence/description-recovery-84crw7a4（窗口分支新增后按当前夹具程序SHA重跑），插桩通过于sanitized-network-evidence/description-recovery-xwdwbc7y。两编码成功、两种ACK丢失及已有未知发送器拒绝；原PID/会话不替换，Start一份、Finish/Abort零。旧GET/Ice/系统Qt未整体插桩，receiver/数据服务普通。
- 网络故障仅异常结束自有receiver以触发原对象故障停生产；测试SIGKILL不属于产品功能。首次使用告警时后台正确执行已知正常Stop，不满足UNKNOWN，失败记录network-evidence/description-recovery-zubnd_57未计通过，没有把已知结果改成未知来放行。
- 最终完整生产MainWindow→线程RunControl→ECC→两数据服务→receiver两编码通过：fixed-window-evidence/description-recovery-b1zpcird。真实Running后的receiver故障触发原停生产；连接收尾Cancel零/Yes一，随后原receiver清理Cancel/Yes。原PID/会话/目录保持，Run仍Interrupted、duration_incomplete，编号不增、run.csv不存在，UNKNOWN和数据待核对保持。
- 此窗口首次因EccClientStore在故障门槛下漏了abort-failed-receivers而本地拒绝后续清理、测试超时，window-evidence/description-recovery-863kx_za不计通过。修复只补明确负向准入，新增正向仍拒绝及重开持久门槛回归；客户端35通过于client-tests/tests.log。修复后完整窗口重跑成功，界面89通过于fixed-widget-evidence/receiver-policy-widgets-i94s29z2。
- 共享发送器审计链及配置撤销证明涉及的两编码正常九命令基线普通/插桩通过：normal-evidence/description-recovery-8jquzmvo、sanitized-normal-evidence/description-recovery-yix_jj5p。有实际独立receiver、真实TCP/自然EOF/退出和seal核对，板端为合成夹具，不代表物理持续吞吐。
- 最终Linux包内Release ECC/数据服务与完整Running窗口两编码通过：package-window-evidence/description-recovery-14zob6ui。最初审批额度检查失败，动作未执行；恢复后重新通过审批才运行，没有绕过审批或把未执行操作计通过。

插桩构建器新增--tests，只编译/链接实际选择的两个测试程序，默认全套保持；本轮156项编译来源SHA匹配。不是整个旧依赖/GUI插桩。当前Linux23程序/424输入、Windows Qt5.14.2 MinGW六程序/425输入构建通过；新包getdaq-host-20261013-failed-acquisition-breakup（23/138文件）及getdaq-windows-qt5142-20261013-failed-acquisition-breakup（6/233）全部清单核对，Windows匹配DLL/无SDK PATH/初始原生及offscreen窗口通过，不能冒称新命令Windows人工验收。

每组网络512MiB、窗口256MiB保守预算。普通故障网络删除10个raw名称/49152字节，其他最终组cleanup/payload-cleanup确认自有raw零；用户总20GiB上限和测后删除保持，产品raw保留。Qt仍仅控制/小型状态，不参与高流量接收。

最终程序/来源/证据/运行包入口build-failure-breakup/checkpoint.json，goal_complete=false。数据失败会话退休、原对象断电及完整硬件/Run正向核对、已知告警门槛和冷服务完整恢复、现场固件/全局控制权/实际加载、Ubuntu24/macOS/独立目标机/跨主机持续性能仍待。此命令complete不是完整Run恢复。操作顺序、实际EOF和封存区别见[故障连接收尾手册](FAILED_ACQUISITION_BREAKUP.md)。

## 70.40. 原失败数据会话退休（2026-10-14）

新增独立retire-failed-session：要求同一服务对象已确认完成原AbortFailedSession，匹配原会话与原父失败声明，全部原子进程已回收，原目录/锁及审计有效。保留原会话/PID/目录/退出码和未封存标记，状态Retired；不发送信号，不读写/删除raw，不伪造seal。Validate/VerifyStopped/新Start前继续核对退休证明；新Start产生新会话和新目录。原Abort或退休审计被修改后恢复字节不能清已锁定故障。

控制层只允许原控制者/epoch、原实例和原会话；对应已完成Abort才能解除数据门槛，独立失败意图/内部存储故障/冷启动不清除。新增本地及远程EccDataLifecycle方法；远程适配器只沿用原整体Abort证明，逐节点一次POST/原ID观察，正常Stopped且封存确认的兄弟节点明确跳过，全部结果确认才解除对应数据门槛。父ECC正式恢复命令和操作台Run入口尚未接，不能把数据退休当父UNKNOWN解除或采集完成。

- 最终普通原生八场景：build-session-retirement/final-native-evidence/failed-session-retirement-uyvgo5d4。退休及新正常周期、锁占用、原Abort审计改动/恢复仍拒绝、退休证明改动/恢复仍拒绝，以及原Abort/正常周期/失败启动/重开门槛回归。原未封存raw逐字节保留，新周期正常EOF/退出/seal核对通过。
- 最终普通实际认证服务八场景：client-service-evidence/retirement-service-bkb68nb5。包含实际原生HTTP客户端和生产query提交/读取Retired（核对实际响应）；原父声明及本地异常两种失败、锁占用、两种审计故障、独立失败意图、Abort后及退休后服务重启拒绝。错误会话/epoch/快照/实例拒绝，同一操作重复提交只有一份accepted；退休后手动新周期正常完成，旧raw摘要保持。
- 严格C++11 ASan/UBSan/LSan数据核心八场景：sanitized-native-evidence/failed-session-retirement-n06dgfju；插桩认证服务/原生HTTP客户端八场景：sanitized-client-service-evidence/retirement-service-zz2zg_2g。121项编译输入及60份编译命令已核对，包含控制/HTTP头和本地构建文件；系统Qt、原始receiver及远程ECC适配器未整体插桩。数据管理原生工具ldd无Qt/Ice/yaml/Boost/curl，Qt仅低频认证前端。
- 普通远程适配器五场景：remote-evidence/sealed-peer-retirement-rafyq8j3。两Ice编码混合失败/已封存兄弟退休，原父审计损坏拒绝，原两编码负向清理回归。正确actual encoding_minor记录，失败节点退休POST一份、已封存节点零份，原会话/PID保持；新数据周期及正常Finish可用，旧兄弟seal不变。测试直接调用数据适配器，没有经过父ECC退休命令或操作台恢复，不冒充完整Run验收。
- 最初HTTP夹具重复启动服务，并把服务正确412前置条件拒绝当作失败；service-evidence中的首轮结果未计通过。修正为一次启动及实际拒绝语义后重跑，最终八项才是当前证据。早期原生/普通服务结果随最终原Abort审计锁定守卫更新属历史，当前原生以final目录、HTTP以client-service目录为准，先前HTTP结果未证明原生客户端提交覆盖。

各原生/服务组32MiB、远程组4MiB保守总预算，远小于用户20GiB总上限。测试原生与HTTP只用本机合成小帧，自有raw在退出/异常finally清理，最终剩余零；产品旧raw保留。故障注入只结束测试自己启动并核对身份的receiver，产品退休不发信号。详细操作、证据和重启限制见[失败数据会话退休](FAILED_DATA_SESSION_RETIREMENT.md)。整套重构未完成；完整硬件/Run正向核对、已知告警门槛、冷接管、现场/目标平台/持续吞吐仍待。

Linux包内Release数据服务/生产query/receiver另通过八项认证退休测试：package-service-evidence/retirement-service-jzpk39xj；包内receiver/数据服务与普通远程适配器五项通过：package-remote-evidence/sealed-peer-retirement-y9khsbs2。当前Linux完整23程序/424输入、Windows六操作程序/425输入回执匹配；运行包getdaq-host-20261014-session-retirement（23/139文件）和getdaq-windows-qt5142-20261014-session-retirement（6/234）。Windows匹配DLL、无SDK路径、初始原生/offscreen窗口通过，六操作程序字节与70.39包相同；本阶段数据退休没有新增Windows窗口入口，不作为Windows新功能人工验收。

最终来源/程序/测试凭据/包摘要入口build-session-retirement/checkpoint.json，goal_complete=false。987本地Markdown文件链接通过（排除代码示例、不检查锚点）；所有本阶段自有raw最终零。当前70.39综合源码检查点因本阶段修改属历史，旧运行包与原验证记录保留。完整父ECC退休/正向核对命令与操作台Run恢复为下一接续，不能将本阶段数据侧子流程算作整套完成。

## 70.41. 父 ECC 与操作台的原失败数据会话退休（2026-10-14）

新增retire-failed-receivers，要求同一原失败采集对象、原确认停止、原连接收尾及原receiver Abort完成证明。采集对象Broken/准备Prepared之外，还逐项绑定原目录与审计文件；调用原数据适配器退休后，在最后VerifyStopped RPC前后重新核对父证明。成功只解除相应数据门槛，父UNKNOWN、原操作ID及Run Interrupted/duration_incomplete保持，不封存、归档、启动或推进编号。

主窗口Retire original failed data sessions...默认Cancel/明确Yes，经过实际线程RunControl、生产客户端与持久意图存储，保存native_receiver_retirement_operation并刷新状态。ECC通用状态解码器补Retired及原回收证明检查；数据原始载荷仍在独立原生接收器。

已完成当前夹具的普通实际HTTP两编码：build-ecc-session-retirement/fixed-network-evidence/description-recovery-vcvdml48；完整MainWindow/RunControl两编码：final-window-evidence/description-recovery-ru87ebaa。网络先确认缺原Abort证明的请求failed且零数据退休/硬件动作，再完成原Abort/退休；窗口逐步真实Running、故障策略停生产、连接收尾、Abort、Cancel/Yes退休。最终原会话/PID/目录/回收及未封存标记保持，父UNKNOWN/控制门槛保留、数据Retired、不完成Run、不写run.csv或增加编号。

完整offscreen界面89项通过：widget-evidence/receiver-policy-widgets-5f7h2vwq；客户端/持久意图35项通过：client-tests/tests.log，包含保持下连接收尾→Abort→退休、正向拒绝及重开门槛。首次普通沙箱客户端运行无法绑定自有localhost，保存sandbox-bind-failed.log、未计通过；按已授权localhost联调范围运行后才35/0。

覆盖错误明确撤回：首轮新网络场景receiver-retirement-network-0误入既有receiver查询分支，缺专属结果凭据被运行器拒绝，不计退休通过。首轮原生receiver-retirement-0误归normal分支，未计通过；修正两处分流并更新旧通用恢复拒绝文案断言后重跑。窗口早期两编码结果window-evidence/description-recovery-dinye484通过，但网络夹具随后修改；当前以final-window根匹配当前程序SHA为准。

原生退休守卫与正常回归、插桩及后续原停止证明引用补强的最终范围见下文；当前不是整套重构或完整Run恢复完成声明。操作与原理见[父 ECC 数据退休](ECC_FAILED_SESSION_RETIREMENT.md)。


进一步核对原停止证明的依赖：连接收尾/Abort现在保留当时实际停止证明的引用，即使后来明确重新核对停止，也同时检查原依赖和当前停止证明。对子对象Broken的只读停机核对会保留Failed，故退休接受该原关闭上下文时还必须核对全部原sender缓存为Fresh/Disconnected且非UNKNOWN，不查询或接管远端发送器。

补强前原生普通/严格C++11 ASan/UBSan/LSan各八场景通过：final-native-evidence/description-recovery-l__6bqki、sanitized-native-evidence/description-recovery-lalv8_xp。这两组包含六项原退休/审计/丢应答/最后RPC后修改守卫和两编码正常原生生命周期，数据生命周期是替身；随后证明引用及阶段守卫补强后，以上整组程序SHA仅历史，不能声称全部匹配当前程序。当前专项普通两场景通过closed-state-evidence/description-recovery-nd6ssqhf，插桩同两通过closed-state-sanitized-evidence/description-recovery-z53trxot：重新停止核对后可退休，随后修改旧停止证明在零数据退休下拒绝。此前retained-stop-evidence及retained-stop-sanitized-evidence因只接受Broken而拒绝fresh-stop用例，未计通过。

最终当前ECC完整主窗口两编码通过closed-state-window-evidence/description-recovery-1gbiyzz4；独立check_retirement_window_evidence.py逐项核对七份实际本地请求文件/规范请求SHA/持久意图/终态观察，与后端八份accepted及终态相符（多出一份原故障策略自动停生产）。确认/取消次数之外还核对实际落盘，不只信窗口自报passed。旧fixed-network/final-window/retained-stop-window结果为各修改阶段历史，当前程序结果以closed-state根为准。

当前Linux23程序/424输入、Windows六操作程序/425输入回执匹配；新包getdaq-host-20261014-ecc-session-retirement（23/140文件）与getdaq-windows-qt5142-20261014-ecc-session-retirement（6/235），Windows匹配DLL/无SDK路径/初始原生和offscreen窗口通过，新增菜单Windows人工交互未验。后续停止证明引用及阶段补强不改变Windows六程序字节，最终包与回执仍匹配。数据控制/原始receiver普通，系统Qt及旧GET/Ice未整体插桩；不是现场/独立平台/持续吞吐或完整Run恢复证明。

最终当前插桩实际ECC网络四项通过closed-state-sanitized-network-evidence/description-recovery-i6v7ztmp：两编码退休及正常九命令/真实receiver/自然EOF/封存基线；156项新源码/头及本地嵌入依赖匹配。之前sanitized-network/retained-stop-sanitized-network组已历史。最终Linux包内Release ECC、两数据服务与receiver的完整窗口两编码通过closed-state-package-window-evidence/description-recovery-qajxa8wj，并通过同一七意图/七终态与后端八操作独立核对。

各网络组512MiB、界面256MiB保守总预算；用户20GiB总上限保持，最终本阶段自有raw零、退出/异常finally清理，不删除生产/参考数据。994本地Markdown文件链接通过（排除代码示例、不检查锚点）。最终当前来源/程序/证据/包摘要入口build-ecc-session-retirement/checkpoint.json，goal_complete=false；旧70.40整组源码检查点随本阶段更改属历史。完整Run后续断电/订阅收尾/正向核对、已知告警门槛、冷服务完整恢复、现场/独立目标平台及持续吞吐仍待。

## 70.42. 原采集故障退休后的断电与自有订阅收尾（2026-10-14）

新增unprepare-acquisition-after-failure及完整窗口确认入口。要求原采集/准备对象、原停止/连接/Abort/退休证明，重新核对已停止数据后沿用同一Prepared准备对象监测实际供电并逐板断电；独立板继续处理，全部确认才关闭已知自有订阅。最终只读核对全量元数据、生产/未就绪/供电及MuTaNT run/原订阅，再检查最后数据RPC前后全部父证明，完成仍保持原父UNKNOWN和Run中断。

SystemPreparation正常Unprepare新增实际Execute/订阅关闭/释放及原进度日志的写入者审计链。原准备进度日志会合法追加，父连接证明只允许依据原写入者已确认链推进同一文件的绑定，保留原inode/权限和完整旧字节前缀；其他原证明保持不可变，不把合法追加误作漂移，也不允许任意尾部伪造恢复。

普通实际HTTP三场景已通过build-acquisition-unprepare/network-evidence/description-recovery-gxrtb78p：两Ice编码成功、断电丢ACK失败且不重发。实际ECC→两数据服务→原生receiver，数据已退休/原PID与会话保持，原父UNKNOWN不清；丢ACK模拟为板端已作用但应答失败，独立板继续断电、订阅保留，不能把模拟器内部供电值当产品已提交成功。

普通完整MainWindow/线程RunControl两编码已通过window-evidence/description-recovery-fmxnfa4l：真实Running后原故障策略停止、连接/Abort/退休，再Cancel零/Yes一断电，保存native_acquisition_unprepare_operation及原duration_incomplete，Run仍Interrupted、编号和run.csv不推进。独立check_acquisition_unprepare_window_evidence.py核对每窗口八份实际请求文件/规范SHA/持久意图与终态观察，对应后端九个接受/结果（含一份自动停生产），不只信窗口信号次数。

初步原生六场景已通过native-evidence/description-recovery-36gue1n1，但启动后运行器增加网络分支，故只保留初步范围；最终来源固定的原生组另行重跑，不能把该组直接当最终来源一致证明。数据生命周期是明确替身；两编码成功、断电丢ACK、监测失败、退休证明损坏的零动作拒绝、最后数据RPC后证明变化拒绝均已覆盖。最终原生/插桩/包内范围见后文；当前不是整套或完整Run恢复完成声明。

Linux完整23程序/424输入及Windows Qt5.14.2 MinGW六程序/425输入构建通过；新包getdaq-host-20261014-acquisition-unprepare（23/141文件）与getdaq-windows-qt5142-20261014-acquisition-unprepare（6/236）清单已核对，Windows匹配DLL/无SDK路径/初始原生及offscreen窗口通过。新增断电菜单Windows人工交互和现场/目标平台/持续吞吐未验。新操作只传控制元数据，不读写/转发/删除raw；网络512MiB预算、用户20GiB总上限及测后删除保持。步骤、原理和丢响应处理见[原采集故障断电](FAILED_ACQUISITION_UNPREPARE.md)。


最终来源固定的原生六场景通过final-native-evidence/description-recovery-dygd36e9；该组才作为当前来源/程序一致证明，初步组仅历史。客户端/持久意图35项通过client-tests/tests.log，包含保持下断电准入、后续正向拒绝及重开仍保持；完整offscreen界面89项通过widget-evidence/receiver-policy-widgets-qig7cx7o，含新菜单默认取消/明确确认。包内Release ECC/两数据服务/receiver完整窗口两编码通过package-window-evidence/description-recovery-9uq6va4z，并经相同八请求/八终态/后端九操作独立核对。系统Qt/旧GET/Ice不在整体插桩范围，receiver和数据服务普通。


最终严格C++11 ASan/UBSan/LSan原生六场景通过sanitized-native-evidence/description-recovery-acszfsc0；实际ECC网络五场景通过sanitized-network-evidence/description-recovery-0e570c_1（两编码断电、断电丢ACK、正常九命令两编码/实际receiver自然EOF与封存）。普通正常网络两编码另通过normal-evidence/description-recovery-gwt959t4。156编译来源/头及嵌入依赖摘要匹配；插桩只覆盖新ECC/硬件/配置/管理核心与低频ECC前端，旧GET/Ice、系统Qt及原始receiver/数据服务未整体插桩，不能扩大到实机或持续吞吐。

最终本阶段自有raw零，退出/异常finally清理；用户20GiB总上限保持。1000本地Markdown文件链接通过（排除代码示例、不检查锚点）。最终来源/程序/执行证据/包摘要入口build-acquisition-unprepare/checkpoint.json，goal_complete=false；70.41综合当前源码检查点随本阶段更改属历史，原包与原证据保留。下一步原失败采集的完整正向核对、运行中持久门槛与Run中断收尾仍待，不得把本步骤断电complete算作完整Run恢复。


## 70.43. 原采集故障清理后的只读核对恢复（2026-10-14，后端/菜单验证）

本阶段尚未完成交付验收。独立 `held` 日志保留原 Start 成功历史，同时持久化绑定原操作的故障门槛；恢复只读核对原停止、连接、Abort、退休及断电证明，提交 Described 后必须重新 Prepare。旧 Run 保持 Interrupted，不补封存、不生成完成 CSV。说明见 [原采集核对恢复](ACQUISITION_RECONCILIATION.md)。

- 持久日志：build-acquisition-recovery/journal-tests.log 通过，新增范围错误拒绝、成功历史保留、故障记录重开、只读恢复及保持记录写入失败继续阻止操作。
- 普通原生四项：build-acquisition-recovery/native-evidence/description-recovery-p9e0h0d4/result.json，通过 acquisition-recovery-0/1/audit/late-audit；数据生命周期明确替身，未运行真实接收进程。初轮 description-recovery-0502ea8p 因测试断言误将 reconciled 当 complete 失败，不计入通过。
- 实际跨服务两编码：build-acquisition-recovery/network-evidence/description-recovery-i5w5iumt/result.json；实际 ECC、两个数据服务及原生接收进程，本机合成板端。证明无恢复硬件写入、原 Run 不完成、监视回 watching、手动新 Prepare；第二轮新故障仍需后续结果，不能从 watching 字样推断通过。
- Qt生产客户端/持久存储37项通过：build-acquisition-recovery/client-tests/tests.log，额外未确认清理失败仍保持门槛，重开后相同。完整操作台 build-acquisition-recovery/gui/build.log 编译通过；这不是完整菜单确认交互验收。
- 第二轮新Run故障监视两编码通过：build-acquisition-recovery/network-rearm-evidence/description-recovery-m7gcn9ty/result.json。重新Prepare/Configure/Start后新会话和PID不同，第二次接收故障仍仅停生产一次；tests/check_acquisition_recovery_evidence.py独立核对原Start complete→held、reconciled、同一完成行策略复位和两次绑定不同原操作的停止记录，原请求文件与规范SHA匹配，自有raw零。
- 实际Windows Qt5.14.2/MinGW六程序构建通过，build-windows-qt5142/windows-build-receipt.json记录425输入及六个PE x86_64程序；程序SHA已复核。没有新的Windows菜单交互/干净机验收。
- 严格C++11 ASan/UBSan/LSan新代码插桩通过：78编译单元/156来源，原生四项根sanitized-native-evidence/description-recovery-zqcbweng；跨服务两编码根sanitized-network-evidence/description-recovery-bis4bzcn，均位于build-acquisition-recovery。跨服务包含恢复后第二轮故障，独立记录核对通过。旧GET/Ice/系统Qt及receiver/数据服务未整体插桩。来源156/依赖4/生成2摘要已逐项与当前文件核对。
- 完整offscreen界面89项/0失败：build-acquisition-recovery/widget-evidence/receiver-policy-widgets-5lbop26t；新增恢复菜单取消零请求、确认一次。它不是完整Running MainWindow→RunControl→服务→原Run记录的恢复闭环，该项仍待。
- Linux23程序/424输入与Windows六程序/425输入回执摘要均匹配。新运行包build-packages/getdaq-host-20261014-acquisition-recovery（23程序/142文件）与getdaq-windows-qt5142-20261014-acquisition-recovery（6/237）已生成，Windows匹配DLL/无SDK PATH/初始原生及offscreen窗口检查通过；Windows完整新菜单人工交互未验。Linux包内恢复及新Run第二次故障两编码复跑通过：build-acquisition-recovery/package-network-evidence/description-recovery-_oj4it70/result.json；独立持久记录核对通过。阶段检查点为build-acquisition-recovery/checkpoint.json，goal_complete=false，完整Running窗口恢复闭环及整体任务仍未完成。70.42包与来源检查点是对应历史阶段证据，不能沿用作本阶段源码匹配证明。Ubuntu24/macOS、Windows本阶段新菜单、真机和持续跨主机吞吐仍未验。测试总预算20GiB、结束清理自有raw保持；上述普通两组清理记录均为零。


## 70.44. 完整运行窗口的原采集恢复与中断Run记录（2026-10-14）

通过真实 MainWindow→RunControl→认证ECC→两个数据服务/原生receiver的本机链路，从Running接收故障开始，依次取消/确认连接收尾、Abort、退休、断电与只读恢复，保存原Run的恢复结果后明确新Prepare。原Run保持Interrupted/duration_incomplete、无CSV、不自动增加编号；原接收会话/PID及未封存标记保持。

- 普通两编码窗口组通过：build-acquisition-recovery-window/window-evidence/description-recovery-o9dignkc/result.json；tests/check_acquisition_recovery_window_evidence.py独立核对每窗口十份请求/持久意图/终态与后端十一操作（多一项原策略自动停生产），并检查原Start complete/held及恢复reconciled、原Run恢复字段和手动新Prepare。
- 普通独立输入故障两编码通过：input-hold-evidence/description-recovery-sc2tlku_，位于build-acquisition-recovery-window。即使清理证明完整，额外输入故障仍使两次不同ID恢复请求拒绝，无硬件/数据重放，原Start成功历史保留。数据生命周期明确测试替身。
- 首轮description-recovery-4_ot57go窗口实际流程完成，但外围断言把AsAd供电位掩码误当布尔1，失败不计通过；改为准确匹配声明asadMask后重跑上述完整组。没有放宽生产核对或改写该失败证据。
- 本阶段生产源码未改；前轮Linux424/Windows425输入和程序回执须继续逐项匹配，不能将验证源码变化解释为新生产功能。本轮普通测试程序已更新，70.43综合检查点中的测试输入/对应程序路径只作历史，旧包与原证据保留。
- 新编译两个严格C++11 ASan/UBSan测试入口，复用此前已验证且本次来源/头文件/依赖/生成摘要未变的70个核心对象，保存复用对象SHA与准确编译/链接命令；旧插桩程序未覆盖。插桩独立输入故障两编码通过：sanitized-input-evidence/description-recovery-xm3wbzdv；完整窗口两编码通过：sanitized-window-evidence/description-recovery-zlcsitbe，均位于build-acquisition-recovery-window，独立持久记录核对通过。系统Qt、普通窗口/receiver/数据服务和旧库没有因此整体插桩。
- 用户20GiB所有流总量限制保持，每网络组512MiB，原生守卫组零raw；各已完成组自有raw清理为零。Qt仅控制/小型状态，不接收或转发raw。包内Release窗口两编码通过：package-window-evidence/description-recovery-sjapd5gx，独立持久核对通过。阶段检查点为build-acquisition-recovery-window/checkpoint.json，goal_complete=false。新文档包build-packages/getdaq-host-20261014-acquisition-recovery-window（23程序/142文件）和getdaq-windows-qt5142-20261014-acquisition-recovery-window（6/237）与本轮生产回执匹配，程序与70.43包相同；Windows沿用已核对相同程序/DLL的初始窗口与无SDK PATH证据，清单明确标注来源，不冒充新菜单人工验收。真机、目标平台、持续跨主机吞吐、独立告警/源故障和冷恢复仍待，整体目标未完成。


## 70.45. 独立告警与ICE源UNKNOWN不能被接收恢复清除（2026-10-14）

本阶段生产代码未改，补充两条不同层次的拒绝路径，范围见[恢复说明](ACQUISITION_RECONCILIATION.md)。

- 独立硬件告警普通两编码通过：build-independent-recovery-holds/alarm-evidence/description-recovery-dqwaogzb/result.json。实际ECC、两个数据服务/原生TCPreceiver，接收故障停止后原告警订阅又报告告警，两种策略各一次原ID停生产；完成连接、Abort、退休、断电后，两次新ID恢复请求因独立告警失败，不写硬件、不恢复接收监视。tests/check_independent_recovery_holds.py独立核对规范请求SHA/持久失败、原Start complete/held、没有reconciled和最终Retired/父UNKNOWN。
- ICE源普通两编码通过：build-independent-recovery-holds/source-evidence/source-failure-ecc-edcmo_f_。实际受控源、源读出线程及ICEreceiver，异步源故障明确停源后仍UNKNOWN/无End，测试显式持有父接收故障门槛；未知sender连接收尾拒绝，两次恢复拒绝，原Start complete与接收PID/会话保持，未回收/封存/确认End。源专属清理核对尚未实现，因此未达到完整退休断电，不能用本例证明“源故障在完整清理后仍独立阻止恢复”。独立证据核对通过。
- 插桩构建新编译受控源/servant、ICE客户与生成协议、三个源读出组件和两个夹具入口；复用核对来源/头/依赖未变的新ECC核心对象。首轮链接因新对象放在静态库后而失败，不计通过；修正对象在库前的顺序后重建通过：九个严格C++11编译、70个已验证复用核心对象，准确命令/当前来源与对象SHA位于build-independent-recovery-holds/sanitized/build-receipt.json。插桩告警两编码根sanitized-alarm-evidence/description-recovery-id7b_q1m，源两编码根sanitized-source-evidence/source-failure-ecc-dr8wxscu，均在build-independent-recovery-holds，独立证据核对通过。旧GET/Ice静态库、系统Qt及普通receiver/数据服务没有因此整体插桩。
- 告警组保守512MiB，源组2MiB，全流总量仍低于20GiB；已完成组自有raw清理为零。源例退出时的自有子进程回收不当作产品Abort/正常seal。包内Release告警两编码根package-alarm-evidence/description-recovery-uj9afa7s、源两编码根package-source-evidence/source-failure-ecc-4zrdzxqs，均在build-independent-recovery-holds，独立核对通过。普通/插桩/包内共十二场景通过，阶段检查点为build-independent-recovery-holds/checkpoint.json，goal_complete=false。Linux23程序/142文件与Windows六程序/237文件新文档包分别为build-packages/getdaq-host-20261014-independent-recovery-holds和getdaq-windows-qt5142-20261014-independent-recovery-holds；生产字节/424及425输入回执保持，Windows明确沿用相同程序/DLL的旧smoke证据，不代表新菜单人工验收。70.44综合检查点的测试输入/普通SDK夹具程序随本轮更新属历史，生产回执仍匹配。冷恢复、独立故障正向解除、现场/目标平台/持续吞吐仍待，整体未完成。


## 70.46. 明确绑定原告警的失败采集只读恢复（2026-10-14）

新增reconcile-acquisition-with-alarm及GUI独立确认入口，要求同一原receiver UNKNOWN、已完成原独立告警负向停止、完整连接/Abort/退休/断电证明；成功一次持久保存两策略复位、原停止ID和原告警观察SHA，回Described后手动新Prepare。普通接收恢复仍拒绝独立告警；Describe/Prepare恢复统一检查独立门槛。源UNKNOWN、普通已知告警、Prepare告警和冷服务的正向解除仍不属于此入口；不证明物理原因已经修复。

- 客户端43项通过：build-alarm-acquisition-recovery/client-tests/tests.log。包含严格告警字段/原ID绑定、相同归属持久清理与额外失败保持，以及三种实际HTTP延迟状态/告警/接收缓存于本次完成后返回不会重锁。历史完成查询不会解除后续新故障。
- 完整offscreen界面89项通过：widget-evidence/receiver-policy-widgets-jio6kr87，位于build-alarm-acquisition-recovery；新菜单默认取消、Cancel零请求/Yes一次。真实窗口与worker完整闭环另有下列实际服务组，不能单凭菜单信号证明。
- 最终普通服务六项：cold-cache-network-evidence/description-recovery-59_xjkbm；最终插桩服务六项：sanitized-network-evidence/description-recovery-167bh8sl，均在build-alarm-acquisition-recovery。分别覆盖新确认两编码、Prepare原清理后仍不能绕过告警两编码、旧接收恢复继续拒绝独立告警两编码。确认后新Prepare/Configure/Start，再次告警产生新的Stop；原Run不补完成。重开校验新完成记录的历史重放，实际解码并保存cold receiver/alarm缓存，仍拒绝旧确认和直接Start。独立check_alarm_acquisition_recovery_evidence.py核对规范请求/原告警SHA、同一完成行双策略复位、原Start complete/held/reconciled、再次告警及冷门槛。
- 当前插桩正常九命令基线两编码：sanitized-normal-evidence/description-recovery-xoo96t3d。77个严格C++11 ASan/UBSan/LSan编译配方/155来源，最后增量实际复用69并编译8单元，准确命令和摘要在sanitized。旧GET/Ice库、系统Qt及普通receiver/数据服务未整体插桩。
- 最终当前服务的完整窗口两编码：sanitized-window-evidence/description-recovery-_oyz387s；包内Release窗口两编码：package-window-evidence/description-recovery-6zfy5jsx；包内新确认/再次告警/冷缓存两编码：package-network-evidence/description-recovery-4ods9wrj，均在build-alarm-acquisition-recovery。窗口采用生产MainWindow/RunControl组件探针，后端和接收进程为实际程序。独立check_alarm_acquisition_recovery_window_evidence.py核对每窗口十份实际请求/本地意图/终态与十二后端操作，多出的两项分别为原接收故障与原告警自动停生产。原journal记录native_alarm_acknowledgement_operation、hardware_recovery_complete和duration_incomplete，仍Interrupted/无CSV/不推进编号，手动新Prepare成功。
- 前一轮普通网络六项、真实窗口两编码和正常两编码也通过，但其后发现冷重开策略存在latched+watching格式不一致；该轮不证明修正后的程序。已修为无原ID时startup-reconciliation、有原ID时reopened-held，完成重放同时要求transition=0。原SDK测试/服务程序保存在historical-before-cold-cache-fix，历史执行根final-network-evidence/description-recovery-badvpxjh、window-evidence/description-recovery-3cxe0hp0、normal-evidence/description-recovery-htsist0g不计入当前二十项网络/窗口/正常执行。最早network-evidence仅前期探索版本，亦不计当前通过。
- Linux完整23程序/424输入及用户Windows Qt5.14.2 MinGW六程序/425输入实际构建通过；新包build-packages/getdaq-host-20261014-alarm-acquisition-recovery（23/142文件）与getdaq-windows-qt5142-20261014-alarm-acquisition-recovery（6/237）清单核对，Windows匹配DLL、无SDK PATH、初始原生及offscreen窗口检查通过；新菜单人工操作及独立干净Windows未验。阶段检查点build-alarm-acquisition-recovery/checkpoint.json保留goal_complete=false。

所有已完成组合成raw已删除；各网络组512MiB、窗口回归256MiB，所有流总量保持用户20GiB限制。Qt只处理控制/小型状态，原始数据仍由独立原生程序传输。普通已知/Prepare告警专用正向解除、源UNKNOWN完整清理/正向核对、冷服务恢复、真实加载/全局控制权、Ubuntu24/macOS/独立Windows、现场与持续跨主机吞吐仍待。70.45及更早综合当前来源检查点随生产改动已历史，原包与证据保留；整套目标未完成。


## 70.47. 原失败 Prepare 的明确告警确认与只读恢复（2026-10-14）

新增 `reconcile-preparation-with-alarm`，明确绑定原失败Prepare、原自动告警停止ID和原观察摘要；同一服务原停止/断电/自有订阅关闭证明齐全，且当前描述/停止/供电/数据状态只读核对通过，才提交已知Described。原失败记录保留，必须手动新Prepare；不启动receiver、不确认原始数据完整，也不证明物理告警原因修复。普通恢复命令仍拒绝独立告警，错误ID和冷对象接管拒绝。

- 客户端46项、最终完整offscreen QWidget89项通过；客户端含两种确认命令的严格必填/禁止/不同ID、成功后只清对应原Prepare与已确认收尾、额外未确认收尾在重开后仍保持。最终窗口回归位于final-widget-evidence/receiver-policy-widgets-43f4c57l，客户端日志client-tests/tests.log，均在build-alarm-preparation-recovery。
- 最终普通网络四项：final-network-evidence/description-recovery-mxwc0xzp；插桩网络四项：sanitized-network-evidence/description-recovery-_q91exhw。各含Prepare告警确认两编码及原采集告警恢复两编码。Prepare先验证普通恢复仍拒绝、错误告警ID拒绝，再确认成功且零恢复硬件写入、手动新Prepare成功；冷重开仍拒绝旧对象，历史完成行可重放。原采集回归另验证新Run再次告警停止与原Start/held/reconciled历史。两个独立检查器分别核对准确请求文件/规范SHA、原告警观察摘要及同一完成行只复位对应策略。
- 普通完整窗口两编码：window-evidence/description-recovery-3rpdl2jr；插桩后端窗口两编码：sanitized-window-evidence/description-recovery-pz649lh5；包内Release后端窗口两编码：package-window-evidence/description-recovery-xs50zohx。前端是生产MainWindow/RunControl组件探针，实际ECC/两个数据服务独立进程；每窗口五份本地意图和终态对应六个后端操作，多出原告警策略自动停生产。Cancel零请求、Yes一次，原Prepare失败/自动停止/人工断电/明确确认/手动新Prepare到Prepared，数据服务始终Fresh、无receiver。check_preparation_alarm_window_evidence.py独立比对准确请求文件、摘要、客户端/后端持久记录与原告警绑定。
- 包内Prepare确认/冷重开两编码：package-network-evidence/description-recovery-9fj6jw1p；插桩正常九命令流程两编码：sanitized-normal-evidence/description-recovery-ep187o25。当前共18个网络/窗口/正常场景；先期network-evidence/description-recovery-bms7apof的六项也通过，但之后增加窗口夹具分支，最终SDK夹具摘要已改变，故先期六项只保留历史、不计入当前18项。
- 插桩新核心77个严格C++11 ASan/UBSan/LeakSanitizer编译配方，155份跟踪来源，本次77全部重新编译、零对象复用。准确命令/来源/依赖/生成摘要在sanitized；旧GET/Ice库、系统Qt、普通receiver/数据服务以及普通窗口前端未整体插桩，不扩大验证范围。
- Linux完整23程序/424输入、用户实际Windows Qt5.14.2 MinGW六程序/425输入重新构建通过。新运行包build-packages/getdaq-host-20261014-alarm-preparation-recovery（142文件）和getdaq-windows-qt5142-20261014-alarm-preparation-recovery（237文件）清单核对；Windows匹配DLL/插件、无SDK PATH及初始原生与offscreen窗口通过，新菜单人工与独立干净Windows未验。当前检查点build-alarm-preparation-recovery/checkpoint.json保留goal_complete=false。

所有已完成组测试raw已删除。每网络组512MiB、窗口回归256MiB；并发三网络组保守总预算1.5GiB，低于用户所有流20GiB上限。Qt只处理控制/小型状态，原始数据继续走独立原生进程。普通已知Stop告警、源UNKNOWN完整清理/正向恢复、冷服务对象恢复、运行中配置切换/实际加载/全局控制权、Ubuntu24/macOS/独立Windows、现场兼容与持续跨主机吞吐仍未完成。70.46及更早综合检查点随当前生产/测试来源与SDK程序更新已历史，原包和证据保留；整套目标尚未完成。


## 70.48. 普通已知告警的正常收尾与人工点击等待（2026-10-14）

新增 `release-stopped-alarm`：同一原服务的已知Ready、原自动正常Stop或held-no-run观察、原请求和实际原生审计均匹配，且无独立故障时，明确正常Breakup/receiver排空退出封存、Unprepare断电及自有订阅关闭，再只读核对描述/停止/供电/订阅。最后数据查询后再次检查原Stop、Breakup及新观察证明，最终Reset提交Described，网络完成行才复位原告警。原Run仍Interrupted，不补CSV或推进编号；确认不证明物理告警原因修复。

- 客户端49项、原生Qt流程模型24项、最终完整offscreen QWidget89项通过，日志分别client-tests/final-tests.log、native-gui-tests/tests.log、final-widget-evidence/receiver-policy-widgets-t7vb89io/widgets.log，均在build-stopped-alarm-release。窗口默认Cancel零请求/Yes一次；正常Stop的两个原操作字段可相同，held-no-run允许明确空告警操作ID，其余旧命令规范字节和UNKNOWN确认规则保持。
- 最终普通网络六项：network-evidence/description-recovery-a1wy_0v7，自动Stop/原Ready/断电应答丢失各两编码。新核心插桩网络四项：sanitized-network-evidence/description-recovery-k_rk0hzc，自动Stop编码0、原Ready编码1、断电应答丢失两编码。成功后手动新Prepare/Configure/Start，第二次告警仍停止；冷重开验证历史而不接管。断电失败可能已有正常seal，但父UNKNOWN和原告警保持，换ID不重放。独立check_stopped_alarm_release_evidence.py核对原成功记录未改写、规范请求/原观察SHA、同一完成行告警复位、原生Breakup/Reset及最终真实审计清单。
- 最终完整窗口普通两编码：stable-window-evidence/description-recovery-8zjdq4iu；新核心插桩后端两编码：stable-sanitized-window-evidence/description-recovery-ei3hx8p2；包内Release后端两编码：stable-package-window-evidence/description-recovery-v6v1daae。前端为生产MainWindow/RunControl组件探针，后端和receiver为独立实际进程；每窗口六份实际请求/本地意图/终态对应七后端操作，多出原告警自动Stop。独立check_stopped_alarm_release_window_evidence.py核对原Run Interrupted/无CSV、原会话/PID/目录、正常reaped/seal、明确确认及手动新Prepare。
- 插桩旧恢复回归两项：sanitized-regression-evidence/description-recovery-tod2qoxl（Prepare告警确认编码0、receiver UNKNOWN告警确认编码1）；旧采集检查器以--expected-ack 1明确本组只选一场景，其余逐条证明不放宽。正常九命令流程插桩两编码：sanitized-normal-evidence/description-recovery-tnzofe3x。最终包内普通告警收尾两项：stable-package-network-evidence/description-recovery-qr2g2mf6（自动Stop编码0、原Ready编码1）。当前合计22个网络/窗口/正常执行；不把其他阶段/中间版本结果合并计数。
- 包内前一窗口暴露人工点击与只读轮询之间的时序问题，deadline停在新Prepare之前；该失败保留在package-window-evidence/description-recovery-pbakfoyn，不计通过。最终实现恢复后新元数据等待，并最多暂存一次轮询中的人工Progress；新状态/原操作ID仍相同才持久提交一次，变化/故障/过期取消。native-gui-tests/progress-regression-result.json及隔离保留旧Progress行为的程序证明：旧行为确定丢点击，最终实现健康时只发一次、故障时取消。旧metadata专用探索用例和中间窗口结果只诊断，不冒充最终版本验证。
- 插桩77个严格C++11 ASan/UBSan/LeakSanitizer编译配方、155份跟踪来源，77全部重新编译；准确命令/来源/依赖/生成摘要在sanitized。系统Qt、旧GET/Ice库、普通receiver/数据服务及普通窗口前端未整体插桩。后续GUI时序修正不修改这组原生核心来源；Qt模型/客户端/完整窗口另以最终实际程序验证。接收器/数据服务固定副本在runtime-baseline，检查点逐角色绑定实际路径与SHA。
- Linux完整23程序/424输入、实际用户Windows Qt5.14.2 MinGW六程序/425输入最终重建通过。最终包build-packages/getdaq-host-20261014-stopped-alarm-release-final（142文件）及getdaq-windows-qt5142-20261014-stopped-alarm-release-final（237文件）包含最终GUI修正；Windows匹配DLL/插件、无SDK PATH及初始原生/offscreen窗口通过，新菜单人工和独立干净Windows未验。早期无final后缀包和对应结果仍保留，不作为最终GUI验收。当前检查点build-stopped-alarm-release/checkpoint.json保留goal_complete=false。

已完成组的自有raw均已删除。每网络组512MiB、窗口回归256MiB，最大并发保守预算3GiB，所有流合计保持用户20GiB上限；不删除生产/参考数据。Qt只处理控制和小型元数据，原始数据通路不经GUI。源UNKNOWN完整收尾/正向恢复、部分失败后更完整的硬件恢复、冷服务对象接管、运行中配置切换/实际加载/全局控制权、Ubuntu24/macOS/独立Windows及现场兼容/持续跨主机吞吐仍未完成。70.47及更早整组当前来源匹配随本轮更新已历史，原包/证据保留；整套目标尚未完成。


## 70.49 ICE源退休与同一原采集的明确恢复（2026-10-15）

新增可选 `DataSourceRecovery`，保留原控制接口字段与方法。原生产停止已确认后，明确退休原故障源数据连接，不发End，旧源UNKNOWN、错误及计数保留；完整连接/Abort/数据退休/断电证明通过后，`reconcile-acquisition-with-sources`只读回到Described，旧Run仍中断，必须人工新Prepare。普通采集恢复仍拒绝源故障，冷服务不接管。详见[操作及原理](FAILED_ICE_SOURCE_RECOVERY.md)。

当前结果共22场景：原生两编码，普通源控制七场景，完整MainWindow两编码，候选包内HTTP两编码，新核心/受控源插桩HTTP两编码及插桩源控制七场景。源控制覆盖成功、其他控制者、丢ACK、空闲失败和原控制连接丢失，均不制造End/封存、不重复原停止。原生和HTTP另核对新会话准确数据字节与正常封存；GUI只覆盖恢复后人工新Prepare。

独立检查器逐项核对源退休清单/子文件SHA及恢复观察绑定；GUI每场景12份实际请求/规范SHA/本地持久意图及终态对应12后端操作。原Start成功/held与reconciled历史保留，旧Run Interrupted/时长不完整、无CSV、不推进编号，原接收会话/PID/目录身份及已回收未封存保留。客户端52/完整QWidget89/NativeRunControl模型24项通过。

Linux23程序/424生产输入、用户实际Windows Qt5.14.2 MinGW六程序/425输入重新构建，候选包143/238清单文件；Windows匹配DLL、无SDK PATH/初始窗口smoke通过，不是独立目标机或新菜单人工验收。严格C++11 ASan/UBSan85条编译配方/172来源记录、零对象复用；旧GET/Ice、系统Qt、普通receiver/数据服务未整体插桩。

本轮普通完整窗口初次因夹具漏选编码0导致外层检查失败，原根`window-evidence/description-recovery-qf4uqa5q`不计通过。初次插桩因工具漏链接SourceMfmRing失败，`sanitized/`不计通过；修正后的`sanitized-final/`编译和运行结果才计入。本轮前期普通HTTP结果发生于新增窗口夹具/范围元数据修正前，保留为历史，不与当前包内结果重复计数。原生最新结果已含退休清单绑定与准确新数据字节断言。

当前结果及来源/程序/包摘要见`build-source-retirement/checkpoint.json`。各测试网络组512MiB，原生2MiB、源控制8MiB、QWidget256MiB，最大并发网络预算1GiB；用户所有并发测试流合计20GiB上限保持，结束删除自有raw。Qt只传控制/小型状态，原生进程传数据。环境为Ubuntu20.04.6/WSL、GCC9.4、Qt5.12.8。板端getHwServer/IceEirfu不重构；冷/部分失败恢复、实际配置加载/运行中选择/全局控制权、Ubuntu24/macOS/独立Windows及现场/持续跨主机吞吐仍待，整套未完成。


## 70.50 原正常接收周期的控制权交接（2026-10-15）

新增同计划 `handoff-control`，规范请求绑定原控制范围/持有期和明确的新范围/持有期。只允许同一健康服务对象中正常Stopped周期，重新核对原接收器已回收/正常退出、封存、目录与输出锁、文件集合和大小；不读事件载荷、不修复失败会话。交接证明和新归属同一持久complete行提交，旧ID仅查询，同ID改接班者拒绝，丢应答不自动重发。

原生CLI可提交/查询，旧命令规范字节无新增字段。多节点适配器先保存全部请求后逐节点各POST一次，成功或不确定后原对象均保持门槛；预绑定新父快照/随机持有期的新适配器，仅可凭完整回执并向各原服务新查原完成ID/即时归属/原封存周期后认领，不凭Stopped自动接管。不是跨节点原子事务、计划参数加载、板卡/跨主机全局锁或完整ECC/GUI配置切换。

当前26场景：普通/插桩数据服务各九（正常新周期、实际丢应答、冷服务、证明/规范请求/目录身份篡改、封存大小变更、输出锁占用、独立保持），普通/新适配器插桩各四（两编码明确新对象核对/新周期、丢应答不接管、回执变更后恢复仍保持）。独立检查器核对实际规范请求域/SHA、逐条持久owner变更、原请求文件、交接文件摘要及正常封存周期。原数据字节在交接测试中保持，测试完成删除自有raw。

既有数据服务normal/ownership/restart/preconditions四场景、八项原生HTTP故障以及协议/凭据检查通过；旧运行器未自动清理的49152字节自产raw已在其进程终止后按私有结果根删除，并保存payload-cleanup.json。本轮其他新运行器均在finally内清理，使用2MiB载荷预算；用户所有并发测试流20GiB上限保持。Qt仍只在控制前端使用。

普通/插桩数据服务各60条严格C++11配方/121输入；适配器插桩16编译、62个对象经实际仓库依赖核对后复用，266项依赖记录，另168项夹具依赖与编译前已有记录一致。系统Qt、旧GET/Ice、普通receiver未整体插桩。Linux23程序/424输入和实际Windows六程序/425输入重建通过。Windows程序与上一阶段字节相同，新239文件文档包沿用相同DLL/程序的既有初始窗口检查；不是新交互人工或独立目标机验收。Linux新包144文件。

早期remote-evidence/remote-controller-handoff-f7if9_dh最后一个场景因测试期待错误门槛文本而失败，不计通过；修正后Gate先返回UNKNOWN，普通/插桩四项均通过。阶段检查点为build-controller-handoff/checkpoint.json，结果根与操作原理见[手册](DATA_CONTROL_HANDOFF.md)。70.49来源/SDK程序匹配已随本轮源码更新而历史，原包与原证据保留。

完整ECC配置版本切换、接收计划参数切换、板卡主机占用连续交接及GUI、冷/部分交接恢复、Ubuntu24/macOS/独立Windows、实机和持续跨主机吞吐仍待；板端getHwServer/IceEirfu保留现部署，整套未完成。

70.50补充边界：候选包服务/CLI的success、ack-loss、cold三项额外联调，在启动前因自动审批额度限制未执行，不计通过；26个已通过场景来自前述有构建收据的普通/插桩程序。候选包仅作文件/构建来源完整性核对，保留该补充待验项。


## 70.51 不取得板卡占用的下一实例离线规划（2026-10-15）

新增原生 `PlanNativeEccConfiguration` 和既有服务程序的 `--plan-config` 模式，抽取并共用真实构造原有的工件绑定/参数规范化/描述与发送器校验/规范快照序列化，提前输出父快照和数据控制范围。正常启动随后才取得占用、创建运行根及绑定数据适配器；没有新增可执行程序。

普通/新代码ASan/UBSan各两编码、每组七项，共28个离线场景通过：独立文件清单绑定/先前真实启动快照/控制范围摘要核对，重复输入确定性，缺失新运行和占用目录，已持有的主机占用，非法相对路径、重复发送器及冲突命令模式。只替换私有测试认证文件路径，先前实际启动快照作为规范内容依据；测试独立重算固定输入文件摘要，不仅比较软件自报。

专用拦截库拒绝并记录connect/bind/listen/sendto/sendmsg，四组记录均无命中；未创建规划的运行根/占用根，已有占用文件不改，未输出认证值，没有raw载荷。ASan运行将运行库放在拦截库之前；启用ASan/UBSan但禁用泄漏检查，未宣称LSan通过。插桩20条严格C++11编译、62个对象按实际仓库依赖复用、374依赖；旧GET/Ice和系统Qt未整体插桩。两改动C++编译单元的严格C++11语法检查通过，Linux23程序/424生产输入重新构建。

当前结果根和使用步骤见[规划手册](ECC_SUCCESSOR_PLANNING.md)，检查点为build-ecc-successor-planning/checkpoint.json。早期无完整runner摘要的同类探索记录不重复计数。当前Linux文档包145清单文件，程序字节与普通测试一致；Windows操作台源码本轮未变，保留上一阶段包，尚未刷新本轮整个后端来源收据。70.50源/SDK程序的当前匹配随本轮改动已历史，原包与原证据保留。

自动审批额度限制期间，需要该审批的联网/进程检查未执行；离线验证在额外拒绝网络调用的环境完成，不替代联网启动回归或真实硬件。规划不生成新进程实例/持有期，不切换数据计划、不取得控制权、不证明实际配置加载。完整配置切换、板卡占用连续交接、冷/部分恢复、GUI、目标平台与现场/持续跨主机吞吐仍待，整套未完成。

## 70.52 持久接班身份与真实作业启动核对（2026-10-15）

新增原生C++11 `EccSuccessorPlan`及`--create-successor-plan`、`--check-successor-plan`；保存精确固定输入、父快照、数据范围和一次生成的随机持有期。文件绑定原路径/身份/权限/单硬链接，内容变化保持故障；实际激活取得存活期独占锁，独占持久写入标记，不自动冷重用。ECC作业和服务启动已接计划与原交接回执，构造前及父根绑定核对快照/范围，数据适配器缺少回执时在远端查询/命令之前拒绝。正常启动方式保留。

普通SDK与新代码ASan/UBSan构建各两编码、每编码19个离线场景；另有实际统一交付程序19项及原离线规划七项回归，共102项。额外网络拒绝/记录库未观察到网络尝试，无原始载荷。覆盖复制、硬/符号链接、计划/激活标记篡改与恢复保持、未激活/重复/已消费拒绝、错误父根/快照、缺少回执、实际ECC作业构造拒绝无效回执及CLI冲突。离线ASan组明确关闭泄漏检查。

审批恢复后，实际两数据服务/原生receiver联调普通及新控制ASan/UBSan/LeakSanitizer各六场景：保存身份后接班两编码、原交接正常两编码、应答丢失及接纳回执篡改。保存身份场景先正常采集封存，离线生成目标但不建立新审计根，原适配器向该目标一次交接，再实际构造新的 `EccControlJobs`，重新核对原交接和停止/封存状态。计划/激活标记/原回执/新状态的摘要、范围与持有期一致；不写硬件，原载荷摘要保留。独立检查器各核对12次节点交接提交与原请求/证明/正常封存周期。此组数据服务与receiver为普通构建；旧GET/Ice与系统Qt未整体插桩。新控制插桩构建分别为服务21编译/62对象复用/376依赖、离线夹具19/60/269、联网夹具19/60/281，全部使用严格C++11配方并按实际依赖核对复用。

同版实际ECC独立服务的原普通完整流程两编码另通过，共14个最终联网场景；接班场景通过的是实际作业构造，而不是带新参数启动独立ECC可执行文件的全链路。最终结果根：

- 普通身份：`build-ecc-successor-identity/final-ordinary-encoding{0,1}/`。
- 插桩身份：`build-ecc-successor-identity/sanitized-encoding{0,1}/`。
- 统一交付程序：`build-ecc-successor-identity/delivery-ordinary/`；规划回归：`planning-regression/`。
- 普通交接：`build-ecc-successor-identity/final-network-evidence-v2/remote-controller-handoff-bsbnjdla/`。
- 插桩交接：`build-ecc-successor-identity/sanitized-network-evidence/remote-controller-handoff-1fsfppc5/`。
- 原服务流程：`build-ecc-successor-identity/normal-service-regression/description-recovery-912oodh3/`。

普通/插桩交接组各16个载荷文件/131072字节，原服务流程八个/131072字节，均已删除；前期普通交接探索组也已清理，不重复计入最终场景。并发保守总上界514MiB，低于用户所有流20GiB上限；检查点全阶段无.graw。新功能不进入原始数据通道，没有新增第三方依赖。

Linux完整23程序/426输入、用户实际Windows Qt5.14.2 MinGW六操作端/427输入已重新构建。候选目录包为 `build-packages/getdaq-host-20261015-ecc-successor-identity-candidate`及`getdaq-windows-qt5142-20261015-ecc-successor-identity-candidate`。Windows六程序字节与既有独立DLL初始启动验证包一致，匹配程序/DLL不变，仅刷新当前构建收据与文档；明确复用原smoke，不声称新人工验收。

权威本地证据索引为 `build-ecc-successor-identity/checkpoint.json`，复核入口 `python3 tests/check_ecc_successor_identity_checkpoint.py`。70.51源码/SDK整组映射随本轮变更成为历史，原包、原证据和原摘要不改。

仍待：带新参数的独立ECC进程全链路、旧ECC服务对外交接入口、板卡占用连续移交、不同接收计划切换、完整GUI、冷/部分失败恢复，以及Ubuntu24/macOS/独立Windows、真实硬件和持续跨主机吞吐验收。整套重构未完成，不从子用例数量推导总体百分比。详细操作/文件含义/失败边界见[接班身份说明](ECC_SUCCESSOR_IDENTITY.md)。

## 70.53 原ECC服务交接、操作工具与两个独立进程（2026-10-15）

原服务新增认证命令`handoff-data-control`与只读`GET /v1/control-handoff`，保持现有单作业、原实例/快照/客户端/持有期绑定。只接受正常收尾后已知Idle/Described、所有原接收器退出/解锁/封存且健康的周期；新鲜无采集的Idle拒绝。数据绑定必须相同，新目标范围/持有期必须不同。受理记录持久保存后，原实例永久拒绝后续新控制，包括负向命令；重复原ID只查询缓存，目标改变则拒绝，冷重开从原记录恢复门槛。原后台故障处理不提交新的交接后控制动作。

新增原生C++11 `getdaq-ecc-handoff submit/inspect`。它核对新旧固定输入、原服务身份、相同实验/工件/数据计划，保存原请求/目标/状态，仅一次POST；`inspect`只GET，不改ID重发。提交期间持计划共享锁，启动者的独占锁与之互斥；激活/审计历史已出现时拒绝。已有证据目录拒绝再次submit，已有结果不覆盖。CLI动态依赖核对无Qt，没有新增第三方库；原始DAQ数据继续走独立原生接收器。

最终普通与新控制ASan/UBSan/LeakSanitizer各两编码，共四个完整真实本机网络场景通过。每场景先经过原正常完整工作流及两次接收周期，完成Reset，再原服务交接、停旧进程、带固定计划和原回执启动新的独立ECC进程，核对原两数据服务的实例/操作/证明/正常封存周期。新实例与旧实例不同，持有期/范围/计划/激活标记/操作请求键绑定一致；交接及接班启动不写硬件。已消费计划再次启动拒绝；旧ECC冷重开后的新Prepare/Reset/负向控制均拒绝，且不把历史回执作为新实例实时权限。

编码1场景还执行真实操作CLI：先在计划独占锁被持有时确认提交被拒绝；实际提交后确认CLI共享锁阻止另一个启动者取得独占锁。测试专用拦截器实际消费一次CLI收到的POST应答并返回EOF，工具退出不确定结果且保存保持文件，随后测试启动器仅执行inspect查询原操作。每组精确记录`post`及`drop`各一次；重复目录submit拒绝，数据节点交接各一次。专用拦截器/启动器不交付、不记录令牌/请求头或载荷。API相同ID及更改目标、未采集及不同数据绑定的反例也通过。未声称覆盖节点交接中途失败后的完整恢复。

最终结果根：

- 普通：`build-ecc-service-handoff/locked-final-ordinary/description-recovery-5kj172xi/`。
- 插桩：`build-ecc-service-handoff/locked-final-sanitized/description-recovery-j38v8huj/`。

Qt客户端重新构建后52项通过。纯C++11协议测试新增四个目标字段的请求键/缺失/误用检查，旧请求规范字节保持；再读取上述四场景的真实身份、最终状态、网络受理和终态日志，140份实际记录通过。独立检查器核对四次父交接意图与八次节点持久提交、原证明摘要及新状态，区分当前原实例回执和冷缓存。

新控制插桩采用严格C++11：网络夹具20条重编译/59对象复用/284依赖；服务另三条/80复用/376依赖；最终操作工具另一条/78复用/269依赖。后两组只在编译配方、依赖及已记录对象SHA一致时复用首组新对象，保存编译对象摘要和父收据。实际普通/插桩最终程序均用于对应联调；数据服务与receiver为普通构建，旧GET/Ice及系统Qt未整体插桩。

最终普通/插桩组各八个.graw、131072字节已删除；全阶段含前期探索组均无自有.graw。两组并发保守上界1GiB，低于用户所有流20GiB约束；失败和信号路径也清理自有载荷。首个仍要求Described的探索暴露既有正常流程最终回Idle，随后补“正常封存后的Idle/Described”门槛；共享锁补充前的通过组只作诊断，不重复计入四最终场景，也不改原结果摘要。

Linux完整24程序/427输入（增加操作CLI）、实际Windows Qt5.14.2 MinGW六操作端/428输入重建通过。Windows GetDAQ字节更新，重新用匹配DLL生成独立目录，完成无SDK PATH的offscreen和短暂Windows原生初始窗口检查；不是交互功能、独立机器或实机验收。最后仅POSIX CLI共享锁补充未改变Windows程序字节，最终来源收据已刷新，窗口证明仍对应同一组实际程序。当前候选目录：

- `build-packages/getdaq-host-20261015-ecc-service-handoff-candidate`。
- `build-packages/getdaq-windows-qt5142-20261015-ecc-service-handoff-candidate`。

权威本地检查点：`build-ecc-service-handoff/checkpoint.json`；复核：`python3 tests/check_ecc_service_handoff_checkpoint.py`。其记录普通/插桩结果、客户端、协议记录、实际程序、编译依赖、包及源码/文档摘要。70.52原包和证据保持，当前源码/SDK整组匹配随本轮变化只作历史。

两个独立ECC进程的同计划更替已有本机证明。仍待不同实验参数/接收计划受控切换、连续板卡主机占用移交、GUI、部分/冷交接恢复，以及Ubuntu24/macOS/独立Windows、真实板卡和持续跨主机吞吐验收；整套重构未完成。操作步骤、文件与原理见[服务交接手册](ECC_SERVICE_HANDOFF.md)。


## 70.54 不同接收计划的数据服务切换（2026-10-16）

这是数据服务层的计划更替，尚未接入父ECC不同配置编排、连续板卡主机占用移交或GUI。原70.53同实验/同接收计划的ECC进程更替仍保留原边界。整套重构未完成，无真机验收。

新增原生C++11 `SupervisorNextPlan`、`ReceiverCycleEvidence`、`SupervisorPlanPredecessor`，服务认证命令 `retire-plan`、只读 `/v1/plan-retirement` 和 `/v1/predecessor`，以及离线 `--plan-config`、独占0400文件 `--write-plan` 和一次性 `--predecessor-proof` 启动。原服务仅在同一实例/控制者的正常Stopped并完整封存周期受理，受理即持久保持；新服务核对原请求/审计完成/正常周期、目标计划、程序摘要及内存策略，再取得旧部署租约、写激活标记并绑定预先授权的新归属。冷旧实例或已消费启动不自动接管。

普通/严格C++11 ASan/UBSan/LeakSanitizer/新候选包Release各12项，共36项不同计划场景：

- 正常切换、实际消费一次POST应答后的原操作查询、原服务仍持部署租约时拒绝且不消费新根/标记。
- 复制证明、原封存漂移、raw大小改变、新审阅计划内容改变、旧实例冷重开保持。
- 激活后周期证据、原生命周期审计或原部署锁身份改变：查询和新Start保持，恢复原字节/路径也不解除。
- 新服务已激活后，持有旧receiver输出独占锁并移动全部旧receiver输出目录，仍能核对前驱及完成实际新接收周期；旧载荷内容保持。

成功周期核对实际receiver进程命令行：队列、最大帧、轮转均从原值1改为2MiB，配置/工件/计划摘要变化，新服务实例不同，范围/持有期与原退休目标一致。新鲜或活动周期、错误会话/归属/认证、总内存策略放大、缺失审计父目录、嵌套根、缺失输出根和替换receiver程序均拒绝。规范计划预览与实际封存字节一致，重复写入拒绝覆盖。

普通及候选包组为服务注入测试专用 `.graw` 读取打开拦截器，先用1字节自检确认确实拒绝读取，再确认服务没有任何raw读取尝试；receiver只写入，控制层只对raw做stat。拦截器不交付。插桩组单独运行相同生产源，系统Qt及普通receiver未整体插桩，不能由此声称所有第三方库均经过内存验证。

最终证据根均位于 `build-data-plan-transition/`：

- `verified-ordinary/data-plan-transition-kggggb2j`。
- `verified-sanitized/data-plan-transition-rctq_u6v`。
- `package-evidence/data-plan-transition-gkq0k_dp`。
- 原同计划交接九项回归：`same-plan-regression/controller-handoff-5l835ykx`。

新独立检查器直接核对原请求、唯一受理/完成行、原完成状态、退休证明/周期证据/新计划摘要、激活标记、新实例和归属、实际receiver参数及归档目录。结果绑定实际运行的普通/插桩/候选包程序摘要和当前运行器，不以PASS文本代替关联核对。每个不同计划组32个.graw共131071字节（故意截短一个字节的反例包含在内），同计划回归22个共90112字节，已全部删除；失败和信号路径同样清理自有raw。每组2MiB保守上限，本轮并行普通/插桩预算共4MiB，低于用户所有流合计20GiB要求。

数据核心及服务普通/插桩构建各62条严格C++11编译配方、125输入，保存源码/生成文件/配方和七程序摘要。这里是配方总数，不表示每次增量都重编译62个对象。普通/插桩codec检查均通过，旧请求规范字节保持。Linux完整24程序/431输入和用户实际Windows Qt5.14.2 MinGW六操作端/432输入重建通过。新Windows独立目录重新部署匹配DLL，完成无SDK PATH、offscreen及短暂原生初始窗口检查；不是人工全部功能或独立机器验收。

新候选包：

- `build-packages/getdaq-host-20261016-data-plan-transition-candidate`，24程序、149文件。
- `build-packages/getdaq-windows-qt5142-20261016-data-plan-transition-candidate`，六操作端、243文件。

后续仅刷新包内文档，运行程序和DLL保持实际测试字节。旧70.53包/检查点保持历史，源码变化后不能把其整组摘要描述为仍匹配当前工作树。

检查点 `build-data-plan-transition/checkpoint.json`，只读复核：

```bash
python3 tests/check_data_plan_transition_checkpoint.py
```

该检查点明确 `goal_complete=false`、父ECC不同计划及GUI尚未集成。已通过九场景的较早探索结果不重复计入36最终场景；归档锁问题修复前的结果也不代表当前实现。后续应接父ECC审阅/各节点原操作/新计划启动核对，再接操作台。仍待部分/冷切换恢复、连续主机占用、Ubuntu24/macOS/独立Windows、现场协议与实际加载、持续跨主机高吞吐验收。步骤、文件边界和失败处理详见[计划切换操作与原理](DATA_PLAN_TRANSITION.md)。


## 70.55 父ECC与新数据计划的完整接班及新周期（2026-10-16）

新增父命令retire-data-plans，严格共享协议把排序且唯一的每节点agent/目标计划摘要/节点本机审阅路径与原实例、快照、客户端、新ECC计划/范围/持有期及新数据绑定一起纳入请求键。保持8KiB请求总预算，误用字段、重复/乱序节点、相对或非规范路径、超预算编码均拒绝；旧请求规范字节不变。原服务只接受正常收尾到已知Idle/Described、完整正常封存且健康的周期，保存全部节点意图后各POST最多一次。受理即持久保持，部分失败、冷重开及重复旧ID均不能重放新控制。

原生操作工具增加retire-submit/retire-inspect，核对新旧固定配置、目标计划及准确数据绑定，沿用证据目录和接班身份共享锁。当前要求节点集合、控制地址/端口、控制者和超时策略保持；不同计划分别代入原端点集合重算绑定，必须等于下一ECC计划。新父收据getdaq.remote-plan-retirement关联每个原退休请求、请求键和证明摘要。

原ECC关闭、各新数据服务用原证明启动后，新ECC带保存的身份及父收据启动；认证查询各节点新身份和实时前驱证明，要求不同原实例、准确目标计划和归属，初次接纳仍为Fresh/未使用。校验原退休请求、原正常会话和目标后，才允许显式新Describe/Prepare/Configure/Start；不会把旧Stopped周期接成新周期，不自动开始采集。板端为本机合成GET/Ice夹具，未改变getHwServer/IceEirfu范围。

最终九项真实进程场景：

- 普通五项：不同配置两编码、第二节点拒绝后的部分保持、原同计划交接两编码回归。
- 新控制ASan/UBSan/LeakSanitizer三项：不同配置两编码、部分保持。
- 新Linux候选包一项：包内ECC服务/工具、数据服务与receiver完成编码1.1全链路。

其中五次新配置周期实际改变合成实验工件中的一个AsAd触发延迟，配置/工件摘要不同；receiver队列/最大帧/轮转从1改为2MiB。旧ECC先完整收尾/退休，各节点换实例，新ECC核对后从Fresh开始，完成新的Describe、Prepare、Configure、Start、Stop、Breakup、Reset。新状态关联新计划、新配置、新持有期/范围及新会话，raw字节与独立发送端一致。

两次部分场景实际移除第二节点的自有审阅文件；第一节点受理退休一次、第二节点零受理，父操作失败，无完整父收据，旧ECC和冷重开均保持；不重试、不回滚，也不把保持称作恢复。编码1.1执行真实retire-submit/retire-inspect与重复submit目录拒绝。本轮没有注入新退休命令的实际POST丢应答；原70.53的丢应答证据不替代该范围。

最终根均在build-ecc-plan-transition：

- locked-ordinary/description-recovery-k9t35sxf。
- locked-sanitized/description-recovery-qo2xyvaa。
- locked-package/description-recovery-r7fxl2fl。

独立check_ecc_plan_transition_evidence.py核对父请求/请求键、唯一受理/完成、原正常周期、各节点退休证明/目标计划/完成摘要、接班身份/激活标记、新实例/归属/会话及封存结果。纯C++11协议测试新增目标字段/键/严格拒绝检查，并读取九组315份实际身份、状态、意图和终态。实际codec路径为build-ice-compat/data-supervisor/control-protocol/ecc-protocol-tests；同根旧control-protocol目录遗留程序不能替代当前构建输出。

插桩网络22条严格C++11重编译/59对象复用，服务3条/82复用，工具1条/80复用。新数据周期证据与计划过渡两翻译单元显式加入基线缺失的链接清单，不能依靠旧对象解析符号；后两组仅在配方、实际依赖及对象摘要一致时复用网络组。普通数据服务、receiver、旧GET/Ice和系统Qt未整体插桩。

最终普通24个.graw共360448字节，插桩16个共229376字节，包内6个共81920字节，共46个/671744字节已删除；含前期探索及汇总故障组，阶段全部自有raw零。每组512MiB预算，重验时最多四组并发保守预算2GiB，低于用户所有流20GiB总上限。早期汇总脚本把前驱节点变量命名为before，覆盖程序摘要表，导致流程完成后汇总失败；已改名并重跑全部最终组，早期组不计入九项结果。

Linux整套24程序/431输入与实际Windows Qt5.14.2 MinGW六操作端/432输入重建通过。新候选包：

- build-packages/getdaq-host-20261016-ecc-plan-transition-candidate：24程序、151文件。
- build-packages/getdaq-windows-qt5142-20261016-ecc-plan-transition-candidate：六操作端、245文件。

Windows新目录重新部署匹配DLL，完成无SDK PATH、offscreen和短暂原生初始窗口检查；不代表新切换GUI，因为该界面尚未实现。随后仅刷新文档，程序/DLL字节保持实际检查版本。README按安装/操作/数据/故障/架构导航整理，原专题说明保留FEATURE_HISTORY，不混作当前完成状态。

权威检查点build-ecc-plan-transition/checkpoint.json，只读复核：

```bash
python3 tests/check_ecc_plan_transition_checkpoint.py
```

检查点明确整套goal_complete=false；父ECC不同实验/接收计划路径已有本机证明，下一步GUI、连续板卡占用移交和部分/冷切换恢复。仍待Ubuntu24/macOS/独立Windows、真实硬件协议与实际加载、持续跨主机高吞吐验收。原70.54及更早包/检查点保留历史，源码/文档变化后不能称其整组摘要匹配当前工作树。详细操作与文件用途见[ECC计划切换](ECC_PLAN_TRANSITION.md)。

## 70.56 操作台审阅新计划、服务端审阅锁及回执导出（2026-10-16）

生产窗口PlanTransitionWindow沿用h/cpp/ui，MainWindow菜单接RunControl及NativeRunControl工作线程。两份本地有界JSON仅供审阅：保存接班身份必须为共享C++11编码的规范原字节，节点目标准确覆盖原节点且计划不同；提交前重读两份文件拒绝变化。默认Cancel不产生意图或POST；Yes先持久保存原请求再单次提交。原客户端持久保持禁止所有后续新命令，重开仅观察原操作。窗口不启动远程服务，不读取raw。

原ECC针对可选successor_plan_file在受理前读取并持共享锁，核对准确文件身份、私有父目录/0400单链接文件、规范路径和摘要，要求新审计根及激活标记尚不存在。证明目录保存reviewed-successor-plan.json；父完成持久化后释放审阅锁。冷日志核对使用已保存副本，不能因下一代已消费身份而把正常历史误判成缺失文件，也不以冷重开解除原控制门槛。已存在独占启动锁、目标不匹配、篡改后恢复均有离线守卫场景。

修正两个真实联调暴露的问题：合法接收计划请求可超过4KiB，本地日志的字符串读写预算统一为64KiB，父请求仍保持8KiB；正常退休后control/data两侧needs_reconciliation=true表示预期冻结，回执核对允许双方保持，但独立错误、UNKNOWN、活动操作、非Idle/Described或不完整正常周期仍拒绝。原保持不被清除。身份副本格式化、改变字段顺序或换行会在保存意图之前拒绝。

验证结果：

- Qt客户端55 passed、0 failed；完整界面91 passed、0 failed。包括一次提交/保存先于网络、原状态及回执绑定、冻结与重开、不可变回执、长请求日志，以及Cancel零提交、文件变化拒绝、Yes一次、错误结果与换实例清缓存和准确导出。
- 23项离线接班身份/CLI场景通过，网络调用拦截记录零尝试；其中四项新增服务端审阅锁/启动锁互斥、目标不匹配和篡改保持。
- 三项实际MainWindow/RunControl、认证HTTP、两个独立数据服务与receiver完整场景通过：普通、插桩新控制组件、候选包后端各一项Ice编码1.1。原周期正常结束，实际窗口取消后确认一次、原服务锁阻止提前消费、父回执准确导出、旧控制按钮禁用。随后明确更换数据服务和ECC，真实新工件/不同receiver参数完成新周期；原冷门槛和身份消费后重复启动拒绝保持。
- 独立检查器绑定父原请求/请求键/唯一受理、每个原正常周期及子退休证明/完成摘要、新范围/持有期/实例、新周期封存，以及实际GUI探针/读取拦截器摘要。探针主动尝试一次自有单字节raw读取以证明拦截器工作，日志准确一条raw-read-denied；额外生产界面读取会增加记录并失败。自检文件当场删除。
- 当前纯C++11协议程序读取三组共105份实际身份/状态/意图/终态，规范编码和键绑定通过；程序路径为build-ice-compat/data-supervisor/control-protocol/ecc-protocol-tests。

证据根均位于build-ecc-plan-review：

- corrected-network/description-recovery-cb_e_dcw。
- sanitized-run/description-recovery-fdykqztd。
- package-run/description-recovery-bjnhkc5v。
- 离线current-offline/successor-identity-7y_c04zn；客户端/窗口日志client-current-tests.log与widget-current-tests.log；实际协议actual-records.log。

插桩网络22条严格C++11重编译、59对象复用，服务3条重编译、82复用。配方、实际依赖及对象摘要全部核对；网络组启用ASan/UBSan/LeakSanitizer。系统Qt、普通GUI探针、旧GET/Ice以及普通数据服务/receiver未整体插桩，不把本组称为整套软件内存安全证明。

最终Linux24程序/434输入、用户实际Windows Qt5.14.2 MinGW六操作端/435输入重建。新候选包为build-packages/getdaq-host-20261016-ecc-plan-review-candidate（24程序）及getdaq-windows-qt5142-20261016-ecc-plan-review-candidate（六程序）。Windows在新目录部署匹配DLL、无SDK PATH、offscreen和短暂原生初始窗口通过；新切换流程仍是Linux offscreen实际窗口验证，不是独立Windows机器或人工全部功能验收。包内网络场景使用新Linux包的ECC、数据服务及receiver，窗口探针单独构建且摘要绑定。

三组各六个raw/81920字节，共18个/245760字节已删除，阶段含失败探索组自有raw零。每网络组512MiB、最大两组并发保守1GiB，遵守用户全部流合计20GiB及测后删除。最早GUI探索暴露正常保持状态被误判，修正后重跑普通/插桩/包内最终三组，失败探索不计入通过数。

详细操作与架构见[ECC操作台切换](ECC_PLAN_TRANSITION_GUI.md)。检查点为build-ecc-plan-review/checkpoint.json，复核命令python3 tests/check_ecc_plan_review_checkpoint.py。70.55及更早整组源码映射因本次更改保留为历史，原包和摘要不改写。

整套重构未完成。连续板卡占用移交、部分/冷切换恢复、控制端点/节点变化、Ubuntu24/macOS/独立Windows、现场协议/真实加载和持续跨主机大流量仍待；板端getHwServer/IceEirfu保持不重构。三项窗口场景不能证明这些范围完成。

## 70.57 离线依赖预检查与当前操作文档整理（2026-10-16）

新增tools/verify-dependencies.py，仅使用Python标准库，不下载、不解压、不连接任何服务。full核对105个实际文件：依赖清单、两个SHA256SUMS、93份yaml源码/许可、三个固定源码归档及声明的许可文件。归档大小与dependencies.json摘要及archives/SHA256SUMS一致；yaml固定清单拒绝改动、缺失及未审阅新增文件，路径越界、符号链接和重复条目拒绝。许可核对存在性并记录实际摘要，不把本地摘要当作上游真实性认证。

control只需95个文件，不要求Ice；data不要求third_party目录或任何本地第三方库，检查数零；data-host只核对依赖元数据，不要求yaml/Ice。Qt/编译器仍由平台提供，工具不替代SDK探测或检查全部第一方源码。full/control/data/data-host四份实际预检查回执保存在build-dependency-delivery。

统一build-suite入口在SDK查询/编译与创建构建目录之前执行对应预检查，回执增加embedded_dependencies准确文件摘要；--plan和运行包--verify-stage保持原只读行为。WSL Windows入口也调用control检查并保存95份摘要。source_hashes增加检查脚本、依赖清单与两个checksum文件，防止修改后误用旧构建回执。

12项Python测试通过，包括同大小归档损坏、归档元数据缺失、许可缺失、yaml源码改动/新增、越界路径、符号链接、重复摘要、control无需Ice和data无需third_party；集成反例确认预检查失败时未查询SDK、未创建输出目录。测试只写临时小文件，退出清理，无原始DAQ数据。四脚本py_compile通过。

Linux完整24程序/438输入、实际用户Windows Qt5.14.2 MinGW六程序/439输入重新构建通过。所有30个C++程序与70.56已验证程序逐字节一致；本轮不重复运行无变化的网络/接收测试，也不声称新增硬件或持续吞吐覆盖。证据为unchanged-programs.json及两个真实构建回执。

新运行候选目录build-packages/getdaq-host-20261016-offline-dependencies-candidate（24程序/152文件）及getdaq-windows-qt5142-20261016-offline-dependencies-candidate（六程序/246文件）。Windows按用户Kit在新目录部署匹配DLL，完成无SDK PATH/offscreen与短暂原生初始窗口检查。文档及依赖元数据更新后逐文件重新核对清单，原70.56包保持不变。

整理REFACTOR_STATUS，区分已接主机主链路、原对象条件恢复、缺失功能及现场验收，不再将早期“ECC未接GUI/Prepare未接系统”等状态当成当前事实。同步修正OPERATIONS的Linux整套输出路径、Windows已验证Kit/六程序说明；硬件、单板准备、依赖手册、third_party README和初始方案的历史定位。1036个本地Markdown文件链接通过，检查不包括锚点或外部网址。本轮不新增必要第三方库。

检查点build-dependency-delivery/checkpoint.json记录当前构建输入、预检查回执、工具/测试/文档、运行包及与70.56程序字节一致性；复核命令python3 tests/check_dependency_delivery.py。70.56完整源码/文档映射因本轮整理保留为历史，不覆盖其原证据。没有产生raw，用户所有测试流20GiB上限和测后删除要求不变。

整套重构仍未完成。当前明确缺口见REFACTOR_STATUS第3节；本轮文档整理或构建通过不能完成单板维护整合、部分/冷恢复、跨主机迁移、真实板端加载、目标平台或持续吞吐验收。

## 70.58 原生选定CoBo重准备及ECC持久操作（2026-10-16，HTTP/GUI待接）

本阶段仅完成原生库和ECC C++接线，不发布新的整套运行包，也不将主机重构或单板维护完整向导标为完成。教程式诊断配置/选定板采样、生产HTTP与Qt确认入口仍待；使用者目前没有新的维护按钮或CLI。操作、原理和限制见[维护说明](COBO_MAINTENANCE.md)。

SystemPreparationClient::ReprepareCoBo仅接受原Prepared对象中准确的活动目标和重新确认的数据停止声明。全节点名称/元数据、生产停止/未Ready与目标完整计划先核对；明确注销目标旧订阅、释放其回调监听器，再按原固定设置执行完整单板Prepare并建立新订阅。其他CoBo/MuTaNT准备对象、订阅和保存对齐结果保留，不重复系统对齐。成功仍为Prepared，后续Configure和Start必须明确执行；失败不重试、不reset订阅服务、不自动回滚。

原自动延迟校准自身包含firmwareEnable脉冲，测试按完整原校准序列核对并要求最终firmwareEnable/intEnable归零，不能删除合法校准以满足“从未置位”的错误断言。最早探索测试因此失败，修正断言后重跑普通最终六项，探索不计入通过数。未以此验证电气相位或真实波形。

NativeEccCoordinator::ReprepareCoBo(operationId,coboId)复用持久状态机、互斥、固定输入及真实数据生命周期接口的停止查询；目标进入请求键及操作意图。命令内部编号11，保留旧1—9及恢复记录10，转换只能Prepared→Prepared。公共普通Execute若未提供准确目标会拒绝；未知/停用目标、错误状态、数据未停、重复ID、UNKNOWN及冷实例保持拒绝。共享纯C++11控制状态编码支持transition=11，12及以上拒绝；旧规范字节不改变。

本机普通和新代码ASan/UBSan/LeakSanitizer各六项单板库场景，共12项：两编码成功选择C0重准备并明确后续Configure；非目标节点名称改变、停用节点仍生产均在目标写入/注销前拒绝；目标旧注销和新订阅的已应用丢应答均保持Failed，不重放。成功场景实际比较另一活动CoBo及停用CoBo的寄存器/写计数、全部MuTaNT值/写计数、链路/TDC/机箱间对齐调用计数及非目标告警对象/订阅计数。测试为三MuTaNT、两活动CoBo及一停用CoBo的合成系统，不据此声称全部现场拓扑通过。

普通/插桩ECC各两编码，共四项。每项从Describe/Prepare后维护C0，明确Configure/Start/Stop/Breakup，再维护C1并完成第二个控制周期和正常Reset。核对无目标/错误目标/数据运行时零新持久操作、重复ID不重放、维护日志编号11与目标绑定、冷重开保留两项完成记录且继续禁止接管。数据生命周期明确为TestData替身，无真实receiver、HTTP数据服务或seal；这些控制周期不是实际DAQ数据链路证明。

状态机转换矩阵/失败保持/并发检查、持久日志锁/崩溃/损坏/存储失败回归、纯C++11编码测试通过。新增维护转换与编号10仍禁止作为普通命令的检查；Native直接冷重开读取11记录并保持已验证。日志CLI旧程序不能读取新维护命令时应升级，不能手改编号绕过。

独立check_cobo_maintenance_evidence.py核对归档控制元数据：六项维护意图/两完成/四保持，原订阅关闭链、原系统完成引用及维护链；ECC准确的原请求键、目标C0/C1、Prepared来源/目标、唯一完成、两轮普通命令序列和末尾冷opened记录。原始/tmp目录通过有界归档复制后可按映射核对，后续无需保留原临时路径。普通根ordinary-evidence；插桩根sanitized-run/preparation-gmrwq0vv与sanitized-run/coordinator-nrq0v0rp，均在build-cobo-maintenance。

插桩准备组27条严格C++11重编译/54对象复用，ECC组1条重编译/80复用；实际依赖、配方、对象摘要与复用来源核对。旧GET/Ice/系统Qt未整体插桩；测试链接配方复用已有网络验证基线，不等于生产硬件核心新增Qt依赖。程序及元数据、源码/编译回执见本阶段检查点。全部测试无receiver或raw，阶段自有raw为零，用户全部流20GiB总上限不变。

检查点build-cobo-maintenance/checkpoint.json，复核命令python3 tests/check_cobo_maintenance_checkpoint.py；其production_http_or_gui_integrated、whole_suite_rebuilt、new_runtime_package、real_receiver_executed、goal_complete均为false。最近完整运行包仍为70.57，不能据旧包宣称本轮维护已经交付给Qt用户。70.57及更早全源码映射保留为历史，不改旧包和摘要。下一步继续生产HTTP/GUI审阅确认及真实receiver组合，随后才进行整套重建与新包验证；物理对齐/采样、冷恢复和其余整套缺口继续保留。

## 70.59 选定CoBo维护的HTTP/实际操作台与接收链路（2026-10-16）

70.58的原生接口已接认证HTTP和实际MainWindow/RunControl。只读`GET /v1/cobo-maintenance-targets`返回固定活动CoBo；`reprepare-cobo`必须携带目标ID。原实例/快照/客户端归属、Prepared前提、单次本地意图、原ID结果观察和原生目标请求键均保持。窗口默认取消，不直接访问板端或raw。操作与原理见[单板维护](COBO_MAINTENANCE.md)。

最终普通证据根为`build-cobo-maintenance-window/ordinary-exit-gated/description-recovery-1zh61n8q`，插桩服务证据根为`build-cobo-maintenance-window/sanitized-exit-gated/description-recovery-xsu2lfx_`。两组各运行编码0/1，共四个最终场景；之前探索及未加显式退出门槛的运行不重复计数。每个场景都通过实际操作台默认Cancel零意图、Yes单次意图、维护期间普通控制按钮禁用、目标C0及原身份绑定、成功保持Prepared。未知/停用C2拒绝且无硬件写入或受理记录，同ID改目标拒绝，相同原请求只返回已有结果；C1、停用C2、C1订阅及系统对齐计数保留。

维护后由网络夹具通过实际ECC HTTP执行两个正常数据周期，包含明确Configure/Start、Pause/Resume/Stop、再次Start、Breakup和Reset；两独立数据服务管理真实原生receiver，合成发送器直接TCP传raw。夹具在删除raw前独立比对输出字节和封存记录。**GUI只发起维护，后续采集命令由网络夹具执行**；不把它说成这次GUI完成了整场Run操作。两编码场景采用两活动CoBo、无MuTaNT的独立模式，实际MuTaNT及电气对齐不在本轮网络验证范围内。

`tests/check_cobo_maintenance_window_evidence.py`独立核对GUI保存的唯一请求、网络请求键与唯一受理/完成、原生编号11的目标请求键、原订阅关闭和维护审计链、后续普通命令及receiver退出/封存记录。原始读取拦截器绑定实际探针与库摘要，每场景只有自有单字节raw读取自检产生的一条拒绝；生产GUI没有额外raw读取。场景显式调用ECC正常停止并检查退出码0，避免析构清理吞掉退出错误；保存`maintenance-ecc-exit.json`，检查器强制读取。

插桩组的ECC服务来自`sanitized-server-final/build-receipt.json`：29条严格C++11 ASan/UBSan编译、56个依赖未变对象复用，启用LeakSanitizer并核对正常退出；最终服务SHA与运行记录一致，日志无对应错误。网络夹具、Qt探针、普通数据服务和receiver为普通构建，旧GET/Ice及系统Qt未整体插桩。不把早期未使用的插桩网络夹具构建当作最终运行证明。

客户端58项、完整QWidget93项通过。当前实际生成的纯C++11协议程序`build-ice-compat/data-supervisor/control-protocol/ecc-protocol-tests`读取四场景140条实际记录并核对规范请求字节/键及结果通过；旧`build-ice-compat/control-protocol/`里遗留的可执行文件不再是当前目标，不能凭目录名选择。Linux整套24程序/441输入、用户实际Windows Qt5.14.2 MinGW六程序/442输入已重建，源与程序摘要核对一致。

本轮候选目录为`build-packages/getdaq-host-20261016-cobo-maintenance-candidate`与`build-packages/getdaq-windows-qt5142-20261016-cobo-maintenance-candidate`，交付清单与Windows匹配DLL启动结果由各包清单记录。构建成功和初始窗口不等于Windows新功能人工验收、独立机器或Ubuntu24/macOS验收。阶段索引为`build-cobo-maintenance-window/checkpoint.json`；旧70.58/70.57检查点及运行包保留历史，不改写旧摘要。

最终两组共16个raw、262144字节已删除；整个阶段自有raw为零。每组512MiB保守预算，并行普通/插桩组总预留1GiB，低于用户全部流合计20GiB上限。这里没有持续高速采集、真实硬件加载、物理对齐修复或冷服务接管证明；整套重构目标仍未完成。

## 70.60 原实例部分交接/计划退休的显式补齐（2026-10-16）

新增原生`RemoteReceiverManager::CompletePendingHandoff`、认证ECC命令`complete-data-handoff`及`getdaq-ecc-handoff complete-submit/complete-inspect`。新请求只增加`failed_operation_id`绑定原父请求，不能重新指定目标；正常ECC命令编号不变。本轮没有增加Qt接续按钮，冷实例及已受理但执行失败的节点仍不支持该入口。完整前提和逐步命令见[接续说明](PARTIAL_HANDOFF_COMPLETION.md)。

原生控制边界由`NativeEccCoordinator::CheckHandoffCompletionBoundary`单独核对：保留的原生日志为已知Idle/Described，不可变输入及主机占用没有变化，没有独立硬件/输入/存储/源或接收故障。它不忽略硬件UNKNOWN，也不清除数据交接本身的永久保持；不读回硬件或宣称确认当前物理状态。

最终验证范围如下，前期探索和边界修正前运行不重复计数：

| 组 | 场景与证据 |
| --- | --- |
| 原生普通四场景 | `ordinary-current/remote-controller-handoff-5af981a3`，两编码各覆盖受理后丢回复及一节点退休、第二节点未受理；真实节点服务与原生receiver |
| 生产HTTP普通两场景 | `network-corrected/description-recovery-16q2wizh`，两编码部分退休补齐、独立新数据服务与新ECC完成新配置采集周期 |
| 插桩ECC服务两场景 | `network-sanitized/description-recovery-nhdv80rt`，相同两编码完整新周期；新控制代码ASan/UBSan/LeakSanitizer |
| 新Linux运行包一场景 | `package-run/description-recovery-kzsbiet3`，编码1完整新周期；四个实际包内程序摘要与清单绑定 |

表中路径均在`build-handoff-completion/`下，共九个最终场景。原生四场景走数据适配器，原NativeEccCoordinator保持Idle，没有伪造普通硬件控制流程。HTTP及包内五场景则由真实ECC进程协调旧周期、部分退休、明确接续、新数据实例、不同工件/接收计划和新ECC的Describe/Prepare/Configure/Start/Stop/Breakup/Reset。底层板卡仍是本机合成Ice服务，独立模式无MuTaNT，没有实机或吞吐证明。

所有已受理节点在接续时仅GET原操作；原生丢回复场景另注入伪404，验证无法把不确定发送当作未受理并再次POST。第二节点因缺审阅计划受理前拒绝时，显式接续仍会失败且不重发已退休节点；恢复原准确计划后，只对明确未受理节点补交同一原请求，两个原节点各唯一受理。恢复不能更换原实例、计划、归属、会话或父审计。旧请求仍failed，新请求独立complete；原ECC及数据控制永久保持。

编码0通过HTTP完成一次仍缺文件的失败接续，再完成正确接续；编码1使用新CLI提交、只读inspect和重复submit目录拒绝。工具同时核对新旧固定配置及原接班身份，在提交/观察期间持原身份共享锁。原ECC请求归属、错误父ID、另一客户端、已完成后另建接续请求、原冷实例接续及冷导出均拒绝。正常关闭/回收原ECC、新ECC和冷观察进程，显式核对退出码0，避免析构清理掩盖退出错误。

独立`tests/check_partial_handoff_completion_evidence.py`核对原节点意图准确字节/摘要、新接续意图引用、原节点唯一受理/完成、请求键及证明摘要、GET-only/补交节点、原失败历史与新完成请求绑定、原接班身份副本，以及新旧实例/计划/会话和新receiver正常退出/封存。它按完整JSON记录读取历史remote审计，兼容内嵌规范状态对象带换行的旧记录，不修改旧字节。包内组另绑定ECC、CLI、数据服务及receiver四个实际包内程序SHA。

插桩ECC服务由29条严格C++11编译及56个依赖未变对象复用生成，构建回执程序SHA与实际运行一致；日志无AddressSanitizer/UndefinedBehaviorSanitizer/LeakSanitizer错误。夹具、CLI、数据服务、receiver、旧GET/Ice和系统Qt未整体插桩。Qt客户端58项兼容回归通过；纯C++11协议程序读取五个HTTP/包内场景189条真实记录并核对规范请求/键及结果通过。

Linux24程序/441输入、用户实际Windows Qt5.14.2 MinGW六操作端/442输入已重建。新候选包为`build-packages/getdaq-host-20261016-partial-handoff-completion-candidate`（154文件）与`getdaq-windows-qt5142-20261016-partial-handoff-completion-candidate`（248文件）；Windows匹配DLL、无SDK PATH/offscreen及短暂原生初始窗口通过，不代表独立机器或新功能人工验收。70.59源码/文档整组映射转为历史，原检查点和旧包保持；本轮索引为`build-handoff-completion/checkpoint.json`，goal_complete保持false。

最终四组42个raw、507904字节已删除；整个阶段自有raw为零。原生组2MiB、每个HTTP/包内组512MiB预算，普通/插桩并行的最大保守预留1GiB，继续遵守用户全部流合计20GiB及测后删除约束。操作台接续入口、冷恢复、连续占用/跨主机控制、真实加载、目标平台和持续大流量验收仍待，板端getHwServer/IceEirfu不重构。

## 70.61 Qt中显式补齐原部分计划退休（2026-10-16）

**四个最终组合场景、客户端63项及窗口95项回归通过。最终源码与工件摘要见 `build-qt-handoff-completion/checkpoint.json`；goal_complete=false。** 整套重构仍未完成，本节不覆盖真机或全部目标平台。

- MUSIC形式的 `PlanTransitionWindow.h/.cpp/.ui` 已增加原部分退休补齐与原接续ID观察；MainWindow→RunControl→NativeRunControl工作线程→EccSystemClient→认证HTTP均已接入。默认Cancel不产生意图或POST，Yes先持久保存新意图再提交一次；新请求只有原失败父ID，不允许更换下一代目标。原父failed及旧控制冻结始终保留。
- 原退休提交前，操作台保存 `plan-retirement-context.json`，把准确字节SHA-256和原请求键写入观察日志，然后保存退休意图，再POST。重开时核对文件身份/内容、日志顺序、原请求、实例、正常封存周期及目录单写者。缺少原前状态的旧失败记录不能事后补造；孤立文件、内容改写和在原意图之后追加context均拒绝。客户端回归覆盖原失败→失败接续→另一显式接续成功，以及操作台本地记录重开后只读导出、继续拒绝新控制。
- 原前状态只用于回执中的原节点/会话/封存证明。查询与窗口验收仍检查当前原实例、已知Idle/Described、无硬件UNKNOWN/错误/活动切换及原控制/数据冻结；旧正常状态不能覆盖当前硬件故障。已加入客户端与窗口反例。
- 最终普通服务使用本轮全套交付构建的实际ECC、原生数据服务和receiver，进行两种编码的实际MainWindow流程；插桩服务组采用70.60已构建的ECC ASan/UBSan/LeakSanitizer程序，须再次核对其tracked_inputs未变。GUI/fixture/数据服务/receiver/旧GET/Ice/系统Qt均不声称整体插桩。独立核对入口为 `tests/check_partial_handoff_window_evidence.py`；普通组可附 `--delivery-receipt build-delivery/full/full/build-receipt.json` 绑定三项实际程序摘要。
- 组合场景先让第二节点的准确原计划文件缺失。实际窗口确认原退休失败，取消补齐不写意图，首次明确补齐仍失败；由夹具恢复同一路径、准确字节和0400权限，再通过窗口另一次明确确认补齐成功。已退休节点只查询原子操作，两个原子操作各受理一次；保存原失败和首次失败接续，导出完整原回执，旧控制按钮继续禁用。随后网络夹具明确更换数据服务和新ECC，执行新计划的新采集周期及正常seal；新周期由网络夹具驱动，不能称完整GUI新采集操作已验收。
- Qt专用raw读取防护只注入窗口探针，先拒绝它自有的一字节graw自检，随后日志必须仅有这一条拒绝。窗口仅传控制元数据，独立原生receiver及数据服务不引入Qt。本轮全套构建的receiver和supervisor与70.60候选包逐字节相同。最终每组传输预留512MiB，两组并发上限1GiB；所有流合计20GiB的用户限制继续有效，runner的finally负责删除测试自有raw，最终检查必须无残留。
- Linux本轮回执位于 `build-delivery/full/full/build-receipt.json`（24程序/441输入）；用户真实Qt5.14.2 MinGW7.3 x86_64回执位于 `build-windows-qt5142/windows-build-receipt.json`（六程序/442输入）。新候选包为 `build-packages/getdaq-host-20261016-qt-partial-completion-candidate`（154文件）与对应 `getdaq-windows-qt5142-20261016-qt-partial-completion-candidate`（248文件）；完整清单及Windows匹配DLL、无SDK PATH/offscreen与短时原生初始窗口检查通过。普通组实际运行交付构建目录中的三项原生程序，生成包后核对与包内摘要相同；不称为从包内路径执行了完整GUI流程。窗口初始启动不代表Windows完整新流程或独立机器验收。
- 最终证据为 `build-qt-handoff-completion/ordinary-accepted/description-recovery-hwzegwnd` 与 `sanitized-accepted/description-recovery-a2jhudaz`（后者同属该阶段根）。最终两组runner删除24个传输raw、327680字节；四个一字节GUI防护自检文件也由探针删除。包括前期验证轮在内，阶段目录raw残留0；过期夹具凭据已删除。70.60及更早检查点/候选包保持原字节，其全源码映射因当前Qt与文档变更已历史化；本轮借用其未变后端程序的摘要须明确绑定，不能把旧包称为包含新Qt入口。冷ECC实例恢复、已受理后执行失败子操作、连续板卡占用移交、配置跨文件持久化事务、跨主机归档和真实大流量/硬件/目标平台验收仍未完成。板端getHwServer/IceEirfu不重构。

## 70.62 持久交接上下文与不读取凭据的离线审阅（2026-10-18）

本轮补齐冷恢复所需的持久证据及 `getdaq-ecc-handoff review-cold`，**不执行实际冷接管，不清除UNKNOWN，不授予新控制权；整套goal_complete仍为false**。详细操作及字段含义见 [COLD_HANDOFF_REVIEW](COLD_HANDOFF_REVIEW.md)。旧版没有后端上下文时拒绝，不从当前状态补造。

- `RemoteReceiverManager`在发送首个交接/计划退休子请求之前保存 `handoff-context.json`，包括全部原实例/计划、正常Stopped的PID/会话/目录/seal、原范围/持有期和准确原意图摘要；首条审计承诺上下文SHA-256。上下文进入既有不可变证据检查，字节/身份变化会保持存储故障，即使恢复字节也不自动解除。HTTP409/412收到后另行同步记录原操作ID/请求键/状态码，其他结果不冒充未受理。
- 新 `RemoteHandoffEvidence`按完整JSON记录解析有界审计；旧规范状态可跨物理行。它核对根快照/范围、请求/代理ID派生、正常周期、文件身份/权限/单链接及目录集合，合并后续接续尝试。任何后续不明或受理证据压过旧拒绝，不因目录排序改变结论；只有当前完整证据才可用于展示。每文件1MiB、总元数据64MiB、8192目录项/记录预算，不扫描事件文件。
- 配置读取分为默认控制构造和命名 `OfflineEvidenceOnly`构造。前者继续校验ECC/节点凭据；后者只核对同一固定输入，通过纯函数计算接收绑定，不建立HTTP客户端或读取令牌。`EccControlJobs`在创建数据客户端和原生控制器之前拒绝离线配置；其输入绑定与默认构造不变。报告与交接回执有不同kind，不可作为恢复凭据。
- 最终普通元数据20类与严格C++11 ASan/UBSan/LeakSanitizer元数据20类通过；测试输入均删除，未生成raw。覆盖旧上下文缺失、截断日志、拒绝无先前发送、500冒充拒绝、后续不明压过旧拒绝、重复发送、未封存/假完成、硬链接及内容/目录漂移保持等。普通组额外注入禁止网络的动态库并核对零调用；插桩组LSan需要正常进程检查权限，使用退出0的最终记录，不以沙箱内LSan无法检查进程的结果作为通过依据。
- 最终动态组合共12场景：原生数据适配器普通/插桩各四（两种编码、交接与计划退休）；完整ECC网络服务普通/插桩各两编码。原生组真实receiver及节点服务，但不是完整硬件流程；网络组实际ECC、节点服务、正常周期、部分失败/原ID接续、旧失败保留、原子请求各受理一次、明确服务更替、新实例新计划周期及seal、冷重开拒绝与正常退出均核对。插桩仅覆盖构建记录中的新控制/审阅代码及相应夹具；网络fixture、CLI、数据服务、receiver和旧GET/Ice/系统库未整体插桩。
- 每个网络场景在原部分失败时审阅一次，在全部完成后再次审阅；最后明确停止全部服务并删除测试ECC及节点认证文件，仍能通过禁止网络的CLI生成离线报告。同时默认配置缺凭据拒绝、离线配置创建控制器拒绝，原输入绑定及原生审计字节不变。报告的当前身份核对、控制许可、UNKNOWN清除、冷恢复执行、raw读取均为false。独立核对入口 `tests/check_cold_handoff_evidence.py`复用原请求/子历史/seal核对并绑定新上下文和报告；不单凭passed标记推断完成。
- 实际全套交付构建中的CLI另在两种编码已关闭、凭据已删除的现场夹具目录上执行成功，重复报告目录拒绝、原生日志SHA不变、网络拦截日志零调用。程序摘要对应随后候选包；这里从构建输出路径运行，不声称从包内路径执行了全部动态场景。Linux24程序/443输入、用户Qt5.14.2 MinGW7.3 x86_64六程序/444输入重建；receiver/supervisor及Linux GetDAQ、Windows六程序与70.61相同字节。GUI功能测试未重跑，不把旧63/95项换算成本轮新增覆盖。
- 插桩构建按每个编译单元核对实际头依赖、编译配方和对象摘要；变更的头使相关对象重编译，无关旧缓存可以复用。最终元数据目标5条严格重编译/77对象复用，远程夹具6/76，服务8/78；`tests/check_handoff_overlay_reuse.py`独立重新预处理缓存对象，核对当前依赖与原记录，不把全局旧头已变化的缓存整组当作当前输入。旧vendor库不声称全部插桩。
- 最终四组runner删除48个传输raw、524288字节，阶段raw残留必须为0。网络组预算512MiB、原生组2MiB，组合并发预留低于1GiB；用户所有流总计20GiB上限及测后删除继续执行。元数据临时测试输入另行清理，不把删除测试raw扩展为产品自动删除原始数据。
- 新候选为 `build-packages/getdaq-host-20261018-cold-handoff-evidence-candidate`（155清单文件）及对应 `getdaq-windows-qt5142-20261018-cold-handoff-evidence-candidate`（249文件）；完整清单及Windows匹配DLL、无SDK PATH/offscreen/短时原生初始窗口检查通过。具体源码/工件摘要及最终证据目录见 `build-cold-handoff-evidence/checkpoint.json`。70.61源码映射因本轮变更历史化，旧包与旧摘要保持。实际冷实例接管、已受理后失败子操作恢复、配置持久化事务/迁移、跨主机归档及控制权、实机/目标平台/持续吞吐仍未验收；板端getHwServer/IceEirfu不重构。

## 70.63 原生冷数据交接补齐核心（2026-10-18）

**原生核心普通四场景、新控制代码ASan/UBSan/LeakSanitizer四场景、原实例补齐回归四场景及ECC日志测试通过。生产CLI、ECC网络父操作和Qt尚未连接这个新核心；整套重构仍未完成。** 此阶段没有新建整套交付包，70.62的候选包保留原字节，不能当作已包含本阶段核心。

- 新增 `RemoteReceiverManager::CompleteColdHandoff`。调用方必须提供整个动作期间的固定输入及接班身份检查；核心从原快照、持久交接上下文和全部后续尝试中恢复准确原数据范围/持有期/节点请求。原管理对象须已销毁，原生ECC日志使用新增 `ExclusiveReadOnly` 模式取得独占锁，既不新建日志，也不追加opened。历史必须Idle或Described且没有未完成命令、UNKNOWN或错误；原生重启核对要求保留，不恢复原硬件控制权。
- 取锁前绑定原日志的字节和文件身份，取得锁并解析后再次核对；整个动作持续检查原快照、实验工件、上下文、原意图、历史尝试和本次新审计。实验工件发生变化、缺少调用方检查或活ECC仍持锁，均在创建新尝试/发送节点请求前拒绝。ECC日志测试核对独占只读阻止其他写者/读者、不能Begin/Reconciled、不会改写日志，以及共享观察者存在时不能取得独占只读锁。
- 四个冷场景包含两种编码的部分计划退休和丢回复交接。原管理器及原NativeEccCoordinator实际析构后，由新建临时管理器只从文件恢复原请求；已受理节点只查询原ID，明确拒绝节点才补交准确原请求。计划仍缺失时保存独立失败尝试，恢复同一路径/准确字节/0400权限后另行明确补齐。丢回复后模拟404仍拒绝任何新POST；恢复正常查询后只GET取得原完成证明。
- 本轮同时修正原实例及冷补齐的全节点预检查顺序：已受理节点的原结果和当前正常封存周期均须在任何补交之前核对。退休场景对已受理节点的当前状态响应注入另一合法长度的会话/目录（仅测试代理修改响应，不改服务或raw）；两种编码分别覆盖缺失节点在前/在后的排序。全部检查失败时本次审计没有node_explicit_submit，真实服务的缺失节点仍未受理。撤回响应注入后才允许下一次明确尝试完成。
- 原节点各受理一次、准确原请求/请求键/证明、原失败历史、原范围/持有期/下一代目标、正常seal及原raw字节均核对。原生日志保持原字节，原上下文和原remote日志也不修改。这里是实际receiver与两个节点服务的**直接数据适配器周期**，原生控制日志只有Idle头记录；没有完整ECC硬件状态转换、网络父操作或生产CLI的冷恢复证明。`tests/check_cold_data_completion_evidence.py`独立复核最终八个冷场景；既有完成证据核对器复核四个原实例回归。
- 最终插桩程序使用严格C++11、pedantic-errors、ASan/UBSan构建，LSan开启并正常退出。当前配方重编译两单元、复用80对象；`tests/check_handoff_overlay_reuse.py`核对对象摘要及实际头依赖。相应新原生代码及夹具进入此插桩范围；旧GET/Ice、节点服务、receiver及系统库未整体插桩。没有新的Qt功能测试或目标平台验收，不沿用旧GUI通过数充当本轮新覆盖。
- 最终普通冷组为 `build-cold-handoff-completion/ordinary-final/remote-controller-handoff-w8k4aina`，插桩冷组为同阶段根下 `sanitized-final/remote-controller-handoff-93xoe4g4`，原实例回归为 `live-regression-final/remote-controller-handoff-_25_zqwb`。最终三组runner删除28个传输raw共229376字节；包括中间验证轮的阶段raw残留0，过期测试凭据已清理。每组预留2MiB，实际两组并行预留4MiB；仍遵守用户所有流合计20GiB及测后删除约束。新源码/工件索引为 `build-cold-handoff-completion/checkpoint.json`，明确goal_complete=false、whole_suite_rebuilt=false、new_runtime_package=false。
- 原理和调用方责任见 [离线审阅及冷数据核心](COLD_HANDOFF_REVIEW.md#7-开发中的冷数据补齐核心)。后续仍需生产CLI、原网络父意图及接班文件租约、明确确认/回执导出、新实例完整周期；已受理后失败子操作、任意硬件冷恢复、配置跨文件事务、跨主机控制/归档、真实硬件/持续吞吐及Ubuntu24/macOS/独立Windows验收仍未完成。板端getHwServer/IceEirfu不重构。
## 70.64 独立冷数据补齐CLI、原父记录核对和接班新周期（2026-10-18）

**最终12个网络组合场景通过：10个冷CLI场景、2个原实例HTTP/CLI补齐回归。** 冷场景包括计划退休和同计划交接，两种编码均覆盖。真正执行的是独立工具进程，原ECC已经正常退出；旧硬件对象没有被接管，旧父失败保留。Qt及远程恢复任务入口、任意硬件冷恢复和已受理后失败任务恢复仍未完成，goal_complete=false。

- `getdaq-ecc-handoff`新增`cold-complete-submit`和`cold-complete-inspect`，命令名后六个参数。CLI核对原固定配置、network策略、原已受理父请求、交接前状态、原后端上下文及准确接班身份；动作期间持接班文件共享租约，逐次重验所有绑定。原生核心独占只读原控制日志并核对正常释放历史，不启动旧NativeEccCoordinator，不写原native/network日志。原父请求仍failed，已有HTTP `complete-data-handoff`仍拒绝冷实例。
- submit先保存新`cold-request.json`、原请求与原状态副本，再调用原生核心一次；部分失败另记completion-held，不自动POST重试。inspect只读准确本次完成审计、节点证明和末条回执SHA，重建同字节回执，不POST、不访问网络。当前两命令仍校验配置引用的认证文件，不能称无需令牌的审阅；后者是独立的review-cold。
- 计划退休场景先正常完成原ECC两个周期及释放，再让第二节点的准确原计划文件缺失。原父failed、第一节点只受理一次；活原ECC时冷命令在日志锁边界拒绝且没有创建核心动作。正常退出原ECC后，仍缺计划的一次明确冷尝试失败，inspect确认没有持久完成；恢复原路径、准确字节和0400权限后，另一次明确尝试完成。旧节点只GET、第二节点补交原请求，两个节点各受理一次。
- 同计划场景由测试代理在第一个节点实际受理handoff-control后丢应答，原父failed。原ECC退出后，代理暂时让原操作GET返回404：冷尝试保持不明，审计中无新POST标记；恢复真实查询后另一次明确尝试只GET两个原ID并完成。数据服务实例与计划保持，接班ECC随后建立新的接收周期、PID/会话/输出目录和正常seal；没有靠替换节点实例消除原不明。
- 每个冷场景核对重复submit目录拒绝、不同冷操作ID不能复用原外部证据、只读inspect及外部回执删除后的同字节重新导出。删除的只是夹具自有回执，用于模拟工具完成后外部导出丢失，不是产品删除原证据。inspect额外注入禁止connect/bind/listen/sendto/sendmsg的动态库并核对零调用；ASan程序以匹配libasan先加载，不能通过错误预加载跳过检查。
- 原network/native审计在冷工具期间准确字节不变。接班计划按原回执消费，新服务/新ECC执行明确的新Describe、Prepare、Configure、Start、Stop、Breakup、Reset及正常封存；同计划沿用数据节点实例，退休计划明确更换节点实例。随后夹具明确重开原ECC一次，原控制继续拒绝、原失败不改、原活实例补齐拒绝。独立检查允许这一次预期opened追加，不把它记为冷工具修改日志；所有ECC进程正常退出码0。
- `tests/check_cold_handoff_cli_evidence.py`独立核对原父规范请求/摘要、后端正常上下文、原子请求及各一次受理/完成、完成回执SHA、接班激活、新旧实例/计划/会话及seal。普通混合组的两项原实例回归由既有完成检查器显式选择原场景，不改写运行记录来匹配检查器；默认旧检查范围不变。
- 最终普通混合组为`build-cold-handoff-cli/ordinary-verified/description-recovery-e168f6yi`（两冷退休、两原实例）；插桩CLI退休组为同阶段根下`sanitized-verified/description-recovery-n8i_6ii8`，插桩CLI同计划组为`sanitized-same-plan/description-recovery-r61k20as`，两组各两编码。实际全套交付构建组为`delivery-verified/description-recovery-eee1og0h`，四个冷场景使用本轮构建目录内的ECC、CLI、receiver和supervisor；摘要与本轮build-receipt逐一核对，不能说从包内路径执行了这些场景。
- 插桩的是新CLI、其原生冷补齐/日志代码及构建回执覆盖的其他新控制代码。网络fixture、普通ECC服务、节点服务、receiver、旧GET/Ice及系统库未整体插桩；普通CLI和插桩CLI使用独立程序路径，其余普通服务可复用同一已冻结程序。所有预期失败的工具日志也扫描ASan/UBSan/LSan标记，不能把插桩崩溃当作正确拒绝。当前最终CLI构建一单元严格C++11/pedantic-errors/ASan/UBSan重编译、82对象复用；缓存沿受SHA约束的祖先回执保留配方，按当前头依赖和对象摘要核对。独立检查重跑28个继承缓存单元的依赖图，旧vendor未宣称整体插桩。
- Linux24程序/445输入和用户实际Qt5.14.2 MinGW六程序/446输入完成重建。新候选为`build-packages/getdaq-host-20261018-cold-handoff-cli-candidate`（156清单文件）及`getdaq-windows-qt5142-20261018-cold-handoff-cli-candidate`（250文件）；完整清单、Windows匹配DLL、清除SDK PATH后的help/offscreen及短时原生初始窗口检查通过，不代表独立Windows机器完整功能验收。新增源码与工件索引为`build-cold-handoff-cli/checkpoint.json`。70.63及之前的源码映射因后续变更历史化，旧包/检查点保持原字节。
- 最终四组runner删除72个传输raw、983040字节；阶段全部自有raw残留0，过期夹具凭据由runner清理。每组传输预留512MiB，最多两组并行保守预留1GiB；用户所有流合计20GiB上限及测后删除继续执行。没有新的GUI功能回归、物理板端/持续吞吐或Ubuntu24/macOS/独立Windows验收。操作步骤、参数/错误表和原理时序见[冷数据补齐手册](COLD_DATA_HANDOFF_COMPLETION.md)。


## 70.65 独立冷交接恢复服务、Qt窗口和完整回执元数据（2026-10-18）

- 新增`getdaq-handoff-service`及MUSIC式`ColdHandoffWindow.h/.cpp/.ui`。原ECC退出后，Windows/Ubuntu操作台可连接独立恢复端口。固定服务配置绑定原/新配置、准确接班计划、原父请求/正常前状态、CLI路径及摘要和私有任务根；客户端不能提交可执行文件、服务器路径或任意硬件命令。原生CLI仍负责数据补齐，Qt只传意图/状态/回执。
- 服务先持久保存准确request及accepted/running，再直接启动CLI。全部HTTP要求独立Bearer认证；错误路由、重复JSON字段、任意command/path字段和旧实例新请求均有拒绝检查。同ID准确重复只返回原记录；不同意图同ID拒绝。正常关闭等待当前子任务，关窗只停止本地请求。
- 窗口身份审阅后确认；Cancel不创建ID，Yes先保存本地意图再POST一次。真实窗口进程覆盖准确原ID查询、同窗口载入原意图后的只GET观察、回执同字节导出、错误服务身份拒绝及收回旧回执。每个最终场景各执行一次submit窗口和一次inspect窗口，不把这记成独立Windows机器完整操作验收。
- 任务GET只读保存的状态；明确inspect另存新任务并调用原`cold-complete-inspect`。GET元数据HTTP仍存在，不能称整个窗口“零网络”。CLI本身与70.64封存版本字节相同，其只读检查不POST/查询节点的边界保留；本轮没有再次注入禁止网络库，70.64的独立CLI拦截证据仍是此前验证。
- 重开未完成任务派生为unknown，不自动重放、不收养/终止PID；原submit未知期间拒绝新submit，明确inspect可证明准确原attempt已经完成，原unknown不被改写。崩溃夹具使用有摘要绑定的受控包装程序：先保持一个真实存活的子进程，SIGKILL恢复服务并重开，核对unknown/不重发/新submit保持；释放闸门后同一子进程exec准确原生CLI并正常完成，随后明确inspect产生独立回执。该测试不冒充已经覆盖任意CLI中途断电点或原硬件冷接管。
- 普通四场景位于`build-cold-handoff-service/one-mib-network/description-recovery-s6wfqv1s`：两种编码下的计划退休、同计划交接。插桩服务两场景位于`build-cold-handoff-service/one-mib-sanitized-network/description-recovery-5t91n_d5`：退休0、同计划1。使用真实独立ECC/恢复服务/节点/receiver和生产窗口进程，板端为本机模拟。核对活原ECC的日志锁拒绝、缺计划明确拒绝、丢回复后404拒绝重发、原请求补齐、准确重复请求观察、回执丢失重导出、服务正常重开、受控崩溃及下一代ECC完整新周期和正常seal。
- `tests/check_cold_recovery_service_evidence.py`独立核对服务policy/原意图/请求键、原节点各一次受理/完成、冷回执摘要、父failed保持、原native/network字节前缀和仅一次后续明确opened、新实例/计划/会话及封存。崩溃恢复只GET原子请求，原未知任务的磁盘running记录保留，由重开派生unknown；新检查任务独立complete。两组独立核对通过，负例日志同样检查插桩错误。
- 插桩范围为五个翻译单元：恢复服务、入口、生成moc、HTTP服务器、冷交接协议，严格C++11/pedantic-errors/ASan/UBSan，正常退出进程启用LSan。主动SIGKILL的那一次不声称跑完泄漏检查。原生CLI、原ECC/节点服务/receiver、Qt/旧GET/Ice及其他基线库未在本轮整体插桩，构建配方/依赖/基线归档和对象摘要在`sanitized-one-mib/build-receipt.json`。
- 回执支持原CLI的完整1MiB上限，字符串转义后的窗口HTTP响应独立限8MiB；其他控制请求默认1MiB不变。协议检查使用约700KiB含大量转义的回执，核对字节/摘要完整保存、未知状态不能携带成功回执、越界/任意命令或路径拒绝。HTTP探针只用内存传2MiB元数据，核对新预算成功、默认预算拒绝且仅两次GET。初次联调捕获的默认4KiB编码上限已修复；较早失败/被新上限取代的组不计最终通过数。
- 公共HttpClient变更后，本轮重新构建并运行既有Qt回归，95项通过、0失败/跳过，见`regression/result.txt`。这不沿用未重跑的其他历史63项计数；两项新协议/HTTP检查及六个动态组合另列。
- 完整构建为Linux25程序/453输入、用户实际Qt5.14.2 MinGW六程序/454输入。新候选目录含`cold-handoff-window-candidate`，Linux159清单文件、Windows252文件，完整摘要、匹配DLL、去SDK PATH的help/offscreen及短时Windows原生初始窗口检查通过。接收器、supervisor、原ECC服务及交接CLI与70.64封存包字节相同；默认数据热路径未加入Qt。六场景实际运行已记录的开发基线、独立插桩服务及生产窗口探针；普通新恢复服务与本轮完整交付构建摘要一致，不能把其他开发基线也称为候选包程序。使用完整交付构建的全部ECC/CLI/supervisor/receiver/恢复服务组合的一项额外补验，因自动审批审查额度不可用而未执行，仍列待补验。
- 最终两组删除36个传输raw、491520字节；阶段全部自有raw残留0，过期测试凭据已清理。每组预留512MiB、最多两组保守预留1GiB，用户所有测试流合计20GiB及测后删除约束保持。既有Qt小型模拟文件由自有临时目录清理，HTTP预算探针没有写载荷文件。
- 新封存索引为`build-cold-handoff-service/checkpoint.json`，记录当前源码、实际程序、包和最终工件摘要，整套goal_complete仍为false。原70.64及更早包、摘要与归档程序不改；其源码映射因本轮变化历史化。操作及部署步骤见[详细窗口手册](COLD_DATA_HANDOFF_WINDOW.md)。已受理后执行失败子任务、任意硬件冷恢复、配置迁移/跨文件持久化事务、跨主机归档/连续控制权、现场真实加载、Ubuntu24/macOS/独立Windows及持续吞吐仍待。


## 70.66 冷恢复完整交付构建组合补验（2026-10-18）

- 自动审批恢复后，在新目录`build-cold-handoff-delivery/network/description-recovery-2biddvxb`执行编码1计划退休完整组合。全部五个产品角色使用实际完整交付构建，程序摘要逐一对应70.65 Linux候选清单；测试程序仍为保留fixture和生产窗口探针，不称包内路径执行。
- 独立检查器核对普通窗口提交、原父failed、原节点各一次受理、回执重导出、服务重开/受控崩溃、不自动重发、下一代新周期和正常seal，全部通过。删除6个测试raw共81920字节，阶段残留0，测试凭据清理。
- 产品代码、70.65候选包及原检查点保持原字节；新增索引`build-cold-handoff-delivery/checkpoint.json`绑定原70.65索引和本次证据。70.65当时记录的额外组合未执行仍是历史事实，本阶段补齐；没有新增平台、真机或吞吐验收，也不标整套完成。详见[补验说明](COLD_HANDOFF_DELIVERY_VALIDATION.md)。


## 70.67 配置来源共享读取、独占保存与中断加载保护（2026-10-18）

普通`XcfgDocument::Load`现在检查物理根及include的未完成Save All标记，成功前重新核对来源。新增`XcfgIoTransaction`覆盖配置加载、来源检查、工作区候选/暂存、单文件保存/结构/历史、Save All、明确恢复和导出/副本。共享读者各持独立保留，写者持登记门并拒绝活动读者；同线程嵌套复用、读转写拒绝。登记门最多等250ms，读者最多256，失活判定不采用固定年龄强抢。POSIX私有目录以0700原子创建，权限/符号链接错误拒绝。它只协调同用户、同操作环境和同缓存域的新版工具，不协调外部编辑器、旧程序、跨主机或Windows与WSL。原生DAQ代码及依赖未改。操作和原理见[来源并发手册](CONFIGURATION_SOURCE_TRANSACTIONS.md)。

最终核心普通与严格C++11 ASan/UBSan/LeakSanitizer各135项通过；完整QWidget95项通过，包含后台快照运行时打开另一配置的原有测试。早期全部独占方案导致此处两项失败，最终共享读设计通过原测试，未放宽测试期望。沙箱内完整界面运行因本机监听和测试缓存权限失败，该日志保留；最终95项取自已授权沙箱外正常运行，不混用两个结果。实际Windows Qt5.14.2 MinGW核心134通过、0失败、1跳过，跳过项为Unix目录权限夹具`workspaceSaveAllPreparesEveryFileBeforeReplacingAny`，不能声称Windows ACL同样通过。

最终日志位于`build-config-transactions/core/final-result.txt`、`sanitized-core/final-result.txt`、`widgets/final-unrestricted-result.txt`及`windows-core/final-result.txt`。测试程序构建回执为`linux-build-receipt.json`（五目标、461输入）及`windows-core-build-receipt.json`（两目标、459输入）；最终源码与程序摘要绑定，不能用此前`rw-result.txt`代替最终两项小修正后的回归。插桩启用detect_leaks和halt_on_error；配置测试及探针构建启用严格C++11，系统Qt并未整体插桩。

三个最终独立进程组均使用真实配置核心：普通`rw-process-final/transactions-w48tycl4`、插桩`rw-process-sanitized-final/transactions-y_deowkf`及实际Windows`rw-process-windows-final/transactions-eaaphv0o`，均在该阶段根下。每组覆盖：Save All第一份同步后暂停，独立观察A=2/B=1而普通加载拒绝；释放后读到2/2；预加载写者受活动读者阻止且来源不变，同时另一个读者可读1/1；写者在第一份同步后立即退出，普通加载因原未完成记录拒绝，明确完成准确原候选后读2/2；读者立即退出留下保留后，下一个真实Save All可清理失活保留并完成。Linux另核对目录0755及符号链接拒绝。实际Windows使用NTFS自有临时XML，结束删除原目录并保留小型副本；副本保留的原路径为历史证据，不是原路径仍存在或跨机器可直接重放的承诺。立即退出子进程没有正常LeakSanitizer退出检查，不计作该子进程无泄漏证明。

完整主机构建Linux25程序/454输入、实际Windows六程序/455输入已完成；配置改造没有新增第三方依赖。receiver、supervisor、原ECC服务及交接CLI与70.65候选包程序字节相同。新候选目录使用`getdaq-host-20261018-config-transactions-candidate`及`getdaq-windows-qt5142-20261018-config-transactions-candidate`；交付检查、程序与文档绑定以本阶段回执和清单为准，旧候选包不改写。本轮没有新增真机、Ubuntu24、macOS、独立Windows机器、跨主机控制或持续吞吐验收。文件重命名/路径迁移、任意外部读者可见的跨文件持久原子事务、已受理后失败子操作及任意硬件冷恢复仍未完成，不标记整套目标完成。


70.67补充交付核验：Linux包161清单文件、Windows包254文件，匹配Kit DLL、去SDK PATH/help/offscreen及短时Windows原生初始窗口通过；Linux包另执行六个help和offscreen初始截图。`tests/run_config_transaction_delivery.py`从两个新包实际执行`getdaq-config snapshot`：正常来源生成快照并由同包无Qt的`getdaq-config-check snapshot`校验；真实Save All第一份同步后A=2/B=1时包内CLI拒绝且不创建快照；保存者退出后的原未完成记录同样拒绝，准确原候选明确恢复后重新生成并通过校验。最终两组为`delivery-linux/delivery-me3z_5wv`及`delivery-windows/delivery-2c9t1hrs`，均在`build-config-transactions`下。这是包内配置CLI流程，不是包内完整ECC/DAQ采集或Windows全界面验收；本轮完整ECC/DAQ组合仍引用70.66历史证据。Windows早次运行仅因WSL执行位未设置而未启动CLI，测试自有NTFS临时目录已在finally删除；随后只修改新包两个PE文件的Linux执行位，内容摘要不变，最终场景通过。

本阶段配置探针及包内配置场景不产生DAQ载荷；完整界面使用的自有模拟临时文件随测试退出清理，阶段目录最终扫描无`.graw`。仍执行所有传输流合计20GiB及结束删除的用户约束。新源码使70.66及更早的源码映射成为历史，封存的旧包与工件摘要保持原样。最新检查点为`build-config-transactions/checkpoint.json`，整套goal_complete=false。


## 70.68 配置来源文件路径迁移与新旧路径明确恢复（2026-10-18）

`XcfgSourceMigration`在已保存的多根工作区核对源到新路径的映射，改写已知入站include及移动来源内部的出站include。根文件及依赖可以同时迁移，全部根的有效对象/值/类型/单位/范围必须相同；保留未知属性、评论、CDATA与未修改排版。目标必须不存在、父目录已存在，不提供路径互换、自动覆盖或未打开工程扫描。GUI以MUSIC形式h/cpp/ui提供逐源原始/候选文本与完整根报告、默认Cancel、明确Yes及只读禁用；成功重开新根/来源并清旧历史。CLI增加preview-path-migration、migrate-path、preview-path-recovery、recover-path及帮助。原理及操作见[路径迁移手册](CONFIGURATION_PATH_MIGRATION.md)。

迁移的`move-UUID`独立记录准确原始/候选字节、摘要、根映射和路径存在状态。先持久标记全部相关路径，准备全部新增/替换内容，再发布新文件、替换引用，最后删除旧来源；逐次同步与核对后才保存完成及清标记。恢复窗口按记录种类分派，明确完成候选或回到原始集合，可创建或删除原记录中的路径；第三种内容、改变副本/预览、活写者或已完成重放均拒绝。普通Load在原路径消失时也核对原位置标记，封闭虚拟来源图只供迁移候选及恢复校验，不向普通加载开放。原`save-UUID`格式不改变，不能互换两类记录。

最终核心普通及严格C++11 ASan/UBSan/LeakSanitizer各146项通过；实际Windows Qt5.14.2 MinGW核心145通过/0失败/1 Unix目录权限夹具跳过。完整QWidget98项通过，最终日志`build-config-migration/widgets/confirmation-final-result.txt`，包括确认框期间改变字段/方向并重新预览后仍不得执行旧确认的检查。其他最终日志为core/sanitized-core/windows-core下final-result.txt。后续仅界面确认守卫和CLI帮助变化，普通/插桩核心及三个探针程序摘要与其已通过运行记录一致，不把没重跑的项当新增覆盖。系统Qt未整体插桩。

普通、插桩及实际Windows独立进程最终根分别为`process-ordinary-final/migration-ayumkti9`、`process-sanitized-final/migration-ihu5lhz9`和`process-windows-final/migration-q_48fiqk`，均在`build-config-migration`下。每组12个真正exit79场景：根或共享依赖迁移，在created/replaced/removed同步阶段退出，再分别完成candidate或恢复original；另有两个活写者保持场景。普通加载在活写者存在时busy，在退出后未完成记录存在时拒绝；独立Python逐字节核对文件内容及存在状态，重新读取两个根并拒绝完成重放。Windows使用自有NTFS临时来源，结束删除原目录，只保留小型副本；副本原路径是历史证据，不是跨机器直接重放包。立即退出者不具备正常LeakSanitizer退出检查，插桩组正常恢复/读取子进程及负例输出检查相应错误。

Windows初次发布失败未计通过。独立Qt/MoveFileEx诊断明确close_only_success=0/error32，临时文件对象销毁后success=1/error0；据此在发布前销毁Qt临时文件句柄，保留准确自有清理路径。Linux新路径通过同目录已同步临时文件的link拒绝替换；Windows使用不带REPLACE_EXISTING的MoveFileExW。另将中断单测收紧为必须到达准确阶段并得到自有中断错误，避免提前失败被当恢复成功。此诊断只验证该句柄条件，不证明任意Windows故障、NTFS ACL或真实断电。

完整构建Linux25程序/459输入、Windows六程序/460输入，生产配置层仅使用原有Qt Core、C++11和系统文件接口，没有新增第三方依赖，也不进入DAQ热路径。本轮没有新增真机、Ubuntu24/macOS、独立Windows机器、任意外部读者跨文件原子性或持续吞吐验收；其他未完成项继续见REFACTOR_STATUS。


70.68补充交付核验：新候选为`build-packages/getdaq-host-20261018-path-migration-candidate`（162清单文件）及`getdaq-windows-qt5142-20261018-path-migration-candidate`（255文件）；完整摘要、匹配DLL、无SDK PATH/help/offscreen及短时Windows原生初始窗口通过。Linux包另做七个help（核对新命令出现在配置CLI帮助中）及初始截图。receiver、supervisor、原ECC服务和交接CLI与70.67包字节相同。全部测试来源及程序以本阶段回执绑定；Windows核心145项及三个探针与先前已通过最终运行的程序摘要一致，最后仅界面守卫及CLI帮助更新没有改动这些程序字节，不虚报再次运行核心。

两个包内实际运行`tests/run_source_migration_delivery.py`的最终组为`build-config-migration/delivery-linux/delivery-n0eh3f2n`及`delivery-windows/delivery-vo14sopf`。预览后逐字节确认原来源不变，实际迁移共享依赖并正确更新根，随后包内快照由同包无Qt检查器核验；再由已绑定探针分别制造根文件已删除的真实exit79中断，以包内CLI预览/执行candidate和original双向恢复，核对完整字节/存在状态、两个根的有效快照及完成重放拒绝。它是包内配置CLI流程，不是包内完整ECC/DAQ、Windows新窗口人工操作、真机或持续吞吐验收。

本阶段Linux/Windows配置进程组及包内CLI不创建DAQ载荷；完整界面使用的小型自有模拟临时数据随退出清理，阶段目录最终无`.graw`。Windows测试自有NTFS源目录在finally删除，保留的小型证据副本不冒充原路径仍存在。用户全部传输流合计20GiB及结束删除约束保持。检查点`build-config-migration/checkpoint.json`保留goal_complete=false，验证入口`python3 tests/check_source_migration_checkpoint.py`；70.67源码映射成为历史，原655项工件不改写。

## 70.69 Ubuntu24用户空间全套构建与运行验证（2026-10-19）

官方Ubuntu Base 24.04.5经固定签名指纹及SHA256核对，项目专用rootfs安装签名仓库工具链。实际uid1000、GCC13.3、Qt5.15.13/glibc2.39执行，源码只读；内核仍为宿主WSL2的5.15，不能算独立Ubuntu24机器、图形桌面、实机或持续吞吐验收。使用方法和失败原因见[Ubuntu24说明](UBUNTU24_VALIDATION.md)。

完整25程序构建成功；基础尝试`build-ubuntu24-validation/rootfs/build/getdaq-platform/platform-dd5c0911f0eb4d2b9cf92d9cb7d50822.json`状态complete，绑定851项产品/测试/工具输入。核心146、QWidget98、严格C++11原生CTest18项通过；迁移探针12个真实exit79和两个活写者场景、共享读/独占写与中断恢复组、原生receiver十二个传输场景通过。接收传输合计49739线字节，自有raw已删。CTest明确在构建目录运行并检查18项，不能凭零测试的退出码0计通过。

第一轮核心夹具被新版Qt的4KiB XML名称限制提前拒绝；改为40个有界长名称，仍要求Load成功、项目聚合路径保护拒绝修改、准确源字节不变。该反例在Qt5.12.8及本轮Qt5.15.13各146项核心回归中通过，实际Windows Qt5.14.2为145通过/1 Unix权限夹具跳过。未更改生产配置逻辑或放宽Qt限制。第二轮四项界面失败来自专用chroot无可见根挂载，statvfs成功而QStorageInfo无效；私有命名空间中自绑定rootfs后诊断恢复，界面98项通过，Sentry磁盘保护不改。两轮失败工件及修复前后诊断保留独立目录。

额外四个网络场景normal-0、normal-1、start-ack-loss、truncated运行同一完整构建的实际ECC、两个数据服务和原生receiver，全部返回0；合成板端、原生数据直传，不是完整GUI采集。启用网络测试目标前后25个产品程序摘要未变。该组删除14个raw共245760字节，阶段残留0，预算512MiB；全阶段继续执行所有测试传输流合计不超过20GiB的限制。本轮未运行新的ASan/UBSan或系统Qt整体插桩。

Ubuntu24候选为`build-packages/getdaq-ubuntu24-20261019-userspace-candidate`，25程序、163项交付文件；原70.68Linux/Windows包及1133项历史工件保持原字节并复核。包内清单/运行库/help/初始窗口和配置CLI检查以`package-validation/result.json`为准；整套项目仍未完成，后续缺口继续见[状态表](REFACTOR_STATUS.md)。70.69封存入口为`build-ubuntu24-validation/checkpoint.py`及同目录checkpoint.json，保留goal_complete=false；后续源码变化时不能把旧来源映射当当前通过。

## 70.70 在线导出socket与写盘发布锁隔离（2026-10-19）

原Exporter在同一互斥锁内处理连接和send，写盘线程Publish因此可能等待导出网络操作。本轮把socket调用移至锁外，只在取帧共享所有权、入队和更新计数时持锁；断开请求由Publish标记，socket及连接容器的实际关闭由工作线程完成。保留八客户端、4096消息、每客户端字节预算、部分消息完整分配计费和小端uint64帧头，没有新增依赖。详细操作与边界见[导出锁隔离](EXPORTER_LOCK_ISOLATION.md)。

## 70.71 Windows完整界面/客户端与GET头兼容（2026-10-19）

使用用户实际Qt5.14.2/MinGW7.3 x64完整构建并运行QtTest。正常Windows插件、产品相同Fusion风格下，TestGetDAQ为97通过/0失败/1原有POSIX接收锁跳过；TestEccSystemClient为63通过/0失败/0跳过。Qt运行库由匹配Kit部署，PATH缩减，测试在独立原生Windows临时目录执行和清理。不是Windows独立干净机器/真实远端服务或硬件验收。手册见[Windows功能检查](WINDOWS_FUNCTIONAL_VALIDATION.md)。

编译首先发现测试lambda默认参数被MinGW的C++11检查拒绝，改为显式传参；Qt5.14可变参数QSKIP宏的空实参诊断改用公开qSkip函数，原有一个跳过范围和全部断言不变。实际配方为-std=gnu++11 -pedantic-errors。两次失败配方/源码/日志保留，未降低编译检查取得通过。

旧Kit离屏插件缺字体，指定系统字体后仍在自动确认框处崩溃；两次失败及清理回执保留。相同程序在正常Windows插件/Fusion下完成全套，不修改SDK，不声称修复offscreen插件。GET请求只在非空type的POST分支设置Content-Type，修复后原界面9条对应告警为0；认证、请求顺序、单次上传/未知结果语义不变，63/97全套重新运行通过。

宿主Qt5.12客户端63项通过。Ubuntu24新的六控制程序、客户端63项和界面98项完整运行依据为build-ubuntu24-validation/rootfs/build/getdaq-windows-functional/result.json，status必须complete，raw_remaining必须0；源和程序摘要单独绑定，不用Windows结果代替Linux运行。该轮未执行新的ASan/UBSan或实机/持续DAQ吞吐。

新Windows六操作端候选为build-packages/getdaq-windows-qt5142-20261019-functional-candidate，匹配DLL/无SDK PATH/六help及原生初始窗口按本轮部署回执核对；生产包不需要Qt Test DLL。新Ubuntu24完整候选为build-packages/getdaq-ubuntu24-20261019-control-http-candidate，六控制程序重编译，其余19程序保留70.70原字节，25程序来源在新full-build-receipt明确区分。原70.70的705项工件和旧包不改，源码映射因本轮HTTP/文档变化历史化。

封存为build-windows-functional/checkpoint.json及同目录checkpoint.py，goal_complete=false。Windows模拟数据有512MiB保护和独立目录清理，Linux自有模拟数据亦清理；所有流继续合计不超过20GiB，Qt不搬运原始DAQ。实际硬件、独立平台和状态表中的功能缺口仍保留。

## 70.72 已受理后失败的节点计划退休补齐（2026-10-19，在研）

新节点命令complete-plan-retirement规范化回准确原failed退休请求，同一活实例/同一正常封存周期补齐原目标元数据。原failed、原请求和永久控制冻结保留；不同既有字节、独立故障和UNKNOWN不覆盖、不绕过。新增认证只读端点/v1/plan-retirement-completion，父RemoteReceiverManager核对已完成补齐及原证明，不自动提交节点修复。操作说明见[失败节点计划退休补齐](FAILED_PLAN_RETIREMENT_COMPLETION.md)。

宿主Ubuntu20.04/GCC9普通及ASan/UBSan/LeakSanitizer节点各七个实际服务/receiver场景通过。三种元数据正向场景还明确启动新计划实例、完成新周期并核对seal；其他场景核对异字节保留、独立seal故障、冷实例拒绝及完成后重开仍冻结。证据为build-failed-retirement/parent-integration下node-ordinary及node-sanitized；各result.json绑定执行程序和runner摘要，raw_remaining=0。

同一宿主父原生库普通/插桩各两个Ice编码场景通过，使用两个实际HTTP节点及独立receiver、合成板端servant。先真实受理后失败，恢复输入后父仍拒绝且零修复POST，再由单独节点操作明确补齐，父只读取得完整回执。原节点failed与父原意图/审计字节保留、各原退休只受理一次、节点修复只受理一次、硬件写入增加0、旧控制仍冻结。独立RemoteHandoffEvidence可重读新增补齐观察。父库证据在parent-ordinary及parent-sanitized；不把该证据称为实际新链Qt窗口或完整ECC HTTP父请求验收。

普通旧控制十九场景、八HTTP故障及codec/凭据、旧计划切换十二场景通过，插桩旧计划切换十二场景通过。旧控制夹具的raw由终态包装器按准确自有目录删除，已有清理回执保留；包装器仅检查自己的三个输出子树，避免把并行插桩夹具在用文件误判为残留。最终ordinary-result.json为passed。早先缺catalog构建及清理检查失败记录保留，不改写为成功。

Ubuntu24.04.5用户空间/GCC13节点普通/插桩各七场景、codec及旧计划切换各十二场景通过，result.json为complete、raw_remaining=0；目录为build-ubuntu24-validation/rootfs/build/getdaq-failed-retirement/run-c5333696ae354a78b011cce792f694cc。共用WSL内核，不是独立Ubuntu机器。父服务/交接CLI的Ubuntu24构建和组合回归使用另一新目录；最终完整记录与候选封存尚待，不能把节点结果替代父服务或GUI验收。

新的原生构建仍使用严格C++11与pedantic-errors，旧本地Ice/GET/bzip静态库及系统Qt未整体插桩；数据receiver复用70.70准确字节，默认热路径无Qt。前轮70.71的828项历史工件本轮复核通过，旧候选不改。本轮尚不宣称新的整套交付检查点，目标仍未完成；所有传输测试流合计不超过20GiB，结束删除自有raw，不清理生产数据。

本轮后续Ubuntu24父库普通/插桩各两编码的新恢复场景均通过；实际ECC HTTP/CLI既有计划更替普通两编码通过，插桩第一种编码通过。插桩plan-transition-complete-1在原退休边界的两组历史运行出现配置为1500ms的客户端HTTP超时，原父failed已落盘，未进入节点新修复分支；原失败组及独立复现均保留。随后用原失败组完全相同的ECC服务、测试fixture、数据服务、receiver和handoff CLI二进制摘要，在同一Ubuntu24隔离环境独立复跑一次并完整通过；新结果在build-ubuntu24-validation/rootfs/build/getdaq-failed-retirement/plan-complete-original-binary-983f4ab4faab4ed4ad551b9c04524988，外层result.json SHA256=e67959c8e310b6020b06bcc43d0db387b9345d067346d40d3c298c8a765c768a，runner结果SHA256=581e7d88fdb18dc315f25cf793616349f74191460679f076beb1d6529d55f4dc。该结果绑定原ECC 6a03f8d940da2351063e6949249c199ea9dbd5ca068076664485b4b5c2fad7fb、原fixture 2804b0f19073f43e60713b1aaa74baec93b1b6ba19901e675991186eb985857b、原handoff CLI fe71dba27a6b59792554b8326891945a90e4d40711f3fc143786cc9988eb9bc6；ASan/UBSan与LeakSanitizer开启，raw_remaining=0。旧故障在原二进制上未能复现，具体超时调用和成因仍未确认，所以保留失败记录，不放宽1500ms截止时间，也不声称根因已修复。

为定位及改善操作诊断，controlhttp.cpp的运行时错误现追加固定请求方法/路径，不含认证头或请求体。只替换该客户端对象、服务端程序保持不变的诊断夹具完成了同一场景，但该次未触发超时，所以不称为根因修复。诊断结果在Ubuntu24目录http-diagnostic-af94639dbf384c478d47c1566dbcb8eb/evidence；先前完整功能构建的源码摘要因此对这一错误文本变化历史化。宿主http-context-ordinary/http-context-sanitized分别将当前HTTP对象置于原核心库之前，现有八个HTTP故障各通过，核对单连接、不重发、截止时间、方法/路径和凭据不泄露；codec也通过，不创建raw。不能据此声称全部服务端已重编译为该诊断版本。

本轮仍未封存新候选或完整检查点，prepared包装脚本不会把失败回执当作成功。后续需在原二进制下继续捕捉超时调用和耗时，并完成实际新链的ECC父请求及窗口组合验证。阶段主机与Ubuntu24自有测试raw均复核为0；新旧结果分别保留，不用成功复跑覆盖历史失败。

随后在 `ecc-network-service-tests` 增加 `plan-transition-failed-node-0/1`：两个原子退休都实际受理后，让第二个私有数据服务在读取精确审阅计划时遇到一次性故障；恢复同一计划后，明确提交节点补齐，再由原ECC通过实际HTTP父操作读取补齐证明并导出原始退休回执。测试修正了注入器备份文件名与审计请求读取方式；Ubuntu24增量构建器也修复了构建目录外生成源码使用`relative_to`导致的增量编译异常。测试runner按本用例的真实范围检查原交接失败、完成观察及前后旧实例持续fenced，不再要求它生成只适用于成功新周期的回执。

Ubuntu24.04用户空间/GCC13.3在WSL2内核上，经rootless user/mount/pid namespace和只读源码挂载完成实际运行。普通版两个编码变体均exit 0；独立ASan/UBSan构建也两个变体均exit 0，LeakSanitizer开启；使用实际ECC HTTP服务、两个实际数据服务和receiver，故障由私有测试注入器确定性触发。两个变体都核对原节点请求/failed历史未改、修复请求只接受一次、父完成只读取修复证明并导出原退休回执、硬件写入数不增加，以及父原控制实例在修复前后保持fenced。结果边界：合成硬件servant、localhost、不是实机/跨主机测试、不测吞吐、不包含Qt窗口和新计划周期启动。

最终总回执`build-ubuntu24-validation/rootfs/build/getdaq-failed-retirement/parent-failed-node-7a8c0eaf083c459a8463939edbf59f7d/result.json`的SHA256为`f09ee2a8660ae3a88e0346c58efd44be3f5d180f39f53a5e31b29384fb413f36`，status=complete、raw_remaining=0；其清理回执同目录`cleanup.json`也为raw_remaining=0，阶段目录复扫`.graw`为0，服务/测试进程为0。普通runner结果`ordinary-evidence/description-recovery-ay2lafzt/result.json` SHA256 `4cea94634fc1bb557101dc49554edec806d5d592ba37091d924cbcf724183d7a`；插桩runner结果`sanitized-evidence/description-recovery-ouix_b5s/result.json` SHA256 `e87157d67adeed85870156fdba6aaec89d02315f64e0dec2cd4bfac80b7a6f92`。两runner各自绑定fixture、ECC服务、supervisor、receiver和注入器摘要；runner配置总传输预算512MiB，低于用户20GiB上限。C++测试源SHA256=`34f81aad07fda9d69e9e39120e6341750847456f0c0fbd30590e0f7283a0a0d4`，构建脚本=`c8091b9695cc5afaecdb17bbcdaea6e665aaa87bf829cfa702dda5300ccaa22b`，场景runner=`b02c1d322bb0e24f72a152d235be2a11dc633b51a1c1acf47d6f2c6d005c8ef4`。此结果关闭了“新失败节点ECC父HTTP场景尚无运行结果”的缺口，但之前 `plan-transition-complete-1` 插桩HTTP超时仍未定位；Ubuntu24生产Qt父补齐窗口现已有独立通过证据，70.72新失败节点修复与窗口合并场景仍待，新候选封存和整套现场验收仍待完成。

Ubuntu24.04用户空间内用仓库现有`plan_transition_window_probe`重新构建实际Envoy MainWindow/RunControl链，执行`plan-transition-completion-window-1`。结果目录`build-ubuntu24-validation/rootfs/build/getdaq-failed-retirement/plan-transition-qt-window-9c20a74e919141ee9574925001542eda`：Qt offscreen生产窗口通过取消无副作用、显式确认只提交一次、原失败保持、保存原退休上下文、旧控制按钮禁用、父补齐后只读观察、回执导出及服务端review lease仍阻止新启动；raw_remaining=0。外层result SHA256=`81e05cd289aa31442a68dff19741ab200b3a1b96b296479d0250b18948e8921c`，runner result SHA256=`163c0618fc2568e4a6797935a4b79e62541f5315ce2871457dcdde37926ed67e`，probe SHA256=`787e51d9be1c54b29ba9ffaa25bcb6932e1cec3631d7bdbef73c3503ccad1d81`。这是Ubuntu合成回环/offscreen窗口证据，不是真实桌面、独立机器或现场验收；它本身只验证原计划部分退休后的GUI父完成流程。下述新组合复跑单独补齐“失败节点显式修复+Qt父完成”证据。

随后在Ubuntu24.04用户空间/GCC13.3中，使用实际ECC HTTP服务、两个数据服务、native receiver及生产Qt窗口，复跑`plan-transition-failed-node-window-0/1`两个编码变体。两组exit 0；每组均在两个原子子退休请求已受理后注入一次性计划读取故障，确认原失败保留，再对同一节点/原请求执行一次显式修复，最后由Qt窗口在取消无副作用后显式完成原父操作；核对旧控制持续fenced、父完成无硬件写入、回执可导出。窗口日志分别记录`PASS production plan review window, Cancel/Yes, original server lease, retirement receipt and old-control hold`，`partial-completion-result.json`明确`operator_window=true`及`explicit_node_repair=true`。独立runner结果为`build-ubuntu24-validation/rootfs/build/getdaq-failed-retirement/plan-transition-failed-node-qt-window-rerun/evidence/description-recovery-os5e1mot/result.json`，SHA256=`6ae831c9f7df3488a7e3a494a171634a0c576d47b1effb7e088a03625051baf8`，runner脚本SHA256=`25e6e8a28d254933da3cf016dee108f2e18e42d1c22fabf4d7b09d80ff8d0b9e`。实际fixture/ECC/receiver/supervisor/Qt probe/injector的程序SHA256按顺序为`5433bbcb6e9ea2b488c01b7ce54fccf757f8579d946d22159c61eefb2b663791`、`8875192ac82389da3a6dd98370ad5d80b631c82ced95cea17aefa0ef39996ea0`、`83b611db1018d6836351633fc8aba22567dc3d72778c7ea57fa7e90ab65c42ac`、`d7fd3f15ea17470cf51c27056c57b9f652d6959009df0ecef1c21bd9376bbd4a`、`f2321937817d040b36fbcb8401b49845d50c48740e96f477c86c76e481dbd9d8`、`2895554bd9a1ae5f0f6a4fe6c5abb652188f82fc6893c0b9813b362fc807cbbd`。该次复跑为普通构建，未启用ASan/UBSan/LeakSanitizer；512MiB为runner总线量上限，实际8个合成`.graw`共131072字节，清理回执逐个列出，raw_remaining=0。测试由本机Ubuntu24用户空间及localhost合成servant组成，共用WSL内核，不是独立机器、真实硬件、跨主机或吞吐验收。先前的一次组合通过回执记录的构建缓存后来为释放空间而被清理；本段结果来自清理后的重新构建和实际重新运行，不依赖那批已清除的二进制。

随后补齐同组合的ASan/UBSan/LeakSanitizer版本：新ECC网络fixture和ECC Qt HTTP服务由Ubuntu24 GCC13严格C++11插桩构建，数据服务也使用当前源码的Sanitizer构建；Qt窗口探针、receiver和故障注入器按回执所列程序运行。两个编码变体均exit 0，runner启用`detect_leaks=1:halt_on_error=1`和UBSan错误门槛，并检查所有日志未出现Sanitizer错误。结果目录`build-ubuntu24-validation/rootfs/build/getdaq-failed-retirement/plan-transition-failed-node-qt-window-sanitized/evidence/description-recovery-iiw5_nfr`，`result.json` SHA256=`a45df7e747e97b8ce22b2fa325bda86e8e3a42fbd2832c3f3d24e850925faae2`，状态`passed=true`、`sanitized=true`、两个场景exit 0、`raw_remaining=0`。fixture/ECC/receiver/data service/Qt probe/injector SHA256分别为`e90e1adae15b6b2baad16d89ca5519aebd0bebe35da5d6a966b7bdcc73bd94a4`、`d7ee0d2786c9520a64fb4d6a19137f591b24fc8978cb153e11f0388458cd96da`、`83b611db1018d6836351633fc8aba22567dc3d72778c7ea57fa7e90ab65c42ac`、`306c8852c5a3a96699df8120a9c6ef67eb96fec3275ad77ed3521a25fe5c05d3`、`f2321937817d040b36fbcb8401b49845d50c48740e96f477c86c76e481dbd9d8`、`2895554bd9a1ae5f0f6a4fe6c5abb652188f82fc6893c0b9813b362fc807cbbd`。预算上限512MiB；8个私有`.graw`共131072字节，清理回执逐个列出且无残留。Qt AUTOGEN在隔离namespace不能通过libuv启动子进程，故该次只对Sanitizer supervisor手工执行同一Qt5.15 MOC、GCC13编译与链接；其运行库、自动信号/槽及测试窗口均真实运行。该项不改变产品代码，完整普通/插桩E2E结论分别保留。

截至当时的记录，70.72已完成此组合场景的普通及ASan/UBSan/LeakSanitizer双编码回归，候选尚未生成。完整HTTP插桩回归`plan-transition-complete-1`的历史超时调用/根因仍未确认；现场硬件/跨主机及持续吞吐验收继续待做。

随后在同一Ubuntu24.04.5用户空间中，使用当前 `controlhttp.cpp`（SHA256=`b93a89b007b3ff04e870103428fd0b7259c4987475b79334aa30b70fb12b6130`）重建普通及ASan/UBSan/LeakSanitizer的完整监督服务、查询CLI、catalog和测试fixture，并以两种构建分别运行失败后显式补齐七场景及旧计划切换十二场景，codec也通过。十项构建/运行步骤全部exit 0，终回执 `build-ubuntu24-validation/rootfs/build/getdaq-failed-retirement/run-5a44b6a37f6f4e9ab66f6752d20f41cb/result.json` SHA256=`e541b2f975a73639d130e88c8ba38030695d79910871a279528dd0149c89cbab`，绑定两套服务/查询/catalog/fixture摘要；普通与插桩两个工作流各自 `raw_remaining=0`，合计测试数据预算上限138412032字节（132 MiB）。这关闭当前诊断代码尚缺统一服务/查询构建及工作流验证的缺口，不解释或修复先前完整ECC `plan-transition-complete-1`插桩回归的两次1500ms超时；该超时的具体调用/根因仍未知，失败记录与原截止时间保留。此处为本机Ubuntu24用户空间/localhost合成数据，不是独立机器、真板或吞吐验收。

再用该统一构建中的普通/插桩数据监督服务，重建ECC父级原生fixture、handoff CLI及ECC服务，并运行父级恢复和四个实际HTTP场景（`plan-transition-complete-0/1`、`plan-transition-failed-node-0/1`）。普通与ASan/UBSan/LeakSanitizer均通过全部场景，十项构建/场景命令exit 0；此前超时的 `plan-transition-complete-1` 在两种构建中均通过，但这次没有复现历史超时，不能据此推断其根因已修复。父链总回执 `build-ubuntu24-validation/rootfs/build/getdaq-failed-retirement/parent-e10e6f5dbf164d4eb42e3a52dbb76288/result.json` SHA256=`5ed11e37ff41e20f278d817105e90fec40dd37797217aa6298ff2ffffa0b1984`；普通/插桩HTTP runner结果SHA256分别为`9fb9a48474d04535bcd5c994a7bc4d3f91cf313ee2c940ecc35d598d7b6c1861`及`6a7f6d17417aff6de1d70552f3d15ce5481444734e285d91d08963c15c86474c`，两者 `raw_remaining=0`、20个`.graw`各自删除且清理回执无残留；每个HTTP runner预算512MiB，合计1GiB，低于用户20GiB总上限。此证据来自本机Ubuntu24用户空间、localhost及合成板端servant，不是真机、独立内核/机器或持续吞吐验收；历史失败和1500ms截止时间保留。

随后在同一统一服务/查询构建上执行完整 `run_supervisor_control_tests.py`：普通及ASan/UBSan/LeakSanitizer各22个结果项全部exit 0，覆盖18项服务/receiver场景、8项原生HTTP报文故障、codec和凭据测试。递归检查普通与插桩证据日志均无ASan/LSan/UBSan错误；实际集成HTTP客户端诊断会在超时/丢失POST应答中显示固定方法和路径，例如`Supervisor HTTP deadline expired ... [GET /v1/identity]`与`Incomplete supervisor HTTP response ... [POST /v1/operations]`，不记录认证头或请求体。回执 `build-ubuntu24-validation/rootfs/build/getdaq-failed-retirement/current-control-regression-09bb9b92762b4275a994bffbd87cd3b0/result.json` SHA256=`a34ae2bf31abbda57a69bd9dc7c10bfb81c9a4901056c87e5449eb8a2da1a364`，绑定两种构建的服务/查询/fixture/receiver摘要；控制runner SHA256=`5db38a1757a24fb53e9f9c5eb62f0ffe32c632e4097887446f2d912f393f7515`，驱动SHA256=`17c368f44af10b99277dbbbe32676d711935fdf7c173e29f1cb94a337b1df48a`。测试累计生成46个`.graw`、679936字节，均由本轮清理，残留为0，低于20GiB总量限制。这个故障矩阵验证诊断信息与控制协议边界，不会重现或解释历史父级计划切换超时。

对两次历史 `plan-transition-complete-1` 超时现场的进一步对照显示，父ECC网络操作日志已持久记录 `handoff-data` 的 `accepted`、`running`、`failed`，失败原因是节点监督服务返回HTTP 409。测试代码在HTTP POST成功返回202后才会调用 `Terminal()` 轮询该操作（`GET /v1/operations/handoff-data`）；只有取得终态才会写出`partial-retirement-operation.json`并进入显式补齐断言分支，而失败现场没有该文件。因此超时位于提交请求应答或后续终态轮询，但旧客户端日志没有请求方法/路径，父日志也无法证明客户端是否收到POST的202，不能在这两者中作出确定判断。保留的独立插桩复跑通过了同一场景：日志中该handoff POST返回202，随后`GET /v1/operations/handoff-data`返回200；结果`build-ubuntu24-validation/rootfs/build/getdaq-failed-retirement/http-timing-1108c684810e4fdcb710b4b55a8c21a0/evidence/description-recovery-egla_maz/result.json` SHA256=`eb7ed4f2141f97de08d34c648bf77cd54db66daf11cddc140a732168704e1a61`，Sanitizer/LeakSanitizer开启、512MiB总预算、`raw_remaining=0`。该成功时序不能解释旧超时；新客户端错误现会包含固定方法/路径，若重现可据此分辨。保留1,500ms截止和历史失败工件，不将节点409视作HTTP超时根因。

2026-10-20新增Ubuntu24候选包`build-packages/getdaq-ubuntu24-20261020-failed-retirement-candidate`，由70.72当前Ubuntu24普通构建替换`getdaq-supervisor`、`getdaq-supervisor-query`、`getdaq-catalog`、`getdaq-ecc-server`、`getdaq-ecc-select`和`getdaq-ecc-handoff`；其余19程序与基底`getdaq-ubuntu24-20261019-control-http-candidate`逐字节相同。为减小交付体积，六个本轮程序仅在包内副本剥离debug信息；原构建二进制摘要和包内交付摘要分别记录，剥离后的六个程序再次通过Ubuntu24加载器检查和`--help`。候选约86 MiB。候选生成器为`build-failed-retirement/package-current-ubuntu24.py`，回执为同目录`package-current-ubuntu24-result.json`；文件清单、程序摘要、动态依赖和更新CLI用法可用`python3 build-failed-retirement/package-current-ubuntu24.py --verify`复核。使用Ubuntu24 rootfs动态加载器对包内25个程序解析依赖，无缺失；接收器保持无Qt依赖。包清单还绑定节点、服务和ECC父级验证回执摘要。该候选不是新一轮完整25程序重编译，不包括插桩二进制/故障注入器，也没有独立Ubuntu机器、真实桌面、真板、跨主机或吞吐验收。历史`plan-transition-complete-1`两次1500ms超时尚未定位根因；保留原失败和截止时间，70.72与整体重构均未完成。测试自有raw为0。

随后以包内剥离后的真实控制二进制执行`plan-transition-failed-node-0/1`集成场景：receiver、supervisor、ECC HTTP service及handoff CLI均从Ubuntu24候选包启动，测试fixture和一次性故障注入器独立保留在测试目录。因当前宿主不能创建嵌套user namespace，使用静态本机启动器把每个包内程序交给候选Ubuntu24 rootfs的动态加载器；这验证的是包内程序在该Ubuntu24运行库下的集成行为，不是独立Ubuntu机器验收。两个编码场景均通过，runner回执`build-failed-retirement/package-smoke/run-af754b47912b4603aec9bead18bf2cf4/description-recovery-2xzu991j/result.json` SHA256=`9a98b3cfa292b4a79ed1852d0a389ffb1e92a8ac1452ab78b0a107a5a74ebc51`；外层包烟测`package-smoke-result.json`绑定包内5个实际交付程序摘要。共生成并清理8个合成`.graw`、131,072字节，测试运行上限512MiB，`raw_remaining=0`。可由`python3 build-failed-retirement/run-package-smoke.py`重跑。它是普通交付二进制的合成HTTP集成烟测，不是插桩包、完整Qt桌面、真板、跨主机或吞吐验收。

随后对历史插桩超时场景`plan-transition-complete-1`做两次当前Ubuntu24 ASan/UBSan/LeakSanitizer复跑，实际使用当前保留的插桩fixture、ECC服务/handoff CLI、supervisor、receiver及Ubuntu24 `libasan.so.8`。两次均通过且均未重现1500ms超时：回执分别为`build-failed-retirement/timeout-diagnostic/attempt-e55b0a25be664e3bb109248358f5c77c/attempt-result.json`（SHA256=`8383d59d19b53888bb7d096efdeb52c3221f396cd126640bc9d02ed4ea2afc41`）及`attempt-f5ac52fd9fa84e1291c9fc4cc814e9dc/attempt-result.json`（SHA256=`3756d0f745324496e5353c83ed72c96e0040e347804fbaa2ecb26bf1b5c0cfd6`）。两次都启用LeakSanitizer、各自声明512MiB传输上限、实测生成并清理81,920字节，残留`.graw`均为0；测试runner SHA256=`037490ec6741335db8b921864abb3be74e825ea977a601540c40e67a4a3a7cd8`，复跑脚本SHA256=`bfea73c836548438982fdfa70010179e37df19c356f2fbcc682e0e7e81e78714`。其中一次普通沙箱运行因禁止创建本地socket失败，未计作产品失败或通过；获准回环运行通过。两次通过只能说明当前复跑没有重现问题，POST应答与终态GET中的哪一步曾超时及历史根因仍未确认，旧失败记录和1,500ms截止时间保留。

2026-10-20空间清理：`build-packages/`此前存放110份按日期/功能累积的Ubuntu、Windows和通用主机候选副本（约8.48GB）。保留当前Ubuntu24完整控制候选`getdaq-ubuntu24-20261020-failed-retirement-candidate`及用户Qt5.14.2 MinGW操作端`getdaq-windows-qt5142-20261019-functional-candidate`，清除其余108份过期候选；源码、测试报告、运行证据与这两份交付包保留。删除清单及精确回收字节数记录在`build-packages/cleanup-20261020.json`。历史阶段文档保留当时的包名和验证结论，但已删除的旧候选副本不再可从该路径取得；如需复验旧版本，应从相应源码阶段重新构建。另清除两份无效超时诊断尝试（一次被中断、一次被沙箱网络权限拒绝）的临时工件，两个有效通过回执及日志保留。

## 70.73 跨主机SSH归档流的本机契约验证（2026-10-20）

`apps/datasupervisor/catalogtransfer.cpp`实现一次性OpenSSH归档发送：先核对来源请求/manifest/receipt和receiver seals，再通过SSH标准输入/输出传递清单与原始文件；接收端逐文件校验长度及SHA-256、同步目标并回读全部内容。目标最终目录以Linux `renameat2(RENAME_NOREPLACE)`或macOS `renamex_np(RENAME_EXCL)`独占发布，目标竞争时保留竞争目录并拒绝覆盖。发送端进度回调按时间/字节节流，含目标端同步及回读阶段；CLI把JSON进度写入标准错误，标准输出只保留最终小型回执。SSH子进程使用CLOEXEC管道和检查过的POSIX spawn操作。无Qt、ECC、HTTP、OpenSSL或curl载荷路径；OpenSSH是此可选CLI的系统运行时依赖。

严格C++11、Ubuntu 20.04 GCC 9.4宿主构建：`cmake -S apps/datasupervisor -B /tmp/getdaq-ssh-package-build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DGETDAQ_BUILD_SUPERVISOR_SERVICE=OFF -DGETDAQ_RECEIVER_EXECUTABLE="$(pwd)/build-receiver/getdaq-receiver"`；构建`getdaq-catalog`、`catalog-transfer-tests`及`receiver-catalog-tests`。`ctest --output-on-failure -R '^(receiver-catalog-filesystem|catalog-transfer-filesystem)$'`为2/2通过（24.86秒，接收归档22.44秒、传输2.42秒）；CLI帮助列出发送和接收参数。宿主构建SHA256：`getdaq-catalog`=`4bea83a716233b539ef399653981a1b5efa44df3f938e07f323391242b8a92c9`，`catalog-transfer-tests`=`4bab367f16f1b936d16ac6879825a40a7227a63124872effae05b1889d23cd45`。

本地假SSH测试实际启动生产`getdaq-catalog`接收命令，验证路径带空格、协议握手、完成回执、进度累计字节与回执相同、源保留、重复目的拒绝。额外在接收端已接受协议后创建竞争最终目录，发送结束时独占发布必须失败；竞争目录未被替换，含意图的`.partial`证据保留在测试夹具并随夹具清理。该SSH夹具传输4个合成文件，合计139264字节；归档传输测试中的大文件最大2 MiB。并跑的接收归档回归保留既有512 MiB稀疏文件场景，仍低于用户20 GiB总上限；本次测试后`/tmp`、`/dev/shm`和工作区中的`.graw`残留均为0。远端路径空间、重传冲突和竞争发布均已覆盖，测试没有连接真实主机。

随后运行`tools/package_ubuntu24_catalog_ssh.py`，从已验证的`getdaq-ubuntu24-20261020-failed-retirement-candidate`生成独立`build-packages/getdaq-ubuntu24-20261020-ssh-archive-candidate`。本阶段只重新构建`getdaq-catalog`，24个其他程序按基础候选逐字节复用；包内25程序均由Ubuntu24 rootfs加载器做动态依赖检查，且拒绝解析到rootfs外部；目录清单校验完整且无未登记文件/符号链接。新候选约87 MiB，交付`getdaq-catalog` SHA256=`e9de473369df8f48987404d466978ada10727fe8f0230bf2625978526c45e6b8`。接着设置`GETDAQ_CATALOG_TEST_REMOTE_PROGRAM`指向一个Ubuntu24加载器启动器，再次运行`catalog-transfer-filesystem`；该回归调用候选包内接收二进制，1/1通过，未访问外部网络。回执`build-catalog-ssh/fake-ssh-test-result.json`嵌入候选`validation/catalog-ssh-test.json`，候选清单将其结果摘要与其他文件一并校验；基线清单摘要见`build-catalog-ssh/package-result.json`。生成器先在同级暂存目录构建/验证再发布，并校验清单涵盖全部文件；包内测试接收程序SHA与实际候选程序一致。测试后工作区、`/tmp`及`/dev/shm`均未发现`.graw`。

本地假SSH只验证POSIX进程/管道/协议/目标目录行为，不验证SSH密钥、known_hosts、远端登录shell、实际两机权限与防火墙、吞吐/中断恢复或macOS SDK及目标卷。独占重命名在目标文件系统不支持时会失败并保留部分目录，不退化为覆盖式发布；Apple资料说明该能力因卷而异：[Apple独占重命名卷支持属性](https://developer.apple.com/documentation/foundation/urlresourcevalues/volumesupportsexclusiverenaming)。当前Ubuntu24候选已含SSH CLI；Windows候选仍未包含该POSIX/OpenSSH功能。完整操作、密钥限制示例及失败处置见[跨主机SSH操作手册](CATALOG_TRANSFER.md#12-跨主机发送已完成归档)。70.74后续候选重建器源码SHA=`6ac8974d245a1416e68e79a59f18f0564c0db5c21207d6e9e3c6ba11c4c209af`。本节原70.73源码摘要：`catalogtransfer.cpp`=`e8a822e222f4cd20bf1859df2f41bd3e1e82e69cdb487ce2f3cc5abddc894124`、`catalogtransfer.h`=`33621510537cddf0acaca579d7846099bb8b774682373d93a76ad22303863fa6`、`catalogmain.cpp`=`56ad7298f14514eee73ba7eef83e013dbde2cbbc1ec57d3e95f6f492306b1155`、`catalog_transfer_test.cpp`=`4559049c57bd0583d2ec71482df7d7673140beb53359edf4fb6e4a2e98df8ec4`；当时`package_ubuntu24_catalog_ssh.py`=`ee1682781984deca4cc34e94e9fe1708dfe7deee8ba71e916a17b632b73c8f0d`。

## 70.74 在线导出工作线程故障清理（2026-10-20）

`apps/datareceiver/exporter.cpp`的导出线程此前在`accept`等操作抛出异常时只将`export_failed=true`、客户端数清零；已接受的socket仍打开，观察客户端会等到接收器最终析构才断开。现在故障处理先置失败状态，再由仍唯一拥有客户端socket的工作线程调用`CloseClient`，丢弃客户端待发送队列、关闭socket、更新断开计数并清空连接容器。在线导出依然是best-effort观察支路，不让它影响数据接收/写盘的成功语义。

Linux `exporter-lock-tests`使用链接器包装只对下一次`accept`注入EIO。客户端已连接后，断言工作线程报告失败、客户端数归零、断开计数为1且对端立即读到EOF。原有的暂停send并并发发布测试仍执行，并核对旧小端长度头与两帧顺序。普通与ASan/UBSan/LeakSanitizer两种构建均使用严格C++11、Ubuntu GCC 9.4；每种构建的`receiver-core`和`exporter-lock`测试均2/2通过。Sanitizer运行启用`detect_leaks=1:halt_on_error=1`及`UBSAN_OPTIONS=halt_on_error=1`。自有证据在`build-exporter-worker-failure/`，包括两个构建收据、20项输入SHA、完整CTest日志及摘要JSON；不保留可执行文件。失败注入本身发送零帧；原并发消息fixture载荷为5120字节。测试夹具退出后工作区、`/tmp`与`/dev/shm`均未发现`.graw`。

故障证据仅覆盖Linux localhost上的一次`accept`失败和原锁反例，不覆盖每种系统调用错误或macOS。70.74现已打入`build-packages/getdaq-ubuntu24-20261020-ssh-exporter-failure-candidate`：从70.73 SSH候选继承，重新构建`getdaq-receiver`，其余24程序复用；交付接收器SHA=`040d7ecdcfc8d5e259a6313d977e18643541677cfd98ad32145eb3471104b095`，源构建程序SHA=`4d6d64d492a52afba94692c1673fa14c742e7a2f382f5be3b1dfe9e52abd6f88`。包内25个程序均经Ubuntu24 loader依赖检查，catalog与receiver帮助检查通过；接收器交付二进制经包内集成回归，TCP分片/端序/双观察端/封存/目录锁/慢客户端/畸形帧全部通过。慢客户端场景写入32 MiB临时合成raw，测试完成自动清理；验证运行于Ubuntu24用户空间loader和Linux本机内核，不代表独立Ubuntu24机器。回执在候选包`validation/receiver-integration-test.json`，包结果见`build-exporter-worker-failure/package-result.json`。候选生成脚本校验清单中的回执摘要、回执所属候选、对应程序SHA及20GiB合成数据上限；`python3 tests/package_ubuntu24_catalog_ssh_test.py`的4项校验回执继承路径、修改后的回执、程序摘要不匹配和超限拒绝。脚本当前SHA=`64b99b7d88c1f3b9eb638719f6392616cc2f3c4af1fb6392f662c65999238cad`，测试SHA=`de843f5e7a0bbfc6f749e72d974818cfdbbc65f2c4406588940ff5109b2ce3d4`。这不是macOS、硬件或持续吞吐验收，也不授权直接覆盖现场进程。源码SHA256：`exporter.cpp`=`53f5e2559f6da211f605888549b493d7b28d301a086d26650ba8d3e9e14bd7af`、`CMakeLists.txt`=`764cbc6fbc3c913b55189e4b61fac63d44912e9ac47fba298a83abe4b511d969`、`exporter_lock_test.cpp`=`35f27d0bdc2aef1f3bbb210af20f9a57462ab7c1d3753b0f5f5552c60352f844`。

## 70.75 Ubuntu 24接收器交付候选双流负载复核（2026-10-20）

保留的Ubuntu 24候选包接收器以两条并行localhost TCP流各接收512MiB，总传输1GiB（远低于用户规定的全流合计20GiB上限）。每流131072个4KiB合成CoBo帧，各轮转为四个128MiB文件。候选包二进制SHA256=`040d7ecdcfc8d5e259a6313d977e18643541677cfd98ad32145eb3471104b095`；启动器用Ubuntu 24 rootfs动态加载器和运行库启动它。两流均核对发送与落盘总SHA、每文件SHA、字节数、帧数、EOF、退出状态和持久封存。两流RSS约20.2MiB。测试载荷总计1073741824字节，按自动清理收据完整删除，剩余`.graw`为0。

结果`build-exporter-worker-failure/load/receiver-load-rtfy0pgj/result.json`、清理回执、候选清单SHA、候选接收器SHA、启动器SHA和测试脚本SHA由`build-exporter-worker-failure/load/package-candidate-load-binding.json`绑定。测试环境为Ubuntu 24用户空间动态加载器/库运行于WSL2 Linux 5.15宿主内核，只有本机回环网络；不是独立Ubuntu24机器、macOS、真实硬件或真实多CoBo运行。总负载低于约49GiB物理内存且发送器含Python合成与SHA成本，因此不把运行时间换算成DAQ或磁盘持续吞吐指标。此证据补强候选程序的数据接收、完整落盘、持久封存和删除验证；未关闭其他平台/真实设备/长时间吞吐验收缺口。

## 70.76 跨主机SSH归档流显式续传与发布恢复（2026-10-20）

`getdaq-catalog`现提供显式发送端`--resume-catalog-ssh`和接收端`--resume-catalog-stream`。首传在目标`.partial`保存原请求、catalog清单、完成收据和来源检查点；检查点绑定源root的设备/inode、元数据摘要及每个raw文件的设备/inode/大小/权限/owner/链接数/mtime/ctime。续传时接收端为每个已有文件报告长度与SHA-256，发送端使用原来源验证全部前缀后才发送续传确认；接收端再核验文件描述符与前缀并只追加后缀。完整文件摘要、同步、回读与独占目录发布仍是完成条件。原完成目录若已发布但最终SSH回执丢失，显式续传会核验原来源checkpoint和完整目标后返回原回执，传输零载荷。普通发送仍拒绝已有目录或partial，不做自动重试、自动删除或来源身份变更后的接管。

新增的发布恢复回归模拟：所有raw和`transfer-manifest.json`已同步、完成回执pending文件已写入、但最终目录发布尚未完成；恢复只复用已核验数据且不重新传输载荷，完成独占发布后`--verify-transfer`与原完成收据一致。SSH测试仍通过本地假`ssh`可执行入口，不连接外部网络；另覆盖16,896字节的非对齐partial、损坏远端前缀拒绝、来源变化拒绝、只发送未传后缀、最终回执丢失后的零载荷核对、重复普通发送拒绝及竞争目录不覆盖。合成测试文件为小型fixture，测试结束由fixture清理；本项远低于用户规定的累计20GiB测试数据上限。

Ubuntu 20.04.6 / WSL2宿主、GCC 9.4、严格C++11下，`getdaq-catalog`及`catalog-transfer-tests`构建通过；最新`catalog-transfer-filesystem`集成CTest普通构建1/1通过（2.90秒）。ASan/UBSan构建同一CTest 1/1通过（4.13秒），使用`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`和`UBSAN_OPTIONS=halt_on_error=1`。尝试开启LeakSanitizer时，测试启动的进程出现运行时信息`LeakSanitizer does not work under ptrace`并退出；故此项**没有LeakSanitizer结论**。普通宿主`getdaq-catalog --help`列出两端续传CLI。关键源码SHA256：`catalogtransfer.cpp`=`b810d486b81e3d81001ac26414a13c51906fcf40073ac0563563c707fe953303`、`catalogtransfer.h`=`89d08d1aa6f5514448fe3521a3120a6433128eda982eba7886ca065ce119ad49`、`catalogmain.cpp`=`a18ee7dbb740a31a58cd30838879daed2ab0feaaf3b4b1a358e44afad673ce15`、`catalog_transfer_test.cpp`=`65f485e8bd5d0e6a956c9649ec5df843345f365bd53fce1b61e63409b80d63af`。宿主普通`getdaq-catalog`SHA256=`d9124dec186811dc0f73f4b573775dcb4f8019c36eef2c115e5a6f8cf4589e6c`；这是本轮debug测试二进制，单独交付候选于下述段落。

Ubuntu 24候选`build-packages/getdaq-ubuntu24-20261020-ssh-resume-final-candidate`从此前已验证的SSH/接收器候选派生，只重建并替换`getdaq-catalog`，其余24个程序按字节复用。交付目录约87MiB，25个程序均通过Ubuntu24 rootfs加载器依赖解析且未越出rootfs；交付catalog SHA256=`f1c2709d125dbf358d142696e40ada673bfe2409690d4b56654d107f0da24b45`。用候选包内catalog作为假SSH远端接收程序，`catalog-transfer-filesystem` 1/1通过；4个合成文件、139264字节，测试后raw为0。回执`build-catalog-ssh-resume-final/catalog-ssh-test-result.json`绑定远端程序SHA，包内证据为`validation/catalog-ssh-test.json`；生成清单和程序摘要可用打包结果`build-catalog-ssh-resume-final/package-result.json`及`python3 tools/package_ubuntu24_catalog_ssh.py --verify --candidate build-packages/getdaq-ubuntu24-20261020-ssh-resume-final-candidate`复核。打包器在catalog被替换时会移除其旧catalog SSH测试回执、保留未被替换receiver回执；新增的回执失效/路径安全测试与原校验器合计6项通过。候选由Ubuntu24用户空间loader/库运行于Linux本机内核测试，不是独立Ubuntu机器或真实SSH两机验收。

结果为Linux本机文件系统/进程与假SSH端点契约，不证明真实OpenSSH、两台独立主机、现场权限/网络、Windows、macOS、目标卷独占重命名或持续吞吐。需要这些现场和平台证据后，才能将此阶段推进为跨平台交付验收。

补充Ubuntu24用户空间运行检查：将同一严格C++11 `catalog-transfer-tests`测试程序放在Ubuntu24 rootfs动态加载器与库下执行；候选包中的`getdaq-catalog`也由相同loader启动，用作跨文件系统CLI进程及假SSH接收端。测试程序完整返回0，报告12个原生传输场景通过；结果为`build-catalog-ssh-resume-final/ubuntu24-runtime-test.json`，日志SHA256=`01e3b60c4c4f2f726162cbc3a61ed60234151a72d1802622b6097ebc404202ac`，raw清理检查为0。此检查运行于Linux宿主内核，不是独立Ubuntu24机器或网络SSH验收。

## 70.77 旧生成物清理与交付打包器路径防护（2026-10-20）

按用户要求检查工作区空间后，删除已被续传候选取代的`getdaq-ubuntu24-20261020-ssh-exporter-failure-candidate`（约87MiB）和10月3日的旧本机qmake `build/`（约3.5MiB）。续传候选沿用已测试的receiver程序和包内接收器回执，因此无需保留重复的旧包；其构建/负载结果仍在`build-exporter-worker-failure/`中。保留当前Ubuntu24续传候选、Windows Qt5.14.2 MinGW候选、Ubuntu24 rootfs及有效验证回执。删除后工作区约1.6GiB，主要包含约1.3GiB的Ubuntu24 rootfs；当前文件系统报告约621GiB可用，测试目录未发现`.graw`。清理不触及源码、第三方依赖或生产数据。

审查`tools/package_ubuntu24_catalog_ssh.py`时，发现根候选路径先`resolve()`会隐藏根符号链接，且继承回执路径的中间目录若为符号链接可导致unlink越界。现在在解析候选根前拒绝符号链接，校验基础/最终包时检查完整文件清单、路径与特殊文件；删除失效回执前逐级检查每个路径组件，并只允许删除普通文件。新增三项回归测试覆盖候选根链接、基础包内部链接及指向外部目录的证据父链接，并确认外部目标保持原样。打包器测试9/9通过，现有Ubuntu24续传候选完整校验通过；文档链接检查1174项通过。候选包本身没有改写或重新打包。Linux本地符号链接测试不代表Windows打包器行为。

## 70.78 Ubuntu 24/macOS原生数据面自动化与本机命令验证（2026-10-20）

新增`.github/workflows/native-data-path.yml`，在Pull Request与手动触发时分别使用`ubuntu-24.04`和`macos-14` runner。两边都检查离线`data`依赖档案，构建不链接Qt的接收器及归档/传输核心，执行receiver integration、receiver本机测试、目录/传输/控制协议测试；Linux另运行链接器注入的exporter-lock回归。工作流先执行`ctest -N`并核对预期测试数，再从构建目录运行CTest，避免旧版CTest误把“没有测试”报告为成功。它是可重复的真实平台验收入口；本地工作树并未触发GitHub Actions，因此本节不声称Mac runner已经通过。

本机按工作流命令在GCC 9.4 Linux环境构建并复跑：receiver CTest 2/2通过；`tests/receiver_integration.py`的TCP分片、两种端序、多观察端、完整写盘/封存、目录互斥、32MiB慢观察端隔离、截断帧及无效长度场景通过。脚本未传入sentry程序，因此跨进程sentry归档子项按设计显示SKIP。监督器C++11构建后，`receiver-catalog-filesystem`、`catalog-transfer-filesystem`和`supervisor-control-codec` 3/3通过（25.68秒）。集成测试首次运行碰到随机端口被占用；测试现同时保留多端口预约直到选齐，并在启动明确报`Address already in use`时最多重新分配三次，其他启动失败仍直接报错；修正后的整组回归通过。最大单个合成raw场景32MiB，临时数据自动清理，`/tmp`可见范围检查无残留`.graw`。

YAML由PyYAML解析、数据依赖档案核对通过。该环境不能提供Apple SDK；macOS真实编译、`renamex_np(RENAME_EXCL)`在目标卷上的执行、`otool`依赖核对、Qt Mac Kit部署与签名仍需Actions/真实Mac补验。Ubuntu本机命令成功仅验证该Linux工具链，不替代Ubuntu 24 runner证据。

## 70.79 新建ECC数据链路默认单入口（2026-10-20）

检查发现`docs/DATA_SERVICE.md`已要求统一接收器部署禁用旧ECC第二目标`exporter_port`，但操作台`NodeConfig`默认仍为46007，新建配置会让板端额外向旧DataExporter建立原始数据连接。现在未显式设置时默认0，只生成指向统一接收器的router DataLink；YAML中明确设置非零`exporter_port`仍保留旧第二目标，便于尚在使用旧DataExporter的配置继续运行。同步更新数据服务说明，明确旧板端第二目标与新receiver下游`--export-port`方向相反。

使用Qt5.12.8/GCC9.3在Linux构建并运行完整控制台QtTest，98/98通过；`protocolValidation`、`soapStringParametersAndTopology`、`yamlCompatibility`专项共5/5通过，覆盖新默认不生成46007 DataLink、显式端口仍产生第二目标、配置YAML往返保留该显式值。完整测试第一次在受限sandbox里因loopback和系统接口不可用而失败；获准的本机测试执行同一二进制后全套通过。临时构建`/tmp/getdaq-single-ingress-test`已删除，工作区和`/tmp`均无新`.graw`。本改动仅影响控制面默认目标，不改高流量接收/导出代码；未连接真实CoBo，也未在用户Windows Qt5.14.2上构建。

## 70.80 操作台旧ECC第二数据目标标识（2026-10-20）

节点配置表将原来的“Exporter port/host”改为“Legacy ECC target port/host”，并为router与旧ECC第二目标增加表头提示：前者是板端连入主接收器，接收器的`--export-port`是分析客户端连入的下游端口；后者则是旧ECC配置下板端主动建立的可选第二条数据连接。端口0表示关闭第二目标，推荐的新统一接收部署使用默认值0。

本轮为纯Qt表格文案/tooltip修改，没有改变配置字段、XML协议或数据路径；未增加镜像实现的UI单测。Qt5.12.8/GCC9.3严格C++11的完整`getdaq-tests`构建成功；`protocolValidation`、`soapStringParametersAndTopology`、`yamlCompatibility`共5/5通过。20MiB临时构建目录随后删除。此结果不代表用户Windows Qt5.14.2、macOS或硬件验收通过。

## 70.81 接收器最低可用磁盘保护（2026-10-20）

为避免接收器持续写盘直到文件系统耗尽，增加可选`--min-free-mib`（默认0以兼容既有命令），并允许固定托管计划按source设置`min_free_mib`。该值进入计划摘要，监督器在创建本次启动审计/输出目录和启动子进程前逐输出卷预检；receiver和ICE receiver仍在进程启动前复查，并在写入线程累计约1MiB数据或处理较大帧时调用`statvfs`复核。写线程评估配置保护线、已有内存缓存、当前帧、下一次复核窗口与额外分配余量；无法查询空间或触发保护线时，丢弃未落盘缓存，保留未封存失败目录，不生成成功seal。空间预检是best-effort检查，不是排它配额；其他进程仍可并发占用空间。

Ubuntu20.04宿主/GCC9.4 C++11构建：receiver核心边界/溢出/检查节流测试通过。新增运行时边界用例先缓存256字节，再令16MiB下一帧触及预测余量保护线；writer拒绝该帧，Abandon不刷新缓存，已创建的`.graw`保持0字节且没有seal，私有夹具析构后清理目录。该用例测试真实`statvfs`结果加待写帧预算，不填满磁盘，也不声称实际余量从高值持续降到保护线。supervisor计划codec通过；16项实际receiver进程生命周期测试通过，新增的大保护值场景证明在新Start意图、运行目录与子进程之前被拒绝，状态仍为Fresh。托管supervisor与receiver实际回环联调通过（正常生命周期、8项本机HTTP传输故障、codec及凭据），实际ICE receiver 14项合成入口场景通过，状态端点均核对配置的最低余量。ICE测试夹具线上预算上限32MiB，远低于用户要求的所有测试流总计20GiB；本轮Ubuntu20主机套件的50个测试`.graw`合计1,441,802字节，随后全部随私有`/tmp`证据树删除。

随后在项目Ubuntu24.04.5用户空间/GCC13.3重新构建receiver与Qt启用的监督服务，均为C++11并对两者使用`-pedantic-errors`；vendored Ice/GET的ICE receiver和fixture也从当前源码构建成功。该用户空间下receiver core、supervisor codec、16个进程生命周期、监督服务normal与8项HTTP控制故障、ICE 14项入流场景均通过。23个测试`.graw`为671,749字节；连同上面Ubuntu20轮次，两轮总计73个`.graw`/2,113,551字节，远低于20GiB，测试构建与raw证据树已经删除。按文件路径内源码摘要及编译后二进制摘要绑定的小型回执保留在`build-ubuntu24-validation/rootfs/build/getdaq-disk-reserve-u24-result.json`，SHA256=`b45e72621419d2ccd0edf022ee72bb2a7152ea48bff7e6bd8d29e3e2e5b810de`。这是Ubuntu24用户空间运行于WSL2 Linux内核的验证，不等于独立Ubuntu机器、真实硬件或DAQ/磁盘持续吞吐；本轮未重制交付候选包。官方Ubuntu base镜像、签名、校验和及验签receipt已经恢复，验证rootfs保持ready。

手册见[数据服务磁盘章节](DATA_SERVICE.md#6-内存背压和磁盘)、[远程接收器控制计划](REMOTE_RECEIVER_CONTROL.md)及[ICE入流](ICE_DATA_INGRESS.md)。本验证没有真实CoBo、独立Ubuntu24、macOS或Windows测试，也没有持续写盘/DAQ吞吐证据；历史候选包尚未因这项改动重制，源码树测试不能替代包验收。

## 70.82 当前源码Ubuntu24全量交付候选（2026-10-20）

为使70.81磁盘保护进入可审阅交付物，在通过官方签名与SHA256验证的Ubuntu Base 24.04.5用户空间中，从当前工作树以`--profile full`重新构建25个程序，而不是从旧候选复用二进制。环境为GCC 13.3、Qt 5.15.13；构建输入回执记录459个源文件/构建入口摘要，并在构建完成后重新计算确认459项均与工作树匹配。构建包及操作方式见[主机统一构建手册](HOST_SUITE_BUILD.md)。

候选目录为`build-packages/getdaq-ubuntu24-20261020-current-source-candidate/`，包含25个程序、中文文档、示例配置、许可证和SHA清单。`manifest.json`列出完整包内文件；`validation/full-build-receipt.json`记录完整构建环境、命令、25个程序摘要和输入摘要；`validation/delivery-validation.json`汇总本节回归与限制。复核包文件：

本候选构建回执SHA256=`e82ae9ae9427da1d66a3b5dd8edc0532a26f88d8b48d8378dee892a2242d215a`，验证回执SHA256=`8a88388aa719467c119ca457d1595215cd7f5fd55ebf9143c1b37ef6553435a6`。receiver SHA256=`34511044c27ef96fe04f96b9ca5bb523e17d140b64ed15218bda3ae8cdc658fc`，catalog SHA256=`61ac3c4575035e5fadada9066bf313c5c0db8b2f926fd16ec0a35b12ecba21ca`。验证回执还绑定八个关键测试/构建脚本摘要与25个候选程序摘要。

```bash
python3 tools/build-suite.py --verify-stage \
  build-packages/getdaq-ubuntu24-20261020-current-source-candidate
```

每个候选程序均用Ubuntu24动态加载器执行`--list`，25/25通过，依赖解析保持在已验签rootfs内部；`getdaq-receiver --help`和`getdaq-catalog --help`也通过。新候选receiver与sentry实际运行TCP/写盘/封存集成：分片TCP、端序、多个导出观察端、慢客户端隔离、截断/无效长度拒绝、目录互斥和封存归档均通过，最大单个`.graw`为32MiB。新候选catalog作为本机传输测试的实际CLI及假SSH接收端，12项跨文件系统复制/续传/中断/损坏拒绝/竞争发布场景通过；单个raw夹具最大2MiB。supervisor核心另通过`disk-reserve`启动前预检回归，该场景未创建raw、启动意图或Run目录。接收器与catalog测试使用的自有数据已由夹具清理；工作树、`/tmp`和rootfs临时目录检查没有剩余`.graw`。

为让跨文件系统回归有真实不同的`st_dev`，修正Ubuntu24隔离runner：私有挂载命名空间内为`/dev/shm`挂载256MiB tmpfs，测试文件不接触主机共享`/dev/shm`，挂载随runner退出卸载。初次回归因rootfs缺少`/dev/shm`无法创建目标夹具；补齐后12项全通过。这是验证环境修复，不改变产品数据路径。

本候选已替代旧的Ubuntu24续传快照。清理后移除了旧87MiB候选、275MiB临时构建目录及约340MiB Ubuntu apt包/索引缓存；保留已安装编译器、Qt和831MiB验证rootfs，日后在此rootfs安装新软件前需重新`apt update`。本轮工作树及临时位置均无残留`.graw`。验证结果证明本机Linux内核/Ubuntu24用户空间加载、构建与合成数据路径，不代表独立Ubuntu24机器、macOS、Windows人工完整操作、真实SSH双机、板端、长时间运行或DAQ持续吞吐验收。测试限制继续按每轮输入/输出合计低于20GiB，并在结束后删除自有数据。

## 70.83 当前源码Windows Qt功能回归（2026-10-20）

使用用户的Windows Qt 5.14.2 MinGW 7.3 x64，在原生Windows进程中构建并执行当前工作树的完整界面和ECC HTTP客户端QtTest。构建器按严格C++11选项（`-std=c++11 -pedantic-errors`）编译两个测试程序；Windows运行器通过QtTest Makefile中的`QT_TESTCASE_BUILDDIR`恢复编译时测试路径，尊重已有的WSL `Z:`映射，在Windows本机临时目录运行，并设置与产品相同的正常`qwindows`平台插件和Fusion样式。GUIless客户端不传`-style`参数。测试结果：界面97通过、0失败、1项原有POSIX锁平台跳过；ECC客户端63通过、0失败、0跳过；两个实际进程退出码均为0。

当前构建回执SHA256=`c7b5e856d46c452c3ebec7a3f73ad5273093c30918914ccad0568952edca61d0`，列有875个源码、测试、配置和依赖输入摘要；两个测试程序SHA256分别为`85a51833baf53034c015044e31eb2ec5a789e35c140949834078640ca108c2ce`（界面）与`25905480a9b7e9d1ee03f1945179b68b705fe1c0f7835a9df383b52c49b01660`（客户端）。结果摘要SHA256=`499a25293ee6b71f50eb3a061dd5b1ff2fa07ffa0d8ae2f2bf9b1db5ed7a68cc`；原始QtTest日志摘要依次为`d9ea7d69d939ea80f2275a2d4c914cccf0ceb7498df60200a7b03a613a4d5351`和`9b4f427e34ebd2d77c5483ad4c1eac2a979a735fdb044242f223fcfab3d5f7a8`。临时清理回执SHA256=`f27e633d149f8bd6cf7ca8d57c6963c40dc02c5bd3624f50c51f23fefee436e9`，记录测试后无残留`.graw`、总量0字节、Windows临时根已删除。便于离线检查，当前Windows候选`build-packages/getdaq-windows-qt5142-20261020-current-source-candidate/validation/`内保存了规范JSON结果、两份原始QtTest日志和清理回执，并由包清单摘要绑定。104MiB左右的QtTest中间构建随后删除，仅保留产品运行候选、本文证据和下列可复现入口。

```bash
python3 tests/build_windows_qt_validation.py \
  --kit /mnt/d/Software/QT/5.14.2/mingw73_64 \
  --compiler /mnt/d/Software/QT/Tools/mingw730_64 \
  --output build-windows-qt5142-tests
python3 tests/run_windows_qt_validation.py \
  --build-root build-windows-qt5142-tests --only all
```

运行器每个测试最长240秒、Windows临时卷至少保留1GiB空闲、自有临时目录不超过512MiB；运行期间监视该目录中的`.graw`总量不超过20GiB，结束时清理并写回执。实际QtTest负载不是DAQ吞吐基准。本结果验证当前源码测试程序在用户这台Windows环境中的本机功能，不代表独立干净Windows机器、人工完整操作、现场Linux服务、真实板卡、硬件恢复或大流量数据传输验收。对应手册见[Windows功能回归说明](WINDOWS_FUNCTIONAL_VALIDATION.md)。

## 70.84 当前源码候选包文档同步与清单复核（2026-10-20）

发现Windows功能回归手册仍把Ubuntu运行目录指向较早的10月19日控制候选包，与当前已生成的Ubuntu24全量候选不一致。更新[Windows Qt功能回归说明](WINDOWS_FUNCTIONAL_VALIDATION.md)和[Windows Qt部署手册](WINDOWS_QT5142.md)，明确当前Windows操作端与Ubuntu24主机候选的准确路径、各自包含内容和验证边界，并在两平台文档中补充“不重编译程序、仅刷新文档”的操作步骤。

新增`tools/refresh-package-docs.py`，支持对已存在且有效的host/Windows候选同步README与Markdown/PNG手册。工具限定候选位于`build-packages/`、拒绝符号链接，先验原包，再更新包内SHA并重验；同时逐项核对程序、运行库、插件和验证材料SHA完全不变。源码文档更新后运行该工具刷新当前候选：Ubuntu24 `getdaq-ubuntu24-20261020-current-source-candidate`仍为169个清单文件；Windows `getdaq-windows-qt5142-20261020-current-source-candidate`仍有6个程序、264个清单文件。两包校验均通过，产品程序、Windows功能测试二进制和运行依赖未重新构建或改写。

文档本地链接复核为1190项通过。该阶段只改善交付文档一致性和候选包的文档维护，不新增产品功能，也不改变Ubuntu/Windows平台、硬件、真实网络或DAQ吞吐验收范围。

## 70.85 当前Ubuntu24候选双流1GiB接收回归（2026-10-20）

此前工作树宿主的glibc为2.31，不能直接启动Ubuntu24候选所需的glibc 2.38/较新GLIBCXX二进制。扩展`tests/receiver_sustained_load.py`以支持成对指定目标动态加载器和库搜索路径；结果仍绑定候选中的真实`getdaq-receiver` SHA，而不是临时包装器。新增安全回归检查覆盖加载器参数配对以及启动失败后载荷清理；`tests/receiver_load_safety_test.py`五项通过。

通过已验签Ubuntu24用户空间的动态加载器运行当前候选二进制，对localhost并发接收两条512MiB合成TCP流，总输入1GiB（低于20GiB上限）。每流131,072帧、4个128MiB轮转文件。逐文件回读SHA与发送端一致；两流字节/帧计数、自然EOF、receiver正常退出、seal完整/持久均核对通过。候选二进制SHA256=`34511044c27ef96fe04f96b9ca5bb523e17d140b64ed15218bda3ae8cdc658fc`，实际Ubuntu24 loader SHA256=`c20a2dc8917c755f02b94049356320fe1f62ac7d9f8994731f807d9df39302da`。两接收器峰值RSS均21,072KiB；结果、逐文件SHA、receiver日志及清理收据位于`build-receiver/ubuntu24-current-source-load-20261020/receiver-load-m6mvgdd7/`。

清理回执确认8个`.graw`共1,073,741,824字节删除、剩余0文件；本轮自有测试数据已清零。运行在Ubuntu24用户空间+WSL2 Linux 5.15内核，使用localhost合成发送器，数据量未超过主机内存；约0.52秒发送阶段和约1.63秒全程时间受Python生成/哈希、页缓存、WSL存储和调度影响，**不能当作持续磁盘吞吐、DAQ速率、零丢失或真板验收**。真实硬件、生产网卡/文件系统、压力下长时间内存峰值仍待现场环境验证。重现命令和运行器说明见[接收器性能手册](RECEIVER_PERFORMANCE.md#42-当前ubuntu24候选双流回归2026-10-20)。

## 70.86 ECC/SOAP字符串与内嵌XML请求回归（2026-10-20）

旧GET源码`source/get/GetBench-software-release-20210208/src/get/soap/ecc.hh`将`configID`和`table`声明为gSOAP `std::string`；服务实现把收到的字符串内容交给`ConfigId::fromXml`及数据链路解析。新版保留默认SOAP转义字符串和为旧envoy部署保留的`soap_inline_xml`模式。原回归只覆盖默认转义模式，不能证明显式内嵌XML分支仍生成等价参数语义。

新增Qt测试`soapStringAndInlineXmlParameterForms`：对两种`Protocol::EccRequest`分别解析外层SOAP，再解析转义的XML字符串或直接读取内嵌节点；核对三个SubConfigId类型和值（含XML特殊字符）、DataRouter名称/地址/端口/类型及单入口路由一致。还核对两种XML命名空间语义：字符串参数内层XML无命名空间；inline XML子元素继承外层SOAP默认`urn:ecc`。旧版`ConfigId::fromXml`将gSOAP字符串参数传给TinyXML并按元素名查找，因此该形状与旧解析方式相符，但不是服务端真实处理的进程证据。新增`soapNamespacedResponseEnvelopes`覆盖带SOAP Envelope/Body及ECC operation-response前缀的GetState、Describe响应字段提取，以及SOAP Fault内容识别。Qt5.12.8/GCC9.3严格`-pedantic-errors`完整getdaq-tests构建通过；两新用例及既有`soapStringParametersAndTopology`、`protocolValidation`定向运行6/6通过。所有响应均为本地构造的SOAP形状夹具，不是旧gSOAP服务器抓包或进程联调；现场实际部署应选择哪种参数格式、服务端操作顺序及状态读回仍待核对。

本轮Qt测试构建位于`/tmp/getdaq-soap-compat-20261020`，未发送DAQ或生成`.graw`。协议语义说明见[协议手册](PROTOCOLS.md)。

## 70.87 真实OpenSSH localhost归档回环验证（2026-10-20）

为补足此前只通过假`ssh`入口执行协议的验证，新增`tests/run_catalog_ssh_loopback.py`，启动临时、仅监听`127.0.0.1`的OpenSSH服务，并使用系统OpenSSH客户端、临时Ed25519主机/客户端密钥和隔离的`known_hosts`。发送端运行本次C++11构建的`catalog-transfer-tests`与配套`getdaq-catalog`；远端实际接收命令运行Ubuntu24当前源码候选包中的`getdaq-catalog`，经已验证Ubuntu24动态加载器及库启动。该方式实际经过SSH认证、远程命令shell、流式传输和归档接收，而非模拟SSH进程。

真实SSH场景发送2个来源、4个合成`.graw`，总载荷40,960字节；独立核对接收归档完整、内容匹配且来源仍保留。测试日志还记录同一测试程序中的SHA向量与12个原生catalog传输场景通过。回执`build-receiver/ssh-loopback-real-openssh-20261020-final/result.json`绑定runner、C++测试源、宿主测试程序、宿主catalog及Ubuntu24候选远端catalog的SHA-256；回执确认目标仅为`127.0.0.1`、临时sshd已停止、临时密钥已删除、4个raw共40,960字节已清理、残留为0。合成SSH场景自身预算为1MiB，且runner限制累计测试raw不超过用户规定的20GiB。

使用相同源码和相同测试/CLI SHA重新配置构建后，在旧版CTest兼容调用方式下执行`cd /tmp/getdaq-ssh-loopback-build && ctest -R '^catalog-transfer-filesystem$' --output-on-failure`，注册的`catalog-transfer-filesystem`通过1/1（2.73秒），运行其中的12个原生传输场景及假SSH回归。真实OpenSSH回环由独立runner通过显式环境开关运行同一测试二进制；回执所绑定的测试与CLI SHA和本次CTest构建完全一致。两种运行都通过，但真实SSH不是CTest默认环境启动的。重建目录及日志在核对哈希后删除。

这是单台Linux/WSL环境中的localhost SSH进程互通验证，不是两台独立机器、生产账号/密钥、真实防火墙/挂载磁盘、macOS、Windows SSH或现场吞吐验收；它不测DAQ采集、不代表持续带宽或零丢失保证。完整运行方式见runner源码和[跨主机SSH操作手册](CATALOG_TRANSFER.md#12-跨主机发送已完成归档)。

## 70.88 真实OpenSSH有界128MiB归档流回归（2026-10-20）

在70.87的小样本真实SSH验证后，将`tests/catalog_transfer_test.cpp`和`tests/run_catalog_ssh_loopback.py`扩展为可配置合成文件大小与载荷预算。运行器在建立临时服务及写数据前计算准确预期载荷；C++测试又在写文件前核对两个来源的`.graw`大小总和，超过预算或20GiB总上限即拒绝。文件内容比对采用64KiB分块SHA-256并核对文件类型/大小，额外内存不随raw大小线性增长。默认仍为小样本预算；更大运行必须显式指定预算。

本次每个来源生成一个64MiB `data0.graw`和一个4KiB `data1.graw`，传输载荷共134,225,920字节（128MiB加8KiB）；预算精确设为同一数值，远低于20GiB。真实SSH runner以显式环境开关运行同一个`catalog-transfer-tests`二进制：系统OpenSSH客户端连接临时127.0.0.1 sshd，密钥认证后由Ubuntu24当前候选程序接收。逐文件校验、总字节数、完成回执及来源保留全部通过。C++回归还确认来源fixture目录析构后消失；runner检查该目录下无残留fixture，并显式删除远端目标raw。源端4个raw共134,225,920字节、目标端4个raw共134,225,920字节，合计8个真实文件、268,451,840字节已清理，残留为0。另在关闭真实SSH环境开关时，注册CTest `catalog-transfer-filesystem`通过1/1；日志报告其中12个原生传输场景和假SSH回归通过。结果`build-receiver/ssh-loopback-real-openssh-20261020-128m-cleanup-final/result.json`记录runner/测试源/两端程序SHA、传输预算、源/目标清理数和CTest日志SHA。临时密钥、sshd、构建目录及构建日志均清除。将预算设为1字节的拒绝预检也通过，且没有创建证据目录或启动传输。

重现构建与运行（GCC 9.4、CMake、系统`ssh`/`sshd`/`ssh-keygen`/`ssh-keyscan`）：

```bash
cmake -S apps/datasupervisor -B /tmp/getdaq-ssh-large-build \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DGETDAQ_BUILD_SUPERVISOR_SERVICE=OFF
cmake --build /tmp/getdaq-ssh-large-build \
  --target catalog-transfer-tests getdaq-catalog --parallel 2
(cd /tmp/getdaq-ssh-large-build && \
  ctest -R '^catalog-transfer-filesystem$' --output-on-failure)

python3 tests/run_catalog_ssh_loopback.py \
  --test /tmp/getdaq-ssh-large-build/catalog-transfer-tests \
  --catalog /tmp/getdaq-ssh-large-build/getdaq-catalog \
  --remote-catalog build-packages/getdaq-ubuntu24-20261020-current-source-candidate/bin/getdaq-catalog \
  --loader build-ubuntu24-validation/rootfs/usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 \
  --library-path build-ubuntu24-validation/rootfs/lib/x86_64-linux-gnu:build-ubuntu24-validation/rootfs/usr/lib/x86_64-linux-gnu \
  --large-file-bytes 67108864 --payload-budget-bytes 134225920 \
  --evidence-dir build-receiver/ssh-loopback-real-openssh-20261020-128m-cleanup-final
```

该测试运行于单台Linux/WSL本机内核和localhost网络；它验证有界流式归档与真实SSH进程互通，不是双机网络/磁盘持续吞吐、独立Ubuntu24、macOS/Windows或硬件DAQ验收。

## 70.89 SOAP 1.1 响应结构校验收紧（2026-10-20）

旧版 `getEccSoapServer.cpp` 通过 gSOAP SOAP 服务发布 ECC 操作。此前客户端解析器只在任意 XML 深处寻找 `ErrorCode`；一个没有 SOAP Envelope/Body 或操作响应包装、却带有 `ErrorCode=0` 的 XML 也可能被当成成功。现要求 SOAP 1.1 Envelope 及其直接 Body 使用标准 SOAP 1.1 namespace，Body 中必须直接包含本地标签名以 `Response` 结尾的操作包装，或标准 SOAP Fault。Envelope/Body 的前缀差异可接受；操作包装的 namespace 前缀和 URI不强制，以兼容旧 gSOAP 的命名差异。成功字段只从该响应包装内提取；裸 XML、错误Envelope namespace、缺失响应包装、重复字段和DTD均拒绝。测试用本机假ECC服务现在也返回完整Envelope/Body/操作响应包装，不再使用裸`Reply`。

在 Qt 5.12.8/GCC 9.3、C++11 下从当前源码完整构建 `tests/getdaq-tests` 成功。定向运行 `protocolValidation`、`soapNamespacedResponseEnvelopes`、`soapStringAndInlineXmlParameterForms`、`singleCoboLifecycle`、`simulatedLifecycle`，5个指定用例通过（含初始化/清理共7项，0失败）。最初受限环境运行完整套件为29通过/71失败；直接检查确认沙箱禁止loopback监听。随后将相同测试二进制放在允许本机loopback的执行环境运行，并将Qt测试缓存指向临时目录，最终完整结果为**100通过、0失败、0跳过**，总时间51.502秒。覆盖完整界面/配置、SOAP请求响应、sentry HTTP及模拟器网络流程；其中网络端点仍是本机测试服务，不是旧gSOAP服务或真实硬件。报告与日志保存在`build-receiver/qt-suite-20261020-100pass/result.json`和`full-test.log`；测试程序SHA256=`f3c1c6786187968f2c7beaee35028debdf0feae960afbcdf61d113a572ff4d5d`，日志SHA256=`73b09d50056edac925e33f29009e58b7dabe71716948a95d7cb1bb1d8346f54b`。临时构建目录`/tmp/getdaq-qt-suite-rerun`之后清理；测试使用的临时目录自动移除，工作区和`/tmp/getdaq-tests-*`扫描无残留`.graw`。本轮未运行大流量DAQ负载。

本变化只强化SOAP响应判定，不构成对旧gSOAP服务器的真实进程兼容证明。旧服务抓包/进程互通、独立平台和硬件验收仍待执行。

## 70.90 当前源码Windows Qt回归与候选包更新（2026-10-20）

为避免70.89 SOAP解析器变更只进入源码、而Windows候选仍带旧程序，本轮使用用户安装的Qt 5.14.2 MinGW套件重新构建六个操作端程序，并用新运行目录替换旧候选。源代码按C++11编译。旧目录先保留到新目录部署成功、manifest验证通过，再执行目录切换；最终候选路径为`build-packages/getdaq-windows-qt5142-20261020-current-source-candidate`。

Qt自动回归结果：Widgets/配置完整测试99通过、0失败、1跳过；跳过项是仅适用于POSIX文件锁的场景。ECC客户端63通过、0失败、0跳过。SOAP响应Envelope/Body/操作包装测试及负例通过。两套运行均使用构建出的Windows测试程序，Windows测试报告为`build-receiver/windows-qt-suite-20261020-current-source/summary.json`，原始输出及清理回执同目录；测试临时目录已移除，回执记录raw剩余0文件、0字节。

产品重建包含GetDAQ、getdaq-sentry、getdaq-simulator、getdaq-config、getdaq-config-check和getdaq-config-upload。新候选含260个经SHA-256绑定的文件。打包后用SDK不在`PATH`中的环境逐个执行控制工具帮助检查，并通过offscreen与Windows原生QPA初始窗口短暂启动；随后独立验证运行目录所有文件和六个程序哈希，均通过。产品构建和部署摘要（含manifest SHA-256）在`build-receiver/windows-qt-suite-20261020-current-source/runtime-deployment-summary.json`。候选包内README、操作文档和配置样例随后与当前源码同步并重新计算manifest；临时编译/部署目录已删除，候选保留供用户检查。

这组结果确认的是用户Windows主机上Qt 5.14.2 MinGW下的自动化回归、产品编译和本地运行目录完整性；1项POSIX锁场景在Windows被跳过。它不代表独立Windows机器安装、人工完整操作、旧gSOAP服务器互通、板卡DAQ或持续高负载验收。跨主机/硬件数据流仍需现场条件验证。

## 70.91 旧GetBench gSOAP操作命名空间回归（2026-10-20）

复核历史源码`source/get/GetBench-software-release-20210208/src/get/soap/getEccSoapServer.cpp`后，确认运行时`Namespace namespaces[]`将`ecc`操作前缀映射到`urn:http://localhost:8083/`；同一源码树`ecc.hh`中的生成注释写`urn:ecc`，但服务器文件的显式命名空间表才是本测试所依据的运行配置。于是`soapNamespacedResponseEnvelopes`新增GetState响应夹具，覆盖SOAP 1.1 Envelope/Body、`ecc:GetStateResponse`及旧服务URI，并核对State和Transition解析结果。

Qt 5.12.8/GCC 9.3/C++11下重新构建完整Qt测试二进制后运行该定向测试，初始化、目标用例及清理共3项通过，0失败、0跳过。证据为`build-receiver/legacy-gsoap-namespace-20261020/result.json`和`test.log`。此处按历史服务器源码复现协议结构，未启动gSOAP服务器，也未对实际gSOAP序列化输出抓包；仍不能替代真实旧服务器互通验收。

## 70.92 旧GetBench SOAP请求QName兼容（2026-10-20）

进一步对照归档客户端与服务端的`Namespace namespaces[]`，二者都将`ecc`映射到`urn:http://localhost:8083/`，而当前控制台此前将请求Body操作放在`urn:ecc`。同时，历史接口`ecc.hh`没有声明`schema elementForm: qualified`；gSOAP默认将局部参数元素生成为unqualified，因此旧接口请求形状是命名空间限定的`ecc:Operation`包装，加上无命名空间的`configID`/`table`参数及其内嵌字符串XML。gSOAP namespace table用于解析和生成运行时前缀/URI，elementForm默认值见[官方namespace table说明](https://www.genivia.com/doc/guide/html/group__group__namespace.html)及[官方用户指南](https://www.genivia.com/doc/guide/html/index.html)。

`Protocol::EccRequest`现按该旧接口映射输出操作包装与无命名空间参数。测试HTTP假服务现在也用旧服务URI返回`ecc:*Response`，并增加请求QName/参数无命名空间断言；仅匹配本地元素名、未核对命名空间的假服务此前掩盖了这一差异。完整Qt套件在Ubuntu20.04/Qt5.12.8/GCC9.3/C++11下100通过、0失败、0跳过（51.815秒）；用户Windows Qt5.14.2 MinGW当前源码回归为Widgets 99通过、1项POSIX锁场景跳过，以及ECC客户端63通过，均0失败。Linux结果在`build-receiver/legacy-gsoap-namespace-20261020/full-suite-result.json`，Windows结果在`build-receiver/windows-qt-suite-20261020-current-source/legacy-soap-windows-regression.json`。六个Windows产品程序随后按修正后的源码重建并替换候选包，260文件manifest、SDK-free帮助检查与Windows原生初始窗口smoke均通过；当前回执见`build-receiver/windows-qt-suite-20261020-current-source/runtime-deployment-summary.json`。

这是按旧gSOAP源码配置和官方序列化默认规则构造的本机回归，不是旧gSOAP服务实际运行/抓包；仍须现场服务互通、真实板卡以及主机网络验收。

## 70.94 原生失败节点交接补齐请求生成器（2026-10-20）

在70.93的节点与父端补齐路径上，加入`getdaq-supervisor-query build-handoff-completion`。操作员提供原节点请求文件、原失败操作ID和一个不同的新操作ID；命令先GET服务identity、原operation与当前status，核对请求文件字节与规范编码完全一致，并核对规范原`handoff-control`请求键、当前服务实例/计划/控制者/配置/工件、原控制范围与epoch、原操作明确failed、服务无活动操作、父失败控制门槛仍保持，以及原操作保存的正常Stopped周期和当前生命周期逐字段相同。只有全部通过才将原请求的command/operation ID/失败引用转换为`complete-control-handoff`规范请求并打印至stdout。它只读，不发送POST，也不接受改写原接班范围或持有期的选项；正常单次提交仍使用原有`submit`命令，后续用`operation`观察新ID。

`tests/run_failed_handoff_completion_tests.py`不再手写补齐JSON：测试先篡改原successor epoch，再把原请求改成格式等价但非规范字节，两者都被拒绝且服务操作目录未创建；规范原请求则成功生成精确补齐请求，按既有CLI提交一次并核对完成证明、原failed历史、旧owner拒绝、新owner接受与服务冷重启后保持门槛。配合`failed-handoff-parent-completion`，验证父ECC不POST节点补齐、只读核对操作者结果、写入精确父回执并由successor认领后启动新周期。最终组合CTest **2/2通过**（节点服务1.87秒、父端双Ice编码10.16秒，合计12.03秒）。

Ubuntu20.04.6/WSL2、GCC9.4.0、严格C++11下构建监督服务、查询CLI、ECC服务、父端集成夹具、故障注入器及接收器。测试仅用本机localhost合成控制和接收进程；节点runner的raw总量硬上限64MiB，父端runner同样限制64MiB。最终复跑共删除73,728字节合成raw，runner确认余量为0；临时目录扫描无自有`.graw`/`.raw`。小型结果收据`build-receiver/failed-handoff-recovery-20261020/result.json`绑定测试程序、runner和修改源码摘要。此结果不是Ubuntu24/macOS、独立数据主机、现场板卡或持续DAQ吞吐验收；具体人工操作及限制见[失败节点交接补齐手册](FAILED_CONTROL_HANDOFF_COMPLETION.md)。临时构建目录`/tmp/getdaq-handoff-parent`和`/tmp/getdaq-receiver-native`完成回归后删除。

本轮先因节点CLI请求生成器需要Status字段`operation_id`而发现状态严格字段列表遗漏该字段，已补充并要求它等于原失败ID；原父端测试也发现既有未补齐回执不含补齐元数据，解析器现允许两字段同时缺席或同时为空，但不允许只出现其中一个字段，修复后的节点服务与父端集成均通过。

## 70.93 原生数据面 Linux 回归复核（2026-10-20）

按`.github/workflows/native-data-path.yml`中的Linux数据面目标，在Ubuntu 20.04.6/WSL2、GCC 9.4.0、CMake 3.16.3下从当前源码重建receiver和catalog/supervisor测试目标。内嵌依赖检查通过；`receiver-core`与`exporter-lock` 2/2、目录/catalog传输/控制协议codec 3/3通过。真实TCP集成覆盖分片与合并MFM帧、字节序、两个导出观察者、精确落盘与seal、慢导出端隔离和错误尾帧；最大合成raw 32 MiB，全部夹具成功清理。目录传输测试另在不同文件系统间验证SHA与中断/冲突恢复；它使用小型夹具，用户规定的所有原始流累计20 GiB上限未接近。

小型日志与结果摘要保存在`build-receiver/native-data-ci-20261020/`。599 MiB临时构建目录已删除；回归后没有自有`.graw`残留。该结果仅是Ubuntu20 WSL本机复核，不能替代工作流声明的Ubuntu24/macOS runner，也不是物理多机、板端或持续吞吐验收；sentry归档集成因本轮未提供sentry二进制而跳过。

## 70.95 Ubuntu24交付候选刷新失败交接查询CLI（2026-10-20）

70.94增加的`getdaq-supervisor-query build-handoff-completion`此前只在源码构建中，Ubuntu24交付候选仍含旧查询程序。本轮使用Ubuntu20.04.6/WSL2、GCC9.4.0、CMake3.16.3、C++11重新构建`getdaq-supervisor-query`、监督服务和独立receiver；查询程序及监督服务使用严格`-Wall -Wextra -Wpedantic`项目选项。先以Ubuntu24.04.5 rootfs的glibc2.39动态加载器直接执行新程序`--help`，确认25程序候选目标用户空间可加载并显示新命令。

随后以本机localhost合成服务运行`tests/run_failed_handoff_completion_tests.py`：节点原操作明确失败、改写successor的请求及非规范原请求被拒绝，规范请求生成阶段无POST，操作者显式提交后证明与新owner范围匹配，旧owner拒绝、新owner可观察，冷启动仍保持控制隔离。测试程序记录本组最大合成raw预算64MiB；实际生成并在脚本结束时删除8192字节，`raw_remaining=0`。完整回执被复制进交付候选`validation/failed-handoff-completion-service.json`，增量构建/运行时摘要位于`validation/program-refresh-getdaq-supervisor-query.json`。

`tools/refresh-ubuntu24-program.py`先核对原候选清单和测试结果与构建程序SHA的绑定，在同目录临时副本替换一个程序、剥离交付副本调试符号，并使用Ubuntu24动态加载器执行依赖及`--help`检查。新包完整性检查通过，25个程序记录仍齐全；其中本轮只重新构建查询CLI，其余24个程序沿用原完整候选，不能描述成25个程序全部由70.95重新编译。之后`tools/refresh-package-docs.py`把当前README和docs同步至包内，再次验证172个清单文件，确认文档刷新没有改变程序或其他运行时文件。候选路径保持`build-packages/getdaq-ubuntu24-20261020-current-source-candidate/`。

复现构建与测试（仅小型本机合成raw；测试runner会删除）：

```bash
cmake -S apps/datareceiver -B /tmp/getdaq-active-receiver \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build /tmp/getdaq-active-receiver --parallel 2 --target getdaq-receiver
cmake -S apps/datasupervisor -B /tmp/getdaq-active-supervisor \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DGETDAQ_BUILD_SUPERVISOR_SERVICE=ON
cmake --build /tmp/getdaq-active-supervisor --parallel 2 \
  --target getdaq-supervisor-query getdaq-supervisor
g++ -std=c++11 -Wall -Wextra -Wpedantic -fPIC -shared \
  tests/handoff_fault_injector.cpp -ldl \
  -o /tmp/getdaq-active-supervisor/handoff-fault-injector.so
python3 tests/run_failed_handoff_completion_tests.py \
  --service /tmp/getdaq-active-supervisor/getdaq-supervisor \
  --receiver /tmp/getdaq-active-receiver/getdaq-receiver \
  --client /tmp/getdaq-active-supervisor/getdaq-supervisor-query \
  --injector /tmp/getdaq-active-supervisor/handoff-fault-injector.so \
  --evidence /tmp/getdaq-active-supervisor/evidence
python3 tools/refresh-ubuntu24-program.py \
  --candidate build-packages/getdaq-ubuntu24-20261020-current-source-candidate \
  --program getdaq-supervisor-query \
  --binary /tmp/getdaq-active-supervisor/getdaq-supervisor-query \
  --test-result /tmp/getdaq-active-supervisor/evidence/<run>/result.json
python3 tools/refresh-package-docs.py --kind host \
  --candidate build-packages/getdaq-ubuntu24-20261020-current-source-candidate
python3 tools/build-suite.py --verify-stage \
  build-packages/getdaq-ubuntu24-20261020-current-source-candidate
```

本机socket测试在受限环境需允许localhost loopback；它不访问CoBo、真实ECC或远程数据主机。Ubuntu24 rootfs加载检查仍使用WSL宿主内核，不等于独立Ubuntu24安装验收；本次也没有运行macOS、真实双机SSH或持续DAQ吞吐。

## 70.96 归档传输增加认证 TCP/IP 通道（2026-10-20）

新增独立原生TCP发送/监听CLI，用于采集结束后传输已封存归档；不替代实时DAQ接收器，不让Qt/ECC承载数据。连接使用HMAC-SHA-256双向随机挑战应答及独立0600凭据，归档字节本身仍明文。旧SSH入口保留以兼容，文档标明新路径并解释密钥/网络边界。SSH/TCP新版接收端共用目标根目录锁，避免两种传输同时写入同一归档根。

Ubuntu20.04.6/WSL2、GCC9.4.0、C++11构建`getdaq-catalog`、`system-experiment-tests`和`catalog-transfer-tests`通过。HMAC标准RFC向量、常量时间比较测试通过；本机真实TCP回环依次验证错误密钥在创建目标前拒绝、发送第一份小文件后主动断线、复用同一partial显式续传缺失部分、24,576字节小型合成归档接收端回读验证、已完成归档续传不重发载荷。原本地目录复制/续传、冲突、不完整写入与源/前缀损坏拒绝用例也通过。测试fixture退出后清理自有文件；最大TCP载荷24,576字节，低于20GiB总上限。CLI `--help`正常。

测试需允许本机localhost socket；受限沙箱内`bind/listen`被系统禁止，放行本机回环后同一测试通过。没有进行两台独立主机、防火墙、TLS/加密、Ubuntu24/macOS实机或持续吞吐验收；不能据此宣称跨主机部署已验收。复现：

```bash
cmake -S apps/datasupervisor -B /tmp/getdaq-auth-tcp-build \
  -DBUILD_TESTING=ON -DGETDAQ_RECEIVER_EXECUTABLE=/tmp/no-receiver \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/getdaq-auth-tcp-build --parallel 2 \
  --target getdaq-catalog system-experiment-tests catalog-transfer-tests
/tmp/getdaq-auth-tcp-build/experiment/system-experiment-tests
/tmp/getdaq-auth-tcp-build/catalog-transfer-tests \
  /tmp/getdaq-auth-tcp-build/getdaq-catalog
```

## 70.97 Ubuntu24候选加入认证TCP归档CLI（2026-10-20）

70.96的新TCP代码原先只在源码树，Ubuntu24完整候选仍是旧catalog程序。本轮在Ubuntu20.04.6/WSL2、GCC9.4、C++11从当前源码构建`getdaq-catalog`、`system-experiment-tests`和`catalog-transfer-tests`。新增`tests/run_catalog_authenticated_tcp_tests.py`为候选更新生成小型SHA绑定回执，并把整个catalog测试套件的合成raw总量设64MiB上限，预先清理专用变量并使用私有/tmp与/dev/shm测试根。

测试进程通过Ubuntu24 glibc2.39动态加载器运行在WSL宿主内核，HMAC RFC向量及完整catalog传输回归通过。真实localhost TCP验证错密钥不创建目标、首文件后断开、同一partial显式续传、接收端全量回读及完整目标续传无载荷；全套catalog fixture产生并清理15,196,160字节，payload上限64MiB、raw_remaining=0、私有测试根为空。回执记录在候选`validation/catalog-authenticated-tcp.json`。

`tools/refresh-ubuntu24-program.py`经扩展后只允许查询CLI和catalog各自对应的严格回归回执；包内证据同时绑定源码构建SHA和剥离后交付SHA。首次完整包复核发现历史manifest其余24程序路径仍指向旧构建机绝对路径；确认25个程序实际都在候选`bin/`后，刷新器把全部程序路径规范成候选相对路径，解决目标机不可复现路径，同时依文件SHA保持其他程序原字节。`tests/package_ubuntu24_catalog_ssh_test.py`覆盖新认证TCP回执匹配/不匹配，11/11通过。

候选`build-packages/getdaq-ubuntu24-20261020-current-source-candidate/`之后由打包器逐项用Ubuntu24 loader检查25/25程序及依赖，catalog和receiver帮助检查、回归收据绑定均通过；构建套件包清单复核通过。随后候选文档同步器刷新操作手册与状态文档，产品程序/运行库保持不变。最终候选有25个程序、174个manifest文件；本阶段仅重建catalog，24个程序从原全量候选复用。`getdaq-catalog`交付SHA256为`e3f2f2ec0ea1e6e7b6d2f1e17bfef9f024e4dfe3215e04691a23a107998e5f9b`；当前manifest摘要可从候选包manifest直接复核。候选回执不等于独立Ubuntu24机器、两机网络、防火墙、现场数据卷、macOS、真板或持续DAQ吞吐验收；完整25程序也未在本阶段从源码重建。

复现刷新（需要本机允许localhost TCP；Ubuntu24加载器/库路径以本机验签rootfs实际位置为准）：

```bash
cmake -S apps/datasupervisor -B /tmp/getdaq-catalog-refresh-build \
  -DBUILD_TESTING=ON -DGETDAQ_RECEIVER_EXECUTABLE=/tmp/no-receiver \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/getdaq-catalog-refresh-build --parallel 2 \
  --target getdaq-catalog system-experiment-tests catalog-transfer-tests
python3 tests/run_catalog_authenticated_tcp_tests.py \
  --loader build-ubuntu24-validation/rootfs/usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 \
  --library-path build-ubuntu24-validation/rootfs/lib/x86_64-linux-gnu:build-ubuntu24-validation/rootfs/lib:build-ubuntu24-validation/rootfs/usr/lib/x86_64-linux-gnu:build-ubuntu24-validation/rootfs/usr/lib:build-ubuntu24-validation/rootfs/usr/local/lib \
  --catalog /tmp/getdaq-catalog-refresh-build/getdaq-catalog \
  --system-tests /tmp/getdaq-catalog-refresh-build/experiment/system-experiment-tests \
  --transfer-tests /tmp/getdaq-catalog-refresh-build/catalog-transfer-tests \
  --evidence /tmp/getdaq-catalog-refresh-build/catalog-authenticated-tcp-result.json \
  --candidate build-packages/getdaq-ubuntu24-20261020-current-source-candidate
python3 tools/refresh-ubuntu24-program.py \
  --candidate build-packages/getdaq-ubuntu24-20261020-current-source-candidate \
  --program getdaq-catalog \
  --binary /tmp/getdaq-catalog-refresh-build/getdaq-catalog \
  --test-result /tmp/getdaq-catalog-refresh-build/catalog-authenticated-tcp-result.json
python3 tools/refresh-package-docs.py --kind host \
  --candidate build-packages/getdaq-ubuntu24-20261020-current-source-candidate
python3 tools/package_ubuntu24_catalog_ssh.py --verify \
  --candidate build-packages/getdaq-ubuntu24-20261020-current-source-candidate
python3 tools/build-suite.py --verify-stage \
  build-packages/getdaq-ubuntu24-20261020-current-source-candidate
```

## 70.98 认证TCP拒绝错误密钥续传且保持partial不变（2026-10-20）

70.97覆盖错误密钥不能创建新归档，但没有显式证明错误密钥尝试续传时不会触碰已有partial。本轮在`tests/catalog_transfer_test.cpp`扩展真实localhost TCP回归：先用正确凭据制造一个中断partial，记录其传输意图和源请求元数据；使用错误凭据执行显式resume必须失败，并核对两份元数据字节保持不变；随后仍以正确凭据完成同一partial续传、接收端读回验证及完成归档零载荷复续传。更新`tests/run_catalog_authenticated_tcp_tests.py`，要求输出包含此情形，并把它作为独立布尔值写入回执。

GCC9.4/C++11下`getdaq-catalog`、`system-experiment-tests`与`catalog-transfer-tests`构建通过。认证TCP完整runner通过Ubuntu24 glibc2.39用户空间loader运行（WSL宿主Linux内核、本机loopback），新增场景、既有HMAC RFC向量及catalog回归均通过；生成并删除15,196,160字节raw，最大预设预算64MiB，`raw_remaining=0`且私有测试根为空。回执SHA绑定catalog、测试程序及runner源码。此次仅强化测试，不改变生产传输实现或70.97交付候选二进制；需后续产品代码改动时再刷新候选。仍未做两台独立主机、防火墙、独立Ubuntu24/macOS实机、真实板端或持续吞吐验收。

## 70.99 错误密钥续传不修改接收端raw前缀（2026-10-20）

复核70.98后发现，中断场景留下了intent/request元数据，但还没有一个确定存在的接收端raw前缀，因此不能据此声称错误凭据不会改变已有partial payload。本轮加强`AuthenticatedTcpLoopback`夹具：在受控partial中创建与封存源对应的`source_0/data0.graw`半文件前缀及匹配seal，然后以错误凭据尝试显式resume；核对文件长度与SHA-256、intent/request原字节均保持不变。接着用正确凭据完成同一partial续传，并独立验证接收端完成归档和完成后续传零payload。runner要求回归输出中出现`raw-prefix mutation`，回执新增`wrong_credential_resume_rejected_without_partial_raw_mutation`字段。

GCC9.4/C++11构建通过；完整runner在Ubuntu24 glibc2.39用户空间loader、WSL宿主内核及localhost socket下通过。完整catalog相关回归生成并删除15,200,256字节合成raw，预算上限64MiB，`raw_remaining=0`，私有`/tmp`与`/dev/shm`根为空。回执绑定catalog、系统配置测试、传输测试和runner SHA。本次仅改变测试/证据，不改生产传输程序，因此无需重建候选二进制；仍不代表两机、防火墙、独立Ubuntu24/macOS、真实硬件或持续DAQ吞吐验收。

## 71.00 CTest要求认证TCP续传场景必须实际运行（2026-10-20）

复核macOS/Linux原生数据工作流时发现，`catalog-transfer-filesystem`此前只看测试程序退出码；认证TCP回归若意外从测试目标移除，CI仍可能通过。现为该CTest添加`PASS_REGULAR_EXPRESSION`，要求输出明确包含错误密钥新传输/续传拒绝且partial元数据与raw前缀不变的场景标记。更新macOS构建手册，说明该断言以及仅在`macos-14` Actions任务真实成功后才算Mac CI验收。

GCC 9.4、严格C++11配置与构建`getdaq-catalog`及`catalog-transfer-tests`通过。受限执行首次运行因禁止localhost bind/listen未执行完回归；在获准的本机loopback权限下重跑，CTest 1/1通过且记录`Required regular expression found`。用例打印并核验15,200,256字节合成raw，夹具报告`owned raw cleaned`；独立临时构建目录约14 MiB，验证后删除。随后同步当前操作文档到Ubuntu24候选（174文件）与Windows候选（6程序/261文件），两包刷新前后验证均通过，产品程序及运行库保持不变。此为WSL/Linux本机测试，不代表GitHub Actions已经运行，也不是macOS、独立Ubuntu24、双主机/防火墙、现场DAQ或持续吞吐验收；这些边界保持未完成。

## 71.01 原生CI加入独立sentry归档联调（2026-10-20）

原生数据工作流此前调用`tests/receiver_integration.py`时没有传入sentry二进制，因此脚本跳过了跨进程归档场景。现Ubuntu runner安装Qt5开发包，在隔离的`build-platform/sentry-bin`构建低频sentry，并把它传给接收器集成脚本；接收器和批量归档程序保持原生无Qt构建。macOS矩阵仍不安装Qt，只运行原生接收/归档数据路径。该工作流同时保留71.00认证TCP输出标记断言。

当前环境Ubuntu20.04.6/WSL2、GCC9.4、系统Qt5.12.8下重建接收器、归档/传输/协议测试及独立sentry进程。接收器CTest 2/2、监督/归档CTest 3/3通过；认证TCP CTest要求的场景标记被发现。真实TCP接收集成通过分片、字节序、慢观察端、精确32MiB文件哈希、封存、错误尾帧拒绝；带独立sentry参数的复跑另通过活动归档拒绝、离线检查器、文件改动拒绝及只归档一次。所有raw均由测试夹具删除；最大单个接收raw为32MiB，总量远低于用户20GiB上限。Qt sentry用显式`GETDAQ_DESTDIR`隔离构建输出；源码树内已有`bin/getdaq-sentry`由默认qmake DESTDIR构建覆盖重编。测试后独立构建目录删除；工作流YAML解析通过。GitHub Actions本身、macOS runner、独立Ubuntu24、真实双主机/防火墙、现场DAQ及持续吞吐仍未验证。

## 71.02 原生数据工具动态依赖禁止Qt（2026-10-20）

71.01在Linux CI中为低频sentry另装Qt后，系统存在Qt库可能掩盖原生DAQ工具误链接Qt的风险。新增`tools/check-no-qt-runtime.py`：Linux解析`ldd`，macOS解析`otool -L`，检查实际动态依赖；工作流把`getdaq-receiver`、`getdaq-inspect`、`getdaq-catalog`与`getdaq-supervisor-query`都作为必查程序，并确保query目标确实构建。这个断言补强“Qt只用于低频操作/监控、数据接收和批量归档独立于Qt”的架构边界。

Ubuntu20.04.6/WSL2、GCC9.4/C++11且宿主已装Qt5.12.8环境中，当前源码重建四个程序后检查全部报告无Qt依赖；同一检查器检查Qt链接的`bin/getdaq-sentry`，准确发现`libQt5Network.so.5`和`libQt5Core.so.5`并拒绝，证明负例有效。工作流Linux步骤也会要求此负例被拒绝。对macOS`otool -L`解析器使用模拟输出验证系统库通过、QtCore.framework被检测；这不替代真实Mac执行。Python语法检查通过。临时构建目录和本轮pyc已清理。当前文档再次同步Ubuntu24候选（174文件）和Windows候选（6程序/261文件），刷新前后验证均通过且程序/运行库未变。YAML本机可解析但未触发GitHub Actions；macOS分支未实机运行，不能宣称目标平台验证完成。71.01已记录的本机集成回归和候选包边界仍适用。

## 71.03 认证 TCP 实际 CLI 双进程端到端回归（2026-10-20）

此前认证 TCP 的完整场景由测试代码直接调用传输 API；本轮补上用户实际使用的命令行路径，要求测试程序接收 `getdaq-catalog` 路径并启动独立子进程。先执行 `--generate-transfer-token`，检查凭据文件权限为0600、格式完整，且CLI输出不泄露密钥；再以实际 `--listen-catalog-tcp 127.0.0.1 0 ... --once` 启动接收进程，解析其就绪端口，随后由另一独立发送进程执行 `--send-catalog-tcp`。两端结束后，测试读取接收端完整归档并与发送回执核对，确认来源仍在。

GCC 9.4、严格C++11下构建`getdaq-catalog`、`catalog-transfer-tests`及`system-experiment-tests`通过。`catalog-transfer-filesystem` CTest 1/1通过，断言输出必须包含CLI双进程场景标记。完整runner在Ubuntu24 glibc2.39用户空间loader及WSL/Linux宿主内核下通过，HMAC向量、错误凭据与续传安全、断点续传、回读和CLI实际发送均通过；生成并删除15,224,832字节合成raw，预算64MiB，`raw_remaining=0`且runner拥有的`/tmp`与`/dev/shm`根目录为空。回执为候选`validation/catalog-authenticated-tcp-cli.json`。测试需要本机loopback bind/listen权限；这是单机两个进程的命令行集成，不是独立双主机、现场防火墙/凭据布置、独立Ubuntu24安装、macOS、真实硬件或持续吞吐验收。传输内容仍是明文，HMAC只做认证。


## 71.04 实际TCP CLI错误凭据拒绝与继续服务（2026-10-20）

71.03已覆盖真实命令行的凭据生成和成功发送，本轮再验证CLI层的失败路径：启动真实`getdaq-catalog --listen-catalog-tcp ... --once`进程后，先由另一真实CLI进程使用格式正确但错误的共享密钥发送。发送CLI必须以非零状态退出，并输出`TCP transfer authentication rejected`；测试确认接收根目录没有最终归档或`.partial`。同一监听进程随后接受正确凭据的CLI发送，完成后端回执与目标全量读回一致，来源保持不变。这同时覆盖监听器拒绝未认证连接后继续服务的真实进程行为。

GCC9.4/严格C++11构建通过，认证TCP CTest 1/1通过，且CTest输出断言要求实际出现CLI错密钥拒绝、无目标和随后成功发送标记。Ubuntu24 glibc2.39用户空间loader（运行于WSL宿主内核）的完整runner通过；共生成并清理15,224,832字节raw，预算64MiB，`raw_remaining=0`，runner的`/tmp`与`/dev/shm`私有根为空。更新回执位于Ubuntu24候选`validation/catalog-authenticated-tcp-cli.json`。这是单机loopback协议/CLI验证，不是独立双机、防火墙、现场密钥管理、独立Ubuntu24/macOS、硬件或持续吞吐验收；TCP载荷仍是明文，HMAC只做认证。


## 71.05 归档 TCP 客户端连接阶段设置总时限（2026-10-20）

归档发送端此前在地址解析后调用阻塞式`connect()`；对无响应的远端地址，用户只能等待操作系统TCP重试周期结束。现改为非阻塞连接配合`poll()`和单一15秒总截止时间，按`getaddrinfo`返回的地址顺序尝试，并用`SO_ERROR`确认真正的连接结果；成功后恢复阻塞模式，再设置现有60秒socket读写空闲超时。该15秒约束socket连接阶段及多地址尝试总时间，不包含系统`getaddrinfo`名称解析时间；这是采集后归档CLI行为，不进入DAQ接收数据面。

新增真实CLI关闭loopback端口场景：要求命令在5秒内以清楚的连接失败信息返回，同时保持监听/成功发送/错误密钥拒绝的已有真实进程回归。GCC9.4、严格C++11构建通过，`catalog-transfer-filesystem` CTest 1/1通过；Ubuntu24 glibc2.39用户空间loader及WSL宿主内核完整回归通过，生成并删除15,224,832字节raw（预算64MiB、`raw_remaining=0`、临时根为空）。`getdaq-catalog`已通过`tools/refresh-ubuntu24-program.py`刷新到当前Ubuntu24候选，其余24个程序未重建；25个程序loader/依赖和包内回归收据、175文件清单验证通过，交付catalog SHA-256为`d9b7eb40040f12fcf288487d787cf7585a941e4cb8af430a32d81a84e44e3388`。收据`validation/program-refresh-getdaq-catalog.json`记录刷新，相关运行结果见候选`validation/catalog-authenticated-tcp.json`。此回归验证的是关闭端口快速拒绝和正常loopback连接；没有等待黑洞网络实测完整15秒分支，也未验证两台独立主机、现场DNS/防火墙、独立Ubuntu24/macOS、真实硬件或持续吞吐。


## 71.06 当前源码全套主机程序重建及候选包复核（2026-10-20）

在Ubuntu20.04.6/WSL2主机上，使用统一`tools/build-suite.py --profile full`入口从当前源码重新构建完整主机套件。嵌入依赖105个文件校验通过，构建回执状态为`complete`，输出25个程序。另将完整套件暂存到全新目录，168个文件通过`--verify-stage`。这是对当前源码可构建性和交付清单完整性的复核；没有把当前主机的产物替换进Ubuntu24或Windows候选。

随后只读检查既有Ubuntu24候选：25程序/175文件清单通过`--verify-stage`，其中原生`getdaq-catalog`通过无Qt运行时依赖检查。既有Windows Qt5.14.2 MinGW候选的6个程序/261文件通过运行库和包完整性检查。本轮没有生成DAQ原始数据；临时全量构建目录和168文件暂存包在核验后删除。

复核命令：

```bash
python3 tools/build-suite.py --profile full --build-root /tmp/getdaq-current-full --jobs 4
python3 tools/build-suite.py --stage /tmp/getdaq-full-stage --build-root /tmp/getdaq-current-full
python3 tools/build-suite.py --verify-stage /tmp/getdaq-full-stage
python3 tools/build-suite.py --verify-stage build-packages/getdaq-ubuntu24-20261020-current-source-candidate
python3 tools/deploy-windows-from-wsl.py --verify build-packages/getdaq-windows-qt5142-20261020-current-source-candidate
python3 tools/check-no-qt-runtime.py --binary build-packages/getdaq-ubuntu24-20261020-current-source-candidate/bin/getdaq-catalog
```

本记录不证明独立Ubuntu24/Windows/macOS机器运行、真实板端互通、双机TCP传输、防火墙/目标卷行为、人工完整GUI流程或持续高速采集；整套重构仍未完成。此次复核未更改候选程序或其清单。

## 71.07 旧配置数据库依赖与功能入口核对（2026-10-20）

为决定哪些旧配置组件属于必须重构范围，本轮检查冻结源码中GetBench数据库构建开关、管理器和ConfigWidgets界面动作。`configure.ac`将`enable-database-support`默认设为`no`，显式启用时仍要求外部`ConfigDatabase` pkg-config包；`BackEnd::setConfigManager`只对file URL使用文件管理器，数据库支持关闭时拒绝其他URL。`ConfigDatabaseManager`映射四种类别并使用`date`时间版本查询，连接用户/密码从数据库URL取得。对应ConfigWidgets的Import/Export/Include Database槽函数为空。

据此，当前证据只支持“旧数据库后端存在于源码、默认关闭、旧GUI数据库动作未实现”，不能证明现场在使用某个DBMS，也不能安全确定驱动、连接字符串或凭据部署方式。本轮没有引入数据库依赖或伪造迁移行为；本地`.xcfg`编辑和固定版本文件仓库继续作为新软件路径。源码审阅及待补现场证据见[SOURCE_REVIEW_AND_PLAN.md第7节](SOURCE_REVIEW_AND_PLAN.md#7-依赖减量策略)。

更新后的方案说明已同步至Ubuntu24和Windows候选包。`refresh-package-docs.py`前后包校验通过，工具确认可执行文件及运行库未变；Ubuntu24仍为25程序/175文件，Windows仍为6程序/261文件。全仓文档本地链接检查1237项通过。本记录是源码静态审阅，不是任何数据库连接/导入/导出行为测试。

## 71.08 include共享类信息双向补齐及当前源码回归（2026-10-20）

修正共享类导入的兼容方向：当include导入带unit/range的成员，而本地同类成员先前没有ClassInfo时，本地成员现在继承该共享信息；只有双方均有ClassInfo且属性不兼容才拒绝。此行为对照冻结CompoundConfig `Scalar::updateClassInfo()`中无属性成员采用现有类信息的规则。配置编辑手册同步说明导入顺序、有效属性与声明属性的区别。

验证：`xcfg-shared.pro`以`-pedantic-errors -std=c++11`构建，完整核心147项通过。Qt5.12.8/GCC9.3下完整GUI测试程序成功构建；本轮配置相关4个用例通过，其中包含无本地元数据时include提供共享信息、值/范围检查和GUI有效属性查询。完整GUI套件89项通过、12项失败；失败均为受限Linux沙箱禁止绑定localhost监听端口的网络/服务用例，不是配置编辑失败。用户实际Qt5.14.2/MinGW 7.3 Windows runner下Widgets 100通过、0失败、1项Unix文件锁测试跳过；ECC客户端63通过、0失败。测试清理回执显示两套测试raw剩余0文件、0字节，raw限额20GiB。随后从当前源码重新构建六个Windows操作端程序，并部署到`build-packages/getdaq-windows-qt5142-20261020-classinfo-candidate`；6程序/265文件manifest（含构建、测试结果及清理回执），移除SDK PATH后的命令帮助、offscreen启动和Windows原生窗口smoke均通过。构建、QtTest结果及清理回执随包保存在候选`validation/`目录；通过`manifest.json`核对摘要，不依赖临时中间目录。这不代表独立Windows机器、macOS、独立Linux机、真实板端或DAQ持续吞吐验收。

## 71.09 Ubuntu24当前源码全量回归及交付候选（2026-10-20）

在已验签Ubuntu24.04.5用户空间、GCC13.3、Qt5.15.13、glibc2.39及uid1000下，从本轮源码全量重新构建25个主机程序，并运行完整平台回归。第一次runner尝试的GUI实际101项全部通过，但旧守门值99使该次总回执标为failed；将Qt5.15.13真实期望值更新为101后重新运行，最终平台回执状态complete。严格C++11配置核心147/147，Widgets 101/101，原生contract CTest 19/19；源迁移、配置读写事务和接收器TCP/ZBUF/FDT/ICE传输场景通过。两次attempt保留在同一有界工作区，总wire数据99,478字节；测试runner确认raw不超过20GiB、`.graw`剩余0。构建回执、测试源码摘要、平台回执、核心/GUI日志、原生协议日志及迁移/事务/传输结果摘要随候选`validation/`目录提供。

最终候选为`build-packages/getdaq-ubuntu24-20261020-classinfo-candidate`，包含25个主机程序及182个SHA-256清单文件；`python3 tools/build-suite.py --verify-stage ...`通过。该环境是WSL宿主内核上的Ubuntu24用户空间，不是独立Ubuntu24机器；没有真实板端、两台物理主机、生产数据或持续吞吐验收。

## 71.10 Ubuntu24新候选认证TCP归档CLI回归（2026-10-20）

对71.09候选中的实际`getdaq-catalog`运行Ubuntu24动态加载器下的认证TCP回归，而非引用旧候选的catalog二进制。候选程序SHA-256为`a16f76489554387a2d9d6b34f5b9d829ef0437da5ec8dd1c024209dd442e014b`；HMAC向量、错误密钥在建目标前拒绝、partial/raw前缀不变、失败续传保护、正确凭据续传、接收端读回、已完成无载荷续传、真实CLI分进程监听/发送、关闭端口限时拒绝及帮助参数检查全部通过。回执绑定当前候选程序、system experiment tests、catalog transfer tests及runner SHA。

GCC13.3/C++11 Ubuntu24用户空间构建`catalog-transfer-tests`通过，回归在Ubuntu24 glibc2.39 loader和WSL Linux宿主内核下完成。共生成并删除15,224,832字节合成raw，预设64MiB上限（低于20GiB用户限制）；结束时raw剩余0、两个私有测试根为空。回执随候选`validation/ubuntu24-catalog-authenticated-tcp.json`保存，181文件包增加该SHA绑定回执后为182文件；`python3 tools/build-suite.py --verify-stage`通过。此为单机localhost协议/CLI测试，不是两台独立主机、防火墙/现场凭据、独立Ubuntu24、macOS、真实DAQ或持续吞吐验收。

## 71.11 Ubuntu24候选接收器四路并发回归（2026-10-20）

对71.09全量候选的实际`getdaq-receiver`运行四个并行localhost连接，每路256MiB、4KiB合成帧、128MiB文件轮转，合计1,073,741,824字节。Ubuntu24.04.5用户空间/GCC13.3/glibc2.39 ELF loader与WSL宿主Linux内核运行。四路分别完成65,536帧和两个归档文件；发送端SHA与按落盘文件序号重读得到的整流SHA一致，逐文件SHA/字节数、帧数、自然EOF、receiver退出状态和持久seal均通过。实测墙钟（含全文件SHA核对）约1.54秒；每个receiver峰值RSS约21,036KiB，接收器CPU约0.36–0.39秒/路。约49GiB物理内存大于本次1GiB载荷，Python生成/哈希、页缓存和虚拟化存储都会影响计时，因此该数据不是DAQ速率或持续磁盘写入指标。

合成`.graw`在哈希/封存核验后自动删除；候选内`validation/ubuntu24-receiver-four-stream-load/payload-cleanup.json`记载删除1,073,741,824字节、剩余0，测试脚本执行前的总预算上限20GiB。运行回执绑定候选接收器SHA `34511044c27ef96fe04f96b9ca5bb523e17d140b64ed15218bda3ae8cdc658fc`、测试脚本/loader摘要、逐流结果和接收日志。候选由182增至193个SHA清单文件，`python3 tools/build-suite.py --verify-stage`通过。测试首次在受限沙箱因localhost bind权限被拒，fixture清理回执为0字节；之后仅在获准loopback权限下运行上述成功测试。此项是同机四进程合成并发覆盖，不代表双机网络、生产网卡/磁盘、真实板端或持续零丢失验收。操作与复现边界见[接收性能手册4.3节](RECEIVER_PERFORMANCE.md#43-当前候选四路并发回归2026-10-20)。

## 71.12 Ubuntu24标准平台runner接入只读源码安全与四路接收回归（2026-10-20）

`receiver_sustained_load.py`原先将并发锁放在`tests/`，与Ubuntu24验证将源码以只读方式挂载到`/code`不兼容。现将checkout专属锁放进系统`TMPDIR`（此runner设为可写的`/build/tmp`），并加入安全单测，确保锁路径位于源码树之外。Ubuntu24标准`--stage all`已固定运行`receiver-load-safety`和`receiver-four-stream`，不再依赖手工单独执行这两项。

2026-10-20在Ubuntu24.04.5用户空间、GCC13.3、Qt5.15.13、glibc2.39 loader及WSL2宿主内核下，以只读`/code`完成全量runner：25个主机程序构建成功，配置核心147/147、Widgets101/101、原生CTest19/19；迁移、事务及原有接收传输组通过；新增接收负载安全测试6/6，四个真实接收子进程各收256MiB，总计1,073,741,824字节。发送端与落盘整流SHA、逐文件SHA/字节数、65,536帧/流、EOF、seal与退出状态通过。执行器回执绑定的接收器SHA为`34511044c27ef96fe04f96b9ca5bb523e17d140b64ed15218bda3ae8cdc658fc`，与Ubuntu24交付候选内receiver完全一致。

单轮全部传输测试wire量为1,073,791,563字节（四路负载1,073,741,824加其他传输49,739），低于20GiB上限。四路生成的`.graw`逐一校验封存后已删除1GiB；清理回执为`remaining_generated_payload_files=0`，runner汇总`raw_remaining=0`。源码树中没有遗留测试raw。平台回执、全量日志和四路发送/接收/清理摘要归档在候选包`validation/ubuntu24-platform-71.12/`。

这次证明runner与只读源码挂载兼容，并证明候选接收器在本机Ubuntu24用户空间完成四流合成负载和清理；并非独立Ubuntu24内核、双主机网络/防火墙、生产网卡/磁盘、真实板端或持续DAQ吞吐验收。测试数据低于物理内存容量，不据此给出生产速率。

## 71.14 C++11 MFM按事件离线合并器（2026-10-20）

将旧`CoBoFrameViewer`命令行`cobo-frame-merge`按事件合并用途迁入无Qt原生数据工具`apps/datareceiver/getdaq-merge`。输入扫描只保留事件号、输入序号、文件偏移和帧字节数；事件帧按事件号/输入顺序/输入偏移排序，然后以1MiB缓冲分块复制；输出采用MFM type `0xFF01` revision 1、逻辑头长20字节，内嵌CoBo/RIBF帧内容不改写。最大单帧1GiB、索引帧数2000万；`getdaq-merge`进入`build-suite.py`的原生数据profile，运行库依赖守门复用数据路径规则。

GCC9.4/C++11独立临时构建`/tmp/getdaq-merge-build`成功，`mfm-merge`测试通过，覆盖乱序事件排序、重复事件分组与输入顺序、CoBo类型过滤、输出帧长度和类型、原始子帧字节未变化、已有目标文件不会被覆盖。测试夹具原始数据总计112字节，清理后无残留。`python3 tools/build-suite.py --profile data --build-root /tmp/getdaq-merge-suite-build --jobs 2`成功构建5个程序，包括新合并器；程序为原生数据工具，Qt、Boost、Ice、yaml-cpp、curl等动态依赖受现有检查拒绝。

同一独立receiver CTest运行中，`mfm-merge`和`receiver-core`通过；既有`exporter-lock`失败并输出`socket failed`，当前尚未确认是沙箱socket限制还是该旧测试需修整，因此只报告为未通过，不将其归因于新合并器。没有执行大文件压力/磁盘吞吐测试，也没有拿旧可执行文件做逐文件输出差分；Windows交付候选和旧Ubuntu24候选尚未更新。详细用途、限制和使用方式见[MFM合并手册](DATA_MERGE.md)。


## 71.13 Ubuntu24接收压力锁文件符号链接与硬链接防护（2026-10-20）

71.12将接收压力测试锁移至可写`TMPDIR`以兼容只读源码挂载后，进一步收紧该锁文件入口。`receiver_sustained_load.py`现在以`O_NOFOLLOW|O_CLOEXEC`打开固定路径，并检查目标必须是当前用户拥有的普通文件且`st_nlink==1`，之后将权限设为`0600`并取得非阻塞独占锁。平台缺少安全打开标志、锁路径为符号链接、所有者异常或存在额外硬链接时，runner在创建压测目录/数据前失败关闭；稳定锁文件不随运行清除，避免并发进程因unlink而分别锁住不同inode。

安全回归从6项增至9项，新增符号链接拒绝且目标字节不变、硬链接拒绝且目标不变、有效锁为`0600`并阻止同一checkout第二个进程。普通Ubuntu主机Python测试9/9通过；随后在Ubuntu24.04.5用户空间/GCC13.3/Qt5.15.13/glibc2.39 loader、WSL2宿主内核和只读`/code`下重新运行标准`--stage all`，全套25程序重建成功，核心147/147、Widgets101/101、原生CTest19/19及迁移/事务/接收传输场景通过，接收锁安全9/9，四路并发接收通过。候选receiver SHA仍为`34511044c27ef96fe04f96b9ca5bb523e17d140b64ed15218bda3ae8cdc658fc`。

本轮wire总量仍为1,073,791,563字节（receiver四路1,073,741,824，其他传输49,739），小于20GiB；合成raw已删除1GiB，清理余量0，工作目录没有`.graw`。更新后的完整证据归档于候选`validation/ubuntu24-platform-71.13/`。这些测试证明本机临时目录锁的文件身份/权限边界以及Ubuntu24用户空间runner兼容性，不等于独立双机、目标机、实板或持续DAQ吞吐验收。

## 71.16 Ubuntu24候选增量加入MFM合并器（2026-10-20）

保留原候选`build-packages/getdaq-ubuntu24-20261020-classinfo-candidate`不变，将其复制为新候选`build-packages/getdaq-ubuntu24-20261020-mfm-merge-candidate`，再通过`tools/refresh-ubuntu24-program.py`增量加入`getdaq-merge`。原候选有25个程序；新候选共有26个程序、279个清单文件，其中25个原程序逐字节复用，只有`getdaq-merge`由本轮GCC9.4/C++11 Release构建提供。程序SHA256=`585b4b8933dae2cc70f8648075cc4f85725674907f3402c62890c8e3ce48d467`。包内`validation/program-refresh-getdaq-merge.json`记录源构建SHA=`8026d6e9eabe26e93b4d588bd799d213c2a344081b7f94f9f015f6247c5aa51d`、测试收据、Ubuntu24 loader依赖解析及帮助检查；候选manifest随包提供并通过校验。

合并专项收据绑定C++测试和实际命令行输出：事件排序/分组、输入顺序、MFM外层头、内部字节保持、已有输出保护及`--help`均通过。此次两组小型输入/输出总共生成224字节raw并在测试结束后删除，回执记录删除224字节且剩余0；远低于20GiB上限。没有使用真实采集文件、实板或生产数据。

`python3 tools/build-suite.py --verify-stage ...`通过，`python3 tools/package_ubuntu24_catalog_ssh.py --verify --candidate ...`通过，包刷新器在Ubuntu24 loader下检查新程序及完整包依赖通过，`tests/package_ubuntu24_catalog_ssh_test.py`为12/12，文档本地链接检查1248项通过，`verify-dependencies.py --profile data`通过。候选文档也已同步为本轮最新版本；文档刷新前后产品程序与运行库摘要保持不变。此为建立在先前25程序全量构建候选上的单程序增量刷新，不是26程序全量重编译；验证使用Ubuntu24 rootfs用户空间和本机内核，不代表独立Ubuntu24主机、双机TCP/防火墙、macOS、现场硬件或持续DAQ吞吐验收。

## 71.17 exporter-lock socket失败的沙箱归因复核（2026-10-20）

71.14曾记录`exporter-lock`在受限执行环境中以`socket failed`退出，原因待确认。使用当前源码在`/tmp/getdaq-exp-lock`重新配置并编译完整原生receiver套件后，普通沙箱中运行该用例仍立即得到`socket failed`；允许本机loopback socket的受控执行下，同一构建、同一二进制通过`exporter-lock` CTest 1/1。随后同一环境下完整原生 CTest `mfm-merge`、`receiver-core`、`exporter-lock`为3/3通过，执行时间0.19秒。

因此本次已有直接证据将该失败归因于测试执行环境禁止socket创建，而不是 exporter 或新合并器缺陷；无需修改生产代码。测试只传输约数KB合成数据，没有生成raw。该结论适用于本机loopback专项测试，不代表外部网络、双主机或目标平台验收。临时构建`/tmp/getdaq-exp-lock`在完成后清理。

## 71.18 原生数据接收器CMake测试开关修正（2026-10-20）

审阅`apps/datareceiver/CMakeLists.txt`发现它传入`-DBUILD_TESTING=OFF`时仍无条件注册`mfm-merge`、`receiver-core`、Linux `exporter-lock`测试，并始终暴露测试基准`mfm-stream-benchmark`。这与主机交付构建的生产模式约定不符。现使用CMake/CTest标准`include(CTest)`并将上述验证目标放在`if(BUILD_TESTING)`内，保持默认开发模式开启；发布构建关闭时仅配置生产程序。

GCC9.4/CMake3.16验证：`BUILD_TESTING=OFF`成功构建`getdaq-receiver`、`getdaq-inspect`和`getdaq-merge`；生成目标列表不含三个测试可执行文件或benchmark，CTest枚举为0项，配置时也不再报告该变量未使用。`BUILD_TESTING=ON`完整构建后，获准loopback的CTest三项`mfm-merge`、`receiver-core`、`exporter-lock` 3/3通过。普通沙箱对loopback的限制及测试数据边界见71.17；当前合成测试不产生raw文件。

这项修正只作用于开发/构建目标是否存在，不改任何生产程序源码或CLI行为；Ubuntu候选内程序SHA保持原样。发布包文档与完整manifest随后刷新复验。证据为`apps/datareceiver/CMakeLists.txt`、本节命令结果和[主机端构建说明](HOST_SUITE_BUILD.md)。

## 71.19 当前全量构建清单和Ubuntu24 runner同步（2026-10-20）

`getdaq-merge`加入统一构建计划后，两个守门器仍保留旧程序数量：Ubuntu24全量runner固定要求25个程序，`host_suite_delivery_test.py`仍要求`data` profile为4个。前者会在全量构建成功后把runner判为失败，后者在本机已复现失败。现将全量runner改为从当前`build-suite.py --profile full`计划推导准确程序集合，并显式要求计划包含`getdaq-merge`；数据profile单测改为比较receiver和数据CLI目标集合。

用例`python3 tests/host_suite_delivery_test.py`为4/4通过；同一严格C++11统一构建在Ubuntu 20.04.6/WSL2宿主、GCC9.4、Qt5.12.8下重新构建当前源码完整套件，构建回执状态`complete`、26/26程序；回执程序名与Ubuntu24 runner当前计划的26项目集合完全相符。内嵌依赖105文件检查通过。未在本轮实际Ubuntu24 rootfs中执行完整GUI/平台runner，宿主全量构建不冒充Ubuntu24全量交付或硬件验收；Ubuntu24候选仍是25个原程序复用、仅合并器增量构建的包。

本轮全量构建只产生程序/对象，不运行DAQ，也没有raw载荷；临时构建随后清理。证据为`tests/host_suite_delivery_test.py`、`tests/ubuntu24_platform_validation.py`及其核对的`build-suite.py`计划。独立Ubuntu24、macOS、真实双机/板端和持续吞吐仍待现场环境。

## 71.20 普通 TCP 归档传输与Ubuntu24当前源码全量验证（2026-10-20）

根据用户确认，已将已完成归档的跨主机TCP通道改为普通TCP/IP，不做共享密钥认证或TLS加密。发送/监听CLI不再要求凭据文件，也移除了凭据生成命令；握手后直接进入归档流。保留原有分块传输、SHA-256内容校验、partial前缀核对、显式断点续传、目标不覆盖和完成目录读回校验。SHA-256仅检查内容，不提供身份识别或抗篡改认证；现场需要用可信专用网络和防火墙限制监听端口。

Ubuntu24.04.5用户空间、GCC13.3、Qt5.15.13和glibc2.39 loader下，从本轮源码全量重建统一full profile的26个主机程序，内嵌兼容依赖105文件。严格C++11配置核心147/147、Qt Widgets101/101、动态发现的原生CTest20/20通过；源码迁移、配置事务、receiver传输、压测安全9/9及四路接收全部通过。runner实测总wire量1,073,791,563字节，其中四路接收载荷1,073,741,824字节；receiver传输回归49,739字节。四路生成的1GiB `.graw` 已校验后删除，传输夹具也逐项清理，结束时`.graw`剩余0。

另外，在同一Ubuntu24用户空间对全量构建的`getdaq-catalog`运行普通TCP专用回归：无认证连接、监听CLI与发送CLI两个独立进程、关闭端口快速拒绝、首文件后断开、显式partial续传、接收端SHA读回、完成归档零载荷复续传全部通过。该回归生成并删除15,220,736字节合成raw，预算上限64MiB，私有`/tmp`与`/dev/shm`根目录为空，raw_remaining=0。Ubuntu24源码构建二进制SHA=`b58adcc8d112ac3d5c45e3d5e27f14ed8118028c0a88a4cd8b0d3edfef6cf5aa`，回归记录在当前交付候选`validation/catalog-plain-tcp.json`。

Ubuntu24交付候选`build-packages/getdaq-ubuntu24-20261020-mfm-merge-candidate`现在仍包含26个程序；本轮只将`getdaq-catalog`刷新为上述Ubuntu24当前源码构建版本，其余25个程序沿用此前候选版本。候选当前281个SHA-256清单文件，manifest SHA-256=`1e15a216f2dc79abfe1119ea8eee6cd8db948b1f414fff43458b44ea9ea373f8`。`build-suite.py --verify-stage`、包内Ubuntu24 loader/帮助检查、`package_ubuntu24_catalog_ssh.py --verify`、候选文档刷新及相关12项包回执测试通过。新协议的真实双机网络、生产防火墙、目标磁盘、独立Ubuntu24主机、macOS、真实DAQ硬件和长时间持续吞吐仍未验收。

## 71.21 当前源码接收器16 GiB四流写盘与读回校验（2026-10-20）

使用`tests/build_receiver_validation.py`从本轮当前源码以严格C++11构建原生接收器（14个编译单元、24个构建输入），并在Linux x86_64/WSL2宿主上运行四个并行localhost TCP接收器；每流4 GiB，合计16 GiB，4 KiB合成CoBo帧，1 GiB文件轮转。发送端对全部数据求SHA-256，接收后完整读取所有落盘文件，逐文件及整流摘要、字节数、帧数、自然EOF、进程退出和持久seal均一致。每流1,048,576帧、4个数据文件；聚合墙钟（含全文件读回校验）25.20秒。接收器峰值RSS每路20,804 KiB，CPU时间约4.79–5.36秒/路。该运行不是硬件DAQ速率：Python合成/哈希发送、localhost、WSL虚拟化磁盘和页缓存共同影响结果；16 GiB载荷仍低于约49 GiB系统内存，因此不证明超过内存缓存的持续磁盘性能或真实多板零丢失。

16 GiB低于用户规定的20 GiB单轮合计上限。成功回执绑定接收器SHA-256 `d30d467ec73b0d23882570dd40f462ea9be11ac081efbd160799971f1fb795df`和测试脚本SHA；`payload-cleanup.json`证明已删除17,179,869,184字节、剩余raw为0。严格C++11构建目录中的`mfm-merge`、`receiver-core`、`exporter-lock` CTest在获准loopback环境下3/3通过。精简证据（结果、清理回执、构建回执、CTest日志）保存在`build-receiver/host-four-stream-16gib-20261020/`；原始`.graw`已由测试脚本删除，临时构建随后删除。本地流量/文件系统回归不替代独立双机、物理板卡、macOS或现场持续吞吐验收。

## 71.22 旧配置拆分标量兼容读取与只读保护（2026-10-20）

旧CompoundConfig的实际 typed-scalar 访问器在注释、处理指令或CDATA将文本分成多个XML text节点时，最终保留最后一个文本节点。新版配置读取现对显式`type`的拆分标量沿用这个可观察值，同时在`Node::valueEditable`标记为只读；直接保存明确拒绝，操作台输入框和保存按钮也会禁用并解释需要用户先显式整理XML。无`type`的拆分文本仍拒绝加载，避免把旧对象/标量推断不确定的配置当成可编辑参数。

验证：严格C++11的`xcfg-shared-tests` 148/148通过，含comment/CDATA/processing-instruction拆分值为`b`、禁止写回及无类型拒绝；原始CCfg/TinyXml测试参考程序以55个编译单元从保留源码独立构建成功，当前探针与原程序运行192个用例、384次进程，171个相同、6个既有文档化扩展、7个严格拒绝、8个双方拒绝，新增typed-split-text落入相同结果。Qt操作台测试目标107个源码单元也成功构建。71.22当时未能运行新增GUI禁用状态测试，因为沙箱阻止写入工作区外Qt测试缓存；此项随后由71.23在用户Windows Qt5.14.2环境补测通过。全部用例使用临时小型XML，没有raw DAQ传输，也没有改写`source/`旧库。

本对照只覆盖枚举的XML/标量配置行为，不证明整个CompoundConfig、远端配置库协议、数据库或网络扩展兼容；真实硬件配置应用及窗口人工操作仍待独立验收。

## 71.23 拆分标量修复的Linux与Windows当前源码交付候选（2026-10-20）

从当前源码执行Linux `full` profile，构建回执`build-delivery/split-scalar-full/full/build-receipt.json`为complete，26个主机程序全部生成。环境为WSL2宿主内核/Linux glibc 2.31、Qt 5.12.8、CMake 3.16.3。新候选`build-packages/getdaq-linux-host-20261020-split-scalar-candidate`通过170文件manifest校验，并将26个程序全部交由现有Ubuntu 24 rootfs动态加载器做依赖解析，未发现rootfs外依赖；四个原生数据工具另通过无Qt运行依赖检查。该候选是当前宿主构建包，不是Ubuntu24原生构建/全套Ubuntu24 runner结果；加载器解析不代表程序运行、GUI完整测试或独立主机验收。此前Ubuntu24候选不含本轮拆分标量修复。

使用用户提供的`D:\Software\QT\5.14.2\mingw73_64`及配套`D:\Software\QT\Tools\mingw730_64`，从当前源码实际构建Windows Qt 5.14.2 MinGW x86_64六个操作程序。构建回执`build-windows-qt5142-split-scalar/windows-build-receipt.json`记录GetDAQ、config、config-check、sentry、simulator及config-upload六个程序；新候选`build-packages/getdaq-windows-qt5142-20261020-split-scalar-candidate`经文档刷新后有262个manifest文件。Qt运行库部署完整性6程序通过；SDK-free PATH启动检查及GetDAQ初始窗口offscreen smoke通过。随后同一Kit独立构建完整GUI QtTest目标，并在Windows以QtTest 5.14.2运行`configurationLegacySplitScalarIsDisplayedReadOnly`及`configurationCdataValueEditPreservesCommentsAndReviewedUndo`，两项各3/3通过（init、目标用例、cleanup）。整套Widgets QtTest随后以正常Windows `qwindows` 插件和Fusion样式完成：101 passed、0 failed、1 skipped，约61秒；跳过项为现有Unix接收锁平台用例。初次用`offscreen`插件运行时在消息框路径中断，改用Windows插件后完整运行成功，因此offscreen中断不计为产品失败。程序SHA与构建回执匹配，当前源码摘要包含`common/config/xcfgdocument.cpp`及`apps/envoy/configwindow.cpp`。这仍不是完整人工配置流程或独立干净Windows机器验收。

## 71.24 Ubuntu24当前源码全量构建与平台回归（2026-10-20）

使用已验证的Ubuntu 24.04.5用户空间，以GCC13.3、Qt5.15.13、CMake3.28.3从只读挂载的当前源码执行标准`tests/ubuntu24_platform_validation.py --stage all`。26个full-profile程序完整重建；内嵌依赖检查105个文件通过。构建期间第一次已生成完整产品和测试程序，随后runner完整执行测试，但旧结果门槛仍要求配置核心147、Widgets101，导致首轮最终状态为failed；日志实测核心148/148、Widgets102/102，其余各命令均返回0。将门槛更新到本轮新增拆分标量测试后的准确计数后重跑，又发现证据搜索重复收集同一工作目录上次attempt的旧四路结果。现runner将传输与四路结果限定为当前attempt开始后生成的回执，保留旧attempt证据；第三次完整`all`运行状态为complete。

最终回执`build-ubuntu24-validation/rootfs/build/getdaq-platform-71-24-split-scalar/platform-eb98583cb3f245ea910b9243cc2a7ce6.json`绑定当前源码摘要、程序SHA和19条命令：26程序build；严格C++11配置核心148/148；Qt Widgets102/102；动态发现的原生CTest20/20；配置来源迁移、跨文件事务、receiver传输、负载安全和四路接收全部通过。环境为Ubuntu24用户空间+WSL2宿主Linux内核，因此结果不是独立Ubuntu24机器或硬件验收。

receiver传输回归wire 49,739字节，四路并发接收合成raw 1,073,741,824字节，合计1,073,791,563字节（约1.0GiB，低于20GiB限制）。发送与落盘摘要、文件轮转、EOF、seal及进程状态均通过；四路payload清理回执记录删除1GiB、剩余文件0。全部runner命令结束后工作树和测试目录均无`.graw`。三次尝试的每次payload均不超过1GiB并在当次结束清理；失败尝试保留小型结果/清理回执供诊断，不把旧payload重复计入当前attempt。

本次修复的平台runner门槛和attempt证据筛选均已在第三次完整运行中验证。完整Ubuntu24候选`build-packages/getdaq-ubuntu24-20261020-split-scalar-candidate`由这26个当前源码程序生成，另附完整构建回执、平台attempt回执、QtTest结果、19条runner日志及本轮传输/清理证据；随后在Ubuntu24用户空间对候选目录内的26个程序逐一运行`--help`或无参数用法检查，26/26按预期退出，程序SHA与manifest相符。包内附该smoke回执及runner源码SHA。候选共200个manifest文件；SHA清单、包文档刷新及host-suite完整性检查通过。它替代旧增量构建候选作为本轮最新Linux部署候选。没有真实板端、独立主机、外网双机、macOS或超过内存缓存的长时间DAQ磁盘吞吐验收。

## 71.25 Windows Qt5.14.2当前源码Widgets与ECC客户端完整回归（2026-10-20）

用用户提供的Qt5.14.2 MinGW 7.3 x64 Kit，在当前工作树严格C++11重建并部署Qt Widgets与ECC system client完整测试程序。真实Windows进程、正常`qwindows`插件和Fusion样式下，Widgets QtTest 101 passed、0 failed、1 skipped；跳过项是Unix接收锁用例。ECC system client为63 passed、0 failed、0 skipped，两个进程退出码均为0。此前针对拆分标量只读显示和CDATA编辑撤销的两个专项仍各3/3通过。

Windows runner独立私有临时目录删除成功，清理回执记录临时根已移除、raw文件0个/0字节。新结果绑定在`build-receiver/windows-qt-suite-20261020-current-source/build-receipt.json`及`native-windows-results/`；runner/build脚本SHA由测试回执的输入清单绑定。完整运行只证明该用户Windows/Qt Kit的测试结果，不代表独立干净Windows安装、现场真实ECC远端、板端/DAQ硬件或完整人工操作验收。Windows候选包`build-packages/getdaq-windows-qt5142-20261020-split-scalar-candidate`已附测试构建回执、QtTest日志/汇总、清理回执及复现脚本；274个manifest文件完整性验证通过，刷新文档前后六个程序和运行库均未改变。

本轮是应用构建、依赖解析和初始窗口启动检查；没有运行现场硬件/板端命令、DAQ流量或生成raw。它未证明Windows手动配置完整流程、独立干净机器安装、Ubuntu24实机、macOS、两机网络或持续DAQ吞吐；新增的拆分值GUI专用QtTest仍待可写的测试运行环境执行。

## 71.26 跨平台归档发布失败关闭与独占目录回归（2026-10-20）

复核已封存归档的最终目录发布路径后，发现非Linux/macOS POSIX回退仍采用“先`lstat`检查目标不存在、再普通`rename`”的两步操作；其他进程可在两步之间创建同名目标，普通`rename`随后会替换它。现已移除该回退：Linux继续用`renameat2(RENAME_NOREPLACE)`，macOS继续用`renamex_np(RENAME_EXCL)`，没有已验证原子独占重命名API的平台明确拒绝发布并保留`.partial`。macOS独占发布错误现在附带系统`errno`文本，方便识别目标卷不支持该能力等原因。该修改不改变普通TCP/IP传输协议或实时DAQ数据路径。

当前源码在Ubuntu 20.04 / GCC 9.4 / CMake 3.16.3以严格C++11重新构建`getdaq-catalog`及接收目录、归档传输和控制协议测试程序。CTest先在无loopback权限的默认沙箱中因TCP监听被系统拒绝；获准受控本机loopback后，`receiver-catalog-filesystem`、`catalog-transfer-filesystem`、`supervisor-control-codec`共3/3通过。归档传输测试中的竞争目标回归确认竞争目录未被替换，失败时`.partial`仍保留。fixture由测试作用域自动清理；同时确认并清除了`/tmp/getdaq-catalog-test-CsCL2z`中的前次遗留合成数据（9个`.graw`、共57,344字节），本轮新测试没有留下raw。单轮测试输入远低于20 GiB。

Apple官方文档说明`RENAME_EXCL`通过`renamex_np`请求目标已存在时返回`EEXIST`，且支持情况取决于实际文件系统，见[Apple exclusive rename capability](https://developer.apple.com/documentation/foundation/urlresourcevalues/volumesupportsexclusiverenaming)和[Apple APFS APIs](https://developer.apple.com/library/archive/documentation/FileManagement/Conceptual/APFS_Guide/ToolsandAPIs/ToolsandAPIs.html)。本次执行环境没有Apple SDK，故这不是macOS编译、APFS目标卷或GitHub Actions验收；应继续由`macos-14` CI及部署目标卷实测确认。独立Ubuntu24、真实双机、防火墙、板端和持续DAQ吞吐也仍待验收。

## 71.27 归档流SIGPIPE线程隔离与Ubuntu24候选增量刷新（2026-10-20）

并发检查发现，归档流原用`sigaction`临时设置进程级`SIGPIPE=SIG_IGN`。多个线程同时传输时，一个会话退出可能恢复旧处理器，影响仍在运行的另一个会话。现改为仅在当前线程屏蔽`SIGPIPE`；当关闭管道/套接字的写操作生成待处理信号时，在恢复原线程屏蔽状态前消费该信号。原有处理器不再被代码改写。

GCC9.4/CMake3.16.3严格C++11构建`getdaq-catalog`和`catalog-transfer-tests`成功。测试在线程中的传输会话阻塞于对端回复期间读取进程的`SIGPIPE`处理器，确认其与会话前相同；关闭对端后该会话捕获EOF并退出。默认沙箱因禁止loopback监听而失败，受控本机loopback下`catalog-transfer-filesystem` CTest 1/1通过。随后将当前二进制和测试程序置于Ubuntu24.04.5用户空间动态加载器下，普通TCP回归通过：无认证TCP、连接拒绝、断线后显式续传、接收端回读、完成归档零载荷续传和进程级信号隔离均有回执。该负载生成并删除15,245,312字节合成raw，预算64MiB、剩余0；不接触硬件。

新回执`build-packages/getdaq-ubuntu24-20261020-split-scalar-candidate/validation/catalog-plain-tcp.json`绑定当前构建的`getdaq-catalog` SHA=`26cdaf7bf856c0e4c2850146aa2269c3e335d1d675f8cc27d4dd76c74c96ed0d`；通过`tools/refresh-ubuntu24-program.py`只刷新该包内的`getdaq-catalog`，其余程序沿用候选原字节。刷新后的程序SHA=`bc64c369205f4db4b95ad091157f2658baa4bdbde74fddd6daa11fb623181bb8`，包有202项manifest文件。Ubuntu24 loader、CLI帮助及整包验证通过；这仍是WSL宿主内核上的Ubuntu24用户空间回归，不等同于独立双机、现场网络、macOS或DAQ持续吞吐验收。

## 71.28 Ubuntu24当前源码全量构建与平台回归复核（2026-10-20）

针对71.27归档流并发信号处理变更，使用当前源码在Ubuntu24.04.5用户空间/WSL宿主内核通过`tools/validate-ubuntu24-from-wsl.py run`重跑`tests/ubuntu24_platform_validation.py --stage all`。项目runner完整构建full profile的26个程序及测试目标；配置核心148/148、Widgets102/102、动态发现的原生CTest20/20、来源迁移、跨文件事务、receiver传输/压测安全和四路接收均通过。最终回执`build-ubuntu24-validation/rootfs/build/getdaq-platform-71-28-current/platform-44ffbadd7f994e8a87b16de671231982.json`状态为`complete`，所有runner命令退出码为0。

四路接收器合成raw共1,073,741,824字节，其他传输49,739字节，总wire 1,073,791,563字节，低于20GiB上限。清理回执记录删除1GiB、剩余raw文件0；平台回执`raw_remaining=0`。该负载是有限本机回归，不证明独立机器、真实硬件或长期持续磁盘吞吐。

本次全量构建的Ubuntu24 `getdaq-catalog` SHA为`7cf70e4b558aa34a0b86e74ddfb005015e56bfcac580bfb449046e2e33de6d5f`，但本轮没有用它替换候选包程序；候选仍是71.24全量包加71.27按普通TCP专项验证后增量刷新过的`getdaq-catalog`，共26程序/202文件。另一次尝试直接由Ubuntu20宿主启动新Ubuntu24 glibc二进制，其分进程CLI因宿主缺少GLIBC_2.38/GLIBCXX_3.4.32而不能启动；测试夹具已自动清除，未留下raw。这是执行环境的动态库边界，不是Ubuntu24 runner失败，也不算新候选验收。独立Ubuntu24主机、macOS、Windows完整人工配置、双机网络/防火墙、板端和DAQ持续吞吐仍未验收。

## 71.29 Ubuntu24当前源码全量候选及归档TCP验收（2026-10-20）

将71.28 Ubuntu24 full-profile构建产物正式整理为新候选`build-packages/getdaq-ubuntu24-20261020-71-28-current-source-candidate`，旧候选未覆盖。来源回执`build-ubuntu24-validation/rootfs/build/getdaq-platform-71-28-current/delivery/full/build-receipt.json`为`complete/full`，26个程序的路径均存在且SHA与回执相同；其中`getdaq-catalog`源码构建SHA=`7cf70e4b558aa34a0b86e74ddfb005015e56bfcac580bfb449046e2e33de6d5f`。候选包含26个全量构建程序，194项manifest文件，总大小约91MB。完整平台attempt、构建及测试回执和19条日志复制到包内`validation/ubuntu24-platform-71.28/`；包不包含构建中间对象及合成raw。

逐个通过Ubuntu24.04.5 loader解析新包中全部26个程序，并按CLI约定执行`--help`或无参数用法；Qt操作台使用该rootfs的offscreen插件。26/26退出符合预期，SHA与manifest相同，无硬件访问、无`.graw`残留。另在该Ubuntu24用户空间以当前源码重新构建`catalog-transfer-tests`，连同71.28全量runner的`system-experiment-tests`，对包内**确切**`getdaq-catalog`运行`tests/run_catalog_plain_tcp_tests.py`。无认证TCP发送/监听、连接拒绝、线程级SIGPIPE隔离、断线续传、接收端读回、完成后零载荷续传均通过；结果`build-ubuntu24-validation/rootfs/build/getdaq-platform-71-28-current/evidence/package-catalog-plain-tcp.json`绑定catalog SHA及测试程序摘要。合成raw生成并删除15,245,312字节，64MiB预算内，raw余量0、私有测试目录为空。

最后`tools/build-suite.py --verify-stage`与`tools/package_ubuntu24_catalog_ssh.py --verify`均通过，包文档按71.29状态刷新后再次验证。此候选仍只在WSL内核上的Ubuntu24用户空间验证；独立Ubuntu24主机、两台真实网络主机及防火墙、板端、macOS和长期/超内存DAQ磁盘吞吐尚未验收。普通TCP无认证且不加密，部署需限制在可信网络。

## 71.30 正常DAQ收尾文档边界和普通TCP回归标签校正（2026-10-20）

复核`apps/hardware/systemacquisition.cpp`、`apps/ecc/native/nativeecccoordinator.cpp`与`DATA_SENDER_CONTROL.md`后，发现手册还把“完整生产协调”称作待实现，和当前代码不符。现已明确当前已接入的分层：`SystemAcquisitionClient::Stop`先停止硬件生产并刷新发送器、保留数据连接；正常Breakup执行剩余停止确认、逐sender Disconnect并产生EOF，NativeEccCoordinator随后调用receiver Finish并核验数据服务Stopped/封存状态；Reset/Unprepare另行检查和撤销准备。低层`DataSenderClient::Stop`只确认板端Stop RPC，无法单独证明EOF、完整数据或seal。同步修改数据发送器架构图、总状态及后续工作段，区分已接的软件协调与尚缺的现场证据。

同时修正`tests/run_catalog_plain_tcp_tests.py`最终错误信息中遗留的“authenticated-TCP”称呼。由于测试runner摘要属于交付回执，使用更新后的runner在Ubuntu24.04.5用户空间对当前候选包内确切`getdaq-catalog`重新执行无认证普通TCP回归：线程级SIGPIPE隔离、直连收发、连接拒绝、中断续传、读回校验及完成后零载荷续传均通过。raw生成并删除15,245,312字节，预算64MiB、`raw_remaining=0`；回执绑定catalog SHA=`7cf70e4b558aa34a0b86e74ddfb005015e56bfcac580bfb449046e2e33de6d5f`及更新后的runner SHA。测试不连接真实板卡或外部主机。

Ubuntu24候选内plain-TCP证据已换成当前runner回执，包内用户手册同步至上述收尾边界。独立`build-suite.py --verify-stage`及Ubuntu24 catalog候选验证器应在包文档刷新后通过；产品程序和运行库未重建。该修正不补足现场控制权/真实加载版本、真实事件完整性、真实双机、macOS、干净Windows人工流程或持续吞吐验收。

## 71.31 旧CompoundConfig未知属性边界与真实配置编译回归（2026-10-20）

对照随仓库保留的CompoundConfig `XmlToConfigVisitor::extractObjectProperties` 和 `extractScalarProperties`，确认旧硬件配置解析器遇到未知XML属性会拒绝整份配置：对象只接受索引(`id`)与`include`，标量只接受`id/type/unit/range`。新`XcfgDocument`为了配置编辑器无损保留未知源内容，不应在通用加载时丢弃它们；但此前硬件编译路径只特别拒绝`class`，其他未知属性可能被忽略。

现由`ExperimentReader`在硬件参数编译入口遍历全部已加载节点（包含include来源）执行旧属性白名单校验，并在错误中给出属性、路径、文件和行号。这样未被当前子编译器查询到的节点也不会静默放行；配置编辑器继续保存未知属性原文。泛型路径共享`unit`/`range`、具体值与`type`仍按原有来源规则解析。

在Ubuntu20.04.6/WSL2宿主的GCC9.4.0、Qt5.12.8下，以`-std=c++11 -pedantic-errors`构建`tests/xcfg_shared.pro`，新增测试确认合法标量属性通过、标量`vendor`/`class`及对象自定义属性在硬件reader边界拒绝。完整`xcfg-shared-tests`为149 passed、0 failed、0 skipped；其中三个编辑器未知属性保留样例改为通过`XcfgDocument`直接核验编辑语义，不把它们冒充可由旧硬件runtime接受的配置。随后从同一源码重建`bin/getdaq-config`，使用仓库真实`configure-ExternalTrigger-2CoBo_1Mutant.xcfg`成功生成`getdaq.system-experiment-settings`，使用真实`configure-pedestals.xcfg`和四片`0x202`版本映射成功生成`getdaq.cobo-configure-image`。均只产生小型JSON，无raw或板端访问。旧parser的白名单依据见`source/cconfig/CompoundConfig-release-20210208/src/CCfg/Io/XmlToConfigVisitor.cpp`。

本轮没有执行Ubuntu24.04.5用户空间全量构建或刷新Ubuntu候选；随后Windows当前源码重建和回归见71.32。该修改加强本地配置兼容边界，不代表完整CompoundConfig、数据库/网络来源或现场固件已兼容。

## 71.32 Windows当前源码配置边界回归及运行候选（2026-10-20）

使用用户提供的`D:\Software\QT\5.14.2\mingw73_64`与配套MinGW 7.3.0 x86_64，从当前源码严格C++11重建六个Windows操作程序。构建回执`build-windows-qt5142-legacy-attr-boundary/windows-build-receipt.json`列出程序及源码输入摘要，`getdaq-config.exe` SHA为`e5d400f9d39b0d00ab5b73c5ce5fb5f12e31cc1c7454c8fefd8e57ad2e9f6212`。

在原生Windows QtTest 5.14.2下运行配置共享测试：新增未知属性编译拒绝用例3/3通过（含init/cleanup），全套配置测试148通过、0失败、1跳过；跳过项为Unix专用文件锁测试。日志随新运行候选保存。以同一构建的配置工具对真实`configure-ExternalTrigger-2CoBo_1Mutant.xcfg`与`configure-pedestals.xcfg`成功生成`getdaq.system-experiment-settings`和`getdaq.cobo-configure-image`；对带未知CompoundConfig对象属性的临时配置按预期拒绝且不写出工件。只记录小型JSON的类型、长度和SHA摘要，不把配置生成文件复制进运行候选。

全新Windows候选`build-packages/getdaq-windows-qt5142-20261020-legacy-attr-boundary-candidate`包含当前源码六个程序。Qt/MinGW匹配运行库、SDK-free PATH下六程序帮助、offscreen及原生Windows初始窗口检查均通过。另用同一当前源码重建Widgets/ECC客户端测试程序并在Windows原生插件下运行：Widgets101通过、0失败、1项Unix锁测试跳过；ECC客户端63通过、0失败、0跳过；私有测试根已删除、raw余量0。候选manifest完整性272/272项通过，包内SHA绑定六程序构建、完整QtTest构建、两组全文日志、清理回执和配置编译摘要。该验证没有执行完整人工配置流程、独立干净Windows安装、真实板端或DAQ传输，不代表这些场景已验收。

同一71.32继续在项目Ubuntu24.04.5用户空间运行全量平台流程。首轮实际报告配置核心149/149、Widgets102/102、CTest20/20和其他全部通过，但runner门槛仍要求核心148，故只因计数门槛失败；将准确计数更新为149后完整重跑成功，最终平台回执`build-ubuntu24-validation/rootfs/build/getdaq-platform-71-32/platform-7bdbea76e68a4533af759b526b6b2618.json`为`complete`，19条命令均为0。对本轮生成的Ubuntu24 `getdaq-config`使用真实外触发和pedestal配置编译通过；未知对象属性被拒绝且不产出输出。新完整候选`build-packages/getdaq-ubuntu24-20261020-71-32-current-source-candidate`的26个程序在Ubuntu24用户空间逐一启动检查26/26通过；候选包含平台、构建、测试、迁移/事务/接收、配置边界及清理证据，817项manifest复核通过。四路接收合成raw为1,073,741,824字节，聚合测试wire为1,073,791,563字节（约1.0GiB，低于20GiB）；回执记录raw剩余0。Windows完整Widgets/ECC当前源码回归同样在本轮补测并通过。两个Ubuntu用户空间完整运行均清理本轮raw。最终证据仍只覆盖WSL宿主内核上的Ubuntu24.04.5用户空间和用户指定Windows Qt Kit，不是独立Ubuntu主机、真实双机、板端、macOS或持续吞吐验收。

## 71.33 DataRouter ByteStorage字节存储模式补齐（2026-10-20）

复核旧`source/get/GetBench-software-release-20210208/src/get/daq/DataRouter.cpp`后确认，旧DataRouter除按MFM帧解析存储外，还支持`ByteStorage`：把收到的TCP/ZBUF字节块原样写入文件，不解析MFM帧。现为无Qt的`getdaq-receiver`增加显式`--storage-mode byte-storage`，默认仍是更严格的`frame-storage`。字节模式保留输入字节顺序，以独立写盘线程及有界队列落盘，按收到的块轮转文件，并在正常EOF后同步文件和目录、记录字节数与封存状态；它不证明MFM帧或事件完整。由于FDT心跳/GOODBYE需要按帧边界识别，字节模式拒绝FDT、解析帧导出及管理式FDT周期。

Ubuntu24.04.5用户空间/WSL宿主内核中，对候选内的接收器运行receiver-core、mfm-merge、exporter-lock和进程级TCP字节存储回归共4/4通过。专项检查确认收到的字节未变化/重排、正常EOF后封存及FDT被拒绝。测试总计生成并清理786,818字节合成raw，`raw_remaining=0`，远低于20GiB限制。回执位于候选`validation/receiver-byte-storage.json`，增量刷新回执为`validation/program-refresh-getdaq-receiver.json`。源码构建接收器SHA-256为`a5ccd246418f5b3a32562f6ad719ab3bb14c663713a0b5dc29eb515dae19d801`；剥离调试信息后的交付版SHA-256为`a792fc09c688c3e1e08c60f2686e0c6deb3e158d5f50fcbcbf2fa3e974212702`。Ubuntu24候选仍含26个程序，本轮只刷新`getdaq-receiver`，其余25个程序复用原字节；不是整套重编译。

测试使用WSL宿主内核上的Ubuntu24用户空间和本机TCP，没有真实板端/DAQ流、独立Ubuntu24主机、跨主机网络或持续磁盘吞吐证明。逐文件/帧完整性验证仅由默认`frame-storage`承载；字节模式封存不等价于帧格式正确。独立Ubuntu24/macOS/Windows数据接收器验收、真实板端互通、现场数据和长时间吞吐仍待完成。详细操作与限制见[接收器传输手册](RECEIVER_TRANSPORTS.md#原样字节存储模式)。

## 71.34 ICE ByteStorage接入DataSupervisor固定计划（2026-10-20）

此前独立`getdaq-ice-receiver`虽能选择`byte-storage`，但统一进程管理计划没有保存和传递该选择。本轮在`ReceiverSourcePlan`及计划JSON增加可选`storage_mode`；默认`frame-storage`仍省略字段以保持旧计划摘要不变。byte-storage模式由DataSupervisor传入receiver CLI，并在启动状态、结束seal及catalog复制的seal中核验；默认老seal未写模式时按`frame-storage`解释。计划验证拒绝FDT和启用帧导出端口的组合。TCP/ZBUF receiver也将字节模式写入结束seal，ICE seal继续记录存储模式。

Ubuntu20.04.6/WSL宿主内核/GCC9.4.0下，C++11重建`getdaq-receiver`、`getdaq-ice-receiver`、DataSupervisor核心和托管测试程序成功。实际loopback runner共11个场景通过，包括新ByteStorage场景的固定计划解码、Supervisor启动、65,537字节任意二进制ICE载荷逐字节对照、同连接确切字节数结束、状态/seal核对和catalog归档；已有普通ICE编码、混合TCP/ICE、缺结束、截断、receipt/catalog篡改及启动失败场景也通过。最大测试预算8MiB（20GiB硬上限），本次11场景累计生成并删除434,180字节raw，`remaining_raw=0`。实际证据与SHA位于`build-delivery/split-scalar-full/full/native/managed-ice-byte-storage-evidence/`，最新回执为`managed-ice-xq_yyx1d/result.json`与`payload-cleanup.json`。

此验证是本机ICE兼容库和loopback，不使用真实板端或旧客户端；没有跨主机/Ubuntu24独立安装/macOS、真实DAQ吞吐或长时间运行结论。ByteStorage保存原始字节，不验证MFM帧和事件完整性。配置字段、CLI限制及分层说明见[ICE数据入口](ICE_DATA_INGRESS.md)和[接收器传输手册](RECEIVER_TRANSPORTS.md#原样字节存储模式)。

## 71.35 Ubuntu24全量候选与托管字节存储回归（2026-10-20）

在项目专用Ubuntu24.04.5用户空间/WSL宿主内核/GCC13.3中，从当前源码重新构建26个完整套件程序。平台runner当前源码输入摘要与构建一致，所有19条构建/测试命令返回0：配置核心149/149、Widgets102/102、原生CTest21/21、源迁移、配置事务、receiver传输、加载安全和四路并发接收均通过。四路数据每路256MiB，合计raw 1,073,741,824字节；连同其他测试aggregate wire为1,073,791,563字节，低于20GiB，runner回执与清理回执确认raw剩余0。平台记录为`build-ubuntu24-validation/rootfs/build/getdaq-platform-71-34/platform-953616262b994baba2b978d35c91b187.json`。

全量候选`build-packages/getdaq-ubuntu24-20261020-71-34-current-source-candidate`含26个当前程序及477个manifest文件（约91.3MB），绑定全量构建、平台和专项回执。候选26/26通过Ubuntu24 loader依赖解析，逐程序启动检查26/26，CLI帮助入口3/3；`getdaq-receiver`仍无Qt/Ice等旧运行依赖。候选内实际程序专项回归结果：TCP ByteStorage及receiver-core/mfm-merge/exporter-lock为4项通过，生成并删除786,818字节；ICE入口15场景通过，生成并删除147,457字节；托管ICE/混合/异常生命周期11场景通过，生成并删除434,180字节。ByteStorage托管场景逐字节比对65,537字节任意二进制输入，核对固定计划、同连接准确字节数结束、状态/seal及catalog；所有专项raw清理回执为0剩余。测试上限分别为64MiB、32MiB、8MiB，单轮均远低于20GiB。

Windows Qt5.14.2 MinGW候选`build-packages/getdaq-windows-qt5142-20261020-legacy-attr-boundary-candidate`只刷新了README/文档及manifest，刷新前后六个程序和运行库摘要不变。新Ubuntu候选与Windows文档刷新候选校验均通过。

验证环境是WSL宿主内核上的Ubuntu24用户空间及本机loopback，不是独立Ubuntu24机器或两台主机。没有真实板端/旧ICE客户端、现场DAQ数据、macOS、目标Windows实机传输或持续吞吐验收；四路1GiB回归不能当作生产DAQ速率保证。byte-storage只保证接收字节与输入顺序相符，不校验MFM帧/事件格式。专项结果与SHA可在候选`validation/`内复核。

## 71.36 DataSupervisor ByteStorage计划codec边界回归（2026-10-20）

此前ByteStorage已由接收器集成和托管ICE进程测试覆盖，但`supervisor-control-tests codec`没有直接锁定计划JSON的存储模式字段。现增加以下C++测试：默认`frame-storage`不进入规范计划，保持旧计划字节；`byte-storage`必须进入计划身份且编码/解码往返完全一致；未知大小写、空值及其他存储模式拒绝；ByteStorage与FDT、帧导出端口组合拒绝。

Ubuntu20.04.6/WSL宿主内核、GCC9.4.0下重新构建`supervisor-control-tests`并运行`codec`，通过（`PASS strict plan/request scope and exact 64-bit status`）。本测试仅解析和验证计划，不启动接收器、不打开数据端口、不创建数据目录，未产生raw文件。本轮没有改动产品二进制；Ubuntu24的既有候选仍需由候选SHA和各自回执判断覆盖范围。

尝试对打包的Qt `getdaq-supervisor`执行`--check-config`/`--plan-config`时，沙箱禁止本机`getifaddrs`所需netlink访问，返回`Cannot inspect local receiver interfaces`；WSL受控入口另因`UtilBindVsockAnyPort: socket failed 1`无法启动。因此不能把Qt服务层检查记为通过，也不能由该环境错误断定服务实现失败。需在允许读取本机接口的Ubuntu24环境补跑服务层验证。

本轮将`tools/refresh-package-docs.py`扩展为可调用Ubuntu24候选的专用完整校验器；对上述26程序Ubuntu24候选和六程序Windows候选刷新文档。刷新前后校验通过，程序/运行依赖/回归收据保持原摘要；Ubuntu24候选仍为477项manifest文件且无`.graw`，Windows候选仍为272项。此操作只同步文档，不把71.36的测试源文件变化宣称为产品全量重建。

## 71.45 Sentry严格配置检查进入Ubuntu24候选（2026-10-21）

为避免已验证的配置修复停留在源码，本轮新增`tests/run_sentry_config_tests.py`，并扩展`tools/refresh-ubuntu24-program.py`的Sentry回归收据校验。程序先对回执runner SHA、测试二进制SHA、八个具名负例、错误退出码、无监听启动、零raw和临时根已清理逐项校验；刷新器再复制到新包暂存目录、剥离调试信息、用Ubuntu24动态加载器解析依赖并运行帮助检查，最后通过完整包校验后才替换候选。

本机Ubuntu20.04.6的GCC9.4.0/Qt5.12.8以严格C++11选项重建`getdaq-sentry`，源码构建SHA-256为`1b666aec91a2ded2cdcc5fdab82aa8a8215ae03a8f0c6742ae82c460f1fc928f`。使用项目Ubuntu24.04用户空间rootfs的loader与Qt5.15运行库执行时，八种配置（port/settle_ms小数或字符串，process_name/disk_name/bind类型错误，check_process非布尔）均在监听前以退出码2拒绝；生成0字节/0个`.graw`，私有测试目录清理完毕。可复核的测试回执随交付包保存在`build-packages/getdaq-ubuntu24-20261020-71-34-current-source-candidate/validation/sentry-config-strict.json`。

Ubuntu24候选`build-packages/getdaq-ubuntu24-20261020-71-34-current-source-candidate`原有481项manifest文件，刷新后为27个程序、483项文件。交付Sentry SHA-256为`0d4eeac8e26d4ae2827551ec547bf77bb47fe9a3a601e7af7d927d3107386a4a`，包内`validation/sentry-config-strict.json`和`validation/program-refresh-getdaq-sentry.json`记录源/交付SHA、测试收据及Ubuntu24 loader检查。候选完整性校验通过；只有Sentry在本轮重建，其他26个程序复用，不能视作全量重编译或独立Ubuntu24主机验收。

尝试通过Windows侧Qt5.14.2/MinGW重建时，WSL Windows互操作在调用`qmake.exe -query QT_VERSION`前返回`UtilBindVsockAnyPort: socket failed 1`，故本轮未更新Windows程序候选。macOS、真实板端、两台独立主机或持续DAQ吞吐未在本轮验证。该配置负例不传输DAQ载荷。

## 71.46 当前Ubuntu24 catalog候选普通TCP逐进程回归（2026-10-21）

为确认重构后的最新交付候选实际包含用户要求的无认证普通TCP，而不沿用旧候选的测试结论，本轮对候选内确切`getdaq-catalog` SHA=`dba2444b5e6ef1432a389f896c384267f7bcd8cb0a5c23a053a62f88b0586e3d`运行普通TCP专项回归。通过Ubuntu24.04 rootfs在隔离用户空间中的动态加载器执行，测试覆盖线程级SIGPIPE隔离、普通TCP新传输、断线显式续传、完整接收端读回、完成后零载荷续传、独立监听/发送CLI进程和连接拒绝；八项场景全部通过。最新回执为`build-catalog-plain-current-20261021/catalog-plain-tcp-result-71-48.json`，SHA-256=`e17cfbd8034b651a75955166872ab86cb06f8b17a20332b1fa5e587036afd637`，绑定精确候选程序、runner和两测试程序摘要。

本次合成raw累计15,245,312字节，runner预设上限64MiB（用户硬上限20GiB），生成量与删除量相同，`raw_remaining=0`，两个私有测试根为空。runner还针对第二个临时根创建失败新增异常清理：在Ubuntu24 rootfs不可写`/dev/shm`的受控失败检查中，明确报告权限错误并清除已创建的第一个临时根；成功回归则在只对该次用户/挂载命名空间生效的96MiB tmpfs `/dev/shm`上完成。测试流量只经同一Linux内核的本机loopback，没有连接另一台主机或DAQ硬件。

Ubuntu24候选原有27个程序、483项清单；本节只验证现有catalog字节，没有重编译或替换程序。Windows候选程序仍是旧版；本节也不构成独立Ubuntu24主机、macOS、真实双机网络、防火墙或持续DAQ吞吐验收。

## 71.47 catalog回归runner临时目录失败清理自动化（2026-10-21）

把71.46人工确认的初始化异常清理边界固化为`tests/test_catalog_plain_tcp_runner.py`：测试导入真实runner，注入`/dev/shm`第二临时根创建失败，核对错误向调用者传播、已创建的第一个私有根被删除、没有写出成功证据。用例通过1/1，无网络、无DAQ载荷、无`.graw`。`.github/workflows/native-data-path.yml`现于Linux和macOS job在数据面构建前运行该测试；本机测试、Python语法与workflow YAML解析通过，GitHub Actions远端尚未运行。

这是runner错误清理的自动回归，不替代完整catalog TCP集成或两台主机验收。程序、运行库和交付候选SHA不变；当前Ubuntu24候选完整性483项及Windows候选273项复核通过。

## 71.48 catalog runner保护既存证据并复测当前候选（2026-10-21）

进一步检查失败路径时发现runner曾无条件删除指定证据路径上的文件。本轮改为拒绝覆盖已有普通文件或符号链接，只用独占创建写新回执，并且仅在本次已成功创建但写入失败时删除自己的部分文件。`tests/test_catalog_plain_tcp_runner.py`现有2项故障回归：第二临时根创建失败后清理第一根；遇到已有证据时拒绝运行且逐字节保留旧内容。两项均通过。该检查没有创建raw。

使用更新后的runner对同一Ubuntu24候选catalog再次完成八项普通TCP回归。程序SHA仍为`dba2444b5e6ef1432a389f896c384267f7bcd8cb0a5c23a053a62f88b0586e3d`，新runner SHA=`4462974073dd91cc9ffb96fec0ec9687c3fa2ef85fc085d8bf88034b71583ad0`。合成raw生成并删除15,245,312字节，64MiB预算内，`raw_remaining=0`；rootfs私有测试根已清理。测试及最终清单边界仍是Ubuntu24 rootfs用户空间、宿主Linux内核和同机loopback，不能外推为真实双机或DAQ现场验收。Linux/macOS workflow已包含新的runner回归，但Actions/Mac尚未实际运行。
## 71.49 Windows Qt build entrypoint native fallback (2026-10-21)

WSL 当前调用 Windows `qmake.exe` 时可能因 `UtilBindVsockAnyPort: socket failed 1` 在启动前失败。为支持从 Windows 本机继续构建，`tools/build-windows-from-wsl.py` 和 `tools/deploy-windows-from-wsl.py` 现在根据 Python 平台选择 Windows 原生路径、`cmd.exe` 与 PowerShell；WSL 下仍使用原来的 `wslpath` 与互操作入口。入口名字保留旧名称以兼容现有命令。`docs/WINDOWS_QT5142.md` 增加用户 Qt 5.14.2/MinGW 7.3 路径的原生 Windows 操作步骤。

`tests/test_windows_build_entrypoints.py` 的3项测试验证原生路径不调用`wslpath`、批处理/PowerShell命令选择，以及WSL命令保持原互操作路径，3/3通过；测试已加入Linux/macOS原生数据工作流。另有Python编译、workflow YAML解析、catalog测试runner清理测试2/2、1287个Markdown本地链接及现有Windows候选6程序/273文件完整性检查通过。候选可执行文件和运行库未变。

本次环境是Linux，因此**尚无原生Windows Qt编译或部署通过证据**。请在Windows可访问的源码工作树根目录运行Windows专用手册第4、5节的命令；若源码只位于WSL Linux文件系统，Windows Python不能直接读取该路径。此项是构建入口与文档的准备及本机单元测试，不应表述为Windows原生构建验收。

## 71.50 接收器socket级SIGPIPE隔离（2026-10-21）

`getdaq-receiver`此前在`main()`全局忽略`SIGPIPE`，以保护在线导出socket向断开的客户端写入；这也改变同一进程其他线程的信号处理行为。现在网络工具为Linux发送使用`MSG_NOSIGNAL`，为macOS接受的状态/导出客户端socket设置`SO_NOSIGPIPE`；状态HTTP响应和分析帧推送都通过socket级保护。主程序移除了全局`SIGPIPE`忽略。

GCC 9.4/C++11干净构建`getdaq-receiver`和`exporter-lock-tests`通过。Linux本机loopback下，`exporter-lock` CTest 1/1通过：无任何全局SIGPIPE忽略时，强制重置观察客户端并在导出线程被发送调用阻塞期间继续发布，工作进程未被信号终止，随后正确断开客户端；既有并发发布锁隔离、帧顺序/长度兼容及工作线程故障清理检查仍通过。随后重跑`tests/receiver_integration.py`，常规TCP分帧/大小端/多观察者/封存、32MiB慢消费者及截断/非法帧场景均通过；32MiB合成raw只存在于fixture私有临时目录，测试退出后目录自动清除。macOS分支由`SO_NOSIGPIPE`实现，但本轮未运行macOS构建/测试；整条接收器/硬件发送路径仍需现场验收。

## 71.51 CMake 3.16接收器回归runner与候选增量刷新（2026-10-21）

为把71.50修复装入Ubuntu24候选，对照当前Ubuntu20开发基线运行`tests/run_byte_storage_tests.py`时发现：runner向CTest传`--test-dir`，但本机CMake 3.16的CTest不支持该选项，会静默在调用者源码目录运行并报告“没有测试”。现改为在指定构建目录设置`cwd`后运行CTest，并对摘要命令采用相同方式。候选程序本身没有失败；是测试启动目录兼容问题。

用`tools/build-suite.py --profile data`以GCC 9.4/CMake 3.16/C++11从当前源码构建数据工具，并另外配置`BUILD_TESTING=ON`的接收器回归目录。最终runner中的CTest与独立ByteStorage TCP集成全通过；源端receiver SHA-256=`c0749e333988546c66414cda63db53378e3f34a0dc0e17a76612e477b3a980a0`。回归合计合成`.graw` 786,818字节，低于64MiB专项上限及20GiB总限制，全部删除、`raw_remaining=0`。初次runner失败后已在修复runner的版本重跑成功；两个临时构建目录随后删除。

用`tools/refresh-ubuntu24-program.py`只替换现有Ubuntu24全量候选的`getdaq-receiver`，通过Ubuntu24 rootfs loader依赖检查和`--help`，增量测试回执及源码SHA绑定随包提供；刷新后候选27程序/484文件，delivery receiver SHA-256=`8d0f35b61fe33448760ca6e5834b0f1f14de6094282eddb85ddfcd31b65f7b3d`。这是26程序全量构建后多次增量更新的组合候选，不是27程序从当前源码全量重建，也不是独立Ubuntu24机器、真实DAQ硬件或持续吞吐验收。Windows Qt候选本轮只同步文档，其程序与运行库未变。

## 71.52 Ubuntu24候选接收器包内字节实测（2026-10-21）

直接用项目Ubuntu24 rootfs动态加载器启动候选包中的确切`bin/getdaq-receiver`，重跑`tests/receiver_integration.py`。TCP分帧、两种端序、多观察客户端、落盘与seal、慢导出消费者和畸形/截断输入场景全部通过。慢消费者流量为32MiB，文件SHA与输入一致；生成的raw仅放在fixture的私有临时目录并在正常退出时删除。没有传输超过20GiB的文件，也没有硬件访问。

本次测试对象是Ubuntu24候选交付字节（SHA-256=`8d0f35b61fe33448760ca6e5834b0f1f14de6094282eddb85ddfcd31b65f7b3d`），运行在WSL宿主Linux内核上；这加强了候选运行证据，但仍不是独立Ubuntu24物理机、真实CoBo、多机网络或持续磁盘DAQ吞吐验收。fixture跳过了需要Sentry二进制的跨进程归档附加场景。


## 71.54 attpc_conduit帧接收限制与导出回归（2026-10-21）

只读检查公开 `ATTPC/attpc_conduit` 当前 `src/backend/exporter_receiver.rs`、`src/backend/graw_frame.rs` 与 `src/backend/constants.rs`：接收器从TCP流读8字节小端`u64`长度，再精确读取消息体；GRAW解析器将消息体作为完整MFM帧读取。地址常量为11个CoBo端点 `192.168.41.60` 至 `.70`，统一端口 `46491`。其外层长度封装与本地 `apps/datareceiver/exporter.cpp` 一致，但下游解析不是无条件接受：`EXPECTED_META_TYPE=8`会严格要求MFM首字节为`0x08`。本地旧GET `CoboFormats-Rev-5.xcfg`确认Rev5大端256字节块格式满足该值；Rev0（5）、Rev1–4（6）及小端Rev5（`0x88`）无法通过这个检查。

已在[数据服务手册](DATA_SERVICE.md)写明部署映射边界：新服务的bind/export-port独立配置，必须给Conduit每个CoBo接收任务提供可达的专属端点；多路数据集中到一台主机时不能假定单端口会替Conduit区分来源。Conduit端点配置尚未由本项目改动。

更新`tests/receiver_integration.py`加入GET Rev5大端及小端帧：当前Ubuntu24候选接收器经loopback导出，Python回归按Conduit使用的小端uint64读取长度，并对两种Rev5 payload逐字节比对；两种帧均被本地接收器原样保存/导出。完整receiver集成通过，慢消费者场景32MiB且精确SHA一致，所有测试raw由私有临时目录清理。此测试验证本地出口保留了兼容/不兼容帧的头和字节，并未运行Conduit本体/Python分析管线，也没有真实socket连接、多CoBo或现场吞吐证据；不能称为下游软件联调通过。`python3 tools/check-documentation-links.py`通过：本地Markdown文件链接有效（脚本不检查锚点）。整套平台及现场验收仍未完成。


## 71.55 Mordicus ControlCore trunk 源码来源和构建范围复核（2026-10-21）

复核本地 `source/mordicus/trunk/.svn/wc.db`（只读SQLite访问）：记录repository path为`ECC/trunk`、工作副本基线revision 2010，根目录最后改动revision 1439、时间2013-11-22。快照内`src/ECC.cpp`和生成头标注Ice 3.3.1；`README`/`NEWS`为Sample模板、`ChangeLog`为0字节；`configure.ac`要求MDaq及Ice。对照当前`tools/build-suite.py`，full计划收集/构建的是发布版MDaq与GetBench所需接口及新控制服务，不编译这个旧ControlCore目录。

将该目录明确分类为历史参考代码，不根据`trunk`名称判断其更新，也不加入当前默认依赖或构建。同步更新[源码审阅方案](SOURCE_REVIEW_AND_PLAN.md)及[离线依赖说明](DEPENDENCIES_AND_OFFLINE_BUILD.md)。此外对control/data/data-host/full运行`tools/verify-dependencies.py`及`tools/build-suite.py --plan`：依赖核验分别通过95/0/1/105个文件，计划分别列出6/6/7/27个程序，不包含mordicus/trunk。`python3 tools/check-documentation-links.py`通过（1295项本地Markdown文件链接）。此为本地快照和构建计划静态审计；没有访问或证明远端SVN当前版本，也没有证明现场是否使用过该ControlCore。无产品代码、程序二进制或运行库变化。


## 71.56 Ubuntu24候选接收器四路18GiB轮转压力回归（2026-10-21）

使用当前Ubuntu24候选内的确切`bin/getdaq-receiver`（SHA-256=`8d0f35b61fe33448760ca6e5834b0f1f14de6094282eddb85ddfcd31b65f7b3d`），由Ubuntu24用户空间动态加载器在WSL宿主Linux内核启动。命令为：

```bash
python3 tests/receiver_sustained_load.py \
  build-packages/getdaq-ubuntu24-20261020-71-34-current-source-candidate/bin/getdaq-receiver \
  --evidence build-receiver/host-18gib-candidate-20261021 \
  --streams 4 --mib-per-stream 4608 --rotate-mib 1024 \
  --loader build-ubuntu24-validation/rootfs/usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 \
  --library-path build-ubuntu24-validation/rootfs/lib/x86_64-linux-gnu:build-ubuntu24-validation/rootfs/usr/lib/x86_64-linux-gnu
```

4路并发各发送4,831,838,208字节，聚合18,432MiB = 19,327,352,832字节，低于本工作区每轮20GiB硬上限。每路以1GiB轮转成4个1GiB文件和1个512MiB文件，总计20个`.graw`。全部发送SHA与轮转文件顺序拼接SHA匹配；接收完整字节/帧数、EOF确认、正常退出及durable seal均通过。全测试/读回哈希墙钟为32.70秒；该数包含Python生成和哈希，不能解释为DAQ或磁盘持续吞吐率。

证据位于`build-receiver/host-18gib-candidate-20261021/receiver-load-4th8xlkl/`：`result.json`绑定二进制、runner、loader摘要与逐流文件校验；`payload-cleanup.json`确认删除19,327,352,832字节，实际`.graw`剩余0。该目录总占用约76KiB。主机`MemTotal=52,629,581,824`字节，结果明确`payload_exceeds_physical_memory=false`，所以虽接近20GiB限制，仍不能证明负载大于内存缓存。该测试仅为单主机loopback合成数据，不是物理多主机、实板、目标网卡或现场DAQ验收。性能边界见[接收器性能手册](RECEIVER_PERFORMANCE.md)。

## 71.57 当前源码full构建、套件回归与Widgets门槛修正（2026-10-21）

在Ubuntu 20.04.6/WSL2、GCC 9.4、Qt 5.12.8开发环境，从当前源码以`tools/build-suite.py --profile full`构建27个程序；嵌入依赖检查105个文件通过。配置核心、来源迁移probe及跨文件事务probe以严格C++11构建通过；Widgets测试程序按Qt工程默认`gnu++11`配置构建通过。22项原生CTest全部通过；配置核心QtTest 149/149、Widgets QtTest 103/103；来源迁移的12个崩溃/恢复组合及2个活动写入者场景、跨文件事务回归通过。

本轮发现`tests/ubuntu24_platform_validation.py`仍要求Widgets恰好102项，而当前源码的`sentryConfigStrictFields`使完整Widgets结果为103项；核心为149项。已将runner的Widgets门槛改为103，并通过Python语法检查。随后通过`tools/validate-ubuntu24-from-wsl.py`尝试完整`--stage all`，但现有WSL入口在启动Windows辅助进程时再次报`UtilBindVsockAnyPort: socket failed 1`并以1退出，Ubuntu24 runner没有启动。调用计划/退出码小回执保留在`build-ubuntu24-validation/tasks/`。需在可用Ubuntu24主机或修复后的WSL入口重新执行；本轮Ubuntu20套件结果不能替代Ubuntu24运行。

用本轮全量构建的`getdaq-receiver`运行12项TCP/ZBUF/FDT协议与出口测试（总wire 49,739字节），并进行4路各256MiB、1GiB聚合接收/轮转/哈希/EOF/seal回归；负载安全runner的9个清理/预算测试通过。所有合成`.graw`均按回执删除，两个接收测试套件的raw余量均为0，合计远低于20GiB。证据和输入/程序摘要在`build-receiver/current-source-full-suite-20261021/`；当前源码接收器SHA-256=`c0749e333988546c66414cda63db53378e3f34a0dc0e17a76612e477b3a980a0`。这些测试在Ubuntu20开发宿主及本机loopback完成，不代表Ubuntu24、独立主机、macOS、Windows人工验收、真实板端或持续DAQ吞吐。

## 71.58 Sentry 示例对齐当前接收器进程名（2026-10-21）

通用 `config/sentry/node.example.json` 此前仍检查旧进程名 `dataRouter`；按示例部署新接收器时，Sentry状态会把新进程报告为缺失。现改为检查 `getdaq-receiver`。该名字正好15字节，符合Linux `ps -o comm` 的名称宽度。旧 dataRouter 与 Windows `DataExporter.exe` 的部署仍可在配置中填写现场真实进程名。归档专用的 `native-receiver.example.json` 保持 `check_process=false`，因为归档在接收器退出并完成封存后进行；两个样例现在分别表达“运行中进程监控”和“封存后归档”生命周期。

新增 `tests/test_sentry_examples.py` 并接入Linux/macOS原生数据CI，锁定两个样例的进程名、检查开关和独立数据目录。该测试、workflow YAML解析及1292条本地Markdown链接检查通过；没有生成DAQ负载或修改程序二进制。GitHub Actions尚未运行，旧平台候选包尚未刷新，本条不构成目标平台或现场部署验收。

## 71.59 Sentry 生命周期说明与样例一致（2026-10-21）

复核71.58更改后发现[DATA_SERVICE.md](DATA_SERVICE.md)仍将通用样例描述为传统`dataRouter`进程检查，与实际`node.example.json`的新`getdaq-receiver`默认值不一致。现已改为说明通用样例监控新接收器；部署旧`dataRouter`或Windows `DataExporter.exe`时，应在本机配置副本里填入实际进程名。归档样例仍要求接收器退出，依赖目录锁和封存凭据检查。

扩展`tests/test_sentry_examples.py`，同时检查两个JSON样例和数据服务手册的进程名/生命周期说明，避免样例变化后操作手册留下相反指引。测试通过（无raw生成）；`tools/check-documentation-links.py`通过1293条本地Markdown文件链接。离线依赖清单105项通过。已将最新文档刷新到Ubuntu24候选（27程序/484文件）和Windows候选（6程序/263文件）；两边包校验均通过，刷新工具确认程序与运行库保持原摘要。此为文档同步，不是新源码构建、目标机运行或现场验收。

## 71.60 Sentry 对真实接收器进程的运行态检查（2026-10-21）

扩展`tests/receiver_integration.py`：在同机启动当前源码构建的`getdaq-receiver`和Sentry，配置`process_name=getdaq-receiver`、`check_process=true`；receiver运行期间`/status`返回200并回报进程名，receiver正常退出后同一接口返回503并指出进程缺失。该测试直接覆盖此前仅由JSON静态检查保护的Linux `ps -o comm` 运行逻辑。Sentry已使用同一runner继续通过原有接收、慢导出及封存归档集成场景。

Ubuntu20.04.6/GCC9.4/Qt5.12.8从当前源码构建；接收器SHA=`c0749e333988546c66414cda63db53378e3f34a0dc0e17a76612e477b3a980a0`，Sentry SHA=`1b666aec91a2ded2cdcc5fdab82aa8a8215ae03a8f0c6742ae82c460f1fc928f`，集成runner SHA=`d37e626dd11310a30b46acc554c7fbbbb74e3c6b3513b8e03763fbb99315ed8b`；六组集成结果退出码0。回归合成输入合计33,591,296字节（含32MiB慢导出夹具），远低于20GiB上限；所有`.graw`由私有临时目录清理，剩余0。精简SHA绑定收据见`build-receiver/sentry-process-check-20261021.json`。最新文档已刷新至Ubuntu24候选（27程序/484文件）和两个Windows候选（分别6程序/273文件、6程序/263文件），完整性检查通过且二进制保持不变。Ubuntu/macOS GitHub Actions尚未运行；此为单机loopback证据，不是独立平台、双机、实板或持续DAQ吞吐验收。

## 71.61 macOS全套CI增加Sentry进程生命周期联调入口（2026-10-21）

此前`macos-host-suite`只构建并检查full profile的27个程序，不运行receiver/Sentry集成。现在在该job构建回执通过后，执行`tests/receiver_integration.py`，输入来自同一full build目录的`getdaq-receiver`、`getdaq-inspect`和Qt `getdaq-sentry`，覆盖Mac的POSIX `ps -A -o comm=`进程名识别、活进程200/退出503以及已有TCP/慢导出/封存用例；最终always步骤检查工作区没有残留`.graw`。合成负载沿用runner的有界32MiB慢客户端和小型帧夹具，低于20GiB总上限。

本机用PyYAML解析workflow通过，核对full profile生成路径确为`build-platform/macos-full-suite/full/receiver/`与`.../bin/`，并确认步骤顺序为完整构建/回执检查后再运行集成和清理检查。同步更新[Mac构建指南](MACOS_BUILD.md)，区分无Qt数据面job和安装Qt的full-suite job，并提供从本机full build产物执行同一Sentry生命周期集成的命令。最新指南已刷新到Ubuntu24候选（27程序/484文件）和两个Windows候选（6程序/273文件、6程序/263文件），包校验通过且程序/运行库摘要保持不变。Linux同一runner在71.60已通过；GitHub Actions的macOS runner尚未运行，不能据此声称macOS编译或运行验收。源码检查不生成DAQ数据，未改产品二进制。后续应观察真实Actions结果并处理Mac专属差异。
## 71.62 Ubuntu24当前源码完整套件GitHub Actions入口（2026-10-21）

为避免Ubuntu24 WSL辅助入口在Windows进程启动阶段因`UtilBindVsockAnyPort: socket failed 1`而无法启动验证runner，`.github/workflows/native-data-path.yml`新增`ubuntu24-current-source-full-suite` job，使用GitHub托管`ubuntu-24.04`虚拟机执行`tests/ubuntu24_platform_validation.py --work /tmp/getdaq-ubuntu24-full-suite --jobs 2 --stage all`。job安装`build-essential`、CMake、Python及Qt5开发包，输出放在源码树外的隔离`/tmp`路径；平台runner在启动前核验Ubuntu24.04、非root身份、x86_64和至少4GiB可用空间。

该`all`阶段从当前检出源码构建full profile的27个程序，以及严格C++11配置核心、迁移/事务probe和启用`-pedantic-errors`的native CTest；Widgets测试按Qt工程的C++11标准构建。随后要求Qt核心149/149、Widgets103/103、当次注册的所有CTest通过，并执行接收协议、安全和四路各256MiB的回归。runner汇总整个接收测试wire字节，不超过20GiB；四路合成raw自动删除，要求隔离工作目录无`.graw`残留。工作流额外添加`if: always()`清理检查，只扫描固定的runner专属`/tmp/getdaq-ubuntu24-full-suite`目录，发现失败遗留时删除并令job失败。

本机静态核验：PyYAML成功解析workflow；检查了job运行平台/超时、完整阶段命令、步骤先后、源码树外工作目录及always清理；平台runner`--help`接受`all`阶段，`tools/build-suite.py --profile full --plan`确认完整计划为27个程序。另配置always上传14天的构建/测试摘要、平台回执、测试证据JSON和诊断日志；上传清单不包含`.graw`。静态检查没有执行Ubuntu24平台runner，也没有生成raw。GitHub Actions尚未实际运行，因此当前只能证明验收入口已配置，不能声称Ubuntu24 CI通过、独立部署机验收、macOS/Windows验收、真实硬件联调或持续DAQ吞吐测试。执行后应记录workflow run/commit身份、完整job结果及清理报告，再更新平台状态。


## 71.84 失败节点交接补齐Qt窗口本机联调（2026-10-21）

GetDAQ新增菜单入口 **Failed data-node handoff completion...**，用于受限的“原节点服务仍为同一实例、原控制权未变化、原`handoff-control`已明确失败、接收周期正常停止并封存”场景。窗口通过`QProcess`调用同目录`getdaq-supervisor-query`，不在Qt线程实现协议/传输、不打开或转发`.graw`。请求生成阶段只做只读核验；保存后由操作者检查，提交需再次明确确认，当前窗口只允许启动一次提交。加载保存请求后仅开放观察。

Ubuntu20.04.6/WSL2、GCC9.3、Qt5.12.8下，`tests/tests.pro`当前源码完整Widgets QtTest结果为 **105 passed, 0 failed, 0 skipped**。新增`failedDataHandoffCompletionIsReachableButInitiallyReadOnly`通过，核对菜单动作、窗口可打开且提交/观察初始禁用、结果区只读。第一次在受限sandbox运行时，13项现有需要本机TCP监听的测试无法bind；在允许loopback的环境完整重跑后105项全通过。Qt offscreen平台打印的`propagateSizeHints`/`raise`警告不影响结果。

另由生产窗口进程连接本机真实DataSupervisor/receiver合成夹具，故障注入使原handoff证明写入失败。窗口生成规范补齐请求；取消确认没有POST，随后明确确认仅提交一次，观察结果完成；重新加载原请求只允许观察。原失败历史保持，旧owner仍被拒绝、successor核验成功，服务冷重启后控制隔离门槛仍在。窗口测试收据`build-receiver/failed-handoff-window-71-84/failed-handoff-43avu009/result.json`记录`passed=true`、`raw_remaining=0`、`maximum_aggregate_test_bytes=67108864`；实际两个合成raw各4096字节，共8192字节，已删除。结果绑定服务SHA=`b8b6d39313426ddddc65ed6f77c1eb3b434f216d298f06c65ccaae8f59b0ed89`、接收器SHA=`bb5888ce68948d93a306e50b91eeee0987e0933c5c4a159c5daf28e08af0ea7b`、查询CLI SHA=`f2b1f21b5cf57e6306dc9dad47aa7a1bc2f5f90b78a3e6115a846bbd2ca0b2ea`、Qt窗口探针SHA=`6f8f6ef6b76313ac2b69aa43b2f66eb462b4e1ab534ee2a9b34525033a0c4720`。窗口动作回执为`completion-request.json.window-result.json`。

该证据只覆盖Ubuntu20用户空间、本机loopback、合成服务和数据生命周期，不是Ubuntu24整套重建、两台独立主机、Windows新源码GUI人工操作、macOS、真实板卡或持续DAQ吞吐验收。板端`getHwServer`/`IceEirfu`不在重构范围；Qt仍只承担低频控制/元数据，DAQ数据路径保持原生C++。原手册[失败节点交接补齐](FAILED_CONTROL_HANDOFF_COMPLETION.md)已补充窗口操作步骤。Ubuntu24 full suite仍需在这次新增代码后重跑，并产出包含新窗口的交付包。

## 71.85 Ubuntu24.04.5当前源码全套验证纳入失败交接窗口（2026-10-21）

在经官方签名和SHA256验证的Ubuntu24.04.5用户空间中，经项目WSL runner以非root用户对当前源码执行`tests/ubuntu24_platform_validation.py --stage all`。GCC13.3.0、CMake3.28.3、Qt5.15.13环境下，full profile 27/27个程序构建成功；Core QtTest149/149、Widgets105/105、原生CTest22/22、配置迁移、跨文件事务、接收协议/安全及四路接收回归通过。四路接收总载荷1GiB；全部测试wire汇总1,073,799,755字节，远低于20GiB。窗口失败交接夹具复用64MiB硬上限，实际生成并删除8192字节raw；整轮runner确认`raw_remaining=0`且测试工作区无`.graw`。

平台回执：`build-ubuntu24-validation/rootfs/build/getdaq-platform-71-84/platform-deb54f0096554b9d89b4470dfd9d84df.json`；窗口子场景记录`failed_handoff_window_scenario.passed=true`、`original_failed_preserved=true`、`exact_live_completion=true`、`successor_owner_observed=true`、`cold_restart_fenced=true`。当前源码full构建输出用于制作独立候选`build-packages/getdaq-ubuntu24-20261021-71-84-current-source-full-candidate/`，27程序；GetDAQ另经`refresh-ubuntu24-program.py`完成Ubuntu24 loader依赖、`--help`、offscreen启动、ready日志和截图检查，并执行UI元数据/SOAPAction专项测试。该候选GetDAQ交付SHA=`df50dc5db337414d8d0155cc103a178b4b5456dbb8a9c4940620a0cc25124e22`，来源构建SHA=`783f29583a3722835d488429a50b93d9ee92442a607595d3cd882f2a349ad0fb`。包内刷新回执为`validation/program-refresh-GetDAQ.json`；专项UI回归为同次Ubuntu24 runner的`evidence/getdaq-ui-package.json`。

Ubuntu24 suite和候选运行于用户空间加载器/WSL2宿主内核，不是独立Ubuntu24机器；loopback服务、receiver数据均为合成测试，不是实验室板端、双机网络或持续物理磁盘性能证明。无生产数据写入。

## 71.86 Windows Qt5.14.2/MinGW当前源码失败交接窗口构建和运行包（2026-10-21）

使用用户提供的`D:\Software\QT\5.14.2\mingw73_64`和MinGW7.3构建当前源码六个操作程序；完整构建收据`build-windows-qt5142-71-84/windows-build-receipt.json`绑定所有程序SHA及x86_64 PE。另以严格C++11构建Windows原生QtTest：Widgets **104 passed、0 failed、1 skipped**（跳过项是Unix专属文件锁测试；总用例105，新增`failedDataHandoffCompletionIsReachableButInitiallyReadOnly`通过），ECC客户端63/63。runner `tests/run_windows_qt_validation.py --only all`返回退出码0，清理回执`build-windows-qt-validation-71-84/native-windows-results/cleanup-receipt.json`确认临时测试根删除、raw删除0字节、聚合上限20GiB未触发。当前六程序Windows包`build-packages/getdaq-windows-qt5142-20261021-71-84-current-source-candidate/`有263个manifest文件，SDK-free PATH检查和原生窗口smoke通过。没有传输DAQ数据。

这是用户实际Windows Qt Kit、同一用户机器和本地源码共享目录上的验证；不代表独立干净Windows部署、人工连接现场服务、真实ECC/gSOAP或硬件。完整回执和限制见Windows候选manifest及[Qt5.14.2构建手册](WINDOWS_QT5142.md)。

## 71.87 Windows缺失助手保护和当前源码重建（2026-10-21）

复核71.86候选后发现，Windows GetDAQ 虽然能打开失败节点交接窗口，但包内没有同目录`getdaq-supervisor-query.exe`；Linux候选包含该CLI，而其服务核心依赖POSIX实现，不能直接在MinGW下构建。新增应用启动时的helper可用性检查：helper缺失/不可执行时显示显眼说明，禁用请求生成、提交和远端观察；本地请求文件仍可只读加载。新增`failedDataHandoffDisablesOperationsWhenNativeQueryHelperIsMissing`覆盖这一保护。此项使Windows运行界面准确表达限制，但交接恢复流程依然没有Windows实现，不能记作Windows全功能通过。

Ubuntu20.04.6/Qt5.12.8/GCC9.3的当前源码完整Widgets QtTest在允许loopback监听的环境 **106 passed, 0 failed, 0 skipped**。首次受限环境运行时13个绑定localhost的既有测试失败；放开本机loopback后全套通过。`tests/test_windows_build_entrypoints.py` 13/13通过，Ubuntu24 runner脚本Python语法检查通过。

重新使用用户实际`D:\Software\QT\5.14.2\mingw73_64`/MinGW7.3从当前源码构建六个Windows控制程序；严格C++11 Windows QtTest结果为Widgets **105 passed, 0 failed, 1 skipped**（Unix专属文件锁用例跳过，共106项），ECC客户端63/63。原生runner退出码0，清理回执确认私有目录已删、raw为0、20GiB总测试载荷限制未触发。新候选为`build-packages/getdaq-windows-qt5142-20261021-71-87-current-source-candidate/`，六程序/263 manifest文件；SDK-free PATH帮助及offscreen和原生窗口smoke通过。构建/测试输出在`build-windows-qt5142-71-87/`和`build-windows-qt-validation-71-87/`。

新UI改动之后Ubuntu24完整`--stage all`尚未重跑；此前71.85结果不代表这次工作树的Ubuntu24验收。当前Windows也不能执行交接恢复流程；独立干净Windows、macOS原生、真实服务/板卡、双机普通TCP及持续DAQ吞吐仍未验收。没有生产DAQ数据经过窗口。

## 71.88 Ubuntu24当前源码full套件与候选包（2026-10-21）

在Ubuntu24.04.5用户空间/WSL2宿主内核经项目runner运行`tests/ubuntu24_platform_validation.py --work /build/getdaq-platform-71-87 --jobs 3 --stage all`，针对包含71.87窗口缺失helper保护的当前源码完成full profile重建。GCC13.3.0、CMake3.28.3、Qt5.15.13环境中27/27程序构建成功；Core QtTest149/149、Widgets106/106、CTest22/22、配置迁移/事务、receiver协议/安全及失败交接窗口生产进程故障注入回归全部通过。

Runner回执`build-ubuntu24-validation/rootfs/build/getdaq-platform-71-87/platform-afa5805edec749249d57a9afcd7136fc.json`绑定本次平台/测试结果：合成wire总量1,073,799,755字节（receiver四流共1GiB），低于20GiB总测试上限；交接窗口夹具实际生成并删除8192字节raw，最终`raw_remaining=0`。窗口结果标明原失败保留、准确补齐完成、successor owner观察成功及冷重启后仍fenced。首次在默认sandbox权限下，13个localhost监听用例不能bind；在显式允许本机loopback监听后完整runner通过。

基于同一full build receipt制作Ubuntu24候选`build-packages/getdaq-ubuntu24-20261021-71-87-current-source-full-candidate/`，包含27个当前程序/174 manifest文件。候选包内GetDAQ以Ubuntu24 loader运行`--help`、offscreen启动、ready日志、截图检查；GetDAQ与QtTest源程序摘要分别为`983500b25bac08eccd777de50b580237dbdf32649260759a18144d4a68800d0f`和`a8a8b57ca2d7d9fbb92bae67ce8e90a6455aa6b7317f0c3eafc74b8678efdfc9`，包专项UI回归通过，raw为0。候选完整性和文档刷新前后校验通过。本环境共享WSL2宿主内核，不证明独立Ubuntu24、现场服务/板卡或持续DAQ磁盘性能；总体重构仍未完成。
## 71.91 Windows 原生监督器查询客户端与七程序包（2026-10-21）

- 为解除Windows失败交接窗口因同目录缺少`getdaq-supervisor-query.exe`而禁用操作的限制，将共享C++11协议target与客户端分开。Windows查询CLI采用WinSock实现固定长度HTTP、单次POST/不重试，Win32凭据读取验证当前用户持有的受保护DACL（仅当前用户及LocalSystem ACE）、普通文件/单链接/非reparse和内容持续一致；本机请求文件也使用有界、非reparse读取。Windows命令行使用`wmain`，检查UTF-16代理对后转为UTF-8。该CLI为低频元数据控制，不处理原始DAQ帧。该凭据认证HTTP与用户决定的无认证普通TCP归档复制为不同功能。
- 使用用户指定`D:\Software\QT\5.14.2\mingw73_64`及MinGW7.3工具通过真实Windows qmake/g++从当前源码构建7个控制程序。`getdaq-supervisor-query.exe` SHA-256=`2306cb1b9e5ca5a48c8d9e3d411f434248f91921278e8b055328bd2a90428d45`；PE导入含`WS2_32.dll`、`ADVAPI32.dll`、系统DLL及MinGW运行库，无Qt DLL依赖。候选`build-packages/getdaq-windows-qt5142-20261021-seven-program-query-candidate`通过七程序/268文件完整性检查、SDK PATH剥离后的七个`--help`、GetDAQ offscreen初始窗口及原生Windows窗口smoke。
- Linux CMake query目标和独立qmake项目均构建成功；GCC9.4目标CTest `supervisor-control-codec`、`receiver-catalog-filesystem`通过2/2。`tests/test_windows_build_entrypoints.py` 14/14、相关Python入口语法检查和1335项本地Markdown链接通过。构建及启动smoke没有生成DAQ raw。
- 已新增`tests/windows_supervisor_query_integration.py`并接入Windows GitHub Actions，针对受保护临时凭据、一次身份查询和401后不重试的回归当时尚未运行。新包可以让GUI找到helper，但完整成功/失败远程交接流程、独立干净Windows、macOS、两台真实主机、现场硬件/吞吐仍待验收。71.88 Ubuntu24全套验证没有在本轮源码变化后重跑。整体重构未完成。

## 71.92 Windows监督器查询客户端原生loopback回归（2026-10-21）

- 使用本机Windows Python 3.10运行`tests/windows_supervisor_query_integration.py`，验证Windows helper的受保护临时ACL凭据、401拒绝且不重试、成功身份GET及单次控制POST/202接受回执；凭据未出现在输出，临时根已删除。当前完整回执见71.94。
- 尝试把WSL UNC目录指定为Windows测试临时根时，`icacls`无法在该共享路径设置受保护NTFS ACL；改用Windows自身`%TEMP%`运行后通过。GitHub Actions仍使用工作区内Windows本地临时根并负责清理。
- 本回归覆盖helper认证及一次POST接受，不覆盖服务端完整状态机、独立干净Windows或现场服务。Windows GitHub Actions、macOS、双机普通TCP、实板与持续DAQ吞吐仍待完成；整体重构未完成。

## 71.93 Ubuntu24当前源码全套复验及候选包（2026-10-21）

- 在Ubuntu24.04.5用户空间/WSL2宿主内核上，从当前源码重建full profile的27个程序及测试程序；Core QtTest **149/149**、Widgets QtTest **106/106**、原生CTest **22/22**、配置迁移/事务、receiver传输协议、安全、四路接收和失败交接窗口服务联调全部通过。纳入Windows控制客户端GET/POST测试脚本后的最新平台回执：`build-ubuntu24-validation/rootfs/build/getdaq-platform-71-93/platform-75c484c809d144198985f8c79d9db0e5.json`；测试总wire为1,073,799,755字节，低于20GiB；窗口夹具8192字节raw已删，整轮`raw_remaining=0`。
- 首次执行发现测试runner将`getdaq-supervisor-query`固定拼为`delivery/full/native/getdaq-supervisor-query`，但当前full profile把该程序输出到`delivery/full/bin/`，导致交接测试启动前找不到helper。现改为从本轮build receipt读取监督服务、接收器和query helper的路径；新增测试保护路径选择，`tests/host_suite_delivery_test.py` **5/5**通过。之后重跑同一隔离work根的全套验证成功。
- 从该全量构建产物制作候选`build-packages/getdaq-ubuntu24-20261021-71-93-current-source-full-candidate`：27程序/172 manifest文件，包验证通过，27个程序SHA逐一与平台验证回执匹配。通用`build-suite.py --stage`已修复为写入包内`bin/<程序名>`相对路径，避免manifest引用构建主机/chroot路径；Ubuntu24文档刷新器在宿主环境通过，证明候选可移动并独立复核。包保留当前程序和文档。该结果是WSL2宿主内核上的Ubuntu24用户空间证据，不是独立机器验收；GitHub Actions、macOS原生、双机普通TCP、真实旧服务/板卡及持续DAQ吞吐仍未验证。测试raw为0。

## 71.94 Windows监督器查询成功身份与单次POST回归（2026-10-21）

- 扩展`tests/windows_supervisor_query_integration.py`，对本机真实Qt5.14.2/MinGW helper运行两个mock场景：受保护ACL凭据的identity GET收到401后只请求一次并以失败退出；另一个场景返回有效身份/规范计划，helper校验plan摘要与请求范围，再只发送一次`POST /v1/operations`，核对202 accepted响应中的操作ID、command、request key、owner和plan字段。
- Windows Python 3.10本机实跑通过，helper SHA-256仍为`2306cb1b9e5ca5a48c8d9e3d411f434248f91921278e8b055328bd2a90428d45`。先针对构建输出运行，再针对Windows候选包内同SHA helper完整重跑。回执`build-windows-qt5142-query-port/query-tests/supervisor-query-loopback-post.json`及`supervisor-query-packaged-loopback.json`：401场景1个请求/不重试，成功场景1个GET+1个POST、状态200/202且接受回执校验通过；三次总请求，凭据未记录，临时根已删除。没有原始DAQ或大文件传输。
- Windows GitHub Actions已调用同一脚本入口，但Actions结果尚未取得；mock只验证客户端协议/单次POST结果核验，不运行监督器服务端状态机，不代表独立Windows、跨主机、现场或硬件验收。完整交接和其他平台验收仍待完成。

## 71.95 最新Ubuntu24候选包普通TCP传输复验（2026-10-21）

对71.93全量构建生成的Ubuntu24候选包内**确切**`getdaq-catalog`重新运行`tests/run_catalog_plain_tcp_tests.py`。在Ubuntu24.04.5用户空间/WSL2宿主内核中，当前候选SHA-256=`e5ab61dae6aaeaa23dd8a3039280f32e4712fdb5e9f11105e29314f90db88646`；测试程序由当前源码以GCC13.3/C++11重新构建。无认证普通TCP、独立CLI监听/发送、连接拒绝上限、线程级SIGPIPE隔离、断线后的显式partial续传、接收端完整读回及完成后零载荷续传均通过。收据`build-ubuntu24-validation/rootfs/build/getdaq-platform-71-93/catalog-tcp-current/current-package-plain-tcp.json`绑定确切候选程序SHA、两个测试程序摘要及runner SHA。

本轮合成raw生成并删除15,245,312字节，预算上限64MiB，`raw_remaining=0`且两个私有测试根为空。独立候选`build-packages/getdaq-ubuntu24-20261021-71-95-current-source-full-candidate`保留相同27个程序，并在173项清单中绑定该回执；刷新文档及回执后，候选完整性再次通过。回归使用同机loopback，不是两台独立主机、现场防火墙/网卡或长时间吞吐验收；普通TCP不认证、不加密，仍应只在可信且受防火墙限制的网络使用。候选及操作边界见[跨主机普通TCP操作说明](CATALOG_TRANSFER.md#13-普通-tcpip-发送已完成归档)。

回归runner现在输出可复核的候选标识、程序和runner SHA、场景清单、合成数据预算与清理计数。`tools/package_ubuntu24_catalog_ssh.py --record-catalog-plain-tcp-test`只在候选路径和`getdaq-catalog` SHA匹配、所有必需场景为真、私有测试根为空且数据删除核算一致时才把收据加入包清单；包测试14/14通过，测试已列入Linux/macOS原生数据工作流。这里是本机回归和workflow静态配置证据，GitHub Actions/macOS远端job仍需实际运行。

## 71.96 Windows查询helper不确定POST结果不自动重试（2026-10-21）

使用用户提供的Windows Python 3.10，在Windows本地临时目录运行当前Windows候选包内的确切`getdaq-supervisor-query.exe`。helper SHA-256=`2306cb1b9e5ca5a48c8d9e3d411f434248f91921278e8b055328bd2a90428d45`，与候选manifest逐字节匹配。loopback场景包括：身份GET收到401后停止；有效身份GET后只发送一次POST并验证202 accepted；POST已被服务端完整接收、随后服务端断开连接；以及服务端完整接收POST后返回HTTP 500。后两种均以错误结束、不报告成功、不发送重试。测试服务器在预期请求之后继续监听至客户端超时窗口，以发现延迟重试；四个场景累计7个HTTP请求，未检测到重试或凭据回显。

回执为`build-windows-qt5142-query-port/query-tests/supervisor-query-loopback-no-retry.json`；受保护测试凭据、Windows本地临时目录和包装helper临时目录均已清理。本次不产生DAQ raw。该结果只验证Windows查询客户端的HTTP调用次数与结果处理，不验证服务器端状态机、独立部署机、网络故障下服务端是否已持久接受、完整GUI交接或实板行为；连接断开/500后的真实操作状态仍必须通过只读查询核对，不能换操作ID重发。Windows GitHub Actions尚未运行。

## 71.97 当前Linux host full候选构建与归档路径回归（2026-10-21）

在Ubuntu 20.04.6用户空间、WSL2 Linux 5.15.167.4宿主内核中，使用GCC 9.4.0、CMake 3.16.3、Qt 5.12.8从当前源码执行`tools/build-suite.py --profile full`，构建回执为`complete`，包含27个主机端程序。候选`build-packages/getdaq-linux-host-20261021-current-source-full-candidate`经文档刷新和host package verifier复核，包含173个清单文件。数据接收、检查、目录归档和查询程序经`check-no-qt-runtime.py`复核无Qt运行时依赖。

当前候选的原生回归通过：接收器TCP集成覆盖分片、多字节序、导出观察、精确落盘/封存、慢消费者和错误帧，慢消费者场景写入32MiB合成raw且逐字节SHA一致；归档filesystem、catalog transfer和控制协议CTest通过3/3。普通TCP完整CLI回归覆盖无认证收发、连接拒绝上限、中断后显式partial续传、接收端读回及完成后零载荷续传。回执`validation/catalog-plain-tcp-loopback.json`绑定候选内确切`getdaq-catalog` SHA、测试程序和runner SHA，并明确记录Ubuntu 20.04/WSL2宿主loader；合成raw生成/删除各15,245,312字节，预算64MiB，`raw_remaining=0`。

测试runner现在必须由调用者传入`--runtime`，并将原样值写入回执，避免把任意宿主loader误记为Ubuntu24 rootfs。上述TCP测试只证明同机loopback与宿主Linux文件系统，不代表macOS、独立主机互通、真实板卡、现场网络或持续DAQ吞吐；macOS Actions也尚无实际运行结果。所有传输测试数据低于用户规定的20GiB并在结束后清理。


## 71.98 当前Linux host交付包普通TCP归档回归（2026-10-21）

针对当前Linux host full候选包内确切`getdaq-catalog`执行普通TCP传输专项回归。候选`build-packages/getdaq-linux-host-20261021-current-source-full-candidate`含27个程序和173项清单；包内程序SHA-256=`f1251e3b243a29d03f96e2a74ad469938f47537276e203ba29f25ff324bff89d`。独立CLI监听/发送、无认证普通TCP、连接拒绝上限、SIGPIPE线程隔离、中断后显式partial续传、完整读回及完成后的零载荷续传场景通过；收据`validation/catalog-plain-tcp-loopback.json`绑定确切交付程序、测试程序和runner摘要。

回归在Ubuntu20.04.6用户空间/WSL2宿主内核运行，最大预算64MiB，生成并删除15,245,312字节合成raw，收据确认raw与私有测试根剩余均为0。候选包刷新文档后完整性验证通过。该证据是本机loopback，不代表独立双机网络、现场防火墙/磁盘、macOS、板端或持续DAQ吞吐；普通TCP不认证、不加密。

## 71.99 macOS full CI增加操作端QtTest门槛（2026-10-21）

`.github/workflows/native-data-path.yml`的`macos-host-suite`在Qt 5.15.2 clang_64全量主机程序构建及收据检查后，使用`tests/build_qt_control_validation.py`分别构建Widgets和ECC客户端QtTest，以offscreen执行并检查每套非零通过、零失败。测试使用工作区内私有`TMPDIR`；`if: always()`清理步骤统计并删除其中raw，限制20GiB；工作流上传两套构建收据、源码摘要、结果和构建日志，保留14天。`tests/test_windows_build_entrypoints.py`覆盖工作流步骤顺序与限额合同。

本地YAML与4段内嵌Python语法检查通过，工作流合同测试15/15，普通TCP runner清理测试2/2，文档链接检查1336项通过；这不是远端Actions证据。GitHub macOS runner尚未执行，不能声称Apple Clang、Qt macOS插件或Mac QtTest已通过。

## 71.100 Linux QtTest复验及macOS工作流命令检查（2026-10-21）

为核对新增的macOS full workflow QtTest步骤，在Ubuntu 20.04.6/WSL2、GCC 9.4.0、Qt 5.12.8环境中使用同一C++11构建助手从当前源码构建`tests/tests.pro`和`tests/ecc_system_client.pro`，并以offscreen运行：Widgets 106/106通过，ECC客户端63/63通过，均为0失败、0跳过。私有临时根位于`/tmp/getdaq-macos-qttest-command-check/qt-test-temp`；运行后核算`.graw`为0个/0字节并删除该临时根。

此结果验证了构建助手、QtTest命令及源码测试在Linux/Qt5.12上的可执行性，不是macOS/Apple Clang/Qt macOS kit实测。`.github/workflows/native-data-path.yml`中的macOS full suite现配置为全量构建后再构建并执行Widgets与ECC客户端QtTest，收集结果并在总量不超过20GiB的门槛内清理临时raw；GitHub macOS runner尚未实际运行，必须等待其真实job结果才能确认Mac构建、插件加载与测试通过。独立Mac、部署目标卷、双机普通TCP、真实板端和持续DAQ吞吐仍未验收，整体重构未完成。


## 71.101 Windows Qt5.14.2 当前源码 Widgets 与 ECC客户端回归（2026-10-21）

在用户Windows主机使用指定的Qt 5.14.2 MinGW7.3重新构建当前源码`tests/tests.pro`与`tests/ecc_system_client.pro`，构建输入收据覆盖917个源码/配置输入，状态`complete`。Windows原生QtTest结果：Widgets 105通过、0失败、1跳过（Unix专属锁测试）；ECC客户端63通过、0失败、0跳过。Widgets测试程序SHA-256=`e0eb9140e502e534d49f264a585623a6822c28c0eec1948715f80d96cc0950d1`，ECC客户端测试程序SHA-256=`b2fe631077faed39b7260a9cf24c770ea299f7aa5b421898224baa615a448771`。

构建与结果位于`build-windows-qt5142-20261021-current-qt-tests-101/`。Windows原生运行器使用私有TEMP；清理回执确认临时根已删除、无raw生成/残留、20GiB限额未触发。该项是用户当前Windows机器上的本机Qt/MinGW测试，不是独立干净Windows部署，也不代表Linux ECC服务、macOS、板端或真实DAQ验收；没有接入生产数据。


## 71.102 当前候选 AGET 系统工件到原生 Prepare/Configure 模拟联调（2026-10-21）

针对当前 Linux host full 候选内确切`getdaq-config`运行`tests/aget_compiler_integration.py`与`tests/system_experiment_integration.py`。单片和整板 CoBo AGET 离线编译、金值寄存器、混合型号/共享约束、来源摘要、非法配置拒绝及失败不产生/覆盖工件通过；系统配置编译、工件摘要检查、实例/节点绑定和异常工件拒绝共37个进程场景通过，旧配置源文件字节保持不变。两项测试均设置私有`XDG_CACHE_HOME`，避免验证用户缓存目录权限影响运行；私有缓存已随测试结束删除。

另以当前源码从`third_party/ice-compat`构建`getdaq-system-config`和`hardware-system-preparation-tests`，将完整系统工件交给原生`SystemPreparationClient`及本机模拟GET Ice节点，分别以Ice编码1.0、1.1完成Prepare和Configure。模拟读回检查包含编译所得readoutDepth、AGET活动掩码及initDone；审计绑定配置/工件摘要，并明确`actual_loaded_configuration_verified=false`。回执`build-platform/aget-system-hardware-71-102/aget-system-integration-receipt.json`绑定确切程序和输入SHA；3,119,952字节小型审计日志保存在该工作目录，fixture临时目录已删除，raw生成/残留均为0。

这是Linux/WSL2本机候选和模拟硬件验证，不是实板寄存器、电气时序、时钟同步、现场服务、独立Ubuntu24、macOS或长时间DAQ验收。它验证整系统编译工件能够接入原生Prepare/Configure，不改变此前的硬件边界声明。

## 71.103 移除已弃用的归档 HMAC 代码并刷新 Linux 全量候选（2026-10-21）

用户已明确选择归档跨主机链路使用无认证普通 TCP/IP。生产发送/监听早已采用该协议，但公共 SHA-256 源文件仍附带没有调用者的 HMAC-SHA-256 和定时字符串比较函数，系统配置测试也保留认证向量。本轮删除这两个未使用 API、实现及相应测试断言；保留文件 SHA-256、增量 SHA-256 和其向量测试。历史 Ubuntu24 文档将70.97章节标题标明为已被普通 TCP 替代，避免旧方案被误认为现行功能。

严格 C++11/GCC9.4从当前源码构建`system-experiment-tests`并通过，覆盖原配置工件、严格JSON和 SHA-256摘要测试；针对新候选包内确切`getdaq-catalog`运行普通 TCP专项runner，覆盖线程级SIGPIPE隔离、无认证收发、连接拒绝上限、断线后显式partial续传、目标读回、完成后零载荷续传及独立CLI收发。包内catalog SHA-256=`60a4e234ed4282db88f1db189fe5fa288356f8c1425ff4f16b9335dfc42485fc`，回执`validation/catalog-plain-tcp-loopback.json`同时绑定runner和测试程序SHA。首次在默认沙箱执行因不能绑定 localhost 失败；同一回归获准绑定本机回环后通过。该runner将合成raw上限限制为64MiB，实际生成并删除15,245,312字节，结束确认测试根为空且raw余量0；未进行大流量压力测试。

随后从这次工作区源码完整构建Linux host full profile，27/27程序构建成功；候选`build-packages/getdaq-linux-host-20261021-71-103-no-auth-full-candidate`含173项清单，程序SHA与构建回执绑定，`build-suite.py --verify-stage`通过。构建和测试目录位于`/tmp/getdaq-full-71-103`及本次临时验证目录，验证结束后删除；本轮没有更换Ubuntu24、Windows或macOS候选包，因此它们仍未包含本次公共头文件/实现清理。Windows端同轮使用用户指定Qt 5.14.2/MinGW7.3从当前源码重建7个操作程序，部署候选`build-packages/getdaq-windows-qt5142-20261021-71-103-no-auth-current-source-candidate`为7程序/268项清单，完整性及SDK-free PATH/离屏窗口smoke通过，未重跑Windows QtTest。Ubuntu24二进制候选未重建（仅同步当前文档），macOS仍未运行。此项不提供独立双机、防火墙、独立目标平台、板卡、macOS或持续吞吐验收；普通TCP继续不认证、不加密。

## 71.104 Ubuntu24当前源码全套回归与新候选（2026-10-21）

在已验签的Ubuntu24.04.5用户空间、WSL2宿主内核上，以GCC13.3.0、CMake3.28.3、Qt5.15.13运行`tests/ubuntu24_platform_validation.py --work /build/getdaq-platform-71-104 --jobs 3 --stage all`。该轮覆盖71.103移除HMAC实现后的当前工作树：full profile 27/27程序构建成功；Core QtTest 149/149、Widgets QtTest 106/106、原生CTest 22/22通过；配置迁移/事务、接收器传输/安全、四流接收和失败交接窗口故障注入均通过。平台回执为`build-ubuntu24-validation/rootfs/build/getdaq-platform-71-104/platform-e192b38524cf48c0ab4c771db66619e0.json`，记录测试wire汇总1,073,799,755字节（约1.074GB，低于20GiB）、失败交接窗口夹具生成8192字节raw、`raw_remaining=0`。构建及Qt测试回执随候选包保存在`validation/ubuntu24-platform.json`和`validation/ubuntu24-test-build.json`。

从同一平台回执核对SHA并封装候选`build-packages/getdaq-ubuntu24-20261021-71-104-current-source-full-candidate/`，包含27个程序、174项manifest。逐程序摘要与平台回执一致；`build-suite.py --verify-stage`、Ubuntu24候选运行库/包校验均通过。该候选包含当前公共SHA-256实现清理后的程序。平台验证工作目录中没有遗留`.graw`。用户空间与WSL2宿主共用内核，因此不代表独立Ubuntu机器；本轮也没有真实双机、防火墙/网卡、macOS、旧服务/板卡或持续物理DAQ吞吐验收。

## 71.105 Windows Qt5.14.2 当前源码 Widgets 与 ECC 客户端回归（2026-10-21）

使用用户指定的`D:\Software\QT\5.14.2\mingw73_64`及MinGW7.3，从当前工作树构建`tests/tests.pro`和`tests/ecc_system_client.pro`，严格C++11编译参数包含`-pedantic-errors`。Windows原生测试结果为Widgets 105通过、0失败、1跳过（Unix专属文件锁用例）；ECC客户端63通过、0失败、0跳过。构建收据记录917项源/配置输入，测试runner收据记录两个Windows进程退出码均为0，raw文件/字节残留均为0；运行器使用Windows私有临时目录并在结束时清理。回执位于`build-windows-qt5142-20261021-71-105-current-qt-tests/build-receipt.json`和`native-windows-results/summary.json`。

将本次原生QtTest证据并入当前七程序Windows候选`build-packages/getdaq-windows-qt5142-20261021-71-103-no-auth-current-source-candidate/`；程序本体沿用71.103已经从包含HMAC清理的当前源码构建的七程序，仅同步文档及测试回执，不将测试程序混入运行包。Windows候选完整性仍为七程序/270文件，SHA校验通过；详细操作说明见[Windows Qt5.14.2手册](WINDOWS_QT5142.md)。这是用户Windows主机本机证据，不代表独立干净Windows、macOS、Linux服务、现场板卡或DAQ吞吐验收。
## 71.109 禁止空CoBo拓扑执行系统对齐（2026-10-21）

复核 `apps/hardware/systemalignment.cpp` 后发现，独立系统对齐入口的选项验证允许所有CoBo端点均为disabled；实际对齐流程还会对系统级MuTaNT和CoBo link manager执行状态检查与写入，因此空目标拓扑不应进入硬件调用。现在 `ValidateSystemAlignmentOptions` 在活动CoBo数量为0时立即抛出配置错误，并在 `tests/hardware_alignment_test.cpp` 增加全禁用拓扑回归。

使用GCC 9.4/C++11构建 `hardware-alignment-tests` 后，测试程序在两个协议编码变体下均报告全部native alignment测试通过；不连接真实硬件。随后 `tools/build-suite.py --profile full` 从当前工作树完成Linux host全量构建：27个主机程序。候选包 `build-packages/getdaq-linux-host-20261021-71-109-active-alignment-candidate/` 为172项文件，`tools/build-suite.py --verify-stage` 完整性检查通过。平台回执记录为WSL2 Linux、glibc 2.31；它不能替代Ubuntu24目标用户空间、独立主机、Windows/macOS或物理板卡验证。

本轮没有生成或传输DAQ raw测试载荷。临时全量构建目录位于 `/tmp/getdaq-alignment-active-board-full`，定向测试构建位于 `/tmp/getdaq-alignment-active-board-check`；在归档前删除。测试输出中的“all tests passed”是定向硬件协议模拟回归，不表示实机对齐成功。



## 71.123 归档普通 TCP 拒绝超限元数据（2026-10-21）

针对无认证普通TCP归档协议增加接收端边界回归。测试线程通过本地socketpair传送协议字节，逐项声明超限的request（1,048,577/1,048,576）、manifest（16,777,217/16,777,216）、receipt（65,537/65,536）和source checkpoint（33,554,433/33,554,432字节，前者为声明长度、后者为上限）；四种都收到有界`GDAQFAIL`，且均在正文读取/内存分配和归档暂存创建前拒绝。同一测试套件另有实际TCP listener/send loopback 用例。代码见`tests/catalog_transfer_test.cpp`，并在已验签Ubuntu24.04.5用户空间执行；重建后的`catalog-transfer-tests`完整通过12个文件系统/归档传输场景，另明确报告并通过四种超限元数据拒绝、普通TCP新传输/中断后显式续传/读回/完成态零载荷续传。

本轮合成raw总量15,269,888字节（运行器单轮上限64MiB，低于用户20GiB限制），测试结束后自有raw和临时根均为0；未访问硬件。日志`build-receiver/catalog-tcp-oversized-metadata-71-123.log`和摘要回执`build-receiver/catalog-tcp-oversized-metadata-71-123.json`绑定本轮测试程序及源码SHA。构建复用了既有Ubuntu24验证树，仅重新编译数据监督器传输核心和测试二进制；产品候选的运行程序未变化。

这一回归只证明本机Ubuntu24用户空间中协议元数据上限的拒绝行为，不替代独立双机网络、macOS或持续吞吐验收。

## 71.124 普通 TCP 元数据回归与封存接收内存收敛（2026-10-21）

在71.123超限元数据回归基础上，移除`StreamSource`中未使用的seal字符串集合。每个seal通过校验并写入归档后，不再将整份字符串保留到整个传输结束；线上格式和内容校验不变，但多source归档不再额外累计这些副本。

在Ubuntu24.04.5用户空间/WSL2宿主内核中，按当前源码重建`catalog-transfer-tests`和`getdaq-catalog`，运行普通TCP专项runner。12个归档文件系统/传输回归通过；request、manifest、receipt、source checkpoint四种声明长度超过各自上限1字节的输入均在读取正文、分配正文缓冲或建立归档暂存前返回有界失败。新传输、拒绝连接、中断后显式续传、接收端完整读回和完成态零载荷续传通过。[专项回执](../build-receiver/catalog-plain-tcp-71-124.json)绑定catalog SHA-256 `33ceb62fc95ccaaa43f262b031a83c48b6b412530d36ff6ddaaf03689522bd84`、transfer tests与system tests；生成并删除的合成raw为15,269,888字节，低于64MiB runner预算和20GiB用户限制，`raw_remaining=0`。

该精确catalog二进制随后增量刷新进入Ubuntu24候选，刷新前后`build-suite.py --verify-stage`均通过，候选177项文件，其余程序按SHA复用。接着27个Ubuntu24候选程序均经目标用户空间启动smoke，回执为[`ubuntu24-catalog-smoke-71-124.json`](../build-receiver/ubuntu24-catalog-smoke-71-124.json)；不访问硬件、不生成raw。Linux host full profile也从当前源码重建27个程序并生成175项候选，程序SHA逐一匹配构建回执；候选完整性、27程序`--help`/用法入口和六个原生DAQ工具无Qt动态依赖检查通过。Linux构建回执为[`linux-full-71-124/build-receipt.json`](../build-receiver/linux-full-71-124/build-receipt.json)。本轮还将Linux host、Ubuntu24及Windows候选文档同步到源码手册；Windows运行程序本轮未重建。Ubuntu24用户空间及Linux host构建共享WSL2宿主内核；本轮不构成独立双机、macOS原生、现场板卡或持续DAQ吞吐验收，整体重构仍未完成。

## 71.126 旧 SOAP 响应节点唯一性校验与当前 Linux/Windows 构建（2026-10-21）

旧 ECC 响应解析器现在要求 SOAP Body 恰有一个顶层操作响应或 SOAP Fault；未知直接子节点和多个响应节点均明确拒绝，防止把两个不同XML节点中的字段拼成一个看似有效的状态响应。历史gSOAP `ResponseState`/`ResponseText` 和标准`*Response`响应形状继续兼容。

新增回归构造一个响应只含`ErrorCode`、第二个响应只含`State/Transition`，并断言整体拒绝；未知Body节点也拒绝。Linux GCC 9.3/Qt 5.12.8 C++11定向用例通过，完整Widgets QtTest为106通过、0失败、0跳过。Ubuntu24.04.5用户空间GCC13.3/Qt5.15.13也从本轮源码full重建27个程序，Core 151/151、Widgets106/106、CTest22/22及迁移、事务、接收传输、负载清理、四路接收和失败交接窗口验证均通过；四路256MiB接收总1,073,741,824字节，整轮测试wire合计1,073,799,755字节，`raw_remaining=0`。本机widgets测试受限沙箱初次运行因localhost监听与POSIX锁失败，放宽本地测试权限后完整106/106通过。测试生成的`.graw`均由runner清理。

Linux host full profile从本次当前源码重建27个程序，候选`build-packages/getdaq-linux-host-20261021-71-126-current-source-full-candidate`含175项并通过包校验。用户Windows Qt 5.14.2/MinGW 7.3从当前源码构建7个控制程序；Widgets QtTest 105通过、0失败、1项既有Unix锁用例跳过，ECC客户端63/63通过，两组测试`raw_files_remaining=0`。新Windows运行候选`build-packages/getdaq-windows-qt5142-20261021-71-126-current-source-candidate`含7程序/271项，Windows原生SDK-free启动检查及窗口smoke通过。Ubuntu24当前源码候选`build-packages/getdaq-ubuntu24-20261021-71-126-current-source-full-candidate`由该次full构建产物生成，27程序/175项并通过目标loader、运行库和包校验；Windows与Ubuntu24均包含本次GUI解析修改。macOS原生构建未运行。

这项校验只证明客户端XML解析边界和上述平台构建，不证明真实旧ECC服务在所有错误、超时及并发情况下的行为；现场服务版本仍需联调。Windows测试摘要与清理回执保存在`build-receiver/windows-qt-71-126/`；Ubuntu24精简回执、两套QtTest结果、CTest日志和构建摘要保存在`build-receiver/ubuntu24-platform-71-126/`。Ubuntu24用户空间共享WSL2内核，不等同独立机器验收。

## 71.125 512 MiB 单文件普通 TCP 归档回归（2026-10-21）

新增可选大文件回归模式，使用当前Linux host交付候选中的真实`getdaq-catalog`发送CLI和独立监听CLI进程。每个source各生成一个512 MiB `.graw`及一个4 KiB文件，完成封存后经本机TCP发送到独立目标；发送完成后重新读取目标全体文件并校验SHA-256完成回执，确认源catalog仍在。合成内容由固定模式填充，不来自实验数据。

Ubuntu 20.04.6用户空间/WSL2内核的Linux host候选实测最大单文件536,870,912字节，源与目标raw合计2,147,500,032字节（约2 GiB）；发送端、接收端和测试harness最大RSS为4,524、4,492和19,332 KiB。回执[`catalog-large-tcp-71-125.json`](../build-receiver/catalog-large-tcp-71-125.json)绑定Linux候选`getdaq-catalog` SHA-256 `c7cec6ee7b8ee5b17f61c43db9fe910038f3dd7299c257a2a2a19535eaae9aaa`。

同一C++11测试程序和大文件runner又加载Ubuntu24.04.5目标loader/库，并使用Ubuntu24候选内的实际`getdaq-catalog`完成第二次独立CLI发送与全量读回。其候选程序SHA-256为`bfe5e7650c0b5f514b4169f27fd09ad77f366c212c8327fe05707b61ccc243fd`，sender/receiver/harness峰值RSS为4,716、4,912和19,492 KiB；[Ubuntu24回执](../build-receiver/catalog-large-tcp-ubuntu24-71-125.json)确认同样2,147,500,032 raw字节全部删除。两次运行均低于runner硬上限16 GiB及用户20 GiB限制，runner清理源/目标私有树后`raw_files_remaining=0`。常规小型`catalog-transfer-filesystem` CTest也在loopback许可环境通过1/1。

该结果证明本机独立CLI大文件路径按流传输且用户进程RSS较小；RSS不包含内核页缓存，单机loopback不代表两台主机、实际网卡/防火墙/目标磁盘或长时间DAQ持续吞吐。Ubuntu24回归通过已安装目标loader/运行库直接启动包内程序，仍共享WSL2宿主内核。独立大文件runner默认512 MiB每个大raw，预先核算源+目标总量并要求留256 MiB磁盘余量；硬预算为16 GiB，超过预算的输入在创建测试根和证据前拒绝。超时后runner终止整个自有进程组并清理自有测试树；[`test_catalog_large_tcp_runner.py`](../tests/test_catalog_large_tcp_runner.py)的预算/超时合同测试2/2通过，并加入Linux/macOS native-data-path workflow（该workflow本轮未运行）。

在Linux host上复现可先严格构建测试harness，再让runner启动候选包内真实CLI。`--evidence`必须使用一个此前不存在的新路径；默认文件大小为512 MiB：

```bash
cmake -S apps/datasupervisor -B /tmp/getdaq-catalog-large-test-build \
  -DBUILD_TESTING=ON -DGETDAQ_BUILD_SUPERVISOR_SERVICE=OFF \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror'
cmake --build /tmp/getdaq-catalog-large-test-build \
  --target catalog-transfer-tests getdaq-catalog -j 4
python3 tests/run_catalog_large_tcp_test.py \
  --loader /lib64/ld-linux-x86-64.so.2 \
  --library-path /lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu \
  --transfer-tests /tmp/getdaq-catalog-large-test-build/catalog-transfer-tests \
  --catalog build-packages/getdaq-linux-host-20261021-71-124-current-source-full-candidate/bin/getdaq-catalog \
  --evidence build-receiver/catalog-large-tcp-new.json \
  --candidate build-packages/getdaq-linux-host-20261021-71-124-current-source-full-candidate \
  --runtime 'Ubuntu 20.04 userspace / WSL2 kernel'
```

## 71.122 旧 ECC gSOAP 双进程互操作与 Qt 完整回归（2026-10-21）

将 gSOAP 2.8.124 官方源代码包固定到`third_party/archives/gsoap-2.8.124.orig.tar.gz`，SHA-256=`1f6727ccf0e84a5dc7c86a554d0b0e2adb12f66b6e601b21d081de9041443800`，大小5,271,963字节；依赖预检full为109项通过、control为95项通过。原包`LICENSE.txt`、`GPLv2_license.txt`和`license.pdf`一并保留。该版本用于可离线复现的测试构建；旧GetBench配置只证明版本>=2.8.13时添加`-z1`，没有证明现场安装的确切gSOAP版本。

新增`tools/run-legacy-gsoap-interop.py`：验证固定归档摘要，在临时目录安全解包；无需安装flex/yacc，使用发行包内预生成的Bison解析器和Visual Studio词法器重建`soapcpp2`；从历史`source/get/GetBench-software-release-20210208/src/get/soap/ecc.hh`按旧项目命令`soapcpp2 -z1 -L -x -pecc -n`生成服务绑定；再构建仅监听127.0.0.1的独立gSOAP服务和Qt测试客户端。需要本机GCC/G++、Qt5 qmake和localhost socket；默认仅运行专项测试，可用`--full-qt-suite`把真实服务联调一起纳入完整Widgets回归。所有编译、生成绑定和运行文件位于私有临时目录，正常结束后自动删除；测试树内`.graw`检查受20GiB限制并确保清理。此探针不产生DAQ raw，不访问硬件，不把gSOAP链接进产品。

实际双进程测试覆盖当前Qt`EccClient`用于操作台控制的10个旧ECC操作：`GetState`、`Describe`、`Prepare`、`Configure`、`Start`、`Stop`、`Pause`、`Resume`、`Breakup`和`Undo`。gSOAP服务接受旧命名空间`urn:http://localhost:8083/`及SOAP 1.1 `SOAPAction: ""`；服务端逐项确认九个文本操作都解码到`configID`和包含`DataLinkSet`/46005端口的`table`，`GetState`状态/转场及九个`ResponseText`控制响应均由当前客户端成功读取。**完整Qt Widgets回归107通过、0失败、0跳过**，包括10操作双进程用例；最终完整回归收据为`build-receiver/legacy-gsoap-interop-71-122.json`，绑定QtTest日志`build-receiver/legacy-gsoap-interop-71-122.log`及SHA-256；10操作专项复跑3/3通过，严格核对九组文本参数。定向失败注入还揭示旧生成绑定将响应根命名为`ResponseState`和`ResponseText`，不是操作名加`Response`。客户端解析器现接受这两个精确旧DTO根名，并继续要求SOAP 1.1 Envelope/Body、唯一有效`ErrorCode`及GetState的合法State/Transition；SOAP Fault仍被识别为错误。

同一解析修复随后纳入Linux host full profile，从当前源码构建27/27程序并生成候选`build-packages/getdaq-linux-host-20261021-71-122-current-source-full-candidate`（175项清单）。27个包内程序逐个启动检查通过，GetDAQ offscreen窗口截图校验通过；包内程序SHA与完整构建收据逐一匹配。摘要证据位于`build-receiver/linux-host-current-source-full-71-122/`；私有构建目录已删除，smoke临时目录已清理，raw剩余0。之后同一SOAP修复源码也分别重建Ubuntu24和Windows候选，见本节后续平台段落。

Windows用户提供的Qt 5.14.2/MinGW 7.3工具链也从当前源码重建七个控制程序。新运行候选`build-packages/getdaq-windows-qt5142-20261021-71-122-current-source-candidate`含7程序/271项清单；逐程序SHA与原生构建收据一致，SDK-free PATH帮助、离屏启动和Windows原生窗口smoke通过。原生Widgets QtTest 105通过、0失败、1项Unix目录锁用例跳过；ECC客户端QtTest 63/63通过。测试清理回执为0个raw、0字节并已移除私有临时目录。证据位于`build-receiver/windows-current-source-20261021-71-122/`。这是用户当前Windows环境验证，不是独立干净安装验收。

Ubuntu24.04.5用户空间/WSL2内核也从同一当前源码full构建27/27程序。核心QtTest 151/151、Widgets 106/106、CTest 22/22，以及配置迁移、事务、接收传输/安全、四路1GiB接收和失败交接窗口测试通过；平台回执记录合成wire总量1,073,799,755字节（低于20GiB上限），`raw_remaining=0`。新候选`build-packages/getdaq-ubuntu24-20261021-71-122-current-source-full-candidate`含27程序/175项清单，逐程序摘要与该次Ubuntu24构建收据一致；27程序经目标loader启动smoke通过，无硬件访问或raw残留。精简回执及测试结果位于`build-receiver/ubuntu24-current-source-full-71-122/`。这仍是WSL2共享内核用户空间，不是独立Ubuntu24机器验收。**截至本记录，Linux host、Windows、Ubuntu24三个新候选均包含SOAP响应解析修复；真实现场旧服务、独立主机、macOS、板卡和持续DAQ吞吐仍未验收。**

此证据比本地HTTP mock更强，但仍是用固定接口和2.8.124构造的测试服务，不代表现场服务版本、完整GetBench实现、所有旧服务操作的业务/错误/并发行为、板端操作或真实DAQ链路。生成器与生成代码在GPL许可范围，探针只在测试进程使用；正式交付仍不得带入生成代码，若以后改变该边界须先核查许可。真实现场服务抓包/联调继续待办。复现见[`tests/legacy-gsoap/README.md`](../tests/legacy-gsoap/README.md)。

## 71.121 Linux host Qt 5.12.8 当前源码 QtTest 与 CTest 回归（2026-10-21）

在 Ubuntu 20.04.6 用户空间、WSL2 Linux 5.15.167.4 内核、GCC 9.4.0、Qt 5.12.8 下，针对当前源码重新构建并运行核心 QtTest（151/151）与 Widgets QtTest（106/106），均为0失败、0跳过；原生CTest 22/22通过。QtTest 独立回归监控到合成`.graw`峰值196,659字节（20GiB预算内），测试结束后0个文件/0字节残留；CTest阶段raw峰值0、余量0。证据收据为`build-receiver/linux-host-qt-current-source-20261021-71-120/qt-recheck.json`及`result.json`。第一次 CTest 调用使用了本机 CMake 3.16 不支持的`--test-dir`，其“未发现测试”不计入通过；改为从构建目录运行后才得到22/22。该结果补齐了Linux host Qt 5.12.8回归，不等于独立Ubuntu主机验收；Ubuntu24用户空间共享WSL2内核，macOS、真实双机TCP、旧gSOAP现场服务、物理板卡与持续DAQ吞吐仍待验证。
## 71.129 macOS 构建收据自动记录 otool 依赖（2026-10-21）

`tools/build-suite.py` 现在根据目标平台检查动态依赖：Linux继续记录`ldd`；macOS通过`otool -L`记录库清单，并在数据程序出现Qt、Ice、yaml、Boost或curl依赖时拒绝构建完成。`otool`不可用或执行失败会使检查失败，不再在macOS收据中默认为`checked:false`。构建收据保留原Linux `ldd`字段，macOS使用`otool`字段。

新增交付测试覆盖macOS otool成功/失败、原生数据程序动态库边界、可选ICE程序的Qt隔离，并确认检查使用库名而不误扫包含`ice`字符串的构建目录路径。`python3 tests/host_suite_delivery_test.py` 11/11通过，Python语法检查通过；Linux候选中的实际`getdaq-receiver`通过当前`ldd`检查。Ubuntu/macOS/Windows三候选文档刷新及原包完整性校验通过，执行文件和运行库未变化；Markdown本地链接1388条通过。没有生成DAQ `.graw`。

本机运行在Linux，macOS检查使用模拟`otool`输出；这不证明Apple Clang构建、Mac动态链接或目标Mac运行正常。macOS原生构建/运行仍需要Mac runner或现场Mac证据，候选复制后的RPATH、架构和实际加载也仍需目标机检查。

## 71.130 macOS 构建收据增加 Mach-O 架构检查（2026-10-21）

在71.129的`otool -L`依赖检查上，`tools/build-suite.py`现在对每个macOS程序执行`lipo -archs`，将支持架构列表与构建主机架构写入`mach_o`收据字段；若无法运行`lipo`、输出无效或程序未包含主机架构，则拒绝通过依赖检查。Universal Binary只需包含本机架构。该检查减少把错架构二进制纳入候选的风险，但不验证目标Mac的RPATH、运行库部署或程序真实运行。

新增3项macOS架构检查测试，覆盖架构别名、Universal Binary、架构不匹配和`lipo`失败。交付测试13/13通过，Python语法检查通过，本地Markdown链接1389项通过。Linux host、Ubuntu24和Windows当前候选仅刷新文档并完成对应包完整性验证；产品程序及运行库未改变，没有生成DAQ `.graw`。

测试在Linux上使用模拟`otool`/`lipo`结果，不能证明Apple Clang原生构建、Mac架构检测工具实际行为或目标Mac运行。需要GitHub macOS runner/真实Mac构建以及目标设备RPATH和运行验收；整体重构仍未完成。

## 71.132 macOS lipo 架构输出格式回归（2026-10-21）

核对[LLVM `llvm-lipo` 命令参考](https://www.llvm.org/docs/CommandGuide/llvm-lipo.html)：`-archs`输出以空格分开的架构名称；带文件名和“Non-fat file”文字的是`-info`格式。当前构建器调用`lipo -archs`并按架构名称列表解析，行为与该命令合同一致。新增回归确保误传`-info`描述格式会作为无效输出失败，不会把文件名文本记录为架构。

`tests/host_suite_delivery_test.py`现14/14通过，覆盖thin/universal架构、平台别名、不匹配、工具失败及错误描述格式拒绝；Python文档链接检查通过。此轮没有修改产品C++程序，也没有执行Apple工具或Mac构建，故不构成macOS原生验证；整体重构仍未完成。

## 71.133 macOS CI 保存工具链、测试日志和清理回执（2026-10-21）

macOS `macos-host-suite`现在把工具链版本写入`macos-toolchain.txt`，额外记录`lipo -version`；Actions产物包含full profile各组件构建日志、Widgets/ECC客户端构建/部署日志、两套QtTest结果及`qt-test-cleanup.json`。清理步骤在删除私有QtTest临时目录前统计`.graw`文件数量和字节总数，记录20GiB预算、目录删除状态和剩余文件数；超限或发现原始文件会在清理后使job失败。失败时由`if: always()`继续尝试清理和上传回执/日志，便于识别未启动测试、测试失败、raw残留与构建错误。

Windows工作流入口合同回归16/16通过，其中检查新日志与清理回执被加入上传清单；交付回归14/14、Python语法和workflow YAML解析通过。另在私有临时目录执行该工作流内联清理代码：3字节合成`.graw`被统计、删除并记为失败残留，随后空目录清理通过，两种情况均生成正确JSON回执。该机器本轮为Linux，未运行macOS job；没有Apple构建、QtTest、receiver或Sentry实测证据。该回执机制不会保存/清理CI私有目录之外的文件；整体重构仍未完成。

## 71.134 macOS QtTest 临时目录清理 helper 行为测试（2026-10-21）

将macOS workflow内联清理逻辑拆为`tests/cleanup_macos_qttest.py`。CI入口固定清理`build-platform/macos-full-suite/qt-test-temp`，不接受命令行任意删除路径；helper逐级拒绝符号链接/非目录父路径和清理根，清理前统计`.graw`数量/逻辑字节数，清理私有树后原子替换写出`qt-test-cleanup.json`，任何已发现raw或超过20GiB的总量都会让步骤失败。原子回执通过同目录临时文件与`replace`发布。

新增`tests/test_macos_qttest_cleanup.py`四项测试：空目录成功、3字节`.graw`被统计且删除后回报失败、符号链接根被拒绝且目标文件不变、父目录符号链接被拒绝且外部目标文件保持不变。16项Mac workflow合同测试、14项host delivery测试、四项清理测试、Python语法和workflow YAML解析通过。所有合成raw仅3字节并在测试临时目录退出前删除；三个当前平台候选刷新文档后完整性校验通过，程序/运行库不变。

本轮Linux主机没有运行GitHub Actions或macOS runner；测试helper行为不证明QtTest/macOS实际会创建哪些数据，也不替代目标平台验收。整体重构仍未完成。

## 71.135 当前候选文档与清理边界回归复核（2026-10-21）

复核71.134清理helper后，为防止固定相对目录的父级被替换为符号链接、导致清理路径逃出工作区，`validate_private_parent()`现在在删除前逐级`lstat`清理根的中间目录，拒绝符号链接或非目录父级。新增的父目录符号链接测试将工作区内`build-platform`指向工作区外临时目录，CLI以拒绝状态退出，外部目标文件保持不变；测试在自动删除的临时夹具内完成。macOS清理行为测试4/4、workflow合同16/16、host delivery 14/14通过。

三个最新平台候选已使用`tools/refresh-package-docs.py`同步当前操作/验证文档，并由刷新器在更新前后运行对应原生包校验：Linux host `getdaq-linux-host-20261021-71-126-current-source-full-candidate` 175项、Ubuntu24 `getdaq-ubuntu24-20261021-71-126-current-source-full-candidate`、Windows Qt5.14.2 `getdaq-windows-qt5142-20261021-71-126-current-source-candidate` 7程序/271项。程序和运行库字节保持不变；本轮未重建产品程序，也未生成DAQ raw。

本地检查不能证明GitHub Actions/macOS runner执行成功。macOS原生构建、独立目标机、双机TCP、真实现场服务/板卡与持续DAQ吞吐仍需相应环境验收；整体重构未完成。

## 71.137 当前源码 Linux 与 Ubuntu24 全量构建、归档 TCP 回归及候选（2026-10-21）

当前工作树在Linux host和Ubuntu24.04.5用户空间分别重新构建完整`full` profile，均为27个程序，第三方嵌入依赖预检109项通过。当前候选不覆盖旧包：Linux host为`build-packages/getdaq-linux-host-20261021-71-137-current-source-full-candidate`，Ubuntu24为`build-packages/getdaq-ubuntu24-20261021-71-137-current-source-full-candidate`。两个候选均由各自本次构建的精确程序摘要生成，manifest完整性检查通过；逐个启动所有27个包内程序的smoke也通过。smoke没有访问硬件、没有生成raw，Qt首窗截图由私有临时目录处理。

Linux host使用Ubuntu20.04.6/WSL2宿主内核、GCC 9.4.0、CMake 3.16.3及Qt 5.12.8。全量构建receipt记录27程序和109个嵌入依赖文件检查。当前包内`getdaq-catalog`普通TCP专项以独立CLI listener/sender进程验证无认证协议，覆盖新建归档、传输中断后的显式续传、接收端完整读回、已完成传输的无payload续传、四种超大元数据长度在创建暂存文件前拒绝、关闭端口的有界失败和SIGPIPE线程隔离；配置/系统实验核心及归档传输测试均通过。合成raw上限预先设为64MiB，本次实际生成并删除15,269,888字节，私有测试根为空、`raw_remaining=0`。证据位于候选`validation/linux-host-build-receipt.json`、`validation/linux-host-program-smoke.json`和`validation/linux-host-plain-tcp.json`。

Ubuntu24使用Ubuntu24.04.5用户空间、GCC 13.3、Qt 5.15.13和WSL2 5.15.167.4宿主内核。当前源码完整回归为配置核心151/151、Widgets 106/106、原生CTest 22/22；配置路径迁移、事务恢复、接收传输、安全、失败交接窗口和四路并发接收均通过。四路回归合计接收1,073,741,824字节，聚合所有合成wire共1,073,799,755字节（低于20GiB），platform receipt记录`raw_remaining=0`；失败交接窗口另外产生的8,192字节已清理。包内27程序经Ubuntu24目标loader启动smoke通过，无硬件访问、无raw残留。精简平台回执、构建/Qt测试回执及核心/Widgets/CTest结果随包保存在`validation/`；原始runner工作目录在提取回执后清理。

71.137不是完整项目验收：Ubuntu24用户空间仍共享WSL2内核；本轮未在用户Windows Qt5.14.2上重建（Windows完整候选仍是71.126），macOS Actions/Apple原生构建、独立机器部署、两台物理主机之间的TCP、防火墙/目标磁盘行为、现场旧gSOAP版本、板端硬件和长时间物理DAQ吞吐仍无证据。Linux host全量构建和本机普通TCP结果不能代替这些验收；整套重构仍未完成。

## 71.131 macOS CI 核验构建收据架构（2026-10-21）

在71.130的架构收据字段上，`.github/workflows/native-data-path.yml`的macOS `Verify the complete suite receipt`步骤现在读取每个程序的`runtime.mach_o`，将记录的`host_architecture`与runner上的`platform.machine()`规范化结果比较，并要求该架构出现在二进制架构列表中。这样实际Mac CI运行时会同时检查构建成功、每个产品都产生架构记录、收据主机架构与runner一致以及二进制确实包含runner架构。

`tests/test_windows_build_entrypoints.py`的macOS工作流合同回归16/16通过，检查收据架构断言处于完整构建之后、QtTest之前；`tests/host_suite_delivery_test.py` 13/13通过，Python语法和YAML解析通过。本轮没有启动GitHub Actions；测试仅验证工作流合同与Linux上的模拟macOS工具行为。没有新增Mac原生构建或目标设备运行证据，整套重构仍未完成。

## 71.138 Windows Qt 5.14.2 当前源码构建、原生回归与候选包（2026-10-21）

使用用户提供的 `D:\Software\QT\5.14.2\mingw73_64` 和 MinGW 7.3，从当前源码构建7个Windows控制程序；全量源码构建回执与C++11 QtTest构建回执已放入Windows候选包 `build-packages/getdaq-windows-qt5142-20261021-71-138-current-source-candidate/validation/`。Widgets QtTest **105通过、0失败、1跳过**（Unix目录锁用例，仅Windows不适用），ECC客户端 **63/63通过**。两个套件退出码均为0；清理回执记录临时目录已移除、raw文件0个/0字节、20GiB上限未触发。

新Windows x86_64候选包含7程序、278个manifest文件，原生清单/摘要验证通过；SDK-free PATH下各控制程序帮助检查、Qt离屏窗口和Windows原生窗口smoke均成功，截图保存在候选目录。程序不连接真实旧ECC服务器或板卡，本轮没有传输DAQ数据。候选使用用户本机Qt/MinGW完成，不等同独立干净Windows安装验证。

本次补齐Windows当前源码构建证据，但整套重构仍未完成：macOS原生构建/运行、独立目标主机部署、两台物理主机普通TCP、现场旧gSOAP版本/板卡、持续DAQ写盘和吞吐均未验收；Ubuntu24候选仍是WSL2用户空间，共享宿主内核。Linux/Ubuntu24的71.137记录见上文。

## 71.140 三平台当前源码候选刷新与 Ubuntu24 整套回归（2026-10-21）

为避免71.139 CoBo LED维护入口只停留在源码/本机联调，本轮从当前工作树分别重建交付候选。Linux host `full` profile 使用 `tools/build-suite.py` 编译27个程序，候选 `build-packages/getdaq-linux-host-20261021-71-139-current-source-full-candidate/` 共177项清单；27个候选程序均通过包内帮助/启动smoke（GetDAQ使用临时截图），smoke无硬件访问、无raw生成。构建收据、源程序SHA及manifest均绑定，`build-suite.py --verify-stage`通过。

Ubuntu24.04用户空间在本轮源码上重新构建完整`full` profile共27个程序，并构建当前Core/Widgets/native-contracts测试程序。回归为Core QtTest **151/151**、Widgets QtTest **106/106**、CTest **22/22**，配置来源迁移、配置事务、receiver传输、安全、四路并发接收和失败交接窗口均通过。四路接收各256 MiB，总负载1,073,741,824字节；本轮合成wire总量1,073,799,755字节（低于20 GiB），runner收据为`raw_remaining=0`，四路负载清理回执记录自动删除1 GiB且剩余生成payload为0。失败交接窗口额外生成8,192字节并按回执清理。当前候选 `build-packages/getdaq-ubuntu24-20261021-71-139-current-source-full-candidate/`含27程序、完整构建/平台/测试收据；候选27程序经Ubuntu24动态加载器启动及帮助检查通过，包验证通过。Ubuntu24.04用户空间仍运行在WSL2共享内核上，不等同独立Ubuntu24机器验收。

Windows使用用户提供的Qt 5.14.2/MinGW 7.3从同一源码重建7个控制程序，并以严格C++11构建Widgets/ECC客户端测试。原生Windows Widgets QtTest为**105通过、0失败、1跳过**（Unix目录锁用例仅适用于POSIX）；ECC客户端QtTest **64/64**。两个测试进程退出码均为0，清理收据报告临时工作目录已移除、raw为0个/0字节，20 GiB限额未触发。新候选 `build-packages/getdaq-windows-qt5142-20261021-71-139-current-source-candidate/`含7程序、Qt/MinGW运行库及构建和测试证据，共284项manifest；SDK-free PATH、Qt离屏窗口及Windows原生窗口smoke成功，Windows包完整性验证通过。该结果来自用户当前Windows主机，不代表独立干净安装验收。

本轮的Linux/Ubuntu24/Windows候选都使用71.139当前源码并已同步操作/验证文档。没有访问真实硬件，也没有真实双机传输；Ubuntu24/Windows均不构成独立机器证据，Ubuntu24还共享WSL2内核。macOS Apple原生构建、独立目标机安装、两台物理主机普通TCP、现场旧gSOAP服务/板端及持续物理DAQ吞吐仍未验收。数据接收性能不能由Qt GUI测试代替，GUI依旧只承担低速配置/控制，不读取raw。整体重构未完成。

## 71.139 选定 CoBo LED 维护入口及 ECC 网络端到端联调（2026-10-21）

为旧ECC控制台的 LED 维护命令增加了受限的 Qt 操作入口。操作者必须处于同一固定ECC实例的健康Prepared状态、所有接收数据进程已停止；窗口只允许选择固定清单中的活动CoBo、AsAd 0..3、P/B/U/D灯型和Off/On/Slow/Fast状态。服务端检查CoBo硬件版本只支持2或5，持久化后只发出一条 `CoBoLedModify` 调用。取消确认不建请求；确认会以唯一operation ID提交一次，丢失回复只查询原操作、不自动重试。同一ID改变目标或LED状态被拒绝。完成后必须仍为Prepared；操作ACK不表示物理灯态已验证。旧“全部闪烁/翻转/自定周期”交互没有迁移。Run未活动时“Stop Run”此前仍可用，本轮改为仅存在活动Run时启用。

底层 `ecc-state-tests`、严格ECC协议测试、单板准备夹具及Coordinator夹具通过；Qt ECC客户端64/64通过。Linux Qt 5.12.8离屏探针执行实际主窗口和RunControl，两个编码场景各完成取消/确认、两个原请求持久化、忙碌期间控制禁用、ECC原操作/审计请求键绑定、仅指定C0/AsAd3/U/Fast调用一次、其他CoBo/订阅/系统对齐不变，以及后续receiver正常停止和封存。独立证据检查通过。GUI通过单独编译的 `deny_raw_payload_reads` 拦截器尝试读取自有测试 `.graw`，确认被拒绝并记录单条拒绝日志；Qt控制界面没有读取DAQ raw。

本机测试使用真实ECC HTTP服务、两个数据服务进程和native receiver，但硬件/板端由测试夹具替代，所有传输均为本机合成TCP数据，不代表真实板卡或持续吞吐。测试期间累计生成196,608字节合成 `.graw`，均已按SHA记录并删除；证据目录确认 `raw_remaining=0`，未接近20GiB上限。最终可复查结果位于`build-receiver/cobo-led-20261021/network/ecc-network-system-gu5icggg/`，检查命令为`python3 tests/check_cobo_maintenance_window_evidence.py <该目录>`。单板准备/Coordinator元数据夹具和Qt客户端输出同在`build-receiver/cobo-led-20261021/`。此轮尚未重建Linux/Ubuntu24/Windows交付候选；macOS、独立双机、现场旧ECC/板卡和物理DAQ吞吐仍未验收，整套重构仍未完成。
# 71.146 离线 `.gmat` 波形查看器及三端候选刷新（2026-10-21）

新增`apps/waveformviewer` Qt Widgets程序`getdaq-waveform-viewer`，作为独立离线工具打开既有`.gmat`矩阵。窗口选择帧、AGET和通道；程序基于固定矩阵布局做偏移读取，每次仅取512个binary64样本（4096字节），图形使用Qt自带QPainter绘制。输入大小必须是1,114,112字节整帧的整数倍。该程序不读网络、不启动接收器，不进入Qt控制指令或高吞吐数据链，也不依赖ROOT/Qwt。用户先用`getdaq-matrix`准备兼容矩阵。

旧源码对照：`CoBoFrameViewer::setRawFrame`承接`.graw`并通过旧数据模型解码，再由ROOT绘制；`.root`浏览器依赖ROOT TTree及字典。当前实现只提供`.gmat`单通道二维波形主路径，未移植旧Viewer所有.raw格式分支、ROOT TTree统计/拟合、3D、FITS或编辑功能；不能把它描述为旧Viewer的全功能等价替代。

验证与交付：Linux host使用Qt5.12.8/GCC9.4 full profile从当前源码构建28个程序并新建`build-packages/getdaq-linux-host-20261021-71-146-current-source-full-candidate/`（178项）；`--verify-stage`通过，包内查看器`--help`和offscreen窗口启动通过。Ubuntu24.04用户空间使用Qt5.15.13构建新GUI，`tests/run_waveform_viewer_package_test.py`检查`--help`和offscreen窗口持续运行，生成0 raw/0字节的小回执；`refresh-ubuntu24-program.py`在既有71.143完整候选上安全增量添加新程序，刷新后28程序/194文件，Ubuntu24 loader及候选完整性通过。该候选的其余27程序复用原包，不是当前源码全量重编译。Windows经用户提供的Qt5.14.2/MinGW7.3实际全量构建8个Qt控制程序，新候选`build-packages/getdaq-windows-qt5142-20261021-71-146-current-source-candidate/`共8程序/278文件，所有程序SDK-free `--help`、manifest、GetDAQ原生Windows窗口smoke及打包查看器使用自带Qt offscreen插件保持运行5秒的启动测试通过。三端候选README/文档均刷新并通过程序/运行文件保留校验。Linux/Ubuntu临时构建目录已清理；没有生成DAQ raw。统一Linux control profile 8程序构建、Windows构建入口测试16/16和文档链接检查1413项通过。仍未验证macOS原生、独立Ubuntu24机器、实板或持续吞吐；这不是旧ROOT/3D/FITS完整Viewer等价实现。详情见[波形查看器手册](WAVEFORM_VIEWER.md)。

# 71.147 CoBo `.graw` 直接离线浏览本机回归（2026-10-21）

查看器现在可以直接打开 `.graw`。新增 `apps/waveformviewer/grawreader.{h,cpp}`：以只读 `QFile` 按 MFM 长度扫描，不载入整个文件；识别独立 CoBo type-1 rev0–5、type-2 rev5，以及 type `0xFF01` rev1 layered 容器中的 CoBo 子帧。索引在 `QThread` 后台运行，关闭窗口时请求取消。选择波形时只读一个 CoBo 帧；对输入帧限4MiB，layered容器限1GiB、嵌套8层、索引最多200万CoBo帧。type-1用显式AGET/通道/桶和值并保留样本顺序，允许所选通道稀疏但拒绝重复桶；compact type-2依照旧Viewer按AGET逐样本恢复通道/桶，并检查四个AGET完整。`.gmat`读取和旧格式不变。程序只依赖Qt5 Core/Gui/Widgets；没有把QtConcurrent、ROOT或网络组件加入查看器。

新增 `tests/test_getdaq.cpp` 三个QtTest槽：独立type-1与layered type-1索引/取样、完整compact type-2解码、8字节截断输入拒绝；另检查索引后头部变化被拒绝，预置取消标志可立即终止索引。Ubuntu本机 `qmake` 使用 Qt5.12.8/C++11重建查看器与测试程序；三个专项槽 `QT_QPA_PLATFORM=offscreen .../getdaq-tests waveformViewerIndexesRawAndLayeredFrames waveformViewerDecodesCompactCoboFrames waveformViewerRejectsTruncatedMfmInput` 为3项通过、0失败、0跳过。合成 `.graw` 测试夹具总计约283,656字节，均放在各自测试的 `QTemporaryDir`，成功后自动删除；远低于用户20GiB限制，未访问DAQ或网络。Linux host当前源码full构建28个程序并生成候选 `build-packages/getdaq-linux-host-20261021-71-147-current-source-full-candidate`（178项manifest）；完整包校验和包内查看器 `--help` / offscreen启动测试通过，smoke生成0 raw/0字节。Windows使用用户提供的Qt5.14.2/MinGW7.3从同一当前源码构建8个控制程序，并生成 `build-packages/getdaq-windows-qt5142-20261021-71-147-current-source-candidate`（278项manifest）；包内所有程序SDK-free `--help`、GetDAQ offscreen与Windows原生窗口启动、清单及运行库校验通过。Windows Widgets QtTest按严格C++11重建，108通过、0失败、1项Unix专属目录锁测试跳过；上述三个新增MFM回归均在原生Windows进程通过。Windows QtTest runner完成清理，临时根已删除、raw剩余0/0字节、清理时统计0个遗留文件/0字节；20GiB预算未超限。

这些是合成帧及Linux/Windows本机工具链证据，不代表真实实验 `.graw` 文件差分验证。随后使用专用Ubuntu24.04.5用户空间/Qt5.15.13从同一工作树单独重建查看器；帮助与offscreen窗口启动测试通过，未生成DAQ文件。以71.143完整候选为基线新建候选`build-packages/getdaq-ubuntu24-20261021-71-147-current-source-full-candidate/`，通过`tools/refresh-ubuntu24-program.py`只增量替换查看器；其余27程序沿用基线。Ubuntu24 loader依赖、帮助命令和194项manifest校验全部通过，回归回执绑定查看器源程序SHA并记录`raw_remaining=0`。这是Ubuntu24用户空间/WSL2共享内核验证，不是独立Ubuntu24主机验收。Linux host候选共180项（包含构建和查看器smoke回执），Windows候选共282项（包含当前构建回执、108通过/1跳过的QtTest结果及raw清理回执）；三端文档均已同步并重新校验。macOS原生构建、真实实验`.graw`差分及ROOT TTree、FITS、3D、统计/拟合仍未验收/移植。整体重构未完成。

## 71.148 隔离网络命名空间中的接收器 TCP 回归（2026-10-21）

为验证原始数据接收不只在loopback上可用，新增`tests/run_receiver_netns_tcp_test.py`。回归建立两个独立Linux network namespace，以临时veth互联；发送端在一个命名空间通过普通未认证TCP连接另一命名空间中的真实打包`getdaq-receiver`，使用`byte-storage`收取32MiB确定性合成字节流。发送源与接收落盘各32MiB，合计合成raw 67,108,864字节，低于20GiB上限；测试核对源/目标SHA-256相同、自然EOF seal为complete、接收器退出码0，并删除源/目标两个`.graw`及私有临时树。结果回执`build-receiver/receiver-netns-tcp-71-148.json`绑定当前runner SHA及候选内receiver SHA `c0749e333988546c66414cda63db53378e3f34a0dc0e17a76612e477b3a980a0`，并以`raw_remaining=0`结束；Linux候选`validation/receiver-netns-tcp.json`保存相同回执。清单刷新后候选181项，包校验通过。

该路径确实跨两个网络命名空间和IP接口，但仍是同一宿主内核、同一机器/文件系统；byte-storage不解析MFM。因此它不证明两台物理主机、防火墙/真实网卡/目标磁盘、归档复制器`getdaq-catalog`、板端TCP/ZBUF/FDT互通或持续物理DAQ吞吐。普通TCP没有认证和加密。完整步骤与限制见[接收传输手册](RECEIVER_TRANSPORTS.md#隔离网络命名空间的普通-tcp-回归)。

## 71.176 离线波形图像/CSV导出与Widgets全量回归（2026-10-22）

`getdaq-waveform-viewer`新增“Export plot/data...”操作，可将当前选中波形另存为2倍尺寸PNG或逐样本CSV。CSV包含源文件、帧、CoBo/AsAd（有信息时）、AGET、通道、bucket与ADC，字段按CSV双引号规则转义，使用CRLF；PNG和CSV均由`QSaveFile`写入并原子提交。失败不会覆盖已有输出。测试检查字段转义、样本次序及精度、非有限样本拒绝且保留旧目标，以及PNG可读性和输出尺寸。`.gmat`非整数/超范围有限值仍可完整显示与导出，但不参与整数CoBo ADC脉冲指标计算；非有限值拒绝当前波形。

Linux/WSL Qt5.12.8/GCC9.3按C++11增量构建成功；查看器帮助和offscreen窗口启动通过。Widgets全量QtTest在允许localhost监听的环境中为113通过、0失败、0跳过，新增导出槽通过。默认沙箱运行因其loopback限制导致13个网络绑定场景失败；放行本机回环后全量通过，默认环境的100项其它测试也通过。Windows构建入口回归16/16、文档链接检查1479项通过；尝试查询用户的Windows qmake时，WSL互操作在启动Windows进程前返回`UtilBindVsockAnyPort: socket failed 1`，所以没有Windows编译结果，也未刷新Linux/Ubuntu24/Windows候选；该环境错误不能判为代码构建失败。macOS、真实文件差分或DAQ硬件/吞吐也未验收。测试使用少量内存样本和临时PNG/CSV，无DAQ raw。实现及用户操作见[波形查看器手册](WAVEFORM_VIEWER.md)。

## 71.177 Linux host当前源码全套候选与28程序启动smoke（2026-10-22）

本机Ubuntu20.04.6/WSL2 host使用GCC9.4、Qt5.12.8，从当前源码通过`python3 tools/build-suite.py --profile full --build-root build-delivery/full --jobs 2 --stage build-packages/getdaq-linux-host-20261022-71-176-current-source-full-candidate`构建28个程序；receipt状态complete并绑定502项源码/构建输入。候选180文件，`tools/build-suite.py --verify-stage`通过。之后用`tests/ubuntu24_package_program_smoke.py`（不传`--rootfs`，因此直接以Linux host运行）对候选全部28程序逐一核SHA并启动；GetDAQ保存并检查离屏首窗PNG，其余CLI按各自帮助参数/退出码核验。回执`build-receiver/linux-host-71-176-package-smoke.json`为complete、28程序、`hardware_accessed=false`、`raw_files_remaining=0`、临时目录清理完成。临时full构建树（255MiB）和只读计划JSON已删除，已验证的86MiB候选包和精简smoke回执保留。无需DAQ raw，也无数据传输负载。

这是Linux host/WSL2内核与当前系统动态库上的全套构建和包内启动验证，不是Ubuntu24用户空间或独立Linux机器验收。Ubuntu24候选仍是71.172；Windows本轮native构建因WSL启动Windows qmake时`UtilBindVsockAnyPort: socket failed 1`未能开始，macOS原生和现场硬件/真实实验文件/持续DAQ吞吐未验证。Linux候选仅确认启动和帮助，不覆盖硬件控制或运行期网络/采集。

## 71.179 波形查看器逐帧导航与自动扫描（2026-10-22）

旧版`CoBoFrameViewer`提供前后帧动作、自动扫描及可配置扫描周期；新`getdaq-waveform-viewer`此前只有帧号输入框。现新增Previous/Next、首尾循环、Auto scan及50–300000 ms周期设置，默认2秒。自动扫描仅在有效文件成功打开后启用，替换文件时停止旧播放；该控件仍只驱动本地离线文件读取，不接触DAQ收发链。

增加帧导航边界QtTest，覆盖中间步进、首尾循环、单帧文件、空范围及非法索引。Linux/WSL Qt5.12.8/GCC9.3严格C++11完整Widgets回归114/114；`getdaq-waveform-viewer`独立C++11构建、`--help`与offscreen窗口持续启动通过。Ubuntu24 runner Widgets通过门槛从113同步更新到114，Windows/Linux入口合同测试16/16、本地Markdown链接检查1482项及workflow YAML解析通过。没有生成DAQ raw。Ubuntu24/Windows/macOS全量候选没有因本轮UI变更而重建，目标平台UI回归和实体实验文件尚待执行。

## 71.178 Windows Qt CI清单修正与契约回归（2026-10-22）

`tools/build-windows-from-wsl.py`直接使用`tools/build-suite.py`的`CONTROL`清单，其中包含8个程序（GetDAQ、sentry、simulator、config、config-check、config-upload、supervisor-query、waveform-viewer）。Windows GitHub Actions原先将receipt期望集合写成7个，遗漏波形查看器。现由PowerShell验收步骤读取同次构建生成的`windows-build-plan.json`，并将计划中的程序集合与receipt逐项比较；不再维护一份手写期望清单。`tests/test_windows_build_entrypoints.py`检查该数据流以及8项CONTROL集合，构建入口测试16/16通过。workflow以PyYAML BaseLoader解析通过；Markdown本地链接检查1481项通过。

本轮没有调用Windows原生工具链或执行GitHub Actions，所以只证明CI验收逻辑引用权威构建计划，不证明Windows编译、打包或运行已经通过。改动不生成DAQ/raw数据。

## 71.200 Pulser amplitude offline scan planner core and CLI build (2026-10-22)

Added `BuildPulserAmplitudeScan` and `getdaq-config plan-pulser-scan`. The planner requires an explicit enabled CoBo ID, a baseline `Generator/amplitudeStart` equal to the requested start, an ascending inclusive integer-mV interval from -1000 to +1000, exact endpoint divisibility, and at most 256 points. Each output is a complete immutable system artifact; only the selected CoBo amplitude changes and both digests are recalculated. The manifest labels hardware loading unverified, requires a separate full Configure per point, and leaves event count unspecified. This migrates only bounded configuration-plan generation, not old Gain/AutoGain event loops or gain-analysis behavior.

The C++11 `system-experiment-tests` target rebuilt and passed, covering selected CoBo at list index 1, exact generated voltages, complete-settings comparison, digest freshness/distinction, and rejection of missing/disabled CoBo, mismatched start, invalid step/range, out-of-range voltage, and more than 256 points. Linux Qt5.12.8 `getdaq-config` rebuilt successfully and its help exposes the new command. `tests/system_experiment_integration.py` passed 59 real CLI/native-checker process cases, including the ExternalTrigger example's three-point `-1000, -995, -990 mV` plan, per-point configuration/artifact SHA-256 checks, complete settings comparison, native artifact checks, and refusal to overwrite an existing plan. The runner uses a private mode-0700 XDG cache; an initial attempt with the ambient cache was refused because its transaction directory could not be safely created. Its evidence reports zero raw bytes generated and zero remaining. No board, network, receiver, or DAQ data were used.

The planner is not a Gain/AutoGain replacement. Old loop semantics, event collection, gain calculation, result export and stop/failure recovery remain unmigrated. Windows Qt5.14.2 and macOS compilation/CLI execution are also pending. Usage and limits are documented in [Pulser amplitude scan plan](PULSER_AMPLITUDE_SCAN_PLAN.md).

## 71.234 Run预留修正后的完整QtTest（2026-10-22）

修正`runReservationFailureDoesNotPublishPartialDirectory`：暂存目录断言现在枚举`RunDirectory()`的实际父目录，而非只检查输出根目录。Qt5.12.8/GCC9.3按严格C++11从当前源码重新构建`tests/tests.pro`，随后运行完整`getdaq-tests`：120通过、0失败、0跳过，耗时约52秒。运行环境为`QT_QPA_PLATFORM=offscreen`，布局/raise提示为插件能力警告，不影响断言。专项原子预留用例、模拟生命周期、配置快照闭包/重试均通过。

测试没有访问板卡或真实DAQ。Qt测试用例内合成数据使用各自临时目录并由fixture清理；本轮私有构建树`/tmp/getdaq-runreserve-atomic`（约22 MiB）已删除。此结果仅验证Linux Qt Widgets程序当前测试覆盖，不代表Windows/macOS全量构建、独立主机部署、物理双机传输、现场ECC/板卡、持续DAQ吞吐或扫描逐点完整Configure/采集编排已验收；整套重构仍未完成。

## 71.256 扫描journal未知操作的Envoy人工对账入口（2026-10-22）

Envoy离线扫描窗口新增journal状态检查与未知操作人工对账。界面先重新校验计划输出，再读取与计划SHA绑定的本机私有journal。只有journal表明仍有未决且结果未知的同一操作时，才显示其点索引、操作名和operation ID，并允许操作者选择“Confirmed completed”或“Confirmed not sent”。确认前需要操作者ID和观察说明；调用核心会重新校验计划摘要/点数、当前journal身份与原operation ID，使用独占存储锁追加对账事件。已执行只推进至原操作映射目标；未发送回到原状态但旧operation ID不能复用。入口没有ECC、硬件、receiver、catalog或DAQ文件调用。

观察说明只以SHA-256摘要写入journal，需由操作员将说明及其独立依据留存在实验记录中。ID是声明，摘要不是认证、证据真实性校验或硬件读回。

Linux host使用Qt5.12.8/GCC9.3严格C++11完整Widgets测试程序构建成功。扫描窗口与模型两个定向QtTest合计4/4（含init/cleanup）；覆盖计划重开、真实journal创建、模拟重启后unknown识别、空观察拒绝、“Confirmed not sent”持久恢复到Reviewed、旧operation ID二次处理拒绝，并在真实Envoy界面自动化点击“Confirmed completed”、填写operator/evidence输入后重开journal验证推进到`ECCStarted`。该case通过本地`getdaq-config`生成小型JSON计划，复用既有`getdaq-config-check`校验器。全量Widgets测试尚未在本轮执行。没有板卡/网络/receiver/raw数据访问；不产生`.graw`。Windows Qt5.14.2与macOS本轮未构建。

## 71.301 Reviewed扫描计划显式接纳已运行ECC实例（2026-10-23）

扫描GUI允许操作员从`Reviewed`状态选择“Verify / adopt current ECC instance”。确认框说明控制台不会启动ECC进程或发送硬件命令，并要求操作员确认该服务已在控制台外启动。NativeRunControl先把`adopt-ecc` intent写入扫描journal，再发起现有只读身份/状态/receiver观察；完整响应验证成功后，协调器才完成adoption并持久化运行身份摘要，journal状态推进到`EccStarted`。失败/断连会保留待对账intent，不自动重试。journal schema升级为5记录新操作；v1–v4读取兼容保持。

Linux `build-ecc` C++11 `ecc-journal-tests`重建并运行通过，覆盖成功接纳/重开持久化及错客户端身份拒绝且不推进状态。Qt5.12.8/GCC9.3完整`tests/tests.pro` Widgets目标构建成功；`offlineScanPlanBuilderIsAvailableFromConsole`和`scanExecutionJournalRoundTripQuarantineAndReconciliation`共4/4通过（含init/cleanup）。本地Markdown链接检查1724项通过。仅使用小型控制元数据，没有发起网络/ECC/receiver/板卡请求或生成DAQ raw；临时Qt构建目录已清理。Windows Qt5.14.2/MinGW、macOS和现场未在本轮验证；自动逐点Configure/Run/事件数Stop和receiver/catalog收尾仍未实现。

## 71.352 Native catalog替换加入持久化单机发布与中断阶段恢复（2026-10-23）

新增`PublishReceiverCatalogReplacement(old,new,candidate)`和`getdaq-catalog --publish-catalog-replacement OLD NEW CANDIDATE`。发布将完整旧catalog保留到operation ID派生的backup路径，再使用不覆盖目标的目录rename发布schema 2候选。父目录中持久化intent、独占lock和old-preserved/candidate-published/candidate-verified阶段标记；重复调用按journal字段、catalog receipt摘要、请求身份和各目录身份判断进度，无法匹配已知状态时保留所有路径并报错。候选的逻辑raw集合仍遵守同一source精确同名采用新文件、不同名旧raw保留。backup中的同名旧raw当前仍存在，本阶段不执行数据文件清理。

Linux GCC 9.4/C++11下完整构建通过。`catalog-replacement-file-set`和`receiver-catalog-filesystem` CTest 2/2通过，含两种合成中断状态：旧catalog rename到backup后尚未写marker；候选rename到正式target后尚未写published marker。测试确认同operation重试完成发布、正式catalog receipt正确且旧backup raw仍在。此前schema 1 TCP/文件系统传输回归`catalog-transfer-filesystem`在获准的本机loopback环境1/1通过；默认受限执行因本机socket权限在元数据等待处失败，不是协议断言失败。测试使用微小fixture，远低于20 GiB并由fixture清理；本阶段259 MiB临时构建树已删除。Markdown本地文件链接1791项通过。

测试模拟的是已落盘目录阶段状态，不是杀进程/断电故障注入。Windows、macOS实际编译和rename运行、ECC多节点一致性、旧冲突raw清理、Native RunControl/GUI/MCP Start接线和硬件DAQ验收仍未完成；生产入口仍拒绝覆盖已有Run。

## 71.353 schema 2连续替换保持current与retained文件语义（2026-10-23）

`CatalogReplacementEntry`增加current-session来源标识。`PlanReceiverCatalogReplacement()`仍把schema 2的`files`与`retained_files`合并成同一个逻辑文件集合参与名称优先级，但`PrepareReceiverCatalogReplacement()`只把新输入catalog中属于当前sealed session的获选文件放入候选`files`；新输入已保留的历史文件和旧输入未冲突文件写入`retained_files`。因此replacement schema 2可继续作为后续合并输入，不会把历史raw计入本次receiver seal规定的received file count。

GCC 9.4/C++11重建`getdaq-catalog`、`receiver-catalog-tests`、`catalog-replacement-tests`和`catalog-transfer-tests`成功。`catalog-replacement-file-set`与`receiver-catalog-filesystem` CTest 2/2通过；新增回归从已含一个旧保留source的schema 2 catalog再次生成schema 2候选，独立inspect成功，并核对当前session的2个文件、该source的1个retained文件，以及另一个retained-only source的2个文件分别归类正确。测试只产生既有小型fixture，低于20 GiB，fixture自动清理；临时CMake树验证后清理。

此修复只保障连续catalog逻辑合并，不解决已有Run接收新session时的目标目录冲突。Native GUI仍拒绝覆盖已有Run；多节点新catalog全部成功后的旧同名raw清理、父子journal、Run号推进、MCP Start控制和真实硬件/现场验证尚未完成。
## 71.390 MCP单CoBo Reprepare工具与预检回归（2026-10-24）

`getdaq-control-mcp`在`allow_control=true`时公布`ecc_reprepare_cobo`，严格限制参数为`cobo_id`、`operation_id`、`confirmation`，确认值必须精确匹配目标和操作ID。调用前检查ECC固定身份/快照、活动目标schema及唯一性、选定CoBo状态为Prepared/Configured、父ECC为无活动操作的健康Prepared且无未知/transition/error/reconciliation；随后提交既有`reprepare-cobo`请求。只读配置隐藏该工具，且即使伪造调用也拒绝。

Qt 5.12 standalone CMake配置及MCP C++11构建通过；`ctest --output-on-failure`运行stdio集成测试1/1通过，覆盖工具发现、无效确认拒绝、有效目标/operation ID转发及只读模式隐藏/拒绝。`python3 -m py_compile tests/ecc_mcp_test.py`通过。该测试用假ECC HTTP服务，只证明MCP适配器合同和预检转发，不证明真实ECC网络服务/硬件的Prepare成功；服务端仍负责二次门控。未访问板卡或receiver、未生成raw文件；临时构建目录在验证后删除。

## 71.391 MCP单CoBo Reprepare预检负例回归（2026-10-24）

扩展`tests/ecc_mcp_test.py`的假ECC服务，使单CoBo生命周期、全局controller状态及operation-active可注入。新增三个独立拒绝场景：目标生命周期未知、父系统非Prepared、父操作在途。每种条件下MCP均返回`isError=true`，HTTP请求增量中无POST，服务端operation数量不变。现有错误确认无请求、Prepared/Configured目标成功转发、owner epoch复用、busy 409保留和只读工具隐藏/拒绝也全部在同一stdio测试内执行。

`python3 -m py_compile tests/ecc_mcp_test.py`通过；Qt5.12 standalone CMake/C++11构建成功；在允许本机loopback socket的环境，`ctest --output-on-failure`为1/1通过。该集成使用假ECC HTTP服务，验证MCP适配器预检合同，不替代真实ECC服务的二次门控或硬件测试。未访问板卡、receiver或raw数据；测试专用临时构建树验证后删除。

## 71.392 MCP只读逐CoBo生命周期投影回归（2026-10-24）

新增公开只读工具`ecc_cobo_lifecycles`，无参数并转发认证GET `/v1/cobo-maintenance-targets`；工具在`allow_control=false`时仍显示。stdio假ECC测试检查schema可发现、返回固定CoBo及其Prepared生命周期投影，并在只读配置下再次调用成功。工具说明区分ECC生命周期/journal缓存投影与硬件读回，并声明服务重启时可能UNKNOWN。原`ecc_hardware_state`仍单独执行真实控制位观察。

Qt5.12 standalone MCP CMake/C++11构建及stdio集成CTest 1/1通过；同一测试还覆盖Run/CoBo控制确认与边界、Prepared/Configured重准备成功路径、三种Reprepare预检拒绝、只读模式和owner epoch复用。Markdown本地链接检查通过。假ECC只证明MCP路由和工具合同，不代表真实服务或硬件状态；没有硬件写入、receiver或raw数据。

## 71.393 MCP逐CoBo只读投影严格绑定回归（2026-10-24）

新增`CoBoTargets()`共享验证入口，先获取并核对固定ECC client ID、instance和snapshot，再核验maintenance-targets schema/kind/绑定摘要、target数量、ID唯一性、生命周期状态、操作阶段与精确逐板字段集合。`ecc_cobo_lifecycles`只在该校验成功后返回紧凑JSON；`ecc_reprepare_cobo`复用同一验证，避免两条代码路径规则漂移。stdio假服务新增snapshot不一致和重复CoBo ID负例，均返回MCP错误；既有只读工具可见、Prepared/Configured动作及危险状态失败关闭回归继续通过。

`python3 -m py_compile tests/ecc_mcp_test.py`、Qt5.12 standalone CMake/C++11构建、允许loopback环境下stdio CTest 1/1及Markdown链接检查通过。测试不访问真实ECC/硬件、receiver或raw；验证后删除临时构建和Python缓存。

## 71.394 MCP逐CoBo操作凭据关联只读查询（2026-10-24）

`ecc_operation`工具说明新增同一CoBo跟踪流程：将`ecc_cobo_lifecycles`返回的精确最近操作ID用于查询操作请求、phase、message和result；工具schema增加`readOnlyHint=true`，并明确此调用不重试、不改变硬件状态。stdio回归的假ECC服务为CoBo投影提供`prepare-C0-0`，再调用`ecc_operation`查询该ID，并核对两边ID一致、结果phase为complete；`allow_control=false`时两个只读工具仍可调用，隐藏控制操作依旧拒绝。

Qt5.12 standalone CMake/C++11构建成功，Python语法检查通过。默认沙箱禁止本机loopback绑定；在允许本机测试权限下stdio集成CTest 1/1通过。Markdown链接检查通过。测试使用假ECC HTTP服务，不访问实板、receiver或raw，也不证明真实服务操作状态。临时构建目录和Python缓存验证后清理。

## 71.395 legacy逐节点状态推进/回退顺序对照（2026-10-24）

参考旧版 `BackEnd::prepareNode/configureNode` 的实现，其指定 `nodeType/instanceId` 最终只调用对应Node的`call_prepare/call_configure`；系统级Prepare/Configure另行协调MuTaNT/CoBo顺序。扩展 `legacySelectedNodeProgressAndRegressRemainIndependent`：MuTaNT单节点Configure在CoBo未到Ready时被拒绝且无mock ECC动作；CoBo到Ready后再Configure MuTaNT成功；分别回退CoBo/MuTaNT时只改变被选中节点状态。实现上保护现有legacy逐节点工作流语义。

Qt5.12.8/GCC9.3/C++11下用`qmake tests/tests.pro`从当前源码重建完整Widgets测试目标成功。定向运行 `QT_QPA_PLATFORM=offscreen ./getdaq-tests legacySelectedNodeProgressAndRegressRemainIndependent` 为3/3通过（用例及init/cleanup，约1.1秒），需允许本机loopback mock ECC。没有访问硬件、receiver或raw。此结果不是Native多板独立CoBo FSM实现或实板验证；该差异仍是后续重构项。

## 71.401 Native Unprepare终态逐CoBo更新（2026-10-24）

正常`Unprepare`在所有CoBo电源关闭及本对象自有告警注销均得到确认后，把活动板阶段更新为`fresh`，非活动板仍为`disabled`；`UnprepareAfterFailure`的完成和后续只读核验成功也投影为fresh。两种清理操作若已触达板的监控、断电或告警注销结果不确定，保留Failed门控并把已触达活动板标为`unknown`；失败清理的只读核验失败也撤回此前Fresh投影。不会自动重试失败命令。

Linux GCC/C++11重建`hardware-system-release-tests`成功。获准localhost ICE/TCP的`normal-0`专项通过，覆盖Run结束状态、Breakup、正常Unprepare、重新配置周期及活动CoBo Fresh/禁用CoBo Disabled断言；`fault-2`注入AsAd断电应答失败，通过已触达活动板Unknown和不重试断言。模拟节点，无真实硬件、receiver或DAQ raw。

## 71.400 Native Breakup配置失效同步逐CoBo生命周期（2026-10-24）

同一`SystemAcquisitionClient`执行Breakup，完成全部sender断开并清除配置后，`SystemPreparationClient::InvalidateConfiguration`现在把活动CoBo生命周期转为`prepared`，禁用CoBo保持`disabled`；采集协调器从同一准备对象复制逐板状态，避免界面仍显示`configured`。配置失效阶段在任何CoBo写入发出前记录目标为touched；若中间写入/应答不确定，仅把已触达的活动CoBo标为`unknown`，保留未触达板原状态，父准备和采集协调器都保持Failed且不自动重试。该回退只撤销配置就绪位，不等价于sender EOF或receiver封存。

Linux GCC/C++11 `broken-acquisition-stop-tests`重建成功。`--invalidation-success`localhost ICE mock回归通过，核对采集状态Broken、准备状态Prepared及两份逐板快照一致（活动C0/C1 Prepared、禁用C2 Disabled）；`--invalidation-failure`在C1 `initDone`写入后注入应答丢失，核对准备和采集状态均Failed，已触达C0/C1均Unknown、未触达C2仍Disabled。无实板、真实receiver或DAQ raw；默认沙箱禁止loopback监听，需在允许本机mock的环境运行。

## 71.399 Native单CoBo维护失败状态保守投影（2026-10-24）

`SystemPreparationClient`的`SetCoBoLed`、`BlinkCoBoLeds`、`FlipCoBoLed`、`PulseCoBoLed`及`TriggerAsAdPulser`现在显式区分硬件调用是否已发出。若LED/Pulser动作可能已在板端执行而应答丢失，只将目标CoBo生命周期标记为`unknown`，将父协调器置为Failed并禁止自动重试；Pulser在硬件调用前失败则继续保留Prepared。UNKNOWN内存状态先于尽力写入审计，避免审计错误使缓存状态保持旧值。

Linux GCC/C++11重建`hardware-system-preparation-tests`成功。允许本机ICE/TCP回环的`--maintenance`专项为11场景通过，覆盖`normal-0`/`normal-1`两种编码的既有单板维护场景以及五类注入的远端应答丢失：逐项检查目标C0为unknown、活动C1为prepared、禁用C2为disabled，父状态Failed且重复调用没有第二次硬件动作。测试使用模拟CoBo/MuTaNT，无实板、真实receiver、DAQ raw或现场确认；默认沙箱拒绝回环绑定，需在允许本机mock的环境运行。

## 71.398 Native逐CoBo状态来源标注（2026-10-24）

`cobo_lifecycle_source`区分`live-controller-cache`、`controller-baseline`和`unavailable`。无当前prepare/acquisition对象时逐板阶段来自控制器基线；busy或采样期间控制器变化则生命周期数组清空并标为unavailable。Qt-free解码器严格要求一致快照只能使用前两种可用来源，不一致/busy只能为unavailable。Envoy硬件状态窗口按来源显示提示：controller baseline不是持久单板FSM，也不是板端读回。

Qt5.12.8/GCC9.3/C++11完整`getdaq-tests`重建成功；协议来源接受/拒绝、baseline UNKNOWN说明及既有过期清除三个定向用例5/5（含init/cleanup）通过。Native `native-ecc-coordinator-tests` C++11增量构建成功，loopback mock下`normal-0`和`normal-1`均PASS，覆盖fresh/unknown baseline、prepare/acquisition活缓存及正常运行状态。默认沙箱不允许loopback监听，提权本机mock运行通过。只用合成硬件/数据生命周期替身，不接实板、receiver或raw；该回归不实现持久FSM或硬件读回。

## 71.397 Native服务重启后的逐CoBo UNKNOWN投影（2026-10-24）

`ObserveHardwareStateJson()`在没有当前prepare/acquisition缓存时，根据控制器基线是否已知投影CoBo状态：服务重启保留`needsReconciliation`/`outcomeUnknown`时，启用CoBo为`unknown`、禁用CoBo为`disabled`；新鲜且未门控的控制器才使用`fresh`。扩展`native_ecc_coordinator_test.cpp`正常流程，在重开协调器并确认reconciliation门控保留后检查所有CoBo均为UNKNOWN/disabled，同时验证只读观察没有硬件写入或operation ID变化。

Linux GCC/C++11重建`native-ecc-coordinator-tests`成功。允许localhost Ice/TCP的`normal-0`与`normal-1`均PASS，覆盖两种ICE编码。测试使用mock硬件和显式数据生命周期替身，不启动真实receiver、不封存真实DAQ数据、不访问实板；默认受限环境禁止loopback socket，需在允许本机测试的环境运行。结果只证明重启时对未知状态的安全投影，不代表硬件读回、持久CoBo FSM恢复或恢复门控解除。

## 71.396 Native逐CoBo运行阶段观察端到端回归（2026-10-24）

扩展`tests/native_ecc_coordinator_test.cpp`的正常Native ECC流程，在Describe、Prepare、Configure/Connect、Run start、Pause、Resume、Stop、Breakup、再次Configure和Reset后，调用只读硬件观察接口核对每个固定CoBo的对应阶段；未启用的CoBo在整条流程中保持`disabled`。每次观察还断言`consistent=true`、来源为`live-controller-cache`且`cobo_lifecycle_is_hardware_readback=false`，并比较mock硬件写计数与ECC operation ID确保查询没有写入或改变操作。

Linux GCC/C++11重建`native-ecc-coordinator-tests`成功；`normal-0`和`normal-1`在允许localhost Ice/TCP后均通过，分别覆盖两种ICE编码。默认受限环境的首跑因socket `Operation not permitted`未能运行，放行loopback后的两场景均PASS。fixture使用mock硬件和数据生命周期测试替身，不启动真实receiver，不封存真实DAQ数据，不访问板卡。该回归确认当前控制器对象中的逐板运行阶段及只读查询一致；不证明跨ECC服务重启恢复、真实硬件状态或独立持久CoBo FSM。两份小型审计fixture与root evidence已清理。

## 71.402 Native CoBo生命周期阶段强类型化（2026-10-24）

将`apps/hardware/systempreparation.cpp`中的内部CoBo生命周期容器从自由字符串替换为封闭`CoBoLifecycle`枚举，并通过唯一名称映射序列化到原有JSON文本，保持硬件观察协议、审计字段和采集对象快照格式不变。无效枚举值会失败关闭。该项是阶段表达方式重构，不新增状态转移或硬件行为。

Linux GCC/C++11 `hardware-system-preparation-tests`、`hardware-system-release-tests`和`native-ecc-coordinator-tests`增量构建成功。localhost ICE `failure-unprepare-0`与`failure-unprepare-power-loss` Native场景各以退出码0通过；释放`fault-2`场景以退出码0通过，分别验证清理成功Fresh和断电结果未知Unknown。`hardware-system-preparation-tests ... --maintenance`在300秒限时内报告ICE 1.0和1.1的基础 selected-CoBo重准备/Configure流程PASS；之后进入应答丢失故障序列但未完成最后汇总，进程以超时退出，故不计作整套通过。Native `normal-0`在90秒限时内也未完成。没有实板、receiver或raw。该验证仅覆盖明确退出码为0的清理投影场景，不证明所有生命周期路径、独立持久FSM或跨平台构建。

## 71.403 Configure前置条件失败保留逐CoBo阶段（2026-10-24）

`tests/hardware_system_preparation_test.cpp --lifecycle-preflight`完成一次Native系统Prepare后，将C1的`pipeCtrl.firmwareEnable`观察值置为运行态，再调用系统Configure。Configure在全部CoBo停止前置检查处失败，父协调器进入`Failed`，硬件写入数保持不变；逐板缓存仍为活动C0/C1 `prepared`、非活动C2 `disabled`。还读回C0/C1的`firmwareEnable`和`intEnable`（仅预置为运行态的C1 `firmwareEnable`保持为1）以及所有板的`initDone`均为0，确认前置校验未改写板端生产状态或readiness。这说明前置校验失败不会误把尚未进入配置阶段的CoBo改成`unknown`；该结论与已触发硬件写入后的不确定状态门控相互补充。

Linux GCC/C++11 `hardware-system-preparation-tests`重建成功；允许localhost ICE/TCP后该专项以退出码0通过。仅模拟硬件，无实板、receiver或DAQ raw。

## 71.404 操作日志自动留存与界面导出（2026-10-24）

右下操作日志面板新增`Export visible log…`，将当前显示文本写入操作员选择的目标文件；使用`QSaveFile`提交，避免导出中断留下半份目标文件。界面提示连接会话日志自动保存；原有连接日志机制在成功连接时创建唯一会话文件，断开/重连回归确认两次连接文件不同、关闭标记落在对应文件且会话内容不串写。当前视图限最近2000行，因此“Export visible log”精确表示导出当前可见缓冲；完整会话以自动保存日志文件为准。

Linux Qt 5.12.8、GCC 9.3、C++11完整`tests/tests.pro` Widgets测试程序从当前源码构建成功。`visibleOperationLogCanBeExported`和`eachConnectionGetsAnIndependentPersistentOperationLog`均在offscreen Qt平台退出码0，各自QtTest报告init/test/cleanup共3 passed、0 failed。尚未在Windows Qt5.14.2、macOS或现场运行。

## 71.405 Native扫描与Run元数据控制路径复验（2026-10-24）

从当前工作树重建完整`tests/tests.pro` QtTest程序，定向运行三个用例：`manualRunNumberRollbackRemainsAvailableUntilRunStarts`验证连接后Run号、说明与自定义参数可编辑，Run进入Running后锁定；`nativeScanRunFinishesReceiverAndCatalogAfterEventTarget`使用fake ECC HTTP服务验证逐点身份/事件基线绑定、Run start、事件数到达后唯一Stop、状态读回、Breakup、receiver finish/seal、catalog和Run记录完成；`offlineScanPlanBuilderIsAvailableFromConsole`验证Threshold扫描的自动Run入口仍禁用。三项均退出码0，合计QtTest 7 passed、0 failed（含各用例初始化/清理）。

扫描UI测试从既有`bin/getdaq-config`与`bin/getdaq-config-check`调用辅助CLI；它们本轮未重建，因此本结果证明当前Widgets代码及控制集成fixture，不证明这两个CLI与本轮源码同步。fake服务只交换小型控制/归档元数据，没有receiver进程、真实封存、板卡或DAQ raw；未运行完整QtTest套件。Windows Qt5.14.2本轮仍因WSL启动Windows程序时`UtilBindVsockAnyPort: socket failed 1`未能验证，macOS和现场亦未验收。

## 71.408 逐CoBo生命周期投影纯函数提取（2026-10-24）

将Native ECC网络服务中内嵌的单板生命周期投影拆到独立`cobolifecycleprojection`纯函数；服务仍从当前控制器状态和持久操作日志构造输入，并只传递只读操作引用，避免为每个CoBo复制包含结果JSON的完整历史操作。定向回归覆盖：完成的单板Configure/Reprepare只影响目标板；另一板的LED维护记录保留其生命周期阶段；单板操作运行中只把目标板投影为transitioning；失败操作令目标板UNKNOWN；全局Configure会覆盖此前逐板维护结果并同步整机状态；控制器要求reconciliation时所有投影均UNKNOWN。

Linux GCC/C++11通过`cobo-lifecycle-projection-tests`构建并运行，所有断言通过；`getdaq-ecc-select`、`getdaq-ecc-handoff`和`getdaq-ecc-server`下游目标也重新构建成功。测试为纯内存投影，不访问网络、板卡、receiver或raw；未重跑完整native/Qt套件。该项只隔离既有投影逻辑，不实现用户独立CoBo持久状态机，也不改变UI或状态转换语义。

## 71.409 Native维护目标列表批量CoBo投影（2026-10-24）

维护目标列表原先逐个CoBo重建操作历史引用并扫描完整日志；现改为按ID建立目标索引后单次扫描日志、同时应用逐板投影。单目标前置校验仍走相同投影核心。新增等价性断言，核对批量结果与逐CoBo投影一致，并沿用71.408的整机生命周期覆盖、板间隔离、失败和UNKNOWN门控断言。

Linux GCC/C++11专用回归目标通过；改动后的`getdaq-ecc-native`静态库、`getdaq-ecc-server`、`getdaq-ecc-select`和`getdaq-ecc-handoff`均重建成功。测试只用内存中的小型合成操作历史，没有网络、硬件、receiver或raw验证；未运行完整Native/Qt套件。该调整降低历史扫描及复制开销，不改变状态机或对外协议。

## 71.410 CoBo生命周期更新与审计集中化（2026-10-24）

`SystemPreparationClient`现通过统一的`SetCoBoLifecycle()`更新逐板阶段并写入既有审计事件；入口验证索引属于冻结配置，且禁止活动/禁用CoBo跨越各自边界。Prepare、Configure、单板维护、配置失效、Unprepare及失败清理/只读确认路径都改用该入口；UNKNOWN仍通过既有失败关闭路径设置。没有新增转移，不改JSON协议或板端行为。

Linux GCC/C++11重建`hardware-system-preparation-tests`、`hardware-system-release-tests`及`native-ecc-coordinator-tests`成功。localhost Ice mock的完整三MuTaNT/两活动CoBo准备和Configure回归在编码1.0/1.1均通过；释放`normal-0`和断电故障`fault-2`均通过，覆盖成功回到Fresh与不确定状态保留UNKNOWN/不重放。mock不访问真实板卡、receiver或raw；三个临时审计文件已删除。本轮未运行完整Native/Qt套件。

## 71.435 Envoy运行台布局与完整Qt回归（2026-10-24）

运行台改为上下两层：上层左侧容纳标题、配置和Run控制，右侧显示记录参数表，约占半屏高度；下层将设备/磁盘状态表、曲线和操作日志横跨全宽。Run按钮区收紧，整机及单CoBo按钮均为Regress在左、Progress在右；运行按钮保留“Run start / Run stop”。日志导出入口位于左上Tools > Logs > Export visible log。

Qt5.12.8/GCC9.3/C++11下完整`tests/tests.pro`从当前源码构建成功；允许localhost mock socket后完整QtTest **148/148通过**。新增/更新的几何断言验证上层约占半屏、参数表处于右侧、下层横跨全宽，并覆盖两组阶段按钮左右顺序。正式Envoy `apps/envoy/envoy.pro`目标在独立临时目录构建成功。offscreen Qt测试未访问真实硬件、receiver或DAQ raw；未在Windows Qt5.14.2、macOS或现场运行。

## 71.449 数据主机本地暂存扫描 successor plan（2026-10-25）

`getdaq-supervisor --stage-successor-plan SOURCE --stage-plan-target TARGET`新增本机暂存步骤，要求配合当前service配置。它重验规范receiver plan、继承的receiver executable、资源上限及scan execution/deployment/audit-root点号关联；只在原audit root的同级创建执行专属0700目录，并在既存0700私有plan目录内以O_EXCL方式发布0400计划。已有目标不会被覆盖。返回receipt明确`plan_retired=false`、`receiver_started=false`。此命令不传输文件、不操作正在运行的旧service或硬件，也不授予successor控制权。

Linux GCC/C++11重建`getdaq-supervisor`与query客户端通过；`run_data_plan_transition_tests.py`完整12场景localhost服务/receiver回归通过，新增场景验证bundle plan逐字节暂存、摘要、目录/文件权限、目标计划尚未创建、重复目标拒绝，之后原有正常退休/successor启动生命周期仍通过。仅合成帧，无现场receiver、真实DAQ raw或硬件验证。

## 71.450 远端扫描 successor plan 认证暂存（2026-10-25）

为扫描执行部署包增加远端目标主机暂存操作`stage-successor-plan`。控制请求携带规范receiver plan JSON、目标文件路径和SHA-256，经现有bearer认证`POST /v1/operations`进入固定operation journal。只有此命令允许最多2 MiB请求（plan最多1 MiB），普通控制请求仍限8 KiB；HTTP服务仅对该POST路径放宽body预算。接收计划的canonical编码/摘要、原receiver executable、内存上限、scan execution/deployment/point与audit-root绑定、目标未存在和目录权限在接受前预检。操作执行时复用`StageScanSuccessorPlanBytes()`创建0700执行审计父目录并O_EXCL发布0400 plan，不启动receiver、不退休旧plan。

Linux GCC/C++11重新构建`getdaq-supervisor`、`getdaq-supervisor-query`和`supervisor-control-tests`成功。协议codec测试覆盖大于8 KiB但小于2 MiB的stage请求、摘要与plan tamper拒绝，并确认普通Start上限仍是8 KiB。`data-plan-transition-staging`完整12场景通过（13.57秒）：增加的大请求经认证HTTP与query CLI提交/精确重放成功；冷重启重放操作日志后，既有退休和successor流程继续通过。测试使用localhost和有界合成receiver帧，fixture清理raw；没有现场数据、实板或跨机网络验收。bearer控制面仍为现有HTTP，未新增TLS；详见[扫描执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md)。

## 71.451 Data file tools 关闭等待期间锁定菜单入口（2026-10-25）

Data file tools关闭时若子进程仍在运行，窗口先隐藏并请求终止；此时禁用Tools菜单入口并显示“正在停止”的提示，避免重新打开仍在关闭的窗口。子进程退出、窗口销毁后，菜单入口恢复；Run启动互锁保持到窗口实际销毁。若子进程忽略SIGTERM，2秒后强制终止。

Qt 5.12.8/GCC 9.3/C++11完整Widgets测试目标及正式Envoy `apps/envoy/envoy.pro`均从当前源码构建成功。关闭专项10/10通过，覆盖窗口叉号、Close、Esc、忽略SIGTERM的子进程强杀收尾，以及MainWindow菜单禁用/恢复。仅Linux offscreen与合成sleep进程，无真实数据处理、硬件或跨平台验收。

## 71.452 ECC handoff后认证服务退休（2026-10-25）

新增authenticated `POST /v1/shutdown`，请求精确绑定ECC `instance_id`、`snapshot_sha256`及原始data handoff operation ID。服务只在原实例handoff完成、无提交/观察在途、硬件控制状态已知且处于Idle/Described时接受，并重新核对保留的handoff receipt。正常receiver停止与seal验证由handoff操作完成边界保证；handoff后数据管理器会按设计持久fence，所以shutdown检查分离读取硬件`ControlStatus()`，不把aggregate data reconciliation fence误判为硬件故障。accepted响应刷新后服务正常退出，析构释放host board lock。handoff前的shutdown请求被拒绝。

Linux GCC/C++11重建`getdaq-ecc-server`和`ecc-network-service-tests`成功。获准localhost的`ecc-network-service-handoff-service-0/1`（Ice encoding 1.0/1.1）均通过，分别86.61秒和87.36秒；验证successor取得同一host board lock，原ECC重启仍fenced。每场景生成并自动清理65,536字节合成raw，剩余0。仅合成服务、receiver与Ice硬件fixture，不代表实板或真实双主机验收。shutdown原语尚未接入扫描executor/journal多点自动推进。

## 71.453 补齐完成后的ECC服务退休（2026-10-25）

`ValidateServiceShutdown`此前只接受原始handoff operation本身为`complete`，会拒绝一个已获明示补齐、并生成完整receipt的合法恢复结果（原operation必须保留`failed`历史）。现与`ControlHandoffJson()`保持相同的双操作规则：接受原handoff complete，或原handoff failed且其准确`complete-data-handoff`补齐operation complete，同时仍要求同一实例/快照/operation绑定、无在途操作、已知Idle/Described硬件控制状态及精确receipt。没有放宽冷启动：仅有历史失败/补齐日志仍不能关闭或接管。

Linux GCC/C++11重建ECC服务和跨进程fixture成功。新增`ecc-network-service-plan-transition-complete-0` **1/1通过**（139.91秒），覆盖原handoff部分失败、操作者显式补齐、服务认证shutdown正常退出、successor取得同一host board lock及原服务重启后持续fenced；收据标记`graceful_shutdown_after_handoff=true`。该场景清理81,920字节合成raw，剩余0。修改后普通handoff场景`ecc-network-service-handoff-service-0`复跑也通过（87.21秒）。localhost合成服务/receiver/Ice夹具，无实板或现场双机验收；扫描executor/journal仍未自动调用退休或推进下一点。

## 71.454 受管ECC进程退出证据绑定扫描journal（2026-10-25）

新增Qt-free POSIX `EccServiceProcessSupervisor`私有构造的退出证据值，记录受管PID、固定service配置摘要、ECC实例/服务快照、原handoff operation和精确shutdown请求/接受响应摘要，并绑定执行ID、冻结执行快照SHA、点序号与journal runtime-identity摘要。扫描点必须先处于已绑定的`Bound`状态且有receiver事件基线；shutdown前以schema-7 `managed-service-shutdown-intent`持久化PID/配置/实例/handoff身份。Supervisor只接受精确匹配的HTTP 200响应，并等待同一子进程正常退出码0。journal校验意图匹配、scan/runtime身份匹配以及请求/接受响应的规范摘要后，才记录`managed-service-retired`并推进到`Retired`。未完成intent在journal重开时进入`NeedsReconciliation`。普通`retire` intent和人工对账路径未放开；schema 1–6的读取兼容保留。

Linux GCC/C++11成功重建`getdaq-ecc-native`、`ecc-service-process-supervisor-tests`、`scan-execution-state-tests`、`ecc-journal-tests`和`system-experiment-tests`。scan execution状态测试、ECC journal/store测试、experiment核心测试均退出码0；localhost假ECC HTTP + 子进程集成回归通过，验证shutdown前意图落盘、正常退出证据、journal原子更新/重开/下一点推进、跨执行或runtime身份拒绝，以及shutdown响应摘要篡改拒绝。loopback集成在受控本机权限下运行，未访问硬件、真实receiver或DAQ raw。当前仍未接入Envoy扫描执行器/deployment选择；不代表逐点自动扫描闭环、真实板卡或跨平台验收。

## 71.455 逐点部署bundle只读重验（2026-10-25）

`VerifyScanPointDeploymentBundle`及`getdaq-scan-deploy verify PLAN_DIRECTORY POLICY.json SNAPSHOT.json BUNDLE_DIRECTORY`现可重开已生成的逐点部署bundle。验证器重新绑定源plan/policy/snapshot，严格核对manifest字段、execution ID、冻结snapshot摘要、点序、逐点配置/artifact摘要；要求bundle目录属主正确且非group/world可写，manifest、artifact、ECC service配置和receiver plan均为属主0400普通文件，并核验文件身份/大小、service固定配置、receiver规范编码/摘要、deployment/audit root与每点唯一目标plan文件名。此处验证本地完整性和包内身份一致，不认证来源、不安装文件、不启动服务或控制硬件。

Linux GCC/C++11重建`getdaq-ecc-native`、`getdaq-scan-deploy`及`fixed-ecc-configuration-tests`成功。固定配置/部署集成回归通过，验证两点bundle可经API和CLI重验；改写manifest snapshot SHA或改写后重封存receiver plan均被拒绝，恢复原plan字节后重验成功。仅使用本机合成计划、固定配置及Ice fixture；没有实板、真实receiver、DAQ raw或硬件命令。此入口是后续部署选择的只读基础，不表示Envoy已接入逐点服务启动/配置装载/自动推进。

## 71.456 受管ECC启动意图与进程证据（2026-10-25）

受管启动桥`StartManagedEccServiceForScanPoint`先将schema-8 `managed-service-start-intent`同步到扫描journal，绑定冻结执行快照、当前点、唯一operation ID、ECC executable摘要和固定service配置摘要，再启动进程。`EccServiceProcessSupervisor::StartForScanPoint`启动前重验两份输入摘要；exec成功且被监督的确切子进程仍存活时，才生成私有`EccServiceProcessStartEvidence`。journal校验证据和先前intent完全对应后才从`Reviewed`推进到`ECCStarted`。随后仍需单独绑定新鲜runtime identity，exec成功不等于ECC HTTP服务Ready。

启动异常会追加start-unknown并转入`NeedsReconciliation`；如果持久intent后崩溃或重开，悬空intent同样自动隔离，绝不自动重试。受管关闭/Retired仍使用既有进程退出证据；schema 8 journal应用关闭事件时保持schema版本8。旧schema 1–7仍可读取。

Linux GCC/C++11重建`getdaq-ecc-native`、`ecc-service-process-supervisor-tests`及固定ECC配置集成目标通过；ECC journal/store与scan execution state测试退出码0。localhost合成ECC endpoint/子进程测试通过，新增覆盖受管启动持久意图、成功exec/PID和输入摘要绑定、schema-8重启回放、startup intent悬空隔离、expected executable摘要不符时未创建子进程但写入隔离，以及schema-8后续shutdown intent重开隔离。集成测试需要允许受控localhost回环socket；未访问硬件、真实receiver或DAQ raw。启动桥仍未接入Envoy/deployment GUI自动推进，服务Ready仍须后续identity观察验证；Windows/macOS/现场未验收。

## 71.482 RemoteReceiverManager恢复流程实现拆分（2026-10-25）

将`RemoteReceiverManager::Impl`和跨实现文件共用的私有辅助定义放入`remotereceivermanager_p.h`；完整handoff、cold handoff续接、catalog替换续接及控制权接管实现放入新的`remotereceivermanager_recovery.cpp`。普通receiver远端控制入口仍由`remotereceivermanager.cpp`实现。该重排保留已有持久化顺序、journal和状态/协议行为，并把新源文件加入Native ECC CMake目标。拆分后的主实现文件192行、恢复实现352行，私有共享头823行，说明后续仍可继续细分实现职责。

Linux GCC/C++11重建`getdaq-ecc-native`、`native-ecc-coordinator-tests`和`native-ecc-remote-receiver-tests`成功。`native-ecc-remote-receiver-normal-0`在获准localhost Ice/TCP回环后通过，1/1、86.42秒；`failed-handoff-parent-completion`通过。集成回归使用本机服务、receiver和合成数据；无实板或现场DAQ数据验证。文档本地链接检查随后通过。

## 71.483 RemoteReceiverManager操作与观察职责拆分（2026-10-25）

继续将catalog替换续接、Mutation/ReadOnly命令处理、receiver operation轮询及handoff/retirement完成证据验证、服务状态观察和predecessor周期验证分别移入`remotereceivermanager_catalog.cpp`、`remotereceivermanager_mutation.cpp`、`remotereceivermanager_operations.cpp`与`remotereceivermanager_observation.cpp`。共享私有头保留类型、状态和方法声明，缩减至252行。原持久化意图/审计顺序、远端POST/GET策略、receiver周期身份及UNKNOWN门控不变；新增文件均注册到Native ECC CMake目标。

Linux GCC/C++11构建`getdaq-ecc-native`、`native-ecc-coordinator-tests`和`native-ecc-remote-receiver-tests`成功。当前拆分状态下`failed-handoff-parent-completion`通过（1/1，6.30秒）、`native-ecc-remote-receiver-normal-0`通过（1/1，86.38秒），`native-ecc-remote-receiver-catalog-finalize-failure`通过（1/1，10.02秒）。三个集成用例使用获准的localhost Ice/TCP、本地服务和合成receiver数据；未访问实板或现场DAQ raw。文档链接检查通过1996项。

## 71.492 SupervisorControl job and observation separation

Moved the private `SupervisorControl::Impl` declaration into `controljobs_p.h`, worker/journal/store behavior into `controljobs_impl.cpp`, and public observations into `controljobs_observation.cpp`; submission and operation encoding remain in `controljobs.cpp`. Each accepted control operation keeps its `request.json` at the operation root, while DataSupervisor audit files use a dedicated `data/` child directory.

Linux GCC/C++11 rebuilt `getdaq-data-supervisor-core`, `supervisor-control-tests`, `native-ecc-remote-receiver-tests`, and `getdaq-supervisor`. `supervisor-control-service`, `data-supervisor-process-lifecycle`, `failed-handoff-parent-completion`, and `failed-retirement-parent-completion` passed. These covered mutated request detection during long operations, failed startup recovery, explicit handoff completion, and retirement proof completion. Synthetic local receiver processes and allowed localhost/Ice services were used; no physical board, production receiver or acquired DAQ raw was used. The implementation split was checked after an extraction-script error had truncated the `Run` worker at its `next={}` default argument; that worker body was reconstructed and then exercised by the listed regressions.

## 71.493 Offline Data Tools close lifetime

Set `Qt::WA_DeleteOnClose` in the `OfflineDataToolsWindow` constructor so accepted close events destroy the modeless top-level window without relying on its caller to configure deletion. The close path still holds Run/menu gates until an active tool process exits and the window is destroyed. Qt 5.12.8 focused tests built with qmake and passed 13/13 under the offscreen platform: all three close controls, process-running close, forced termination of an unresponsive child, menu reopen, and close-pending gating. This validates local Qt behavior only, not Windows/macOS or field use.

## 71.494 ScanPlanWindow policy implementation separation (2026-10-25)

Moved scan-plan review storage/catalog setup, executable discovery, runtime policy-control presentation and immutable stopping-policy creation into the new `apps/envoy/scanplanwindow_policy.cpp`. `scanplanwindow.cpp` is now 683 lines and the policy unit is 289 lines. Both `apps/envoy/envoy.pro` and `tests/tests.pro` register the translation unit. The change preserves UI and policy behavior.

Qt 5.12.8/GCC 9.3 rebuilt the `getdaq-tests` target with qmake. Offscreen `scanPlanOutputInspectionRejectsMalformedPlans` and `offlineScanPlanBuilderIsAvailableFromConsole` passed: 4 passed, 0 failed, including QtTest setup and cleanup. No hardware or network was accessed.

## 71.495 ScanPlanWindow plan creation separation (2026-10-25)

Moved artifact/output selection, existing-plan validation/opening, scan-mode controls and plan creation into `apps/envoy/scanplanwindow_creation.cpp`. `scanplanwindow.cpp` is now 481 lines and the creation unit is 214 lines. Both qmake projects register the file; no UI, plan format or execution behavior was intentionally changed.

The Qt test target and production Envoy application rebuilt with Qt 5.12.8/GCC 9.3. Four focused scan-plan/journal tests passed, including the Native scan completion integration test under local simulation fixtures: 6 passed, 0 failed. Full qmake Qt suite was also run before this second extraction (177 passed, 0 failed, 2 skipped); the two skips require optional offline tool binaries. No physical hardware or acquired DAQ data was used.

## 71.496 NativeRunControl scan finalization separation (2026-10-25)

Moved scan-point event observation, Stop submission, fresh ECC state confirmation, unresolved-operation/deadline handling and receiver/catalog finalization checks into `apps/envoy/nativeruncontrol_scanfinalization.cpp`. `nativeruncontrol_scan.cpp` is now 517 lines; finalization is 322 lines. `apps/envoy/core.pri` registers the new translation unit for the production application and consumers.

Qt 5.12.8/GCC 9.3 rebuilt `getdaq-tests` and production Envoy. Offscreen scan controller guard and local Native simulation tests passed 5/5 including setup/cleanup; the integration case verifies event-target Stop, receiver finish and catalog completion. Local synthetic services only; no physical hardware or acquired DAQ data.

## 71.497 NativeRunControl scan identity and baseline observation separation (2026-10-25)

Moved frozen scan deployment identity verification, execution journal binding and event-count baseline capture into `apps/envoy/nativeruncontrol_scanidentity.cpp`. The remaining `nativeruncontrol_scan.cpp` contains scan-point lifecycle initiation and pending lifecycle observation and is 289 lines; the identity/baseline unit is 244 lines. `apps/envoy/core.pri` registers the new source for Envoy and Qt test consumers.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and `getdaq-tests`. Offscreen tests for connected-controller guards and Native simulated receiver/catalog completion passed 5/5 including setup/cleanup. The integration test uses local synthetic services only; no physical hardware or acquired DAQ data.

## 71.498 XcfgDocument effective projection and override separation (2026-10-25)

Moved `EffectiveChildren`, effective node/scalar projection, checked effective-tree capture, override preview and override application into `common/config/xcfgdocument_effective.cpp`. Shared error/path helpers moved into the private `xcfgdocumentinternal_p.h`; source editing and parser semantics are unchanged. Registered the unit in `common/config/xcfg.pri`, `tests/xcfg_shared.pro` and `tests/current_configuration_probe.pro`.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy, the full Widgets test target, Xcfg shared tests and current-configuration probe. `xcfg-shared-tests` passed 151/151. Seven focused configuration workspace/history/effective snapshot/publication tests passed 9/9 including setup/cleanup. Sandbox execution initially could not acquire the test configuration-source transaction lock; the same suite passed under normal local filesystem permissions. No hardware or DAQ data was used.

## 71.499 SystemPreparationClient CoBo maintenance separation (2026-10-25)

Moved `ReprepareCoBo`, `ConfigureCoBo`, selected-board LED actions and `TriggerAsAdPulser` into `apps/hardware/systempreparation_maintenance.cpp`. The private `SystemPreparationClient::Impl` and shared inventory/hardware-step helpers are in `systempreparation_p.h`; whole-system setup, Prepare/Configure and failure/Unprepare remain in `systempreparation.cpp` (526 lines). CMake adds the maintenance unit to `getdaq-hardware-core`.

Linux GCC/C++11 rebuilt `getdaq-hardware-core` and `hardware-system-preparation-tests`. `hardware-system-preparation-tcp` passed 1/1 in 224.72 seconds using local Ice simulator fixtures. It exercises Prepare/Configure, selected-CoBo maintenance, failure gates and release audit paths. No physical hardware or DAQ data was involved.

## 71.500 SystemPreparationClient recovery and release separation (2026-10-25)

Moved production stop after failure, configuration invalidation, failed-preparation unprepare and verification, normal Unprepare, and their retained audit-chain getters from `systempreparation.cpp` into `apps/hardware/systempreparation_recovery.cpp`. Initial constructor/Prepare/Configure coordination remains in the original unit, reduced to 233 lines. Added the new translation unit to `apps/hardware/CMakeLists.txt`; no operation sequence or state behavior was intentionally changed.

Linux GCC/C++11 rebuilt `getdaq-hardware-core` and `hardware-system-release-tests`. The focused `hardware-system-release-fault-0` integration test passed 1/1 in 44.88 seconds using local Ice simulator fixtures. The initial sandbox attempt was blocked because the test simulator could not bind its loopback socket; the same test passed with local loopback access. No physical hardware or DAQ data was used.

## 71.501 ConfigWindow effective snapshot workflow separation (2026-10-25)

Moved `ExportEffectiveSnapshot`, `CompareEffectiveSnapshot`, snapshot report presentation and `StartSnapshotTask` into `apps/envoy/configwindow_snapshot.cpp`. Registered the translation unit in both production `apps/envoy/envoy.pro` and `tests/tests.pro`; configuration window implementation is now 547 lines. No snapshot format or UI behavior was intentionally changed.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the full `getdaq-tests` executable. `configurationEffectiveSnapshotsRemainAvailableWhileConnected`, `configurationSnapshotBackgroundCompletionCannotReviewAnotherDocument`, and `configurationPublicationBuildsReviewedLocalPackageWhileConnected` passed 5/5 including setup/cleanup. A first run in the restricted sandbox could not open temporary local XCFG files; rerunning the same tests with normal local filesystem permissions passed. No hardware or DAQ data was used.

## 71.502 ECC control observation codec separation (2026-10-25)

Moved ECC source/receiver observations, receiver event counts, receiver failure-status decoding and failed-bound-receiver comparison into `common/control/eccprotocol_observation.cpp`. Promoted the shared data-node lifecycle decoder to an inline private helper so system-status and receiver-observation codecs share the exact validation. Updated both `common/control/CMakeLists.txt` and `common/control/control.pri`; wire format and acceptance rules are unchanged.

Linux GCC/C++11 rebuilt `getdaq-control-protocol` and `ecc-protocol-tests`; CTest `ecc-protocol` passed 1/1. Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the complete `getdaq-tests` executable via qmake using the updated protocol source list. No hardware or DAQ data was used.

## 71.503 HardwareClient plan execution separation (2026-10-25)

Moved `HardwareClient::Process`—whole-plan software validation, remote metadata preflight, ordered command execution, checkpoint handling and audit completion—into `apps/hardware/hardwareclient_plan.cpp`. Connection, inventory/description binding, create/destroy and read/write methods remain in `hardwareclient.cpp`, now 212 lines. Shared Ice client state and metadata helpers live in private `hardwareclient_p.h`; `apps/hardware/CMakeLists.txt` registers the execution unit. No API or command semantics were intentionally changed.

Linux GCC/C++11 rebuilt `getdaq-hardware-core` and `hardware-client-tests`. CTest `hardware-client-tcp` passed 1/1 in 222.95 seconds against local Ice simulator fixtures. The initial sandbox attempt could not bind its loopback socket; rerunning the same test with local loopback access passed. No physical hardware or DAQ data was used.

## 71.504 EccSystemClient read-only observation separation (2026-10-25)

Moved `QueryMaintenanceTargets`, configuration/readback, ownership, receiver/event, scan-runtime identity, source and receiver-failure queries into `apps/envoy/eccsystemclient_observations.cpp`. Identity/status/alarm gating and command/handoff submission remain in `eccsystemclient.cpp`, now 337 lines. Shared UTF-8 and bounded error formatting helpers are declared in private `eccsystemclient_p.h`. Updated Envoy and standalone ECC client/probe qmake source/header lists; network request paths, identity checks and control gates were not intentionally changed.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy, the full `getdaq-tests` executable and `ecc-system-client-tests`. The dedicated client suite passed 69/69 in 11.692 seconds. It uses an in-process localhost HTTP fixture and verifies read-only observation and existing command/fault behavior. No hardware or DAQ data was used.

## 71.505 Catalog stream sender/receiver implementation separation (2026-10-25)

Moved `ReceiveReceiverCatalogStream` and `SendReceiverCatalogStream` into `catalogtransfer_stream.cpp` and `catalogtransfer_send.cpp`. `catalogtransfer.cpp` now owns local copy/resume and transfer verification (375 lines). The private `catalogtransfer_internal.h` shares the bound source model, protocol/checkpoint helpers and a single SIGPIPE guard; the CMake target registers both stream units. Wire framing, resume identity checks, durable publication order and failure replies are unchanged.

Linux GCC/C++11 rebuilt `getdaq-data-supervisor-core`, `catalog-transfer-tests` and `getdaq-catalog`. The catalog transfer integration suite passed all 12 native scenarios, including plain TCP transfer, interrupted transfer resume, completed resume without payload, CLI connection refusal and separate-process listener/send/readback. The first sandboxed execution could not bind localhost; rerunning with localhost access passed. Documentation link validation passed 2,011 local Markdown links. Tests use synthetic files and local endpoints; no physical receiver or production DAQ data was used.

## 71.506 Receiver catalog scan binding and summary separation (2026-10-25)

Moved the private scan-point/catalog Run binding verifier, binding publication and multi-point index generation into `receivercatalog_scanbinding.cpp`. Threshold and raw-waveform catalog analyses remain in `receivercatalog_scananalysis.cpp` and call the same binding verifier through a private declaration, so point/configuration/artifact, Run/session and sealed-catalog checks are not duplicated. CMake registers the new unit; output schemas and analysis behavior are unchanged.

Linux GCC/C++11 rebuilt the data-supervisor core, catalog tests and CLI. The receiver catalog test executable passed all 68 checks, including threshold scan binding and synthetic AutoGain, Pulser, PeakingTime and layered/compact MFM analysis. Tests used synthetic metadata/raw fixtures; no receiver, network or hardware was accessed.

## 71.507 ConfigWindow export and workspace-copy dialog separation (2026-10-25)

Moved the reviewed XML export and editable workspace-copy dialog implementations into `apps/envoy/configwindow_export.cpp`. `configwindow.cpp` retains the main editor construction, selection/tree editing and history/structure workflows, now 411 lines; both the production Envoy and Qt test qmake projects compile the new unit. Dialog behavior and asynchronous revision guards are unchanged.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the full `getdaq-tests` executable. Four focused workspace-copy/XML-export tests passed (6 QtTest cases including setup/cleanup), covering preview/save, read-only behavior and stale-review rejection. The first restricted run could not open temporary XCFG files; the same tests passed with normal local filesystem access. No hardware or DAQ data was used.

## 71.508 ScanExecutionJournal codec direction separation (2026-10-25)

Separated `ScanExecutionJournal::Encode` and `Decode` into `scanexecutionjournal_encode.cpp` and `scanexecutionjournal_decode.cpp`. Both continue to use the same strict private codec helpers; schema versions, exact-field checks, event ordering/replay and restart quarantine semantics are unchanged. Updated the experiment CMake and qmake source lists.

Linux GCC/C++11 rebuilt `getdaq-experiment-core` and `scan-execution-state-tests`; the state test passed, covering lifecycle replay, intent-before-action persistence, restart quarantine and tamper rejection. Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the full `getdaq-tests` target; the focused journal round-trip/quarantine/reconciliation test passed (3 cases including setup/cleanup). No hardware or DAQ data was used.

## 71.509 MCP local settings and credential boundary separation (2026-10-25)

Moved local MCP config parsing, credential-file checks and client-epoch persistence into `apps/ecc/mcp/mcpsettings.{h,cpp}`. `main.cpp` retains JSON-RPC dispatch, tool schemas, ECC HTTP requests and per-tool authorization/confirmation checks. Shared input validation and the settings record are declared in the private MCP header; file ownership/mode requirements, identity binding and epoch creation behavior are unchanged. Both CMake and qmake register the new source.

Linux GCC/C++11 built the CMake MCP target; the existing MCP stdio integration test passed, covering tool schema, read-only requests, lifecycle/CoBo/catalog/recovery confirmations, owner reuse and busy/not-queued errors. A qmake build also succeeded. The integration test uses an ephemeral localhost mock; no physical hardware or DAQ data was used.

## 71.510 MCP tool schema factory separation (2026-10-25)

Moved the pure JSON tool schema and metadata factories into `apps/ecc/mcp/mcptoolschema.{h,cpp}`. JSON-RPC dispatch remains in `main.cpp`; tool definitions, argument schemas, annotations and confirmation language are unchanged. Updated both CMake and qmake source lists.

Linux GCC/C++11 CMake and qmake builds succeeded. The MCP stdio integration test passed for tool schema, read-only calls, lifecycle/CoBo/catalog/recovery confirmations, owner reuse and busy/not-queued error reporting. It runs against a local HTTP fixture; no physical hardware or DAQ data was used.

## 71.511 MCP stdio framing separation (2026-10-25)

Moved JSON-RPC error response construction and bounded newline-delimited stdin/stdout framing into `apps/ecc/mcp/mcpstdio.{h,cpp}`. The 1 MiB request bound, malformed JSON responses and response framing are unchanged; CMake and qmake include the new source.

Linux GCC/C++11 CMake and qmake builds succeeded. The MCP stdio integration test passed, covering oversized-request rejection, tool schemas, read-only operations, lifecycle and recovery confirmations, owner reuse and busy/not-queued errors against a local HTTP fixture. No physical hardware or DAQ data was used.

## 71.512 Scan point binding session separation (2026-10-25)

Moved the binding session and its intent, response validation, Complete/MarkUnknown and BeginScanPointBinding paths into `apps/envoy/scanplanreview_binding.cpp`. Shared private session storage and journal/snapshot helpers are declared in `scanplanreview_p.h`; journal review, context loading and reconciliation remain in `scanplanreview.cpp`. Registered the unit in production and test qmake projects without changing journal transitions or binding gates.

Qt 5.12.8/GCC 9.3 rebuilt the complete Qt test target. Focused scan journal recovery/reconciliation and native scan-to-catalog binding tests passed 7/7. The initial restricted execution could not bind the in-process localhost HTTP fixture; the same tests passed with loopback access. The fixture uses metadata only, with no physical hardware or DAQ payload.

## 71.513 Envoy CoBo maintenance and selected-node control separation (2026-10-25)

Moved selected status-table target resolution, maintenance window creation and signal wiring, target/status projection, and selected-node Progress/Regress handlers into `apps/envoy/mainwindow_maintenance.cpp`. Production Envoy and tests both register this unit; Native whole-system and legacy per-node semantics are preserved.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the full QtTest target. Focused Native one-CoBo lifecycle, legacy selected-node Progress/Regress, and CoBo maintenance confirmation tests passed 5/5. A restricted test run could not open the legacy local TCP fixture; the same tests passed with loopback access. No physical hardware or DAQ data was used.

## 71.514 Envoy run configuration file workflow separation (2026-10-25)

Moved the RunConfig read/validation path, YAML load/save handlers, Nodes/Paths dialog and field-row add/remove handlers into `apps/envoy/mainwindow_configuration.cpp`. The production and test qmake projects register the new unit; validation and run settings semantics are unchanged.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the complete QtTest target. Focused Native GUI setup/configuration and manual Run-number rollback tests passed 4/4. No hardware or DAQ data was used.

## 71.515 ROOT viewer event canvas separation (2026-10-25)

Moved 2D waveform, heatmap, histogram, 3D surface painting and drag rotation into `apps/rootviewer/rootcanvas.{h,cpp}`. `main.cpp` now retains the window, file/event navigation and statistics workflow, reduced from 671 to 323 lines. The optional ROOT viewer CMake target includes the new component.

ROOT 6.26/Qt 5.12.8/GCC 9.4 built `getdaq-root-viewer`. Offscreen `getdaq-root-viewer --surface-smoke` passed; a simulated mouse drag changed 262,458 rendered pixels. This is a UI rendering smoke test, not ROOT file ingestion, DAQ or hardware validation.

## 71.516 Configuration source referrer inventory separation (2026-10-25)

Moved the bounded, read-only XML include inventory and reverse-referrer scan to `common/config/xcfgsourcereferrers.cpp`; migration preview/apply/recovery remain in `xcfgsourcemigration.cpp`. Production qmake and the standalone xcfg shared-test project register the new source. Existing scan limits and incomplete-reference reporting are unchanged.

Qt 5.12.8/GCC 9.3 production Envoy rebuild succeeded. The standalone shared-configuration target built and passed all 151 tests, including both transitive-referrer/read-only and external/malformed-reference cases. Its cache location was redirected to `/tmp` so the test could create the transaction lock within the writable test area. No physical hardware or DAQ data was used.

## 71.517 Sentry status sampling separation (2026-10-25)

Moved the process probe and `SentryFiles::Status` from `apps/sentry/sentryservice.cpp` to `sentryservice_status.cpp`; status payload fields and failure behavior are unchanged. Sentry, simulator and Qt test qmake source lists all include the new unit.

Qt 5.12.8/GCC 9.3 rebuilt the Sentry executable, simulator and complete Qt test binary. Focused catalog/status and HTTP endpoint regressions passed; the HTTP test used the local loopback fixture. Documentation link check passed 2015 links. No physical hardware or DAQ raw was used.

## 71.518 RunControl transition execution separation (2026-10-25)

Moved ordered transition submission, acknowledgement/state confirmation, plan completion and failure handling into `apps/envoy/runcontrol_execution.cpp`; `core.pri` registers it for production and tests. Sequence numbers, journal phases, request order and post-acknowledgement state checks are unchanged.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the complete Qt test target. `simulatedLifecycle`, `readyDuringTransitionCannotStart` and `legacySelectedNodeProgressAndRegressRemainIndependent` passed. The TCP fixture tests ran with local loopback access. No physical hardware or DAQ data was used.
## 71.519 RunControl command and recovery separation (2026-10-25)

Moved legacy and Native Progress/Regress, Run start/stop, offline-tool gating and journal recovery handlers into `runcontrol_commands.cpp`; registered it through `core.pri`. Connection/polling/status handling and Sentry/catalog response coordination remain in `runcontrol.cpp`. Safety checks, replacement reservation and run snapshot identity matching are unchanged.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the full `getdaq-tests` executable. `simulatedLifecycle`, `readyDuringTransitionCannotStart`, `legacySelectedNodeProgressAndRegressRemainIndependent` and `runReplacementWaitsForAllCatalogsBeforeCleanupAndIncrement` passed. The three local TCP fixture tests required loopback access. No hardware or DAQ data was used.
## 71.520 Sentry HTTP request routing separation (2026-10-25)

Moved `SentryService::Handle` into `sentryservice_http.cpp`; registered it in the Sentry, simulator and Qt test qmake projects. Route and method checks, request identity validation, busy-job admission, delayed dispatch and compact HTTP replies are unchanged.

Qt 5.12.8/GCC 9.3 rebuilt Sentry, simulator and `getdaq-tests`. The focused Sentry catalog/replacement/config tests passed (6 tests); `sentryHttpEndpoints` passed with loopback access. No hardware or DAQ data was used.
## 71.521 Receiver catalog replacement preparation separation (2026-10-25)

Moved the candidate preparation phase to `receivercatalogreplacementprepare.cpp` and shared the directory durability check through private `receivercatalogreplacement_p.h`; registered the unit in the data-supervisor CMake target. Catalog schema, hardlink identity checks and replacement operation ordering remain unchanged.

Linux GCC/C++11 rebuilt the data-supervisor core, receiver catalog tests and `getdaq-catalog`. All 68 receiver catalog checks passed, including replacement preparation, chained retained raw, publication/finalization and restart recovery. Synthetic fixtures only; no physical receiver or production data was used.
## 71.522 Receiver catalog replacement observation separation (2026-10-25)

Moved read-only replacement phase inspection into `receivercatalogreplacementinspect.cpp`; shared transaction identity, marker and catalog match helpers are declared in the private replacement header and defined in the transaction implementation. Observation remains non-mutating and recovery-disabled.

Linux GCC/C++11 rebuilt the data-supervisor core, receiver catalog tests and `getdaq-catalog`. All 68 receiver catalog checks passed, including phase classification across prepare/publish/finalize and restart recovery. Tests used synthetic metadata/raw fixtures only.
## 71.523 Receiver catalog replacement finalization separation (2026-10-25)

Moved replacement finalization and verified superseded raw-link cleanup into `receivercatalogreplacementfinalize.cpp`; registered the source in the data-supervisor CMake target. Collision rules, inode/session checks, per-file journal markers and retry behavior are unchanged.

Linux GCC/C++11 rebuilt the supervisor core, receiver catalog tests and catalog CLI. All 68 receiver catalog checks passed, including old raw preservation, exact same-name cleanup and finalization retry/idempotence. Tests used synthetic metadata/raw fixtures only.
## 71.524 Receiver catalog replacement publication separation (2026-10-25)

Moved the publication operation into `receivercatalogreplacementpublish.cpp`; common transaction identity/marker/durability helpers now live in `receivercatalogreplacementhelpers.cpp`. The data-supervisor target registers both units alongside the dedicated prepare, inspect and finalize phases. Rename-no-replace and journal transition behavior are unchanged.

Linux GCC/C++11 rebuilt the data-supervisor core, receiver catalog tests and catalog CLI. All 68 receiver catalog checks passed across the complete replacement flow, including retry and cleanup safety. Synthetic metadata/raw fixtures only.
## 71.525 XcfgDocument shared scalar property projection separation (2026-10-25)

Moved class metadata projection, scalar resolution, declaration property enumeration, value/range validation and text encoding into `xcfgdocumentproperties.cpp`. Registered it in `xcfg.pri`, `xcfg_shared.pro` and `current_configuration_probe.pro`; source loading, lookup and edit transactions remain in `xcfgdocument.cpp`.

Qt 5.12.8/GCC 9.3 production Envoy and full Qt tests rebuilt; the standalone shared configuration suite passed all 151 tests with an isolated cache. Coverage includes include-derived class properties, equivalent/conflicting ranges, whitespace-safe scalar edits, and changed-source rejection.
## 71.526 Waveform frame decoding separation (2026-10-25)

Moved selected-frame `.graw/.gmat` decoding and integer ADC sample validation into `waveformframe.{h,cpp}`; the window retains navigation, metadata, display and metrics coordination. Registered the source in the shared qmake fragment for Envoy, standalone viewer and tests.

Qt 5.12.8/GCC 9.3 production Envoy, standalone viewer and full Qt test target rebuilt. Eight focused waveform viewer tests passed; the offscreen 3D surface/rotation smoke test passed with 96,353 changed pixels. No DAQ hardware or acquisition was used.

## 71.527 Config-tool scan-plan command separation (2026-10-25)

Moved offline scan-plan/policy/snapshot command handling and bounded file/staging helpers from the CLI entry point into `apps/configtool/scanplancommands.{h,cpp}`; registered the new unit in the production project and CLI probe. Command behavior and publication safeguards are unchanged.

Qt 5.12.8/GCC 9.3 rebuilt production `getdaq-config` and `getdaq-config-probe`. `tests/system_experiment_integration.py` passed all 68 process cases with the fresh CLI and offline native configuration inspector, including scan generation, policy/snapshot validation and overwrite rejection; the source configuration remained unchanged. No hardware or DAQ data was used.

## 71.528 Sentry catalog transaction separation (2026-10-25)

Moved legacy catalog creation and explicit replacement/finalization into `sentryservice_catalog.cpp`; shared journal writing, failure replies and strict old-catalog validation are implemented through private `sentrycatalog_p.h` and `sentrycatalog_helpers.cpp`. Registered both files in the Sentry, simulator and Qt test qmake projects. Receiver lock/seal checks, move ordering, collision policy and recovery semantics are unchanged.

Qt 5.12.8/GCC 9.3 rebuilt Sentry, simulator and the complete Qt test target. Five catalog/configuration tests passed; `sentryHttpEndpoints` passed with localhost loopback enabled. No physical receiver, hardware or production DAQ data was accessed.

## 71.529 Xcfg inherited-object override separation (2026-10-25)

Moved inherited object override planning and preview verification into `xcfgstructureeditor_override.cpp`, and include replacement/import expansion into `xcfgstructureeditor_include.cpp`; shared XML generation/error helpers are declared in the private `xcfgstructureeditor_p.h`. Registered both implementations with production and standalone shared-configuration qmake projects. No override, include or effective-configuration policy changed.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the complete Qt test target. All 151 shared-configuration tests passed, including shallow object override, inherited include expansion, concrete-sibling preservation, history reversibility and tampering/source-drift rejection. Documentation link validation passed 2015 local links.

## 71.530 Xcfg inherited materialization separation (2026-10-25)

Moved inherited-object materialization planning to `xcfgstructureeditor_materialize.cpp` and shared child insertion/path-tail helpers to private `xcfgstructureeditor_p.h`; production Envoy and standalone shared-configuration qmake targets register the new source. Effective values, include/comment handling and bounded traversal behavior are unchanged.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the complete Qt test target. All 151 shared-configuration tests passed, covering inherited subtree materialization, included descendants, preserved comments/quoted attributes, and history/preview source checks.

## 71.531 System acquisition hardware preflight separation (2026-10-25)

Moved the frozen-topology preflight into `systemacquisition_preflight.{h,cpp}` and registered the implementation in the hardware CMake target. Node identity, plan/sender validation, stopped-state observation order, audit recording and interlock behavior are unchanged.

Linux GCC/C++11 rebuilt `getdaq-hardware-core` and `hardware-system-acquisition-tests`. `hardware-system-acquisition-tcp` passed (1/1, 245.04 s) with synthetic local CoBo/MuTaNT endpoints. No physical hardware or acquired DAQ data was used.

## 71.532 Scan-plan runtime coordination separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and `getdaq-tests` after registering `scanplanwindow_runtime.cpp` in both qmake projects. `offlineScanPlanBuilderIsAvailableFromConsole` and `scanEventBaselineRequiresConnectedNativeController` passed. Documentation link validation passed 2015 local links. The complete Qt suite was attempted but could not complete in the sandbox because local listener creation was denied; configuration tests also reported busy source transactions. No hardware or DAQ data was used.

## 71.533 Receiver scan analysis separation (2026-10-25)

Linux GCC/C++11 rebuilt `getdaq-data-supervisor-core`, `getdaq-catalog`, `getdaq-supervisor-query` and `receiver-catalog-tests` after separating MFM threshold analysis from scan waveform analysis. `receiver-catalog-filesystem` passed (1/1, 23.81 s). Documentation link validation passed 2015 local links. The filesystem fixture used synthetic metadata and raw inputs; no hardware or production DAQ raw was accessed.

## 71.534 Native ECC failure reconciliation separation (2026-10-25)

Linux GCC/C++11 rebuilt `getdaq-ecc-native` and `native-ecc-coordinator-tests` with `nativeecccoordinator_reconciliation.cpp`. `native-ecc-host-ownership` and `native-ecc-fault-prepare-0` passed (2/2) using synthetic local Ice endpoints; the tests required localhost bind permission. No physical hardware or DAQ data was accessed.

## 71.535 Hardware plan execution separation (2026-10-25)

Linux GCC/C++11 rebuilt the hardware core plus client and system-acquisition test targets with `hardwareclient_execute.cpp`. The full `hardware-client-tcp` regression passed (1/1, 226.10 s) using synthetic local Ice devices. No physical hardware or production DAQ data was accessed.

## 71.536 ECC request codec separation (2026-10-25)

Linux GCC/C++11 rebuilt `getdaq-control-protocol`, `ecc-protocol-tests` and `hardware-client-tests` with `eccprotocol_request.cpp` registered in CMake. The ECC protocol executable passed its strict-field, enum, bounds, canonical-byte, key/result-binding and history checks. Qt 5.12.8/GCC 9.3 qmake rebuilt the ECC system-client test target using the updated shared `control.pri`; all 69 tests passed, including command identity, persist-before-submit, recovery and read-only observation checks. The first sandboxed launch could not bind its localhost fixture; the same test passed when rerun with loopback access. Documentation link validation passed 2015 local links. No physical hardware or DAQ data was used.

## 71.537 ECC configuration evidence codec separation (2026-10-25)

Linux GCC/C++11 rebuilt the control protocol, ECC protocol tests and hardware-client tests with `eccprotocol_configuration.cpp`. The portable ECC protocol executable passed strict schema, bounds, canonical encoding and identity-binding checks. Qt 5.12.8/GCC 9.3 qmake rebuilt the ECC system-client test target with both split codec units; all 69 tests passed, including configuration cache identity, historical Configure readbacks, forged-proof rejection, ownership state and recovery gates. The test used localhost fixtures. Documentation link validation passed 2015 local links. No physical hardware or DAQ data was used.

## 71.538 System acquisition failure handling separation (2026-10-25)

Linux GCC/C++11 rebuilt `getdaq-hardware-core` and `hardware-system-acquisition-tests` with `systemacquisition_failure.cpp`. The registered `hardware-system-acquisition-tcp` test passed (1/1, 233.94 s) using synthetic localhost Ice CoBo/MuTaNT endpoints. Both encoding 1.0 and 1.1 lifecycle scenarios passed, including global preflight, Pause/Resume, UNKNOWN outcomes and explicit alarm Stop; additional shared-buffer and trigger/counter initialization cases passed. No physical hardware or production DAQ data was accessed.

## 71.539 Remote catalog publication evidence separation (2026-10-25)

Linux GCC/C++11 rebuilt `getdaq-ecc-native`, `remote-handoff-evidence-tests` and `native-ecc-remote-receiver-tests` with the new catalog evidence unit. `remote-cold-audit-evidence` passed (1/1, 1.09 s), covering persisted handoff/catalog evidence identity and tamper checks; `native-ecc-remote-receiver-catalog-replace` passed (1/1, 5.58 s), covering replacement publish/receipt/finalize evidence against synthetic local services. No physical hardware or production receiver data was accessed.

## 71.540 Supervisor control journal recovery separation (2026-10-25)

Linux GCC/C++11 rebuilt `getdaq-data-supervisor-core`, `supervisor-control-tests` and `getdaq-supervisor` with `controljobs_recovery.cpp`. `supervisor-control-codec` passed (1/1); `supervisor-control-service` passed (1/1, 11.54 s), covering service restart and durable control-journal recovery; `data-supervisor-process-lifecycle` passed (1/1, 21.59 s), covering retained failed-session cleanup and lifecycle recovery. Both integration suites used synthetic local receiver processes. No physical hardware or production receiver data was accessed.

## 71.541 Configuration structure editor translation-unit separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and `getdaq-tests` with `configwindow_structure.cpp` registered in both qmake projects. `configurationStructureEditsEnterTheSameUndoHistory`, `configurationStructureMaterializationIsReviewedBeforeSaving`, `configurationStructureScalarPropertiesShowSharedChanges`, `configurationStructurePreviewCannotCrossOpenOrReadonlyBoundary` and `configurationGuiShallowObjectOverrideHasReviewAndUndo` passed. Data file tools close/reopen and active-child shutdown regressions passed across four test groups, including titlebar, Close, Escape, SIGTERM-ignoring child and menu/Run gating. Tests used isolated temporary Qt cache/home paths and synthetic local processes. No physical hardware or DAQ data was accessed.

## 71.542 MainWindow configuration presentation separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and `getdaq-tests` with `mainwindow_configpresentation.cpp` registered in both qmake projects. `nativeOwnershipViewExpiresAndRejectsLateConnectionReplies`, `nativeConfigurationViewSeparatesSelectionAndHardwareProof`, `compactConsoleLayoutKeepsParametersAndNodeRowsVisible`, `statusTableUsesOnlyConfiguredNodeHeight` and `consoleReflowsWhenConfiguredNodesShrink` passed. The tests used isolated temporary Qt cache/home paths and synthetic UI data only; no hardware or DAQ data was accessed.

## 71.543 ECC client observation policy separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt production Envoy, `getdaq-tests` and `ecc-system-client-tests` with `eccclientstore_observationpolicy.cpp` registered in the shared qmake project and direct client-probe target. `eccClientStoreClearsOnlyExactUnknownLifecycleOperation` passed, verifying that a matching known completion clears only its own unknown hold and remains clear after reopening the client store. Tests requiring localhost fixture binding stopped in this restricted sandbox with `Cannot bind fixture`; no hardware or DAQ data was used.

## 71.544 Native RunControl connection lifecycle separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt production Envoy, `getdaq-tests` and `native-gui-tests` with `nativeruncontrol_connection.cpp` registered through the shared Envoy core project. `wholeSystemOnceAndSealBeforeRunRecord` passed (3/3 including init/cleanup), covering Connect, lifecycle control, finalization, Disconnect and durable Run completion against a synthetic localhost ECC fixture. No physical hardware or DAQ data was used.

## 71.545 MainWindow recovery menu construction separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and `getdaq-tests` with `mainwindow_recoverymenus.cpp` registered in both qmake projects. `nativeGuiMetadataAndWholeSystemControls` passed (3/3 including init/cleanup), covering menu/action creation and recovery gates; `offlineDataToolsMenuWindowClosesAndCanReopen` passed all three close paths (5/5 including init/cleanup). Documentation link validation passed 2015 local links. No hardware or DAQ data was used.

## 71.546 MainWindow ECC observation views separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and `getdaq-tests` with `mainwindow_observations.cpp` registered in both qmake projects. Ownership observation expiry/late-response binding, accepted configuration versus hardware proof, node status/recovery gates and main GUI metadata/control tests passed (6/6 including init/cleanup). No hardware or DAQ data was used.

## 71.562 Scan plan runtime observation separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target with `scanplanwindow_observation.cpp` registered; the production Envoy qmake project parsed successfully. `offlineScanPlanBuilderIsAvailableFromConsole` passed (3/3 including init/cleanup), covering the offline scan-plan UI and its runtime/journal observation paths. Only the offscreen platform's `propagateSizeHints` warnings appeared. No hardware or DAQ data was used.

## 71.561 RunConfig YAML persistence separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target with `runconfig_yaml.cpp` registered through shared `core.pri`; the production Envoy qmake project parsed successfully. `yamlCompatibility` and `automaticRunSettingsRoundTrip` passed (6/6 including init/cleanup), covering legacy YAML defaults, UTF-8 fields, automatic-run settings serialization and rejection without mutating the existing config. No hardware or DAQ data was used.

## 71.560 CoBo PLL synthesis separation (2026-10-25)

Linux GCC/C++11 rebuilt `getdaq-experiment-core`, `getdaq-hardware-core` and `hardware-clock-tests` with `cobopllconfiguration.cpp` registered. The synthetic-device hardware clock test passed for firmware versions 1.0 and 1.1, covering PLL/R-CoBo/AsAd packet images, preflight checks and unknown-stop behavior. The Qt 5.12.8/GCC 9.3 `getdaq-tests` target also rebuilt through qmake with the new unit. No physical hardware or DAQ data was used.

## 71.559 Scan plan UI construction separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target with the four new scan-plan UI builders registered in the test qmake project; the production Envoy qmake project also parsed successfully. `offlineScanPlanBuilderIsAvailableFromConsole` passed (3/3 including init/cleanup), exercising dialog creation, controls, scan plan creation/review and related offline lifecycle UI. The offscreen Qt platform emitted only its known `propagateSizeHints` warnings. No hardware or DAQ data was used.

## 71.558 Scan journal binding and point replay separation (2026-10-25)

Linux GCC/C++11 rebuilt `getdaq-experiment-core`, `scan-execution-state-tests` and `system-experiment-tests` with `scanexecutionjournal_decode_bindings.cpp` registered. Both tests passed, exercising snapshot/runtime binding, receiver event baselines, point transitions, operation recovery and tamper rejection. Documentation-link validation passed 2025 local links. No hardware or DAQ data was used.

## 71.557 Scan journal operation-event replay separation (2026-10-25)

Linux GCC/C++11 rebuilt `getdaq-experiment-core`, `scan-execution-state-tests` and `system-experiment-tests` with `scanexecutionjournal_decode_operations.cpp` registered. Both tests passed, exercising intent-before-action, restart quarantine, reconciliation and tamper rejection. Documentation-link validation passed 2024 local links. No hardware or DAQ data was used.

## 71.556 Scan journal managed-service replay separation (2026-10-25)

Linux GCC/C++11 rebuilt `getdaq-experiment-core`, `scan-execution-state-tests` and `system-experiment-tests` with `scanexecutionjournal_decode_managedservice.cpp` registered. Both tests passed, covering scan lifecycle and managed-service journal evidence/replay alongside strict artifact decoding. Documentation-link validation passed 2023 local links. No hardware or DAQ data was used.

## 71.555 Scan journal managed-service lifecycle separation (2026-10-25)

Linux GCC/C++11 rebuilt `getdaq-experiment-core`, `scan-execution-state-tests` and `system-experiment-tests` with `scanexecutionjournal_managedservice.cpp` registered. Both tests passed; scan lifecycle, restart quarantine and tamper-rejection coverage remains green. Documentation-link validation passed 2022 local links. No hardware or DAQ data was used.

## 71.554 Scan journal successor validation separation (2026-10-25)

Linux GCC/C++11 rebuilt `getdaq-experiment-core`, `scan-execution-state-tests` and `system-experiment-tests` with `scanexecutionjournal_successor.cpp` registered. Both tests passed; the scan test exercises journal lifecycle, intent-before-action, restart quarantine and tamper rejection. Documentation-link validation passed 2021 local links. No hardware or DAQ data was used.

## 71.553 System scan point artifact validation separation (2026-10-25)

Linux GCC/C++11 rebuilt `getdaq-experiment-core` and `scan-execution-state-tests` with `systemscanplan_pointvalidation.cpp` registered. The scan execution test passed, covering lifecycle, intent-before-action journal behavior, restart quarantine and tamper rejection; the system experiment codec test also passed, covering bounded strict JSON and malformed artifact inputs. The point validator preserves per-kind artifact checks and continues to run within the same full-plan validation path. No hardware or DAQ data was used.

## 71.552 Scan deployment bundle verification separation (2026-10-25)

Linux GCC/C++11 rebuilt `getdaq-ecc-native`, `getdaq-scan-deploy` and `fixed-ecc-configuration-tests` with `scanpointdeployment_verify.cpp`. The fixed ECC configuration/scan deployment integration executable passed against synthetic local Ice fixtures, covering offline bundle creation, verification against the frozen execution snapshot, CLI create/verify behavior, sealed artifact and receiver-plan validation, and altered manifest/plan rejection. The test reports that no hardware executed. No physical hardware or DAQ data was used.

## 71.551 Run replacement transaction separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and `getdaq-tests` with `runrecorder_replacement.cpp` registered through the shared qmake project. `runRecorderExplicitReplacementRetainsOldRunUntilFinalization`, `runRecorderReplacementRejectsIncompleteOrMismatchedRun`, `runRecorderWritesNativeReferencesWithOptionalCatalogMirror` and `runRecorderCompletesNativeRunInDurableOrder` passed. Synthetic-localhost `runReplacementWaitsForAllCatalogsBeforeCleanupAndIncrement` passed with loopback enabled, verifying Run replacement waits for every node catalog before cleanup and numbering advance. No physical hardware or DAQ data was used.

## 71.550 Run configuration snapshot storage separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and `getdaq-tests` with `runrecorder_snapshot.cpp` registered through the shared qmake project. `configurationSnapshotClosureAndRetry` and `configurationSnapshotMissingInclude` passed, covering external include capture, immutable retry verification, corrupt-snapshot rejection and missing-include failure. `runRecorderExplicitReplacementRetainsOldRunUntilFinalization` and `runRecorderReplacementRejectsIncompleteOrMismatchedRun` passed; the synthetic-localhost `runReplacementWaitsForAllCatalogsBeforeCleanupAndIncrement` integration passed with loopback enabled. No physical hardware or DAQ data was used.

## 71.549 Scan plan review snapshot storage separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the complete `getdaq-tests` target with `scanplanreview_snapshot.cpp` registered in both qmake projects. `offlineScanPlanBuilderIsAvailableFromConsole` passed (3/3 including init/cleanup), exercising private review snapshot creation, duplicate no-overwrite behavior, review journal creation/idempotence, frozen runtime-context loading and recovery/reconciliation paths. This Linux run also compiled the POSIX directory-permission implementation; the Windows ACL implementation was moved intact but not compiled here. No physical hardware or DAQ data was accessed.

## 71.548 EccSystemClient CoBo maintenance API separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt production Envoy, native GUI tests and ECC system-client tests after moving fixed-target maintenance queries and selected-CoBo maintenance command adapters into `eccsystemclient_maintenance.cpp`. `selectedMaintenanceBindsMetadataAndPersistsBeforePost`, `selectedLedSubmissionPersistsExactPreparedTargetBeforePost` and `selectedMaintenanceRejectsForeignTargetsAndWrongState` passed (5/5 including init/cleanup), verifying reviewed-target binding, persist-before-submit ordering and rejection of unselected or ineligible boards. No physical hardware or DAQ data was accessed.

## 71.547 EccSystemClient control-command separation (2026-10-25)

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and `ecc-system-client-tests` with `eccsystemclient_control.cpp` registered in production, test and direct probe qmake projects. `productionStoreReopensPendingWithoutReplay` and `reviewedPlanRetirementPersistsAndHoldsEveryNewCommand` passed (4/4 including init/cleanup) using synthetic localhost services. No physical hardware or DAQ data was used.
## 71.656 Bound scan catalog source verification shared by analysis modes (2026-10-26)

Moved the duplicated receiver-catalog raw-file access path from threshold and waveform scan analyzers into `receivercatalog_scansource.cpp`. It resolves the requested source from the immutable receiver request and catalog manifest, validates the source-directory index and recorded device/inode/size/type, opens raw files with `openat(..., O_NOFOLLOW)`, checks descriptor/path identity before and after the consumer reads, and confirms the request, manifest and completion receipt remain unchanged. Threshold hit-pattern and AutoGain/Pulser/PeakingTime waveform consumers now share that implementation. A subsequent scan-point loader extraction also moved per-point binding, catalog selection and target-CoBo artifact checks behind one private interface; metric calculations, CLI commands and report JSON remain with their coordinators.

A fresh CMake configure with GCC 9.4 rebuilt `getdaq-data-supervisor-core`, `receiver-catalog-tests` and `getdaq-catalog`. The receiver catalog integration executable reported **62 native checks passed**, including standard and compact type-2 AutoGain, layered MFM, Pulser and PeakingTime waveform response, threshold hit-pattern aggregation, catalog integrity and CLI cases. It used bounded synthetic files only; no hardware, live receiver or external network was used. The full repository test suite and other platform builds were not run for this source-only split.

## 71.713 Waveform decoder format separation (2026-10-27)

Qt 5.12.8/GCC 9.3 rebuilt the full `getdaq-tests` binary and the standalone `getdaq-waveform-viewer` after splitting type-1 and compact type-2 sample decoding into separate implementation units. All eight waveform-viewer tests plus the integrated viewer run-gating test passed (11/11 including QtTest setup/cleanup), covering raw/layered type-1, compact type-2, truncated MFM, analysis, navigation, export and run gating. The tests used synthetic/raw fixture files; no physical hardware or external receiver data was used. The full repository suite and non-Linux platform builds were not run for this change.

## 71.714 MFM merge input indexing separation (2026-10-27)

Fresh CMake/GCC 9.4 Release configuration of `apps/datareceiver` built all default tools and tests, including `getdaq-merge` and `merge-tests` with the new `merge_index.cpp` unit. CTest passed 7/7. The merge regression covers sorted event grouping, stable input/offset ordering, unsupported-frame filtering, byte-preserved subframes, existing-output protection, input/output alias rejection and truncated-header rejection before output creation. Two tests requiring local sockets initially failed under sandbox restrictions and passed when rerun with local loopback enabled. All test files were small and temporary; no external network, receiver or physical hardware was used. Optional FITS/ROOT profiles and non-Linux builds were not run.

## 71.715 Exclusive MFM merge output reservation (2026-10-27)

GCC 9.4/C++11 rebuilt every default DataReceiver tool and test target after switching `getdaq-merge` to exclusive POSIX output creation and bounded descriptor writes. CTest passed 7/7; the two local-loopback tests passed when rerun with socket access. `mfm-merge` now forks concurrent merge processes using the same requested output and verifies they reserve separate numbered paths with identical contents. This covers the prior check-then-truncate race while preserving existing output bytes and event payloads. No hardware, external network or production data was used. Optional FITS/ROOT and non-Linux profiles were not built.

## 71.716 Configuration selection export publication boundary (2026-10-27)

Qt 5.12.8/GCC 9.3 rebuilt the complete Qt test binary and standalone `getdaq-config-upload` after selection export was moved to `apps/configtransfer/selectionfile.cpp`. The focused Qt test passed, verifying exact file bytes, owner-read-only permissions, refusal to replace an existing destination, cleanup after failed publication, and rejection of empty content. CLI `--help` succeeded. These checks did not contact a repository service or production configuration; the broader repository/ECC selection integration was not rerun.

