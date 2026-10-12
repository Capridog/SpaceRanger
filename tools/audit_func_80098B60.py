"""Audit the resident boundary of func_80098B60.

Splat assigned func_80098B60 a size of 0x20, i.e. [0x80098B60, 0x80098B80).
The first three words are a complete handwritten routine (mtc0 $a0, CP0
Compare; jr $ra; nop) -- libultra osSetCompare. The remaining 0x14 bytes
contain words that decode as instructions, one of which is `j 0x800008D8`.
0x800008D8 is not a basic-block entry in ANY of the nineteen low-memory
overlays, and N64Recomp will refuse the jump target.

This tool tests whether those trailing bytes are reachable code at all, by
searching every DECLARED function body (resident and overlays) for a
reference to them: direct J/JAL, conditional branch, or lui+addiu/ori
pointer construction. Scanning only declared bodies avoids counting data
words that merely decode as branches.

Static ROM analysis only. Writes a report; changes no symbols.
"""
from pathlib import Path
import hashlib
import sys
import tomllib

ROOT = Path(__file__).resolve().parents[1]
ROM_PATH = ROOT / "rom/baserom.us.rev0.z64"
SYMS = ROOT / "config/toystory2.us.rev0.syms.toml"
REPORT = ROOT / "docs/func_80098B60-boundary-audit.txt"
EXPECTED_SHA = "b8b0db1259ca80b4397c3c58ef63d38100a69d436ed3f097079eb81d07e764c9"

FUNC = 0x80098B60
DISPUTED = range(0x80098B6C, 0x80098B80, 4)
NEXT_FUNC = 0x80098B80
JUMP_OUT = 0x800008D8


def main():
    rom = ROM_PATH.read_bytes()
    digest = hashlib.sha256(rom).hexdigest()
    if digest != EXPECTED_SHA:
        raise RuntimeError("Unexpected ROM SHA256; refusing to use fixed offsets")
    cfg = tomllib.loads(SYMS.read_text())

    def words(sec):
        for f in sec["functions"]:
            for v in range(f["vram"], f["vram"] + f["size"], 4):
                o = sec["rom"] + v - sec["vram"]
                yield v, int.from_bytes(rom[o:o + 4], "big")

    watch = {FUNC, NEXT_FUNC, *DISPUTED}
    refs = {w: [] for w in watch}
    for sec in cfg["section"]:
        table = dict(words(sec))
        for v, w in table.items():
            op = w >> 26
            if op in (2, 3):
                t = (((v + 4) & 0xF0000000) | ((w & 0x03FFFFFF) << 2)) & 0xFFFFFFFF
                if t in refs:
                    refs[t].append((sec["name"], v, "j" if op == 2 else "jal"))
            elif op in (1, 4, 5, 6, 7, 20, 21, 22, 23):
                imm = w & 0xFFFF
                t = (v + 4 + ((imm - 0x10000 if imm & 0x8000 else imm) << 2)) & 0xFFFFFFFF
                if t in refs:
                    refs[t].append((sec["name"], v, "branch"))
            elif op == 15:                                     # lui
                rt, hi = (w >> 16) & 0x1F, (w & 0xFFFF) << 16
                for d in (4, 8, 12, 16):
                    n = table.get(v + d)
                    if n is None:
                        continue
                    nop_, nrs = n >> 26, (n >> 21) & 0x1F
                    if nrs != rt:
                        continue
                    if nop_ == 9:                              # addiu
                        lo = n & 0xFFFF
                        t = (hi + (lo - 0x10000 if lo & 0x8000 else lo)) & 0xFFFFFFFF
                    elif nop_ == 13:                           # ori
                        t = hi | (n & 0xFFFF)
                    else:
                        continue
                    if t in refs:
                        refs[t].append((sec["name"], v, "lui+imm"))

    declared = {f["vram"]: f["size"] for s in cfg["section"] for f in s["functions"]}
    out = ["SpaceRanger resident boundary audit: func_80098B60",
           f"ROM SHA256: {digest}",
           "Static ROM analysis only. No symbols were modified by this tool.",
           "",
           f"Declared size in config: 0x{declared.get(FUNC, 0):X}",
           f"Next declared function:  0x{NEXT_FUNC:08X}",
           "",
           "Words in the Splat extent:"]
    res = next(s for s in cfg["section"] if s["name"] == "resident")
    for v in range(FUNC, NEXT_FUNC, 4):
        o = res["rom"] + v - res["vram"]
        w = int.from_bytes(rom[o:o + 4], "big")
        note = "  <- disputed" if v in DISPUTED else ""
        out.append(f"  0x{v:08X}  {w:08X}{note}")
    out += ["",
            "0x80098B60 40845800 = mtc0 $a0, $11 (CP0 Compare); 0x80098B64 jr $ra;",
            "0x80098B68 nop delay slot. That is a complete routine of 0xC bytes",
            "(libultra osSetCompare).",
            "",
            "Inbound references found in DECLARED function bodies only:"]
    for t in sorted(refs):
        kinds = sorted({k for _s, _v, k in refs[t]})
        out.append(f"  0x{t:08X}: {len(refs[t]):3d} reference(s) {kinds}")
    disputed_refs = sum(len(refs[v]) for v in DISPUTED)
    out += ["",
            f"References to any disputed word (0x{DISPUTED[0]:08X}..0x{NEXT_FUNC - 4:08X}): "
            f"{disputed_refs}",
            f"References to 0x{FUNC:08X}: {len(refs[FUNC])}",
            f"References to 0x{NEXT_FUNC:08X}: {len(refs[NEXT_FUNC])}",
            "",
            f"The word at 0x80098B74 decodes as `j 0x{JUMP_OUT:08X}`. That address is",
            "not a basic-block entry in any of the nineteen low-memory overlays",
            "(see docs/lowmem-overlay-table-audit.txt), so it cannot be a real",
            "tail call into overlay code.",
            ""]
    if disputed_refs == 0 and len(refs[FUNC]) > 0:
        out += ["CONCLUSION: the disputed bytes are unreachable from every declared",
                "function body, while func_80098B60's first three words form a complete",
                "routine with real callers. The correct extent is:",
                "",
                f"  func_80098B60 = 0x{FUNC:08X}, size 0xC",
                "",
                "The remaining 0x14 bytes are inter-function filler and must not be",
                "recompiled. This matches the earlier func_80007C38 correction.",
                "",
                "HARD FAILURE COUNT: 0"]
        rc = 0
    else:
        out += ["CONCLUSION: UNRESOLVED. The disputed bytes have inbound references;",
                "the boundary must not be shortened on this evidence.",
                "",
                "HARD FAILURE COUNT: 1"]
        rc = 1
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text("\n".join(out) + "\n", encoding="utf-8")
    print("\n".join(out))
    print(f"\nReport written to {REPORT}")
    return rc


if __name__ == "__main__":
    sys.exit(main())
