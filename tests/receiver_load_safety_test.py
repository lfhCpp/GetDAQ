#!/usr/bin/env python3
"""Check load-test disk budgets and cleanup after failures/interruption.

The fake receiver writes only 4 KiB; no hardware or TCP listener is required.
"""
import json
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock


SCRIPT = Path(__file__).with_name('receiver_sustained_load.py').resolve()
SPEC = importlib.util.spec_from_file_location('receiver_sustained_load', SCRIPT)
LOAD_TEST = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(LOAD_TEST)


class LoadSafetyTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='getdaq-load-safety-')
        self.root = Path(self.temporary.name)
        self.evidence = self.root / 'evidence'
        self.receiver = self.root / 'fake-receiver'
        self.receiver.write_text(
            '#!/usr/bin/env python3\n'
            'import pathlib, sys, time\n'
            "folder = pathlib.Path(sys.argv[sys.argv.index('--output') + 1])\n"
            "(folder / 'synthetic_0.graw').write_bytes(b'x' * 4096)\n"
            "if (pathlib.Path(__file__).parent / 'sleep').exists(): time.sleep(60)\n"
            'sys.exit(7)\n')
        self.receiver.chmod(0o700)
        self.loader = self.root / 'fake-loader'
        self.loader.write_text(
            '#!/usr/bin/env python3\n'
            'import os, pathlib, sys\n'
            "assert len(sys.argv) >= 4 and sys.argv[1] == '--library-path'\n"
            'assert pathlib.Path(sys.argv[2]).is_dir()\n'
            "os.execv(sys.argv[3], sys.argv[3:])\n")
        self.loader.chmod(0o700)
        self.children = []

    def tearDown(self):
        for child in self.children:
            if child.poll() is None: child.terminate()
            try: child.communicate(timeout=15)
            except subprocess.TimeoutExpired: child.kill();child.communicate()
        self.temporary.cleanup()

    def command(self, *options):
        return [sys.executable, str(SCRIPT), str(self.receiver),
                '--evidence', str(self.evidence), *options]

    def test_concurrency_lock_uses_writable_temp_not_source_tree(self):
        temp_parent = Path(tempfile.gettempdir())
        lock_path = LOAD_TEST.concurrency_lock_path()
        self.assertEqual(lock_path.parent, temp_parent)
        self.assertNotEqual(lock_path.parent, SCRIPT.parent)
        self.assertEqual(lock_path.name, 'getdaq-receiver-load-' + str(os.getuid()) + '-' +
                         LOAD_TEST.hashlib.sha256(os.fsencode(str(SCRIPT.parents[1]))).hexdigest()[:16] + '.lock')

    def test_concurrency_lock_rejects_symlink_without_touching_target(self):
        target = self.root / 'lock-target'
        target.write_bytes(b'leave this file unchanged')
        with mock.patch.object(LOAD_TEST.tempfile, 'gettempdir', return_value=str(self.root)):
            lock_path = LOAD_TEST.concurrency_lock_path()
            lock_path.symlink_to(target)
            with self.assertRaises(RuntimeError):
                LOAD_TEST.acquire_concurrency_lock()
        self.assertEqual(target.read_bytes(), b'leave this file unchanged')
        self.assertTrue(lock_path.is_symlink())

    def test_concurrency_lock_rejects_hardlink_without_touching_target(self):
        target = self.root / 'lock-target'
        target.write_bytes(b'leave this file unchanged')
        with mock.patch.object(LOAD_TEST.tempfile, 'gettempdir', return_value=str(self.root)):
            lock_path = LOAD_TEST.concurrency_lock_path()
            os.link(target, lock_path)
            with self.assertRaisesRegex(RuntimeError, 'link count'):
                LOAD_TEST.acquire_concurrency_lock()
        self.assertEqual(target.read_bytes(), b'leave this file unchanged')
        self.assertEqual(target.stat().st_nlink, 2)

    def test_concurrency_lock_is_private_and_exclusive(self):
        with mock.patch.object(LOAD_TEST.tempfile, 'gettempdir', return_value=str(self.root)):
            first = LOAD_TEST.acquire_concurrency_lock()
            try:
                self.assertEqual(LOAD_TEST.concurrency_lock_path().stat().st_mode & 0o777, 0o600)
                with self.assertRaises(BlockingIOError):
                    LOAD_TEST.acquire_concurrency_lock()
            finally:
                first.close()

    def test_aggregate_limit_rejected_before_directory_or_process(self):
        result = subprocess.run(self.command('--streams', '2', '--mib-per-stream', '10241'),
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 2)
        self.assertIn('ALL streams', result.stderr)
        self.assertFalse(self.evidence.exists())

    def assert_cleaned(self):
        receipts = list(self.evidence.glob('receiver-load-*/payload-cleanup.json'))
        self.assertEqual(len(receipts), 1)
        receipt = json.loads(receipts[0].read_text())
        self.assertEqual(receipt['deleted_bytes'], 4096)
        self.assertFalse(list(self.evidence.rglob('*.graw')))
        self.assertFalse(list(self.evidence.rglob('result.json')))

    def test_startup_failure_removes_partial_payload(self):
        result = subprocess.run(self.command('--streams', '1', '--mib-per-stream', '1'),
                                capture_output=True, text=True, timeout=15)
        self.assertNotEqual(result.returncode, 0)
        self.assert_cleaned()

    def test_optional_target_loader_launches_candidate_and_keeps_cleanup(self):
        result = subprocess.run(self.command('--streams', '1', '--mib-per-stream', '1',
                                             '--loader', str(self.loader), '--library-path', str(self.root)),
                                capture_output=True, text=True, timeout=15)
        self.assertNotEqual(result.returncode, 0)
        self.assert_cleaned()

    def test_loader_and_library_path_must_be_paired(self):
        result = subprocess.run(self.command('--streams', '1', '--mib-per-stream', '1',
                                             '--loader', str(self.loader)),
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 2)
        self.assertIn('must be supplied together', result.stderr)
        self.assertFalse(self.evidence.exists())

    def test_interrupt_cleanup_and_concurrent_budget_guard(self):
        (self.root / 'sleep').touch()
        child = subprocess.Popen(self.command('--streams', '1', '--mib-per-stream', '1'),
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.children.append(child)
        deadline = time.monotonic() + 5
        while not list(self.evidence.glob('receiver-load-*/stream-*/*.graw')):
            if child.poll() is not None: self.fail(child.communicate())
            if time.monotonic() > deadline: self.fail('fake receiver did not create payload')
            time.sleep(0.02)
        concurrent = subprocess.run(self.command('--streams', '1', '--mib-per-stream', '1'),
                                    capture_output=True, text=True, timeout=10)
        self.assertEqual(concurrent.returncode, 2)
        self.assertIn('run tests sequentially', concurrent.stderr)
        child.terminate()
        child.communicate(timeout=15)
        self.assertNotEqual(child.returncode, 0)
        self.assert_cleaned()


if __name__ == '__main__':
    unittest.main()
