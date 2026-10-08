#!/usr/bin/env python3
"""Chapter 2: test the tests. Break the multiplier and the MAC one line at a time; every mutant must be caught. Two mutants are EQUIVALENT (the saturation compare written with <= and >= where < and > were: at exactly the limit the clamp returns the value it already had)."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
V = "rtl/mul8.v"; MAC = "rtl/mac.v"
MUL = [
 (V, "mul8_sa: the sign row is added instead of subtracted", "r6 - r7;", "r6 + r7;"),
 (V, "mul8_sa: row 3 is shifted by 2", "b[3] ? (ax <<< 3)", "b[3] ? (ax <<< 2)"),
 (V, "mul8_sa: row 5 is left out of the sum", "r4 + r5 + r6", "r4 + r6"),
 (V, "mul8_sa: row 5 is selected by bit 6", "b[5] ? (ax <<< 5)", "b[6] ? (ax <<< 5)"),
 (V, "mul8_sa: a is zero-extended instead of sign-extended", "wire signed [15:0] ax = a;", "wire signed [15:0] ax = {8'b0, a};"),
 (V, "mul8_sa: the sign row ignores the sign bit", "wire signed [15:0] r7 = b[7] ?", "wire signed [15:0] r7 = b[6] ?"),
 (V, "mul8_beh: the operands are treated as unsigned", "assign p = a * b;", "assign p = $unsigned(a) * $unsigned(b);"),
 (V, "mul8_beh: the top bit of the product is lost", "assign p = a * b;", "assign p = (a * b) & 16'h7fff;"),
]
MACM = [
 (MAC, "mac: the product is subtracted", "wire signed [32:0] wide = acc + p;", "wire signed [32:0] wide = acc - p;"),
 (MAC, "mac: clr with en loads the product without sign extension", "acc <= en ? {{16{p[15]}}, p} : 32'sd0;", "acc <= en ? {16'b0, p} : 32'sd0;"),
 (MAC, "mac: clr with en loads zero", "acc <= en ? {{16{p[15]}}, p} : 32'sd0;", "acc <= 32'sd0;"),
 (MAC, "mac: en takes priority over clr, so clr only works when en is 0", "else if (clr) acc <= en ? {{16{p[15]}}, p} : 32'sd0;\n        else if (en) acc <= summed;", "else if (en) acc <= summed;\n        else if (clr) acc <= 32'sd0;"),
 (MAC, "mac: the upper clamp is one too low", "? 32'sd2147483647 :\n", "? 32'sd2147483646 :\n"),
 (MAC, "mac: the lower clamp is one too high", "? -32'sd2147483648 : wide[31:0];", "? -32'sd2147483647 : wide[31:0];"),
 (MAC, "mac: the SAT parameter is inverted", "(SAT == 0) ? wide[31:0] :", "(SAT == 1) ? wide[31:0] :"),
 (MAC, "mac: the wrapped sum drops bit 31", "(SAT == 0) ? wide[31:0] :", "(SAT == 0) ? {1'b0, wide[30:0]} :"),
 (MAC, "mac: reset loads 1", "if (rst) acc <= 32'sd0;", "if (rst) acc <= 32'sd1;"),
 (MAC, "mac: en is ignored", "else if (en) acc <= summed;", "else acc <= summed;"),
 (MAC, "mac: the sum is only 32 bits wide, so overflow cannot be seen", "wire signed [32:0] wide = acc + p;", "wire signed [31:0] wide32 = acc + p; wire signed [32:0] wide = wide32;"),
 (MAC, "mac: the accumulator is 24 bits (a narrower register)", "output reg signed [31:0] acc);\n    wire signed [15:0] p", "output reg signed [31:0] acc);\n    wire signed [15:0] p"),
]
MACM = [m for m in MACM if "24 bits" not in m[1]]
EQUIV = [
 (MAC, "EQUIVALENT: the upper clamp triggers at the limit itself (>=)", "(wide > 33'sd2147483647)", "(wide >= 33'sd2147483647)"),
 (MAC, "EQUIVALENT: the lower clamp triggers at the limit itself (<=)", "(wide < -33'sd2147483648)", "(wide <= -33'sd2147483648)"),
]
F = ["rtl/mul8.v", "rtl/mac.v"]
print("NOTE: this script deliberately breaks copies of the RTL. 'caught' lines are EXPECTED: they show the testbenches can detect the mistake.")
ok = True
c, n, l = hw.mutate(MUL, F, "mul8_tb", ["tb/mul8_tb.v"]); print("\n".join(l)); print(f"\nmultipliers: {c} of {n} broken circuits caught"); ok &= c == n
c, n, l = hw.mutate(MACM, F, "mac_tb", ["tb/mac_tb.v"]); print("\n" + "\n".join(l)); print(f"\nMAC: {c} of {n} broken circuits caught"); ok &= c == n
c, n, l = hw.mutate(EQUIV, F, "mac_tb", ["tb/mac_tb.v"]); print("\nTwo changes that look like bugs and are not:"); print("\n".join(l)); ok &= c == 0
sys.exit(0 if ok else 1)
