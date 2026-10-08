#!/usr/bin/env python3
"""Chapter 10: the synthesis flow and gate-level verification of GA-2. Usage: ch10_run.py   (about 10 minutes: it simulates a 31,000-cell netlist several times)"""
import os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model"))
import hw, synth
R = hw.ROOT
def line(out): return ([l for l in out.splitlines() if l.startswith(("PASS", "FAIL", "MISMATCH"))] or out.strip().splitlines()[-2:])[0]
print("== 1. the toy cell library (lib/toy.lib, made by tools/gen_toylib.py): area in NAND2 equivalents, delay in ns")
txt = open(os.path.join(R, "lib", "toy.lib")).read()
for m in re.finditer(r"cell\((\w+)\) \{\n    area : ([\d.]+);", txt):
    d = re.search(r'cell_rise\(scalar\) \{ values\("([\d.]+)"\)', txt[m.end():]).group(1); print(f"  {m.group(1):6s} area {m.group(2):>5s}   delay {d} ns")
print("\n== 2. is the timing analyzer right? a hand-built circuit: y = INV(NAND2(INV(a), a)); q = DFF(NAND2(...)); z = INV(q)")
subprocess.run(["yosys", "-q", "-p", "read_verilog -sv lib/toy_cells.v lib/chain_check.v; hierarchy -top chain; proc; write_json out/chain_net.json"], cwd=R)
r = synth.sta(os.path.join(R, "out", "chain_net.json"), "chain")
print(f"  by hand: y path 0.04+0.07+0.04 = 0.15, +0.10 setup = 0.25; flop path 0.11+0.10 = 0.21; clock-to-q 0.30 + INV 0.04 + 0.10 = 0.44 -> worst 0.44 ns at output z")
print(f"  analyzer: {r['critical_ns']:.2f} ns at {r['endpoint']}")
print("\n== 3. synthesize every unit and the whole chip (memories stay as macros)")
RT = ["rtl/dma.v", "rtl/systolic.v", "rtl/requant.v", "rtl/exp_lut.v", "rtl/divu.v", "rtl/ga2_vec.v", "rtl/ga2_sm.v", "rtl/ga2_mm.v", "rtl/ga2.v"]; MEM = ["rtl/sram.v", "rtl/rowmem.v"]
print(f"{'unit':10s} {'cells':>7s} {'area (GE)':>10s} {'flip-flops':>11s} {'critical path':>14s} {'gate depth':>11s} {'max clock':>10s}  critical endpoint")
for top in ("exp_lut", "divu", "requant", "systolic", "ga2_st", "ga2_rq", "ga2_vadd", "ga2_amax", "ga2_sm", "ga2_mm", "ga2"):
    s = synth.synth(RT, top, top, memories=MEM)
    if not s["ok"]: print(top, "FAILED", s["log"][-300:]); continue
    t = synth.sta(os.path.join(R, "out", top + "_net.json"), top); ff = s["cells"].get("DFF", 0)
    print(f"{top:10s} {s['ncells']:7d} {s['area']:10.0f} {ff:11d} {t['critical_ns']:11.2f} ns {t['depth']:11d} {1000/t['critical_ns']:7.0f} MHz  {t['endpoint']}")
print("\n== 4. gate-level simulation of the whole chip: the SAME testbench and program suite, now driving the netlist of toy cells (memories as behavioural macros)")
G = ["lib/toy_cells.v", "rtl/sram.v", "rtl/rowmem.v", "out/ga2_net.v", "tb/extmem_rw.v", "tb/ga2_tb.v"]
print(subprocess.run([sys.executable, "model/ga2_progs.py", "out/ga2_programs.hex", "60", "1"], cwd=R, capture_output=True, text=True).stdout.strip(), "(78 programs)")
rc, out = hw.sim_verilator(G, "ga2_tb"); print(f"  Verilator, netlist, all 78 programs        : {line(out)} (exit {rc})")
print(subprocess.run([sys.executable, "model/ga2_progs.py", "out/ga2_programs.hex", "12", "3"], cwd=R, capture_output=True, text=True).stdout.strip(), "(30 programs, for the slower runs below)")
rc, out = hw.sim_icarus(G, "ga2_tb"); print(f"  Icarus (4-state), flip-flops start as x     : {line(out)} (exit {rc})")
rc, out = hw.sim_icarus(G, "ga2_tb", defines=("POWERUP_ZERO",)); print(f"  Icarus (4-state), flip-flops start at 0     : {line(out)} (exit {rc})")
for seed in (1, 2, 3):
    rc, out = hw.sim_verilator(G, "ga2_tb", args=("--x-initial", "unique", "--x-assign", "unique"), runargs=("+verilator+rand+reset+2", f"+verilator+seed+{seed}")); print(f"  Verilator, random power-up state, seed {seed}  : {line(out)} (exit {rc})")
