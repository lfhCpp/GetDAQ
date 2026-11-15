#!/usr/bin/env python3
"""Verify the bounded macOS QtTest cleanup helper without DAQ-scale data."""
from pathlib import Path
from unittest import mock
import tempfile
import unittest

import cleanup_macos_qttest as cleanup


class MacQtTestCleanupTest(unittest.TestCase):
    def test_empty_private_root_is_removed_and_receipt_is_complete(self):
        with tempfile.TemporaryDirectory(prefix='getdaq-macos-cleanup-') as temporary:
            root = Path(temporary) / 'qt-test-temp'
            root.mkdir()
            receipt_path = Path(temporary) / 'qt-test-cleanup.json'
            receipt = cleanup.cleanup_private_root(root, receipt_path)
            self.assertEqual(receipt['status'], 'complete')
            self.assertTrue(receipt['temporary_root_removed'])
            self.assertEqual(receipt['raw_files_remaining'], 0)
            self.assertTrue(receipt_path.is_file())

    def test_raw_file_is_counted_removed_and_reported_as_failure(self):
        with tempfile.TemporaryDirectory(prefix='getdaq-macos-cleanup-') as temporary:
            root = Path(temporary) / 'qt-test-temp'
            root.mkdir()
            raw = root / 'synthetic.graw'
            raw.write_bytes(b'123')
            receipt = cleanup.cleanup_private_root(root, Path(temporary) / 'qt-test-cleanup.json')
            self.assertEqual(receipt['status'], 'failed')
            self.assertEqual(receipt['raw_files_found_before_cleanup'], 1)
            self.assertEqual(receipt['raw_bytes_found_before_cleanup'], 3)
            self.assertTrue(receipt['temporary_root_removed'])
            self.assertEqual(receipt['raw_files_remaining'], 0)
            self.assertFalse(raw.exists())

    def test_symlink_root_is_rejected_without_removing_target(self):
        with tempfile.TemporaryDirectory(prefix='getdaq-macos-cleanup-') as temporary:
            target = Path(temporary) / 'valuable-directory'
            target.mkdir()
            (target / 'keep.txt').write_text('keep')
            root = Path(temporary) / 'qt-test-temp'
            root.symlink_to(target, target_is_directory=True)
            with self.assertRaisesRegex(RuntimeError, 'symlink'):
                cleanup.cleanup_private_root(root, Path(temporary) / 'qt-test-cleanup.json')
            self.assertTrue((target / 'keep.txt').is_file())

    def test_cli_rejects_symlinked_parent_without_touching_target(self):
        with tempfile.TemporaryDirectory(prefix='getdaq-macos-cleanup-') as temporary:
            workspace = Path(temporary) / 'workspace'
            workspace.mkdir()
            outside = Path(temporary) / 'outside'
            outside.mkdir()
            (outside / 'keep.txt').write_text('keep')
            (workspace / 'build-platform').symlink_to(outside, target_is_directory=True)
            with mock.patch.object(cleanup, 'ROOT', workspace):
                self.assertEqual(cleanup.main(), 2)
            self.assertTrue((outside / 'keep.txt').is_file())


if __name__ == '__main__':
    unittest.main()
