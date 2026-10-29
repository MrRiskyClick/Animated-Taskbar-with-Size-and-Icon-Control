"""Mock-only lease-file transport tests; no real taskbar calls."""
from __future__ import annotations

import os
from pathlib import Path
import tempfile
import time
import unittest

from watchdog import core, lease_protocol


class LeaseProtocolTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        core.initialize_mock(self.root, 44)

    def tearDown(self):
        self.tmp.cleanup()

    def test_valid_record_has_fixed_wire_schema(self):
        record = core.prepare(self.root, 70, test_duration=3)
        self.assertTrue(core.ready(self.root, record["trial_id"], record["token"]))
        core.guarded_mock_apply(self.root, record["trial_id"])
        self.assertTrue(core.tick(self.root, record["trial_id"], record["token"]))
        lease = lease_protocol.lease_path(self.root)
        lines = lease.read_text(encoding="ascii").splitlines()
        self.assertEqual(lines[0], "ATB_LEASE_V1")
        self.assertEqual(lines[1], "id=" + record["trial_id"])
        self.assertEqual(lines[3], "phase=ACTIVE")
        self.assertEqual(lines[8], "END")
        self.assertLessEqual(lease.stat().st_size, 512)
        self.assertTrue(core.recover(self.root))
        self.assertFalse(lease.exists())

    def test_expiry_revokes_file_and_restores_mock(self):
        record = core.prepare(self.root, 70, test_duration=0.1)
        self.assertTrue(core.ready(self.root, record["trial_id"], record["token"]))
        core.guarded_mock_apply(self.root, record["trial_id"])
        self.assertTrue(core.tick(self.root, record["trial_id"], record["token"]))
        time.sleep(0.2)
        self.assertFalse(core.tick(self.root, record["trial_id"], record["token"]))
        self.assertFalse(lease_protocol.lease_path(self.root).exists())
        self.assertEqual(core.read_json(core.target_path(self.root)), {"height": 44})

    def test_invalid_lease_cannot_be_exported(self):
        record = core.prepare(self.root, 70, test_duration=3)
        with self.assertRaises(core.SafetyError):
            lease_protocol.encode_lease(record, 1)
        record["phase"] = "ACTIVE"
        record["watchdog_pid"] = 123
        record["heartbeat_utc"] = record["created_utc"]
        with self.assertRaises(core.SafetyError):
            lease_protocol.encode_lease(record, 0)
        record["trial_id"] = "bad"
        with self.assertRaises(core.SafetyError):
            lease_protocol.encode_lease(record, 1)

    def test_mock_restart_revokes_earlier_lease(self):
        first = core.prepare(self.root, 70, test_duration=1)
        self.assertTrue(core.ready(self.root, first["trial_id"], first["token"]))
        core.guarded_mock_apply(self.root, first["trial_id"])
        self.assertTrue(core.tick(self.root, first["trial_id"], first["token"]))
        self.assertTrue(lease_protocol.lease_path(self.root).exists())
        core.recover(self.root)
        second = core.prepare(self.root, 66, test_duration=1)
        self.assertNotEqual(second["trial_id"], first["trial_id"])
        self.assertFalse(lease_protocol.lease_path(self.root).exists())


if __name__ == "__main__":
    unittest.main()
