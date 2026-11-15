#!/usr/bin/env python3
"""Verify the supervisor runner removes only its owned raw files on exit."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TESTS = Path(__file__).resolve().parent
sys.path.insert(0, str(TESTS))
from run_supervisor_control_tests import cleanup_owned_raw_files


class SupervisorCleanupTest(unittest.TestCase):
    def test_cleanup_stays_inside_runner_root(self):
        with tempfile.TemporaryDirectory(prefix="getdaq-cleanup-") as temporary:
            root = Path(temporary) / "evidence"
            root.mkdir()
            outside = Path(temporary) / "outside.graw"
            outside.write_bytes(b"preserve")
            owned = root / "scenario"
            owned.mkdir()
            (owned / "owned.graw").write_bytes(b"remove")
            (owned / "notes.txt").write_text("keep")
            (owned / "link.graw").symlink_to(outside)

            cleanup_owned_raw_files(root)

            self.assertFalse((owned / "owned.graw").exists())
            self.assertFalse((owned / "link.graw").is_symlink())
            self.assertEqual((owned / "notes.txt").read_text(), "keep")
            self.assertEqual(outside.read_bytes(), b"preserve")

    def test_cleanup_runs_during_uncaught_exception_and_keyboard_interrupt(self):
        with tempfile.TemporaryDirectory(prefix="getdaq-cleanup-exit-") as temporary:
            root = Path(temporary) / "evidence"
            root.mkdir()
            module_dir = str(TESTS)
            for statement, expected_exit in (
                    ("raise RuntimeError('injected failure')", 1),
                    ("def interrupt():\n    raise KeyboardInterrupt\n"
                     "run_with_keyboard_interrupt_cleanup(interrupt)", 130)):
                with self.subTest(statement=statement):
                    raw = root / "failed-scenario.graw"
                    evidence = root / "evidence.txt"
                    script = "\n".join((
                        "import sys",
                        "from pathlib import Path",
                        "sys.path.insert(0, " + repr(module_dir) + ")",
                        "from run_supervisor_control_tests import register_owned_raw_cleanup, run_with_keyboard_interrupt_cleanup",
                        "root = Path(" + repr(str(root)) + ")",
                        "register_owned_raw_cleanup(root)",
                        "(root / 'failed-scenario.graw').write_bytes(b'payload')",
                        "(root / 'evidence.txt').write_text('keep')",
                        statement,
                    ))
                    result = subprocess.run([sys.executable, "-c", script],
                                            capture_output=True, text=True, check=False)
                    self.assertEqual(result.returncode, expected_exit, result.stderr)
                    self.assertFalse(raw.exists())
                    self.assertEqual(evidence.read_text(), "keep")


if __name__ == "__main__":
    unittest.main()
