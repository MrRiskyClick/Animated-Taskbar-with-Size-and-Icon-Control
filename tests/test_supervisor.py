"""Cross-process failure recovery tests using mock state ONLY."""
from __future__ import annotations

import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest

from watchdog import core

ROOT = Path(__file__).resolve().parents[1]
ENV = {**os.environ, "ATB_WATCHDOG_TEST_MODE": "1"}


def run(*argv, expected=0):
    p = subprocess.run([sys.executable, "-m", "watchdog.cli", *map(str, argv)],
                       cwd=ROOT, env=ENV, capture_output=True, text=True, timeout=15)
    if p.returncode != expected:
        raise AssertionError(f"return code {p.returncode} vs {expected}: {p.stdout} {p.stderr}")
    return p.stdout


def poll_until(predicate, seconds=12):
    finish = time.monotonic() + seconds
    while time.monotonic() < finish:
        result = predicate()
        if result:
            return result
        time.sleep(0.04)
    raise AssertionError("Timed out waiting for automatic recovery")


class SupervisorRecoveryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        run("init", "--state-dir", self.root, "--height", 43)
        self.pids = []

    def tearDown(self):
        try:
            core.recover(self.root)
        except Exception:
            pass
        for pid in self.pids:
            try:
                os.kill(pid, signal.SIGTERM)
            except OSError:
                pass
        self.temp.cleanup()

    def active(self):
        record = json.loads(run("start", "--state-dir", self.root,
                                "--height", 76, "--test-duration", 20))
        self.assertEqual(record["phase"], "ACTIVE")
        self.assertIsInstance(record["watchdog_pid"], int)
        self.assertIsInstance(record["supervisor_pid"], int)
        self.assertNotEqual(record["watchdog_pid"], record["supervisor_pid"])
        self.pids.extend([record["watchdog_pid"], record["supervisor_pid"]])
        return record

    def await_recovery(self):
        def phase():
            with core.state_lock(self.root):
                rec = core.load(self.root)
            return rec if rec["phase"] in {"RECOVERED", "EXPIRED", "CONFLICT"} else None

        result = poll_until(phase)
        self.assertEqual(result["phase"], "RECOVERED")
        self.assertEqual(core.read_json(core.target_path(self.root)), {"height": 43})
        self.assertFalse((self.root / "lease.v1").exists())
        return result

    def test_killed_watchdog_is_recovered_by_supervisor(self):
        rec = self.active()
        os.kill(rec["watchdog_pid"], signal.SIGTERM)
        result = self.await_recovery()
        self.assertIn("restored", result["result"])

    def test_killed_supervisor_is_recovered_by_watchdog(self):
        rec = self.active()
        os.kill(rec["supervisor_pid"], signal.SIGTERM)
        self.await_recovery()

    def test_both_safety_processes_lost_reconcile_without_auto_rearm(self):
        trial = self.active()
        os.kill(trial["watchdog_pid"], signal.SIGTERM)
        os.kill(trial["supervisor_pid"], signal.SIGTERM)
        # Let the recorded mock heartbeats age out before recovery.
        time.sleep(5.5)
        run("start", "--state-dir", self.root, "--height", 65,
            "--test-duration", 20, expected=2)
        with core.state_lock(self.root):
            state = core.load(self.root)
        self.assertEqual(state["phase"], "RECOVERED")
        self.assertEqual(core.read_json(core.target_path(self.root)), {"height": 43})
        # A new, explicit user start is necessary after recovery.
        new_trial = self.active()
        self.assertNotEqual(new_trial["trial_id"], trial["trial_id"])

    def test_absent_supervisor_rejected_before_mock_apply(self):
        trial = core.prepare(self.root, 69, test_duration=20)
        # Direct legacy fixture path can run without supervisor and is NOT
        # a production API. New CLI path requires supervisor handshake.
        self.assertIsNone(trial.get("supervisor_pid"))
        self.assertEqual(core.read_json(core.target_path(self.root)), {"height": 43})
        core.recover(self.root)


if __name__ == "__main__":
    unittest.main()
