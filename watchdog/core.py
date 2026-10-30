"""Independent trial journal and mock-only recovery adapter.

NOT a Windows taskbar snapshot or Windhawk disable implementation. The only
modifiable 'target' is mock-taskbar.json INSIDE the user-selected state dir.
"""
from __future__ import annotations

import contextlib
import json
import os
from pathlib import Path
import tempfile
import time
import uuid
from watchdog import mock_credentials

if os.name == "nt":
    import msvcrt
else:
    import fcntl

VERSION = 2
PRODUCTION_SECONDS = 120
ACTIVE_PHASES = frozenset(("PREPARED", "READY", "ACTIVE", "RECOVERY_FAILED", "CONFLICT"))
TERMINAL_PHASES = frozenset(("EXPIRED", "RECOVERED"))
POLL_SECONDS = 0.20
MAX_TIME_DRIFT = 0.5


class SafetyError(RuntimeError):
    """Fail-closed journal or recovery error."""


@contextlib.contextmanager
def state_lock(root: Path):
    """Cross-process OS-managed lock: released when a process dies."""
    root.mkdir(parents=True, exist_ok=True)
    with (root / ".trial.lock").open("a+b") as fh:
        fh.seek(0, os.SEEK_END)
        if fh.tell() == 0:
            fh.write(b"0")
            fh.flush()
        fh.seek(0)
        if os.name == "nt":
            msvcrt.locking(fh.fileno(), msvcrt.LK_LOCK, 1)
        else:
            fcntl.flock(fh.fileno(), fcntl.LOCK_EX)
        try:
            yield
        finally:
            fh.seek(0)
            if os.name == "nt":
                msvcrt.locking(fh.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(fh.fileno(), fcntl.LOCK_UN)


def atomic_json(path: Path, value: dict):
    """Flush and replace a same-directory temp file; no destructive in-place edit."""
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, tmp_name = tempfile.mkstemp(prefix=".pending-", dir=str(path.parent))
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            json.dump(value, f, sort_keys=True, separators=(",", ":"))
            f.write("\n")
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp_name, path)
        # On POSIX, flush directory entry after rename. Windows does not allow
        # a portable Python fsync on directory handles.
        if os.name != "nt":
            directory_fd = os.open(path.parent, os.O_RDONLY)
            try:
                os.fsync(directory_fd)
            finally:
                os.close(directory_fd)
    finally:
        if os.path.exists(tmp_name):
            os.unlink(tmp_name)


