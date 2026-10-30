"""Release trust is always blocked in source control until independently approved."""
from __future__ import annotations
import hashlib
import json
import pathlib
import tempfile
import unittest
from scripts.build_diagnostic_manifest import diagnostic_manifest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class ReleasePolicyTests(unittest.TestCase):
    def test_committed_policy_denies_all_releases(self):
        policy = json.loads((ROOT / 'release' / 'watchdog-approval.json').read_text())
        self.assertEqual(policy['schema'], 1)
        self.assertEqual(policy['target'], 'atb-native-watchdog')
        self.assertIs(policy['approved'], False)
        self.assertIsNone(policy['sha256'])
        self.assertIsNone(policy['signer_thumbprint'])
        self.assertIsNone(policy['source_commit'])
        self.assertEqual(policy['artifact_name'], 'atb-watchdog.exe')

    def test_build_manifest_cannot_claim_trust(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = pathlib.Path(directory) / 'binary_identity_probe_win.exe'
            binary.write_bytes(b'isolated synthetic diagnostic')
            meta = diagnostic_manifest(binary, 'a' * 40)
            self.assertEqual(meta['sha256'], hashlib.sha256(binary.read_bytes()).hexdigest())
            self.assertEqual(meta['artifact_kind'], 'test-only-windows-identity-diagnostic')
            for key in ('trusted', 'signed', 'deployable', 'release_approved'):
                self.assertIs(meta[key], False)
            with self.assertRaises(ValueError):
                diagnostic_manifest(binary, 'invalid')
            with self.assertRaises(ValueError):
                diagnostic_manifest(binary.with_name('atb-watchdog.exe'), 'a' * 40)


if __name__ == '__main__':
    unittest.main()
