"""Versioned, bounded, read-only lease-file transport for experimental IPC.

The watchdog publishes a public (non-secret) health observation. This is
NOT authorization to mutate Windows; no production C++ mod should trust a
writable file as authentication or sole lease source. Only MOCK tests use it.
"""
from __future__ import annotations
import os
from pathlib import Path
import re
import tempfile
import time

from watchdog.core import SafetyError

LEASE_FILENAME = "lease.v1"
MAX_LEASE_AGE_MS = 1500
MAX_LEASE_SPAN_MS = 120000
ID_RE = re.compile(r"^[0-9a-f]{32}$")


def lease_path(root: Path) -> Path:
    return root / LEASE_FILENAME


def _millis(seconds: float) -> int:
    return int(seconds * 1000)


def encode_lease(record: dict, sequence: int) -> bytes:
    if record.get("phase") != "ACTIVE":
        raise SafetyError("Lease export only allowed for active trials")
    trial_id = record.get("trial_id")
    if not isinstance(trial_id, str) or not ID_RE.fullmatch(trial_id):
        raise SafetyError("Invalid trial ID")
    if not isinstance(sequence, int) or sequence < 1:
        raise SafetyError("Invalid sequence")
    created = _millis(record["created_utc"])
    deadline = _millis(record["deadline_utc"])
    beat = _millis(record["heartbeat_utc"])
    if not (0 < deadline - created <= MAX_LEASE_SPAN_MS):
        raise SafetyError("Invalid trial deadline")
    if not (created <= beat < deadline + 2000):
        raise SafetyError("Heartbeat out of trial bounds")
    pid = record.get("watchdog_pid")
    if type(pid) is not int or pid <= 0:
        raise SafetyError("Invalid watchdog PID")
    # Exact key order matches the C++ parser; no JSON dependency in Explorer.
    return (
        "ATB_LEASE_V1\n"
        f"id={trial_id}\n"
        f"pid={pid}\n"
        "phase=ACTIVE\n"
        f"created_ms={created}\n"
        f"deadline_ms={deadline}\n"
        f"heartbeat_ms={beat}\n"
        f"sequence={sequence}\n"
        "END\n"
    ).encode("ascii")


def publish_lease(root: Path, record: dict, sequence: int):
    """Atomic replacement of short observer record; journal remains authority."""
    path = lease_path(root)
    data = encode_lease(record, sequence)
    if len(data) > 512:
        raise SafetyError("Lease exceeds maximum size")
    root.mkdir(parents=True, exist_ok=True)
    fd, tmp_name = tempfile.mkstemp(prefix=".lease-", dir=str(root))
    try:
        with os.fdopen(fd, "wb") as handle:
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(tmp_name, path)
    finally:
        if os.path.exists(tmp_name):
            os.unlink(tmp_name)


def revoke_lease(root: Path):
    # A failed unlink must not count as a successful revoke; the C++ side also
    # enforces freshness and expiry even if a stale lease file is left behind.
    try:
        lease_path(root).unlink()
    except FileNotFoundError:
        return
