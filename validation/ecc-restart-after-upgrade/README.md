# Original ECC restart after runtime update

The reported failure had two independent causes: unchanged experiment release
IDs reused immutable runtime files even after program-directory/observation
changes; inspection required the old program directory to equal the new agent
installation. A terminated ECC then could not answer a normal Stop state query.

Each accepted prepare now writes a separate immutable runtime recipe and keeps a
latest pointer; new activation requests bind the preparation ID. Past files are
retained and stale recipe IDs fail before process startup. Inspection checks the
registered original program hash, stored recipe and absence of exact processes
using its original preserved installation, while still requiring explicit
hardware-stop/data-preservation confirmation. Normal Stop on an already
terminated record explains the Review terminated group step without querying an
exited server. No absent process is treated as proof of hardware Idle.

32 product tests, 14 unified project checks and 8 old-protocol deployment checks
pass. The new integration simulates an agent runtime-directory update, releases
an inspected old original ECC, prepares unchanged content repeatedly without
overwriting the audit, rejects stale activation, and starts/stops/restarts the
real SOAP server at Idle in the new directory. Synthetic data and loopback
services only; no physical board acquisition or hardware commands were used.

Existing target instance journals were preserved. Current operator inspection
is still required for its terminated group; the update does not auto-release
hardware uncertainty or replay prior failed/unknown operations.
