"""Stage 0: check declared compatibility targets, NOT actual Windows compatibility."""
import json
import pathlib

root = pathlib.Path(__file__).resolve().parents[1]
manifest = json.loads((root / "compatibility" / "targets.json").read_text(encoding="utf-8"))
expected = {
    "taskbar-dock-animation",
    "windows-11-taskbar-styler",
    "windows-11-start-menu-styler",
    "windows-11-notification-center-styler",
    "taskbar-tray-system-icon-tweaks",
    "taskbar-icon-size",
}
targets = manifest["targets"]
ids = [t["id"] for t in targets]
assert len(ids) == len(set(ids)), "Duplicate compatibility mod IDs"
assert set(ids) == expected, "Missing required compatibility mod(s)"
assert manifest["verification"].startswith("requirements-only")
for item in targets:
    assert item["author"] and item["policy"] and item["overlap"]
    assert item["source"].startswith("https://github.com/ramensoftware/windhawk-mods/blob/")
    assert len(item["hosts"]) > 0
    assert item["role"] in {"animation_reference", "coexist", "geometry_reference_and_coexistence"}
animation = next(t for t in targets if t["id"] == "taskbar-dock-animation")
assert "two animation controllers" in animation["policy"]
print("Compatibility manifest contract passed (metadata only, no Windows tested).")
