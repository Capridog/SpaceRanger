"""Reject ROM text accidentally declared as executable function bodies."""

from __future__ import annotations

import hashlib
import sys
import tomllib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ROM = ROOT / "rom/baserom.us.rev0.z64"
SYMS = ROOT / "config/toystory2.us.rev0.syms.toml"
EXPECTED_SHA256 = "b8b0db1259ca80b4397c3c58ef63d38100a69d436ed3f097079eb81d07e764c9"


def main() -> int:
    rom = ROM.read_bytes()
    if hashlib.sha256(rom).hexdigest() != EXPECTED_SHA256:
        raise RuntimeError("Wrong ROM revision")
    sections = tomllib.loads(SYMS.read_text(encoding="utf-8"))["section"]
    suspects = []
    for section in sections:
        for func in section["functions"]:
            offset = section["rom"] + func["vram"] - section["vram"]
            data = rom[offset:offset + func["size"]]
            if len(data) < 8 or len(data) != func["size"]:
                continue
            if all(byte in (0, 10, 13) or 32 <= byte <= 126 for byte in data):
                suspects.append((section["name"], func["name"], offset, data))
    for section, name, offset, data in suspects:
        print(f"{section}/{name} ROM 0x{offset:X}: {data!r}", file=sys.stderr)
    print(f"text-only function bodies: {len(suspects)}")
    return 1 if suspects else 0


if __name__ == "__main__":
    raise SystemExit(main())
