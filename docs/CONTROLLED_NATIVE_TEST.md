# Controlled Native Windhawk Test — USER-OPERATED ONLY

**Status:** Experimental, not a production release. This is the first *functional*
native taskbar animation/pip/overhang test. It is not another inventory or
read-only screenshot diagnostic. The user is the sole UI/behavioral tester.

**Hard rule:** No remote access to the user's computer, no automatic
Explorer/process restart, and no automated Windhawk enable/install. All
development and CI remain in GitHub. The user runs local commands manually.

**Source branch:** `feature/full-native-mod-integration-v1`.
**Native source:** `experiments/atb-native-visual-trial.wh.cpp`.
**Independent watcher:** `experiments/run-native-overhang-trial.ps1`.
**Read-only preflight:** `experiments/check-local-trial-prerequisites.ps1`.

## Before beginning

1. Review the exact code in GitHub before retrieving it. Do not install from
   an unreviewed or failed CI commit. Do not use a binary from a download site.
2. Do not run this against an unknown Windhawk version. This developer
   prototype targets **Windhawk 1.7.x installed mode**, with a specifically
   verified `local@atb-animated-taskbar-trial` mod. It fails closed on
   portable/custom-storage installations.
3. **No need to disable other Windhawk mods.** Do not change other mod
   settings to help our trial pass. If another mod changes native taskbar
   transforms/regions at the same time, this prototype may refuse overhang
   instead of clobbering its changes.
4. Keep Ctrl+Shift+Esc (Task Manager) available as the user's manual method
   to restart Windows Explorer if needed; the system may require a manual
   reboot if Explorer is permanently unresponsive. We will not automate it.
5. The watcher changes **only** our local Windhawk mod's persistent
   `Disabled` REG_DWORD from 0 to 1 at expiration, by design. It also
   leaves a revoked `lease.v1` and a small `watcher.lock` under
   `%LOCALAPPDATA%\AnimatedTaskbarTrial`. It must not change Windows
   taskbar alignment/auto-hide/grouping, system appbar reservation, or other
   mods' persistent settings.

## Retrieve reviewed source (user executes when test is authorized)

Do not run this while developing; GitHub remains the source of truth.

```powershell
$repo = 'https://github.com/MrRiskyClick/Animated-Taskbar-with-Size-and-Icon-Control.git'
$path = Join-Path (Get-Location) 'Animated-Taskbar-Git'
if (Test-Path -LiteralPath $path) { throw "Will not overwrite existing path: $path" }
git clone --single-branch --branch feature/full-native-mod-integration-v1 $repo $path
if ($LASTEXITCODE -ne 0) { throw 'Git clone failed' }
git -C $path status --short
```

For an actual trial, pin the working tree to the **exact GitHub Actions-green
commit** from the accompanying release note before proceeding.

## Install the test mod manually in Windhawk's editor

1. In Windhawk, create or import a **local** mod with the source from
   `experiments/atb-native-visual-trial.wh.cpp`. Do not install a random
   similarly named catalog mod. It must be the ID
   `local@atb-animated-taskbar-trial` in Windhawk's own configuration.
2. Keep the new mod **disabled** until its watchdog is running. If Windhawk
   automatically tries to initialize it before the lease, it fails closed,
   because `Wh_ModInit` refuses activation without an appropriate watcher.
3. For a full native layout test, enable the experimental **Native unclipped
   overhang** setting only while the mod itself remains disabled. The
   setting is OFF by default. Do not change Windows' taskbar auto-hide mode.
4. If you have **Taskbar Dock Animation** (Ph0en1x-dev) separately
   enabled, our read-only preflight will refuse the trial because the two
   native mods hook and animate the same buttons. This project will NOT
   disable that other mod or change its configuration. You may choose to
   disable it yourself for the duration of a later controlled test. Other
   Windhawk mod settings must be left unchanged.
5. Open **64-bit PowerShell**. Execute this *read-only preflight* from the
   cloned repository:
   ```powershell
   & '.\experiments\check-local-trial-prerequisites.ps1'
   ```
   If it reports missing key, unsupported mode, or insufficient permission:
   **STOP**. Do not edit registry permissions or recreate keys to force it.
   Report the exact failure message; GitHub code will be updated instead.

## One-shot 120-second visual test

1. After the preflight passes, start this script manually in the same folder:
   ```powershell
   & '.\experiments\run-native-overhang-trial.ps1'
   ```
   It publishes an expiring ATB2 lease in a *new* PowerShell process and
   refuses to rearm over any unresolved prior trial journal. Keep that
   terminal open for the trial.
2. In Windhawk, manually **enable only the Animated Taskbar local trial**.
   Move your pointer over the native application row. Test icon enlargement,
   smooth neighbour displacement, bounce, Start/search/tray behavior,
   grouped previews, right-click Jump Lists and running-window pips.
3. The watchdog independently disables this one mod at expiration and
   attempts to restore the native HWND/region. In normal operation the
   XAML UI timer also restores temporary properties.
4. Verify that the desktop work area and original taskbar footprint are
   unchanged; other Windhawk mods should still be enabled.

If Windows freezes: **do not** ask this project to force a restart.
The user can restart Windows Explorer manually through Task Manager or
reboot. After Explorer recreates its HWND, our temporary geometry and XAML
objects are gone. The revoked lease and `Disabled=1` prevent silent rearm.

## Observations for the next GitHub iteration

Report actual UI outcomes only (user confirmation required): whether the
taskbar remained compact, magnification renders outside its normal height,
whether moving over empty overhang clicks underlying apps, whether all
native buttons remain clickable, and whether the 0/1/2/3/4+ pips are correct.
Record any conflict with other mods and whether normal behavior returns
after expiration/manual restart.

## Important limitations

- GitHub CI validates real HWND geometry on disposable runner OS, but
  **cannot prove** that your version of XAML visually paints or hit-tests
  magnified native buttons correctly.
- A completely frozen Explorer process might ignore async resize requests
  until you restart it. This is why manual Explorer/reboot recovery is
  permitted rather than an unsafe automatic shell kill.
- This remains a **controlled functional candidate** and must not be
  described as fully tested or published to Windhawk before the user's
  real-world results.
