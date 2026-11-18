# Read-only legacy prepared.json migration

The first per-operation recipe fix still attempted to replace the legacy
`prepared.json`, which had been frozen 0400. QSaveFile refused to open that
existing read-only file despite a writable parent directory. Fresh-install
restart tests did not reproduce this upgrade-only permission condition.

The latest mutable selector is now `current-preparation.json`. A legacy
`prepared.json` remains unchanged and is read only as a fallback. Both activation
dispatch and recipe loading prefer the new selector. No chmod or audit deletion
is used. Inspection/stop responses no longer print a saved old startup command
as though it were being executed.

14 integration checks pass with a 0700 legacy directory and explicit frozen
0400 pointer seeded before update, including byte/mode preservation, repeated
prepare, stale-recipe rejection and original ECC restart. 8 legacy protocol
checks and 32 product regressions pass.

On the user's already inspection-released target group, the previously requested
service startup was completed using its existing project, source, ports and data
folder. Fresh original ECC status returned Idle, receiver listening, and Sentry
HTTP 200. Legacy files retained identical hashes and permissions. No hardware
Describe/Prepare/Configure/Start or acquisition was sent. Private state, logs,
credentials and process identifiers are excluded.
