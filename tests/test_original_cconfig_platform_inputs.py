#!/usr/bin/env python3
"""Platform headers must survive cconfig's project-local source preparation."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('original_cconfig',ROOT/'tools/prepare-original-cconfig.py')
prepare=importlib.util.module_from_spec(spec);spec.loader.exec_module(prepare)

class PlatformInputs(unittest.TestCase):
    def test_platform_headers_are_verified_and_refreshed_by_qmake_preparation(self):
        vendor=ROOT/'third_party/legacy-get'
        manifest=json.loads((vendor/'SOURCE_MANIFEST.json').read_text())['files']
        with tempfile.TemporaryDirectory(prefix='getdaq-cconfig-platforms-') as directory:
            output=Path(directory)
            prepare.prepare(output)
            for platform in ['win32','linux','macos']:
                relative='boost_1_55_0/boost/config/platform/'+platform+'.hpp'
                self.assertIn(relative,manifest)
                original=(vendor/relative).read_bytes()
                self.assertEqual(hashlib.sha256(original).hexdigest(),manifest[relative]['sha256'])
                self.assertEqual((output/relative).read_bytes(),original)
            # Existing Qt Creator shadow builds must receive newly added headers
            # when qmake is run again, without requiring a separate Boost install.
            windows=output/'boost_1_55_0/boost/config/platform/win32.hpp'
            windows.unlink()
            prepare.prepare(output)
            self.assertEqual(windows.read_bytes(),(vendor/windows.relative_to(output)).read_bytes())
            inputs=json.loads((output/'original-inputs.json').read_text())
            key='third_party/legacy-get/boost_1_55_0/boost/config/platform/win32.hpp'
            self.assertEqual(inputs[key],hashlib.sha256(windows.read_bytes()).hexdigest())

if __name__=='__main__':unittest.main()
