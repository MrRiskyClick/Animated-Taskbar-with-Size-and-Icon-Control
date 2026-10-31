"""Static audit of developer-only native Windhawk source, not proof of safe Explorer behavior.
No Windows shell access, no registry mutations and no mod compilation in this test.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
MOD = (ROOT / 'experiments' / 'atb-native-visual-trial.wh.cpp').read_text(encoding='utf-8')
WATCHER = (ROOT / 'experiments' / 'run-visual-trial-watchdog.ps1').read_text(encoding='utf-8')

class NativeContract(unittest.TestCase):
    def test_metadata(self):
        for token in ('// @id              atb-animated-taskbar-trial',
                      '// @include           explorer.exe',
                      '// @architecture      x86-64'):
            self.assertIn(token, MOD)

    def test_only_trial_can_start_hooks(self):
        start = MOD.index('BOOL Wh_ModInit()')
        end = MOD.index('void Wh_ModAfterInit()', start)
        init = MOD[start:end]
        self.assertIn('if (!atb_trial::Arm())', init)
        self.assertIn('return FALSE;', init)
        self.assertLess(init.index('atb_trial::Arm()'), init.index('HookTaskbarViewDllSymbols('))

    def test_current_scale_based_spacing(self):
        start = MOD.index('void ApplyAnimation(double mouseX, DockAnimationContext& ctx, double intensity, double dtSec) {')
        end = MOD.index('\nvoid ResetAllIconScales(', start)
        body = MOD[start:end]
        self.assertIn('Pass 1: update the actual interpolated size', body)
        self.assertIn('Pass 2: derive all X positions', body)
        self.assertIn('state.waveTranslateX = shift;', body)
        self.assertIn('displayedScale = state.waveScale * bounceMultiplier;', body)
        self.assertNotIn('state.waveTranslateX +=', body)

    def test_release_guard(self):
        self.assertIn('deadlineTick', MOD)
        self.assertIn('lastCheckedTick', MOD)
        self.assertIn('if (!atb_trial::Check())', MOD)
        self.assertIn('AtbRestoreAll();', MOD)
        self.assertIn('if (!ReadLease(&expires))', MOD)

    def test_no_unsupported_system_rollback_claim(self):
        # This PR cannot be marketed as full rollback.
        self.assertNotIn('SHAppBarMessage(', MOD)
        self.assertNotIn('SystemParametersInfoW(', MOD)
        self.assertNotIn('RegSetValueEx', MOD)
        self.assertNotIn('TerminateProcess(', MOD)
        self.assertIn('The PowerShell process', (ROOT / 'docs' / 'NATIVE_VISUAL_ENGINE_STATUS.md').read_text(encoding='utf-8'))

    def test_pips_have_real_group_count_and_reverse_cleanup(self):
        self.assertIn('g_atbGetGroupFromButton', MOD)
        self.assertIn('g_atbGetGroupCount', MOD)
        self.assertIn('raw > 0 ? raw - 1 : 0', MOD)
        self.assertIn('AtbRestorePips();', MOD)
        self.assertIn('if (count <= 3)', MOD)
        self.assertIn('bar.Width(22.0)', MOD)
        self.assertIn('overlay.IsHitTestVisible(false)', MOD)

    def test_native_overhang_is_explicit_and_geometry_lease_bound(self):
        self.assertIn('EnableNativeOverhang: false', MOD)
        self.assertIn('g_settings.enableNativeOverhang', MOD)
        self.assertIn('if(!g_settings.enableNativeOverhang || a.active ||', MOD)
        self.assertIn('atb_trial::geometry.valid', MOD)
        self.assertIn('ATB3 %lu %llu %llu %lu %ld %ld %ld %ld %ld %u %llu %c', MOD)
        self.assertIn('std::memcmp(&geometry.original,&observed.original,sizeof(RECT))', MOD)
        self.assertIn('SWP_NOSENDCHANGING', MOD)
        self.assertIn('AtbNativeWorkareaStillPreserved', MOD)
        self.assertIn('AtbSetRegion(bottomOnly)', MOD)
        self.assertIn('AtbMakeBottomRegion()', MOD)
        self.assertIn('AtbBaselineRegionMatches(hwnd)', MOD)
        self.assertIn('AtbRestoreBaselineRegion()', MOD)
        self.assertIn('originalUtcDeadline', MOD)
        self.assertIn('geometry.baselineRegionHash!=observed.baselineRegionHash', MOD)
        self.assertIn('bool AtbBaselineRegionMatches(HWND hwnd)', MOD)
        self.assertNotIn('Overhang refused: native HWND already has a region', MOD)
        self.assertIn('AtbRestoreNativeOverhang();', MOD)
        self.assertNotIn('SetTaskbarAutoHide', MOD)
        self.assertNotIn('ABM_NEW', MOD)

    def test_secondary_monitor_never_controls_primary_native_hwnd(self):
        self.assertIn('GetCursorPos(&cursor)', MOD)
        self.assertIn('PtInRect(&before,cursor)', MOD)
        self.assertIn('MonitorFromPoint(cursor,MONITOR_DEFAULTTONULL)', MOD)
        self.assertIn('winrt::get_abi(frame)!=winrt::get_abi(a.frame.get())', MOD)

    def test_native_root_grows_but_original_height_children_remain_bottom_anchored(self):
        self.assertIn('taskbarFrame.Height(a.frameAppliedHeight);', MOD)
        self.assertIn('const double extraDip=(double)extra/a.dpiScale;', MOD)
        self.assertIn('original.appliedHeight=rootStripHeight;', MOD)
        self.assertIn('child.Name()==L"RootGrid"', MOD)
        self.assertIn('rootGrid.Height(rootStripHeight);', MOD)
        self.assertIn('rootGrid.RenderTransform(translated);', MOD)
        self.assertIn('a.shiftedChildren.push_back(std::move(original));', MOD)
        self.assertIn('element.TransformToVisual(visualRoot).TransformBounds(', MOD)
        self.assertNotIn('auto shouldShift=[]', MOD)
        self.assertIn('for(auto it=a.shiftedChildren.rbegin();', MOD)
        self.assertIn('child.Height(it->originalHeight)', MOD)

    def test_no_foreign_window_region_is_erased(self):
        self.assertIn('ATB_REGION_FINGERPRINT_V2', MOD)
        self.assertIn('AtbOurRegionFingerprintMatches()', MOD)
        self.assertIn('GetRegionData(region,0,nullptr)', MOD)
        self.assertIn('GetWindowRgn(a.hwnd,current)', MOD)
        watcher = (ROOT / 'experiments' / 'run-native-overhang-trial.ps1').read_text(encoding='utf-8')
        self.assertIn('RegionFingerprintMatches($trialHwnd)', watcher)
        self.assertIn('RegionFingerprintSelfTest()', watcher)
        self.assertIn('RegionPreservationSelfTest()', watcher)
        self.assertIn('Original taskbar window region:', watcher)
        self.assertIn('ATB3 {0}', watcher)
        self.assertIn('Watchdog restored exact original taskbar window region', watcher)
        self.assertIn('SetWindowRgn($trialHwnd,$restore,$true)', watcher)
        self.assertNotIn('SetWindowRgn($trialHwnd,[IntPtr]::Zero,$true)', watcher)
        self.assertIn('GetRegionData(region,length,buffer)', watcher)
        self.assertNotIn('Stop-Process -Name explorer', watcher)

    def test_manual_restart_only_and_no_persistent_shell_settings(self):
        # Hard requirement: a hung Explorer is left for the user to restart.
        # The native mod only changes transient HWND/XAML state, never global
        # taskbar preferences, registry or power/restart state.
        for forbidden in (
            'RegSetValueEx(', 'RegCreateKeyEx(', 'RegDeleteTree(',
            'SHAppBarMessage(', 'SystemParametersInfoW(',
            'TerminateProcess(', 'ExitWindowsEx(', 'InitiateSystemShutdown',
        ):
            self.assertNotIn(forbidden, MOD, forbidden)

        watcher = (ROOT / 'experiments' / 'run-native-overhang-trial.ps1').read_text(encoding='utf-8')
        disarm = (ROOT / 'experiments' / 'windhawk-17-own-mod-disarm.ps1').read_text(encoding='utf-8')
        for forbidden in (
            'Stop-Process -Name explorer', 'Restart-Computer',
            'taskkill.exe', 'shutdown.exe', 'Restart-Service',
            'Set-ScheduledTask', 'Register-ScheduledTask',
        ):
            self.assertNotIn(forbidden.lower(), (watcher + disarm).lower(), forbidden)
        self.assertIn("local@atb-animated-taskbar-trial", disarm)
        self.assertIn("Assert-AtbOwnModReadable", watcher)
        self.assertIn("OpenSubKey($keyName,$false)", disarm)
        self.assertNotIn("Disable-OnlyAtbLocalMod", watcher)
        self.assertIn('Write-Lease "REVOKED', watcher)
        self.assertIn('AtbRestoreNativeOverhang();', MOD)
        self.assertIn('if (!atb_trial::Arm())', MOD)

    def test_no_admin_requirement_or_production_registry_writes(self):
        module = (ROOT / 'experiments' / 'windhawk-17-own-mod-disarm.ps1').read_text(encoding='utf-8')
        watcher = (ROOT / 'experiments' / 'run-native-overhang-trial.ps1').read_text(encoding='utf-8')
        preflight = (ROOT / 'experiments' / 'check-local-trial-prerequisites.ps1').read_text(encoding='utf-8')
        self.assertIn('Assert-AtbOwnModReadable', watcher)
        self.assertIn('Assert-AtbOwnModReadable', preflight)
        self.assertIn('OpenSubKey($keyName,$false)', module)
        self.assertIn('Write-Lease "REVOKED', watcher)
        self.assertIn('explorerPid!=GetCurrentProcessId()', MOD)
        for forbidden in ('SetValue(', 'Disable-OnlyAtbLocalMod',
                          'Assert-AtbOwnModWritable', 'Start-Process -Verb RunAs',
                          'RegSetValueEx', 'Set-ItemProperty'):
            self.assertNotIn(forbidden, module + watcher + preflight)
        self.assertIn("if($disabled -ne 1)", watcher)
        self.assertIn('trial mod is enabled', preflight.lower())

    def test_watchdog_isolation(self):
        self.assertIn('$SelfTest', WATCHER)
        self.assertIn('atb-dev-selftest-', WATCHER)
        self.assertIn('Publish-Lease "REVOKED', WATCHER)

if __name__ == '__main__':
    unittest.main()
