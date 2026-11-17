import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    'refresh_package_examples', ROOT / 'tools/refresh-package-examples.py')
REFRESH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REFRESH)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class RefreshPackageExamplesTest(unittest.TestCase):
    def test_updates_only_packaged_examples_and_keeps_program(self):
        with tempfile.TemporaryDirectory(prefix='getdaq-package-test-') as parent, \
             mock.patch.object(REFRESH, 'PACKAGES', Path(parent)), \
             tempfile.TemporaryDirectory(prefix='.refresh-examples-test-', dir=parent) as temporary:
            candidate = Path(temporary) / 'candidate'
            candidate.mkdir()
            manifest = {'schema': 'getdaq.host-suite:1', 'status': 'complete',
                        'profile': 'full', 'programs': {'fixture': {}}, 'files': {}}
            executable = candidate / 'bin/fixture'
            executable.parent.mkdir()
            executable.write_bytes(b'fixture executable')
            manifest['files']['bin/fixture'] = sha(executable)
            examples = REFRESH.source_examples()
            for relative, source in examples.items():
                target = candidate / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(b'{"process_name":"dataRouter"}\n'
                                   if relative.endswith('sentry/node.example.json')
                                   else source.read_bytes())
                manifest['files'][relative] = sha(target)
            (candidate / 'manifest.json').write_text(json.dumps(manifest), encoding='utf-8')
            program_hash = sha(executable)

            REFRESH.refresh('ubuntu24', candidate)

            refreshed = json.loads((candidate / 'manifest.json').read_text(encoding='utf-8'))
            config_path = candidate / 'share/config/sentry/node.example.json'
            config = json.loads(config_path.read_text(encoding='utf-8'))
            self.assertEqual(config['process_name'], 'getdaq-receiver')
            self.assertEqual(refreshed['files']['share/config/sentry/node.example.json'],
                             sha(config_path))
            self.assertEqual(refreshed['configuration_examples']['status'],
                             'refreshed-from-current-source')
            self.assertEqual(sha(executable), program_hash)
            self.assertEqual(set(refreshed['files']),
                             {'bin/fixture', *examples.keys()})

    def test_rejects_candidate_outside_build_packages(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaises(ValueError):
                REFRESH.refresh('ubuntu24', Path(temporary))


if __name__ == '__main__':
    unittest.main()
