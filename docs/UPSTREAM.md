# Upstream Windhawk research and proposed reuse

**Research index, 2026-10-29.** Links refer to upstream source, not code already copied into this repository. Confirm source revision and license before transplanting any code. Many sources are **GPL-3.0**, which has distribution/source obligations. No third-party source has yet been copied.

| Project / exact source | Candidate techniques to reuse or adapt | Boundary / caution |
|---|---|---|
| [Taskbar height and icon size (m417z), `taskbar-icon-size.wh.cpp`](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-icon-size.wh.cpp) | Windows 11 native frame height, button/icon sizing, DPI correction, taskbar re-measure techniques; inspect hooks affecting `taskbar.dll` and `Taskbar.View.dll` | Reuse only validated hooks with original licenses/attribution; our work-area reservation is **not** assumed equal to render height |
| [Taskbar Dock Animation (Ph0en1x-dev), `taskbar-dock-animation.wh.cpp`](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-dock-animation.wh.cpp) | Find native button host and XAML elements; per-monitor contexts; hover magnet/radius, scale, easing, render-frame callbacks, animation settings | **Known icon clipping**; do not copy the clipping defect; investigate host/parent clip chain first |
| [Windows 11 Taskbar Styler (m417z), `windows-11-taskbar-styler.wh.cpp`](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/windows-11-taskbar-styler.wh.cpp) and [styling guide](https://github.com/ramensoftware/windows-11-taskbar-styling-guide) | XAML element selectors and visual-state style application; running/active/attention pip `RunningIndicator`; persistent weak refs and styling callbacks | Style layout is not the same as moving a native task list outside its HWND; investigate unsupported Windows builds |
| [Floating Dock Helpers & Auto-Hide Motion](https://github.com/jinSeong-P/windhawk-floating-dock/tree/main/mods) | Transparent empty-region click-through, visual transforms, layout/hit-test update following animation, tray vs app group positioning, stable app-button interactions | GPL-3.0; built for different docking goals; never copy its required auto-hide scheme uncritically |
| [Taskbar Dock Mode (Synapse-L)](https://github.com/Synapse-L/taskbar_dock_mode/blob/main/taskbar-dock-mode.wh.cpp) and [README](https://github.com/Synapse-L/taskbar_dock_mode) | Work-area reservation mechanisms, native auto-hide state, Explorer shell hooks, restored original state on unload | Its design removes **all** reserved area; our Taskbar 1 must reserve a configurable **nonzero** height. Undocumented hooks, Windows update risk |
| [Multirow taskbar (m417z)](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-multirow.wh.cpp) | Native app list XAML tree traversal and task-list layout without replacing Windows buttons | Row layout is not identical to split render region |
| [Multi-tray taskbar](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-multi-tray.wh.cpp) | Defensive XAML extraction, read-compare-write UI updates, delayed taskbar creation, hotplug/monitor handling and per-taskbar hit testing | Large complex sample; use conceptual patterns only until relevant source and license reviewed |
| [Windhawk mod creation docs](https://github.com/ramensoftware/windhawk/wiki/Creating-a-new-mod) | Mod metadata, settings, `Wh_ModInit`, `Wh_ModUninit`, mod lifetime and settings changes | Mod unload is not a substitute for an **independent** watchdog or durable journal |

## Candidate source sections — work orders

1. **Sizing**: review `TaskbarHeight`, `IconSize`, `TaskbarButtonWidth`, and small-taskbar fields in `taskbar-icon-size.wh.cpp`; identify the minimum hook set for reserved height, separate from button visuals.
2. **Animation**: review `DockAnimationContext`, `TaskbarIconInfo`, `IconAnimState`, rendering callback subscription, cursor distance calculations and bounce/scale. Determine which transforms cause parent clipping.
3. **Pips**: inspect XAML selector `Taskbar.TaskListLabeledButtonPanel@RunningIndicatorStates > Rectangle#RunningIndicator` and its `ActiveRunningIndicator`, `InactiveRunningIndicator`, `RequestingAttentionRunningIndicator` states as documented by styling guide. Reuse behavior, alter only geometry.
4. **Click-through**: compare layout passes and hit-test rectangles from Floating Dock Helpers against Win32 layered/region hit-testing. Require proof that tooltips and Jump Lists remain correct.
5. **Work-area**: compare Synapse-L use of `SHAppBarMessage(ABM_SETSTATE, ABS_AUTOHIDE)` and internal hide hooks; document it as **reference only**, since toggling user auto-hide has side effects and doesn't naturally provide small nonzero reservation.
6. **Recovery**: explicitly audit Wh_ModUninit-style cleanup upstream; *none of these tools proves full system-state rollback*. Build durable independent recovery from first principles.

## License and copying policy

- Each upstream project's license must be verified at the pinned source commit. GitHub's UI default license labels do not by themselves license snippets from all contributors.
- Existing m417z and other upstream mods include GPLv3 notices. When substantially adapting or combining GPL code, distribute the derivative under GPL-compatible terms and preserve copyright notices, source and license.
- This first branch uses **original, non-copied scaffolding** only. License selection for any GPL-derived implementation must be completed before integration.
- Keep an eventual `THIRD_PARTY_NOTICES.md` with source URL, source commit SHA, file/line/function name, license and transformations.
