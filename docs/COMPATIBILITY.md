# Windhawk compatibility contract and test matrix

Updated 2026-10-29. This is a **requirement and verification plan**, not a claim that any combination has been tested or is currently compatible.

## Guiding principle

The project exists because **Ph0en1x-dev's Taskbar Dock Animation** has the desired macOS-like native taskbar icon magnification, but icons clip when the taskbar is compact. Preserve and improve its hover interaction rather than replacing it with an unrelated animation.

The user's existing Windhawk appearance mods are first-class compatibility targets. The project must not require permanently removing the chosen Start, Notification Center, Taskbar Styler or system-tray modifications just to obtain magnification.

**Important distinction:** These mods remain separately installed, configurable, and updatable; they are NOT bundled wholesale into our mod. We may adapt specific verified source techniques after licensing and attribution review. Reusing animation functionality internally means **only one animation engine should control the same native button transforms** at a time.

## Required integration references

| Mod | Official mod ID / source | Host process | Intended integration / owner | Conflict risk |
|---|---|---|---|---|
| **Taskbar Dock Animation** — Ph0en1x-dev | [taskbar-dock-animation](https://windhawk.net/mods/taskbar-dock-animation); [source](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-dock-animation.wh.cpp) | `explorer.exe` | Primary animation UX reference: radius/distance-based magnification, easing, scaling, icon motion. Audit clipping and retain desired feel. **No double animation** when our engine is active. | **Very high** — both may transform the same buttons and manipulate pointer/animation frame state. |
| **Windows 11 Taskbar Styler** — m417z | [windows-11-taskbar-styler](https://windhawk.net/mods/windows-11-taskbar-styler); [source](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/windows-11-taskbar-styler.wh.cpp) | `explorer.exe` | User owns themes, brushes, backgrounds, pip geometry/colors, native XAML styling. Our mod must preserve styles and visual states as much as possible. | **High** — same native XAML controls, transforms, layout and resources. |
| **Windows 11 Start Menu Styler** — m417z | [windows-11-start-menu-styler](https://windhawk.net/mods/windows-11-start-menu-styler); [source](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/windows-11-start-menu-styler.wh.cpp) | `StartMenuExperienceHost.exe`, `SearchHost.exe`, `SearchApp.exe` | Do not modify its process/styles; ensure Start/search entry points, focus, positioning and mouse hit-testing continue to work. | **Lower direct hook overlap**, but **moderate interaction risk** for popups, focus, z-order and anchors. |
| **Windows 11 Notification Center Styler** — m417z | [windows-11-notification-center-styler](https://windhawk.net/mods/windows-11-notification-center-styler); [source](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/windows-11-notification-center-styler.wh.cpp) | `ShellExperienceHost.exe`, `ShellHost.exe` | Leave styled notification/calendar/quick settings panels unmodified; preserve tray click-to-open, positioning and accessibility. | **Lower direct hook overlap**, **moderate tray/popup interactions**. |
| **Taskbar tray system icon tweaks** — m417z | [taskbar-tray-system-icon-tweaks](https://windhawk.net/mods/taskbar-tray-system-icon-tweaks); [source](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-tray-system-icon-tweaks.wh.cpp) | `explorer.exe` | This mod owns which native tray icons, bell and Show Desktop controls are visible and how they appear; our Taskbar 1 must preserve this. | **High** if our code touches tray XAML creation/layout or hit-testing. |
| **Taskbar height and icon size** — m417z (already in user's setup) | [taskbar-icon-size](https://windhawk.net/mods/taskbar-icon-size); [source](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-icon-size.wh.cpp) | `explorer.exe` | Existing height/icon/button-width changes are a compatibility baseline. Our mod may eventually supersede its overlapping sizing, but must offer equivalent settings and a clean migration preserving user values. | **Very high** — overlapping native geometry/work area changes. |

Check exact mod versions and OS build during each test. Do not assume these links refer to permanently pinned code; pin SHAs before source reuse.

## Conflict handling policy

1. On initialization, collect diagnostic information about known related Windhawk mods/settings where a safe supported mechanism exists. If enumeration isn't supported, request manual mod inventory; do **not** inspect or alter other mods' private storage without permission.
2. Distinguish **shared hooks** (high risk), **shared XAML elements** (style/transform/measure contention) and **cross-process UI interactions** (lower direct overlap but positioning/focus issues).
3. **One owner per property**. Examples: Taskbar Styler may own background/pip colors; our dock owns extra render region and animation transforms; tray tweak mod owns tray visibility; native Windows owns pinning/grouping/previews.
4. If exact overlap is unavoidable, use a documented compatibility mode or decline to enable the conflicting feature with actionable guidance. **Never silently disable another mod or overwrite its settings**.
5. With our integrated animation engine enabled, test the original Dock Animation separately as a baseline; do not run both animation engines on the same button until a safe coordination mechanism has been proven. This is not permission to call that combination "compatible".
6. Preserve native Start/tray/notification behavior, hover previews/Aero Peek, grouping, task-button context menus and Jump Lists. Fail closed on unsupported Windows or undocumented XAML tree.
7. Trial rollback must restore **our changes only** and never reset the user's custom theme, taskbar preferences, or other mod settings.

## Compatibility acceptance matrix

For each target, test: target alone; our mod alone; target + our mod; full chosen theme stack + our mod; enabling/disabling in both orders; editing theme/settings while our trial is active; rollback/Explorer restart; Windows light/dark and multi-monitor/DPI.

| Scenario | Expected observation |
|---|---|
| SimplyTransparent Taskbar Styler theme + our dock | Transparent theme remains, no unwanted background/pip resets |
| Different Taskbar Styler custom pip rules | Running/active indicators remain at the user-configured position and colors unless explicitly overridden by our settings |
| Start Menu Styler while icon region overlays maximized app | Start/search buttons launch native themed popup at expected location, with correct focus and click-through |
| Notification Center Styler + tray tweaks | Network/volume/calendar/bell panels launch; hidden system icons stay hidden; click target remains reachable |
| Compact size via height/icon-size mod | No clipped magnification, work-area reservation stays configured; acknowledge geometry hook conflicts until measured |
| Original Dock Animation baseline vs our integrated behavior | Animation radius/scale/easing feel preserved; clipping reduced/eliminated when host boundary permits |
| Two simultaneous animation engines intentionally enabled | Must not corrupt Explorer or stack transforms; a fail-closed warning is acceptable until proven compatible |
| Trial expires during Start menu/notification panel open | Reversible layout cleanup without breaking opened shell surfaces |
| Re-enable after rollback | Requires a fresh explicit 120s trial; no reapplication across Explorer restart |

**User manually validates real visuals and behaviors.** Automated tests measure hooks, geometry, diagnostic events and rollback; screenshots are not proof of correct animations or click-through.

## Integration implementation order

1. Pin upstream source revisions/license and document method-level reuse.
2. Add non-mutating detection / diagnostic report for process ownership and likely overlap.
3. First pass the independent 120-second watchdog and recovery gates.
4. Prototype native taskbar split using the original animation disabled; validate native behavior and theme coexistence.
5. Integrate selected Ph0en1x-dev animation techniques in the native extended region and compare to original behavior.
6. Verify every coexistence row and rollback after each trial; no shipping on untested claims.
