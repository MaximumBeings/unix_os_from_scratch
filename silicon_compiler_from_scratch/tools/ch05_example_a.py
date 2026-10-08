#!/usr/bin/env python3
"""Chapter 5, running example A: see the overlap. Run the streamer (TILE=16, CPW=2, LAT=4, 4 tiles) serially and double-buffered in Icarus, record when every load and compute starts and ends, draw both timelines as text, and compare the measured durations with the schedule model. Writes out/ch05_example_a.json. Usage: ch05_example_a.py"""
import json, os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
sys.path.insert(0, os.path.join(hw.ROOT, "model")); import dbuf_gold
R = hw.ROOT; TILE, CPW, LAT, T = 16, 2, 4, 4
subprocess.run([sys.executable, "model/dbuf_gold.py", "out/dbuf_vectors.hex", str(TILE), str(CPW), str(LAT), str(T)], cwd=R, capture_output=True, check=True)
L = TILE + LAT + dbuf_gold.L0; C = TILE * CPW + dbuf_gold.C0; ms, md = dbuf_gold.cycles(T, TILE, CPW, LAT)
print(f"shape: {T} tiles of {TILE} words, compute {CPW} cycles per word, memory latency {LAT} cycles")
print(f"schedule model: load of one tile L = {TILE}+{LAT}+3 = {L} cycles, compute of one tile C = {TILE}*{CPW}+3 = {C} cycles")
print(f"                serial = T*(L+C) = {T}*({L}+{C}) = {ms};  double-buffered = L + (T-1)*max(L,C) + C = {L} + {T-1}*{max(L,C)} + {C} = {md}\n")
res = {"L": L, "C": C, "modes": {}}; ok = True
for mode, name in ((0, "serial"), (1, "double-buffered")):
    rc, out = hw.sim_icarus(["rtl/sram.v", "rtl/dma.v", "rtl/dbuf.v", "tb/extmem.v", "tb/dbuf_trace_tb.v"], "dbuf_trace_tb", defines=(f"MODE={mode}", f"TILE={TILE}", f"CPW={CPW}", f"LAT={LAT}")); assert rc == 0, out
    ev = []; total = None
    for l in out.splitlines():
        p = l.split()
        if p and p[0] == "E": ev.append((int(p[1]), p[2], int(p[3])))
        if p and p[0] == "TOTAL": total = int(p[1])
    ls = {n: c for c, w, n in ev if w == "LOAD_START"}; ld = {n: c for c, w, n in ev if w == "LOAD_DONE"}; cs = {n: c for c, w, n in ev if w == "COMP_START"}; cd = {n: c for c, w, n in ev if w == "COMP_DONE"}
    res["modes"][name] = {"load": [(ls[n], ld[n]) for n in range(T)], "comp": [(cs[n], cd[n]) for n in range(T)], "total": total}
    print(f"== {name}: total {total} cycles (model {ms if mode == 0 else md}: {'agrees' if total == (ms if mode == 0 else md) else 'DISAGREES'})"); ok &= total == (ms if mode == 0 else md)
    print("   tile   load start..done   (cycles)    compute start..done   (cycles)")
    for n in range(T): print(f"   {n:3d}    {ls[n]:4d} .. {ld[n]:4d}      ({ld[n]-ls[n]:3d})        {cs[n]:4d} .. {cd[n]:4d}       ({cd[n]-cs[n]:3d})")
    width = total // 6 + 1; row_l = [" "] * width; row_c = [" "] * width
    for n in range(T):
        for t in range(ls[n] // 6, ld[n] // 6 + 1): row_l[t] = "L" if row_l[t] == " " else "#"
        for t in range(cs[n] // 6, cd[n] // 6 + 1): row_c[t] = "C" if row_c[t] == " " else "#"
    print("   timeline (each character = 6 cycles; L = DMA loading, C = compute):"); print("   DMA     |" + "".join(row_l) + "|"); print("   compute |" + "".join(row_c) + "|\n")
dl = res["modes"]["double-buffered"]["load"]; dc = res["modes"]["double-buffered"]["comp"]
print(f"measured per-tile durations, counting both the first and the last cycle: load {dl[0][1]-dl[0][0]+1} cycles (model L = {L}), compute {dc[0][1]-dc[0][0]+1} cycles (model C = {C}).")
print(f"In the double-buffered run, the load of tile 1 starts at cycle {dl[1][0]}, while the compute of tile 0 started at {dc[0][0]} and ends at {dc[0][1]}: the two overlap for {dc[0][1]-dl[1][0]+1} cycles.")
print(f"Here C > L, so after the first load the compute unit is never idle between tiles: it starts tile n+1 at cycle {dc[1][0]}, the very cycle after tile n ends ({dc[0][1]}).")
json.dump(res, open(os.path.join(R, "out", "ch05_example_a.json"), "w")); sys.exit(0 if ok else 1)
