#!/usr/bin/env python3
"""Run bounded cold-audit metadata tests in an automatically removed root."""
import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: run_remote_cold_evidence_test.py PATH_TO_TEST_BINARY")
    binary = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="getdaq-remote-cold-audit-") as directory:
        root = Path(directory) / "fixtures"
        root.mkdir(mode=0o700)
        subprocess.run([binary, str(root)], check=True, timeout=30)


if __name__ == "__main__":
    main()
