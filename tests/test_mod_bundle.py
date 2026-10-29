"""Static one-file Windhawk bundling safety test."""
from __future__ import annotations

import unittest
from scripts.bundle_windhawk import MARKER, bundle


class BundledModTests(unittest.TestCase):
    def test_no_shell_mutation_and_no_unresolved_include(self):
        result = bundle()
        self.assertNotIn(MARKER, result)
        self.assertIn("LeaseV1IsFresh", result)
        self.assertIn("Wh_GetStringSetting", result)
        self.assertIn("120-second read-only observation", result)
        self.assertIn("ReadBoundedFile", result)
        # No native taskbar geometry or registry state may be modified by
        # this Stage 1b diagnostic build.
        for forbidden in ("SetWindowPos(", "MoveWindow(",
                          "RegSetValueEx", "Wh_SetFunctionHook(",
                          "SHAppBarMessage(ABM_SETSTATE"):
            self.assertNotIn(forbidden, result)


if __name__ == "__main__":
    unittest.main()
