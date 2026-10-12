"""Reject direct-call/lookup disagreements at the host's native OS boundaries.

The host lookup table and the recompiler symbol table must agree: mapping a
function pointer to osMotorInit_recomp does not redirect a generated direct call
to an unnamed guest copy. That mismatch caused the gameplay SI wait deadlock.
"""
from pathlib import Path
import re
import tomllib
from map_lowmem_overlays import RUNTIME_CALLBACK_ENTRIES

ROOT = Path(__file__).resolve().parent.parent


def main():
    symbols = tomllib.loads((ROOT / 'config/toystory2.us.rev0.syms.toml').read_text())
    resident = next(s for s in symbols['section'] if s['name'] == 'resident')
    functions = {f['vram']: f['name'] for f in resident['functions']}
    host = (ROOT / 'src/stage79_first_vi_audio_wakeup.cpp').read_text()
    mappings = set(re.findall(
        r'add_loaded_function\(\s*static_cast<int32_t>\(0x([0-9A-Fa-f]+)u\),'
        r'\s*((?:__)?os\w+_recomp)\)', host))
    if not mappings:
        raise SystemExit('No native OS mappings found; audit parser needs updating.')
    failures = []
    for oid, addresses in RUNTIME_CALLBACK_ENTRIES.items():
        section = next(s for s in symbols['section'] if s['name'] == f'ovl{oid:02d}')
        mapped = {f['vram'] for f in section['functions']}
        for addr in addresses - mapped:
            failures.append(f'Missing object callback in ovl{oid:02d}: 0x{addr:08X}.')
    if functions.get(0x800933A0) != 'osViSetSpecialFeatures':
        failures.append('Game VI feature requests must reach the native renderer.')
    if functions.get(0x800A0540) != 'osEPiStartDma':
        failures.append('Audio sample DMA must use native osEPiStartDma; the guest PI manager is absent.')
    level2 = next(s for s in symbols['section'] if s['name'] == 'ovl02')
    if not any(f['vram'] == 0x800009F8 and f['size'] == 0x21C
               for f in level2['functions']):
        failures.append('Missing Level 2 runtime callback at 0x800009F8 (size 0x21C).')
    for address, native in sorted(mappings):
        actual = functions.get(int(address, 16))
        if actual != native.removesuffix('_recomp'):
            failures.append(f'0x{address}: lookup={native}, direct={actual}')
    # Guard the restored audio-bank patch against ROM/config drift.
    config = tomllib.loads((ROOT / 'config/toystory2.us.rev0.recomp.toml').read_text())
    patches = config['patches'].get('instruction', [])
    expected = {'func': 'func_8001923C', 'vram': 0x800192F8, 'value': 0xAFA00030}
    if expected not in patches:
        failures.append('Missing RAM-only default_fxbank patch at 0x800192F8.')
    rom = (ROOT / 'rom/baserom.us.rev0.z64').read_bytes()
    offset = 0x800192F8 - resident['vram'] + resident['rom']
    if int.from_bytes(rom[offset:offset + 4], 'big') != 0xAFB00030:
        failures.append('Unexpected original instruction at audio-bank patch address.')
    print(f'Checked {len(mappings)} native OS mappings, Level 2 callback, and audio-bank patch.')
    for failure in failures:
        print(failure)
    raise SystemExit(bool(failures))


if __name__ == '__main__':
    main()
