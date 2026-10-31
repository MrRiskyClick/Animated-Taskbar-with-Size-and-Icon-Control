# Native overhang v0.3.2 — controlled user retest

**Status:** developer test candidate; only the user verifies actual Windows
taskbar visuals and interactions.

**CI-green source+watchdog commit:**
`e1cae144ea6c0bbc723c4484d6f6012f3a09c624`.
[Native GitHub CI run 41148864223](https://github.com/MrRiskyClick/Animated-Taskbar-with-Size-and-Icon-Control/actions/runs/41148864223):
all three jobs passed (official Windhawk 1.7.3 DLL, portable contracts,
and GitHub-hosted Windows watchdog/region snapshot fixtures).

### Why this build is different

User's v0.3.1 log showed every attempt refused at:
`Overhang refused: native HWND already has a region`.
The code refused any pre-existing native window region.

v0.3.2 introduces a coordinated **ATB3** watchdog + native source. The
watchdog takes an independent exact pre-trial window-region snapshot; the
lease pins its 64-bit hash and original mode. The native mod only temporarily
replaces that window region when it matches the watcher snapshot, shifts
the original hit region down and merges in scaled icon bounds above it.
Both independently restore the exact original region only when they can
verify temporary ownership. A foreign change is left untouched. The old
ATB2 watcher CANNOT authorize this new native mod.

### Update the existing clean Git clone (user executes)

Start from:
`C:\Users\Dylan\Desktop\taskbar mod v0.1` in Windows PowerShell.

```powershell
$dest = Join-Path (Get-Location) "Animated-Taskbar-Git"
$commit = "e1cae144ea6c0bbc723c4484d6f6012f3a09c624"
if (git -C $dest status --porcelain) { throw "Local Git changes: stop" }
git -C $dest fetch origin feature/full-native-mod-integration-v1
if ($LASTEXITCODE -ne 0) { throw "Fetch failed" }
git -C $dest checkout --detach $commit
if ($LASTEXITCODE -ne 0) { throw "Checkout failed" }
if ((git -C $dest rev-parse HEAD).Trim() -ne $commit) { throw "Wrong revision" }
Get-Content -LiteralPath "$dest\experiments\atb-native-visual-trial.wh.cpp" -Raw -Encoding UTF8 | Set-Clipboard
```

**Important for Windows PowerShell 5.1:** ALWAYS use `-Encoding UTF8`
when copying the source. Without it, localized settings become corrupted
and Windhawk reports non-printable characters.

### Replace the existing local mod source (not a new mod)

1. In Windhawk, open the existing
   `local@atb-animated-taskbar-trial` editor.
2. Leave **Enable mod OFF**. Replace all editor source with clipboard content.
3. Confirm metadata `@version 0.3.2` and compile in the editor.
4. In mod settings, keep **Native unclipped overhang ON**.
   Enable Logging ON. No registry or elevation changes.

### Supervised one-shot test

Run in the same PowerShell window:
```powershell
& "$dest\experiments\check-local-trial-prerequisites.ps1"
```
If it fails, do NOT enable the mod.

After preflight passes, run the **updated ATB3** watchdog:
```powershell
& "$dest\experiments\run-native-overhang-trial.ps1"
```
Confirm it prints
`[ATB] Original taskbar window region: mode=1, fingerprint=...`
(or mode 0 if Windows changed) then
`[ATB] Geometry trial armed ...`.

Only after `armed`, manually enable the trial mod in Windhawk.
Mouse over several native icons for magnification/push-out.
Check clipping, clicks, Start, tray, grouped previews and Jump Lists.
120 seconds after the watchdog starts, authorization is revoked.
Manually turn the local mod **OFF** after the trial.

If Explorer becomes unresponsive, only the user can restart Explorer
through Task Manager or reboot. There is no automatic Explorer restart
and no registry writes. If the independent watchdog cannot restore
geometry or original region while Explorer is frozen, a manual restart
may be necessary.

### Return evidence

Please report:
- whether the enlarged icons still clip at the top;
- whether Start/tray and normal icon clicks work;
- any WinHawk log lines with `[ATB]`, especially
  `Watchdog owns original HWND region`,
  `Native overhang trial active`,
  `XAML RootGrid bottom-anchored`, or
  `Overhang refused`;
- whether original taskbar state returns after expiry.

A successful CI build is **not** proof that native Windows 11 XAML painting,
hit-testing, or other Windhawk mod compatibility is correct.
