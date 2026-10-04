"""Drawing helpers for the chapter pictures of the OS book: flat vector scenes on an 800 x 300 canvas, written as SVG text. Each scene is a function of a palette that draws the chapter's own subject
(a clock for the timer chapter, a bank for the wire-transfer chapter, an umbrella for insurance, ...). Nothing here is random: the same script always writes the same pictures."""
W, H = 800, 300
PALETTES = {   # (background top, background bottom, main, accent, light, dark)
    "hw":      ("#0d1b2a", "#1b3a5c", "#3fa7d6", "#f2c14e", "#e7f1fa", "#08121c"),
    "screen":  ("#101820", "#1d2b3a", "#59c3c3", "#f45b69", "#ebf5f5", "#0a0f14"),
    "irq":     ("#1d1230", "#3a1d5e", "#b388ff", "#ffd166", "#f2e9ff", "#120a1f"),
    "time":    ("#2b1d0e", "#5a3a14", "#f4a259", "#5bc0be", "#fdf0e0", "#1a1007"),
    "mem":     ("#14213d", "#2a3f6e", "#8ecae6", "#ffb703", "#eaf6fb", "#0b1226"),
    "task":    ("#2d1b0e", "#6b3a12", "#ff9f1c", "#2ec4b6", "#fff1dc", "#1a0f06"),
    "lock":    ("#2a1015", "#5c1a24", "#ef476f", "#ffd166", "#ffe8ec", "#16080b"),
    "ring":    ("#1a0f2e", "#2f1b52", "#c77dff", "#80ffdb", "#f3e8ff", "#0e0719"),
    "disk":    ("#10261c", "#1d4a37", "#52b788", "#ffd166", "#e6f6ee", "#08150f"),
    "net":     ("#0a1f3d", "#134e8e", "#4cc9f0", "#f72585", "#e6f7fd", "#06142a"),
    "bank":    ("#0b2a1a", "#14532d", "#4ade80", "#fbbf24", "#e8fbef", "#06170e"),
    "ins":     ("#0b2a30", "#145a66", "#2dd4bf", "#fb7185", "#e6fbf8", "#061a1e"),
    "invest":  ("#1f1a07", "#4a3d0c", "#facc15", "#34d399", "#fef9d7", "#120f03"),
    "media":   ("#1f0a2b", "#4a1470", "#e879f9", "#38bdf8", "#fbeaff", "#12051a"),
    "atm":     ("#0f1f2e", "#1f4b6b", "#7dd3fc", "#86efac", "#e8f6fe", "#08121b"),
    "travel":  ("#0b2540", "#1e6fa8", "#93c5fd", "#fde047", "#eef6ff", "#071829"),
    "sport":   ("#0f2a14", "#1f6b2e", "#86efac", "#fb923c", "#eafaee", "#08170c"),
    "bet":     ("#2a0f0f", "#6b1f1f", "#f87171", "#fcd34d", "#ffecec", "#160707"),
    "car":     ("#1f2937", "#475569", "#38bdf8", "#f97316", "#f1f5f9", "#0f141b"),
    "auction": ("#2a1608", "#6b3a0f", "#fbbf24", "#60a5fa", "#fff3dc", "#170c04"),
    "edgar":   ("#0a1a2f", "#1d3b63", "#7dd3fc", "#fbbf24", "#eaf4ff", "#050d18"),
    "health":  ("#082a2a", "#0f5a5a", "#5eead4", "#fb7185", "#e6fffb", "#041717"),
    "ref":     ("#1c1917", "#44403c", "#fbbf24", "#a3e635", "#faf5eb", "#0f0d0c"),
}
class Svg:
    def __init__(self, key, title, label_n=None):
        self.p = PALETTES[key]; self.o = []; self.title = title
    @property
    def bg1(self): return self.p[0]
    @property
    def main(self): return self.p[2]
    @property
    def acc(self): return self.p[3]
    @property
    def light(self): return self.p[4]
    @property
    def dark(self): return self.p[5]
    def add(self, s): self.o.append(s); return self
    def rect(self, x, y, w, h, fill=None, stroke=None, sw=2, rx=6, op=1): return self.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill or "none"}"' + (f' stroke="{stroke}" stroke-width="{sw}"' if stroke else "") + (f' opacity="{op}"' if op != 1 else "") + "/>")
    def circ(self, x, y, r, fill=None, stroke=None, sw=2, op=1): return self.add(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{fill or "none"}"' + (f' stroke="{stroke}" stroke-width="{sw}"' if stroke else "") + (f' opacity="{op}"' if op != 1 else "") + "/>")
    def line(self, x1, y1, x2, y2, c=None, sw=2, dash=None, op=1): return self.add(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{c or self.light}" stroke-width="{sw}" stroke-linecap="round"' + (f' stroke-dasharray="{dash}"' if dash else "") + (f' opacity="{op}"' if op != 1 else "") + "/>")
    def poly(self, pts, fill=None, stroke=None, sw=2, op=1): return self.add('<polygon points="' + " ".join(f"{x},{y}" for x, y in pts) + f'" fill="{fill or "none"}"' + (f' stroke="{stroke}" stroke-width="{sw}" stroke-linejoin="round"' if stroke else "") + (f' opacity="{op}"' if op != 1 else "") + "/>")
    def path(self, d, fill=None, stroke=None, sw=2, op=1): return self.add(f'<path d="{d}" fill="{fill or "none"}"' + (f' stroke="{stroke}" stroke-width="{sw}" stroke-linecap="round" stroke-linejoin="round"' if stroke else "") + (f' opacity="{op}"' if op != 1 else "") + "/>")
    def text(self, x, y, s, size=16, fill=None, anchor="middle", bold=False, op=1): return self.add(f'<text x="{x}" y="{y}" font-family="ui-monospace,Menlo,Consolas,monospace" font-size="{size}" fill="{fill or self.light}" text-anchor="{anchor}"' + (' font-weight="bold"' if bold else "") + (f' opacity="{op}"' if op != 1 else "") + f'>{str(s).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")}</text>')
    def arrow(self, x1, y1, x2, y2, c=None, sw=3):
        import math
        c = c or self.acc; a = math.atan2(y2 - y1, x2 - x1); L = 11
        self.line(x1, y1, x2, y2, c, sw)
        return self.poly([(x2, y2), (x2 - L * math.cos(a - .4), y2 - L * math.sin(a - .4)), (x2 - L * math.cos(a + .4), y2 - L * math.sin(a + .4))], fill=c)
    # ---- reusable objects (x, y is the top-left corner unless said otherwise)
    def chip(self, x, y, s=70, label="CPU"):
        for k in range(1, 5):
            t = x + k * s / 5
            self.line(t, y - 9, t, y, self.light, 3); self.line(t, y + s, t, y + s + 9, self.light, 3); self.line(x - 9, y + k * s / 5, x, y + k * s / 5, self.light, 3); self.line(x + s, y + k * s / 5, x + s + 9, y + k * s / 5, self.light, 3)
        self.rect(x, y, s, s, self.dark, self.main, 3, 5); return self.text(x + s / 2, y + s / 2 + 5, label, max(11, s // 6), self.main, bold=True)
    def monitor(self, x, y, w=140, h=100, screen=None):
        self.rect(x, y, w, h, self.dark, self.light, 4, 8); self.rect(x + 8, y + 8, w - 16, h - 16, screen or "#0b1020", None, 0, 3)
        self.rect(x + w / 2 - 10, y + h, 20, 14, self.light, None, 0, 2); return self.rect(x + w / 2 - 35, y + h + 14, 70, 7, self.light, None, 0, 3)
    def folder(self, x, y, w=70, h=52, fill=None, label=None):
        f = fill or self.acc; self.path(f"M{x},{y + 8} h{w * .38} l8,-8 h{w * .62 - 8} v{h} h-{w} z", fill=f, stroke=self.dark, sw=2)
        if label: self.text(x + w / 2, y + h / 2 + 8, label, 11, self.dark, bold=True)
        return self
    def file(self, x, y, w=44, h=56, label=None, fill=None):
        self.path(f"M{x},{y} h{w - 12} l12,12 v{h - 12} h-{w} z", fill=fill or self.light, stroke=self.dark, sw=2)
        for k in range(3): self.line(x + 8, y + 22 + k * 9, x + w - 8, y + 22 + k * 9, self.dark, 1.5, op=.5)
        return self.text(x + w / 2, y + h + 14, label, 11, self.light) if label else self
    def packet(self, x, y, w=110, h=44, label="IP", fill=None):
        self.rect(x, y, w, h, fill or self.main, self.light, 2, 6); self.rect(x, y, w * .3, h, self.acc, None, 0, 6); return self.text(x + w * .15, y + h / 2 + 5, label, 13, self.dark, bold=True)
    def cloud(self, x, y, s=1.0, fill=None):
        f = fill or self.light
        for dx, dy, r in ((0, 0, 22), (26, -10, 28), (56, 0, 22), (28, 10, 26)): self.circ(x + dx * s, y + dy * s, r * s, f, op=.9)
        return self
    def server(self, x, y, w=90, h=110):
        self.rect(x, y, w, h, self.dark, self.main, 3, 6)
        for k in range(3): self.rect(x + 8, y + 10 + k * 32, w - 16, 22, "#0b1626", self.light, 1.5, 3); self.circ(x + w - 20, y + 21 + k * 32, 4, self.acc if k != 1 else "#6ee7b7")
        return self
    def bank(self, x, y, s=1.0):
        w = 150 * s; self.poly([(x, y + 40 * s), (x + w / 2, y), (x + w, y + 40 * s)], self.light, self.dark, 2)
        for k in range(5): self.rect(x + 14 * s + k * 26 * s, y + 48 * s, 14 * s, 62 * s, self.light, self.dark, 1.5, 2)
        self.rect(x - 6 * s, y + 112 * s, w + 12 * s, 14 * s, self.light, self.dark, 2, 2); self.rect(x - 14 * s, y + 126 * s, w + 28 * s, 12 * s, self.main, self.dark, 2, 2)
        return self.circ(x + w / 2, y + 22 * s, 8 * s, self.acc, self.dark, 1.5)
    def coin(self, x, y, r=18, sym="$"):
        self.circ(x, y, r, self.acc, self.dark, 2.5); self.circ(x, y, r * .72, None, self.dark, 1.5, op=.6); return self.text(x, y + r * .33, sym, r * 0.95, self.dark, bold=True)
    def lockicon(self, x, y, s=1.0, open_=False):
        self.rect(x, y + 22 * s, 44 * s, 34 * s, self.acc, self.dark, 2.5, 5)
        self.path(f"M{x + 9 * s},{y + 22 * s} v-{10 * s} a{13 * s},{13 * s} 0 0 1 {26 * s},0 v{10 * s}" if not open_ else f"M{x + 9 * s},{y + 22 * s} v-{14 * s} a{13 * s},{13 * s} 0 0 1 {26 * s},0 v{2 * s}", stroke=self.light, sw=5 * s)
        return self.circ(x + 22 * s, y + 38 * s, 5 * s, self.dark)
    def grid(self, x, y, cols, rows, cw, ch, gap=3, fills=None, stroke=None):
        for r in range(rows):
            for c in range(cols): self.rect(x + c * (cw + gap), y + r * (ch + gap), cw, ch, (fills(r, c) if fills else self.main), stroke or self.dark, 1.5, 2)
        return self
    def svg(self):
        defs = (f'<defs><linearGradient id="bg" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="{self.p[0]}"/><stop offset="1" stop-color="{self.p[1]}"/></linearGradient>'
                f'<pattern id="dots" width="26" height="26" patternUnits="userSpaceOnUse"><circle cx="2" cy="2" r="1.2" fill="{self.p[4]}" opacity=".14"/></pattern></defs>')
        return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" role="img" aria-labelledby="t"><title id="t">{self.title}</title>{defs}'
                f'<rect width="{W}" height="{H}" fill="url(#bg)"/><rect width="{W}" height="{H}" fill="url(#dots)"/>' + "".join(self.o) + "</svg>\n")
