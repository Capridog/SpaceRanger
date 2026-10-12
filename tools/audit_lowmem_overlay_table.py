"""Enumerate the Toy Story 2 (US rev0) low-memory overlay table and classify
resident J/JAL targets that land below the resident VRAM base.

Motivation
----------
Pass 9 blocked on `No function found for jal target 0x80000AC4 in
func_80021D94`. 0x80000AC4 is NOT a function entry in either currently
mapped overlay (lowmem0 = loader id 16, lowmem98 = loader id 98). Static
analysis of the game's own loader table shows the ROM contains NINETEEN
low-memory overlays, every one of them loaded to VRAM 0x80000400.

Static ROM analysis only. Reads the ROM and config; writes a report.
Does not modify recompilation symbols and does not execute ROM code.

Self-contained: implements the small subset of R4300i decoding that
control-flow classification needs, so it does not depend on rabbitizer
or splat and can be run anywhere the ROM is present.
"""
from pathlib import Path
import hashlib
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
ROM_PATH = ROOT / "rom/baserom.us.rev0.z64"
SYMS_PATH = ROOT / "config/toystory2.us.rev0.syms.toml"
REPORT = ROOT / "docs/lowmem-overlay-table-audit.txt"
EXPECTED_SHA = "b8b0db1259ca80b4397c3c58ef63d38100a69d436ed3f097079eb81d07e764c9"

# Resident section, as configured in config/toystory2.us.rev0.syms.toml.
RES_ROM, RES_VRAM, RES_SIZE = 0x1840, 0x80006840, 0xA1D5C
# Loader dispatch table, proven in docs/lowmem98-source-audit.txt.
LOADER_TABLE_VRAM, LOADER_IDS = 0x800A0920, 99
# Every low-memory overlay is DMA'd to this VRAM address.
OVERLAY_VRAM = 0x80000400
# Common overlay transfer helper (func_800859A4).
TRANSFER = 0x800859A4


# --------------------------------------------------------------------------
# Minimal R4300i decoder: only what CFG classification needs.
# --------------------------------------------------------------------------
REGS = ["zero","at","v0","v1","a0","a1","a2","a3","t0","t1","t2","t3","t4","t5","t6","t7",
        "s0","s1","s2","s3","s4","s5","s6","s7","t8","t9","k0","k1","gp","sp","fp","ra"]


class Ins:
    __slots__ = ("pc","w","op","rs","rt","rd","sa","funct","imm","simm","target","kind","mnem")

    def __init__(self, w, pc):
        self.pc, self.w = pc, w
        self.op, self.funct = w >> 26, w & 0x3F
        self.rs, self.rt, self.rd = (w >> 21) & 0x1F, (w >> 16) & 0x1F, (w >> 11) & 0x1F
        self.sa, self.imm = (w >> 6) & 0x1F, w & 0xFFFF
        self.simm = self.imm - 0x10000 if self.imm & 0x8000 else self.imm
        self.target, self.kind, self.mnem = None, "other", None
        if w == 0:
            self.kind, self.mnem = "nop", "nop"
        elif self.op == 0:
            self.mnem = {8: "jr", 9: "jalr"}.get(self.funct, f"special.{self.funct}")
            if self.funct in (8, 9):
                self.kind = self.mnem
            elif self.funct in (32, 33):
                self.mnem = "addu"
            elif self.funct == 37:
                self.mnem = "or"
        elif self.op in (2, 3):
            self.mnem = self.kind = "j" if self.op == 2 else "jal"
            self.target = (((pc + 4) & 0xF0000000) | ((w & 0x03FFFFFF) << 2)) & 0xFFFFFFFF
        elif self.op == 16 and self.rs == 16 and self.funct == 24:
            self.kind = self.mnem = "eret"
        elif self.op in (1, 4, 5, 6, 7, 20, 21, 22, 23):
            self.kind = "branch"
            self.mnem = {1: "regimm", 4: "beq", 5: "bne", 6: "blez", 7: "bgtz",
                         20: "beql", 21: "bnel", 22: "blezl", 23: "bgtzl"}[self.op]
        else:
            self.mnem = {8: "addi", 9: "addiu", 13: "ori", 15: "lui", 35: "lw",
                         43: "sw"}.get(self.op, f"op.{self.op}")

    def text(self):
        r = lambda i: "$" + REGS[i]
        if self.kind == "nop": return "nop"
        if self.kind in ("j", "jal"): return f"{self.mnem} 0x{self.target:08X}"
        if self.kind == "jr": return f"jr {r(self.rs)}"
        if self.kind == "jalr": return f"jalr {r(self.rd)}, {r(self.rs)}"
        if self.kind == "eret": return "eret"
        if self.mnem == "lui": return f"lui {r(self.rt)}, 0x{self.imm:04X}"
        if self.mnem in ("addiu", "addi"): return f"{self.mnem} {r(self.rt)}, {r(self.rs)}, {self.simm}"
        if self.mnem == "ori": return f"ori {r(self.rt)}, {r(self.rs)}, 0x{self.imm:04X}"
        if self.mnem in ("lw", "sw"): return f"{self.mnem} {r(self.rt)}, {self.simm}({r(self.rs)})"
        if self.mnem in ("addu", "or"): return f"{self.mnem} {r(self.rd)}, {r(self.rs)}, {r(self.rt)}"
        return f"{self.mnem} 0x{self.w:08X}"


