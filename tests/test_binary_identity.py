"""Stage-1h identity verifier tests on isolated Windows Actions workers.

Unsigned test mode ONLY checks an independently calculated SHA-256 pin.
The production policy needs BOTH a pinned hash and WinVerifyTrust success.
No Explorer, Windhawk, Windows taskbar or registry interaction.
"""
from __future__ import annotations

import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


@unittest.skipUnless(sys.platform == "win32" and
                     os.environ.get("ATB_BINARY_IDENTITY_PROBE"),
                     "Windows identity probe executable is required")
class BinaryIdentityTests(unittest.TestCase):
    def setUp(self):
        self.binary = Path(os.environ["ATB_BINARY_IDENTITY_PROBE"]).resolve()
        self.assertTrue(self.binary.is_file())
        self.sha = hashlib.sha256(self.binary.read_bytes()).hexdigest()

    def run_probe(self, *args):
        return subprocess.run([str(self.binary), *map(str, args)],
                              capture_output=True, text=True, timeout=15)

    def test_exact_sha_pinned_file_passes_in_test_mode(self):
        result = self.run_probe("file", self.binary, self.sha, 0)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_wrong_sha_rejected(self):
        wrong = "f" * 64 if self.sha != "f" * 64 else "0" * 64
        result = self.run_probe("file", self.binary, wrong, 0)
        self.assertEqual(result.returncode, 3)

    def test_tampered_binary_on_disk_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            altered = Path(directory) / "mutated-unsigned-test-copy.exe"
            shutil.copyfile(self.binary, altered)
            with altered.open("ab") as fd:
                fd.write(b"altered file bytes")
            result = self.run_probe("file", altered, self.sha, 0)
            self.assertEqual(result.returncode, 3)

    def test_unsigned_test_binary_refused_by_strict_signed_policy(self):
        # The compiled GitHub CI test binary is not Authenticode-signed.
        # If the binary ever becomes signed, the strict check may legitimately
        # pass. CI explicitly verifies it currently remains unsigned.
        result = self.run_probe("file", self.binary, self.sha, 1)
        self.assertEqual(result.returncode, 3, result.stdout)

    def test_correct_current_process_and_creation_time(self):
        result = self.run_probe("self", self.sha, 0)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_stale_creation_time_rejected(self):
        result = self.run_probe("self-stale", self.sha, 0)
        self.assertEqual(result.returncode, 3)

    def test_wrong_pid_rejected(self):
        result = self.run_probe("wrong-pid", self.sha, 0)
        self.assertEqual(result.returncode, 3)

    def test_invalid_hex_or_policy_fails_closed(self):
        for arguments in (("self", "NOT-A-DIGEST", 0),
                          ("file", self.binary, "g" * 64, 0),
                          ("file", self.binary, self.sha.upper(), 0),
                          ("file", self.binary, self.sha, "maybe")):
            result = self.run_probe(*arguments)
            self.assertEqual(result.returncode, 2, result.stderr)

    def test_nonexistent_path_fails_closed(self):
        missing = self.binary.with_name("missing-identity-target.exe")
        result = self.run_probe("file", missing, self.sha, 0)
        self.assertEqual(result.returncode, 3)

    def test_pinned_signature_policy_not_skippable_with_wrong_digest(self):
        result = self.run_probe("file", self.binary, "0" * 64, 1)
        self.assertEqual(result.returncode, 3)


if __name__ == "__main__":
    unittest.main()
