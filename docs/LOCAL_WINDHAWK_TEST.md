# First local Windhawk compile test — safe diagnostic package

## What this is

- **Testable Windhawk mod:** [dist/animated-taskbar.wh.cpp](../dist/animated-taskbar.wh.cpp) is a generated, stand-alone, single-file C++ Windhawk mod that targets Explorer. It can be pasted into Windhawk's mod editor and compiled.
- **What it does:** reads primary taskbar/work-area rectangles, starts an optional read-only lease/IPC observation, then stops observation after 120 seconds. No Explorer hooks or taskbar/XAML/work-area/registry writes.
- **What it does NOT do yet:** resize the taskbar, show a taller transparent input region, animate native icons, draw pips, or change the user's existing styles. The standalone motion/pip algorithms in [src/core/dock_motion.hpp](../src/core/dock_motion.hpp) are not connected to native buttons.
- The optional lease is for diagnostics, not authenticated permission. **DO NOT activate taskbar-changing experiments before the independent watchdog and real rollback gates pass.**

## Verify the source is single-file and current

The checked-in dist file is generated from the development branch's source and three bundled local headers:

```powershell
py -3 scripts/bundle_windhawk.py --check
py -3 scripts/bundle_windhawk.py
```

Compare the resulting dist file to the version in GitHub. Do not paste **src/animated-taskbar.wh.cpp**, which requires local headers not present in Windhawk's single-file compiler.

## Windhawk compilation-only smoke test

1. Record Windows version/build and Windhawk version. Keep all existing Windhawk settings and mods unchanged. Do not overwrite any existing mod with the same ID.
2. In Windhawk, select **Create a New Mod**. Replace the starter template with the **entire** source of [dist/animated-taskbar.wh.cpp](../dist/animated-taskbar.wh.cpp).
3. Compile using the Windhawk editor's **Compile Mod** command. Report compiler output, including the first error line if compilation fails.
4. Keep all diagnostic settings blank/zero. Exit editing mode; if enabled, the mod should log geometry and read-only observation messages under ID `animated-taskbar-size-icon-control`. There should be **no visual taskbar changes**.
5. Disable/remove this test mod when done. Do **not** disable or reset Taskbar Styler, Dock Animation, Start Menu Styler, Notification Center Styler or tray tweaks.
6. **If the taskbar actually changes**, stop the test, disable this mod and report the full Windhawk log; this is not expected from the diagnostic source.

The 120-second observation timeout does not revert anything because no state is modified. It is **not** the requested independent 120-second recovery mechanism.

## Test the movement and pip mathematics without shell changes

On Windows with a C++17 compiler, or in GitHub CI:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -pedantic tests/dock_motion_tests.cpp -o dock_motion_tests
./dock_motion_tests
```

This checks icon-gap preservation during hover motion even at 250% scale, idle return and pip mapping: 0=none, 1–3=corresponding count, 4+=bar. It does not enumerate real application windows or draw indicators. The original HTML design is the behavioral reference, not production-native code.

## Prerequisites before a live animation trial

- Confirm installed Windhawk version: 1.x lacks the official 2.x CLI disable pathway. Do not assume the CLI exists.
- Finish authenticated native watchdog↔Explorer handshake and narrowly scoped inverse journal; prove loss/restart/expiry handling and **120-second no-reapply** under fault injection.
- Identify supported native taskbar XAML controls and inspect clipping, grouping, window count and pip ownership without mutating state.
- Integrate the tested motion calculations with native icon transforms, keeping only **one** hover animation engine active on those elements. Preserve the user's installed theme mods' settings.
- Only then package a **separate explicitly armed 120-second live trial**, followed by the user's visual/interaction tests and an explicit go/no-go before submitting to Windhawk.

References: [SAFETY.md](SAFETY.md), [STAGES.md](STAGES.md), [TESTING.md](TESTING.md).
