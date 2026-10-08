#!/usr/bin/env python3
"""The Capra logo: a front view of an ibex head with long, curved, ringed horns. Writes docs/assets/capra-logo.svg (white on transparent, for the indigo header) and docs/assets/capra-favicon.svg (white on an indigo disc)."""
import math, os
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "docs", "assets")
BG = "#3f51b5"
def bez(p0, p1, p2, p3, t):
    u = 1 - t; return tuple(u**3 * a + 3 * u * u * t * b + 3 * u * t * t * c + t**3 * d for a, b, c, d in zip(p0, p1, p2, p3))
def horn(sign, rings=True):
    # one horn as two cubic segments: up and out from the forehead, then sweeping back in at the tip (a lyre curve); sign -1 = left
    S = lambda x, y: (32 + sign * (x - 32), y)
    segs = [(S(37, 26), S(47, 22), S(54, 14), S(53, 6)), (S(53, 6), S(52.5, 2.5), S(49, 1.2), S(45.5, 3.2))]
    d = "M%.1f,%.1f " % segs[0][0] + " ".join("C%.1f,%.1f %.1f,%.1f %.1f,%.1f" % (*b[1], *b[2], *b[3]) for b in segs)
    ticks = []
    if rings:
        pts = [bez(*segs[0], t / 6) for t in range(1, 6)]
        for k, (x, y) in enumerate(pts):
            a = bez(*segs[0], (k + 0.9) / 6); b = bez(*segs[0], (k + 1.1) / 6); ang = math.atan2(b[1] - a[1], b[0] - a[0]) + math.pi / 2
            w = 3.0 - 0.3 * k; ticks.append(f'<line x1="{x - w*math.cos(ang):.1f}" y1="{y - w*math.sin(ang):.1f}" x2="{x + w*math.cos(ang):.1f}" y2="{y + w*math.sin(ang):.1f}" stroke="{BG}" stroke-width="1.1"/>')
    return d, ticks
def svg(fill, bg, disc):
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" role="img" aria-label="Capra">']
    if disc: parts.append(f'<circle cx="32" cy="32" r="32" fill="{bg}"/>')
    for sign in (-1, 1):
        d, ticks = horn(sign); parts.append(f'<path d="{d}" fill="none" stroke="{fill}" stroke-width="4.6" stroke-linecap="round" stroke-linejoin="round"/>'); parts += ticks
    parts.append(f'<ellipse cx="21" cy="33" rx="7.5" ry="3.2" transform="rotate(-25 21 33)" fill="{fill}"/><ellipse cx="43" cy="33" rx="7.5" ry="3.2" transform="rotate(25 43 33)" fill="{fill}"/>')   # ears
    parts.append(f'<path d="M25,27 C25,25 39,25 39,27 L37.5,44 C36.5,52 34.5,55 32,55.5 C29.5,55 27.5,52 26.5,44 Z" fill="{fill}"/>')                                                              # head
    parts.append(f'<path d="M29,54 L35,54 L32,63 Z" fill="{fill}"/>')                                                                                                                                  # beard
    parts.append(f'<ellipse cx="28.3" cy="35" rx="1.5" ry="1.1" fill="{bg}"/><ellipse cx="35.7" cy="35" rx="1.5" ry="1.1" fill="{bg}"/><ellipse cx="30.4" cy="52.2" rx="0.9" ry="1.2" fill="{bg}"/><ellipse cx="33.6" cy="52.2" rx="0.9" ry="1.2" fill="{bg}"/>')   # eyes, nostrils
    parts.append("</svg>"); return "".join(parts)
open(os.path.join(OUT, "capra-logo.svg"), "w").write(svg("#ffffff", BG, False)); open(os.path.join(OUT, "capra-favicon.svg"), "w").write(svg("#ffffff", BG, True)); print("logo written")
