# Pulser amplitude scan plan

`getdaq-config plan-pulser-scan` creates a bounded set of complete system experiment artifacts for a pulser amplitude scan. It is an offline configuration planner. It does not contact an ECC server or board, write registers, start a run, receive data, or move `.graw` files. The Qt configuration application remains outside the high-rate data path.

## Command

First compile a complete system artifact using the normal profile workflow, then run:

```text
getdaq-config compile-system experiment.xcfg profile.json system-experiment.json
getdaq-config plan-pulser-scan system-experiment.json C0 -1000 0 500 pulser-plan
```

The command arguments after the artifact are the exact CoBo ID, start voltage, stop voltage, positive step in integer millivolts, and a new output directory. The target must be an enabled CoBo in the artifact. Its existing `Generator/amplitudeStart` must equal the requested start voltage. The range is ascending and inclusive, must land exactly on the stop voltage, stays within -1000 through +1000 mV, and contains no more than 256 points. Invalid input is rejected before publishing output.

The output directory must not already exist and its parent must exist. The planner writes into a private temporary directory under that parent and publishes the completed directory as one operation. Each point is a complete immutable system artifact. Only the selected CoBo's `Generator/amplitudeStart` changes; other CoBos and settings remain the same. Configuration and artifact SHA-256 values are recalculated and recorded in the manifest.

```text
pulser-plan/
  scan-manifest.json
  point-000.json
  point-001.json
  ...
```

The manifest marks hardware loading as unverified, requires a separate full Configure for every point, and leaves acquisition event count unspecified. Review every generated artifact before use.

## Relationship to old GetController code

The old GetController Gain/AutoGain branches manipulate pulser amplitude while running a measurement loop. This planner migrates only the configuration-point generation part. It deliberately makes the requested endpoint explicit and inclusive, and does not reproduce the old loop's implicit stop/step behavior. It is not an automatic gain-finding routine: no event collection, gain calculation, threshold selection, stop criterion, result table, or recovery behavior is implemented here. Those need a separately reviewed acquisition workflow before they can be automated safely.

For each point, an operator must use the supported ECC state flow and associate a distinct acquisition run and archive with that exact artifact. Do not hot-change a running run or hand-edit the generated JSON. Hardware readback, external pulser calibration, actual voltage accuracy, and analysis results are not proven by a configuration digest.

After the corresponding receiver catalog is complete, `getdaq-catalog --bind-scan-point` can verify the selected point's two digests against the catalog plan and write an independent Run/archive association receipt. It never starts acquisition or changes the catalog; see [Scan point to Run archive binding](SCAN_RUN_BINDING.md).

## Validation scope

The C++11 core tests cover explicit-ID selection when the selected CoBo is not first, the exact point sequence, unchanged non-target settings, digest changes, and rejection of disabled/missing boards, mismatched start, non-divisible ranges, descending ranges, out-of-range values, and plans over 256 points. The planner and CLI do not create test DAQ data. A successful unit test is not a hardware, Windows, macOS, or acquisition-throughput acceptance test.
