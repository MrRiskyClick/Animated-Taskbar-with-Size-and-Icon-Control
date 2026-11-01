# ATB Rust Dock v0.3 — Native Windows developer preview

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
- **Start + Task View:** independently rendered Win11-style Start and Task View
  buttons, invoking native Windows Start and Win+Tab without shell mutation.
  These are approximated vector icons, NOT the original Windows shell art assets.
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
  a long bar for four or more; width, height, horizontal/vertical offsets,
  spacing, RGB color, shape (square/rounded/ellipse), corner radius and opacity.
  Pips have a **separate click-through layered HWND** so their alpha can be
  adjusted independently of app icons without changing Explorer.
- **Native settings popup:** right-click anywhere on an app icon, select
  **Settings...** to open a separate native Win32 window. Six categories
  contain **33 live numeric sliders**, all functional. Use arrow keys on a
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
| Indicator appearance | Horizontal offset, red/green/blue color, independent opacity, shape, corner radius |
| DWM previews | Opening hover delay, dismissal delay, tile width/height, columns, tile gap, vertical distance |

All settings are in `%LOCALAPPDATA%\ATBRustDock\settings.ini`, never in the
registry or Explorer. The dock saves the selected manual application order
in the same file. The original old two timing properties are migrated.

## Versioned desktop updates

**v3 is the short display name of Rust package version 0.3.0.** The CI builds
from `Cargo.toml` and names the artifact `ATB-Dock-v3`, containing
`ATB-Dock-v3.exe` and `build-info.json`. Each future release increments
the package version (0.4.0 -> v4, etc.) so executable and Desktop folder names
update automatically. Rebuilds within the same numbered version retain that
name, but `build-info.json` records the exact Git commit, run ID and SHA-256.

To update an existing installation (GitHub CLI `gh` must already work), from
regular PowerShell run:

```powershell
$updater = Join-Path $env:TEMP 'ATB-update-desktop.ps1'
Invoke-WebRequest 'https://raw.githubusercontent.com/MrRiskyClick/Animated-Taskbar-with-Size-and-Icon-Control/feature/rust-native-dock-v0/rust-dock/scripts/update-desktop.ps1' -OutFile $updater
powershell.exe -NoProfile -ExecutionPolicy Bypass -File $updater
```

The updater requires a *successful build for the exact current PR head* before
touching the old installation. It checks the manifest commit and EXE SHA-256,
closes only the standalone ATB process, renames an old `ATB-Dock-v0.2` Desktop
folder to `ATB-Dock-v3` when appropriate, copies `ATB-Dock-v3.exe`, removes
superseded ATB executable names, and prints the version and commit. If an old
folder has non-ATB files it is retained. No auto-run; the user starts the EXE
manually. No Windhawk/Explorer mods are changed.

To compile from source:
```powershell
cargo test --manifest-path rust-dock/Cargo.toml
cargo build --manifest-path rust-dock/Cargo.toml --release
```

Close the dock via **right-click → Exit ATB Dock**, or close its console
window. No Explorer restart. Keep native taskbar buttons visible for the
first evaluation if you want a fallback.

## Diagnosing exact taskbar order and icon identity (read-only)

With your native taskbar app buttons **visible** (do not enable the hiding mod
for this comparison), run from the repository checkout on Windows PowerShell 5.1:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\rust-dock\diagnostics\inspect-native-taskbar-readonly.ps1
```

The local output is `ATB-taskbar-inventory.json` on your Desktop. It includes
read-only UI Automation button names, AutomationIds, screen coordinates and
available pinned .lnk shortcut targets/icon locations. It never clicks buttons,
reads/writes Windhawk registry configuration, changes the taskbar, manipulates
pins or restarts Explorer. **Review the JSON before sharing:** it contains
local application paths and window names. The data will guide a future safe
identity/order resolver; this script does **not** make the Rust dock match yet.

## Still to implement / correctness boundaries

- **Exact native icon assets and live taskbar ordering remain UNRESOLVED.**
  Running apps still use executable identity with a 256px icon-resource request;
  Windows Explorer may instead render an app's pinned shell shortcut or packaged
  app identity, causing Windhawk and other app-icon mismatches. Manual drag order
  is not automatic Explorer order. The OS does not supply a public supported API
  returning rendered taskbar glyph pixels + ordered pinned identities together;
  a read-only accessibility/shortcut identity resolver needs platform validation.
- **Do not hide your native taskbar icons while evaluating exact-order/icon parity**;
  keep the original visible as a reference for your manual tests.

## Further production work

Read-only Explorer taskbar order/identity, pinned-but-not-running applications,
exact shortcut/packaged-app icon resources, shell-equivalent context menus and
jump lists, native task view replacement, multi-monitor/DPI coverage,
GPU Direct2D rendering and fully event-driven idle scheduling.

**CI success is compiler/math testing, not proof of actual desktop rendering.**
Only the user performs visual and interaction testing on their Windows PC.
