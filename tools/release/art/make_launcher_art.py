"""Original launcher art for Toy Story 2: Recompiled.

logo.svg      - wordmark ("TOY STORY 2" / "RECOMPILED"), text converted to paths
background.svg - space backdrop: nebula, stars, ringed planet, moon, rocket

Everything is vector and uses only features lunasvg (RmlUi's SVG renderer)
supports: paths, circles, ellipses, linear/radial gradients, opacity, groups,
transforms. No filters, no <text>.
"""
import math, random, sys, io
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.varLib.instancer import instantiateVariableFont

FONTS = sys.argv[1]
OUT = sys.argv[2]

PURPLE = '#7B3FE4'
PURPLE_L = '#B48CF5'
PURPLE_D = '#2A0E5C'
LIME = '#A8F02B'
LIME_D = '#5FB000'
WHITE = '#FFFFFF'


def text_path(font, text, size, x, y, tracking=0.0):
    """Return (svg path d, advance width) for text with baseline at y."""
    gs = font.getGlyphSet()
    cmap = font.getBestCmap()
    upm = font['head'].unitsPerEm
    scale = size / upm
    pen = SVGPathPen(gs)
    cx = 0.0
    for ch in text:
        gname = cmap.get(ord(ch))
        if gname is None:
            cx += size * 0.3
            continue
        tp = TransformPen(pen, (scale, 0, 0, -scale, x + cx, y))
        gs[gname].draw(tp)
        cx += gs[gname].width * scale + tracking
    return pen.getCommands(), cx - tracking


def load_fonts():
    bungee = TTFont(f'{FONTS}/Bungee-Regular.ttf')
    fredoka = TTFont(f'{FONTS}/Fredoka-VF.ttf')
    fredoka = instantiateVariableFont(fredoka, {'wght': 700, 'wdth': 100})
    return bungee, fredoka


def star_path(cx, cy, r, inner=0.28, points=4, rot=0.0):
    pts = []
    for i in range(points * 2):
        rr = r if i % 2 == 0 else r * inner
        a = rot + math.pi * i / points - math.pi / 2
        pts.append(f'{cx + rr * math.cos(a):.1f},{cy + rr * math.sin(a):.1f}')
    return 'M' + ' L'.join(pts) + ' Z'


def make_logo(bungee, fredoka):
    T1, S1, T2, S2 = 'SPACE', 200, 'RANGER', 178
    d1, w1 = text_path(bungee, T1, S1, 0, 0, tracking=6)
    d2, w2 = text_path(fredoka, T2, S2, 0, 0, tracking=8)
    W, H = int(max(w1, w2 + 80) + 160), 470
    # Center each line in the canvas.
    x1 = (W - w1) / 2 - 30
    x2 = (W - w2) / 2 + 50
    d1, _ = text_path(bungee, T1, S1, x1, 215, tracking=6)
    d2, _ = text_path(fredoka, T2, S2, x2, 400, tracking=8)
    s = []
    s.append(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}">')
    s.append('<defs>')
    s.append(f'<linearGradient id="lime" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#E4FF8A"/><stop offset="0.45" stop-color="{LIME}"/><stop offset="1" stop-color="{LIME_D}"/></linearGradient>')
    s.append(f'<linearGradient id="purple" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="{PURPLE_L}"/><stop offset="0.5" stop-color="{PURPLE}"/><stop offset="1" stop-color="#4A1A9E"/></linearGradient>')
    s.append('</defs>')
    # Line 1: lime letters, white then deep purple outline, drop shadow.
    s.append(f'<g transform="translate(0,10)"><path d="{d1}" fill="#12052E" stroke="#12052E" stroke-width="30" stroke-linejoin="round" opacity="0.55"/></g>')
    s.append(f'<path d="{d1}" fill="{PURPLE_D}" stroke="{PURPLE_D}" stroke-width="30" stroke-linejoin="round"/>')
    s.append(f'<path d="{d1}" fill="{WHITE}" stroke="{WHITE}" stroke-width="12" stroke-linejoin="round"/>')
    s.append(f'<path d="{d1}" fill="url(#lime)"/>')
    # Line 2: purple letters, lime then deep purple outline, slight tilt.
    s.append(f'<g transform="rotate(-4 {W/2:.0f} 330)">')
    s.append(f'<g transform="translate(0,10)"><path d="{d2}" fill="#12052E" stroke="#12052E" stroke-width="30" stroke-linejoin="round" opacity="0.55"/></g>')
    s.append(f'<path d="{d2}" fill="{PURPLE_D}" stroke="{PURPLE_D}" stroke-width="30" stroke-linejoin="round"/>')
    s.append(f'<path d="{d2}" fill="{LIME}" stroke="{LIME}" stroke-width="12" stroke-linejoin="round"/>')
    s.append(f'<path d="{d2}" fill="url(#purple)"/>')
    s.append('</g>')
    # Sparkles.
    for cx, cy, r in [(x1 - 34, 70, 36), (x1 + w1 + 34, 46, 28), (x2 + w2 + 16, 330, 24)]:
        s.append(f'<path d="{star_path(cx, cy, r + 8)}" fill="{PURPLE_D}"/>')
        s.append(f'<path d="{star_path(cx, cy, r)}" fill="{WHITE}"/>')
    s.append('</svg>')
    return '\n'.join(s)


