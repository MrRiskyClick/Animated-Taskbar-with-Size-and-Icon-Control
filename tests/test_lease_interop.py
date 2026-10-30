"""Cross-language wire-format test: Python publisher to compiled C++ reader."""
from __future__ import annotations
import os
from pathlib import Path
import subprocess
import tempfile
import time
import unittest

from watchdog import core, lease_protocol


class NativeInteroperability(unittest.TestCase):
    def test_native_reader_accepts_healthy_and_rejects_invalid(self):
        binary = os.environ.get("ATB_NATIVE_LEASE_PROBE")
        if not binary:
            self.skipTest("Probe must be compiled and set by CI")
        exe = Path(binary).resolve()
        self.assertTrue(exe.is_file())
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            core.initialize_mock(root, 44)
            record = core.prepare(root, 70, test_duration=6)
            self.assertTrue(core.ready(root, record["trial_id"], record["token"]))
            core.guarded_mock_apply(root, record["trial_id"])
            self.assertTrue(core.tick(root, record["trial_id"], record["token"]))
            lease = lease_protocol.lease_path(root)

            def probe(trial):
                return subprocess.run([str(exe), str(lease), trial],
                                      capture_output=True, timeout=5).returncode

            self.assertEqual(probe(record["trial_id"]), 0)
            self.assertEqual(probe("ffffffffffffffffffffffffffffffff"), 3)
            lease.write_bytes(b"falsified observer record")
            self.assertEqual(probe(record["trial_id"]), 3)
            # Recovery of the actual MOCK value is independent of observer.
            state = core.recover(root, reason="manual")
            self.assertEqual(state["phase"], "RECOVERED")
            self.assertFalse(lease.exists())
            self.assertEqual(core.read_json(core.target_path(root)), {"height": 44})
            self.assertEqual(probe(record["trial_id"]), 3)


if __name__ == "__main__":
    unittest.main()
