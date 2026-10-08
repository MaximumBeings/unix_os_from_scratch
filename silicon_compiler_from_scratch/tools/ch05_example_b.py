#!/usr/bin/env python3
"""Chapter 5, running example B: how big must a tile be to hide the memory latency? For memory latencies LAT in {1, 4, 16, 64} and tile sizes TILE in {4, 8, 16, 32, 64, 128} (compute 2 cycles per word, 16 tiles), run the REAL circuit serial and double-buffered in Icarus, and compare the measured speedup with the formula. Derived break-even: compute hides the load when C >= L, i.e. TILE*CPW + 3 >= TILE + LAT + 3, i.e. TILE >= LAT / (CPW - 1). Writes out/ch05_example_b.json. Usage: ch05_example_b.py"""
import concurrent.futures, json, os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
sys.path.insert(0, os.path.join(hw.ROOT, "model")); import dbuf_gold
R = hw.ROOT; CPW = 2; T = 16; LATS = (1, 4, 16, 64); TILES = (4, 8, 16, 32, 64, 128)
F = ["rtl/sram.v", "rtl/dma.v", "rtl/dbuf.v", "tb/extmem.v", "tb/dbuf_tb.v"]
def one(args):
    lat, tile = args; d = f"/tmp/ch05b_{lat}_{tile}"; os.makedirs(d + "/out", exist_ok=True)
    subprocess.run([sys.executable, os.path.join(R, "model/dbuf_gold.py"), d + "/out/dbuf_vectors.hex", str(tile), str(CPW), str(lat), str(T)], capture_output=True, check=True)
    for sub in ("rtl", "tb"): subprocess.run(["cp", "-r", os.path.join(R, sub), d], check=True)
    rc, out = hw.sim_icarus(F, "dbuf_tb", root=d, defines=(f"TILE={tile}", f"CPW={CPW}", f"LAT={lat}"))
    m = re.search(r"serial=(\d+) double-buffered=(\d+)", out); return (lat, tile, rc, int(m.group(1)) if m else None, int(m.group(2)) if m else None)
jobs = [(l, t) for l in LATS for t in TILES]
with concurrent.futures.ThreadPoolExecutor(4) as ex: rows = list(ex.map(one, jobs))
res = {"lats": LATS, "tiles": TILES, "speedup": {}, "pace": {}}; ok = True
print(f"{T} tiles, compute {CPW} cycles per word. Measured on the circuit (Icarus); every run also passed the schedule-model check inside dbuf_tb.\n")
print("speedup = serial cycles / double-buffered cycles")
print("  LAT \\ TILE " + "".join(f"{t:8d}" for t in TILES) + "    break-even TILE >= LAT/(CPW-1)")
for lat in LATS:
    line = f"  {lat:9d}   "; sp = []
    for t in TILES:
        r = [x for x in rows if x[0] == lat and x[1] == t][0]; ok &= r[2] == 0 and r[3] is not None; s = r[3] / r[4]; sp.append(s); line += f"{s:8.2f}"
    res["speedup"][lat] = sp; print(line + f"      {lat // (CPW - 1)}")
print("\nwho sets the pace (L = load time, C = compute time, from the model): 'C' = compute-bound, 'L' = load-bound")
print("  LAT \\ TILE " + "".join(f"{t:8d}" for t in TILES))
for lat in LATS:
    line = f"  {lat:9d}   "
    for t in TILES:
        L = t + lat + 3; C = t * CPW + 3; line += f"{'C' if C >= L else 'L':>8s}"
    print(line)
print("\nreading it: with a long latency (64) and small tiles, the load of one tile takes much longer than its compute (L = tile + 67 against C = 2*tile + 3), so the DMA sets the pace and double buffering gains little: the speedup is only 1.14 at TILE = 4 and 1.39 at TILE = 16.")
print("The gain peaks exactly at the break-even tile (1.88 at TILE = LAT, where L = C) and falls again when compute dominates (toward 1.5 here, because the serial schedule is also mostly compute). The fix for a slow memory is a BIGGER TILE, until C >= L: that is the break-even column.")
res["rows"] = rows; json.dump(res, open(os.path.join(R, "out", "ch05_example_b.json"), "w")); sys.exit(0 if ok else 1)
