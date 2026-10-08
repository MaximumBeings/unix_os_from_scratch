#!/usr/bin/env python3
"""Chapter 8: the attention step as GA-2 programs, run on the RTL in both simulators, with the cost of the KV cache measured. Usage: ch08_run.py"""
import os, re, sys
os.environ["GA2_EXT"] = "8192"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import hw, ga2_isa as I, ga2_progs as G, attn
F = ["rtl/sram.v", "rtl/dma.v", "rtl/systolic.v", "rtl/requant.v", "rtl/exp_lut.v", "rtl/divu.v", "rtl/rowmem.v", "rtl/ga2_vec.v", "rtl/ga2_sm.v", "rtl/ga2_mm.v", "rtl/ga2.v", "tb/extmem_rw.v", "tb/ga2_tb.v"]
def line(out): return ([l for l in out.splitlines() if l.startswith(("PASS", "FAIL", "MISMATCH"))] or out.strip().splitlines()[-2:])[0]
print("== 1. the 'cached' program for the third token (L = 3), as assembled\n")
ex3 = attn.make_example(3, 1); P3 = attn.rq_params(ex3); prog3 = attn.program(3, "cached", P3)
for k, ins in enumerate(prog3): print(f"{k:3d}  {I.disassemble(ins)}")
print(f"\n{len(prog3)} instructions; the same step without a cache ('recompute') is {len(attn.program(3, 'recompute', P3))}.")
print("\n== 2. both programs for L = 2, 4, 8, 16, 32 on the circuit (the last step of a decode loop, example seed 1), against the reference simulator")
progs = []; meta = []
for L in (2, 4, 8, 16, 32):
    ex = attn.make_example(L, 1); last = attn.sequence(ex)[-1]
    for var in ("cached", "recompute"):
        prog, ext_before, out8 = last[var]; progs.append((prog, ext_before)); meta.append((L, var, prog, out8))
    assert last["cached"][2] == last["recompute"][2]
words = [len(progs)]
for prog, ext in progs: words += G.record(prog, ext)
open(os.path.join(hw.ROOT, "out", "ga2_programs.hex"), "w").write("\n".join("%08x" % (w & 0xFFFFFFFF) for w in words) + "\n")
res = {}
for name, fn in (("Icarus", hw.sim_icarus), ("Verilator", hw.sim_verilator)):
    rc, out = fn(F, "ga2_tb", defines=("EXTW=8192", "DUMP")); print(f"  {name:10s}{line(out)} (exit {rc})")
    for m in re.finditer(r"COUNTERS (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)", out): res[int(m.group(1))] = [int(g) for g in m.groups()[1:]]
print("\n== 3. what the KV cache saves (cycle counters read from the circuit; the two programs give identical outputs at every step)")
print(f"{'L':>3s} {'program':10s} {'instr':>5s} {'total':>7s} {'matrix':>7s} {'DMA':>6s} {'vector':>7s}   {'cycles saved by the cache':>26s}")
for i, (L, var, prog, out8) in enumerate(meta):
    tot, mm, dma, vec, n = res[i]; extra = ""
    if var == "cached": extra = f"{res[i+1][0] / tot:5.2f}x faster than recompute"
    print(f"{L:3d} {var:10s} {n:5d} {tot:7d} {mm:7d} {dma:6d} {vec:7d}   {extra:>26s}")
print("\n== 4. where the cycles go in one cached step at L = 32 (from the cycle model, whose totals match the circuit's counters above)")
ex = attn.make_example(32, 1); prog = attn.program(32, "cached", attn.rq_params(ex)); per = {}
for ins in prog:
    if ins["op"] != "HALT": per.setdefault(ins["op"], [0, 0]); per[ins["op"]][0] += 1; per[ins["op"]][1] += 2 + I.wait_cycles(ins)
tot = sum(v[1] for v in per.values())
for op, (n, c) in sorted(per.items(), key=lambda kv: -kv[1][1]): print(f"  {op:5s} {n:3d} instructions {c:6d} cycles {100*c/tot:5.1f}%")
print(f"  total {tot} cycles")
print("\n== 5. how big is a KV cache? (derived arithmetic, not measured)")
print(f"GA-2 head: 2 (K and V) x {attn.D} (head width) x 1 byte (int8) = {2*attn.D} bytes per token; a 32-token context holds {32*2*attn.D} bytes.")
for name, layers, heads, dh, byt in (("a 7B-class transformer (32 layers x 32 heads x 128), fp16", 32, 32, 128, 2), ("the same with an int8 cache", 32, 32, 128, 1)):
    per_tok = 2 * layers * heads * dh * byt; print(f"{name}: {per_tok:,} bytes per token = {per_tok/1024:.0f} KiB; a 4096-token context = {per_tok*4096/2**30:.2f} GiB")
