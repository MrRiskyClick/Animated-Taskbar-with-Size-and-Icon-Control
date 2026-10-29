# 04 — STAGES & GATES

Stages are sequential: passing a stage requires evidence, not just compiling.

| Stage | Deliverable | Release gate |
|---|---|---|
| 0 — Research + inert scaffold | Docs, upstream source map, original mockable rollback core, non-mutating Windhawk probe | Static inspection, mock tests pass; no shell mutation |
| 1 — Safety gate | Independent watchdog, durable journal, 120s TTL, expiry/disarm and Explorer-restart recovery | **Failure injection** passes before any active taskbar mutation |
| 2 — Native taskbar inventory | Read-only HWND/XAML/monitor inventory, OS/build/symbol detection | Unknown build fails closed; no wrong-monitor handles |
| 3 — Reserved geometry | Reversible Taskbar 1 height and native work-area reservation | Maximized work-area edge stays at configured reserved bar; safe rollback |
| 4 — Native visual extension | Native task buttons render in taller unreserved region, tray remains native | No clipping; grouping, hover previews, Jump Lists still function |
| 5 — Interaction | Hit-test empty transparent region / icon buttons / tooltips / menus | Click-through and mouse/keyboard native behavior tested |
| 6 — Customization | Width, size, spacing, pip alignment, offset, toggle, hover magnification | Visual + behavior tested; no regression on expiry |
| 7 — Compatibility & packaging | Multi-monitor, DPI, Explorer restart, fullscreen, coexisting mods, docs and release | Test matrix complete; safe rollback on unknown OS/failed symbol |

### Stage 1 failure-injection checklist (must pass before Stage 3)

- Ordinary 120-second expiry.
- Loss of heartbeat / missed deadline.
- Explorer crash and restart.
- Explorer frozen / blocked callback.
- Loss of watchdog process, crash and machine reboot while armed.
- Secondary monitor disconnect and DPI change during trial.
- Unexpected mod unload, Windhawk restart, Windows update changing symbols.
- Reverse-order restoration when an intermediate operation fails.
- User modification of a value during trial (do not overwrite unexpectedly).
- Escalation path has an explicit bounded timeout; does not endlessly restart Explorer.

### Success criteria — native behavior

- Taskbar system tray and date/time remain visible and usable.
- Pinned/running icons, order, app grouping and taskbar activation continue to reflect shell state.
- Aero Peek/thumbnail preview, context menus/Jump Lists, active application indicator, drag-and-drop and keyboard interactions are explicitly checked.
- Dock icons magnify without crossing a crop rectangle; no extra maximized-window reservation; clicks in transparent free space reach underlying app.
- When trial expires, the taskbar returns to its **recorded pre-trial baseline**, and the mod cannot silently reapply.

### Stage 0 status

Documentation/scaffold only. No claims about successful Windhawk compilation, native rehosting, watchdog readiness, or visual behavior until independently demonstrated.