def is_flow_end(ins):
    """True when `ins` terminates a basic block, so ins.pc + 8 starts a new one."""
    return ins.kind in ("jr", "j", "eret")


# --------------------------------------------------------------------------
class Rom:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        digest = hashlib.sha256(self.data).hexdigest()
        if digest != EXPECTED_SHA:
            raise RuntimeError("Unexpected ROM SHA256; refusing to use fixed offsets")
        self.sha = digest

    def word(self, off):
        return int.from_bytes(self.data[off:off + 4], "big")

    def res(self, vram):
        if not (RES_VRAM <= vram < RES_VRAM + RES_SIZE):
            return None
        return self.word(RES_ROM + vram - RES_VRAM)

    def res_ins(self, vram):
        w = self.res(vram)
        return None if w is None else Ins(w, vram)


def trace_loader_handler(rom, handler):
    """Recover (cart_source, vram_dest, size) from one loader handler.

    Each handler builds a0 = 0xB00xxxxx cart address, a1 = VRAM destination,
    a2 = byte count, then calls the common transfer helper. a1's low half is
    set by an `ori` that sits in the transfer call's DELAY SLOT, so the delay
    slot must be executed before the arguments are snapshotted.
    """
    reg, pc, seen = {}, handler, set()
    for _ in range(96):
        if pc in seen:
            return None
        seen.add(pc)
        ins = rom.res_ins(pc)
        if ins is None:
            return None

        def apply(i):
            if i is None:
                return
            if i.mnem == "lui":
                reg[i.rt] = (i.imm << 16) & 0xFFFFFFFF
            elif i.mnem in ("addiu", "addi"):
                reg[i.rt] = ((reg.get(i.rs, 0) if i.rs else 0) + i.simm) & 0xFFFFFFFF
            elif i.mnem == "ori":
                reg[i.rt] = ((reg.get(i.rs, 0) if i.rs else 0) | i.imm) & 0xFFFFFFFF
            elif i.mnem in ("addu", "or") and i.rt == 0 and i.rs in reg:
                reg[i.rd] = reg[i.rs]

        apply(ins)
        if ins.kind in ("jal", "j"):
            apply(rom.res_ins(pc + 4))          # delay slot executes first
            src, dst, size = reg.get(4), reg.get(5), reg.get(6)
            if (ins.kind == "jal" and src is not None and dst is not None
                    and size is not None and (src >> 28) == 0xB and (dst >> 24) == 0x80):
                return src, dst, size, ins.target
            if ins.kind == "j":
                pc = ins.target
                continue
            pc += 8
            continue
        if ins.kind == "jr":
            return None
        pc += 4
    return None


