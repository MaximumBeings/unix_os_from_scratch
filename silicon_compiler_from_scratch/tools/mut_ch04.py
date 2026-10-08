#!/usr/bin/env python3
"""Chapter 4: test the tests. Break the systolic array one line at a time and run the testbench for two array shapes (3x3 with K=5 and 4x4 with K=7); a mutant counts as caught if ANY shape catches it, and the table shows which."""
import os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
S = "rtl/systolic.v"
M = [
 (S, "pe: the product is subtracted", "acc <= acc + a_in * b_in;", "acc <= acc - a_in * b_in;"),
 (S, "pe: the product is computed unsigned", "acc <= acc + a_in * b_in;", "acc <= acc + $unsigned(a_in) * $unsigned(b_in);"),
 (S, "pe: the product is cut to 16 bits and zero-extended", "acc <= acc + a_in * b_in;", "acc <= acc + {16'b0, a_in * b_in};"),
 (S, "pe: a is passed on unchanged but b is replaced by a", "b_out <= b_in; acc", "b_out <= a_in; acc"),
 (S, "pe: a is not passed on", "else begin a_out <= a_in;", "else begin a_out <= 8'sd0;"),
 (S, "pe: b is not passed on", "b_out <= b_in; acc", "b_out <= 8'sd0; acc"),
 (S, "pe: the accumulator does not clear on clr", "b_out <= 8'sd0; acc <= 32'sd0; end", "b_out <= 8'sd0; end"),
 (S, "pe: clr does not clear the pass-through a register", "begin a_out <= 8'sd0; b_out", "begin b_out"),
 (S, "pe: clr does not clear the pass-through b register", "b_out <= 8'sd0; acc <= 32'sd0; end", "acc <= 32'sd0; end"),
 (S, "pe: the product is multiplied twice (acc adds it two times)", "acc <= acc + a_in * b_in;", "acc <= acc + 2 * a_in * b_in;"),
 (S, "array: the rows of A enter in reverse order", "assign aw[i][0] = a_edge[8*i +: 8];", "assign aw[i][0] = a_edge[8*(N-1-i) +: 8];"),
 (S, "array: the columns of B enter in reverse order", "assign bw[0][i] = b_edge[8*i +: 8];", "assign bw[0][i] = b_edge[8*(N-1-i) +: 8];"),
 (S, "array: the top edge is fed from the a bus", "assign bw[0][i] = b_edge[8*i +: 8];", "assign bw[0][i] = a_edge[8*i +: 8];"),
 (S, "array: the result matrix is transposed", "assign c[32*(i*N + j) +: 32] = acc;", "assign c[32*(j*N + i) +: 32] = acc;"),
 (S, "array: b is passed down to the wrong column", ".b_out(bw[i+1][j])", ".b_out(bw[i+1][(j+1)%N])"),
 (S, "array: a is passed to the wrong row", ".a_out(aw[i][j+1])", ".a_out(aw[(i+1)%N][j+1])"),
]
EQUIV = [
 (S, "EQUIVALENT: the sum is written product-first (addition commutes)", "acc <= acc + a_in * b_in;", "acc <= a_in * b_in + acc;"),
]
F = [S]; SHAPES = [(3, 5), (4, 7)]
def run_all(muts):
    res = {m[1]: [] for m in muts}
    for N, K in SHAPES:
        subprocess.run([sys.executable, "model/systolic_model.py", "out/systolic_vectors.hex", str(N), str(K), "100"], cwd=hw.ROOT, capture_output=True)
        c, n, lines = hw.mutate(muts, F, "systolic_tb", ["tb/systolic_tb.v"], defines=(f"NN={N}", f"KK={K}"))
        for line in lines: label, status = line.rsplit(": ", 1); res[label].append(status)
    return res
print("NOTE: this script deliberately breaks copies of the RTL. 'caught' lines are EXPECTED: they show the testbench can detect the mistake.")
r = run_all(M); caught = 0
for label, st in r.items():
    ok = "caught" in st; caught += ok
    print(f"{label}: {'caught' if ok else 'NOT CAUGHT'}   [3x3,K=5: {st[0]}; 4x4,K=7: {st[1]}]")
print(f"\nsystolic array: {caught} of {len(M)} broken circuits caught")
r2 = run_all(EQUIV); print("\nA change that looks like a bug and is not:")
for label, st in r2.items(): print(f"{label}: {'caught' if 'caught' in st else 'NOT CAUGHT'}")
sys.exit(0 if caught == len(M) and not any("caught" in s for s in r2.values()) else 1)
