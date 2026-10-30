# Animated Taskbar — user-approved specification and release gates

## Visual and interaction contract

| Requirement | Current implementation / status |
|---|---|
| Single overlapping taskbar surface, never a separate two-tier dock | **Unresolved.** Native taskbar height remains the user's system height; no independent overhang HWND yet |
| Taskbar system strip height independent of magnification/icon height | **Not implemented.** A pure `taskbar_regions.hpp` model now checks the relationship but cannot change Explorer |
| Native icons rise above the compact taskbar without clipping | **Not implemented.** Native HWND is only 42 physical px in the user's verified 3840×2160 test |
| Smooth native hover magnification, neighbours pushed apart in the same frame | **Coded and compiled**. Bounds and per-frame positioning require the user's visual confirmation |
| No icon overlap, including during faster animations, large scales, bounce | **100,000 native spacing math cases tested**, runtime pointer geometry not yet manually verified |
| Pinned/unpinned separation line absent | We add **no** new separator; any stock separator from another mod is outside this implementation |
| 0 pip for pinned/not running; 1/2/3 pips for 1/2/3 running windows; bar for 4+ | **Coded, unverified on user's Windows build**. Uses group-count symbols adapted from digART's Taskbar Count Badges |
| Retain Start/Search/Task View, grouped previews, Jump Lists, tray, notifications | Native buttons still used. Runtime compatibility not yet manually tested |
| Compatible with installed Taskbar Styler and taskbar sizing mods | Original RenderTransform and native pip appearance are saved/restored conditionally. Other mods may still compete for taskbar hooks. Not verified |
| Native app button hit region follows icon growth; empty overhang click-through | **Not implemented.** Pure geometry mask modeled and tested only |
| 120-second autonomous expiry; never re-arm silently; restore on idle | **Partially coded.** Untrusted external lease and XAML UI timer; no full independently proven recovery for hung Explorer, startup/reboot |
| Source edits in GitHub only | **Mandatory.** Do not execute commands via remote desktop, desktop commander or any remote computer agent |
| CI, package and test artifacts generated in GitHub-hosted Actions | **Implemented** for portable motion/region tests, native compile, isolated PowerShell mock watchdog; monitor CI |
| Manual real-world visual/behavioral testing | **User only.** No ADB, screenshots, or simulated stills as verification |

## Go/no-go

A successful GitHub Actions DLL build is **not** approval to install.
To authorize even a 120-second live trial:
1. Finish a native independent recovery agent and verify Windhawk-version-specific mod disable, journal reconciliation and no changes to other mods.
2. CI failure injection on a disposable Windows runner or VM for watcher killed, failed IPC, blocked Explorer dispatch, corrupted ownership and startup recovery.
3. Separately implement the real native overhang/work-area/hit-test behavior; verify bounds logically and on a disposable Windows environment.
4. Full source review and no automatic restart of Explorer, registration of services/tasks, or changes to system auto-hide without explicit approval.
5. Provide a **PowerShell retrieval/build command for the user to execute**, not remote PC execution. User controls install and visual verification.
6. Preserve `main`. Only merge/publish after the user approves the actual tested UI.

### Known upstream sources

- Ph0en1x-dev native Dock Animation (MIT) supplies native event/render foundation.
- digART Taskbar Count Badges (GPLv3) supplies group-count ABI technique.
- kua8/Nectar (GPLv3) provides architecture reference only (not copied).
- Zach Vaughn MacOS Style Dock (GPLv3) is a relevant native workaround,
  but its auto-hide/appbar approach modifies system behavior and is not reused.

## Current posture

This repository contains **an experimental developer branch, NOT a finished
production Windhawk mod**. Do not promote a DLL or install a mod automatically.

## Confirmed GitHub-hosted Windows result

`tests/win32_disposable_appbar.cpp` successfully registered its **own**
42px appbar reservation while the actual window had 96px physical height.
GitHub Actions reported exactly a 42px desktop work-area delta and passed.
This validates the smaller appbar/larger owned HWND primitive, not the final
native Shell_TrayWnd integration. [CI run 41124137009](https://github.com/MrRiskyClick/Animated-Taskbar-with-Size-and-Icon-Control/actions/runs/41124137009).

## Additional GitHub-only rollback verification

The disposable Windows appbar experiment now also tests that `ABM_REMOVE`
and destroying **its own test HWND** return the monitor's work area exactly
back to its pre-test rectangle (up to 3 seconds to allow recalculation).
It fails CI if the original work area does not return. This is not the
missing independent rollback of the actual Explorer taskbar or Windhawk mod.
