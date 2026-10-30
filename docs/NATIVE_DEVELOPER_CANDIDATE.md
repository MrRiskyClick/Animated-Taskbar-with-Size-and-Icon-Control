# Native animated taskbar — developer test candidate (NOT A RELEASE)

**Branch:** `feature/full-native-mod-integration-v1`

All implementation changes are committed in GitHub. GitHub Actions builds
and tests without accessing the user's PC. This version is an **unapproved
developer candidate** until independently authenticated recovery and
manual UI acceptance are complete.

## Actual code present

- Real `Taskbar.TaskListButton` hover magnification.
- Two-pass, frame-synchronous neighbour displacement based on **current**
  icon sizes and bounce multiplier; animations don't use a lagging
  independent horizontal translation.
- Native grouped-window count lookup + XAML running indicators:
  0 = no pip, 1/2/3 = corresponding dots, 4+ = 22px bar. Original native
  indicator opacity and visual ownership are saved/restored conditionally.
- New optional `EnableNativeOverhang`, **OFF by default**, whose
  activation requires a fresh, matching `ATB2` lease with:
  Explorer PID, live independent watchdog PID, 120s UTC deadline,
  fresh heartbeat, **original native taskbar rectangle**, and
  the exact expected top coordinate.
- On opt-in hover, calls `SetWindowPos(SWP_NOSENDCHANGING)` on the
  **existing native** `Shell_TrayWnd` (no mirrored taskbar), checks
  exact resulting HWND bounds, and confirms **the monitor work area is
  unchanged**. If either invariant fails it immediately attempts restore.
- Expands the native XAML `TaskbarFrame` root to the extended HWND height
  to give transformed controls a potential hit-test surface, while
  bottom-anchoring the original-height `TaskbarBackground`, `ItemsRepeater`
  and `Gripper`. Captures and restores each original Height/transform.
  Still requires manual verification of real XAML pointer interactions.
- A native window region covers the bottom original strip plus only
  the enlarged icon rectangles above it. Empty overhang is outside
  the window region, to avoid stealing pointer events from other apps.
  The region is updated from the *current* animated sizes per frame.
- On lease expiration, mod unload and settings changes: restore original
  XAML transform (if still owned), pip visuals, window region, and
  original HWND rectangle with compare-before-restore. Monitor geometry
  is checked while idle as well as during active animation; a foreign
  region fingerprint aborts cleanup rather than clobbering that mod.
- An **independent, one-shot 120-second PowerShell watchdog**
  `experiments/run-native-overhang-trial.ps1` writes a bounded
  ATB2 lease, revokes it at expiration, and independently restores the
  native window rectangle if it matches the trial's expected geometry.
  The region is restored only when both its ownership marker and
  FNV-1a fingerprint match the actual Win32 region, protecting concurrent
  modifications from other taskbar mods. The watcher also REFUSES to rearm
  an unresolved previous lease. It creates no service or scheduled task.
  On expiry it writes **NO registry settings**. Standard user accounts
  cannot write Windhawk's protected installed-mode HKLM key, and must NOT
  elevate or change permissions. Instead the lease is pinned to the original
  Explorer PID and live watcher process with a 120-second deadline. A
  manual Explorer restart/reboot cannot reuse that live expiring lease.
  The Windhawk UI may still show this mod enabled after expiry, but it is
  inert; the user can switch it off manually. Read-only inspection of
  installed Windhawk 1.7 is required. Portable/custom storage fails closed.

## What GitHub Actions has objectively verified

- Official Windhawk 1.7.3 compiler fully compiled and linked the native mod.
- Portable motion model, 100,000 collision/scale calculations, native
  overhang region geometry tests pass.
- A GitHub-runner-owned disposable HWND can be 96px tall while a 42px
  appbar reservation is reported, and removing it restores the work area.
- More significantly, the **actual native GitHub-hosted `Shell_TrayWnd`**
  can be extended upward using `SWP_NOSENDCHANGING` while preserving
  desktop work area, then restored by another process.
- Parent crash injection: the resizing process exits forcibly; its
  **independent watchdog** restores the native window geometry.
- The PowerShell ATB2 `-SelfTest` mode runs isolated inside TEMP, never
  accesses Explorer, verifies window-region fingerprint conflict detection,
  and finishes with the lease marked `REVOKED`.
- A negative test verifies that a prior unresolved lease cannot silently
  rearm. An additional isolated HKCU registry fixture tests **read-only**
  inspection of the own mod and competing mods, rejecting malformed
  states and ensuring that no Windhawk registry value is modified.

## Manual clipping defect — October 30, 2026

The user completed the first supervised 120-second Windows trial with the
experimental overhang setting enabled, and **observed actual icons still
clipping at the taskbar edge**. The watcher completed normally, revoked the
ATB2 lease, and printed that the native mod was inert. This is a real
behavioral failure; earlier CI geometry probes were not visual proof.

