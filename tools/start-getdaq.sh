#!/usr/bin/env bash
set -euo pipefail
launcher_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ -d "$launcher_dir/site" ]]; then
    getdaq_root="$launcher_dir"
else
    getdaq_root="$(cd -- "$launcher_dir/.." && pwd)"
fi
cd -- "$getdaq_root"
# Compatibility only; GetDAQ also reads the saved site JSON when opened directly.
if [[ -f site/network.env ]]; then source site/network.env; fi
getdaq_runtime="$(<site/gui-runtime.txt)"
if [[ ! "$getdaq_runtime" =~ ^runtime-[A-Za-z0-9._-]+$ ]]; then
    echo 'Invalid GUI runtime selection in site/gui-runtime.txt' >&2
    exit 1
fi
echo 'Starting GetDAQ…' >&2
exec "$getdaq_root/$getdaq_runtime/bin/GetDAQ" "$@"
