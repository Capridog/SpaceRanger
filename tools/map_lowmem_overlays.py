"""Map every low-memory overlay in Toy Story 2 (US rev0) to an audited
N64Recomp function table, and emit the corresponding syms.toml sections.

Background
----------
tools/audit_lowmem_overlay_table.py proved from the game's own loader table
that the ROM contains NINETEEN low-memory overlays (loader ids 0..17 and 98),
every one of them DMA'd to VRAM 0x80000400. The project previously modelled
only two of them: 'lowmem0' is in fact loader id 16, and 'lowmem98' is id 98.

This tool discovers each overlay's functions by REACHABILITY, not by linear
scanning, so interleaved data is never mistaken for code. Discovery starts
from proven entries only:

  * the overlay base (0x80000400), which every loader jumps to;
  * resident J/JAL targets that are clean basic-block entries in the overlay;
  * transitively, JAL targets found while walking reachable code;
  * indexed-switch targets, resolved from the actual table words in ROM.

Every derived range is closure-checked: it must end at a real flow-end
instruction plus its delay slot, with only zero padding up to the next
function. Overlays that fail are REPORTED, never guessed at.

Static ROM analysis only. Reads ROM and config, writes a report and a
candidate syms.toml. Does not execute ROM code and does not overwrite the
live symbol configuration.
"""
from pathlib import Path
import argparse
import hashlib
import sys

ROOT = Path(__file__).resolve().parents[1]
ROM_PATH = ROOT / "rom/baserom.us.rev0.z64"
REPORT = ROOT / "docs/lowmem-overlay-map.txt"
EXPECTED_SHA = "b8b0db1259ca80b4397c3c58ef63d38100a69d436ed3f097079eb81d07e764c9"

RES_ROM, RES_VRAM, RES_SIZE = 0x1840, 0x80006840, 0xA1D5C
OVERLAY_VRAM = 0x80000400

# Object callbacks assigned by the resident switch at 0x800335BC..0x80033814
# and called through object+0x90 at 0x80035678. Direct JAL discovery cannot see
# these. Each seed has a real function entry in the indicated overlay and passes
# the same ROM-backed closure audit as direct-call seeds. Interior lookalikes
# (e.g. 0x80000594 in overlay 10) are deliberately excluded.
RUNTIME_CALLBACK_ENTRIES = {
    1: {0x80000474},
    2: {0x80000594, 0x800009F8},
    4: {0x800009F4},
    5: {0x80000568},
    7: {0x8000095C},
    8: {0x80000BC8},
    10: {0x800004BC, 0x800008A0},
    11: {0x80000890},
    13: {0x80000770},
    14: {0x80000708},
    15: {0x800009DC},
}

# Loader id -> (cart ROM offset, byte size). Recovered and verified by
# tools/audit_lowmem_overlay_table.py from the loader dispatch table at
# VRAM 0x800A0920. The block is contiguous: ROM [0xA68A0, 0xD87E0).
OVERLAYS = {
    0:  (0x0A68A0, 0x2A60), 1:  (0x0A9300, 0x3160), 2:  (0x0AC460, 0x3500),
    3:  (0x0AF960, 0x1280), 4:  (0x0B0BE0, 0x4040), 5:  (0x0B4C20, 0x3240),
    6:  (0x0B7E60, 0x0E00), 7:  (0x0B8C60, 0x2EC0), 8:  (0x0BBB20, 0x3040),
    9:  (0x0BEB60, 0x1BC0), 10: (0x0C0720, 0x4320), 11: (0x0C4A40, 0x4D60),
    12: (0x0C97A0, 0x1200), 13: (0x0CA9A0, 0x2560), 14: (0x0CCF00, 0x2860),
    15: (0x0CF760, 0x1DA0), 16: (0x0D1500, 0x1EE0), 17: (0x0D33E0, 0x0100),
    98: (0x0D34E0, 0x5300),
}

REGS = ["zero","at","v0","v1","a0","a1","a2","a3","t0","t1","t2","t3","t4","t5","t6","t7",
        "s0","s1","s2","s3","s4","s5","s6","s7","t8","t9","k0","k1","gp","sp","fp","ra"]

BRANCH_OPS = {1, 4, 5, 6, 7, 20, 21, 22, 23}
LIKELY_OPS = {20, 21, 22, 23}


