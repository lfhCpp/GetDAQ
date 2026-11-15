#!/usr/bin/env python3
"""Failure-path checks for offline source preflight; no build SDK or network."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('dependencies',ROOT/'tools/verify-dependencies.py')
deps=importlib.util.module_from_spec(spec);spec.loader.exec_module(deps)


class DependencyVerificationTests(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory(prefix='getdaq-dependencies-')
        self.addCleanup(self.temporary.cleanup)
        self.root=Path(self.temporary.name);self.vendor=self.root/'third_party';self.vendor.mkdir()
        self.yaml=self.vendor/'yaml-cpp';self.yaml.mkdir()
        sums=[]
        for name in ['LICENSE','include/yaml-cpp/yaml.h','src/parse.cpp']:
            file=self.yaml/name;file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes(name.encode())
            sums.append(hashlib.sha256(file.read_bytes()).hexdigest()+'  '+name+'\n')
        (self.yaml/'SHA256SUMS').write_text(''.join(sums))
        archive=self.vendor/'archives';archive.mkdir();records=[];sums=[]
        for i,name in enumerate(deps.ARCHIVES):
            payload=('archive-'+str(i)).encode();(archive/name).write_bytes(payload)
            digest=hashlib.sha256(payload).hexdigest();license_name='licenses/'+str(i)+'/LICENSE'
            license_file=self.vendor/license_name;license_file.parent.mkdir(parents=True);license_file.write_text('license')
            records.append(dict(local_path='archives/'+name,size_bytes=len(payload),sha256=digest,license_file=license_name))
            sums.append(digest+'  '+name+'\n')
        (archive/'SHA256SUMS').write_text(''.join(sums))
        (self.vendor/'licenses/getbench').mkdir();(self.vendor/'licenses/getbench/COPYING').write_text('license')
        (self.vendor/'ATTPC-LICENSE').write_text('license')
        self.manifest=dict(schema_version=1,dependencies=records);self.write_manifest()

    def write_manifest(self):
        (self.vendor/'dependencies.json').write_text(json.dumps(self.manifest))

    def test_complete_source_without_board_dependency(self):
        value=deps.verify(self.root,'full')
        self.assertTrue(value['passed']);self.assertFalse(value['network_used']);self.assertFalse(value['board_services_required'])
        self.assertIn('third_party/archives/'+deps.ARCHIVES[0],value['checked_files'])

    def test_corrupt_archive_fails_even_at_same_size(self):
        p=self.vendor/'archives'/deps.ARCHIVES[0];p.write_bytes(b'x'*p.stat().st_size)
        with self.assertRaisesRegex(ValueError,'checksum mismatch'):deps.verify(self.root,'full')

    def test_missing_archive_manifest_entry_fails(self):
        self.manifest['dependencies'].pop();self.write_manifest()
        with self.assertRaisesRegex(ValueError,'Missing archive manifest'):deps.verify(self.root,'full')

    def test_missing_license_fails(self):
        (self.vendor/self.manifest['dependencies'][0]['license_file']).unlink()
        with self.assertRaisesRegex(ValueError,'Missing embedded'):deps.verify(self.root,'full')

    def test_corrupt_yaml_fails(self):
        (self.yaml/'src/parse.cpp').write_text('altered source')
        with self.assertRaisesRegex(ValueError,'yaml-cpp checksum mismatch'):deps.verify(self.root,'control')

    def test_path_traversal_fails(self):
        p=self.yaml/'SHA256SUMS';p.write_text(p.read_text()+hashlib.sha256(b'outside').hexdigest()+'  ../outside\n')
        with self.assertRaisesRegex(ValueError,'Invalid embedded dependency path'):deps.verify(self.root,'control')

    def test_symlink_instead_of_source_fails(self):
        p=self.yaml/'src/parse.cpp';p.unlink()
        try:p.symlink_to(self.yaml/'LICENSE')
        except (OSError,NotImplementedError):self.skipTest('Symlinks unavailable')
        with self.assertRaisesRegex(ValueError,'Dependency symlink'):deps.verify(self.root,'control')

    def test_duplicate_checksum_fails(self):
        p=self.yaml/'SHA256SUMS';p.write_text(p.read_text()+p.read_text().splitlines()[0]+'\n')
        with self.assertRaisesRegex(ValueError,'duplicate checksum'):deps.verify(self.root,'control')

    def test_unreviewed_yaml_file_fails(self):
        (self.yaml/'src/unreviewed.cpp').write_text('extra source')
        with self.assertRaisesRegex(ValueError,'inventory differs'):deps.verify(self.root,'control')

    def test_build_rejects_before_sdk_and_output_creation(self):
        spec=importlib.util.spec_from_file_location('suite',ROOT/'tools/build-suite.py')
        suite=importlib.util.module_from_spec(spec);spec.loader.exec_module(suite)
        output=self.root/'not-created'
        with mock.patch.object(suite,'verify_dependencies',side_effect=ValueError('bad archive')), \
             mock.patch.object(suite,'tool_output') as sdk, \
             mock.patch('sys.argv',['build-suite.py','--profile','full','--build-root',str(output)]):
            with self.assertRaisesRegex(ValueError,'bad archive'):suite.main()
        sdk.assert_not_called();self.assertFalse(output.exists())

    def test_control_does_not_require_ice(self):
        (self.vendor/'archives'/deps.ARCHIVES[0]).unlink()
        self.assertTrue(deps.verify(self.root,'control')['passed'])

    def test_data_does_not_require_any_third_party_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            value=deps.verify(Path(directory),'data')
            self.assertEqual(value['checked_files'],{})


if __name__=='__main__':unittest.main()
