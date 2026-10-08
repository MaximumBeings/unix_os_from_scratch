#!/usr/bin/env python3
"""Draw a VCD waveform file as text. Usage: vcd_ascii.py FILE.vcd T0 T1 [signal ...]   (times in the VCD's time unit; signals by full name such as ripple_tb.sum, default all of the top scope)
One character per time step; a 1-bit signal is drawn as _ and #, a bus as its hexadecimal value, held until it changes. No GUI needed."""
import re, sys
def parse(path):
    sig = {}; scope = []; ids = {}; cur = 0; events = []
    for line in open(path):
        t = line.split()
        if not t: continue
        if t[0] == "$scope": scope.append(t[2])
        elif t[0] == "$upscope": scope.pop()
        elif t[0] == "$var": name = ".".join(scope + [t[4]]); ids.setdefault(t[3], []).append(name); sig[name] = {"w": int(t[2]), "ev": []}
        elif t[0].startswith("#"): cur = int(t[0][1:])
        elif t[0][0] in "01xz" and len(t[0]) > 1 and not t[0].startswith("$"):
            for n in ids.get(t[0][1:], []): sig[n]["ev"].append((cur, t[0][0]))
        elif t[0][0] == "b":
            for n in ids.get(t[1], []): sig[n]["ev"].append((cur, t[0][1:]))
    return sig
def value_at(ev, t):
    v = "x"
    for tt, val in ev:
        if tt <= t: v = val
        else: break
    return v
def draw(path, t0, t1, names=None):
    sig = parse(path); names = names or [n for n in sig if n.count(".") == 1]; width = max(len(n) for n in names); out = []
    out.append(" " * (width + 2) + "".join(str((t // 10) % 10) if t % 10 == 0 else " " for t in range(t0, t1)) + "   (time, tens)")
    for n in names:
        w = sig[n]["w"]; row = ""; prev = None
        for t in range(t0, t1):
            v = value_at(sig[n]["ev"], t)
            if w == 1: row += {"0": "_", "1": "#", "x": "?", "z": "?"}[v]
            else:
                h = (format(int(v, 2), "X") if set(v) <= set("01") else "?"); row += (h if v != prev else "=")
            prev = v
        out.append(f"{n:{width}s}  {row}")
    return "\n".join(out)
if __name__ == "__main__": print(draw(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4:] or None))
