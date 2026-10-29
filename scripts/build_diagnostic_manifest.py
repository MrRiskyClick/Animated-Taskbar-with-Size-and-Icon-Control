"""Describe an unsigned Windows test build without implying release approval.

Produces only untrusted diagnostic evidence; never signs, installs or publishes.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re


def diagnostic_manifest(executable: Path, source_sha: str) -> dict:
    if not re.fullmatch(r"[0-9a-f]{40}", source_sha):
        raise ValueError("Expected exact 40-character lowercase source commit SHA")
    if executable.name != "binary_identity_probe_win.exe" or not executable.is_file():
        raise ValueError("Only the isolated identity diagnostic may be packaged")
    with executable.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    return {
        "schema": 1,
        "artifact_kind": "test-only-windows-identity-diagnostic",
        "artifact_name": executable.name,
        "source_commit": source_sha,
        "sha256": digest,
        "trusted": False,
        "signed": False,
        "deployable": False,
        "release_approved": False,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate UNTRUSTED diagnostic manifest")
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    manifest = diagnostic_manifest(args.candidate, args.source_commit)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"UNTRUSTED diagnostic evidence saved to {args.output}")


if __name__ == "__main__":
    main()
