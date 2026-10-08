"""Pictures for the book: a goat or ibex of one species standing on a rock on another world, with that world's neighbours in the sky. Flat vector scenes on an 800 x 300 canvas, written as SVG text.
Nothing is random: every 'random' detail (stars, rocks, craters) comes from a generator seeded with the scene's name, so the same script always writes the same pictures. The animals are stylised silhouettes
that show what tells the species apart (horn shape above all); they are drawings, not portraits."""
import math, random
W, H = 800, 300
# species: coat, belly, horn colour, horn style, size
SPECIES = {
    "alpine":   dict(name="Alpine ibex", sci="Capra ibex", coat="#8a7560", belly="#c9b9a2", horn="#c8b48c", style="scimitar", size=1.0),
    "markhor":  dict(name="Markhor", sci="Capra falconeri", coat="#b9a58a", belly="#efe6d6", horn="#6b5a3e", style="spiral", size=1.0),
    "nubian":   dict(name="Nubian ibex", sci="Capra nubiana", coat="#c19a6b", belly="#ecd9b8", horn="#4a3a28", style="sickle", size=0.95),
    "siberian": dict(name="Siberian ibex", sci="Capra sibirica", coat="#a68f6d", belly="#e5d6b9", horn="#b49a6a", style="scimitar", size=1.05),
    "walia":    dict(name="Walia ibex", sci="Capra walie", coat="#6b4a35", belly="#d8c3a4", horn="#3d2f22", style="heavy", size=1.0),
    "iberian":  dict(name="Iberian ibex", sci="Capra pyrenaica", coat="#9a8468", belly="#d8c9ad", horn="#a89468", style="lyre", size=0.95),
    "tur":      dict(name="West Caucasian tur", sci="Capra caucasica", coat="#7d6246", belly="#bfa987", horn="#2f2418", style="tur", size=1.0),
    "bezoar":   dict(name="Bezoar ibex (wild goat)", sci="Capra aegagrus", coat="#a8845a", belly="#e2cfaa", horn="#3a2c1d", style="bezoar", size=0.95),
    "domestic": dict(name="Domestic goat", sci="Capra hircus", coat="#e8e0d0", belly="#f7f2e8", horn="#8d7f68", style="short", size=0.8),
}
# worlds: sky top, sky bottom, ground, far ridge, near rock, name, accent text
WORLDS = {
    "earth":    dict(name="Earth", sky=("#14243f", "#5b8fc4"), far="#7d93ad", ridge="#53647a", ground="#6e7a6a", rock="#8c8f93", stars=40, sun="#fff2c2", bodies=[("moon", 650, 70, 24)]),
    "moon":     dict(name="the Moon", sky=("#000000", "#06070d"), far="#5d5d62", ridge="#47474b", ground="#8b8b90", rock="#a4a4a9", stars=140, sun=None, bodies=[("earth", 600, 80, 46), ("sun", 120, 50, 12)]),
    "mars":     dict(name="Mars", sky=("#3a1f18", "#c97f5a"), far="#a8603f", ridge="#8a4a30", ground="#b5663f", rock="#8c4a2e", stars=18, sun="#ffe9d0", bodies=[("phobos", 640, 60, 11), ("deimos", 520, 95, 6), ("sun", 150, 80, 12)]),
    "europa":   dict(name="Europa", sky=("#000000", "#0a0d18"), far="#b9cfe0", ridge="#8fb0c6", ground="#dbe8f0", rock="#c6d9e6", stars=110, sun=None, bodies=[("jupiter", 560, 110, 120), ("io", 190, 55, 14)]),
    "titan":    dict(name="Titan", sky=("#5a3d12", "#d9a441"), far="#a47a30", ridge="#7e5a22", ground="#8c6a2c", rock="#6e4f1c", stars=0, sun=None, bodies=[("saturn", 600, 80, 70)]),
    "io":       dict(name="Io", sky=("#000000", "#1a1204"), far="#c9a227", ridge="#a87c1c", ground="#d9b83a", rock="#8c6a14", stars=90, sun=None, bodies=[("jupiter", 540, 100, 150), ("europa", 160, 60, 9)]),
    "enceladus":dict(name="Enceladus", sky=("#000000", "#050a14"), far="#e4eef5", ridge="#bfd2df", ground="#f2f7fa", rock="#d6e4ee", stars=120, sun=None, bodies=[("saturn", 480, 120, 150), ("rings", 480, 120, 150)]),
    "pluto":    dict(name="Pluto", sky=("#000000", "#0a0b14"), far="#b79a86", ridge="#8f7566", ground="#d8c2b0", rock="#a08572", stars=130, sun=None, bodies=[("charon", 560, 90, 70), ("sun", 130, 60, 5)]),
    "twin":     dict(name="a planet with two suns", sky=("#2a1040", "#e8845a"), far="#6a3f7a", ridge="#4a2b5a", ground="#5a3a62", rock="#3d2547", stars=30, sun=None, bodies=[("sunbig", 150, 120, 36), ("sunsmall", 260, 80, 16), ("moonpurple", 650, 70, 28)]),
    "triton":   dict(name="Triton", sky=("#000000", "#06101e"), far="#cfdde6", ridge="#9fb7c6", ground="#e9f0f4", rock="#b7ccd8", stars=120, sun=None, bodies=[("neptune", 570, 100, 110)]),
}
def rng(seed): return random.Random(sum(ord(c) * (i + 1) for i, c in enumerate(seed)))
class Svg:
    def __init__(self, title, uid="s"): self.o = []; self.title = title; self.uid = uid; self.nclip = 0
    def add(self, s): self.o.append(s); return self
    def circ(self, x, y, r, fill, op=1, stroke=None, sw=1): return self.add(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}" fill="{fill}"' + (f' opacity="{op}"' if op != 1 else "") + (f' stroke="{stroke}" stroke-width="{sw}"' if stroke else "") + "/>")
    def ell(self, x, y, rx, ry, fill, op=1, rot=0): return self.add(f'<ellipse cx="{x:.1f}" cy="{y:.1f}" rx="{rx:.1f}" ry="{ry:.1f}" fill="{fill}"' + (f' opacity="{op}"' if op != 1 else "") + (f' transform="rotate({rot} {x:.1f} {y:.1f})"' if rot else "") + "/>")
    def poly(self, pts, fill, op=1): return self.add('<polygon points="' + " ".join(f"{x:.1f},{y:.1f}" for x, y in pts) + f'" fill="{fill}"' + (f' opacity="{op}"' if op != 1 else "") + "/>")
    def path(self, d, fill="none", stroke=None, sw=2, op=1, cap="round"): return self.add(f'<path d="{d}" fill="{fill}"' + (f' stroke="{stroke}" stroke-width="{sw}" stroke-linecap="{cap}" stroke-linejoin="round"' if stroke else "") + (f' opacity="{op}"' if op != 1 else "") + "/>")
    def rect(self, x, y, w, h, fill, op=1): return self.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{fill}"' + (f' opacity="{op}"' if op != 1 else "") + "/>")
    def text(self, x, y, s, size=13, fill="#ffffff", anchor="start", bold=False, op=1): return self.add(f'<text x="{x}" y="{y}" font-family="ui-sans-serif,system-ui,Segoe UI,Helvetica,Arial,sans-serif" font-size="{size}" fill="{fill}" text-anchor="{anchor}"' + (' font-weight="bold"' if bold else "") + (f' opacity="{op}"' if op != 1 else "") + f'>{str(s).replace("&", "&amp;").replace("<", "&lt;")}</text>')
    def clip(self, x, y, r):
        self.nclip += 1; cid = f"{self.uid}c{self.nclip}"; self.add(f'<clipPath id="{cid}"><circle cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}"/></clipPath>'); return cid
    def render(self): return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" role="img" aria-label="{self.title}"><defs><linearGradient id="{self.uid}sky" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="SKY0"/><stop offset="1" stop-color="SKY1"/></linearGradient></defs>' + "".join(self.o) + "</svg>"
