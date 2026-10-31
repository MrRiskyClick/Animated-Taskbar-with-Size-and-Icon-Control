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
- Native group-window lookup and 0/1/2/3/4+ pip **code exists** in the integration branch, but it has not been visually or functionally verified on the user's Windows build. The handling of secondary monitors, taskbar recycling and other pip mods remains unverified.
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
- Full compiler + linker: exit code 0, generated 821,248-byte DLL in a separate TEMP working directory; the DLL was NOT loaded into Explorer.
- The PowerShell watcher: AST parse error count 0.
- Native two-pass geometry: 100,000 randomized variable-icon-width, up-to-250%-scale and bounce-aware layouts PASS using a static Windows test executable (exit code 0).
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

## GitHub-only integration update — October 30, 2026

On `feature/full-native-mod-integration-v1`:
- Native pip code uses optional Windows 11 TaskListGroupViewModel symbol lookup
  and a 0/1/2/3/4+ renderer, restoring the original indicator on cleanup
  while attempting not to overwrite another mod's intervening style updates.
- Native XAML dispatcher timer monitors lease loss even when pointer movement
  stops; UI callbacks and our transforms are detached on its owning thread.
- `src/core/taskbar_regions.hpp` models visual overhang separately from the
  original taskbar's desktop reservation, and tests which *future* regions
  should accept mouse input. **It is not yet connected to Explorer**.
- GitHub Actions builds and links the mod with a checksum-pinned official
  Windhawk 1.7.3 compiler, runs portable motion/overhang tests and an isolated
  mock Windows watchdog test.
- `docs/NECTAR_ARCHITECTURE_REVIEW.md` records why Nectar's mirrored
  replacement dock architecture cannot be directly reused for our
  native-controls requirement.
- GPLv3/MIT upstream attributions are in `experiments/THIRD_PARTY_NOTICES.md`.

**Release blockers:** independent real watchdog/rollback still incomplete;
separate unclipped native visual window and hit-testing not implemented; custom
pip and cross-mod behavior require manual Windows testing. Do NOT describe this
as a finished full mod or suggest installing on the user's primary PC.
All development/code changes and automated tests must occur solely through
GitHub and GitHub Actions, not through remote desktop connectors or any
commands run remotely on the user's device.
