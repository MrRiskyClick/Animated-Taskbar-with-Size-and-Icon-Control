# First manual functional test — GitHub CI-green source

**Reviewed test source commit:** `babf39cffc9f2813528b444e4316f5a3dd70dd94`

**GitHub Actions verification:**
[Native Visual CI #41141979586](https://github.com/MrRiskyClick/Animated-Taskbar-with-Size-and-Icon-Control/actions/runs/41141979586)
— **SUCCESS**, including:
- Full official Windhawk 1.7.3 Clang compilation/link.
- Native animation geometry, 100,000 frame-coherent size/spacing scenarios.
- Disposable 42px reservation/96px window and click-through geometry.
- Native `Shell_TrayWnd` resize with work area unchanged; independent
  watchdog restoration, including after forced controller death.
- ATB2 watcher isolated revocation, stale lease fail-closed behavior,
  region fingerprint conflict protection.
- Read-only installed-mode prerequisite fixture, independently revoked
  PID-bound trial lease and rejection of stale/dead watcher leases.
- Competing Ph0en1x-dev Dock Animation registry fixture is rejected
  without modifying that mod.

**Important:** The above establishes a functioning compiled developer
candidate and Windows primitive/recovery behavior on GitHub-hosted runners.
It does **not** prove the native XAML controls render or hit-test correctly
on the user's Windows installation. That is the next manual test.

## Retrieve exact verified source — user runs on their PC ONLY

Use PowerShell in whichever directory you want the Git repository to reside.

```powershell
$repo = 'https://github.com/MrRiskyClick/Animated-Taskbar-with-Size-and-Icon-Control.git'
$dest = Join-Path (Get-Location) 'Animated-Taskbar-Git'
$commit = 'babf39cffc9f2813528b444e4316f5a3dd70dd94'
if (Test-Path -LiteralPath $dest) {
    throw "Destination exists, will not overwrite: $dest"
}
git clone --single-branch --branch feature/full-native-mod-integration-v1 $repo $dest
if ($LASTEXITCODE -ne 0) { throw 'Clone failed' }
git -C $dest checkout --detach $commit
if ($LASTEXITCODE -ne 0) { throw 'Pinned checkout failed' }
if ((git -C $dest rev-parse HEAD).Trim() -ne $commit) {
    throw 'Commit provenance mismatch'
}
Write-Host "Verified source at $dest"
```

**This command only creates a new Git repository folder.** It does not
install Windhawk, run the mod, edit Windows settings, or open any process.

## Functional test steps

Read [CONTROLLED_NATIVE_TEST.md](CONTROLLED_NATIVE_TEST.md) before proceeding.

1. Import the exact `experiments/atb-native-visual-trial.wh.cpp` source
   via Windhawk's local editor, keeping the mod disabled.
2. To test unclipped height, set its `EnableNativeOverhang` to true
   **while the mod remains disabled** (default is false).
3. Check the local mod identity and permission using the source tree's
   `experiments/check-local-trial-prerequisites.ps1` (read-only).
4. Start the standalone `experiments/run-native-overhang-trial.ps1` watcher
   in a user-controlled PowerShell window. If any prerequisite fails,
   STOP and report the error rather than modifying registry permissions.
5. Manually enable this one local Windhawk mod during the watchdog lease,
   then test the real taskbar UI. The watchdog expires at 120 seconds from
   its start, revokes the lease and requests native restoration. The
   Windhawk toggle may remain enabled: **manually switch it OFF after testing**.
6. If Explorer becomes unresponsive, the user handles Task Manager's
   **Restart Windows Explorer** or reboots manually. The project will
   **never** automate this escalation.

Do not disable other Windhawk mods automatically. A separately enabled
Taskbar Dock Animation sharing our native hooks causes the read-only
preflight to refuse this test. You may choose to disable it manually before
a subsequent attempt.

## Manual acceptance checklist

- Native taskbar retains original reserved height even when icons grow.
- Enlarged icons are visible without clipping and push neighbours.
- Empty overhang clicks through; enlarged icons still receive clicks.
- Start, search/task view, running app activation, grouped previews,
  right-click Jump Lists and tray remain functional.
- Pinned app without running windows has no pip; 1,2,3 windows have 1,2,3
  pips; 4+ uses a wide bar.
- After the trial, original geometry returns and other Windhawk mods
  remain unchanged. If necessary, a user-controlled Explorer restart
  recreates the original native taskbar without re-enabling the trial mod.

**Not production-released:** manual validation and independent base
taskbar/icon resource-size controls are still outstanding. Retain PR #23
as a draft until user approval. All further source changes stay in GitHub.
