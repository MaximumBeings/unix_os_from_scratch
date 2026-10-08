#!/usr/bin/env python3
"""Appendix B: test the tests of the primer's demonstration modules. Each mutant breaks one line of rtl/appx_b.v; the testbench must notice."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
V = "rtl/appx_b.v"
MUT = [
 (V, "Gray: the shift is by 2", "assign g = b ^ (b >> 1);", "assign g = b ^ (b >> 2);"),
 (V, "Gray: the output is plain binary", "assign g = b ^ (b >> 1);", "assign g = b;"),
 (V, "counter: the enable is ignored", "else if (en) b <= b + 1'b1;", "else b <= b + 1'b1;"),
 (V, "counter: it counts by two", "else if (en) b <= b + 1'b1;", "else if (en) b <= b + 2'd2;"),
 (V, "counter: reset loads 1", "if (rst) b <= 0; else if (en)", "if (rst) b <= 1; else if (en)"),
 (V, "swap: the non-blocking swap is written with blocking assignments", "if (rst) begin a <= 4'd1; b <= 4'd2; end else begin a <= b; b <= a; end", "if (rst) begin a <= 4'd1; b <= 4'd2; end else begin a = b; b = a; end"),
 (V, "swap: the pair is not reset to 1, 2", "if (rst) begin a <= 4'd1; b <= 4'd2; end else begin a <= b; b <= a; end", "if (rst) begin a <= 4'd2; b <= 4'd1; end else begin a <= b; b <= a; end"),
 (V, "extend: the sign extension is a zero extension", "assign sext = {{4{x[7]}}, x};", "assign sext = {4'b0000, x};"),
 (V, "extend: the sign extension replicates the wrong bit", "assign sext = {{4{x[7]}}, x};", "assign sext = {{4{x[6]}}, x};"),
 (V, "extend: the zero extension fills with ones", "assign zext = {4'b0000, x};", "assign zext = {4'b1111, x};"),
]
c, n, l = hw.mutate(MUT, [V], "appx_b_tb", ["tb/appx_b_tb.v"], workers=4); print("\n".join(l)); print(f"\nappendix B demonstration modules: {c} of {n} broken circuits caught"); sys.exit(0 if c == n else 1)