def main():
    rom = Rom(ROM_PATH)
    out = ["SpaceRanger low-memory overlay table audit",
           f"ROM SHA256: {rom.sha}",
           "Static ROM analysis only. No symbols or configuration were modified.",
           ""]

    # ---- 1. enumerate the loader table -----------------------------------
    overlays = {}
    default_handlers = {}
    for i in range(LOADER_IDS):
        handler = rom.res(RES_VRAM + 0 if False else 0)  # placeholder, replaced below
    table_off = RES_ROM + LOADER_TABLE_VRAM - RES_VRAM
    for i in range(LOADER_IDS):
        handler = rom.word(table_off + i * 4)
        traced = trace_loader_handler(rom, handler)
        if traced is None:
            default_handlers.setdefault(handler, []).append(i)
            continue
        src, dst, size, callee = traced
        if dst >= RES_VRAM:
            continue
        overlays[i] = (src & 0x0FFFFFFF, dst, size, handler, callee)

    out.append(f"Loader dispatch table: VRAM 0x{LOADER_TABLE_VRAM:08X} "
               f"(resident ROM 0x{table_off:X}), {LOADER_IDS} entries.")
    out.append(f"Low-memory overlays recovered: {len(overlays)}")
    out.append("Handlers with no overlay transfer (shared default tails):")
    for h, ids in sorted(default_handlers.items()):
        out.append(f"  handler 0x{h:08X}: ids {ids[0]}..{ids[-1]} ({len(ids)} entries)")
    out.append("")
    out.append(" id   handler     cart source   ROM range              VRAM range             size")
    prev_end = None
    contiguous = True
    for i, (r, dst, size, handler, callee) in sorted(overlays.items()):
        gap = "" if prev_end is None or prev_end == r else f"  <-- GAP 0x{r - prev_end:X}"
        if prev_end is not None and prev_end != r:
            contiguous = False
        out.append(f"{i:3d}   0x{handler:08X}  0xB0{r:06X}    "
                   f"[0x{r:06X},0x{r + size:06X})   "
                   f"[0x{dst:08X},0x{dst + size:08X})  0x{size:X}{gap}")
        prev_end = r + size
    first = min(v[0] for v in overlays.values())
    out += ["", f"Overlay block: ROM [0x{first:06X}, 0x{prev_end:06X}), "
                f"total 0x{prev_end - first:X} bytes, contiguous={contiguous}.",
            f"Every overlay is loaded to VRAM 0x{OVERLAY_VRAM:08X}; "
            "all nineteen share the same address window.",
            f"Transfer helper: func_{TRANSFER:08X}.", ""]

    # ---- 2. entry classification helpers ---------------------------------
    def ov_word(oid, vram):
        r, dst, size, _h, _c = overlays[oid]
        if not (dst <= vram < dst + size):
            return None
        return rom.word(r + vram - dst)

    def is_entry(oid, vram):
        """A clean entry: either the overlay base, or preceded by a
        (flow-end, delay-slot) pair so the address starts a basic block."""
        if ov_word(oid, vram) is None:
            return False
        if vram == overlays[oid][1]:
            return True
        w = ov_word(oid, vram - 8)
        return w is not None and is_flow_end(Ins(w, vram - 8))

    # ---- 3. the Pass 9 blocker -------------------------------------------
    BLOCKER = 0x80000AC4
    out += ["-" * 72,
            f"Pass 9 blocker: jal target 0x{BLOCKER:08X} in func_80021D94", "-" * 72]
    owners = [oid for oid in overlays if is_entry(oid, BLOCKER)]
    for oid in (16, 98):
        w = ov_word(oid, BLOCKER)
        prev = Ins(ov_word(oid, BLOCKER - 4), BLOCKER - 4)
        out.append(f"  overlay {oid:3d} (current '{'lowmem0' if oid == 16 else 'lowmem98'}'): "
                   f"0x{BLOCKER:08X} = {Ins(w, BLOCKER).text()}   "
                   f"[-4] {prev.text()}  -> INTERIOR, not an entry")
    out.append(f"  overlays where 0x{BLOCKER:08X} is a clean entry: {owners}")
    out.append("")

    # ---- 4. func_80021D94 is a per-level dispatcher -----------------------
    DISPATCH = 0x80021D94
    JT = 0x800A10A0
    cases = []
    for n in range(17):
        body = rom.word(RES_ROM + JT - RES_VRAM + n * 4)
        ins = rom.res_ins(body)
        cases.append((n + 1, body, ins.target))
    out += ["-" * 72,
            f"func_{DISPATCH:08X}: 17-case dispatcher over jump table 0x{JT:08X}",
            "Hypothesis under test: case N calls into low-memory overlay N.",
            "-" * 72]
    hits = 0
    for n, body, tgt in cases:
        ok = n in overlays and is_entry(n, tgt)
        hits += ok
        detail = "overlay base" if n in overlays and tgt == overlays[n][1] else ""
        if not detail and n in overlays and ok:
            detail = Ins(ov_word(n, tgt), tgt).text()
        out.append(f"  case {n:2d}  body 0x{body:08X}  jal 0x{tgt:08X}  "
                   f"overlay {n:2d}  {'ENTRY' if ok else 'not an entry'}  {detail}")
    out += ["", f"  RESULT: {hits}/17 dispatcher targets are clean entries in overlay N.", ""]

    # ---- 5. every resident reference into low memory ----------------------
    known = set(int(m, 16) for m in re.findall(r"vram = (0x[0-9A-Fa-f]+)", SYMS_PATH.read_text()))
    refs = {}
    for pc in range(RES_VRAM, RES_VRAM + RES_SIZE, 4):
        ins = rom.res_ins(pc)
        if ins.kind in ("jal", "j") and ins.target is not None and ins.target < RES_VRAM:
            refs.setdefault(ins.target, []).append(pc)
    unresolved = [t for t in sorted(refs) if t not in known]
    out += ["-" * 72,
            "Resident J/JAL targets below the resident VRAM base", "-" * 72,
            f"  distinct targets: {len(refs)}",
            f"  already present as a symbol vram in syms.toml: {len(refs) - len(unresolved)}",
            f"  not yet symbols (remaining blockers of this class): {len(unresolved)}",
            "",
            "  target      refs  owning overlay(s) where it is a clean entry"]
    for t in sorted(refs):
        owners = [oid for oid in overlays if is_entry(oid, t)]
        mark = "  " if t in known else " *"
        note = ""
        if not owners:
            note = "  (below 0x80000400 - outside every overlay)" if t < OVERLAY_VRAM else "  (NO OWNER - needs individual analysis)"
        out.append(f" {mark} 0x{t:08X} {len(refs[t]):5d}  {owners if owners else 'NONE'}{note}")
    out += ["", "  '*' marks targets with no symbol yet.", ""]

    # ---- 6. overlay 1 function map ---------------------------------------
    out += ["-" * 72, "Overlay 1 (loader id 1) function map", "-" * 72]
    r1, d1, s1, _h, _c = overlays[1]
    lo, hi = d1, d1 + s1

    def w1(v):
        return rom.word(r1 + v - d1)

    starts = {lo}
    starts |= {t for t in refs if lo <= t < hi and is_entry(1, t)}
    for v in range(lo, hi, 4):
        ins = Ins(w1(v), v)
        if ins.kind == "jal" and ins.target is not None and lo <= ins.target < hi:
            starts.add(ins.target)
    # code/data split: the final `jr $ra` in the overlay ends the code region
    rets = [v for v in range(lo, hi, 4) if Ins(w1(v), v).kind == "jr" and Ins(w1(v), v).rs == 31]
    code_end = rets[-1] + 8
    while code_end < hi and w1(code_end) == 0:
        code_end += 4
    ascii_probe = b"".join(w1(v).to_bytes(4, "big") for v in range(code_end, min(code_end + 16, hi), 4))
    out += [f"  ROM [0x{r1:06X},0x{r1 + s1:06X}) -> VRAM [0x{lo:08X},0x{hi:08X}) size 0x{s1:X}",
            f"  code region:  [0x{lo:08X}, 0x{code_end:08X})  size 0x{code_end - lo:X}",
            f"  data region:  [0x{code_end:08X}, 0x{hi:08X})  size 0x{hi - code_end:X}",
            f"  first data bytes: {ascii_probe!r}  <- identifies this overlay as level 01",
            ""]
    order = sorted(starts)
    failures, ranges = [], []
    out.append("  function ranges (closure-checked; extent trimmed to the real "
               "flow-end + delay slot, trailing zero padding excluded):")
    for n, s in enumerate(order):
        limit = order[n + 1] if n + 1 < len(order) else code_end
        # The function is closed iff some flow-end instruction v in [s, limit)
        # has its delay slot at v+4 and only zero padding between v+8 and limit.
        true_end, closer = None, None
        v = limit - 8
        while v >= s:
            if is_flow_end(Ins(w1(v), v)) and all(w1(q) == 0 for q in range(v + 8, limit, 4)):
                true_end, closer = v + 8, Ins(w1(v), v)
                break
            v -= 4
        ok = true_end is not None
        if not ok:
            failures.append(s)
            true_end, closer = limit, Ins(w1(limit - 4), limit - 4)
        pad = limit - true_end
        ranges.append((s, true_end))
        called = "resident-called" if s in refs else "overlay-internal"
        out.append(f"    {'OK ' if ok else 'BAD'}  lowmem01_func_{s:08X}  "
                   f"[0x{s:08X},0x{true_end:08X})  size 0x{true_end - s:<6X}  {called:16s} "
                   f"closes with: {closer.text()}" + (f"  (+0x{pad:X} padding)" if pad else ""))
    out.append(f"  closure failures: {len(failures)}")
    out.append("")
    out.append("  Proposed syms.toml section (not applied by this tool):")
    out.append("")
    out.append('  [[section]]')
    out.append('  name = "lowmem01"')
    out.append(f'  rom = 0x{r1:X}')
    out.append(f'  vram = 0x{lo:08X}')
    out.append(f'  size = 0x{s1:X}')
    out.append('')
    out.append('  functions = [')
    for s, e in ranges:
        out.append(f'    {{ name = "lowmem01_func_{s:08X}", vram = 0x{s:08X}, size = 0x{e - s:X} }},')
    out.append('  ]')

    outbound = set()
    for v in range(lo, code_end, 4):
        ins = Ins(w1(v), v)
        if ins.kind in ("jal", "j") and ins.target is not None and not (lo <= ins.target < hi):
            outbound.add(ins.target)
    cross = sorted(t for t in outbound if t < RES_VRAM)
    out += [f"  outbound J/JAL from overlay 1 code: {len(outbound)} distinct targets",
            f"  of which below the resident base (cross-overlay): {len(cross)} {[hex(t) for t in cross]}",
            ""]

    out += ["-" * 72,
            f"HARD FAILURE COUNT: {len(failures)}",
            "-" * 72]
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text("\n".join(out) + "\n", encoding="utf-8")
    print("\n".join(out))
    print(f"\nReport written to {REPORT}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
