#!/bin/bash
set -e
getdaq_root="$(cd "$(dirname "$0")" && pwd)"
getdaq_source="$getdaq_root"
if [[ -f "$getdaq_root/code/tools/build-macos.py" ]]; then
    getdaq_source="$getdaq_root/code"
fi
if [[ "$(uname -s)" != Darwin ]]; then
    echo 'Run this command on the Mac after copying the software package.' >&2
    exit 2
fi
exec python3 "$getdaq_source/tools/build-macos.py" \
    --build-root "$getdaq_root/build-macos" \
    --output "$getdaq_root/dist-macos" "$@"
