"""Portable checks for macOS tool recipes; these do not claim native Mac validation."""
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock

ROOT=Path(__file__).resolve().parents[1]
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,ROOT/path)
    result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result
patcher=module('ice_platform','tools/prepare-ice-platform.py')
prepare=module('original_controller','tools/prepare-original-getcontroller.py')
suite=module('suite_macos','tools/build-suite.py')
packager=module('package_macos','tools/package-macos.py')

class MacOSOriginalToolsTests(unittest.TestCase):
    def test_cpu_detection_for_apple_silicon_intel_and_32bit(self):
        compiler=shutil.which('c++')
        if not compiler:self.skipTest('C++ preprocessor required')
        source=ROOT/'third_party/legacy-get/ice/cpp/include/IceUtil/Config.h'
        with tempfile.TemporaryDirectory() as temp:
            dest=Path(temp)/'Config.h';before=source.read_bytes();patcher.patch_config(source,dest)
            stamp=dest.stat().st_mtime_ns;patcher.patch_config(source,dest);self.assertEqual(stamp,dest.stat().st_mtime_ns)
            prefix=dest.read_text().split('// Check for C++ 11 support',1)[0]+'\n#endif\n'
            for cpu,width in [('__aarch64__',8),('__x86_64__',8),('__i386__',4)]:
                args=[compiler,'-E','-dM','-x','c++','-undef','-D__APPLE__','-D'+cpu,
                      '-D__SIZEOF_POINTER__='+str(width),'-D__BYTE_ORDER__=1234','-D__ORDER_LITTLE_ENDIAN__=1234','-']
                output=subprocess.check_output(args,input=prefix,text=True)
                self.assertIn('#define ICE_LITTLE_ENDIAN',output)
                self.assertIn('#define ICE_'+('64' if width==8 else '32'),output)
            self.assertEqual(before,source.read_bytes())

    def test_darwin_recipe_excludes_unavailable_posix_timers_and_linux_libraries(self):
        with tempfile.TemporaryDirectory() as temp:
            base=Path(temp);ice=base/'ice';(ice/'vendor/ice-test/cpp/include').mkdir(parents=True)
            prepare.prepare(base/'build',ice,target_system='Darwin')
            recipe=(base/'build/getcontroller.pri').read_text()
            wrappers='\n'.join(p.read_text() for p in (base/'build/units').glob('*.cpp'))
            for source in ('mdaq/mt/Timer.cpp','mdaq/mt/SigAction.cpp','mdaq/daq/NetPerfProcessor.cpp','get/daq/FrameCounter.cpp'):
                self.assertNotIn(source,wrappers)
            self.assertIn('get/daq/FrameStorage.cpp',wrappers)
            self.assertIn('get/TestController.cpp',wrappers)
            self.assertNotIn('-lrt',recipe);self.assertNotIn('-ldl',recipe)
            inputs=json.loads((base/'build/original-inputs.json').read_text())
            self.assertTrue(all(p.startswith('third_party/legacy-get/') for p in inputs))

    def test_macos_suite_contains_original_controller_and_no_linux_agent(self):
        args=type('Args',(),dict(profile='control',cmake='cmake',qmake='qmake',make='make',jobs=2))()
        with mock.patch.object(suite.platform,'system',return_value='Darwin'):
            plan=suite.plan(args,Path('/tmp/getdaq-macos-plan'))
        programs={name for step in plan for name in step['programs']}
        self.assertIn('getdaq-getcontroller',programs);self.assertIn('getdaq-cconfig-editor',programs)
        self.assertNotIn('getdaq-node-agent',programs)

    def test_packager_refuses_incomplete_build_and_missing_helpers(self):
        for receipt in [{'status':'incomplete'},{'status':'complete','programs':{}}]:
            with self.assertRaises(ValueError):packager.program_plan(receipt)

    def test_macho_minimum_version_uses_binary_metadata(self):
        for text,version in [('Load command 1\ncmd LC_BUILD_VERSION\n minos 11.0\n sdk 14.0\n','11.0'),
                             ('Load command 3\ncmd LC_VERSION_MIN_MACOSX\n version 10.13\n','10.13')]:
            with mock.patch.object(packager.subprocess,'check_output',return_value=text):
                self.assertEqual(packager.minimum_macos(Path('GetDAQ')),version)

if __name__=='__main__':unittest.main()
