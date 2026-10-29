"""MOCK-only process credential helpers.

One-use trial secrets travel on an inherited stdin pipe, never on process
command lines, and the journal stores only a SHA-256 verifier. This is not
production authenticated Windows IPC: private ACLs, OS peer verification,
replay resistance, and a native implementation are still required.
"""
from __future__ import annotations

import hashlib
import hmac
import re
import secrets
from typing import BinaryIO

TOKEN_HEX_CHARS = 64
_PATTERN = re.compile(rb"[0-9a-f]{64}\Z")


class CredentialError(ValueError):
    """Fail closed on bad or missing mock credential."""


def new_token() -> str:
    return secrets.token_hex(32)


def digest(token: str) -> str:
    if not isinstance(token, str):
        raise CredentialError("Invalid trial credential format")
    try:
        encoded = token.encode("ascii")
    except UnicodeEncodeError as exc:
        raise CredentialError("Invalid trial credential format") from exc
    if not _PATTERN.fullmatch(encoded):
        raise CredentialError("Invalid trial credential format")
    return hashlib.sha256(encoded).hexdigest()


def matches(token: str, expected_digest: str) -> bool:
    if not isinstance(expected_digest, str):
        return False
    try:
        encoded = expected_digest.encode("ascii")
    except UnicodeEncodeError:
        return False
    if not _PATTERN.fullmatch(encoded):
        return False
    try:
        return hmac.compare_digest(digest(token), expected_digest)
    except CredentialError:
        return False


def send(stream: BinaryIO, token: str) -> None:
    digest(token)
    stream.write(token.encode("ascii") + b"\n")
    stream.flush()
    stream.close()


def receive(stream: BinaryIO) -> str:
    line = stream.readline(TOKEN_HEX_CHARS + 2)
    if len(line) != TOKEN_HEX_CHARS + 1 or not line.endswith(b"\n"):
        raise CredentialError("Missing or oversized trial credential")
    value = line[:-1]
    if not _PATTERN.fullmatch(value):
        raise CredentialError("Malformed trial credential")
    return value.decode("ascii")
