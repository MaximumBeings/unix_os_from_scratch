#!/usr/bin/env python3
"""Puts each chapter's picture under its page's title as a MARKDOWN image (mkdocs rewrites its path for the page's final URL; a raw <img> with a relative path is not rewritten and breaks). Chapters get
assets/art/ch-NN.svg, the appendices appx-a.svg to appx-d.svg. Files are read and written with their line endings untouched (a chapter contains literal carriage returns). Running it again only refreshes the alt text from the picture's own <title>."""
import glob, os, re
root = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
STYLE = '{ style="display:block;margin:0 auto;max-width:100%;height:auto;border-radius:6px" }'
def alt(svg): return re.search(r"<title[^>]*>(.*?)</title>", open(svg).read()).group(1)
def add(md, name):
    text = open(md, newline="").read(); svg = os.path.join(root, "assets", "art", name + ".svg"); a = alt(svg)
    if "assets/art/" in text:
        new = re.sub(r"!\[[^\]]*\]\(([./]*assets/art/" + name + r"\.svg)\)", lambda m: f"![{a}]({m.group(1)})", text)
        if new != text: open(md, "w", newline="").write(new)
        return 0
    lines = text.split("\n"); i = next(k for k, l in enumerate(lines) if l.startswith("# "))
    lines[i + 1:i + 1] = ["", f"![{a}](../assets/art/{name}.svg){STYLE}"]; open(md, "w", newline="").write("\n".join(lines)); return 1
n = 0
for md in sorted(glob.glob(os.path.join(root, "part*", "*.md"))):
    m = re.match(r"(\d+)-", os.path.basename(md))
    if m: n += add(md, f"ch-{int(m.group(1)):02d}")
for L in "abcd": n += add(glob.glob(os.path.join(root, "appendix" + L.upper(), "*.md"))[0], "appx-" + L)
print("added to", n, "pages")
