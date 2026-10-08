#!/usr/bin/env python3
"""Chapter 10: the synthesis flow and a small static timing analyzer.
  synth(files, top, tag, memories=())   run Yosys: read the RTL (memories are kept as black boxes, as a chip uses a memory compiler's macros), elaborate, optimize, map to the toy cell library (lib/toy.lib),
                                        write out/<tag>_net.v (a Verilog netlist of toy cells) and out/<tag>_net.json; returns area, cell counts and the log.
  sta(json_path, top)                   the longest register-to-register (or input/output) path through the netlist, in ns, using the library's delays and these macro assumptions: a synchronous memory's data output appears
                                        MACRO_CLK_TO_Q after the clock; an asynchronous memory read takes MACRO_READ from address to data; everything entering a memory or a flip-flop must arrive SETUP before the clock.
The analyzer is ~60 lines: it adds up cell delays along the longest path, ignoring wires, fan-out and clock skew. It is a model of timing, not a signoff tool."""
import json, os, re, subprocess, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SETUP, MACRO_CLK_TO_Q, MACRO_READ, DFF_CLK_TO_Q = 0.10, 0.50, 0.40, 0.30
def lib_delays():
    txt = open(os.path.join(ROOT, "lib", "toy.lib")).read(); d = {}
    for m in re.finditer(r"cell\((\w+)\) \{(.*?)\n  \}", txt, re.S):
        v = re.search(r'cell_rise\(scalar\) \{ values\("([\d.]+)"\)', m.group(2)); d[m.group(1)] = float(v.group(1)) if v else None
    return d
def synth(files, top, tag, memories=(), lib="lib/toy.lib"):
    out = os.path.join(ROOT, "out"); script = []
    if memories: script.append("read_verilog -sv -lib " + " ".join(memories))
    script += ["read_verilog -sv " + " ".join(files), f"hierarchy -top {top}", f"synth -flatten -top {top}", "dfflegalize -cell $_DFF_P_ x", f"dfflibmap -liberty {lib}", f"abc -liberty {lib}", "opt_clean -purge",
               f"tee -o out/{tag}_stat.txt stat -liberty {lib}", f"write_verilog -noattr out/{tag}_net.v", f"write_json out/{tag}_net.json"]
    p = subprocess.run(["yosys", "-q", "-l", f"out/{tag}_synth.log", "-p", "; ".join(script)], cwd=ROOT, capture_output=True, text=True)
    if p.returncode != 0: return {"ok": False, "log": p.stdout + p.stderr}
    stat = open(os.path.join(out, f"{tag}_stat.txt")).read()
    cells = {m.group(1): int(m.group(2)) for m in re.finditer(r"^\s+(\w+)\s+(\d+)\s*$", stat.split("Number of cells")[1], re.M)} if "Number of cells" in stat else {}
    area = re.search(r"Chip area for (?:top )?module '\\?\w+': ([\d.]+)", stat)
    return {"ok": True, "area": float(area.group(1)) if area else None, "cells": cells, "ncells": sum(cells.values()), "stat": stat}
def sta(json_path, top):
    delays = lib_delays(); mod = json.load(open(json_path))["modules"][top]; ports = mod["ports"]; cells = mod["cells"]
    driver = {}; endpoints = []          # bit -> (cell name, delay, [input bits])  ; endpoints: (bit, extra)
    arrivals = {}
    for name, c in cells.items():
        t = c["type"]; dirs = c.get("port_directions", {}); conn = c["connections"]
        if t == "DFF":
            driver[conn["Q"][0]] = (name, DFF_CLK_TO_Q, []); endpoints.append((conn["D"][0], name + ".D"))
        elif t in delays and delays[t] is not None:
            ins = [b for pn, bits in conn.items() if pn != "Y" for b in bits]
            for b in conn["Y"]: driver[b] = (name, delays[t], ins)
        else:                            # a memory macro (black box)
            outs = [pn for pn, dr in dirs.items() if dr == "output"]; ins = [pn for pn, dr in dirs.items() if dr == "input"]
            if t == "rowmem":
                ra = [b for b in conn["raddr"]]
                for b in conn["rdata"]: driver[b] = (name + ".read", MACRO_READ, ra)
                for pn in ("we", "waddr", "wdata"): endpoints += [(b, name + "." + pn) for b in conn[pn]]
            else:
                for pn in outs:
                    for b in conn[pn]: driver[b] = (name + "." + pn, MACRO_CLK_TO_Q, [])
                endpoints += [(b, name + "." + pn) for pn in ins if pn not in ("clk",) for b in conn[pn]]
    for pn, p in ports.items():
        if p["direction"] == "output": endpoints += [(b, "output " + pn) for b in p["bits"]]
    sys.setrecursionlimit(1000000); pred = {}
    def arr(b):
        if not isinstance(b, int): return 0.0
        if b in arrivals: return arrivals[b]
        if b not in driver: arrivals[b] = 0.0; return 0.0           # a primary input
        nm, d, ins = driver[b]; best = 0.0; bp = None
        for i in ins:
            a = arr(i)
            if a >= best: best, bp = a, i
        arrivals[b] = best + d; pred[b] = (nm, bp); return arrivals[b]
    worst, wb, wname = -1.0, None, ""
    for b, nm in endpoints:
        if isinstance(b, int):
            a = arr(b)
            if a > worst: worst, wb, wname = a, b, nm
    path = []; b = wb
    while isinstance(b, int) and b in pred: nm, b2 = pred[b]; path.append(nm); b = b2
    return {"critical_ns": worst + SETUP, "endpoint": wname, "depth": sum(1 for p in path if not p.endswith(".read")), "path_head": list(reversed(path))[:6]}
if __name__ == "__main__":
    r = synth(["rtl/requant.v"], "requant", "requant"); print(r["area"], r["ncells"], sta(os.path.join(ROOT, "out", "requant_net.json"), "requant"))
