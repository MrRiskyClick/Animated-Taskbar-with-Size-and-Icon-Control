"""Tests of opt-in, exact-ID, version-gated Windhawk CLI adapter.

Runs with a fake runner only. NEVER invokes installed Windhawk or Explorer.
"""
from __future__ import annotations

import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

from watchdog.core import SafetyError
from watchdog.windhawk_control import OUR_MOD_ID, WindhawkCli


class FakeRunner:
    def __init__(self, version="windhawk-cli 2.0.0-alpha.8"):
        self.calls = []
        self.version = version
        self.disable_exit = 0

    def __call__(self, argv, timeout):
        self.calls.append((list(argv), timeout))
        if argv[1:] == ["--version"]:
            return subprocess.CompletedProcess(argv, 0, self.version, "")
        if argv[1:] == ["mod", "disable", OUR_MOD_ID]:
            return subprocess.CompletedProcess(argv, self.disable_exit, "success", "")
        raise AssertionError(f"Unexpected command: {argv}")


class CliAdapterTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = Path(self.temp.name) / "windhawk-cli.exe"
        self.path.write_text("test placeholder, never executed", encoding="utf-8")
        self.runner = FakeRunner()
        self.adapter = WindhawkCli(self.path, self.runner)

    def tearDown(self):
        self.temp.cleanup()

    def test_only_probes_and_dry_runs_by_default(self):
        command = self.adapter.disable_our_mod()
        self.assertEqual(command, [str(self.path), "mod", "disable", OUR_MOD_ID])
        self.assertEqual(len(self.runner.calls), 1)
        self.assertEqual(self.runner.calls[0][0][1:], ["--version"])

    def test_refuses_other_mod_ids(self):
        with self.assertRaises(SafetyError):
            self.adapter.disable_our_mod("windows-11-taskbar-styler")
        self.assertEqual(self.runner.calls, [])

    def test_refuses_windhawk_one_x(self):
        self.runner.version = "windhawk-cli 1.7.3"
        with self.assertRaises(SafetyError):
            self.adapter.disable_our_mod()
        self.assertEqual(len(self.runner.calls), 1)

    def test_refuses_unidentified_version(self):
        self.runner.version = "something else 2.0"
        with self.assertRaises(SafetyError):
            self.adapter.disable_our_mod()

    def test_requires_explicit_authorization_even_when_execute_true(self):
        with mock.patch.dict(os.environ, {"ATB_EXPLICIT_OWN_MOD_DISABLE": ""}):
            with self.assertRaises(SafetyError):
                self.adapter.disable_our_mod(execute=True)
        self.assertEqual(len(self.runner.calls), 1)

    def test_disable_only_exact_id_after_opt_in(self):
        with mock.patch.dict(os.environ, {"ATB_EXPLICIT_OWN_MOD_DISABLE": OUR_MOD_ID}):
            command = self.adapter.disable_our_mod(execute=True)
        self.assertEqual(command[-3:], ["mod", "disable", OUR_MOD_ID])
        self.assertEqual(self.runner.calls[-1][0][1:], ["mod", "disable", OUR_MOD_ID])

    def test_failed_disable_is_not_success(self):
        self.runner.disable_exit = 5
        with mock.patch.dict(os.environ, {"ATB_EXPLICIT_OWN_MOD_DISABLE": OUR_MOD_ID}):
            with self.assertRaises(SafetyError):
                self.adapter.disable_our_mod(execute=True)

    def test_missing_cli_fail_closed(self):
        self.path.unlink()
        with self.assertRaises(SafetyError):
            self.adapter.disable_our_mod()

    def test_executable_name_must_match(self):
        second = self.path.with_name("explorer.exe")
        second.write_text("test", encoding="utf-8")
        with self.assertRaises(SafetyError):
            WindhawkCli(second, self.runner).disable_our_mod()


if __name__ == "__main__":
    unittest.main()