def make_background():
    W, H = 1920, 1080
    rnd = random.Random(2026)
    s = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}">']
    s.append('<defs>')
    s.append('<radialGradient id="sky" cx="0.3" cy="0.75" r="1.1"><stop offset="0" stop-color="#3B1A78"/><stop offset="0.45" stop-color="#1E0D48"/><stop offset="1" stop-color="#0A0520"/></radialGradient>')
    s.append('<radialGradient id="neb1" cx="0.5" cy="0.5" r="0.5"><stop offset="0" stop-color="#9B5CFF" stop-opacity="0.55"/><stop offset="1" stop-color="#9B5CFF" stop-opacity="0"/></radialGradient>')
    s.append('<radialGradient id="neb2" cx="0.5" cy="0.5" r="0.5"><stop offset="0" stop-color="#7EE01A" stop-opacity="0.28"/><stop offset="1" stop-color="#7EE01A" stop-opacity="0"/></radialGradient>')
    s.append('<radialGradient id="planet" cx="0.35" cy="0.3" r="0.8"><stop offset="0" stop-color="#C9A2FF"/><stop offset="0.45" stop-color="#7B3FE4"/><stop offset="1" stop-color="#2A0E5C"/></radialGradient>')
    s.append('<radialGradient id="moon" cx="0.35" cy="0.3" r="0.8"><stop offset="0" stop-color="#E9FFB0"/><stop offset="0.5" stop-color="#A8F02B"/><stop offset="1" stop-color="#3F7A00"/></radialGradient>')
    s.append('<linearGradient id="ring" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#5FB000"/><stop offset="0.5" stop-color="#D8FF7A"/><stop offset="1" stop-color="#5FB000"/></linearGradient>')
    s.append('<linearGradient id="flame" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#FFF6B0"/><stop offset="0.5" stop-color="#FFB13B"/><stop offset="1" stop-color="#FF5A1F" stop-opacity="0"/></linearGradient>')
    s.append('<linearGradient id="hull" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#D9D3EA"/><stop offset="0.45" stop-color="#FFFFFF"/><stop offset="1" stop-color="#A79CC4"/></linearGradient>')
    s.append('<linearGradient id="fade" x1="0" y1="0" x2="1" y2="0"><stop offset="0.45" stop-color="#0A0520" stop-opacity="0"/><stop offset="1" stop-color="#0A0520" stop-opacity="0.55"/></linearGradient>')
    s.append('</defs>')
    s.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    # Nebula clouds.
    for cx, cy, rx, ry, g in [(420, 820, 760, 420, 'neb1'), (1500, 260, 620, 340, 'neb1'), (980, 560, 700, 300, 'neb2'), (240, 180, 420, 260, 'neb2')]:
        s.append(f'<ellipse cx="{cx}" cy="{cy}" rx="{rx}" ry="{ry}" fill="url(#{g})"/>')
    # Stars.
    for _ in range(260):
        x, y = rnd.uniform(0, W), rnd.uniform(0, H)
        r = rnd.choice([0.8, 1.0, 1.2, 1.6, 2.2])
        o = rnd.uniform(0.35, 1.0)
        s.append(f'<circle cx="{x:.0f}" cy="{y:.0f}" r="{r}" fill="#FFFFFF" opacity="{o:.2f}"/>')
    for _ in range(14):
        x, y = rnd.uniform(60, W - 60), rnd.uniform(40, H - 40)
        r = rnd.uniform(8, 16)
        col = rnd.choice(['#FFFFFF', '#D8FF7A', '#D9C2FF'])
        s.append(f'<path d="{star_path(x, y, r, inner=0.18)}" fill="{col}" opacity="0.9"/>')
    # Ringed planet (lower left), ring split into back and front halves.
    px, py, pr = 470, 800, 260
    s.append(f'<ellipse cx="{px}" cy="{py}" rx="{pr * 1.85:.0f}" ry="{pr * 0.42:.0f}" fill="none" stroke="url(#ring)" stroke-width="34" opacity="0.55" transform="rotate(-14 {px} {py})"/>')
    s.append(f'<circle cx="{px}" cy="{py}" r="{pr}" fill="url(#planet)"/>')
    for i, (dy, w, o) in enumerate([(-120, 0.86, 0.22), (-40, 0.98, 0.18), (50, 0.97, 0.20), (140, 0.80, 0.16)]):
        s.append(f'<ellipse cx="{px}" cy="{py + dy}" rx="{pr * w:.0f}" ry="18" fill="#2A0E5C" opacity="{o}"/>')
    # Front half of the ring: clip by drawing only the lower arc.
    rx, ry = pr * 1.85, pr * 0.42
    a = math.radians(-14)
    def rp(t):
        x, y = rx * math.cos(t), ry * math.sin(t)
        return px + x * math.cos(a) - y * math.sin(a), py + x * math.sin(a) + y * math.cos(a)
    pts = [rp(math.radians(t)) for t in range(0, 181, 4)]
    d = 'M' + ' L'.join(f'{x:.1f},{y:.1f}' for x, y in pts)
    s.append(f'<path d="{d}" fill="none" stroke="url(#ring)" stroke-width="34" stroke-linecap="round"/>')
    s.append(f'<path d="{d}" fill="none" stroke="#2A0E5C" stroke-width="4" opacity="0.5" transform="translate(0,14)"/>')
    # Darken the right side so menu text stays readable.
    s.append(f'<rect width="{W}" height="{H}" fill="url(#fade)"/>')
    s.append('</svg>')
    return '\n'.join(s)


