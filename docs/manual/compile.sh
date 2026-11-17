#!/usr/bin/env bash
set -euo pipefail
getdaq_manual_dir="$(cd "$(dirname "$0")" && pwd)"
getdaq_manual_build="$(mktemp -d "${TMPDIR:-/tmp}/getdaq-manual.XXXXXX")"
trap 'rm -rf -- "$getdaq_manual_build"' EXIT
if command -v xelatex >/dev/null 2>&1; then
    getdaq_manual_engine=(xelatex)
elif command -v xetex >/dev/null 2>&1; then
    (cd "$getdaq_manual_build" && xetex -ini -etex -jobname=xelatex -interaction=nonstopmode '\input latex.ltx') >"$getdaq_manual_build/format.log" 2>&1
    getdaq_manual_engine=(xetex "-fmt=$getdaq_manual_build/xelatex.fmt")
else
    echo 'XeLaTeX / XeTeX is required.' >&2
    exit 1
fi
for getdaq_manual_pass in 1 2; do
    if ! (cd "$getdaq_manual_dir" && "${getdaq_manual_engine[@]}" -interaction=nonstopmode -halt-on-error "-output-directory=$getdaq_manual_build" GetDAQ-Quick-Guide.tex) >"$getdaq_manual_build/compile.log" 2>&1; then
        tail -60 "$getdaq_manual_build/compile.log" >&2
        exit 1
    fi
done
cp "$getdaq_manual_build/GetDAQ-Quick-Guide.pdf" "$getdaq_manual_dir/GetDAQ-Quick-Guide.pdf"
echo "PDF generated: $getdaq_manual_dir/GetDAQ-Quick-Guide.pdf"
