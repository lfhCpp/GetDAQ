#!/usr/bin/env python3
"""Remove the macOS QtTest private temp tree and persist a bounded cleanup receipt."""
import json
from pathlib import Path
import shutil
import stat
import sys

ROOT = Path(__file__).resolve().parents[1]
RAW_LIMIT_BYTES = 20 * 1024 ** 3


def cleanup_private_root(root, receipt_path, limit_bytes=RAW_LIMIT_BYTES):
    root = Path(root)
    receipt_path = Path(receipt_path)
    if root.is_symlink():
        raise RuntimeError('Refusing to remove a symlink as the QtTest temporary root')
    if root.exists() and not root.is_dir():
        raise RuntimeError('QtTest temporary root is not a directory')

    raw = [path for path in root.rglob('*.graw')
           if path.is_file() or path.is_symlink()] if root.exists() else []
    raw_bytes = sum(path.lstat().st_size for path in raw if not path.is_symlink())
    if root.exists():
        shutil.rmtree(root)
    remaining = [path for path in root.rglob('*.graw')
                 if path.is_file() or path.is_symlink()] if root.exists() else []
    receipt = {
        'status': 'complete' if raw_bytes <= limit_bytes and not raw and not root.exists() else 'failed',
        'raw_files_found_before_cleanup': len(raw),
        'raw_bytes_found_before_cleanup': raw_bytes,
        'raw_limit_bytes': limit_bytes,
        'temporary_root_removed': not root.exists(),
        'raw_files_remaining': len(remaining),
    }
    receipt_path.parent.mkdir(parents=True, exist_ok=True)
    temporary_receipt = receipt_path.with_name(receipt_path.name + '.tmp')
    temporary_receipt.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    temporary_receipt.replace(receipt_path)
    return receipt


def validate_private_parent(root):
    root = Path(root)
    relative = root.absolute().relative_to(ROOT.absolute())
    current = ROOT
    for part in relative.parts[:-1]:
        current = current / part
        try:
            mode = current.lstat().st_mode
        except FileNotFoundError:
            continue
        if stat.S_ISLNK(mode) or not stat.S_ISDIR(mode):
            raise RuntimeError('Refusing QtTest cleanup through a symlink or non-directory parent')


def main():
    root = ROOT / 'build-platform/macos-full-suite/qt-test-temp'
    receipt_path = ROOT / 'build-platform/macos-full-suite/qt-test-cleanup.json'
    try:
        validate_private_parent(root)
        receipt = cleanup_private_root(root, receipt_path)
    except Exception as error:
        print('macOS QtTest cleanup failed:', error, file=sys.stderr)
        return 2
    print(json.dumps(receipt, sort_keys=True))
    return 0 if receipt['status'] == 'complete' else 1


if __name__ == '__main__':
    sys.exit(main())
