"""Stage-1g Windows read-only peer integration tests. NO Explorer injection."""
from __future__ import annotations
import os
from pathlib import Path
import secrets
import subprocess
import sys
import time
import unittest


@unittest.skipUnless(sys.platform == "win32" and
                     os.environ.get("ATB_NATIVE_IPC_PROBE") and
                     os.environ.get("ATB_READONLY_PEER_HOST"),
                     "Stage-1g Windows-only test binaries required")
class ReadOnlyPeerTests(unittest.TestCase):
    def setUp(self):
        self.server_exe = Path(os.environ["ATB_NATIVE_IPC_PROBE"]).resolve()
        self.host_exe = Path(os.environ["ATB_READONLY_PEER_HOST"]).resolve()
        self.suffix = secrets.token_hex(12)
        self.servers = []

    def tearDown(self):
        for server in self.servers:
            if server.poll() is None:
                server.kill()
            server.wait(timeout=8)
            if server.stdout:
                server.stdout.close()
            if server.stderr:
                server.stderr.close()

    def server(self, mode="observer-server"):
        p = subprocess.Popen([str(self.server_exe), mode, self.suffix],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.servers.append(p)
        return p

    def observe(self, pid, path=None):
        result = subprocess.run(
            [str(self.host_exe), self.suffix, str(pid),
             str(path if path is not None else self.server_exe)],
            capture_output=True, timeout=9, text=True)
        return result

    def test_correct_pid_and_binary_returns_only_diagnostic(self):
        server = self.server()
        result = self.observe(server.pid)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("READ_ONLY_PEER_OK", result.stdout)
        self.assertEqual(server.wait(timeout=7), 0)

    def test_wrong_pid_is_refused(self):
        server = self.server()
        result = self.observe(os.getpid())
        self.assertNotEqual(result.returncode, 0)
        self.assertNotEqual(server.wait(timeout=8), 0)

    def test_wrong_executable_is_refused(self):
        server = self.server()
        result = self.observe(server.pid, self.host_exe)
        self.assertNotEqual(result.returncode, 0)
        self.assertNotEqual(server.wait(timeout=8), 0)

    def test_wrong_reply_cannot_pass_read_only_probe(self):
        server = self.server("observer-server-bad")
        result = self.observe(server.pid)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(server.wait(timeout=8), 0)

    def test_missing_server_fails_without_hanging(self):
        result = self.observe(os.getpid())
        self.assertNotEqual(result.returncode, 0)

    def test_invalid_suffix_fails_without_a_connection(self):
        p = subprocess.run([str(self.host_exe), "invalid_!suffix", "9999",
                            str(self.server_exe)], capture_output=True, timeout=9)
        self.assertNotEqual(p.returncode, 0)


if __name__ == "__main__":
    unittest.main()
