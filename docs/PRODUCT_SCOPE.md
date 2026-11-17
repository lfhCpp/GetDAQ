# GetDAQ supported product baseline

Baseline: `original-get-backends-v1` (2026-11-17).

The product uses original `getEccSoapServer` for electronics control and ATTPC
`DataExporter` for managed reception. An existing external `DataRouter` or
`DataExporter` is used directly by selecting its receiver IP / TCP data port in
the existing-ECC connection. No exporter is required on an external router host.

Retained functions:

- Original main Run Control layout, node selection, XML configuration IDs and
  YAML files; the main `.ui` layout file is protected by a recorded SHA256.
- Original cconfiged and GetController as independent integrated tools.
- Master / Slave configuration release distribution, automatic worker caching,
  multiple backend instances, exact instance startup and shutdown.
- SOAP Describe / Prepare / Configure / Start / Stop, Pause / Resume, automatic
  Run conditions and selected-node progress/regress with original ordering.
- Sentry file counters, measured throughput, disk checks and run catalogs.
- Run journals, warning-only missing backups/archive failures, and explicitly
  confirmed replacement of an existing Run number.
- Offline waveform viewing, specific events/frames and channel selection;
  inspect/merge/matrix tools, with optional FITS/ROOT converters.

Removed product routes:

- Simulation and fake service states/rates.
- Rewritten native ECC, native receiver, supervisor and MCP controller.
- Native scan execution, plan handoff, reconciliation and hardware override UI.
- Rewritten configuration-editor windows and manual native credentials/hashes.
- Direct board-control UI and the old separate-GUI-instance setup workflow.

Retired profiles are diagnosed explicitly and never silently converted into
hardware-control profiles. Source releases containing retired native groups
must have those groups removed or recreated as original backend groups.
Historical run records, raw data, user configurations and the historical
`source/` reference tree are retained. Shared XML/MFM/storage code needed by
current file handling remains available.

`config/product-features.json` records this baseline. Run
`python3 tools/verify-product-boundary.py` to check the supported build graph,
retired entry points and protected main layout. The original backend source
check remains `python3 tools/verify-original-backends.py`.

Native macOS compilation/physical deployment and physical hardware acquisition
still need validation. Linux software-only tests do not replace those checks.
