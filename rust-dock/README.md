# ATB Rust Dock v0.2 — Native Windows developer preview

A standalone Rust/Win32 dock built and tested in GitHub Actions. It does NOT
replace, resize, alter the work-area reservation of, or inject into the
Windows Explorer taskbar. Windows 11; no installer, WebView, Tauri, helper
service, administrator access or explorer.exe restart.

**Safety:** the discontinued v0.3.x Windhawk overhang experiment remains
disabled. This standalone executable does not require it. Your working
`ATB Hide Taskbar Buttons` mod and existing `SimplyTransparent` theme
are not modified.

## Features implemented

- **High-resolution native app icons:** request an icon resource at 256×256
  from each executable; fall back to a large window-provided icon, class icon
  or system icon. Actual quality still depends on the application's resources.
- **Start button:** independently drawn crisp four-pane Windows logo at the
  beginning of the icon row, invokes the Windows Start menu on click.
- **Running apps:** read-only top-level window enumeration, grouping by
  process executable, click to activate, grouped DWM live hover thumbnails.
- **Order:** running applications can be reordered by dragging their dock
  icons; the order is saved to per-user settings. This is NOT yet an automatic
  replica of Explorer's **pinned** taskbar sequence, and pinned apps without
  open windows are not yet included. An accurate read-only native taskbar
  ordering/identity mapping is a separate milestone.
- **Animated magnification:** spring-based neighbor push-out with independently
  adjustable size, spacing, stiffness, damping and neighbor falloff.
- **Pips:** one indicator for one window, 2 for two, 3 for three,
  a long bar for four or more; width, height, offsets and spacing adjustable.
- **Native settings popup:** right-click anywhere on an app icon, select
  **Settings...** to open a separate native Win32 window. Five categories
  contain **26 live numeric sliders**, all functional. Use arrow keys on a
  focused slider for 1-unit adjustments. Every change saves automatically.
- **DWM hover previews:** configurable opening/dismissal delays, preview tile
  width/height, columns, spacing and distance from the dock; click to activate
  and preview X to request that one window close.
- **Native fullscreen guard:** dock hides when a foreground window occupies
  the entire primary display. This remains a prototype for multi-monitor and
  exclusive fullscreen games.

## Settings panels

| Panel | Sliders |
| --- | --- |
| Position & layout | Dock height, width %, horizontal offset, vertical offset, icon vertical position |
| Icon appearance | Resting size, magnified size, gap, independent Start size, opacity |
| Animation | Spring stiffness, damping, neighbor falloff, frame interval |
| Running indicators | Pip width, height, vertical position, between-pip gap, 4+ window bar length |
| DWM previews | Opening hover delay, dismissal delay, tile width/height, columns, tile gap, vertical distance |

All settings are in `%LOCALAPPDATA%\ATBRustDock\settings.ini`, never in the
registry or Explorer. The dock saves the selected manual application order
in the same file. The original old two timing properties are migrated.

## Build or run

Download the developer EXE from the successful
[GitHub Actions run](https://github.com/MrRiskyClick/Animated-Taskbar-with-Size-and-Icon-Control/actions/workflows/rust-dock-ci.yml)
under `atb-rust-dock-windows-dev`. The EXE runs in user space.

To compile from source:
```powershell
cargo test --manifest-path rust-dock/Cargo.toml
cargo build --manifest-path rust-dock/Cargo.toml --release
```

Close the dock via **right-click → Exit ATB Dock**, or close its console
window. No Explorer restart. Keep native taskbar buttons visible for the
first evaluation if you want a fallback.

## Still to implement

Exact Windows taskbar pinned order and pinned-but-not-running applications,
modern packaged-app icon resolution, shell-equivalent context menus and
jump lists, native task view replacement, multi-monitor/DPI coverage,
GPU Direct2D rendering and fully event-driven idle scheduling.

**CI success is compiler/math testing, not proof of actual desktop rendering.**
Only the user performs visual and interaction testing on their Windows PC.
