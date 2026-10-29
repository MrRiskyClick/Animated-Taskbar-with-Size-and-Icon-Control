"""Mock credential contract tests. Never contacts Explorer or Windhawk."""
from __future__ import annotations

import io
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

from watchdog import core, mock_credentials


class CredentialTests(unittest.TestCase):
    def test_random_independent_secrets_and_constant_time_digest(self):
        first, second = mock_credentials.new_token(), mock_credentials.new_token()
        self.assertEqual(len(first), 64)
        self.assertNotEqual(first, second)
        self.assertTrue(mock_credentials.matches(first, mock_credentials.digest(first)))
        self.assertFalse(mock_credentials.matches(second, mock_credentials.digest(first)))
        self.assertFalse(mock_credentials.matches(first, "garbage"))

    def test_rejects_unicode_bad_length_and_missing_data(self):
        for bad in ("", "abc", "Z" * 64, "a" * 63, "a" * 65, "a" * 64 + "🐱"):
            with self.assertRaises(mock_credentials.CredentialError):
                mock_credentials.digest(bad)
        for malformed in (b"", b"\n", b"a" * 64, b"Z" * 64 + b"\n",
                          b"a" * 65 + b"\n", b"abc\n"):
            with self.assertRaises(mock_credentials.CredentialError):
                mock_credentials.receive(io.BytesIO(malformed))

    def test_pipe_transfer_to_separate_process_hides_secret_from_argv(self):
        token = mock_credentials.new_token()
        program = ("import sys,hashlib; line=sys.stdin.buffer.readline(66);"
                   "print(hashlib.sha256(line[:-1]).hexdigest())")
        argv = [sys.executable, "-c", program]
        self.assertNotIn(token, " ".join(argv))
        child = subprocess.Popen(argv, stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        mock_credentials.send(child.stdin, token)
        child.wait(timeout=5)
        with child.stdout, child.stderr:
            output = child.stdout.read().decode("ascii").strip()
            errors = child.stderr.read().decode("ascii")
        self.assertEqual(child.returncode, 0, errors)
        self.assertEqual(output, mock_credentials.digest(token))

    def test_journal_never_persists_raw_token_and_rejects_wrong_token(self):
        with tempfile.TemporaryDirectory() as dirname:
            root = pathlib.Path(dirname)
            core.initialize_mock(root, 44)
            prepared = core.prepare(root, 77, test_duration=8)
            self.assertIn("token", prepared)
            stored = core.journal_path(root).read_text(encoding="utf-8")
            self.assertNotIn(prepared["token"], stored)
            self.assertNotIn('"token":', stored)
            self.assertIn("token_digest", stored)
            self.assertFalse(core.ready(root, prepared["trial_id"],
                                        mock_credentials.new_token()))
            self.assertTrue(core.ready(root, prepared["trial_id"], prepared["token"]))
            core.recover(root)

    def test_old_journal_schema_fails_closed(self):
        with tempfile.TemporaryDirectory() as dirname:
            root = pathlib.Path(dirname)
            core.initialize_mock(root, 44)
            prepared = core.prepare(root, 77, test_duration=8)
            with core.state_lock(root):
                value = core.load(root)
                value["schema"] = 1
                core.atomic_json(core.journal_path(root), value)
            with self.assertRaises(core.SafetyError):
                core.reconcile_stale(root)
            self.assertEqual(core.read_json(core.target_path(root)), {"height": 44})


if __name__ == "__main__":
    unittest.main()
