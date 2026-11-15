#!/usr/bin/env bash
# 主机套件共用统一入口；保留单独算法/兼容库的构建方式。
set -euo pipefail
getdaq_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
getdaq_jobs="${GETDAQ_JOBS:-4}"
case "${1:-all}" in
  all|control|data|supervisor|ecc|experiment|ice) getdaq_component="${1:-all}" ;;
  *) echo 'Usage: bash tools/build-linux.sh [all|control|data|supervisor|ecc|experiment|ice]' >&2; exit 2 ;;
esac
case "$getdaq_component" in
  all) getdaq_profile=full ;;
  control) getdaq_profile=control ;;
  data) getdaq_profile=data ;;
  supervisor) getdaq_profile=data-host ;;
  *) getdaq_profile= ;;
esac
if [[ -n "$getdaq_profile" ]]; then
  exec python3 "$getdaq_root/tools/build-suite.py" --profile "$getdaq_profile" --jobs "$getdaq_jobs"
fi
if [[ "$getdaq_component" == experiment ]]; then
  cmake -S "$getdaq_root/common/experiment" -B "$getdaq_root/build-experiment" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$getdaq_root/build-experiment" --parallel "$getdaq_jobs"
fi
if [[ "$getdaq_component" == ecc ]]; then
  cmake -S "$getdaq_root/apps/ecc" -B "$getdaq_root/build-ecc" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$getdaq_root/build-ecc" --parallel "$getdaq_jobs"
fi
if [[ "$getdaq_component" == ice ]]; then
  cmake -S "$getdaq_root/third_party/ice-compat" -B "$getdaq_root/build-ice-compat" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$getdaq_root/build-ice-compat" --parallel "$getdaq_jobs"
fi
