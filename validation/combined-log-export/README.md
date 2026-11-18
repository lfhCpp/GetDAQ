# Combined TXT log snapshots and Clear

32 Qt product tests and 11 real-original-backend project checks pass. The new
log-panel probe keeps Event selected while receiving ECC, Data and shared Sentry
logs from two authenticated agents; it exports a merged snapshot after Clear and
verifies Event history remains. Full private project trees and credentials are
not included. CI builds this probe and runs it with project integration.

The GUI serializes immutable read results into a bounded session timeline. It
orders local observation UTC plus sequence, assembles fragments, preserves
rotation/partial-line facts and atomically saves a frozen snapshot. Native text
is retained; untimestamped backend output is not given invented source-event
times. Initial reads are tails, and snapshot headers disclose collection scope,
budget/discard count and pending reads. Clear is display-only and does not reset
read cursors or delete raw/audit/export history; in-flight responses cannot undo
a Clear. Reads latch a fixed file-size boundary rather than chasing appends.

Configuration transfer remains explicit: saving Nodes writes YAML settings.
Use project Sync or service startup to publish and distribute configurations.
No physical board commands or live acquisition restarts were used.
