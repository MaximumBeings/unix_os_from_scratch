#!/usr/bin/env python3
"""Chapter 6: test the tests. Break the exp table, the divider and the softmax one line at a time. The softmax testbench runs for two lengths (N=8 and N=3); a softmax mutant counts as caught if either catches it."""
import os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
L = "rtl/exp_lut.v"; V = "rtl/divu.v"; S = "rtl/softmax.v"
lut = open(os.path.join(hw.ROOT, L)).read()
def ent(d): return re.search(r"8'd%d: e = 16'd(\d+);" % d, lut).group(0), int(re.search(r"8'd%d: e = 16'd(\d+);" % d, lut).group(1))
def lutmut(d, label, f):
    old, v = ent(d); return (L, label, old, "8'd%d: e = 16'd%d;" % (d, f(v)))
LUT = [lutmut(0, "table entry 0 (exp(0) = 1.0) is one too small", lambda v: v - 1), lutmut(64, "table entry 64 is one too large", lambda v: v + 1),
       lutmut(188, "the last nonzero entry is zero", lambda v: 0), lutmut(255, "the last entry is 1", lambda v: 1), lutmut(17, "entry 17 has two digits swapped", lambda v: int(str(v)[::-1])) ]
LUT_EQ = [(L, "EQUIVALENT: the default branch returns 1 (all 256 values of d are listed, so it is unreachable)", "default: e = 16'd0;", "default: e = 16'd1;")]
DIV = [
 (V, "divu: compares with > so an exact fit is not subtracted", "wire fits = shifted >= {1'b0, d_r};", "wire fits = shifted > {1'b0, d_r};"),
 (V, "divu: the divisor is not subtracted", "wire [W:0] r_next = fits ? shifted - {1'b0, d_r} : shifted;", "wire [W:0] r_next = shifted;"),
 (V, "divu: the quotient bit is inverted", "wire [W-1:0] q_next = {q[W-2:0], fits};", "wire [W-1:0] q_next = {q[W-2:0], !fits};"),
 (V, "divu: one iteration too few", "cnt <= W; end", "cnt <= W - 1; end"),
 (V, "divu: division by zero gives quotient 0", "wire fits = shifted >= {1'b0, d_r};", "wire fits = d_r != 0 && shifted >= {1'b0, d_r};"),
 (V, "divu: the remainder output drops its top bit", "rem <= r_next[W-1:0];", "rem <= {1'b0, r_next[W-2:0]};"),
 (V, "divu: a start during a division restarts it", "else if (start && !busy) begin", "else if (start) begin"),
 (V, "divu: the numerator bits are taken from the wrong end", "wire [W:0] shifted = {r[W-1:0], n_r[W-1]};", "wire [W:0] shifted = {r[W-1:0], n_r[0]};"),
]
SOFT = [
 (S, "softmax: the maximum starts at 0 instead of -128", "m <= -8'sd128; sum <= 0;", "m <= 8'sd0; sum <= 0;"),
 (S, "softmax: the sum is not cleared between rows", "m <= -8'sd128; sum <= 0;", "m <= -8'sd128;"),
 (S, "softmax: the minimum is found instead of the maximum", "if (xs[idx] > m) m <= xs[idx];", "if (xs[idx] < m) m <= xs[idx];"),
 (S, "softmax: the table is indexed by x, not by the distance from the maximum", ".d(dfull[7:0])", ".d(xs[idx])"),
 (S, "softmax: the distance is x - m", "wire [8:0] dfull = {m[7], m} - {xs[idx][7], xs[idx]};", "wire [8:0] dfull = {xs[idx][7], xs[idx]} - {m[7], m};"),
 (S, "softmax: the numerator of the reciprocal is 2^37", ".num(40'd1 << 38)", ".num(40'd1 << 37)"),
 (S, "softmax: no rounding constant", "56'd2097152;", "56'd0;"),
 (S, "softmax: the output slice is shifted down one bit", "p[17*idx +: 17] <= rounded[38:22];", "p[17*idx +: 17] <= rounded[37:21];"),
 (S, "softmax: the output slice is shifted up one bit", "p[17*idx +: 17] <= rounded[38:22];", "p[17*idx +: 17] <= rounded[39:23];"),
 (S, "softmax: the sum leaves out the last element", "sum <= sum + lut_e;", "sum <= (idx == N - 1) ? sum : sum + lut_e;"),
 (S, "softmax: the first element's exponent is stored in the wrong slot", "ev[idx] <= lut_e;", "ev[idx == 0 ? 1 : idx] <= lut_e;"),
 (S, "softmax: the divider reads the sum one cycle too early (before the last add)", "DIVS: begin d_start <= 1'b1; state <= WAITD; end", "DIVS: begin d_start <= 1'b1; state <= WAITD; sum <= sum - 1; end"),
 (S, "softmax: the reciprocal is not latched", "WAITD: if (d_done) begin r <= d_quot;", "WAITD: if (d_done) begin r <= r;"),
 (S, "softmax: the maximum search stops one element early", "MAXS: begin if (xs[idx] > m) m <= xs[idx]; if (idx == N - 1)", "MAXS: begin if (xs[idx] > m) m <= xs[idx]; if (idx == N - 2)"),
]
def gold(*a): subprocess.run([sys.executable, "model/softmax_gold.py"] + list(a), cwd=hw.ROOT, capture_output=True)
print("NOTE: this script deliberately breaks copies of the RTL. 'caught' lines are EXPECTED: they show the testbenches can detect the mistake.")
gold("table", "out/exp_table.hex"); gold("div", "out/divu_vectors.hex", "20000"); ok = True
c, n, l = hw.mutate(LUT, [L], "exp_lut_tb", ["tb/exp_lut_tb.v"]); print("\n".join(l)); print(f"\nexp table: {c} of {n} broken circuits caught"); ok &= c == n
c, n, l = hw.mutate(LUT_EQ, [L], "exp_lut_tb", ["tb/exp_lut_tb.v"]); print("\n".join(l)); ok &= c == 0
c, n, l = hw.mutate(DIV, [V], "divu_tb", ["tb/divu_tb.v"]); print("\n" + "\n".join(l)); print(f"\ndivider: {c} of {n} broken circuits caught"); ok &= c == n
res = {m[1]: [] for m in SOFT}
for N in (8, 3):
    gold("soft", "out/softmax_vectors.hex", str(N), "600")
    c, n, lines = hw.mutate(SOFT, ["rtl/exp_lut.v", "rtl/divu.v", S], "softmax_tb", ["tb/softmax_tb.v"], defines=(f"NN={N}",))
    for line in lines: label, status = line.rsplit(": ", 1); res[label].append(status)
caught = 0; print()
for label, st in res.items():
    good = "caught" in st; caught += good; print(f"{label}: {'caught' if good else 'NOT CAUGHT'}   [N=8: {st[0]}; N=3: {st[1]}]")
print(f"\nsoftmax unit: {caught} of {len(SOFT)} broken circuits caught"); ok &= caught == len(SOFT)
sys.exit(0 if ok else 1)
