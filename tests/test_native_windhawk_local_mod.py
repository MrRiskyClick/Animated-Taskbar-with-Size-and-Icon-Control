"""Static contract for an experimental Windhawk source. NOT a native compile/runtime test."""
from pathlib import Path
import re
import unittest

SOURCE=Path(__file__).resolve().parents[1] / "mods" / "animated-taskbar-safe-preview.wh.cpp"

class LocalModContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source=SOURCE.read_text(encoding="utf-8")

    def test_windhawk_metadata_and_unique_mod_id(self):
        c=self.source
        self.assertIn("// ==WindhawkMod==", c)
        self.assertIn("// ==/WindhawkMod==", c)
        self.assertIn("@id              animated-taskbar-safe-preview", c)
        self.assertIn("@include           explorer.exe", c)
        self.assertIn("@architecture      x86-64", c)
        self.assertIn("@license           MIT", c)
        self.assertEqual(c.count("BOOL Wh_ModInit()"), 1)
        self.assertEqual(c.count("void Wh_ModBeforeUninit()"), 1)

    def test_disabled_on_load_and_explicit_rearm(self):
        c=self.source
        self.assertIn("- ArmOneShotPreview: false", c)
        self.assertIn("g_previousArmSetting = (Wh_GetIntSetting(L\"ArmOneShotPreview\") != 0)", c)
        self.assertIn("g_liveArmed.store(false);", c)
        self.assertIn("requested && !g_previousArmSetting", c)
        self.assertIn("g_liveDeadline.store(GetTickCount64() + 120000ULL)", c)
        self.assertIn("if (prior) return;", c)
        self.assertIn("RestoreOwnedXamlTransforms();", c)
        self.assertIn("g_ownedTransforms.emplace(ownedKey", c)
        self.assertIn("recorded->second.installed", c)
        self.assertIn("element.RenderTransform(entry.second.previous);", c)
        self.assertIn("element.RenderTransformOrigin(entry.second.previousOrigin);", c)
        self.assertIn("if (!IsLiveTrialActive()) return original();", c)
        self.assertIn("if (!IsLiveTrialActive()) {", c)

    def test_spacing_uses_actual_current_scale_and_no_position_spring(self):
        c=self.source
        self.assertIn("- SpacingFactor: 100", c)
        self.assertIn("if (g_settings.spacingFactor < 1.0)", c)
        self.assertIn("current += (desiredScale - current) * alpha", c)
        self.assertIn("extraWidths[i] = std::max(0.0, current - 1.0)", c)
        self.assertIn("ctx.icons[i].state.waveTranslateX = finalShift;", c)
        self.assertIn("} else if (currentIntensity <= 0.001) {", c)
        self.assertNotIn("ctx.icons[i].state.waveTranslateX += (targetTranslateX", c)

    def test_no_windows_registry_taskbar_height_changes(self):
        c=self.source
        for api in ("RegSetValue", "RegCreateKey", "SHAppBarMessage",
                    "SetWindowPos(", "SystemParametersInfoW("):
            self.assertNotIn(api,c)

if __name__=="__main__":
    unittest.main()
