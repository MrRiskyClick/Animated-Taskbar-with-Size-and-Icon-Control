"""Bundle original read-only C++ diagnostics into one Windhawk source file.

No network, no Windhawk installation, no Windows taskbar modifications.
"""
from __future__ import annotations

import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src" / "animated-taskbar.wh.cpp"
HEADER = ROOT / "src" / "core" / "lease_v1.hpp"
IPC_HEADER = ROOT / "src" / "core" / "diagnostic_ipc_win.hpp"
TRANSPORT_HEADER = ROOT / "native" / "ipc_timeout_win.hpp"
TARGET = ROOT / "dist" / "animated-taskbar.wh.cpp"
MARKER = '#include "core/lease_v1.hpp" // Inlined by scripts/bundle_windhawk.py'
IPC_MARKER = '#include "core/diagnostic_ipc_win.hpp" // Inlined by scripts/bundle_windhawk.py'
TRANSPORT_MARKER = '#include "../../native/ipc_timeout_win.hpp"'


def strip_pragma(header: str) -> str:
    return header.replace("#pragma once\n", "")


def bundle() -> str:
    main = SOURCE.read_text(encoding="utf-8")
    lease = HEADER.read_text(encoding="utf-8")
    ipc = IPC_HEADER.read_text(encoding="utf-8")
    transport = TRANSPORT_HEADER.read_text(encoding="utf-8")
    if main.count(MARKER) != 1 or main.count(IPC_MARKER) != 1:
        raise RuntimeError("Expected one of each Windhawk diagnostic injection marker")
    if ipc.count(TRANSPORT_MARKER) != 1:
        raise RuntimeError("Expected exactly one nested IPC transport header")
    if not lease.startswith("// Stage-1b original code:"):
        raise RuntimeError("Unexpected lease header")
    if not ipc.startswith("// Stage 1g:"):
        raise RuntimeError("Unexpected read-only IPC header")
    if not transport.startswith("// Stage 1f:"):
        raise RuntimeError("Unexpected overlapped transport header")
    embedded_ipc = strip_pragma(ipc).replace(
        TRANSPORT_MARKER,
        "// BEGIN EMBEDDED OVERLAPPED IPC TRANSPORT\n"
        + strip_pragma(transport)
        + "\n// END EMBEDDED OVERLAPPED IPC TRANSPORT",
    )
    result = main.replace(
        MARKER,
        "// BEGIN EMBEDDED LEASE PARSER\n" + strip_pragma(lease)
        + "\n// END EMBEDDED LEASE PARSER",
    ).replace(
        IPC_MARKER,
        "// BEGIN EMBEDDED READ-ONLY IPC CLIENT\n" + embedded_ipc
        + "\n// END EMBEDDED READ-ONLY IPC CLIENT",
    )
    if result.count("// ==WindhawkMod==") != 1 or result.count("// ==/WindhawkMod==") != 1:
        raise RuntimeError("Windhawk mod header is missing or duplicated")
    if MARKER in result or IPC_MARKER in result or TRANSPORT_MARKER in result:
        raise RuntimeError("Local-only include escaped the single-file bundle")
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="Validate bundle only")
    args = parser.parse_args()
    result = bundle()
    if args.check:
        print("Read-only Windhawk single-file bundle contract passed; no output.")
    else:
        TARGET.parent.mkdir(parents=True, exist_ok=True)
        TARGET.write_text(result, encoding="utf-8")
        print(f"Generated {TARGET.relative_to(ROOT)} (read-only diagnostic only).")


if __name__ == "__main__":
    main()
