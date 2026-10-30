# Animated Taskbar — Interactive Preview v0.4

**A real, runnable visual-interaction demo**, not an actual Explorer/Windhawk taskbar mod.

## Open it

Download the standalone `preview.html` file from this folder, or open it locally in your browser (Edge, Chrome, Firefox). There is no installer, build, server, network connection or Windhawk dependency.

To test the interactive mock desktop, hover from icon to icon at the bottom and compare slow and fast pointer movement. Change the Layout/Motion/Style settings to evaluate icon size, focus magnification and neighbor influence, animation responsiveness, dock lift and compact system-bar spacing, overlapping composition by default (optional separate floating dock), backdrop blur, glow, opacity and colors. Click icons to open *simulated* app windows. Click **Copy preset JSON** to export settings for future native implementation; if the browser blocks clipboard access, it saves a JSON file instead.

The v0.2 configuration is stored under a separate browser local-storage key, so a previous v0.1 floating-mode preference cannot silently override the new overlapping default. **Reset** restores its defaults.

## Changes in v0.2

- The dock overlaps the system bar by default; separate floating mode is optional and OFF.
- The system bar height is controlled only by its own slider, even if icons are larger than the bar.
- **Icons vertical placement** moves the whole dock up/down independently of the system bar in overlapping mode.
- **Spacing between icons** sets the CSS gap independently of icon size.
- **Pips vertical placement** independently moves the running/active status indicators relative to their icons.
- Icon size now ranges up to 112 px. The optional floating-dock separation control is hidden in overlapping mode.

## Changes in v0.3 — collision-free dock magnification

Ph0en1x-dev's [Taskbar Dock Animation](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-dock-animation.wh.cpp) uses cumulative expansion and centered shifting to move neighboring taskbar icons as they grow.

Our browser demo now follows the same geometric principle: every frame it calculates each icon's **current animated width gain**, shifts subsequent icons by accumulated gains, and subtracts half of the total expansion to keep the dock centered. Unlike the original mod's adjustable spacing sensitivity (50% by default), this preview uses the full current expansion so that growing icons do not overlap even at high magnification.

The separator participates in the push layout. Icon scales and horizontal offsets are always computed from the **same frame**, rather than using separate springs that could get out of sync. This works with the existing gap, neighbor-influence, magnification and responsiveness controls. Existing v0.2 stored settings remain readable (the storage key was intentionally preserved).

## What this proves

The dock, spring simulation, pointer response, switching layouts and controls work in a standalone browser window. This lets the owner choose appearance and interaction before developing risky Explorer taskbar hooks.

## What it does **not** prove

- It does not replace, hide, resize, hook or otherwise modify the Windows taskbar.
- The system bar, running apps, tray, Explorer desktop and windows are **simulated**. Clicking an icon does not launch an installed app.
- It does not validate Windows work area, multi-monitor support, native click hitboxes, accessibility, other Windhawk mod coexistence, or real rollback.
- It cannot serve as the watchdog required to authorize taskbar changes.

The existing native watchdog, IPC, Authenticode and rollback work remains isolated on the Stage 1 development branch. **Real shell modifications are still blocked** pending successful native state-capture and restoration tests.

## Testing

v0.3 was measured in Chromium at 1540×930 with rapid pointer traversal across icons. Tests sampled 165 animated states across zero-gap and wide-gap layouts, 48–112 px icons, up to 2.2× magnification, animation responsiveness from 5% to 95%, and neighbor influence both ON and OFF. Every sampled adjacent pair retained its chosen nonnegative gap; there were no JavaScript errors. These are browser visual-geometry measurements, not proof of native Windows taskbar behavior.


Browser interactions were checked in headless Chromium at 1440×900: overlapping layout defaults ON, bar height stays 35 px with 95 px icons, icon vertical offset, gap and pip offset each change their intended geometry, optional floating mode, Reset, and absence of runtime script errors. The browser does not verify live Windows taskbar functionality. Automated repository tests verify that this demo stays self-contained and offline; they are not equivalent to live Windows taskbar testing.

## Changes in v0.4 — accepted visual specification

The fake pinned/unpinned separator has been removed; the preview no longer invents a Windows divider. Window-state pips are now **zero for 0 running windows, one/two/three dots for 1–3 windows, or one wide bar for 4+**. The browser demo provides Shift+Click to open an additional simulated window and right-click to close one. These are simulated counts, **not** actual Windows window enumeration. The approved v0.4 HTML is the reference appearance for the forthcoming Windhawk implementation.
