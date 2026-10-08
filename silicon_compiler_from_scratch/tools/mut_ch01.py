#!/usr/bin/env python3
"""Chapter 1: test the test. Break the adder one line at a time and check that the testbench notices. The last mutant is deliberately EQUIVALENT (it changes the circuit's text but not its behaviour), to show what a survivor looks like when it is not a gap in the test."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import hw
M = [
 ("rtl/adder4.v", "sum: the carry-in is left out of the sum", "assign sum  = a ^ b ^ cin;", "assign sum  = a ^ b;"),
 ("rtl/adder4.v", "sum: an AND instead of an XOR", "assign sum  = a ^ b ^ cin;", "assign sum  = a ^ b & cin;"),
 ("rtl/adder4.v", "carry: the carry-in term is dropped", "assign cout = (a & b) | (cin & (a ^ b));", "assign cout = (a & b);"),
 ("rtl/adder4.v", "carry: OR instead of AND between the generate and propagate terms", "assign cout = (a & b) | (cin & (a ^ b));", "assign cout = (a & b) & (cin & (a ^ b));"),
 ("rtl/adder4.v", "chain: every stage takes the adder's own carry-in", "full_adder fa(.a(a[i]), .b(b[i]), .cin(c[i]),", "full_adder fa(.a(a[i]), .b(b[i]), .cin(c[0]),"),
 ("rtl/adder4.v", "chain: stage i feeds stage i, not stage i + 1", ".cout(c[i + 1]));", ".cout(c[i]));"),
 ("rtl/adder4.v", "chain: the carry-in is tied to zero", "assign c[0] = cin;", "assign c[0] = 1'b0;"),
 ("rtl/adder4.v", "output: the carry-out is taken one stage early", "assign cout = c[4];", "assign cout = c[3];"),
 ("rtl/adder4.v", "wiring: bit 1 of b is replaced by bit 1 of a", ".b(b[i]),", ".b(i == 1 ? a[i] : b[i]),"),
 ("rtl/adder4.v", "structure: only three of the four stages are built", "for (i = 0; i < 4; i = i + 1)", "for (i = 0; i < 3; i = i + 1)"),
]
EQUIV = [("rtl/adder4.v", "EQUIVALENT: carry written with OR instead of XOR for the propagate term", "assign cout = (a & b) | (cin & (a ^ b));", "assign cout = (a & b) | (cin & (a | b));")]
F = ["rtl/adder4.v"]; TB = ["tb/adder4_tb.v"]
print("NOTE: this script deliberately breaks copies of rtl/adder4.v. 'caught' lines are EXPECTED: they show the testbench can detect the mistake.")
c, n, lines = hw.mutate(M, F, "adder4_tb", TB); print("\n".join(lines)); print(f"\n{c} of {n} broken circuits caught")
c2, n2, l2 = hw.mutate(EQUIV, F, "adder4_tb", TB); print("\nAn equivalent change (same behaviour, different text):"); print("\n".join(l2))
sys.exit(0 if c == n and c2 == 0 else 1)
