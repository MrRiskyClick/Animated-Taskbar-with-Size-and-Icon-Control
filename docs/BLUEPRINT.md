# 02 — BLUEPRINT

**Proposed architecture; not yet verified on the user's Windows build.**

```text
     screen / monitor
    ┌─────────────────────────────────────────────────┐
    │ maximized app area, extending beneath dock     │
    │                                                 │
    │  Taskbar 2: visual/input extension (transparent)│
    │  native task-list buttons + icons + animation   │
    │  click-through outside interactive hit regions  │
    ├─────────────────────────────────────────────────┤ ← work-area edge
    │  Taskbar 1: reserved native bar + real system tray│
    └─────────────────────────────────────────────────┘
```

### Four responsibilities

A. **Reservation controller:** native taskbar height/work-area calculation (not a second AppBar). The only reservation is Taskbar 1. Evaluate native taskbar size hooks against Windows version and DPI, and record original settings/geometry.

B. **Native button host/visual controller:** find the existing Windows 11 task-list XAML host and controls; investigate transformations, render surface and clip chain. Preserve native input, grouping and hover/thumbnail behavior. Potential outcome: reparenting is not viable; this is an explicit research gate.

C. **Hit-test controller:** transparent portions must pass mouse input to windows beneath, while icon rectangles, native tooltips, tray and popup surfaces remain interactive. Do not globally set `WS_EX_TRANSPARENT` on taskbar itself: that would make buttons/tray inaccessible. Maintain monitor-local interactive regions and update on magnification, icon reorder, popups, DPI and animation frames; check how HWND vs XAML hit testing interacts.

D. **Trial coordinator and recovery:** independent watchdog process or external controller owns absolute 120-second deadline and disarm policy; mod-local heartbeat/expiry is only a secondary defense. Persist a pre-change journal with strict baseline and inverse operations. When expired, request rollback and disable future application; escalation may restart Explorer if safely supported, but only after watchdog testing.

### Dataflow (target)

```text
Windhawk settings → validate → verify watchdog armed/healthy
     → persist state journal → apply reversible operation → health report
     → optional confirm? NO: current requirement always expires at 120s
     → 120s or failed heartbeat → rollback in reverse order → mark trial EXPIRED
     → disable re-application (including Explorer restart)
```

Every mutation must register an inverse action **before** attempting application. Journal entries contain affected monitor, target component, previous value, expected applied value and guarded restoration; check for external modifications to avoid overwriting unrelated user changes.

### Technical dependencies / APIs to investigate

- Explorer: `Shell_TrayWnd`, `Shell_SecondaryTrayWnd`, `taskbar.dll`, `Taskbar.View.dll` symbols, Windows 11 task list XAML visual tree.
- Height geometry: DPI-aware native frame sizing plus monitor work area; validate via `GetMonitorInfo`, `SHAppBarMessage`, actual maximized-window geometry.
- Native button structure: taskbar app list XAML, grouping and icon representation; shell-owned thumbnails and menus.
- Render/input: WinUI/XAML `RenderTransform`, clip and root HWND; composition surface; `WM_NCHITTEST`, region/hit testing; popup HWND ownership and Z-order.
- Animation: composition or XAML transforms (native visual objects) with a maximum scale that does not exceed the explicitly allocated render surface.
- Independent watchdog: explore **documented, reliable** Windhawk control/disable mechanism. Until proven, do not enable shell mutation.

### Critical architectural risks

| Risk | Impact | Gate |
|---|---|---|
| Native task-list cannot be hosted outside original window/clip | Blocks two-region native design | Prototype clip test + native previews |
| Expanded HWND blocks clicks | Disrupts maximized apps | Pixel/region hit-test with click-through tests |
| Moving native buttons shifts thumbnail/Jump List anchors incorrectly | Native regression | Hover/launch/group/menu tests |
| Reserved area follows expanded height | Violates central requirement | Work-area/maximize geometry measurement |
| Explorer hangs/crashes | No in-process rollback | Independent external watchdog |
| OS update changes internal symbols | Explorer instability | OS build allowlist + fail-closed |
| Mod overlaps existing size/animation/styler mods | Undefined overrides | Detection, isolation test, user-controlled disable |

### Initial API model (planned; not implemented)

```text
Safety:  trialDuration=120s (fixed), diagnosticOnly=true
Native:  reservedHeight=42 DIP, trayPreserved=true
Dock:    enabled=false, height=100 DIP, offsetY=0 DIP, alignment=center
Button:  iconSize=24 DIP, buttonWidth=44 DIP, spacing=6 DIP
Pip:     enabled=true, width=12, height=3, offsetX=0, offsetY=0
Motion:  enabled=false, maxScale=1.5, radius=80 DIP, easing=...
Input:   only button/popup regions interactive, otherwise click-through
```

These are conceptual defaults. User-specific actual taskbar geometry will be measured, **not guessed from screenshots**.

## Mod ownership and compatibility overlay

A separate native-shell **compatibility layer** must track ownership of all properties we touch. Desired boundaries: native Windows owns grouping/preview/Jump Lists; m417z Taskbar Styler owns user-selected theme/brushes and pip styles; m417z tray-tweaks mod owns visibility of system-tray controls; our project owns the separate render-region geometry and integrated Ph0en1x-style native-button motion. Existing m417z taskbar height mod may contend for geometry, requiring explicit migration or coordinated settings. Start Menu/Notification Center stylers run primarily in separate processes but rely on taskbar button anchors, Z-order and hit-test correctness.

Do not alter external mods' stored settings, bypass their hooks, or promise safe coexistence without Windows-host experiments. Preserve original style properties before touching them and restore only our changes on trial expiry. Full matrix: [COMPATIBILITY.md](COMPATIBILITY.md).
