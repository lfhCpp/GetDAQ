# Original SOAP ECC, DataExporter and DataRouter

GetDAQ now creates `getEccSoapServer` and `DataExporter` / `dataRouter` instances. The rewritten
`getdaq-ecc-server` is retired from normal builds and automatic startup. Existing
original SOAP ECC connections continue to work. A saved native group must be
recreated as an original backend group; its old audit and data are retained.

The main Run Control layout is unchanged. Master / Slave manages files and
processes; it does not implement the electronics control sequence. Only the
existing log header gains the approved log-selection buttons.

## Offline build

Required platform tools: C/C++ compiler, CMake, Python 3, and Qt 5 for the GUI,
node agent and file monitor. Ice, Boost, CompoundConfig, TinyXML, DHSM, MFM,
gSOAP and Asio are built from project-local sources. No separately installed
GET software, Boost or Ice is required. The historical `source/` tree is not a
build input. Operating-system C/C++ runtime libraries remain required.

The normal `scripts/build-ubuntu.sh` / full host-suite build includes all three
original programs. To reuse an existing bundled Ice build:

```sh
python3 tools/build-original-backends.py \
  --build-dir build-original-backends \
  --ice-build build-delivery/full/native --jobs 2
```

Outputs: `build-original-backends/build/getEccSoapServer` and `DataExporter` / `dataRouter`.
The normal suite stages the executables into `bin/` and format definitions into
`share/get-bench/format/`.

## Configure in the GUI

1. On a worker, choose **Slave** and copy its pairing code. Keep the node agent
   running. The worker needs a local data directory, but no source configuration
   directory or NFS mount.
2. On the master choose **Master** → **Create ECC / data services**. Use this
   computer, or add the worker using its pairing code.
3. Add an ECC group. Select the original XML/XCFG directory and experiment ID.
   For `pedestals`, the repository must contain `describe-pedestals.xcfg`,
   `configure-pedestals.xcfg` (or `.xml`). Describe also automatically accepts `describe-cobo0.xcfg` for CoBo index 0. Include
   their referenced files inside that directory, using relative references.
   Board IPs and electronics settings are those in these original files; use
   the original configuration editor to edit them.
4. Choose the ECC and data execution hosts. They can be the same computer.
   Choose the project directory on each host. Enter the ECC host IP, receiver
   control IP, receiver data IP and their separate ports. The receiver IP must be
   a local address on the selected data host, reachable from the CoBo.
5. Select **Start services and use this configuration**. GetDAQ publishes an
   immutable release, caches and copies exact bytes into each host's project
   directory, starts the selected original receiver and SOAP ECC, then checks
   the shared Sentry. No hardware
   preparation or acquisition occurs during service startup.
6. In Run Control choose **Connect**, then Describe / Prepare / Configure /
   Start as usual. Stop acquisition before stopping backend services.

Ports have different purposes:

| Setting | Purpose |
|---|---|
| SOAP ECC | GUI sends Describe, Prepare, Configure, Start and Stop |
| Alarm callback | Original ECC receives electronics alarms |
| DataExporter control | Ice control endpoint; separate from incoming raw data |
| CoBo data destination | CoBo sends raw frames to this TCP port |
| Waveform TCP export | DataExporter sends length-prefixed frames to clients |
| Sentry monitor | File counts, byte counters and run archiving |

Each group receives its own ports and process identities. New groups use the
explicit project directory on each execution host for configurations and raw data.
Groups in the same project share one Sentry per host/project folder. Multiple groups may run on one computer. Use different
ports and assign each board to only one active ECC. Process stops target the
selected instance, rather than killing all programs by executable name.

New console profiles use SOAP and explicitly map the selected configuration IDs
(the experiment, with the original CoBo-specific Describe fallback). Before publishing, the master rereads its source
files. Workers use the downloaded release; they never need the master's path.

## Scope of source adaptations

Original hardware-control, frame parsing, file-writing and TCP export logic are
retained. Build compatibility changes are limited to:

- Project-local static dependencies and explicit compiler definitions/includes.
- Initialize the original ECC backend after program startup to avoid an old
  Boost filesystem static-initialization crash.
- Apply the original SOAP namespace table explicitly with current gSOAP.
- DataExporter: configurable export port (default remains 46491), explicit
  format-resource environment path, instance identity argument, and orderly
  SIGINT/SIGTERM shutdown through the original receiver/server stop methods.

