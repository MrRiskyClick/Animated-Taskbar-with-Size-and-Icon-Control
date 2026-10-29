"""End-to-end tests of independent watchdog using isolated mock files only."""
from __future__ import annotations

import json
import os
import pathlib
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

from watchdog import core

ROOT = pathlib.Path(__file__).resolve().parents[1]
ENV = {**os.environ, "ATB_WATCHDOG_TEST_MODE": "1"}


def cli(*args, expect=0):
    cmd = [sys.executable, "-m", "watchdog.cli", *map(str, args)]
    p = subprocess.run(cmd, cwd=ROOT, env=ENV, capture_output=True, text=True,
                       timeout=12)
    if p.returncode != expect:
        raise AssertionError(
            f"Command {cmd!r} returned {p.returncode} != {expect}: "
            f"stdout={p.stdout!r} stderr={p.stderr!r}"
        )
    return p.stdout.strip()


def until(predicate, timeout=5):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        result = predicate()
        if result:
            return result
        time.sleep(0.03)
    raise AssertionError("Timed out waiting for watchdog result")


class WatchdogTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.folder.name)
        cli("init", "--state-dir", self.root, "--height", 44)
        self.pid = None

    def tearDown(self):
        # Avoid leaving test watchdogs running when an assertion fails.
        if self.pid is not None:
            try:
                os.kill(self.pid, signal.SIGTERM)
            except (OSError, ProcessLookupError):
                pass
        self.folder.cleanup()

    def start(self, *, duration=0.8, new_height=78):
        response = cli("start", "--state-dir", self.root,
                       "--height", new_height, "--test-duration", duration)
        record = json.loads(response)
        self.pid = record["watchdog_pid"]
        self.assertEqual(record["phase"], "ACTIVE")
        self.assertEqual(core.read_json(core.target_path(self.root)), {"height": new_height})
        return record

    def assert_phase(self, phase):
        def status():
            with core.state_lock(self.root):
                record = core.load(self.root)
            return record if record["phase"] == phase else None
        return until(status)

    def test_two_process_expiry_and_verify_restored_value(self):
        trial = self.start()
        result = self.assert_phase("EXPIRED")
        self.assertEqual(result["trial_id"], trial["trial_id"])
        self.assertEqual(core.read_json(core.target_path(self.root)), {"height": 44})
        self.assertIn("restored", result["result"])

    def test_manual_rollback_early(self):
        self.start(duration=4)
        record = json.loads(cli("recover", "--state-dir", self.root))
        self.assertEqual(record["phase"], "RECOVERED")
        self.assertEqual(core.read_json(core.target_path(self.root)), {"height": 44})

    def test_external_edits_not_clobbered_on_expiration(self):
        self.start()
        with core.state_lock(self.root):
            core.atomic_json(core.target_path(self.root), {"height": 90})
        result = self.assert_phase("CONFLICT")
        self.assertEqual(core.read_json(core.target_path(self.root)), {"height": 90})
        cli("start", "--state-dir", self.root, "--height", 70,
            "--test-duration", 0.6, expect=2)

    def test_crashed_watchdog_manual_recovery_and_no_auto_rearm(self):
        record = self.start(duration=8)
        os.kill(record["watchdog_pid"], signal.SIGTERM)
        time.sleep(0.1)
        # Stage-1 prototype does not have a service to auto-respawn yet:
        # explicit recovery is supported; real Explorer gating remains BLOCKED.
        result = json.loads(cli("recover", "--state-dir", self.root))
        self.assertEqual(result["phase"], "RECOVERED")
        self.assertEqual(core.read_json(core.target_path(self.root)), {"height": 44})
        self.assertNotEqual(result["phase"], "ACTIVE")

    def test_stale_or_corrupt_journal_blocks_apply(self):
        self.start(duration=4)
        with core.state_lock(self.root):
            core.atomic_json(core.journal_path(self.root), {"broken": True})
        p = subprocess.run(
            [sys.executable, "-m", "watchdog.cli", "start",
             "--state-dir", str(self.root), "--height", "77",
             "--test-duration", "1"], cwd=ROOT, env=ENV, capture_output=True,
            text=True, timeout=10
        )
        self.assertEqual(p.returncode, 2)
        self.assertEqual(core.read_json(core.target_path(self.root)), {"height": 78})

    def test_original_is_captured_before_mock_application(self):
        self.start(duration=3)
        with core.state_lock(self.root):
            record = core.load(self.root)
            self.assertEqual(record["baseline"], {"height": 44})
            self.assertEqual(record["expected"], {"height": 78})
            self.assertLess(record["created_mono"], record["deadline_mono"])

    def test_no_test_duration_outside_test_mode(self):
        with mock.patch.dict(os.environ, {"ATB_WATCHDOG_TEST_MODE": ""}):
            with self.assertRaises(core.SafetyError):
                core.prepare(self.root, 70, test_duration=0.1)

    def test_recovery_before_apply_is_idempotent(self):
        with core.state_lock(self.root):
            self.assertFalse(core.journal_path(self.root).exists())
        rec = core.prepare(self.root, 70, test_duration=1)
        self.assertEqual(rec["phase"], "PREPARED")
        recovered = core.recover(self.root)
        self.assertEqual(recovered["phase"], "RECOVERED")
        self.assertEqual(core.read_json(core.target_path(self.root)), {"height": 44})
        self.assertEqual(core.recover(self.root)["phase"], "RECOVERED")

    def test_deadline_handles_backward_monotonic(self):
        record = core.prepare(self.root, 77, test_duration=2)
        self.assertTrue(core.elapsed(
            record, now_utc=record["created_utc"],
            now_mono=record["created_mono"] - 3
        ))

    def test_lease_requires_live_heartbeat_and_active_phase(self):
        record = self.start(duration=3)
        self.assertTrue(core.lease_is_healthy(self.root, record["trial_id"]))
        self.assertFalse(core.lease_is_healthy(self.root, "wrong-id"))
        os.kill(record["watchdog_pid"], signal.SIGTERM)
        time.sleep(0.4)
        self.assertFalse(core.lease_is_healthy(
            self.root, record["trial_id"], max_heartbeat_age=0.2))
        core.recover(self.root)
        self.assertFalse(core.lease_is_healthy(self.root, record["trial_id"]))

    def test_no_duplicate_live_trial(self):
        self.start(duration=3)
        with self.assertRaises(core.SafetyError):
            core.prepare(self.root, 60, test_duration=1)


if __name__ == "__main__":
    unittest.main()
