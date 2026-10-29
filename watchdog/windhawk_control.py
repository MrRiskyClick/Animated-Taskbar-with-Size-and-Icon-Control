"""Conservative Windhawk 2.x CLI control adapter.

This module is NOT connected to the Explorer mod or to the watchdog loop.
No actions are performed on import. A disable action is permitted ONLY for
our exact mod ID, after a verified Windhawk 2.x CLI probe; never edit Windhawk
private registry values or modify other Windhawk mods.

Windhawk 1.x cannot use this adapter: unsupported -> fail closed.
"""
from __future__ import annotations

import os
from pathlib import Path
import re
import subprocess
from typing import Callable, Sequence

from watchdog.core import SafetyError

OUR_MOD_ID = "animated-taskbar-size-icon-control"
VERSION_PATTERN = re.compile(r"(?i)\bwindhawk-cli(?:\.exe)?\s+2\.[0-9]+(?:\.[0-9]+)?(?:[-+][\w.\-]+)?\b")


def run_command(argv: Sequence[str], timeout: float) -> subprocess.CompletedProcess:
    return subprocess.run(list(argv), capture_output=True, text=True,
                          check=False, timeout=timeout, shell=False)


class WindhawkCli:
    def __init__(self, executable: Path, runner: Callable = run_command):
        self.executable = Path(executable)
        self.runner = runner

    def probe(self) -> str:
        if not self.executable.is_file():
            raise SafetyError("Windhawk 2.x CLI executable not present")
        if self.executable.name.casefold() != "windhawk-cli.exe":
            raise SafetyError("Refusing unrelated executable")
        try:
            response = self.runner([str(self.executable), "--version"], 4)
        except (OSError, subprocess.TimeoutExpired) as exc:
            raise SafetyError("Windhawk CLI version probe failed") from exc
        if response.returncode != 0:
            raise SafetyError("Windhawk CLI version probe returned failure")
        version = response.stdout.strip()
        if not VERSION_PATTERN.search(version):
            raise SafetyError("Windhawk CLI is not an identified 2.x version")
        return version

    def disable_our_mod(self, mod_id: str = OUR_MOD_ID, *, execute: bool = False) -> list[str]:
        """Return a verified command, or execute *only after explicit opt-in*.

        A successful subprocess exit is NOT proof that shell changes have been
        reversed. Production caller must verify mod disabled and own rollback.
        """
        if mod_id != OUR_MOD_ID:
            raise SafetyError("Cannot disable other installed Windhawk mods")
        self.probe()
        argv = [str(self.executable), "mod", "disable", OUR_MOD_ID]
        if not execute:
            return argv

        # This extra gate is intentionally not exposed by the watchdog CLI.
        if os.environ.get("ATB_EXPLICIT_OWN_MOD_DISABLE") != OUR_MOD_ID:
            raise SafetyError("Explicit own-mod disable authorization absent")
        try:
            response = self.runner(argv, 8)
        except (OSError, subprocess.TimeoutExpired) as exc:
            raise SafetyError("Disabling mod failed or timed out") from exc
        if response.returncode != 0:
            raise SafetyError("Windhawk CLI rejected disable operation")
        return argv
