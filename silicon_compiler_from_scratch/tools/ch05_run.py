#!/usr/bin/env python3
"""Chapter 5: the scratchpad, the DMA and the double-buffered tile streamer through the flow. Usage: ch05_run.py"""
import os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
sys.path.insert(0, os.path.join(hw.ROOT, "model")); import dbuf_gold
F = ["rtl/sram.v", "rtl/dma.v", "rtl/dbuf.v", "tb/extmem.v", "tb/dbuf_tb.v"]
def gen(tile, cpw, lat, T): return subprocess.run([sys.executable, "model/dbuf_gold.py", "out/dbuf_vectors.hex", str(tile), str(cpw), str(lat), str(T)], cwd=hw.ROOT, capture_output=True, text=True).stdout.strip()
def sim(fn, tile, cpw, lat): return fn(F, "dbuf_tb", defines=(f"TILE={tile}", f"CPW={cpw}", f"LAT={lat}"))
def line(out): return ([l for l in out.splitlines() if l.startswith(("PASS", "FAIL", "MISMATCH"))] or out.strip().splitlines()[-2:])[0]
print("== 1. eight shapes in both simulators: TILE words, CPW compute cycles per word, LAT memory latency, T tiles")
print("(the first shape is the one the four overhead constants of the schedule model were read from; the other seven are predictions)")
for cfg in [(16, 1, 4, 6), (16, 2, 4, 6), (8, 4, 10, 5), (32, 1, 2, 3), (4, 8, 20, 7), (8, 1, 1, 9), (64, 1, 30, 4), (8, 2, 4, 1)]:
    print(gen(*cfg))
    for name, fn in (("Icarus", hw.sim_icarus), ("Verilator", hw.sim_verilator)):
        rc, out = sim(fn, *cfg[:3]); print(f"  {name:10s}{line(out)} (exit {rc})")
print("\n== 2. what double buffering buys: TILE=16, LAT=4, 16 tiles, sweeping the compute time per word (measured in Icarus)")
print(f"{'CPW':>4s} {'load L':>7s} {'compute C':>10s} {'serial':>7s} {'double':>7s} {'speedup':>8s}  who sets the pace")
for cpw in (1, 2, 4, 8, 16):
    gen(16, cpw, 4, 16); rc, out = sim(hw.sim_icarus, 16, cpw, 4)
    import re; m = re.search(r"serial=(\d+) double-buffered=(\d+)", out); s, d = int(m.group(1)), int(m.group(2))
    L = 16 + 4 + dbuf_gold.L0; C = 16 * cpw + dbuf_gold.C0
    print(f"{cpw:4d} {L:7d} {C:10d} {s:7d} {d:7d} {s/d:8.2f}  {'the load (memory bound)' if L > C else 'the compute (compute bound)'}")
print("\n== 3. Yosys: what the pieces cost")
GATES = "abc -g AND,NAND,OR,NOR,XOR,XNOR,ANDNOT,ORNOT,MUX; opt_clean; stat"
for top, files in (("dma", ["rtl/dma.v"]), ("dbuf", ["rtl/sram.v", "rtl/dma.v", "rtl/dbuf.v"])):
    g = hw.synth_stats(files, top, f"synth -flatten -top {top}; {GATES}"); l = hw.synth_stats(files, top, f"synth_ice40 -flatten -top {top}; stat")
    ff = sum(v for k, v in l["cells"].items() if k.startswith("SB_DFF")); ram = l["cells"].get("SB_RAM40_4K", 0)
    print(f"{top:5s} generic gates {g['total']:6d}   iCE40 cells {l['total']:5d} (flip-flops {ff}, block RAMs {ram})")
