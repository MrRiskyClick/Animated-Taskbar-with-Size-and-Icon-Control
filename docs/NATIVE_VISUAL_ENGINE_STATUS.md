# Native visual developer trial — build and safety status

**Branch:** `feature/native-visual-engine-v1` — development source only.

## What is now implemented

- A real single-file Windhawk C++ mod based on Ph0en1x-dev's MIT-licensed
  Taskbar Dock Animation 1.9.2, not a read-only diagnostic.
- Native `TaskbarFrame::OnPointerMoved/Exited` symbol hooks;
  `Taskbar.TaskListButton` XAML enumeration and per-frame transforms.
- Collision-free neighbour positioning: all scales are updated first, and
  positions are derived from **those same current scales** in the second pass;
  translate is not independently interpolated and low spacing factors cannot
  reduce required spacing. Retains original upstream smooth scale and bounce.
- XAML transform ownership and restoration: store original RenderTransform
  and RenderTransformOrigin, compose the original last, and restore only if
  our own group is still installed (do not overwrite another theme update).
- A single-use 120-second *external lease-gated* activation path. Requires
  a companion independent PowerShell process; checks the watcher PID,
  Explorer PID, 1.8-second heartbeat freshness, UTC deadline and a local
  monotonic maximum deadline. On revocation while rendering, detaches and
  restores on the UI thread. Default/no watchdog: **mod init refuses**.

## What is NOT implemented (must not be claimed)

- Unclipped magnified icons beyond the current 42px native HWND.
- Separate taller visual region with only 42px reserved desktop work area;
  transparent click-through and extended native hit-test are unimplemented.
- Native number-of-open-windows detection or 0/1/2/3/4+ custom pip rendering.
- Fully tested compatibility with Taskbar Styler / Height and Icon Size / tray mods.
- Watchdog **trusted authentication**, code signing, external disable for
  Windhawk 1.x, automatic Explorer restart on frozen UI, boot-time recovery,
  owner-scoped real-taskbar journal or native rollback failure injection.

The PowerShell process **only publishes/revokes the lease**; it does not restart
Explorer or change Windhawk settings. If Explorer is hung and never processes
callbacks, its transient XAML transforms may remain until Explorer recovers
or restarts. For a frozen OS no user-mode timer can guarantee real-time rollback.
This is a **developer-only safety prototype**, not the accepted independent
recovery system. Do **NOT** activate on the primary desktop until the
Stage 1 acceptance/failure-injection gate is completed.

## Compile/syntax validation on authorized Windows desktop

2026-10-30: the repository was cloned into a temporary directory (not installed
as a Windhawk mod) and compiled with the **actual installed Windhawk compiler**:

```powershell
& 'C:\Program Files\Windhawk\Compiler\bin\clang++.exe' `
  '@C:\Program Files\Windhawk\Compiler\compile_flags.txt' `
  '-IC:\Program Files\Windhawk\Compiler\include' -fsyntax-only `
  'experiments\atb-native-visual-trial.wh.cpp'
```

- Initial native engine: syntax-check exit code 0.
- XAML transform ownership changes: syntax-check exit code 0.
- The PowerShell watcher: AST parse error count 0.
- Isolated `-SelfTest` run on Windows: output confirmed initial lease, 2.5s
  deadline, revocation and exit code 0. Its final fixture file contained
  `REVOKED`; no real `%LOCALAPPDATA%\AnimatedTaskbarTrial\lease.v1`
  was written during the self-test.

**These do not establish any live animation, clipping, button functionality
or real-world restoration result.** User manually validates UI/behavior.
The developer did not enable a taskbar-changing mod or touch current Windhawk
mods. A real Windhawk compile within its UI may also expose loader errors
not caught by C++ syntax-check.

## Development test source

- `experiments/atb-native-visual-trial.wh.cpp`
- `experiments/run-visual-trial-watchdog.ps1`
- `src/core/dock_motion.hpp` portable algorithm from earlier stage
- `tests/dock_motion_tests.cpp` collision-free numerical testing

## Remaining gates

1. Independent deterministic rollback proof under helper kill, Explorer
   crash/freeze, mod unload, settings changes and UI blocked during expiry.
   Disable own mod without touching others; no silent re-arm after restart.
2. Trial visual region with work-area separation and measured click-through.
3. Exact live grouped-window-count mapping, custom pip geometry/ownership.
4. Cross-mod compatibility (Taskbar Styler, original animation standalone
   disabled, height/icon-size, tray, Start and notification stylers) and
   monitors/DPI/fullscreen.
5. User manual visual/interaction acceptance; then Windhawk publication review.

**Do not suggest that the timer itself is a completed independent rollback.**
