"""Bundle the original C++ lease parser into a Windhawk single-file mod.

No network access, no Windows system changes, no automatic mod installation.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src" / "animated-taskbar.wh.cpp"
HEADER = ROOT / "src" / "core" / "lease_v1.hpp"
TARGET = ROOT / "dist" / "animated-taskbar.wh.cpp"
MARKER = '#include "core/lease_v1.hpp" // Inlined by scripts/bundle_windhawk.py'


def bundle() -> str:
    main = SOURCE.read_text(encoding="utf-8")
    header = HEADER.read_text(encoding="utf-8")
    if main.count(MARKER) != 1:
        raise RuntimeError("Expected one lease header injection point")
    if not header.startswith("// Stage-1b original code:"):
        raise RuntimeError("Unexpected header content")
    return main.replace(MARKER, "// BEGIN EMBEDDED LEASE PARSER\n" +
                        header.replace("#pragma once\n", "") +
                        "\n// END EMBEDDED LEASE PARSER")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true",
                        help="Validate source/header and produce no output")
    args = parser.parse_args()
    result = bundle()
    if result.count("// ==WindhawkMod==") != 1 or result.count("// ==/WindhawkMod==") != 1:
        raise RuntimeError("Missing balanced Windhawk mod metadata block")
    if MARKER in result:
        raise RuntimeError("Unreplaced local header include")
    if args.check:
        print("Windhawk single-file bundling contract passed; no output.")
    else:
        TARGET.parent.mkdir(parents=True, exist_ok=True)
        TARGET.write_text(result, encoding="utf-8")
        print(f"Generated {TARGET.relative_to(ROOT)} (diagnostic only).")


if __name__ == "__main__":
    main()
