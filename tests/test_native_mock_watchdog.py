"""Windows-native watchdog integration tests against MOCK temporary files only.

No Windhawk, Explorer, taskbar window, registry, system service or install.
The binary only accepts state dirs named atb-native-mock-* under Windows TEMP.
"""
from __future__ import annotations
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

BIN = os.environ.get("ATB_NATIVE_MOCK_WATCHDOG")


@unittest.skipUnless(sys.platform == "win32" and BIN, "Windows native mock executable required")
class NativeMockProcessTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="atb-native-mock-")
        self.root = Path(self.temp.name)
        self.exe = str(Path(BIN).resolve())
        self.env = dict(os.environ, ATB_NATIVE_MOCK_TEST_MODE="1")
        self.workers = []
        self.cmd("init", self.root, 44)

    def tearDown(self):
        for p in self.workers:
            if p.poll() is None:
                p.kill()
            p.wait(timeout=5)
            if p.stdout:
                p.stdout.close()
            if p.stderr:
                p.stderr.close()
        error = None
        for _ in range(30):
            try:
                self.temp.cleanup()
                return
            except PermissionError as exc:
                error = exc
                time.sleep(.1)
        if error:
            raise error

    def cmd(self, *args, expected=0, env=None):
        result = subprocess.run([self.exe, *map(str, args)],
                                env=env or self.env, capture_output=True,
                                text=True, timeout=8)
        self.assertEqual(result.returncode, expected,
                         f"got={result.returncode}, expected={expected}, "
                         f"stdout={result.stdout}, stderr={result.stderr}")
        return result.stdout

    def arm(self, seconds=4, height=70):
        out = self.cmd("arm", self.root, height, seconds)
        trial = out.strip().split("TRIAL=", 1)[-1]
        self.assertEqual(len(trial), 32)
        return trial

    def worker(self, trial):
        proc = subprocess.Popen([self.exe, "run", str(self.root), trial],
                                env=self.env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.workers.append(proc)
        def ready():
            output = self.cmd("status", self.root)
            return "PHASE=READY" in output
        self.until(ready, timeout=5)
        return proc

    def until(self, callback, timeout=8):
        finish = time.monotonic() + timeout
        while time.monotonic() < finish:
            if callback():
                return
            time.sleep(.05)
        self.fail("Timed out waiting for native watchdog state transition")

    def status(self):
        return self.cmd("status", self.root)

    def target(self):
        return (self.root / "target.v1").read_text(encoding="ascii")

    def test_watchdog_restores_mock_at_expiry(self):
        trial = self.arm(seconds=3)
        process = self.worker(trial)
        self.cmd("apply", self.root, trial)
        self.assertIn("height=70", self.target())
        self.until(lambda: "PHASE=RECOVERED" in self.status(), timeout=8)
        self.assertIn("height=44", self.target())
        self.assertEqual(process.wait(timeout=4), 0)

    def test_external_edit_is_not_overwritten(self):
        trial = self.arm(seconds=3)
        self.worker(trial)
        self.cmd("apply", self.root, trial)
        # Third-party edit must be an atomic replacement, not an in-place
        # truncate that the independent status reader could catch halfway.
        outside = self.root / "target.external.tmp"
        # Write raw bytes: Python text mode would convert LF -> CRLF
        # on Windows, producing an invalid strict-format fixture.
        outside.write_bytes(b"ATB_NATIVE_MOCK_TARGET_V1\nheight=88\nEND\n")
        os.replace(outside, self.root / "target.v1")
        self.until(lambda: "PHASE=CONFLICT" in self.status(), timeout=8)
        self.assertIn("height=88", self.target())
        self.cmd("arm", self.root, 66, 3, expected=3)

    def test_malformed_external_target_is_not_overwritten(self):
        trial = self.arm(seconds=6)
        self.worker(trial)
        self.cmd("apply", self.root, trial)
        malformed = b"ATB_NATIVE_MOCK_TARGET_V1\r\nheight=88\r\nEND\r\n"
        outside = self.root / "target.bad.tmp"
        outside.write_bytes(malformed)
        os.replace(outside, self.root / "target.v1")
        # An unknown external format cannot be restored or silently assumed
        # to equal either the baseline or the value installed by this trial.
        self.cmd("recover", self.root, expected=3)
        self.assertEqual((self.root / "target.v1").read_bytes(), malformed)

    def test_watchdog_killed_then_explicit_reconcile(self):
        trial = self.arm(seconds=6)
        worker = self.worker(trial)
        self.cmd("apply", self.root, trial)
        worker.kill()
        worker.wait(timeout=5)
        time.sleep(1.8)
        self.cmd("apply", self.root, trial, expected=3)
        self.cmd("reconcile", self.root)
        self.assertIn("PHASE=RECOVERED", self.status())
        self.assertIn("height=44", self.target())

    def test_reconcile_does_not_interrupt_live_trial(self):
        trial = self.arm(seconds=6)
        worker = self.worker(trial)
        self.cmd("apply", self.root, trial)
        self.cmd("reconcile", self.root, expected=3)
        self.assertIn("PHASE=ACTIVE", self.status())
        self.cmd("recover", self.root)
        self.assertEqual(worker.wait(timeout=4), 0)

    def test_no_short_deadline_without_explicit_mock_test_mode(self):
        safe_env = {k:v for k,v in self.env.items() if k != "ATB_NATIVE_MOCK_TEST_MODE"}
        self.cmd("arm", self.root, 77, 3, expected=2, env=safe_env)
        self.assertFalse((self.root / "journal.v1").exists())

    def test_wrong_trial_cannot_mutate_target(self):
        trial=self.arm(seconds=6)
        self.worker(trial)
        self.cmd("apply", self.root, "f"*32, expected=3)
        self.assertIn("height=44", self.target())
        self.cmd("recover", self.root)

    def test_corrupted_journal_fails_closed_without_overwriting(self):
        trial=self.arm(seconds=6)
        (self.root / "journal.v1").write_text("corrupt state", encoding="ascii")
        self.cmd("run", self.root, trial, expected=3)
        self.cmd("recover", self.root, expected=3)
        self.assertIn("height=44", self.target())

    def test_no_implicit_rearm_after_recover(self):
        trial=self.arm(seconds=6)
        worker=self.worker(trial)
        self.cmd("apply", self.root, trial)
        self.cmd("recover", self.root)
        self.assertEqual(worker.wait(timeout=4), 0)
        self.assertIn("height=44", self.target())
        self.cmd("apply", self.root, trial, expected=3)
        # A later, explicit arm is a new distinct trial.
        new=self.arm(seconds=6,height=68)
        self.assertNotEqual(trial,new)
        self.assertIn("PHASE=PREPARED",self.status())
        self.cmd("recover",self.root)

    def test_rejects_directory_outside_named_temp_fixture(self):
        with tempfile.TemporaryDirectory(prefix="ordinary-") as outside:
            self.cmd("init", outside, 44, expected=2)


if __name__ == "__main__":
    unittest.main()
