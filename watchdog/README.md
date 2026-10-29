# Stage 1 — independent watchdog prototype (MOCK ONLY)

This directory contains executable **Python 3.12+ standard-library** code that starts a separate process and enforces a test expiry on a single mock-only file called `mock-taskbar.json`.

**This does not yet satisfy the requirement to automatically disable the Windhawk mod or recover a crashed Explorer. DO NOT install it as a Windows taskbar safety solution.** It has NO Explorer, registry, XAML, Windhawk, process termination or real taskbar writes.

## Design details

- `core.prepare()` records expected + original state and a deadline **before** touching a mock value.
- `cli.start` launches the watchdog in a separate process and waits for a positive readiness acknowledgement; it refuses an unarmed mock mutation.
- `watch` periodically checks both wall-clock and monotonic deadlines and refreshes a journal heartbeat. The default deadline is 120 seconds.
- `recover()` performs compare-before-restore: it restores the baseline only when the mock value still equals this trial's applied value; if modified by someone else it leaves it untouched and records `CONFLICT`.
- A journal persists on disk using write-and-fsync, then same-directory replace; a process-independent OS file lock protects concurrent access.
- A corrupt or unresolved journal blocks new trials. Re-arming after expiry requires an explicit new command. `lease_is_healthy` provides a fail-closed read-only guard prototype for the future Explorer integration.
- Timer reduction for CI is gated on `ATB_WATCHDOG_TEST_MODE=1`; no test command accepts any path to arbitrary real taskbar files.

## Running local tests (no Windows side effects)

```powershell
$env:ATB_WATCHDOG_TEST_MODE="1"
py -3 -m unittest discover -s tests -p "test_watchdog.py" -v
Remove-Item Env:\ATB_WATCHDOG_TEST_MODE
```

For a manual mock demonstration **from the repository root**:

```powershell
$env:ATB_WATCHDOG_TEST_MODE="1"
$folder = Join-Path $env:TEMP "animated-taskbar-watchdog-demo"
py -3 -m watchdog.cli init --state-dir $folder --height 44
py -3 -m watchdog.cli start --state-dir $folder --height 78
py -3 -m watchdog.cli status --state-dir $folder
# After ~120 seconds, status should read EXPIRED and mock height restored.
py -3 -m watchdog.cli recover --state-dir $folder
Remove-Item Env:\ATB_WATCHDOG_TEST_MODE
```

Do not use an existing trial directory: `init` refuses overwrites. The developer is not required to run this yet: GitHub Actions runs the mock tests.

## Unfinished production safety gates

1. Implement a **native Windows watchdog** or packaged runtime that does not depend on a machine's Python setup.
2. Bind the actual Windhawk mod and watchdog using authenticated versioned IPC, stable trial IDs, and a heartbeat lease checked before mutations. The current Windhawk mod remains inert and does not consume this journal.
3. Add durable, reversible adapters **only** for identified taskbar-owned properties. Snapshot/write/inverse operations need precise registry/XAML/geometry ownership rules; never snapshot or reset unrelated theme mods.
4. Verify a Windhawk-version-aware, supported **automatic disable** mechanism. Windhawk 2.0 alpha appears to expose a CLI, but that does not establish a safe, stable command for the user's installed version; do not blindly write Windhawk's private registry.
5. Prove startup/reboot reconciliation, watchdog restart handling, and Explorer crash/hang recovery. A killed watchdog currently leaves the mock active until explicit recovery; future mod-side lease must fail closed and a startup mechanism must resolve stale trials.
6. Isolate / test state journal directories under per-user permissions, atomicity across power loss, process startup failures, and write failures. Windows `os.replace` and `fsync` have filesystem and power-loss limitations.
7. Test on an expendable Windows environment before live taskbar changes. Do not enable active shell modifications until all Stage 1 failure gates pass.

## Status

Mock-only CI coverage is useful evidence for the *journal and external process logic*, not proof of Windows taskbar restoration.

## Stage 1b Windhawk CLI research

Windhawk 2.x has an official `windhawk-cli.exe mod disable <id>` command. `windhawk_control.py` is an isolated, *dry-run-by-default* adapter that only permits our exact mod ID and refuses Windhawk 1.x/unknown CLI versions. **It is not called by the expiry loop** and unit tests use a fake runner. See [Windhawk integration design](../docs/WINDHAWK_INTEGRATION.md). Never infer that this adapter gives the existing 1.x installation an automatic 2-minute restore.

## Stage 1b observer file and C++ bridge

While the **mock** trial is active, the independent watchdog publishes `lease.v1`, a tiny, fixed-order, ASCII lease observation that the read-only native observer (`src/core/lease_v1.hpp`) can check. It contains the trial ID, deadline, heartbeat freshness, PID and sequence. It is revoked on normal rollback; stale records expire if the watchdog dies.

The Windhawk diagnostic mod reads an optionally configured file and trial ID. The one-file bundle is generated by `python scripts/bundle_windhawk.py`. CI tests the Python publisher, C++ parser, and the bundle's absence of direct shell mutation. **No real Windows taskbar changes, mod disable or trusted IPC integration are implemented.**

## Stage 1c — mock dual-process failure recovery

`watchdog/cli.py start` now launches a dedicated `watchdog.supervisor` process and confirms its heartbeat **before** launching the primary watchdog and applying any mock change. Each process watches the other's heartbeat and the trial deadline. Killing either one is expected to trigger automatic mock restoration by the other. The helper is not installed as a Windows service and has no boot-time recovery integration.

This eliminates the earlier *single process killed → always requires manual mock recovery* weakness in the common one-process-failure case, but **it does not provide guaranteed 120-second recovery for Windows**. If both processes die or a machine reboot interrupts the trial, manual reconciliation is still needed. A real mod must use authenticated, permission-scoped IPC, and capture/undo exactly the Windows properties it changes.
