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

    def test_watchdog_isolation(self):
        self.assertIn('$SelfTest', WATCHER)
        self.assertIn('atb-dev-selftest-', WATCHER)
        self.assertIn('Publish-Lease "REVOKED', WATCHER)

if __name__ == '__main__':
    unittest.main()
