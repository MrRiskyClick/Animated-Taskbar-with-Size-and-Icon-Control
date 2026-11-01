# ATB Rust Dock v0.4 / Desktop v4 — Native Windows developer preview

A standalone Rust/Win32 dock built and tested in GitHub Actions. It does NOT
replace, resize, alter the work-area reservation of, or inject into the
Windows Explorer taskbar. Windows 11; no installer, WebView, Tauri, helper
service, administrator access or explorer.exe restart.

**Safety:** the discontinued v0.3.x Windhawk overhang experiment remains
disabled. This standalone executable does not require it. Your working
`ATB Hide Taskbar Buttons` mod and existing `SimplyTransparent` theme
are not modified.

## Features implemented

- **Read-only native shortcut icon sourcing (v4):** inspect existing taskbar
  pinned .lnk targets and assigned icon locations using Windows ShellLink COM.
  Prefer that 256px resource for matched running apps before falling back to
  the executable and window-provided icons. This often helps custom icons like
  Windhawk, but not all packaged or AUMID-based taskbar icons are resolved.
  It does not read the rendered glyph pixels from Explorer's private XAML UI.
- **Start + Task View:** independently rendered Win11-style Start and Task View
  buttons, invoking native Windows Start and Win+Tab without shell mutation.
  These are approximated vector icons, NOT the original Windows shell art assets.
- **Running apps:** read-only top-level window enumeration, grouping by
  process executable, click to activate, grouped DWM live hover thumbnails.
- **Taskbar order observation (v4):** a dedicated read-only Windows UIA COM
  thread observes native taskbar accessibility labels/positions every 6 seconds.
  Identifiable **running** apps are ordered by native positions, and unmatched
  apps retain their saved manual order. Existing pinned desktop .lnk files
  with identifiable .exe targets are also represented when not running and
  launch using their original shortcuts. This is partial: taskbar buttons can
  be hidden from UIA by a theme/mod, labels may not map uniquely to executable
  names, and packaged/AUMID-only pins are not yet fully supported.
- **Animated magnification:** spring-based neighbor push-out with independently
  adjustable size, spacing, stiffness, damping and neighbor falloff.
- **Pips (v4):** independently styled ordinary 1/2/3 indicators, currently
  foreground app indicator, and 4+ window line. Real equal-width/height round
  dots, rounded rectangles with pixel corner radii, ellipses, and squares.
  Each state has separate geometry, position, RGB color and opacity, rendered
  in **three** mouse-transparent layered windows. Rounding a 2-3 px-high bar
  is inherently hard to discern; increase its height to observe round corners.
- **Native settings popup:** right-click an app icon, choose **Settings...**.
  Ten category panels in a native scrollable list expose **54 numeric sliders**
  including foreground app and 4+ line styles. Every change saves automatically.
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
| Running indicators | Ordinary pip width/height, vertical/horizontal offset and spacing |
| Ordinary pip style | RGB color, opacity, shape, corner radius and exact dot diameter |
| Focused app geometry | Focus mode (same count/line/dot), shape, size, x/y offset, corner radius |
| Focused app colors | Independent RGB channels and opacity |
| 4+ line geometry | Independent line shape, width, height, x/y offset and corner radius |
| 4+ line colors | Independent RGB channels and opacity |
| DWM previews | Opening hover delay, dismissal delay, tile width/height, columns, tile gap, vertical distance |

All settings are in `%LOCALAPPDATA%\ATBRustDock\settings.ini`, never in the
registry or Explorer. The dock saves the selected manual application order
in the same file. The original old two timing properties are migrated.

## Versioned desktop updates

**v4 is the short display name of Rust package version 0.4.0.** The CI builds
from `Cargo.toml` and names the artifact `ATB-Dock-v4`, containing
`ATB-Dock-v4.exe` and `build-info.json`. Each future release increments
the package version (0.5.0 -> v5, etc.) so executable and Desktop folder names
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
folder to `ATB-Dock-v4` when appropriate, copies `ATB-Dock-v4.exe`, removes
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

- **Exact native icon assets/order are not fully resolved.** v4 uses pinned
  shortcut icon locations and read-only UIA positions for matches, with safe
  executable-icon and saved-order fallbacks. This does NOT obtain raw icon
  pixels from Explorer, nor guarantee 1:1 parity for all AppUserModelIDs,
  packaged/nonstandard pinned apps, invisible taskbar controls or native Start glyphs.
  Windows Start remains a drawn approximation. Manual UI test is mandatory.
- **Do not hide your native taskbar icons while evaluating exact-order/icon parity**;
  keep the original visible as a reference for your manual tests.

## Further production work

Complete Explorer taskbar order/identity, packaged/AUMID-only pinned applications,
packaged/AUMID icon resources, shell-equivalent context menus and
jump lists, exact native Start icon glyph parity, multi-monitor/DPI coverage,
GPU Direct2D rendering and fully event-driven idle scheduling.

**CI success is compiler/math testing, not proof of actual desktop rendering.**
Only the user performs visual and interaction testing on their Windows PC.