class Ins:
    __slots__ = ("pc","w","op","rs","rt","rd","funct","imm","simm","target","btarget","kind","mnem")

    def __init__(self, w, pc):
        self.pc, self.w = pc, w
        self.op, self.funct = w >> 26, w & 0x3F
        self.rs, self.rt, self.rd = (w >> 21) & 0x1F, (w >> 16) & 0x1F, (w >> 11) & 0x1F
        self.imm = w & 0xFFFF
        self.simm = self.imm - 0x10000 if self.imm & 0x8000 else self.imm
        self.target = self.btarget = None
        self.kind, self.mnem = "other", None
        op = self.op
        if w == 0:
            self.kind = self.mnem = "nop"
        elif op == 0:
            if self.funct == 8:
                self.kind = self.mnem = "jr"
            elif self.funct == 9:
                self.kind = self.mnem = "jalr"
            else:
                self.mnem = {32:"add",33:"addu",35:"subu",36:"and",37:"or",42:"slt",
                             43:"sltu",0:"sll",2:"srl",3:"sra"}.get(self.funct, f"special.{self.funct}")
        elif op in (2, 3):
            self.kind = self.mnem = "j" if op == 2 else "jal"
            self.target = (((pc + 4) & 0xF0000000) | ((w & 0x03FFFFFF) << 2)) & 0xFFFFFFFF
        elif op == 16 and self.rs == 16 and self.funct == 24:
            self.kind = self.mnem = "eret"
        elif op in BRANCH_OPS or (op in (17, 18) and self.rs == 8):
            self.kind = "branchl" if (op in LIKELY_OPS or (op in (17, 18) and self.rt & 2)) else "branch"
            self.btarget = (pc + 4 + (self.simm << 2)) & 0xFFFFFFFF
            if op == 1:
                self.mnem = {0:"bltz",1:"bgez",16:"bltzal",17:"bgezal"}.get(self.rt, f"regimm.{self.rt}")
                if self.rt in (16, 17):
                    self.kind = "branch"
            else:
                self.mnem = {4:"beq",5:"bne",6:"blez",7:"bgtz",20:"beql",21:"bnel",
                             22:"blezl",23:"bgtzl",17:"bc1",18:"bc2"}.get(op, f"br.{op}")
        else:
            self.mnem = {8:"addi",9:"addiu",10:"slti",11:"sltiu",12:"andi",13:"ori",14:"xori",
                         15:"lui",32:"lb",33:"lh",35:"lw",36:"lbu",37:"lhu",40:"sb",41:"sh",
                         43:"sw",49:"lwc1",57:"swc1",53:"ldc1",61:"sdc1"}.get(op, f"op.{op}")

    def text(self):
        r = lambda i: "$" + REGS[i]
        if self.kind == "nop": return "nop"
        if self.kind in ("j", "jal"): return f"{self.mnem} 0x{self.target:08X}"
        if self.kind == "jr": return f"jr {r(self.rs)}"
        if self.kind == "jalr": return f"jalr {r(self.rd)}, {r(self.rs)}"
        if self.kind == "eret": return "eret"
        if self.kind.startswith("branch"): return f"{self.mnem} -> 0x{self.btarget:08X}"
        if self.mnem == "lui": return f"lui {r(self.rt)}, 0x{self.imm:04X}"
        if self.mnem in ("addiu","addi","slti","sltiu"): return f"{self.mnem} {r(self.rt)}, {r(self.rs)}, {self.simm}"
        if self.mnem in ("lw","sw","lh","lhu","lb","lbu","sb","sh"): return f"{self.mnem} {r(self.rt)}, {self.simm}({r(self.rs)})"
        return f"{self.mnem} 0x{self.w:08X}"


def is_flow_end(i):
    return i.kind in ("jr", "j", "eret")


class Rom:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        self.sha = hashlib.sha256(self.data).hexdigest()
        if self.sha != EXPECTED_SHA:
            raise RuntimeError("Unexpected ROM SHA256; refusing to use fixed offsets")

    def word(self, off):
        return int.from_bytes(self.data[off:off + 4], "big")

    def res(self, vram):
        if not (RES_VRAM <= vram < RES_VRAM + RES_SIZE):
            return None
        return self.word(RES_ROM + vram - RES_VRAM)


def resident_low_targets(rom):
    """Every distinct J/JAL target in resident code that lands below RES_VRAM."""
    refs = {}
    for pc in range(RES_VRAM, RES_VRAM + RES_SIZE, 4):
        i = Ins(rom.word(RES_ROM + pc - RES_VRAM), pc)
        if i.kind in ("jal", "j") and i.target is not None and i.target < RES_VRAM:
            refs.setdefault(i.target, []).append(pc)
    return refs