# ------------------------------------------------------------------ celestial bodies
def body(s, kind, x, y, r):
    if kind == "sun": s.circ(x, y, r * 2.2, "#fff6d0", .18); s.circ(x, y, r, "#fff3b0")
    elif kind == "sunbig": s.circ(x, y, r * 1.8, "#ffd9a0", .25); s.circ(x, y, r, "#ffe9b0")
    elif kind == "sunsmall": s.circ(x, y, r * 1.8, "#ffb3a0", .25); s.circ(x, y, r, "#ffcfc0")
    elif kind == "moon": s.circ(x, y, r, "#e4e4e8"); [s.circ(x + dx * r, y + dy * r, cr * r, "#c4c4ca") for dx, dy, cr in ((-.3, -.2, .25), (.3, .25, .18), (.1, -.45, .12))]
    elif kind == "moonpurple": s.circ(x, y, r, "#d9b8f0"); s.circ(x + r * .3, y - r * .2, r * .22, "#b894d6")
    elif kind == "earth":
        s.circ(x, y, r, "#2f6fb5"); cid = s.clip(x, y, r); s.add(f'<g clip-path="url(#{cid})">')
        [s.ell(x + dx * r, y + dy * r, ew * r, eh * r, "#4f9a5b", rot=rot) for dx, dy, ew, eh, rot in ((-.25, -.2, .35, .22, -20), (.3, .25, .3, .2, 30), (.05, -.7, .3, .12, 0), (-.4, .5, .25, .15, 10))]; s.ell(x, y - r * .93, r * .5, r * .1, "#f4f9ff", .9); s.add("</g>"); s.circ(x, y, r, "none", stroke="#9fd0ff", sw=1.5, op=.7)
    elif kind in ("phobos", "deimos"): s.ell(x, y, r * 1.2, r, "#9a8a7e", rot=-15)
    elif kind == "jupiter":
        s.circ(x, y, r, "#d8b48a"); cid = s.clip(x, y, r); s.add(f'<g clip-path="url(#{cid})">')
        for k, c in enumerate(("#b98a5c", "#e8cfa8", "#a8744a", "#e2c399", "#b58452", "#d6ad7e")): s.rect(x - r, y - r + k * r * .36 + r * .12, 2 * r, r * .2, c, .6)
        s.ell(x + r * .3, y + r * .22, r * .16, r * .09, "#c0583a"); s.add("</g>"); s.circ(x, y, r, "none", stroke="#f5e6cf", sw=1.5, op=.4)
    elif kind == "saturn":
        s.circ(x, y, r, "#e7d3a1"); cid = s.clip(x, y, r); s.add(f'<g clip-path="url(#{cid})">')
        for k, c in enumerate(("#d2b87f", "#f0e0b8", "#c9ad73", "#efdcae")): s.rect(x - r, y - r * .6 + k * r * .33, 2 * r, r * .14, c, .55)
        s.add("</g>")
    elif kind == "rings": s.add(f'<ellipse cx="{x}" cy="{y}" rx="{r*1.9:.1f}" ry="{r*.42:.1f}" fill="none" stroke="#d9c48f" stroke-width="{r*.22:.1f}" opacity=".75" transform="rotate(-14 {x} {y})"/>'); s.add(f'<ellipse cx="{x}" cy="{y}" rx="{r*1.5:.1f}" ry="{r*.33:.1f}" fill="none" stroke="#b9a373" stroke-width="{r*.07:.1f}" opacity=".7" transform="rotate(-14 {x} {y})"/>')
    elif kind == "neptune": s.circ(x, y, r, "#3b6fd6"); s.ell(x + r * .2, y - r * .1, r * .22, r * .1, "#1f3f8c", .8); s.circ(x, y, r, "none", stroke="#8fb4ff", sw=1.5, op=.5)
    elif kind == "charon": s.circ(x, y, r, "#9a928c"); s.ell(x - r * .3, y - r * .5, r * .45, r * .22, "#6e5d57", .8)
    elif kind in ("io", "europa"): s.circ(x, y, r, "#f0d57a" if kind == "io" else "#e4eaf0"); s.circ(x - r * .2, y + r * .1, r * .3, "#d9a43a" if kind == "io" else "#c1a58f", .7)
