#!/usr/bin/env python3
"""
audit_recomp_refs.py -- enumerate every generated function that calls one of
the *_recomp symbols the runtime does not provide.

N64Recomp (main.cpp:428-441) renames any syms.toml function whose name matches
its built-in ignored_funcs / reimplemented_funcs list to "<name>_recomp" and
suppresses the body. reimplemented_funcs are supplied by N64ModernRuntime;
ignored_funcs are NOT, so a call to one is an unresolved external unless the
caller is itself suppressed.

This reports the callers so they can be identified and named.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
GEN = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "RecompiledFuncs"

TARGETS = [
    "__osDispatchThread_recomp",
    "__osEnqueueAndYield_recomp",
    "__osProbeTLB_recomp",
    "osMapTLBRdb_recomp",
]

FUNC_RE = re.compile(r"^\s*(?:RECOMP_FUNC\s+)?void\s+(\w+)\s*\(")


def main():
    if not GEN.is_dir():
        sys.exit(f"missing {GEN}")

    # symbol -> {caller: count}
    refs = {t: {} for t in TARGETS}
    # caller -> file
    where = {}

    for path in sorted(GEN.glob("*.c")):
        cur = None
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            m = FUNC_RE.match(line)
            if m:
                cur = m.group(1)
                where.setdefault(cur, path.name)
                continue
            for t in TARGETS:
                if t in line:
                    refs[t][cur] = refs[t].get(cur, 0) + 1

    callers = set()
    for t in TARGETS:
        print(f"{t}")
        if not refs[t]:
            print("    (no references)")
        for c, n in sorted(refs[t].items(), key=lambda kv: kv[0] or ""):
            callers.add(c)
            print(f"    {c:<20} x{n}  [{where.get(c, '?')}]")
        print()

    print("=" * 60)
    print(f"distinct calling functions: {len(callers)}")
    for c in sorted(callers, key=lambda s: s or ""):
        print(f"  {c}")
    raise SystemExit(1 if callers else 0)


if __name__ == "__main__":
    main()
