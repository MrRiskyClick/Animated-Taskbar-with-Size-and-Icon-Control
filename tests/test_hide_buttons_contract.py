"""Offline-only guard for ATB Hide Taskbar Buttons.

This check cannot prove behavior on a user's Windows build. It prevents
accidental introduction of native taskbar HWND geometry and XAML diagnostics
consumer changes, which previously caused a real Explorer taskbar incident.
"""
import pathlib
import re

ROOT=pathlib.Path(__file__).resolve().parents[1]
source=(ROOT/"experiments/atb-hide-taskbar-buttons.wh.cpp").read_text(encoding="utf-8")

required=[
    "// @id              atb-hide-taskbar-buttons",
    "// @include         explorer.exe",
    "L\"Taskbar.TaskListButton\"",
    "L\"Taskbar.TaskListLabeledButton\"",
    'L"StartButton"',
    'L"TaskViewButton"',
    "element.Visibility(Visibility::Collapsed);",
    "RestoreButtonsOnThisThread();",
    "element.Visibility(original.visibility);",
    "element.Width(original.width);",
    "element.Height(original.height);",
    "winrt::make_weak(element)",
    "GetTaskbarXamlRoot",
    "TaskbarCollapsibleLayoutXamlTraits_ArrangeOverride_Hook",
]
for token in required:
    assert token in source, f"Missing hide-buttons contract: {token}"
assert source.index("g_unloading = true;")<source.index("void Wh_ModUninit()")
assert source.count("bool Wh_ModInit()")==0
assert source.count("BOOL Wh_ModInit()")==1
assert source.count("void Wh_ModBeforeUninit()")==1
assert "xamlom.h" not in source
for forbidden in (
    "SetWindowPos(", "SetWindowRgn(", "GetWindowRgn(",
    "SHAppBarMessage(", "ABM_SETPOS", "WM_WORKAREA",
    "SystemParametersInfo(", "SetWindowLongPtr(",
    "DwmEnableBlurBehindWindow(", "RegisterDragDrop(",
    "RegSetValue", "SetValueEx", "CreateWindowEx(",
):
    assert forbidden not in source, f"Prohibited Explorer mutation in hide-only mod: {forbidden}"
# Every mod toggles independently: never reuse the native trial ID or the
# Taskbar Styler ID. Also do not load a second XAML diagnostics consumer.
assert "atb-animated-taskbar-trial" not in source
assert "windows-11-taskbar-styler" not in re.findall(r"// @id\s+([^\s]+)",source)[0]
assert "VisualTreeWatcher" not in source
assert "IVisualTreeService" not in source
print("PASS: distinct hide-only Windhawk mod, native XAML hooks, nontray targets")
print("PASS: original visibility/geometry recorded and restored on owning XAML UI thread")
print("PASS: no taskbar HWND resize, region, appbar reservation or competing XAML diagnostics")
