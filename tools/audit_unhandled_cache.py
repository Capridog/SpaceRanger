#!/usr/bin/env python3
"""
audit_unhandled_cache.py -- find every function that will be recompiled and
still contains a CACHE instruction.

N64Recomp has no emission for the R4300 `cache` opcode (op 0x2F); it aborts the
whole run with "Unhandled instruction: cache". The libultra cache routines
never trip this only because they are named, which makes N64Recomp suppress
their bodies. Any function carrying a `cache` that is NOT named/ignored/stubbed
is therefore a hard blocker.

Scans every section in syms.toml with that section's own rom/vram base, so
overlay functions are decoded correctly too.
"""
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent
ROM = ROOT / "rom" / "baserom.us.rev0.z64"
SYMS = ROOT / "config" / "toystory2.us.rev0.syms.toml"
RECOMP = ROOT / "config" / "toystory2.us.rev0.recomp.toml"

CACHE_OPS = {
    0x00: "Index_Invalidate_I", 0x01: "Index_Writeback_Invalidate_D",
    0x04: "Index_Load_Tag_I", 0x05: "Index_Load_Tag_D",
    0x08: "Index_Store_Tag_I", 0x09: "Index_Store_Tag_D",
    0x10: "Hit_Invalidate_I", 0x11: "Hit_Invalidate_D",
    0x15: "Hit_Writeback_Invalidate_D", 0x18: "Hit_Writeback_I",
    0x19: "Hit_Writeback_D",
}

SEC_RE = re.compile(
    r'\[\[section\]\]\s*name\s*=\s*"([^"]+)"\s*rom\s*=\s*(0x[0-9A-Fa-f]+)\s*'
    r'vram\s*=\s*(0x[0-9A-Fa-f]+)\s*size\s*=\s*(0x[0-9A-Fa-f]+)',
    re.S,
)
FUNC_RE = re.compile(
    r'\{\s*name\s*=\s*"([^"]+)"\s*,\s*vram\s*=\s*(0x[0-9A-Fa-f]+)\s*,'
    r'\s*size\s*=\s*(0x[0-9A-Fa-f]+)'
)


def suppressed_names():
    """Names N64Recomp will suppress: its built-in lists, plus config stubs/ignored."""
    sl = (ROOT / "deps" / "N64ModernRuntime" / "N64Recomp" / "src"
          / "symbol_lists.cpp")
    builtin = set(re.findall(r'"([^"]+)"', sl.read_text(
        encoding="utf-8", errors="replace")))
    cfg = RECOMP.read_text(encoding="utf-8", errors="replace")
    cfg_names = set()
    for key in ("stubs", "ignored"):
        m = re.search(key + r"\s*=\s*\[(.*?)\]", cfg, re.S)
        if m:
            cfg_names |= set(re.findall(r'"([^"]+)"', m.group(1)))
    return builtin, cfg_names


def main():
    rom = ROM.read_bytes()
    text = SYMS.read_text(encoding="utf-8", errors="replace")
    builtin, cfg_names = suppressed_names()

    sections = []
    for m in SEC_RE.finditer(text):
        sections.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16),
                         m.end()))
    sections.sort(key=lambda s: s[3])

    # Each section owns the function entries between its header and the next.
    bounds = [(s[0], s[1], s[2], s[3],
               sections[i + 1][3] if i + 1 < len(sections) else len(text))
              for i, s in enumerate(sections)]

    blockers, suppressed_hits = [], []
    for sec, srom, svram, start, end in bounds:
        for fm in FUNC_RE.finditer(text, start, end):
            name, vram, size = fm.group(1), int(fm.group(2), 16), int(
                fm.group(3), 16)
            base = vram - svram + srom
            hits = []
            for off in range(0, size, 4):
                chunk = rom[base + off:base + off + 4]
                if len(chunk) < 4:
                    break
                w = int.from_bytes(chunk, "big")
                if (w >> 26) == 0x2F:
                    op = (w >> 16) & 0x1F
                    hits.append((vram + off, op))
            if not hits:
                continue
            is_supp = name in builtin or name in cfg_names
            rec = (sec, name, vram, size, hits)
            (suppressed_hits if is_supp else blockers).append(rec)

    def show(recs, title):
        print(f"=== {title}: {len(recs)} ===")
        for sec, name, vram, size, hits in recs:
            ops = ", ".join(
                f"0x{vv:08X} op=0x{o:02X} {CACHE_OPS.get(o, '?')}"
                for vv, o in hits[:3])
            more = f" (+{len(hits) - 3} more)" if len(hits) > 3 else ""
            print(f"  [{sec}] {name} @0x{vram:08X} size=0x{size:X}")
            print(f"        {ops}{more}")
        print()

    show(blockers, "WILL BE RECOMPILED and contains cache -- BLOCKERS")
    show(suppressed_hits, "contains cache but is suppressed (fine)")
    raise SystemExit(1 if blockers else 0)


if __name__ == "__main__":
    main()
