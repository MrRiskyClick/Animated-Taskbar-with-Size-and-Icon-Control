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
- Moves the native XAML `TaskbarFrame` to the lower edge of the extended
  window, without changing the original taskbar reserved strip.
- A native window region covers the bottom original strip plus only
  the enlarged icon rectangles above it. Empty overhang is outside
  the window region, to avoid stealing pointer events from other apps.
  The region is updated from the *current* animated sizes per frame.
- On lease expiration, mod unload and settings changes: restore original
  XAML transform (if still owned), pip visuals, window region, and
  original HWND rectangle with compare-before-restore.
- An **independent, one-shot 120-second PowerShell watchdog**
  `experiments/run-native-overhang-trial.ps1` writes a bounded
  ATB2 lease, revokes it at expiration, and independently restores the
  native window rectangle if it matches the trial's expected geometry.
  Its region cleanup only applies when the owner marker matches. This
  watchdog creates no service, scheduled task or registry setting.

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
  accesses Explorer, and finishes with the lease marked `REVOKED`.

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
