"""Static one-file Windhawk bundling safety test."""
from __future__ import annotations

import unittest
from scripts.bundle_windhawk import MARKER, IPC_MARKER, TRANSPORT_MARKER, bundle


class BundledModTests(unittest.TestCase):
    def test_no_shell_mutation_and_no_unresolved_include(self):
        result = bundle()
        self.assertNotIn(MARKER, result)
        self.assertNotIn(IPC_MARKER, result)
        self.assertNotIn(TRANSPORT_MARKER, result)
        self.assertIn("atb_readonly::Probe", result)
        self.assertIn("atb_transport::OpenClient", result)
        self.assertIn("diagnosticPipeSuffix", result)
        self.assertIn("diagnosticPeerPid", result)
        self.assertIn("diagnosticPeerExecutable", result)
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