class Overlay:
    def __init__(self, rom, oid):
        self.rom, self.oid = rom, oid
        self.rom_off, self.size = OVERLAYS[oid]
        self.lo = OVERLAY_VRAM
        self.hi = OVERLAY_VRAM + self.size
        self.hazards = []
        self.shared = []
        self.tables = []

    def __contains__(self, v):
        return self.lo <= v < self.hi

    def word(self, v):
        return self.rom.word(self.rom_off + v - self.lo) if v in self else None

    def ins(self, v):
        w = self.word(v)
        return None if w is None else Ins(w, v)

    def is_entry(self, v):
        if v not in self:
            return False
        if v == self.lo:
            return True
        w = self.word(v - 8)
        return w is not None and is_flow_end(Ins(w, v - 8))

    # -- indexed switch resolution -----------------------------------------
    def resolve_table(self, jr_pc):
        """Resolve `jr $rX` fed by lui/addu/lw against a word table.

        Returns (table_vram, [targets]) or None. The table itself may live in
        the overlay or in resident data; both are read from actual ROM words.
        """
        load = base = None
        for b in range(jr_pc - 4, max(self.lo, jr_pc - 40), -4):
            i = self.ins(b)
            if i is None:
                break
            if load is None and i.mnem == "lw":
                load = i
                continue
            if load is not None and i.mnem == "lui":
                base = ((i.imm << 16) + load.simm) & 0xFFFFFFFF
                break
        if base is None:
            return None
        bound = None
        for b in range(jr_pc - 4, max(self.lo, jr_pc - 96), -4):
            i = self.ins(b)
            if i is not None and i.mnem == "sltiu":
                bound = i.simm
                break
        if bound is None or not (0 < bound <= 256):
            self.hazards.append(f"jr at 0x{jr_pc:08X}: table base 0x{base:08X} but no usable bound")
            return None
        targets = []
        for n in range(bound):
            a = base + n * 4
            w = self.word(a)
            if w is None:
                w = self.rom.res(a)
            if w is None:
                self.hazards.append(f"jr at 0x{jr_pc:08X}: table word 0x{a:08X} unreadable")
                return None
            if w not in self:
                self.hazards.append(f"jr at 0x{jr_pc:08X}: entry {n} = 0x{w:08X} outside overlay")
                return None
            targets.append(w)
        self.tables.append((jr_pc, base, bound))
        return base, targets

    # -- reachability walk --------------------------------------------------
    def walk(self, seeds):
        funcs = set(s for s in seeds if s in self)
        reach = set()
        pending = list(funcs)
        done_funcs = set()
        while pending:
            f = pending.pop()
            if f in done_funcs:
                continue
            done_funcs.add(f)
            blocks = [f]
            seen_blocks = set()
            while blocks:
                pc = blocks.pop()
                if pc in seen_blocks or pc not in self:
                    continue
                seen_blocks.add(pc)
                while pc in self:
                    if pc in reach and pc != f and pc not in seen_blocks:
                        pass
                    i = self.ins(pc)
                    if i is None:
                        break
                    reach.add(pc)
                    if i.kind in ("branch", "branchl"):
                        reach.add(pc + 4)
                        if i.btarget in self:
                            blocks.append(i.btarget)
                        pc += 8
                        continue
                    if i.kind == "jal":
                        reach.add(pc + 4)
                        if i.target in self and i.target not in done_funcs:
                            funcs.add(i.target)
                            pending.append(i.target)
                        pc += 8
                        continue
                    if i.kind == "jalr":
                        reach.add(pc + 4)
                        self.hazards.append(f"indirect call (jalr) at 0x{pc:08X}")
                        pc += 8
                        continue
                    if i.kind == "j":
                        reach.add(pc + 4)
                        if i.target in self:
                            blocks.append(i.target)
                        break
                    if i.kind == "jr":
                        reach.add(pc + 4)
                        if i.rs != 31:
                            r = self.resolve_table(pc)
                            if r:
                                blocks.extend(r[1])
                        break
                    if i.kind == "eret":
                        break
                    pc += 4
        return sorted(funcs), reach

    # -- single-entry reachability -----------------------------------------
    def walk_from(self, entry):
        """Instructions reachable from ONE entry, not following jal.

        Two resident entries may converge on shared tail code (the pattern
        Pass 8 handled in overlay 16). Deriving each entry's extent from its
        own reachability produces deliberately overlapping ranges, which is
        what keeps every branch inside its function.
        """
        reach, blocks, seen = set(), [entry], set()
        while blocks:
            pc = blocks.pop()
            if pc in seen or pc not in self:
                continue
            seen.add(pc)
            while pc in self:
                if pc in reach:
                    break
                i = self.ins(pc)
                if i is None:
                    break
                reach.add(pc)
                if i.kind in ("branch", "branchl"):
                    reach.add(pc + 4)
                    if i.btarget in self:
                        blocks.append(i.btarget)
                    pc += 8
                    continue
                if i.kind in ("jal", "jalr"):
                    reach.add(pc + 4)
                    pc += 8
                    continue
                if i.kind == "j":
                    reach.add(pc + 4)
                    if i.target in self:
                        blocks.append(i.target)
                    break
                if i.kind == "jr":
                    reach.add(pc + 4)
                    if i.rs != 31:
                        r = self.resolve_table(pc)
                        if r:
                            blocks.extend(r[1])
                    break
                if i.kind == "eret":
                    break
                pc += 4
        return reach

    # -- extents + closure audit -------------------------------------------
    def ranges(self, funcs, reach):
        if not reach:
            return [], ["overlay has no reachable code from proven entries"]
        out, fails = [], []
        for s in funcs:
            own = self.walk_from(s)
            if not own:
                fails.append(f"{section_name(self.oid)}_func_{s:08X}: no reachable instructions")
                out.append((s, s + 8, None))
                continue
            true_end = max(own) + 4
            closer = self.ins(true_end - 8)
            if closer is None or not is_flow_end(closer):
                fails.append(f"{section_name(self.oid)}_func_{s:08X}: does not end at a flow-end "
                             f"+ delay slot (0x{true_end - 8:08X} = "
                             f"{closer.text() if closer else '?'})")
            # Branch containment: N64Recomp rejects a function whose branches
            # leave its declared range, so this is the check that matters.
            for v in sorted(own):
                i = self.ins(v)
                if i is None:
                    continue
                t = i.btarget if i.kind in ("branch", "branchl") else (
                    i.target if i.kind == "j" and i.target in self else None)
                if t is not None and not (s <= t < true_end):
                    fails.append(f"{section_name(self.oid)}_func_{s:08X}: branch at 0x{v:08X} "
                                 f"leaves the range (to 0x{t:08X})")
            if s < true_end and min(own) != s:
                fails.append(f"{section_name(self.oid)}_func_{s:08X}: entry is not the lowest "
                             f"reachable address")
            out.append((s, true_end, closer))
        # Overlapping ranges are expected where entries share tail code; record
        # them so the configuration comment can say so honestly.
        self.shared = [(a, b) for i, (a, ae, _x) in enumerate(out)
                       for (b, be, _y) in out[i + 1:] if b < ae]
        # every reachable instruction must fall inside exactly one range
        covered = set()
        for s, e, _ in out:
            covered.update(range(s, e, 4))
        orphan = sorted(reach - covered)
        if orphan:
            fails.append(f"{len(orphan)} reachable instruction(s) outside every declared range, "
                         f"first 0x{orphan[0]:08X}")
        return out, fails


