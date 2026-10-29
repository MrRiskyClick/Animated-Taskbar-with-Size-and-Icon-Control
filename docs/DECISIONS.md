# Architectural decisions / open questions

## ADR-001 — Prefer native task-list rehosting to reimplementation

**Accepted as investigation direction, not proved technically.** User wants a two-region arrangement specifically to preserve Windows thumbnail previews (Aero Peek), grouping, pinning and Jump Lists. A separately drawn dock wouldn't inherit all that behavior. Do not claim a second Explorer-managed taskbar can be created from a supported API.

Decision gate: prove native task-list can exist/appear above the work-area boundary while preserving controls, or stop and present measured failure.

## ADR-002 — No mutation before watchdog

**Accepted hard gate.** A process-local timer doesn't protect against Explorer hangs. The watchdog must be independent, journaled, able to block reapplication, and failure-tested. Stage-0 diagnostic code remains inert.

## ADR-003 — Work-area reservation not equal visual dock size

**Desired behavior; technical feasibility pending.** Native tray bar reserves Taskbar 1 height, larger visual dock overlays maximized apps without reserving extra space. Need to validate actual Win32 HWND/XAML clip chain and work-area updates.

## ADR-004 — Sources and licensing

**Accepted:** document third-party source locations first. No copying unverified-license code into repo. Anticipate GPL-3.0 obligations before combining upstream GPL source.

## ADR-005 — Timer semantics

**Accepted:** expiry after two minutes, unconditionally; a user confirmation/extend option is *not* included. After expiry, an installed Windhawk mod can remain listed in the UI but must have no active effects and cannot re-arm automatically. Determine whether actual UI-level disable can be safely invoked through a validated interface.

## Open technical decisions

- Is a separate HWND genuinely necessary, or can the existing native taskbar window/XAML rendering boundary be extended safely?
- Can native UI controls be moved without breaking shell-managed preview anchors, context menus and keyboard navigation?
- Which documented Windhawk control enables watchdog-driven disabling without private configuration hacks?
- What safe monitor-aware strategy gives click-through over arbitrary underlying apps without sacrificing tray and task-list interaction?
- How do concurrent Windhawk size/styler/animation mods interact? Conflict detector vs explicit user instruction to disable others?
- What licensing model will be selected when upstream GPLv3 material is actually incorporated?

## ADR-006 — Source animation is Ph0en1x-dev, independent theme mods coexist

**Accepted product requirement; feasibility and licensing pending.** Ph0en1x-dev Taskbar Dock Animation's magnification and ease should serve as the behavioral starting point. The taskbar project must be able to run with the user's existing Windhawk theme stack: m417z Taskbar Styler, Start Menu Styler, Notification Center Styler, Taskbar tray system icon tweaks and sizing mod, subject to measured technical limits.

**Decision:** Integrate only the necessary animation patterns into the new renderer after GPL/source review. Do not bundle whole unrelated mods or silently disable them. Never stack two animation engines on the same native controls; if overlap is unavoidable, warn and require explicit choice for that one feature. Continue to support the other theme mods. Record tests in [COMPATIBILITY.md](COMPATIBILITY.md).

## ADR-007 — System Restore is tertiary recovery, not automatic rollback

**Accepted:** The developer maintains a manual pre-test Windows System Restore point. This may recover system files/settings if Explorer or Windows won't start, via WinRE, but is not a guaranteed whole-PC snapshot or replacement for a tested 120-second watchdog. Don't publish the developer's personal restore point metadata in this public repository. Emergency steps: [EMERGENCY_RECOVERY.md](EMERGENCY_RECOVERY.md).
