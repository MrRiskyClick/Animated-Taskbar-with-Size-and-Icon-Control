# Nectar architecture review — 2026-10-30

**Reference:** [kua8/Nectar](https://github.com/kua8/Nectar)
**License:** GPLv3. Reviewed source only; nothing copied.

## Summary

Nectar is a **separate Tauri/Rust + React dock application**. It hides the
Windows taskbar and *mirrors* its pinned/running applications, rather than
moving `Taskbar.TaskListButton` native XAML controls. It therefore cannot be
dropped into our Windhawk-native implementation without breaking the native
button / context menu / preview / tray ownership requirements.

However, its division of responsibilities is a useful design reference:

| Concern | Nectar technique | Applicability |
|---|---|---|
| Rendering outside the native taskbar HWND | Own borderless dock WebView/window | Architecture only; native XAML task buttons cannot be blindly reparented across separate windows |
| Workspace reservation | `register_dock_appbar_inner`: `SHAppBarMessage(ABM_NEW/ABM_QUERYPOS/ABM_SETPOS)` on fixed mode; separately positions other visibility modes | Confirm that reserved strip and rendering region must be managed separately; don't change user auto-hide setting or appbar bounds as a shortcut |
| Mouse input and click-through | `set_ignore_cursor_events` and `WS_EX_TRANSPARENT` on Nectar-owned windows, with dynamic input detection | Reusable conceptual pattern for a *future* optional visual-only overhang host, but native hit-target ownership remains unresolved |
| Recovery | `restore_taskbar_and_exit` on normal close; taskbar-hidden marker checked on startup, then `restore_taskbar_after_crash` | Reinforces fail-closed journals + independent supervisor, but startup-only restoration is not an independently guaranteed 120-second rollback |
| Multi-monitor | Separate dock windows selected per monitor | Track monitor/DPI and taskbar HWND per display; cannot infer all displays from primary 3840×2160 inspection |
| Pinned/running | Own mirrored app model; import from pinned .lnk files | Reject for our project: do not replace native Windows state with a parallel app model |

## Specific useful implementation: appbar reservation smaller than window

In Nectar's `src-tauri/src/services.rs::register_dock_appbar_inner`,
`ph` is the **actual physical height** of its dock window, while `pr`
is a smaller **reserved strip** (56 logical units multiplied by the Nectar
scale and monitor DPI scale). The code:

1. Registers the dock's own HWND using `ABM_NEW`.
2. Calls `SHAppBarMessage(ABM_QUERYPOS)` and `ABM_SETPOS` with a rectangle
   only `pr` pixels high.
3. Independently calls `SetWindowPos` to place the actual HWND at the bottom
   with height `ph`.

This is **strong evidence that reservation and visual window geometry can
be deliberately decoupled for a user-owned independent dock window**.
It is *not* evidence that we can perform those same operations on
Windows' own `Shell_TrayWnd`, because Windows shell code owns and continually
recalculates the native taskbar window's position and appbar registration.

Our next safe prototype is a disposable **test HWND owned by the test process
on a GitHub-hosted Windows runner**. It must not touch
`Shell_TrayWnd`, taskbar auto-hide, pinned apps or Windhawk installation.
Only after successful independent recovery design should a separate native
Shell_TrayWnd integration ever be attempted.

## Verified implementation points

- `src-tauri/src/main.rs`: startup crash marker + normal window close
- `src-tauri/src/commands.rs`: `init_dock_window`, `set_taskbar_visibility`,
  `restore_taskbar_and_exit`
- `src-tauri/src/services.rs`: `register_dock_appbar_inner`,
  `unregister_appbar_native`, mouse-pass-through in the window style guard
- `src/Dock.tsx`: separate React dock with its own app model

## Decisions for Animated Taskbar

1. **Retain native XAML taskbar controls.** We are building a Windhawk mod,
   not a standalone taskbar replacement. No `ShowWindow(SW_HIDE)` on the
   native taskbar, no mirrored pin database.
2. **Do not automatically enable Windows taskbar auto-hide** or use an
   additional appbar that changes the user's desktop reservation.
3. Separate four outputs in code and tests:
   - **reservedStripPx**: configured Windows shell desktop reservation
   - **visualOverhangPx**: amount icons need above the strip
   - **hitRegion**: interactive icon bounds in the overhang (requires native
     hit targets, not only graphics)
   - **clickThroughRegion**: empty overhang to fall through to underlying apps.
4. We have not found a safe Windows API combination that keeps the *native*
   button tree working above its 42px HWND while independently reserving
   exactly 42px, without taking ownership from another taskbar mod.
   This is a **real unresolved implementation** and must remain an explicit
   release blocker. Do not fake it with `Clip=null` (no HWND effect).
5. Use **Nectar's recovery concepts only**: separate identity/version checks,
   deadline, independent watchdog and owner-scoped compare-before-restore. No
   code copied from Nectar; licensing remains GPLv3 from digART integration.

## Follow-up research, GitHub-only

- Inspect [MacOS Style Dock](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/macos-style-dock.wh.cpp),
  which actually keeps native buttons and provides 'magnify over windows',
  but does so by toggling native auto-hide and managing appbar reservation.
  That is an architectural reference, **not compatible as-is** with the
  user's don't-change-existing-mods and compact reserved taskbar constraints.
- Define and CI-test pure geometry for strip/overhang/hit-region independence.
- Prototype any live Win32/XAML window geometry only under an independently
  proved native watchdog, and only in manually user-approved trial builds.

**No external code was copied from Nectar into the mod.**

## GitHub-only follow-through

Implemented a **disposable owned-HWND appbar proof** at
`tests/win32_disposable_appbar.cpp`, executed only on GitHub-hosted Windows
runners. It registers an owned, temporary HWND as a 42px bottom appbar
reservation but positions its actual window to 96px; it prints whether the
work area reflects the 42px strip, then unregisters its own appbar and
destroys the HWND on exit. Where GitHub's session lacks an interactive shell
or appbar service, it clearly prints `SKIP` or `INCONCLUSIVE`; it must
never report that it verified real `Shell_TrayWnd` without having done so.

This narrows a necessary OS primitive but does **not** prove the native
Windows taskbar can safely take that geometry, that XAML content will render
above its frame, or that native hit-testing will follow enlarged buttons.
The native Explorer modification remains disabled pending the recovery gate.
