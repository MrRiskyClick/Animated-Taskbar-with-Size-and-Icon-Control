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
        for control in ("iconsize", "height", "lift", "iconoffset", "icongap",
                        "pipoffset", "twotier", "dockbg",
                        "maxscale", "radius", "spring", "rise", "magnify",
                        "neighbors", "glow", "opacity", "blur", "corners",
                        "compact", "export", "reset"):
            self.assertIn(f'id="{control}"', self.html)

    def test_overlapping_default_and_independent_bar_height(self):
        # Layout contract: icon scaling never determines system bar height.
        self.assertIn("twotier:false", self.html)
        self.assertIn("#stage.unified #system-bar", self.html)
        self.assertIn("height:var(--bar-height);min-height:0", self.html)
        self.assertNotIn("height:calc(var(--icon-size) + 22px)", self.html)
        self.assertIn("bottom:calc(7px + var(--icon-offset))", self.html)

    def test_new_icon_spacing_and_pip_controls_bind_to_css(self):
        self.assertIn("gap:var(--icon-gap)", self.html)
        self.assertIn("bottom:calc(-7px + var(--pip-offset))", self.html)
        self.assertIn("bottom:calc(-6px + var(--pip-offset))", self.html)
        for key in ("--icon-gap", "--pip-offset", "--icon-offset"):
            self.assertIn("root.style.setProperty('" + key + "'", self.html)

    def test_icon_push_tracks_current_animated_scale(self):
        # All icon translations must be solved from the actual rendered
        # scale on the same frame. Independent position springs would lag.
        for expression in (
            "const extraWidths=items.map(a=>(a.current-1)*state.iconsize)",
            "const totalExtra=extraWidths.reduce",
            "const shift=precedingExtra+extraWidths[i]/2-totalExtra/2",
            "a.el.style.transform=`translate3d(",
            "const separatorShift=extraWidths.slice(0,boundary).reduce",
            "separator.style.transform=`translate3d(",
        ):
            self.assertIn(expression, self.html)
        # Spacing must not be discounted by the Windhawk default 50%
        # factor, which can permit overlap at extreme magnification.
        self.assertNotIn("const spacingFactor=0.5", self.html)

    def test_simulated_desktop_and_export_are_explicit(self):
        for expected in ("simulated", "Windows unchanged", "No system changes",
                         "ATB_DEMO", "localStorage", "application dock"):
            self.assertIn(expected.lower(), self.html.lower())


if __name__ == "__main__":
    unittest.main()
