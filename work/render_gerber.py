"""Minimal Gerber renderer (top view) for silk-mirroring checks.
usage: python render_gerber.py <layer.gbr> <outline.gko> <out.png> [scale_px_per_mm]
Handles FS/MO, ADD C/R/O apertures, D01/D02/D03, G01/G02/G03 (arcs approximated), G36/G37 regions.
"""
import re, sys, math
from PIL import Image, ImageDraw

def parse(path):
    txt = open(path, encoding="utf-8", errors="ignore").read()
    unit = 1.0 if "%MOMM" in txt else 25.4
    m = re.search(r"%FSLAX(\d)(\d)Y(\d)(\d)\*%", txt)
    dec = int(m.group(2)) if m else 6
    ap, cur, x, y, mode, region, poly = {}, None, 0.0, 0.0, 1, False, []
    prims = []  # ("line", x1,y1,x2,y2,w) | ("flash", x,y,ap) | ("poly", pts)
    for blk in re.finditer(r"%ADD(\d+)([A-Z]),?([^*]*)\*%", txt):
        p = [float(v) * unit for v in blk.group(3).split("X") if v]
        ap[int(blk.group(1))] = (blk.group(2), p)
    body = re.sub(r"%[^%]*%", "", txt)
    for cmd in body.split("*"):
        cmd = cmd.strip()
        if not cmd: continue
        if cmd.startswith("G36"): region, poly = True, []; continue
        if cmd.startswith("G37"):
            if len(poly) > 2: prims.append(("poly", poly))
            region = False; continue
        g = re.match(r"G0?([123])", cmd)
        if g: mode = int(g.group(1)); cmd = cmd[g.end():]
        d = re.match(r"D(\d+)$", cmd)
        if d and int(d.group(1)) >= 10: cur = int(d.group(1)); continue
        mx = re.search(r"X(-?\d+)", cmd); my = re.search(r"Y(-?\d+)", cmd)
        mi = re.search(r"I(-?\d+)", cmd); mj = re.search(r"J(-?\d+)", cmd)
        md = re.search(r"D0?([123])$", cmd)
        nx = int(mx.group(1)) / 10 ** dec * unit if mx else x
        ny = int(my.group(1)) / 10 ** dec * unit if my else y
        op = int(md.group(1)) if md else None
        if op == 1:
            pts = [(x, y), (nx, ny)]
            if mode in (2, 3) and (mi or mj):
                cx = x + (int(mi.group(1)) / 10 ** dec * unit if mi else 0); cy = y + (int(mj.group(1)) / 10 ** dec * unit if mj else 0)
                a0, a1, r = math.atan2(y - cy, x - cx), math.atan2(ny - cy, nx - cx), math.hypot(x - cx, y - cy)
                if mode == 3 and a1 <= a0: a1 += 2 * math.pi
                if mode == 2 and a1 >= a0: a1 -= 2 * math.pi
                pts = [(cx + r * math.cos(a0 + (a1 - a0) * k / 24), cy + r * math.sin(a0 + (a1 - a0) * k / 24)) for k in range(25)]
            if region: poly.extend(pts if not poly else pts[1:])
            else:
                w = ap.get(cur, ("C", [0.15]))[1][0] if cur in ap else 0.15
                for a, b in zip(pts, pts[1:]): prims.append(("line", a[0], a[1], b[0], b[1], w))
        elif op == 2:
            if region:
                if len(poly) > 2: prims.append(("poly", poly))
                poly = [(nx, ny)]
        elif op == 3 and cur in ap:
            prims.append(("flash", nx, ny, ap[cur]))
        x, y = nx, ny
    return prims

def render(layer, outline, out, s=20):
    P, O = parse(layer), parse(outline)
    xs = [v for p in O if p[0] == "line" for v in (p[1], p[3])]; ys = [v for p in O if p[0] == "line" for v in (p[2], p[4])]
    x0, x1, y0, y1 = min(xs) - 1, max(xs) + 1, min(ys) - 1, max(ys) + 1
    W, H = int((x1 - x0) * s), int((y1 - y0) * s)
    im = Image.new("RGB", (W, H), "white"); dr = ImageDraw.Draw(im)
    T = lambda x, y: ((x - x0) * s, (y1 - y) * s)
    for p in O:
        if p[0] == "line": dr.line([T(p[1], p[2]), T(p[3], p[4])], fill=(200, 0, 200), width=2)
    for p in P:
        if p[0] == "line": dr.line([T(p[1], p[2]), T(p[3], p[4])], fill="black", width=max(1, int(p[5] * s)))
        elif p[0] == "poly": dr.polygon([T(a, b) for a, b in p[1]], fill="black")
        elif p[0] == "flash":
            kind, prm = p[3]; (cx, cy) = T(p[1], p[2]); r = prm[0] * s / 2
            if kind == "C": dr.ellipse([cx - r, cy - r, cx + r, cy + r], fill="black")
            else: rh = (prm[1] if len(prm) > 1 else prm[0]) * s / 2; dr.rectangle([cx - r, cy - rh, cx + r, cy + rh], fill="black")
    im.save(out)
    return W, H

if __name__ == "__main__":
    print(render(sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4]) if len(sys.argv) > 4 else 20))
