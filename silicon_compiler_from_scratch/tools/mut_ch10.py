#!/usr/bin/env python3
"""Chapter 10: test the tests of the FLOW. Break one cell function in the library description that synthesis reads (the simulation models stay correct), re-synthesize the requantizer, and run the gate-level simulation
against the golden vectors. If synthesis used the wrong function, the netlist is wrong, and the simulation must fail. A cell the mapper never uses cannot matter (reported as such)."""
import concurrent.futures, os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw, synth
R = hw.ROOT; LIB = open(os.path.join(R, "lib", "toy.lib")).read()
subprocess.run([sys.executable, "model/requant_gold.py", "out/requant_vectors.hex", "3000"], cwd=R, capture_output=True)
MUT = [("XOR2", '"(A^B)"', '"!(A^B)"'), ("XNOR2", '"!(A^B)"', '"(A^B)"'), ("NAND2", '"!(A&B)"', '"(A&B)"'), ("NOR2", '"!(A|B)"', '"(A|B)"'), ("INV", 'function : "!A"', 'function : "A"'),
       ("MUX2", '"((S&B)|(!S&A))"', '"((S&A)|(!S&B))"'), ("AOI21", '"!((A&B)|C)"', '"!((A|B)&C)"'), ("OAI21", '"!((A|B)&C)"', '"!((A&B)|C)"'), ("NAND3", '"!(A&B&C)"', '"!(A&B)"'),
       ("NOR3", '"!(A|B|C)"', '"!(A|B)"'), ("AND2", '"(A&B)"', '"(A|B)"'), ("OR2", '"(A|B)"', '"(A&B)"'), ("BUF", 'function : "A"', 'function : "!A"')]
def one(i):
    cell, old, new = MUT[i]; m = re.search(r"cell\(%s\) \{.*?\n  \}" % cell, LIB, re.S); body = m.group(0)
    if old not in body: return cell, "BAD ANCHOR"
    lib = LIB.replace(body, body.replace(old, new, 1)); path = f"out/badlib_{cell}.lib"; open(os.path.join(R, path), "w").write(lib)
    s = synth.synth(["rtl/requant.v"], "requant", f"badflow_{cell}", lib=path)
    if not s["ok"]: return cell, "synthesis failed (caught at synthesis)"
    used = s["cells"].get(cell, 0)
    rc, out = hw.sim_icarus(["lib/toy_cells.v", f"out/badflow_{cell}_net.v", "tb/requant_tb.v"], "requant_tb")
    return cell, ("caught" if rc != 0 or "PASS" not in out else "NOT CAUGHT") + f"   (the mapper used {used} {cell} cells)"
print("NOTE: this script deliberately mis-describes one cell to the synthesis tool. 'caught' lines are EXPECTED: gate-level simulation notices the wrong logic.")
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool: rows = list(pool.map(one, range(len(MUT))))
for cell, r in rows: print(f"library says {cell} computes the wrong function: {r}")
