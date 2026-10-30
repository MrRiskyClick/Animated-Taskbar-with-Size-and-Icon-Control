# Native taskbar inspection — actual test evidence (2026-10-30)

## Verified on user's PC

This report records **user-provided Windhawk debug logs**, not a device run
performed by the developer or by automation. The user manually hovered the
taskbar. Visual/behavioral verification remains user-owned.

| Timestamp | Observed event |
|---|---|
| 16:56:50.343 | Primary taskbar `Shell_TrayWnd`=(0,2118)-(3840,2160); work=(0,0)-(3840,2118) |
| 16:56:50.351 | Native inventory init entered |
| 16:57:26.514 | Taskbar.View.dll pointer hook registered after ~36 seconds of symbol resolution |
| 16:57:28.310 | `Taskbar.TaskbarFrame` found; read-only XAML traversal started |
| 16:57:28.319 | Survey completed: 69 nodes, six `Taskbar.TaskListButton` elements, 13 taskbar-class nodes, zero explicitly set `Clip` properties reported on relevant visited elements |

### Actual dimensions

- Screen: **3840 x 2160 physical px**
- Taskbar HWND: **3840 x 42 px**, bottom-aligned.
- Primary monitor work area: **3840 x 2118 px**.
- `Taskbar.TaskbarFrame`: **3840 x 42**.
- `Taskbar.TaskbarBackground`: **3840 x 42**.
- `Microsoft.UI.Xaml.Controls.ItemsRepeater`: **3840 x 42**.
- Two `Taskbar.ExperienceToggleButton` elements: **43 x 42**, **42 x 42**.
- Six `Taskbar.TaskListButton` elements: **42 x 42**.
- `Taskbar.Gripper`: **120 x 24**.

This successfully verifies native taskbar symbol resolution, pointer-hook
registration, UI-thread XAML traversal and inspection on this Windows build.
**It does NOT verify that writing transforms, custom pips, extended hit regions,
or independent automatic recovery work**.

### What zero `Clip` reports means

The inspector read `FrameworkElement::Clip` and logged no explicit
geometry for inspected relevant elements. It did **not** inspect implicit
scroll-viewport/Composition/Win32 HWND clipping, and the 42px HWND still
bounds native rendering. Do **not** assert that clipping is disabled.

## Upstream technique and critical tradeoff

An independently developed experimental integrated
[MatheusSMA theme](https://github.com/MatheusSMA/matheussma-theme) documents
the two coordinated operations needed to keep magnified native icons visible:

1. Hook `TaskbarConfiguration::GetFrameSize` to make the taskbar's native
   window taller than the original frame.
2. Pin the native buttons, taskbar background and system tray controls to
   the original-height strip (otherwise controls stretch to fill the new frame).

Its README explicitly states that **the enlarged taskbar also reserves extra
desktop work area**. It additionally discusses transform ownership collisions
when both Windows 11 Taskbar Styler and the dock animation modify the same XAML
controls. This is *evidence of a possible clipping mitigation, not a solution*
to our separate reserved work-area requirement.

Other references:
- https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-dock-animation.wh.cpp
- https://github.com/MatheusSMA/matheussma-theme/blob/main/parts/glue.cpp
- https://github.com/MatheusSMA/matheussma-theme/blob/main/parts/dock-adaptation.cpp

### Architecture acceptance constraints

- One **visually overlapping** taskbar, not a detached/two-tier dock.
- Keep appbar reservation equal to the compact **configured system bar**
  height. Extending the visual HWND must not silently make maximized windows
  stop higher on the display.
- Allow **unclipped app icon visuals AND native pointer interactions** in
  the extension. Transparent unused regions need proper click-through.
- Preserve native tray, clock, notifications, task grouping, Jump Lists,
  previews and styles owned by other mods.
- One owner per native `RenderTransform`; do not run two competing
  magnification engines.
- Existing portable motion core computes spacing from **current scales per
  frame** (not independent position interpolation). Do not claim native
  integration before XAML bindings have been tested.
- Native running-window counts/indicators not yet available from the inspector;
  intended 0=none, 1..3=count, >=4=bar remains a requirement.
- DPI: physical-pixel HWND observations are not interchangeable with XAML DIPs.
  The successful 42px observations here do not prove all DPI configurations.

### Next coding milestone

**Complete the real independent recovery gate in Stage 1**, then implement
native visual changes on an isolated development branch.

Recommended order:
1. Confirm Windhawk edition/version and Windows build while preserving user mods.
2. Build a trusted native watchdog bound to a single-use session token and
   a real Windows property inverse journal. Ensure no auto-rearm on restart.
3. Test deadline, Explorer crash/hang, watchdog-loss, reboot/startup and
   nonclobbering restore behavior *without* mutating production taskbar state.
4. Prototype reversible increased visual height + bottom-pinned XAML controls,
   **with reserved work-area preservation and click-through** under the trusted
   lease, separately from the animation itself.
5. Connect magnification + grouped pip state; exercise user-owned compatibility
   and manual visual tests. Never claim success from static captures.

**No active taskbar-changing prototype is cleared for installation yet.**