# ------------------------------------------------------------------ the goat
def bez(p, t): return tuple((1 - t) ** 3 * p[0][k] + 3 * (1 - t) ** 2 * t * p[1][k] + 3 * (1 - t) * t * t * p[2][k] + t ** 3 * p[3][k] for k in (0, 1))
def tapered(s, pts, w0, w1, col, ridges=0, rcol="#2a2118", glow=None):
    """A horn drawn as many short segments whose width shrinks from w0 to w1, with transverse ridges (the growth rings that make an ibex's horn look knobbly)."""
    n = 36; prev = bez(pts, 0)
    for k in range(1, n + 1):
        t = k / n; cur = bez(pts, t); w = w0 + (w1 - w0) * t
        s.path(f"M{prev[0]:.1f},{prev[1]:.1f} L{cur[0]:.1f},{cur[1]:.1f}", stroke=col, sw=w, cap="round")
        if ridges and k % ridges == 0 and k < n - 1:
            dx, dy = cur[0] - prev[0], cur[1] - prev[1]; L = math.hypot(dx, dy) or 1; nx, ny = -dy / L, dx / L
            s.path(f"M{cur[0]-nx*w*.55:.1f},{cur[1]-ny*w*.55:.1f} L{cur[0]+nx*w*.55:.1f},{cur[1]+ny*w*.55:.1f}", stroke=rcol, sw=1.6, op=.55, cap="butt")
        prev = cur
    if glow: s.path("M" + " L".join(f"{bez(pts, t / 24)[0]:.1f},{bez(pts, t / 24)[1] - 1.5:.1f}" for t in range(25)), stroke=glow, sw=1.3, op=.35)
