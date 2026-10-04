#!/usr/bin/env python3
"""Chapter 47: turns the full serial capture (about 1300 lines, because every earlier chapter's demo runs first) into the excerpt the chapter page shows: the first 19 lines (entry, memory map, paging),
an explicit elision marker, then this chapter's own marketplace demo from its first line to its last, with two kinds of noise folded away and COUNTED: the periodic `tick:` lines of the timer interrupt,
and runs of consecutive `fat16_*` driver trace lines (the filesystem driver narrates every file it creates, reads or deletes). Usage: make_excerpt.py SERIAL_TXT > serial_excerpt_out.txt"""
import re, sys
L = open(sys.argv[1], errors="replace").read().split("\n"); start = next(i for i, l in enumerate(L) if "Starting this chapter's own marketplace demo" in l)
out = L[:19] + [f"...[Chapters 8-46's own output, lines 20-{start}, unchanged in kind from Chapter 46's page]...", ""]
ticks = 0; fat = 0; seg = L[start:]
def flush():
    global fat
    if fat: out.append(f"    [... {fat} lines of fat16_* driver trace omitted ...]"); fat = 0
for l in seg:
    if re.match(r"tick: \d+$", l): ticks += 1; continue
    if l.startswith("fat16_"): fat += 1; continue
    flush(); out.append(l)
flush()
print("\n".join(out)); print(f"\n[the excerpt omits {ticks} `tick:` lines printed by the timer interrupt while the demo ran]", file=sys.stderr)
