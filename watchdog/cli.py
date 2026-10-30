"""CLI for a mock-only independent watchdog prototype.

Commands are sandbox-only. No Explorer hooks, registry writes, Windhawk mod
enable/disable, or user taskbar changes. The watchdog is a *separate process*.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time

from watchdog import core, mock_credentials


def print_state(record: dict):
    output = dict(record)
    # Token is a local trial capability, not appropriate for routine logs.
    output.pop("token", None)
    print(json.dumps(output, indent=2, sort_keys=True), flush=True)


def _detached_process(argv: list[str], token: str) -> subprocess.Popen:
    kwargs = {"stdin": subprocess.PIPE,
              "stdout": subprocess.DEVNULL,
              "stderr": subprocess.DEVNULL}
    if os.name == "nt":
        kwargs["creationflags"] = (subprocess.DETACHED_PROCESS |
                                   subprocess.CREATE_NEW_PROCESS_GROUP)
    else:
        kwargs["start_new_session"] = True
    process = subprocess.Popen(argv, **kwargs)
    mock_credentials.send(process.stdin, token)
    return process


def run_start(root: Path, height: int, duration: float | None) -> int:
    if os.environ.get("ATB_WATCHDOG_TEST_MODE") != "1":
        raise core.SafetyError(
            "Stage-1 prototype only runs with ATB_WATCHDOG_TEST_MODE=1; "
            "no Windows shell integration exists"
        )
    prior, reconciled = core.reconcile_stale(root)
    if reconciled:
        raise core.SafetyError(
            "Previous abandoned trial was reconciled; inspect restored state "
            "and explicitly invoke start again. No automatic rearm.")
    record = core.prepare(root, height, test_duration=duration)
    try:
        # The recovery supervisor starts and positively acknowledges BEFORE
        # the watchdog or any simulated mock state mutation is allowed.
        supervisor = _detached_process([
            sys.executable, "-m", "watchdog.supervisor",
            "--state-dir", str(root.resolve()), "--trial-id", record["trial_id"],
            ], record["token"])
        ready_deadline = time.monotonic() + 5
        while time.monotonic() < ready_deadline:
            if supervisor.poll() is not None:
                raise core.SafetyError("Independent recovery supervisor terminated early")
            with core.state_lock(root):
                current = core.load(root)
                if (current.get("supervisor_pid") == supervisor.pid and
                        core._supervisor_fresh(current)):
                    break
            time.sleep(0.025)
        else:
            raise core.SafetyError("Independent recovery supervisor readiness timed out")

        child = _detached_process([
            sys.executable, "-m", "watchdog.cli", "watch",
            "--state-dir", str(root.resolve()), "--trial-id", record["trial_id"],
            ], record["token"])
        ready_deadline = time.monotonic() + 5
        while time.monotonic() < ready_deadline:
            if child.poll() is not None or supervisor.poll() is not None:
                raise core.SafetyError("Safety process terminated before readiness")
            with core.state_lock(root):
                current = core.load(root)
                if (current["phase"] == "READY" and
                        current["watchdog_pid"] == child.pid and
                        current.get("supervisor_pid") == supervisor.pid and
                        core._supervisor_fresh(current)):
                    break
            time.sleep(0.025)
        else:
            raise core.SafetyError("Independent watchdog readiness timed out")
        core.guarded_mock_apply(root, record["trial_id"])
    except Exception:
        # Trial is blocked and mock restoration attempted even after partial
        # startup failures. This is not a Windows shell recovery mechanism.
        core.recover(root, "failed-start")
        raise
    with core.state_lock(root):
        current = core.load(root)
    print_state(current)
    return 0

def run_watch(root: Path, trial_id: str, token: str) -> int:
    if not core.ready(root, trial_id, token):
        raise core.SafetyError("Watchdog could not acquire the prepared trial")
    while core.tick(root, trial_id, token):
        time.sleep(core.POLL_SECONDS)
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Experimental MOCK-ONLY 120-second rollback watchdog"
    )
    sub = parser.add_subparsers(dest="action", required=True)
    for action in ("init", "start", "watch", "status", "recover", "reconcile"):
        cmd = sub.add_parser(action)
        cmd.add_argument("--state-dir", required=True, type=Path)
        if action == "init":
            cmd.add_argument("--height", type=int, default=42)
        if action == "start":
            cmd.add_argument("--height", type=int, required=True)
            cmd.add_argument("--test-duration", type=float, default=None)
        if action == "watch":
            cmd.add_argument("--trial-id", required=True)
            # Trial credential is read from child stdin, not from argv.

    args = parser.parse_args(argv)
    root = args.state_dir.resolve()
    try:
        if args.action == "init":
            if os.environ.get("ATB_WATCHDOG_TEST_MODE") != "1":
                raise core.SafetyError("Mock initialization is test-mode only")
            core.initialize_mock(root, args.height)
            print("Initialized isolated mock target; no taskbar changes.")
        elif args.action == "start":
            return run_start(root, args.height, args.test_duration)
        elif args.action == "watch":
            return run_watch(root, args.trial_id, mock_credentials.receive(sys.stdin.buffer))
        elif args.action == "status":
            with core.state_lock(root):
                print_state(core.load(root))
        elif args.action == "recover":
            print_state(core.recover(root, "manual"))
        elif args.action == "reconcile":
            record, was_recovered = core.reconcile_stale(root)
            if record is None:
                print("No trial journal exists; no action.")
            else:
                print_state(record)
                print(f"Reconciliation attempted: {was_recovered}")
    except (core.SafetyError, OSError, ValueError) as exc:
        print(f"FAIL CLOSED: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