def horns(s, style, hx, hy, col):
    h = lambda *p: tuple((hx + a, hy + b) for a, b in p)
    if style == "scimitar":      # the ibex: a huge arc up and back, then down, ringed along its length
        tapered(s, h((0, 0), (-18, -78), (-112, -72), (-104, -2)), 11, 4, col, ridges=2, glow="#f0e6cc")
        tapered(s, h((6, 2), (-10, -64), (-92, -62), (-90, 6)), 8, 3, "#8a7a5a", ridges=3, rcol="#1e160e")
    elif style == "spiral":      # markhor: a corkscrew, two strands wound round each other, leaning back
        for ph, c, w in ((0, col, 6.5), (math.pi, "#a28d66", 5)):
            pts = [(hx + 2 - 10 * math.sin(t * 2 * math.pi * 2.3 + ph) - t * 36, hy - 66 * t) for t in [i / 48 for i in range(49)]]
            s.path("M" + " L".join(f"{x:.1f},{y:.1f}" for x, y in pts), stroke=c, sw=w)
    elif style == "sickle":      # the Nubian ibex: a smooth, ridged sickle
        tapered(s, h((0, 0), (-16, -60), (-82, -62), (-86, -14)), 10, 3.5, col, ridges=2, glow="#e9dcc0")
    elif style == "heavy":       # the Walia: thick and knobbed, curving back
        tapered(s, h((0, 0), (-14, -46), (-60, -58), (-76, -28)), 13, 6, col, ridges=2, rcol="#8d7a60")
    elif style == "lyre":        # the Iberian ibex: out, then up and inward like a lyre
        tapered(s, h((0, 0), (14, -26), (38, -44), (20, -72)), 8, 4, col, ridges=3); tapered(s, h((-8, 0), (-26, -28), (-48, -40), (-34, -68)), 8, 4, "#8b7a58", ridges=3)
    elif style == "tur":         # the tur: short, thick, swept outward and back
        tapered(s, h((0, 0), (-6, -30), (-34, -42), (-52, -26)), 13, 6, col, ridges=3, rcol="#6b5a46")
    elif style == "bezoar":      # the wild goat: a keeled scimitar, shorter than the ibex's
        tapered(s, h((0, 0), (-10, -50), (-60, -66), (-82, -30)), 9, 3.5, col, ridges=4, rcol="#6b5a46")
    elif style == "short":
        tapered(s, h((0, 0), (-2, -16), (-12, -24), (-22, -18)), 7, 3.5, col)
