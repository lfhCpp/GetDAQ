# Unified project verification

Build the full 14-program suite with `tools/build-suite.py --profile full`.
Run Qt product tests, original-backends payload/integration tests, and
`tests/unified_project_integration.py` with the controller probe built from
`tests/unified_project_controller_probe.pro`.

Project integration runs two real authenticated HTTPS node agents on one Linux
host, both original receiver implementations and synthetic MFM data. It verifies
shared working directories/Sentry, whole-folder delivery, configuration staging,
immutable snapshots, exact process reuse/health and bounded authenticated logs.
This is not physical multi-host or board acceptance. The separately updated
target Ubuntu host built all programs and preserved its manager/repository IDs.
Main `.ui` SHA remains unchanged; the existing log header gains three buttons.
Private credentials, project files, process journals and integration trees are
not included here.
