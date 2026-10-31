> **SAFETY HOLD — October 30, 2026:** The v0.3.3 live taskbar trial made application icons disappear; disabling the mod did not recover them. The user manually restarted Explorer to restore normal operation. **All live testing steps below are superseded and MUST NOT be executed.** v0.3.4 and the updated normal watchdog fail closed. See [incident report](./INCIDENT_2026-10-30_LIVE_TASKBAR_RECOVERY.md). Keep the user's installed mod OFF. No automatic Explorer restarts, admin elevation or remote PC control.

# Manual Explorer / reboot recovery contract

**Decision (October 30, 2026):** The user explicitly rejected automatic
Explorer restart, process termination and automatic PC reboot. If required,
**the user alone** will restart Explorer or reboot Windows.

## The only intended persistent data from the 120-second developer trial

The watcher makes **no Windhawk registry writes**, because installed-mode HKLM keys are protected from unprivileged users. The mod's loader requires a freshly updated lease with its exact Explorer PID, a live watchdog process, and a strict expiration time. After restart/reboot, the old lease is invalid and cannot activate the mod. Windhawk's UI may continue to show the mod as enabled; it is inert until a new supervised lease. The user can disable it manually.

A revoked/expired `ATB2` file in
`%LOCALAPPDATA%\AnimatedTaskbarTrial\lease.v1` is also retained to block
silent rearming until a new trial is deliberately prepared and verified.
The file is not a Windows boot-time launcher and does not install a service,
scheduled task or driver.

**No other persistent taskbar or Windows system setting is part of the
mod's approved mutation scope.** In particular: no auto-hide preference,
Windows work-area/appbar registration, taskbar alignment, pinned icons,
running app grouping, system tray preferences, global Windhawk safe-mode,
other Windhawk mod's configurations, Windows Explorer startup policy,
or boot-time tasks may be changed.

## State recovery by mechanism

| Trial modification | Storage | Why manual restart/reboot restores it |
| --- | --- | --- |
| `Shell_TrayWnd` rectangle | Live window state in Explorer | Explorer creates a new taskbar window after process restart |
| `SetWindowRgn` interactive pixels | Live HWND region | Region belongs to the destroyed window |
| Extra XAML frame dimensions and children transforms | XAML objects in Explorer | Original XAML objects are destroyed when Explorer exits |
| Hover animation and custom running pips | In-process Windhawk/XAML objects | New session starts without a valid ATB2 trial |
| Our mod's Windhawk enabled switch | Persistent Windhawk configuration, **not modified** | May remain enabled; native mod rejects stale/missing PID-bound lease |
| ATB2 lease and lock files | Per-user local app data | Expired/revoked leases are rejected on load |

These behaviors are supported by normal Windows process/window lifetime
semantics and repository source inspection, but **cannot guarantee that an
arbitrary third-party mod, Windows shell bug or system corruption is
impossible**. The user must visually verify behavior on their build.

## If the Windows taskbar behaves abnormally during a controlled trial

If Windhawk's UI is responsive, disable **Animated Taskbar Native Hover
(Developer Trial)**. Do not disable other mods.

If Explorer needs recovery, the user may open Task Manager
(Ctrl+Shift+Esc), locate **Windows Explorer**, and select **Restart task**.
If the computer is not responsive enough, the user may choose to reboot.
**Nothing in the trial automatically restarts Explorer or the computer.**

After recovery: verify the regular taskbar height, ability to click Start,
running app controls and tray, and that other Windhawk mods remain enabled.
Do not re-enable the experimental mod until logs and original behavior are
reviewed.

## What the independent watchdog does when Explorer freezes

1. Revoke the ATB2 lease; any responsive mod stops its active changes.
2. Revoke the PID-bound trial lease; a fresh Explorer process cannot reapply the mod without a new lease.
3. Attempt compare-before-restore of its own taskbar window/region.
4. If Explorer cannot process the restoration, **report the failure** and
   leave Explorer running. There is no automatic restart or termination.
5. A later manual Explorer restart or reboot recreates the native window.
   Without a fresh PID-matched ATB2, the experiment must not reapply even if the Windhawk UI switch remains enabled.

## Release gate

Before any installation instructions are issued, GitHub Actions must pass:
- Source restrictions against registry writes in the native DLL,
  appbar/auto-hide modification and Explorer/reboot termination.
- Read-only own-mod and competing-mod registry fixture, rejecting write access requirements and preserving all values.
- A **new-process** fixture checking that our mod remains disabled,
  representing the persistent state that survives restart.
- Real disposable-runner native taskbar geometry + external watchdog
  recovery, including a forced trial-controller exit.
- Genuine official Windhawk compiler/linker build.

Manual UI tests on the user's computer are still required for rendered
overhang, hit-testing, pinned icons, previews, Jump Lists, app tray and
other Windhawk mod interaction. CI cannot prove visual safety.

**No automatic recovery escalation that restarts/terminates Explorer is
authorized. This contract overrides earlier optional escalation ideas.**
