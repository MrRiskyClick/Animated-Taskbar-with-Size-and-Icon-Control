# Safety and rollback specification — mandatory before shell mutation

## Hard requirement

When the experimental mod is enabled, its active changes **automatically expire after exactly 120 seconds (monotonic deadline)**. There is no indefinite "keep changes" action in the initial testing phase. Leaving the mod listed as enabled inside Windhawk is not equivalent to continuing the experiment; after expiration the mod must be **inert**, and it must not reapply after restart unless explicitly armed for a new trial.

A countdown **inside Explorer alone is unsafe**. It cannot fire if Explorer hangs/crashes. A proper rollback requires an independent watchdog that monitors an external deadline and persisted trial state.

## Two-layer recovery architecture (to implement)

### Layer 1: mod-local guard

- On load, do not mutate anything until successful handshake with watchdog and receipt of a single-use trial ID.
- Validate supported Windows build, symbols, monitor topology and reversible operations.
- Register inverse operation in a journal **before** each modification.
- On expiry, mod unload, settings change, exception or explicit abort: stop applying changes and revert in reverse order.
- Avoid detached threads accessing unloaded code; Windhawk lifecycle/re-entry and Explorer restart must be tested.

### Layer 2: independent watchdog

- Launch/arm **outside `explorer.exe`** before the mod can apply changes. Heartbeat alone is not sufficient; persist `deadline_utc` and a monotonic/signed or elapsed-time equivalent with boot identity.
- Journal in a per-user durable path with atomic writes and restricted permissions: baseline settings + exact scope and expected post-values, operation IDs, trial ID, state and rollback results. Protect against stale/corrupt journal. Do not assume arbitrary process-local XAML pointers survive Explorer restart.
- At the 120s deadline, if heartbeat missing, if Explorer dies, or on watchdog restart with an expired/unresolved trial: force the feature into a **disabled/unarmed** state using a *verified documented control channel*. Test actual Windhawk disable handling: no presumed CLI/API.
- Revert persisted OS settings and work-area effects only if they still match expected trial-owned values; prevent clobbering concurrent user changes.
- An Explorer restart can reinitialize transient XAML/layout state **after** disabling reapplication, but may disrupt apps and is an escalation path, not guaranteed comprehensive restoration.
- If the independent watchdog itself terminates, safe operation must be fail-closed; the mod must detect lost watchdog/expired lease and self-revert if responsive. If both watchdog and Explorer are suspended, no software-only 120-second guarantee is possible; document residual risks.

## Snapshot scope: possible vs impossible

| State | Strategy | Confidence |
|---|---|---|
| Our mod's own settings, hooks, injected XAML style and transforms | Record values/ownership; undo in reverse or unload hooks | Possible, requires Windows build validation |
| Our changes to taskbar height, monitor work area, alignment, auto-hide | Persist **original** values, compare before restore, force shell recalculation | Feasible in principle; must test |
| Pinned items, taskbar grouping, system tray user prefs | **Do not touch** these in experiments; do not promise to snapshot all shell state | Avoid mutation |
| Arbitrary Windows internals, other third-party mod changes and open application window state | No complete reliable snapshot API; may be affected by shell restart | Not guaranteed |

## Transaction journal state machine

```text
IDLE -> PREPARED (snapshot captured) -> ARMED (watchdog confirms lease)
  -> APPLYING -> ACTIVE -> REVERTING -> REVERTED -> IDLE
                         └─(error/TTL)-> REVERTING
PREPARED/ARMED after restart => ABORT/ROLLBACK, never auto-resume
Unknown/corrupt state => block mutations, enter SAFE MODE and report
```

Use absolute journal status transitions with durable writes. Non-reentrant rollback. Idempotent inverse operations. Restore only verified trial-owned values. Preserve failure logs for diagnostics.

## Acceptance before Stage 3

- 120-second timer applies to **all** active experimental changes.
- Independent process stays alive after Explorer failure and can disable reapplication.
- Demonstrated rollback after Explorer crash, Explorer hang, watchdog crash/restart, logout and multi-monitor change, as technically possible.
- Test manual rollback on a separate test Windows session; visible taskbar restored, correct work area, original system tray, pinned applications unchanged.
- No unapproved scheduled startup task/service and no permanent registry patch.
- If unable to reliably stop reapplication, **do not ship active shell mutation**. Manual Windhawk disable/restart is only an emergency instruction, not a substitute for the 120s safeguard.

## Stage 0 status

Only a **non-mutating diagnostic prototype** and simulated rollback unit tests may ship at this point. They are explicitly not a production watchdog or comprehensive Windows savestate.

## External mod settings and manual recovery

When restoring our experiment, never overwrite the saved settings of Windows 11 Taskbar Styler, Start Menu Styler, Notification Center Styler, Taskbar tray system icon tweaks, Taskbar height/icon-size or the original Ph0en1x-dev Dock Animation. Original theme/background/pip/clock/system tray settings are user-owned. If an unguarded hook collision would risk losing their state, stop the trial before applying changes.

A **manual Windows System Restore point** is a valuable third-layer escape hatch (including via WinRE after a boot failure), but can be absent or fail and does not guarantee complete restoration of all mod data or personal files. It **must not be used to justify** disabling the independent 120-second expiration/recovery requirement. See [EMERGENCY_RECOVERY.md](EMERGENCY_RECOVERY.md).

## Stage 1a implementation audit (2026-10-29)

**Present on `feature/stage1-watchdog-prototype`:**
- External subprocess watchdog with cross-process file lock and flushed, replace-based JSON journal.
- 120-second production-default wall-clock + monotonic expiration (test-only shortened deadlines).
- Acknowledged readiness before isolated mock modification; original and expected values persisted first.
- Compare-before-restore mock rollback, conflict detection, idempotent recovery, fail-closed lease health helper, refusal to rearm unresolved or corrupt journals.
- Windows and Linux CI tests passed for these mock-only paths.

**NOT PRESENT / not allowed for production:**
- Mod communication or automatic Windhawk disable.
- Windows native shell state capture/inverse operations, taskbar/registry writes, actual taskbar recovery.
- Watchdog crash auto-respawn and reboot reconciliation, real mod-side heartbeat enforcement, durable integrity hardening across arbitrary power loss.
- Failure injection on a live Windows Explorer shell, Windows build compatibility allowlist and complete rollback verification.

Even with tests passing, **Stage 1 full safety gate is OPEN**. No source file may add active native taskbar mutation until the missing items are implemented and tested.

## Stage 1c — independent mock recovery supervisor

The mock harness now arms an **independent recovery supervisor before it launches the watchdog and before it changes the mock target**. The supervisor monitors the watchdog journal heartbeat and 120-second deadline. The watchdog monitors the supervisor heartbeat in return. If either process is killed or stops sending heartbeats, the surviving process attempts compare-before-restore of the mock original value, revokes the lease observation, and prevents automatic rearming. Tests intentionally terminate each of the two processes independently on Windows and Linux.

**Not a production guarantee:** If both processes die or Windows loses power, no process is left to perform the mock rollback until a separate startup reconciliation mechanism runs. Both processes still share a writable mock journal and pass a trial token in command-line arguments, which is NOT authenticated IPC or appropriate secret handling. Native Explorer layout changes, true Windows state snapshots, actual Windhawk disable and watchdog installation are **not implemented**. The Stage 1 safety gate remains OPEN.
