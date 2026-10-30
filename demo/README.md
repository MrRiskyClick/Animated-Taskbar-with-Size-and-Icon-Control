# Animated Taskbar — Interactive Preview v0.1

**A real, runnable visual-interaction demo**, not an actual Explorer/Windhawk taskbar mod.

## Open it

Download the standalone `preview.html` file from this folder, or open it locally in your browser (Edge, Chrome, Firefox). There is no installer, build, server, network connection or Windhawk dependency.

To test the interactive mock desktop, hover from icon to icon at the bottom and compare slow and fast pointer movement. Change the Layout/Motion/Style settings to evaluate icon size, focus magnification and neighbor influence, animation responsiveness, dock lift and compact system-bar spacing, unified versus two-tier composition, backdrop blur, glow, opacity and colors. Click icons to open *simulated* app windows. Click **Copy preset JSON** to export settings for future native implementation; if the browser blocks clipboard access, it saves a JSON file instead.

The configuration is stored in browser local storage where available. **Reset** restores its defaults.

## What this proves

The dock, spring simulation, pointer response, switching layouts and controls work in a standalone browser window. This lets the owner choose appearance and interaction before developing risky Explorer taskbar hooks.

## What it does **not** prove

- It does not replace, hide, resize, hook or otherwise modify the Windows taskbar.
- The system bar, running apps, tray, Explorer desktop and windows are **simulated**. Clicking an icon does not launch an installed app.
- It does not validate Windows work area, multi-monitor support, native click hitboxes, accessibility, other Windhawk mod coexistence, or real rollback.
- It cannot serve as the watchdog required to authorize taskbar changes.

The existing native watchdog, IPC, Authenticode and rollback work remains isolated on the Stage 1 development branch. **Real shell modifications are still blocked** pending successful native state-capture and restoration tests.

## Testing

Browser interactions were checked in headless Chromium at 1440×850, including hover growth and return, slide controls, two-tier switch, simulated window open/close, reset, and absence of runtime script errors. Automated repository tests verify that this demo stays self-contained and offline; they are not equivalent to live Windows taskbar testing.
