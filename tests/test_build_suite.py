#!/usr/bin/env python3
"""Build-plan checks that do not invoke Qt, CMake, or compiler tools."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('build_suite', ROOT / 'tools/build-suite.py')
suite = importlib.util.module_from_spec(spec)
spec.loader.exec_module(suite)


class BuildSuitePlanTests(unittest.TestCase):
    def test_linux_full_and_data_host_include_independent_node_agent(self):
        if suite.platform.system() != 'Linux':
            self.skipTest('Linux node-agent deployment profile')
        for profile in ('full', 'data-host'):
            args = type('Args', (), {'profile': profile, 'cmake': 'cmake', 'qmake': 'qmake', 'make': 'make', 'jobs': 2})()
            steps = suite.plan(args, Path('/tmp/getdaq-plan-test'))
            agent = next(step for step in steps if step['component'] == 'node-agent')
            self.assertIn('getdaq-node-agent', agent['programs'])
            self.assertTrue(any(value.endswith('apps/deployment/nodeagent.pro') for value in agent['configure']))

    def test_qt_prefix_is_added_only_to_cmake_configure_commands(self):
        with tempfile.TemporaryDirectory(prefix='getdaq-build-plan-') as temporary:
            args = type('Args', (), {
                'profile': 'full', 'cmake': 'cmake', 'qmake': 'qmake',
                'make': 'make', 'jobs': 2,
            })()
            steps = suite.plan(args, Path(temporary))

        suite.add_cmake_qt_prefix(steps, '/opt/qt')
        for step in steps:
            command = step['configure']
            if '-S' in command and '-B' in command:
                self.assertIn('-DCMAKE_PREFIX_PATH=/opt/qt', command)
            else:
                self.assertNotIn('-DCMAKE_PREFIX_PATH=/opt/qt', command)
        mcp = next(step for step in steps if step['component'] == 'mcp')
        self.assertEqual(mcp['configure'][0], 'qmake')
        self.assertFalse(any(value.startswith('-D') for value in mcp['configure']))


if __name__ == '__main__':
    unittest.main()