The subsequent unverified code change replaces the fragile depth-1/2
TaskbarBackground/ItemsRepeater/Gripper shifting with the single direct
`RootGrid` child of `TaskbarFrame` (the hierarchy also used by the
upstream Windhawk taskbar-multirow mod). The root frame grows while the
original-height RootGrid is translated down once. For the native Win32
hit-test mask, each live icon uses `TransformToVisual(XamlRoot.Content)`
and `TransformBounds`, avoiding synthetic Y positions and failing
gracefully when transform bounds are unavailable. After a refused first
layout probe, pointer movement retries no more than every 700ms while the
original strict ATB2 lease is valid. More precise refusal logging was added.

**Manual verification is required after the new CI passes.** Do not mark
the clipping defect fixed merely because the source compiles or the
GitHub-hosted native HWND probe passes.

## What is NOT yet verified

- The actual XAML `TaskbarFrame` **painting above the original frame**
  after window enlargement, including its background and system tray.
- Enlarged icon **mouse hit testing**, native app activation, jump lists,
  drag/reorder, grouped previews and system tray behavior. A Win32 window
  region by itself does not prove XAML's nested controls receive input.
- Actual user-provided installed Windhawk mod compatibility. Other
  modules can update XAML or native HWNDs concurrently; this code tries
  not to overwrite externally owned transforms and regions.
- Recovery if `explorer.exe` becomes indefinitely unresponsive.
  The out-of-process watcher uses asynchronous window repositioning;
  a frozen UI may not process it until Explorer resumes. The code
  **does not restart or terminate Explorer**.
- The new external watcher is currently PowerShell + a file lease, **not
  signed/authenticated named-pipe IPC**. It has no durable, integrity-verified
  production journal or guaranteed boot-time restoration.
- User-controlled taskbar **base height** and physical native icon asset
  sizing are not integrated (the overhang retains the system reservation).
- Actual visual/behavioral acceptance. **Only the user tests those.**


**Windhawk version note:** Windhawk 1.7.3 has no supported CLI for disabling
a single mod; its original source stores `Disabled` as a REG_DWORD and
the standard installation uses its `Engine\\Mods` registry configuration.
The independent watcher deliberately does NOT write this key, because
normal users lack permission. Only read-only inspection is supported. The
lease gate (watcher alive, fresh heartbeat, exact Explorer PID and deadline)
prevents reapplication after expiration or a manual Explorer restart.

## User decision: manual restart or manual reboot ONLY

The user will manually restart Windows Explorer or reboot Windows if
absolutely necessary. **No automatic Explorer restart, termination or
forced reboot is permitted.** The mod must not add services, scheduled
tasks, startup entries or settings changes to achieve automatic recovery.

The experimental HWND position, HWND region, XAML transforms and pips
are runtime process/window state; they are not written as permanent
Windows preferences. After manual Explorer restart/reboot, Windows
recreates these objects. The watchdog changes **no persistent Windhawk setting**. Its only
persistent data is the revoked/stale lease in the user's local app-data
folder. The mod can remain shown as enabled in Windhawk, but an expired
or mismatched lease prevents it from running. The user can manually
disable the switch afterward.

**This is an architectural expectation, not a guarantee of zero possible
Windows bugs.** See
[MANUAL_RESTART_RECOVERY.md](MANUAL_RESTART_RECOVERY.md) for exact
state scope, CI assertions and user-controlled recovery steps.

## Release / installation decision

The native source compiles, but the independent recovery gate established in
`docs/SAFETY.md` is still not fully met. Do **not** merge to `main`,
submit to Windhawk, or instruct users to install it as a reliable tool.

For future manual testing, the single-file Windhawk source is:
`experiments/atb-native-visual-trial.wh.cpp`.

The separate geometry watcher is:
`experiments/run-native-overhang-trial.ps1`.

Neither file should be executed automatically by CI on the user's PC.
No automatic activation, background watcher scheduling or Explorer
restarts are authorized. GitHub Actions uses only `-SelfTest` for the
PowerShell script.

A read-only source checkout, if the user chooses, is safe and
does not install or enable a Windhawk mod:

```powershell
$destination = Join-Path (Get-Location) 'Animated-Taskbar-Git'
if (Test-Path -LiteralPath $destination) { throw "Destination already exists: $destination" }
git clone --single-branch --branch feature/full-native-mod-integration-v1 --depth 1 `
  https://github.com/MrRiskyClick/Animated-Taskbar-with-Size-and-Icon-Control.git $destination
git -C $destination status --short
```

**Source checkout only**. It does not change Windhawk, taskbar settings,
Explorer, or any unrelated local files.
