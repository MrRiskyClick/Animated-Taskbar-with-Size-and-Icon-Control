"""Validate PNG-backed ICO resources generated from user-supplied icon art."""
from pathlib import Path
import struct
import zlib

root = Path(__file__).resolve().parents[1]
for filename in ("WindowsLogo.ico", "TaskView.ico"):
    data = (root / "assets" / filename).read_bytes()
    assert data[:6] == b"\x00\x00\x01\x00\x01\x00", filename
    size, offset = struct.unpack_from("<II", data, 14)
    assert size > 100 and offset + size == len(data), filename
    png = data[offset:]
    assert png[:8] == b"\x89PNG\r\n\x1a\n", filename
    pos = 8
    seen_iend = False
    while pos < len(png):
        length = struct.unpack_from(">I", png, pos)[0]
        typ = png[pos + 4:pos + 8]
        payload = png[pos + 8:pos + 8 + length]
        expected = struct.unpack_from(">I", png, pos + 8 + length)[0]
        assert (zlib.crc32(typ + payload) & 0xFFFFFFFF) == expected, (filename, typ)
        pos += 12 + length
        if typ == b"IEND":
            seen_iend = True
            break
    assert seen_iend and pos == len(png), filename
    print(f"PASS: {filename}: embedded PNG/icon directory and chunk CRCs valid")
