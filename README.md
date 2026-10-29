# Animated Taskbar with Size and Icon Control

A **research-first, safety-gated Windhawk project** to retain the native Windows 11 taskbar features while separating the reserved taskbar work area from the visual area used for app-button animation.

> **Current status: Planning / Stage 0 — no active shell modifications, and no tested watchdog. Do not install as a taskbar-changing mod.**

## Goal

- **Region 1:** compact native taskbar that reserves only its configured height; retains the Windows system tray, clock, notifications, network, audio, and other shell controls.
- **Region 2:** transparent extended rendering/input region containing **real native Windows task buttons** with controllable sizing, spacing, alignment, indicators, and hover magnification. It must not reserve extra work area.
- Native grouping, activation, thumbnail previews / Aero Peek, and Jump Lists should be preserved if verified in prototypes; no unsupported claims that a second native Windows taskbar can simply be instantiated.
- Click-through over genuinely empty regions, with interactive icons and system tray remaining clickable.

## Mandatory trial policy

**Every experimental change expires after 120 seconds.** Before any taskbar-changing code is enabled, the implementation must pass an independent watchdog / recovery gate, transactional state capture/revert, non-reapplication on restart, and failure-injection tests. A process-local timer alone is insufficient. See [Safety and rollback](docs/SAFETY.md).

No code from third-party mods is incorporated yet; references and prospective reuse are documented with licensing review.

## Documentation order

1. [Plan](docs/PLAN.md)
2. [Blueprint](docs/BLUEPRINT.md)
3. [Frame](docs/FRAME.md)
4. [Stages / acceptance gates](docs/STAGES.md)

Supporting: [Upstream references](docs/UPSTREAM.md), [Safety](docs/SAFETY.md), [testing](docs/TESTING.md), [architecture decisions](docs/DECISIONS.md).

## Current implementation

`src/animated-taskbar.wh.cpp` is intended to begin as a **non-mutating diagnostic skeleton**, not a functional taskbar modifier. All experimental hooks, geometry changes and watchdog arming are gated pending verification. `tests/` contains a mockable rollback-contract prototype.

## Development branch policy

Develop on a feature branch; keep `main` stable. Never auto-install or enable an unverified Explorer hook on the developer's PC. The user performs real visual/interaction testing on Windows.

## Primary project sources

- Windhawk mod documentation: https://github.com/ramensoftware/windhawk/wiki/Creating-a-new-mod
- Windhawk mods: https://github.com/ramensoftware/windhawk-mods
- Taskbar sizing: https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-icon-size.wh.cpp
- Dock animation: https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-dock-animation.wh.cpp

See `docs/UPSTREAM.md` for exact proposed reuse and provenance.

## First-class Windhawk compatibility targets

The original [Ph0en1x-dev Taskbar Dock Animation](https://windhawk.net/mods/taskbar-dock-animation) is the **behavioral reference** for the new magnification engine. The project must coexist with separately installed [Windows 11 Taskbar Styler](https://windhawk.net/mods/windows-11-taskbar-styler), [Start Menu Styler](https://windhawk.net/mods/windows-11-start-menu-styler), [Notification Center Styler](https://windhawk.net/mods/windows-11-notification-center-styler), [Taskbar tray system icon tweaks](https://windhawk.net/mods/taskbar-tray-system-icon-tweaks), and [Taskbar height and icon size](https://windhawk.net/mods/taskbar-icon-size) where technically feasible. Avoid two simultaneous icon-animation engines; do not automatically alter other mods' settings.

See [Compatibility matrix](docs/COMPATIBILITY.md) and [Emergency recovery](docs/EMERGENCY_RECOVERY.md). Compatibility has **not yet been demonstrated**.

## Safety watchdog prototype (Stage 1a)

The development branch includes an independent subprocess watchdog and durable **mock-only** trial journal, with two-minute expiration by default. It uses separate processes and compare-before-restore to avoid clobbering externally edited mock settings. Windows + Linux mock CI tests pass.

This does **not** yet recover the Windows taskbar, disable the Windhawk mod, restart after failure or guarantee restoration. Do not treat it as a production safety solution. See [watchdog/README.md](watchdog/README.md), [Stage 1 safety](docs/SAFETY.md) and [Stage 1 milestones](docs/STAGES.md).

## Stage 1b — version-gated Windhawk control

A narrow [Windhawk CLI adapter](watchdog/windhawk_control.py) is under development. It recognizes the supported Windhawk 2.x CLI, constructs a command to **disable only this project's mod**, refuses requests to change other mods, and requires explicit opt-in before invoking anything. The adapter is **not yet wired to watchdog expiry**, and CI uses a fake command runner only. See [version-specific integration plan](docs/WINDHAWK_INTEGRATION.md). Windhawk 1.x remains an unsolved independent-disable integration and is intentionally fail-closed. The active taskbar safety gate is still open.
