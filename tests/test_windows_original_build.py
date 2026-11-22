"""Portable preparation checks; native Windows acceptance is a separate test."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest import mock
ROOT=Path(__file__).resolve().parents[1]
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,ROOT/path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
original=module('original_windows_build','tools/build-original-backends.py')
platform=module('original_windows_platform','tools/prepare-original-windows.py')
class WindowsOriginalBuild(unittest.TestCase):
    def test_original_sources_unchanged_and_network_overlay_is_selected(self):
        vendor=ROOT/'third_party/legacy-get';source=vendor/'mdaq/src/mdaq/utl/net/TcpConnection.cpp';before=source.read_bytes()
        with tempfile.TemporaryDirectory() as temp:
            output=Path(temp);sources=platform.prepare(vendor,output,[source])
            self.assertEqual(sources,[output/source.relative_to(vendor)])
            self.assertIn('SOCKET a_bsd_socket',sources[0].read_text())
            self.assertIn('typedef u_long in_addr_t;',(output/'utilities/src/utl/net/IpAddress.h').read_text())
            self.assertIn('GetAdaptersAddresses',(output/'mdaq/src/mdaq/utl/net/IpDetect.cpp').read_text())
            self.assertIn('std::replace(fileTimestamp',(output/'getbench/src/get/daq/FrameStorage.cpp').read_text())
        self.assertEqual(source.read_bytes(),before)
    def test_windows_recipe_uses_native_threads_sockets_and_filesystem(self):
        with tempfile.TemporaryDirectory() as temp:
            build=Path(temp)/'original';ice=Path(temp)/'ice';(ice/'vendor/ice-local/cpp/include').mkdir(parents=True)
            with mock.patch.object(original,'run') as execute:original.prepare(build,ice,windows=True)
            text=(build/'CMakeLists.txt').read_text()
            self.assertIn('libs/thread/src/win32/',text);self.assertNotIn('libs/thread/src/pthread/',text)
            self.assertIn('windows_file_codecvt.cpp',text);self.assertIn('ws2_32 mswsock iphlpapi rpcrt4',text)
            self.assertIn('windows-platform/getbench/src/get/soap/EccSoapServer.cpp',text)
            self.assertIn('SIGBREAK',(build/'getEccSoapServer.cpp').read_text());self.assertIn('SIGBREAK',(build/'DataExporter-main.cpp').read_text())
            self.assertNotIn('-DLINUX',execute.call_args_list[1].args[0])
    def test_opt_in_qt_creator_entry_does_not_replace_default_gui_build(self):
        project=(ROOT/'GetDAQ.pro').read_text();self.assertIn('console.file = apps/envoy/envoy.pro',project);self.assertIn('win32:equals(GETDAQ_BUILD_BACKENDS, 1)',project)
if __name__=='__main__':unittest.main()