bungee, fredoka = load_fonts()
open(f'{OUT}/logo.svg', 'w').write(make_logo(bungee, fredoka))
open(f'{OUT}/background.svg', 'w').write(make_background())
print('ok')


def make_moon():
    s = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 160 160" width="160" height="160">']
    s.append('<defs>' + '<radialGradient id="moon" cx="0.35" cy="0.3" r="0.8"><stop offset="0" stop-color="#E9FFB0"/><stop offset="0.5" stop-color="#A8F02B"/><stop offset="1" stop-color="#3F7A00"/></radialGradient>' + '</defs>')
    s.append('<circle cx="80" cy="80" r="70" fill="url(#moon)"/>')
    for cx, cy, r in [(60, 60, 12), (105, 105, 9), (50, 115, 7)]:
        s.append(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="#3F7A00" opacity="0.45"/>')
    s.append('</svg>')
    return '\n'.join(s)


def make_rocket():
    s = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="-330 -330 660 660" width="660" height="660">']
    s.append('<defs>')
    s.append('<linearGradient id="flame" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#FFF6B0"/><stop offset="0.5" stop-color="#FFB13B"/><stop offset="1" stop-color="#FF5A1F" stop-opacity="0"/></linearGradient>')
    s.append('<linearGradient id="hull" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#D9D3EA"/><stop offset="0.45" stop-color="#FFFFFF"/><stop offset="1" stop-color="#A79CC4"/></linearGradient>')
    s.append('</defs>')
    s.append('<g transform="rotate(38)">')
    s.append('<path d="M-34,150 Q0,330 34,150 Z" fill="url(#flame)"/>')
    s.append('<path d="M-60,40 L-110,150 L-40,120 Z" fill="#7B3FE4" stroke="#2A0E5C" stroke-width="8" stroke-linejoin="round"/>')
    s.append('<path d="M60,40 L110,150 L40,120 Z" fill="#7B3FE4" stroke="#2A0E5C" stroke-width="8" stroke-linejoin="round"/>')
    s.append('<path d="M0,-170 C70,-110 70,60 46,150 L-46,150 C-70,60 -70,-110 0,-170 Z" fill="url(#hull)" stroke="#2A0E5C" stroke-width="9" stroke-linejoin="round"/>')
    s.append('<path d="M0,-170 C30,-145 46,-115 52,-80 L-52,-80 C-46,-115 -30,-145 0,-170 Z" fill="#A8F02B" stroke="#2A0E5C" stroke-width="9" stroke-linejoin="round"/>')
    s.append('<circle cx="0" cy="-10" r="30" fill="#2A0E5C"/><circle cx="0" cy="-10" r="20" fill="#7FD3FF"/><circle cx="-7" cy="-17" r="6" fill="#FFFFFF"/>')
    s.append('<path d="M-46,95 L46,95" stroke="#7B3FE4" stroke-width="14"/>')
    s.append('<path d="M0,40 L0,150" stroke="#2A0E5C" stroke-width="8"/>')
    s.append('</g></svg>')
    return '\n'.join(s)


open(f'{OUT}/moon.svg', 'w').write(make_moon())
open(f'{OUT}/rocket.svg', 'w').write(make_rocket())