def goat(s, key, x, y, scale=1.0, flip=False, pose="stand"):
    sp = SPECIES[key]; k = scale * sp["size"]; coat, belly = sp["coat"], sp["belly"]; dark = "#24190f"; i = Svg("")
    leg = lib_dark(coat, .52); s.nclip += 1; gid = f"{s.uid}g{s.nclip}"
    s.add(f'<defs><linearGradient id="{gid}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="{lib_light(coat, .18)}"/><stop offset=".55" stop-color="{coat}"/><stop offset="1" stop-color="{lib_dark(coat, .78)}"/></linearGradient></defs>')
    fur = f"url(#{gid})"; sock = key in ("alpine", "siberian", "nubian", "iberian", "bezoar")
    def leg_path(x0, y0, x1, y1, w=7.5, far=False):
        c = lib_dark(leg, .8) if far else leg
        i.path(f"M{x0},{y0} L{x1},{y1}", stroke=c, sw=w); 
        if sock: i.path(f"M{x1 - (x1-x0)*.28:.1f},{y1 - (y1-y0)*.28:.1f} L{x1},{y1 - 2}", stroke="#efe9dd", sw=w + .5)
        i.ell(x1 + 1, y1, 5.2, 3.4, dark)
    if pose == "stand":
        leg_path(-34, -48, -34, -3, far=True); leg_path(42, -48, 42, -3, far=True)           # far legs
        i.ell(0, -66, 55, 28, fur); i.ell(4, -52, 46, 14, belly, .85); i.path("M-50,-84 C -20,-96 20,-96 48,-86", stroke=lib_dark(coat, .7), sw=5, op=.55)   # body, belly, dark back stripe
        i.path("M-54,-72 C -64,-80 -64,-92 -58,-94", stroke=coat, sw=6)                       # tail
        i.poly([(38, -84), (60, -116), (78, -106), (62, -66)], fur); [i.path(f"M{44+a*5},{-90+a*3} l-5,9", stroke=lib_dark(coat, .75), sw=1.2, op=.5) for a in range(5)]   # neck with fur strokes
        hx, hy = 78, -108
        leg_path(-42, -46, -43, -3); leg_path(34, -46, 35, -3)
    else:                                                                                      # resting: haunches down, forelegs stretched forward, head up
        i.ell(-6, -34, 56, 25, fur); i.ell(-2, -22, 46, 11, belly, .8); i.path("M-56,-38 C -66,-46 -66,-56 -60,-58", stroke=coat, sw=6)
        i.ell(-34, -18, 26, 14, lib_dark(coat, .85)); leg_path(8, -28, 66, -8, w=8); leg_path(24, -22, 82, -6, w=8, far=True)
        i.poly([(30, -48), (56, -92), (76, -84), (58, -34)], fur)
        hx, hy = 78, -88
    i.ell(hx, hy, 19, 12, fur, rot=-18); i.ell(hx + 16, hy + 8, 10, 7, "#d8c9b2", rot=-18); i.circ(hx + 24, hy + 5, 2.4, "#1b1410")          # head, light muzzle, nose
    i.ell(hx - 11, hy - 4, 11, 4.6, coat, rot=38); i.ell(hx - 11, hy - 3, 7, 2.4, "#6d5742", .8, rot=38)                                       # ear
    i.circ(hx + 3, hy - 3, 2.3, "#17110c"); i.path(f"M{hx-1},{hy-6} L{hx+7},{hy-5}", stroke="#2a2118", sw=1.2, op=.6)                         # eye with lid
    i.poly([(hx + 9, hy + 11), (hx + 16, hy + 11), (hx + 12, hy + 38), (hx + 8, hy + 36)], "#2b2017", .95)                                     # a long dark beard
    if key in ("alpine", "siberian", "markhor"): i.poly([(hx - 14, hy + 2), (hx - 2, hy + 18), (hx + 8, hy + 14), (hx - 6, hy - 2)], "#e6dfd0", .55)
    horns(i, sp["style"], hx - 8, hy - 5, sp["horn"])
    s.add(f'<g transform="translate({x},{y}) scale({-k if flip else k},{k})">' + "".join(i.o) + "</g>"); return s
def _mix(c, t, target):
    c = c.lstrip("#"); r, g, b = (int(c[a:a + 2], 16) for a in (0, 2, 4)); return "#%02x%02x%02x" % tuple(int(v + (target - v) * t) for v in (r, g, b))
