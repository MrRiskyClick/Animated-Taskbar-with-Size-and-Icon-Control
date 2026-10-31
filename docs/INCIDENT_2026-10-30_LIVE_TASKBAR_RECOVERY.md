# INCIDENT — Live Windows taskbar app icons disappeared; recovery failed
**Status:** OPEN / NO LIVE TESTING ALLOWED
**Observed:** 2026-10-30, at ~21:45 local time
**Affected native mod:** `local@atb-animated-taskbar-trial` (v0.3.3)
**User-reported resolution:** Manually restarting Windows Explorer restored the icons.
**Permitted engineering scope:** GitHub source and hosted CI only. No remote access to the user's Windows computer, no automatic Explorer restart, process termination, or reboot.

## Directly observed evidence

The user supplied a screenshot of the tray with Start, clock and system tray
still visible but **app/tasklist icons missing**. The mod was manually
disabled, but the icons did not return. A **manual Explorer restart** restored
the taskbar.

DbgViewMini output from explorer.exe PID 7180:

- `21:45:48.381`: watcher accepted; animation hooks installed.
- `21:45:49.000`: original HWND region present, mode=1,
  fingerprint `7967201477181557329`.
- `21:45:49.179`: XAML `RootGrid` moved downward by 66 DIP,
  frame height changed from 42 to 108 DIP.
- `21:45:49.351`: native overhang activated, 66 px; claimed
  reserved desktop unchanged at initialization.
- `21:45:49.468`: UI timer detected native geometry **or** work-area mismatch;
  immediately attempted rollback.
- `21:45:49.468`: rollback logged
  `Region ownership no longer verified; preserved foreign state`
  then `Native overhang restoration requested`. There was no
  `Original taskbar region restored exactly` message.
- `21:45:50.584`: module reported stale/missing/dead watcher lease.
- `21:45:54.511`: mod unload reported XAML transforms/pips restored.

PowerShell normal-run output displayed `Geometry trial armed, ... expiration
120000 ms`, then printed trial expired/revoked and ended with
`Atomic lease update failed. Win32: 5`.

**Interpretation limits:** The exact geometry/region discrepancy and the
underlying cause of Win32 error 5 have not been isolated. The source's
`AtbNativeWorkareaStillPreserved()` combines work-area and HWND checks
into one condition, so the log cannot distinguish them. The displayed
`120000 ms` describes the configured deadline, **not proof the process
survived for two minutes**; the native logs show authorization became
invalid much earlier. The source's unconditional completion message can
also be misleading when an exception is already in flight.

## Proven safety design defect in v0.3.3

In `AtbRestoreNativeOverhang()`, if the temporary region's
`AtbIsOurRegion()` / `AtbOurRegionFingerprintMatches()` check fails,
the code removes `ATB_REGION_FINGERPRINT_V2` and
`ATB_OWNED_OVERHANG_V2` **without restoring the previous region**.

The independent PowerShell watchdog's `Stop-NativeTrial()` refuses to
restore its saved original HRGN unless the exact matching
`ATB_OWNED_OVERHANG_V2` marker and region fingerprint are still present.
Consequently, the in-process error branch can remove the watchdog's only
authority to restore a region, leaving the taskbar with a potentially
incorrect clipping mask.

This design flaw follows directly from the source. It is consistent with
the screenshot and logs, though the exact region geometry at the moment
of failure was not captured. **It cannot be considered safe rollback.**

Microsoft documents that `SetWindowRgn` determines the area in which a
window may draw and emits `WM_WINDOWPOSCHANGING` /
`WM_WINDOWPOSCHANGED`. Calling it on Explorer's native
`Shell_TrayWnd` is not equivalent to an isolated test HWND:
https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowrgn

## Immediate containment

Native trial version **0.3.4** introduces an unconditional early
`return FALSE` at the beginning of `Wh_ModInit`, before lease arming,
XAML callbacks, native hooks or HWND changes. The matching normal-mode
PowerShell watchdog throws `SAFETY HOLD` before loading Win32 helpers,
opening the lease journal or touching Explorer; `-SelfTest` can still run
against disposable, process-owned windows and isolated temp files.

These updates only affect the GitHub branch. They **do not disable or patch
the already compiled v0.3.3 mod on the user's PC**. User should leave the
installed mod OFF; do not conduct another taskbar trial or manually change
Windows/registry permissions.

## Exit criteria before contemplating another live experiment

1. Redesign so taskbar icon visibility and original `Shell_TrayWnd` region
   cannot become dependent on an optimistic fingerprint and
   `SetWindowPos` shell relayout. Prefer a reversible, independently owned
   surface rather than modifying the primary taskbar clipping region.
2. Separate and log actual HWND rectangle and monitor work-area deltas.
   Treat `SetWindowRgn` window-position messages as a first-class input.
3. Complete and transactionally validate XAML root-frame/child restoration,
   *including* app-icon/task-list visibility after a failed update. Never
   report restored from only a successful C++ call.
4. Demonstrate that loss of any one of lease write, timer, Explorer
   responsiveness, region ownership marker, original-region fingerprint, or
   window update does NOT remove the only independent restoration path.
5. Error-inject `MoveFileEx` access denied (Win32 5), stale heartbeats,
   partial ownership-marker loss, shell-initiated window region changes,
   and monitor/work-area mismatch on a Windows-hosted **disposable**
   environment. A `-SelfTest` that bypasses live-path rollback is not
   sufficient.
6. Test a safe, user-authorized manual-recovery procedure. Do not restart
   Explorer automatically. No real-device test without explicit new
   user approval and an independently reviewed recovery gate.
7. Keep the PR draft and main branch unchanged; never claim that green
   GitHub CI proves native Explorer safety.

## Safety conclusion

**No further live ATB3 native overhang tests on the user's PC.**
The 120-second timeout is not by itself an independent, reliable recovery
mechanism. Fixing a single fingerprint check or retrying lease writes is not
enough to justify re-enabling the experiment.
