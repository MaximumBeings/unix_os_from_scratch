#!/usr/bin/env python3
"""Chapter 3: test the tests. Break the requantizer one line at a time; every real mutant must be caught. Equivalent mutants are reported separately."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
R = "rtl/requant.v"
M = [
 (R, "rounding constant left out (truncate)", "wire [56:0] sum = {1'b0, prod} + {1'b0, half};", "wire [56:0] sum = {1'b0, prod};"),
 (R, "rounding constant is 2^s instead of 2^(s-1)", "(56'd1 << (s - 6'd1))", "(56'd1 << s)"),
 (R, "clamp removed (low 7 bits kept)", "wire [6:0] mag_q = (shifted > limit) ? 7'd127 : shifted[6:0];", "wire [6:0] mag_q = shifted[6:0];"),
 (R, "clamp returns 126", "? 7'd127 : shifted[6:0]", "? 7'd126 : shifted[6:0]"),
 (R, "clamp at 128 (allows -128)", "wire [56:0] limit = 57'd127;", "wire [56:0] limit = 57'd128;"),
 (R, "rounding done on the signed value (floor), not the magnitude", "wire [31:0] mag = neg ? (~acc + 32'd1) : acc;", "wire [31:0] mag = acc;"),
 (R, "negative sign not restored", "wire signed [7:0] signed_q = neg ? -$signed({1'b0, mag_q}) : $signed({1'b0, mag_q});", "wire signed [7:0] signed_q = $signed({1'b0, mag_q});"),
 (R, "relu ignored", "assign q = (relu && signed_q[7]) ? 8'sd0 : signed_q;", "assign q = signed_q;"),
 (R, "relu always on", "assign q = (relu && signed_q[7]) ? 8'sd0 : signed_q;", "assign q = signed_q[7] ? 8'sd0 : signed_q;"),
 (R, "mantissa truncated to 23 bits", "wire [55:0] prod = mag * m;", "wire [55:0] prod = mag * {1'b0, m[22:0]};"),
 (R, "product truncated to 48 bits", "wire [55:0] prod = mag * m;", "wire [55:0] prod = (mag * m) & 56'hFFFFFFFFFFFF;"),
 (R, "shift amount off by one", "wire [56:0] shifted = sum >> s;", "wire [56:0] shifted = sum >> (s + 6'd1);"),
 (R, "shift uses only 5 bits", "wire [56:0] shifted = sum >> s;", "wire [56:0] shifted = sum >> s[4:0];"),
 (R, "most negative accumulator not handled (magnitude as 31 bits)", "wire [31:0] mag = neg ? (~acc + 32'd1) : acc;", "wire [31:0] mag = neg ? {1'b0, (~acc[30:0] + 31'd1)} : acc;"),
 (R, "ties round toward zero (rounding constant minus one)", "{1'b0, half};", "{1'b0, half} - 57'd1;"),
]
EQUIV = [
 (R, "EQUIVALENT: shift of 0 adds a half (1 << 63 is cut to 0 by the 56-bit wire)", "(s == 6'd0) ? 56'd0 :", ""),
 (R, "EQUIVALENT: clamp compares against 126 (a magnitude of 127 is returned as 127 either way)", "wire [56:0] limit = 57'd127;", "wire [56:0] limit = 57'd126;"),
 (R, "EQUIVALENT: compare against the limit with >= 128 instead of > 127", "(shifted > limit)", "(shifted >= 57'd128)"),
]
F = [R]
print("NOTE: this script deliberately breaks copies of the RTL. 'caught' lines are EXPECTED: they show the testbench can detect the mistake.")
c, n, l = hw.mutate([m for m in M if m[1]], F, "requant_tb", ["tb/requant_tb.v"]); print("\n".join(l)); print(f"\nrequantizer: {c} of {n} broken circuits caught")
c2, n2, l2 = hw.mutate(EQUIV, F, "requant_tb", ["tb/requant_tb.v"]); print("\nChanges that look like bugs and are not:"); print("\n".join(l2))
sys.exit(0 if c == n and c2 == 0 else 1)
