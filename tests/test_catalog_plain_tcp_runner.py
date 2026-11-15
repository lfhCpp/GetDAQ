#!/usr/bin/env python3
"""Verify catalog transfer runner cleanup when its second private root fails."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    'catalog_plain_tcp_runner', ROOT / 'tests/run_catalog_plain_tcp_tests.py')
RUNNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUNNER)


class CatalogPlainTcpRunnerCleanupTest(unittest.TestCase):
    def test_first_temporary_root_removed_if_shared_root_creation_fails(self):
        with tempfile.TemporaryDirectory(prefix='getdaq-catalog-runner-cleanup-test-') as outer:
            outer_path = Path(outer)
            evidence = outer_path / 'evidence' / 'result.json'
            created_roots = []
            original_mkdtemp = tempfile.mkdtemp

            def fail_shared_root(prefix, dir=None):
                if dir == '/dev/shm':
                    raise PermissionError('injected shared temporary-root failure')
                created = Path(original_mkdtemp(prefix=prefix, dir=outer))
                created_roots.append(created)
                return str(created)

            arguments = type('Arguments', (), {
                'max_test_bytes': RUNNER.MAX_BYTES,
                'loader': sys.executable,
                'library_path': '',
                'catalog': sys.executable,
                'system_tests': sys.executable,
                'transfer_tests': sys.executable,
                'evidence': evidence,
                'candidate': 'cleanup-test',
                'runtime': 'test runtime',
            })()
            with mock.patch.object(RUNNER.tempfile, 'mkdtemp', side_effect=fail_shared_root):
                with self.assertRaisesRegex(PermissionError, 'injected shared temporary-root failure'):
                    RUNNER.run(arguments)

            self.assertEqual(len(created_roots), 1)
            self.assertFalse(created_roots[0].exists())
            self.assertFalse(evidence.exists())
            self.assertFalse(list(outer_path.glob('getdaq-catalog-tcp-test-*')))

    def test_existing_evidence_is_preserved(self):
        with tempfile.TemporaryDirectory(prefix='getdaq-catalog-evidence-test-') as outer:
            evidence = Path(outer) / 'existing.json'
            evidence.write_text('{"keep":true}\n', encoding='utf-8')
            arguments = type('Arguments', (), {
                'max_test_bytes': RUNNER.MAX_BYTES,
                'loader': sys.executable,
                'library_path': '',
                'catalog': sys.executable,
                'system_tests': sys.executable,
                'transfer_tests': sys.executable,
                'evidence': evidence,
                'candidate': 'preserve-test',
                'runtime': 'test runtime',
            })()

            with self.assertRaisesRegex(FileExistsError, 'Refusing to overwrite'):
                RUNNER.run(arguments)
            self.assertEqual(evidence.read_text(encoding='utf-8'), '{"keep":true}\n')


if __name__ == '__main__':
    unittest.main()
