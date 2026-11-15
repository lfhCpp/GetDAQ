# 71.774 Offline converter CLI argument parsing shared

Moved duplicate unsigned-number parsing and option-value extraction from the matrix, FITS and ROOT converter entry points into the private C++11 header `apps/datareceiver/cli_options.h`. The command-specific option sets, option matching, error wording, help text, converter calls and output summaries remain at their respective entry points. The helper retains decimal `strtoull` parsing, rejection of empty/negative/trailing-garbage values, uint64 bounds and support for both `--key value` and `--key=value`.

Built `getdaq-matrix` and `getdaq-fits` from `/tmp/getdaq-fits-refactor-build`; configured an optional ROOT 6.26 / CFITSIO 3.47 build and linked `getdaq-root`. Then rebuilt the current Linux full profile: 30 programs, 1,054 receipt-bound inputs and all 109 embedded dependency files passed verification. No tests, CLI commands, file conversions, GUI flows or hardware operations were run. Target platform/hardware acceptance remains open.

# 71.773 Current-source Windows Qt 5.14.2 control-suite build

The Windows Qt 5.14.2 / MinGW 7.3 x64 toolchain rebuilt the current `control` profile from source in `build-windows-refactor-772`. All nine programs were emitted as x86_64 PE files, including `getdaq-waveform-viewer`; the receipt binds 1,054 inputs and embedded dependency verification passed offline. The compiler log contains no warning/error lines. Receipt: `build-windows-refactor-772/windows-build-receipt.json`.

Build-only evidence using the installed Windows Kit through WSL interop with elevated local build permission. No tests, launch/GUI flow, deployment smoke or hardware flow were run. The receipt records `hardware_accepted=false` and `independent_windows_machine_accepted=false`; macOS, independent Windows deployment and field acceptance remain open.

# 71.772 Current-source Linux full host-suite build after Waveform Viewer CLI split

`python3 tools/build-suite.py --profile full --build-root /tmp/getdaq-full-refactor-772 --jobs 4` completed with status `complete`: all 30 current-source host programs built; 1,053 input records are bound in the receipt and all 109 embedded dependency files passed offline verification. This includes `getdaq-waveform-viewer` with the new standalone CLI module. Receipt: `/tmp/getdaq-full-refactor-772/full/build-receipt.json`.

Build-only evidence on Linux/WSL2 with CMake 3.16.3, Qt 5.12.8 and GNU Make 4.2.1. No tests, startup/GUI smoke, runtime service flow, hardware access or DAQ data flow were run. The receipt explicitly records `target_platform_and_hardware_accepted=false`; Windows, macOS, independent-host and field acceptance remain open.

# 71.771 Waveform viewer CLI separated from GUI entry point

Moved offline PeakingScan and PulserResponse argument parsing, analysis dispatch, CSV output, and their existing help text from `apps/waveformviewer/main.cpp` into `waveformviewer_cli.cpp/.h`. The GUI entry now handles only CLI handoff, the surface smoke flag, and Qt window startup. Registered the CLI source only in the standalone viewer target; Envoy and aggregate Qt targets continue to use `waveformwindow.pri` without pulling in the standalone `main` or CLI. Command names, argument parsing, output fields, diagnostics, and GUI startup path were moved without intentional behavior changes.

Built the standalone viewer with Linux Qt 5.12.8/C++11 from a fresh qmake shadow directory; compilation and linking succeeded, including the new CLI unit. No command, GUI smoke, or tests were run. Broader CLI regression and Windows/macOS builds remain open.

# 71.770 Current-source optional ICE ingress build

Rebuilt the optional `getdaq-ice-receiver` executable and `getdaq-ice-ingress-core` library from current source in the existing ICE-enabled native CMake tree. This confirms the shared disk-reserve unit added at 71.762 is registered in and compiles through both production receiver paths. No tests, receiver launches, data traffic, hardware access or field acceptance were performed. This does not change the status of direct DataExporter replacement validation.

# 71.769 Scan-plan command implementations separated by scan type

Split PeakingTime, Pulser, Threshold and AutoGain plan-generation commands from the shared dispatcher into `scanplancommand_{peaking,pulser,threshold,autogain}.cpp`. The dispatcher now selects by command name; each implementation retains its existing argument-count gate, parsing rules, point-generation call, artifact order, manifest fields, output text and error handling. Both `apps/configtool/configtool.pro` and the strict `tests/configtool_probe.pro` register all four units. Linux Qt 5.12.8 production ConfigTool and fresh C++11 `-pedantic-errors` direct-source probe compiled and linked. No tests or plan-generation commands were run.

# 71.768 Current-source Linux full host-suite build

`python3 tools/build-suite.py --profile full --build-root /tmp/getdaq-full-refactor-767 --jobs 4` completed with status `complete`: all 30 current-source host programs built and all 109 embedded dependency files were verified without network access. The receipt records Linux/WSL2, `target_platform_and_hardware_accepted=false`. No tests, launch smoke, GUI flow, hardware calls or DAQ data operations were performed. This proves the registered Linux full-profile sources build together, not full application correctness or target-platform/field acceptance. Receipt: `/tmp/getdaq-full-refactor-767/full/build-receipt.json`.

# 71.767 Generated scan-plan publication shared

Moved generated scan-plan target preflight and staged directory publication into `scanplancommands_publication.cpp`, shared by the PeakingTime, Pulser, Threshold and AutoGain CLI paths. Each command still parses its own arguments, builds its own points and writes its point artifacts in the same order; the helper owns the parent/target checks, private staging directory, manifest write and final no-replace directory rename. Registered the helper in the standalone ConfigTool and strict direct-source probe qmake projects. A Linux Qt 5.12.8/C++11 production ConfigTool build compiled and linked successfully; no tests or command/runtime flows were run.

# 71.766 Failure-only system unprepare separated from normal release

Moved `UnprepareAfterFailure`, `VerifyUnpreparedAfterFailure`, and their retained cleanup/verification audit-chain accessors from `systempreparation_unprepare.cpp` into `systempreparation_failureunprepare.cpp`. Normal `Unprepare` and its proof accessor remain in the normal-release unit. Preconditions, per-CoBo ordering, no-replay behavior, failure latching, alarm-subscription handling and audit-chain contents were moved unchanged. Registered the source in the hardware CMake target; Linux rebuilt `getdaq-hardware-core` successfully. No tests or runtime hardware operations were run.

# 71.765 Hardware plan structural validation separated

Moved deterministic, local-only validation of bound-description prerequisites, annotations, conditional/read references, polling-only arguments and LED argument placement from `HardwareClient::Process` into `hardwareclient_planvalidation.cpp`. The coordinator still owns step-count validation, audit-chain reset, remote metadata/width checks, operation serialization and execution. Validation order, input-dependent errors and the bound-description state snapshot are unchanged. Registered the new unit in the hardware CMake target and rebuilt `getdaq-hardware-core` successfully on Linux; no tests, runtime hardware calls or physical-device validation were run.

# 71.764 Native ECC receiver failure cleanup separated

Moved receiver Abort/Retire recovery coordination out of the general failure-recovery translation unit while preserving its parent-stop and cleanup proofs, journal binding, audit order, and UNKNOWN semantics. Registered the source in the native ECC library and rebuilt that library on Linux. No recovery tests or runtime flow were run.

# 71.763 Configuration value save coordination separated

Moved scalar-value save/UI coordination, multiline editing and override-preview cleanup to a dedicated Envoy source unit. Preserved wildcard confirmation, read-only connection behavior, background validation/save, and line-ending review. Production and aggregate Qt qmake projects register the unit; all nine Linux control programs rebuilt. No tests or GUI interaction were run.

# 71.762 Shared receiver disk-reserve policy separated

Moved disk-reserve arithmetic and check-window cadence out of `FrameStore` into a shared native policy unit used by both the raw TCP receiver and optional ICE ingress receiver. Updated the strict ICE build manifest's compile-count/hash inputs. The affected Linux CMake targets rebuilt successfully. No tests, strict standalone ICE build, runtime acquisition, or hardware validation were run.

# 71.761 Configuration value editor delegate separated

Moved type/range-aware Qt editor creation and value commit handling from the `ConfigWindow` coordinator into a private delegate module. Production Envoy and aggregate Qt tests both register the module. The Linux control profile rebuilt all nine programs with a complete receipt. No tests or GUI/runtime interaction were run.

# 71.760 DataReceiver identity policy separated

Moved receiver configuration/identity validation from the socket lifecycle translation unit to `receiver_identity.cpp`, preserving the existing conditions and diagnostics. The standalone CMake target rebuilt and linked. No protocol test, runtime sender, or hardware acceptance was performed.

# 71.759 Sentry HTTP request validation separated

Moved Sentry route/method resolution and bounded catalog request JSON validation from `SentryService::Handle` into `sentryservice_http_validation.cpp` with a private request value type. The HTTP coordinator retains service-busy admission, catalog-busy ownership, settle delay, async job lifecycle, and replies. Kept the original order in which route/method checks precede busy admission and mutation-body validation follows it. Registered the source/header in Sentry, Simulator and aggregate Qt test projects. A fresh Linux Qt control profile compiled all nine programs, including Sentry and Simulator, with a complete build receipt; the aggregate Qt test target was not part of that profile and no tests or runtime flow were run. Windows, macOS and hardware acceptance remain open.

# 71.758 ICE ingress status projection separated

Moved `IceIngressRecorder::Status()` JSON serialization from recorder lifecycle/input handling into `apps/iceingress/iceingress_status.cpp`. The response field names, order, values, error mutex boundary, and atomic reads are retained. Registered the unit in the optional ICE receiver library and updated the strict C++11 source-count expectation from 19 to 20. After 71.757–71.758, the current CMake `getdaq-ice-receiver` target built successfully with both units; protocol tests, strict standalone receipt and runtime/hardware validation were not run. The ICE application/platform audit remains open.

# 71.757 ICE ingress output preflight boundary

Moved ICE receiver storage-mode checks, output-directory emptiness checks, and non-writing storage-constructor validation into `apps/iceingress/iceingress_preflight.cpp`. The recorder still acquires the directory lease before this preflight and starts its writer only after validation succeeds. Preserved the validation order and existing errors. Registered the unit in the optional ICE receiver library and updated the strict C++11 source-count expectation from 18 to 19. The current CMake `getdaq-ice-receiver` target built successfully after this and the 71.758 extraction; protocol tests, strict standalone receipt and runtime/hardware validation remain unverified. The ICE application/platform audit remains open.

# 71.756 Configuration value save and effective review moved off the UI thread

Scalar declaration saves and the edit-history before/after effective snapshots now run in a QtConcurrent worker with a window-modal progress indicator, so large wildcard configurations do not block Qt's event loop. `[*]` edits require confirmation and explain inheritance precisely: instances without their own value declaration inherit the changed default; explicit declarations remain unchanged. The UI accepts no overlapping configuration edits during the save and refreshes history/tree when it completes. Linux Qt 5.12.8 production Envoy rebuilt and linked; GUI responsiveness was not manually checked. See [validation 71.756](VALIDATION.md#71756-configuration-value-save-and-effective-review-moved-off-the-ui-thread-2026-11-10).

# 71.755 Configuration values edit inline with type-aware controls

Scalar values now edit directly in the tree on double-click/F2. Enumerations and booleans use dropdowns, bounded integers use range-aware spin boxes, and other scalar types use typed text fields. Source comparison confirms the old editor behaved this way; its multiselection menu supports copy/cut/delete only and has no batch-value editing action. See [validation 71.755](VALIDATION.md#71755-configuration-values-edit-inline-with-type-aware-controls-2026-11-10).

# 71.754 Configuration editor workspace and value editing improved

Reworked the GET configuration window so the source tree and editing controls occupy a dedicated **Edit configuration** tab, while multi-document save, XML export, workspace copy, binary conversion, repository, hardware-description, snapshot, and publication actions live in **Configuration tools**. Selection text distinguishes objects from editable scalar parameters; Find value source selects/scrolls to the resolved source declaration. A larger modal editor remains available for multiline values; object double-click opens its structure editor. See [validation 71.754](VALIDATION.md#71754-configuration-editor-workspace-and-value-editing-improved-2026-11-10).

# 71.753 Receiver catalog source-session validation separated

Moved source receiver lease acquisition, seal/configuration binding, raw-file identity/size validation, and repeatable source snapshot checks into `apps/datasupervisor/receivercatalog_sources.cpp`. `ReceiverCatalog::Impl` retains target/replacement preconditions, transaction orchestration, durable intent publication, hard-link creation, and completion receipt publication. Source leases and validation order are preserved. Registered the source in `apps/datasupervisor/CMakeLists.txt`. The production DataSupervisor core library and `getdaq-catalog` executable compiled and linked on Linux. No tests or runtime/catalog/hardware flow was run. This is a bounded responsibility split; the full DataSupervisor responsibility and platform audit remains open. See [validation 71.753](VALIDATION.md#71753-receiver-catalog-source-session-validation-separated-2026-11-04).

# 71.752 Simulator configuration-workspace generation separated

Moved simulator-only XCFG and initial `envoy.yml` generation from `Simulator::Start` into `simulator_configuration.cpp`, separating workspace/config generation from ECC and Sentry listener startup. Moved existing `.graw` accounting, symlink/type checks and aggregate byte-budget enforcement into the simulator data unit beside timed writes. Existing JSON-as-YAML values and preservation of an existing Run configuration remain unchanged. Registered the source in standalone Simulator and aggregate Qt qmake targets. The standalone Simulator and aggregate `getdaq-tests` executable compiled and linked with Qt 5.12.8; standalone Simulator also compiled and linked with Windows Qt 5.14.2/MinGW 7.3 as x86_64 PE. No tests or simulator/runtime/hardware flow was run. The Windows evidence covers Simulator only, not a fresh full nine-program control profile. See [validation 71.752](VALIDATION.md#71752-simulator-configuration-workspace-generation-separated-2026-11-04).

# 71.751 Native Windows control-suite build

At 71.751, the Qt 5.14.2 / MinGW 7.3 x64 `control` profile rebuilt all nine programs, including the 71.750 scan confirmation units. The receipt records `complete`, all outputs are x86_64 PE, all 1,031 recorded source/dependency inputs matched their hashes at build time, and embedded dependency verification passed offline. This build predates the Simulator source changes recorded at 71.752. The compiler log contains no warning/error lines. No tests, GUI/runtime launch, deployment smoke, macOS or hardware flows were run. Application and platform acceptance remains incomplete. See [validation 71.751](VALIDATION.md#71751-native-windows-qt-5142-control-build-with-scan-confirmation-split-2026-11-04).

# 71.750 Native scan confirmation responsibilities separated

Split scan lifecycle-operation confirmation from scan Run Start/Stop confirmation and event-target receiver finalization into separate production units. The shared router retains fresh ECC status validation, exact recovery operation matching, and unresolved-journal handling; operation ordering and persistence behavior are unchanged. Production Envoy rebuilt and linked from a clean Qt 5.12.8 qmake shadow directory. No tests or runtime/hardware flows were run. This is a bounded Envoy responsibility split, not completion of the scan workflow or the application audit. See [validation 71.750](VALIDATION.md#71750-native-scan-confirmation-responsibilities-separated-2026-11-04).

# 71.749 Current-source Linux full host build

A fresh `tools/build-suite.py --profile full` build at `/tmp/getdaq-host-full-748` completed with all 30 host programs and 109 embedded dependency files verified. This includes the 71.748 Sentry seal-validation boundary and current Qt control sources. No tests or runtime/hardware flows were run; the receipt keeps `target_platform_and_hardware_accepted=false`. See [validation 71.749](VALIDATION.md#71749-current-source-linux-full-host-build-2026-11-04).

# 71.748 Sentry receiver-seal validation boundary

Moved Native receiver-seal decoding and incoming `.graw` name/size reconciliation out of the Sentry archive transaction into `apps/sentry/sentrycatalog_seal.cpp`; the file-byte accumulator now explicitly rejects unsigned overflow. Registered the helper in Sentry, Simulator and aggregate Qt tests. Linux Qt 5.12.8 Sentry and Simulator production builds linked successfully. The current Windows Qt 5.14.2/MinGW build rebuilt all nine control programs with no warning/error output. No tests or runtime/archive operation was run. See [validation 71.748](VALIDATION.md#71748-sentry-receiver-seal-validation-boundary-2026-11-04).

# 71.747 Windows Qt warning cleanup and control-suite rebuild

Clarified warning-prone control flow in Xcfg conversion and Envoy transfer/recovery dialogs, removed an unused codec helper, and replaced deprecated `QSet::fromList` with Qt 5.12-compatible key insertion. The affected Linux Envoy project compiled and linked, and the native Windows Qt 5.14.2 / MinGW 7.3 build rebuilt all nine x86_64 control executables. The refreshed Windows log has no compiler warnings or errors. No tests, runtime launch, deployment, macOS or hardware flow was run. See [validation 71.747](VALIDATION.md#71747-windows-qt-warning-cleanup-and-control-suite-rebuild-2026-11-04).

# 71.746 Native Windows Qt 5.14.2 control-suite build

The actual Qt 5.14.2 / MinGW 7.3 x64 kit rebuilt the current `control` profile from a fresh shadow build through the Windows qmake toolchain. All nine executables were verified as x86_64 PE files and the build script confirmed the source tree did not change during compilation. Receipt: `build-windows-refactor-745/windows-build-receipt.json`. There were 27 emitted compiler warning instances across repeated shared-source builds; the build succeeded. QtTest, GUI launch/manual interaction, independent deployment and hardware acceptance were not run. This closes the Windows production-build gate for the current control profile only. See [validation 71.746](VALIDATION.md#71746-native-windows-qt-5142-control-suite-build-2026-11-04).

# 71.745 Envoy standalone Qt build and binding-index validation

Built the production Envoy qmake project from a fresh shadow directory, including the shared clients and integrated waveform viewer. The build exposed a signed/unsigned scan-binding index comparison; validation now checks that the JSON value is an exact non-negative integer within `unsigned int` range before comparing it to the journal index. The updated source compiled and `GetDAQ` relinked. No tests or runtime flows were run; this closes only a Linux Qt production-build check, not Envoy's full responsibility audit or Windows/macOS/hardware acceptance. See [validation 71.745](VALIDATION.md#71745-envoy-standalone-qt-build-and-binding-index-validation-2026-11-04).

# 71.744 Configuration repository filesystem adapter separated

Moved repository POSIX file/directory identity, stable bounded reads, exclusive synced file publication, directory enumeration, and descriptor RAII to private `configurationrepository_files_p.h/.cpp`. Repository object/version checks, fault latching, and commit sequencing remain in `ConfigurationRepository`. The standalone Linux `getdaq-configuration-repository` target rebuilt successfully; regression/service scenarios were not run. This backend remains POSIX-only. See [validation 71.744](VALIDATION.md#71744-configuration-repository-filesystem-adapter-separated-2026-11-04).

# 71.743 SystemPreparation plan compilation separated

Extracted immutable configuration copy/normalization, topology and cross-device constraint checks, and CoBo/MuTaNT hardware step compilation to private `systempreparationplan_p.h/.cpp`. This unit performs no hardware RPC. The coordinator retains client creation and Prepare/Configure orchestration. Registered the new source in `getdaq-hardware-core`; the Linux native target built successfully. No hardware-simulator or physical-hardware flow was run. Full hardware and platform audit remains open. See [validation 71.743](VALIDATION.md#71743-systempreparation-plan-compilation-separated-2026-11-04).

# 71.742 DataReceiver merged-output transaction separated

Moved merged-event encoding, output-file reservation/rotation, exact frame copying, durability, and rollback cleanup to private `merge_output_p.h/.cpp`. The merge coordinator still indexes/sorts and selects events, then tracks results. Registered the module in production and test CMake targets. A clean standalone Linux C++11 `getdaq-merge` build passed; merge tests were not run. Cross-platform and application-wide audits remain open. See [validation 71.742](VALIDATION.md#71742-datareceiver-merged-output-transaction-separated-2026-11-04).

# 71.741 ICE ingress persistence strategies separated

Split the ICE recorder's byte-storage and frame-storage writer implementations into `iceingress_writer_bytes.cpp` and `iceingress_writer_frames.cpp`. The recorder still coordinates bounded queueing, end/failure lifecycle, status, and receipt publication. Updated the strict ICE source-count contract to 18 and coverage inventory to eight production `.cpp` files. The focused Linux `getdaq-ice-receiver` target rebuilt successfully. The standalone strict ICE build/archive validation and its synthetic integration cases were not rerun. Full ICE/application/platform responsibility audits remain open. See [validation 71.741](VALIDATION.md#71741-ice-ingress-writer-strategies-separated-2026-11-04).

# 71.740 Current-source Linux full host build

A fresh full-profile build from current source completed with status `complete`: all 30 host programs built and 109 embedded dependency files passed offline checksum verification. This includes the latest ECC recovery service units and all qmake control programs. Evidence is `/tmp/getdaq-host-full-739/full/build-receipt.json`. The build ran under Linux/WSL2 with Qt 5.12.8/GCC 9; it is not native Windows/macOS or hardware acceptance. No tests were run here, and a clean single-invocation 91/91 CTest result remains pending. See [validation 71.740](VALIDATION.md#71740-current-source-linux-full-host-build-2026-11-04).

# 71.739 ECC recovery immutable service policy extracted

Moved fixed service configuration parsing, pinned input ownership and digest checks, task-root/lock setup, immutable policy creation, and revalidation into `recoveryservicepolicy_p.h/.cpp`. The service coordinator retains authenticated HTTP endpoints, task admission, and native child execution; durable task history is handled by `RecoveryTaskStore`. The production handoff-service target rebuilt, and synthetic localhost `cold-handoff-service-0` passed with `raw_remaining=0`; sanitizers and physical hardware were not used. Overall ECC and application responsibility/platform audits remain incomplete. See [validation 71.739](VALIDATION.md#71739-ecc-recovery-immutable-service-policy-extracted-2026-11-04).

# 71.738 ECC recovery task storage boundary extracted

Moved recovery-task history loading, request/state integrity checks, durable request creation, and atomic state persistence out of the HTTP/process coordinator in `recoveryservice.cpp` into private `recoverytaskstore_p.h/.cpp`. Bounded pinned-file reads and metadata path checks are in `recoveryfiles_p.h/.cpp`. The service retains configuration/policy and credential validation, lease ownership, request validation, HTTP routing and process orchestration. Registered both units in the handoff-service CMake target. The handoff service rebuilt, and synthetic localhost `cold-handoff-service-0` passed against the actual ECC service process, two data services and native receivers; runner receipt reports `passed=true`, `raw_remaining=0`. This is a focused recovery-path integration, not hardware or cross-platform acceptance. Full ECC and application responsibility audits remain open. See [validation 71.738](VALIDATION.md#71738-ecc-recovery-task-storage-boundary-extracted-2026-11-04).

# 71.737 Remote handoff history reduction separated

Moved the historical delivery-phase reducer out of `RemoteHandoffEvidence::Impl` into private `remotehandoffevidence_delivery.{h,cpp}`. The evidence owner retains filesystem binding/revalidation; the reducer classifies journaled submission, explicit rejection, acceptance, completion receipts and node outcomes. This only classifies recorded evidence and grants no replay authority. Registered in the ECC CMake library. Rebuilt the ECC native library and evidence test executable; `remote-cold-audit-evidence` passed 1/1 with synthetic metadata. Other ECC responsibilities and platform acceptance remain open. See [validation 71.737](VALIDATION.md#71737-remote-handoff-delivery-history-reducer-separated-2026-11-04).

# 71.736 Catalog replacement observation query split

Moved catalog replacement observation retrieval and identity/evidence validation into private `queryrunner_catalogreplacement.{h,cpp}`. The helper keeps the query bound to the persisted Publish operation and current service, rejects recovery-authorizing or raw-reading observations, and returns only the validated response. Registered in Windows/POSIX CMake and qmake. Linux CMake and strict qmake builds passed; the synthetic supervisor-control service integration passed 1/1 and Windows registration contracts 20/20. Native Windows compilation and the full platform audit remain open. See [validation 71.736](VALIDATION.md#71736-catalog-replacement-observation-query-split-2026-11-04).

# 71.735 Supervisor query transition-proof validation separated

Moved plan-retirement and activated-predecessor response validation into private `queryrunner_plantransition.{h,cpp}`. `queryrunner.cpp` retains identity setup, selecting the GET endpoint, and command output; proof target, owner, request-key, digest and stopped-cycle checks are unchanged. Registered in Windows/POSIX CMake and standalone qmake projects. Linux CMake and strict qmake builds passed; the supervisor control and plan-transition integration CTests passed 2/2, and Windows build-entrypoint contracts passed 20/20. The Windows contract verifies registration, not native execution. Full platform and application audits remain open. See [validation 71.735](VALIDATION.md#71735-supervisor-query-plan-transition-proof-validation-separated-2026-11-04).

# 71.734 DataSupervisor control journal storage extracted

Moved supervisor control journal file lifecycle and integrity checks into `controljobs_journal_p.h` / `controljobs_journal.cpp`. The component owns the fixed control directory, immutable policy and operations journal identities, bounded read, append/fsync and content verification. `SupervisorControl::Impl` retains event replay, operation state, dispatch and recovery decisions. The persisted schema, limits and lifecycle behavior are unchanged. Registered the source in the POSIX DataSupervisor CMake target. A fresh complete native tree built the core, control/process test executables, service and production CLIs. Journal/control/lifecycle CTests passed 3/3 with local loopback, including journal-fault and restart/reopen cases. Platform and full-suite acceptance remain open. See [validation 71.734](VALIDATION.md#71734-datasupervisor-control-journal-storage-boundary-2026-11-04).

# 71.733 Complete native CMake profile and CTest evidence

The complete current-source CMake profile exposed a missing `artifactcommands.cpp` registration in the native `getdaq-config-check` target; added the source and rebuilt the full tree successfully. The profile registers 91 tests. The four-worker loopback-enabled run passed 90/91 in 19m 14s; the one failure used a fixed evidence directory left populated by an earlier run. After inspecting and removing only that isolated synthetic test output, the failed case passed 1/1. All 91 cases have individual passing evidence, but a clean single-invocation 91/91 result is still pending. The longest system/fault tests take several minutes. Qt Widgets, Windows/macOS and hardware acceptance were not part of this run. See [validation 71.733](VALIDATION.md#71733-full-native-cmake-build-and-91-case-regression-2026-10-27). The full Linux host-profile build separately completed 30 programs and all 109 dependency checks (71.732). Overall application/platform refactoring remains open.

# 71.732 Current-source native build and CTest evidence

A receiver-disabled native CMake build completed successfully with 85 registered cases. With local loopback enabled and four workers, 84 passed in the full invocation; the fixed-ECC evidence-directory case passed separately after removing the stale empty directory. The full 85-case run was not repeated after cleanup. The separate Linux full-profile build completed all 30 programs and verified 109 embedded dependency inputs. The complete profile's expanded 91-case results are recorded at 71.733. See [validation 71.732](VALIDATION.md#71732-clean-native-cmake-build-and-85-case-ctest-audit-2026-10-27).

# 71.731 Configuration artifact verification commands separated

Moved the `getdaq-config-check` configuration snapshot, publication, experiment, selection, registration, and readback-audit command handling into `apps/configcheck/artifactcommands.{h,cpp}`. `main.cpp` now coordinates the artifact and scan command families and retains shared usage output. Command forms, JSON output and diagnostics are preserved; `configcheck.pro` registers the new source. A clean Qt 5.12.8/C++11 production build passed. Positive invocations for all six artifact command families passed; the existing system-experiment CLI integration was extended to assert the `experiment` command's digests and hardware flag and passed 95 process cases. `--help` returned 0, no arguments 2 and a missing snapshot 1. Overall application/platform refactoring remains open. See [validation 71.731](VALIDATION.md#71731-configuration-artifact-verification-commands-separated-2026-10-27).

# 71.730 Envoy log persistence separated from presentation

Moved application/session log file selection, fallback paths, writes and connection-log close into `ConsoleLogStore`, leaving widget rendering, severity metadata and export in `MainWindow`. Production and aggregate test qmake registrations are both updated. The complete Qt test target and production Envoy rebuilt; focused log/severity regression passed 5/5, the app startup smoke created the expected log, and a blocked user-data path correctly fell back to the cache directory. Full Widgets and cross-platform verification remain open. See [validation 71.730](VALIDATION.md#71730-envoy-log-persistence-separated-from-presentation-2026-10-27).

# 71.729 CoBo LED plan generation separated

Moved LED plan constructors into `coboled.cpp`; `cobomonitoring.cpp` now owns the AsAd power/monitoring plan family. A private append helper lets power-cycle planning compose LED actions without coupling the two public plan families. Rebuilt the production hardware/system-config CLIs and relevant tests; two local mock-service CTests pass. See [validation 71.729](VALIDATION.md#71729-cobo-led-plan-generation-separated-2026-10-27).

# 71.728 Hardware description payload conversion separated

Moved construction of the Ice `NodeConfig` and matching CREATE audit JSON out of `HardwareClient::Create` into private `hardwareclient_description.cpp`; lifecycle transaction ordering and UNKNOWN handling remain in the client coordinator. Rebuilt the hardware core, both relevant mock-service test targets, and all four hardware production CLIs. The description and client CTests passed 2/2 using local mock services. See [validation 71.728](VALIDATION.md#71728-hardware-description-payload-conversion-separated-2026-10-27).

# 71.727 Configtool direct-source probe rebuilt

The strict qmake `tests/configtool_probe.pro` executable is now actually built after the 71.708 parser registration; its help path and seven offline register-compiler integration suites pass. A misleading-indentation warning in `XcfgSaveJournal::Load` was clarified by putting the loop body and following return on separate lines. Full suite and platform acceptance remain open. See [validation 71.727](VALIDATION.md#71727-configtool-direct-source-probe-rebuilt-2026-10-27).

# 71.726 Configuration repository core test is self-contained

The configuration repository's full core regression now generates its own temporary synthetic publication package, so `configuration-repository-core` no longer depends on a frozen artifact from an earlier phase. Strict C++11/pedantic build: 51 compile commands over 121 inputs; full core CTest: 1/1. Network/Qt service integration remains a separate gate. See [validation 71.726](VALIDATION.md#71726-self-contained-configuration-repository-core-ctest-2026-10-27).

# 71.725 Configuration repository CTest coverage

Registered the self-contained configuration-repository object-store smoke test as `configuration-repository-object-store`; the CMake project previously built its test executable without registering a CTest. A strict C++11/pedantic build passed 51 compiles/118 inputs, and the smoke passed 1/1 with cleanup. The larger immutable-publication/commit/recovery runner still requires a validated publication fixture and was not run here. See [validation 71.725](VALIDATION.md#71725-configuration-repository-object-store-ctest-registration-2026-10-27).

# 71.724 `getdaq-catalog` scan CLI extracted

Separated scan binding/summary and waveform analysis command handling into `catalogscancli.cpp`, and moved copy/resume, verification, and network transfer commands plus port/progress parsing into `catalogtransfercli.cpp`. `catalogmain.cpp` retains request/replacement orchestration and delegates those command families. The standalone strict C++11/pedantic build passed **154 compiles/264 inputs**, and the scan/catalog plus transfer filesystem tests passed **2/2** with localhost loopback enabled. The complete 151-test suite was not rerun; platform and hardware gates remain open. See [validation 71.724](VALIDATION.md#71724-getdaq-catalog-scan-and-transfer-cli-responsibility-split-2026-10-27).

# 71.723 Current-source Linux full host profile rebuilt

Rebuilt the current full Linux host profile after the DataReceiver inspector split and the optional ICE test registration. All **30 programs** built, 109 embedded dependency inputs were checked, and the receipt is complete. The current `getdaq-inspect` binary passed all nine synthetic CLI scenarios; generic MFM inspection and hit-pattern CSV/threshold behavior both remain present. The package regression removed its 280 synthetic bytes and the full build tree contains no `.graw` leftovers. The broader 151-test CTest set has not been rerun as one suite; the 71.718 full 149/149 result and this turn's six affected ICE tests plus DataReceiver 8/8 are the available evidence. Windows, macOS and hardware acceptance remain open.

# 71.722 ICE source-control tests registered and small-app boundaries audited

Audited the small ICE-related apps that previously had only limited review notes: `getdaq-ice-check` remains a compact read-only ping/status CLI; `ControlledIceSource` owns producer and stream lifecycle while `SourceControlServant` adapts the generated Ice RPC interface; `SourceRingReader`, `SourceRingReadout`, and `SourceMfmRing` retain distinct ring-consumption, worker lifecycle, and MFM framing responsibilities. No further source split was justified by the current boundaries.

Registered the source-control failure-commit test and the existing 19-case `run_ice_source_control_tests.py` receiver integration in the optional native ICE CTest configuration. Rebuilt the selected native targets and passed **6/6** focused checks (ICE check help, source-control lifecycle, source-fault commit, and three ring-reader tests). The integration receipt confirms synthetic payload cleanup; the isolated build tree has no leftover `.graw` files. The last complete native suite still represents 149 tests at milestone 71.718; this new 151-test registry has only had its affected tests rerun. Overall per-app/platform audit remains open.

# 71.721 DataReceiver inspection commands separated

Moved CoBo hit-pattern counting and threshold-scan CLI parsing/output from `apps/datareceiver/inspect.cpp` into `inspect_hitpattern.cpp`. The generic inspector retains streaming structural validation and its existing usage text; command forms, CSV bytes, diagnostics, and frame-counting behavior remain unchanged. Registered the new translation unit in the standalone receiver CMake target and registered the existing synthetic CLI regression as CTest `hitpattern-cli`.

The complete default standalone DataReceiver target set compiled with GCC 9.4/C++11. All eight default CTest cases passed with loopback permitted, including `cobo-hit-pattern`, `hitpattern-cli`, receiver core, exporter locking, and ByteStorage integration. The first run's two socket failures were sandbox permission errors; rerunning the same tests with loopback access passed 8/8. The CLI test automatically deleted its small synthetic frame files. Overall DataReceiver responsibility/platform audit and the complete host-software refactor remain open.

# 71.720 Windows Qt test finding and pending selection-publication verification

The Qt 5.14.2/MinGW 7.3 full control profile built nine x86_64 programs before the latest source edit. Its native Widgets run found one failure (168 passed, 1 failed, 11 skipped): publishing a selection receipt to an existing directory left a temporary file. Added a preflight check in `selectionfile.cpp` for existing targets and symlinks before the temporary file is created. Linux Qt aggregate rebuild and the focused test pass 3/3; Windows has not rebuilt this edit because WSL interop failed before qmake startup with `UtilBindVsockAnyPort: socket failed 1`. Do not mark the Windows gate as passed. macOS and hardware gates remain open.

# 71.719 Full current-source Qt Widgets suite 180/180

Rebuilt the aggregate Qt Widgets test target from `tests/tests.pro` with Qt 5.12.8/GCC 9.3 in `/tmp/getdaq-qt-718`. Ran it offscreen with an isolated HOME/XDG profile, localhost loopback, and the current full-profile `getdaq-inspect`, `getdaq-matrix` and `getdaq-merge` binaries supplied to the offline-data-tools integration. All **180 QtTest rows passed**, with **0 failures and 0 skips**, in 112.729 seconds. This includes waveform/Run gating, automatic Run policy and UI, per-connection operation logs, offline tools and close behavior, warnings, configuration/workspace UI, and simulator integration.

QtTest removed its temporary fixture root; a check found no remaining raw files there. No physical hardware or production DAQ data was used. Linux Qt 5.12.8 evidence does not establish Windows/macOS builds or hardware acceptance. See [validation 71.719](VALIDATION.md#71719-full-current-source-qt-widgets-suite-180180-2026-10-27).

# 71.718 Current-source native regression suite 149/149

Reconfigured and rebuilt the native CMake aggregation from the current source in a fresh `/tmp/getdaq-native-717-clean` tree with ICE receiver enabled and `GETDAQ_RECEIVER_EXECUTABLE` bound to the current full-profile receiver binary. The complete serial CTest run passed **149/149** in 5,537.69 seconds. This includes the previously failing retirement, handoff and plan-transition tests, plus hardware lifecycle, ECC network and remote receiver failure scenarios.

The isolated tree contained 71 generated synthetic `.graw` files totaling 1,777,669 bytes after the tests; these test artifacts were removed and a follow-up scan found zero `.graw` files. No production DAQ data or physical hardware was used. This is Linux host regression evidence only; Qt Widgets, Windows/macOS and target hardware verification remain separate gates. See [validation 71.718](VALIDATION.md#71718-current-source-native-regression-suite-149149-2026-10-27).

# 71.717 Full Linux host profile and supervisor qmake registration

The clean full Linux host profile exposed that `getdaq-supervisor-query.pro` omitted `controlprotocol_request_validation.cpp`, although the CMake protocol library already registered it. The omission left command parsing, operation-ID checks and request validation unresolved at link time. Registered the shared implementation in the qmake production target; no protocol behavior changed.

`python3 tools/build-suite.py --profile full --build-root /tmp/getdaq-host-full-716 --jobs 4` then completed successfully, building all 30 programs and checking their runtime dependencies. The receipt explicitly does not establish target-platform or hardware acceptance. This is a full host build result, not a full test-suite pass: the post-retirement 149-test rerun and per-application/platform audits remain outstanding. See [validation 71.717](VALIDATION.md#71717-full-linux-host-profile-and-supervisor-qmake-registration-2026-10-27).

# 71.712 Plan retirement status and full-suite failure audit

After running the post-71.707 native CTest suite, `data-plan-transition-staging` exposed that a normally completed retirement left `control_needs_reconciliation` set merely because the separate `planRetired` fence was latched. Normal retirement now clears the reconciliation status while retaining `planRetired`, which still rejects all new commands and replay for that instance. Recovery of a failed retirement remains distinct and retains its reconciliation hold. Rebuilt the full native tree; `failed-retirement-completion-service`, `supervisor-control-service`, `data-plan-transition-staging`, and `data-supervisor-process-lifecycle` pass 4/4.

The full 149-test run completed in 5,451.24 seconds with 143 passing and six failures. The five exclusive-create failures were caused by retained evidence paths in this reused build tree: `fixed-ecc-configuration-and-scan-deployment` and `native-ecc-receiver-{normal-0,normal-1,truncated,disconnect-ack-loss}` all passed when rerun with fresh evidence/audit paths. The remaining failure was the retirement-status regression fixed above. All six failed cases now have passing targeted evidence on the current source; a full 149-test rerun after the final distinction between normal retirement and repaired failure remains outstanding. Linux build, per-application and platform audits remain incomplete. Synthetic fixtures only; no physical hardware or production acquisition data was used. See [validation 71.712](VALIDATION.md#71712-plan-retirement-state-and-full-suite-audit-2026-10-27).

# 71.711 Simulator ECC control protocol separated

Moved the simulated ECC SOAP response builder and command parsing/state-transition handler from `apps/simulator/simulator.cpp` into `simulator_control.cpp`. Startup still owns local ECC/Sentry listeners, the simulated configuration workspace and output budget initialization; timed generated-data writing remains in `simulator_data.cpp`. Registered the control unit in the standalone Simulator and full Qt test qmake projects. See [validation 71.711](VALIDATION.md#71711-simulator-ecc-control-protocol-separated-2026-10-27). Overall application/platform refactoring remains open.

# 71.710 Sentry configuration loading separated

Moved `SentryConfig::Load` and `Validate` into `apps/sentry/sentryconfig.cpp`. `sentryservice.cpp` now owns service construction and listening startup. Registered the configuration unit in the standalone Sentry, Simulator and full Qt test qmake projects. The existing configuration schema and validation behavior are unchanged. See [validation 71.710](VALIDATION.md#71710-sentry-configuration-loading-separated-2026-10-27). Full application/platform refactoring remains open.

# 71.709 Receiver CLI parsing separated and MiB scaling corrected

Moved `getdaq-receiver` option decoding and usage output into `apps/datareceiver/receiver_options.{h,cpp}`; `main.cpp` now builds the argument vector, validates receiver identity, installs signals and owns service startup. Added direct parser tests and registered the new translation unit/test in the DataReceiver CMake project. Corrected `--max-frame-mib` and `--export-queue-mib` conversion in both native and ICE receiver parsers from GiB scaling to MiB scaling, matching documented units and existing ICE option assertions. See [validation 71.709](VALIDATION.md#71709-receiver-cli-parsing-separated-and-mib-scaling-corrected-2026-10-27). Full host-suite and platform refactoring remains open.

# 71.708 Register compiler CLI option parsing separated

Moved register compiler command-shape recognition and all compiler-specific option parsing into `apps/configtool/configtool_registercommands_options.{h,cpp}`. `configtool_registercommands.cpp` now handles usage display, XCFG loading and compiler dispatch using the parsed options. Existing CLI syntax and diagnostics are preserved. Registered the parser in both the production configtool qmake project and direct-source probe. Linux production build and AGET, MuTaNT, CoBo Configure/readout/clock/monitoring/calibration compiler integration suites pass. See [validation 71.708](VALIDATION.md#71708-register-compiler-cli-option-parsing-separated-2026-10-27). Overall application/platform refactoring remains open.

# 71.706 T2K ROOT event decoding separated

Moved `TT2K` / `T2KEvent` branch binding and matrix validation/conversion into `apps/rootviewer/t2keventadapter.{h,cpp}`. `RootTreeReader` now selects the file/tree format, delegates legacy ROOT5 frames to `Root5TreeAdapter`, T2K matrices to `T2KEventAdapter`, and retains current ROOT6 `GET::GDataFrame` decoding. Registered the adapter in the optional viewer and reader/statistics test targets. All three optional ROOT viewer read/statistics tests passed. See [validation 71.706](VALIDATION.md#71706-t2k-root-event-decoding-separated-2026-10-27). Overall application/platform refactoring remains open.

# 71.705 Transfer worker process isolated

Moved `TransferJobs::Impl::Work` into `apps/datasupervisor/transferjobs_worker.cpp` and placed the private `Impl` declaration in `transferjobs_p.h`. Transfer service construction, durable job state, submission and explicit recovery coordination remain in `transferjobs.cpp`; the new unit owns child process creation, progress observation, completion receipt verification and worker result publication. Registered the worker in the data-supervisor core. The native transfer-jobs service integration suite passed, including duplicate/restart no-replay and explicit verify/resume paths. Overall application/platform refactoring remains open.

# 71.704 Windows scan journal file I/O separated

Moved Win32 journal reads, secure temporary-file writes, and create/replace publication into `scanexecutionjournalstore_windows_io.cpp` with a private interface. The store implementation retains directory and lock checks, active-writer identity, transition validation and fault latching. Registered the unit in the ECC Windows CMake target and Envoy qmake sources. Linux ECC journal and state regression executables pass; this environment has no MinGW compiler, so the changed Windows translation unit still needs native Windows build and journal tests. Overall application/platform refactoring remains open.

# 71.703 Xcfg workspace edit history separated

Moved staged multi-source edits, detached dependency validation, bounded edit-history bookkeeping and undo/redo revalidation into `common/config/xcfgworkspace_edits.cpp`. The remaining workspace core coordinates source checks, root opening, document projection and preview creation; graph validation and Save All persistence stay in their dedicated units. Registered the source in shared and direct-test qmake projects. See [validation 71.703](VALIDATION.md#71703-xcfg-workspace-edit-history-separated-2026-10-27). Overall application/platform refactoring remains open.

# 71.702 Xcfg workspace graph building separated

Moved multi-root document graph loading, full declaration validation, effective-snapshot capture, shared-source consistency checks and source-budget enforcement into `common/config/xcfgworkspace_build.cpp`. Workspace open/edit/history/preview orchestration remains in `xcfgworkspace.cpp`; Save All persistence remains in `xcfgworkspace_save.cpp`. Registered the new source in shared and direct-test qmake projects. See [validation 71.702](VALIDATION.md#71702-xcfg-workspace-graph-building-separated-2026-10-27). Overall application/platform refactoring remains open.

# 71.701 Remote receiver mutation intent persistence separated

Moved remote intent, handoff-context and audit-proof file publication from `RemoteReceiverManager::Impl::Mutation` into `remotereceivermanager_mutationintent.cpp`. `Mutation` retains operation gates, parent/peer state and plan validation, source declarations and request construction; `ExecuteMutation` retains remote execution, audit progression and reconciliation. Registered the source in the native ECC CMake library. Request bytes, write order and handoff checks remain unchanged. See [validation 71.701](VALIDATION.md#71701-remote-receiver-mutation-intent-persistence-separated-2026-10-27). Overall application/platform refactoring remains open.

# 71.700 Workspace Save All transaction separated

Moved `XcfgWorkspace::SaveAll` into `common/config/xcfgworkspace_save.cpp`, isolating preview revalidation, prepared atomic writes, save-journal lifecycle, per-file synchronization, progress callbacks and incomplete-save recovery state from workspace loading, staging, history and preview generation. Registered the implementation in shared qmake and direct-source Xcfg tests; save ordering and recovery behavior are unchanged. See [validation 71.700](VALIDATION.md#71700-workspace-save-all-transaction-separated-2026-10-27). Overall application/platform refactoring remains open.

# 71.699 Supervisor control command dispatch separated

Extracted the accepted-operation command switch from `SupervisorControl::Impl::Run` into `controljobs_dispatch.cpp` and declared the private `Dispatch` method in `controljobs_p.h`. `Run` retains the operation lock, accepted-request identity recheck, running/completion journal records, reconciliation updates and failure handling. The moved command body matches its pre-extraction snapshot after normalizing indentation and the extraction-boundary newline; no request schema or command policy changed. See [validation 71.699](VALIDATION.md#71699-supervisor-control-command-dispatch-separated-2026-10-27). Overall application/platform refactoring remains open.

# 71.698 RunControl monitoring and Sentry observation callbacks separated

Moved node freshness/state predicates, status publication, legacy polling and Sentry response/finalization acknowledgements from `runcontrol.cpp` into `runcontrol_observation.cpp`. The connection/bootstrap and shutdown unit now retains controller lifecycle and journal writing; command sequencing and run finalization remain in their existing dedicated units. Registered the observer in shared Envoy qmake. See [validation 71.698](VALIDATION.md#71698-runcontrol-monitoring-and-sentry-observation-callbacks-separated-2026-10-27). Overall application/platform refactoring remains open.

# 71.697 ECC client store responsibilities separated

Split `EccClientStore` implementation state, durable replay/state checks, public queries, retirement APIs and write/observation mutations into dedicated source units registered in the production and direct-source qmake projects. The store's existing journal and policy behavior was moved without intended semantic changes. Linux strict C++11 builds and ECC client tests passed; the updated Windows Qt 5.14.2/MinGW build and native test runner passed. See [validation 71.697](VALIDATION.md#71697-ecc-client-store-responsibilities-separated-2026-10-27). The complete application/platform audit remains open.

# 71.696 Windows scan journal filesystem support split and native validation

Moved Windows handle ownership, user SID/ACL checks, path normalization, file identity/regular-file validation and complete writes into the private `scanexecutionjournalstore_windows_support.{cpp,_p.h}` adapter. `scanexecutionjournalstore_windows.cpp` retains journal state, lock ownership, identity rechecks and atomic publication. Registered the support unit in the Windows CMake and Envoy qmake targets; no journal schema or safety rule changed. A strict Qt 5.14.2/MinGW build plus native Windows Widgets/ECC-client tests and the dedicated journal safety regression passed. See [validation 71.696](VALIDATION.md#71696-windows-scan-journal-filesystem-support-split-and-native-validation). The ECC and full application/platform audits remain open.

# 71.695 Remote receiver mutation execution and cold evidence separated

Moved the sequential remote-command execution, per-node observation/result verification, audit progression and failure/reconciliation handling from `RemoteReceiverManager::Impl::Mutation` into `remotereceivermanager_mutationexecution.cpp`. The original unit retains authorization gates, state refresh, plan/context validation and durable intent/handoff-context construction. Also moved cold parent-history verification and cold completion-receipt construction into `coldhandoff_evidence.cpp`; the CLI coordinator retains fixed-input checking, operation gates and persistence/publication. Registered both sources and CTest coverage. Remote POST order, UNKNOWN retention, no-replay/no-automatic-cleanup behavior, completion-request identity and handoff fencing remain validated. See [validation 71.695](VALIDATION.md#71695-remote-receiver-mutation-execution-and-cold-evidence-separated-2026-10-27). Overall refactoring and application/platform coverage remain open.

# 71.694 ECC client recovery policy separated

Moved catalog-replacement resume eligibility, original retirement-context validation and failed data-handoff completion checks from `EccClientStore::Impl` into `eccclientstore_recoverypolicy.{h,cpp}`. The store retains its durable journal, fixed-policy and filesystem identity responsibilities, delegating only the recovery decisions. Registered the unit in the shared production Envoy core and both direct-source ECC client qmake targets. Added direct eligibility boundary coverage. Qt 5.12.8 rebuilt production `GetDAQ`, `ecc-system-client-tests` and the input-fault probe; all 70 ECC client QtTest results passed, including retirement persistence/reopen and the new catalog-resume policy test. The full `getdaq-tests` target rebuilt and its native catalog-recovery policy check passed. No physical hardware or production acquisition data was used. Overall refactoring and application/platform coverage remain open.

# 71.693 Host application coverage audit and config-transfer build repair

Added a per-application coverage baseline in `docs/REFACTOR_COVERAGE.md`. The audit confirmed that `configtransfer`'s standalone qmake project omitted the already-split `configurationrepositoryclient_upload.cpp`; registered that source so `getdaq-config-upload` links again. The small `icecheck` CLI and the separate `icecontrol`/`sourcereader` modules were reviewed; no artificial source split was warranted. See [validation 71.693](VALIDATION.md#71693-host-application-coverage-audit-and-config-transfer-build-repair). All application coverage remains open until each application and platform gate is reviewed.

# 71.692 Supervisor control-request validation separated

Moved command names/classification, request bounds, operation-ID checks and full `SupervisorControlRequest` semantic validation into `apps/datasupervisor/controlprotocol_request_validation.cpp`. `controlprotocol.cpp` now owns request JSON decoding/encoding and request identity derivation. Registered the unit in the supervisor protocol target; command schemas, limits and refusal behavior are unchanged. See [validation 71.692](VALIDATION.md#71692-supervisor-control-request-validation-separated-2026-10-27). The per-application audit is tracked in [refactor coverage](REFACTOR_COVERAGE.md); no application is marked complete. Overall refactoring continues.

# 71.691 ICE data-sender failure recovery separated

Moved ICE source observation, failure-stop and failed-source retirement APIs plus retirement-receipt validation into `apps/hardware/datasenderclient_ice.cpp`. `datasenderclient.cpp` now holds proxy setup, endpoint identity and ordinary sender lifecycle methods. Registered the new unit in the hardware core. Source/session binding, audit ordering, UNKNOWN retention and no-retry rules are unchanged. See [validation 71.691](VALIDATION.md#71691-ice-data-sender-failure-recovery-separated-2026-10-27). Overall refactoring continues.

# 71.690 Transfer job startup validation separated

Moved transfer-agent option validation into `apps/datasupervisor/transferjobs_options.cpp`, separating fixed-root and executable binding checks from transfer job lifecycle and worker orchestration. The validation rules and error behavior are unchanged. Registered the unit in the data-supervisor core target. See [validation 71.690](VALIDATION.md#71690-transfer-job-startup-validation-separated-2026-10-27). Overall refactoring continues.

# 71.689 ICE receiver receipt publication separated

Moved `IceIngressRecorder::SaveReceipt` into `apps/iceingress/iceingress_receipt.cpp` and shared its JSON quoting/raw-file-name helpers through private `iceingress_helpers_p.h`; `iceingress.cpp` now keeps recorder lifecycle, bounded input and writer/status behavior. Registered the receipt unit in the optional ICE core library and updated strict-build/archive compile expectations. Receipt schema, file identity checks, fsync/rename sequence and completion gates are unchanged. See [validation 71.689](VALIDATION.md#71689-ice-receiver-receipt-publication-separated-2026-10-27). Overall refactoring continues.

# 71.688 ICE ingress startup option parsing separated

Moved `getdaq-ice-receiver` argument decoding, bound checks, managed identity validation, listener/storage compatibility checks, and output-directory normalization into `apps/iceingress/iceingress_options.{h,cpp}`. `main.cpp` now consumes a validated options value before constructing the recorder or initializing Ice. Added standalone options coverage to CTest and updated the strict ICE build receipt to compile/run the test. Receiver protocol and shutdown logic are unchanged. See [validation 71.688](VALIDATION.md#71688-ice-ingress-startup-option-parsing-separated-2026-10-27). Overall refactoring continues.

# 71.687 Configuration checker scan commands separated

Moved `getdaq-config-check` scan-plan, execution-policy and execution-snapshot command handling plus bounded, non-link file access into `apps/configcheck/scancommands.{h,cpp}`. The general configuration/readback CLI remains in `main.cpp`. Registered the source in both the standalone qmake application and CMake target. CLI syntax, JSON output and filesystem refusal rules are unchanged. See [validation 71.687](VALIDATION.md#71687-configuration-checker-scan-commands-separated-2026-10-26). Overall refactoring continues.

# 71.686 Simulator generated-data writer separated

Moved `Simulator::WriteData` into `apps/simulator/simulator_data.cpp`, isolating timed synthetic output generation and its hard byte-budget enforcement from simulator setup and HTTP lifecycle handling. Registered the source in the standalone simulator and Qt test projects; output markers, write-failure behavior and generation budget are unchanged. See [validation 71.686](VALIDATION.md#71686-simulator-generated-data-writer-separated-2026-10-26). Overall refactoring continues.

# 71.685 Configuration repository upload-plan validation separated

Moved upload-plan decoding, object-reference validation, publication-file assembly and complete publication verification into `apps/configrepository/configurationrepository_plan.cpp`, using a private prepared-commit value passed to the durable version publisher. The repository store retains object reads, fault latching, locking and no-overwrite version publication. Upload schema, identity checks, size budgets and persistence order are unchanged. Registered the new source in the configuration-repository CMake target. See [validation 71.685](VALIDATION.md#71685-configuration-repository-upload-plan-validation-separated-2026-10-26). Overall refactoring continues.

# 71.684 CoBo lifecycle transition policy separated

Moved lifecycle state naming, initial-state selection and the complete allowed-transition matrix into `apps/hardware/cobolifecycle_policy.cpp`. `cobolifecycle.cpp` now focuses on the fixed-topology ledger, durable journal integration and synchronized state updates. Transition policy and UNKNOWN-on-persistence-failure behavior are unchanged. Registered the new unit in the hardware CMake target. See [validation 71.684](VALIDATION.md#71684-cobo-lifecycle-transition-policy-separated-2026-10-26). Overall refactoring continues.

# 71.683 Scan plan review decision recording separated

Moved `RecordScanPlanReview` into `apps/envoy/scanplanreview_record.cpp`, separating the reviewed-decision journal creation/binding/transition path from recovery inspection, runtime context loading and unknown-operation reconciliation. The public API and immutable plan/snapshot identity checks remain unchanged. Registered the unit in production Envoy and Qt test qmake projects. See [validation 71.683](VALIDATION.md#71683-scan-plan-review-decision-recording-separated-2026-10-26). Overall refactoring continues.

# 71.682 Receiver catalog copy/resume orchestration separated

Moved receiver-catalog transfer orchestration into `apps/datasupervisor/catalogtransfer_copy.cpp`. `catalogtransfer.cpp` now owns shared POSIX catalog-transfer filesystem validation and resume-prefix checking, while the new unit coordinates copy/resume setup, payload streaming, manifest verification and completion receipt publication. The transfer wire metadata, source-preservation rules, no-overwrite behavior and resume-prefix checks are unchanged. See [validation 71.682](VALIDATION.md#71682-receiver-catalog-copyresume-orchestration-separated-2026-10-26). Overall refactoring continues.

# 71.681 ECC client operation transport separated

Moved control-fault latching, scan-lifecycle observation state, durable intent submission, historical operation observation, and response processing into `apps/envoy/eccsystemclient_operations.cpp`. `eccsystemclient.cpp` now focuses on fixed endpoint/credential setup and identity/status/alarm queries. Registered the new unit in Envoy and standalone ECC client/probe qmake targets. See [validation 71.681](VALIDATION.md#71681-ecc-client-operation-transport-separated-2026-10-26). Overall refactoring continues.

# 71.680 Configuration range parser separated

Moved scalar and range text parsing into `common/config/configurationrange_parse.cpp`, with private comparison declarations in `configurationrangeinternal_p.h`. `configurationrange.cpp` now owns value normalization, range comparison and canonical encoding. Updated CMake, shared qmake fragments, and direct-source production/test builders so all consumers link both units. See [validation 71.680](VALIDATION.md#71680-configuration-range-parser-separated-2026-10-26). Overall refactoring continues.

# 71.679 Scan journal reconciliation separated

Moved unknown-operation quarantine, observed lifecycle completion, and operator reconciliation transitions into `common/experiment/scanexecutionjournal_reconciliation.cpp`. Regular intent/completion transitions remain in `scanexecutionjournal_operations.cpp`; event fields, transition checks and reconciliation limits are unchanged. Registered the new unit in the experiment-core CMake and qmake projects. See [validation 71.679](VALIDATION.md#71679-scan-journal-reconciliation-separated-2026-10-26). Overall refactoring continues.

# 71.678 Envoy responsive layout separated

Moved `MainWindow::resizeEvent` and `MainWindow::ArrangeConsole` into `apps/envoy/mainwindow_layout.cpp`, isolating responsive sizing, acquisition-button reflow, and parameter/status/plot height allocation from console widget initialization and live status presentation. Registered the unit in the production Envoy and GUI test qmake projects. See [validation 71.678](VALIDATION.md#71678-envoy-responsive-layout-separated-2026-10-26). Overall refactoring continues.

# 71.677 ECC request semantic validation separated

Moved `ValidateEccControlRequest` from the request JSON codec into `common/control/eccprotocol_request_validation.cpp`, registering the unit in both CMake and qmake production source lists. Request schemas and validation rules are unchanged. See [validation 71.677](VALIDATION.md#71677-ecc-request-semantic-validation-separated-2026-10-26). Overall refactoring continues.

# 71.676 Structural edit persistence separated

Moved reviewed structural-edit persistence into `common/config/xcfgstructureeditor_apply.cpp`. The Apply path still regenerates and compares the preview, rechecks document and include/import source bytes, writes through `QSaveFile`, reloads the document, and verifies the effective snapshot before returning success. Registered the unit in the shared production Xcfg qmake fragment and the standalone shared tests. See [validation 71.676](VALIDATION.md#71676-structural-edit-persistence-separated-2026-10-26). Overall refactoring continues.

# 71.675 CompoundConfig binary codec separated

Moved the legacy CompoundConfig version-1 binary Reader/Writer and scalar/range wire conversion into `common/config/xcfgbinary_codec.cpp` behind `xcfgbinary_codec_p.h`. `XcfgBinary` retains XML inspection, round-trip semantic checks, import/export previews and stale-review-safe publication. Registered the codec in the shared production Xcfg qmake fragment and the standalone shared test project. See [validation 71.675](VALIDATION.md#71675-compoundconfig-binary-codec-separated-2026-10-26). Overall refactoring continues.

# 71.674 Fixed ECC input file binding separated

Moved secure input-file opening, bounded content snapshots, reference SHA checks, read-only enforcement, aggregate input budgeting and canonical input binding SHA generation into `apps/ecc/native/fixedeccconfiguration_inputs.cpp` behind `fixedeccconfiguration_inputs_p.h`. `FixedEccConfiguration` retains service and hardware policy decoding and delegates file identity/content rechecks to the private input-set component. POSIX no-follow opens, owner/mode/link/inode checks and fault latching remain enforced. Registered the new source in `getdaq-ecc-native`. See [validation 71.674](VALIDATION.md#71674-fixed-ecc-input-file-binding-separated-2026-10-26). Overall refactoring continues.

# 71.673 POSIX scan journal file reading separated

Moved bounded no-follow journal reads and descriptor/path identity rechecks into `scanexecutionjournalstore_read.cpp`, behind a private store interface. The store implementation retains directory/lock ownership, writer state, creation and successor publication policy; read size, type, owner, mode, link count, complete-read, and before/after identity checks remain enforced. Registered the unit in POSIX CMake and Envoy qmake builds. See [validation 71.673](VALIDATION.md#71673-posix-scan-journal-file-reading-separated-2026-10-26). Full refactoring continues.

# 71.672 FITS event serialization and CFITSIO support separated

Moved staged event-channel FITS generation into `fits_writer_event.cpp` and shared CFITSIO file/error/keyword helpers into `fits_writer_support.{cpp,p.h}`. `fits_writer.cpp` now coordinates conversion lifecycle and writes the grouping/index table. The index row schema, channel HDUs, grouping references, limits, checksums, and publish-after-finish contract are unchanged. Registered the new implementation units in both optional FITS production and test targets. See [validation 71.672](VALIDATION.md#71672-fits-event-serialization-and-cfitsio-support-separated-2026-10-26). Full refactoring continues.

# 71.671 Native Run Control scan event baseline flow separated

Moved `QueryScanEventCountBaseline` and `FinishScanEventCountBaselineQuery` from `nativeruncontrol_scanidentity.cpp` into `apps/envoy/nativeruncontrol_scanbaseline.cpp`. Runtime ECC identity validation/binding remains in the identity unit; the baseline unit retains the Ready-state and receiver-session checks, event-count query, and journal binding. Registered the new production source through Envoy's shared `core.pri`. See [validation 71.671](VALIDATION.md#71671-native-run-control-scan-event-baseline-flow-separated-2026-10-26). Full refactoring continues.

# 71.670 Xcfg scalar XML text codec separated

Moved legacy XML scalar text decoding and scalar-value escaping into `common/config/xcfgscalartext.cpp`. `XcfgDocument` file loading/editing now calls the shared private decoder, and effective export plus scalar editing share the same encoder. Registered the production unit in the common Xcfg qmake fragment, standalone shared tests, and current-configuration probe; whitespace folding, entity/CDATA handling, split-scalar read-only behavior, and edit token preservation are unchanged. See [validation 71.670](VALIDATION.md#71670-xcfg-scalar-xml-text-codec-separated-2026-10-26). Full refactoring continues.

# 71.669 Envoy Run command handlers separated

Moved the Envoy window's Connect/Disconnect, system progress/regress, Run start/stop, finalization/recovery, failed-catalog replacement resume, and automatic-run settings handlers into `apps/envoy/mainwindow_runcommands.cpp`. The main window continues to own state and UI; the new unit groups command actions and their gating in one production source file. Registered it in the Envoy and Qt test qmake projects without changing signals or policy. See [validation 71.669](VALIDATION.md#71669-envoy-run-command-handlers-separated-2026-10-26). Full refactoring continues.

# 71.668 ECC service request routing separated

Split the production ECC HTTP request handler into a small authenticated method/content-type dispatcher plus dedicated GET and POST endpoint units (`eccservice_get.cpp` and `eccservice_post.cpp`). Shared error encoding and receiver-source path decoding now live behind the service-private interface. Registered both endpoint units in the production CMake target. Endpoint paths, status codes, validation, async observation/submission handling and shutdown response timing are preserved.

See [validation 71.668](VALIDATION.md#71668-ecc-service-request-routing-separated-2026-10-26). Full refactoring continues.

# 71.667 Xcfg source migration marker and completion lifecycle separated

Moved recovery marker installation/ownership checks, completion-record validation, observed-state revalidation and final marker retirement into `common/config/xcfgsourcemigration_store_state.cpp`. These operations now share strict record JSON validation and durable file helpers through the private migration interface; record creation, path migration execution and recovery direction policy remain separate. Registered the new production unit in the shared Xcfg qmake fragment and standalone shared tests. Marker contents, completion schema and write/cleanup order are unchanged. See [validation 71.667](VALIDATION.md#71667-xcfg-source-migration-marker-and-completion-lifecycle-separated-2026-10-26). Full refactoring continues.

# 71.666 Xcfg source migration recovery-record decoding separated

Moved path-migration recovery manifest decoding and integrity validation into `common/config/xcfgsourcemigration_store_decode.cpp`. It validates strict JSON structure, root/move mappings, canonical source paths and stored original/candidate payload hashes; record creation, state observation, recovery policy and durable source publication remain in the store and recovery units. Registered the decoder in the shared Xcfg qmake fragment and standalone shared-test project. No recovery ordering or on-disk schema changes. See [validation 71.666](VALIDATION.md#71666-xcfg-source-migration-recovery-record-decoding-separated-2026-10-26). Full refactoring continues.

# 71.665 Waveform viewer 3D surface renderer separated

Moved `WaveformCanvas::ProjectSurfacePoint` and `WaveformCanvas::DrawSurface` into `apps/waveformviewer/waveformcanvas_surface.cpp`, with the new source registered through the shared `waveformwindow.pri` used by the standalone viewer, Envoy and Qt tests. The canvas unit retains 2D plotting, trace state and input handling. Rendering and interaction behavior are intended to remain unchanged. See [validation 71.665](VALIDATION.md#71665-waveform-viewer-3d-surface-renderer-separated-2026-10-26). Full refactoring continues.

## 71.664 Sentry catalog replacement finalization separated

Moved `SentryFiles::FinalizeReplacement` out of the legacy catalog/Run archival unit into `apps/sentry/sentryservice_replacement.cpp`. The catalog unit retains incoming Run reservation, sealed-file checks and journaled movement; replacement finalization retains its operation identity, resumable file-preservation and cleanup checks. Registered the new production source in Sentry, Simulator and Qt test qmake projects. See [validation 71.664](VALIDATION.md#71664-sentry-catalog-replacement-finalization-separated-2026-10-26). Full refactoring continues.

## 71.663 DataReceiver matrix codec separated

Moved CoBo type-1 frame validation/sample-order decoding, legacy binary64 little-endian `.gmat` encoding and codec geometry limits into `matrix_codec.{cpp,p.h}`. `matrix.cpp` now coordinates selected-frame streaming, output-byte policy and staged publication. Registered the codec in both the `getdaq-matrix` and `matrix-tests` CMake targets; MFM selection, sample-arrival ordering, bounds and file publication remain unchanged. See [validation 71.663](VALIDATION.md#71663-datareceiver-matrix-codec-separated-2026-10-26). Full refactoring continues.

## 71.662 ROOT Viewer window responsibilities separated

Moved the private `RootViewerWindow` declaration to `rootviewerwindow_p.h`; the existing factory and constructor remain in `rootviewerwindow.cpp`, file/event navigation is in `rootviewerwindow_navigation.cpp`, and statistics worker/progress/results are in `rootviewerwindow_statistics.cpp`. Registered the two new production units in the optional ROOT Viewer CMake target. File selection, T2K restrictions, event navigation, scan cancellation and display behavior are unchanged. See [validation 71.662](VALIDATION.md#71662-root-viewer-window-responsibilities-separated-2026-10-26). Full refactoring continues.

## 71.661 ROOT event plot rendering separated

Moved shared event-plot axes, waveform rendering and heatmap rendering from `rootcanvas.cpp` into `rootcanvas_eventplots.cpp`; Canvas state, input handling, event layout, histogram drawing and 3D surface drawing stay in their focused units. Registered the renderer in the optional ROOT Viewer CMake target. The public canvas API and pixels/plot data behavior are unchanged. See [validation 71.661](VALIDATION.md#71661-root-event-plot-rendering-separated-2026-10-26). Full refactoring continues.

## 71.660 ConfigWindow export workflows separated

Split the XML export review and editable workspace-copy review from `configwindow_export.cpp` into `configwindow_xml_export.cpp` and `configwindow_workspacecopy.cpp`, each with its own Qt dependencies. Registered both production and test qmake targets. The dialogs' preview, stale-revision checks, read-only restrictions, background work and publication behavior remain unchanged. Production Envoy and the complete Qt test executable rebuilt; all four focused export/workspace-copy regression tests passed (6/6 QtTest results including setup and cleanup) with the required local Qt test cache access. No hardware or acquisition data was used. Full refactoring continues.

## 71.659 Supervisor query response validation separated

Moved HTTP response decoding, plan-scope checks and operation/handoff/retirement response validation from the Supervisor query CLI coordinator into `queryrunner_validation.{cpp,p.h}`. The CLI retains argument handling and request sequencing; the exact response schemas, identity bindings, proof rules and one-shot POST behavior remain unchanged. The failed-handoff recovery gate now accepts an empty current `operation_id` when the service reports no active operation, while rejecting a different non-empty operation ID. Registered the validator in Linux/Windows CMake and qmake query targets. Full refactoring continues.

## 71.658 ConfigWindow tree projection separated

Moved configuration-tree rebuild, recursive source-node projection and selection/editability presentation from `configwindow.cpp` into `configwindow_tree.cpp`. Loading and edit orchestration, history, export, snapshots and structure editing remain in their dedicated units. The production Envoy and Qt test qmake projects both register the new unit. See [validation 71.658](VALIDATION.md#71658-configwindow-tree-projection-separated). Full refactoring continues.

## 71.657 Scan analysis path checks and report publication shared

Threshold and waveform scan analysis now share path-boundary validation and exclusive durable report publication in `receivercatalog_scanreport.cpp`. The common publisher preserves existing-output refusal, `.pending` creation, exclusive hard-link publication and parent-directory sync; each analysis keeps its report schema and analysis-specific messages. See [validation 71.657](VALIDATION.md#71657-scan-analysis-path-checks-and-report-publication-shared). Full refactoring continues.

## 71.656 Bound scan catalog raw-file verification shared by analysis modes

Extracted the common bound-catalog source selection, no-follow raw-file opening, inode/size checks before and after reading, and catalog request/manifest/receipt stability checks into `receivercatalog_scansource.cpp`. Threshold hit-pattern analysis and AutoGain/Pulser/PeakingTime waveform analysis now consume the same verified raw-file boundary. The later shared scan-point loader in `receivercatalog_scanpoint.cpp` also consolidates point binding, catalog selection and target-CoBo artifact validation. See [validation 71.656](VALIDATION.md#71656-bound-scan-catalog-raw-file-verification-shared-by-analysis-modes) and [71.657](VALIDATION.md#71657-scan-analysis-path-checks-and-report-publication-shared). Full refactoring continues.

## 71.655 RunRecorder persistence test separation

Moved Run reservation, explicit replacement, native reference publication, durable completion and interrupted-recovery test bodies into `tests/runrecorder_tests.cpp`, retaining their original QtTest slots as delegates. The RunRecorder test unit is excluded from all six standalone window probes. See [validation 71.655](VALIDATION.md#71655-runrecorder-persistence-test-separation). The full refactoring is not complete; current test-suite verification is recorded below.

## 71.654 Sentry service and catalog test separation

Moved Sentry catalog replacement, cleanup recovery, configuration validation and HTTP service cases into `tests/sentry_service_tests.cpp`, preserving each QtTest slot as a delegate. Local-only fixture coverage is unchanged. See [validation 71.654](VALIDATION.md#71654-sentry-service-and-catalog-test-separation). Full refactoring continues.

## 71.653 SOAP protocol and payload test separation

Moved legacy SOAP request/response encoding and parsing tests into `tests/soap_protocol_tests.cpp`, preserving the original QtTest slot names as delegates. The optional gSOAP process interop test remains guarded by `GETDAQ_GSOAP_INTEROP`. See [validation 71.653](VALIDATION.md#71653-soap-protocol-and-payload-test-separation). Full refactoring continues.

## 71.652 Operation log and console control test separation

Moved connection-session logging, visible-log export and event-severity tests into `tests/operation_log_tests.cpp`; manual Run-number/layout and elapsed-time tests join `consolelayout_tests.cpp`. Existing QtTest slots remain delegates. See [validation 71.652](VALIDATION.md#71652-operation-log-and-console-control-test-separation). Full refactoring continues.

## 71.651 Native status projection and cache test separation

Moved the three Native receiver/status projection and cache tests into `tests/native_status_tests.cpp`, leaving the metadata and single-CoBo interaction tests in `native_gui_tests.cpp`. Existing QtTest slots remain as delegates, and all six standalone probes exclude the new test-only source. See [validation 71.651](VALIDATION.md#71651-native-status-projection-and-cache-test-separation). Full refactoring continues.

## 71.650 Scan-plan output validation test separation

Moved `scanPlanOutputInspectionRejectsMalformedPlans` from `tests/test_getdaq.cpp` into `tests/scanplan_output_tests.cpp`, preserving the QtTest slot as a delegate. The separate module keeps coverage for manifest policy, point artifact integrity, path safety and Unix symlink rejection. See [validation 71.650](VALIDATION.md#71650-scan-plan-output-validation-test-separation). Full refactoring continues.

## 71.649 Native GUI and whole-system control test separation

Moved `nativeGuiMetadataAndWholeSystemControls` from `tests/test_getdaq.cpp` into `tests/native_gui_tests.cpp`, preserving the existing QtTest slot as a delegate. The large GUI regression now has its own focused test unit, and the six standalone window probes exclude that test-only source. See [validation 71.649](VALIDATION.md#71649-native-gui-and-whole-system-control-test-separation). Full refactoring continues.

## 71.648 Offline scan-plan and native scan integration test separation

Moved the offline scan-plan builder GUI test and the complete Native scan Run-to-catalog integration test out of `tests/test_getdaq.cpp` into dedicated test units; the original QtTest slots remain as delegates. `test_getdaq.cpp` decreased by 772 lines. Both extracted tests passed against the current `getdaq-config` and `getdaq-config-check` CLI builds; six standalone window probes exclude the new test-only objects. See [validation 71.648](VALIDATION.md#71648-offline-scan-plan-and-native-scan-integration-test-separation). Full refactoring continues.

## 71.647 Legacy Run Control integration test separation

Moved 11 local HTTP/Run Control integration test bodies into `tests/legacy_runcontrol_tests.cpp`; original QtTest slot names remain as delegates, and fixture/helper access now uses the shared `MockNode` and test-support units. The six standalone window probes now exclude all 11 test-only source objects, including the waveform-viewer test/fixture pair that had remained in their generated makefiles. See [validation 71.647](VALIDATION.md#71647-legacy-run-control-integration-test-separation). Full refactoring continues.

## 71.646 Data file tools close path made explicit

Idle close now schedules explicit deferred destruction from `OfflineDataToolsWindow::closeEvent`, so the title-bar close control, in-window Close button, and Escape all release the parent `QPointer` and Run/menu gates without relying on `WA_DeleteOnClose`. Closing while a child process runs still hides the window, requests termination, and retains the gate until process exit. Focused close/reopen and child-process tests passed. See [validation 71.646](VALIDATION.md#71646-data-file-tools-close-path-made-explicit). Full refactoring continues.

## 71.645 Configuration compiler test suite separation

Moved 23 Mutant, CoBo configuration, AGET and hardware-description compiler test bodies into `tests/configuration_compiler_tests.cpp`; original QtTest slots remain as delegates. Registered the unit in the full qmake test target and excluded it from all six standalone window probes. See [validation 71.645](VALIDATION.md#71645-configuration-compiler-test-suite-separation). Full refactoring continues.

## 71.644 Legacy HTTP/SOAP test server fixture separation

Moved the reusable local `MockNode` HTTP/SOAP server from `tests/test_getdaq.cpp` into `tests/mocknode_fixture.{p.h,cpp}`. Its operation handling, mutable test controls, response bodies and trace recording are unchanged; the full Qt test target registers the fixture, while standalone window probes exclude it. See [validation 71.644](VALIDATION.md#71644-legacy-httpsoap-test-server-fixture-separation). Full refactoring continues.

## 71.643 Native recovery and control policy test separation

Moved seven in-memory Native recovery, finalization, maintenance and plan-retirement policy test bodies into `tests/native_recovery_policy_tests.cpp`; existing QtTest slot names remain as delegating entry points and the snapshot test reuses the shared Run Control fixture. Registered the unit in the full qmake test target and excluded it from standalone window probes. See [validation 71.643](VALIDATION.md#71643-native-recovery-and-control-policy-test-separation). Full refactoring continues.

## 71.642 Offline data-tool and integrated viewer test separation

Moved the Data file tools close/reopen, child-process shutdown, multi-window Run locking, and integrated waveform-viewer lifecycle tests into `tests/offline_tools_tests.cpp`. Existing QtTest slots remain as delegating entry points, and the tests continue to share the synthetic MFM fixtures and Run Control helpers. Registered the new unit in the main test target and excluded test-only sources from standalone window probes. See [validation 71.642](VALIDATION.md#71642-offline-data-tool-and-integrated-viewer-test-separation). Full refactoring continues.

## 71.641 Console status and responsive layout test separation

Moved the configured-CoBo overview, reference/live layout, compact-width, status-row sizing, and node-count reflow tests into `tests/consolelayout_tests.cpp`; the existing QtTest slot names remain delegating entry points. The tests reuse the shared Run Control fixtures from 71.640. Registered the new unit in the main qmake test target and excluded test-only modules from standalone window probes. See [validation 71.641](VALIDATION.md#71641-console-status-and-responsive-layout-test-separation). Full refactoring continues.

## 71.640 Envoy automatic-run test suite separation

Moved the nine automatic-run policy, simulation, lifecycle and UI tests from the large `tests/test_getdaq.cpp` body into `tests/autorun_tests.cpp`; their QtTest slot names remain as delegating entry points. Extracted `Config`, `At`, `Phase` and `MakeFiles` into `getdaq_test_support.{p.h,cpp}` and retained thin wrappers for the remaining suite. The main Qt test project registers both new units, while standalone window probes exclude test-only sources. See [validation 71.640](VALIDATION.md#71640-envoy-automatic-run-test-suite-separation). Full refactoring continues.

## 71.639 ECC handoff recovery CLI separation

Moved shared receipt validation, explicit completion of a failed original handoff, and read-only cold-handoff evidence review from `apps/ecc/handoff/main.cpp` into `handoffrecoverycommands.{p.h,cpp}`. The original submit/inspect flow uses the shared validator; command behavior and evidence gates remain unchanged. Registered the module in `getdaq-ecc-handoff` and extended the network runner with a CLI-backed partial-completion case. See [validation 71.639](VALIDATION.md#71639-ecc-handoff-recovery-cli-separation). Full refactoring continues.

## 71.638 Waveform viewer test suite separation and qmake integration

Moved the eight waveform-viewer test bodies into `tests/waveformviewer_tests.cpp` and their synthetic MFM builders into `tests/waveformviewer_testfixtures.cpp`; the original QtTest slot names remain as delegating entry points, and the shared fixtures remain available to offline data-tool tests. The full Qt test link exposed that the POSIX scan-journal publisher from 71.637 was not registered in Envoy's shared qmake source list, so `core.pri` now includes it for both production and Qt tests. See [validation 71.638](VALIDATION.md#71638-waveform-viewer-test-suite-separation-and-qmake-integration). Full refactoring continues.

## 71.637 POSIX scan journal atomic publication separation

Moved private temporary-file writing, file identity checks, no-overwrite creation, atomic replacement, and directory `fsync` into `apps/ecc/scanexecutionjournalstore_publish.cpp`. `ScanExecutionJournalStore` retains lifecycle successor validation, directory/lock ownership, and active journal identity checks; Windows storage and journal semantics are unchanged. Registered the publisher in the POSIX CMake library and the shared Envoy qmake source list. See [validation 71.637](VALIDATION.md#71637-posix-scan-journal-atomic-publication-separation). Full refactoring continues.

## 71.636 ECC journal replay and durable storage separation

Extracted the bounded journal file scan and record/state-machine replay into `apps/ecc/eccjournal_replay.cpp`, returning a recovered state snapshot to the storage owner. `eccjournal.cpp` retains file creation, locks, inode/size validation, append, `fsync`, and lifecycle writes. The replay rules and journal format are unchanged. Registered the new unit in the POSIX CMake library and the native ECC direct-source sanitizer builder. See [validation 71.636](VALIDATION.md#71636-ecc-journal-replay-and-durable-storage-separation). Full refactoring continues.

## 71.635 Waveform raw indexing and sample decoding separation

Split `GrawFrameReader` into `grawreader_index.cpp` for bounded MFM/nested-frame indexing and `grawreader_waveform.cpp` for selected-channel sample loading and type-1/type-2 decoding. Shared frame and memory limits live in the private `grawreader_internal.h`; the public reader API and record schema are unchanged. Registered both modules through the shared waveform qmake fragment used by Envoy, the standalone viewer and Qt tests. See [validation 71.635](VALIDATION.md#71635-waveform-raw-indexing-and-sample-decoding-separation). Full refactoring continues.

## 71.634 Receiver seal verification and publication separation

Moved the receiver's final raw-file enumeration, byte-count and sender-EOF completeness checks, identity manifest construction, and durable atomic seal publication into `receiver_seal.cpp`. Socket lifecycle and status reporting remain in `receiver.cpp`; the seal conditions and file format are unchanged. See [validation 71.634](VALIDATION.md#71634-receiver-seal-verification-and-publication-separation). Full refactoring continues.

## 71.633 Receiver storage writer separation

Extracted the receiver writer coordinator to `receiver_write.cpp`, with raw ByteStorage and parsed MFM FrameStorage behavior in `receiver_write_bytestorage.cpp` and `receiver_write_framestorage.cpp`. The two backends retain their distinct flush, disk-reserve, and error-preservation rules; the receive socket thread and status surface remain in `receiver.cpp`. Registered all units in the receiver target. See [validation 71.633](VALIDATION.md#71633-receiver-storage-writer-separation). Full refactoring continues.

## 71.632 Supervisor query entry point separation

Moved the shared query command implementation and response validation out of `querymain.cpp` into `queryrunner.cpp`, with the narrow `RunQuery()` declaration in `queryrunner.h`. The entry point now only launches the runner and, on Windows, converts validated UTF-16 arguments to UTF-8. Registered the runner in both platform CMake paths and the qmake project. See [validation 71.632](VALIDATION.md#71632-supervisor-query-entry-point-separation). Full refactoring continues.

## 71.631 FITS MFM event decoding separation

Moved type-1 and compact type-2 CoBo frame validation and channel/sample extraction from the FITS conversion coordinator into `apps/datareceiver/fits_decode.cpp`, exposing only the private `FitsInternal::DecodeEvent()` operation. The conversion coordinator retains stream selection, size policy and staged publication; the writer independently owns CFITSIO output. Both optional FITS targets register the decoder. See [validation 71.631](VALIDATION.md#71631-fits-mfm-event-decoding-separation). Full refactoring continues.

## 71.630 FITS event writing and index management separation

Moved CFITSIO event tables, grouping-index row serialization and checksum finalization into the private `FitsInternal::Writer` component in `apps/datareceiver/fits_writer.cpp` and `fits_writer_p.h`. `fits.cpp` now owns MFM streaming and the staged directory's size checks, sync and atomic no-replace publication; it only calls the writer at each decoded event and before publication. Both optional FITS executable and test targets register the component. See [validation 71.630](VALIDATION.md#71630-fits-event-writing-and-index-management-separation). Full refactoring continues.

## 71.629 ROOT viewer UI and application bootstrap separation

Moved the offline `RootViewerWindow` and its asynchronous statistics worker out of `apps/rootviewer/main.cpp` into `rootviewerwindow.cpp`, with a narrow factory declaration in `rootviewerwindow.h`. The entry point retains ROOT thread-safety setup, QApplication creation and the synthetic surface smoke mode; it owns the returned window with `std::unique_ptr`, preserving the prior stack lifetime. Registered the UI unit in the optional ROOT viewer target. See [validation 71.629](VALIDATION.md#71629-root-viewer-ui-and-application-bootstrap-separation). Full refactoring continues.

## 71.628 ROOT viewer 3D surface rendering separation

Moved `EventCanvas::ProjectSurfacePoint()` and `DrawSurface()` into `apps/rootviewer/rootcanvas_surface.cpp`, isolating the 3D bucket/channel/ADC surface projection and painter path from histogram, waveform and heatmap rendering. The canvas API and rendering behavior are unchanged; registered the new source in the optional ROOT viewer target. See [validation 71.628](VALIDATION.md#71628-root-viewer-3d-surface-rendering-separation). Full refactoring continues.

## 71.627 Scan-plan subprocess result dispatch separation

Moved Catalog binding completion and immutable stopping-policy creation/check completion into `apps/envoy/scanplanwindow_processoperations.cpp`. `ProcessFinished()` now performs the common button refresh/output read and dispatches by the active operation, while normal plan-output validation remains in `scanplanwindow_process.cpp`. Catalog UNKNOWN quarantine, plan revalidation, policy checker requirements and no-hardware guarantees are preserved. Registered the new unit in both production and test qmake projects. See [validation 71.627](VALIDATION.md#71627-scan-plan-subprocess-result-dispatch-separation). Full refactoring continues.

## 71.626 Scan-plan subprocess failure and close lifecycle separation

Moved `ScanPlanWindow::ProcessError()` and `closeEvent()` into `apps/envoy/scanplanwindow_processlifecycle.cpp`. Scan-plan output processing remains in `scanplanwindow_process.cpp`; failed-to-start recovery gates, persisted binding UNKNOWN handling, and the rule that the dialog stays open while a subprocess is active are unchanged. Registered the new unit in both production and test qmake projects. See [validation 71.626](VALIDATION.md#71626-scan-plan-subprocess-failure-and-close-lifecycle-separation). Full refactoring continues.

## 71.625 Native scan pending-lifecycle observation separation

Moved `NativeRunControl::ObservePendingScanLifecycleOperation()` into `apps/envoy/nativeruncontrol_scanrecovery.cpp`, isolating read-only inspection of the original persisted Describe/Prepare/Configure operation from scan start and lifecycle submission. Its plan/journal/request/identity/state binding and no-resend behavior are unchanged; the shared qmake source list registers the new unit for both Envoy and Qt tests. See [validation 71.625](VALIDATION.md#71625-native-scan-pending-lifecycle-observation-separation). Full refactoring continues.

## 71.624 Scan-plan policy-state refresh separation

Moved `ScanPlanWindow::UpdatePolicyControls()` into `apps/envoy/scanplanwindow_policystate.cpp`, separating runtime/journal-derived button gating and current-point presentation from immutable stopping-policy creation. The refresh logic, verification checks, lifecycle labels and enablement conditions are unchanged; both production and test qmake projects register the new unit. See [validation 71.624](VALIDATION.md#71624-scan-plan-policy-state-refresh-separation). Full refactoring continues.

## 71.623 SystemPreparation selected-output maintenance separation

Moved selected-CoBo LED set/blink/flip/pulse and AsAd pulser operations into `apps/hardware/systempreparation_cobooutputs.cpp`. `systempreparation_maintenance.cpp` now focuses on whole-board reprepare and selected-board Configure. Original Prepared/ownership gates, per-command audit intents, preflight checkpoints, UNKNOWN latching and no-replay behavior are unchanged. Registered the output-maintenance unit in the hardware CMake target. See [validation 71.623](VALIDATION.md#71623-systempreparation-selected-output-maintenance-separation). Full refactoring continues.

## 71.622 Native ECC read-only observations separation

Moved controller status, ownership/configuration readback JSON, alarm/source observations, and budgeted hardware-state observation into `apps/ecc/native/nativeecccoordinator_observations.cpp`. The coordinator implementation now focuses on initialization and command submission; observations retain the same lock/epoch consistency checks, RPC budget, bounded errors, and explicit “not hardware readback/configuration proof” fields. Registered the new source in the Native ECC CMake target. See [validation 71.622](VALIDATION.md#71622-native-ecc-read-only-observations-separation). Full refactoring continues.

## 71.621 SystemPreparation Unprepare release separation

Moved `UnprepareAfterFailure`, its final read-only verification and audit-chain accessors, plus normal `Unprepare` and its retained audit accessor, into `apps/hardware/systempreparation_unprepare.cpp`. `systempreparation_recovery.cpp` now contains production-stop-after-failure and configuration invalidation. Registered the new unit in the hardware CMake target; release command order, owned-alarm checks, per-CoBo lifecycle transitions, UNKNOWN handling and no-replay behavior are preserved. See [validation 71.621](VALIDATION.md#71621-systempreparation-unprepare-release-separation). Full refactoring continues.

## 71.620 Native ECC failure cleanup separation

Moved `UnprepareAfterFailure`, `BreakupAfterFailure`, and `UnprepareAcquisitionAfterFailure` into `apps/ecc/native/nativeecccoordinator_failurecleanup.cpp`; recovery retains production-stop and source/receiver-retirement orchestration. Registered the new translation unit in the Native ECC CMake target. The final read-only acquisition-unprepare check also binds its confirmed `Fresh` lifecycle append to the retained preparation audit, accepting it only when the old prefix and file identity still match. UNKNOWN state, original-operation binding, no-replay gates, and hardware/data lifecycle boundaries remain intact. See [validation 71.620](VALIDATION.md#71620-native-ecc-failure-cleanup-separation). Full refactoring continues.

## 71.619 Data file tools 关闭完成与标题栏关闭按钮

显式启用独立工具窗口的标题栏关闭按钮。运行离线子进程时关闭窗口仍先隐藏并异步终止进程；进程结束后显式延迟销毁已隐藏窗口，不再依赖再次触发其关闭事件。主窗口在对象销毁前继续保持 Run 互锁。见[验证71.619](VALIDATION.md#71619-data-file-tools-关闭完成与标题栏关闭按钮)。完整重构继续。

## 71.618 ECC journal JSON codec separation

Moved bounded UTF-8 and operation-ID validation, canonical string quoting, strict field-set/type/integer decoding, and journal state decoding into `apps/ecc/eccjournal_codec.cpp`, declared by the private `eccjournal_p.h`. `eccjournal.cpp` retains POSIX locking, append/fsync, replay and lifecycle transitions; record schema, bounds and no-replay behavior are unchanged. Registered the codec in the POSIX ECC journal CMake target. See [validation 71.618](VALIDATION.md#71618-ecc-journal-json-codec-separation). Full refactoring continues.

## 71.617 Xcfg source migration durable record and file transaction separation

Moved bounded migration record storage, integrity/identity checks, marker lifecycle, atomic file publication and recovery execution into `xcfgsourcemigration_store.cpp`. `xcfgsourcemigration.cpp` now only revalidates the reviewed workspace and delegates the already-bound preview to the storage transaction. The recovery API continues to share the same record validation, source observation, lock, marker and no-replay operations through the private migration interface. Registered the unit in shared and direct-source qmake projects. See [validation 71.617](VALIDATION.md#71617-xcfg-source-migration-durable-record-and-file-transaction-separation). Full refactoring continues.

## 71.616 SystemAlignment binding, preflight and command execution separation

Moved Ice proxy/interface binding and no-write hardware preflight into `systemalignment_binding.cpp`, and moved audit-backed link, slot-location, alignment and hardware-command operations into `systemalignment_operations.cpp`. The private `systemalignment_p.h` holds the shared single-use client state and narrow helper declarations; `systemalignment.cpp` retains options validation and end-to-end orchestration. RPC order, persist-before-command audit records, UNKNOWN handling, and no-retry/no-restoration behavior are unchanged. Registered both units in the hardware CMake target. See [validation 71.616](VALIDATION.md#71616-systemalignment-binding-preflight-and-command-execution-separation). Full refactoring continues.

## 71.615 Xcfg source-path migration recovery workflow separation

Moved migration-record classification and the reviewed recovery preview/apply workflow into `xcfgsourcemigration_recovery.cpp`. The private migration interface now shares the validated record model and narrowly scoped load, observe, lock, marker, state-check and execution operations; recovery still rechecks semantic equivalence, binds the exact manifest and observed disk state, and refuses completed records. Registered the new unit in the shared qmake fragment and direct-source Xcfg test project. See [validation 71.615](VALIDATION.md#71615-xcfg-source-path-migration-recovery-workflow-separation). Full refactoring continues.

## 71.614 System-description option validation separation

Moved topology-to-description option construction, policy/endpoint validation and required hardware-field checks from `systemdescription.cpp` into `systemdescription_options.cpp`. The private `systemdescription_p.h` shares only role/policy naming and fixed inventory, stopped-state and bit-check helpers with the remote coordinator. Describe/verification command order, audit records and hardware gates are unchanged. See [validation 71.614](VALIDATION.md#71614-system-description-option-validation-separation). Full refactoring continues.

## 71.613 Config-tool register compiler command separation

Moved specialized register/hardware-description compiler command dispatch, argument validation and its usage text from `main.cpp` into `configtool_registercommands.cpp`, with the private interface in `configtool_registercommands_p.h`. `main.cpp` retains application bootstrap and routes the existing CLI unchanged; production and direct-source test qmake projects register the new unit. See [validation 71.613](VALIDATION.md#71613-config-tool-register-compiler-command-separation). Full refactoring continues.

## 71.612 Scan-point deployment preparation and sealed bundle publication separation

Separated frozen-point artifact/receiver-plan preparation in `scanpointdeployment.cpp` from sealed directory publication in `scanpointdeployment_publish.cpp`. Shared `PreparedPoint` and receiver-template state plus the scan-derived audit-root helper are declared in the private deployment header. The full input set is still validated before creating the bundle directory; sealed file modes, canonical bytes, identity checks and manifest contents are unchanged. See [validation 71.612](VALIDATION.md#71612-scan-point-deployment-preparation-and-sealed-bundle-publication-separation). Full refactoring continues.

## 71.611 Scan-plan CLI dispatch and implementation separation

Reduced `scanplancommands.cpp` to command dispatch. Moved policy/snapshot commands into `scanplancommands_metadata.cpp`, offline peaking/pulser/threshold/AutoGain plan generation into `scanplancommands_generation.cpp`, and shared bounded file, directory and decimal validation into `scanplancommands_support.cpp`, declared by the private `scanplancommands_p.h`. Registered the units in `configtool.pro`; command syntax, output schemas and no-overwrite behavior are unchanged. See [validation 71.611](VALIDATION.md#71611-scan-plan-cli-dispatch-and-implementation-separation). Full refactoring continues.

## 71.610 Receiver plan transition staging and predecessor retirement separation

Moved predecessor retirement-proof validation and activation into `receiverplantransition_predecessor.cpp`, while successor-plan staging remains in `receiverplantransition.cpp`. Shared stable-file identity, bounded metadata sealing, absent-target checks and successor validation now live in the private `receiverplantransition_support.cpp` unit. Registered the units in the supervisor target and direct-source sanitizer build; plan schemas, identity checks, retirement ordering and activation behavior are unchanged. See [validation 71.610](VALIDATION.md#71610-receiver-plan-transition-staging-and-predecessor-retirement-separation). Full refactoring continues.

## 71.609 XcfgWorkspace structural editing and cross-file move separation

Moved structural edit review/staging into `xcfgworkspace_structure.cpp` and cross-file move review/staging into `xcfgworkspace_move.cpp`. The workspace core retains document loading, staged source/history management and Save All; behavior and review checks are unchanged. Registered both units in the shared qmake fragment and the direct-source shared configuration test project. See [validation 71.609](VALIDATION.md#71609-xcfgworkspace-structural-editing-and-cross-file-move-separation). Full refactoring continues.

## 71.608 Offline AGET configuration compiler stage separation

Separated single-chip AGET, whole-CoBo AGET, readout and Configure compilation into `agetcompiler.cpp`, `agetcompiler_cobo.cpp`, `agetcompiler_readout.cpp` and `agetcompiler_configure.cpp`. Shared experiment reading and bounded new-file JSON publication are declared in the private `agetcompiler_p.h`; output schemas, defaults, validation order and hardware-deployment exclusions are unchanged. Registered all units in the shared configuration qmake fragment. See [validation 71.608](VALIDATION.md#71608-offline-aget-configuration-compiler-stage-separation). Full refactoring continues.

## 71.607 Waveform viewer window responsibility separation

Moved the offline viewer's file/index lifecycle, channel/navigation controls, and frame presentation/export methods out of the monolithic `waveformwindow.cpp` into `waveformwindow_file.cpp`, `waveformwindow_controls.cpp` and `waveformwindow_presentation.cpp`. The private `waveformwindow_p.h` holds only the shared window state and method declarations; file parsing, navigation behavior, rendering, metrics and export semantics are unchanged. Registered all units in the shared waveform viewer qmake fragment. See [validation 71.607](VALIDATION.md#71607-waveform-viewer-window-responsibility-separation). Full refactoring continues.

## 71.606 ECC service process shutdown separation

Moved credential validation, bounded authenticated HTTP shutdown exchange, exact response binding and managed-child exit wait into `apps/ecc/native/eccserviceprocesssupervisor_shutdown.cpp`. Startup/scan-point start remain in `eccserviceprocesssupervisor.cpp`; the new private header shares only process state and helper declarations. Shutdown intent/evidence order, no-kill timeout behavior and fixed identity checks are unchanged. Registered the new source in the Native ECC target. See [validation 71.606](VALIDATION.md#71606-ecc-service-process-shutdown-separation). Full refactoring continues.

## 71.605 Scan runtime fresh identity observation separation

Moved the fresh pre-Describe ECC identity/status and receiver observation checks into `common/control/scanruntimeidentity_initial.cpp`. `scanruntimeidentity.cpp` now contains the later-state revalidation paths; both share the same private canonical identity observation helper. Validation order, frozen deployment bindings, node-set checks and canonical observation bytes remain unchanged. Registered the new unit in CMake and qmake; repaired the ECC system-client qmake test source list to include its existing extracted client-store storage implementation. See [validation 71.605](VALIDATION.md#71605-scan-runtime-fresh-identity-observation-separation). Full refactoring continues.

## 71.604 Remote receiver cold recovery separation and catalog recovery gate repair

Moved cold handoff reconstruction and cold catalog-replacement continuation into `apps/ecc/native/remotereceivermanager_coldrecovery.cpp`, leaving live handoff completion and receipt adoption in `remotereceivermanager_recovery.cpp`. While validating the split, fixed the supervisor's catalog recovery gate: only an eligible failed publish/finalize with the exact sealed session, Run identity and replacement request can enable continuation; a successful continuation clears the reconciliation latch, and recovery is unavailable under an independent hold. See [validation 71.604](VALIDATION.md#71604-remote-receiver-cold-recovery-separation-and-catalog-recovery-gate-repair). Full refactoring continues.

## 71.603 Catalog transfer wire protocol separation

Moved bounded stream reads/writes, integer and tag encoding, progress-aware acknowledgements, remote error replies and source metadata contract decoding into `apps/datasupervisor/catalogtransfer_protocol.cpp`. Copy/resume orchestration and publication remain separate from the shared sender/receiver wire helpers. Frame tags, field bounds, error handling and transfer checkpoint contents are unchanged. Full refactoring continues.

## 71.602 XcfgDocument IO and query separation

Moved XML source loading/include traversal, scalar token decoding/editing, source revalidation and safe value writes into `common/config/xcfgdocument_io.cpp`. `xcfgdocument.cpp` now contains path lookup, child indexing and enumeration. Registered the new unit in the shared qmake fragment and both direct-source configuration probes. Source bounds, include semantics, transaction locking, original-byte preservation and save/reload checks are unchanged. Full refactoring continues.

## 71.601 Bound scan waveform metric extraction

Moved layered MFM traversal, CoBo sample-layout decoding and waveform metric accumulation into the private `receivercatalog_scanwaveformmetrics.{p.h,cpp}` unit. Bound scan analysis retains point/binding validation, secure catalog file reads and identity rechecks, catalog immutability checks and exclusive report publication. Supported type-1 and compact type-2 handling, frame bounds, selected-channel rules, metric thresholds and report values are unchanged. Full refactoring continues.

## 71.600 Catalog replacement directory-sync helper separation

Moved the replacement transaction's `SyncDirectory` implementation from `receivercatalogreplacement_p.h` into the existing `receivercatalogreplacementhelpers.cpp` unit. The private header now exposes only its declaration. Descriptor-bound identity checks before/after `fsync` and the durable catalog replacement ordering are unchanged. See [validation 71.600](VALIDATION.md#71600-catalog-replacement-directory-sync-helper-separation). Full refactoring continues.

## 71.599 Remote handoff evidence filesystem support separation

Moved bounded directory enumeration, parent-path extraction, strict JSONL record scanning and evidence-file open/identity/content checks from `remotehandoffevidence_p.h` into `remotehandoffevidence_support.cpp`. The private header now holds only the `EvidenceFile` state and declarations, reduced from 75 to 34 lines. Directory entry/record/size budgets, no-follow opens, owner/mode/link-count checks and inode/content revalidation are unchanged. Registered the support unit in `getdaq-ecc-native`. See [validation 71.599](VALIDATION.md#71599-remote-handoff-evidence-filesystem-support-separation). Full refactoring continues.

## 71.598 HardwareClient device description and helper separation

Moved hardware-name validation, 64-bit GET/Ice conversion, register masks and command-exception text extraction from `hardwareclient_p.h` into `hardwareclient_support.cpp`. Moved communicator destruction, remote device lookup, device-description decoding/policy comparison and register/field write-width checks into `hardwareclient_devices.cpp`. The private header now retains shared client state and the proxy-options template, reduced from 140 to 57 lines. Endpoint encoding/timeout, description binding, access checks and wire-value semantics are unchanged. Registered both units in `getdaq-hardware-core`. See [validation 71.598](VALIDATION.md#71598-hardwareclient-device-description-and-helper-separation). Full refactoring continues.

## 71.597 ECC control-job validation and alarm observation separation

Moved sealed-receiver checks, fixed-configuration startup validation, ECC command mapping and request validation from `ecccontroljobs_p.h` into `ecccontroljobs_validation.cpp`. Moved alarm observation JSON serialization into `ecccontroljobs_observation.cpp` and moved worker shutdown/join behavior out of the private header into `ecccontroljobs.cpp`. The header now keeps shared job state and declarations, reduced from 148 to 100 lines. Command acceptance rules, successor binding, alarm evidence schema and worker join order are unchanged. Registered the validation unit in `getdaq-ecc-native`. See [validation 71.597](VALIDATION.md#71597-ecc-control-job-validation-and-alarm-observation-separation). Full refactoring continues.

## 71.596 RemoteReceiverManager state and lifecycle validation separation

Moved manager construction, reconciliation latching, command gates, parent-storage integrity checks and status publication from `remotereceivermanager_p.h` into `remotereceivermanager_state.cpp`. Moved command deadlines, remote identity checks, sender/plan matching, request construction and Ready/Stopped cycle validation into `remotereceivermanager_validation.cpp`. Strict JSON record checks, HTTP response validation and bound-file verification now live in `remotereceivermanager_support.cpp`; audit-proof revalidation is also out of line. The private header now retains shared state and declarations, reduced from 252 to 127 lines. Ownership binding, no-retry behavior, request schemas, receiver identity/session checks and verified-seal requirements are unchanged. Registered all three units in `getdaq-ecc-native`. See [validation 71.596](VALIDATION.md#71596-remotereceivermanager-state-and-lifecycle-validation-separation). Full refactoring continues.

## 71.595 SystemPreparation runtime state and preflight separation

Moved shared audit/lifecycle methods (`Record`, `Check`, `Execute`, `Fail` and CoBo lifecycle updates) from `systempreparation_p.h` into `systempreparation_runtime.cpp`. Moved canonical node-identity checks, hardware preflight, Configure revalidation and release preflight into `systempreparation_preflight.cpp`; bounded text, hardware-step and node-inventory helpers now have one implementation in `systempreparation_support.cpp`. The private header retains shared state and declarations, reduced from 239 to 68 lines. Prepare/configure ordering, ownership and endpoint checks, readback capture, release gates and lifecycle persistence are unchanged. Registered all three units in `getdaq-hardware-core`. See [validation 71.595](VALIDATION.md#71595-systempreparation-runtime-state-and-preflight-separation). Full refactoring continues.

## 71.594 SystemAcquisition internal execution and journal separation

Moved all 19 inline `SystemAcquisitionClient::Impl` method bodies out of `systemacquisition_p.h`. Lifecycle/audit state updates now live in `systemacquisition_journal.cpp`; hardware client construction, preflight, node start/stop, verification and quiesce logic live in `systemacquisition_nodes.cpp`. The private header retains shared state and method declarations, reduced from 271 to 60 lines. Stop ordering, UNKNOWN handling, lifecycle checkpoints and audit records are unchanged. Registered both units in `getdaq-hardware-core`. See [validation 71.594](VALIDATION.md#71594-systemacquisition-internal-execution-and-journal-separation). Full refactoring continues.

## 71.593 DataSenderClient remote validation and transition separation

Moved remote Ice identity/session validation and its UNKNOWN-state handling into `datasenderclient_remotevalidation.cpp`, and moved sender state transitions, one-shot RPC ordering and command audit handling into `datasenderclient_transition.cpp`. Shared implementation state, the Ice parser and failure-retirement verification now live in the private `datasenderclient_p.h` interface. Source observation, explicit failure retirement and public operation entry points remain in `datasenderclient.cpp`; validation and lifecycle semantics are unchanged. Registered the new units and private header in `getdaq-hardware-core`. See [validation 71.593](VALIDATION.md#71593-datasenderclient-remote-validation-and-transition-separation). Full refactoring continues.

## 71.592 GetDAQ control MCP server implementation separation

Split the MCP server from the command-line bootstrap into JSON-RPC protocol handling, authenticated HTTP transport/request submission, read-only tool handlers and control-tool handlers. Registered the implementation units in both CMake and qmake; tool names, schemas, confirmation checks, request bindings and transport behavior are unchanged. See [validation 71.592](VALIDATION.md#71592-getdaq-control-mcp-server-implementation-separation). Full refactoring continues.

## 71.591 Native ECC coordinator preflight and initialization separation

Moved coordinator lifecycle/hardware preflight into `nativeecccoordinator_validation.cpp`, and moved coordinator construction plus fixed-target ownership and operation-path integrity checks into `nativeecccoordinator.cpp`. Existing configured-plan binding, direct TCP ownership reservation, immutable artifact checks and lifecycle gates are unchanged. See [validation 71.591](VALIDATION.md#71591-native-ecc-coordinator-preflight-and-initialization-separation). Full refactoring continues.

## 71.590 Native ECC recovery state verification separation

Moved `NativeEccCoordinator::Impl::VerifyRecovery` into `nativeecccoordinator_recovery.cpp`, isolating original-failure identity checks, retained cleanup/source proofs and final read-only verification from shared coordinator state. Recovery target, operation binding, proof rechecks and no-cold-adoption behavior are unchanged. See [validation 71.590](VALIDATION.md#71590-native-ecc-recovery-state-verification-separation). Full refactoring continues.

## 71.589 Native ECC hardware command application separation

Moved `NativeEccCoordinator::Impl::Apply` into `nativeecccoordinator_apply.cpp`, separating operation intent/audit binding, hardware lifecycle and maintenance actions, receiver start/finish handling, stopped-alarm verification and failure evidence capture from shared coordinator state. Mutation order, command-specific checks and the state-machine commit boundary are unchanged. Registered the unit in `getdaq-ecc-native`. See [validation 71.589](VALIDATION.md#71589-native-ecc-hardware-command-application-separation). Full refactoring continues.

## 71.588 Native ECC recovery proof validation separation

Moved the retained source-retirement, preparation-cleanup, production-stop, failure-connection, receiver-abort/retirement and acquisition-Unprepare proof checks from `nativeecccoordinator_p.h` into `nativeecccoordinator_recoveryproofs.cpp`. Proof presence, immutable-file checks, dependency order and storage-fault latching are unchanged. Registered the unit in `getdaq-ecc-native`. See [validation 71.588](VALIDATION.md#71588-native-ecc-recovery-proof-validation-separation). Full refactoring continues.

## 71.587 ECC network observation projection separation

Moved controller status JSON and CoBo lifecycle projection methods from `ecccontroljobs_p.h` into the existing `ecccontroljobs_observation.cpp`. State fields, operation ordering and lifecycle projection remain unchanged; the shared private header now retains declarations only. See [validation 71.587](VALIDATION.md#71587-ecc-network-observation-projection-separation). Full refactoring continues.

## 71.586 ECC network journal integrity and append separation

Moved fixed-input/control-store validation and durable ECC network journal append from the shared private header into `ecccontroljobs_journal.cpp`; the low-level write/fsync helper is local to that unit. Existing identity/content checks, fault latching, input-fault allowances, sequence budgeting and append/check ordering are unchanged. See [validation 71.586](VALIDATION.md#71586-ecc-network-journal-integrity-and-append-separation). Full refactoring continues.

## 71.585 ECC network recovery binding checks separation

Moved exact handoff-completion binding, alarm-recovery binding and known stopped-alarm release validation from `ecccontroljobs_p.h` into `ecccontroljobs_recoverychecks.cpp`. Existing original-operation, client/instance, hold-state, controller-state and request-proof checks are unchanged. Registered the new source in `getdaq-ecc-native`. See [validation 71.585](VALIDATION.md#71585-ecc-network-recovery-binding-checks-separation). Full refactoring continues.

## 71.584 ECC network request acceptance separation

Moved `EccControlJobs::Impl::AcceptLocked` into `ecccontroljobs_acceptance.cpp`, isolating request admission and intent publication from the service entry points, worker loops, journal replay and command execution. Existing identity/idempotency checks, recovery and maintenance gates, durable accepted-event ordering, fault handling and worker launch behavior are unchanged. Registered the source in `getdaq-ecc-native`. See [validation 71.584](VALIDATION.md#71584-ecc-network-request-acceptance-separation). Full refactoring continues.

## 71.583 ECC network journal recovery separation

Moved `EccControlJobs::Impl` construction and persisted network-journal validation/replay from `ecccontroljobs_p.h` into `ecccontroljobs_journal.cpp`, leaving the constructor declaration and shared state in the private header. Policy binding, sequence/scope checks, operation and hold recovery, and unfinished-intent UNKNOWN handling are unchanged. Registered the unit in `getdaq-ecc-native`. See [validation 71.583](VALIDATION.md#71583-ecc-network-journal-recovery-separation). Full refactoring continues.

## 71.582 ECC network command execution separation

Moved `EccControlJobs::Impl::Run` from the shared private header into `ecccontroljobs_run.cpp`, leaving only its declaration in `ecccontroljobs_p.h`. The command execution transaction, reconciliation gates, one-shot hardware calls, audit sequencing and failure/UNKNOWN handling are unchanged. Registered the unit in `getdaq-ecc-native`. See [validation 71.582](VALIDATION.md#71582-ecc-network-command-execution-separation). Full refactoring continues.

## 71.581 Catalog transfer verification separation

Moved copied-catalog manifest/evidence binding, directory-tree and payload verification plus streaming SHA-256 calculation from `catalogtransfer.cpp` into `catalogtransfer_verify.cpp`. Copy/resume staging, source leases, prefix checking and publication ordering remain in the transfer unit; verification requirements and completion receipts are unchanged. Registered the unit in `getdaq-data-supervisor-core`. See [validation 71.581](VALIDATION.md#71581-catalog-transfer-verification-separation). Full refactoring continues.

## 71.580 DataSupervisor status protocol separation

Moved supervisor lifecycle status and receiver process-observation JSON encoding/decoding from `controlprotocol.cpp` into `controlprotocol_status.cpp`. Request command parsing, canonical request validation, mutation classification and operation-key generation remain in the request protocol unit. Schemas, field sets, numeric bounds, receiver identity checks and proof gates are unchanged. Registered the new source in `getdaq-supervisor-protocol`. See [validation 71.580](VALIDATION.md#71580-datasupervisor-status-protocol-separation). Full refactoring continues.

## 71.579 Data sender local validation separation

Moved bounded text/receiver/protocol option checks and whole-sender-set uniqueness validation from `datasenderclient.cpp` into `datasenderclient_validation.cpp`, with the small shared private interface in `datasenderclient_validation_p.h`. Runtime Ice validation, lifecycle transitions, command auditing, source observation and failed-source recovery remain in the client implementation. Canonical receiver and endpoint identity rules are unchanged. Registered the new unit in `getdaq-hardware-core`. See [validation 71.579](VALIDATION.md#71579-data-sender-local-validation-separation). Full refactoring continues.

## 71.578 Native Run Control observation and status coordination separation

Moved status polling, ECC status decoding, receiver/source/failure/alarm observation handling, status projection, post-status lifecycle continuation and the stale-status refresh gate from `nativeruncontrol.cpp` into `nativeruncontrol_observation.cpp`. Intent submission/operation reconciliation, controller state, failure journaling and final presentation publication remain in their existing units. Freshness gates, event polling priority, stale-response discard, scan confirmation and status fields are unchanged. Registered through shared Envoy `core.pri`. See [validation 71.578](VALIDATION.md#71578-native-run-control-observation-and-status-coordination-separation). Full refactoring continues.

## 71.577 DataSupervisor receiver lifecycle and seal verification separation

Moved managed receiver identity checks, output-lock binding, readiness/status validation, executable/argv selection and final receiver seal/file-set verification from `datasupervisor_impl.cpp` into `datasupervisor_receiver.cpp`. Supervisor journal/store state, deployment/catalog recovery checks and overall status publication remain in the implementation unit. Child identity, lifecycle gates, completion receipts and file-set acceptance are unchanged. Registered the unit in `getdaq-data-supervisor-core`. See [validation 71.577](VALIDATION.md#71577-datasupervisor-receiver-lifecycle-and-seal-verification-separation). Full refactoring continues.

## 71.576 Xcfg source migration preview separation

Moved reviewed source-graph construction, effective-semantics comparison, include rewrite planning and preview generation from `xcfgsourcemigration.cpp` into `xcfgsourcemigration_preview.cpp`. Shared path canonicalization, source budgets, path ordering, hashing and invariant checks now live in private `xcfgsourcemigration_p.h`. Durable transaction creation, publication, recovery records and restore/finish operations remain in the transaction unit. Existing snapshot equality, stale-review rejection and recovery behavior are unchanged. Registered the source and private header in production and test qmake projects. See [validation 71.576](VALIDATION.md#71576-xcfg-source-migration-preview-separation). Full refactoring continues.

## 71.575 Receiver status endpoint separation

Moved receiver status JSON projection and the bounded HTTP `/status` handler from `receiver.cpp` into `receiver_status.cpp`. Shared JSON string escaping is now a private inline helper used by both status output and receiver seal generation. TCP receive/write workers, cycle-control handling, Run orchestration, and seal publication remain in the receiver unit; endpoint fields, request limits, deadlines, response semantics, and seal bytes retain their behavior. Registered the source in the data-receiver CMake target. See [validation 71.575](VALIDATION.md#71575-receiver-status-endpoint-separation). Full refactoring continues.

## 71.574 Offline data tools window close lifecycle verification

Verified the existing modeless Data file tools close lifecycle for title-bar close, its Close button and Escape, both idle and while a child process is running. Closing during work hides and destroys the window, requests process termination, and keeps the MainWindow Run gate closed until the child exits; the menu action becomes available again afterward. No implementation change was needed. See [validation 71.574](VALIDATION.md#71574-offline-data-tools-window-close-lifecycle-verification). Full refactoring continues.

## 71.573 Console presentation icon and style separation

Moved status lamp painting/vector icon drawing to `consolepresentation_icons.cpp` and menu/console stylesheet definitions to `consolepresentation_styles.cpp`; elapsed text, node health and overview aggregation remain in `consolepresentation.cpp`. Registered both units in production and test qmake projects. Presentation behavior is unchanged. See [validation 71.573](VALIDATION.md#71573-console-presentation-icon-and-style-separation). Full refactoring continues.

## 71.572 ConfigWindow edit-history review separation

Moved undo/redo availability projection and the full reviewed-history dialog from `configwindow.cpp` into `configwindow_history.cpp`. File loading, declaration editing, structure changes, snapshot/export features and the shared edit-history model remain in the main unit. Read-only gating, effective/source diff display, stale-revision checks and save behavior are unchanged. Registered the source in production and test qmake projects. See [validation 71.572](VALIDATION.md#71572-configwindow-edit-history-review-separation). Full refactoring continues.

## 71.571 Plan transition partial-retirement completion separation

Moved saved retirement/completion evidence projection and the explicit partial-retirement completion confirmation from `plantransitionwindow.cpp` into `plantransitionwindow_completion.cpp`. Original-cycle eligibility, successor review, receipt verification/export and retirement submission remain in the main window unit. Original request/status binding, completed-child protections, cancel-by-default confirmation and state recheck after the dialog are unchanged. Registered the source in production and Qt test qmake projects. See [validation 71.571](VALIDATION.md#71571-plan-transition-partial-retirement-completion-separation). Full refactoring continues.

## 71.570 Native scan operation confirmation separation

Moved Native scan lifecycle/Run operation result validation and fresh controller-state confirmation from `nativeruncontrol_scanfinalization.cpp` into `nativeruncontrol_scanconfirmation.cpp`. Event-target stop submission, receiver/catalog finalization evidence and unresolved-intent deadline recording remain in the finalization unit. Exact operation IDs, expected states, unknown-result retention and fresh-status gates are unchanged. Registered the new source through shared Envoy `core.pri`. See [validation 71.570](VALIDATION.md#71570-native-scan-operation-confirmation-separation). Full refactoring continues.

## 71.569 ECC client secure file storage primitive separation

Moved ECC client record file sync, bounded read, exclusive create, protected-directory creation and identity/permission/held-handle guards from `eccclientstore.cpp` into `eccclientstore_storage.cpp`, with the private interface in `eccclientstore_storage_p.h`. Intent sequencing, strict journal replay, retirement policy and reconciliation latching remain in the store unit. File limits, no-follow checks, owner/mode verification, fsync/_commit and atomic no-overwrite behavior are unchanged. Registered the new source through shared Envoy `core.pri`. See [validation 71.569](VALIDATION.md#71569-ecc-client-secure-file-storage-primitive-separation). Full refactoring continues.

## 71.568 MainWindow control-state update separation

Moved `MainWindow::ShowControlSlot` from `mainwindow_status.cpp` into `mainwindow_control_state.cpp`. Connection/run phase presentation, elapsed-time startup, run metadata enablement, lifecycle/recovery action gating, native inspection views and offline-tool gate refresh now sit in a dedicated control-state unit; status table projection remains separate. Control enablement expressions and update ordering are unchanged. Registered the source in both production Envoy and Qt test qmake projects. See [validation 71.568](VALIDATION.md#71568-mainwindow-control-state-update-separation). Full refactoring continues.

## 71.567 Native receiver failure status projection separation

Moved per-node receiver-failure evidence and current-session relevance projection from `nativestatusprojection.cpp` into `nativestatusprojection_failure.cpp`, behind the private `nativestatusprojection_p.h` helper boundary. Base lifecycle rows and receiver/source observation projection remain in the main unit. Freshness, instance/plan/session/PID matching and historical-versus-current evidence labels are unchanged. Registered the new source in shared Envoy `core.pri`. See [validation 71.567](VALIDATION.md#71567-native-receiver-failure-status-projection-separation). Full refactoring continues.

## 71.566 CoBo maintenance window action separation

Moved confirmation dialogs, action-specific request binding and signal emission for single-board Configure/Prepare, LED, and pulser actions from `cobomaintenancewindow.cpp` into `cobomaintenancewindow_actions.cpp`. Target review, lifecycle eligibility, control enablement and result verification remain in the state/view unit. Default-cancel confirmations and exact instance/snapshot/target/parameter bindings are unchanged. Registered the new source through shared Envoy `core.pri`. See [validation 71.566](VALIDATION.md#71566-cobo-maintenance-window-action-separation). Full refactoring continues.

## 71.565 Configuration repository upload workflow separation

Moved publication object chunking/verification, upload-plan construction, same-version resume, remote object observation, commit and receipt acceptance from `configurationrepositoryclient.cpp` into `configurationrepositoryclient_upload.cpp`. Endpoint validation, credential handling, identity/version queries and client lifetime remain in the query unit. Shared SHA-256, strict JSON and bounded-file helpers live in private `configurationrepositoryclient_p.h`; upload identity and byte-integrity checks are unchanged. Registered the new source through shared Envoy `core.pri`. See [validation 71.565](VALIDATION.md#71565-configuration-repository-upload-workflow-separation). Full refactoring continues.

## 71.564 Native Run Control recovery action separation

Moved Native failure cleanup, alarm acknowledgement/release and failed-state reconciliation command requests from `nativeruncontrol_recovery.cpp` into `nativeruncontrol_recoveryactions.cpp`. Run snapshot recovery, replacement-resume eligibility, and asynchronous recovery result handling remain in the original unit. Command gates, retained operation identities, journal evidence and status refresh behavior are unchanged. Registered the new source through shared Envoy `core.pri`. See [validation 71.564](VALIDATION.md#71564-native-run-control-recovery-action-separation). Full refactoring continues.

## 71.563 Native Run Control inspection separation

Moved Native ECC configuration, ownership and Configure readback inspection requests, plus the configuration response handler, from `nativeruncontrol.cpp` into `nativeruncontrol_inspection.cpp`. Status polling, lifecycle continuation, operation submission and reconciliation remain in the core unit. Request generations, cached-configuration behavior, connection guards and signal semantics are unchanged. Registered the new source through shared Envoy `core.pri`. See [validation 71.563](VALIDATION.md#71563-native-run-control-inspection-separation). Full refactoring continues.

## 71.562 Scan plan runtime observation separation

Moved current ECC identity/adoption requests, receiver event-baseline requests and their response handlers from `scanplanwindow_runtime.cpp` into `scanplanwindow_observation.cpp`. Lifecycle advancement, point Run start/status and manual operation reconciliation remain in the runtime unit. Fresh-state checks, adoption confirmation, response decoding and the shared identity-query signal (including the event-baseline continuation flag) are unchanged. Registered the unit in production and Qt test qmake projects. See [validation 71.562](VALIDATION.md#71562-scan-plan-runtime-observation-separation). Full refactoring continues.

## 71.561 RunConfig YAML persistence separation

Moved bounded YAML loading, backward-compatible field defaults/path resolution, validation-before-assign, and atomic `QSaveFile` serialization from `runconfig.cpp` into `runconfig_yaml.cpp`. Default configuration, policy/node validation, directory construction and state-name helpers remain in the model unit. Added the YAML unit to shared production/test qmake sources; the serialization format, limits, relative-path semantics and atomic-commit behavior are unchanged. See [validation 71.561](VALIDATION.md#71561-runconfig-yaml-persistence-separation). Full refactoring continues.

## 71.560 CoBo PLL synthesis separation

Moved CoBo PLL packet synthesis and expert register parameter metadata from `coboclockconfiguration.cpp` into `cobopllconfiguration.cpp`. The overall CoBo/AsAd clock-plan compiler still validates settings, integrates PLL and divider modes, computes effective clock targets and sorts board images. Frequency range validation is shared through the private `coboclockconfiguration_p.h`; register maps, packet ordering and special sync-polarity rewrite are unchanged. Registered the source in experiment-core CMake and qmake projects. See [validation 71.560](VALIDATION.md#71560-cobo-pll-synthesis-separation). Full refactoring continues.

## 71.559 Scan plan UI construction separation

Split `ScanPlanWindow::BuildUi` into focused form, finite-policy, configured-point runtime, and journal-action builders in `scanplanwindow_formui.cpp`, `scanplanwindow_policyui.cpp`, `scanplanwindow_runtimeui.cpp` and `scanplanwindow_journalui.cpp`. The original UI unit now owns window styling, scroll-page setup, section order and process signal wiring. Widget object names, labels, parent page ownership, enablement updates and signal connections are preserved. Registered the units in production and Qt test qmake projects. See [validation 71.559](VALIDATION.md#71559-scan-plan-ui-construction-separation). Full refactoring continues.

## 71.558 Scan journal binding and point replay separation

Moved snapshot binding, runtime identity, event-count baseline, operator review transition and next-point replay from `scanexecutionjournal_decode.cpp` into `scanexecutionjournal_decode_bindings.cpp`. The main decoder now validates the envelope, enforces ordered managed-start completion, dispatches each event to the managed-service, operation or binding handler, then applies restart quarantine and final snapshot-binding checks. Existing ordering and state gates are unchanged. Registered the new source in experiment-core CMake and qmake projects. See [validation 71.558](VALIDATION.md#71558-scan-journal-binding-and-point-replay-separation). Full refactoring continues.

## 71.557 Scan journal operation-event replay separation

Moved generic operation-intent, completion, unknown-result, observed-completion and reconciliation replay from `scanexecutionjournal_decode.cpp` into `scanexecutionjournal_decode_operations.cpp`. Both specialized replay handlers share a private journal replay context, while the main decoder now coordinates the ordered event loop and handles snapshot/runtime bindings, baselines and point transitions. Matching-intent checks, lifecycle mappings, quarantine semantics and legacy retirement behavior are unchanged. Registered the new source in experiment-core CMake and qmake projects. See [validation 71.557](VALIDATION.md#71557-scan-journal-operation-event-replay-separation). Full refactoring continues.

## 71.556 Scan journal managed-service replay separation

Moved managed ECC start-intent/start-result, shutdown-intent/unknown, and process-retirement event replay from `scanexecutionjournal_decode.cpp` into `scanexecutionjournal_decode_managedservice.cpp`. A private replay context carries the ordered journal state needed by the specialized handler; generic snapshot, baseline and operation replay remain in the main decoder. Event ordering, startup-intent resolution gates, identity binding and retirement evidence validation are unchanged. Registered the new source in experiment-core CMake and qmake projects. See [validation 71.556](VALIDATION.md#71556-scan-journal-managed-service-replay-separation). Full refactoring continues.

## 71.555 Scan journal managed-service lifecycle separation

Moved managed ECC service start intent/evidence/unknown handling, shutdown intent/unknown handling, and evidence-bound process retirement from `scanexecutionjournal.cpp` into `scanexecutionjournal_managedservice.cpp`. Basic scan transitions, point advancement, snapshot/runtime identity binding and event-count baseline binding remain in the base journal unit. Durable-intent requirements, deployment identity matching and retirement evidence checks are unchanged. Registered the new source in experiment-core CMake and qmake projects. See [validation 71.555](VALIDATION.md#71555-scan-journal-managed-service-lifecycle-separation). Full refactoring continues.

## 71.554 Scan journal successor validation separation

Moved `ScanExecutionJournal::IsImmediateSuccessorOf` from `scanexecutionjournal_operations.cpp` into `scanexecutionjournal_successor.cpp`. Operation construction and reconciliation remain in the operations unit, while immutable journal-history comparison and its lifecycle/schema-transition checks now have a dedicated translation unit. Registered the new source in experiment-core CMake and qmake projects; successor acceptance rules are unchanged. See [validation 71.554](VALIDATION.md#71554-scan-journal-successor-validation-separation). Full refactoring continues.

## 71.553 System scan point artifact validation separation

Moved per-point hardware configuration checks for PeakingTime, pulser amplitude, threshold and AutoGain scans from `systemscanplan.cpp` into `systemscanplan_pointvalidation.cpp`. The manifest validator retains schema, grid, leaf-name, artifact digest and complete-point-set checks, then delegates each already-decoded artifact to the private `systemscanplan_p.h` boundary. All accepted plan and artifact constraints remain unchanged. Registered the new source in the experiment-core CMake and qmake projects. See [validation 71.553](VALIDATION.md#71553-system-scan-point-artifact-validation-separation). Full refactoring continues.

## 71.552 Scan deployment bundle verification separation

Moved sealed bundle directory/file validation, frozen snapshot binding, per-point artifact/service checks and receiver-plan verification from `scanpointdeployment.cpp` into `scanpointdeployment_verify.cpp`. Shared scan identity/path helpers are declared through the private `scanpointdeployment_p.h`; bundle creation and its validate-before-publish flow remain in the creator unit. Sealed-file ownership/mode checks, point identity, canonical plan checks and tamper rejection are unchanged. Registered the new source in `getdaq-ecc-native`. See [validation 71.552](VALIDATION.md#71552-scan-deployment-bundle-verification-separation). Full refactoring continues.

## 71.551 Run replacement transaction separation

Moved Run-directory reservation, completed-Run identity verification, replacement backup retention and post-completion cleanup from `runrecorder.cpp` into `runrecorder_replacement.cpp`. Atomic file publication is shared through the private `runrecorder_p.h`; journal, native-reference, CSV, snapshot and completion orchestration stay in the main recorder unit. Existing confirmation gates, backup naming, same-filesystem staging, rollback and cleanup ordering are unchanged. Registered the new unit in the shared Envoy qmake project. See [validation 71.551](VALIDATION.md#71551-run-replacement-transaction-separation). Full refactoring continues.

## 71.550 Run configuration snapshot storage separation

Moved per-Run Xcfg include closure, bounded configuration mirroring, staged directory publication and snapshot integrity verification from `runrecorder.cpp` into `runrecorder_snapshot.cpp`, behind the private `runrecorder_p.h` interface. Run reservation/replacement, journal, native reference and completion orchestration remain in `runrecorder.cpp`; snapshot location, limits, manifest schema and completion marker behavior are unchanged. Registered the new unit through the shared Envoy qmake project. See [validation 71.550](VALIDATION.md#71550-run-configuration-snapshot-storage-separation). Full refactoring continues.

## 71.549 Scan plan review snapshot storage separation

Moved scan execution snapshot construction/verification and platform-specific private-directory permission enforcement from `scanplanreview.cpp` into `scanplanreview_snapshot.cpp`. The review unit retains operator review, recovery inspection, runtime-context loading and operation reconciliation; the binding unit continues to share the same fixed-plan validation and journal identity checks. Snapshot byte limits, atomic no-overwrite creation, identity hashes, POSIX 0700 ownership checks and Windows owner-only ACL behavior are unchanged. Registered the unit in Envoy and Widgets test qmake projects. See [validation 71.549](VALIDATION.md#71549-scan-plan-review-snapshot-storage-separation). Full refactoring continues.

## 71.548 EccSystemClient CoBo maintenance API separation

Moved fixed-target maintenance query and selected-CoBo reprepare/configure, LED, blink/flip/pulse and AsAd pulser request adapters into `apps/envoy/eccsystemclient_maintenance.cpp`. The observation unit retains read-only configuration, ownership, receiver and source observations; command intent persistence and submission remain in their existing control path. Fixed maintenance-target identity validation, prepared-state command gates, target membership checks and persist-before-submit behavior are unchanged. Registered the new unit in production Envoy, ECC system-client tests and the direct client probe. See [validation 71.548](VALIDATION.md#71548-eccsystemclient-cobo-maintenance-api-separation). Full refactoring continues.

## 71.491 DataSupervisor implementation and API responsibility separation

Converted the 15 `DataSupervisor::Impl` method bodies from inline private-header definitions into out-of-line implementations in `datasupervisor_impl.cpp`; `datasupervisor_p.h` now holds only private state, child/audit declarations and method signatures. Split public operations into focused units: acquisition validation/start/finish in `datasupervisor_run.cpp`, evidence and process observations in `datasupervisor_observation.cpp`, and catalog transactions/recovery in the existing catalog unit. The small `datasupervisor.cpp` now retains construction, destruction and basic status/configuration accessors. The main implementation decreased from 232 to 39 lines and the private header from 344 to 81 lines. Runtime gates and evidence formats are unchanged.

Linux GCC/C++11 rebuilt `getdaq-data-supervisor-core`, `data-supervisor-tests`, `managed-ice-supervisor-tests`, `supervisor-control-tests` and `getdaq-supervisor`. `supervisor-control-service` and `data-supervisor-process-lifecycle` passed (2/2; 14.57 seconds). The synthetic lifecycle suite includes startup, finish/seal, observation, catalog and failed-session recovery paths; local loopback access was required. No physical hardware, production receiver deployment or acquired DAQ raw was used. See [validation 71.491](VALIDATION.md#71491-datasupervisor-implementation-and-api-responsibility-separation). Full refactoring continues.

## 71.490 DataSupervisor catalog transaction separation

Moved `DataSupervisor::Catalog` and `CanResumeCatalogReplacement` into `datasupervisor_catalog.cpp`. Catalog execution, staged replacement resume checks, publish/finalize identity verification and operation auditing now have a dedicated implementation unit while sharing the same private `DataSupervisor::Impl` state. Replacement authorization, source-lock checks, transaction journal order and old-raw cleanup gates are unchanged. The main implementation decreased from 335 to 232 lines; the catalog unit is 117 lines.

Linux GCC/C++11 rebuilt `getdaq-data-supervisor-core`, `data-supervisor-tests`, `supervisor-control-tests` and `getdaq-supervisor`. `supervisor-control-service` and `data-supervisor-process-lifecycle` passed again (2/2; 14.51 seconds), including catalog replacement and recovery scenarios. Tests use synthetic local processes/receivers and permitted loopback sockets; no physical hardware, production receiver deployment or acquired DAQ raw was used. Documentation-link validation passed 2,003 links. See [validation 71.490](VALIDATION.md#71490-datasupervisor-catalog-transaction-separation). Full refactoring continues.

## 71.489 DataSupervisor recovery lifecycle separation

Moved `AbortStartup`, `AbortFailedSession` and `RetireFailedSession` into `datasupervisor_recovery.cpp`. The shared `DataSupervisor::Impl` definition and its implementation-only audit/storage/process helpers now live in `datasupervisor_p.h`, allowing recovery and normal supervision to use the same private state and proof checks. The public lifecycle contract, audit sequence, PID ownership, timeout and reconciliation gates are unchanged. `datasupervisor.cpp` decreased from 788 to 335 lines; the recovery unit is 165 lines and the private implementation header is 344 lines. CMake registers the new unit.

Linux GCC/C++11 rebuilt `getdaq-data-supervisor-core`, `data-supervisor-tests`, `managed-ice-supervisor-tests`, `supervisor-control-tests` and `getdaq-supervisor`. `supervisor-control-service` and `data-supervisor-process-lifecycle` passed (2/2); lifecycle fixtures cover startup abort, retained failed-session cleanup and retirement among normal/abnormal process and seal scenarios. Tests use local synthetic processes/receivers and required permitted loopback sockets. Documentation-link validation passed 2,002 links. No physical hardware, production receiver deployment or acquired DAQ raw was used. See [validation 71.489](VALIDATION.md#71489-datasupervisor-recovery-lifecycle-separation). Full refactoring continues.

## 71.488 DataSupervisor POSIX receiver process adapter separation

Moved POSIX process and private cycle-control socket helpers from `datasupervisor.cpp` into `receiverprocessposix.cpp` behind the internal `receiverprocessposix_p.h` boundary. The adapter owns safe `posix_spawn` setup, child exit observation/reaping, log descriptor setup and receiver cycle-control socket operations; DataSupervisor retains deployment, journal and lifecycle policy. The main implementation decreased from 882 to 788 lines; the new adapter is 110 lines. Runtime and protocol behavior are unchanged.

Linux GCC/C++11 rebuilt `getdaq-data-supervisor-core`, `data-supervisor-tests`, `managed-ice-supervisor-tests`, `supervisor-control-tests`, `getdaq-supervisor` and `getdaq-supervisor-query`. `supervisor-control-service` and `data-supervisor-process-lifecycle` passed (2/2); the lifecycle suite exercised normal/failed exits, signals, timeouts, partial starts, observation and storage scenarios using local fixtures. Loopback socket tests required the permitted local test environment because the default sandbox denies socket creation. No production hardware, receiver deployment or acquired DAQ raw was used. See [validation 71.488](VALIDATION.md#71488-data-supervisor-posix-receiver-process-adapter-separation). Full refactoring continues.

## 71.487 ECC network control observation separation

Moved the `EccControlJobs` cached identity/status, alarm/failure, handoff, receiver/source/hardware observation and configuration-evidence JSON adapters into `ecccontroljobs_observation.cpp`. The shared private implementation and internal helper definitions now live in `ecccontroljobs_p.h`; command acceptance, worker lifecycle, monitor threads and service-shutdown validation remain in `ecccontroljobs.cpp`. No response fields, lifecycle rules or observation side effects changed. The main implementation decreased from 855 to 275 lines; CMake registers the new unit.

Linux GCC/C++11 rebuilt `getdaq-ecc-native`, `getdaq-ecc-server`, the network service fixture and coordinator tests. The local synthetic `ecc-network-service-normal-0` integration passed (1/1, 89.07 seconds), exercising the service's control and observation endpoints; the data lifecycle and receiver are test fixtures, not production acquisition. No physical hardware or DAQ raw was used. See [validation 71.487](VALIDATION.md#71487-ecc-network-control-observation-separation). Full refactoring continues.

## 71.486 Native ECC recovery and reconciliation separation

Moved `NativeEccCoordinator::Impl` into the internal-only `nativeecccoordinator_p.h`, then moved controller reconciliation, retained failure cleanup, source/receiver retirement and acquisition Unprepare methods into `nativeecccoordinator_recovery.cpp`. The public coordinator API and recovery proof fields are unchanged; `nativeecccoordinator.cpp` now retains construction, normal command dispatch, maintenance and observation methods. The main implementation decreased from 1,060 to 261 lines.

Linux GCC/C++11 rebuilt `getdaq-ecc-native` and its coordinator, filesystem-proof and lifecycle-projection test targets. The filesystem proof, CoBo projection and host ownership unit tests passed (3/3); fake-Ice `fault-start`, `description-recovery-0` and `failure-breakup-unknown` regressions passed. These recovery scenarios use synthetic board descriptions and a test-double data lifecycle; they are not physical hardware, production receiver or DAQ raw validation. See [validation 71.486](VALIDATION.md#71486-native-ecc-recovery-and-reconciliation-separation). Full refactoring continues.

## 71.485 Waveform canvas rendering component separation

Moved the waveform plot widget out of `waveformwindow.cpp` into `waveformcanvas.{h,cpp}`. The canvas now owns 2D trace rendering, 3D surface projection, drag rotation and PNG rendering; the viewer window retains file selection/indexing, frame/channel controls, waveform analysis and export coordination. The shared qmake fragment registers the component for the standalone viewer, Envoy and Widgets tests. Viewer behavior and data formats are unchanged.

Qt 5.12.8/GCC 9.3/C++11 rebuilt the complete Widgets test target and the standalone `getdaq-waveform-viewer`. Eight waveform file/index/analysis/navigation/export tests passed (10/10 including init/cleanup); the standalone viewer's offscreen 3D surface render/drag smoke passed. No DAQ hardware or raw acquisition was used. See [validation 71.485](VALIDATION.md#71485-waveform-canvas-rendering-component-separation). Full refactoring continues.

## 71.484 ScanExecutionJournal operation lifecycle separation

Moved operation intent/complete/observed-complete/UNKNOWN/reconciliation transitions, pending-target lookup and immediate-successor evidence validation from `scanexecutionjournal.cpp` into `scanexecutionjournal_operations.cpp`. The transition/binding implementation remains responsible for point lifecycle and immutable execution/runtime bindings; schema encoding remains in `scanexecutionjournal_codec.cpp`. Journal field layout, schemas, event order and validation semantics are unchanged. The original state implementation decreased from 781 to 407 lines; operation lifecycle logic is 392 lines. CMake and qmake both include the new unit.

Linux GCC/C++11 rebuilt the experiment core, journal tests, ECC journal tests and system experiment tests. `scan-execution-state`, `system-experiment` and `ecc-journal` passed; local documentation links passed 1,996 checks. No hardware or DAQ data was used. See [validation 71.484](VALIDATION.md#71484-scanexecutionjournal-operation-lifecycle-separation). Full refactoring continues.

## 71.481 Remote acquisition-event query adapter separation

Moved the bounded HTTP query, remote service/instance/plan identity checks, source/session/PID binding checks and canonical result encoding for acquisition-event or accepted-file-byte counters into `remoteobservations.cpp`. `RemoteReceiverManager` retains fixed-plan source selection and its original Ready/live receiver-cycle gate, then passes only that verified target context to the query adapter. No control operation or manager state is changed by the observation call.

Linux GCC/C++11 rebuilt `getdaq-ecc-native` and remote receiver fixture targets. `native-ecc-remote-receiver-normal-0` passed (1/1, 84.80 seconds), exercising both event and file-byte observations within the two-service synthetic lifecycle. The manager decreased from 1,363 to 1,345 lines; docs link validation passed 1,993 links. No hardware was accessed. See [validation 71.481](VALIDATION.md#71481-remote-acquisition-event-query-adapter-separation-2026-10-25). Full refactoring continues.

## 71.480 Remote receiver endpoint identity separation

Moved fixed remote-endpoint declaration validation, deterministic endpoint sorting and the public data-binding SHA-256 calculation into `remoteendpointidentity.cpp`, with declarations in private `remoteendpointidentity_p.h`. `RemoteReceiverManager` continues to consume the same normalized options and binding; IPv4 canonicalization, endpoint uniqueness, timeout bounds and credential-path declaration checks are unchanged. The binding still excludes credential contents and short-lived controller epochs. The manager implementation decreased from 1,393 to 1,363 lines; the identity module is 48 lines.

Linux GCC/C++11 rebuilt `getdaq-ecc-native`, `native-ecc-coordinator-tests` and `native-ecc-remote-receiver-tests`. The full `native-ecc-remote-receiver-normal-0` localhost integration passed (1/1, 86.91 seconds), exercising two supervisor processes and actual synthetic receiver data connections; no physical hardware was used. Documentation link validation passed 1,992 links. See [validation 71.480](VALIDATION.md#71480-remote-receiver-endpoint-identity-separation-2026-10-25). Full refactoring continues.

## 71.479 Receiver catalog inspection separation

Moved read-only catalog receipt/manifest/source/seal/file-identity verification into `receivercataloginspection.cpp`, including the logical-path inspector required for renamed replacement backups. Source seal boundary checks and bounded `.graw` directory enumeration are private shared primitives used by both catalog creation and inspection; this avoids duplicating their validation rules. Catalog creation and source lease handling remain in `receivercatalog.cpp`, now 196 lines; the inspector is 159 lines. Public inspection behavior and accepted schemas are unchanged.

Linux GCC/C++11 rebuilt `receiver-catalog-tests`, `catalog-transfer-tests` and `getdaq-catalog`. Catalog and transfer filesystem regressions passed (2/2, 27.05 seconds), covering archive metadata/file identity, replacement recovery/finalization and transfer resume/readback. Documentation link validation passed 1,992 links. No remote host or hardware was used. See [validation 71.479](VALIDATION.md#71479-receiver-catalog-inspection-separation-2026-10-25). Full refactoring continues.

## 71.478 Receiver catalog replacement transaction separation

Moved candidate preparation, publication/recovery, read-only transaction inspection and finalization into `receivercatalogreplacementtransaction.cpp`. The archive assembly unit now owns source-session validation, immutable catalog inspection and initial archive publication; a narrow private `InspectCatalogAt(path, logicalPath)` bridge lets replacement recovery validate a renamed backup against its original logical Run path. Transaction journal schema, phase markers, same-filesystem no-replace renames, recovery classifications, same-name cleanup and failure-preservation behavior are unchanged. Removed an unused descriptor input stream while reducing `receivercatalog.cpp` from 972 to 332 lines; replacement plan and transaction units are 79 and 655 lines.

Linux GCC/C++11 rebuilt `receiver-catalog-tests`, `catalog-transfer-tests` and `getdaq-catalog`. Both catalog and transfer filesystem regressions passed (2/2, 27.58 seconds); Markdown link validation passed 1,990 links. Tests use synthetic local fixtures and do not verify remote hosts or hardware. See [validation 71.478](VALIDATION.md#71478-receiver-catalog-replacement-transaction-separation-2026-10-25). Full refactoring continues.

## 71.477 Receiver catalog replacement plan separation

Moved `PlanReceiverCatalogReplacement()` and its verified file-identity enrichment from `receivercatalog.cpp` to `receivercatalogreplacementplan.cpp`. The new unit builds a same-Run logical file plan only after both catalogs pass inspection, binds each selected filename to source catalog receipt and inode metadata, and rechecks both catalog snapshots before returning. Collision precedence, retained-file handling, canonical validation and public API behavior are unchanged. `receivercatalog.cpp` decreased from 1,038 to 972 lines; the plan module is 79 lines.

Linux GCC/C++11 rebuilt `receiver-catalog-tests`, `catalog-transfer-tests` and `getdaq-catalog`. `receiver-catalog-filesystem` and `catalog-transfer-filesystem` passed (2/2, 26.24 seconds), including replacement transaction recovery and source preservation. Documentation link validation passed 1,989 links. See [validation 71.477](VALIDATION.md#71477-receiver-catalog-replacement-plan-separation-2026-10-25). Full refactoring continues.

## 71.476 Receiver catalog request identity separation

Moved receiver-catalog option validation, canonical request encode/decode, request-key hashing and publication/replacement path derivation into `receivercatalogrequest.cpp`. The private `receivercataloginternal.h` now exposes narrow shared validation/path declarations used by catalog inspection and replacement; `receivercatalog.cpp` retains archive inspection, file linking and replacement transaction policy. Removed stale scan-analysis includes from the transaction/assembly unit. Request schemas, canonical bytes, identity digests, limits and accepted input are unchanged. `receivercatalog.cpp` decreased from 1,120 to 1,038 lines; request handling is 102 lines.

Linux GCC/C++11 rebuilt `receiver-catalog-tests`, `catalog-transfer-tests` and `getdaq-catalog`. `receiver-catalog-filesystem` and `catalog-transfer-filesystem` passed (2/2), including canonical request/schema validation, replacement journals, transfer identity binding, resume and source preservation. Documentation link validation passed 1,989 links. No real receiver, remote host or hardware was used. See [validation 71.476](VALIDATION.md#71476-receiver-catalog-request-identity-separation-2026-10-25). Full refactoring continues.

## 71.475 OpenSSH catalog transfer process boundary separation

Moved OpenSSH subprocess lifecycle and pipe setup out of `catalogtransfer.cpp` into `catalogtransfer_ssh.cpp`: command construction/quoting, close-on-exec pipes, `posix_spawnp`, child wait/reap and remote destination confirmation now form their own transport adapter. Stream framing, source identity checks, payload/resume validation and remote archive publication remain in the catalog transfer implementation; public APIs and transfer protocol are unchanged. `catalogtransfer.cpp` decreased to 589 lines; the SSH adapter is 63 lines.

Linux GCC/C++11 rebuilt `catalog-transfer-tests` and `getdaq-catalog`. The registered `catalog-transfer-filesystem` test passed (1/1), exercising 12 native transfer scenarios, fake OpenSSH command execution, stream/TCP resume, independent readback, corruption refusal and source preservation; the test cleaned 15,269,888 synthetic raw bytes. An initial sandbox run could not create its loopback listener (`EPERM`); the same test passed with local loopback enabled. No real remote host or hardware was used. See [validation 71.475](VALIDATION.md#71475-openssh-catalog-transfer-process-boundary-separation-2026-10-25). Full refactoring continues.

## 71.474 Native ECC filesystem proof primitives separation

Moved Native ECC's identity-bound directory handle, immutable file proof, create-only/fsync writer, and failed-Describe audit capture into `nativeeccfilesystemproof.{p.h,cpp}`. `nativeecccoordinator.cpp` retains the operation/state policy and calls these shared private primitives for the same proof paths. Owner/mode, no-follow, inode, link-count, size/content, retained-prefix and audit-chain checks and their failure messages remain unchanged. Added a focused local filesystem test covering permission changes, symlink rejection, exclusive creation, same-size content tampering, authorized prefix-preserving append and chained audit capture. Also clarified four misleadingly indented proof/manifest checks in the coordinator without changing their conditions.

Linux GCC 9.4/C++11 rebuilt `getdaq-ecc-native`, `native-ecc-filesystem-proof-tests`, `native-ecc-coordinator-tests`, `host-control-ownership-tests` and `cobo-lifecycle-projection-tests`. The filesystem proof, host ownership utility and CoBo lifecycle projection tests passed (3/3). A separate coordinator host-ownership integration fixture could not start because local Ice TCP sockets are denied (`Operation not permitted`); no hardware was accessed. See [validation 71.474](VALIDATION.md#71474-native-ecc-filesystem-proof-primitives-separation-2026-10-25). Full refactoring continues.

## 71.473 Remote receiver observation query separation

Moved bounded read-only receiver endpoint observation from `remotereceivermanager.cpp` into `remoteobservations.cpp`. The observation unit performs the same 1.5-second aggregate query, checks endpoint/controller/instance/plan identity and fixed source ordering, enforces receiver-count and JSON-byte bounds, and returns the same per-node JSON shape. Shared bounded UTF-8-safe remote error sanitization moved with it and remains used by the manager's operation result path. `RemoteReceiverManager::ObserveReceiversJson()` now delegates using its configured endpoint list and cached status; lifecycle and event-counter operations remain in the manager. Added an integration assertion for endpoint identity, available lifecycle data and absence of hardware/ECC actions. The manager decreased from 1,445 to 1,393 lines; the new observation unit is 89 lines.

Linux GCC 9.4/C++11 rebuilt `getdaq-ecc-native`, `native-ecc-remote-receiver-tests` and `supervisor-control-tests`. The `observe-events` integration fixture could not start because this environment denies local Ice TCP sockets (`Operation not permitted`); its new assertion compiled but did not run. Documentation link validation passed 1986 links. No hardware was accessed. See [validation 71.473](VALIDATION.md#71473-remote-receiver-observation-query-separation-2026-10-25). Full refactoring continues.

## 71.472 Configure audit capture separation

Moved secure capture of the completed Configure audit from `systempreparation.cpp` into private `systempreparationauditcapture.{p.h,cpp}`. The capture still opens without following symlinks, checks regular-file identity, effective owner, exact permissions, single link and 1–8 MiB size, reads the full file, then verifies its inode and metadata stayed stable. Configure readback decoding still runs at the same point and uses the same chained audit digest; only the implementation boundary changed.

Linux GCC 9.4/C++11 rebuilt `getdaq-hardware-core` and `hardware-system-release-tests`. A temporary standalone check accepted a valid 0600 audit file and rejected a symlink. Documentation link validation passed 1985 links. No hardware or network was accessed. See [validation 71.472](VALIDATION.md#71472-configure-audit-capture-separation-2026-10-25). Full refactoring continues.

## 71.471 ECC successor-plan transition protocol separation

Moved successor identity decode/encode, reviewed data-plan transition construction and exact plan-retirement receipt verification from `eccprotocol.cpp` into `eccplantransitionprotocol.cpp`. Shared strict JSON, canonical integer, digest, bounded text and counter helpers now live in private `eccprotocolinternal_p.h`, used by the existing ECC codecs and transition protocol unit. Request encoding/validation, schemas, limits, identity binding and receipt acceptance rules are unchanged. CMake and qmake include the new implementation; the main codec decreased from 782 to 642 lines, and the transition unit is 97 lines.

Linux GCC 9.4/C++11 rebuilt the control protocol, Native ECC and coordinator test targets. The ECC protocol test passed canonical bytes, strict fields, bounds and request/result binding; the scan runtime identity test also passed. Qt 5.12.8/GCC 9.3 rebuilt the Envoy target. No hardware or network was accessed. See [validation 71.471](VALIDATION.md#71471-ecc-successor-plan-transition-protocol-separation-2026-10-25). Full refactoring continues.

## 71.470 Scan deployment capture and point binding separation

Moved `CaptureScanRuntimeIdentityDeployment` and `BindScanRuntimeIdentityExpectation` into `scanruntimeidentity_deployment.cpp`. This unit now owns the boundary from configured ECC/receiver observations to a canonical fixed deployment, then binds that deployment to one point of the frozen scan snapshot. Shared digest, node-set and strict bounded-field validation remains in `scanruntimeidentityinternal_p.h`; runtime observation and revalidation remain in `scanruntimeidentity.cpp`. CMake and qmake include the unit. The observation implementation decreased from 514 to 356 lines; deployment capture/binding is 173 lines.

Linux GCC 9.4/C++11 rebuilt the control protocol, Native ECC production and coordinator test targets. Qt 5.12.8/GCC 9.3 rebuilt the complete Widgets test target. The scan runtime identity test passed its fixed identity, fresh receiver-set, idle state, data scope/epoch and exact-plan checks. See [validation 71.470](VALIDATION.md#71470-scan-deployment-capture-and-point-binding-separation-2026-10-25). Full refactoring continues.

## 71.469 Supervisor handoff and retirement file proofs

Moved the data Supervisor controller's handoff proof verification and sealed retirement metadata file handling from `controljobs.cpp` into private `controlproofs.{h,cpp}`. The extracted proof still binds the original request, proof, cycle evidence and successor-plan copy by exact bytes, digest, inode, owner, link count and mode; retirement metadata remains create-only and is sealed read-only after identity checks. Control-operation sequencing and journal records remain in `SupervisorControl`. The coordinator decreased from 729 to 654 lines; the proof module is 98 lines.

Linux GCC 9.4/C++11 rebuilt `getdaq-data-supervisor-core`, `native-ecc-remote-receiver-tests` and `supervisor-control-tests`; the control protocol codec test passed. Network-backed handoff/retirement integration was not run because this environment denies local TCP sockets. No hardware was accessed. See [validation 71.469](VALIDATION.md#71469-supervisor-handoff-and-retirement-file-proofs-2026-10-25). Full refactoring continues.

## 71.468 Native ECC accepted-request proof separation

Moved the private request proof for accepted ECC network operations from `ecccontroljobs.cpp` into `eccrequestproof.{h,cpp}`. The proof still binds the exclusively created operation directory and `request.json` by filesystem identity, owner, mode and exact encoded bytes; when a reviewed successor plan is present, it also checks the retained review lease and the copied plan's identity, permissions and exact bytes. Replay and accepted-operation call sites now share this component. Native ECC operation ordering and journal content are unchanged; the coordinator decreased from 892 to 855 lines.

Linux GCC 9.4/C++11 rebuilt `getdaq-ecc-native` and `native-ecc-coordinator-tests`. Runtime coordinator integration was not run because the sandbox rejects local Ice TCP sockets (`Operation not permitted` in the preceding release fixture); no hardware was accessed. See [validation 71.468](VALIDATION.md#71468-native-ecc-accepted-request-proof-separation-2026-10-25). Full refactoring continues.

## 71.467 Scan runtime identity protocol codec separation

Moved the scan runtime point-identity request, frozen expectation and observation response encoders/decoders from `scanruntimeidentity.cpp` into `scanruntimeidentity_codec.cpp`. Shared strict JSON field checks, canonical bounded integers, digest validation and expectation validation now live in a private inline helper header used by both units. Observation comparison and stable deployment digest generation remain in the runtime identity implementation. CMake and qmake include the codec; schema versions, field sets, limits, validation order and wire bytes are unchanged. The runtime identity implementation decreased from 798 to 514 lines; the codec is 220 lines.

Linux GCC 9.4/C++11 rebuilt `getdaq-control-protocol`, Native ECC production, coordinator tests and fixed-configuration tests. The scan runtime identity protocol test passed. Markdown link validation passed for 1979 links. No hardware or network was used. See [validation 71.467](VALIDATION.md#71467-scan-runtime-identity-protocol-codec-separation-2026-10-25). Full refactoring continues.

## 71.466 System preparation release-audit writer separation

Moved the shared chained audit writer used by SystemPreparation failure stop, configuration invalidation and unprepare/recovery paths into private `systempreparationreleaseaudit.{h,cpp}` implementation units. Record shape, step filenames and sequence, 16 MiB byte limit, append/fsync behavior and returned chain digest are unchanged; the preparation coordinator still owns release ordering and hardware decisions. Registered the new source in `getdaq-hardware-core`.

Linux GCC 9.4/C++11 rebuilt `getdaq-hardware-core` and `hardware-system-release-tests` successfully. The integration executable could not run because its Ice TCP fixture hit the environment's socket restriction (`Operation not permitted`); no hardware was accessed. See [validation 71.466](VALIDATION.md#71466-system-preparation-release-audit-writer-separation-2026-10-25). Full refactoring continues.

## 71.465 ScanPlanWindow异步进程生命周期拆分

将扫描计划外部工具进程的完成分派、失败处理和运行中关闭门控从`scanplanwindow.cpp`移至`scanplanwindow_process.cpp`。原有成功/失败消息、catalog绑定与journal隔离、策略二次独立验证、计划输出核验，以及进程运行时拒绝关窗逻辑保持原样。Envoy与Widgets测试qmake工程均接入新单元；计划控制实现文件由1224行降至958行，新进程回调单元282行。

Qt 5.12.8/GCC 9.3/C++11正式Envoy和Widgets测试目标构建成功。使用本轮当前源码重建的`getdaq-config`和`getdaq-config-check`运行扫描计划UI回归，覆盖真实离线计划生成、策略独立复核、拒绝篡改/重复输出和计划审阅流程；连同输出篡改专项共4/4通过（含init/cleanup）。测试输出仅含offscreen插件的size-hints提示；没有实板或DAQ raw操作。详见[验证71.465](VALIDATION.md#71465-scanplanwindow-asynchronous-process-lifecycle-separation-2026-10-25)。完整重构继续。

## 71.464 Native ECC纯配置编译与运行协调拆分

将`NativeEccOptions`到规范化准备参数、发送器顺序、不可变ECC快照和数据控制范围摘要的纯编译流程从`nativeecccoordinator.cpp`移入`nativeeccplancompiler.cpp`。共用结果类型与内部入口放在私有`nativeeccplancompiler_p.h`；运行协调器继续用同一编译结果初始化控制对象。原有输入路径/摘要、工件、description拓扑、sender策略和严格JSON校验均保留；CMake加入新编译单元。

Linux GCC/C++11重建`getdaq-ecc-native`、`native-ecc-coordinator-tests`和`fixed-ecc-configuration-tests`成功。固定配置/发布/选择绑定与离线逐点部署集成回归通过，测试明确`no hardware executed`。构建同时报告协调器原有的misleading-indentation告警，本次未修改这些无关代码。详见[验证71.464](VALIDATION.md#71464-native-ecc-pure-plan-compilation-and-runtime-coordination-separation-2026-10-25)。完整重构继续。

## 71.463 ScanPlanWindow界面组装拆分

将`ScanPlanWindow`中整段界面构建和信号连接从计划/扫描控制逻辑移至`scanplanwindow_ui.cpp`的私有`BuildUi()`方法；构造函数只创建异步进程并调用该组装入口。控件、样式、布局、信号连接及创建顺序保持不变。控制逻辑文件从1546行降至1224行，新UI组装单元为347行；正式Envoy和Widgets测试工程都已加入该文件。

Qt 5.12.8/GCC 9.3/C++11正式Envoy和完整Widgets测试目标重建成功。扫描计划输出篡改拒绝与控制台入口专项QtTest共4/4通过（含init/cleanup）。没有实板、远端服务或DAQ raw验证。详见[验证71.463](VALIDATION.md#71463-scanplanwindow-ui-assembly-separation-2026-10-25)。完整重构继续。

## 71.462 Native ECC远端操作协议解码拆分

将远端控制请求规范化解码和远端操作响应解码从`remotereceivermanager.cpp`移入独立的`remoteoperationcodec.{h,cpp}`。管理器继续负责HTTP请求、操作轮询和结果协调；codec集中处理精确字段集合、请求身份绑定、阶段与状态、handoff证明和catalog完成回执校验。新增实现接入Native ECC CMake目标，协议schema、校验条件和外部行为保持不变。

Linux GCC/C++11构建Native ECC、supervisor和receiver目标成功。生成的远端receiver集成回归`normal-0`通过：两个真实服务进程与本机合成receiver完成直接raw TCP流程；测试明确不代表硬件或吞吐验收。1975项本地Markdown链接检查通过。无真实设备或DAQ数据验证；完整重构继续。详见[验证71.462](VALIDATION.md#71462-native-ecc-remote-operation-protocol-decoder-separation-2026-10-25)。

## 71.461 扫描执行journal状态模型与编解码拆分

将`ScanExecutionJournal::Encode/Decode`及版本化事件JSON持久化逻辑移入`scanexecutionjournal_codec.cpp`；`scanexecutionjournal.cpp`保留生命周期迁移、操作意图与不变量判断，减少两个职责交织。双方共用的字段解析、状态映射、身份和事件校验规则集中于私有`scanexecutionjournalinternal.h`。状态实现由1613行降至781行，codec为671行；公开接口、schema兼容范围和运行时判定未改变。CMake与`experiment.pri`均加入新编译单元。

Linux GCC/C++11构建`getdaq-experiment-core`和`scan-execution-state-tests`成功，状态/重启隔离/篡改拒绝专项通过；Qt 5.12.8/GCC9.3完整Envoy和Widgets测试目标均重生成并构建成功，journal round-trip/quarantine/reconciliation QtTest 3/3通过（含init/cleanup）。本轮未运行完整Widgets套件、实板或现场DAQ。详见[验证71.461](VALIDATION.md#71461-scan-execution-journal-model-and-codec-separation-2026-10-25)。完整重构继续。

## 71.460 Receiver catalog与扫描归档分析职责拆分

将扫描点与已封存catalog的绑定、绑定索引汇总、Threshold hit-pattern分析，以及PeakingTime/Pulser/AutoGain波形分析从`receivercatalog.cpp`移入`receivercatalog_scananalysis.cpp`。`receivercatalog.cpp`现在只保留catalog身份检查、发布/替换事务与实现；两侧共享的严格JSON容量、规范整数及文件元数据比较移至私有`receivercataloginternal.h`，避免复制安全校验。catalog核心文件由1747行减至1120行，扫描分析为660行；公开API和验证顺序未改变。

Linux GCC/C++11重建`getdaq-data-supervisor-core`、`getdaq-catalog`和`receiver-catalog-tests`成功。直接运行catalog测试程序68项检查通过，覆盖catalog文件系统、扫描绑定和多种流式波形分析；该CMake构建树未发现CTest用例，故以构建生成的测试程序直接执行为证据。无实板、真实receiver、网络或DAQ数据验证；完整重构继续。详见[验证71.460](VALIDATION.md#71460-receiver-catalog-and-scan-archive-analysis-separation-2026-10-25)。

## 71.459 MainWindow菜单装配拆分与控制器初始化顺序

Configuration、Tools、Run及Recovery菜单和恢复/维护动作装配移入`mainwindow_menus.cpp`；RunControl与GUI全部命令/观察信号的绑定移入`mainwindow_connections.cpp`。`InitializePar()`不再承担菜单创建或signal-slot wiring。`RunControl`改在MainWindow构造阶段创建，早于扫描菜单信号连接，消除菜单初始化时读取尚未分配的控制器指针。原菜单动作、控制信号及权限门槛保持；`mainwindow.cpp`由902行降至547行。

Qt5.12.8/GCC9.3/C++11正式Envoy和完整Widgets测试目标构建成功；扫描计划菜单窗口、Native/Legacy控制门槛、ECC观察和日志行为定向QtTest 9/9通过，含自动Run设置锁定/循环和Run号回退行为；两个目标`make -q`均为最新，本地Markdown链接检查1972项通过。未运行全套Widgets和localhost mock；无实板或DAQ raw验证。详见[验证71.459](VALIDATION.md#71459-mainwindow-menu-assembly-and-controller-initialization-order-2026-10-25)。完整重构继续。

## 71.458 MainWindow状态投影与日志职责拆分

将服务状态到界面的职责从`mainwindow.cpp`拆为`mainwindow_status.cpp`（CoBo/Native/Legacy状态表、运行控制准入、ECC配置与占用观察）和`mainwindow_log.cpp`（事件日志着色/持久化、连接会话日志与导出）。两个编译入口`envoy.pro`和`tests/tests.pro`均纳入新单元；信号/槽、状态门槛、日志格式与生命周期保持原样。`mainwindow.cpp`由1234行降至902行。

Qt5.12.8/GCC9.3/C++11正式Envoy与完整Widgets测试目标从当前源码构建成功。状态投影、Native/Legacy整机操作门槛、单CoBo边界、ECC观察过期、日志会话/导出/着色和状态表高度共9项定向QtTest通过；另有离线工具关闭方式、关闭中菜单锁、双窗口互锁8个行为场景通过；1971项本地Markdown链接检查通过。未执行需要本机mock监听的全套Widgets回归；没有实板、receiver或DAQ raw验证。完整重构继续。详见[验证71.458](VALIDATION.md#71458-mainwindow-status-projection-and-connection-logging-extraction-2026-10-25)。

# 重构进度与剩余工作

## 71.457 RunControl分派与Run收尾单元拆分

将`RunControl`中37个Native维护、恢复、扫描和状态查询转发槽移入`runcontrol_native.cpp`；Pause/Resume保留Legacy状态准入，同时把Native路径委托给`NativeRunControl`。另将归档、文件写入、Run replacement清理、完成通知和Finalize入口移入`runcontrol_finalization.cpp`。`runcontrol.cpp`保留Legacy状态机及其异步收尾回调，拆分未改变对外信号、Run门控、操作顺序或审计行为。

Qt5.12.8/GCC9.3/C++11下正式Envoy和完整Widgets测试目标均从当前源码构建成功。定向回归覆盖Legacy整机Describe/Prepare/Configure/Start/Pause/Resume/Stop顺序、Run替换归档等待/旧文件清理/重复归档防护及Data file tools菜单关闭/重开，共8/8通过。完整Widgets回归未在受限sandbox中作为通过证据：其本机mock监听及全局缓存写入受权限限制。没有实板、receiver或DAQ raw验证；Windows当前源码构建仍待恢复WSL到Windows互操作后复验。详见[验证71.457](VALIDATION.md#71457-runcontrol-dispatch-and-run-finalization-unit-extraction-2026-10-25)。

## 71.456 受管 ECC 启动意图与进程证据

POSIX Native层现将ECC受管启动与扫描journal连接：schema-8在fork/exec前持久化点级operation和固定executable/service配置摘要；supervisor核验两份输入，exec成功且同一子进程仍存活后返回私有证据，journal才推进`Reviewed → ECCStarted`。失败或悬空意图进入`NeedsReconciliation`，不自动重试；该证据仅说明进程已执行且存活，不证明HTTP服务Ready，之后仍须独立运行身份核验。schema 1–7继续读取，schema-8保留现有关闭/退休协议。

Linux GCC/C++11 Native、supervisor、journal、state和实验核心目标构建/定向回归通过；受控localhost假ECC测试验证启动证据、摘要不匹配拒绝、悬空intent隔离及与shutdown intent兼容。此原语仍未接入Envoy逐点deployment选择或自动推进，也未验证Windows/macOS、真实receiver、DAQ raw或实板。详见[验证71.456](VALIDATION.md#71456-受管ecc启动意图与进程证据2026-10-25)和[扫描执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md)。

## 71.455 扫描逐点部署bundle只读重验

新增`VerifyScanPointDeploymentBundle`和POSIX native `getdaq-scan-deploy verify`。它将已生成bundle重新绑定冻结execution snapshot，严格检查manifest字段/摘要/点序、属主和0400封存文件、artifact内容、ECC固定配置、receiver plan规范编码与摘要、点级deployment/audit root，以及每点独占的目标plan文件名。它验证的是本地bundle完整性与内部身份一致，不认证来源、不安装远端文件、不启动服务或控制硬件。

Linux GCC/C++11重建`getdaq-ecc-native`、`getdaq-scan-deploy`和`fixed-ecc-configuration-tests`通过。两点bundle经API和CLI重验；篡改manifest冻结snapshot SHA及改写后重封存receiver plan均被拒绝，恢复plan原字节后通过。文档本地链接1967项通过。Envoy仍未接入bundle选择、受管ECC启动和跨点推进；完整重构继续。详见[验证71.455](VALIDATION.md#71455-逐点部署bundle只读重验2026-10-25)与[扫描执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md)。

## 71.454 受管 ECC 进程退出证据绑定扫描journal

POSIX `RetireManagedEccServiceForScanPoint`把受管ECC服务的认证handoff关闭、同一子进程正常退出证据与schema-7扫描journal连接：核对实际service配置摘要等于已审阅摘要，先同步持久化shutdown intent，再发送关闭请求；只有精确accepted响应和同一PID退出码0才可记录`Retired`。超时/异常隔离当前点；崩溃遗留intent在重开时转入`NeedsReconciliation`。旧schema 1–6仍可读，普通退休和人工对账没有获得退休授权。

Native、supervisor/journal、状态机和实验核心目标均构建通过，localhost假ECC HTTP/受管子进程集成及相关定向测试通过。该闭环尚未接入Envoy、逐点deployment选择或自动推进，也没有真实receiver、实板或Windows/macOS证据。详见[验证71.454](VALIDATION.md#71454-受管ecc进程退出证据绑定扫描journal2026-10-25)与[扫描执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md)。

## 71.453 补齐 handoff 后的 ECC 服务退休

认证服务关闭现与既有完整receipt逻辑保持一致：既接受原handoff直接成功，也接受原handoff保持failed、但准确`complete-data-handoff`补齐操作已完成的受限恢复结果；仍拒绝未完成补齐、非原操作、活动提交/观察及未知硬件控制状态。原先只检查原operation `complete` 的门槛会错误拒绝合法补齐后的关闭。

Linux GCC/C++11重建服务及集成夹具通过。部分 handoff 故障补齐后退休/接班/冷fence集成 **1/1通过**（139.92秒），普通handoff原路径复跑 **1/1通过**（87.22秒）；两个测试自动清理合成raw，分别81,920与65,536字节，剩余均0。此闭环仍只是服务原语及跨进程证据，扫描journal未记录shutdown/进程退出，executor未自动启动服务或推进下一点。详见[验证71.453](VALIDATION.md#71453-补齐完成后的ecc服务退休2026-10-25)和[扫描架构](SCAN_EXECUTOR_ARCHITECTURE.md)。

## 71.452 ECC handoff后的认证服务退休

ECC native service新增认证`POST /v1/shutdown`：只有同一实例/快照下的原始data handoff已完成、没有提交或观察在途且硬件控制状态已知并处于Idle/Described时才接受；检查精确handoff receipt，并依赖handoff完成前的全部receiver正常停止/封存验证。服务返回accepted后正常退出，由析构释放host board lock。handoff产生的预期数据reconciliation fence与硬件控制状态分开判断。跨进程回归验证handoff前拒绝、handoff后旧进程退出、successor实际取得同一板卡主机锁及原实例冷重启继续fenced。

Linux GCC/C++11重建`getdaq-ecc-server`与`ecc-network-service-tests`成功。获准localhost集成的Ice encoding 1.0/1.1场景均通过（2/2，各约87秒）；每个场景生成并清理65,536字节合成raw，残留0。未连接真实硬件/数据主机。本轮仅交付安全退休原语；扫描executor尚未编排ECC service启动、shutdown/锁释放证明、journal `Bound → Retired`和下一点推进，整套重构继续。详见[验证71.452](VALIDATION.md#71452-ecc-handoff后认证服务退休2026-10-25)及[扫描执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md)。

## 71.451 Data file tools 关闭等待期间锁定菜单入口

Data file tools关闭时若子进程仍在运行，窗口先隐藏并请求终止；此时禁用Tools菜单入口并给出“正在停止”的提示，避免用户重新打开仍在关闭的窗口。子进程退出、窗口销毁后，菜单入口恢复；Run启动互锁继续保持到窗口实际销毁。

Qt 5.12.8/GCC 9.3/C++11完整Widgets测试目标及正式Envoy `apps/envoy/envoy.pro`均从当前源码构建成功。关闭专项10/10通过，覆盖窗口叉号、Close、Esc、忽略SIGTERM的子进程强杀收尾，以及MainWindow菜单禁用/恢复。仅Linux offscreen与合成sleep进程，无真实数据处理、硬件或跨平台验收。

## 71.450 远端扫描 successor plan 认证暂存

目标数据主机现在可通过现有bearer认证`POST /v1/operations`提交`stage-successor-plan`，由服务在本地暂存规范receiver plan。该专用请求上限为2 MiB、plan上限1 MiB；普通控制请求仍保持8 KiB。服务先校验plan摘要/规范编码、原receiver executable与资源上限、scan execution/deployment/point绑定和目标路径条件，再记录唯一operation并以0400独占封存plan，创建执行专属0700审计父目录。不退休旧plan、不启动receiver、不推进扫描journal。

Linux GCC/C++11的supervisor/query/codec重建成功；大于8 KiB的已认证HTTP请求、query CLI重放、服务冷重启读取、完整12场景plan-transition回归及codec测试通过。数据由合成receiver fixture生成并清理，无实板或现场数据。现有bearer HTTP未增加TLS；跨不可信网络的机密性仍需传输层保护。完整扫描执行器、ECC进程退休/占用释放、故障恢复、Windows/macOS和现场验收仍未完成。详见[验证71.450](VALIDATION.md#71450-远端扫描-successor-plan-认证暂存2026-10-25)和[扫描执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md)。

## 71.447 离线工具按钮可用条件与计时回归

Data file tools的Run tool与真实启动复用同一输入/输出核对：缺少输入、单输入转换有多个文件、输出未填或已存在时灰显并提示原因；未选择项时Remove selected灰显。实际启动仍再次核对，不能用界面可用性替代文件状态检查。

生产GUI构建、专项13/13及最终完整Widgets174/174通过。首次完整回归发现自动Run用例的默认5秒断言对含至少3秒间隔的两轮流程过紧；仅扩大测试等待上限，产品间隔及单调时钟≥3000ms断言保持，Time/FileSize/Either补测通过。证据和失败/修正后日志见[工具可用条件验证](../build-ui/offline-tools-readiness-20261024/README.md)及[验证71.447](VALIDATION.md#71447-离线工具可用性与自动run测试等待上限2026-10-24)。

## 71.446 CoBo历史日志独立终点

准备及采集审计镜像新增scope/记录数/字节数/终点SHA256检查点；历史读取可要求独立终点精确匹配，拒绝完整尾行丢失、过期终点及同长度/记录数的另一条合法历史。旧无终点读取明确不验证尾部完整性；故障负向停止记录null检查点并保持UNKNOWN，不能借历史恢复控制权。

严格C++11单元及相关生产目标构建通过；新增独立审计检查点集成的3场景、完整准备/采集、故障停止/Breakup及Native正常两编码已通过。准备、采集、负向停止、释放及Native共36项CTest最终全部通过（1235.97秒）；[验证产物](../build-cobo-lifecycle-checkpoint-20261024/README.md)和[日志规则](COBO_LIFECYCLE_JOURNAL.md)记录范围。冷恢复仍需结合独立原操作和新鲜硬件/数据证据，整套重构继续。

## 71.445 离线工具接入、菜单禁用态与关闭修复

Tools新增共享波形查看器和离线数据工具窗口，接入原有检查、矩阵转换、事件合并及可选FITS/ROOT转换/ROOT查看器；独立程序保留。窗口/后台任务与普通、扫描点Run启动共用GUI及控制线程互锁，全部关闭并退出任务后才解锁。缺失可选工具、活动Run期间不可用菜单及相关控件显示灰字。Elapsed显示h/min/s，小时超过24不回卷。

修复数据工具窗口退出回调导致无法关闭的问题：改用普通独立工具窗口，叉号、Close、Esc统一关闭/停止路径。程序发现改为初始化/打开Tools时刷新，避免每次状态轮询扫描PATH。专项13项、完整Widgets174项、Native42项与独立查看器启动/3D回归通过；完整结果见[验证71.445](VALIDATION.md#71445-tools离线数据组件计时与关闭回归2026-10-24)。截图及构建日志在[离线工具验证](../build-ui/offline-tools-20261024/README.md)，功能规则见[离线工具](WAVEFORM_VIEWER.md#其他离线数据工具)。未进行真实板卡、真实ROOT/FITS转换或跨平台验收；GUI互锁不等于ECC服务端跨客户端锁。完整重构继续。

## 71.444 自动Run、日志级别与运行中Enable

新增时间/raw大小单条件或OR触发、停止/循环、序列轮数和成功Complete后至少3秒的就绪检查。策略、GUI、Native统计观察与调度拆为独立单元，沿用既有封存/归档；日志警告橙色、错误红色且保留原文。Automatic Run上移至Run setup顶部，Apply/Enable同行，双条件进度整行换行，Experiment行上移。运行中阈值与Apply锁定，Enable可独立开关已保存策略，不停止当前采集。

最终Widgets **163/163**、Native控制 **42/42**通过；真实本地receiver字节观察和协议回归通过，生产目标重建，宽/窄窗口实际Qt截图检查通过。Native大小条件需要更新ECC/supervisor/receiver，未部署现场或验实板/目标平台。详见[验证71.444](VALIDATION.md#71444-自动run诊断着色与运行中enable开关2026-10-24)、[使用规则](AUTOMATIC_RUN.md)及[截图与日志](../build-ui/automatic-run-20261024/README.md)。完整重构继续。

## 71.443 Qt第二轮视觉精修与动态节点区收缩

统一输入框、按钮、字体与禁用态，节点24 px行高；修复小窗口两行按钮裁切、步进/下拉箭头缺失及减少节点后下方面板不随之上移。单节点控制连续排列，底部曲线/日志默认各半并支持拖动分隔线；原状态口径、采样和后台操作保持。

生产Envoy构建及完整Widgets **153/153通过**，1536×1024、1280×800和1.5倍DPI实际Qt离屏截图检查通过。小窗口保留局部滚动；实板、现场显示器和Windows/macOS未验收。详见[验证71.443](VALIDATION.md#71443-qt第二轮视觉精修与动态节点区收缩2026-10-24)和[文件及截图清单](../build-ui/run-control-polish-20261024/README.md)。本轮为GUI精修，完整项目重构继续。

## 71.442 下方曲线/日志等宽并排

速率曲线左、Event log右，各占一半；CoBo就绪数改为单个数量，移除Disk max总览。节点表磁盘字段、后台采样和控制流程保持。生产Envoy构建、定向Qt回归 **8/8** 及1536×1024/1280×800实际Simulation截图检查通过；全项目重构与平台/现场验收继续。详见[验证71.442](VALIDATION.md#71442-速率曲线与日志左右等宽及总览简化2026-10-24)。

## 71.441 参考图浅色Qt控制台与真实状态总览

Run setup 合并配置与运行细节，8个操作宽屏单行/窄屏两行，参数保留右上、设备表与曲线/日志全宽。独立节点灯列、真实 CoBo 总览、最大磁盘占用口径及节点选择框接入原状态与选择绑定；顶栏不再有独立灯点。QSS/图标/摘要和布局拆到展示模块，既有操作、协议、采样、确认与线程行为保持。

最终完整 Widgets **152/152通过**，专项 **7/7通过**；生产 Envoy 构建及1536×1024、1280×800实际 Simulation截图检查通过，1.5倍高DPI离屏启动通过。Native缺失的速率/磁盘仍未知，未伪造图表数据。未验实板、现场receiver、Windows/macOS和实体显示器。详见[验证71.441](VALIDATION.md#71441-参考图驱动的qt浅色run控制台重构2026-10-24)及[界面调整说明](RUN_CONTROL_UI.md)。

## 71.440 CoBo准备与采集共用持久状态日志

逐 CoBo 状态在 Prepare 开始、首个硬件步骤前绑定独占持久日志，准备、维护及采集连续写入；校验固定拓扑、scope、状态转移、文件身份和审计链。正向动作遇日志故障即拒绝，目标 UNKNOWN 仍可见；明确生产停止可以沿独立停止审计执行，并保持 UNKNOWN 和不重发约束。历史读取不会恢复控制权或清除 Native 重启 UNKNOWN。

硬件/Native 两批共 **12/12组回归通过**，包括正常两编码、部分写入、进程中断、日志损坏后的 Start 拒绝和停止不重发。完整旧版逐板 FSM、历史与 ECC 恢复证据的集成、跨服务恢复及现场/平台验收继续推进。详见[验证71.440](VALIDATION.md#71440-cobo连续状态持久记录与日志故障停止回归2026-10-24)和[持久记录说明](COBO_LIFECYCLE_JOURNAL.md)。

## 71.439 Native结果分派与Run/扫描收尾职责拆分

公共操作回调只负责持久化观察、原操作关联及公共错误门槛；catalog续作、扫描、CoBo维护、计划退役、恢复及普通生命周期结果由各自工作流方法处理。普通Run的新鲜状态收尾和扫描封存证据推进分别归属生命周期与扫描单元，扫描deadline/未知结果记录也集中在扫描单元；ScanState构造/析构移出公共编译单元，使公共控制代码不再依赖扫描协调器实现头文件。

新增维护HTTP回归覆盖七种命令、失败/异常状态/丢失flip应答，以及重连只读观察旧flip意图；完整Native控制集成 **38/38通过**，控制器版完整Widgets **150/150通过**，生产Envoy构建成功。随后连接状态灯收进Phase徽标右端，GUI布局专项 **5/5通过**并检查生产离屏截图。工作流仍共享NativeRunControl会话和异步状态；该拆分不代表逐板持久FSM、跨服务恢复或全套平台交付完成。详见[验证71.439](VALIDATION.md#71439-native操作结果按工作流分流及连接状态徽标整理2026-10-24)。

## 71.438 Envoy白底控制台菜单定位与Native计划转换职责拆分

控制台采用白色卡片、中性浅灰画布和青绿色主操作色；Configuration、Tools、Run、Recovery菜单成组置于标题和右侧状态标识之间。顶栏连接状态灯依连接、忙碌及故障状态显示绿、琥珀、红色，并保留文字说明。配置操作横向等宽排列，Run细节保持单行；采集区按钮分成两行并留出间距，卡片高度避免按钮裁切。Connect/Run start绿色、Run stop红色，Regress/Progress以低饱和红绿区分方向。参数表继续位于右上方，下方设备状态、曲线和日志全宽排列；状态行高为20 px。NativeRunControl中的四个计划转换/退役操作已迁至`nativeruncontrol_plantransition.cpp`，原状态门槛和异步行为保持不变。

Linux Qt 5.12.8/GCC 9.3/C++11完整Qt Widgets QtTest **150 passed、0 failed、0 skipped**（59.784秒），生产Envoy构建成功，GUI专项测试验证连接灯状态及按钮布局，1500×1000离屏截图确认最终浅色主题和居中菜单；Markdown本地链接检查1898项通过。没有实板、真实receiver或现场显示验收。全套Native状态机重构继续进行。详见[验证71.438](VALIDATION.md#71438-envoy白底控制台菜单定位与native计划转换职责拆分2026-10-24)。

## 71.437 Envoy色彩与设备表留白修整

控制台统一为中性炭灰色，曲线网格与状态文字同步配色；参数表隐藏行号并缩窄参数列以留下更多值列宽度。设备状态表按配置节点行数计算高度并关闭无用的纵向滚动区，状态操作条紧跟表格，空出的下方面积留给全宽曲线和日志。Qt 5.12.8/GCC 9.3/C++11完整Widgets回归 **150/150通过**，生产Envoy构建成功；未连接实板或现场显示器验收。全套控制台及Native重构仍在继续。详见[验证71.437](VALIDATION.md)。

## 71.436 Envoy控制台布局收紧与Native单CoBo维护单元拆分

界面改为深色工程控制台配色，压缩顶部卡片、删除重复副标题及日志面板提示；连接/阶段操作与Run/恢复操作分两行，参数行高固定22 px以展示更多项目。上层左侧继续容纳配置与控制、右侧为Run记录参数表；设备/磁盘状态、曲线和操作日志仍横跨下方全宽。`NativeRunControl`的单CoBo维护命令实现移入`nativeruncontrol_maintenance.cpp`，异步状态机和原准入/操作语义不变。

Qt 5.12.8/GCC 9.3/C++11完整Widgets回归 **148/148通过**，生产Envoy目标构建成功；无实板/现场显示验收。逐CoBo持久FSM及跨服务恢复等缺口仍未闭合，整套重构继续。详见[验证71.436](VALIDATION.md#71436-envoy控制台布局收紧与native单cobo维护单元拆分2026-10-24)。

## 71.434 Native计划退役状态归组

原始退役请求、receipt查询基准和退役/receipt等待标志现由单一`PlanRetirementState`持有，Connect通过一个`Reset()`重置会话状态。所有退役提交、原请求绑定和状态读回后receipt查询时序保持原样。完整Widgets QtTest **147/147通过**，生产Envoy构建成功。

这是状态所有权归组，未把计划退役I/O或工作流移出控制器；Native整体协调及跨服务恢复仍继续。详见[验证71.434](VALIDATION.md#71434-native计划退役状态生命周期归组2026-10-24)。

## 71.433 Native CoBo维护准入策略集中

七个CoBo维护入口共用纯策略验证健康Prepared及ECC instance/snapshot身份；异步回调复用同一维护命令分类白名单。各按钮原有提示、参数检查和客户端调用保持在控制器中。完整Widgets QtTest **147/147通过**，生产Envoy构建成功。

这统一的是本地准入与结果路由规则，不增加硬件权限或改变CoBo命令行为；Native控制器整体重构及跨服务恢复仍继续。详见[验证71.433](VALIDATION.md#71433-native-cobo维护准入与命令分类策略独立化2026-10-24)。

## 71.432 Native告警观察缓存生命周期独立化

将告警响应解码、最近成功值、原始响应和freshness计时器归入`NativeStatusObservations`；控制器仍负责日志/journal副作用、恢复资格和命令时序。传输错误使观察过期，坏JSON保留上次成功值但不更新时钟。完整Widgets QtTest **146/146通过**，生产Envoy构建成功。

这是状态观察缓存边界拆分，不表示Native恢复编排、跨服务重启恢复、Windows/macOS或现场验收完成；整体重构继续。详见[验证71.432](VALIDATION.md#71432-native告警观察缓存和freshness生命周期归属拆分2026-10-24)。

## 71.431 Native告警恢复身份判定拆分

UNKNOWN acquisition/preparation确认与已知stopped告警释放共用只读告警operation匹配策略；两种模式保留不同phase、UNKNOWN和operation绑定规则，控制器继续负责freshness与命令时序。完整Widgets QtTest **146/146通过**，Envoy独立构建成功；Native恢复编排继续拆分。详见[验证71.431](VALIDATION.md#71431-native告警恢复operation匹配策略独立化2026-10-24)。

## 71.430 Native catalog续作资格策略拆分

失败catalog续作的operation绑定、replacement意图、新鲜Prepared状态、reconciliation hold及存储/client可续作条件现集中由只读策略判定；控制器仍负责本地查询和原续作提交。完整Widgets回归 **145/145通过**，生产Envoy独立构建成功；重构没有启用自动重试，Native整体恢复编排与跨服务重启恢复继续进行。详见[验证71.430](VALIDATION.md#71430-native失败catalog续作资格策略独立化2026-10-24)。

## 71.429 Native恢复运行证据校验独立化

将恢复所需的当前ECC实例/snapshot和receiver session、PID、输出目录及覆盖/阶段核验提取为只读策略，NativeRunControl继续负责journal、配置观察和恢复编排。完整Widgets回归 **144/144通过**，生产Envoy干净构建成功；无真实硬件、receiver或DAQ raw验证。恢复协调整体及跨服务重启证据重建仍在重构。详见[验证71.429](VALIDATION.md#71429-native恢复运行证据策略独立化2026-10-24)。

## 71.428 Native Run恢复快照比较提取

Native Run恢复入口现在调用独立只读策略核对保存Run与当前完整Native部署快照，精确比较Run、目录、ECC身份摘要及固定节点顺序/ID绑定；恢复流程仍执行journal、当前ECC实例、配置观察和receiver状态门槛。新增策略回归和完整Widgets测试 **143/143通过**，生产Envoy构建成功。

这只拆出恢复身份校验；恢复编排和服务重启后的证据重建仍在继续。详见[验证71.428](VALIDATION.md#71428-native恢复run快照身份策略独立化2026-10-24)。

## 71.427 ScanState资源释放生命周期归组

Scan身份/event-baseline协调器与存储、运行快照、扫描operation runner、Run journal store及事件monitor的资源释放已由`ScanState::ReleaseResources()`统一管理，Disconnect和Shutdown共用。Shutdown先持久化Interrupted Run记录，再释放扫描资源并关闭客户端。完整Widgets QtTest **142/142通过**，生产Envoy构建成功。

当前收拢的是资源释放入口，Native连接/扫描状态机及恢复编排尚未独立；整体重构继续。详见[验证71.427](VALIDATION.md#71427-scanstate资源释放入口集中化2026-10-24)。

## 71.426 Native Run持久化顺序由RunRecorder执行

Native Run持久化入口现统一管理引用文件、Run CSV、Complete journal和replacement backup清理顺序；`NativeRunControl`保留证据门槛和内存/GUI状态推进。包含durable结果验证的完整Widgets回归 **142/142通过**，生产Envoy构建成功。

扫描完成门槛和异步操作仍由控制器协调；该方法统一的是完成后的持久化阶段，整体Native重构继续。详见[验证71.426](VALIDATION.md#71426-native-run完成持久化顺序移入runrecorder2026-10-24)。

## 71.425 Run引用持久化移入RunRecorder

Native完成路径的receiver状态、ECC身份、可选配置和catalog引用原子写入已移入`RunRecorder::WriteNativeReferences()`；`NativeRunControl`继续负责证据门槛及完整Run/journal顺序。新增RunRecorder文件内容/可选分支测试通过，完整Widgets套件 **141/141通过**，生产Envoy构建成功。

收尾动作仍由控制器编排，后续继续隔离异步Run终结协调。详见[验证71.425](VALIDATION.md#71425-native-run引用文件写入归属runrecorder2026-10-24)。

## 71.424 Native receiver最终证据规则集中化

Run完成、扫描封存/归档以及data-plan退役共用只读`nativereceiverfinalizationpolicy`，统一检查receiver覆盖、结束/封存证据、catalog文件数与reconciliation门槛；控制器继续负责I/O和命令时序。新增策略回归及完整Widgets套件 **140/140通过**，生产Envoy构建成功。

此前主控制器中存在重复finalization检查；现已集中，但NativeRunControl仍负责总体异步控制/恢复协调，后续继续拆分。详见[验证71.424](VALIDATION.md#71424-native接收器封存与归档门槛策略独立化2026-10-24)。

## 71.423 Native恢复状态从通用字段中归组

把“正在等待恢复所需元数据”的门控字段归入专属`RecoveryState`，恢复相关连接重置、失败处理和busy投影统一读取该状态。Qt完整Widgets测试 **139/139通过**，生产Envoy从当前源码重建成功。

当前只是恢复状态所有权归组，恢复校验和异步编排仍在`NativeRunControl`成员中；后续继续拆出恢复策略/协调职责，并保持原有失败关闭行为。跨服务重启恢复、Windows/macOS和现场验收仍未完成。详见[验证71.423](VALIDATION.md#71423-native恢复等待状态归组2026-10-24)。

## 71.422 采集按钮语义和参数面板布局

已修正控制区：整机组和节点组保留Progress/Regress名称，并在各自组内调整左右位置；只有正式采集启停按钮命名为Run start/Run stop，Run start在左。参数与操作日志面板初始按1:2分配宽度。当前Linux完整Widgets回归139/139通过，生产Envoy构建成功。

该项仅调整界面布局和文案，不改变控制动作；Windows、macOS及现场显示效果仍需验收。完整Native控制器重构与各平台/现场工作继续进行。详见[验证71.422](VALIDATION.md#71422-采集控制按钮区分与底部布局修正2026-10-24)。

## 71.421 Native恢复入口编译边界

Native Run恢复、失败catalog续作及UNKNOWN故障清理/对账入口已收拢到`nativeruncontrol_recovery.cpp`，主控制器文件减少246行。现有完整Widgets测试139/139通过，生产Envoy构建成功。

目前仍是同一控制器共享状态上的成员方法拆分；下一步继续把恢复前置条件、持久恢复状态与异步执行协调从`NativeRunControl`中分离，并维持失败关闭、原operation ID和现有reconciliation证据规则。跨ECC服务重启恢复及各平台/现场验证仍未完成。详见[验证71.421](VALIDATION.md#71421-native恢复入口独立编译单元2026-10-24)。

## 71.420 Native整机状态行投影独立化（2026-10-24）

将`NativeRunControl::Status()`中从已验证ECC整机快照构造固定CoBo GUI行的投影抽至`BuildNativeSystemStatusRows()`，保留source-agent归属检查、运行态receiver全集门槛及UNKNOWN呈现。完整QtTest **139/139通过**，生产Envoy目标独立重建成功。详见[验证71.420](VALIDATION.md#71420-native整机状态到cobo行模型投影拆分2026-10-24)。

Status响应代际处理、缓存更新、identity/操作推进和告警查询仍属于控制器；Native整体编排职责、跨服务重启恢复、各目标平台和实板验收均继续进行。

## 71.419 Native状态观察缓存职责拆分（2026-10-24）

将Native receiver/source/receiver-failure响应的解码、集合校验、缓存、新鲜度、错误去重和重置规则移入独立`NativeStatusObservations`；`NativeRunControl`保留告警日志、Run journal副作用及控制决策，扫描逻辑读取统一观察对象。完整Qt Widgets回归 **138/138通过**，生产Envoy目标从当前源码重建成功。详见[验证71.419](VALIDATION.md#71419-native状态观察缓存生命周期独立化2026-10-24)。

这是Native控制器继续分层的一步，不代表控制器编排、跨服务重启恢复、Windows/macOS或实板验收完成；整体重构继续。

## 71.418 Native硬件观察到GUI状态行的投影独立化（2026-10-24）

将Native receiver进程、数据源及接收失败留存证据投影到Envoy状态行的逻辑移出`NativeRunControl::EmitStatus()`，放入独立只读投影模块。控制器只计算并传入各观察的新鲜度、错误和当前快照，再发出投影后的行模型；投影器不提交控制命令或修改观察。新增回归覆盖新鲜且完整匹配的receiver、PID/session不符以及receiver观察过期。

Qt 5.12.8/GCC 9.3/C++11生产Envoy及完整Widgets测试工程均重建成功；新增用例单独通过，完整套件 **137/137通过**。该模块化没有改变数据接收或控制流程；Native控制器职责、逐CoBo跨服务重启恢复及目标平台/现场验证仍需继续。详见[验证71.418](VALIDATION.md#71418-native硬件观察到gui状态行投影独立化2026-10-24)。

## 71.435 Envoy运行台布局调整（2026-10-24）

上半屏左侧容纳标题、配置和Run控制，右侧显示记录参数表；下半屏将设备/磁盘状态、曲线和操作日志横跨全宽。整机及单CoBo控制保持Regress在左、Progress在右；紧凑化Run控制，并把日志导出放到Tools > Logs。当前源码完整Qt回归148/148通过，正式Envoy目标构建成功；无实板/现场验收。完整布局断言和验证范围见[验证71.435](VALIDATION.md#71435-envoy运行台布局与完整qt回归2026-10-24)。逐CoBo持久FSM等结构性缺口仍未闭合，整套重构继续。

## 71.417 Native扫描事件样本绑定逻辑独立化（2026-10-24）

将receiver事件样本解码后与冻结部署身份、Fresh系统Running状态、receiver根观察、目标CoBo生命周期及实际运行PID的交叉验证提取为纯控制协议函数。Native控制器保留异步调度、事件计数决策和Stop/刷新动作；绑定函数本身不做I/O或控制操作。增加CMake独立测试，覆盖成功绑定及过期观察、错误发送者、错误系统身份/状态、receiver部署绑定不符和PID不符拒绝。

Qt 5.12.8/GCC 9.3/C++11完整Widgets测试工程重建成功；在允许localhost mock服务的环境下完整回归 **136/136通过**，其中事件阈值扫描成功Stop及receiver/catalog/journal收尾通过。独立CMake绑定测试通过。该模块只强化扫描事件证据绑定，不改变扫描动作顺序；跨服务重启恢复、各平台构建和实板验证仍待完成。详见[验证71.417](VALIDATION.md#71417-native扫描事件样本绑定逻辑独立化2026-10-24)。

## 71.416 Native扫描状态集中管理（2026-10-24）

在扫描运行实现独立编译后，将其journal、identity、事件监控、点级策略和进行中操作证据收拢至`NativeRunControl`私有的`ScanState`，减少主控制器中的散落状态字段；析构放在扫描实现单元以正确释放不完整类型。完整Qt Widgets回归从当前源码重建后136/136通过，扫描Run事件目标与receiver/catalog收尾链通过。

目前完成的是状态归组，NativeRunControl仍承担整个调度协调器职责；专属扫描协调器、服务重启后的恢复及Windows/macOS/现场验证仍未完成。详见[验证71.416](VALIDATION.md#71416-native扫描运行状态集中收拢2026-10-24)。

## 71.415 Native扫描控制实现独立编译单元（2026-10-24）

将Native控制器中约680行扫描运行编排实现及其安全扫描文件读取辅助函数拆至独立`nativeruncontrol_scan.cpp`，并登记到共享qmake工程；状态对象和公开控制接口暂时保持一致。完整Qt测试工程重建成功，扫描点事件达标后Stop、receiver/catalog收尾的localhost元数据集成用例通过。

这是结构拆分，未改变职责边界或故障恢复语义。更完整的Native控制器分层、跨服务恢复、独立持久逐CoBo状态及各目标平台/现场验收仍待完成，整体重构继续。详见[验证71.415](VALIDATION.md#71415-native扫描控制实现拆分2026-10-24)。

## 71.414 新采集客户端不再清除未知CoBo状态（2026-10-24）

新采集协调器现在只校验共享生命周期账本是否为活动板`Configured`、禁用板`Disabled`，不再把所有活动板静默重置为Configured。若上次硬件命令结果为`Unknown`或板卡仍处于其他阶段，新Run会被拒绝，避免掩盖未确认硬件结果。状态机单测、两种ICE编码的完整采集模拟回归和Standalone/禁用主触发回归通过；无实板验收。详见[验证71.414](VALIDATION.md#71414-新采集客户端保留未知cobo状态2026-10-24)。

## 71.413 Prepare与Run共用逐CoBo状态账本（2026-10-24）

准备协调器现在持有一份固定CoBo ID/顺序的进程内生命周期账本；采集协调器读取并更新同一实例，准备和采集的状态JSON视图保持实时一致。账本集中检查节点索引、启用边界和状态转移，消除了两份数组之间的同步拷贝；新采集协调器创建时要求该账本已经确认处于`configured/disabled`，不再重置状态。Breakup仍将共享状态变化镜像写入Run审计。它尚未持久化，也无法在ECC或GUI进程重启后恢复逐板状态；硬件读回、独立进程恢复以及现场验收仍待完成。验证结果和范围见[验证71.413](VALIDATION.md#71413-preparerun共用固定拓扑cobo状态账本2026-10-24)。

## 71.412 准备与采集阶段统一CoBo FSM（2026-10-24）

逐板Prepare/Configure与采集运行阶段现在共享一个强类型生命周期及转移校验核心，Connect、Run、Pause/Resume、Stop、Disconnect和Breakup状态镜像均通过校验，不再维护第二套字符串转移表。两种ICE编码采集回归、Breakup正常及部分失败一致性回归通过。详见[验证71.412](VALIDATION.md#71412-prepareconfigure与run共享强类型cobo生命周期fsm2026-10-24)。

逐CoBo状态仍由存活协调器对象持有；独立持久状态账本、服务重启安全恢复、旧版完整节点失败语义迁移及现场硬件验收仍待完成。

## 71.411 CoBo生命周期转移核心抽取（2026-10-24）

CoBo生命周期枚举、状态名称、初态和转移矩阵已拆至独立硬件模块；系统准备对象更新逐板状态前会验证固定配置的启用/禁用边界和合法转移，并保留原生命周期审计格式。完整准备两种ICE编码、正常释放和断电故障mock路径通过。验证范围和限制见[验证71.411](VALIDATION.md#71411-cobo生命周期状态机核心与转移门控2026-10-24)。

该转移核心目前仍由原系统准备对象持有逐板状态；Native逐CoBo FSM独立持久化、ECC服务重启恢复、旧版单节点失败语义迁移及现场验收仍未完成，整套重构继续。

## 71.410 CoBo生命周期更新集中入口（2026-10-24）

系统准备对象现在通过唯一入口更新逐CoBo阶段并同步写审计，验证节点索引及活动/禁用配置边界。Prepare/Configure、单板维护、配置失效、正常释放和失败核验均已接入。GCC/C++11 mock准备两种ICE编码及释放正常/断电故障回归通过；无实板、receiver或raw。详见[验证71.410](VALIDATION.md#71410-cobo生命周期更新与审计集中化2026-10-24)。

## 71.409 Native CoBo列表状态投影单次日志扫描（2026-10-24）

维护目标列表现在批量执行逐板状态投影：一次建目标索引并扫描操作日志，避免按每个CoBo重复扫描；单板命令预检保留单目标接口。Linux GCC/C++11定向等价性回归及ECC server/select/handoff下游构建成功。状态来源和未完成的独立持久FSM边界不变。详见[验证71.409](VALIDATION.md#71409-native维护目标列表批量cobo投影2026-10-24)。

## 71.408 Native逐CoBo生命周期投影模块化（2026-10-24）

将ECC服务中的逐CoBo生命周期投影拆成独立纯函数，增加多板状态隔离、维护状态保留、单板转换中、失败及控制器reconciliation用例。Linux GCC/C++11定向构建和测试通过。它仍是控制器/journal历史投影，不是独立持久CoBo状态机或硬件读回；详见[验证71.408](VALIDATION.md#71408-逐cobo生命周期投影纯函数提取2026-10-24)。

## 71.407 当前源码Linux full交付构建复验（2026-10-24）

当前工作树Linux `full` profile重建29/29程序成功，109项嵌入依赖检查通过；独立暂存包192文件校验通过，29/29程序帮助入口启动smoke通过（无硬件访问、无raw payload）。构建和启动检查使用Ubuntu/WSL Linux用户空间，不等于独立Ubuntu24、Windows Qt5.14.2、macOS、双机、实板或DAQ吞吐验收。详见[验证71.407](VALIDATION.md#71407-当前源码linux-full交付构建与程序启动复验2026-10-24)。逐CoBo持久FSM及Native Run替换跨服务重启授权恢复仍是主要结构性缺口，整套重构继续。

## 71.406 当前源码完整Qt Widgets回归复验（2026-10-24）

从当前工作树重建完整Qt Widgets测试目标，允许本机loopback后 **136/136通过**，覆盖GUI配置编辑/Run号回退与按钮左右顺序、legacy和Native状态机、单点扫描成功收尾、连接操作日志及可见日志导出。严格受限sandbox禁止本机测试服务监听，故首次测试失败不作为代码结论；放行后同一套测试全通过。当前源码仍有若干`-Wmisleading-indentation`编译警告。详见[验证71.406](VALIDATION.md#71406-当前源码完整qt-widgets回归复验2026-10-24)。

这只刷新Qt 5.12.8/GCC 9.3/Linux用户空间回归证据；Windows Qt 5.14.2、macOS、独立Ubuntu24、真实双机/CoBo、receiver raw和持续吞吐仍未验收。逐CoBo独立持久FSM及Native Run替换服务重启后恢复仍是未完成的结构性工作；整套重构继续进行。

## 71.405 Native扫描与Run元数据控制路径复验（2026-10-24）

当前完整Widgets QtTest工程从工作树重建成功，并复验三条关键路径：连接后Run号/说明/参数可编辑、Run启动后锁定；Native扫描点成功链在事件数到达后Stop并完成Breakup、receiver finish/seal、catalog和Run journal；Threshold扫描仍保持手动，自动Run按钮与执行策略入口禁用。该验证加强现有路径证据，不代表多点自动配置/推进或实板采集已完成。

Qt 5.12.8/GCC9.3/C++11三项定向QtTest均退出码0；扫描UI辅助`getdaq-config`及`getdaq-config-check`来自既有`bin/`，本轮没有重建这些CLI。测试只用小型fake服务元数据，无实板、receiver进程或raw。详见[验证71.405](VALIDATION.md#71405-native扫描与run元数据控制路径复验2026-10-24)。

## 71.404 操作日志自动留存与界面导出（2026-10-24）

主界面会在每次成功连接后建立独立会话日志，断开时写入结束标记并关闭；日志保存于用户数据目录的`logs/connections`，不可写时回退缓存目录或进程临时目录。右下日志面板新增“Export visible log…”按钮，原子导出当前面板显示的内容；窗口仅保留最近2000行，完整连接记录以自动生成的会话文件为准。

Qt 5.12.8/GCC9.3/C++11完整Widgets测试工程当前源码构建成功；新增可见日志导出用例和每次连接生成独立日志文件回归均通过（各含init/test/cleanup共3/3）。未验证Windows Qt5.14.2、macOS或现场部署。详见[验证71.404](VALIDATION.md#71404-操作日志自动留存与界面导出2026-10-24)。

## 71.403 Configure前置条件失败保留逐CoBo阶段（2026-10-24）

新增快速的`--lifecycle-preflight` Native硬件准备回归：在Configure元数据校验后、任何板级配置写入前，让C1的生产停止观察失败；父准备协调器进入Failed，硬件写入增量为0，活动C0/C1保持Prepared，禁用C2保持Disabled；并逐项确认C0/C1的firmwareEnable与intEnable未变化、C0/C1/C2的initDone仍为0。此时没有启动板级Configuring阶段；与硬件命令已发出后需标Unknown的故障路径区分。

Linux GCC/C++11 `hardware-system-preparation-tests`重建成功；localhost ICE专项退出码0。该测试只用模拟板，不是实板验证。详见[验证71.403](VALIDATION.md#71403-configure前置条件失败保留逐cobo阶段2026-10-24)。

## 71.402 Native CoBo生命周期状态改为强类型（2026-10-24）

`SystemPreparationClient`内部逐板阶段由自由字符串改为封闭的`CoBoLifecycle`枚举（Fresh/Preparing/Prepared/Configuring/Configured/Unknown/Disabled），审计JSON、观察接口和采集协调器继续使用原有文本值，外部协议不变。避免内部拼写漂移；该改动不扩大旧版逐Node直接调用的适用状态，也不声称已实现独立持久CoBo FSM。

Linux GCC/C++11硬件准备、释放及Native协调器测试目标增量构建成功。localhost ICE `failure-unprepare-0`、`failure-unprepare-power-loss`及释放`fault-2`均以退出码0通过，覆盖Fresh/Unknown结果；完整`--maintenance`在300秒限时内分别报告ICE 1.0/1.1基础重准备和单板Configure PASS，但后续故障序列未完成、命令超时，因此不算整套通过；`normal-0`在90秒限时内未完成。无实板、receiver或raw。详见[验证71.402](VALIDATION.md#71402-native-cobo生命周期阶段强类型化2026-10-24)。

## 71.401 Native Unprepare终态逐CoBo更新（2026-10-24）

成功完成供电关闭及自有告警注销后，准备对象的活动CoBo阶段现在落到`fresh`，禁用板仍为`disabled`。普通Unprepare及Failed清理路径中任何已触达板的断电/监控/注销结果不确定时，该板被保守标成`unknown`并保留失败门控；Failed清理后的只读核验成功确认Fresh，核验失败撤回该状态。

Linux GCC/C++11 `hardware-system-release-tests`构建成功；localhost ICE专项`normal-0`覆盖Breakup后Unprepare、重新Prepare/配置循环及状态回退通过，`fault-2`注入CoBo断电应答失败并验证已触达板UNKNOWN及不自动重试。没有真实硬件、receiver或raw。详见[验证71.401](VALIDATION.md#71401-native-unprepare终态逐cobo更新2026-10-24)。

## 71.400 Native Breakup配置失效同步逐CoBo生命周期（2026-10-24）

同一采集对象Breakup在全部sender断开并成功清除CoBo `initDone`/`mutantMode`后，准备协调器逐板状态现在回到`prepared`，采集协调器同步该快照；配置失效发生部分硬件写入后失败时，所有已触达活动板变为`unknown`，两个协调器都保留该结果，未触达/禁用板仍保持原状态。配置失效审计记录结束后再提交生命周期转移；中途失败时不重放写入。

Linux GCC/C++11 `broken-acquisition-stop-tests`重建成功。两个独立localhost ICE mock专项分别验证Breakup成功回到Prepared和C1写入应答丢失后C0/C1为UNKNOWN、C2为disabled，准备/采集两份快照一致。没有真实硬件、receiver或raw。详见[验证71.400](VALIDATION.md#71400-native-breakup配置失效同步逐cobo生命周期2026-10-24)。

## 71.399 Native单CoBo维护命令失败时保守标记状态（2026-10-24）

单CoBo LED Set/Blink/Flip/Pulse和AsAd Pulser命令在进入远程硬件调用后若失败，现只将目标CoBo生命周期标记为`unknown`，父级协调器进入Failed且拒绝自动重试；未触达硬件的Pulser前置失败仍保留Prepared。逐板状态审计尽力记录，审计本身失败不会覆盖内存中的UNKNOWN状态。

Linux GCC/C++11 `hardware-system-preparation-tests`重建成功。获准本机ICE loopback后，维护专项11个场景通过，其中新增5类命令在模拟“远端已执行但应答丢失”时逐一核对目标C0为unknown、其他活动C1仍prepared、禁用C2仍disabled且重复调用不再访问硬件。只用模拟节点，无实板、真实receiver或raw；文档证据见[验证71.399](VALIDATION.md#71399-native单cobo维护失败状态保守投影2026-10-24)。

## 71.398 Native逐CoBo阶段来源精确标注（2026-10-24）

硬件状态观察的`cobo_lifecycle_source`现在区分`live-controller-cache`（存活prepare/acquisition对象）、`controller-baseline`（无对象缓存时根据控制器reconciliation基线生成fresh/unknown/disabled）和`unavailable`（忙碌或观察期间状态变化，逐板数组为空）。Qt-free严格解码器校验来源值与busy/consistent/生命周期数组匹配；Envoy窗口也按来源说明逐板阶段证据，避免将冷启动UNKNOWN误称为存活缓存。所有来源都明确不是板端读回。

Qt5.12.8/GCC9.3/C++11完整Widgets测试目标重建成功，协议、来源标签、UNKNOWN提示及过期清除定向QtTest为5/5；Native C++11目标重建成功，`normal-0`/`normal-1`两种ICE编码生命周期回归通过。默认沙箱禁止loopback监听，允许本机mock后测试通过；测试用mock硬件和数据生命周期替身，无实板、receiver或DAQ raw。详见[验证71.398](VALIDATION.md#71398-native逐cobo状态来源标注2026-10-24)。独立持久CoBo FSM和服务重启恢复仍未实现。

## 71.397 Native ECC服务重启后的逐CoBo未知状态投影（2026-10-24）

修正只读硬件状态观察的冷启动回退：协调器没有当前prepare/acquisition缓存且控制器仍处于`needsReconciliation`或`outcomeUnknown`时，启用CoBo投影为`unknown`，禁用CoBo仍为`disabled`；只有没有未知/恢复门控的新鲜控制器才投影`fresh`。这避免服务重启后把未验证的硬件状态误报为fresh。新增正常流程重启断言，确认UNKNOWN投影不写硬件且不清除reconciliation门控。

Linux GCC/C++11重建Native协调器测试目标成功；`normal-0`和`normal-1`两种ICE编码端到端回归通过。只读观察仍是控制器缓存投影，不是硬件读回；本变更不恢复持久CoBo FSM，也不解除冷启动门控。测试使用mock硬件与数据生命周期替身，无真实receiver、板卡或raw。见[验证71.397](VALIDATION.md#71397-native服务重启后的逐cobo-unknown投影2026-10-24)。

## 71.396 Native逐CoBo运行阶段观察端到端回归（2026-10-24）

扩展Native ECC正常生命周期集成场景：每次全局命令完成后，经只读硬件观察接口逐块核对CoBo阶段，覆盖Describe后的fresh、Prepare后的prepared、Configure/Connect后的ready、Run start后的running、Pause/Resume往返、Stop后的stopped、Breakup回到prepared、再次Configure及Reset后fresh；禁用CoBo始终为disabled。每次查询同时断言生命周期来源为存活控制器缓存、明确不是硬件读回，且不增加硬件写入或改变ECC operation ID。

Linux GCC/C++11重建Native ECC协调器测试目标成功；`normal-0`和`normal-1`两种ICE编码的完整mock硬件生命周期均通过。数据生命周期使用显式测试替身，无真实receiver/seal、板卡或DAQ raw。该回归验证当前对象内逐板运行状态与对外观察一致，不实现服务重启后的独立持久CoBo FSM；冷启动仍须UNKNOWN并待后续恢复设计。详见[验证71.396](VALIDATION.md#71396-native逐cobo运行阶段观察端到端回归2026-10-24)。

## 71.395 legacy逐节点推进/回退顺序对照测试（2026-10-24）

扩展legacy `RunControl` QtTest，逐节点验证CoBo与MuTaNT状态彼此独立：MuTaNT仍在Prepared时，CoBo尚未Configure则拒绝推进MuTaNT；CoBo进入Ready后才允许MuTaNT Configure；随后分别回退节点并核对状态不串改。与旧 `BackEnd::prepareNode/configureNode` 直接转发到选定Node及系统级CoBo/MuTaNT顺序相符。该测试保护既有legacy行为，不能证明Native多板有独立CoBo FSM。

Qt5.12.8/GCC9.3/C++11从当前源码重建完整Widgets测试目标成功；定向QtTest 3/3（目标用例加init/cleanup）通过，需本机loopback mock。独立单板持久FSM及服务重启恢复仍未实现。见[验证71.395](VALIDATION.md#71395-legacy逐节点状态推进回退顺序对照2026-10-24)。

## 71.394 MCP逐CoBo操作凭据可追踪到只读操作查询（2026-10-24）

`ecc_cobo_lifecycles`返回的最近单板`operation_id`现在可直接交给`ecc_operation`查询同一操作的阶段与结果。该操作查询明确标注只读，说明不重试、不改变硬件状态；只读MCP配置下，生命周期投影和精确操作查询都保持可用。stdio集成回归验证CoBo投影中的ID与查询结果一致，并验证只读模式行为。

Qt5.12 standalone CMake/C++11构建通过；stdio回归在允许loopback的环境1/1通过；Python语法检查和Markdown链接检查通过。测试使用假ECC HTTP服务，不证明真实服务或实板状态；逐板journal投影和冷启动UNKNOWN边界保持不变。见[验证71.394](VALIDATION.md#71394-mcp逐cobo操作凭据关联只读查询2026-10-24)。

## 71.389 修正Native单CoBo Configure隔离回归的历史计数误判

单CoBo Configure隔离测试原先要求禁用CoBo的累计写入数绝对为0，混入了此前准备流程的历史计数。改为比较Configure operation前后的各板计数：目标板必须增加，另一活动板与禁用板必须保持不变。重建Native ECC coordinator测试并在允许localhost Ice/TCP的环境重跑`maintenance-0`，1/1通过；审计确认操作命令17完成，父ECC仍为Prepared。仅为测试断言修正，没有改硬件执行逻辑；测试使用合成硬件及数据生命周期替身，不代表实板验收。验证见[71.389](VALIDATION.md#71389-native单cobo-configure-写入隔离断言使用操作增量2026-10-24)。

## 71.388 Native逐CoBo运行阶段接入硬件观察与MCP

`SystemAcquisitionClient`现按每块CoBo跟踪本次运行对象内的`configured/ready/starting/running/resuming/stopping/paused/stopped/unknown`阶段，覆盖 Connect、Run start、Pause/Resume、Stop、Disconnect、Breakup及失败负向停止；部分停止失败只把未确认的板记为unknown，已确认停止的其他板保留stopped。硬件观察GET增加逐板阶段字段并标注`live-controller-cache`、`is_hardware_readback=false`；Native GUI加入缓存阶段列，控制MCP提供只读`ecc_hardware_state`工具。查询仍按既有预算执行固定硬件位读取，不执行恢复或写入。

阶段仅存在于同一存活控制器对象，不跨服务重启恢复，不是板端寄存器读回或完整旧版独立CoBo FSM。Linux模拟ICE两编码的采集生命周期TCP及Native正常流程共3/3通过；硬件观察两编码各通过；Qt协议/状态窗口定向测试5/5（含init/cleanup），MCP stdio/认证HTTP回归1/1。验证细节见[验证71.388](VALIDATION.md#71388-native逐cobo运行阶段远端观察与mcp查询2026-10-24)。未连接实板；Windows Qt 5.14.2、macOS、服务重启恢复和现场验收仍待完成。

## 71.387 Hardware准备协调器逐CoBo阶段状态

`SystemPreparationClient`现在为每块活动CoBo分别维护 `fresh → preparing → prepared → configuring → configured` 阶段；非活动板为 `disabled`。单板重准备和单板Configure会更新目标板阶段；单板Configure只接受该板当前为Prepared，重复请求在硬件写入前拒绝。准备/配置动作开始后若结果不确定则记为 `unknown`；全局Configure仅在系统同步流程完整成功后才把活动板提交为Configured。系统审计记录逐板阶段事件，`CoBoLifecycleJson()`提供同一存活协调器内的缓存状态快照。

该状态当前仍是本机对象内存和命令审计阶段，不跨ECC进程重启恢复，也不是硬件实时读回或旧GetBench完整逐板FSM。全局MuTaNT配置、跨机箱对齐及同步仍由系统协调器拥有。后续还需打通远端状态展示/MCP查询、Run/Stop逐板阶段、失败恢复和持久恢复边界；不可据此宣称完整迁移。验证见[71.387](VALIDATION.md#71387-hardware准备协调器逐cobo阶段状态回归2026-10-24)。

## 71.386 Native逐CoBo生命周期状态开始参与命令门控

ECC网络层现用与界面相同的固定目标生命周期投影校验单板命令：`configure-cobo`只接受已知Prepared目标；`reprepare-cobo`接受已知Prepared或Configured目标；LED与单次Pulser诊断要求已知Prepared/Configured目标。投影为unknown时拒绝，仍同时要求父ECC健康Prepared及数据/硬件既有门槛。该约束从只读显示推进到实际命令转移门控，但阶段证据仍来自服务当前保留的生命周期journal投影，不是板端读回、独立CoBo持久状态存储或完整的旧版逐板失败/恢复FSM。

真实ECC HTTP服务、双Supervisor、receiver、生产MainWindow维护窗口本机loopback场景 `cobo-maintenance-window-0` 1/1通过（124.246秒）。选中C0首次Configure完成后再次Configure返回HTTP 412；测试核实拒绝请求没有新增操作日志或硬件写入。证据校验器通过，合成raw 4个共65,536字节已全部删除（剩余0）。没有真实ECC网络、实板或吞吐验收。详见[验证71.386](VALIDATION.md#71386-native逐cobo命令转移门控回归2026-10-24)。独立逐板持久状态、旧版故障和多节点协调语义、服务重启恢复、Windows/macOS及实板验收仍待完成。

## 71.385 Native逐CoBo生命周期状态投影

Native固定活动CoBo现在共享ECC system status/持久operation journal计算出逐板生命周期显示。单板`reprepare-cobo`完成只把目标板标为Prepared；`configure-cobo`只把目标板标为Configured；同一CoBo的LED/Pulser等维护操作仅更新最近操作证据；整机状态命令投影到整个活动板集合。ECC维护目标schema 2传输逐板生命周期、最后operation ID/命令/phase，主窗口按数据行关联CoBo状态，维护窗口显示选中板状态及最近操作。目标元数据只在连接/明确检查/操作完成后刷新，不插入高频system status/operation观察轮询。

该视图来自ECC操作journal，不是板端实时读回。当前状态投影不等同于独立CoBo状态机：服务冷启动后硬件状态不作为已知事实恢复，先显示UNKNOWN；旧`prepareNode/configureNode`各板中间状态、部分失败语义和系统多板顺序协调仍待显式迁移。硬件`getHwServer`/`IceEirfu`不在重构范围内。验证与限制见[71.385](VALIDATION.md#71385-native逐cobo生命周期状态投影及gui网络回归2026-10-24)。

## 71.384 Connect/Disconnect用户路径生成独立连接日志

连接日志回归已从直接调用状态回调改为点击真实主窗口Connect/Disconnect按钮，在legacy模拟模式下走完RunControl连接/断开生命周期。验证用户数据目录`logs/connections/`、请求/关闭记录、两次连接文件唯一及会话内容隔离；定向QtTest 4/4通过（含init/cleanup）。该测试不连接真实ECC或DAQ。单板独立Prepare/Configured状态和服务重启后的Native Run恢复仍是主要未完成项，整套重构继续进行。

## 71.383 跨层回归确认单板 Configure 与连接日志/UI门控

本机 ECC/双Supervisor/Native receiver/生产Qt维护窗口网络场景 **1/1通过**；单板 Configure 的 HTTP 请求按同一 operation ID 等到终态后再核验，审计确认只写选中CoBo且父状态仍为 Prepared。全套 Qt Widgets 135/135通过（71.379）；本轮新增门控断言和每连接独立操作日志测试共4项（含init/cleanup）通过。MCP控制工具的stdio、明确确认字符串、控制模式开关及只读模式测试通过。两次Run start/Run stop按钮保持左/右布局，Connect后编辑与每次连接独立日志已有Widgets回归覆盖。验证细节见[验证71.383](VALIDATION.md#71383-单板-configure-持久ecchttpgui维护网络回归2026-10-24)。

这仍不是完整单CoBo状态机迁移：逐板独立 Prepare/Configured 状态、跨节点生命周期协调、Windows Qt 5.14.2/macOS原生回归和实体硬件验收尚未完成；整体重构继续进行。

## 71.382 Native单板 Configure 跨层接入

`SystemPreparationClient::ConfigureCoBo(coboId, auditDirectory, dataProcessesStoppedConfirmed)`可在同一存活的系统 Prepared 对象中重用构造时固定的目标板 Configure 计划。操作前核验拓扑、生产停止、`initDone=0`及目标计划；执行后再次确认全系统仍停止且未Ready。仅目标CoBo接收配置写入；不运行系统槽位/跨机箱对齐、不操作其他CoBo或MuTaNT、不置`initDone`、不启动采集。每个操作有独立`configure-cobo.jsonl`及硬件命令审计；父对象成功仍为Prepared，之后仍可明确执行整机Configure。

两种ICE编码的 selected-CoBo 硬件仿真回归通过；现在操作已接入持久ECC命令（新增枚举值17并保留旧值）、精确目标绑定的HTTP请求、Native维护窗口和仅在 `allow_control=true` 时公开的 `ecc_configure_cobo` MCP工具。入口要求健康且新鲜的Prepared状态、接收进程已停止、明确目标CoBo及操作ID确认；不接受停用目标，不触碰其他板/MuTaNT/整机对齐，不置 `initDone`，不启动Run。成功后整机仍为Prepared，后续整机Configure或Run start仍需操作者明确发起。GUI按钮和 MCP 都要求确认；MCP只在批准控制模式出现，结果不确定时按同一操作ID观察，不盲目重试。

Qt界面门控、MCP确认与只读模式、ECC协议/状态/journal、Native协调器及真实ECC HTTP+双数据Supervisor+模拟板的单板写入隔离测试通过；无实体硬件验收。该功能是“整机Prepared上下文中的单板Configure”，不是独立的单板 `prepareNode/configureNode` FSM：没有单板独立Prepare/Configured状态，也没有复刻旧系统级的多板顺序、跨机箱对齐与MuTaNT流程。板端 `getHwServer`/`IceEirfu` 保持不改。本 slice 不能代表整个Native CoBo状态机移植完成。

## Native多板系统中的独立CoBo状态机仍未移植

旧 GetBench ECC 接口明确提供 `prepareNode(nodeType, instanceId, config)` 与 `configureNode(...)`，分别调用选定 `Node` 的 `call_prepare/call_configure`；同时旧系统级 `prepare/configure` 还负责 MuTaNT/CoBo 顺序、槽位识别、跨机箱时钟延迟对齐、同步脉冲等整机协调。因此，旧的单节点操作不能简单等同于整机状态转换，也不能在新界面把一块板显示成独立状态、却实际推进全体 ECC。

当前 Native Envoy 对单活动CoBo且无MuTaNT的快捷按钮映射整机 `Run start/Run stop` 生命周期；多CoBo部署的选中行仍提供维护入口，现已包括 `reprepare-cobo`、单板 Configure、LED和Pulser操作。71.385新增了由全局生命周期和journal投影的逐板状态显示，但它不是硬件读回或完整的单板持久FSM。下一步需继续对比旧版 `prepareNode/configureNode` 的各节点中间状态、重试/失败语义和整机协调依赖，决定哪些要成为持久化的逐板状态；再为兼容状态设计显式迁移/恢复门槛和跨层故障回归。无需改写板端 `getHwServer`/`IceEirfu`。独立单板FSM与现场硬件验收前，不应宣称Native单CoBo状态机已重构完成。

源码依据：旧 `source/get/GetBench-software-release-20210208/src/get/rc/BackEnd.cpp` 的 `prepareNode/configureNode` 与 `onPrepare/onConfigure`，`SystemManager.cpp` 的整机准备顺序，以及当前 `apps/envoy/mainwindow.cpp` 的 `ShowCoBoMaintenanceTargetsSlot`。这是已核实的功能差异与后续工作边界，不代表单板 Native 操作已经实现。

## 71.381 Native catalog失败续作可重建父manager一次性上下文

Native远端catalog替换现在有一个一次性重建入口，可在销毁原ECC父`RemoteReceiverManager`后，重新验证父Publish审计、固定部署和存活数据Supervisor实例里的原operation/sealed Run证据，再沿用同一replacement做受限续作。Publish失败和Finalize失败两项同机loopback回归均通过；后者确认不重复Publish。详细验证见[71.381](VALIDATION.md#71381-native-catalog失败续作重建父管理器上下文2026-10-24)。

这是底层helper，不是完整cold recovery：它尚未接入ECC service，依赖调用方持续核验硬件健康Prepared及独立holds，也不恢复长期ECC控制owner。数据Supervisor自身重启会改变service instance且当前recovery gate未持久化，故该路径拒绝续作。ECC服务重启后的正式流程、Supervisor重启后授权恢复、MCP/GUI流程和物理双机均未完成；整体重构仍在进行。

## 71.380 Native Publish恢复证据保留原请求

父级只读Publish审计解析器现会返回每个固定节点的完整、已验证原始Supervisor请求，并以25个metadata cases覆盖中断与成功审计。重建恢复上下文时可逐项核对原session、实例、plan和operation身份；这个类型仍不提交请求或授权重试。严格C++11完整`getdaq-ecc-native`与证据测试构建成功，CTest 1/1通过。验证细节见[71.380](VALIDATION.md#71380-native-catalog审计保留原始逐节点publish请求2026-10-24)。ECC父服务重启后的跨节点恢复与显式硬件重核仍待完成。

## 71.379 完整Widgets套件通过

当前Qt 5.12.8/C++11完整`tests/tests.pro`重新构建后，在允许loopback的环境运行 **135/135通过**。覆盖HTTP/状态机/Native扫描、每次连接独立操作日志、Run编号回退、Run start在左及Connect后参数可编辑等回归。受限环境首跑的本机TCP监听失败已在loopback可用环境复跑通过；不涉及硬件或大文件传输。详情见[验证71.379](VALIDATION.md#71379-完整widgets套件在允许loopback的环境通过2026-10-24)。Linux full交付构建仍参照71.378；Native Run替换父级服务重启后安全续作、Windows/macOS原生验证及实板验收仍待完成。

## 71.378 当前源码full交付包及Envoy连接日志/UI回归

当前工作树Linux `full` profile重新构建29/29个程序，109项嵌入依赖检查通过；192文件暂存包校验通过，逐个启动帮助入口smoke 29/29通过且未触硬件、未生成raw。当前Qt 5.12.8完整Widgets测试程序重新构建后，连接独立操作日志、Run编号回退/按钮左右顺序/Connect后编辑、GUI生命周期相关用例共5项通过；临时构建/暂存/测试目录已清理，测试raw为0。证据与环境边界见[验证71.378](VALIDATION.md#71378-当前源码full交付包及envoy连接日志ui回归2026-10-24)。

此证据为Ubuntu 20.04用户空间/WSL2内核环境，不代表独立Ubuntu 24、Windows Qt 5.14.2、macOS、物理双机、CoBo或真实DAQ吞吐验收。Windows Qt本轮无法从WSL启动（`UtilBindVsockAnyPort: socket failed 1`），不是Windows构建结果。Native Run替换在服务重启后重建父级状态并安全续作仍未完成，整套重构继续进行。

## 71.377 Native Run替换父级Publish审计只读解析强化

显式恢复现在会在任何子节点查询前重新解析磁盘上的原Publish审计，校验受限文件身份、父controller快照/范围、固定有序节点集合、原始请求和稳定replacement ID，并按journal逐项核对`before_post`、接受/拒绝、catalog receipt、完成、unknown及未尝试记录。恢复路径再将审计内节点实例/计划与当前固定部署比较，并核对配置/工件摘要。此解析只生成只读证据结论，不授权跨服务重启恢复，也没有开放冷启动重发；恢复仍要求原ECC/RemoteReceiverManager生命周期和当前cycle状态。

25项只读审计元数据回归、两节点loopback Publish失败恢复和Finalize失败恢复均通过；相关严格C++11目标构建成功。合成raw在测试后清除并确认无残留；未访问硬件。新增证据解析尚不等于ECC/data-host重启后的cold recovery，仍需构建可审查的父Run持久状态与逐节点授权恢复闭环。见[验证71.377](VALIDATION.md#71377-native-run替换父级publish审计只读解析强化2026-10-23)。整套重构仍未完成。

## 71.376 Envoy按每次连接保存独立操作日志

在用户数据目录`logs/connections/`下为每次Connect请求创建唯一日志文件；记录Connect请求、该会话后续操作和断开/程序关闭标记，同时保留既有完整进程日志。路径创建失败时按现有用户数据、缓存、进程隔离临时目录顺序回退。新QtTest实际建立两次隔离连接，验证生成不同文件、各自会话内容不串写并记录断开标记；Run编号/按钮布局和GUI生命周期回归也通过。没有DAQ raw；测试进程自己的临时日志及构建目录清理。具体记录见本轮回归源码测试`eachConnectionGetsAnIndependentPersistentOperationLog`。

## 71.375 主控制按钮命名与参数布局收敛

全局和下方选中节点的状态机快捷键统一显示为左侧`Run start`、右侧`Run stop`；Native选中CoBo的Prepared维护入口保留独立语义，不改名为Run生命周期控制。参数表名称列固定初始宽度180px，值列扩展填充剩余空间。README已同步说明Connect后Run元数据可修改至正式Start run，以及底部右侧为只读操作日志、`Save YAML`只保存配置不导出raw。完整Qt测试目标在当前源码重建，按钮布局/Run元数据定向测试及GUI生命周期测试通过（各3/3，含初始化/清理）；无DAQ raw，约束内临时构建已删除。Windows Qt5.14.2、macOS和高DPI/窄屏人工布局仍需单独验证。

## 71.374 数据节点重启后可用原生CLI核验Native catalog替换

数据主机的认证HTTP控制面可按原replacement Publish operation ID重新检查正式catalog、stage/candidate和事务journal；原生`getdaq-supervisor-query catalog-replacement-observation`进一步核对历史operation和观察响应的节点/实例/controller/request-key/replacement身份。Prepare ID不能冒充Publish观察，未认证请求拒绝。它只读取有限catalog元数据，不读raw payload，不修复文件或授权恢复。真实service/receiver合成`catalog-restart`覆盖服务重启前后的HTTP与原生CLI观察；8项HTTP transport fault cases及codec/credential回归通过；catalog替换与filesystem CTest 2/2通过。合成raw 294,912字节，12个测试文件已清理。详见[验证71.374](VALIDATION.md#71374-native-catalog替换重启观察增加原生查询cli2026-10-23)。

这为cold recovery增加了节点侧持久operation证据读取能力，但ECC父级Run协调审计、跨节点证据重建、Finalize/Publish恢复授权及GUI/MCP重启后安全恢复仍未接通。不能将只读观察视为恢复机制；整套重构仍在进行。

## 71.372 Native Run替换增加只读事务阶段观察

新增catalog replacement观察API和CLI，可在不读raw payload的情况下按文件系统身份/journal把prepared、两个rename中断窗口、published和finalized分类；结果明确`recovery_authorized=false`。占用事务锁时观察器失败关闭，反例确认不改变目录。严格C++11相关目标构建成功，filesystem CTest 1/1通过。CLI观察只提供诊断证据，不修复事务，也不构成ECC或Supervisor服务冷启动恢复；父级跨节点审计、真实服务路径和目标平台仍需继续验证。详见[验证71.372](VALIDATION.md#71372-native-run替换只读检查中断阶段2026-10-23)与[Native Run替换规范](NATIVE_RUN_REPLACEMENT.md)。整套重构仍未完成。

## 71.371 当前源码Linux全套构建与QtTest通过

当前Ubuntu 20.04.6/WSL2、GCC 9.4.0/CMake 3.16.3/Qt 5.12.8 `full` profile从源码构建29/29程序；556项源码/构建输入绑定到receipt，109项内嵌依赖核对通过，包内29程序启动smoke 29/29成功。完整`tests/tests.pro` QtTest从当前源码重建后134/134通过。证据保存在`build-receiver/linux-host-current-71-371/`。该环境不是独立Ubuntu24；当前仍缺独立Ubuntu24/macOS验证、现场板卡和真实DAQ吞吐验收，也不能由本机程序启动smoke代替。整套重构仍未完成。详见[验证71.371](VALIDATION.md#71371-当前源码linux-full构建29程序启动与完整qttest2026-10-23)。

## 71.370 Windows Qt 5.14.2 Run替换句柄回归通过

修正Sentry Finalize替换Windows原生`catalog.json`时未关闭读句柄的问题，并在删除旧Run备份前关闭其清单句柄。用户提供的Qt 5.14.2/MinGW 7.3下完整Widgets测试133通过、0失败、1项Unix专属跳过；ECC客户端69/69通过。没有生成raw，测试清理回执正常。验证边界及证据见[验证71.370](VALIDATION.md#71370-windows原生qttest修复sentry-run替换文件句柄边界2026-10-23)。这只证明该Windows QtTest环境，不代表现场Run替换、独立Windows部署、真实硬件、macOS、独立Ubuntu24或持续DAQ吞吐已验收；整套重构仍未完成。

## 71.369 MCP将ECC 409 busy原因返回给控制host

MCP收到ECC HTTP 409后，现在在JSON-RPC工具错误中转发有界服务端`error`文本。因此GUI操作期间由MCP提交的命令会明确返回`busy/not queued`，而不是只显示Qt网络层泛化的Conflict错误；没有自动排队、重试或硬件写入。严格C++11 MCP构建成功，stdio假ECC回归1/1通过，验证错误状态和原因被host看到且operation未被接纳。无raw、真实host、真实ECC、硬件或现场验收。见[验证71.369](VALIDATION.md#71369-mcp清晰报告ecc并发busy拒绝2026-10-23)。整套重构仍未完成。

## 71.368 GUI与MCP共享ECC owner时并发命令失败关闭

ECC网络服务对operation acceptance使用同一串行锁。新增的回归在Prepare已被接纳并仍运行时，使用相同`client_id`和`client_epoch`、不同operation ID模拟第二个控制客户端提交Describe：服务返回HTTP 409 `busy/not queued`，模拟硬件日志写次数没有增长。既有测试同时保留同ID同请求只返回原operation、另一epoch不能接管的检查。完整`ecc-network-service-normal-0`本机loopback集成测试通过（88.74秒）。

因此GUI和MCP可同时连接并共享已登记的控制owner，但命令执行并不并行、也不会自动排队；同一时刻仅一个新operation能被接纳，另一个入口应等待当前操作完成、刷新权威状态后由操作者重新提交。若MCP复用不同epoch会被拒绝，不能自动接管。该结果不验证真实MCP host/GUI同时操作硬件，也不证明跨机现场网络、CoBo或DAQ吞吐。详见[验证71.368](VALIDATION.md#71368-gui与mcp共享owner时服务端拒绝并发控制命令2026-10-23)。

## 71.367 Native Run替换恢复接入GUI并通过完整Qt回归

主窗口现在显示专用的“Resume failed catalog replacement”按钮和当前恢复资格原因；只有Native ECC连接、该Run仍活动、状态新鲜、控制器健康Prepared、没有其他未完成/控制故障，并且本机journal与ECC操作存储能证明原operation是同一Run的失败替换catalog时按钮才会启用。操作员点击后会看到原失败operation ID和新恢复operation ID，并再次明确确认；GUI使用新的恢复ID，始终引用原失败ID，经RunControl转发到NativeRunControl和ECC HTTP服务。成功后重新查询数据主机状态、复核全部catalog receipt，再走既有Run完成和Run号递增路径。MCP控制入口仍是独立客户端，服务端会串行仲裁命令；本次未验证两个客户端竞态。

实现中修正了`EccClientStore`实例ID的C++类型错误，并修正了`EccSystemClient::CanSubmit`此前要求所有recovery命令必须已有本地控制故障的条件：catalog恢复现在要求新鲜、匹配身份且健康Prepared的状态，并拒绝本地控制故障。新Native GUI假ECC集成用例覆盖替换catalog失败、只开放精确恢复资格、以新operation ID恢复和Run最终完成；完整Qt GUI状态机回归 **27/27通过**。Linux Qt 5.12.8/C++11 `apps/envoy/envoy.pro`当前源码构建成功；测试与app构建临时目录（约36 MiB）已删除且目录内无raw。未验证Windows Qt 5.14.2、macOS、真实MCP host同时发命令、物理双机或硬件现场。整套重构仍未完成。

## 71.366 Native Run替换Finalize恢复和审计回归通过

所有数据主机Publish成功后现在进入Finalize，逐节点清除仅在同一source内与新Run完整raw文件名相同的旧链接，并保留不同名旧raw。Finalize失败后停止后续节点；恢复时先核对原Publish审计和每节点操作结果，已全部Publish的节点不重复Publish，再续作Finalize。修复父审计JSONL中嵌入JSON带换行导致的后续恢复解析失败；相关严格C++11目标构建成功，文件系统、Supervisor协议及替换成功/Prepare失败/Publish失败/Finalize失败恢复共6项CTest全部通过。合成raw约1.4MiB，测试后已删除本轮10个fixture根并确认无raw残留。详情见[验证71.366](VALIDATION.md#71366-native-run替换finalize恢复与父审计jsonl修复2026-10-23)。

该证据是同机双Supervisor loopback，不是物理双机或实板验收；冷启动恢复、MCP host正向真实ECC调用和GUI/MCP并发冲突处理、Windows Qt 5.14.2当前源码、macOS、独立Ubuntu24及持续DAQ吞吐仍需验证。GUI恢复按钮在[71.367](#71367-native-run替换恢复接入gui并通过完整qt回归)补齐。整套重构未完成。MCP适配器源码在`apps/ecc/mcp/main.cpp`，ECC HTTP服务在`apps/ecc/service/`。

## 71.364 Native Run替换恢复正向ECC服务回归通过

真实ECC网络服务loopback现已验证Run替换Publish失败后的显式恢复：服务继续同一replacement，两个本机数据节点的正式catalog receipt均重新核验，控制与数据reconciliation门槛清除；测试exit 0。12个合成raw共147,456字节，已全部删除。该证据覆盖ECC服务和两个本机Supervisor，不代表MCP stdio host正向端到端、物理双机或硬件验收。详情及范围见[验证71.364](VALIDATION.md#71364-native-run替换正向ecc服务恢复集成回归2026-10-23)。

这仍不构成Run替换或整套重构完成：跨主机Publish不是原子事务，服务/数据主机重启后的cold recovery不支持，旧session/catalog backup中的同名旧raw未物理回收；Windows Qt 5.14.2、macOS、独立Ubuntu24、实板和持续DAQ吞吐也仍需单独验收。

## 71.363 Native Run替换恢复接入ECC协议与MCP

ECC服务和MCP现已接通一个受限的Run替换Publish恢复请求：它必须引用原失败替换catalog operation，使用全新的恢复operation ID，并满足服务端保留的live Publish凭据、同一ECC/client身份与epoch、健康Prepared状态及无独立故障保持等条件。MCP只有在显式`allow_control=true`时才显示`ecc_resume_catalog_replacement`，并要求将原失败ID和新恢复ID绑定到显式确认参数。重启后的cold recovery仍不支持。

协议/server/MCP目标已构建，协议测试1/1、MCP假HTTP集成通过；真实ECC loopback还通过了不合格普通catalog恢复请求返回HTTP 409且无远端副作用的拒绝门槛测试。另见[验证71.364](VALIDATION.md#71364-native-run替换正向ecc服务恢复集成回归2026-10-23)：正向服务恢复已由真实ECC网络服务与两个本机Supervisor节点集成验证，但MCP stdio host尚无连接真实ECC的完整正向端到端验收，GUI也没有恢复入口。准确测试范围和raw清理回执见[验证71.363](VALIDATION.md#71363-native-run替换恢复接入ecc协议与mcp-2026-10-23)、[验证71.364](VALIDATION.md#71364-native-run替换正向ecc服务恢复集成回归2026-10-23)、[MCP手册](ECC_CONTROL_MCP.md#受限恢复-ecc_resume_catalog_replacement)和[Native替换规范](NATIVE_RUN_REPLACEMENT.md)。

这不是Run替换完成：跨节点发布仍不是全局原子事务，Native旧session/catalog backup内的同名旧raw尚未物理回收；Windows Qt 5.14.2、macOS、独立Ubuntu24、实板/双机和持续DAQ吞吐也未由本次覆盖。整套重构仍未完成。

## 71.362 Native Run替换增加受限的Publish失败恢复

跨节点Publish失败后，现在可从原Publish审计执行显式恢复：逐节点查询原operation ID，验证终态失败/完成和未尝试节点的operation确实不存在，再以同一replacement ID、每节点新operation ID继续。数据主机只在live Supervisor仍持有完全相同的失败Run/session/replacement身份、全部receiver正常退出且seal与存储核验通过时允许一次恢复。ECC/DataSupervisor重启后不支持冷恢复；unknown/缺失状态失败关闭。严格C++11构建及Selected CTest 18/18通过，合成raw共约3.2 MB且已清理。测试为同机loopback，不是物理双机。详见[验证71.362](VALIDATION.md#71362-native跨节点publish失败后的显式恢复2026-10-23)与[Native Run替换规范](NATIVE_RUN_REPLACEMENT.md)。

此进展不代表Run替换完成：发布仍非跨主机原子事务，旧catalog backup及session内同名旧raw仍未物理回收；cold recovery、Windows Qt 5.14.2、macOS、独立Ubuntu24、CoBo现场与持续吞吐未验证。

## 71.361 Native Run替换在Publish首个不确定节点后停止

跨节点替换Publish现在遇到首个失败/不确定结果就停止后续节点提交，并在父审计中明确标注未尝试节点；正常替换、Prepare屏障失败和Publish首失败场景的loopback CTest 3/3通过，合成raw仅240 KiB且已清理。仍缺父级Observe/Resume、节点级已完成集合及恢复闭环，旧session和catalog backup内同名旧raw也尚未物理清理。详情见[验证71.361](VALIDATION.md#71361-native跨节点publish首失败停止后续提交2026-10-23)与[Native Run替换规范](NATIVE_RUN_REPLACEMENT.md)。真实双机、硬件及目标平台未验收。

## 71.360 当前源码full profile与全程序启动检查

Linux `full` profile从当前源码构建29/29程序、109项嵌入依赖检查通过；192文件临时候选包验证通过，29程序启动smoke 29/29成功且无raw。启动检查暴露并修正`getdaq-ice-check --help`错误返回2的问题，新增CTest回归1/1通过。证据、环境边界和限制见[验证71.360](VALIDATION.md#71360-当前源码full-profile全量构建及29程序启动smoke2026-10-23)。

本轮Linux/WSL2启动验证不替代Windows Qt 5.14.2、macOS、独立Ubuntu、实板、物理双机或持续DAQ验收；Native Run跨节点失败恢复和旧同名raw物理回收仍未完成。

## 71.359 当前数据主机profile构建与完整Supervisor测试

当前Linux `data-host` profile为7/7程序构建成功，嵌入依赖检查1/1通过；Supervisor完整CTest为27/27通过。63个微型合成raw共1,581,061字节，均小于20GiB限制，两个临时构建/测试目录已删除。结果及边界见[验证71.359](VALIDATION.md#71359-当前数据主机profile构建与supervisor完整测试2026-10-23)。

数据主机测试通过不等于全项目完成。Native Run替换还缺跨节点publish部分失败后的父级恢复，以及旧同名raw在session/catalog backup内的物理回收；本轮也未验证Windows Qt 5.14.2、macOS、独立Ubuntu、实板、物理双机或持续DAQ吞吐。

## 71.358 当前control套件与完整Qt Widgets回归（2026-10-23）

当前源码在Qt 5.12.8/GCC 9.3/C++11下，`control` profile 9/9程序构建成功、95项嵌入依赖检查通过；从同一工作树重新构建的完整`tests/tests.pro` QtTest为134/134通过。它锁定了现有控制台/配置编辑、Run回退及legacy同名替换恢复等界面与业务边界，不代表Native Run跨节点Publish部分失败恢复已实现。回归只使用本机mock/项目临时fixture，无真实CoBo/ECC/DAQ吞吐验证。Windows Qt5.14.2、macOS及独立Ubuntu24未在本轮构建。详见[验证71.358](VALIDATION.md#71358-当前源码control交付构建及完整qt-widgets回归2026-10-23)。整套重构仍未完成。

## 71.357 控制MCP新增显式Run归档工具（2026-10-23）

`getdaq-control-mcp`现提供`ecc_catalog_run`：AI host可在明确传入Experiment、Run号、是否替换、operation ID及确认字段后，请求固定ECC归档当前已停止/封存的receiver session。工具继承已有Bearer认证、client identity/snapshot/epoch绑定；默认`allow_control=false`时不展示控制工具，隐藏调用仍失败关闭。归档仅为控制元数据，不让MCP或Qt接收raw数据。

C++11 MCP程序构建成功；获准本机loopback假ECC CTest 1/1通过，验证无效确认不发HTTP、有效catalog请求字段绑定、epoch复用和只读模式。重要限制：这是归档控制入口，不替代Envoy Run Start授权或自动Run号管理；同名覆盖目前是catalog逻辑集合替换，旧session和catalog backup内同名旧raw尚未物理清理。跨节点publish后部分失败的父级恢复还未闭环。真实MCP host批准体验、真实ECC、Windows Qt5.14.2、macOS、双机/CoBo与DAQ负载仍待验收。详见[ECC MCP手册](ECC_CONTROL_MCP.md)、[验证71.357](VALIDATION.md#71357-ecc控制mcp新增run归档替换工具2026-10-23)及[Native Run替换规范](NATIVE_RUN_REPLACEMENT.md)。整套重构仍未完成。

## 71.356 Native Run替换增加跨节点Prepare屏障（2026-10-23）

Native Run替换现分成两个明确的Supervisor catalog阶段。ECC先用同一个稳定替换身份向所有数据主机提交`prepare`：每个节点将新session写入独立stage、校验并生成schema 2候选；只有所有节点的prepare请求均完成且回执/请求绑定复核通过，RemoteReceiverManager才创建第二轮`publish`意图，逐节点交换旧catalog备份和候选目录。阶段字段、stable replacement ID与子操作ID均进入请求身份；旧的单阶段catalog请求仍按原编码和执行路径解释。

严格C++11完整ECC/Supervisor构建完成。相关CTest **5/5通过**：替换计划、receiver catalog文件系统、Supervisor codec、双逻辑主机成功替换，以及双逻辑主机prepare失败屏障。失败用例先为Run建立旧catalog，再使一节点catalog父目录不可写；另一个节点确实完成prepare，但两个正式旧catalog均保持原receipt，且父审计中不存在publish intent。正常用例确认每主机prepare和publish请求使用相同稳定替换身份、最终schema 2 catalog可独立重开、旧非同名raw保留。服务测试也已在获准loopback下通过。所有payload均为微型合成raw，远低于20 GiB；临时raw fixture已清理。

屏障解决的是“某节点尚无候选时另一个节点已发布”的前置风险，不代表多节点整体原子提交或已解决恢复：任一prepare失败会留下已成功节点的stage/candidate及失败审计，系统不自动重放；publish阶段发生部分主机失败时，已发布主机不回滚，尚无父级Observe/Resume及逐节点恢复/重试闭环。旧catalog backup和session中的同名旧payload也仍保留；只保证正式catalog逻辑集合用新文件取代同名项并保留其他旧raw，未完成物理回收。Run替换尚未接入MCP；Windows Qt5.14.2、macOS、Ubuntu24独立部署、真实双机/CoBo/DAQ及吞吐均未验收。整套重构仍未完成。见[验证71.356](VALIDATION.md#71356-native-run替换增加跨节点prepare屏障2026-10-23)及[Native Run替换规范](NATIVE_RUN_REPLACEMENT.md)。


## 71.355 Native Run替换授权接入Envoy、ECC与数据主机（2026-10-23）

Native Start确认现在把显式替换意图写进本机Run journal，并沿Envoy ECC请求、ECC父协议、RemoteReceiverManager、数据主机Supervisor协议传到`DataSupervisor::Catalog`。旧协议字段缺省时字节编码兼容；替换标志计入请求身份。每个数据主机按replacement stage、candidate准备、正式catalog发布顺序处理，并由现有schema 2逻辑集合确保同source精确同名新raw优先、不同名旧raw保留。Native Run完成后只清理本机旧Run记录备份；catalog旧目录备份继续保留，未删除旧raw。新增的Native GUI回归确认只有已确认的Start带替换标记，并保持Run号正常递增路径。

GCC 9.4严格C++11编译`getdaq-ecc-native`、supervisor控制服务及远端接收器集成fixture成功。catalog替换判定器、receiver catalog文件系统和Supervisor控制codec CTest **3/3通过**；获准的127.0.0.1隔离环境中普通远端receiver回归`native-ecc-remote-receiver-normal-0`及双数据主机替换fixture`native-ecc-remote-receiver-catalog-replace`各 **1/1通过**。替换fixture运行两个独立ECC数据主机服务及实际receiver进程，检查父intent内两条Supervisor请求均绑定`catalog_replace_existing=true`，新schema 2 catalog在两节点发布，旧目录backup及非同名raw保留；两节点同机loopback，不是物理双机测试。Qt 5.12.8/GCC 9.3 Native GUI目标编译成功，catalog完成门槛、确认替换journal/intent和Run号递增定向QtTest **5/5通过**。DataSupervisor单机连续session/catalog fixture也通过。所有集成fixture只生成微小合成raw，远低于20 GiB，临时raw目录已删除。没有真实DAQ/CoBo或持续吞吐测试。

这只是逐节点接线，不是完整可恢复的跨节点替换事务：ECC当前按节点提交stage+publish，没有“所有节点均已Prepare后再统一Publish”的屏障；集成fixture尚未注入节点间部分发布故障以验证父级Observe/Resume。旧catalog备份及旧session中被同名新raw取代的raw尚未逐项删除，因而“同名覆盖”目前是正式catalog逻辑集合替换，不是旧载荷回收。双节点fixture使用不同的session时间/进程文件名，没有制造完全相同的raw文件名碰撞；同名优先由独立替换计划/候选测试覆盖。Windows Qt 5.14.2、macOS、独立Ubuntu24现场部署和真实硬件验收尚未覆盖，整套重构未完成。详见[验证71.355](VALIDATION.md#71355-native-run替换授权接入控制链并验证逐节点归档2026-10-23)及[Native Run替换规范](NATIVE_RUN_REPLACEMENT.md)。

## 71.353 schema 2连续替换保持current与retained边界

修复替换计划对schema 2输入的分类：selected file现在同时保留“来自逻辑新Run”与“属于该catalog sealed current session”两个事实。候选生成只把新Run的current-session文件写入`files`；新输入已有的`retained_files`和旧Run非冲突文件继续写到`retained_files`，不会增加本次receiver seal计数。新增从schema 2 catalog再次生成候选的实盘目录测试，校验含本次文件的source行、retained-only source行和独立Inspect均正确。

GCC 9.4/C++11构建`getdaq-catalog`、receiver catalog和replacement测试程序成功；`catalog-replacement-file-set`、`receiver-catalog-filesystem` CTest 2/2通过。全程使用既有微型fixture，小于20 GiB，测试fixture自动清理；临时构建树随后移除。该修复仅保证catalog的连续逻辑合并正确；Native Run启动仍拒绝已有Run，尚未实现新session暂存catalog、多节点全新catalog齐备后的同名旧raw清理、Run号推进或MCP调用。详见[验证71.353](VALIDATION.md#71353-schema-2连续替换保持current与retained文件语义2026-10-23)和[Native Run替换规范](NATIVE_RUN_REPLACEMENT.md)。

## 71.352 Native catalog替换加入可恢复的单机发布阶段

`PublishReceiverCatalogReplacement()`及`getdaq-catalog --publish-catalog-replacement OLD NEW CANDIDATE`现可将已验证的schema 2候选发布到正式catalog目录。事务以operation ID固定journal/旧目录backup，持有独占锁，通过不覆盖目标的rename先保留旧catalog、再发布新catalog；receipt和阶段marker经同步并重开校验。重复调用可从“old已移入backup、marker未写”和“candidate已到target、published marker未写”两个合成崩溃窗口恢复。恢复测试通过，且确认旧catalog/raw完整保留；本层不清理backup中的同名旧raw，因此尚未完成用户要求的同名覆盖文件回收，更未接入Start、Run号推进、ECC多节点或MCP。Linux renameat2及macOS renameatx_np实现，其他平台失败关闭。

GCC 9.4/C++11完整构建通过；替换计划/候选和receiver catalog CTest 2/2通过，schema 1传输TCP/文件系统回归1/1通过（默认沙箱禁止loopback socket，获准后本机测试通过）。传输测试使用既有小型fixture，远低于20 GiB并在结束时清理；本轮259 MiB临时构建树在交付前删除。Markdown本地链接1793项通过。这里的“恢复”仅为构造的文件系统阶段状态回归，不等于SIGKILL/断电、真实多主机或DAQ现场验证。详见[验证71.352](VALIDATION.md#71352-native-catalog替换加入持久化单机发布与中断阶段恢复2026-10-23)及[Native Run替换事务规范](NATIVE_RUN_REPLACEMENT.md)。

## 71.351 schema 2候选绑定正式路径并支持改名后校验

schema 2候选现在把completion receipt绑定到Run正式catalog路径，并在manifest记录正式catalog root/target及由operation ID派生的旧catalog备份路径。候选尚在暂存位置时可独立检查；filesystem回归再模拟同卷将旧catalog移到备份、候选改名到正式路径，并确认正式路径上的新目录仍可独立检查。GCC 9.4/C++11下catalog/替换两项CTest 2/2通过，schema 1文件系统/TCP传输回归1/1通过；142 MiB临时构建树和合成raw已清理，文档链接1791项通过。

这里验证的是catalog凭据可跨暂存到正式路径的改名，不是生产发布事务：尚无正式发布CLI/API、持久化替换journal、崩溃恢复或同名旧raw清理，也未接入ECC多节点与Native GUI/MCP Start。Native已有Run仍拒绝替换。详见[验证71.351](VALIDATION.md#71351-schema-2候选正式路径绑定与改名后重开检查2026-10-23)及[替换事务规范](NATIVE_RUN_REPLACEMENT.md)。

## 71.350 候选元数据与原catalog凭据绑定、schema 1传输回归

准备候选前将输入manifest摘要和新request身份与初始completion receipt绑定；候选完成后再验证输入仍是原receipt的完整catalog。当前C++11/Linux构建后，替换/ReceiverCatalog CTest 2/2，既有schema 1 catalog TCP/文件系统传输回归1/1通过（传输项因loopback权限在本机隔离环境执行）；总计相关CTest 3/3。20 MiB测试构建树和全部临时raw已清理，测试文件远低于20 GiB；文档链接1789项通过。正式Run目录交换、故障恢复、多节点ECC和GUI/MCP接线仍未实现，已有Native Run仍拒绝替换。详见[验证71.350](VALIDATION.md#71350-候选输入快照绑定完成凭据并保持旧归档传输兼容2026-10-23)。

## 71.349 Native替换候选硬链接物化及schema 2检查

`getdaq-catalog --prepare-catalog-replacement OLD NEW CANDIDATE`现能把两份匹配的完整catalog合并为独立schema 2候选：同source精确同名采用新Run raw，旧Run其他文件和new-run已撤掉的source以硬链接保留；各文件记录来源receipt摘要及inode/大小身份。新旧schema检查兼容，候选可继续作为下一次合并输入；CLI、候选重开检查和实际硬链接身份测试通过。GCC 9.4/C++11构建成功，相关CTest 2/2通过，文档链接1785项通过；小型raw fixtures与18 MiB构建目录已清理。

这仍是本机事务的Prepare前半段：候选没有operation journal/故障恢复，未交换到正式Run目标，也未清理旧catalog的冲突链接；ECC多节点、RunControl/GUI/MCP仍未接线，正式流程继续拒绝覆盖已有Run。详情及隔离使用边界见[验证71.349](VALIDATION.md#71349-生成并独立验证native-schema-2替换候选2026-10-23)和[替换事务规范](NATIVE_RUN_REPLACEMENT.md)。

## 71.348 替换计划具备后续执行所需的文件来源凭据

计划条目现在携带实际获选raw所在的已验证catalog、source子目录、completion receipt摘要和文件大小/device/inode；回归逐项与fixture的`lstat()`身份比较。此元数据使下一阶段能按原始身份建立硬链接保留集，并继续由71.347的前后快照检查阻止目录变化。GCC 9.4/C++11构建成功，catalog与计划CTest 2/2通过；18 MiB构建树及低于20 GiB的临时测试数据已清理。此刻仍未构建或发布merged catalog，也未接到ECC多节点事务、Run journal或GUI；Native已有Run仍被拒绝替换。详见[验证71.348](VALIDATION.md#71348-替换计划携带已验证raw文件身份与来源2026-10-23)。

## 71.347 只读替换计划校验catalog快照一致性

生成保留集后再次比较两份完整catalog的request、manifest和完成凭据；若目录在校验与计划计算期间变化则拒绝返回，避免用陈旧验证结果判断覆盖范围。Linux GCC 9.4/C++11目标成功构建，既有catalog与替换计划CTest 2/2通过。专用18 MiB构建目录及临时测试文件已清理；1 GiB逻辑稀疏raw上限低于20 GiB。该更改仍只产生计划，不发布catalog或清理旧raw；Native已有Run替换、多节点提交和GUI流程仍未实现。详见[验证71.347](VALIDATION.md#71347-替换计划返回前重新验证catalog快照2026-10-23)。

## 71.346 从已验证catalog生成只读替换计划

替换计划现在可以从两份完整且身份匹配的ReceiverCatalog生成：严格检查experiment/Run/deployment和不同session/operation身份，再按source把新同名文件、旧Run独有文件分类。相关既有catalog与新测试合计CTest 2/2通过，1 GiB逻辑稀疏fixture和51 MiB专用构建树均已清理。此步骤仍完全只读，Native catalog发布、Run journal、ECC多节点协调、GUI Start流程及Finalize尚未接通；Native已有Run仍拒绝替换。详见[验证71.346](VALIDATION.md#71346-从完整receivercatalog生成只读run替换计划2026-10-23)和[事务规范](NATIVE_RUN_REPLACEMENT.md)。

## 71.345 现有Native catalog回归保持通过

将替换判定器加入Supervisor核心库后，同时重建catalog CLI、新判定器测试和既有ReceiverCatalog文件系统测试；GCC 9.4/C++11 CTest 2/2通过。原测试仍确认不可变seal/catalog、session身份、硬链接和源raw保护。夹具最大1 GiB逻辑稀疏raw，低于20 GiB，测试后清理；51 MiB构建目录已删除。Native Run级替换仍未接入，不能据此称为Run覆盖完成。详见[验证71.345](VALIDATION.md#71345-native替换判定器与现有catalog文件系统回归2026-10-23)。

## 71.344 Native Run替换判定器首阶段

新增纯C++11 catalog文件集合判定器：同一source下新raw的精确同名文件优先，旧的不同名raw保留，source之间不发生名字碰撞。Supervisor核心静态库构建成功，判定器定向CTest 1/1通过。此阶段只验证名字集合，不读raw payload；尚未接入catalog manifest、session硬链接迁移、ECC多节点可恢复事务或GUI，Native已有Run仍然拒绝覆盖。设计和剩余验收步骤见[Native Run替换事务规范](NATIVE_RUN_REPLACEMENT.md)，证据见[验证71.344](VALIDATION.md#71344-native-run替换的按source文件集合判定器2026-10-23)。

## 71.343 当前源码完整QtTest回归

Qt 5.12.8/GCC 9.3/C++11下完整`tests/tests.pro`目标构建成功；在允许本机mock loopback和私有fixture的隔离环境中完整QtTest 134/134通过。受限默认沙箱首跑的loopback/fixture访问失败不代表产品回归。模拟器测试数据预算合计约1.125 MiB，所有测试临时数据已清理；没有真实ECC、CoBo、接收主机或DAQ验证。证据见[验证71.343](VALIDATION.md#71343-当前源码完整qttest回归2026-10-23)。

Native已有Run时仍拒绝session/catalog同名替换，这是当前Run覆盖语义尚未闭环的主要功能缺口；QtTest通过不能证明Native跨主机恢复、Windows Qt 5.14.2、macOS或现场运行。整套重构仍未完成。
已把用户确认的精确文件名规则及Native所需的catalog保留集、多节点恢复状态机和验收门槛整理到[Native Run替换事务规范](NATIVE_RUN_REPLACEMENT.md)。该规范不代表功能已实现；现有拒绝覆盖行为继续保护旧数据。

## 71.342 Run覆盖变更后的Linux full重建与启动检查

在71.341 legacy同名替换实现之后，当前源码Linux `full` profile重建29/29程序，109项嵌入依赖核验通过；暂存候选清单校验通过，隔离环境下29个程序启动smoke全部通过，GetDAQ离屏启动。临时构建/候选产物已清理，无硬件访问或raw遗留。full构建关闭测试目标，不能代替CTest/QtTest；也不代表独立Ubuntu24、Windows、macOS或现场验收。证据与边界见[验证71.342](VALIDATION.md#71342-run覆盖变更后的当前linux-full重建与29程序启动检查2026-10-23)。

## 71.341 Legacy Sentry Run回退按同名raw覆盖

Run号回退不提前删数据；legacy Sentry在Start确认和新Run完成后仅用同名新raw替换旧文件，旧Run的非同名raw通过同卷rename保留到新catalog。Finalize进度可重试及恢复rename后回执前中断；不复制raw载荷。Sentry同名覆盖/恢复、RunRecord、RunControl、GUI和loopback HTTP定向测试11/11通过。legacy Sentry与Envoy产品目标也已从当前源码分别编译并通过帮助/离屏启动冒烟，但尚未重建full profile或bin交付物。Native每Run session/catalog替换仍未实现；Windows/macOS及真实数据主机、CoBo现场流程未验收。详见[验证71.341](VALIDATION.md#71341-legacy-sentry-run回退仅覆盖同名raw2026-10-23)。

## 71.340 Run编号手动回退界面门槛回归

Run编号在控制台连接期间不可编辑；断开后可以手动调回旧编号。新增离屏QtTest覆盖该编辑门槛，Qt5.12.8完整测试目标构建成功，定向用例3/3通过，无DAQ/raw或硬件访问。此处只验证UI编辑门槛；legacy同名覆盖实现见[验证71.341](VALIDATION.md#71341-legacy-sentry-run回退仅覆盖同名raw)，Native仍拒绝已有Run/catalog替换。

## 71.339 Ubuntu24程序清单支持当前MCP及历史候选

完整主机程序清单新增MCP后现为29项；Ubuntu24候选校验器依照构建计划校验当前29项，并继续接受明确列出的MCP加入前26/27/28项历史候选。16项包校验器回归、构建计划测试和1776项文档链接检查通过。仅更新清单兼容逻辑和说明，未做Ubuntu24 rootfs加载或新候选打包。详见[验证71.339](VALIDATION.md#71339-ubuntu24候选校验器兼容新增mcp程序2026-10-23)。

## 71.338 Linux full profile 全部29个主机程序构建通过

修正`tools/build-suite.py`把CMake Qt前缀误传给qmake MCP项目的问题，并新增纯Python构建计划回归。当前Ubuntu20.04/WSL2主机Qt5.12.8下重新执行`full` profile后，控制GUI、MCP、原生接收、ECC/监督器及配置仓库合计29个程序全部构建成功；109项内嵌依赖检查通过。详情及平台限制见[验证71.338](VALIDATION.md#71338-linux-full-构建修复qmake参数污染2026-10-23)。这是一轮构建验证，不含硬件验收、全量CTest、Windows/macOS原生验证或现场吞吐测试；整体重构仍未完成。

## 71.337 配置核心15项回归与旧CTest命令修正

配置与扫描操作手册中的CTest示例现在从构建目录运行，兼容项目使用的旧版CTest，防止“零测试却返回成功”。当前GCC9.4/CMake3.16.3/C++11完整重建纯C++实验配置核心并实际发现、通过15项CTest；无Qt、硬件、ECC、receiver或raw数据访问。详情见[验证71.337](VALIDATION.md#71337-旧版ctest可用的配置核心验证命令2026-10-23)。

## 71.336 同名Raw活动文件锁保护回归

receiver现在有回归保证：若完全同名的旧Raw文件仍被活动接收句柄排他锁定，新的写入会拒绝且不截断旧内容；旧句柄关闭后再遵循既有同名覆盖规则。当前receiver六项CTest均通过，ByteStorage的393,353字节合成载荷已校验并清理。此项只加强精确文件名碰撞的并发安全，不完成Run级覆盖；Native catalog/session替换仍是未完成项。详情见[验证71.336](VALIDATION.md#71336-同名raw活动文件拒绝覆盖2026-10-23)。

## 71.335 Native Run完成后持久化下一个Run编号

Native Run在Run记录与journal完成后递增编号，并在用户配置路径可写时自动保存新编号；若正在运行的配置本身就是本Run目录中的`run.yml`快照，则不会用递增后的值覆盖这份历史快照。此前Native只更新内存/UI，重启后会从用户配置重新读回旧编号。Qt5.12.8/GCC9.3/C++11 `native-gui-tests`和完整Envoy目标重新构建成功，新增配置关闭重开回归，以及既有catalog完成门槛、ECC配置快照用例共5/5通过；文档链接1771项通过。均使用本机假ECC和小型临时配置，无raw。详见[验证71.335](VALIDATION.md#71335-native-run完成后保存递增编号2026-10-23)。Native现有Run/catalog替换仍未实现；此项只闭合成功Run后的编号持久化。

## 71.334 新增ECC控制MCP入口

已新增`getdaq-control-mcp`本机stdio适配器，让支持MCP的AI host查询ECC身份/状态/receiver/操作；控制工具默认关闭，显式开启后仍由ECC认证HTTP与服务端FSM执行。MCP只交换低频JSON，不参与DAQ数据收发。Qt5.12.8/GCC9.4下CMake、qmake构建及假ECC loopback CTest通过；Windows Qt5.14.2、macOS、真实服务/板卡和MCP host审批体验仍待验。配置与边界见[ECC MCP手册](ECC_CONTROL_MCP.md)，测试详见[验证71.334](VALIDATION.md#71334-ecc控制mcp适配器与本机假服务联调2026-10-23)。整套重构仍未完成。

## 71.333 同名Raw文件类型检查不再受FIFO阻塞

Raw存储同名路径现以非阻塞方式打开后再检查文件类型，FIFO碰撞会被拒绝，不会卡住接收线程；receiver-core FIFO回归及完整receiver构建树共6项CTest通过。ByteStorage合成raw为393,353字节且清理为0，低于20 GiB限制。仅加强精确文件名碰撞的稳健性，不提供Run号映射或Native Run/catalog/session替换。详见[验证71.333](VALIDATION.md#71333-同名raw碰撞拒绝fifo且不阻塞2026-10-23)。

## 71.332 Envoy应用日志可写目录回退

用户数据目录不可写时，Envoy自动尝试缓存和进程隔离的临时目录，并在界面显示实际回退日志路径。Qt5.12.8/C++11当前源码构建及Linux离屏只读路径启动验证通过。详见[验证71.332](VALIDATION.md#71332-envoy应用日志只读目录回退2026-10-23)。

## 71.331 ICE接收链路集成构建及回归

从仓库的ICE兼容集成工程构建了接收器、ICE入口、catalog和DataSupervisor相关目标；15个实际ICE入口loopback场景、receiver-core与catalog文件系统测试通过。所有合成raw共147,457字节，测试后全部删除。详细证据与适用范围见[验证71.331](VALIDATION.md#71331-ice入口集成回归与精确同名覆盖代码路径构建2026-10-23)。这验证同名覆盖代码所在的接收构建路径，不等同于重复文件名专门用例，也不等同Run级回退替换。

## 71.330 DataReceiver精确同名覆盖

写盘层已实现并验证“精确文件名碰撞时覆盖同名raw文件”：仅安全的单硬链接普通文件可截断，硬链接归档、符号链接、不安全权限及活动文件会拒绝。核心单测及GET命名TCP集成回归通过，集成共发送13,676,684字节合成数据并清除生成raw。该行为不是Run级覆盖：Native管理接收使用独立session目录，Native catalog对已有Run仍拒绝；不能据此宣称Run号回退会自动删旧Run。证据见[验证71.330](VALIDATION.md#71330-精确同名raw覆盖回归2026-10-23)。

## 71.329 DataReceiver原生回归六项通过

在Ubuntu 20.04.6/WSL2、GCC 9.4.0、CMake 3.16.3下增量构建DataReceiver测试目标并执行完整CTest，6/6通过，覆盖MFM合并、GMAT转换、CoBo hit pattern、receiver core、有界TCP exporter隔离和ByteStorage原样字节/EOF封存。合成raw总计393,353字节，在测试临时目录自动删除；确认无残余`.graw`。详见[验证71.329](VALIDATION.md#71329-datareceiver-六项原生回归2026-10-23)。

此回归只证明当前原生DataReceiver与离线工具的有限本机路径，不改变Native Run覆盖尚未实现、跨平台和现场双机/硬件/持续DAQ吞吐仍待验收的结论；整套GetDAQ重构仍未完成。

## 71.328 Run编号回退覆盖的legacy流程接入

Run spinbox允许手动回退编号；legacy SOAP/Sentry模式遇到已有本机Run时，Start要求明确确认。替换只接受身份匹配且journal为`Complete`的旧Run，本机旧记录和各Sentry旧catalog先保留在隐藏备份。所有新catalog和本机Run完成后才清理旧数据并递增编号；失败/未知结果保留旧备份及同一operation ID，Run号不前进。默认Reserve/catalog仍拒绝覆盖。Native receiver session/catalog替换尚未实现。

Linux Qt5.12.8/C++11的Run Control与Sentry定向回归13/13通过，Envoy和Sentry可执行目标构建通过。随后在仅允许隔离本机mock监听的测试环境重跑完整Widgets QtTest，132通过、0失败、0跳过；此前受限环境的47通过/69失败由localhost监听和临时配置夹具访问限制造成，不作为代码回归结果。完整测试私有目录内`.graw`总量受监控并保持低于20 GiB，结束时无raw残留。没有连接真实硬件、数据主机或DAQ。详见[覆盖事务设计与当前实现边界](RUN_NUMBER_AND_OVERWRITE.md)及[验证71.328](VALIDATION.md#71328-run编号回退覆盖legacy流程接入2026-10-23)。

仍需实现Native receiver session/catalog安全替换和跨数据主机可恢复协调，并在Windows Qt5.14.2、macOS及真实现场验证。当前legacy逐节点替换不是跨主机原子事务：失败时可能有节点已发布新catalog，但旧数据仍留在隐藏备份，需恢复或人工检查。

## 71.326 扫描状态API不再提供无证据退休入口

普通`ScanExecutionState`转换不再允许`Bound → Retired`，避免调用方绕过journal直接构造下一点授权。历史退休事件仍能解码，但历史标记阻止`WithNextPoint()`把旧通用回执当成当前退休证据；明确的下一点操作需等待ECC退出、receiver/catalog终态和控制权释放证据接入。C++11 `scan-execution-state-tests`与`ecc-journal-tests`均从当前源码构建并运行通过，覆盖状态API、旧成功记录兼容与旧记录不可推进。详见[验证71.326](VALIDATION.md#71326-内存状态机拒绝无证据退休并隔离旧退休授权2026-10-23)。

## 71.325 扫描journal退休历史兼容性回归

补充回归验证：旧日志里的未决`retire`操作重开后仍需对账，不能由通用“确认已执行”接口越过证据门槛；明确确认未发送会回到`Bound`，而既有已完成退休日志仍能解码。`scan-execution-state-tests`构建及运行通过。本轮未重跑完整QtTest、GUI或硬件。详见[验证71.325](VALIDATION.md#71325-扫描journal退休历史兼容性回归2026-10-23)。

## 71.324 扫描journal阻止无证据退休与跨点推进

扫描GUI尚未开放点位退休，但底层journal此前可用通用成功回执把 `Bound` 推进为 `Retired`。现已拒绝通用退休intent、完成回执和人工“确认已执行”对账；历史退休日志继续可解码。缺少ECC退出、receiver/归档终态和主机控制权释放的绑定证据时，不允许开始下一点。C++11扫描状态journal测试通过，1749个本地文档链接通过；本轮没有GUI/硬件/DAQ活动。详见[验证71.324](VALIDATION.md#71324-扫描journal阻止无证据退休与跨点推进2026-10-23)。

## 71.323 扫描Run控制范围及中断状态卡片校正

扫描窗口标题和说明已区分离线计划生成与在线Native ECC控制，并明确原始DAQ传输仍走独立receiver；`RunStarted`/`RunStopped` journal阶段新增琥珀色待处理提示，要求先检查实时ECC并查看/对账journal。该改动只校正操作员对已有流程和恢复要求的认识，不扩展跨点自动Run。Qt5.12.8/GCC9.3/C++11下完整QtTest目标构建成功，扫描窗口文案与单CoBo整系统FSM路由两项专项均3/3通过；无硬件、receiver或DAQ/raw访问。详见[验证71.323](VALIDATION.md#71323-扫描窗口的run作用范围与中断状态提示2026-10-23)。

再次对照GetBench全系统`BackEnd::createEngine()`与GetController单`TestController`工作流：两者语义不同；Native唯一CoBo快捷键只有一块匹配CoBo且不含MuTaNT时转发给整套ECC状态机，多CoBo的单板重准备仍是Prepared-only维护操作，不构造独立板级生命周期FSM。旧SOAP逐节点Progress/Regress仍属兼容路径，见[状态归属说明](ECC_STATE_OWNERSHIP.md)。

## 71.320 Native整机与旧SOAP单节点状态作用范围可视化

状态表Transition单元格增加稳定/正在转换/不可用三态视觉标记和清楚的悬停说明。依据归档GetBench源码，旧SOAP支持选中节点单独Prepare/Configure以及现存逐节点Progress/Regress；ECC Run生命周期则由全局状态机管理。Native的单CoBo快捷按钮复用整机ECC状态机，严格限制为唯一CoBo且数据源ID一致、无MuTaNT；多CoBo没有被界面描述成独立单板生命周期FSM。Qt5.12.8/GCC9.3/C++11相关GUI专项测试4/4通过。详见[验证71.320](VALIDATION.md#71320-状态表transition提示及旧单板操作语义核对2026-10-23)。

## 71.319 当前源码full profile 28程序重建与启动验证

包括71.318状态表列宽修改在内的当前源码，在Ubuntu20.04.6/WSL2、Qt5.12.8下full profile重新构建28/28成功；构建receipt绑定549项输入，109个嵌入依赖文件核验通过。SHA/运行库检查、188文件临时包manifest和28程序启动smoke通过，未访问硬件、没有raw遗留。CMake发布构建未启用CTest，`ctest -N`为0；不要将本条写成测试套件通过或Ubuntu24验收。临时验证目录已删除。详见[验证71.319](VALIDATION.md#71319-linux-full-当前源码重建与程序启动smoke2026-10-23)。

Windows Qt5.14.2/MinGW原生重建当前仍因本轮WSL interop无法启动Windows qmake而没有新构建证据；macOS、独立Ubuntu24、双机归档、真实ECC/CoBo与持续DAQ吞吐仍未验收。板端`getHwServer`/`IceEirfu`维持既定不重写边界。

## 71.318 主状态表完整显示Transition标题

从实际初始窗口截图发现72 px列宽会截断`Transition`，现将Native和兼容状态表该列设为104 px，仍可由操作员拖动调整，并添加QtTest最小宽度断言。Qt5.12.8/GCC9.3/C++11完整Widgets测试目标构建；`nativeGuiMetadataAndWholeSystemControls` 3/3通过。Envoy生产目标已重建，`bin/GetDAQ`更新后以1440×960离屏截图人工复核通过。未连接任何服务或硬件，无DAQ/raw操作。详见[验证71.318](VALIDATION.md#71318-主界面transition列标题宽度修正2026-10-23)。

该视觉修正发生在71.317 full profile构建/28程序启动smoke之后，因此full构建收据不绑定此一处最新UI源文件；完整28程序full构建尚需在当前源码重跑。Windows Qt5.14.2/MinGW仍受当前WSL无法启动Windows程序的环境问题影响，macOS和实际CoBo状态变化亦未验证。

## 71.317 Linux full 套件从当前源码构建与启动检查

Ubuntu 20.04.6/WSL2主机上构建时549项源码输入与full构建收据一致；28个程序的SHA256和`ldd`记录复核通过，109个嵌入依赖文件已验证。临时188文件运行目录的28程序启动smoke通过，GUI在offscreen无连接启动并保存临时截图；没有访问硬件或生成DAQ/raw文件。71.318随后修改Envoy状态表Transition列宽，因此full构建/启动smoke不包含该最新GUI改动。构建配置关闭测试目标，`ctest -N`为0，所以这不是CTest或Ubuntu24验收。详见[验证71.317](VALIDATION.md#71317-linux-full-套件当前源码启动检查2026-10-23)。

按用户提供路径进行Windows Qt 5.14.2/MinGW当前源码构建时，Windows qmake无法通过当前WSL互操作启动（`UtilBindVsockAnyPort: socket failed 1`）；该次结果是环境启动受阻，不代表代码编译错误。需要在Windows原生终端使用同一Qt kit重跑。独立Ubuntu24/macOS、硬件和持续DAQ吞吐仍未验收。

## 71.316 扫描窗口显示当前点journal进度

扫描运行区现在以状态卡片显示当前点和journal状态；计划未Review、未知/待决操作、`Bound`归档绑定等情况分别给出说明。`Bound`不会用成功绿态暗示硬件已释放，文案直接说明ECC退休与下一点配置仍未自动化。该卡片只读本地计划/journal，不采集硬件状态。Qt5.12.8/GCC9.3/C++11完整QtTest及Envoy从当前源码构建，完整QtTest126/126、状态卡片离屏GUI测试3/3、文档链接1737通过。详见[验证71.316](VALIDATION.md#71316-扫描窗口显示journal当前点进度2026-10-23)。

逐点自动执行仍缺每点独立ECC/工件生命周期管理；卡片没有把journal状态扩展成硬件事实，真实硬件/平台和吞吐验收继续开放。

## 71.315 扫描绑定从journal读取当前点

Run/catalog绑定入口不再弹出可输入的计划点号，而是从经过验证的扫描journal取当前点；只有该点处于`CatalogVerified`且无待决操作时才允许选catalog，并把实际journal点写入CLI参数和输出名。这与现有单点绑定约束一致，避免了向用户展示实际上会被拒绝的其它点。QtTest目标和Envoy内的相关代码从当前源码构建，扫描绑定链通过，完整QtTest在获准运行loopback/临时fixture后126/126通过。详见[验证71.315](VALIDATION.md#71315-扫描绑定自动采用journal当前点2026-10-23)。

整体逐点自动化仍未完成：`Bound`后缺少有现场语义定义的ECC退休与下一点硬件工件加载/配置切换；禁止仅为打开下一journal点而伪造`Retired`。真实ECC/receiver/板卡、Windows Qt5.14.2/MinGW、macOS及持续吞吐仍未验收。

## 71.314 扫描归档绑定与journal状态同步

GUI绑定Run catalog时现在先把 `bind` intent 持久写入当前扫描journal，且仅允许 `CatalogVerified` 的当前点；异步catalog工具运行期间持有journal独占锁，只有验证发布文件与工具输出一致、并核对计划摘要/点索引后才推进到 `Bound`。失败时将原操作保留为未知，阻止静默重放。Qt5.12.8/GCC9.3/C++11的Envoy与QtTest目标构建通过，定向扫描回归3/3、完整QtTest126/126，文档链接1735项通过。此进度还没有ECC退休或下一点配置切换/推进；真实catalog和硬件、Windows/macOS及现场仍未验收。详见[验证71.314](VALIDATION.md#71314-扫描归档绑定写回journal2026-10-23)与[扫描执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md)。

## 71.312 扫描Run成功后的数据收尾与状态机显示

事件目标Stop得到回执且新鲜ECC状态确认后，扫描单点Run会完成Breakup、receiver EOF/排空/退出与seal核验、catalog核验及Run记录归档。扫描Run入口要求`native_catalog=true`。本机fake HTTP集成测试覆盖整条成功顺序。扫描journal后续在71.313增加`ReceiverFinished`和`CatalogVerified`阶段；Run catalog绑定、ECC退休及自动推进仍未实现，真实ECC/receiver和跨进程Run/Stop恢复仍待验收。

主界面的Transition列显示旧GetBench状态机中的迁移动作名称。旧源码`source/get/GetBench-software-release-20210208/src/get/rc/BackEnd.cpp`定义整套ECC的Describe/Prepare/Configure/Breakup及嵌套Active内的Ready/Running/Paused；单CoBo快捷入口继续表示唯一CoBo上的整套ECC系统，不表示多CoBo环境中的独立逐板状态机。Envoy与完整QtTest目标以Qt5.12.8/GCC9.3/C++11构建成功；最终完整QtTest 126/126通过。仅操作小型元数据，无DAQ/raw文件。完整过程见[验证71.312](VALIDATION.md#71312-扫描事件目标后的receiver封存与ecc状态显示2026-10-23)、[验证71.313](VALIDATION.md#71313-扫描点收尾阶段的journal推进2026-10-23)和[扫描执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md)。

## 71.313 扫描点receiver与catalog阶段写入journal

在71.312的成功收尾基础上，Stop确认后、发送Breakup前先持久化绑定已确认Stop operation ID的`receiver-finish` intent；只有新鲜整系统状态确认每个receiver正常退出并seal后，才推进到`ReceiverFinished`。随后先持久化与即将提交的catalog ECC请求（包括同一operation ID）完全一致的`catalog-verify` intent，再发送唯一catalog操作；只有新鲜ECC状态含有与receiver文件数相符的catalog receipt，journal才推进到`CatalogVerified`。Breakup/catalog失败或超时、catalog状态缺失或不一致时保留未知intent并停止，不生成新operation ID重放。

Run记录完成后释放扫描journal独占锁。71.314已把操作员catalog绑定成功状态写回journal为`Bound`；仍不会退休ECC或推进下一个点。Qt5.12.8/GCC9.3/C++11定向fake HTTP集成测试通过并断言Run收尾最终journal为`CatalogVerified`、无待处理intent；fixture只交换小型控制元数据。完整套件结果及剩余边界见[验证71.313](VALIDATION.md#71313-扫描点收尾阶段的journal推进2026-10-23)与[验证71.314](VALIDATION.md#71314-扫描归档绑定写回journal2026-10-23)。

## 71.311 扫描原ID查询故障的安全联锁恢复

有效ECC响应明确返回生命周期操作`phase=unknown`时，Native客户端会为该扫描观察建立可恢复的短暂控制保持。普通HTTP网络错误本身不会触发这个客户端故障标志；尚未解决的请求仍由`EccClientStore`的pending/reconciliation机制约束。现在只有同一Describe/Prepare/Configure operation ID取得complete结果、结果与原请求及预期目标状态完全一致、最新status仍报告该ID和目标状态、告警观察健康且控制故障代次未变化时，才清除该操作自身的保持。告警、状态未知及其他独立保持仍生效；`EccClientStore`也只会在上述精确条件满足时移除该ID的`heldOperations`。启用告警监控时须等待新告警观察，并在解除保持后重新读取Native identity/status。

为不依赖本机TCP监听，`HttpClient`新增默认关闭的进程内测试Transport；生产默认仍走Qt `QNetworkAccessManager`。Qt5.12.8/GCC9.3/C++11下完整`apps/envoy/envoy.pro`、完整`tests/tests.pro`和独立`tests/ecc_system_client.pro`均从源码构建成功；3项ECC联锁回归合计5 passed、0 failed，Native主界面、离线扫描入口及单CoBo整系统状态机3项回归合计5 passed、0 failed（均含初始化/清理）。文档链接检查1728项通过。没有连接ECC/receiver/硬件/DAQ，也没有生成或传输raw；临时构建目录在验证后清理，`bin/GetDAQ`保留为新构建。

## 71.310 未决扫描生命周期操作的只读恢复

扫描执行在进程重启后会把只有持久化intent、没有完成记录的操作隔离为`NeedsReconciliation`。扫描窗口现在可对Describe/Prepare/Configure尝试读取同一个Native ECC operation ID：Native客户端从本机请求日志取回原始请求，并核对命令、operation ID、客户端epoch、实例、部署快照及规范请求SHA与scan journal逐项匹配。恢复操作只调用`GET /v1/operations/<原ID>`，不会重新提交命令。只有收到与原请求字节完全一致的`complete`响应、控制器结果为预期目标且带相同operation ID，并由后续新鲜`/status`再次报告相同操作和目标状态，journal才写schema-6 `operation-observed-complete`事件并解除隔离。404、失败、超时、目标/身份不一致和journal写入失败均保留人工对账状态。Run/Stop不自动恢复，因为事件计数监视器和receiver session尚不能安全跨重启重建。

验证：严格C++11 ECC journal目标及新增重启隔离后读取原始请求摘要、成功恢复、拒绝非生命周期恢复、schema重开持久化用例通过；Qt5.12.8/GCC9.3/C++11 `apps/envoy/envoy.pro`和完整`tests/tests.pro`目标构建成功，Native主界面元数据和离线扫描入口QtTest通过。构建仅使用小型控制元数据；未访问ECC、receiver、硬件或DAQ路径。Loopback fake HTTP恢复链本轮未运行，因此服务GET与UI到服务的实链仍待可监听本地socket的环境验证。

## 71.309 主窗口单CoBo系统快捷入口视觉区分

Native主窗口在“恰好一个活动CoBo、无MuTaNT、且唯一数据源ID匹配固定CoBo”时，将选中行的Progress/Regress快捷入口分别显示为蓝色主操作和淡红色回退操作；多板或未满足条件时，继续显示中性色Prepared-only维护入口。按钮增加面向辅助技术的范围描述，直接说明单CoBo快捷入口改变的是整套固定ECC的生命周期，而不是为单板另建一个状态机。命令、启用条件和请求路径不变。

旧源码对照详见[ECC状态归属说明](ECC_STATE_OWNERSHIP.md)：GetController的GetTest是一个窗口控制一个目标CoBo的本地测试流程；GetBench的SM_ECC则由同一个SystemManager驱动系统生命周期。Native目前只对唯一CoBo系统开放系统级快捷入口，不把多板ECC拆成虚构的逐板Configure/Start/Stop状态机。

Linux Qt5.12.8/GCC9.3/C++11完整Widgets测试目标在独立`/tmp`目录编译链接成功；单CoBo/多板快捷入口回归`nativeSingleCoBoLifecycleUsesWholeEccTransitions`和Native主界面控制回归`nativeGuiMetadataAndWholeSystemControls`通过。完整`apps/envoy/envoy.pro`目标重新编译，并更新Linux可执行文件`bin/GetDAQ`；使用隔离用户数据启动该交付程序的1440×960离屏截图成功，检查了主界面分区和禁用按钮样式。离屏插件输出仅含不支持窗口raise/size-hint的提示。未连接ECC、receiver、硬件或DAQ路径；Windows Qt5.14.2/MinGW和macOS未在此处构建。截图、测试和构建临时目录已删除。

## 71.308 Native扫描点单步Describe/Prepare/Configure

扫描窗口现在能在当前点journal和新鲜ECC身份相符时，逐个请求一次完整Native ECC系统的Describe、Prepare或Configure。每次提交前分别要求当前状态对应`ECCStarted→Described→Prepared→Configured`、无未决操作、身份/状态/receiver观测新鲜且部署摘要一致；操作意图先写入扫描journal，再提交Native操作，确认结果后重新读取ECC状态，只有结果和状态都吻合才推进journal。任何提交不明、失败、超时、状态不符或journal异常均停在需要检查的状态，不自动重放。

这个入口作用于固定的整个ECC系统，不是多板里单CoBo独立状态机；Configure也不会从点JSON装载或切换硬件参数。操作员必须先按正常工件发布/装载流程确认当前ECC固定工件摘要对应本扫描点。旧`RunControl::ProgressSlot(id)`的单Node兼容命令不改变Native ECC的整系统FSM边界；唯一CoBo系统快捷入口和Prepared阶段单板维护的现有范围见[ECC状态归属说明](ECC_STATE_OWNERSHIP.md)。Threshold仍无自动Run，多点调度、点工件切换、receiver Finish/seal/catalog和完整未知结果恢复仍未完成。

Linux Qt5.12.8/GCC9.3/C++11 Envoy完整目标与完整`tests/tests.pro`目标均构建成功；扫描journal、离线计划、CoBo维护和断连门槛相关9个定向测试槽通过（含QtTest init/cleanup共11/11）。新增端到端测试从Native Idle开始覆盖`adopt→describe→prepare→configure→Run→event Stop`并检查journal各状态及操作顺序，成功编译；本环境禁止创建本地TCP监听socket（`socket.bind`返回`PermissionError: Operation not permitted`），故不能运行该fake HTTP链。本轮不连接板卡、不产生DAQ/raw数据。Windows Qt5.14.2/MinGW、macOS及真实ECC/硬件未验证。

## 71.307 主窗口基础标签与禁用主操作的视觉一致性

根据实际离屏截图修正Envoy主窗口两处视觉不一致：普通字段标签现在使用应用的蓝灰色文字；禁用的Connect、Start run、Stop run统一使用禁用背景和边框，不再在不可操作时留下蓝/绿/红的按钮强调色。连接、状态机与操作使能逻辑未改变。

Linux Qt5.12.8/GCC9.3严格C++11完整Envoy目标从源码重新构建并链接成功，更新了`bin/GetDAQ`。设置独立`XDG_DATA_HOME=/tmp/getdaq-gui-data`后运行内置无连接截图选项，检查1440×960主窗口：字段文字颜色统一、禁用主按钮外观一致，日志正常写入。截图和临时用户数据已删除。构建保留了仓库既有`-Wmisleading-indentation`告警；未触发ECC、receiver、硬件或DAQ。

## 71.306 扫描单点Run Stop不再误触发普通Run收尾

新增NativeRunControl本机fake HTTP成功链回归，实际执行扫描点Run、receiver事件计数到达目标、扫描Stop及状态读回。测试期间发现单点Stop成功后原控制流会继续进入普通整次Run收尾路径，可能追加Breakup并提前结束receiver会话，阻碍后续扫描步骤。现在扫描Run的成功确认有独立分支：持久化扫描Stop结果并读回控制器状态后，推进当前点到`RunStopped`、清除本次控制操作并返回Native监控，不执行普通Run的Breakup/Finish/seal/catalog。

fake服务使用localhost和小型JSON控制元数据，不启动ECC/receiver进程、不访问板卡、不生成或传输DAQ/raw文件。Qt5.12.8/GCC9.3严格C++11完整`tests/tests.pro`目标构建成功；新增成功链及扫描journal、离线GUI入口、Native整系统控制、单CoBo生命周期、断连事件基线/Run门槛共7个定向测试通过，连同QtTest init/cleanup为9/9。此结果覆盖控制流和防止误发Breakup，不证明现场ECC/receiver行为、真实数据流、Windows Qt5.14.2/MinGW或macOS构建。

逐点自动Configure/多点推进、receiver Finish/seal/catalog及崩溃恢复仍未完成；Threshold继续不自动Run。

## 71.305 扫描Run入口明确当前单点执行边界

扫描界面此前把受限的“已配置点整系统Run”与离线多点计划放在同一组控件内，但入口名称容易被理解为会加载当前点配置并自动推进整个计划。现在增加醒目的范围说明，按钮改为“Run the currently configured point”，并在确认框说明必须先把匹配的点完整Configure到ECC；该入口不会加载工件、前进到下一点或执行receiver Finish/seal/catalog。身份核验仍须把正在运行的配置/工件摘要与当前冻结点精确匹配，阈值扫描仍无自动Run入口。

同步更新[扫描计划GUI手册](SCAN_PLAN_GUI.md)和[执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md)，区分只读检查、已配置单点Run和仍缺失的多点完整执行，清除事件轮询已接入后留存的旧描述；架构总述也已反映当前有限单点Run/事件Stop能力。Qt界面回归检查范围提示、按钮标题和tooltip；重新构建完整Widgets测试目标，并定向运行单CoBo生命周期、主界面系统状态提示和离线扫描GUI三个用例共5/5通过（含init/cleanup）。未连接ECC、receiver或硬件，无DAQ payload测试。Linux Qt5.12.8；Windows Qt5.14.2和macOS未构建。

## 71.304 修复扫描事件计数轮询被常规刷新饿死

复核71.303 NativeRunControl的定时轮询顺序时发现：每次Poll先启动ECC状态/receiver刷新，随后客户端Busy门槛会挡住receiver事件计数查询。现在在启动常规刷新之前，若整系统Running状态及receiver观察仍新鲜且HTTP客户端空闲，就先发一次事件计数查询并结束本轮；如果计数未达标，再请求新的ECC状态与receiver生命周期观察，为下一次样本建立新鲜边界。达到目标、超时或异常路径继续走原持久Stop意图/未知操作保护。

Linux Qt5.12.8/GCC9.3/C++11完整Envoy目标和完整Qt Widgets测试目标均重新编译链接成功。定向执行扫描journal、GUI整系统控制、单CoBo快捷入口、断连基线与断连Run拒绝，共7/7通过（含init/cleanup）。这些用例不直接执行NativeRunControl假HTTP Run→事件计数→Stop成功路径；该路径仍需集成验证。只用本机小型元数据，无硬件/receiver/DAQ/raw访问。

## 71.303 Native扫描点 Run 与事件计数 Stop 接入控制台

Envoy扫描窗口提供受限的“Run the currently configured point”操作。入口要求当前点为`Configured`、运行身份和事件基线已持久绑定、策略明确给出每点事件数与最长运行时长，并且没有未决journal操作；确认框说明启动的是整套Native ECC系统。NativeRunControl先保存`run-start`意图，再提交一次系统Run，等待操作结果和新鲜状态读回后才开始低频查询当前CoBo receiver累计事件数。每次计数响应都与冻结部署、agent/receiver实例和plan、CoBo来源、session及PID交叉核对。达到事件目标后先保存`run-stop`意图再提交一次整系统Stop；仅在成功回执和新鲜Idle/Ready状态读回后将点推进到`RunStopped`。超时或观察异常会请求Stop但把扫描操作留为未决，不能伪装成功；Threshold按钮保持禁用且不能创建自动Run策略。该入口不会加载点工件、自动推进或结束receiver；完整多点扫描仍未实现。

Linux Qt5.12.8/GCC C++11完整Envoy目标和Widgets QtTest目标构建成功；新增按钮门槛、Threshold禁用、断连Run拒绝以及既有扫描journal用例共6/6通过（含init/cleanup）。没有连接ECC、receiver、板卡或DAQ，也没有生成raw。构建测试目录在`/tmp`，仅保留仓库`bin/GetDAQ`现行程序。

此项尚未做NativeRunControl假HTTP服务成功链端到端测试或现场验证。它只接入单点Run和事件阈值Stop，不代表完整自动扫描：receiver Finish/seal/catalog、跨进程恢复和后续点推进仍未闭环；超时后须人工检查journal。GUI及控制元数据不进入DAQ数据传输通路。Windows Qt5.14.2/MinGW、macOS和独立Ubuntu24尚未原生验证。

## 71.302 修复已绑定扫描点在状态迁移后的身份重验

身份摘要现在只绑定固定服务/客户端/实例、配置工件、数据epoch及receiver部署，不再把可变化的控制器状态、receiver观察时间、session或PID混进摘要。首次接纳仍严格要求ECC处于Idle且接收进程未启动；恢复重验按journal状态逐一要求Idle、Described、Prepared或Configured，Configured还会把ECC状态、receiver lifecycle、session和每个CoBo的PID对应起来。服务请求/响应协议增加显式`adopt`/`revalidate`模式及预期/观察状态，旧schema 1响应和schema 1请求仍按Idle接纳解释。

Linux C++11 `ecc-journal-tests`和`scan-runtime-identity-tests`通过，包含EccStarted与Configured恢复、错误状态/实例拒绝及journal不变检查。Configured receiver以CoBo ID匹配，不依赖列表顺序。没有访问ECC服务、receiver进程、板卡或DAQ数据。

这修复的是扫描恢复时的只读身份重验，不补上ECC Describe/Prepare/Configure命令执行、扫描Run/事件数Stop、receiver收尾和catalog闭环。Threshold仍不自动Run；Windows Qt5.14.2、macOS和现场尚未复验。

## 71.301 从Reviewed计划显式接纳已运行ECC实例

扫描计划审阅后可以由操作员显式选择接纳一个已在控制台外启动的ECC实例。Qt先提示确认，不启动服务或发送硬件命令；NativeRunControl在发起只读身份请求前持久化`adopt-ecc`意图。ECC完整身份/状态/receiver响应与冻结扫描点期望匹配后，journal schema 5才把该实例记录到`EccStarted`并绑定运行身份。核验失败保留待人工对账的原意图，不能直接重试。旧journal schema 1–4仍可读取，只有使用新增接纳事件的journal写成schema 5。

Linux C++11 ECC journal测试及Qt5.12.8完整Widgets目标已构建；journal合同测试通过，GUI接纳入口与扫描journal恢复专项4/4通过。

剩余：这只是接纳已由外部启动的服务和只读绑定，不负责启动ECC进程，也未接Describe/Prepare/Configure/Run/事件数Stop、receiver收尾、catalog或逐点恢复闭环。Windows Qt5.14.2/MinGW、macOS和现场仍未验收。

## 71.299 已绑定扫描点的运行身份恢复重验

现在可对 journal 中已经绑定运行身份的当前点重新执行只读 ECC 观测，覆盖应用重启后 `EccStarted` 至 `Configured` 的恢复阶段。只有当前固定部署观察重新计算出的身份摘要与原 journal 完全一致才通过，且成功不会改写 journal。纯 C++ 正反例和 Qt5.12.8 NativeRunControl 编译通过。

剩余：扫描窗口尚未提供这个恢复入口，也没有将查询结果连入逐点运行控制。自动 Run/事件计数 Stop、receiver 收尾、catalog 验证和恢复闭环仍未完成。

## 71.298 事件基线需要匹配当前receiver生命周期

事件基线协调器现在统一核对ECC固定部署、receiver节点唯一性及可用/空闲状态、带时间戳的Ready生命周期/session、CoBo运行状态与PID；NativeRunControl保留查询前后的Ready状态和观测新鲜度保护。C++11 journal正反例测试通过，qmake还编译验证了两个改动的Qt源文件。

这收紧了自动扫描的基线安全条件，但没有完成扫描成功路径的端到端联调。GUI逐点运行、事件数停止规则、整系统Stop、receiver结束确认/catalog及恢复仍待实现。

## 71.297 ECC事件数查询客户端已补 fake endpoint 全回归

ECC客户端现有的只读 `/v1/receiver-events/{sourceHex}` 路径新增本机fake服务测试：身份先决条件、source十六进制编码、并发抑制、大于2^53的uint64计数精度、PID/source绑定、错误响应撤回和不发送控制POST均得到验证。新增用例3/3、完整ECC system client QtTest 66/66通过，Qt5.12.8/GCC9.3/C++11构建成功。

这仍未覆盖NativeRunControl从成功HTTP响应到扫描journal持久绑定的集成路径；扫描GUI和逐点Run/Stop流程继续未完成。详细结果见[验证71.297](VALIDATION.md#71297-ecc客户端receiver事件查询合同测试2026-10-23)。

## 71.296 Native事件基线查询桥已接入运行控制线程

NativeRunControl现在可对一个已成功绑定运行期ECC身份的扫描点发起只读receiver事件数查询；执行前和结果返回时均要求ECC系统保持新鲜匹配的Ready状态，并对固定agent/receiver计划、receiver session、CoBo source和PID逐项比对。通过后复用Qt-free协调器持久化基线。单CoBo主界面仍驱动整套一板ECC状态机；此接口不会新建逐板FSM或传输raw。

验证：完整qmake QtTest目标成功构建；Native事件基线断连拒绝、单CoBo生命周期和GUI控制三个专项槽共9/9通过（含init/cleanup）。全量Widgets测试本轮不是全绿：47通过、76失败，包含本机端口监听被环境拒绝及工作区/配置文档打开失败；未知原因的失败保持未解决。Linux Qt5.12.8；Windows/macOS未重建。

剩余：本轮未联调事件查询成功响应，离线扫描窗口也尚未调用该线程接口；尚无自动逐点Run控制、事件计数监测/停止、receiver Finish/seal/catalog或失败恢复闭环。事件基线查询本身不能被算作可用的自动采集流程。

## 71.295 receiver事件基线验证后持久绑定

新增Qt-free `ScanEventCountBaselineCoordinator`，将冻结执行快照、当前已配置点、已绑定ECC运行身份和低频receiver事件数响应组成一个受约束的journal操作。只有响应中的ECC部署、CoBo来源、agent、receiver instance及plan与固定期望完全匹配且计数可用时才写入schema-4事件基线；错source不改变journal，`uint64`计数以十进制精度持久化。该核心已经纳入Linux CMake与Envoy qmake构建。

Envoy状态表现在启用交替行底色和清晰的悬停/选中反馈。旧源码复核结论仍是`SM_ECC`为系统级生命周期；单板`CoBoNode::impl_*`执行硬件步骤，Configure/Start分别切换AGET慢控/快控。Native单CoBo快捷操作只在唯一板卡部署内推进同一整系统状态机；多板独立生命周期不属于当前ECC合同。

C++11 ECC journal专项可执行测试通过，Qt5.12.8 qmake完整测试目标成功构建，GUI/单CoBo状态归属/journal回归定向离屏测试6/6通过。当前仍未把该基线协调器接入NativeRunControl的事件数查询与自动Stop，也没有逐点receiver Finish、seal、catalog闭环；Threshold仍仅手动。详见[验证71.295](VALIDATION.md#71295-receiver事件基线协调器与主表交互层2026-10-23)。

## 71.294 主状态表紧凑布局与旧版单节点控制语义复核

Envoy 主状态表改成固定首屏较易阅读的列宽和交互式列宽，支持平滑横向/纵向滚动；原先按完整文本自动撑开所有列会让Native 15列状态表远超普通显示器宽度。长路径、错误详情、session与其它元数据通过单元格悬停读取；状态值与底层轮询保持原样。

对照`source/get/GetBench-software-release-20210208/src/get/rc/BackEnd.cpp`、`SystemManager.cpp`、`Node.cpp`和`CoBoNode.cpp`后确认：旧`SM_ECC`仍代表整套系统；`SystemManager`按照MuTaNT/CoBo依赖次序驱动多个`Node::List`成员。旧GUI/SOAP兼容路径可对一个Node实例调用Progress/Regress，这是节点对象操作；它不意味着并行CoBo都具有独立ECC服务状态机。当前Native ECC接口将固定部署绑定到单个系统级状态机；唯一CoBo、无MuTaNT时，行内快捷按钮仍是整套一CoBo ECC的快捷入口，多板时单板入口保留Prepared-only维护。不新增多板独立Configure/Start/Stop，因为现有ECC服务端协议和控制器没有对应的逐板运行状态/转换合同。

验证：完整QtTest目标及现有Native GUI控制范围测试通过后记录于[验证71.294](VALIDATION.md#71294-主状态表紧凑布局与旧版单节点控制语义复核2026-10-23)。这仅验证GUI元数据呈现和既有控制语义；真实硬件及多板独立生命周期并未由该UI改动实现。

## 71.293 扫描journal持久化逐点事件基线

扫描journal扩展到schema 4：在点状态为Configured且本点runtime identity已绑定后，记录一个不可变的receiver事件基线，包括ECC实例/部署快照/数据绑定、agent/receiver计划、receiver实例、source、session、PID、累计`acquisition_events`和观察时间。64位计数与时间以规范十进制字符串保存。schema 4的`run-start` intent必须有该基线；没有基线的日志重放会拒绝。推进到下一点时，清除runtime identity和旧receiver基线。旧schema 1–3继续按原格式读取。

Qt-free实验核心完整构建与CTest 15/15通过；完整QtTest目标qmake构建成功，扫描journal定向QtTest 3/3通过。当前GUI/NativeRunControl仍未创建该baseline绑定，也未连接事件轮询、达标Stop及恢复；因此这项完成的是durable journal substrate，不代表逐点自动扫描可运行。验证见[71.293](VALIDATION.md#71293-扫描journal持久化逐点事件基线2026-10-23)。

## 71.292 扫描事件数停止判定核心

新增Qt-free、C++11 `ScanEventCountMonitor`：以单次Run启动时的接收器累计事件数为基线，只比较同一ECC实例、部署快照、数据绑定、agent、计划、receiver实例、来源、session和PID。计数不可用、累计计数回退、观测时间不递增或身份变化时，结果为`InspectRequired`；达到策略事件数时才给出`StopAtEventCount`；达到最大时长时给出`StopAtMaximumDuration`并明确该点中止，不能记作成功。该类只判定结果，不读网络、不碰journal、不向ECC发命令。

纯C++11专项测试通过，实验核心CMake构建通过，qmake QtTest全目标构建通过；扫描journal、计划验证、GUI及整系统单CoBo状态专项QtTest 6/6通过。判定器还没有连接到逐点扫描执行器：当前没有扫描Run入口建立该基线和策略上下文，也没有把`StopAtEventCount`变成持久意图、整系统Stop及验证后的状态迁移。Threshold仍手动运行。因此这一项是经过测试的核心能力，不代表自动扫描/自动Stop已经可用。详情见[71.292](VALIDATION.md#71292-扫描事件数停止判定核心2026-10-23)及[扫描执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md)。

## 71.291 Envoy主控制台视觉分区与单CoBo源码对照

主窗口菜单已分成Configuration、Tools、Run、Recovery and safety四类，顶部状态、配置、Run信息和采集控制使用分组卡片，默认尺寸调整为1440×960以给状态表和日志留出空间。离屏截图已检查布局，完整Envoy构建成功，单CoBo/多CoBo GUI专项QtTest 4/4通过。

旧GetController的GetTest控制一个目标CoBo的窗口级FSM；旧GetBench的SM_ECC则是系统生命周期FSM。Native状态仍属于整套ECC；只有唯一CoBo且无MuTaNT时，快捷按钮调用整套ECC状态机。没有新增逐CoBo并行FSM，也没有改变raw数据接收路径。详细验证见[71.291](VALIDATION.md#71291-envoy主界面分区与单cobo生命周期回归2026-10-23)和[ECC状态归属说明](ECC_STATE_OWNERSHIP.md)。

## 71.289 单接收器事件数控制口观察

DataSupervisor按固定计划索引读取单个运行中receiver的`acquisition_events`，并经认证的`GET /v1/receiver-events/SOURCE_INDEX`暴露。ECC现在通过固定remote plan唯一映射来源，转发查询并复核原部署、计划、session和PID；ECC服务提供认证的`GET /v1/receiver-events/<lowercase-hex-utf8-source-id>`，Envoy低频HTTP客户端可查询并检查ECC实例、快照、数据绑定及来源身份。精确计数使用十进制字符串，保留uint64精度。真实本机receiver回环测试确认计数0→2且ECC/板端状态不变；严格协议测试和完整Envoy Qt 5.12.8构建通过。

自动停止仍未完成：NativeRunControl还没有把冻结扫描策略、当前点和累计事件基线接到此查询，也没有在达到必填事件数时提交并验证整系统Stop。Threshold继续保持手动运行。验证详情见[71.290](VALIDATION.md#71290-事件数查询向ecc与envoy转发2026-10-23)及[71.289](VALIDATION.md#71289-单接收器事件数沿本机监督控制口观察2026-10-23)。

## 71.288 接收端逐点事件计数观察基础

新receiver在保留全帧`frames`计数的同时，增加`acquisition_events`观察值：仅对frame-storage解析的MFM frame type <= 2计数，符合旧`GetTest::onDataFrameReceived`过滤拓扑帧后的计数边界。ByteStorage明确标记该值不可用，不尝试从任意字节流推断事件。该元数据是停止策略后续接线的基础，目前没有自动发出ECC Stop，也不表示采集事件数停止闭环已经完成。验证记录见[71.288](VALIDATION.md#71288-接收端逐点事件计数观察基础2026-10-23)。

## 71.286 ECC与扫描Qt-free核心全量合同回归

从当前源码完整构建`apps/ecc`及依赖的实验配置、控制协议目标，CTest 19/19通过，覆盖ECC状态矩阵/journal、扫描状态/journal、运行身份观察、配置编译及硬件计划核心。测试仅使用小型控制元数据和临时审计文件，不启动ECC服务、receiver或硬件。该结果验证的是Qt-free合同，不代表逐点扫描已接通；真实每点部署配置生成、服务启动、Run事件数停止和receiver封存仍未实现。验证记录见[71.286](VALIDATION.md#71286-ecc与扫描qt-free核心全量合同回归2026-10-23)。

## 71.285 CoBo单板维护窗口视觉整理

单板维护窗口补充信息卡片、目标摘要和结果区层级，明确单板命令属于整套ECC处于Prepared时的维护动作；重准备采用主要操作色，单次Pulser使用警示色。增强控件间距和窗口最小尺寸。只改变呈现，不改变命令参数、确认、幂等或单次提交规则。状态机对照见[ECC状态归属说明](ECC_STATE_OWNERSHIP.md)。

## 71.284 扫描计划窗口视觉与窄屏适配

离线扫描计划窗口增加统一的浅色信息层级、标题/说明卡片、输入聚焦态、主要按钮强调和结果状态卡片；较长表单放进可滚动页面，并设置合理的最小窗口尺寸。仅改善呈现与窄屏可用性，不改变离线计划、事件数必填或Threshold手动采集语义。Qt回归验证细节见[71.284](VALIDATION.md#71284-扫描计划窗口视觉与窄屏适配)。

## 71.283 Envoy逐点ECC身份查询接入运行控制线程

`NativeRunControl`现有只读运行身份路径：调用方提供扫描journal路径、plan目录、冻结快照字节和点号；控制线程重新读取并验证manifest、event-count policy及每个点工件，再从本地固定ECC客户端身份和近期匹配的identity/status/receiver观察组合完整期望。`ScanRuntimeIdentityCoordinator`验证该journal当前点已持久完成`start-ecc`且没有pending/unknown动作后才允许HTTP请求；回复只有通过完整期望核对并再次验证journal后才写入身份绑定。`RunControl`的Qt线程桥已暴露该异步调用和结果信号。

当前offline `ScanPlanWindow`还没有调用者，也没有入口建立`start-ecc` intent/完成记录；所以不能启动扫描，身份观察也无法仅凭计划页面到达。尚需安全的逐点部署/启动流程以及Describe/Prepare/Configure/Run/Stop、receiver Finish/seal、catalog绑定与崩溃恢复闭环。Threshold仍manual-only。细节见[扫描执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md)与[71.283验证](VALIDATION.md#71283-envoy逐点ecc身份查询接入运行控制线程2026-10-23)。

## 71.282 旧GetController单CoBo状态语义对照

对照旧`GetController`源码后确认：旧GUI的`Ctrl::State`是窗口级生命周期，底层`TestController`通过一个`targetEndpoint`控制单个目标CoBo（配置时内部CoBo索引为0，并遍历活动AsAd/AGET）；源码未显示一个GUI同时维护多CoBo独立FSM。旧实现会在操作完成前乐观更新`Running`，停止时阻塞等待；新GUI继续以服务端状态和操作确认更新展示。native Envoy的ECC运行状态仍属于整套ECC部署，重复显示在行上的字段已加醒目标注；选中CoBo仍通过Prepared-only维护窗口做重准备/诊断，不能当作逐板Configure/Start/Stop。详见[ECC状态归属说明](ECC_STATE_OWNERSHIP.md)。

本轮在`nativeGuiMetadataAndWholeSystemControls`增加表头范围提示断言，防止把整系统状态误认作单板状态；验证记录见[71.282](VALIDATION.md#71282-旧getcontroller单cobo状态语义对照2026-10-23)。

## 71.281 运行身份查询的 journal 先决条件协调器

新增`ScanRuntimeIdentityCoordinator`，把只读身份请求限制在当前点`start-ecc`已持久完成且尚未绑定身份的状态，并从冻结执行快照生成请求内容。响应到达后再次核对journal，再交给完整身份响应持久化桥；回归覆盖未启动拒绝、正确点请求和重复绑定拒绝。C++11 ECC journal CTest 1/1、Qt journal专项 3/3通过。它尚未接入`NativeRunControl`，更不是自动扫描执行器；真实ECC命令、事件数停止、接收端封存、catalog和重启恢复闭环仍待实现。验证细节见[71.281](VALIDATION.md#71281-扫描运行身份协调入口2026-10-23)。

## 71.280 主窗口状态提示优化

主GUI状态表现在用颜色和悬停说明标识整系统ECC状态、转移中、暂停、离线/陈旧及未知状态；运行阶段标签区分普通、运行、忙碌和需要检查。编译及`nativeGuiMetadataAndWholeSystemControls`回归通过。ECC生命周期仍是整系统控制；单CoBo按钮仍调用原有Prepared-only维护，不构造独立板卡状态机。验证边界与测试记录见[71.280](VALIDATION.md#71280-主窗口ecc状态视觉反馈2026-10-23)。

## 71.279 扫描逐点ECC身份客户端完整锚点与回归

`EccSystemClient::QueryScanRuntimeIdentity`现在只有在调用方给出完整冻结部署期望时才能调用。它先把请求点/配置/工件绑定到当前固定ECC客户端，再通过认证HTTP获取identity/status/receivers一次性响应并核对所有ECC身份、data scope/epoch和receiver节点。完整响应包可交给journal持久化桥，但Envoy现有离线扫描窗口尚未把该客户端操作与当前点的`EccStarted` journal状态联起来。

新加本机HTTP回环覆盖成功路径、请求/期望错误时不发送POST、服务端期望和冻结data scope不符时拒绝。Qt5.12.8/GCC9.3 C++11完整ECC客户端回归65/65通过，ECC journal CTest 1/1通过；临时构建清理、无DAQ raw。

下一阶段仍需建立一个受journal意图约束的扫描协调器：逐点ECC实例/配置部署、单次操作intent、Describe/Prepare/Configure、事件数Run停止、receiver Finish/seal、catalog校验及重启对账。Threshold仍manual-only。Windows本轮界面/客户端改动、macOS、Ubuntu24独立主机、实机和DAQ吞吐没有由本轮新验证。

## 71.277 GUI视觉整理、CoBo单板入口与扫描身份HTTP观察

Envoy主窗口增加统一浅色控制台样式、状态区标题/说明、表格行选中与易读表头；native状态区明确每行的ECC状态属于整套系统。选中CoBo快捷入口调用已有Prepared-only维护窗口，按固定活动目标ID优先、唯一节点名次之进行预选；目标不唯一、服务未确认Prepared、忙碌或活动时按钮不可用。native界面隐藏逐板Regress，legacy单节点Progress/Regress保留。旧版状态机及当前边界详见[ECC状态归属说明](ECC_STATE_OWNERSHIP.md)。

扫描逐点身份现在已接入ECC只读认证端点`POST /v1/scan-runtime-identity`和Envoy `EccSystemClient`。端点将固定ECC实例身份、状态和实时receiver观察作为一次事务返回；拒绝未认证/格式错误请求、并行远程观察和操作提交。客户端核对请求点、执行快照、配置/工件、服务/客户端、身份与receiver观察后才发出成功响应。扫描journal校验桥也已改为要求fresh、available、idle且无receiver session的实时receiver观察后才持久化身份摘要；Envoy尚未把该桥接到逐点操作journal或Configure/Run/Stop流程，不能称为扫描执行器完成。

Qt 5.12.8离屏完整测试程序成功构建；native GUI元数据/系统控制、真实按选中行名称预选CoBo目标、既有维护确认/状态拒绝及单板生命周期测试通过。扫描身份协议与ECC journal CTest 2/2通过，journal专项新增stale receiver拒绝且拒绝时不写journal；固定ECC集成和实际ECC HTTP进程边界测试在本机loopback模拟环境通过，临时证据约3.9 MiB并已删除，没有生成raw或访问真实板卡。文档链接检查通过。上述不代表本轮GUI改动已在Windows Qt5.14.2/macOS重建，也不代表实机或持续吞吐验收。

## 71.276 扫描身份跨进程契约、ECC服务端点与Envoy客户端接入

新增C++11点身份请求/响应严格编码。ECC服务端认证后以固定部署校验点配置/工件SHA，并将身份、状态和实时receiver清单一起作为只读观察响应；Envoy客户端核对响应与冻结执行快照及客户端锚点。接口已经接入，但扫描journal持久绑定及逐点采集执行仍未实现。首次验证时留下的客户端接线缺口已由71.277关闭。见[验证71.276](VALIDATION.md#71276-扫描点身份跨进程契约与-linux-full-回归2026-10-22)和[验证71.277](VALIDATION.md#71277-gui状态归属与运行身份http观察2026-10-23)。

## 71.275 Linux host full 当前源码构建与回归

Ubuntu 20.04.6/WSL2、GCC9.3.0/Qt5.12.8 从当前源码构建 full profile 28/28 程序，109项嵌入依赖检查通过；严格C++11配置核心151/151、Widgets122/122、native CTest25/25，候选包28程序启动 smoke通过，manifest187项。Ubuntu24隔离runner本轮在WSL启动阶段失败，Ubuntu24结论仍停留在71.223；本节不能替代独立Ubuntu、Windows/macOS或现场验收。详见[验证71.275](VALIDATION.md#71275-linux-host-当前源码-full-profile-与全套回归2026-10-22)。

## 71.274 Windows Qt 5.14.2 当前源码构建、QtTest 与运行候选

用户 Qt5.14.2/MinGW7.3 下当前源码 Windows control profile 8/8 构建通过；该构建暴露并修复了 Windows journal 后端最低 SDK 目标宏、ACL句柄`READ_CONTROL`权限、8.3短路径规范化与原子替换API，以及 MinGW 严格 C++11 下 `NOMINMAX` 重定义问题。Widgets QtTest 121通过、0失败、1项POSIX锁用例跳过，ECC client 64/64；独立Windows scan-journal-store测试覆盖私有ACL、单写者、重启隔离、硬链接拒绝和篡改检测并通过。20 GiB raw上限未触发，测试私有临时目录已删除。最终Windows运行候选含8程序/287文件，SHA/架构、SDK-free启动、offscreen及原生窗口 smoke通过，大小81 MiB。见[验证71.274](VALIDATION.md#71274-windows-qt-5142-mingw-全套当前源码构建与扫描-journal-回归2026-10-22)。

该用户主机验证不等于独立干净安装、人工现场配置、Linux ECC 服务、实机板卡或 DAQ 吞吐验收。扫描运行身份构造器仍未接入 Envoy journal，完整逐点 Configure/Run/Stop 仍在开发。macOS 原生构建尚未验证。

## 71.273 从固定 ECC 部署生成扫描点身份期望

新增 Unix 原生 `BuildFixedScanRuntimeIdentityExpectation`：它重新检查固定部署输入，从冻结扫描快照取当前点配置/工件摘要，从固定 ECC 配置推导服务/客户端 ID、启动快照、输入绑定、接收生命周期绑定、数据控制范围以及接收节点计划；只有启动实例、数据 epoch 和节点运行实例从身份/状态响应读取。响应必须与这些固定锚点一致，并通过原有 Idle/fresh 协议检查后才返回期望。固定 ECC 集成测试覆盖正确绑定、错扫描点、错客户端、错data scope、未经控制校验的offline配置拒绝，以及固定输入变更后命令不执行。使用当前 native 构建目标完成编译，并在本机回环 Ice 模拟环境完整运行 `fixed-ecc-configuration-tests` 通过；临时3.2 MiB测试证据已删除。

此原生构造器仍未接到 Envoy `EccSystemClient`/扫描journal持久化桥，也未覆盖 Windows GUI 端，因此不是逐点扫描执行器。下一步是把可信部署期望和真实 ECC 身份/状态查询接入执行服务，再落实 Configure/Run/Stop/receiver seal/catalog 闭环。Windows Qt5.14.2、macOS和实机扫描本轮未测。详见[验证71.273](VALIDATION.md#71273-从固定ecc部署生成逐点扫描身份期望2026-10-22)。

## 71.272 扫描运行身份核对 ECC 客户端 ID

运行身份期望现在必须包含固定的 ECC `client_id`，绑定摘要也记录该字段；即使服务、实例、配置和接收节点都相同，若现场服务身份响应属于另一个控制客户端，也会拒绝绑定。严格 C++11 协议专测与 ECC/journal 持久化回归通过。该字段仍由调用方提供，固定 ECC 配置推导的可信期望、Envoy真实ECC查询接入、receiver/Run逐点编排及硬件扫描均未完成；完整重构仍在进行。验证见[71.272](VALIDATION.md#71272-扫描运行身份增加ecc客户端id核对2026-10-22)。

## 71.271 Threshold自动Run策略关闭并改为人工逐点

按用户确认的规则，PeakingTime、Pulser、AutoGain有限停止策略必须显式填写每点事件数；Threshold计划目前仅离线生成和审阅，不创建自动执行策略，也不自动Run。C++策略模型已移除定时窗口策略并拒绝旧`timed_windows` sidecar；config CLI拒绝该参数；Envoy显示manual-only提示并禁用策略创建按钮。Threshold点配置和手动Run归档关联能力保留。回归增加核心对Threshold事件数/旧定时策略的拒绝、GUI Threshold策略按钮禁用检查及CLI集成拒绝；详细结果见[验证71.271](VALIDATION.md#71271-threshold自动run策略关闭并改为人工逐点2026-10-22)。

## 71.270 从执行快照生成逐点静态身份期望

新增纯 C++11 `BindScanRuntimeIdentityExpectation`，从选定执行快照派生执行快照 SHA、点索引以及该点配置/工件 SHA；调用方只提供ECC服务实例和数据节点等运行期部署字段。严格C++11 CTest 2/2、Linux Qt 5.12.8完整Widgets测试程序构建及两项扫描专项QtTest各3/3通过，文档链接1690项通过。此工厂不验证部署字段来源，真实ECC观察、receiver/Run绑定和扫描执行仍未接入。详见[验证71.270](VALIDATION.md#71270-从执行快照生成逐点静态身份期望2026-10-22)。

## 71.269 绑定桥核对执行快照中的点工件

身份持久化桥现在直接接收执行快照对象，并核对journal执行ID/快照SHA/计划SHA/点数及当前点配置、工件SHA，减少调用方拼接不一致期望的空间。严格C++11 CTest 2/2通过；Qt专项验证与文档链接检查见[验证71.269](VALIDATION.md#71269-运行身份桥直接核对冻结点工件2026-10-22)。真实ECC HTTP适配、可信期望生成、receiver/Run观察与自动扫描仍未接入，完整重构尚未完成。

## 71.268 运行身份绑定执行快照和点索引

ECC运行身份观察摘要现纳入执行快照SHA与扫描点索引；journal桥拒绝与当前journal快照/点索引不匹配的期望，且失败时不落盘。严格C++11协议/存储CTest 2/2、Qt5.12.8离屏两项专项QtTest均通过。调用方仍须安全取得这些期望，真实ECC客户端、receiver/Run观察及自动扫描尚未接入，不能视为完整执行器。详见[验证71.268](VALIDATION.md#71268-运行身份摘要绑定具体执行快照和扫描点2026-10-22)。

## 71.267 ECC身份观察验证后写入journal

新增journal存储桥，在写入前用C++11 ECC协议观察校验器核对`/v1/identity`、`/v1/status`和调用方冻结期望；不匹配时拒绝且不改变journal。POSIX journal CTest 1/1、Qt5.12.8离屏两项专项QtTest均通过。该桥尚未接入Envoy真实ECC客户端，也不生成可信逐点期望或读取运行期receiver/Run身份；没有网络、硬件或DAQ测试，不能视为自动扫描已完成。详见[验证71.267](VALIDATION.md#71267-经协议验证的运行身份观察持久绑定2026-10-22)及[扫描执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md)。

## 71.265 扫描点的运行期身份摘要门槛

新journal schema v3要求每个点在`start-ecc`完成后先保存运行期身份观察摘要，再允许`describe`等操作intent；进入下一点会清除旧绑定。journal inspect摘要也会显示快照和运行期身份SHA。严格C++11核心、POSIX存储/CLI及Qt离屏专项回归通过。调用方验证状态/身份字段的协议适配、receiver/Run身份、ECC执行及Windows/macOS原生测试仍未完成。详见[验证71.265](VALIDATION.md#71265-每点运行期ecc身份摘要门槛2026-10-22)和[执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md)。

## 71.266 ECC观察协议交叉校验

新增Qt-free `ObserveScanRuntimeIdentity`，以ECC `/v1/identity`和`/v1/status`只读响应核对调用方冻结的逐点身份/配置摘要、ECC实例与快照一致性、Idle状态、data controller scope/epoch及完整fresh receiver plan集合，并输出稳定的规范化SHA-256供journal绑定。C++11协议专测1/1通过。它不会认证HTTP服务或证明板端硬件读回；可信期望值的逐点部署、Run身份/接收周期执行、Stop/Finish/catalog绑定及自动扫描仍未实现。Windows/macOS构建未验证。详见[验证71.266](VALIDATION.md#71266-ecc运行身份观察的协议交叉校验2026-10-22)和[扫描执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md)。

## 71.264 加固旧journal快照迁移边界

带外部操作历史的v1 journal可继续读取和恢复，但回归现已明确验证它不能事后绑定新的执行快照。严格C++11扫描journal、POSIX存储和Qt5.12.8离屏审阅专项测试均通过；这加强既有输入绑定的兼容边界，不增加新的硬件/执行功能。详见[验证71.264](VALIDATION.md#71264-旧journal操作历史禁止事后补绑2026-10-22)。

## 71.263 执行journal绑定扫描输入快照

Envoy人工审阅现在要求有效的计划停止策略，并在用户私有journal目录创建/复核完整执行输入快照。最初journal v2把快照字节SHA-256和`SnapshotBound`事件持久化；随后schema v3又增加逐点运行期身份摘要门槛（见71.265）。旧v1 journal可在无操作历史时显式迁移，已有操作记录不能回填快照身份。运行期身份协议交叉校验器已在71.266补入，但仍无可信期望身份的逐点部署、ECC/receiver执行适配或自动扫描。详见[验证71.263](VALIDATION.md#71263-envoy审阅journal绑定完整执行快照2026-10-22)和[扫描执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md)。

## 71.262 扫描计划与停止策略执行快照

新增Qt-free C++11 `ScanExecutionSnapshot`及`getdaq-config create-scan-snapshot`/`getdaq-config-check scan-execution-snapshot`，将原计划、策略字节和逐点配置/工件摘要冻结到独占新建的输入快照。核心与GUI/CLI/只读复核专项测试通过。它还没有运行期ECC/receiver/Run身份；journal摘要绑定见[71.263](#71263-执行journal绑定扫描输入快照)，自动操作未实现，不能启动采集。详见[验证71.262](VALIDATION.md#71262-计划与停止策略的不可变执行快照2026-10-22)和[扫描执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md#5-停止策略与执行策略工件)。

## 71.261 Envoy提供停止策略元数据创建入口

扫描窗口现在可在成功校验计划后按扫描类型填写有限停止条件，二次确认后独占创建绑定计划摘要的sidecar，并调用独立检查器复核。Qt5.12.8/GCC9.3离屏专项QtTest通过。此改动只形成策略元数据，不触发Configure/Run，也不代表自动扫描已完成；策略消费、Threshold物理执行语义和Windows/macOS原生验证仍待完成。详见[验证71.261](VALIDATION.md#71261-envoy扫描窗口创建有限停止策略2026-10-22)、[操作步骤](SCAN_PLAN_GUI.md)及[扫描执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md)。

## 71.260 增加扫描策略的CLI创建流程

`getdaq-config create-scan-policy`现在能在完整重验离线扫描计划后，按明确的事件数/时间上限或Threshold窗口数/时长创建独立策略sidecar，并拒绝覆盖既有文件；`getdaq-config-check`提供只读复核。当前源码对真实旧配置生成的四类计划完成策略创建、有效检查及摘要篡改拒绝，进程集成100 cases通过。该进度仍无Envoy编辑窗口、ECC/receiver自动执行或硬件验收；Threshold物理流程仍待核实。详见[验证71.260](VALIDATION.md#71260-离线创建并只读核验扫描执行策略2026-10-22)和[扫描执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md#5-停止策略与执行策略工件)。

## 71.259 扫描点获得显式有限停止策略模型

新增独立`getdaq.scan-execution-policy`策略模型：事件目标扫描逐点要求事件数与运行时上限，Threshold逐点要求窗口数与窗口时长，并与离线计划SHA、类型、CoBo、点数绑定。Qt-free checker支持只读核验策略文件；C++11核心回归通过。策略尚未加入Envoy编辑器或执行流程，Threshold的触发/Pulser/结果语义仍待核实，故本项不构成自动采集或硬件扫描。详见[验证71.259](VALIDATION.md#71259-扫描停止策略的独立有限策略模型2026-10-22)和[扫描执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md#5-停止策略与执行策略工件)。

## 71.258 保持控制profile与catalog数据工具分离

控制profile不携带`getdaq-catalog`；Envoy的扫描归档入口会先搜索同目录/`PATH`，缺失时允许操作员定位单独安装的同版本数据工具，并在窗口会话内复用。这样控制包不捆绑catalog接收、传输或raw分析能力，UI仍只调用两个元数据命令。Qt5.12.8/GCC9.3/C++11完整Widgets构建及扫描相关定向QtTest 4/4通过。手动定位对话框、真实catalog、Windows和macOS未测试。见[验证71.258](VALIDATION.md#71258-控制端可定位独立catalog元数据工具2026-10-22)及[扫描归档关联指南](SCAN_RUN_BINDING.md)。

## 71.257 Envoy增加扫描点与已封存Run归档关联

已在离线扫描窗口接入catalog的单点绑定和整套扫描索引操作，支持通过`getdaq-catalog`在UI中选择点号、已完成catalog、凭据目录及新索引路径。每次操作前重新验证计划身份，复用native工具已有完整catalog校验及防覆盖边界；不触碰采集控制或raw数据。Qt5.12.8/GCC9.3/C++11测试目标完整构建，Envoy UI/扫描/journal定向QtTest 4/4通过。详细流程和限制见[71.257](VALIDATION.md#71257-envoy扫描计划窗口增加run归档关联入口2026-10-22)及[扫描归档关联手册](SCAN_RUN_BINDING.md)。Windows Qt5.14.2和macOS尚未在本轮验证；GUI选择真实现场catalog的完整人工流程也待验收。

## 71.255 Envoy可重开离线计划并继续人工审阅

为持久化审阅日志补上应用重启后的实际入口：操作员可打开已有计划目录；Envoy重新完整检查manifest、扫描网格、逐点配置语义、点文件及SHA-256，只有验证通过才恢复审阅按钮。该路径仍完全离线，不操作ECC、receiver或DAQ。Qt5.12.8/GCC9.3完整Widgets目标及同源码配置CLI构建成功，真实界面/CLI定向QtTest 3/3（含init/cleanup）通过；临时构建树已清理。Windows MinGW本机验证仍被WSL启动错误阻断，macOS未验证。逐点Configure/Run执行器和现场扫描仍未完成。见[验证71.255](VALIDATION.md#71255-重启后重新打开并审阅离线扫描计划2026-10-22)与[用户操作步骤](SCAN_PLAN_GUI.md#4-审核输出与后续采集)。

## 71.254 Envoy扫描计划人工审阅持久化

扫描计划通过现有manifest和逐点工件完整校验后，Envoy才启用人工审阅按钮。用户显式确认时，应用重新核对计划SHA/点数并在当前用户私有目录的`ScanExecutionJournalStore`记录`Planned→Reviewed`；同一计划再次确认识别为已审阅。POSIX使用专用0700目录，Windows使用为当前用户创建的受保护ACL目录。此功能不记录操作者身份/签名/外部证据，不操作ECC、receiver、硬件或DAQ。

Qt5.12.8/GCC9.3完整测试目标以严格C++11成功构建；`offlineScanPlanBuilderIsAvailableFromConsole`实际生成16点计划、点击UI确认、持久重开验证`Reviewed`并确认重复操作幂等，1/1通过。输出只包含小型JSON和配置工件，无raw。Windows原生编译/运行仍受WSL启动错误限制，macOS也尚未验收。详见[验证71.254](VALIDATION.md#71254-envoy扫描计划人工审阅持久化2026-10-22)、[journal手册](ECC_JOURNAL.md#8-只读查看命令)和[执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md)。

## 71.253 Windows扫描journal原生持久化后端

为Windows Qt/MinGW控制端加入Win32扫描journal存储：要求本地盘绝对路径、当前用户owner及只向当前用户授予访问的ACL；拒绝UNC、reparse point、多硬链接及文件身份/内容外部变化。单writer通过独占lock文件，journal更新用同目录临时文件刷新后原子替换。新增Windows专用恢复/权限回归、离线摘要CLI及GitHub Actions MinGW构建步骤；操作手册说明Qt不进入DAQ热路径。

Windows分支尚未在本机编译或运行：本机WSL启动Windows qmake之前报`UtilBindVsockAnyPort: socket failed 1`，新增Windows CI也尚未执行；POSIX CTest不能证明该后端通过。见[验证71.253](VALIDATION.md#71253-windows扫描journal原生持久化后端2026-10-22)及[操作手册](ECC_JOURNAL.md#71-windows-qtmingw-扫描journal)。

## 71.252 Widgets加入跨平台扫描journal回归

完整Qt Widgets测试现覆盖纯C++11扫描journal的intent、重启隔离、人工确认未发送、旧ID禁止复用和完成事件恢复；测试在现有Windows Qt 5.14.2 MinGW Widgets验证入口内。Linux Qt5.12.8下新增测试源码编译成功，ECC/实验核心CTest 16/16通过。WSL本轮启动Windows qmake时在版本探测阶段报`UtilBindVsockAnyPort: socket failed 1`，故Windows构建/运行尚未验证。详见[验证71.252](VALIDATION.md#71252-qt-widgets跨平台扫描journal模型回归2026-10-22)。

## 71.251 扫描协调器支持异步操作阶段

`ScanOperationRunner`现提供`PersistIntent`/`Complete`/`MarkUnknown`三阶段接口，便于后续异步Qt ECC客户端等待网络时不占用调用栈；本地journal写入及`fsync`仍同步执行，因此若严格要求GUI线程不等待磁盘，应由专用worker持有journal store。原`Execute`同步入口保留。分阶段回归确认intent先于请求、成功状态可跨重启恢复；完整ECC/实验核心CTest 16/16通过。尚未连接Qt ECC客户端或远端服务，HTTP超时需调用方按unknown保守处理。详见[验证71.251](VALIDATION.md#71251-扫描协调器异步阶段接口2026-10-22)和[执行架构](SCAN_EXECUTOR_ARCHITECTURE.md)。

## 71.250 扫描单操作调用顺序协调器

增加Qt-free C++11 `ScanOperationRunner`：先持久化操作意图，再调用一次注入的外部回调，成功后持久化响应摘要；回调抛错按结果未知处理并禁止自动重试。fake callback验证了落盘顺序、单次调用、重开隔离及完成记录写入失败时不重放，ECC/实验核心CTest 16/16通过。当前没有真实ECC/receiver/catalog/硬件适配，不能执行扫描；完整停止策略仍待现场确认。详见[验证71.250](VALIDATION.md#71250-扫描单操作调用顺序协调器2026-10-22)和[执行架构](SCAN_EXECUTOR_ARCHITECTURE.md)。

## 71.249 扫描执行journal离线检查入口

`getdaq-ecc-journal inspect-scan FILE`现可读取并输出扫描执行journal的执行ID、计划摘要、当前点/状态、事件数及pending operation标志。ECC journal真实进程回归验证其能报告重开后的`NeedsReconciliation`和原operation ID；ECC/实验核心CTest 16/16通过。检查使用独占本机锁，首次可能创建0600 `.lock`文件，不连接硬件、不修改journal状态；目前仅POSIX构建，无Windows版本或GUI检查面板。详见[验证71.249](VALIDATION.md#71249-扫描执行journal离线cli检查2026-10-22)和[操作手册](ECC_JOURNAL.md#8-只读查看命令)。

## 71.248 对账事件记录声明操作者ID

扫描journal的人工对账事件现要求填写受限格式的`operator_id`，并在严格JSON回放时验证，使将来的审计记录能保留由谁声明了结果。该字段不是身份认证、签名或防抵赖机制。ECC/实验核心CTest 16/16通过；Qt5.12.8共享qmake核心在严格警告选项下构建及help smoke通过；文档链接1648项通过。详见[验证71.248](VALIDATION.md#71248-对账事件记录声明操作者2026-10-22)。

## 71.247 扫描操作结果的显式人工对账事件

Qt-free C++11 `ScanExecutionJournal`现可将操作员对unknown operation的处理写成不可变、可重放的`operation-reconciled`事件，必须匹配原operation ID并记录观察材料摘要；确认已执行时按原命令映射推进，确认未发送时恢复原状态且禁止复用旧ID。重启后只有intent、没有completion的journal也覆盖对账后持久重开。严格C++11 ECC/实验核心CTest 16/16通过。摘要只关联外部材料，不证明其真实性或硬件状态；目前没有对账UI、证据核验服务或ECC状态查询适配。详见[验证71.247](VALIDATION.md#71247-扫描操作结果人工对账事件2026-10-22)和[扫描执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md)。

## 71.246 扫描journal POSIX持久化与未完成intent隔离

新增 `ScanExecutionJournalStore`，用于Ubuntu/macOS控制端的C++11控制元数据。它要求当前用户拥有的0700目录，以0600单journal和lock文件、单进程独占`flock`、不跟随symlink、固定文件身份/字节校验、同目录私有临时文件、文件及目录`fsync`、创建时linkat不覆盖和更新时原子rename保护持久写入；只接受当前journal的单事件直接后继，文件上限1 MiB。journal API也收紧为只有Planned→Reviewed可不带操作intent，其余生命周期边必须使用匹配的操作名/状态、请求SHA intent，然后以完成响应摘要推进；重开后只有intent而没有完成的操作会进入NeedsReconciliation，不能自动重发。GCC9.4严格C++11下ECC journal集成回归1/1通过，覆盖独占锁、正常持久重开、pending intent重开隔离、外部篡改不覆盖及symlink拒绝。该服务目前仅POSIX构建；没有Windows原生持久化实现，且无人工reconciliation、ECC API集成或硬件动作。详见[验证71.246](VALIDATION.md#71246-扫描journal-posix持久化与未完成intent隔离2026-10-22)。

## 71.245 扫描执行记录严格JSON模型

在71.244状态护栏上新增不可变`ScanExecutionJournal`值模型，记录execution ID、源计划摘要、点数及状态/下一点事件；Decode以严格JSON、精确字段集合、SHA/ID/点数范围检查后，从初态重放全部事件，并拒绝跳步、错序、未知字段及终态恢复。事件数组最多4096项、整体最多1 MiB。GCC9.4严格C++11完整实验核心CTest 14/14通过，Qt5.12.8 `getdaq-config`经共享实验`.pri`重新链接及help smoke通过。它不写磁盘、不加锁、不保障崩溃持久性，也不连接ECC或识别请求受理/结果未知；调用方未来必须将候选编码原子保存后才提交对应动作。详见[验证71.245](VALIDATION.md#71245-扫描执行记录严格json模型2026-10-22)。

## 71.244 扫描执行状态核心第一步

新增无Qt的C++11 `ScanExecutionState`，按显式的单点生命周期顺序拒绝跳步/回退，使用调用方观察到的expected状态避免陈旧状态推进；只有当前点到达`Retired`才允许进入下一点，最多256点，`NeedsReconciliation`不可自动恢复。GCC9.4 Release构建成功，专项CTest 1/1通过。此类只做内存状态转移检查，尚不写执行清单/journal、不驱动ECC/receiver、不包含持久化、崩溃恢复、停止策略或硬件验证；全量扫描执行器仍未完成。详见[验证71.244](VALIDATION.md#71244-扫描执行状态核心第一步2026-10-22)和[扫描执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md)。

## 71.243 模拟器测试raw总量保护

独立HTTP模拟器原先会按CoBo无限追加无效MFM测试`.graw`，可能因操作员忘记停止而持续占盘。现模拟root内已有/新生成raw累计受限，默认1 GiB，配置范围64 KiB–20 GiB；达到上限后只停止生成、不伪造ECC Stop或删除文件。Qt5.12.8 C++11专项测试覆盖精确预算、封存目录跨重启累计和既有Run路径，4/4通过；生产qmake目标和命令行20 GiB拒绝检查通过。详细记录见[验证71.243](VALIDATION.md#71243-模拟器生成数据设置累计上限2026-10-22)。模拟器生成内容仍不是有效MFM帧。

## 71.242 扫描逐点执行器架构审阅

新增[扫描执行器架构说明](SCAN_EXECUTOR_ARCHITECTURE.md)，按当前固定ECC工件摘要/数据接收绑定契约，明确每个扫描点需要独立控制快照、Run、receiver周期和归档绑定；Qt仍不进入raw数据面。复核旧GetController代码后，PeakingTime/Gain/AutoGain使用事件目标，而Threshold按周期Pulser采集窗口推进，执行策略不能共用未定义的默认事件数。本项只有源码审阅和架构文档，不含执行器实现；停止策略建模、故障恢复与现场验收继续待完成。详见[验证71.242](VALIDATION.md#71242-扫描逐点执行器架构核对2026-10-22)。

## 71.241 修正离线依赖操作手册计数

依赖操作手册曾将`full`本地依赖核验写成105个文件；当前固定清单和核验器实际是109个。文档现记录`full` 109、`control` 95、`data`/`analysis` 0、`data-host` 1，并要求以脚本回执为准。依赖核验器回归12/12通过，五种profile均运行通过。详见[验证71.241](VALIDATION.md#71241-修正离线依赖操作手册的文件数量2026-10-22)。

## 71.240 修正macOS扫描计划CI中的检查器路径

复核POSIX full-suite构建计划后发现，`getdaq-config-check`属于Qt-free `CONTROL`应用，发布在`full/bin`；`getdaq-system-config`由原生CMake构建，发布在`full/native`。macOS扫描计划集成原先把两者都写成`full/native`，会导致检查器无法启动。现工作流和路径合同测试改为各自正确的目录，并新增合同断言核对两程序所属构建集合。Windows构建入口合同测试19/19通过；利用本机同版本CMake构建布局确认checker与system-config的位置，真实配置示例、四种计划生成及Qt-free检查的84进程回归通过。没有在macOS runner运行工作流；详见[验证71.240](VALIDATION.md#71240-修正macos扫描集成检查器路径2026-10-22)。

## 71.239 扫描计划协议拒绝未识别字段

Qt-free扫描计划校验器现在按计划类型要求manifest和每个点对象恰好包含该版本定义的字段；遗漏字段、拼错字段、额外字段都在读取点工件前失败。`system-experiment-tests`四类计划均覆盖合法清单、配置摘要篡改、manifest未知字段和点未知字段。生产配置CLI集成测试也可选调用无Qt检查器，现Windows/macOS CI将该检查器传入，真实`.xcfg`生成的四类计划都做有效验收和未知字段负向回归。GCC9.4/C++11实验核心13/13通过；Linux Qt5.12.8配置工具与原生系统配置检查器对真实示例运行84个进程案例通过；Windows构建入口测试19/19通过。详情见[验证71.239](VALIDATION.md#71239-扫描计划协议字段精确校验2026-10-22)。本轮未执行Windows/macOS原生CI或硬件扫描；整套重构仍未完成。

## 71.238 Qt-free扫描计划核心回归

为71.237新加入的共享`ValidateSystemScanPlan`补上纯C++核心回归：使用PeakingTime、Pulser、Threshold和AutoGain四种真实计划生成器产生的完整配置工件构造manifest，检查Qt-free校验器接受完整计划、返回正确摘要，并拒绝篡改后的配置摘要。该测试在不启动Qt、不访问文件系统、不连接硬件的情况下覆盖四种计划的核心语义入口。GCC9.4/C++11增量构建成功，`system-experiment` CTest 1/1通过，临时构建树约7 MiB，随后清理。详情见[验证71.238](VALIDATION.md#71238-qt-free扫描计划核心回归2026-10-22)。Windows Qt5.14.2/macOS和整套Widgets本轮未复验；整套重构仍未完成。

## 71.237 Qt-free扫描计划检查器

扫描manifest和每点配置一致性规则已从Envoy移入共享C++11实验核心。Envoy与`getdaq-config-check scan-plan PLAN_DIRECTORY`共用同一个校验器；Qt-free CLI对扫描目录、manifest和点工件拒绝符号链接/非普通文件，并限制manifest为1 MiB、点工件为64 MiB。四类计划命令行端到端检查、错误Pulser点值和链接输入的拒绝均已验证。GCC9.4实验核心CTest 13/13通过，Qt5.12.8/GCC9.3严格C++11下两项扫描UI定向回归通过。详细证据见[验证71.237](VALIDATION.md#71237-qt-free扫描计划检查器2026-10-22)。Windows MinGW/macOS构建和完整Widgets套件未在本轮执行。

## 71.236 Envoy扫描计划逐点语义闭环

Envoy现解码每个完整系统实验工件，重新核验其内部配置摘要与实际字节摘要，并把工件中的设置逐点与manifest对照：PeakingTime核对16点表，Pulser核对完整电压网格及`amplitudeStart`，Threshold核对范围、点序、全局粗阈值和通道细阈值，AutoGain核对完整电压×活动通道网格、旧Reg3/Reg4通道顺序、唯一选中通道及Pulser幅值。所有计划还须满足点数上限，以及硬件未验证、逐点完整Configure、事件数未指定等安全边界。这个检查只证明当前目录内部语义一致，不能证明manifest来源可信或配置已写入硬件。

Qt5.12.8/GCC9.3严格C++11完整QtTest程序和`getdaq-config`均构建成功；扫描计划窗口回归真实生成四类计划，篡改Pulser manifest中的点电压被拒绝，恢复计划后四类计划全通过。malformed-plan定向回归和端到端生成回归各通过。完整Widgets套件与其他平台本轮未复跑。详细证据见[验证71.236](VALIDATION.md#71236-envoy扫描计划逐点语义闭环2026-10-22)。自动逐点Configure/Run、现场硬件读回和整套重构仍未完成。

## 71.235 离线扫描计划基础完整性门槛

先前已加入manifest安全声明、连续点序、文件大小/类型、符号链接拒绝及逐点SHA-256检查；71.236在这些门槛上补充完整工件解码和扫描值语义比对。历史验证结果见[验证71.235](VALIDATION.md#71235-离线扫描计划完整性验收2026-10-22)。

## 71.234 Run预留修正后的完整Qt回归

修正原子Run预留回归，使其检查真实Run目录父目录内没有遗留`.pending-*`暂存目录。Qt5.12.8/GCC9.3严格C++11重新构建测试程序成功；全量QtTest为120通过、0失败、0跳过。`offscreen`运行仅出现Qt布局提示；测试未连接硬件或执行DAQ，合成测试文件由测试清理逻辑处理。本轮私有构建树位于`/tmp/getdaq-runreserve-atomic`，验证后删除。此项验证不覆盖Windows/macOS构建、真实硬件、扫描逐点编排或持续DAQ吞吐。详见[验证71.234](VALIDATION.md#71234-run预留修正后的完整qttest2026-10-22)。

## 71.233 Run编号预留失败不再留下半成品目录

Envoy在发送采集命令前会先预留Run目录。此前如果保存`run.yml`或初始journal失败，正式Run目录仍已创建，造成该Run编号被残缺记录占用。现在先在同一输出父目录建立私有暂存目录，写完两份记录后再重命名发布；目标Run已存在或发布失败时不覆盖已有记录，未发布的暂存目录由Qt临时目录清理。新增回归验证无效Run配置被拒绝时正式目录不存在，并直接检查Run目录的实际父目录没有`.pending`暂存项；同时验证模拟Run生命周期和现有配置快照闭包/重试。Qt5.12.8/GCC9.3严格C++11测试程序构建成功，相关QtTest 5项通过。测试没有启动DAQ、连接板卡或生成raw文件。详见[验证71.233](VALIDATION.md#71233-run预留事务失败清理2026-10-22)。该改进只覆盖本机Run记录预留，不代表扫描逐点Configure/Run编排或整套重构完成。

## 71.232 Threshold归档分析的计划网格核验

Threshold扫描归档分析现验证manifest的闭区间、步长整除、完整点数和逐点阈值码，再与点工件中的粗/细阈值寄存器核对。合成反例在重建绑定后仍拒绝范围不一致的计划且不发布报告。严格C++11 `receiver-catalog-filesystem` CTest 1/1通过。该改动不解释hit-pattern的物理含义，也不代表阈值自动判定、逐点Run编排或整套重构已完成。证据见[验证71.232](VALIDATION.md#71232-threshold扫描网格一致性校验2026-10-22)和[扫描Run绑定](SCAN_RUN_BINDING.md#4-检查和失败边界)。

## 71.231 扫描分析的计划网格一致性校验

Pulser和AutoGain归档波形分析会核验manifest中的起止电压、步长、完整点数与逐点顺序；AutoGain还检查所选通道区间和旧Reg3/Reg4通道顺序。这样防止内部字段互相矛盾的计划通过重新绑定后输出完整分析。严格C++11 `receiver-catalog-filesystem` CTest 1/1通过，覆盖重新绑定的错误Pulser范围、AutoGain点电压被拒绝且不创建报告。证据见[验证71.231](VALIDATION.md#71231-扫描分析拒绝自相矛盾的电压计划2026-10-22)和[扫描Run绑定](SCAN_RUN_BINDING.md#4-检查和失败边界)。本项补强离线计划验证，不代表旧Gain/AutoGain运行流程迁移、硬件读回或全项目重构完成。

## 71.230 PeakingTime命令行端到端测试

catalog CTest现在把构建出的`getdaq-catalog`路径传给回归程序，实际启动`--analyze-peaking-scan`子进程，校验配置值/波形报告、已有报告不被覆盖，以及非法通道拒绝且不产生输出。GCC9.4严格C++11原生catalog套件49项通过，CTest `receiver-catalog-filesystem` 1/1通过；合成raw 208字节且fixture自动清理。该测试覆盖CLI至报告发布，不代表DAQ传输、Windows/macOS或真实硬件。证据见[验证71.230](VALIDATION.md#71230-peakingtime分析命令行端到端回归2026-10-22)。

## 71.229 PeakingTime扫描 Run 绑定归档波形响应

新增`getdaq-catalog --analyze-peaking-scan`，将此前手工映射文件的PeakingTime归档分析接入逐点Run/catalog绑定。AsAd/AGET/活动通道由操作者显式选择；PeakingTime数值从对应完整点工件读取，并使用芯片版本和点号校验旧16项表。原生MFM流式解码复用现有受限波形路径，只输出配置响应和波形统计，不执行采集、不证明硬件读回或物理性能。GCC9.4严格C++11 Qt-free构建成功，catalog测试46项通过；208字节type-1 fixture输出100 ADC、2/3 buckets并拒绝非法通道，测试后清理。Windows、macOS、真实数据/硬件与平台候选仍未验收。详情见[扫描绑定](SCAN_RUN_BINDING.md#9-peakingtime-扫描归档波形响应)和[验证71.229](VALIDATION.md#71229-peakingtime-扫描-run-绑定波形响应2026-10-22)。

## 71.228 Pulser幅值扫描 Run 绑定波形分析

新增原生 `getdaq-catalog --analyze-pulser-scan`，复用AutoGain的MFM流式解码和归档Run绑定检查，逐点核对Pulser `amplitudeStart`工件及操作者指定的活动通道，再输出波形响应统计。Pulser计划未声明通道，因此命令要求显式提供CoBo/AsAd/AGET/channel；结果不代表板端加载、Pulser实测电压或物理增益。GCC9.4严格C++11 Qt-free构建成功，catalog原生回归44项通过；208字节type-1合成数据验证100 ADC、2/3 peaking/FWHM buckets和非法通道拒绝，夹具自动清理。未验证Windows Qt5.14.2、macOS、真实采集/硬件或刷新平台交付包。整套重构未完成。见[扫描Run绑定](SCAN_RUN_BINDING.md#8-pulser-幅值扫描归档波形响应)及[验证71.228](VALIDATION.md#71228-pulser-幅值扫描-run-绑定波形响应2026-10-22)。

## 71.227 AutoGain 绑定归档波形响应分析

原生`getdaq-catalog --analyze-autogain-waveforms`现在从逐点scan manifest、Run绑定凭据和已封存catalog确定raw，不再依赖手工文件名映射；并校验manifest中的channel/Pulser mV与该点完整配置工件相符。报告每点有效/缺失/无效波形数、平均峰值ADC和peaking/FWHM buckets。GCC9.4/C++11 Qt-free构建及原生catalog 42项回归通过；type-1 rev0..5、compact type-2 rev5及`0xFF01`分层type-1子帧均输出正确统计，三种合成raw均自动清理。当前单帧上限32MiB、分层深度最多8级；不能计算/判断物理增益或合格判据。Windows/macOS、现场归档、板端读回均未验收，平台候选尚未刷新。详见[扫描Run绑定](SCAN_RUN_BINDING.md)、[AutoGain计划](AUTOGAIN_SCAN_PLAN.md)和[验证71.227](VALIDATION.md#71227-autogain-绑定归档波形响应分析2026-10-22)。

## 71.226 Gain/AutoGain归档波形响应表

`getdaq-waveform-viewer --analyze-pulser-response`按操作者映射的通道、Pulser mV和`.graw`文件汇总平均峰值ADC、peaking/FWHM buckets及帧/缺失/无效计数，可将旧Gain/AutoGain扫幅采集归档整理成响应表。它不计算物理增益、不作合格判定，也不核对扫描计划、Run/catalog绑定或Pulser真实输出。Qt5.12.8/GCC9.3/C++11完整Widgets **119/119**通过，CLI以2.3KiB合成帧完成端到端验证；文档链接1608项、Windows构建入口合同19/19通过。Windows Qt5.14.2及macOS原生构建/现场数据仍未验收。详情见[验证71.226](VALIDATION.md#71226-gainautogain-归档波形响应表回归2026-10-22)。

## 71.225 PeakingTime归档波形离线汇总

`getdaq-waveform-viewer`新增`--analyze-peaking-scan`命令，按操作者提供的CoBo/AsAd/AGET/channel及点号—`.graw`文件映射汇总有效/缺失/无效波形数、平均峰值ADC、peaking buckets和FWHM buckets。CLI使用现有原生reader和离散波形算法，不启动GUI，不触碰控制或高速接收路径。当前Qt5.12.8完整Widgets回归118/118通过；2.3KiB合成帧端到端输出预期的100 ADC、3/5 buckets。命令不验证扫描manifest、Run/归档绑定和硬件读回，因此不能用来证明点号映射正确或视为完整扫描执行器。Windows Qt5.14.2、macOS原生构建及真实归档/硬件分析仍待验收。详情见[验证71.225](VALIDATION.md#71225-peakingtime-离线波形汇总与完整-widgets-回归2026-10-22)。

## 71.224 HttpClient取消回归与QtTest门槛同步

新增挂起HTTP请求重复取消测试：回调保持不触发；Qt5.12.8完整Widgets套件117/117通过，专项重复20/20。Ubuntu24/macOS工作流的Widgets总数门槛同步增至117，Windows Qt5.14.2源构建本轮被WSL `UtilBindVsockAnyPort`互操作错误阻断，未形成编译结论。详情见[验证71.224](VALIDATION.md#71224-qt-httpclient取消用例与平台测试门槛更新2026-10-22)。完整重构仍未完成。

## 71.223 Qt网络取消边界回归与Ubuntu24全套候选

`HttpClient::Cancel()`现在先清空活动reply集合、断开回调，并只中止尚未finished的reply，避免对已完成请求重复调用`abort()`。Ubuntu24从本轮当前源码完成28程序full构建、Core151/151、Widgets116/116、CTest23/23及全部23条平台命令；四路1GiB接收和8KiB失败handoff测试均清理完毕。候选`build-packages/getdaq-ubuntu24-20261022-71-223-httpclient-cancel-candidate/`包含同一构建的28个程序和验证回执，启动smoke、清单校验通过。Qt5.12.8本机Widgets116/116，Stop后catalog路径重复20/20。此前一次未复现失败的真实触发原因仍不明，不声称此改动已被证明修复其根因。Ubuntu24验证共享WSL2宿主内核；独立目标机、物理双机、实板、Windows本轮构建、macOS原生CI和持续吞吐仍待验收。见[验证71.223](VALIDATION.md#71223-qt-网络请求取消健壮性与-ubuntu24-全套回归2026-10-22)。整套重构仍未完成。

## 71.222 Ubuntu24 当前源码全量回归与 full 候选

Ubuntu24.04.5用户空间对当前工作树执行完整`--stage all`：GCC13.3.0/CMake3.28.3/Qt5.15.13，28/28程序构建，109项嵌入依赖检查通过；Core151/151、Widgets116/116、CTest23/23及来源迁移、配置事务、receiver传输/安全、失败handoff窗口通过，23条runner命令全部退出码0。四路合成接收负载总计1GiB，自动清理1,073,741,824字节后raw剩余0；handoff raw为8,192字节并清理。由同一构建生成的候选`build-packages/getdaq-ubuntu24-20261022-71-222-current-source-full-candidate/`含28个程序、完整manifest、全套平台回执及包内程序smoke回执，清单验证和28程序loader启动smoke通过。一次早前Widgets运行的单次catalog计数失败未能稳定复现：同一用例隔离重跑20/20，之后完整Widgets和全套矩阵通过；早前失败的具体触发条件仍未知。用户空间共享WSL2内核，不能代替独立Ubuntu、物理双机、实板、Windows/macOS本轮构建或持续DAQ吞吐验收。详见[验证71.222](VALIDATION.md#71222-ubuntu24-当前源码全量回归与完整候选2026-10-22)。整体重构仍未完成。

## 71.221 Ubuntu24 当前源码 full profile 候选构建

项目专用Ubuntu24.04.5用户空间从当前源码新构建full profile 28/28程序，Qt5.15.13、109项嵌入依赖核查通过；候选包内28程序经Ubuntu24 loader启动/help合同及GetDAQ离屏首窗检查通过。候选`build-packages/getdaq-ubuntu24-20261022-71-221-full-current-source-candidate/`含187项manifest（包含SHA绑定的包smoke回执），包清单复核通过。该轮未重跑完整QtTest/CTest/数据集成矩阵；用户空间共享WSL2内核，不代表独立Ubuntu24、Windows/macOS或实机/DAQ吞吐验收。详见[验证71.221](VALIDATION.md#71221-ubuntu24-当前源码-full-profile-候选构建2026-10-22)。整体重构仍未完成。

## 71.220 Full 候选普通 TCP 归档传输回归

Linux full候选包中的目录复制工具经当前源码原生回归验证：未认证普通TCP、断点续传、回读、CLI分进程收发、拒绝服务有界退出、恶意超限元数据拒绝及SIGPIPE隔离通过。合成raw为15,269,888字节，32MiB预算内；结束后0 raw、0剩余文件，临时测试目录已清理。回执绑定候选`getdaq-catalog` SHA，详见[验证71.220](VALIDATION.md#71220-full-候选普通-tcp-归档传输回归-2026-10-22)。验证运行于WSL2本机loopback，不代表物理双机/真实网络吞吐，也没有访问硬件；Ubuntu24独立主机、Windows、macOS仍需验收。

## 71.219 Linux 当前源码 full profile 候选

当前源码的Linux host full profile从干净构建目录构建28/28程序，109项嵌入依赖核查通过，完整候选manifest 186项通过校验。包内28个CLI/程序入口合同通过，GetDAQ离屏主窗口截图启动通过。临时261MiB构建树已清理，候选87MiB且无DAQ raw。此为Qt5.12.8/WSL2 host构建，不是Ubuntu24 rootfs/Windows/macOS/现场验收。详见[验证71.219](VALIDATION.md#71219-linux-当前源码-full-profile-候选-2026-10-22)。

## 71.218 Linux 当前源码 control profile 候选

从当前源码干净构建8个低频控制/配置程序并产出Linux host候选；95项嵌入依赖扫描及166项manifest校验通过。候选内8个程序的`--help`和GetDAQ隔离offscreen主窗口截图烟测通过。此control profile不含接收/传输程序，构建来自WSL Qt5.12.8；不代表Ubuntu24 rootfs、Windows、macOS或硬件验收。详见[验证71.218](VALIDATION.md#71218-linux-当前源码-control-profile-候选-2026-10-22)。

## 71.217 扫描计划输出检查器独立回归

扫描窗口成功摘要前新增独立输出检查器：限制已知扫描类型，检查清单常规文件、元数据、连续整数索引、安全且唯一的点工件文件名，并确认所有点文件是非空普通文件。畸形输出整体报错并提示检查目录，不会以部分文件数暗示计划可用；SHA仍由原生检查器负责。Linux Qt5.12.8严格C++11完整应用与测试套件重建通过，畸形输出测试及四类实际窗口/CLI回归合计4/4。Windows/macOS和现场验收仍待完成。详见[验证71.217](VALIDATION.md#71217-扫描计划输出检查器独立回归2026-10-22)。

## 71.216 离线扫描计划完成摘要

扫描窗口成功后会读取manifest，显示scan kind、CoBo、计划点数和点工件在场数；检查schema/index、点名安全、非空常规文件和符号链接，不重算SHA，完整工件仍由原生检查器校验。Linux Qt5.12.8/C++11离屏真实窗口回归依次生成PeakingTime 16点、Pulser 3点、Threshold 3点、AutoGain 6点，通过3/3。无硬件/raw访问，临时构建已清理。详见[验证71.216](VALIDATION.md#71216-离线扫描计划完成摘要和输出检查2026-10-22)及[窗口手册](SCAN_PLAN_GUI.md)。

## 71.215 GetController `hittedChannel` 分支范围核对

旧默认工作区测试类型清单不含`hittedChannel`。线程分支仅展示固定Pulser幅值注入、事件等待及Reg3/Reg4逐通道切换，没有该分支专用判据或结果输出；AutoGain单幅值配置计划只表达近似配置点，不等价于其在线执行。源码静态证据与迁移边界见[验证71.215](VALIDATION.md#71215-getcontroller-hittedchannel-分支范围核对2026-10-22)和[扫描计划手册](GETCONTROLLER_SCAN_PLAN.md#hittedchannel-分支的迁移边界)。没有硬件/DAQ测试；现场使用情况待确认。

## 71.214 GetController AutoGain 离线配置计划

新增 `BuildAutoGainScan`、`getdaq-config plan-autogain-scan` 和操作台 AutoGain 模式。计划按明确CoBo/AsAd/AGET和活动通道闭区间生成逐点完整系统工件，按旧TestController的Reg3/Reg4单热位顺序遍历通道，通道内升序扫描Pulser整数mV；每点只选择一个通道，双摘要重新计算，最多256点。完整配置/CLI/GUI/manifest/Run绑定和汇总链均覆盖此scan kind。它不执行事件采集、Gain计算/判定、Run自动协调或失败恢复。

Linux/WSL Qt5.12.8 CLI及完整GetDAQ主界面重新构建通过，主界面离屏3秒启动检查无崩溃；68个CLI/native-checker进程集成场景通过；严格C++11实验核心测试验证全部68通道的Reg3/Reg4位映射和旧通道顺序；receiver catalog回归38项原生检查通过，包含AutoGain点绑定和完整计划汇总，使用的测试`.graw`夹具最大512MiB且为稀疏文件；离屏真实Qt窗口AutoGain用例通过，Windows工作流合同19项通过。320字节AutoGain归档合成raw由测试夹具清除。Linux/Ubuntu24、Windows/macOS发布候选未刷新，用户Windows原生构建及真实硬件/DAQ多点工作流没有本轮验收。详情见[AutoGain计划手册](AUTOGAIN_SCAN_PLAN.md)和[验证71.214](VALIDATION.md#71214-autogain-离线配置计划核心与界面验证-2026-10-22)。

## 71.213 Threshold CLI 收据分类修复及 Ubuntu24 候选

Ubuntu24程序增量刷新现在将 `getdaq-catalog` 的阈值分析回归记入专用 `catalog_threshold_analysis`，不再误用普通TCP传输回执；更新catalog后旧二进制对应的catalog测试回执会被失效清理。收据校验、刷新器测试与Windows构建入口合同通过。阈值离线分析已进入Ubuntu24增量候选，包内文档与193项manifest完整性通过，Ubuntu24 loader下包内CLI回归及320字节raw夹具清理通过。详见[验证71.213](VALIDATION.md#71213-阈值扫描分析的-ubuntu24-候选交付2026-10-22)。这不是独立Ubuntu验收；Windows/macOS、实板及整个重构仍待完成。

## 71.212 阈值扫描绑定归档的离线计数

`getdaq-catalog --analyze-threshold-scan`现能按threshold计划点、已验证Run绑定和封存catalog自动选取对应来源的`.graw`，流式统计MFM帧头68通道hit位并生成不可覆盖JSON。它核对扫描点阈值码确实来自完整点工件，逐个核验catalog与文件身份；操作员仍须明确提供MFM数值CoBo ID，因为计划源ID（如`C0`）不等于帧头数值，且需现场确认二者映射。目录回归1/1通过，包含合法重绑后的错误阈值标签拒绝、截断catalog拒绝和不生成失败报告；分析raw夹具320字节并已删除。Linux host full profile当前源码构建28/28程序，185项候选清单校验通过；候选内`getdaq-catalog`绑定点CLI回归通过。Ubuntu24增量候选交付在71.213验证。结果不证明硬件加载、不作物理阈值判定。Windows/macOS候选尚未更新。详见[扫描Run绑定和分析手册](SCAN_RUN_BINDING.md#6-aget-threshold-离线-hit-pattern-分析)及[验证71.212](VALIDATION.md#71212-阈值扫描绑定归档后的自动-hit-pattern-汇总2026-10-22)。

## 71.211 Threshold 扫描 hit-pattern 离线多点汇总

`getdaq-inspect --hit-pattern-scan`可将操作者明确映射的多个raw文件按升序阈值码聚合，输出逐码、逐通道的MFM头hit位计数和匹配帧比例；支持同一扫描点的多文件累加，并拒绝乱序编码/非法输入/计数溢出。整个工具仍是纯C++11离线数据面工具，不引入Qt，不改raw，也不做硬件操作。映射不是由manifest/catalog自动验证，旧代码的物理阈值判据仍未确认，因此结果不作自动通过/停止判定。完整数据CTest 6/6及合成大小端多文件CLI回归通过，详见[验证71.211](VALIDATION.md#71211-threshold-扫描文件的-hit-pattern-多点汇总-2026-10-22)和[数据服务手册](DATA_SERVICE.md#9-离线检查与异常恢复)。

## 71.210 原生离线 MFM hit-pattern 计数

`getdaq-inspect --hit-pattern FILE COBO_ID ASAD_ID AGET_ID`新增纯C++11、无Qt流式离线计数，可按CoBo/AsAd/AGET精确筛选type-1 revision 0..5帧，汇总帧头72位容器的前68个通道命中次数和帧比例。输入只读，最大帧32MiB，内存不随文件大小增长；实现复用原生MFM分帧器，不接入接收或控制数据面。合成大小端帧单元回归、实际CLI小帧夹具通过；raw不足1KiB且在临时目录清理。此项只统计头位，不等于旧Threshold物理判定，且不支持`0xFF01`嵌套/compact type-2；现场数据和自动逐点Configure/Run仍待。详情见[数据服务手册](DATA_SERVICE.md#9-离线检查与异常恢复)及[验证71.210](VALIDATION.md#71210-原生流式hit-pattern离线计数2026-10-22)。

## 71.206 扫描Run归档整套完整性索引

`getdaq-catalog --summarize-scan SCAN_DIRECTORY BINDING_DIRECTORY OUTPUT_JSON`现在可将PeakingTime/Pulser计划的全部点位及已完成Run归档汇成只读JSON索引。它逐点重建并核验绑定凭据，重新检查每个catalog，不接受缺失/过期凭据、重复Run或session，最终输出独占创建且包含扫描manifest和归档回执摘要的完整索引。没有硬件命令或`.graw`载荷读取。GCC9.4严格C++11构建和catalog文件系统CTest 1/1通过，包含成功汇总、拒绝覆盖和过期凭据测试。仅有一个点绑定的合成完整计划夹具，不是实际多点采集/分析验收。逐点自动Configure/Run、失败恢复、Gain/AutoGain/Threshold测量及分析仍未完成。详见[扫描绑定及汇总手册](SCAN_RUN_BINDING.md)与[验证71.206](VALIDATION.md#71206-扫描run归档整套完整性索引2026-10-22)。

## 71.204 扫描点与封存Run关联凭据

`getdaq-catalog --bind-scan-point`现在可在扫描点采集完成、接收归档完成封存后，核对扫描工件SHA与catalog固定接收计划的配置/工件SHA，并生成不可覆盖的独立关联JSON；凭据还绑定Experiment、Run number、receiver session及catalog请求键/manifest/完成回执。验证包含正确点、错误点、已有输出保护和封存目录不变。它是事后记录工具，没有DAQ或硬件控制，也不证明配置已在板端真实加载。见[扫描绑定原理和操作手册](SCAN_RUN_BINDING.md)与[验证71.204](VALIDATION.md#71204-扫描点绑定已完成-run-归档凭据2026-10-22)。自动逐点Configure/Run协调、失败/恢复状态机、自动采集结果和分析仍待实现。

## 71.203 macOS CI 加入配置扫描集成

macOS full-suite现在从同轮full构建产物执行配置编译器/原生工件检查器回归，涵盖PeakingTime和Pulser配置计划并上传回执。CI合同16/16和workflow YAML解析通过；Apple runner尚未运行，macOS native证据仍缺。详见[验证71.203](VALIDATION.md#71203-macos-full-suite-纳入配置扫描-cli-回归2026-10-22)。

## 71.202 Linux host 当前源码 full 候选与扫描回归（2026-10-22）

Linux host当前工作树full profile构建28/28程序成功，109项嵌入依赖检查通过；候选`build-packages/getdaq-linux-host-20261022-71-201-pulser-scan-full-candidate/`完整清单184项。候选28程序help/启动smoke和59进程PeakingTime/Pulser CLI集成通过，包内程序和两类测试回执均已纳入manifest，`raw_remaining=0`。Ubuntu24 71.201候选仍是仅替换配置工具的增量包。此结果共享WSL2宿主内核；不是独立主机、macOS、Windows新版本、现场板卡/旧服务或持续吞吐验收。详见[验证71.202](VALIDATION.md#71202-linux-host-full-候选纳入-pulser-扫描计划2026-10-22)。

## 71.201 Pulser扫描计划进入Ubuntu24交付候选（2026-10-22）

Ubuntu24.04.5用户空间loader下，扩展后的配置集成回归59/59进程通过。新候选`build-packages/getdaq-ubuntu24-20261022-71-200-pulser-scan-candidate/`以71.190 full包为基线，仅增量替换`getdaq-config`，其余27程序复用；28程序包内启动smoke及186项manifest验证通过，配置扫描回执已归档。用户空间共享WSL2内核，不能称作独立Ubuntu或全套当前源码重编译。Windows Actions新回归尚待执行，macOS原生、现场设备/旧服务、双机及持续DAQ仍未验收。详见[验证71.201](VALIDATION.md#71201-pulser-扫描计划刷新-ubuntu24-候选2026-10-22)。

## 71.200 Pulser amplitude 离线扫描计划器核心（2026-10-22）

PeakingTime、Pulser幅值和AGET阈值现均有独立的离线完整配置计划生成器。`getdaq-config plan-threshold-scan`支持显式 CoBo/AsAd/AGET 与0..127闭区间；核心边界测试、64个CLI/native-checker进程场景及完整Widgets 115/115回归均通过，证据见[验证71.209](VALIDATION.md#71209-threshold-离线计划生成与完整-widgets-回归-2026-10-22)。Windows配置扫描入口及macOS full-suite调用同一集成runner，但尚无本轮Windows/macOS原生执行结果。阈值工具不采集事件、不统计`hitPat_0`；旧数据模型已静态解释72位容器和68个实际通道的关系，但`hitPat_0`阈值物理解释和下游结果消费仍待确认。见[AGET阈值计划](THRESHOLD_SCAN_PLAN.md)。

## 71.199 PeakingTime按CoBo ID的核心回归增强（2026-10-22）

系统实验核心测试现把目标CoBo放在配置列表第二项，并逐点将完整系统参数与预期配置比较，验证只有该板活动AGET/ASTRE PeakingTime变化；未知、禁用CoBo拒绝也按ID选择检查。严格C++11 `system-experiment-tests` 构建和运行通过。没有硬件或数据传输，Windows原生Actions仍待执行。详见[验证71.199](VALIDATION.md#71199-peakingtime-单cobo选择按id覆盖回归2026-10-22)。

## 71.198 PeakingTime计划跨平台交付推进（2026-10-22）

单CoBo PeakingTime计划已用54个CLI进程场景在Ubuntu24用户空间loader下验证；Ubuntu24 full候选只增量替换`getdaq-config`，其余27个程序复用71.190候选，并通过完整性和28程序启动检查。该动作不代表Ubuntu24从当前源码全量重编译。Windows Qt5.14.2原生CI现包含相同扫描计划集成，并上传回归证据；工作流合同16/16，但本机WSL无法启动qmake且Actions尚未运行，不能宣称Windows通过。完整记录见[验证71.198](VALIDATION.md#71198-peakingtime-计划接入-ubuntu-24-包与-windows-ci-2026-10-22)。逐点ECC Configure/Run协调、Gain/AutoGain/Threshold迁移、现场硬件及持续DAQ验收仍未完成。

## 71.193 隔离 macOS QtTest 用户目录与缓存（2026-10-22）

macOS QtTest此前只隔离`TMPDIR`，配置核心使用的Qt缓存仍可能写入runner默认HOME。现在HOME、XDG缓存/配置/数据/运行目录和TMPDIR都位于`qt-test-temp`私有根下，并将运行时目录权限设为0700；已有`if: always()`清理器会在结束/失败后统计raw并删除整棵树。入口合同和workflow结构测试通过；本机Qt5.12.8严格C++11重建Core测试，并在全部路径隔离的环境运行151/151，复用同一个清理helper后回执确认0 raw、临时根已删除。该验证不是Apple runner执行证据，仍需真实Actions结果。详见[验证71.193](VALIDATION.md#71193-隔离-macos-qttest-用户目录与缓存2026-10-22)。

## 71.192 macOS CI 补入配置核心 QtTest（2026-10-22）

macOS full host CI 现在和 Ubuntu24 一样构建并运行 `tests/xcfg_shared.pro` 配置核心套件；验证 Core 151 项、Widgets 114 项均全部通过，ECC 客户端要求至少一项通过且零失败。Core 的构建收据、源码摘要、日志和测试结果纳入 Actions 产物。Linux Qt5.12.8 本地按严格 C++11 构建该Core目标并通过151/151；CI入口/清理/交付合同测试及workflow YAML解析通过，当前Linux/Windows/Ubuntu24候选的文档均已刷新，程序和运行库保持原样。此处尚未取得Apple runner执行证据，macOS原生构建仍待Actions实际运行。详见[验证71.192](VALIDATION.md#71192-macos-ci-补入配置核心-qttest2026-10-22)。

## 71.189 Linux host 当前源码全套构建和 receiver 导出回归（2026-10-22）

Linux host/WSL2从当前工作树构建full profile 28/28程序，503项输入构建回执及180项候选清单完整性通过；候选28程序启动smoke全通过。新receiver CTest 5/5，候选包内receiver/Sentry真实进程loopback集成覆盖精确导出、32MiB慢观察者隔离、封存、故障输入和运行进程观察；raw测试数据按脚本清理。详情见[验证71.189](VALIDATION.md#71189-linux-host-当前源码-full-候选与接收导出回归2026-10-22)。这是WSL2共享内核和本机loopback证据，不是独立机器、物理双机、实板或持续吞吐验收。

## 71.190 Linux host 当前候选四路16GiB合成接收回归（2026-10-22）

对71.189候选中的精确接收器SHA完成四路各4GiB、合计16GiB localhost合成接收；全量文件逐字节SHA、1GiB轮转、自然EOF、正常退出和durable seal通过，单进程峰值RSS 20,884KiB。全部16个raw共17,179,869,184字节在校验后删除，收据确认剩余0；工作区复核无raw。详细限制及证据位置见[验证71.190](VALIDATION.md#71190-linux-host-候选四路16gib合成接收回归2026-10-22)和[接收器性能说明](RECEIVER_PERFORMANCE.md)。这是小于物理内存、单主机WSL2/localhost的Python合成负载，不是生产DAQ/网卡/磁盘吞吐或板卡验收。

## 71.191 Ubuntu24 当前源码 full suite 与候选重建（2026-10-22）

经项目标准runner在Ubuntu24.04.5用户空间对当前源码执行完整验证：GCC13.3.0、CMake3.28.3、Qt5.15.13，full profile 28/28程序；Core151/151、Widgets114/114、CTest22/22及迁移/事务/receiver/交接测试全部通过，平台23条命令均成功。合成wire总计1,073,799,755字节，最终raw清理为0。由同一构建精确二进制生成的当前候选28程序/180项manifest通过包完整性及全部程序Ubuntu24 loader启动smoke。详见[验证71.191](VALIDATION.md#71191-ubuntu24-当前源码-full-suite-与候选重建2026-10-22)和[Ubuntu24手册](UBUNTU24_VALIDATION.md)。该用户空间共用WSL2宿主内核，不是独立Ubuntu、物理双机、硬件或持续DAQ吞吐验收。

## 71.188 Windows 配置发布文件提交（2026-10-22）

配置发布器在Windows的每个发布文件写入后、权限设为只读后均调用CRT `_commit`，失败则停止；Unix继续使用文件`fsync`和目录`fsync`。Qt 5.14.2/MinGW原生严格C++11发布器QtTest 5/5通过，含旧XCFG工件完整发布/读回、拒绝覆盖、来源变化拒绝和无效profile不留目录。随后当前源码Windows control profile 8/8程序构建成功，Widgets QtTest 113通过/0失败/1项POSIX目录锁用例跳过，ECC客户端64/64通过。新候选`build-packages/getdaq-windows-qt5142-20261022-71-188-publication-commit-candidate/`的SDK-free帮助、离屏/原生窗口启动和280项包完整性验证通过；测试raw清理后为0。此改进不证明Windows目录项断电持久性或真实掉电恢复。接收器及DAQ路径不变。详情见[验证71.188](VALIDATION.md#71188-windows-配置发布器文件提交2026-10-22)及[Windows操作手册](WINDOWS_QT5142.md)。

## 71.187 Windows 配置路径迁移核心与恢复回归（2026-10-22）

使用用户当前Qt 5.14.2/MinGW 7.3工具链按严格C++11构建配置核心和迁移进程探针；原生Windows核心QtTest为150通过、0失败、1项POSIX目录权限夹具跳过。Windows原生NTFS进程回归覆盖12种中断后的原始/候选恢复方向及两个活跃写者场景，全部通过，使用小型XML夹具且无raw。详细输出见[验证71.187](VALIDATION.md#71187-windows-配置路径迁移核心与中断恢复回归2026-10-22)。本轮没有运行完整Widgets/ECC套件；这不是独立干净Windows、macOS、旧服务/实板或DAQ吞吐验收。

## 71.186 在线导出发布路径缩短持锁区（2026-10-22）

`getdaq-receiver` 的在线导出分支原先在发布互斥区内分配消息并复制完整MFM帧。现在先在锁外创建一份带原有小端uint64长度头的共享消息副本，再在锁内检查每客户端字节/队列预算并入队；单帧已超过预算时直接标记当前客户端断开，不复制载荷。网络发送、socket关闭及连接容器仍由导出工作线程负责。该改动降低大帧复制时占用连接表锁的时间，但复制依旧消耗接收写盘线程CPU和内存带宽，不构成吞吐或延迟提升的实测结论。

GCC 9.4/C++11构建成功；定向 `exporter-lock` 回归在默认沙箱首次因无法创建loopback socket而于测试断言前失败，获准的 `127.0.0.1` 重跑为1/1通过。随后 `tests/receiver_integration.py` 全部数据面场景通过：分片与合并输入、双观察端精确帧和封存、慢观察端断开时32MiB合成raw文件SHA保持一致、损坏尾帧失败可见；锁测试还覆盖了超预算帧不入队并断开观察者。两个临时测试目录由脚本自动清理。最高单场景合成raw 32MiB，低于20GiB。8.8MiB临时构建已删除；本次未重建平台候选，也未做macOS、物理双机、真实分析客户端或持续DAQ吞吐验收。详细依据见[验证71.186](VALIDATION.md#71186-在线导出帧复制移出发布锁2026-10-22)。

## 71.185 复核旧 MuTaNT 对齐命令范围（2026-10-22）

旧 `mutant-align`/`mutant-align-multishelve` 调用当前 MuTaNT 的 TDC AlignmentServer；多机箱版本由操作者分三步运行。旧 `findSlotOfCoBo` 虽以选定 CoBo 为入口，但会切换它的反馈链路、扫描所有 MuTaNT 的结果，再恢复Normal；异常中断不会保证恢复链路。新版将对齐限制在完整拓扑预检和审计中，并由系统 Prepare 协调；单板维护重准备与TDC realignment不同，按2018教程保留其他板和既有对齐结果。没有增加可能错误暗示单板隔离的即时按钮。该结论由收到的GetBench源码静态核对，没有硬件操作或新构建/测试。详见[旧命令迁移核对](LEGACY_ECC_COMMAND_COVERAGE.md#对齐命令的范围源码复核-2026-10-22)和[系统对齐](SYSTEM_ALIGNMENT.md)。

## 71.184 Windows Qt5.14.2/MinGW 纳入最新查看器（2026-10-22）

用户当前Windows机器用Qt5.14.2/MinGW7.3从当前源码严格C++11构建8个control程序；Widgets QtTest113通过、0失败、1项Unix目录锁测试跳过，ECC客户端64/64通过，raw清理为0。Windows候选包八程序/运行库完整性通过；SDK-free帮助、offscreen、GetDAQ原生窗口及Viewer曲面旋转smoke全部通过，曲面画面变化96,353像素。Qt仍不进入DAQ receiver数据路径。独立干净Windows、macOS原生构建、实板、物理双机和持续吞吐仍待验收。详见[验证71.184](VALIDATION.md#71184-windows-qt5142mingw-当前源码构建与三维查看器验证2026-10-22)和[Windows手册](WINDOWS_QT5142.md)。

## 71.183 Ubuntu24 当前源码 full suite 与候选（2026-10-22）

Ubuntu24.04.5项目用户空间按当前源码完成full 28程序构建；Core151/151、Widgets114/114、CTest22/22、迁移/事务、receiver传输/安全及四路接收场景通过。合成wire数据1,073,799,755字节（约1GiB），低于20GiB上限，raw清理余量0。Ubuntu24候选由同次构建精确二进制生成，28个包内程序启动检查、viewer 3D旋转烟测和180项manifest均通过。335MiB临时树归档约2.8MiB必要证据后已删除。Ubuntu用户空间共享WSL2内核；不代表独立机器、macOS、物理双机/板卡或持续吞吐验收。详见[验证71.183](VALIDATION.md#71183-ubuntu24-当前源码-full-suite平台候选与清理2026-10-22)。

## 71.182 Linux host full profile 包含最新查看器的候选刷新（2026-10-22）

Linux host当前源码从头构建full profile 28/28成功，498项输入构建回执与28程序包内启动smoke均完整；新候选`build-packages/getdaq-linux-host-20261022-71-182-current-source-full-candidate/`初始180项manifest校验通过。Qt离屏启动检查程序包含71.181可旋转曲面查看器；测试不生成raw。五个核心数据程序的动态依赖无Qt/Ice/yaml/Boost/curl；临时构建树已删除。此次没有重跑CTest/完整Widgets套件，Linux结果基于WSL共享内核，未刷新Ubuntu24/Windows/macOS候选，亦不构成实机/双机/持续吞吐验收。详见[验证71.182](VALIDATION.md#71182-linux-host-full-profile-刷新离线查看器候选2026-10-22)。

## 71.181 波形查看器补回可旋转三维曲面（2026-10-22）

离线 `.graw/.gmat` 查看器现在可将当前 AGET 绘制为桶号×通道×信号值的可旋转曲面；未选通道置零，PNG导出包含当前视图。实现基于现有Qt Widgets/QPainter栅格绘制，不使用QwtPlot3D、Qt OpenGL绘制API或ROOT库，不新增这些库的直接链接依赖，也不进入DAQ接收链路。Linux Qt5.12.8/GCC9.3严格C++11构建、帮助及合成内存波形的离屏绘制/鼠标拖动smoke通过（旋转前后96,353像素变化）；没有生成raw。此次未重跑完整Widgets套件或刷新Ubuntu24/Windows/macOS候选，真实实验文件及人工视觉检查未完成。详见[验证71.181](VALIDATION.md#71181-grawgmat-查看器补回可旋转三维曲面2026-10-22)及[波形查看器手册](WAVEFORM_VIEWER.md)。整体重构未完成。

## 71.180 波形查看器恢复按 AGET 多通道叠加浏览（2026-10-22）

旧 CoBoFrameViewer 在所选 AGET 页同时绘制所选通道；当前离线查看器已改为 0–67 通道多选，默认全选，按通道着色并显示图例。type-1 `.graw` 单帧只解析一次，所选稀疏空通道会被跳过；type-2 保持完整 AGET 数据校验；`.gmat` 只读取所选通道。PNG导出当前多曲线，CSV逐样本写入对应通道号。FWHM/peaking time仅在只选一条曲线时显示。

Linux Qt5.12.8/GCC9.3 C++11查看器构建、`--help`及offscreen启动通过；多通道type-1/type-2、稀疏通道、非法重复/空选择和多通道CSV专项通过。按最终源码重建的Widgets完整QtTest为114/114（启用localhost，沙箱默认网络权限下有13项仅因监听失败）；测试夹具位于`QTemporaryDir`，没有使用DAQ大文件。Ubuntu24、Windows Qt5.14.2和macOS候选尚未刷新，真实实验文件及实体DAQ仍未验证。详见[验证71.180](VALIDATION.md#71180-波形查看器恢复-aget-多通道叠加浏览2026-10-22)与[查看器手册](WAVEFORM_VIEWER.md)。

## 71.179 波形查看器补回逐帧导航和自动扫描（2026-10-22）

离线查看器新增Previous/Next、首尾循环、Auto scan及50 ms至5 min可调间隔，补回旧CoBoFrameViewer的逐帧浏览和定时扫描主用法。播放器只在文件成功打开后启用，不连接设备、不读取网络或DAQ输入流。导航索引边界和单帧循环进入Widgets QtTest；C++11/Linux Qt5.12.8查看器构建、`--help`、offscreen窗口启动通过，完整Widgets回归114/114。同步更新Ubuntu24 runner的Widgets数量门槛及操作文档；本轮没有重建Ubuntu24/Windows/macOS候选，实体采集文件和硬件均未测试。详见[验证71.179](VALIDATION.md#71179-波形查看器逐帧导航与自动扫描2026-10-22)。

## 71.178 Windows Qt CI 纳入第八个查看器程序（2026-10-22）

统一 `CONTROL` 构建清单已有8个Windows程序（7个Qt程序和原生`getdaq-supervisor-query`），但GitHub Actions的PowerShell receipt gate仍只期望7个，遗漏`getdaq-waveform-viewer`，导致成功构建后也会失败。现已让PowerShell receipt gate直接读取`windows-build-plan.json`中的程序集合，再与构建回执核对；契约测试同时确认该计划由`CONTROL`构建清单生成，避免维护第二份手写名单。`tests/test_windows_build_entrypoints.py`的16项构建入口测试通过，workflow YAML解析通过，Markdown本地链接1481项通过。本轮只验证CI定义，不是Windows原生编译结果。详见[验证71.178](VALIDATION.md#71178-windows-qt-ci清单修正与契约回归2026-10-22)。

## 71.177 Linux host 全套候选刷新波形查看器导出（2026-10-22）

在本机Ubuntu 20.04.6/WSL2 host（GCC 9.4、Qt 5.12.8）从当前源码重新构建full profile全部28个程序，构建回执绑定502项输入；生成并验证候选`build-packages/getdaq-linux-host-20261022-71-176-current-source-full-candidate/`，清单180项。候选包内28个程序逐个执行启动smoke，GetDAQ离屏首窗截图正常，所有CLI按各自帮助行为退出；无硬件访问、无raw残留，私有smoke目录自动清理。候选包含71.176波形PNG/CSV功能。确认包和smoke回执后，删除本轮255MiB临时编译树。它验证Linux host构建与包内启动，不是Ubuntu24、独立干净主机、macOS、现场板卡或持续DAQ吞吐验收；Ubuntu24候选仍停留在71.172，Windows native qmake仍被WSL互操作启动错误阻断。详细见[验证71.177](VALIDATION.md#71177-linux-host当前源码全套候选与28程序启动smoke2026-10-22)。

## 71.176 波形查看器增加PNG/CSV导出（2026-10-22）

当前选中的`.graw`/`.gmat`波形现在可导出为2倍尺寸PNG或包含样本元数据的CSV；写入采用临时文件原子提交，防止失败时破坏已有目标。`.gmat`浮点值完整显示，只有有效整数CoBo ADC样本才参加沿用旧版定义的FWHM/peaking-time分析。Linux/WSL Qt5.12.8/GCC9.3 C++11构建、帮助/offscreen启动和Widgets全量回归113/113通过，导出测试覆盖CSV转义与样本顺序、17位浮点精度、非法样本、旧文件保护及PNG尺寸。Windows构建入口16/16和本地文档链接检查1479项通过；Windows qmake启动被WSL互操作`UtilBindVsockAnyPort: socket failed 1`阻断，尚无该平台编译结果。本轮未刷新Linux/Ubuntu24/Windows/macOS候选；真实实验文件差分、实板及持续DAQ吞吐仍未验收。见[验证71.176](VALIDATION.md#71176-离线波形图像csv导出与widgets全量回归2026-10-22)和[波形查看器手册](WAVEFORM_VIEWER.md)。

## 71.175 当前源码 Widgets 全量回归补验（2026-10-22）

针对71.174波形脉冲分析改动，使用Linux/WSL2 Qt5.12.8/GCC9.3从当前源码重建完整Qt Widgets测试目标，并在正确 `tests/` 工作目录、隔离临时HOME/缓存且允许loopback的环境运行全套：112通过、0失败、0跳过。波形FWHM/peaking time专项测试包含在总数中。普通沙箱因HOME缓存写入和TCP监听边界分别出现伪失败；修正测试运行环境后完整通过。临时构建/日志已删除，本次未留下`.graw`。本项只补充当前Linux/WSL2测试证据，没有刷新Ubuntu24/Windows/macOS候选，实体板卡、独立主机和持续DAQ吞吐仍待验。详见[验证71.175](VALIDATION.md#71175-当前源码-qt-widgets-全量回归2026-10-22)。

## 71.174 离线波形查看器迁移旧版脉冲宽度分析（2026-10-22）

旧 CoBoFrameViewer 中已启用的 peaking time 与 FWHM 现已迁入 `getdaq-waveform-viewer` 当前单通道视图。使用旧阈值（5%上升沿、50%半高、0基线）并按真实bucket差计算；显示桶数和按旧版0.1 GHz假设换算的ns值。稀疏type-1样本按bucket排序；重复桶、无有效交叉或无正向峰值时明确标记不可计算。新逻辑进入Widgets QtTest，测试稠密/稀疏波形和边界输入。本轮Qt5.12.8严格C++11目标构建、帮助及offscreen窗口smoke通过，专项QtTest通过；尚未重跑全平台套件/刷新候选，Windows原生与macOS构建待验。详见[验证71.174](VALIDATION.md#71174-离线波形查看器迁移旧版-fwhm-与-peaking-time-2026-10-22)及[波形查看器手册](WAVEFORM_VIEWER.md)。

## 71.173 新交付包清单包含性校验（2026-10-22）

新生成的host-suite manifest标记完整文件清单，包校验现在拒绝未列出的普通文件及符号链接。交付测试17/17通过。修复71.172 Ubuntu24候选中一份已存在但遗漏登记的启动smoke回执后，候选190个文件与190个manifest条目完全对应；此变更不改动程序或DAQ传输路径。详见[验证71.173](VALIDATION.md#71173-新交付包严格校验完整文件清单2026-10-22)。

## 71.172 当前 Ubuntu 24.04 full profile 全量构建与验收（2026-10-22）

Ubuntu 24.04.5用户空间从本轮当前源码构建28个程序，回执绑定498项输入；Core 151/151、Widgets 111/111、CTest 22/22，以及配置迁移/事务、receiver传输、安全、四路1 GiB合成接收和失败交接窗口验证全部通过。总合成wire数据1,073,799,755字节，低于用户20 GiB限制，收尾`raw_remaining=0`。当前Ubuntu24候选`build-packages/getdaq-ubuntu24-20261022-71-172-current-source-full-candidate/`由该次构建的精确程序生成，28程序Ubuntu24加载器帮助/启动smoke全部通过，manifest及回执在包内。

本轮当前源码已具备Linux host、Ubuntu24用户空间、Windows Qt5.14.2/MinGW三组全量/控制端构建证据；Windows原生QtTest、Ubuntu24 QtTest和多项合成数据回归也通过。Ubuntu24环境共享WSL2宿主内核，Linux host也在WSL内，Windows使用用户提供的主机工具链，不能将三者表述为三个独立干净机器的部署验收。macOS Apple原生构建、独立Ubuntu机器部署、真实双机归档/网络条件、现场gSOAP服务与实体板卡、持续物理DAQ落盘吞吐仍未验收；整体重构未完成。

## 71.171 Windows Qt 5.14.2 当前源码构建和原生测试（2026-10-22）

用户当前 Qt 5.14.2/MinGW 7.3 x86_64 从本轮源码严格 C++11 构建8个控制程序，499项输入回执完整；Windows原生Widgets测试110通过/0失败/1项Unix目录锁跳过，ECC客户端64/64。原生supervisor-query loopback覆盖ACL凭据、401不重试、单次身份GET/控制POST、POST后断连/500不重试。8程序候选完成SDK-free帮助、离屏及原生窗口smoke，297项manifest校验通过；QtTest和凭据临时根已删除，测试raw为0。详情见[验证71.171](VALIDATION.md#71171-当前源码-windows-qt-5142mingw-重建与原生回归2026-10-22)。

这是用户当前Windows工具链验证，不是独立干净安装、Actions、macOS、真实远程ECC/板卡、双机或持续吞吐验收；整套重构仍未完成。

## 71.170 Linux host full profile 当前构建器重建（2026-10-22）

当前构建器从本轮源码构建28/28个full程序，498项输入；新Linux host候选184项manifest、28程序包内smoke、5个核心数据工具无Qt检查及包完整性均通过。通用交付测试16/16；分析profile CTest10/10的四个接收核心程序与本候选SHA相同。详情见[验证71.170](VALIDATION.md#71170-当前构建器的linux-host-full-profile-全量重建2026-10-22)。这是WSL2 host证据，不是独立Ubuntu24/macOS、实板、双机或持续DAQ验收，整套重构仍未完成。

## 71.169 独立可选 ROOT/FITS analysis profile（2026-10-22）

当前源码新增`analysis`套件配置，单独构建7个数据与离线分析程序，仅显式启用时才依赖系统ROOT、CFITSIO和Qt；不引入ECC/Ice，也不改变实时DAQ receiver的数据路径。Linux/WSL2本机7程序构建、CTest 10/10、套件交付测试16/16、包内7程序启动及六个数据/转换工具无Qt检查均通过，候选包完整性复核通过。详情见[验证71.169](VALIDATION.md#71169-独立-rootfits-analysis-profile-构建回归与候选包2026-10-22)。

这只是离线分析profile的Linux/WSL2交付验证，不是独立Ubuntu24/macOS构建，也没有真实实验样本差分；整套重构仍未完成。

## 71.168 当前源码 full profile 构建及DAQ数据面回归（2026-10-22）

从当前源码在Linux/WSL2新建构建目录完成full profile，28个主机程序全部构建成功。验证接收器、文件检查/合并/矩阵与归档catalog共5个数据程序不链接Qt；接收器、Exporter锁、MFM合并、GMAT矩阵和ByteStorage进程级回归5/5通过。28个候选程序的隔离启动检查也全部通过，未访问硬件或生成raw；数据回归最大合成输入393,353字节，测试后raw为0。详见[验证71.168](VALIDATION.md#71168-当前源码主机套件全量构建与数据面回归2026-10-22)。

该环境共享WSL2内核，不是独立Ubuntu24机器；可选ROOT/FITS未在本轮full profile启用。新候选将在本次构建基础上生成。此结果不替代macOS、实板、双物理主机和持续DAQ吞吐验收，整套重构仍未完成。

## 71.167 任意周期 LED 脉冲 Windows 源码与运行包验证（2026-10-22）

当前包含`cobo-led-pulse`迁移的源码已在用户 Qt 5.14.2/MinGW 7.3工具链构建8个Windows控制程序。原生QtTest：Widgets 110通过、0失败、1项POSIX专属跳过；ECC客户端64/64通过，LED任意周期确认窗口定向回归通过。新Windows候选`build-packages/getdaq-windows-qt5142-20261022-ledpulse-candidate/`完整性验证通过，SDK-free帮助命令、离屏及Windows原生窗口smoke通过。合成raw剩余0。详见[验证71.167](VALIDATION.md#71167-任意周期-cobo-led-脉冲-windows-原生回归与当前源码候选2026-10-22)。

这不是独立Windows主机验收、实板灯态验证、macOS原生构建、真实双机或持续DAQ吞吐验收。GUI仍不参与DAQ原始数据收发。整套重构未完成。

## 71.166 旧 ECC 任意周期 LED 脉冲入口（2026-10-22）

`cobo-led-pulse <ledId> <period_ms>`现已进入Qt选定CoBo维护窗口和ECC控制链。LED位`0..15`或全板`16`，周期使用旧板端的半周期毫秒语义，0用于停止所选pulser；服务端将目标、LED和周期绑定到持久操作记录并只执行一次。核心协议/状态/journal、Coordinator双场景及Qt→ECC→Supervisor→receiver两种硬件API端到端验证通过；合成raw共131,072字节并已删除。该项未验证实体灯态。细节见[验证71.166](VALIDATION.md#71166-旧-ecc-任意周期-cobo-led-脉冲迁入及网络联调2026-10-22)和[单板维护说明](COBO_MAINTENANCE.md)。

## 71.165 旧 ECC LED 脉冲功能映射边界复核（2026-10-22）

旧`modifyLED`的Slow/Fast固定半周期（1000/500 ms）及Off/On停止同灯pulser，当前受控LED状态入口已覆盖；旧`cobo-led-pulse`允许自定义半周期，仍没有对应的任意周期操作入口。修正了命令迁移表与剩余工作表中过宽的“任意周期pulse未迁移”描述。此项是旧/新源码静态对照，不新增板卡运行证据。详情见[验证71.165](VALIDATION.md#71165-旧-ecc-led-脉冲功能映射边界复核2026-10-22)。

## 71.164 ECC 网络测试强制清理场景 raw（2026-10-22）

网络端到端 runner 现在在每场景结束后清理自己的合成 `.graw` 输出，记录删除数、字节数、残留和20GiB预算；清理不完整时测试失败。API 0/1 本机测试均通过，分别清理65,536字节并以0 raw结束；新的完整ECC/硬件审计证据复核通过。细节见[验证71.164](VALIDATION.md#71164-ecc-网络测试按场景清理合成-raw-并记录预算2026-10-22)。此项是测试清理与证据改进，不代表实机或DAQ性能验收。

## 71.163 用户 Windows Qt 5.14.2 当前源码构建及维护窗口测试（2026-10-22）

用户提供的 Qt 5.14.2/MinGW 7.3 工具链从当前源码构建8个 x86_64 控制程序并完成新运行包部署。Windows 原生 QtTest为 Widgets 109通过、0失败、1项Unix专属跳过；ECC客户端64/64通过。新增LED GPIO翻转窗口回归覆盖默认Cancel、GPIO 10/全灯16选择、单次提交及错误结果位号拒绝。详细证据见[验证71.163](VALIDATION.md#71163-用户-windows-qt-5142-当前源码构建与-led-flip-原生测试2026-10-22)。

Windows Qt 5.14.2当前源码构建及原生UI回归见[验证71.167](VALIDATION.md#71167-任意周期-cobo-led-脉冲-windows-原生回归与当前源码候选2026-10-22)。本机编译和UI测试不代表真实硬件、现场ECC或独立Windows部署验收。macOS原生构建、独立Ubuntu24、双物理主机、板端/实板及持续高速DAQ仍未验收；整套重构未完成。

## 71.162 旧版 CoBo LED flip 迁入受控维护链（2026-10-22）

旧`cobo-led-flip`会反转当前GPIO，不是幂等的LED状态设置。主窗口现提供明确GPIO位选择：AsAd×灯型映射为0–15，另列全灯16；确认框默认Cancel。请求键同时绑定CoBo与位号，ECC调度器显式分派到flip维护API，状态保持Prepared并只执行一次板端管理器调用，不自动重试。回执明确说明没有物理灯态验证。

Linux/GCC 9.4/Qt 5.12.8下Qt探针、ECC服务与网络夹具构建成功；状态、journal、协议和TCP硬件描述测试4/4通过。Qt→真实ECC HTTP服务→两个Supervisor→接收器→模拟硬件两种API端到端各通过一次，覆盖取消/确认、精确位号、原ID改位拒绝、目标板隔离、busy状态和后续两轮完整采集生命周期。每次64KiB合成raw均已删除，本轮剩余0。CMake现支持`GETDAQ_NATIVE_QT_PROBE`可选路径并注册两个CTest GUI场景；详见[验证71.162](VALIDATION.md#71162-旧版-cobo-led-flip-迁入受控维护链2026-10-22)及[维护操作说明](COBO_MAINTENANCE.md)。

限制：未访问真实板卡；Windows Qt 5.14.2/MinGW、macOS、真实灯态读回和现场DAQ均未验证。整套重构仍在进行。

## 71.161 旧版全板 LED 一次闪烁迁入受控维护链（2026-10-22）

从旧GetBench `GetEccCmd.cpp`及`CtrlNodeI.cpp`核实`cobo-led-blink-all`最终调用板端`LedManager::blinkLEDs()`，旧板端行为是按顺序让所有LED各闪一次。新实现将其作为单独的`blink-cobo-leds`维护操作加入固定活动CoBo链路：Qt默认Cancel并只提交一次，网络请求绑定CoBo/原实例/快照/操作ID，ECC要求健康Prepared和全部数据进程停止，原生硬件预检绑定匹配描述并检查管理器接口，操作审计记录`sequential-once`及`physical_lamp_state_verified=false`。不自动重试；未知结果保持原操作待查询。单灯设置原来的硬件版本2/5限制不改变；flip和任意周期pulse仍未迁移。

实现和本机模拟网络联调已完成：Qt主控台与探针编译成功，协议/状态/journal/硬件客户端回归及两个原生协调器版本通过；真实Qt窗口到ECC服务的两套硬件API集成均通过，确认/取消、唯一目标、单次命令、忙碌互斥和GUI raw读取保护均已覆盖。没有物理板卡、Windows/macOS或现场实验数据验收；旧flip与任意周期pulse仍未迁移。完整验证证据见[验证71.161](VALIDATION.md#71161-旧版全板-led-一次闪烁迁入受控维护链2026-10-22)。整套重构仍未完成。

## 71.160 默认DAQ数据套件独立构建与依赖复核（2026-10-21）

从干净CMake目录以`GETDAQ_BUILD_OPTIONAL_FITS_TOOLS=OFF`、`GETDAQ_BUILD_OPTIONAL_ROOT_TOOLS=OFF`、`GETDAQ_BUILD_OPTIONAL_ROOT_VIEWER=OFF`配置并构建默认数据套件：`getdaq-receiver`、`getdaq-inspect`、`getdaq-merge`、`getdaq-matrix`及相关测试全部成功。`mfm-merge`、`gmat-conversion`、`receiver-core`、`exporter-lock`、`byte-storage-integration`五项CTest 5/5通过。四个程序的动态依赖只含系统C/C++/数学/线程/加载器库，未链接Qt、Ice、Boost、ROOT或CFITSIO；FITS/ROOT保持离线显式可选，原始流仍由独立native receiver处理。进程级字节存储测试使用384KiB合成负载，临时raw及其他夹具在退出时删除，未生成大体积DAQ文件。干净构建目录已删除。该结果确认Linux/GCC 9.4默认依赖边界与本机回归，不代表Ubuntu24独立部署、macOS构建或物理DAQ吞吐验收；整体重构未完成。见[验证71.160](VALIDATION.md#71160-默认daq数据套件独立构建与依赖复核2026-10-21)。

## 71.159 FITS导出支持compact type-2帧（2026-10-21）

可选`getdaq-fits`现在除CoBo type-1 revision 0–5外，也接受完整compact type-2 revision 5读出。按MFM schema提取2-bit AGET索引及12-bit ADC值，并依照各AGET各自的样本到达顺序恢复68通道×512桶；必须恰有4×68×512个样本且每个AGET完整，短读出或超过512桶会拒绝。type-1解码和FITS输出组织保持原样。CFITSIO仍仅在显式启用可选FITS工具时需要，不进入默认DAQ/Qt控制链。

GCC 9.4与系统CFITSIO 3.47下重建`getdaq-fits`及`fits-tests`成功，`fits-conversion` 1/1通过。混合夹具含两个type-1和一个type-2事件，检查276条索引行、272个type-2通道表、AGET/channel元数据、首尾桶样本和checksum；不完整type-2帧被拒绝且不发布。测试合成raw为279,808字节（远低于20GiB）并在测试末尾删除。此为合成布局验证，真实type-2实验文件和旧CCfits输出逐字段差分仍待完成；Windows/macOS CFITSIO也未验证。见[FITS说明](DATA_FITS.md)及[验证71.159](VALIDATION.md#71159-fits导出支持compact-type-2帧2026-10-21)。整体重构未完成。

## 71.158 ROOT查看器兼容旧ROOT5时代对象类名（2026-10-21）

ROOT查看器现在可读取`tree/GDataFrame`分支保存为小写`get::GDataFrame`的旧类名，同时继续读取ROOT6大写`GET::GDataFrame`和T2K树。为避免ROOT6进程在同一翻译单元重复声明同名类，旧名字典、旧数据模型实现、读取/转换适配器在单独的兼容构建单元中编译；此兼容只用于离线查看，不改写ROOT文件，也不进入默认构建或DAQ接收依赖。reader回归以ROOT6写入的小写命名空间树核对事件头、TRef样本关系和边界拒绝；ROOT转换、reader、统计公式、全树扫描回归4/4通过，ROOT查看器及测试目标成功构建，曲面离屏绘制/拖动烟测通过。该测试没有ROOT5运行时，不能证明真实ROOT5生成文件或所有历史Schema差异兼容；真实实验文件和Windows/macOS ROOT仍待实测。见[ROOT操作说明](DATA_ROOT.md)及[验证71.158](VALIDATION.md#71158-root查看器兼容旧root5时代对象类名2026-10-21)。整体重构未完成。

## 71.157 离线ROOT查看器增加可旋转曲面视图（2026-10-21）

`getdaq-root-viewer`新增可鼠标拖动旋转的四面板软件曲面视图，用Qt Widgets/QPainter按深度排序绘制每片AGET的完整68×512样本网格，按ADC值着色；不依赖QwtPlot3D、Qt OpenGL或ROOT GUI，也不接触实时DAQ。每个paint使用内联四顶点面片，避免每个网格单元单独分配多边形内存。ROOT 6.26/16、GCC9.4、Qt5.12.8构建通过；reader、统计公式、全树扫描测试3/3通过。离屏`--surface-smoke`用内存合成完整4×68×512样本，检查绘制像素并注入鼠标拖动，旋转前后262,458像素变化；进程耗时0.33秒、峰值RSS约257,144KiB（含ROOT/Qt，非曲面增量内存）。本轮没有生成`.graw`。曲面语义已迁移但不复制旧QwtPlot3D外观；真实ROOT大文件和目标平台仍待验收，整体重构未完成。见[ROOT操作说明](DATA_ROOT.md)及[验证71.157](VALIDATION.md#71157-离线root查看器增加可旋转曲面视图2026-10-21)。

## 71.156 旧ROOT显示模式范围核对（2026-10-21）

核对归档CoBoFrameViewer源码后澄清：`root2disp`的`data`参数打开四路AGET通道波形浏览器；浏览器内名为`DrawFrame3D`的分支实际用`TH2F`和`COLZ`画桶×通道二维色图。当前viewer的波形及heatmap模式覆盖这两种显示语义。另一个CoBoFrameViewer主界面的QwtPlot3D曲面模式由旧Autoconf条件探测，可选依赖未必存在，不应和ROOT浏览器的二维色图混为一项；`CoBoFrameBrowser.cpp`中拟合实现处于注释块。71.156当时记录曲面模式未迁入；后续实现已由[71.157](VALIDATION.md#71157-离线root查看器增加可旋转曲面视图2026-10-21)更新：现在有不依赖Qwt的Qt软件曲面，但不复制旧库外观。拟合仍是注释代码，不属于旧版已启用功能。这是静态源码核对，无新增运行测试；范围证据见[ROOT工具说明](DATA_ROOT.md)和[验证71.156](VALIDATION.md#71156-旧root显示模式范围核对2026-10-21)。

## 71.155 ROOT查看器支持T2K矩阵树（2026-10-21）

独立离线`getdaq-root-viewer`现在同时读取旧`tree/GDataFrame`和转换器的`TT2K/T2KEvent`矩阵树。T2K读取严格检查272通道×512桶及数组长度，并验证样本处于12位ADC范围；数据映射回四个AGET面板的通道波形/热图，保留零填充点。因T2K树没有旧统计字段，打开时关闭仅适用于GDataFrame的五种旧统计视图。扩展reader回归通过ROOT写入/重开夹具验证两种分支、矩阵边界、零填充及非法维度拒绝；reader、公式/边界、全树统计扫描测试3/3通过，ROOT 6.26/16、GCC9.4、Qt5.12.8离屏窗口启动通过。夹具只产生临时ROOT文件并自动删除，没有生成DAQ raw。真实实验ROOT文件、旧消费者、Windows/macOS ROOT和全分析链仍待验收；整体重构未完成。见[ROOT操作与数据说明](DATA_ROOT.md)及[验证71.155](VALIDATION.md#71155-root查看器支持t2k矩阵树2026-10-21)。

## 71.154 ROOT统计扫描移入可取消工作线程（2026-10-21）

ROOT统计计算已从界面线程移入独立`QThread` worker；worker拥有独立`RootTreeReader`，逐entry报告原子进度，GUI通过事件循环更新进度条并可请求取消，避免全树统计扫描阻塞窗口事件循环。ROOT多线程支持在入口调用`ROOT::EnableThreadSafety()`；接收器完全不引用此代码。新增三事件ROOT合成树专项测试，核对time/frequency/period/bucket/channel-count五种累计值、零时间差频率、工作线程扫描及预先取消；reader、统计公式/边界、统计文件扫描三项回归3/3通过，viewer独立构建成功。测试只生成并删除小ROOT夹具，没有DAQ raw或大文件性能结论。逐entry事件导航仍在GUI线程，单个复杂entry可能短暂等待。见[ROOT工具手册](DATA_ROOT.md)及[验证71.154](VALIDATION.md#71154-root统计扫描移入可取消工作线程2026-10-21)。

## 71.153 ROOT旧统计字段的Qt直方图迁移（2026-10-21）

`getdaq-root-viewer`新增旧`root2disp`五种统计视图：事件时间（秒）、相邻事件频率（Hz）、相邻事件周期（ms）、全树样本桶号分布、每事件非空通道数。统计模式可取消；连续变量两遍扫描ROOT树确定范围并累计固定1000-bin直方图，bucket/channel分别使用512/273-bin整数直方图，不缓存全体事件。独立`rootstatistics` C++11工具核对ticks换算、零间隔频率和直方图边界；reader与统计测试共2/2通过，ROOT viewer目标构建成功，offscreen窗口启动检查运行3秒后按预期超时退出。ROOT统计已在独立QThread扫描并可取消；逐事件导航仍在GUI线程，超大单条entry可能短暂阻塞。该改动没有恢复旧ROOT Canvas的log轴外观，也没有移植完整CoBoFrameViewer 3D/拟合/积分直方图。见[ROOT手册](DATA_ROOT.md)及[验证71.153](VALIDATION.md#71153-root旧统计字段qt直方图迁移2026-10-21)。

## 71.152 可选 ROOT 离线事件查看器（2026-10-21）

新增独立 Qt Widgets 程序 `getdaq-root-viewer`，读取旧 `tree/GDataFrame` 对象分支，支持按 entry 跳转、上一/下一事件、四个 AGET 面板通道波形叠加及 bucket×channel 热图。波形纵轴按当前 AGET 实际样本自动缩放。程序只读 ROOT 文件，不连接DAQ、不接收数据；仅当显式启用 `GETDAQ_BUILD_OPTIONAL_ROOT_VIEWER=ON` 时查找 ROOT 6 与 Qt5 Core/Widgets。ROOT reader 回归用临时小型 ROOT 树验证对象树读回和边界错误，测试结束自动删除。该查看器和此前可选转换器可分别配置，均不进入默认接收器/控制软件依赖。见[ROOT说明](DATA_ROOT.md)及[验证71.152](VALIDATION.md#71152-可选-root-离线事件查看器2026-10-21)。

范围限制：查看器接受ROOT6的`GET::GDataFrame`、隔离字典读取的ROOT5时代`get::GDataFrame`及`T2KEvent`矩阵树；小写命名空间兼容尚无真实ROOT5文件验证。旧`root2disp`波形浏览和其`DrawFrame3D`的`COLZ`桶×通道图已有等价显示模式；可选QwtPlot3D曲面语义现由独立Qt软件绘制模式覆盖，但不复制旧库外观。旧ROOT Canvas精确样式及真实实验文件/旧消费者兼容仍待验收。旧ROOT浏览器的拟合代码为注释状态，不算已启用功能。本机 Linux ROOT 6.26/16、GCC 9.4、Qt5.12.8通过构建和合成reader/converter测试，Qt offscreen GUI启动通过；macOS/Windows ROOT构建和现场ROOT文件尚未验证。整体重构未完成。

## 71.151 可选 `.graw` 到 ROOT 离线转换（2026-10-21）

新增独立可选 `getdaq-root`，迁移旧 `graw2root` 的主要 ROOT 对象树输出，并保留 `--t2k` 的 `TT2K/T2KEvent` 路径。支持 CoBo type-1 rev0–5、type-2 rev5；输出旧 `tree/GDataFrame` 类结构。仅显式启用 `GETDAQ_BUILD_OPTIONAL_ROOT_TOOLS=ON` 时查找 ROOT 6；ROOT 不进入默认构建、DAQ 接收热路径、控制程序或 Qt。ROOT 6.26/16 与 GCC 9.4 下构建成功，`root-conversion` 1/1通过，实际读回树/分支类名、元数据、样本、T2K 272×512布局和缺桶补零；截断输入和输出容量限制通过，小型合成 raw 已删除。真实实验 raw 对比、旧 Viewer 打开、macOS/Windows ROOT 构建和完整分析工作流仍未验收。见[ROOT工具说明](DATA_ROOT.md)及[验证71.151](VALIDATION.md#71151-可选graw-root离线转换2026-10-21)。

## 71.150 可选 `.graw` 到 FITS 离线导出器（2026-10-21）

新增独立命令行工具`getdaq-fits`，迁移旧`cobo-frame-graw2fits`的事件分组索引和每事件/通道FITS表主路径。仅支持CoBo type-1 revision 0–5；输出包含每事件`.fits.gz`、`GROUPING`索引、FITS校验和。支持跳帧、事件数和默认20GiB输出上限；只读输入，拒绝覆盖既有目录，先在私有暂存目录写完并校验后原子发布。CFITSIO是显式选项`GETDAQ_BUILD_OPTIONAL_FITS_TOOLS=ON`的唯一新增库，不进入默认构建、接收数据热路径、控制程序或Qt界面。主机当前系统CFITSIO 3.47下工具构建成功，专项`fits-conversion` 1/1通过：验证1024字节合成raw两事件/四通道索引、文件名引用、样本/桶号读回、checksum、帧数/空间上限及失败清理；合成raw已删除。实际实验文件对照旧CCfits工具、Windows/macOS CFITSIO构建、完整分析覆盖仍待验收，因此该转换器尚不能视为逐字节/全功能旧版兼容。见[FITS工具说明](DATA_FITS.md)与[验证71.150](VALIDATION.md#71150-可选graw-fits离线导出器2026-10-21)。

**当前源码与验证基线（2026-10-21，71.159）：** 可选`getdaq-fits`现支持CoBo type-1 rev0–5及compact type-2 rev5完整读出；混合合成帧回归通过，真实raw差分及旧CCfits消费者核对仍待完成，详见[71.159](VALIDATION.md#71159-fits导出支持compact-type-2帧2026-10-21)。旧ROOT5时代`get::GDataFrame`小写类名现由独立字典/reader适配层读取；ROOT6 `GET::`及T2K矩阵树保持可用。验证以ROOT6生成的小写命名空间夹具完成，真实ROOT5文件兼容仍待确认。ROOT 6.26/16、Qt5.12.8下ROOT转换、reader、统计公式/边界、全树扫描测试4/4通过，离屏3D旋转烟测通过；ROOT不进入默认DAQ接收或Qt控制依赖，真实样本及跨平台差分仍待验收。此前三平台程序候选和普通TCP跨命名空间证据仍见[71.149](VALIDATION.md#71149-归档器普通tcp跨网络命名空间回归2026-10-21)。 离线 `getdaq-waveform-viewer` 已直接读取 `.graw` 与 `.gmat`；Linux host 从当前源码构建 full profile 28个程序，71.147候选181项manifest通过，另纳入本轮归档器跨命名空间TCP回执后共182项。Ubuntu24.04.5用户空间以Qt5.15.13重建查看器，并在71.143全量候选上增量刷新，候选28程序/194项通过目标loader和manifest验证；其余27程序复用基线，且该环境共享WSL2内核。Windows使用用户Qt5.14.2/MinGW7.3当前源码构建8个控制程序，Widgets QtTest 108通过、0失败、1项Unix专属测试跳过，候选282项通过Windows运行库/manifest验证。CoBo type-1 rev0–5、type-2 rev5及layered MFM `0xFF01` rev1直接查看已接入，三端reader/build结果见[71.147](VALIDATION.md#71147-cobo-graw直接离线浏览本机回归2026-10-21)。普通TCP的 `getdaq-receiver` 已在隔离命名空间接收32MiB字节流并核对SHA-256/EOF封存；本轮进一步用候选内 `getdaq-catalog` 通过veth执行无认证跨命名空间归档发送，目标端全量校验通过，测试路径累计25,190,400字节raw全部删除。两项均共享同一内核/主机，不等同物理双机、FrameStorage或持续吞吐验收。Qt界面不参与DAQ数据接收。真实实验`.graw`差分、独立Ubuntu/Windows部署、macOS原生构建、归档器真实双机传输、现场旧gSOAP服务/实板和持续物理DAQ吞吐仍待验收，整套重构未完成。总状态见下方[验收清单](#当前剩余验收清单)，详细证据见[验证71.149](VALIDATION.md#71149-归档器普通tcp跨网络命名空间回归2026-10-21)。
**离线波形查看器及三端候选（71.146，2026-10-21）：** 增加Qt Widgets `getdaq-waveform-viewer`，按文件偏移查看`.gmat`指定帧、AGET、通道，每次只读512个binary64样本（4KiB），不连接DAQ、不进入接收链。Linux host使用当前源码full profile构建28程序，新候选178项清单通过；查看器帮助和离屏GUI启动通过。Ubuntu24候选从完整27程序基线安全增量加入查看器，成为28程序/194项，Ubuntu24 Qt5.15.13构建、离屏启动、loader和包清单校验通过；该包复用其余27程序，并非全量重编译。Windows使用用户Qt5.14.2/MinGW7.3从当前源码完整构建8个Qt控制程序，新候选278项清单通过，8程序SDK-free帮助检查、打包查看器Windows offscreen运行5秒及Windows原生GetDAQ窗口smoke通过。三端候选文档已经刷新。查看器只覆盖旧CoBoFrameViewer单通道二维波形主路径，不替代ROOT TTree、3D、统计拟合、FITS或`.root`浏览。独立主机、macOS原生、真实双机TCP、现场旧gSOAP服务/实板脉冲及持续物理DAQ吞吐仍待验收，整体重构未完成。见[离线波形查看器说明](WAVEFORM_VIEWER.md)及[验证71.146](VALIDATION.md#71146-离线gmat波形查看器及三端候选刷新2026-10-21)。

**直接浏览 CoBo `.graw` 波形（71.147，Linux/Ubuntu24/Windows 当前源码回归）：** `getdaq-waveform-viewer` 新增只读 `.graw` 路径：后台扫描 MFM 头并索引独立及 layered (`0xFF01` rev1) CoBo type-1 rev0–5 / type-2 rev5 子帧；选中时最多读入一个 4 MiB CoBo 帧。type-1支持稀疏通道并以实际桶号作横坐标，type-2按旧 Viewer 的 AGET顺序重建并要求完整读出。Qt 5 Core/Gui/Widgets 和 C++11即可构建，索引使用 QtCore `QThread`，关闭窗口会请求取消索引；没有引入 QtConcurrent、ROOT 或网络依赖。Linux host当前源码full候选28程序/180项并校验通过；本机Qt5.12.8查看器及C++11 QtTest构建通过，普通帧、layered帧、compact type-2帧、截断输入、索引后头部变化和取消均通过，包内帮助/离屏启动通过。Ubuntu24使用Qt5.15.13重建查看器，帮助与离屏启动通过；从71.143基线增量刷新出的新候选28程序/194项通过Ubuntu24 loader和清单校验，其余27程序复用基线。用户Windows的Qt5.14.2/MinGW7.3重新构建8个控制程序并成功部署，Windows Widgets QtTest为108通过、0失败、1项Unix专属目录锁跳过；新增三项MFM回归均通过，Windows原生窗口/SDK-free smoke与282项manifest文件通过。Windows QtTest私有临时根已移除，raw剩余0；测试总raw预算20GiB，实际清理回执记录0文件/0字节遗留。仍缺macOS原生构建、真实实验`.graw`差分和ROOT/FITS全功能覆盖；Ubuntu24验证运行于WSL2共享内核，不是独立主机验收。整体重构未完成。详见[波形查看器手册](WAVEFORM_VIEWER.md)和[验证71.147](VALIDATION.md#71147-cobo-graw直接离线浏览本机回归2026-10-21)。

**macOS 验收证据改进（71.135，2026-10-21）：** QtTest临时目录清理已拆成独立helper，固定工作区内清理位置，逐级拒绝符号链接/非目录父路径与清理根，核算raw并原子保存20GiB预算清理回执。行为测试4/4覆盖空目录、3字节raw、符号链接根和符号链接父目录；workflow合同16/16、host delivery 14/14通过。该历史检查点不代表Mac原生构建；当前整体验收状态见上方71.145及下方剩余清单。GitHub Actions尚未运行，整体重构未完成。详见[71.135验证记录](VALIDATION.md#71135-当前候选文档与清理边界回归复核2026-10-21)。

**最新构建依赖修正（71.118）：** `getdaq-supervisor`现要求Qt5 Core/Network/Concurrent在配置时可用；`data` profile仍构建6个无Qt工具，逐个动态依赖检查通过。修改后Linux host `full` profile再次从当前源码构建27/27；构建入口回归7/7通过，确认纯数据核心在禁用Qt时可配置、管理服务在禁用Qt时及时失败；该回归已加入Linux/macOS native-data-path workflow。三平台候选文档已同步且程序/运行库校验通过。71.118本身没有重跑全平台Qt/CTest及DAQ负载套件；其后的71.119已在Ubuntu24用户空间完成全套平台回归，详见本页当前状态。细节见[71.118验证记录](VALIDATION.md#71118-明确监督服务的-qt-构建依赖并验证无-qt-数据-profile2026-10-21)。

**最新配置迁移改进（71.119）：** 增加只读`discover-project-referrers`，在用户指定目录内有界枚举`.xcfg/.xml` include，展示传递引用者和候选顶层根；发现损坏XML、外部引用、符号链接、未完成保存或扫描预算不足时明确返回不完整。严格C++11配置核心151/151通过，隔离CLI迁移/恢复/快照交付回归通过。Linux host full profile随后从当前源码构建27/27程序，新候选`getdaq-linux-host-20261021-71-119-current-source-full-candidate`含172项并通过包校验；候选内新CLI、两向恢复、快照检查通过，六个数据程序无Qt依赖。Ubuntu24、Windows及macOS交付尚未刷新到71.119；本轮没有重跑全平台Qt/CTest/DAQ负载套件。扫描结果不会自动进入迁移工作区，目录外/其他格式/其他主机仍需人工查证。细节见[71.119验证记录](VALIDATION.md#71119-配置路径迁移增加有界只读引用发现2026-10-21)。

## 71.122 旧 ECC gSOAP 双进程互操作与 Qt 完整回归

本地固定gSOAP 2.8.124源码归档已加入可审计依赖清单，原始许可文件单独保存。构建探针从历史`ecc.hh`使用旧参数`-z1 -L -x -pecc -n`重新生成服务端绑定，编译独立本机服务进程，再由当前`EccClient`发送`GetState`、`Describe`、`Prepare`、`Configure`、`Start`、`Stop`、`Pause`、`Resume`、`Breakup`和`Undo`。服务以旧GetBench命名空间`urn:http://localhost:8083/`运行，逐项捕获并核对9个文本操作传来的`configID`与`table`字符串。专项用例确认10个操作均由生成服务接收且客户端解析成功。Linux host Qt5.12.8下完整Widgets QtTest **107通过、0失败、0跳过**，其中进程互操作用例通过；gSOAP兼容fixture不生成DAQ raw，测试构建使用临时目录并在退出时核算20GiB上限、清理raw。本次最终收据`build-receiver/legacy-gsoap-interop-71-122.json`确认`.graw`剩余0个、0字节，临时树已删除；QtTest日志及SHA同目录保存。

由旧生成器输出实测发现，`GetState`回`ResponseState`、`Describe`回`ResponseText`，而解析器先前只识别`*Response`结尾标签，导致真实gSOAP响应被拒绝。现在精确接受这两个历史DTO名称，同时保留严格SOAP Envelope/Body、字段和Fault校验；GetState状态响应与九个文本控制响应的真实生成序列化均通过当前客户端解析。同一修复随后进入三个平台的当前源码候选：Linux host和Ubuntu24各27程序/175项，Windows 7程序/271项；构建收据、逐程序摘要和包内启动smoke均已核对，平台测试细节见下方71.122验证记录。gSOAP生成器及生成代码只进入测试，不进入产品；真实现场服务版本及业务行为仍待验证。

边界：2.8.124是便于离线复现的兼容测试版本，不代表现场实际精确版本；测试服务对其他操作使用固定响应，未证明完整状态机、全部错误/超时/并发语义、现场旧服务器、板端行为或DAQ吞吐。真实部署互通仍需现场gSOAP服务抓包/联调。详见[71.122完整记录](VALIDATION.md#71122-旧-ecc-gsoap-双进程互操作与qt完整回归2026-10-21)、[构建和许可边界](DEPENDENCIES_AND_OFFLINE_BUILD.md#旧-ecc-soap-服务的依赖状态)。

## 71.121 Linux host Qt 5.12.8 回归与 71.120 Windows Qt5.14.2 当前源码交付

用户Windows Qt5.14.2/MinGW7.3 x64从当前源码重建七个控制程序；Widgets 105通过、0失败、1项既有Unix锁测试跳过，ECC客户端63/63通过。新候选`getdaq-windows-qt5142-20261021-71-119-current-source-candidate`有268项清单，SDK-free帮助、离屏启动和原生Windows窗口smoke通过；包内`getdaq-config.exe`的有效/不完整引用发现CLI，以及实际路径迁移、双方向崩溃恢复和快照检查在Windows原生进程通过。两套QtTest私有临时目录清理完成，raw为0。验证只覆盖用户当前Windows主机；独立Windows、Actions、旧gSOAP真实服务和现场硬件仍待。详见[验证71.120](VALIDATION.md#71120-当前源码-windows-qt-5142-构建测试与运行候选2026-10-21)和[Windows手册](WINDOWS_QT5142.md)。

## 71.119 配置迁移引用发现与 Linux/Ubuntu24 候选刷新

配置路径迁移新增有界只读引用发现；严格C++11核心151/151和迁移CLI交付回归通过。Linux host与Ubuntu24均从当前源码full构建27程序，172项候选清单通过；Ubuntu24全平台回归Core151、Widgets106、CTest22通过，四路1GiB合成接收完成，合成wire总量1,073,799,755字节且`raw_remaining=0`。Ubuntu24 loader对候选27个程序的启动检查通过。Ubuntu24共用WSL2内核；Windows当前源码构建/测试结果另见71.120。macOS原生、独立Ubuntu24、双机、真实gSOAP/板卡和持续DAQ仍待验证。详见[71.119记录](VALIDATION.md#71119-配置路径迁移增加有界只读引用发现-2026-10-21)与[Ubuntu24验证记录](UBUNTU24_VALIDATION.md)。

## 71.117 旧 SOAP Pause/Resume 与跨平台候选刷新

旧服务模式现提供多节点 Pause/Resume，要求全节点新鲜状态一致并在SOAP应答后再次核对；暂停时停止运行计时，恢复后继续累计。Widgets QtTest106/106及真实HTTP/SOAP mock多节点暂停、恢复、暂停态Stop通过。Ubuntu24从当前源码全量构建27程序、完成平台回归，并生成176项清单候选；27程序在Ubuntu24用户空间loader下启动检查和包清单复核通过。Windows Qt5.14.2/MinGW7.3七程序候选包含本轮代码，原生Widgets/ECC QtTest及Windows窗口smoke通过。Linux host候选已由同一源码状态全量构建并通过包内启动检查。Ubuntu24共享WSL2内核；独立机器、macOS、真实旧gSOAP服务、板卡、双机网络和持续DAQ吞吐仍未验收。详见[验证记录71.117](VALIDATION.md#71117-旧-soap-控制模式补齐-pause-resume-2026-10-21)。

## 71.116 Ubuntu24当前源码全套回归与完整运行候选

经授权启动专用WSL入口后，Ubuntu24.04.5用户空间full suite 27程序、Core149、Widgets106、CTest22以及迁移/事务/接收安全/四路1GiB负载/失败交接窗口均通过；新候选含同一构建的27程序并通过Ubuntu24 loader、包内程序启动/帮助和GetDAQ初始窗口截图检查。总wire 1,073,799,755字节，raw清理0。WSL内核、硬件、独立主机及持续物理DAQ验收边界保持未验证。详见[验证71.116](VALIDATION.md#71116-ubuntu24当前源码全套复验及27程序运行候选2026-10-21)、[Ubuntu24操作说明](UBUNTU24_VALIDATION.md)。整体重构未完成。

## 71.115 当前源码 Linux host 全量构建与 QtTest 回归

Ubuntu20.04.6/WSL2宿主全量构建27/27与QtTest106/106已通过；初次受限执行仅因本机TCP监听不可用而有13项失败，同一二进制在本机回环可用权限下完整复跑通过。详细平台、程序摘要、测试输出与未验收边界见[验证记录71.115](VALIDATION.md#71115-当前源码-linux-host-全量构建与-qttest-回归2026-10-21)。本次尝试启动Ubuntu24用户空间被WSL互操作层阻断，未刷新其回归状态。整体重构仍未完成。

## 当前剩余验收清单

旧 ECC 控制台的逐组功能映射和未找到的一对一维护入口见[旧 ECC 命令迁移核对表](LEGACY_ECC_COMMAND_COVERAGE.md)。不要把“旧命令没有同名按钮”误说成主流程缺失；同样也不要把底层函数存在误说成操作者入口已交付。

这里区分“源码/本机回归已经覆盖”和“必须靠真实部署条件验证”的工作。当前没有可靠的加权项目表，因此不以单一百分比表达完成度。

注意：71.181更新离线波形查看器的3D曲面模式。71.182 Linux host、71.183 Ubuntu24及71.184用户Windows Qt5.14.2候选均包含当前源码查看器并完成包内启动检查。Ubuntu24 Core151/151、Widgets114/114、CTest22/22通过；Windows Widgets113通过/1项POSIX测试跳过、ECC客户端64/64；三个平台的曲面旋转smoke均通过。macOS原生构建尚无实际runner结果。

| 项目 | 当前状态 | 尚需完成的证据 |
| --- | --- | --- |
| 主机端软件、配置界面与原生接收路径 | Linux host 71.319从包含71.318 UI改动的当前源码full重建28/28并通过包内28程序启动smoke；发布配置未启用CTest。71.320完成状态表Transition范围提示GUI回归。历史Ubuntu24 71.223与Windows Qt5.14.2 71.274候选的测试仍是较早源码证据，不能代表当前源码。DAQ raw仍由独立原生程序处理，不经过Qt | Linux运行于WSL共享内核，非独立干净机器/Ubuntu24验收。Windows当前源码重建受WSL interop启动qmake失败影响，须在Windows原生环境重跑；macOS原生构建/运行、部署权限/重启、现场服务/板卡及持续吞吐也待验证 |
| 归档跨主机复制 | 已实现无认证普通TCP/IP发送、接收、显式续传和SHA-256内容校验；同机loopback/独立进程及71.149隔离网络命名空间/veth完整归档验证通过；[两台独立主机现场验收步骤](CATALOG_TRANSFER.md#135-两台独立主机的现场验收步骤)已整理 | 尚未取得真实双机互传证据；按手册核对防火墙、路由、目标磁盘、断线续传和实际吞吐。该协议不加密、不认证，只应部署在可信且受限的网络 |
| 旧SOAP控制入口 | 从历史`ecc.hh`实时生成绑定；当前Qt客户端与gSOAP 2.8.124隔离服务进程的10个旧ECC界面操作互通，9个文本操作参数解码及响应解析通过 | 现场精确版本、完整旧服务操作/错误/超时/并发语义仍需抓包和联调；gSOAP测试生成代码不进入产品 |
| 板卡硬件与时序维护 | Linux host维护窗口支持选定CoBo重准备、单灯Off/On/Slow/Fast（半周期1000/500ms）、任意半周期LED脉冲、全灯顺序闪烁一次、GPIO位翻转及单次AsAd pulser；71.166覆盖任意pulse及两API GUI网络联调，Windows Qt5.14.2/MinGW窗口也已回归。板端`getHwServer`/`IceEirfu`保持外部依赖 | 模拟服务证据不验证物理灯态；没有隔离、可证明只影响选定CoBo的TDC realignment入口。旧命令语义及不新增误导性单板按钮的原因见71.185。没有任意寄存器GUI写入。硬件版本、实际输出、维护隔离、故障恢复和控制权切换仍需现场验收 |
| 高负载DAQ接收 | 原生接收器、并发接收和有限时长合成负载已有回归；71.148在隔离网络命名空间间经veth向包内接收器发送32MiB，SHA与EOF seal一致，源/目标共64MiB已清理；Qt不转发raw | 在目标采集主机与物理网卡/磁盘上验证TCP/ZBUF/FDT实机互通、丢帧、磁盘轮转/封存和长时间稳定性；本机namespace回归不代表持续吞吐；测试raw继续限制在20GiB以内并清理 |
| 故障恢复边界 | 已覆盖若干固定身份、失败交接、冷记录审阅和显式续传场景 | 对现场断电、服务重启及不确定结果做受控演练；未知/超时操作不得自动重放，须逐场景确认可恢复边界 |
| 旧数据库/可选分析工具 | 旧数据库GUI入口源码为空，后端默认关闭且依赖未随项目提供；`getdaq-fits`提供CoBo type-1 rev0–5和compact type-2 rev5离线转换（显式CFITSIO）；`getdaq-root`与`getdaq-root-viewer`提供旧对象树转换及GDataFrame事件查看主路径（显式ROOT 6，viewer另需Qt5） | 只有现场提供真实数据库类型/流程后，才定义数据库迁移范围；旧ROOT Canvas精确外观和真实样本/下游消费者兼容仍待验收；QwtPlot3D主要曲面语义已有Qt离线实现但不复制旧外观；旧ROOT浏览器拟合代码为注释，不能列为已启用功能；FITS真实样本与旧CCfits差分仍待完成，不据历史示例推断部署需求 |

`getHwServer`/`IceEirfu`是板端组件，按既定范围保持不重写；“未完成”指主机软件与这些现有组件之间尚缺的现场互通验收，以及上述部署验证。

## 71.114 QtTest测试模式数据目录隔离（2026-10-21）

在71.113复跑路径迁移GUI用例时确认，QtTest启用`QStandardPaths::setTestModeEnabled(true)`后会访问`~/.qttest`；在Windows也需要隔离APPDATA/LOCALAPPDATA。现Ubuntu24平台runner为整个工作目录设置私有HOME/XDG缓存/运行/临时位置，Windows runner将APPDATA与LOCALAPPDATA设到自有临时根，由既有清理逻辑删除。

入口合同回归16/16、Python语法检查通过；这只验证测试runner配置，不代表Ubuntu24/Windows Actions已运行或重新完成平台构建。详见[验证记录71.114](VALIDATION.md#71114-qttest测试模式数据目录隔离2026-10-21)。

## 71.113 配置来源路径迁移当前源码复验（2026-10-21）

此前70.68已经实现独立的来源路径迁移GUI/CLI、迁移预览及原始/候选恢复，但配置迁移表和总验收表仍将其列为待实现；专用旧检查点目录当前已不存在。本轮按当前工作树在Ubuntu20.04/GCC9.3/Qt5.12.8、严格C++11配置下重建核心/进程探针/控制界面测试：配置核心149/149，进程中断恢复12种和活跃写者2种，迁移/恢复界面3/3通过。测试只用临时XML，raw为0，临时构建已删除。

已修正文档中的功能状态冲突并链接[操作手册](CONFIGURATION_PATH_MIGRATION.md)。目前仍不支持任意外部读者可见的跨文件原子提交，也没有扫描未打开的工程引用者；本机回归不构成Windows/macOS/独立Ubuntu、真实断电或真实工程现场验收。详情及适用范围见[验证记录71.113](VALIDATION.md#71113-配置来源路径迁移当前源码复验2026-10-21)。

## 71.108 原生ECC生产调用链文档校正（2026-10-21）

检查NativeEccCoordinator实际调用后，修订了SystemDescription、SystemPreparation/SystemAlignment、CoBo Prepare、sender与receiver生命周期手册中已经过时的“仍未接生产ECC/GUI仍只连接旧SOAP”描述。源码证实Describe/Prepare/Configure与数据生命周期已通过native_ecc固定HTTP入口组合；旧SOAP仍是独立兼容模式。手册同时明确系统级对齐已经随Prepare执行，但没有独立单板重新定位和任意寄存器GUI写入；选定CoBo重准备不应误称为单板重新对齐。全部硬件实物效果仍未验收。

本次修改仅为文档，没有新增运行或硬件证据。三个71.107候选包的文档均已刷新，Linux host保持27程序/175项、Ubuntu24保持27程序/180项、Windows保持7程序/272项；刷新工具在修改前后均通过包完整性检查并保持可执行文件和运行时依赖不变。源码Markdown本地链接检查1339项通过，`third_party/dependencies.json`有效。

## 71.109 禁止空CoBo拓扑执行系统对齐（2026-10-21）

独立 `getdaq-align` 的配置校验此前允许所有CoBo都禁用；即使没有任何目标CoBo，后续仍会读写系统级MuTaNT/CoBo对齐状态。现在校验在开始硬件访问前拒绝零个启用CoBo，并增加无活动板拓扑回归。

GCC 9.4/C++11定向native alignment测试的两个协议编码变体通过；完整Linux host `--profile full`从当前源码构建成功，27个程序、候选包172项文件且完整性复核通过。构建主机为WSL2 Linux/glibc 2.31，不能代表Ubuntu24独立机器、Windows/macOS或物理硬件验收；具体证据见[验证记录71.109](VALIDATION.md#71109-禁止空cobo拓扑执行系统对齐2026-10-21)。整体重构仍未完成。

## 71.110 Ubuntu24当前源码整套回归与全量候选（2026-10-21）

在已验签Ubuntu24.04.5用户空间、WSL2宿主内核上对当前工作树执行`tests/ubuntu24_platform_validation.py --work /build/getdaq-platform-71-110 --jobs 2 --stage all`。full profile 27/27程序构建成功；Core QtTest 149/149、Widgets QtTest 106/106、原生CTest 22/22；配置迁移和事务、12项receiver传输、四路各256MiB接收/轮转/哈希/EOF/seal、失败交接窗口故障注入均通过。整轮aggregate test wire为1,073,799,755字节（低于20GiB）；临时交接raw 8,192字节已删除，平台回执`raw_remaining=0`，测试根内无`.graw`。

候选`build-packages/getdaq-ubuntu24-20261021-71-110-current-source-full-candidate/`由同一轮实际全量构建产物制作，含27个程序、185项manifest文件；其中`getdaq-align` SHA-256=`7a15db2d150b2529aa5a2b18775a3a24f42ffe2cf2efcafdf9eaaa784c3177aa`，对应71.109新增的零活动CoBo拒绝保护。另以候选内确切二进制运行`validate-topology`，有效拓扑通过、零活动CoBo拓扑以预期错误退出；结果与程序SHA写入候选验证收据。包内绑定Ubuntu24平台回执、完整软件构建输入/程序SHA回执、QtTest构建回执及关键集成测试输出；`build-suite.py --verify-stage`通过。新候选是Ubuntu24用户空间/WSL2内核构建，不代表独立Ubuntu机器；真实双机、macOS原生、旧SOAP服务、现场板卡和持续物理DAQ吞吐仍未验收。详见[验证记录71.110](VALIDATION.md#71110-ubuntu24当前源码整套回归与全量候选2026-10-21)。

**71.89 文档状态勘误（2026-10-21）：** 复核 `NativeEccCoordinator`、`RemoteReceiverManager`、ECC 网络入口和 GetDAQ native ECC 调用后，修正 README 与接收生命周期/ECC模块手册中仍称“GUI未接入/仍只用旧SOAP”的过时描述。当前 native ECC 模式通过低频ECC控制服务驱动独立receiver生命周期，Qt不接触原始帧；旧SOAP模式仍保留且不等价。此项只修正文档，没有改变程序行为或新增验收证据。独立物理双机、现场板卡、真实控制权/加载版本和持续吞吐仍待验收。

## 71.90 将监督器计划/协议校验从POSIX I/O中拆出（2026-10-21）
- 新增纯C++11 `getdaq-supervisor-protocol` 静态target，集中固定计划字段校验、规范IPv4检查及计划/控制协议编解码，并从POSIX `getdaq-data-supervisor-core` 移出；`planio` 和控制协议解析不再包含POSIX I/O头。接收端入口仍额外执行本机网卡地址核验，远程控制端解码计划仍允许目标地址不是控制主机本机地址。归档测试脚本同步纳入新库的构建/链接和插桩材料。
- GCC 9.4/C++11从源码构建共享协议target、`getdaq-supervisor-query`、监督器codec和receiver catalog测试成功；`supervisor-control-codec` 与 `receiver-catalog-filesystem` 两项CTest通过。新增回归拒绝IPv4前导零、错误八位组、多播和未指定地址。该target为Windows查询客户端复用协议逻辑提供独立边界；后续Windows实现与交付状态见71.91。Ubuntu当前源码完整套件仍需在这组代码修改后重跑。

## 71.91 Windows 原生监督器查询客户端（2026-10-21）
- Windows查询helper复用纯C++11 `getdaq-supervisor-protocol`，新增WinSock有界HTTP、Win32凭据ACL及无reparse请求文件读取；Unicode命令行经UTF-16校验再转UTF-8。它只做低频控制，不处理DAQ载荷；与用户选定的无认证普通TCP归档复制是不同协议。Windows凭据必须由当前用户持有，使用受保护DACL且只授予当前用户/LocalSystem。
- 用户Qt5.14.2/MinGW7.3真实构建七个Windows控制程序成功；新`getdaq-supervisor-query.exe` PE SHA-256=`2306cb1b9e5ca5a48c8d9e3d411f434248f91921278e8b055328bd2a90428d45`，导入仅有ADVAPI32、WS2_32、Windows系统DLL及MinGW运行库，没有Qt DLL。新候选`build-packages/getdaq-windows-qt5142-20261021-seven-program-query-candidate`含7程序/268文件；无SDK PATH帮助、GetDAQ离屏启动、原生Windows窗口smoke及完整manifest校验通过。
- Ubuntu CMake query目标与独立qmake项目均构建成功；`supervisor-control-codec`、`receiver-catalog-filesystem`通过2/2，Windows入口合同14/14，Markdown本地链接1335项通过。Windows Python 3.10 本机loopback通过受保护临时凭据、一次身份GET、401后不重试、无凭据回显及临时根清理，精简回执为`build-windows-qt5142-query-port/query-tests/supervisor-query-loopback.json`。该回归现也接入GitHub Actions，但Actions尚未运行。新包证明Windows helper已交付，尚不证明完整远程交接恢复、独立干净Windows、双机现场或硬件验收。新Ubuntu24全套验证也尚未重跑；整体重构未完成。

## 71.93 Ubuntu24当前源码全套复验及候选包（2026-10-21）

- 在Ubuntu24.04.5用户空间/WSL2宿主内核上，从当前源码重建完整full profile：27程序、Core QtTest149/149、Widgets106/106、原生CTest22/22、配置迁移与事务、receiver传输/安全、四路接收和失败交接窗口回归全部通过。测试总wire量1,073,799,755字节，低于20GiB；交接窗口生成并删除8192字节raw，整轮`raw_remaining=0`。
- 首次全套运行发现失败交接集成仍把`getdaq-supervisor-query`硬编码在`delivery/full/native/`，但full profile实际将它输出到`delivery/full/bin/`。改为从当前build receipt解析service、receiver和client程序路径，并增加回归测试；`tests/host_suite_delivery_test.py` 5/5通过。最新测试集指纹对应的runner全套退出码0，回执`build-ubuntu24-validation/rootfs/build/getdaq-platform-71-93/platform-75c484c809d144198985f8c79d9db0e5.json`。
- 新候选`build-packages/getdaq-ubuntu24-20261021-71-93-current-source-full-candidate`为27程序/172清单文件；manifest使用包内`bin/<程序名>`相对路径，可脱离构建机移动和刷新文档；Ubuntu24 loader依赖和完整包校验通过，27个程序摘要逐一与平台回执一致。它在WSL2共享宿主内核的Ubuntu24用户空间构建/运行，不是独立Ubuntu机器验收；macOS、双机普通TCP、真实旧服务/板卡及持续DAQ吞吐仍待验。测试raw全部清理；候选保留当前二进制及随包文档。

**历史检查点 71.87（2026-10-21，已被71.107后续结果取代）：** 本轮为失败交接窗口补入“native query helper 缺失时明确警告并禁用生成/提交/观察”的保护；Ubuntu20 Qt5.12.8 Widgets 完整套件 106/106通过。使用用户实际 Qt5.14.2/MinGW 从当前源码重建六个 Windows 操作程序，原生Widgets 105通过、1项Unix专属锁测试跳过，ECC客户端63/63，新的六程序/263文件候选完成SDK-free与原生窗口smoke；本轮合成raw余量0。该阶段Windows六程序候选尚不含`getdaq-supervisor-query.exe`，因此交接恢复操作当时不可用；71.91加入helper、71.107将其纳入七程序Windows候选。此处仅保留历史限制，不代表当前候选状态。Ubuntu24完整套件需要在这次源代码变化后重跑；独立Ubuntu24/Windows/macOS、真实服务/板卡、双机网络与持续DAQ吞吐仍待验收。总体重构未完成。

历史进度汇总（截至71.86，后续71.87及以上的当前源码结果见前述记录；本段保留历史演进，不作当前状态判断）。71.56完成Ubuntu24候选四路18GiB loopback轮转/哈希/EOF/seal回归并删除全部raw；测试流低于52.6GB物理内存，不能证明持续磁盘吞吐。71.57另在Ubuntu20.04.6/GCC9.4/Qt5.12.8从当前源码full构建27个程序，配置核心149/149、Widgets103/103、原生CTest22/22、配置迁移/事务、12项接收协议场景、9项负载安全测试及四路1GiB轮转回归通过，所有合成raw均删除。Ubuntu24 WSL辅助入口在71.62时曾报`UtilBindVsockAnyPort: socket failed 1`；71.80使用扩展权限启动该入口并完整通过Ubuntu24.04.5用户空间`--stage all`。71.58修正通用Sentry示例进程名；71.59同步更正文档；71.60以当前源码在Linux loopback验证Sentry对真实`getdaq-receiver`进程的存活/退出状态检查，原有接收与归档集成也通过；71.61把同一有界集成加入macOS full-suite CI；71.62增加Ubuntu24当前源码`--stage all` CI入口，涵盖27程序全量构建及平台回归；71.63增加Windows Qt5.14.2/MinGW7.3六程序构建CI；71.64将原先WSL专用的Windows QtTest构建/运行工具扩展为原生Windows入口，并让Windows job构建、运行Widgets与ECC客户端完整QtTest。71.65补强测试runner在失败夹具初始化时清理自有目录，并测试符号链接目标保护；71.66增加原生Windows runner对QtTest内嵌构建目录与回执路径一致性的启动前核验，入口回归8/8通过；71.67为传统逐节点地址和Native ECC拓扑来源增加模式提示，Qt5.12.8定向窗口回归3/3通过。71.68重建GetDAQ并经Ubuntu24候选loader/offscreen插件验证主窗口截图启动，通过专用原子刷新工具将该程序更新进Ubuntu24候选；只更新GetDAQ，另外26个程序复用原候选。GitHub Actions尚未实际运行，不可记作平台完整验收。71.60本机合成输入约32.0MiB且已清理；71.68未生成raw，临时构建与截图已清理。上述为71.68时期候选状态；71.85现已另建当前源码Ubuntu24全量候选，71.86另建含新窗口的Windows当前源码候选；71.69已让Qt5.14.2 Windows CI在完整构建/QtTest后部署、校验并上传可检查运行包，但Actions尚未运行，当前候选没有因此更新。71.70修正Ubuntu24候选GetDAQ启动smoke的XDG用户数据隔离并要求首启日志成功；候选仍只更新GetDAQ，其余26程序未重建。71.71在Ubuntu20.04.6/GCC9.4/Qt5.12.8从当前源码重新全量构建27程序，当前配置核心149/149、Widgets103/103、原生CTest22/22及入口回归10/10通过；loopback测试raw已自动删除。71.72将10份随包配置示例同步到当前源码，并分别通过Ubuntu24候选27程序/486文件与Windows候选6程序/273文件完整性复核；程序二进制没有更新。71.73纠正跨主机归档验收范围：用户选择无认证普通TCP/IP；当前源码已有TCP监听、发送及显式续传，71.38完成本机loopback协议回归。两台独立主机的TCP网络/防火墙/磁盘互通仍待实测，SSH只保留为显式兼容入口，不再列为必需验收。71.74将配置示例候选刷新器的2项自动化测试加入Ubuntu/macOS原生数据CI，本机测试2/2、Sentry示例检查及工作流合同通过，GitHub Actions未运行。71.75固化该回归在workflow中的步骤顺序，本机Windows入口合同测试11/11通过。71.76为旧SOAP客户端补齐SOAP 1.1要求的空SOAPAction，本机Qt完整回归104/104通过；模拟HTTP服务验证请求头，真实gSOAP进程互通仍未验收。71.78尝试Windows Qt进程时遇到`UtilBindVsockAnyPort: socket failed 1`；71.81通过扩展权限成功运行用户Qt5.14.2/MinGW的六程序构建、原生QtTest和运行包smoke。GitHub Actions仍未运行。Conduit的Rev5解析限制和默认11个CoBo地址映射尚未实机联调；71.86已验证当前Windows原生构建、QtTest和初始窗口smoke；独立Windows现场人工操作与macOS真实运行仍待验。详见[CI验证71.74](VALIDATION.md#7174-配置示例候选刷新回归纳入ci-2026-10-21)及[普通TCP归档回归71.38](VALIDATION.md#7138-当前源码无认证普通tcp归档传输回归2026-10-20)。71.78复核旧GetBench的SOAPAction规则：SOAP 1.1空action合法；旧soapcpp2命令未启用`-a`/`-A`，按官方文档推断仍按Body分派；该结论尚无真实服务进程验证，协议文档已同步到Ubuntu24和Windows候选。


## 71.84 已失败节点交接补齐的操作窗口

GetDAQ新增 **Failed data-node handoff completion...** 窗口，为仍在原实例、原所有者和正常停止封存周期中的明确失败交接提供受限操作入口。界面调用同目录的 `getdaq-supervisor-query`；生成请求只做只读校验，保存后需人工检查并明确确认才可提交一次。窗口不进入DAQ数据路径、不读取凭据内容；加载已保存请求只能观察。Ubuntu20.04.6/Qt5.12.8本机真实服务夹具通过生成、取消、一次提交、观察、重载只读验证；8KiB合成raw已删除。见[操作说明](FAILED_CONTROL_HANDOFF_COMPLETION.md)和[验证71.84](VALIDATION.md#7184-失败节点交接补齐qt窗口本机联调2026-10-21)。

该窗口只覆盖原实例中明确failed且正常封存的窄恢复条件；不代表冷启动恢复、超时/unknown重试或硬件恢复。其Ubuntu24全源码与当前Windows程序、窗口回归和新检查包已在71.85–71.86完成验证；独立Ubuntu24主机、Windows人工现场操作与macOS原生验收仍待做。整体重构仍未完成。

## 71.85–71.86 失败交接窗口纳入Linux/Windows当前源码验证

71.85在Ubuntu24.04.5用户空间full suite从当前源码重建27程序；Core149/149、Widgets105/105、原生CTest22/22，迁移/事务/接收协议/安全及失败交接窗口生产进程联调全部通过。聚合合成wire 1,073,799,755字节，raw余量0。新Ubuntu24候选`getdaq-ubuntu24-20261021-71-84-current-source-full-candidate`含27程序/174 manifest项；GetDAQ经目标loader帮助、offscreen启动/ready日志、截图和SOAP/UI回归。71.86用户Windows Qt5.14.2/MinGW7.3当前源码六程序构建通过；原生Widgets104通过/0失败/1个Unix锁用例跳过、ECC客户端63/63，新增窗口测试通过，263文件新运行候选完成SDK-free及原生窗口smoke。此用户空间/本机证据不等于独立干净机器或现场验收。macOS原生、真实两机TCP、旧gSOAP现场进程、板卡及持续物理DAQ吞吐仍缺。证据：[验证71.85–71.86](VALIDATION.md#7185-ubuntu24045当前源码全套验证纳入失败交接窗口-2026-10-21)、[Ubuntu24手册](UBUNTU24_VALIDATION.md)、[Windows手册](WINDOWS_QT5142.md)。

## 71.83 Ubuntu24当前交付接收器四路16GiB负载回归

当前Ubuntu24候选中的确切`getdaq-receiver`交付二进制通过四路各4GiB的localhost合成接收、全文件SHA/帧数/EOF/seal核对；载荷共16GiB且自动删除，清理回执剩余0。测试运行于Ubuntu24用户空间/WSL2宿主内核，载荷小于主机内存，故不代表真实DAQ吞吐或持续物理磁盘性能。详见[验证71.83](VALIDATION.md#7183-ubuntu24当前交付接收器四路16gib负载回归2026-10-21)及[接收性能说明](RECEIVER_PERFORMANCE.md)。

## 71.82 Ubuntu24当前交付catalog普通TCP逐进程回归

在Ubuntu24.04.5用户空间/rootfs中，对当前候选交付目录里的确切`getdaq-catalog`二进制进行无认证普通TCP测试。候选程序SHA=`180f0960a093aa41f355dbc5e3dfea2c9e0182629c461b5cc67bdb813202163d`，与测试收据绑定SHA一致。无认证收发、中断后显式续传、读回、完成后零载荷核对、独立listener/sender CLI和连接拒绝边界均通过。生成/删除合成raw各15,245,312字节，上限32MiB，剩余0；候选更新专项回执后为27程序/178文件。测试仍为WSL2宿主内核上的同机loopback，不证明真实双机/防火墙/磁盘、板卡或持续DAQ吞吐。详见[验证71.82](VALIDATION.md#7182-ubuntu24当前交付包普通tcp归档专项回归2026-10-21)及[归档传输手册](CATALOG_TRANSFER.md#13-普通-tcpip-发送已完成归档)。

## 71.81 Windows当前源码全量操作端包与原生Qt测试

用用户Windows Qt5.14.2/MinGW7.3从当前源码构建六个控制程序；Qt Widgets Windows原生QtTest 103通过/0失败/1项Unix锁跳过，ECC客户端63/63通过。SDK-free PATH六程序帮助检查、GetDAQ offscreen和Windows原生窗口smoke均通过，运行包完整性268文件通过。候选为`build-packages/getdaq-windows-qt5142-20261021-71-80-current-source-candidate/`，六程序、匹配运行库和测试摘要已包含在包内。测试TEMP清理完成，无raw、硬件或DAQ传输。见[验证71.81](VALIDATION.md#7181-windows-qt5142mingw当前源码构建测试和运行包2026-10-21)。该验证在用户本机Windows进程完成，不是独立干净Windows、真实ECC服务连接或硬件验收。

## 71.80 Ubuntu24.04.5用户空间全套验证和当前候选包

现有已验签Ubuntu24.04.5用户空间通过项目WSL2 runner完成`--stage all`。GCC13.3.0、CMake3.28.3、Qt5.15.13下，当前源码full profile 27程序全量构建成功；配置核心149/149、Widgets104/104、原生CTest22/22、迁移/事务、接收器协议与安全、四路1GiB接收回归通过。综合wire数据1,073,791,563字节，低于20GiB，生成raw经runner清理后余量0。使用Ubuntu24 loader逐一检查候选内27个程序，包内GetDAQ offscreen启动和ready日志通过。候选为`build-packages/getdaq-ubuntu24-20261021-71-80-current-source-full-candidate/`，27程序/176文件/约91MB；摘要及SHA见[验证71.80](VALIDATION.md#7180-ubuntu24045当前源码全套验证与27程序候选包2026-10-21)。该用户空间共享WSL2宿主内核，不算独立Ubuntu24机器验收。

本次使用标准现有WSL runner；第一次受限sandbox启动被Windows WSL互操作拦截，使用获准的本机工具执行后完整通过。全套执行期间未连接真实硬件。剩余验收包括独立Ubuntu24、Windows和macOS原生、跨主机普通TCP、真实旧gSOAP进程、硬件及现场持续吞吐。

## 71.79 当前源码全量构建及回归

在Ubuntu20.04.6/WSL2从当前应用源码新建隔离目录全量重建27个程序；105个嵌入依赖检查通过。配置核心149/149、完整Widgets 104/104、原生CTest 22/22、迁移与配置事务回归、receiver协议传输和1GiB四路接收/轮转/校验均通过。负载清理及runner安全场景共9项通过，入口合同13/13，配置示例刷新2/2；本轮合成数据总wire 1,073,791,563字节，清理后`.graw`剩余0。证据摘要见[验证71.79](VALIDATION.md#7179-当前源码全量重建平台门槛修正与有界接收回归2026-10-21)。

该回归发现并修正Ubuntu24平台runner的Widgets测试数：SOAPAction用例加入后，实际QtTest总数已由103变为104，旧门槛会误拒绝通过的测试。runner现在锁定104，并有入口合同测试防止阈值过期。此轮仍在Ubuntu20 WSL2本机loopback完成，不代表Ubuntu24、Windows、macOS、独立双机、真实旧gSOAP服务、硬件板或现场持续吞吐验收。后续优先在真实Ubuntu24/GitHub Actions跑`--stage all`，并完成两台主机的普通TCP归档互通和Windows/macOS原生验证。

## 71.78 旧 gSOAP SOAPAction 分派方式源码复核

旧 `ecc.hh`没有声明方法action，旧生成命令未启用gSOAP的`-a`/`-A`按SOAPAction分派选项。根据W3C SOAP 1.1与Genivia文档，空`SOAPAction`是合法的必需头值；推断旧服务继续按SOAP Body方法名分派。此结论来自旧接口/构建源码与官方生成器规则，不是旧服务二进制或现场互通证据。详见[SOAPAction复核记录](VALIDATION.md#7178-旧-getbench-soapaction-分派依据复核-2026-10-21)和[通信协议说明](PROTOCOLS.md)。

## 71.77 Ubuntu24候选SOAP请求头证据补齐

GetDAQ及Widgets测试程序已按当前源码重建。定向拓扑提示与SOAPAction Qt场景均3/3通过，刷新回归合同12/12通过。Ubuntu24用户空间loader下的新GetDAQ候选通过帮助、Qt窗口、截图与启动日志检查，候选完整性为27程序/486文件；只替换GetDAQ，其余26程序字节复用。候选程序SHA-256=`b9adc00a4d5ccc8f720c45aec5ea44757c539063416da14aa319d1c6744ede23`。此回归使用本机mock SOAP服务，不证明真实gSOAP互通，也不是独立Ubuntu24、Windows/macOS或硬件验收。证据见[验证记录71.77](VALIDATION.md#7177-ubuntu24候选纳入旧-soap-请求头回归2026-10-21)。

71.44收紧`getdaq-sentry`配置解析：process/disk/bind必须是字符串，port/settle必须是有限整数并在现有范围内。Qt 5.12.8/C++11从当前源码重建测试程序，`sentryConfigStrictFields`定向通过（3/3含init/cleanup），无DAQ数据或新依赖。详见[验证71.44](VALIDATION.md#7144-sentry配置拒绝类型错误与非整数参数2026-10-21)。

71.43补齐DataSupervisor测试runner在失败退出时清理自有`.graw`：20GiB聚合检查不变；异常退出由退出回调清理，CLI入口显式捕获Ctrl+C并清理后以130退出。清理仅作用于runner私有证据根的普通raw文件和符号链接，不触碰目录外文件；SIGKILL/断电不在保证范围。两个隔离测试、当前源码构建和默认服务矩阵19/19通过；23个测试`.graw`共339,968字节已删除、余量0。详见[验证71.43](VALIDATION.md#7143-datasupervisor失败退出清理自有合成载荷2026-10-20)。

71.42将DataSupervisor默认19个服务/接收器生命周期场景接入Linux CI，并使runner按20GiB聚合硬上限统计合成raw，CI结束时仅清理runner私有测试根内的raw。当前源码全矩阵19/19、8项HTTP故障、codec/credential及前置归档CTest 3/3通过；23条`.graw`路径共339,968字节，全部删除、余量0，临时构建已清理。此为本机Linux重放，不代表Actions/Mac/硬件验收。见[验证71.42](VALIDATION.md#7142-datasupervisor完整服务矩阵接入linux-ci及测试载荷清理2026-10-20)。

71.41将71.40的DataSupervisor计划合同接入`.github/workflows/native-data-path.yml`的Linux job：从原生数据工具构建、3项归档/协议CTest，到同一CMake树启用Qt服务、构建服务及运行preconditions/HTTP故障fixture均按CI顺序验证通过；macOS继续使用无Qt数据面。workflow YAML解析成功，测试后无`.graw`，临时构建已删除。当前结果是本机重放，不是GitHub Actions或Mac验收。详见[验证71.41](VALIDATION.md#7141-linux-ci纳入datasupervisor真实服务计划合同2026-10-20)。

71.40把DataSupervisor集成夹具中原先未单独锁定的`--plan-config`纳入常规回归：规范计划JSON必须与输入计划一致，原始输出SHA必须等于`--check-config`摘要，且离线命令不得泄漏token或创建audit/data。修改后的Python runner在GCC9.4/C++11/Qt5.12.8当前服务和接收器上，preconditions、8项HTTP故障、codec、credential共4组退出码0，无raw生成；临时目录已清理。见[验证71.40](VALIDATION.md#7140-datasupervisor检查计划输出回归合同2026-10-20)。

71.39补齐DataSupervisor离线服务命令验证：当前源码以GCC9.4/C++11/Qt5.12.8构建`getdaq-supervisor`后，`--check-config`与`--plan-config`均成功；SHA绑定到候选接收器，且未启动服务、创建audit/data根或打开端口。先前缺少成功结果是沙箱禁止枚举本机IPv4接口，并非代码失败。临时配置、凭据和构建已清理，没有生成raw。该项不等于运行HTTP服务或现场硬件验收，见[验证71.39](VALIDATION.md#7139-datasupervisor离线配置检查与计划输出2026-10-20)。

71.38复核用户要求的跨主机归档传输：当前源码普通TCP入口不需要凭据，SHA-256只检查内容。GCC9.4严格C++11重新构建`getdaq-catalog`及传输测试；获准本机loopback后，发送、监听、拒绝连接、中断续传、读回、完成后零载荷续传及12项底层传输场景全部通过。合成raw生成/删除15,245,312字节，测试私有目录清空。修正[Run归档架构说明](DATA_RUN_CATALOG.md)为普通TCP主路径并补充无认证安全边界。该次是当前源码单项本机验证，没有改或重建Ubuntu24/Windows部署包；真实双机、防火墙、macOS、板端与持续DAQ仍待验收，见[验证71.38](VALIDATION.md#7138-当前源码无认证普通tcp归档传输回归2026-10-20)。

71.37补齐旧CoBoFrameViewer本地`.gmat`消费者对应的离线波形转换。新增无Qt `getdaq-matrix`，按Viewer读取的AGET/channel布局输出binary64 little-endian，并保持旧转换器每通道的样本到达顺序，且只接受每通道512项完整type 1波形；旧type 2、稀疏/重复/损坏帧会拒绝。默认20GiB输出硬上限，不覆盖已存在路径，失败清理同目录临时文件。严格C++11下专项合成回归通过：输入/输出/索引和值校验、start/count、硬上限和临时文件清理；本轮生成并删除3,900,480字节合成raw，raw剩余0；可执行仅依赖系统C/C++库。新工具已加入data/data-host/full构建计划；Ubuntu24候选现已增量加入该程序并完整核对27程序/481文件，交付SHA=`ba6f83d5587076b7654df019130ea55df97d2b85e97dd7b84768a26c79426c52`，刷新包含目标二进制、格式手册和SHA绑定专项收据。回归合成输入修正为旧格式真实MFM块计数，覆盖revision 0–5。完整receiver CTest中两个需要loopback socket的既有测试被本沙箱拒绝，其余mfm-merge和receiver-core通过；新gmat专项单独通过。真实DAQ文件、旧Viewer联合打开、独立Ubuntu24/macOS和现场设备尚未验收；说明见[矩阵转换手册](DATA_MATRIX.md)及[验证71.37](VALIDATION.md#7137-coboframeviewer-gmat波形矩阵转换2026-10-20)。

历史快照（截至71.52，2026-10-21）：GetDAQ主机控制台、ECC控制和配置工具与原生C++ DAQ接收、存储及传输链路保持分离；Qt只承担低频操作界面。按用户要求，跨主机已封存归档使用无认证普通TCP/IP，SHA-256仅作内容校验。板端`getHwServer`/`IceEirfu`不在重构范围，仍需现场版本互通验收。Ubuntu24候选`build-packages/getdaq-ubuntu24-20261020-71-34-current-source-candidate`现为27程序/484清单文件，在26程序/477文件全量包上增量加入matrix、刷新Sentry并更新receiver；专项回归绑定各自程序SHA，且以Ubuntu24 loader直接运行当前receiver交付字节并通过32MiB慢客户端回归，不代表27程序全量重建。receiver SHA=`8d0f35b61fe33448760ca6e5834b0f1f14de6094282eddb85ddfcd31b65f7b3d`。Windows候选六程序/273文件仅更新文档，运行文件未变。当时仍需独立目标主机、实机、双机、macOS与持续DAQ验收；该快照不能代替本页当前71.93–71.96证据。

71.33补齐旧DataRouter的`ByteStorage`用法：`getdaq-receiver`现在可显式用`--storage-mode byte-storage`把普通TCP/ZBUF输入原样落盘，默认`frame-storage`继续负责MFM解析与帧导出；字节模式拒绝FDT及面向解析帧的功能，避免把FDT控制字节误存成原始MFM流。新模式已通过Ubuntu24用户空间候选程序4项回归，raw生成/清理共786,818字节、剩余0。当前Ubuntu24候选只增量替换接收器、复用其余25个程序。见[验证记录71.33](VALIDATION.md#7133-datarouter-bytestorage字节存储模式补齐2026-10-20)。

71.34将`storage_mode`纳入DataSupervisor来源固定计划、规范摘要、接收器启动参数、运行状态、结束收据与catalog校验。原始字节模式现可由托管ICE入口使用；计划校验拒绝FDT和帧导出组合。Ubuntu20.04.6/GCC9.4严格C++11本机构建通过；loopback实测11个托管ICE、混合TCP/ICE及异常生命周期场景通过，其中ByteStorage场景逐字节对照65,537字节任意二进制载荷，成功完成接收、seal、catalog，并记录`storage_mode=byte-storage`。此轮合成raw累计生成并清理434,180字节，raw余量0；测试runner上限8MiB，远低于20GiB。它只验证本机兼容层，不代表真实ICE旧客户端、双主机、Ubuntu24独立主机、macOS、硬件或持续吞吐验收。见[验证记录71.34](VALIDATION.md#7134-ice-bytestorage接入datasupervisor固定计划2026-10-20)。

71.35在Ubuntu24.04.5用户空间/WSL宿主内核中从当前源码全量重建26程序，并运行完整平台回归：配置核心149/149、Widgets102/102、原生CTest21/21、迁移/事务/receiver传输/安全及四路1GiB接收全部通过，平台测试聚合wire 1,073,791,563字节、raw余量0。新候选`build-packages/getdaq-ubuntu24-20261020-71-34-current-source-candidate`绑定当前构建，26程序均通过Ubuntu24 loader检查和逐程序启动检查；manifest 477文件完整。候选内实际二进制另通过TCP ByteStorage、ICE入口15项及托管ICE/混合生命周期11项回归，全部raw清理；候选还包含完整构建/平台及专项回执。Windows Qt5.14.2候选只刷新了文档，六程序和运行库SHA保持不变。此验证不代表独立Ubuntu24机器、macOS、真实双机/板端、现场数据或持续DAQ吞吐验收。详见[验证记录71.35](VALIDATION.md#7135-ubuntu24全量候选与托管字节存储回归)。

71.36补齐DataSupervisor计划codec对`storage_mode`的直接合同测试：默认`frame-storage`仍省略字段，保持旧规范计划字节；`byte-storage`参与计划身份并往返保持；未知值及FDT/帧导出冲突拒绝。GCC9.4/C++11构建后codec通过。该变化只修改测试，不改产品二进制。Qt服务层`--check-config`/`--plan-config`此前因沙箱禁止netlink接口枚举缺少成功证据，已由71.39在当前源码/Qt5.12.8下补验；服务运行、实际HTTP生命周期仍需独立验证。见[验证记录71.36](VALIDATION.md#7136-datasupervisor-bytestorage计划codec边界回归2026-10-20)。

71.22拆分标量修复按真实旧CCfg/TinyXml回归确认：显式type节点保留旧库最后text node语义，原始拆分XML只读；配置核心148/148通过，192用例/384次旧新进程对照符合预期。71.23从当前源码在WSL Linux宿主重建26个程序并发布新候选`getdaq-linux-host-20261020-split-scalar-candidate`（170文件）；26/26可由现有Ubuntu24 rootfs解析动态依赖，但该包是glibc2.31/Qt5.12.8宿主构建，不宣称为Ubuntu24原生构建。原Ubuntu24候选没有本轮源码修复。Windows使用用户Qt5.14.2 MinGW实际重建六个程序，生成新候选`getdaq-windows-qt5142-20261020-split-scalar-candidate`（262文件）；manifest、SDK-free PATH及GetDAQ初始窗口启动检查通过。拆分值GUI只读行为及CDATA正常编辑/撤销两个专用QtTest随后在同一Windows Qt5.14.2 MinGW上各3/3通过。整套Windows Widgets QtTest用正常`qwindows`插件完成101/101通过、1项Unix锁测试跳过；offscreen插件曾中断于消息框路径，不计为失败。完整人工配置流程仍未验证。

71.24从当前源码在Ubuntu24.04.5用户空间重新全量构建26个程序，并重跑完整平台runner：配置核心148/148、Widgets102/102、原生CTest20/20、迁移/事务/receiver传输/安全/四路接收全通过。四路合成raw为1GiB、总wire约1.074GB，测试自动删除且raw余量0。首轮暴露旧核心/GUI计数门槛，第二轮暴露工作目录复用时旧负载证据被重复收集；两处runner问题均修正，第三轮完整`all`状态通过。此为Ubuntu24用户空间运行在WSL内核，不是独立Ubuntu主机或现场验收。

71.25使用Windows Qt5.14.2 MinGW重新构建当前源码Widgets和ECC客户端测试程序，并在原生Windows插件下运行：Widgets101通过/1项Unix锁测试跳过、ECC客户端63通过，均无失败；临时根目录清理，raw余量0。结果与构建回执已并入当前Windows候选。详情见[验证记录71.25](VALIDATION.md#7125-windows-qt5142当前源码widgets与ecc客户端完整回归2026-10-20)。

71.26修正归档最终目录发布的非Linux/macOS POSIX回退：不再执行有竞态的“检查后普通rename”，而是无原子独占API时失败关闭并保留partial；macOS错误会报告errno。当前Ubuntu20/GCC9.4严格C++11构建通过，接收归档/传输/控制测试3/3通过，其中包含竞争目标不被替换回归。测试首次受默认沙箱禁止loopback影响，获准本机loopback后通过。Apple SDK及macOS Actions尚未运行；独立Ubuntu24、macOS目标卷、真实双机、板端和持续DAQ验收仍待。详情见[验证记录71.26](VALIDATION.md#7126-跨平台归档发布失败关闭与独占目录回归2026-10-20)。

71.27把归档流的SIGPIPE防护从进程级处理器切换为传输线程局部屏蔽，回归确认活动传输不会修改全局处理器；关闭对端时以可捕获错误结束。Ubuntu24用户空间普通TCP回归通过，当前Linux候选只增量替换`getdaq-catalog`并完成包完整性检查。此项不是两台独立主机、macOS或持续吞吐验收。详情见[验证记录71.27](VALIDATION.md#7127-归档流sigpipe线程隔离与ubuntu24候选增量刷新2026-10-20)。

71.28在Ubuntu24.04.5用户空间从当前源码重新全量构建26程序并重跑平台runner：核心148/148、Widgets102/102、CTest20/20、迁移/事务/receiver回归全通过；四路接收1GiB合成raw及约49KiB其他传输全部清理，wire总量1,073,791,563字节，远低于20GiB。该全量构建没有替换部署候选；当前候选仍由71.24全量构建和71.27 `getdaq-catalog`增量刷新组成。Ubuntu24用户空间运行于WSL内核，独立Ubuntu24、现场硬件、双机及持续吞吐仍待。详情见[验证记录71.28](VALIDATION.md#7128-ubuntu24当前源码全量构建与平台回归复核2026-10-20)。

71.29已把71.28当前源码的26个Ubuntu24程序整理成新的完整部署候选`build-packages/getdaq-ubuntu24-20261020-71-28-current-source-candidate`，不再沿用旧候选中的程序。全量构建回执中的26个程序SHA逐一匹配；包内Ubuntu24 loader启动检查26/26通过，归档普通TCP专项测试也绑定新`getdaq-catalog` SHA并通过，合成raw生成/删除15,245,312字节、剩余0。新包195个manifest文件，包含全量构建与平台报告、核心/Widgets结果、runner日志及启动和TCP专项回执；旧候选保留不动。此包仍只在WSL宿主内核的Ubuntu24.04.5用户空间验证，不代表独立机器、真实双机、板端、macOS或持续DAQ验收。详见[验证记录71.29](VALIDATION.md#7129-ubuntu24当前源码全量候选及归档tcp验收2026-10-20)。

71.30复核正常采集收尾后，修正`DATA_SENDER_CONTROL.md`把系统级生产协调称为“待实现”的过时描述：当前NativeEccCoordinator/`SystemAcquisitionClient`已连接停止生产与发送器刷新、Breakup断开、接收服务Finish及停止核验；Unprepare仍是独立阶段，Stop应答本身仍不证明EOF或seal。同步修正普通TCP回归runner报错标签残留的“authenticated-TCP”。对Ubuntu24候选中的确切`getdaq-catalog`重跑plain-TCP测试，失败路径/成功场景均由当前runner执行，生成并清理15,245,312字节合成raw，余量0；包内回执已绑定新runner SHA，完整候选SHA校验及包文档刷新通过。上述是本机合成服务验证，不等同于现场固件行为或实机采集验收。详情见[验证记录71.30](VALIDATION.md#7130-正常daq收尾文档边界和普通tcp回归标签校正2026-10-20)。

71.31修正硬件配置编译器和通用配置编辑器之间的兼容边界：XcfgDocument仍保留未知 XML 属性以支持无损编辑；ExperimentReader在硬件参数编译前按旧 CompoundConfig 规则检查所有已加载/include 节点，scalar仅允许`id/type/unit/range`，object仅允许`id/include`，防止未被当前编译命令查询的未知属性被静默带过。共享泛型路径的unit/range解析保持有效。严格C++11/Qt5.12.8编译与配置工作区QtTest149/149通过；新getdaq-config还用收到的外部触发系统配置和pedestal CoBo配置完成compile-system/compile-cobo-configure。宿主产物已在`bin/getdaq-config`，Ubuntu24候选尚未按此源码刷新，现场配置/板卡行为仍待核对。详情见[验证记录71.31](VALIDATION.md#7131-旧compoundconfig未知属性边界与真实配置编译回归2026-10-20)。

71.32用用户Windows Qt5.14.2/MinGW7.3按当前源码重建6个操作程序；严格C++11配置共享QtTest在原生Windows148项通过，Widgets完整回归101通过/1项Unix锁测试跳过，ECC客户端63/63通过。六程序全新运行候选`build-packages/getdaq-windows-qt5142-20261020-legacy-attr-boundary-candidate`完成匹配Qt/MinGW运行库部署、SDK-free PATH帮助检查、offscreen和Windows原生初始窗口检查，272项manifest验证通过；候选含两套QtTest结果、清理回执及真实配置摘要。另在Ubuntu24.04.5用户空间按当前源码全量重建26个程序；配置核心149/149、Widgets102/102、原生CTest20/20及迁移、事务、接收回归全部通过，四路接收1GiB、合计wire 1,073,791,563字节、raw清理0。新Ubuntu24候选`build-packages/getdaq-ubuntu24-20261020-71-32-current-source-candidate`含26程序/817个清单文件，包内程序启动检查26/26通过，当前getdaq-config真实配置编译和未知属性拒绝回归通过。两候选均只证明当前用户Windows Kit和WSL宿主内核Ubuntu24用户空间，不等同于独立机器、完整人工流程、硬件或持续DAQ验收。详情见[验证记录71.32](VALIDATION.md#7132-windows当前源码配置边界回归及运行候选2026-10-20)。

仍未完成独立Ubuntu24主机完整回归、macOS构建运行、干净Windows完整人工配置验收、真实双机网络/防火墙、旧gSOAP/板端互通、现场硬件操作及持续DAQ压力。最新18GiB localhost候选压力回归仍低于52.6GB物理内存，不能证明超页缓存或真实DAQ磁盘速率。整套重构尚未完成。详情见[验证记录71.32](VALIDATION.md#7132-windows当前源码配置边界回归及运行候选2026-10-20)、[71.31](VALIDATION.md#7131-旧compoundconfig未知属性边界与真实配置编译回归2026-10-20)、[71.30](VALIDATION.md#7130-正常daq收尾文档边界和普通tcp回归标签校正2026-10-20)、[71.29](VALIDATION.md#7129-ubuntu24当前源码全量候选及归档tcp验收2026-10-20)、[71.28](VALIDATION.md#7128-ubuntu24当前源码全量构建与平台回归复核2026-10-20)、[71.27](VALIDATION.md#7127-归档流sigpipe线程隔离与ubuntu24候选增量刷新2026-10-20)、[71.26](VALIDATION.md#7126-跨平台归档发布失败关闭与独占目录回归2026-10-20)、[71.25](VALIDATION.md#7125-windows-qt5142当前源码widgets与ecc客户端完整回归2026-10-20)、[Windows/Linux/macOS构建说明](HOST_SUITE_BUILD.md)。

## 71.19 当前全量构建清单和Ubuntu24 runner同步（2026-10-20）

`getdaq-merge`进入统一profile后，Ubuntu24 full runner仍硬编码25程序，数据profile交付测试仍硬编码4程序；旧数据profile断言失败已复现。runner现从`build-suite.py` full计划推导准确程序名集合，并要求其中有`getdaq-merge`；数据profile测试比较具体目标集合。4项host-suite交付测试通过。当前源码完整full profile在Ubuntu20.04/WSL2宿主、GCC9.4/Qt5.12.8构建26程序全部成功，内嵌依赖检查105文件通过，回执程序名和平台runner计划逐项一致。

Ubuntu24候选仍是增量加入MFM合并器的26程序包，不把Ubuntu20全量构建写成Ubuntu24重建；本轮没有在Ubuntu24 rootfs中执行完整平台runner。完整边界见[验证记录71.19](VALIDATION.md#7119-当前全量构建清单和ubuntu24-runner同步2026-10-20)。

## 71.18 原生数据接收器CMake测试开关修正（2026-10-20）

`apps/datareceiver`此前忽略`BUILD_TESTING=OFF`，发布构建仍生成并注册测试/基准目标。现已改为标准CTest开关：OFF只配置接收器、检查器和合并器；ON注册三个测试及benchmark。GCC9.4/CMake3.16两种配置均实测：OFF三个产品目标构建成功、CTest 0项且无测试目标；ON全量构建，loopback权限开放的CTest 3/3通过。此更改不改生产程序行为；候选产品SHA不变，包内文档/manifest已刷新并重新校验。详细见[验证记录71.18](VALIDATION.md#7118-原生数据接收器cmake测试开关修正2026-10-20)。

## 71.17 exporter-lock socket失败的沙箱归因复核（2026-10-20）

此前`exporter-lock`在受限执行环境中报`socket failed`，本轮从当前源码重建后确认：普通沙箱仍拒绝该loopback socket测试；获准的本机loopback执行中，完整原生CTest `mfm-merge`、`receiver-core`、`exporter-lock` 3/3通过。因此该既有回归失败归因于运行沙箱权限，不是当前导出器或合并器缺陷，生产代码无需变更。仅传输数KB测试数据，无raw产物；目标平台和真实网络仍未验收。具体证据见[验证记录71.17](VALIDATION.md#7117-exporter-lock-socket失败的沙箱归因复核2026-10-20)。

## 71.14 C++11 MFM按事件离线合并器（2026-10-20）

将旧`CoBoFrameViewer`的`cobo-frame-merge`用途收敛为数据侧原生工具`getdaq-merge`，以MFM type `0xFF01`、revision 1输出事件分组后的分层帧；只索引事件号/偏移并以1MiB块复制原始帧，不经过Qt，不增加第三方依赖，不进入DAQ实时接收路径。输入支持CoBo type `0x01`/`0x02`对应受支持revision和旧工具的RIBF type `0x30`，其余类型跳过；限制单输入帧1GiB、索引帧数2000万（索引约480MiB上限），拒绝输出覆盖输入、不会覆盖现存输出，并在异常退出时清除本次创建的不完整输出。

回归在系统GCC9.4/C++11临时构建目录`/tmp/getdaq-merge-build`执行：`getdaq-merge`、`getdaq-inspect`和merge测试编译通过；`mfm-merge`专项测试通过，验证事件排序/分组、类型过滤、内部原帧逐字节保持、MFM输出头及已有目标文件保留。`python3 tools/build-suite.py --profile data --build-root /tmp/getdaq-merge-suite-build --jobs 2`从当前源码构建5个程序成功，`getdaq-merge`纳入原生接收构建列表及Qt/Boost/Ice/yaml/curl运行依赖否决检查。完整receiver CTest中`receiver-core`通过，既有`exporter-lock`在当前环境报`socket failed`，未归因于合并器；需在允许socket测试的目标构建环境重跑。测试原始输入合计不足1KiB，测试生成文件已清理，远低于20GiB上限。

这项验证不是旧`cobo-frame-merge`二进制差分，不证明真实CoBo/RIBF采集格式全覆盖或下游ROOT/FITS兼容，也不是大文件/磁盘性能测试。当前Windows候选尚未加入这个POSIX数据工具；Ubuntu24旧候选也未重新打包。复现参数、选项及格式说明见[MFM合并手册](DATA_MERGE.md)。

## 71.15 CoBoFrameViewer转换器范围及原生CI合同（2026-10-20）

静态核对旧`CoBoFrameViewer`入口与实现后，确认`graw2root`/`root2disp`是一组依赖ROOT的后处理与显示工具，`graw2fits`依赖CCfits/CFITSIO并生成逐事件/通道FITS及索引；它们不属于数据接收/控制运行必需项。现已新增可选`getdaq-fits`复现受限type-1 rev0–5转换，并新增`getdaq-root`保留旧`tree/GDataFrame`及`TT2K/T2KEvent`对象树主路径；ROOT显示、拟合和完整分析仍作为独立外部工作流。`graw2gmat`旧程序调用固定CoBo头布局解析，并对每个通道无条件输出512个样本，而旧`Channel::data()`只保证返回现有样本向量；在未证明每个通道总有512项前，存在越界读取风险。当前找不到格式规格或下游消费者，因此不照搬此输出，若实验组需要须先取得样例与消费程序确认字节序及缺样本语义。依据旧`CoBoEvent.cpp`、`Channel.cpp`、`graw2gmat`及GET工具文档；审阅与范围决定记在`SOURCE_REVIEW_AND_PLAN.md`。

`native-data-path.yml`现将`mfm-merge`纳入Linux和macOS receiver测试合同，预期总数分别为3和2；动态无Qt检查也加入`getdaq-merge`。本地PyYAML解析、矩阵/正则/数量断言通过；`verify-dependencies.py --profile data`确认无需下载依赖；文档链接检查1245项通过。此轮未触发GitHub Actions，macOS实机/CI仍没有新证据。合并器的实际C++11构建及专项测试见71.14。

## 71.10 Ubuntu24新候选认证TCP归档CLI回归（2026-10-20）

最新Ubuntu24候选中的`getdaq-catalog`实际二进制通过普通TCP CLI、错误密钥拒绝、partial保护、正确凭据续传、归档读回、CLI分进程收发及关闭端口超时回归。候选现有182个清单文件，具体数据预算、SHA和限制见[验证记录71.10](VALIDATION.md#7110-ubuntu24新候选认证tcp归档cli回归2026-10-20)。

## 71.11 Ubuntu24候选四路并发接收回归（2026-10-20）

当前Ubuntu24候选`getdaq-receiver`四路localhost并发接收合计1GiB，逐流发送/落盘SHA、帧数、seal和EOF均核对通过，生成raw全数清理。候选现为25程序/193清单文件，包含逐流日志、摘要及清理回执。测试未超过物理内存，不作为DAQ速率或持续磁盘吞吐验收；详见[性能回归记录](VALIDATION.md#7111-ubuntu24候选接收器四路并发回归2026-10-20)。

## 71.09 Ubuntu24当前源码全量构建及平台回归（2026-10-20）

在Ubuntu24.04.5用户空间/GCC13.3/Qt5.15.13/glibc2.39中，从当前源码完整重建25个主机程序并通过平台回归：配置核心147项、GUI101项、原生contract19项，以及迁移/事务/接收传输用例。初次回归已全数运行成功，但守门器使用旧GUI计数99导致总状态failed；按实测将门槛改为101后完整重跑为complete。新候选`build-packages/getdaq-ubuntu24-20261020-classinfo-candidate`含182个SHA清单文件和验证摘要。它仍使用WSL宿主内核，不等于独立Ubuntu24机器、真实板端、双主机或持续吞吐验收。详见[验证记录71.09](VALIDATION.md#7109-ubuntu24当前源码全量回归及交付候选2026-10-20)。

## 71.08 include共享类属性双向补齐（2026-10-20）

include导入的unit/range现在会补齐已解析但没有ClassInfo的同类本地成员；双方均有ClassInfo且不同才报冲突。严格C++11核心147项通过；Qt5.12.8配置GUI相关4项通过。完整Linux GUI为89通过、12失败，失败均因沙箱禁止本地监听端口。用户Windows Qt5.14.2/MinGW回归为Widgets 100通过、1项POSIX锁跳过，ECC客户端63通过；六个操作端由当前源码重建，候选265文件校验及SDK-free、offscreen、Windows原生窗口smoke均通过，测试raw已清理。细节见[验证记录71.08](VALIDATION.md#7108-include共享类信息双向补齐及当前源码回归2026-10-20)。

## 71.07 旧数据库配置功能范围核对（2026-10-20）

源码显示GetBench数据库支持默认关闭，启用仍需外部ConfigDatabase包；旧ConfigWidgets数据库导入、导出和include动作为空。现场实际DBMS/URL/部署用法没有证据，因此没有把未知驱动编进新软件，也没有把旧后端源码误报为可用功能。源码依据和边界见[验证记录71.07](VALIDATION.md#7107-旧配置数据库依赖与功能入口核对2026-10-20)及[依赖减量方案](SOURCE_REVIEW_AND_PLAN.md#7-依赖减量策略)。方案文档已同步到Ubuntu24/Windows候选，程序和运行库摘要保持不变。

## 71.06 当前源码主机套件全量重建复核（2026-10-20）

Ubuntu20.04.6/WSL2上从当前源码完整重建25个主机程序；168文件临时暂存包通过清单校验，随后已删除临时产物。Ubuntu24候选25程序/175文件、Windows Qt5.14.2候选6程序/261文件只读复核通过，原生catalog仍无Qt运行时依赖。该项证明本机全量构建和现有候选完整性，不代表独立目标机、双机、真实硬件或持续吞吐验收；没有替换候选程序。详细命令与范围见[验证记录71.06](VALIDATION.md#7106-当前源码全套主机程序重建及候选包复核2026-10-20)。

已交付的[持久交接证据及离线审阅](COLD_HANDOFF_REVIEW.md)可以在服务停止、认证文件清理后查看历史，但不执行冷恢复或授予控制。70.63完成冷数据补齐原生核心，70.64已由[冷数据交接CLI](COLD_DATA_HANDOFF_COMPLETION.md)连接原ECC父记录、固定接班身份、明确补齐与只读回执重导出；原失败和旧硬件控制冻结保留。70.65新增[独立恢复服务及Qt窗口](COLD_DATA_HANDOFF_WINDOW.md)，通过固定意图调用原生CLI；本轮验证范围见VALIDATION70.65。70.66已补齐[完整交付构建组合联调](COLD_HANDOFF_DELIVERY_VALIDATION.md)，70.65的历史待补验标记不改。任意硬件冷恢复仍未完成。70.61的[Qt计划退休显式补齐](ECC_PLAN_TRANSITION_GUI.md#71-部分失败后在窗口显式补齐)及70.59[单板维护窗口](COBO_MAINTENANCE.md)继续保留。70.65的新候选目录名含`cold-handoff-window-candidate`，以本轮清单和验证记录为准；70.64的`cold-handoff-cli-candidate`不含新的恢复服务/窗口，70.62的`cold-handoff-evidence-candidate`不含后来的冷补齐CLI。不同候选包的覆盖以各自记录和清单为准。

板端 `getHwServer` / `IceEirfu` 保留现场部署，不纳入本次重构范围；新主机与它们的实际互通仍需要验收。旧有75%～80%是当时粗估，不能作为今天的测量值或验收比例。当前没有逐项加权、足以推导新百分比的验收表。

70.68已接入[配置文件路径迁移](CONFIGURATION_PATH_MIGRATION.md)：全部已知include共同改写，新旧路径的存在状态进入独立持久记录，窗口和CLI支持明确完成候选或恢复原始版本。最终核心普通/插桩各146项、完整界面98项、实际Windows145项及1项Unix权限夹具跳过；三组各12个真实进程中断恢复与两个活写者场景通过，两个新包内实际迁移CLI及双向恢复也通过。当前候选目录含`path-migration-candidate`，Linux25程序/162清单文件、Windows六操作端/255文件；清单、匹配DLL、无SDK PATH/help及初始窗口已检查。它不扫描未打开工程或非include路径资源，不是任意外部读者的跨文件原子事务；完整范围见VALIDATION70.68。

70.67的[共享读、独占写与中断加载保护](CONFIGURATION_SOURCE_TRANSACTIONS.md)继续保留；该阶段`config-transactions-candidate`包不含后续路径迁移代码，不改写历史候选或验证数字。

70.69已完成Ubuntu24用户空间的25程序构建和基础回归。70.70增加[在线导出锁隔离](EXPORTER_LOCK_ISOLATION.md)，将导出socket操作移到发布锁外，保留客户端预算及旧长度头。宿主和Ubuntu24各有普通/插桩验证，新receiver与原ECC和两个数据服务完成合成周期。新Ubuntu24候选目录含`export-lock-candidate`：一个接收器重新编译，其余24程序字节复用；Windows操作端仍为70.68候选。具体回执见VALIDATION70.70，不将复用程序说成全部重编译。

70.71进一步完成[实际Windows完整功能回归](WINDOWS_FUNCTIONAL_VALIDATION.md)：Qt5.14.2 MinGW的正常Windows插件/Fusion风格下，界面97通过/原有1项POSIX锁跳过，网络客户端63通过。修正GET空Content-Type告警，新Windows候选名含20261019-functional-candidate；Ubuntu24候选名含control-http-candidate，六控制程序重编译、其余19程序按字节复用。原候选保持不变，本机自动化不替代现场和独立机器验收。

70.72已生成并验证Ubuntu24控制候选`build-packages/getdaq-ubuntu24-20261020-failed-retirement-candidate`；针对历史插桩HTTP超时的当前复跑两次通过但未复现，根因仍未确定，不能视作修复或整套完成。为回应工作区空间占用，已在2026-10-20清除108份过期生成候选副本（约8.30GB），随后生成新的SSH归档候选；当前保留最新Ubuntu24 SSH归档候选`getdaq-ubuntu24-20261020-ssh-archive-candidate`、其控制候选基底和Qt5.14.2 MinGW Windows操作端候选。早期候选删除明细见`build-packages/cleanup-20261020.json`，详细运行边界见[验证记录70.72](VALIDATION.md#7072-已受理后失败的节点计划退休补齐2026-10-19-在研)及[70.73](VALIDATION.md#7073-跨主机ssh归档流的本机契约验证2026-10-20)。源码、测试报告和有效回归证据保留。

70.73已接入[跨主机SSH归档发送](CATALOG_TRANSFER.md#12-跨主机发送已完成归档)：`getdaq-catalog --send-catalog-ssh`使用系统OpenSSH，将已封存catalog及载荷从源机流式送入目标机独立`getdaq-catalog`。原始字节不经过Qt/ECC/HTTP；目标验证每个文件SHA-256并用平台独占重命名发布，冲突不覆盖。Ubuntu24候选`build-packages/getdaq-ubuntu24-20261020-ssh-archive-candidate`已加入该CLI：本阶段重建`getdaq-catalog`一个程序，其余24个从已验证控制候选复用；候选25程序均通过Ubuntu24动态加载器依赖检查。本机假SSH回归还以候选包内接收程序为远端端点通过。70.87/70.88新增真实系统OpenSSH客户端/临时localhost sshd、密钥认证和Ubuntu24候选接收程序回环，最大合成传输载荷134,225,920字节；70.88独立核对源fixture目录移除和目标raw删除，源/目标各4个文件共268,451,840字节已清理。仍不是两台机器验收。生产密钥/权限、真实目标卷、macOS及持续吞吐仍待验证。完整文档与边界见[验证记录](VALIDATION.md#7073-跨主机ssh归档流的本机契约验证2026-10-20)、[70.87](VALIDATION.md#7087-真实openssh-localhost归档回环验证2026-10-20)及[70.88](VALIDATION.md#7088-真实openssh有界128mib归档流回归2026-10-20)。

70.74修复了接收器在线导出工作线程异常退出时遗留已连接观察客户端socket的问题：失败线程现在标记失败并关闭/清空其唯一拥有的连接。Linux故障注入让下一次`accept`返回EIO，验证`export_failed`、客户端/断开计数与远端EOF；普通及ASan/UBSan/LeakSanitizer严格C++11下receiver-core和exporter-lock均2/2通过。修复曾进入独立Ubuntu24候选包：重新构建接收器、其余24个程序复用，经Ubuntu24 loader依赖检查；候选接收器包内TCP集成回归通过，最大合成raw 32 MiB并自动清理。该重复候选后来由70.76续传候选替代并删除；接收器和包内验证回执仍由当前候选复用保留。包校验器回归现为9项通过。结果见[验证记录70.74](VALIDATION.md#7074-在线导出工作线程故障清理2026-10-20)。该验证仍使用Linux本机内核，不代表独立Ubuntu24机器、macOS、现场硬件或持续吞吐验收。

70.75对当时的Ubuntu24交付候选接收器执行双流各512MiB的本机TCP/落盘/封存复核，发送端与全部文件SHA、计数、EOF及封存一致，1GiB合成载荷已按收据删除。候选包、启动器、脚本、结果和清理回执由独立摘要绑定记录。运行在Ubuntu24用户空间库+WSL2 Linux内核，低于物理内存容量，不构成真实DAQ速率、独立Ubuntu24或硬件验收；见[验证记录70.75](VALIDATION.md#7075-ubuntu24接收器交付候选双流负载复核2026-10-20)。该测试包快照后被70.76候选替代并删除，负载证据仍保留在`build-exporter-worker-failure/load/`。另清除10月3日旧qmake构建目录和一个被续传包替代的Ubuntu24快照，合计约90MiB；保留源码、第三方源码、文档和当前Linux/Windows候选。Ubuntu24 rootfs约1.3GiB仍保留用于目标用户空间验证。旧`build-*`路径中的历史临时构建和报告不再保证存在。

70.76把[跨主机SSH归档流](CATALOG_TRANSFER.md#12-跨主机发送已完成归档)补为显式断点续传：源端身份检查点、接收端partial前缀SHA预检、只追加缺失后缀，以及发布中断/发送端丢最终回执后的核对均有本机假SSH测试。宿主严格C++11普通CTest和ASan/UBSan各1/1通过；本环境LeakSanitizer在子进程运行中以“does not work under ptrace”终止，不能记为泄漏检查通过。最初新增打包器回归6项通过，70.77又加入候选根与回执路径符号链接防护回归。Ubuntu24候选`build-packages/getdaq-ubuntu24-20261020-ssh-resume-final-candidate`已重建`getdaq-catalog`、复用其余24个程序；25程序经Ubuntu24 rootfs动态加载器核对，包内候选二进制作为假SSH接收端的续传回归1/1通过。同一传输测试程序及候选CLI又在Ubuntu24 loader/库下运行，12个原生传输场景通过；均为WSL宿主内核和本地假SSH，不是两机验收。候选约87MiB，测试raw已由夹具清理。真实SSH双机、macOS、目标文件系统及吞吐测试仍待完成，见[验证记录70.76](VALIDATION.md#7076-跨主机ssh归档流显式续传与发布恢复2026-10-20)。

70.77按用户的空间要求删除被70.76续传包取代的重复Ubuntu24候选和10月3日旧qmake `build/`，合计约90MiB；Ubuntu24 rootfs和当前交付包保留。还加固候选包验证器对候选根、基础包树和回执路径符号链接的拒绝逻辑，新增回归测试并确认外部回执文件不会被删除。打包器测试9/9、Ubuntu24当前候选完整核验和1174个本地Markdown链接检查通过。证据、清理范围与限制见[验证记录70.77](VALIDATION.md#7077-旧生成物清理与交付打包器路径防护2026-10-20)。

70.78新增[Ubuntu24/macOS原生数据面CI](../.github/workflows/native-data-path.yml)：固定跑独立接收器、目录/传输与协议回归，并核对CTest实际发现的用例数。Linux侧按同一命令构建后receiver 2/2、receiver TCP/写盘/封存/慢观察端场景及监督器3/3通过；一次端口碰撞促使测试修正为预留多个不同端口并只对明确`EADDRINUSE`重试。CI的macOS job 尚未在本次工作树运行，不能称Mac已验收；真实目标卷、Qt打包/签名、硬件、多机和吞吐仍待。见[验证记录70.78](VALIDATION.md#7078-ubuntu-24macos原生数据面自动化与本机命令验证2026-10-20)。

70.79修正新ECC控制配置仍默认向旧DataExporter建立第二条板端数据连接的问题：未显式设置时默认`exporter_port=0`，只保留指向统一接收器的router DataLink；明确设置非零端口的旧YAML仍保留第二目标。Qt5.12.8本机完整控制台测试98/98通过，专项的默认/显式目标与YAML往返验证5/5通过；未连接真实板端，也未在Windows Qt5.14.2构建。新接收器`--export-port`是分析端主动连接的下游出口，与旧第二目标方向不同。详细范围见[验证记录70.79](VALIDATION.md#7079-新建ecc数据链路默认单入口2026-10-20)。

## 1. 当前有哪些可使用的部分

本轮在研增加[已受理后失败的节点计划退休补齐](FAILED_PLAN_RETIREMENT_COMPLETION.md)：`complete-plan-retirement`只处理同一活节点的准确原请求，父ECC只读核对操作者已完成的补齐，不自动替节点修复。宿主普通/插桩节点各七场景及父库各两编码通过，Ubuntu24节点普通/插桩和旧计划回归通过；Ubuntu24实际ECC HTTP `plan-transition-failed-node-0/1`在普通版和ASan/UBSan/LeakSanitizer版各两编码通过。新增的“accepted 后失败→操作者显式节点修复→生产 Qt 父级确认/取消与回执导出”组合，在Ubuntu24用户空间普通和ASan/UBSan/LeakSanitizer构建的两个编码变体均通过；结果及边界见[验证记录70.72](VALIDATION.md#7072-已受理后失败的节点计划退休补齐2026-10-19-在研)。随后当前HTTP诊断版本完成普通/插桩服务、查询CLI及ECC父链统一构建、父工作流和完整监督控制矩阵回归；18个服务场景、8个原生HTTP故障、codec和凭据测试均通过。旧 `plan-transition-complete-1` 失败日志证明父操作 `handoff-data` 已被受理、随后因节点HTTP 409进入失败；测试代码下一步要求先收到POST应答，再轮询该父操作终态，但旧现场没有终态轮询后的首个结果文件，不能证明是否进入该步骤。保留的独立插桩计时复跑观察到POST返回202及 `GET /v1/operations/handoff-data` 成功；它是通过运行，不能确定旧超时发生在POST应答还是终态GET。当前HTTP客户端已在截止错误中给出方法和路径；两种当前构建复跑通过，未复现旧超时，根因仍未证实，1500ms截止及旧失败记录保留。2026-10-20已生成新的Ubuntu24候选包；六个新程序和19个逐字节复用程序均已检查，包内两个失败节点HTTP场景通过，详见[验证记录70.72](VALIDATION.md#7072-已受理后失败的节点计划退休补齐2026-10-19-在研)。候选不是现场验收或整套完成证明。合成场景不表示任意已受理故障可恢复。

2026-10-20当前源码的Ubuntu24全量候选构建与验证已在70.82完成；Windows Qt5.14.2 MinGW当前源码的完整本机QtTest在70.83完成。70.84修正当前候选包文档入口并刷新两个包的文档清单，确认程序和运行依赖字节未变化。70.85对当前Ubuntu24候选接收器执行两流合计1GiB localhost合成写盘/封存回归，逐文件SHA、EOF和封存均匹配，载荷已删除；该规模低于内存且在WSL内核上运行，不能证明持续DAQ/磁盘吞吐。70.86覆盖两种旧ECC SOAP请求参数形式、inline/default命名空间差异及带命名空间响应/故障的本地解析回归，仍没有真实现场gSOAP服务抓包或进程证据。70.83只证明用户这台Windows环境上的当前界面/HTTP客户端测试，不代表独立干净Windows部署、人工完整操作、现场Linux服务、真板或硬件恢复。较早Ubuntu20.04/Qt5.12.8构建见相应历史验证记录；整体重构仍未完成。

2026-10-21已把Sentry严格配置类型/整数解析修复放入Ubuntu24候选：八种无效JSON配置在Ubuntu24用户空间loader下以配置错误退出，刷新程序及完整候选清单校验通过，具体SHA、构建平台和范围见[验证记录71.45](VALIDATION.md#7145-sentry严格配置检查进入ubuntu24候选2026-10-21)。本机Windows构建尝试被WSL互操作层的`UtilBindVsockAnyPort: socket failed 1`阻断，Windows候选仍是旧程序；这不是Windows代码构建失败证据。整体重构仍未完成。

随后对当前27程序Ubuntu24候选内的确切`getdaq-catalog`执行无认证普通TCP逐进程回归，八项传输/续传/读回场景通过，生成并清理15,245,312字节raw；runner创建第二临时根失败时的首根清理也经过故障注入检查。回归与本机/跨主机边界见[验证记录71.46](VALIDATION.md#7146-当前ubuntu24-catalog候选普通tcp逐进程回归2026-10-21)。这只证明当前用户空间候选和同机loopback，不是两台独立主机验收；整体重构仍未完成。

| 职责 | 实现与入口 | 已有依据及实际边界 |
| --- | --- | --- |
| 操作台、Run与监控 | `apps/envoy`，MainWindow/RunControl；[操作手册](OPERATIONS.md) | MUSIC形式h/cpp/ui、工作线程、SOAP兼容客户端/模拟/native模式；Run意图、编号、日志、备份及结果。旧SOAP模式支持按选中节点Progress/Regress，并遵守CoBo/MuTaNT全局顺序约束；独立mock节点回归见[71.321](VALIDATION.md#71321-旧soap模式逐节点progressregress回归2026-10-23)。Native模式的单CoBo快捷入口仍是整套ECC FSM。原始数据不经过窗口 |
| 新ECC整系统控制 | NativeEccCoordinator、`getdaq-ecc-server`；[网络服务](ECC_NETWORK_SERVICE.md) | Describe/Prepare/Configure/Start/Stop/Pause/Resume/Undo/Breakup/Reset及数据生命周期已连接；固定输入、操作ID、认证HTTP和同原对象有条件恢复。Envoy仅在服务端固定目标恰为唯一CoBo、匹配唯一数据源且无MuTaNT时开放“单CoBo系统”选中行Progress/Regress快捷入口，实际仍驱动整套ECC FSM；多板模式保留Prepared-only单板维护。不是旧Ice/SOAP服务的线协议替身 |
| 配置模型与编辑 | `common/config`、ConfigWindow；[配置编辑器](CONFIGURATION_EDITOR.md) | include、默认对象、共享属性、类型/单位/范围、实例覆盖、来源及有效快照；保留未知源文本。旧CompoundConfig有实际运行对照，不代表全部数据库/网络扩展均实现 |
| 结构编辑与多根工作区 | XcfgStructureEditor/XcfgWorkspace；[结构](CONFIGURATION_STRUCTURE_EDITOR.md)、[多文档](CONFIGURATION_MULTI_DOCUMENT.md)、[路径迁移](CONFIGURATION_PATH_MIGRATION.md) | 声明编辑、覆盖、物化、跨文件声明移动、整组历史及Save All恢复已接；新版合作工具共享读/独占写。70.68增加实际来源路径迁移、全部已知include改写与新旧路径明确恢复；不扫描未打开引用者，不提供任意外部读者的跨文件原子可见性 |
| 配置发布与仓库 | 本地发布包、ConfigurationRepository、上传/检查CLI及窗口；[仓库](CONFIGURATION_REPOSITORY.md) | 不可变工件、分块上传、原计划续传、登记与原版本观察；不经GUI传raw，不等于参数已写入板端 |
| ECC配置版本选择 | `getdaq-ecc-select`、固定服务绑定、只读配置页及Run记录；[选择](ECC_CONFIGURATION_SELECTION.md)、[查看](ECC_CONFIGURATION_VIEW.md) | 已选工件绑定实际启动与Run；正常收尾后的不同计划更替见下行。未提供运行中参数热修改 |
| 新接收计划切换 | 原生retire-submit/retire-inspect，操作台审阅/提交/导出；[逐步窗口说明](ECC_PLAN_TRANSITION_GUI.md)、[后端](ECC_PLAN_TRANSITION.md) | 70.54数据层、70.55父ECC及70.56实际窗口已接；不同工件/接收参数形成新周期。全节点/端点策略保持；原实例受理即永久保持，服务更替明确执行 |
| 已失败节点交接补齐 | `Failed data-node handoff completion...`；[操作说明](FAILED_CONTROL_HANDOFF_COMPLETION.md) | 仅操作原实例中明确failed且正常停止/封存的准确请求；生成只读校验、人工确认后单次提交、保存请求重载只读观察。Ubuntu24当前源码窗口组合回归见71.126及该功能专项记录；Windows helper仍以71.94的401拒绝/一次接受POST本机loopback测试为据，不覆盖Windows服务端状态机或完整GUI交接 |
| 硬件描述与寄存器访问 | HardwareCompiler/HardwareClient、`getdaq-hardware`；[硬件后端](HARDWARE_BACKEND.md) | 枚举/描述/精确64位读写、权限/位宽和日志，空节点创建与明确描述替换；native ECC已调用该层，不再仅有独立CLI |
| AGET/ASTRE、CoBo/AsAd参数 | `common/experiment`、各原生子流程；[系统工件](SYSTEM_EXPERIMENT_CONFIGURATION.md) | 参数计算/JSON工件/绑定/Prepare与Configure部署已贯通；71.102以当前Linux候选工件联调模拟硬件、Ice 1.0/1.1均通过。时钟、监控、校准、DAC/脉冲、零抑制/缓冲等复用标准C++11与本地GET接口；实板真实加载与电气效果待验 |
| MuTaNT、对齐及系统准备 | SystemAlignmentClient/SystemPreparationClient；[对齐](SYSTEM_ALIGNMENT.md)、[系统准备](SYSTEM_PREPARATION.md) | 主从/独立模式、定位/机箱顺序、时钟/跨板告警及状态提交已接ECC。选定单板维护窗口已接入原生ECC并完成本机普通组合验证；物理对齐效果仍待实机检查，不能用完整系统Prepare冒充 |
| 采集与发送器 | SystemAcquisitionClient/DataSenderClient；[系统采集](SYSTEM_ACQUISITION.md)、[发送器](DATA_SENDER_CONTROL.md) | 普通TCP及可选受控ICE源、连接/启停/暂停恢复、先停生产后刷新和明确断开已组合；Stop应答不等于EOF/封存 |
| 正常收尾与失败恢复 | Breakup/Unprepare、持久故障保持、窗口确认；[收尾](SYSTEM_RELEASE.md)、[恢复](ACQUISITION_RECONCILIATION.md) | 正常Stop/Breakup/Reset、receiver封存及保留原对象的特定故障收尾/明确恢复已接；非任意故障自动恢复，冷实例不能接管原对象 |
| 原始数据接收/存储/在线导出 | `apps/datareceiver`，`getdaq-receiver`；[数据服务](DATA_SERVICE.md)、[性能](RECEIVER_PERFORMANCE.md) | 原生C++11/POSIX、有界缓冲/分帧/轮转、导出慢客户端隔离；70.70移除导出socket操作对发布锁的占用；71.50将SIGPIPE保护限于各socket，Linux/macOS分别使用MSG_NOSIGNAL/SO_NOSIGPIPE。普通TCP、ZBUF、FDT及独立ICE入口有本机验证；显式ByteStorage兼容模式原样保存TCP/ZBUF字节，FDT仍走帧解析。无Qt/yaml/Ice依赖的默认热路径保持 |
| 接收器进程管理 | DataSupervisor/本地与远程适配器；[进程管理](RECEIVER_PROCESS_MANAGEMENT.md) | 固定计划、身份/PID/会话、就绪、EOF/排空/退出/封存核对与认证控制服务已接。Qt低频管理服务不会代理载荷 |
| 原始文件诊断/离线整理 | `getdaq-inspect`、`getdaq-merge`；[输入边界](SOURCE_MFM_BOUNDARIES.md)、[MFM合并](DATA_MERGE.md) | 结构检查及CoBo/RIBF按事件合并已加入原生数据工具；`getdaq-inspect --hit-pattern`按目标ID流式汇总type-1帧头68个物理通道的命中位（仅头字段计数，不判定阈值响应）；`.graw/.gmat`单通道查看、旧版离散FWHM/peaking-time分析及选中波形PNG/CSV导出、ROOT `GDataFrame`对象树转换与事件查看已有可选工具；旧ROOT Canvas精确绘图风格、真实下游差分及物理结果仍待；已实现的Qt离线曲面不复制旧Qwt外观，历史拟合代码为注释；受限FITS导出另见[getdaq-fits](DATA_FITS.md) |
| 数据归档与副本 | ReceiverCatalog/`getdaq-catalog`、元数据任务窗口；[归档](DATA_RUN_CATALOG.md)、[传输任务](CATALOG_TRANSFER.md) | 原目录封存、同盘硬链接、独立进程跨文件系统复制/逐文件校验、明确本机复制与跨主机普通TCP显式续传（SSH仅保留兼容入口）。71.204新增封存后扫描点/Run独立绑定凭据，先复验原catalog、再逐摘要匹配，保持归档只读；不代表实际硬件配置读回。当前Ubuntu24 full候选以71.183验证、候选目录版本标为71.182：28程序来自同一次全量构建，180项清单及包内启动检查通过；具体套件回归以71.183为准。71.125以512MiB单文件、源和目标raw合计约2GiB完成隔离发送/读回及哈希核对，测试后raw为0。另有隔离网络命名空间/veth传输验证，但这些都不是两台物理主机/独立Ubuntu验收。TCP无身份认证、无加密，SHA-256仅校验内容。真实两机、可信网络/防火墙、macOS目标卷独占发布及持续DAQ吞吐仍未验收 |
| 节点监控 | `apps/sentry`、SentryClient | 进程/磁盘/文件/速率与归档接口；缓存过期和计数重置处理。不是板端硬件健康证明 |
| 只读硬件/配置证据 | live control bits、Configure读回、同机占用窗口 | [实时位](LIVE_HARDWARE_STATE.md)、[读回](CONFIGURE_READBACK_EVIDENCE.md)、[占用](HOST_CONTROL_OWNERSHIP.md)；绑定实例/快照、过期撤回。读回证据不等于当前完整加载验证；同机协作锁不是全局板端锁 |
| 专用依赖与构建 | 本地Ice/bzip2/mcpp/yaml源码、固定GET接口；[离线依赖](DEPENDENCIES_AND_OFFLINE_BUILD.md) | 构建不联网下载。新增编译前源码清单/摘要/许可存在性检查；Qt、编译器及OS工具链需另备，板端移植库不作为主机构建前提 |
| Linux、Ubuntu24与Windows交付 | [整套构建](HOST_SUITE_BUILD.md)、[Ubuntu24](UBUNTU24_VALIDATION.md)、[Windows](WINDOWS_QT5142.md) | Linux host最新完整候选仍为71.219的28程序full构建（之后71.220补充该候选内归档TCP回归）；Ubuntu24最新完整候选71.223从当前产品源码构建28程序，109项嵌入依赖检查通过，Core151/151、Widgets116/116、CTest23/23和23条平台runner命令通过，四路合成接收1GiB、失败handoff 8KiB均清理至raw=0。71.224仅增加Qt取消测试并将Ubuntu24/macOS Widgets门槛更新为117；Linux Qt5.12.8当前完整Widgets 117/117、专项20/20。Windows最近完整用户主机候选仍为71.188的8个control程序及其Widgets113通过/1项POSIX跳过、ECC64/64证据；本轮Windows当前源码构建尝试在启动qmake前被WSL `UtilBindVsockAnyPort`错误阻断，没有编译结论。Ubuntu24/Linux用户空间共享WSL2内核，Windows候选不等于独立干净安装；macOS原生、双机、现场旧服务/板端及持续DAQ吞吐仍未验收 |

## 2. 故障处理已经完成哪些边界

原操作结果未知时禁止盲目重发。普通命令成功也不能消除独立故障保持。下面已有同一原对象的明确操作：

- 失败Describe核对：[说明](DESCRIPTION_RECONCILIATION.md)。
- 失败Prepare停生产、断电收尾、告警确认及只读恢复：[说明](PREPARATION_RECONCILIATION.md)。
- 失败采集停生产、连接关闭、原receiver终止/退休、断电及核对：[说明](ACQUISITION_RECONCILIATION.md)。
- 普通已知Stop后的告警确认和正常收尾：[说明](ALARM_STOP_POLICY.md)。
- 可选ICE源故障后的原连接退休及原对象核对：[说明](FAILED_ICE_SOURCE_RECOVERY.md)。

这些入口有准确前提。原Run仍保留中断历史，恢复后需要明确发起新Prepare；没有“无条件清故障”按钮。同一原实例的部分交接/计划退休已有[HTTP/CLI及计划退休Qt显式补齐](PARTIAL_HANDOFF_COMPLETION.md)，原失败记录和旧控制禁用均保留。原ECC已退出时的数据冷交接已有独立CLI及恢复服务/窗口；本轮新增的原活节点失败退休补齐按上面的独立范围验收，不等于支持全部失败子请求或任意板端故障。

## 3. 当前还缺什么

| 剩余事项 | 所需工作或证据 |
| --- | --- |
| Native控制编排与逐CoBo状态机 | 71.439已按工作流拆开客户端操作结果、扫描未知结果和Run/扫描收尾，但NativeRunControl仍拥有共享会话、计时器和异步状态。71.440已加入准备/采集共用的逐板持久状态日志与日志故障时的停止边界；重启后历史仍仅可读，旧单板失败恢复、持久FSM与ECC恢复证据的完整对齐仍待。已有逐板journal投影和Prepared内维护能力不能充当完整迁移证明。后续需按真实工作流闭环验证，不能仅用拆分文件或增加测试数量判断完成 |
| 单板维护现场验证 | [GUI维护入口](COBO_MAINTENANCE.md)现提供受限单板重准备、LED set/blink/flip/任意周期pulse及Prepared/数据停止条件下单次AsAd pulser诊断。71.166/71.167/71.171的协议、模拟网络、Windows当前源码构建及Qt回归通过，71.172 Ubuntu24当前源码完整候选已包含现行28个程序。硬件应答不证明物理脉冲/灯态；实板波形、不同CoBo型号、现场安全互锁及独立运行包人工验收仍待 |
| 部分/冷实例恢复 | 原实例部分交接、计划退休Qt补齐、冷数据CLI/独立恢复服务，以及本轮同计划控制交接在节点明确`failed`后的原生请求生成、操作者单次补齐和父端只读核验均有严格限定与合成测试。POST应答不确定、`unknown`、节点冷重启后的新补齐、通用交接GUI及任意硬件冷恢复仍不支持；不能删目录或换ID盲目重放 |
| 更广配置迁移与编辑 | 任意外部读者可见的跨文件持久原子事务、未加载工程/非include路径资源及现场实际使用的数据库/网络资源仍需独立处理；已知本地来源的路径迁移、合作互斥和明确恢复已接。旧DB按钮空函数不能当成已实现功能要求，需结合现场用法 |
| GetController 自动扫参 | 操作台 **Scan plan and Run control** 以异步 `getdaq-config` 生成PeakingTime、Pulser、单芯片Threshold及AutoGain离线计划；数据面仍由原生接收器处理。Envoy已提供逐点身份复核、Describe/Prepare/Configure、事件基线、单点Run事件数Stop，并对成功点执行Breakup、receiver finish/seal、catalog、Run记录和计划绑定。fake ECC完整单点成功链见[71.322](VALIDATION.md#71322-单扫描点run至归档绑定的fake-ecc集成回归2026-10-23)。用户要求每点显式事件数；Threshold保持手动，不生成自动Run策略。当前流程不把点工件装载进ECC、不自动切换部署或启动下一点；每点配置装载和跨点失败/崩溃恢复尚未闭环，fake服务结果也不代替现场硬件/receiver验证。journal未知操作可由操作员引用原operation ID并记录声明/观察摘要人工对账，但不验证证据来源。PeakingTime、Pulser、AutoGain、Threshold离线分析及封存Run/catalog关联各有独立CLI；分析边界包括未迁移的旧物理Gain公式/判定、Threshold物理解释和现场`hittedChannel`语义，现有波形分析不证明寄存器读回。Windows Qt5.14.2、macOS、独立Ubuntu24及真实扫描/硬件分析未在当前源码上验收。详见[扫描计划GUI](SCAN_PLAN_GUI.md)、[AutoGain计划](AUTOGAIN_SCAN_PLAN.md)、[源码审阅](SOURCE_REVIEW_AND_PLAN.md)及[扫描绑定](SCAN_RUN_BINDING.md) |
| 连续占用与跨主机控制 | 当前同机协作锁在服务生存期有效；不中断的控制权移交和真实跨主机/板端排他尚未实现 |
| 数据后处理与分布式迁移 | 无认证普通TCP归档发送及显式续传已有本机loopback回归，仍需两台实际主机、防火墙/网卡、目标文件系统及持续吞吐验收；SSH仅为兼容入口，若选用才需要密钥/主机密钥。attpc_conduit的8字节小端长度头已按其公开Rust接收源码只读核对；其MFM首字节过滤仅接受0x08，意味着实际下游解析限于匹配的GET Rev5大端帧。当前候选出口已回归大/小端Rev5逐字节传输，但没有运行Conduit本体。默认11个CoBo地址映射、真实连接/分析与现场吞吐尚未联调。Narval实际输入/导出用法仍需现场确认。ROOT转换、GDataFrame事件浏览及旧root2disp五个统计字段已有独立可选`getdaq-root`/`getdaq-root-viewer`；当前波形viewer新增旧版离散FWHM/peaking-time分析，完整通道绘图/交互和ROOT Canvas精确样式仍未全部迁移；QwtPlot3D主要曲面语义已有Qt离线实现但外观不同，旧ROOT拟合代码为注释且不是已启用功能。受限FITS导出由独立可选`getdaq-fits`提供（CFITSIO非默认依赖） |
| 目标平台 | Linux host最近整套构建记录为71.371：Ubuntu20/WSL2、GCC9.4、Qt5.12.8下full profile 29/29、109项依赖核验及29程序smoke通过。之后71.439控制器版本完整Widgets150/150、Native控制集成38/38通过；连接徽标调整后GUI定向5/5和当前生产Envoy构建通过，但这些结果不等于已刷新29程序全套交付包。Ubuntu24最新完整候选仍为71.223旧源码包。Windows最近71.370源码验证为Qt5.14.2/MinGW7.3 Widgets133通过、1项Unix专属跳过及ECC客户端69/69；完整运行候选71.274不含后续全部变更。macOS没有当前源码native job通过证据。独立Ubuntu24、Windows全量当前包人工验收、macOS、真实双机/板卡及持续DAQ吞吐仍待 |
| 实际硬件 | 现场getHwServer版本/固件、描述和真实配置，读写/错误/超时、时钟/供电/对齐/真实加载与多板启停 |
| 持续高速采集 | Linux host 71.189当前候选通过四路各4GiB、合计16GiB合成接收，含1GiB轮转、整流/逐文件SHA读回、EOF、退出和seal；17,179,869,184字节raw在校验后全部删除。历史Ubuntu24四路18GiB结果仍是更早候选，且负载低于主机内存。上述单机WSL2/localhost合成结果验证有限时长路径，不证明真实磁盘持续吞吐、实验室目标速率、CPU/内存余量、背压或丢帧。仍须在采集主机和目标盘做长时间、多源实测；合成raw遵守不超过20GiB并在每轮结束后删除；完整新证据见[71.190](VALIDATION.md#71190-linux-host-候选四路16gib合成接收回归2026-10-22) |

仍需保留旧客户端时，应明确它们使用旧Ice/SOAP服务；新版固定HTTP不是相同URL/端口上的透明替代。两套控制入口不能同时操作同一板卡。详细源码迁移关系见[原方案审阅](SOURCE_REVIEW_AND_PLAN.md)，历史过程见[专题索引](FEATURE_HISTORY.md)。

## 4. 怎样理解现有测试和检查点

70.56检查点记录客户端55/界面91项、23离线身份场景、普通/新控制插桩/包内三个完整窗口周期；Linux24程序/Windows六程序。阶段源码和文档后来改变时，该完整映射保留为历史，不改旧包或旧摘要。70.57依赖检查与文档整理的证据单独存放在 `build-dependency-delivery/`；本轮C++源码变化后整组映射同样保留为历史。原生维护的控制测试/插桩构建与元数据证据在 `build-cobo-maintenance/`，该70.58阶段不代表HTTP/GUI和真实receiver链路。后续HTTP/GUI与真实receiver证据在 `build-cobo-maintenance-window/`，见VALIDATION70.59。

[VALIDATION.md](VALIDATION.md)保留各阶段具体条件和结果，不把阶段数量、合成场景或单测数量换算成总体完成率。纯计算通过、编译成功、回环联调、目标平台验收、真机运行是不同证据。

70.80操作台节点配置表将旧ECC第二板端目标标注为“Legacy ECC target”，并解释它与接收器下游`--export-port`的连接方向区别，避免误把板端第二流当作分析导出端口。详见[VALIDATION.md](VALIDATION.md)。

该Qt改动在Qt5.12.8/GCC9.3、C++11下完整测试程序构建成功；协议/配置专项5/5通过。临时20MiB构建目录已删除。Windows Qt5.14.2、macOS与硬件尚未实测。

## 70.81 接收器最低可用磁盘保护（2026-10-20）

标准TCP/ZBUF/FDT及ICE接收器支持可选`--min-free-mib`，固定托管计划通过`min_free_mib`传递并纳入计划身份。托管Start在写启动意图、创建运行目录或启动receiver之前检查输出卷；receiver启动和写线程另行复查。可用空间不足或运行时无法检查时，失败Run保留未封存目录且不声称完整；默认0保持旧CLI/计划行为。细节及限制见[VALIDATION70.81](VALIDATION.md#7081-接收器最低可用磁盘保护2026-10-20)和[数据服务说明](DATA_SERVICE.md#6-内存背压和磁盘)。

验证：GCC9.4/C++11 receiver核心通过，包含真实statvfs预算下的大帧预测越线测试；16个本机托管进程场景、supervisor codec、托管服务正常/8个控制传输场景和ICE 14个入口场景通过。随后Ubuntu24.04.5用户空间/GCC13.3对当前receiver、Qt启用的管理服务及vendored ICE receiver重新构建，receiver core、16个进程生命周期、管理服务正常/8个HTTP控制故障和14个ICE入流场景通过。两轮合计73个`.graw`、2,113,551字节，远低于全流20GiB限制，构建与测试目录已删除；Ubuntu24小型SHA绑定回执留在`build-ubuntu24-validation/rootfs/build/getdaq-disk-reserve-u24-result.json`。Ubuntu24共享WSL内核；没有独立Ubuntu机器、实机、长期运行或吞吐结论。后续全量交付候选见70.82；Windows/macOS未在本阶段重跑。

## 70.82 当前源码Ubuntu24全量交付候选（2026-10-20）

从当前源码在已验签Ubuntu24.04.5用户空间以`--profile full`重新构建全部25个主机程序，而非从先前候选复用。候选`build-packages/getdaq-ubuntu24-20261020-current-source-candidate/`包含中文文档、示例配置和许可证，并附25程序完整构建回执、459项源/构建输入摘要与验证汇总。25/25 Ubuntu24动态加载器依赖检查通过；receiver实际TCP/落盘/封存集成、catalog 12项跨文件系统/续传回归、supervisor磁盘预检通过。测试最大单个raw 32MiB，夹具结束后工作区和临时目录无`.graw`。初次catalog跨文件系统测试发现runner未提供`/dev/shm`；现runner在私有命名空间挂256MiB tmpfs后重跑成功。

候选包和验证限制详见[验证记录70.82](VALIDATION.md#7082-当前源码ubuntu24全量交付候选2026-10-20)。该环境运行在WSL2 Linux本机内核，不能替代独立Ubuntu机器；macOS、Windows全功能、真实板端/双机SSH和持续DAQ吞吐仍待，整套重构尚未完成。

## 70.83 当前源码Windows Qt功能回归（2026-10-20）

使用用户的Windows Qt5.14.2/MinGW7.3 x64，对当前工作树严格C++11构建并在原生Windows进程运行完整界面与ECC HTTP客户端QtTest：界面97通过、0失败、原有POSIX锁用例1项跳过；客户端63通过、0失败；实际进程退出码均为0。运行器修正了已有WSL `Z:`映射不应覆盖、Makefile `DEFINES`内嵌`QT_TESTCASE_BUILDDIR`解析、GUIless客户端不能传`-style`及PowerShell `Start-Process`退出码不可用等问题；现使用真实`.NET Process.ExitCode`、QtTest文件日志和独立Windows临时目录。自有`.graw`为0、总量0字节，Windows临时目录清理回执确认已删除；104MiB左右测试中间目录已删除。构建/结果摘要及可复现命令见[验证记录70.83](VALIDATION.md#7083-当前源码windows-qt功能回归2026-10-20)与[Windows功能回归说明](WINDOWS_FUNCTIONAL_VALIDATION.md)。候选包产品完整性核对仍为6程序、264文件。

这不是独立Windows机器、人工完整操作、远程服务/真板互通、macOS、硬件恢复或持续DAQ吞吐验收；这些及跨主机归档网络/现场数据生命周期仍属于整套重构的剩余验收范围。

## 70.84–70.86 后续验证与当前边界（2026-10-20）

70.84同步了当前Ubuntu24与Windows候选包内的操作手册，并确认只改文档时包内程序、Qt运行库和插件摘要不变；文档链接检查1190项通过。70.85用当前Ubuntu24候选接收器完成两路各512MiB、合计1GiB的localhost合成数据接收/落盘/封存，逐文件与发送端摘要、帧数、EOF和退出状态一致；自有`.graw`已按清理回执删除。70.86为SOAP字符串/内嵌XML参数和命名空间化响应包装补充了本地Qt回归，严格C++11完整测试程序构建成功，定向6/6通过。具体输入、命令、摘要及限制见[验证记录70.84–70.86](VALIDATION.md#7084-当前源码候选包文档同步与清单复核2026-10-20)。

这些回归没有闭合剩余的现场工作：实际旧gSOAP服务抓包/进程互通、独立Ubuntu24和macOS运行、两台独立主机上的普通TCP/IP归档互通、真实板卡配置与恢复、完整人工GUI流程及目标负载持续吞吐仍需相应环境证据。普通TCP传输不做应用层认证或加密，SHA-256只用于内容校验。SSH仅保留为显式兼容入口，不是用户要求的必需路径。2026-10-20另清除了未被文档引用的`bin/getdaq-config.exe`调试构建和`tools/__pycache__`缓存；Ubuntu24验证rootfs仍在，因目录内容属于当前环境无法映射的`nobody` UID，清理被文件系统权限拒绝，未使用绕过权限的方式。
## 71.390 ECC控制MCP增加单CoBo Reprepare工具

在MCP允许控制时新增`ecc_reprepare_cobo`，用于系统保持健康Prepared时按原不可变计划重做单个固定活动CoBo的Prepare。工具要求新的operation ID和精确确认字符串；MCP先核验固定ECC身份/快照、唯一固定目标及其Prepared/Configured生命周期证据、父系统Prepared状态和无活动操作，再向ECC服务提交既有`reprepare-cobo`命令。只读模式不公布工具，服务端状态机仍是最终门控。无需改变硬件板端协议。

具体语义、限制与使用条件见[ECC控制MCP手册](ECC_CONTROL_MCP.md)。

## 71.391 MCP单CoBo Reprepare预检失败关闭回归

扩展`ecc_reprepare_cobo`的MCP假服务集成回归：分别注入目标CoBo生命周期`unknown`、父ECC非Prepared和父ECC已有在途操作，确认三种情况下均返回工具错误、不发送控制POST且操作列表不变。此前成功转发、错误确认拒绝和只读模式隐藏/拒绝仍保留。

独立CMake MCP目标重新构建，stdio集成CTest 1/1通过；完整证据见[验证71.391](VALIDATION.md#71391-mcp单cobo-reprepare预检负例回归2026-10-24)。

## 71.392 MCP公开只读逐CoBo生命周期投影

新增`ecc_cobo_lifecycles`只读工具，直接公开ECC固定活动CoBo维护目标和由生命周期/journal投影的最近操作证据，即使控制功能关闭也可查询。描述明确标注状态不是硬件读回，服务重启/历史不确定时可能为UNKNOWN；真实位读取仍由单独的`ecc_hardware_state`工具处理，二者不混淆。

构建、stdio回归和限制见[验证71.392](VALIDATION.md#71392-mcp只读逐cobo生命周期投影回归2026-10-24)。

## 71.393 MCP逐CoBo只读投影复用严格身份/schema校验

将`ecc_cobo_lifecycles`和受控`ecc_reprepare_cobo`共用同一逐CoBo响应校验：先绑定固定ECC client/instance/snapshot，再检查目标配置摘要、schema、活动板数量与唯一ID、生命周期/操作阶段枚举和逐板操作证据字段。只读工具不再把原始HTTP响应未经校验地交给AI。

MCP C++11构建、stdio集成回归及文档链接检查见[验证71.393](VALIDATION.md#71393-mcp逐cobo只读投影严格绑定回归2026-10-24)。

## 71.448 由冻结快照生成逐点固定部署包（2026-10-25）

新增离线扫描部署包builder和POSIX native `getdaq-scan-deploy`入口。它逐点核对快照引用的artifact，要求每个固定agent提供摘要匹配的canonical receiver plan，且receiver source ID集合与ECC sender集合相同；然后为每一点生成只读artifact、完整ECC固定配置和逐agent receiver plan，绑定独立的deployment ID、ECC/receiver审计root及configuration/artifact/plan摘要。manifest记录本地路径、目标数据主机plan路径和所需审计root父目录。带configuration publication/selection的配置明确拒绝，因每点publication/effective snapshot尚未生成。实际CLI通过两点合成输入生成bundle，并拒绝覆盖既有输出目录。

Linux GCC/C++11重建`getdaq-ecc-native`及`fixed-ecc-configuration-tests`成功；两点固定输入集成回归以localhost Ice mock退出码0通过。验证生成ECC配置可重新解析、点artifact及摘要完全吻合、接收端plan摘要/来源绑定正确且各点ECC绑定唯一。无实板、远端data host、receiver进程或DAQ raw。

这是离线材料生成；71.449另提供目标主机本地暂存命令，但扫描执行器尚不能自动、安全地传输并调用该命令。原receiver退休与successor回执、清洁关闭/退休旧ECC实例、释放主机控制占用或启动新实例仍未闭环；扫描journal仍禁止新的`Bound → Retired`和自动下一点推进。publication/selection扫描模板、服务进程监督、故障冷恢复、Windows/macOS构建及现场验收仍未完成，完整重构继续。详见[验证71.448](VALIDATION.md#71448-冻结快照生成逐点固定部署包2026-10-25)、[验证71.449](VALIDATION.md#71449-数据主机本地暂存扫描-successor-plan2026-10-25)和[扫描执行器架构](SCAN_EXECUTOR_ARCHITECTURE.md)。

## 71.482 RemoteReceiverManager恢复流程实现拆分（2026-10-25）

将`RemoteReceiverManager::Impl`及跨实现文件共用的私有校验/HTTP辅助定义集中到`remotereceivermanager_p.h`，并把完整handoff、cold handoff续接、catalog替换续接及控制权接管实现移至`remotereceivermanager_recovery.cpp`；普通远程receiver管理入口留在`remotereceivermanager.cpp`。保持既有持久化顺序、operation journal和状态/协议行为不变。CMake已注册新翻译单元；修正拆分后涉及的条件缩进告警。当前主`.cpp`为192行，恢复实现为352行；共享私有头仍有823行，后续可继续按职责拆分实现体。

Linux GCC/C++11重建Native ECC库及coordinator/remote receiver测试目标成功；`native-ecc-remote-receiver-normal-0`在获准localhost Ice/TCP回环后通过（86.42秒），`failed-handoff-parent-completion`通过。测试使用本机服务、receiver与合成数据，不访问实板或现场DAQ数据。详见[验证71.482](VALIDATION.md#71482-remotereceivermanager恢复流程实现拆分2026-10-25)。

## 71.483 RemoteReceiverManager操作与观察职责拆分（2026-10-25）

继续拆分私有实现：catalog替换续接进入`remotereceivermanager_catalog.cpp`；ReadOnly/Mutation进入`remotereceivermanager_mutation.cpp`；receiver生命周期轮询及已完成operation/handoff proof校验进入`remotereceivermanager_operations.cpp`；服务状态观察、predecessor handoff/plan周期验证与Refresh进入`remotereceivermanager_observation.cpp`。`remotereceivermanager_recovery.cpp`继续承载冷恢复和handoff控制权接管。上述成员函数的请求顺序、journal证据、状态门控和错误语义保持不变。私有实现头由823行降至252行；所有新翻译单元已加入CMake。详见[验证71.483](VALIDATION.md#71483-remotereceivermanager操作与观察职责拆分2026-10-25)。

## 71.492 SupervisorControl job and observation separation

Moved the shared `SupervisorControl::Impl` declaration into `controljobs_p.h`, journal replay/store checks and operation execution into `controljobs_impl.cpp`, and public identity/status/receiver/cycle/catalog observations into `controljobs_observation.cpp`. `controljobs.cpp` retains request encoding, construction and submission. The operation audit directory contains a separate `data` subdirectory for `DataSupervisor` audit files; the accepted request remains at the operation root. Mutating failures retain reconciliation gates, and only an explicitly completed live handoff can release the original owner gate.

Linux GCC/C++11 rebuilt the supervisor core, service, query, and related tests. `supervisor-control-service`, `failed-handoff-parent-completion`, `failed-retirement-parent-completion`, and `data-supervisor-process-lifecycle` passed. A prior extraction script mistook the default argument `next={}` for the function body; the `Run` worker was reconstructed and checked against these operation and recovery regressions. Tests use synthetic local receivers and permitted loopback services; no physical hardware or production DAQ data was used. Full refactoring continues.

## 71.493 Offline Data Tools close lifetime

`OfflineDataToolsWindow` now enables `WA_DeleteOnClose` in its own constructor so its accepted close path destroys the modeless top-level window even when constructed outside the main menu. During a running child process, the window accepts close, requests termination, and keeps the main-window Run/menu gates held until process exit and QObject destruction. Linux Qt 5.12.8 focused tests passed 13/13 across titlebar, Close, Esc, an unresponsive child, menu reopen, and close-pending gate behavior. This local offscreen test does not establish cross-platform window-manager behavior.

## 71.494 ScanPlanWindow policy implementation separation

Moved scan-plan review storage/catalog setup, executable discovery, runtime policy-control presentation, and immutable stopping-policy creation from `scanplanwindow.cpp` into `scanplanwindow_policy.cpp`. The main window implementation decreased to 683 lines and the policy implementation is 289 lines. Both the GetDAQ application and Qt test qmake projects include the new translation unit; UI and policy behavior are unchanged.

Qt 5.12.8/GCC 9.3 rebuilt `getdaq-tests`. Focused offscreen tests `scanPlanOutputInspectionRejectsMalformedPlans` and `offlineScanPlanBuilderIsAvailableFromConsole` passed (4 QtTest cases including setup/cleanup). See [validation 71.494](VALIDATION.md#71494-scanplanwindow-policy-implementation-separation-2026-10-25). Full refactoring continues.

## 71.495 ScanPlanWindow plan creation separation

Moved artifact/output selection, existing-plan validation/opening, scan-mode controls and plan creation into `scanplanwindow_creation.cpp`. The main lifecycle/scan orchestration implementation is now 481 lines; plan creation is 214 lines. Both qmake targets register the new unit.

The Qt test target and production Envoy application both rebuilt successfully with Qt 5.12.8/GCC 9.3. Four focused scan-plan/journal tests passed, including `nativeScanRunFinishesReceiverAndCatalogAfterEventTarget` using local simulation fixtures (6 QtTest cases including setup/cleanup). No physical hardware or acquired DAQ data was used. See [validation 71.495](VALIDATION.md#71495-scanplanwindow-plan-creation-separation-2026-10-25). Full refactoring continues.

## 71.496 NativeRunControl scan finalization separation

Moved scan-point event observation, stop submission, fresh ECC state confirmation, unresolved-operation/deadline handling and receiver/catalog finalization checks from `nativeruncontrol_scan.cpp` into `nativeruncontrol_scanfinalization.cpp`. The scan coordination source is now 517 lines and finalization is 322 lines. The shared production qmake fragment registers the new source for Envoy and dependent targets; behavior and journal boundaries are unchanged.

Qt 5.12.8/GCC 9.3 rebuilt the complete Widgets test target and production Envoy. Scan controller guards and Native simulated receiver/catalog completion passed 5/5 QtTest cases. See [validation 71.496](VALIDATION.md#71496-nativeruncontrol-scan-finalization-separation-2026-10-25). Full refactoring continues.

## 71.497 NativeRunControl scan identity and baseline observation separation

Moved frozen scan deployment identity verification, scan journal binding and event-count baseline capture into `nativeruncontrol_scanidentity.cpp`. `nativeruncontrol_scan.cpp` now retains scan-point lifecycle initiation and pending lifecycle observation at 289 lines; identity/baseline observation is 245 lines. The new unit is registered through `core.pri` for Envoy and Widgets tests.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the complete Qt test target. Native scan identity/baseline guards and simulated receiver/catalog finalization passed 5/5 QtTest cases. See [validation 71.497](VALIDATION.md#71497-nativeruncontrol-scan-identity-and-baseline-observation-separation-2026-10-25). Full refactoring continues.

## 71.498 XcfgDocument effective projection and override separation

Moved effective child/node/scalar projection, source-checked tree capture, override preview and atomic override application into `xcfgdocument_effective.cpp`. Shared path parsing and error propagation now live in `xcfgdocumentinternal_p.h`; parsing, declaration lookup, scalar validation/editing and source transaction behavior remain in `xcfgdocument.cpp`, reduced to 529 lines. Registered the new source in `xcfg.pri`, the Xcfg Qt test project and the standalone current-configuration probe.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy, the full Widgets test target, Xcfg shared tests and the standalone probe. Xcfg shared tests passed 151/151; seven focused configuration workspace/snapshot/publication tests passed 9/9 including setup/cleanup. See [validation 71.498](VALIDATION.md#71498-xcfgdocument-effective-projection-and-override-separation-2026-10-25). Full refactoring continues.

## 71.499 SystemPreparationClient CoBo maintenance separation

Moved retained Prepared-state single-CoBo reprepare/configure and LED/AsAd pulser maintenance methods into `systempreparation_maintenance.cpp`. The shared private `SystemPreparationClient::Impl` definition and hardware-step/inventory helpers moved into `systempreparation_p.h`; whole-system construction, Prepare/Configure and failure/Unprepare coordination remain in `systempreparation.cpp`, now 526 lines. CMake registers the new implementation unit; state gates, hardware sequences and audit behavior are unchanged.

Linux GCC/C++11 rebuilt `getdaq-hardware-core` and `hardware-system-preparation-tests`. The local Ice hardware-simulator integration test `hardware-system-preparation-tcp` passed 1/1 in 224.72 seconds, covering preparation, configuration, single-board maintenance and audited release paths. This is simulator evidence only; no physical hardware or DAQ data was used. See [validation 71.499](VALIDATION.md#71499-systempreparationclient-cobo-maintenance-separation-2026-10-25). Full refactoring continues.

## 71.500 SystemPreparationClient recovery and release separation

Moved retained production-stop, configuration-invalidation, failed-preparation cleanup/verification and normal Unprepare operations into `systempreparation_recovery.cpp`. The constructor and initial Prepare/Configure orchestration remain in `systempreparation.cpp`, reduced from 526 to 233 lines; maintenance operations remain in their separate translation unit. CMake registers the recovery unit. Operation ordering, state gates, audit-chain evidence and failure handling are unchanged.

Linux GCC/C++11 rebuilt `getdaq-hardware-core` and `hardware-system-release-tests`. `hardware-system-release-fault-0` passed in 44.88 seconds against the local Ice simulator. The sandboxed first attempt could not bind the simulator loopback socket; rerunning the same test with local loopback access passed. This is simulator evidence only, not physical hardware. Full refactoring continues.

## 71.501 ConfigWindow effective snapshot workflow separation

Moved effective snapshot export, compare/report presentation and background export/compare task handling into `apps/envoy/configwindow_snapshot.cpp`. The main configuration window implementation is reduced from 600 to 547 lines; `envoy.pro` and `tests/tests.pro` both compile the new unit. Snapshot behavior and the document-revision guard are unchanged.

Qt 5.12.8/GCC 9.3 rebuilt the production Envoy and full `getdaq-tests` target. The focused snapshot export/compare, asynchronous document-switch, and publication tests passed 5/5 including setup/cleanup. A sandboxed test attempt failed while opening local XCFG files; the same tests passed under normal local filesystem permissions. No hardware or DAQ data was used. Full refactoring continues.

## 71.502 ECC control observation codec separation

Moved source, receiver, receiver-event and receiver-failure observation decoders plus failed-bound-receiver comparison into `common/control/eccprotocol_observation.cpp`. The general ECC control codec is reduced from 642 to 433 lines. The shared data-node lifecycle decoder is now an inline private helper in `eccprotocolinternal_p.h`, used by both system-status and receiver-observation decoding. Registered the new unit in CMake and qmake control-protocol source lists; wire formats and validation rules are unchanged.

Linux GCC/C++11 built `getdaq-control-protocol` and `ecc-protocol-tests`; `ecc-protocol` passed 1/1. Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the full QtTest executable through qmake with the updated shared protocol source list. No hardware or DAQ data was used. Full refactoring continues.

## 71.503 HardwareClient plan execution separation

Moved the ordered plan preflight/execution engine from `apps/hardware/hardwareclient.cpp` into `hardwareclient_plan.cpp`. Connection setup, device inventory/description binding, CRUD and direct read/write entry points remain in the primary unit, now 212 lines; the plan engine is 402 lines. Shared Ice implementation state and hardware metadata helpers are isolated in private `hardwareclient_p.h`. CMake registers the new implementation unit; public API and plan/audit behavior are unchanged.

Linux GCC/C++11 rebuilt `getdaq-hardware-core` and `hardware-client-tests`. `hardware-client-tcp` passed 1/1 in 222.95 seconds against the local Ice simulator. The sandboxed attempt could not bind its loopback socket; the same test passed with local loopback access. This is simulator evidence only, not physical hardware. Full refactoring continues.

## 71.504 EccSystemClient read-only observation separation

Moved cached configuration, maintenance-target, Configure readback, ownership, receiver/event, scan-runtime identity, source and receiver-failure queries into `apps/envoy/eccsystemclient_observations.cpp`. The primary client unit retains identity/status/alarm fault gates and command submission, and is reduced from 595 to 337 lines. Shared UTF-8/error formatting helpers are in private `eccsystemclient_p.h`. Registered the new source/header in the Envoy project and standalone ECC client/probe qmake projects; request and fault-gating behavior is unchanged.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy, the full `getdaq-tests` executable and the dedicated ECC client suite. The dedicated client suite passed 69/69, covering observation identity binding, read-only behavior and command/fault gates. Tests use an in-process localhost HTTP fixture; no hardware or DAQ data was used. Full refactoring continues.

## 71.505 Catalog stream sender/receiver implementation separation (2026-10-25)

Moved `ReceiveReceiverCatalogStream` and `SendReceiverCatalogStream` into `catalogtransfer_stream.cpp` and `catalogtransfer_send.cpp`; `catalogtransfer.cpp` retains local copy/resume and integrity verification, reduced to 375 lines. The private `catalogtransfer_internal.h` shares the source model and protocol/checkpoint/validation/publication helpers, including one SIGPIPE guard used by both stream directions. CMake registers both new implementation units. Protocol framing, resume checks, durable publication order and error replies remain unchanged.

Linux GCC/C++11 rebuilt the data-supervisor core, transfer test and catalog CLI. All 12 native catalog transfer scenarios passed, including TCP resume and separate-process readback. The integration suite uses synthetic local fixtures only. See [validation 71.505](VALIDATION.md#71505-catalog-stream-senderreceiver-implementation-separation-2026-10-25). Full refactoring continues.

## 71.506 Receiver catalog scan binding and summary separation (2026-10-25)

Moved scan-point/catalog Run binding verification and publication plus multi-point Run-index generation into `receivercatalog_scanbinding.cpp`. `receivercatalog_scananalysis.cpp` now focuses on threshold and raw-waveform analyses; a private declaration shares the exact same scan binding verifier between those units. CMake registers the new translation unit. Binding/report schemas, file identity checks and output publication behavior are unchanged.

Linux GCC/C++11 rebuilt the data-supervisor core, catalog tests and CLI. The catalog test executable passed all 68 native checks, including threshold binding and synthetic waveform analysis cases. No hardware, network or actual receiver data was used. See [validation 71.506](VALIDATION.md#71506-receiver-catalog-scan-binding-and-summary-separation-2026-10-25). Full refactoring continues.

## 71.507 ConfigWindow export and workspace-copy dialog separation

Moved `OpenXmlExport` and `OpenWorkspaceCopy` into `apps/envoy/configwindow_export.cpp`; the main editor source is now 411 lines. Registered the new unit in both `envoy.pro` and `tests/tests.pro`. The review previews, source/revision guards, background tasks and create-only publication behavior are unchanged.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the full Qt test executable. Four focused XML export/workspace-copy tests passed (6 cases including setup/cleanup); they cover read-only and stale-review gates. See [validation 71.507](VALIDATION.md#71507-configwindow-export-and-workspace-copy-dialog-separation-2026-10-25). Full refactoring continues.

## 71.508 ScanExecutionJournal codec direction separation

Separated the `ScanExecutionJournal` JSON encoder and decoder into `scanexecutionjournal_encode.cpp` and `scanexecutionjournal_decode.cpp`. Both use the existing shared strict codec helpers. Updated CMake and qmake source registration; serialized formats, replay rules, event validation and restart quarantine behavior remain unchanged.

Linux GCC/C++11 rebuilt the experiment core and native journal state test; lifecycle replay, intent-before-action, quarantine and tamper rejection passed. Qt 5.12.8/GCC 9.3 rebuilt Envoy and full Qt tests; the focused journal round-trip/quarantine/reconciliation test passed. See [validation 71.508](VALIDATION.md#71508-scanexecutionjournal-codec-direction-separation-2026-10-25). Full refactoring continues.

## 71.509 MCP local settings and credential boundary separation

Moved MCP configuration parsing, credential-file validation and client-epoch persistence into `mcpsettings.{h,cpp}`. `main.cpp` retains JSON-RPC lifecycle, tool metadata and ECC command/read paths. Shared argument validation and `Settings` live in the private header. CMake and qmake both include the new source; local file ownership/mode checks, client identity binding, control enablement and explicit confirmation requirements remain unchanged.

CMake and qmake builds succeeded. The MCP stdio integration regression passed for tool schemas, read-only operations, control confirmations, owner reuse and busy-operation behavior using a localhost HTTP fixture. See [validation 71.509](VALIDATION.md#71509-mcp-local-settings-and-credential-boundary-separation-2026-10-25). Full refactoring continues.

## 71.510 MCP tool schema factory separation

Moved the pure JSON schema and tool metadata factories into `apps/ecc/mcp/mcptoolschema.{h,cpp}`. The MCP server retains JSON-RPC dispatch and only invokes these factories when listing tools. Tool names, input constraints, read-only/destructive annotations and human-confirmation descriptions are unchanged. Both CMake and qmake register the new source.

Linux GCC/C++11 CMake and qmake builds passed. The MCP stdio integration suite passed, covering schema, read-only calls, lifecycle/CoBo/catalog/recovery confirmation, owner reuse and busy/not-queued reporting with a local HTTP fixture. No hardware or DAQ data was used. Full refactoring continues.

## 71.511 MCP stdio framing separation

Moved JSON-RPC error response construction and bounded newline-delimited stdin/stdout framing into `apps/ecc/mcp/mcpstdio.{h,cpp}`. The MCP executable retains argument parsing, server dispatch and startup error reporting; the 1 MiB input bound and JSON-RPC error behavior are unchanged. CMake and qmake both register the new source.

Linux GCC/C++11 CMake and qmake builds passed. The MCP stdio integration suite passed, including oversized-message rejection, tool schema and read/control confirmation behavior against a local HTTP fixture. No hardware or DAQ data was used. Full refactoring continues.

## 71.512 Scan point binding session separation

Moved `ScanPointBindingSession` intent ownership, catalog response verification, completion/UNKNOWN handling and `BeginScanPointBinding` into `apps/envoy/scanplanreview_binding.cpp`. Shared session state and reviewed-plan/journal helper declarations live in the private `scanplanreview_p.h`; review, inspection, snapshot loading and reconciliation remain in `scanplanreview.cpp`, reduced from 593 to 429 lines. Both production Envoy and Qt tests qmake projects register the new unit. Journal locking, binding validation and state transitions are unchanged.

Qt 5.12.8/GCC 9.3 rebuilt the complete Qt test target. Focused scan journal recovery and native scan/catalog binding tests passed 7/7; the sandboxed loopback fixture attempt failed to bind, and the same tests passed with local loopback access. No physical hardware or DAQ data was used. Full refactoring continues.

## 71.513 Envoy CoBo maintenance and selected-node control separation

Moved status-table target resolution, CoBo maintenance window lifecycle and signal wiring, maintenance status/target projection, and selected-node Progress/Regress handlers into `apps/envoy/mainwindow_maintenance.cpp`. `mainwindow.cpp` is reduced from 547 to 435 lines. Both production and test qmake projects register the new unit; selection rules, UI gating and Native/legacy control behavior are unchanged.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the complete Qt test target. Focused Native one-CoBo lifecycle, legacy selected-node control and CoBo maintenance tests passed 5/5. The legacy TCP fixture required local loopback access; no physical hardware or DAQ data was used. Full refactoring continues.

## 71.514 Envoy run configuration file workflow separation

Moved RunConfig read/validation, YAML load/save dialogs, Nodes/Paths editing and run-record field add/remove handlers into `apps/envoy/mainwindow_configuration.cpp`. `mainwindow.cpp` is reduced from 435 to 390 lines; configuration values, validation and user-visible messages are unchanged. Both production Envoy and Qt tests qmake projects register the new unit.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the complete QtTest target. Focused Native setup/configuration and manual Run-number rollback tests passed 4/4. No hardware or DAQ data was used. Full refactoring continues.

## 71.515 ROOT viewer event canvas separation

Moved waveform, heatmap, histogram and rotatable surface drawing plus canvas mouse handling from `apps/rootviewer/main.cpp` into the standalone `rootcanvas.{h,cpp}` component. The viewer window retains file/event/statistics controls; its main source is reduced from 671 to 323 lines. The optional ROOT viewer CMake target registers the canvas implementation; view inputs and rendering behavior are unchanged.

With ROOT 6.26, Qt 5.12.8 and GCC 9.4, the optional `getdaq-root-viewer` target built successfully. Its offscreen `--surface-smoke` test passed with 262,458 changed pixels after a simulated drag. This verifies rendering and rotation only; no DAQ acquisition or hardware was used. Full refactoring continues.

## 71.516 Configuration source referrer inventory separation

Moved the bounded, read-only XML include inventory and reverse-referrer graph scan into `common/config/xcfgsourcereferrers.cpp`. `xcfgsourcemigration.cpp` now retains preview, migration, apply and recovery operations; directory/file/byte/XML limits, unresolved-reference reporting and read-only behavior are unchanged. Registered the unit in the production qmake fragment and the standalone shared-configuration test project.

Qt 5.12.8/GCC 9.3 built production Envoy and the shared configuration test target. All 151 shared-configuration tests passed, including transitive referrer discovery and incomplete external/malformed reference reports. The transaction-lock test cache was redirected to a temporary test directory. No hardware or DAQ data was used. Full refactoring continues.

## 71.517 Sentry status sampling separation

Moved `SentryFiles::Status` and its process-existence probe into `apps/sentry/sentryservice_status.cpp`. Status response fields, process-check timeout/failure behavior, disk availability checks and `.graw` aggregation remain unchanged. Registered the unit in Sentry, simulator and Qt test qmake projects.

Qt 5.12.8/GCC 9.3 rebuilt production Sentry, simulator and the complete Qt test target. Focused catalog/status and HTTP endpoint tests passed; the endpoint test required localhost loopback access. Documentation link check passed 2015 local links. No hardware or DAQ raw was used. Full refactoring continues.

## 71.518 RunControl transition execution separation

Moved the ordered transition driver (`Begin`, `NextStep`, ECC acknowledgement/state confirmation, plan completion and failure handling) into `apps/envoy/runcontrol_execution.cpp`. `runcontrol.cpp` retains connection management, polling, user lifecycle commands and recovery/catalog coordination. Transition ordering, journal checkpoints and state confirmation requirements are unchanged; `core.pri` registers the new unit for Envoy and Qt tests.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the complete Qt test target. `simulatedLifecycle`, `readyDuringTransitionCannotStart` and `legacySelectedNodeProgressAndRegressRemainIndependent` passed; the two legacy TCP fixtures required localhost loopback access. No physical hardware or DAQ data was used. Full refactoring continues.
## 71.519 RunControl command and recovery separation (2026-10-25)

Moved Progress/Regress, Run start/stop, offline-tool gating and run-journal recovery handlers into `apps/envoy/runcontrol_commands.cpp`. `runcontrol.cpp` now retains connection and metadata management, polling/status publication, journal writes and Sentry/catalog replies; `core.pri` registers the new unit. Command validation, run reservation/replacement safeguards and recovery identity checks are unchanged.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the complete Qt test target. `simulatedLifecycle`, `readyDuringTransitionCannotStart`, `legacySelectedNodeProgressAndRegressRemainIndependent` and `runReplacementWaitsForAllCatalogsBeforeCleanupAndIncrement` passed. The three TCP fixture tests required local loopback access. No physical hardware or DAQ data was used. Full refactoring continues.
## 71.520 Sentry HTTP request routing separation (2026-10-25)

Moved `SentryService::Handle` into `apps/sentry/sentryservice_http.cpp`. The service implementation now owns startup/config checks and filesystem catalog operations, while the new unit owns HTTP route/method/body validation, job admission, delayed dispatch and result replies. Request paths, status codes, concurrency limits and async behavior are unchanged. Sentry, simulator and Qt test qmake projects register the new unit.

Qt 5.12.8/GCC 9.3 rebuilt Sentry, simulator and the complete Qt test target. Six focused catalog/replacement/config tests passed; `sentryHttpEndpoints` passed with local loopback access. Documentation link validation passed 2015 local links. No physical hardware or DAQ data was used. Full refactoring continues.
## 71.521 Receiver catalog replacement preparation separation (2026-10-25)

Moved `PrepareReceiverCatalogReplacement` into `apps/datasupervisor/receivercatalogreplacementprepare.cpp`, separating candidate construction and its hardlink/metadata verification from publication, observation and final cleanup. The shared private `receivercatalogreplacement_p.h` keeps directory fsync/check behavior identical for preparation and the remaining transaction phases. The data-supervisor CMake target registers the new translation unit; catalog format and replacement phase semantics are unchanged.

Linux GCC/C++11 rebuilt the supervisor core, receiver catalog tests and catalog CLI. All 68 receiver catalog checks passed, including replacement candidate identity/provenance, chained retained files, publish/finalize recovery and sparse raw no-copy assertions. Tests used synthetic metadata/raw fixtures; no physical receiver, hardware, network or production DAQ data was accessed. Full refactoring continues.
## 71.522 Receiver catalog replacement observation separation (2026-10-25)

Moved the read-only `InspectReceiverCatalogReplacement` state classifier to `receivercatalogreplacementinspect.cpp`, leaving publication and final cleanup in the transaction unit. Replacement path construction, intent/marker validation, catalog identity matching and directory durability helpers are now shared through the private `receivercatalogreplacement_p.h` boundary. Observation remains read-only and cannot authorize recovery; phase names and proof checks are unchanged.

Linux GCC/C++11 rebuilt the supervisor core, receiver catalog tests and catalog CLI. All 68 receiver catalog checks passed, including prepared/published/finalized classification, restart recovery, candidate identity/provenance and preservation of non-colliding old raw. Synthetic fixtures only; no physical receiver, hardware, network or production DAQ data was accessed. Full refactoring continues.
## 71.523 Receiver catalog replacement finalization separation (2026-10-25)

Moved `FinalizeReceiverCatalogReplacement` into `receivercatalogreplacementfinalize.cpp`. Exact same-source/name collision resolution, inode/device/size proof, active-session lease checks, old raw link removal, per-file completion markers and idempotent recovery are now isolated from candidate preparation, read-only phase inspection and publication. Shared transaction helpers remain behind the private replacement header; finalization policy and receipts are unchanged.

Linux GCC/C++11 rebuilt the data-supervisor core, receiver catalog tests and catalog CLI. All 68 receiver catalog checks passed, including exact collision cleanup, preservation of unrelated old raw, retry after partial progress, finalization idempotence and read-only phase observation. Synthetic fixtures only; no physical receiver, hardware, network or production DAQ data was accessed. Full refactoring continues.
## 71.524 Receiver catalog replacement publication separation (2026-10-25)

Moved `PublishReceiverCatalogReplacement` into `receivercatalogreplacementpublish.cpp` and renamed the remaining common transaction support implementation to `receivercatalogreplacementhelpers.cpp`. Candidate preparation, read-only observation, publication and superseded-raw finalization now each have a dedicated translation unit; the private replacement helper boundary centralizes journal identity, marker and durability checks. Publication order and rename-no-replace behavior are unchanged.

Linux GCC/C++11 rebuilt the data-supervisor core, receiver catalog tests and catalog CLI. All 68 receiver catalog checks passed after the full phase split, covering prepared/published/finalized states, idempotent retries, exact same-name raw cleanup, preservation of unrelated old files and no raw payload reads. Synthetic fixtures only; no physical receiver, hardware, network or production DAQ data was accessed. Full refactoring continues.
## 71.525 XcfgDocument shared scalar property projection separation (2026-10-25)

Moved shared class-property graph projection, effective scalar metadata resolution, declaration-property enumeration, typed/range validation and scalar text encoding into `common/config/xcfgdocumentproperties.cpp`. `xcfgdocument.cpp` retains XML source loading, indexed path lookup, editing and source-consistency checks. Updated the shared production qmake fragment and both standalone configuration probes so every consumer links the extracted implementation. Include precedence, imported metadata conflict rules and edit validation are unchanged.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy, the full Qt test target and the standalone shared-configuration target. All 151 shared-configuration tests passed, including shared metadata projection through includes, range conflict checks, exact scalar edits and source-drift rejection. The isolated shared-test cache was redirected to a fresh private directory. No hardware or DAQ data was used. Full refactoring continues.
## 71.526 Waveform frame decoding separation (2026-10-25)

Moved `.graw` and `.gmat` selected-frame reads, binary sample decoding and integer-CoBo-ADC classification into `apps/waveformviewer/waveformframe.{h,cpp}`. `WaveformWindow::Refresh` now coordinates frame metadata, canvas updates, status messages and pulse metrics without owning the file decoding loops. Registered the unit in the shared waveform viewer qmake fragment used by the standalone viewer, Envoy and tests; file layout and sample validation behavior are unchanged.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy, standalone waveform viewer and complete Qt test target. Eight focused viewer indexing/decode/truncation/metrics/summary/navigation/CSV tests passed. Offscreen `--surface-smoke` passed with 96,353 changed pixels after drag rotation. No hardware or DAQ acquisition was used. Full refactoring continues.

## 71.527 Config-tool scan-plan command separation (2026-10-25)

Moved scan-plan creation, execution-policy/snapshot creation and their bounded input validation/private staging helpers from `apps/configtool/main.cpp` into `apps/configtool/scanplancommands.{h,cpp}`. The CLI entry point dispatches scan commands to the extracted unit and retains compiler, migration, export and configuration commands. Registered the source in the standalone config-tool project and its CLI probe; command syntax, output, validation and no-overwrite publication behavior are unchanged.

Qt 5.12.8/GCC 9.3 rebuilt both the production `getdaq-config` executable and `getdaq-config-probe`. `tests/system_experiment_integration.py` passed all 68 process cases against the freshly built CLI and offline native configuration inspector, covering plan generation, policy and snapshot creation, rejected overwrites and artifact integrity. The integration run verified the original source configuration remained unchanged; no hardware or DAQ data was used. Full refactoring continues.

## 71.528 Sentry catalog transaction separation (2026-10-25)

Moved legacy catalog creation and explicit completed-Run replacement/finalization from `apps/sentry/sentryservice.cpp` into `sentryservice_catalog.cpp`. Shared failure replies, atomic JSON journaling and strict legacy catalog validation now live behind the private `sentrycatalog_p.h` boundary in `sentrycatalog_helpers.cpp`. `sentryservice.cpp` retains configuration loading and server startup; receiver locking, seal checks, file rename order, same-name collision rules, retained non-colliding raw files and replacement recovery behavior are unchanged. Sentry, simulator and Qt tests qmake projects register both new units.

Qt 5.12.8/GCC 9.3 rebuilt Sentry, simulator and the complete Qt test target. `sentryCatalogFiles`, `sentryCatalogExplicitReplacementDefersOldDataCleanup`, `sentryCatalogReplacementRejectsIncompleteOldRun`, `sentryReplacementResumesAfterMergedManifestBeforeBackupCleanup` and `sentryConfigStrictFields` passed; `sentryHttpEndpoints` passed with localhost loopback enabled. No physical receiver, hardware or production DAQ data was used. Full refactoring continues.

## 71.529 Xcfg inherited-object override separation (2026-10-25)

Moved inherited object override planning, projection checks and preview construction into `common/config/xcfgstructureeditor_override.cpp`, and include replacement/import expansion into `common/config/xcfgstructureeditor_include.cpp`. Shared XML element generation and error reporting live in private `xcfgstructureeditor_p.h`; the main structure editor retains ordinary declaration edits and materialization. Registered both new implementations in the common qmake fragment and standalone shared-configuration tests. Override selection, include expansion limits, comment preservation, source verification and effective-value invariants are unchanged.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the complete Qt test target. The standalone shared-configuration suite passed all 151 tests, including inherited object overrides, include projections, materialization, undo/redo and source-drift rejection. Documentation link validation passed 2015 local links. No hardware or DAQ data was used. Full refactoring continues.

## 71.530 Xcfg inherited materialization separation (2026-10-25)

Moved inherited-object materialization planning into `common/config/xcfgstructureeditor_materialize.cpp`. Shared child insertion and path-tail helpers now live in the private `xcfgstructureeditor_p.h`; the primary structure editor retains operation dispatch, source verification, candidate validation and reviewed apply. Production and standalone shared-configuration qmake projects register the new unit. Materialization budgets, comment preservation, include removal and effective-value invariance remain unchanged.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the complete Qt test target. All 151 shared-configuration tests passed, including inherited subtree materialization, include/comment preservation, self-closing sources, history reversibility and source-drift rejection. No hardware or DAQ data was used. Full refactoring continues.

## 71.531 System acquisition hardware preflight separation (2026-10-25)

Moved frozen-topology node identity checks, hardware-plan validation, sender validation and initial stopped-state observations into `apps/hardware/systemacquisition_preflight.{h,cpp}`. `SystemAcquisitionClient::Impl` retains lazy hardware-client setup and supplies the same audited step executor and run-audit recorder. Preflight order, interlock selection, expected register checks and the guarantee that no RunInit write is sent before all targets validate remain unchanged. Registered the new unit in `apps/hardware/CMakeLists.txt`.

Linux GCC/C++11 rebuilt `getdaq-hardware-core` and `hardware-system-acquisition-tests`. The complete `hardware-system-acquisition-tcp` CTest passed (1/1, 245.04 s) using synthetic CoBo/MuTaNT endpoints over localhost; no physical hardware or DAQ raw was used. Full refactoring continues.

## 71.532 Scan-plan runtime coordination separation (2026-10-25)

Moved current ECC identity checks, scan-point lifecycle requests, receiver event-baseline callbacks, Run status handling and pending-journal reconciliation from `apps/envoy/scanplanwindow.cpp` into `scanplanwindow_runtime.cpp`. Registered the new implementation in production Envoy and Qt test qmake projects. Existing journal gates, confirmation prompts, read-only identity checks and lifecycle signals are unchanged.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and the complete Qt test target. `offlineScanPlanBuilderIsAvailableFromConsole` and `scanEventBaselineRequiresConnectedNativeController` passed; documentation link validation passed 2015 local links. A full Qt-suite attempt could not complete in the sandbox: local listener creation was denied and configuration tests reported busy source transactions. No hardware or DAQ data was used. Full refactoring continues.

## 71.533 Receiver scan analysis separation (2026-10-25)

Separated threshold-scan MFM hit-pattern analysis from bound-scan waveform analysis in `apps/datasupervisor/receivercatalog_scananalysis.cpp` and `receivercatalog_scanwaveformanalysis.cpp`. The descriptor-backed bounded raw reader is shared through the private `receivercatalog_scananalysis_p.h`; both paths retain their existing plan/binding/catalog checks and exclusive no-overwrite report publication. Registered the new unit in the data-supervisor CMake target.

Linux GCC/C++11 rebuilt the data-supervisor core, catalog CLI, query CLI and receiver-catalog tests. The `receiver-catalog-filesystem` CTest passed (1/1, 23.81 s); documentation link validation passed 2015 local links. No hardware or DAQ raw was accessed. Full refactoring continues.

## 71.534 Native ECC failure reconciliation separation (2026-10-25)

Moved retained-operation reconciliation, failed Describe/Prepare/Acquisition recovery decisions, and receiver/control-input failure holds from `nativeecccoordinator_recovery.cpp` to `nativeecccoordinator_reconciliation.cpp`. The small operation-ID validator now lives in private `nativeecccoordinator_recovery_p.h`, shared by reconciliation and cleanup implementations. Recovery audit proofs, original-operation identity checks and no-replay/cold-takeover gates are unchanged; the new source is registered in the Native ECC CMake target.

Linux GCC/C++11 rebuilt `getdaq-ecc-native` and `native-ecc-coordinator-tests`. `native-ecc-host-ownership` and `native-ecc-fault-prepare-0` passed using synthetic local Ice hardware endpoints; loopback permission was required. No physical hardware or DAQ data was used. Full refactoring continues.

## 71.535 Hardware plan execution separation (2026-10-25)

Moved the audited step-execution phase of `HardwareClient::Process` into `HardwareClient::Impl::ExecutePlan` in `hardwareclient_execute.cpp`. `hardwareclient_plan.cpp` retains ordered software preflight, remote-description checks, complete plan compilation and the no-write `Validate` path; execution receives those exact validated widths, targets, managers and plan JSON. Audit ordering, interlock checkpoints, RPC order, polling delays and no-replay errors are unchanged. Registered the new unit in the hardware CMake target.

Linux GCC/C++11 rebuilt `getdaq-hardware-core`, `hardware-client-tests` and `hardware-system-acquisition-tests`. The complete `hardware-client-tcp` regression passed (1/1, 226.10 s) against synthetic local Ice devices. No physical hardware or production DAQ data was used. Full refactoring continues.

## 71.536 ECC request codec separation (2026-10-25)

Moved ECC control request field decoding, command-specific request validation, canonical request encoding and request-key hashing into `common/control/eccprotocol_request.cpp`. `eccprotocol.cpp` retains response/status/configuration codecs and operation-envelope validation; operation decoding calls the shared private request decoder. CMake and the shared qmake fragment register the new unit. Wire fields, canonical bytes, request hashes and validation behavior are unchanged.

Linux GCC/C++11 rebuilt the control protocol, ECC protocol test and hardware-client test targets; the ECC protocol executable passed. The Qt 5.12.8/GCC 9.3 ECC system-client qmake target rebuilt and all 69 tests passed, including request identity and persist-before-submit coverage. Documentation link validation passed 2015 local links. No physical hardware or DAQ data was used. Full refactoring continues.

## 71.537 ECC configuration evidence codec separation (2026-10-25)

Moved ECC configuration-cache, ownership-cache, service-identity and Configure historical-readback decoders into `common/control/eccprotocol_configuration.cpp`. Controller status decoding is now a shared private helper used by system status, operation results and readback evidence. `eccprotocol.cpp` retains alarm/maintenance/catalog/system/operation codecs, and the shared CMake and qmake projects register the new unit. Schemas, identity bindings, historical-only evidence claims and bounds are unchanged; the main protocol unit is reduced from 280 to 149 lines.

Linux GCC/C++11 rebuilt the control protocol and dependent hardware-client tests; the ECC protocol executable passed. Qt 5.12.8/GCC 9.3 rebuilt the ECC system-client target and all 69 tests passed. Documentation link validation passed 2015 local links. No physical hardware or DAQ data was used. Full refactoring continues.

## 71.538 System acquisition failure handling separation (2026-10-25)

Moved explicit producer-stop-after-failure, failed ICE source retirement, failure Breakup, retained failure audit access and ICE failure observation APIs into `apps/hardware/systemacquisition_failure.cpp`. The shared private `systemacquisition_p.h` now owns the acquisition implementation state and its fixed preparation/lifecycle/audit context; `systemacquisition.cpp` retains ordinary connect/start/pause/resume/stop/disconnect and Breakup orchestration. The hardware CMake target registers the new unit. Operation gates, stop-once tracking, UNKNOWN retention, audit order and no-replay behavior are unchanged; the public lifecycle implementation file is reduced from 535 to 110 lines.

Linux GCC/C++11 rebuilt the hardware core and system-acquisition test target. `hardware-system-acquisition-tcp` passed (1/1, 233.94 seconds) against synthetic local Ice CoBo/MuTaNT endpoints, covering encoding 1.0 and 1.1, run lifecycle, preflight, UNKNOWN outcomes and explicit failure-stop behavior. No physical hardware or DAQ data was used. Full refactoring continues.

## 71.539 Remote catalog publication evidence separation (2026-10-25)

Separated `RemoteCatalogPublishEvidence` from `RemoteHandoffEvidence` into `apps/ecc/native/remotecatalogpublishevidence.cpp`. The original source now contains only handoff-history evidence validation; a private `remotehandoffevidence_p.h` shares descriptor-bound evidence-file checks, bounded directory enumeration and journal framing without duplicating security-sensitive helpers. The Native ECC CMake target registers the new unit. Evidence schemas, replacement identity bindings, journal phase checks and fault latching are unchanged.

Linux GCC/C++11 rebuilt Native ECC and both focused test targets. `remote-cold-audit-evidence` passed (1/1); `native-ecc-remote-receiver-catalog-replace` passed (1/1, 5.58 s) against local synthetic receivers. No physical hardware or production receiver data was used. Full refactoring continues.

## 71.540 Supervisor control journal recovery separation (2026-10-25)

Moved persisted control-journal replay, storage identity/content revalidation, and exact failed retirement/handoff completion checks into `apps/datasupervisor/controljobs_recovery.cpp`. `controljobs_impl.cpp` retains service construction, durable event append and request execution. The shared private `SupervisorControl::Impl` state and journal order remain unchanged; the data-supervisor CMake target registers the recovery unit.

Linux GCC/C++11 rebuilt the data-supervisor core, control test and supervisor executable. `supervisor-control-codec`, `supervisor-control-service` (1/1, 11.54 s) and `data-supervisor-process-lifecycle` (1/1, 21.59 s) passed. Integration tests used synthetic local receiver/service processes and exercised persisted restart/recovery gates. No physical hardware or production receiver data was used. Full refactoring continues.

## 71.541 Configuration structure editor translation-unit separation (2026-10-25)

Moved `ConfigWindow::OpenStructureEditor` into `apps/envoy/configwindow_structure.cpp`, isolating the structural-change review UI and its Qt/XcfgStructureEditor dependencies from the main configuration window implementation. Registered the new source in production Envoy and Qt test qmake projects. Preview, apply, source-drift guards, shared-property review and history behavior are unchanged; the remaining `configwindow.cpp` is reduced from 424 to 288 lines.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and `getdaq-tests`. Five structure/history tests and four Data file tools close lifecycle groups passed using an isolated temporary Qt cache/home. No hardware or DAQ data was used. Full refactoring continues.

## 71.542 MainWindow configuration presentation separation (2026-10-25)

Moved `MainWindow::SetConfig` into `apps/envoy/mainwindow_configpresentation.cpp`, isolating the full configuration-to-GUI synchronization path: native/legacy controls, status-table schema and sizing, run fields, parameter rows, node selector, and plot binding. The method order and control gates are unchanged; `mainwindow.cpp` decreased from 390 to 259 lines. Registered the new unit in production Envoy and Qt test qmake projects.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and `getdaq-tests`. Five configuration-presentation tests passed, covering native configuration/ownership panels, parameter visibility, configured-node table sizing and reflow after reducing node count. No hardware or DAQ data was used. Full refactoring continues.

## 71.547 EccSystemClient control-command separation (2026-10-25)

Moved `CanSubmit`, lifecycle/plan-retirement submission, retained-retirement validation and control-handoff receipt queries into `apps/envoy/eccsystemclient_control.cpp`. The original client unit retains endpoint/credential validation, identity/status/alarm queries and generic HTTP operation transport; the command unit still routes every write through the intent writer before a single POST, with no automatic replay. Registered the new unit in production Envoy, ECC client tests and direct client-probe qmake projects.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and `ecc-system-client-tests`. `productionStoreReopensPendingWithoutReplay` and `reviewedPlanRetirementPersistsAndHoldsEveryNewCommand` passed (4/4 including init/cleanup) against synthetic localhost services, covering retained-ID observation without replay and durable retirement isolation. No physical hardware or DAQ data was used. Full refactoring continues.

## 71.546 MainWindow ECC observation views separation (2026-10-25)

Moved ownership-response rendering, configuration snapshot display and configure-readback observation forwarding into `apps/envoy/mainwindow_observations.cpp`. `mainwindow_status.cpp` now focuses on node-row and control-state presentation; observation identity checks, stale ownership handling and “hardware load unverified” messaging are unchanged. Registered the new unit in production Envoy and Qt tests.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and `getdaq-tests`. Four focused tests passed: ownership response expiry/late-reply rejection, configuration snapshot versus hardware-proof separation, native node status and recovery gates, and native GUI metadata/control presentation (6/6 including init/cleanup). No hardware or DAQ data was used. Full refactoring continues.

## 71.545 MainWindow recovery menu construction separation (2026-10-25)

Moved native recovery/safety action construction and its Pause/Resume actions into `apps/envoy/mainwindow_recoverymenus.cpp`, separating the long recovery confirmations, status viewers and signal wiring from the base menu/header setup. The call remains at the same point in menu initialization, preserving menu ordering, action object names, enablement updates and signal behavior. Registered the new unit in production Envoy and Qt tests.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy and `getdaq-tests`. `nativeGuiMetadataAndWholeSystemControls` passed (3/3 including init/cleanup), checking native action construction and recovery gating; `offlineDataToolsMenuWindowClosesAndCanReopen` passed (5/5), covering the Tools menu close/reopen paths. Documentation link validation passed 2015 local links. No hardware or DAQ data was used. Full refactoring continues.

## 71.544 Native RunControl connection lifecycle separation (2026-10-25)

Moved `NativeRunControl::UpdateRunMetadata`, `Connect` and `Disconnect` into `apps/envoy/nativeruncontrol_connection.cpp`. This unit now owns native-client construction, intent-writer/store binding, signal wiring, initial identity/status requests and the active/pending-evidence disconnect gate; shutdown, polling and command execution remain in their existing units. Registered the source through the shared Envoy core qmake project.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy, `getdaq-tests` and the dedicated `native-gui-tests`. `wholeSystemOnceAndSealBeforeRunRecord` passed (3/3 including init/cleanup), exercising Connect, lifecycle control, finalization, Disconnect and durable Run completion against a synthetic localhost ECC fixture. No physical hardware or DAQ data was used. Full refactoring continues.

## 71.543 ECC client observation policy separation (2026-10-25)

Moved persisted operation observation validation and recovery-hold updates from `EccClientStore::Impl` into `apps/envoy/eccclientstore_observationpolicy.cpp`. The store still owns request/journal persistence, integrity checks and replay ordering; the extracted policy receives the existing request, observation and hold state and applies the same terminal-result, lifecycle recovery and reconciliation rules. Registered the new unit in production Envoy, Qt tests and the direct client-probe target.

Qt 5.12.8/GCC 9.3 rebuilt production Envoy, `getdaq-tests` and `ecc-system-client-tests`. `eccClientStoreClearsOnlyExactUnknownLifecycleOperation` passed, including reopen after the policy cleared the exact matching unknown hold. Other client tests requiring localhost-bound fixtures could not run in the restricted sandbox (`Cannot bind fixture`). No hardware or DAQ data was used. Full refactoring continues.

## 71.713 Waveform decoder format separation (2026-10-27)

Separated `.graw` sample decoding for standard CoBo type-1 and compact type-2 frames into `grawreader_decode_type1.cpp` and `grawreader_decode_type2.cpp`. `grawreader_waveform.cpp` now owns indexed-frame revalidation, bounded file reads and format dispatch; both standalone viewer and aggregate Qt projects register the private decoder units through the shared qmake fragment. Byte decoding, validation and returned sample order remain unchanged.

Qt 5.12.8/GCC 9.3 rebuilt the complete `getdaq-tests` target and the standalone `getdaq-waveform-viewer`. All eight waveform-viewer tests plus the integrated viewer run-gating test passed (11/11 including QtTest setup/cleanup), covering raw/layered type-1, compact type-2, truncated MFM, analysis, navigation, export and run gating. No physical hardware or DAQ data was used. Broader application and platform audits remain outstanding.

## 71.714 MFM merge input indexing separation (2026-10-27)

Moved input-path collision checks and bounded MFM frame scanning/index creation from `apps/datareceiver/merge.cpp` into `merge_index.cpp`, with the private `merge_internal.h` carrying indexed frame references between the scanner and event-output coordinator. `merge.cpp` now owns event ordering, outer-frame construction, bounded output rotation and exact source-frame copying. Both `getdaq-merge` and `merge-tests` register the scanner unit in the standalone DataReceiver CMake project.

A fresh GCC 9.4/C++11 Release configuration rebuilt every default DataReceiver tool and test target. All seven CTest cases passed, including the two local-loopback tests after rerunning them outside the socket-restricted sandbox. The merge test additionally confirms input/output alias rejection and truncated-primary-header rejection before output creation, alongside event ordering, preserved embedded bytes and existing-output protection. Tests used small synthetic fixtures; no hardware, external network or production receiver data was accessed.

## 71.715 Exclusive MFM merge output reservation (2026-10-27)

Replaced the merger's check-then-open/truncate output path with `open(O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC)` and bounded POSIX descriptor writes. Concurrent mergers now reserve distinct numbered output files; short writes and interrupted writes are handled explicitly, and successful output files are fsynced before completion. The existing failure cleanup still removes outputs produced by an unsuccessful merge.

The DataReceiver default build completed for all tools and test targets. CTest passed 7/7, including fresh local-loopback reruns. The merger test now forks two simultaneous merge processes against the same requested output and verifies both files exist separately and contain identical merged bytes; prior event ordering, source-byte preservation, alias/truncation rejection and existing-output checks also passed. Synthetic fixtures only; no physical hardware or production receiver data.

## 71.716 Configuration selection export publication boundary (2026-10-27)

Extracted selection receipt file publication from the `getdaq-config-upload` response callback into `apps/configtransfer/selectionfile.cpp`. The helper writes complete metadata to a private same-directory temporary file, restricts it to owner-read permissions, then renames it into a new destination without replacing an existing file. Failed writes/publication leave no partial destination or temporary file. Both the standalone upload CLI and aggregate Qt test project register this source.

Qt 5.12.8/GCC 9.3 rebuilt the full `getdaq-tests` target and standalone `getdaq-config-upload`. `configurationSelectionFilePublicationIsAtomicAndNoReplace` passed, covering exact bytes/permissions, existing-target preservation, failed publication cleanup and empty-input rejection. CLI help smoke passed. No network, repository service or production configuration was used in these focused checks; the broader repository/ECC integration was not rerun.
