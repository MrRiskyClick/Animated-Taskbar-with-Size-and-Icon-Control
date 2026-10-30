"""Offline, read-only taskbar visual demo safety contract (no browser install)."""
from __future__ import annotations

from html.parser import HTMLParser
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
PREVIEW = ROOT / "demo" / "preview.html"


class Tags(HTMLParser):
    def __init__(self):
        super().__init__()
        self.tags = []
        self.attributes = []
    def handle_starttag(self, tag, attrs):
        self.tags.append(tag)
        self.attributes.extend(attrs)


class PreviewContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.html = PREVIEW.read_text(encoding="utf-8")
        cls.tags = Tags()
        cls.tags.feed(cls.html)

    def test_entire_demo_runs_offline_in_a_single_html_file(self):
        self.assertIn("<style>", self.html)
        self.assertIn("<script>", self.html)
        for tag in ("iframe", "form", "object", "embed"):
            self.assertNotIn(tag, self.tags.tags)
        for name, value in self.tags.attributes:
            if name in ("src", "href", "action"):
                self.assertFalse(
                    value and (value.startswith("http:") or
                               value.startswith("https:") or
                               value.startswith("//")),
                    f"External network resource found: {name}={value!r}")

    def test_has_no_system_api_or_network_access_paths(self):
        for forbidden in ("fetch(", "XMLHttpRequest", "WebSocket(", "ActiveXObject",
                          "powershell.exe", "windhawk-cli.exe", "child_process",
                          "WScript.Shell", "navigator.serviceWorker", "eval("):
            self.assertNotIn(forbidden, self.html)

    def test_full_set_of_controls_and_feedback_exposed(self):
        for control in ("iconsize", "height", "lift", "twotier", "dockbg",
                        "maxscale", "radius", "spring", "rise", "magnify",
                        "neighbors", "glow", "opacity", "blur", "corners",
                        "compact", "export", "reset"):
            self.assertIn(f'id="{control}"', self.html)

    def test_simulated_desktop_and_export_are_explicit(self):
        for expected in ("simulated", "Windows unchanged", "no Windows changes",
                         "ATB_DEMO", "localStorage", "application dock"):
            self.assertIn(expected.lower(), self.html.lower())


if __name__ == "__main__":
    unittest.main()
