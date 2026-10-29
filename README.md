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
