# 01 — PLAN

Status: **Proposed design; native-shell feasibility unproven**. Date: 2026-10-29.

## Product intent

Give the user a visually small native Windows 11 taskbar and an independent, taller *visual* dock region hosting **native Explorer task buttons**. Real grouping, thumbnail previews/Aero Peek, pinning, Jump Lists, app activation, and keyboard/taskbar interaction matter; imitating Windows buttons in a separate app is not the primary approach.

### Taskbar 1 — system region

- Windows-owned shell taskbar, with configurable bottom reservation height in device-independent pixels (DIP); preserve system tray, notifications, clock, network, audio, Show Desktop and relevant system controls.
- Native application task-list controls should not be duplicated; their visual placement is migrated/extended if the shell permits it. They must remain available when the experimental mod is disabled.
- Taskbar 1 is the *only* reserved work-area boundary for maximized windows.

### Taskbar 2 — application region

- Transparent, floating/extended visual region positioned relative to Taskbar 1; configurable height, width, horizontal and vertical alignment, independent icon size, button width, spacing, running/active/attention indicators (“pips”), their sizing/location, and dock-animation enabled/disabled.
- Transparent empty space must permit pointer interaction with maximized apps underneath. Icon/pip interaction and native thumbnail previews should function normally.
- Can overlay content but must not enlarge work-area reservation. Multi-monitor/DPI handling is required before general release.
- It is **not established** that a second genuine Windows taskbar can be instantiated on the same monitor. Preferred investigation: re-use the existing native task-list/hosting XAML/Win32 elements with an extended visual region and independent work-area control.

## Development principles

1. **Safety first:** no shell-changing feature until the 120-second trial watchdog and transactional rollback are validated. A timer only inside Explorer is insufficient.
2. Original Windows features take precedence over cosmetic imitation. Validate native previews, grouping, Jump Lists and tray independently.
3. Preserve the user's existing Windhawk setup; detect incompatible overlapping mods; never silently override their settings.
4. Feature switches and build/Windows compatibility checks; fail closed for unknown symbols or XAML trees.
5. Develop on feature branches; docs and non-mutating tests first. No unattended user-machine installs or automatic enabling.

## Deliverables

- Provenance and licensing table; feasibility spikes; reversible geometry/reservation control; XAML layout/clip and hit testing; animation and pip control; native behavior compatibility tests; multi-monitor build; rollback/diagnostics; packaging with documented uninstall.
- Documentation sequence: PLAN → BLUEPRINT → FRAME → STAGES. Separate safety and test gates.

## Non-goals initially

- Creating a wholly separate taskbar clone from scratch.
- Replacing the system tray or notification center.
- Forcing an invisible taskbar to reserve the entire dock height.
- Modifying undocumented Windows internals on unknown OS versions without a probe.
- Claiming a complete OS “save state”; capture **only state changed by this mod** and provide a recovery baseline.

## Decision checkpoint

Gate P1: If native app buttons cannot safely be rendered outside the parent HWND/XAML clip without losing interactions, document the failure. Consider a narrowly scoped native-host alternative or re-evaluate architecture; **do not** silently implement a fake taskbar and call it native.

## First-class external mod compatibility (added requirement)

Retain the appearance chosen with **m417z's Windows 11 Taskbar Styler, Start Menu Styler, Notification Center Styler, Taskbar tray system icon tweaks**, and compact taskbar sizing. The new animation must preserve the *feel and core capabilities of Ph0en1x-dev's Taskbar Dock Animation*, improving its clipping constraint rather than replacing its design with unrelated motion.

These are **coexistence targets**, not permission to embed or modify other mods wholesale. If code is adapted, pin revision, review license, attribute, and ensure only one animation controller owns the native task-button transforms. Compatibility with the original simultaneously enabled Dock Animation is **not presumed**; fail closed or require the user to disable only that conflicting duplicate animation, while preserving the other theme mods.

See [Compatibility test matrix](COMPATIBILITY.md) and [emergency recovery runbook](EMERGENCY_RECOVERY.md).
