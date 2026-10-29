# Test strategy

## Stage 0 — automated, non-Windows

Build the pure C++ guard tests with any C++17 compiler:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic -I. tests/trial_guard_tests.cpp -o trial_guard_tests
./trial_guard_tests
```

Tests validate expiry, reversibility, LIFO rollback, partial failure and no reapplication. **They do not test Explorer or Windhawk.**

## Stage 1 — fault injection (Windows test environment)

- Confirm watchdog process is **outside Explorer**, rearming requires explicit user initiation, and arbitrary mod disable can be effected by a verified Windhawk interface.
- Test no-op journal before any actual native taskbar mutation.
- Kill Explorer during trial; simulate a hung Explorer; kill/restart watchdog; restart Windows while armed; disconnect second monitor; change DPI.
- Verify no silent re-arming on Explorer relaunch and no persistent register/height/auto-hide overrides.

## Functional/manual validation by user

Real UI/animation/navigation must be checked **by the user** on the desktop. An agent may build, read logs and compute objective timing/work-area rectangle measurements, but static screenshots cannot establish animation correctness, click-through or native interaction.

Test cases:
- Native pin/unpin; grouping and ungrouped windows; multi-window hover previews and Aero Peek.
- Right-click Jump List, middle click new window, drag reorder, keyboard focus and Win shortcuts.
- Native system tray including Wi-Fi, volume, notifications, clock and tray overflow.
- Hover magnification at 100–250%, rapid cursor movement, rightmost/leftmost icons, DPI transitions; no clipping.
- Click-through between icons/above Taskbar 1 while maximized applications are underneath; inspect previews/menus while dock animates.
- Multi-monitor work areas and different scaling; connect/disconnect; taskbar only on some displays; exclusive fullscreen.
- Mod enable/expire/unload, coexisting Windhawk Taskbar Styler, height/icon-size and Dock Animation disabled to prevent conflicting simultaneous effects.
- Explorer crash/rollback and timeout while taskbar is idle or animating.

## Evidence recording

For each test report Windows version + build, Windhawk version, existing enabled mods and versions, primary/secondary monitor resolution/scaling, configured 120s trial ID, baseline work-area rectangle, post-change rectangle, post-rollback rectangle, plus logs and the user's pass/fail for visual/interaction tests. Never assert "fully restored" from logs alone.
