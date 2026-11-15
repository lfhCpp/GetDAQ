import fcntl
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

TOOLS = Path(__file__).resolve().parents[1] / 'tools'
spec = importlib.util.spec_from_file_location('service_stop', TOOLS / 'stop-getdaq-services.py')
control = importlib.util.module_from_spec(spec)
spec.loader.exec_module(control)

class LaunchControlTests(unittest.TestCase):
    def fixture(self, root, body):
        (root / 'site').mkdir()
        (root / 'logs').mkdir()
        (root / 'runtime-linux/bin').mkdir(parents=True)
        shutil.copy(TOOLS / 'launch-console.py', root / 'site/launch-console.py')
        exe = root / 'runtime-linux/bin/GetDAQ'
        exe.write_text('#!' + sys.executable + '\n' + body)
        exe.chmod(0o700)
        return dict(os.environ, DISPLAY=':test')

    def test_old_service_lock_does_not_block_launch_and_child_owns_no_launch_lock(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            env = self.fixture(root, "import time,os,json\nfrom pathlib import Path\ntime.sleep(.8)\n"
                "fds=[]\nfor p in Path('/proc/self/fd').iterdir():\n try: fds.append(os.readlink(p))\n except OSError: pass\n"
                "Path('fds.json').write_text(json.dumps(fds))\n")
            with (root / 'logs/gui-launch.lock').open('w') as old_lock:
                fcntl.flock(old_lock, fcntl.LOCK_EX)
                result = subprocess.run([sys.executable, str(root / 'site/launch-console.py')], env=env, capture_output=True, timeout=4)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn(b'GetDAQ started', result.stdout)
                deadline = time.monotonic() + 3
                while not (root / 'fds.json').exists() and time.monotonic() < deadline:
                    time.sleep(.1)
                fds = json.loads((root / 'fds.json').read_text())
                self.assertFalse(any('gui-start-v2.lock' in fd or 'gui-launch.lock' in fd for fd in fds))

    def test_startup_failure_is_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            env = self.fixture(root, "import sys\nprint('startup failure',file=sys.stderr)\nsys.exit(7)\n")
            result = subprocess.run([sys.executable, str(root / 'site/launch-console.py')], env=env, capture_output=True, timeout=4)
            self.assertEqual(result.returncode, 1)
            self.assertIn(b'startup failure', result.stderr)

    def test_force_stop_requires_explicit_flag(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            exe = root / 'runtime-old/bin/getdaq-ecc-server'
            exe.parent.mkdir(parents=True)
            shutil.copy(sys.executable, exe)
            ready = root / 'ready'
            code = "import signal,time; from pathlib import Path; signal.signal(signal.SIGTERM,signal.SIG_IGN); Path(%r).touch(); time.sleep(30)" % str(ready)
            process = subprocess.Popen([str(exe), '-c', code])
            try:
                deadline = time.monotonic() + 3
                while not ready.exists() and time.monotonic() < deadline:
                    time.sleep(.05)
                self.assertTrue(ready.exists())
                self.assertEqual(control.stop(root, control.NAMES['ecc'], force=True, assume_yes=True), 0)
                self.assertEqual(process.wait(timeout=3), -9)
            finally:
                if process.poll() is None:
                    process.kill();process.wait()

    def test_stop_is_scoped_to_installation_and_tracks_process_birth(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            exe = root / 'runtime-old/bin/getdaq-ecc-server'
            exe.parent.mkdir(parents=True)
            shutil.copy(sys.executable, exe)
            process = subprocess.Popen([str(exe), '-c', 'import time; time.sleep(30)'])
            try:
                time.sleep(.2)
                own = control.identity(process.pid, root, control.NAMES['ecc'])
                self.assertIsNotNone(own)
                self.assertIsNone(control.identity(process.pid, root / 'another-installation', control.NAMES['ecc']))
                self.assertIsNone(control.identity(process.pid, root, control.NAMES['data']))
                self.assertEqual(control.stop(root, control.NAMES['ecc'], listing=True), 0)
                self.assertIsNone(process.poll())
                self.assertEqual(control.stop(root, control.NAMES['ecc'], assume_yes=True), 0)
                process.wait(timeout=3)
            finally:
                if process.poll() is None:
                    process.kill();process.wait()

if __name__ == '__main__':
    unittest.main()
