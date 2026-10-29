"""Windows-only standalone named-pipe identity and challenge-response tests.

These launch ONLY ipc_probe_win.exe, never Explorer or Windhawk.
"""
from __future__ import annotations
import os
import secrets
import subprocess
import sys
import unittest


@unittest.skipUnless(sys.platform == "win32" and os.environ.get("ATB_NATIVE_IPC_PROBE"),
                     "Windows named-pipe executable is required")
class NativeIpcTests(unittest.TestCase):
    def setUp(self):
        self.exe = os.path.abspath(os.environ["ATB_NATIVE_IPC_PROBE"])
        self.key = secrets.token_hex(32)
        self.pipe_suffix = secrets.token_hex(12)
        self.servers = []

    def tearDown(self):
        for server in self.servers:
            if server.poll() is None:
                server.kill()
            server.wait(timeout=8)
            if server.stdin and not server.stdin.closed:
                server.stdin.close()
            if server.stdout:
                server.stdout.close()
            if server.stderr:
                server.stderr.close()

    def start_server(self):
        server = subprocess.Popen([self.exe, "server", self.pipe_suffix],
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE)
        assert server.stdin is not None
        server.stdin.write((self.key + "\n").encode("ascii"))
        server.stdin.flush()
        server.stdin.close()
        self.servers.append(server)
        return server

    def client(self, mode="client", key=None, extra=""):
        token = key if key is not None else self.key
        return subprocess.run([self.exe, mode, self.pipe_suffix],
                              input=(token + "\n" + extra).encode("ascii"),
                              capture_output=True, timeout=12)

    def finish(self, server, expected):
        result = server.wait(timeout=12)
        self.assertEqual(result, expected,
                         f"server output={server.stdout.read()!r}, errors={server.stderr.read()!r}")

    def test_same_session_same_user_authenticated(self):
        server = self.start_server()
        client = self.client()
        self.assertEqual(client.returncode, 0, client.stderr)
        self.finish(server, 0)

    def test_wrong_token_fails(self):
        server = self.start_server()
        client = self.client(key=secrets.token_hex(32))
        self.assertNotEqual(client.returncode, 0)
        self.finish(server, 3)

    def test_replay_from_previous_trial_fails(self):
        first = self.start_server()
        captured = self.client(mode="client-capture")
        self.assertEqual(captured.returncode, 0, captured.stderr)
        self.finish(first, 0)
        proof_line = next(x for x in captured.stdout.decode().splitlines()
                          if x.startswith("TEST_PROOF="))
        old_proof = proof_line.split("=", 1)[1]
        self.assertEqual(len(old_proof), 64)
        second = self.start_server()
        replay = self.client(mode="client-replay", extra=old_proof + "\n")
        self.assertNotEqual(replay.returncode, 0, replay.stderr)
        self.finish(second, 3)

    def test_invalid_client_secret_rejected_before_authorization(self):
        server = self.start_server()
        bad = self.client(key="invalid")
        self.assertNotEqual(bad.returncode, 0)
        # Server is still waiting for a valid client; teardown kills it.

    def test_server_receives_no_key_through_command_line(self):
        server = self.start_server()
        self.assertNotIn(self.key, " ".join(server.args))
        client = self.client()
        self.assertNotIn(self.key, " ".join([self.exe, "client", self.pipe_suffix]))
        self.assertEqual(client.returncode, 0)
        self.finish(server, 0)


if __name__ == "__main__":
    unittest.main()
