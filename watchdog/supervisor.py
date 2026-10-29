"""Secondary recovery supervisor for the isolated MOCK taskbar experiment.

Runs independently from the watchdog and from explorer.exe. It is a prototype,
not a Windows service or security boundary. Secrets are passed via argv ONLY in
the mock harness; production must instead use authenticated protected IPC.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import sys
import time

from watchdog import core

STALE_SECONDS = 1.5


def register(root: Path, trial_id: str, token: str) -> bool:
    with core.state_lock(root):
        record = core.load(root)
        if (record["trial_id"] != trial_id or record["token"] != token or
                record["phase"] != "PREPARED" or core.elapsed(record)):
            return False
        record["supervisor_pid"] = __import__("os").getpid()
        record["supervisor_heartbeat_utc"] = time.time()
        core.atomic_json(core.journal_path(root), record)
        return True


def heartbeat_fresh(record: dict, field: str, now: float | None = None) -> bool:
    now = time.time() if now is None else now
    value = record.get(field)
    return isinstance(value, (int, float)) and 0 <= now - value <= STALE_SECONDS


def tick(root: Path, trial_id: str, token: str) -> bool:
    with core.state_lock(root):
        record = core.load(root)
        if record["trial_id"] != trial_id or record["token"] != token:
            raise core.SafetyError("Supervisor identity mismatch")
        if record["phase"] in core.TERMINAL_PHASES:
            return False
        if record["phase"] in ("CONFLICT", "RECOVERY_FAILED"):
            return False
        if core.elapsed(record):
            core._recover_locked(root, record, "deadline")
            return False
        if record["phase"] == "ACTIVE":
            if not heartbeat_fresh(record, "heartbeat_utc"):
                core._recover_locked(root, record, "watchdog-lost")
                return False
            if not isinstance(record.get("watchdog_pid"), int):
                core._recover_locked(root, record, "watchdog-missing")
                return False
        record["supervisor_heartbeat_utc"] = time.time()
        core.atomic_json(core.journal_path(root), record)
        return True


def main() -> int:
    parser = argparse.ArgumentParser(description="MOCK-only recovery supervisor")
    parser.add_argument("--state-dir", type=Path, required=True)
    parser.add_argument("--trial-id", required=True)
    parser.add_argument("--token", required=True)
    args = parser.parse_args()
    if __import__("os").environ.get("ATB_WATCHDOG_TEST_MODE") != "1":
        print("Recovery supervisor is mock-only", file=sys.stderr)
        return 2
    root = args.state_dir.resolve()
    try:
        if not register(root, args.trial_id, args.token):
            raise core.SafetyError("Supervisor could not register")
        while tick(root, args.trial_id, args.token):
            time.sleep(core.POLL_SECONDS)
    except (core.SafetyError, OSError, ValueError) as exc:
        print(f"Supervisor failure: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