def read_json(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (FileNotFoundError, OSError, ValueError) as exc:
        raise SafetyError(f"Missing/corrupt state: {path.name}") from exc
    if not isinstance(value, dict):
        raise SafetyError(f"Invalid state object: {path.name}")
    return value


def journal_path(root: Path) -> Path:
    return root / "trial.json"


def target_path(root: Path) -> Path:
    return root / "mock-taskbar.json"


def load(root: Path) -> dict:
    record = read_json(journal_path(root))
    if record.get("schema") != VERSION or record.get("phase") not in (
        ACTIVE_PHASES | TERMINAL_PHASES
    ):
        raise SafetyError("Unknown journal version/phase; modifications blocked")
    expected = record.get("expected")
    baseline = record.get("baseline")
    if (
        not isinstance(expected, dict)
        or not isinstance(baseline, dict)
        or not isinstance(record.get("deadline_utc"), (int, float))
        or not isinstance(record.get("deadline_mono"), (int, float))
        or not isinstance(record.get("created_mono"), (int, float))
        or not isinstance(record.get("trial_id"), str)
        or not isinstance(record.get("token_digest"), str)
        or "token" in record
    ):
        raise SafetyError("Malformed journal; modifications blocked")
    return record


def initialize_mock(root: Path, original_height: int):
    """Set up an isolated fixture; cannot overwrite an existing trial."""
    if type(original_height) is not int or not 16 <= original_height <= 160:
        raise SafetyError("Mock height out of range")
    with state_lock(root):
        if journal_path(root).exists() or target_path(root).exists():
            raise SafetyError("State already exists; refusing overwrite")
        atomic_json(target_path(root), {"height": original_height})


def prepare(root: Path, requested_height: int, *, test_duration: float | None = None) -> dict:
    """Durably capture fixture baseline BEFORE launching any watchdog/apply."""
    if type(requested_height) is not int or not 16 <= requested_height <= 160:
        raise SafetyError("Mock height out of range")
    if test_duration is not None and os.environ.get("ATB_WATCHDOG_TEST_MODE") != "1":
        raise SafetyError("Test durations are disabled outside mock-only test mode")
    duration = float(test_duration) if test_duration is not None else PRODUCTION_SECONDS
    if duration <= 0 or duration > PRODUCTION_SECONDS:
        raise SafetyError("Invalid deadline")
    with state_lock(root):
        if journal_path(root).exists():
            previous = load(root)
            if previous["phase"] not in TERMINAL_PHASES:
                raise SafetyError("Outstanding trial/conflict; recover before rearming")
        # Revoke any prior observer lease before preparing a new trial.
        from watchdog import lease_protocol
        lease_protocol.revoke_lease(root)
        baseline = read_json(target_path(root))
        if set(baseline) != {"height"} or type(baseline["height"]) is not int:
            raise SafetyError("Unexpected mock baseline; no change permitted")
        now_utc, now_mono = time.time(), time.monotonic()
        token = mock_credentials.new_token()
        record = {
            "schema": VERSION,
            "trial_id": uuid.uuid4().hex,
            "token_digest": mock_credentials.digest(token),
            "phase": "PREPARED",
            "baseline": baseline,
            "expected": {"height": requested_height},
            "created_utc": now_utc,
            "created_mono": now_mono,
            "deadline_utc": now_utc + duration,
            "deadline_mono": now_mono + duration,
            "heartbeat_utc": None,
            "watchdog_pid": None,
            "result": None,
        }
        atomic_json(journal_path(root), record)
        return {**record, "token": token}


def elapsed(record: dict, now_utc: float | None = None, now_mono: float | None = None) -> bool:
    now_utc = time.time() if now_utc is None else now_utc
    now_mono = time.monotonic() if now_mono is None else now_mono
    return (
        now_utc >= record["deadline_utc"]
        or now_mono >= record["deadline_mono"]
        or now_mono < record["created_mono"] - MAX_TIME_DRIFT
    )


def ready(root: Path, trial_id: str, token: str) -> bool:
    with state_lock(root):
        record = load(root)
        if record["trial_id"] != trial_id or not mock_credentials.matches(token, record["token_digest"]):
            return False
        if record["phase"] != "PREPARED" or elapsed(record):
            return False
        if record.get("supervisor_pid") is not None and not _supervisor_fresh(record):
            return False
        record["phase"] = "READY"
        record["watchdog_pid"] = os.getpid()
        record["heartbeat_utc"] = time.time()
        atomic_json(journal_path(root), record)
        return True


def guarded_mock_apply(root: Path, trial_id: str):
    """Test-only mutation; never calls a Windows taskbar API."""
    if os.environ.get("ATB_WATCHDOG_TEST_MODE") != "1":
        # Even a production 120s run cannot mutate actual system state here.
        raise SafetyError("Mock application requires explicit test-mode environment")
    with state_lock(root):
        record = load(root)
        if record["trial_id"] != trial_id or record["phase"] != "READY" or elapsed(record):
            raise SafetyError("Watchdog not ready, or trial expired; refusing mutation")
        if record.get("supervisor_pid") is not None and not _supervisor_fresh(record):
            raise SafetyError("Recovery supervisor not healthy; refusing mutation")
        actual = read_json(target_path(root))
        if actual != record["baseline"]:
            raise SafetyError("Pre-change baseline drifted; refusing mutation")
        # The durable journal already contains expected and inverse before edit.
        atomic_json(target_path(root), record["expected"])
        record["phase"] = "ACTIVE"
        atomic_json(journal_path(root), record)


def _recover_locked(root: Path, record: dict, reason: str) -> dict:
    """Compare-before-restore: never overwrite someone else's changed value."""
    if record["phase"] in TERMINAL_PHASES:
        return record
    actual = read_json(target_path(root))
    if actual == record["baseline"]:
        record["phase"] = "EXPIRED" if reason == "deadline" else "RECOVERED"
        record["result"] = "Already original; no changes needed"
    elif actual == record["expected"]:
        try:
            atomic_json(target_path(root), record["baseline"])
        except OSError:
            record["phase"] = "RECOVERY_FAILED"
            record["result"] = "Mock restore write failed; retry needed"
        else:
            record["phase"] = "EXPIRED" if reason == "deadline" else "RECOVERED"
            record["result"] = "Verified mock value restored"
    else:
        record["phase"] = "CONFLICT"
        record["result"] = "Target changed externally; left untouched"
    record["heartbeat_utc"] = time.time()
    atomic_json(journal_path(root), record)
    from watchdog import lease_protocol
    lease_protocol.revoke_lease(root)
    return record


def recover(root: Path, reason: str = "manual") -> dict:
    with state_lock(root):
        record = load(root)
        return _recover_locked(root, record, reason)


def _supervisor_fresh(record: dict, max_age: float = 5.0) -> bool:
    beat = record.get("supervisor_heartbeat_utc")
    if not isinstance(record.get("supervisor_pid"), int):
        return False
    if not isinstance(beat, (int, float)):
        return False
    age = time.time() - beat
    return 0 <= age <= max_age


def tick(root: Path, trial_id: str, token: str) -> bool:
    """Watchdog keeps a bounded heartbeat; True => continue watching."""
    with state_lock(root):
        record = load(root)
        if record["trial_id"] != trial_id or not mock_credentials.matches(token, record["token_digest"]):
            raise SafetyError("Watchdog trial identity mismatch")
        if record["phase"] in TERMINAL_PHASES or record["phase"] in ("CONFLICT", "RECOVERY_FAILED"):
            from watchdog import lease_protocol
            lease_protocol.revoke_lease(root)
            return False
        if elapsed(record):
            _recover_locked(root, record, "deadline")
            return False
        if record.get("supervisor_pid") is not None and not _supervisor_fresh(record):
            _recover_locked(root, record, "supervisor-lost")
            return False
        record["heartbeat_utc"] = time.time()
        atomic_json(journal_path(root), record)
        # Export a read-only health snapshot for future native lease probes.
        # Never treat this file as an authorization credential.
        from watchdog import lease_protocol
        if record["phase"] == "ACTIVE":
            try:
                next_sequence = int(record.get("lease_sequence", 0)) + 1
                record["lease_sequence"] = next_sequence
                atomic_json(journal_path(root), record)
                lease_protocol.publish_lease(root, record, next_sequence)
            except (OSError, SafetyError, ValueError):
                _recover_locked(root, record, "lease-publish-failed")
                raise SafetyError("Lease publishing failed; mock trial reverted")
        return True


def lease_is_healthy(root: Path, trial_id: str, max_heartbeat_age: float = 1.5) -> bool:
    """Future integration read gate: must be checked BEFORE every mutation.

    An active heartbeat is a necessary but not sufficient safety guarantee:
    the watchdog could die immediately after this returns. Windows shell
    mutations remain forbidden until a production mod-side lease/IPC exists.
    """
    try:
        with state_lock(root):
            record = load(root)
            beat = record["heartbeat_utc"]
            return (
                record["trial_id"] == trial_id
                and record["phase"] == "ACTIVE"
                and isinstance(beat, (int, float))
                and 0 <= time.time() - beat <= max_heartbeat_age
                and not elapsed(record)
            )
    except (SafetyError, OSError):
        return False

def reconcile_stale(root: Path) -> tuple[dict | None, bool]:
    """Recover an expired/abandoned mock trial before a future launch.

    Returns (record, did_attempt_recovery). Never arms a new trial.
    An unresolved corrupted journal fails closed and is not overwritten.
    This is invoked on a future start, NOT a boot-time scheduled service.
    """
    if not journal_path(root).exists():
        return None, False
    with state_lock(root):
        record = load(root)
        if record["phase"] in TERMINAL_PHASES:
            return record, False
        if record["phase"] in ("CONFLICT", "RECOVERY_FAILED"):
            raise SafetyError("Unresolved recovery/conflict needs investigation")
        deadline = elapsed(record)
        now = time.time()
        lost_both = (
            record["phase"] == "ACTIVE"
            and isinstance(record.get("heartbeat_utc"), (float, int))
            and isinstance(record.get("supervisor_heartbeat_utc"), (float, int))
            and now - record["heartbeat_utc"] > 5.0
            and now - record["supervisor_heartbeat_utc"] > 5.0
        )
        startup_abandoned = (
            record["phase"] in ("PREPARED", "READY")
            and isinstance(record.get("created_utc"), (float, int))
            and now - record["created_utc"] > 5
        )
        if deadline or lost_both or startup_abandoned:
            return _recover_locked(root, record, "reconcile"), True
        return record, False
