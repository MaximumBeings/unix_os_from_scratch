#!/usr/bin/env python3
"""Chapter 10: how good are the tests at the GATE level? Stuck-at fault grading of the synthesized requantizer.
A stuck-at fault forces one gate output to a constant 0 or 1 (a manufacturing defect). A test set is judged by the fraction of faults it DETECTS, i.e. makes some output differ from the good circuit.
1. synthesize the requantizer to the toy library;  2. sample 150 gate outputs, giving 300 faults;  3. simulate them against the golden vectors (directed cases first, then random), recording the first vector that detects each;
4. for every fault never detected, ask ABC's equivalence checker whether the faulty circuit is equivalent to the good one (a REDUNDANT fault, which no test can detect) or not (a testable fault the vectors missed, for which it returns a distinguishing input).
Usage: ch10_faults.py"""
import os, random, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw, synth, netlist as N
R_ = hw.ROOT; NRAND = 40000
r = synth.synth(["rtl/requant.v"], "requant", "requant"); js = os.path.join(R_, "out", "requant_net.json")
all_nets = N.nets(js, "requant"); sites = random.Random(5).sample(all_nets, 150)
print(f"requantizer: {r['ncells']} gates, {r['area']:.0f} NAND2-equivalents; {len(all_nets)} gate outputs, {len(sites)} sampled, {2*len(sites)} stuck-at faults")
open(os.path.join(R_, "out", "requant_f.v"), "w").write(N.to_verilog_faultable(js, "requant", sites, "requant_f"))
out = subprocess.run([sys.executable, "model/requant_gold.py", "out/requant_vectors.hex", str(NRAND)], cwd=R_, capture_output=True, text=True).stdout.strip(); print(out)
nrows = int(open(os.path.join(R_, "out", "requant_vectors.hex")).readline(), 16); D = nrows - NRAND
rc, log = hw.sim_verilator(["lib/toy_cells.v", "out/requant_f.v", "tb/requant_fault_tb.v"], "requant_fault_tb", defines=(f"NFAULT={len(sites)}",))
assert "FAULTFREE ok" in log, log[-1500:]
first = {}
for m in re.finditer(r"FAULT (\d+) (\d+) (-?\d+)", log): first[(int(m.group(1)), int(m.group(2)))] = int(m.group(3))
print(f"\n== 1. fault coverage as the test set grows ({D} directed vectors come first, then random ones)")
print(f"{'vectors applied':>16s} {'detected':>9s} {'coverage':>9s}")
for k in (50, 200, D, D + 100, D + 1000, D + 10000, nrows):
    det = sum(1 for v in first.values() if 0 < v <= k); print(f"{k:16d} {det:9d} {100*det/len(first):8.2f}%" + ("   <- all directed vectors" if k == D else "   <- everything" if k == nrows else ""))
und = [f for f, v in first.items() if v < 0]
TIE = {"m_23": 1}          # operating constraint: the mantissa is normalized (2^23 <= m < 2^24), as everywhere in the book
print(f"\n== 2. the {len(und)} faults no vector detected: redundant, or a gap in the tests?  (equivalence checking with the input m[23] held at 1)")
red = 0; patterns = []
open("/tmp/ch10_good.blif", "w").write(N.to_blif(js, "requant", tie=TIE))
for (k, val) in und:
    open("/tmp/ch10_bad.blif", "w").write(N.to_blif(js, "requant", fault=(sites[k], val), tie=TIE))
    o = subprocess.run(["yosys-abc", "-c", "cec /tmp/ch10_good.blif /tmp/ch10_bad.blif"], capture_output=True, text=True, timeout=600).stdout
    if "NOT EQUIVALENT" in o:
        pat = {a: int(b) for a, b in re.findall(r"(\w+)=([01])", o.split("Input pattern:")[1])}; g = lambda n, w: sum(pat.get(f"{n}_{i}", 0) << i for i in range(w))
        acc = g("acc", 32); acc -= (1 << 32) if acc >> 31 else 0; patterns.append((acc, g("m", 24) | (1 << 23), g("s", 6), g("relu", 1)))
    elif "equivalent" in o.lower(): red += 1
    else: print("unexpected ABC output:", o[:200])
print(f"  proven REDUNDANT under the constraint (no test can detect them): {red}")
print(f"  TESTABLE but missed by all {nrows} vectors: {len(patterns)}   (the checker returns one distinguishing input for each; a few of them:)")
for a, m_, s_, r_ in patterns[:4]: print(f"    acc={a} m={m_} s={s_} relu={r_}")
print("\n== 3. use those inputs as test vectors (automatic test pattern generation) and grade again")
sys.path.insert(0, os.path.join(R_, "model")); from quant import requant
rows = [open(os.path.join(R_, "out", "requant_vectors.hex")).read().split()][0]; body = rows[1:]
for a, m_, s_, r_ in patterns: q = requant(a, m_, s_, bool(r_)); body.append("%018x" % (((a & 0xFFFFFFFF) << 39) | (m_ << 15) | (s_ << 9) | (r_ << 8) | (q & 255)))
open(os.path.join(R_, "out", "requant_vectors.hex"), "w").write("%018x\n" % len(body) + "\n".join(body) + "\n")
rc, log = hw.sim_verilator(["lib/toy_cells.v", "out/requant_f.v", "tb/requant_fault_tb.v"], "requant_fault_tb", defines=(f"NFAULT={len(sites)}",)); assert "FAULTFREE ok" in log
first2 = {(int(a), int(b)): int(c) for a, b, c in re.findall(r"FAULT (\d+) (\d+) (-?\d+)", log)}
det2 = sum(1 for v in first2.values() if v > 0); tot = len(first2); testable = tot - red
print(f"  with {len(patterns)} extra vectors appended: {det2} of {tot} faults detected; {red} are redundant, so {det2} of {testable} testable faults = {100*det2/testable:.2f}%")
print(f"\n== summary: random + directed vectors alone: {sum(1 for v in first.values() if v > 0)} of {testable} testable faults ({100*sum(1 for v in first.values() if v > 0)/testable:.2f}%); after test pattern generation: {100*det2/testable:.2f}%")