Upstream DataExporter provenance is in `third_party/DataExporter/UPSTREAM.json`.
Original files and their provenance are recorded in the legacy source manifest.
Licenses and source copyright notices are retained.

## Verification boundary

Real original programs have been tested without physical boards: SOAP Idle,
receiver and exporter listeners, synthetic MFM frames written and exported
byte-for-byte, graceful exit, and two authenticated node agents downloading and
starting/stopping the backends. Physical hardware acquisition and native macOS
builds still require validation on their respective hosts.

## Connect an existing DataRouter on another host

The remote host does **not** need DataExporter. Choose **Connect existing ECC**,
enter the existing SOAP ECC endpoint, and set the receiver IP / data port to the
already-running DataRouter's TCP data listener. GetDAQ sends that data target in
the original ECC `DataLinkSet`. This connection path does not launch, locate or
require a DataExporter executable on either host. Do not substitute the router's
Ice management port for its raw-data TCP port.

Sentry is a separate file-monitor/archive service. Its status does not identify
which receiver implementation is in use. The current manual Run start still
checks Sentry health and disk space; a missing monitor must be configured
separately and is not evidence that DataExporter is required.

## Minimal-source audit

The recorded upstream files in `third_party/DataExporter` are now byte-identical
to the pinned upstream revision. GET hardware control, the state machine, MFM
parsing, frame storage and DataExporter's TCP server remain upstream code.

Three small, explicit entry-point patches live in `apps/original-backends/` and
are applied only to build-directory copies:

| Patch | Changes | Reason |
|---|---|---|
| `ecc-entry.patch` | 3 added / 1 removed lines | Initialize the backend after C++ startup; apply the original namespace table with current gSOAP |
| `datarouter-entry.patch` | Instance metadata and graceful shutdown | Integrate the original router without changing frame storage |
| `dataexporter-entry.patch` | 22 added / 3 removed lines | Export-port option, resource lookup, instance metadata, and orderly process exit |

The default export port stays 46491. These patches do not change register
operations, electronics configuration, frame formats, file contents or the TCP
export framing. Build scripts statically compile bundled dependencies. The
current bundle uses file-based ECC configuration; optional database and extra
logging plugins are not included. No DataRouter replacement or automatic
remote DataExporter installation is needed for existing receiver connections.

## Unified project directories and configuration updates

See [Configuration and data paths](CONFIGURATION_AND_DATA_PATHS.md) for the complete
workflow. The master source directory defaults to `<installation>/config`. Its
complete relative file tree is delivered to each selected execution host, cached,
and automatically installed in that host's explicit project data directory.
Original SOAP ECC, DataExporter / DataRouter and shared Sentry run in that directory.
Receiver control and data interfaces are separate settings. No per-instance raw
subdirectory is added for new project groups.

Configuration changes are staged while hardware is not confirmed Idle; they are
applied after services stop or before the next Describe. Existing raw files, Run
records and logs remain. Changed configurations are backed up. Tools → Sync project
configuration supports local, selected remote, or all hosts. Sync can be disabled
explicitly, in which case configuration backup is marked unverified.

The original main window layout is protected; only the existing log header has
Event / ECC / receiver buttons and a host/instance selector. Managed process logs
are authenticated, bounded reads, including the shared Sentry output.

## Restart after a software update

A configuration content release is separate from a local runtime preparation.
Each accepted prepare now stores its immutable runtime recipe under
`deployments/<release>/<instance>/preparations/<operation>/`; a latest-preparation
pointer `current-preparation.json` selects it, and current activation requests bind its preparation ID. The legacy `prepared.json` may be read-only (0400); it is retained unchanged and is only a fallback when no new selector exists.
Updating the program directory or preparing unchanged XML again therefore does
not overwrite the old `service.json`. Old configuration and process audit remain.

If a group was **terminated**, its processes may be absent while the deployment
remains `terminated-needs-inspection`. Normal Stop cannot query an exited ECC.
After verifying board acquisition has stopped and raw data is preserved, use
**Review terminated group**, then **Start project services**. Inspection validates
that the registered old executable/recipe still matches and matching processes
are absent, including preserved old runtime directories; it does not require the
old program to be installed beside the current node agent. Process absence does
not itself prove the hardware stopped, reset hardware UNKNOWN, or complete a Run.

Do not delete deployment journals to get past an immutable-file or inspection
error. Unknown original operations remain retained and are not automatically
replayed. A stale prepared-recipe ID is rejected before starting processes.
