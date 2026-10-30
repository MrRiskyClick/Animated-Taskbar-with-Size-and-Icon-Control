# Animated Taskbar — first real Windhawk test (0.1.0)

**Status: experimental local Windhawk mod, NOT production-safe and NOT approved for general installation.** There is no verified out-of-process watchdog restoring real XAML state after all failure types. This matters because the mod is injected into `explorer.exe`. Never describe this as a tested replacement for Windows taskbar. Do not publish to Windhawk's public catalog until native builds, live Windows testing, and real rollback gates pass.

## Which file

`mods/animated-taskbar-safe-preview.wh.cpp` in the branch `feature/native-windhawk-hover-test`.

The file is a modified derivative of **Ph0en1x-dev / Taskbar Dock Animation v1.9.2**, pinned upstream to `ramensoftware/windhawk-mods` revision `e56788af4cc2878a2d52ed241e9753d07b585597` (MIT by the official collection's default submission policy). Retained original core hook/WinRT traversal and added only native single-use gating plus synchronized icon expansion/positioning. This is not endorsed by the original developer.

## Supported in this first native test

- Actual native Windows 11 taskbar icon hover scale with neighboring icon push.
- One set of XAML transforms, adjusted together each render frame so position follows *current* animated scale rather than a second lagging translation spring.
- Adjustable magnification `MaxScale`, effect radius, speed `LerpSpeed`, fade `FocusDuration`, optional idle bounce, and app-only animation mode.
- Passive by default; an OFF→ON transition for `ArmOneShotPreview` arms one 120-second period. The time cannot be extended by changing other settings. The setting is persistent, but no trial auto-arms when Explorer loads or restarts.
- Restores neutral scale and translation on focus exit, mod disable, settings change or while the rendering loop observes expiry.

## NOT yet implemented from the v0.4 browser design

- Native taskbar height or base icon size independent of one another.
- Native icon vertical offset and resting horizontal gap.
- Native pip position or one/two/three/wide pip indicators based on per-app window counts.
- A native removed app separator (the HTML demo divider is already removed; Windows' native taskbar is different).
- Automatic trusted rollback if the Explorer process/UI thread is unresponsive, another mod overwrites the same XAML property, or the whole computer reboots. **The existing Stage 1 native watchdog only restores mock files.**
- No Windows 10, StartAllBack, third-party taskbar shell, or multi-monitor behavior is verified.

We intentionally do NOT expose controls for functions not implemented in the native code.

## Installation path for a *disposable test environment only*

**Do not test active effects on your daily Windows setup yet** if you require strict independent rollback. First record your existing Windhawk mods/settings and a Windows restore route. The project needs a real independent supervisor before general live testing.

1. In Windhawk (Windows 11 x64), use **Create a new mod** and paste the **entire contents** of `mods/animated-taskbar-safe-preview.wh.cpp`. Compile locally; do not obtain a prebuilt DLL.
2. The test mod is separate from `taskbar-dock-animation` and `taskbar-dock-animation-plus`. They hook the same TaskbarFrame pointer symbols and manipulate the same XAML transforms. **Do not run these together** in an active test. No program will disable or uninstall another mod automatically. Preserve its settings if you temporarily pause it yourself for a test.
3. On a disposable VM or Windows test account with recovery/snapshot, open this mod's **Settings**. Leave `ArmOneShotPreview=false` to confirm the passive mod loads and logs.
4. Once native compilation and rollback behavior have been reviewed, explicitly arm one test by switching **OFF** and saving, then **ON** and saving. Move slowly/quickly across taskbar app icons at initially conservative `MaxScale=125` and `SpacingFactor=100`. The internal 120-second deadline is only best-effort; it is **not an out-of-process safety guarantee**.
5. Immediately disable the local mod if taskbar behavior is abnormal. In Windhawk 1.x use its built-in mod switch; on Windhawk 2.x use its mod UI. If Explorer itself stops functioning, use the known Windhawk safe-mode recovery instructions for *emergency manual fallback only*. Do not reset other mods.
6. Confirm, manually, each icon returns to its original position and size after the test is disarmed and after the mod is disabled. Record any clipping, taskbar replacement, multi-monitor, keyboard/focus and installed-styler interactions.

### Feedback to record

- Windows version via `winver`, installed Windhawk version, existing taskbar-related mod IDs/versions (do not share private keys or other secrets).
- Whether the mod compiled locally; if not, provide the exact first compiler error.
- Does pointer hover animate? Are icons spaced without collision at 125%/150%? Does it stop after the trial expires? Are all original positions restored after disabling?
- Screenshots and screen recordings are **user-owned visual verification**. CI/static snapshots are *not* proof of correct native Windows animation or recovery.

## Submission to Windhawk public catalog

The official collection accepts a PR containing exactly one `mods/<id>.wh.cpp` source file with correct `@github` metadata identifying the PR author. Do **not** open that upstream PR until the native mod is complete and explicitly approved for publication. Verify that the original MIT attribution and updated license/metadata are retained. See https://github.com/ramensoftware/windhawk-mods and https://github.com/ramensoftware/windhawk/wiki/Creating-a-new-mod .