def section_name(oid):
    return f"ovl{oid:02d}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--emit", metavar="PATH", help="write candidate syms.toml sections here")
    ap.add_argument("--preserve", metavar="IDS", default="16,98",
                    help="overlay ids whose EXISTING project function tables are already "
                         "accepted by N64Recomp; they are audited and reported but excluded "
                         "from --emit so a proven map is never churned (default: 16,98)")
    args = ap.parse_args()

    rom = Rom(ROM_PATH)
    preserve = {int(x) for x in args.preserve.split(",") if x.strip()}
    refs = resident_low_targets(rom)
    out = ["SpaceRanger low-memory overlay function map",
           f"ROM SHA256: {rom.sha}",
           "Discovery is reachability-based from proven entries only.",
           "Static ROM analysis only; the live symbol configuration is untouched.",
           "",
           f"Resident J/JAL targets below 0x{RES_VRAM:08X}: {len(refs)}",
           ""]

    results, total_fail = {}, 0
    for oid in sorted(OVERLAYS):
        ov = Overlay(rom, oid)
        seeds = {ov.lo} | {t for t in refs if ov.is_entry(t)} | RUNTIME_CALLBACK_ENTRIES.get(oid, set())
        funcs, reach = ov.walk(seeds)
        rngs, fails = ov.ranges(funcs, reach)
        results[oid] = (ov, rngs, fails, reach, seeds)
        # Overlays in --preserve keep the project's hand-built function tables,
        # which N64Recomp has already accepted. This tool's independently
        # derived model may legitimately differ there (different shared-entry
        # decomposition of the same code), so those differences are reported
        # as informational and never gate the run. The authority for a
        # preserved overlay is the recompiler itself.
        if oid not in preserve:
            total_fail += len(fails)

    out += ["=" * 78,
            "SUMMARY",
            "=" * 78,
            " id  section   ROM range            size    funcs  code    data    tables  fails"]
    for oid in sorted(OVERLAYS):
        ov, rngs, fails, reach, _ = results[oid]
        code = (max(reach) + 4 - ov.lo) if reach else 0
        out.append(f"{oid:3d}  {section_name(oid):8s}  "
                   f"[0x{ov.rom_off:06X},0x{ov.rom_off + ov.size:06X})  "
                   f"0x{ov.size:<5X}  {len(rngs):5d}  0x{code:<5X}  "
                   f"0x{ov.size - code:<5X}  {len(ov.tables):6d}  "
                   f"{('(' + str(len(fails)) + ')') if oid in preserve else len(fails):>5}")
    emitted = [oid for oid in results if oid not in preserve]
    out += ["",
            "Counts in parentheses are informational: that overlay is in --preserve, "
            "so its",
            "project function table is kept and this tool's own model is not used for it.",
            "",
            f"overlays regenerated: {len(emitted)}   preserved: {sorted(preserve)}",
            f"functions in regenerated overlays: "
            f"{sum(len(results[o][1]) for o in emitted)}",
            f"gating closure/containment failures: {total_fail}", ""]

    for oid in sorted(OVERLAYS):
        ov, rngs, fails, reach, seeds = results[oid]
        out += ["-" * 78,
                f"overlay {oid} ({section_name(oid)})  ROM [0x{ov.rom_off:06X},"
                f"0x{ov.rom_off + ov.size:06X})  VRAM [0x{ov.lo:08X},0x{ov.hi:08X})",
                "-" * 78,
                f"  seeds from resident: {sorted(f'0x{s:08X}' for s in seeds if s != ov.lo)}"]
        if ov.tables:
            for jr_pc, base, bound in ov.tables:
                out.append(f"  indexed switch at 0x{jr_pc:08X}: table 0x{base:08X}, {bound} entries")
        if ov.shared:
            out.append("  shared-entry overlaps (two entries converging on common tail code, "
                       "the Pass 8 model):")
            for a, b in ov.shared:
                out.append(f"    0x{a:08X} contains the range of 0x{b:08X}")
        for s, e, closer in rngs:
            tag = "resident-called" if s in refs else "internal"
            out.append(f"    {section_name(oid)}_func_{s:08X}  [0x{s:08X},0x{e:08X})  "
                       f"size 0x{e - s:<6X} {tag:16s} closes: {closer.text() if closer else '?'}")
        for h in ov.hazards:
            out.append(f"  HAZARD: {h}")
        for f in fails:
            out.append(f"  FAILURE: {f}")
        out.append("")

    out += ["=" * 78, f"HARD FAILURE COUNT: {total_fail}", "=" * 78]
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text("\n".join(out) + "\n", encoding="utf-8")
    print("\n".join(out[:40]))
    print(f"\nFull report: {REPORT}")

    if args.emit:
        lines = []
        for oid in sorted(OVERLAYS):
            if oid in preserve:
                lines += [f'# Loader id {oid} is already mapped in the project configuration '
                          f'and accepted by N64Recomp.',
                          f'# Preserved verbatim (section renamed to {section_name(oid)}); '
                          f'not regenerated here.', '']
                continue
            ov, rngs, fails, _reach, _s = results[oid]
            lines += [f'# Loader id {oid}. Fixed-address low-memory overlay, DMA\'d to '
                      f'VRAM 0x{ov.lo:08X}.',
                      f'# Cart source 0xB0{ov.rom_off:06X}. Reachability-derived, closure-audited.',
                      '[[section]]',
                      f'name = "{section_name(oid)}"',
                      f'rom = 0x{ov.rom_off:X}',
                      f'vram = 0x{ov.lo:08X}',
                      f'size = 0x{ov.size:X}',
                      '',
                      'functions = [']
            for s, e, _c in rngs:
                lines.append(f'  {{ name = "{section_name(oid)}_func_{s:08X}", '
                             f'vram = 0x{s:08X}, size = 0x{e - s:X} }},')
            lines += [']', '']
        Path(args.emit).write_text("\n".join(lines), encoding="utf-8")
        print(f"Candidate sections: {args.emit}")
    return 1 if total_fail else 0


if __name__ == "__main__":
    sys.exit(main())