def lib_dark(c, f): return _mix(c, 1 - f, 0)
def lib_light(c, t): return _mix(c, t, 255)
# ------------------------------------------------------------------ a whole scene
def tinted(key, haze_color, t):
    """A copy of a species' colours blended toward the haze, for animals standing farther back."""
    sp = dict(SPECIES[key])
    for f in ("coat", "belly", "horn"): sp[f] = _mix(_mix(sp[f], t, int(haze_color[1:3], 16)), 0, 0) if False else sp[f]
    return sp
def scene(world, seed, cast, scale=1.0):
    """cast: a list of species keys (the first is the 'lead'); they stand in two rows on a rocky ridge: the larger front row, a smaller hazier back row."""
    w = WORLDS[world]; r = rng(seed); n = len(cast); s = Svg(" and ".join(SPECIES[c]["name"] for c in cast[:3]) + f" on {w['name']}", uid="u" + "".join(c for c in seed if c.isalnum()))
    s.add(f'<rect x="0" y="0" width="{W}" height="{H}" fill="url(#{s.uid}sky)"/>')
    for _ in range(w["stars"]): s.circ(r.uniform(0, W), r.uniform(0, 175), r.choice((.6, .9, 1.3)), "#ffffff", r.uniform(.4, 1))
    for kind, bx, by, br in w["bodies"]: body(s, kind, bx, by, br)
    def ridge(base, amp, fill, cnt, seed2, op=1):
        rr = rng(seed + seed2); pts = [(0, H)]
        for k in range(cnt + 1): pts.append((k * W / cnt, base - amp * rr.random() ** .8 - (amp * .5 if k % 3 == 1 else 0)))
        pts.append((W, H)); s.poly(pts, fill, op)
    ridge(205, 70, w["far"], 11, "a", .95); ridge(228, 45, w["ridge"], 9, "b"); s.rect(0, 252, W, 48, w["ground"])
    rr = rng(seed + "r")
    for _ in range(9): s.ell(rr.uniform(20, 780), rr.uniform(262, 294), rr.uniform(8, 22), rr.uniform(3, 7), w["rock"], .8)
    back = cast[1::2]; front = [cast[0]] + cast[2::2]; nf = len(front)
    xf = [150 + (650 - 150) * j / max(1, nf - 1) for j in range(nf)] if nf > 1 else [400]
    gaps = [(xf[j] + xf[j + 1]) / 2 for j in range(nf - 1)] + [60, 740]; xb = sorted(gaps)[:0] or ([60] + gaps[:-2] + [740])[:len(back)] if False else None
    cand = [(xf[j] + xf[j + 1]) / 2 for j in range(nf - 1)] + [95, 705]; cand = sorted(cand)
    xb = (cand if len(back) >= len(cand) else [cand[min(len(cand) - 1, int(k * len(cand) / max(1, len(back))))] for k in range(len(back))])[:len(back)]
    for j, (key, x) in enumerate(zip(back, xb)):                 # back row: smaller, hazier, standing on the far ridge, drawn first
        goat(s, key, x + rr.uniform(-10, 10), 238, .6, flip=(j % 2 == 1)); s.ell(x, 232, 60, 26, w["ridge"], .2)
    for j, (key, x) in enumerate(zip(front, xf)):
        x += rr.uniform(-12, 12); y = 266 + (j % 2) * 3
        s.poly([(x - 85, y + 6), (x - 56, y - 16), (x - 10, y - 26), (x + 44, y - 20), (x + 90, y - 2), (x + 100, y + 8)], w["rock"]); s.poly([(x - 10, y - 26), (x + 44, y - 20), (x + 64, y - 8), (x + 14, y - 12)], "#ffffff", .12)
        goat(s, key, x, y - 22, scale * .88, flip=(j % 2 == 1))
    names = [SPECIES[c]["name"] for c in cast]; seen = []; [seen.append(x) for x in names if x not in seen]
    s.rect(0, 0, W, 34, "#000000", .35); s.text(16, 23, " · ".join(seen[:4]) + (" · …" if len(seen) > 4 else ""), 14, "#ffffff", bold=True); s.text(W - 16, 23, f"on {w['name']}", 14, "#e8e8f0", "end")
    return s.render().replace("SKY0", w["sky"][0]).replace("SKY1", w["sky"][1])
