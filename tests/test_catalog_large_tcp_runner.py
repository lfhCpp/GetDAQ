#!/usr/bin/env python3
"""Verify the opt-in large catalog transfer runner's budget and failure cleanup."""
import importlib.util
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    'catalog_large_tcp_runner', ROOT / 'tests/run_catalog_large_tcp_test.py')
RUNNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUNNER)


def arguments(root, loader, **overrides):
    values = {
        'loader': loader,
        'library_path': '/lib:/usr/lib',
        'transfer_tests': Path(__file__),
        'catalog': Path(__file__),
        'evidence': root / 'evidence.json',
        'candidate': 'runner-test',
        'runtime': 'runner test runtime',
        'file_bytes': 4096,
        'timeout_seconds': 1,
        'rss_limit_kib': 256 * 1024,
    }
    values.update(overrides)
    return type('Arguments', (), values)()


class CatalogLargeTcpRunnerTest(unittest.TestCase):
    def test_aggregate_over_budget_is_rejected_before_setup(self):
        with tempfile.TemporaryDirectory(prefix='getdaq-large-budget-test-') as directory:
            root = Path(directory)
            created = []
            real_mkdtemp = tempfile.mkdtemp

            def record_creation(*args, **kwargs):
                path = Path(real_mkdtemp(*args, dir=directory, **kwargs))
                created.append(path)
                return str(path)

            with mock.patch.object(RUNNER.tempfile, 'mkdtemp', side_effect=record_creation):
                with self.assertRaisesRegex(RuntimeError, '16 GiB test budget'):
                    RUNNER.run(arguments(root, sys.executable, file_bytes=5 * 1024 ** 3))

            self.assertEqual(created, [])
            self.assertFalse((root / 'evidence.json').exists())
            self.assertEqual(list(root.iterdir()), [])

    def test_timeout_terminates_process_group_and_cleans_private_tree(self):
        with tempfile.TemporaryDirectory(prefix='getdaq-large-timeout-test-') as directory:
            root = Path(directory)
            marker = root / 'child-cleaned'
            pid_file = root / 'child.pid'
            fake_loader = root / 'fake-loader'
            fake_loader.write_text(
                '#!/bin/sh\n'
                'sleep 60 &\n'
                'child=$!\n'
                'echo "$child" > "'+str(pid_file)+'"\n'
                'trap \'kill "$child" 2>/dev/null || true; wait "$child" 2>/dev/null || true; '
                'echo cleaned > "'+str(marker)+'"; exit 0\' TERM\n'
                'wait "$child"\n', encoding='utf-8')
            fake_loader.chmod(0o700)
            created = []
            real_mkdtemp = tempfile.mkdtemp

            def record_creation(*args, **kwargs):
                path = Path(real_mkdtemp(*args, dir=directory, **kwargs))
                created.append(path)
                return str(path)

            with mock.patch.object(RUNNER.tempfile, 'mkdtemp', side_effect=record_creation):
                with self.assertRaisesRegex(RuntimeError, 'process group was terminated'):
                    RUNNER.run(arguments(root, fake_loader))

            self.assertEqual(len(created), 1)
            self.assertFalse(created[0].exists())
            self.assertFalse((root / 'evidence.json').exists())
            self.assertTrue(marker.is_file())
            pid = int(pid_file.read_text(encoding='utf-8'))
            with self.assertRaises(ProcessLookupError):
                os.kill(pid, 0)


if __name__ == '__main__':
    unittest.main()
