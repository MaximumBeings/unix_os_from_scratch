#!/usr/bin/env python3
"""Appendix A (digital logic): runnable checks of the ideas of the primer. Pure Python. Usage: appx_a.py"""
import itertools, re
print("== 1. gates are functions of bits: the truth tables of the basic gates")
G = {"AND": lambda a, b: a & b, "OR": lambda a, b: a | b, "XOR": lambda a, b: a ^ b, "NAND": lambda a, b: 1 - (a & b), "NOR": lambda a, b: 1 - (a | b), "XNOR": lambda a, b: 1 - (a ^ b)}
print("  a b | " + " ".join(f"{n:>4s}" for n in G))
for a, b in itertools.product((0, 1), repeat=2): print(f"  {a} {b} | " + " ".join(f"{G[n](a, b):4d}" for n in G))
print("\n== 2. NAND alone is enough (universality): build NOT, AND, OR, XOR from NAND gates only and compare with the real operators on every input")
nand = G["NAND"]; NOT = lambda a: nand(a, a); AND = lambda a, b: NOT(nand(a, b)); OR = lambda a, b: nand(NOT(a), NOT(b))
def XOR(a, b): n = nand(a, b); return nand(nand(a, n), nand(b, n))
ok = all(NOT(a) == 1 - a and AND(a, b) == (a & b) and OR(a, b) == (a | b) and XOR(a, b) == (a ^ b) for a in (0, 1) for b in (0, 1))
print(f"  NOT = 1 NAND, AND = 2 NANDs, OR = 3 NANDs, XOR = 4 NANDs; all four agree with Python's operators on all inputs: {ok}")
print("\n== 3. combinational circuits: a full adder from gates, then a 4-bit ripple adder checked on all 512 input combinations")
def full_adder(a, b, c): p = XOR(a, b); return XOR(p, c), OR(AND(a, b), AND(p, c))
def ripple4(a, b, cin=0):
    s = 0; c = cin
    for i in range(4): bit, c = full_adder((a >> i) & 1, (b >> i) & 1, c); s |= bit << i
    return s | (c << 4)
bad = [(a, b, c) for a in range(16) for b in range(16) for c in (0, 1) if ripple4(a, b, c) != a + b + c]
print(f"  512 cases (a, b in 0..15, carry-in 0/1): {512 - len(bad)} correct, {len(bad)} wrong")
print("  the carry of bit i depends on the carry of bit i-1: depth of the ripple adder in gate levels: 2 per bit for the carry (AND-OR) + 2 for the last sum:")
for n in (4, 8, 16, 32, 64): print(f"    {n:2d} bits: about {2 * n + 2:3d} gate levels (ripple);  a lookahead adder needs about {2 * (n - 1).bit_length() + 4:2d}")
print("\n== 4. sequential circuits: a clocked simulator. State changes only on a rising edge, from the values BEFORE the edge")
def clocked(step, state, inputs):
    trace = []
    for x in inputs: state = step(state, x); trace.append(state)
    return trace
print("  4-bit shift register, input stream 1,0,1,1,0,0,1,0 (new bit enters at the right):")
shift = lambda s, x: ((s << 1) | x) & 15
for i, st in enumerate(clocked(shift, 0, [1, 0, 1, 1, 0, 0, 1, 0])): print(f"    edge {i + 1}: {st:04b}")
print("  a 2-bit counter with enable (counts only when en = 1), reset at start:")
cnt = lambda s, en: (s + en) & 3
print("    states:", clocked(cnt, 0, [1, 1, 0, 1, 1, 1, 0, 0, 1]))
print("\n== 5. finite-state machine: a Moore detector for the pattern 1 0 1 (overlapping allowed)")
T_ = {("S0", 0): "S0", ("S0", 1): "S1", ("S1", 0): "S2", ("S1", 1): "S1", ("S2", 0): "S0", ("S2", 1): "S3", ("S3", 0): "S2", ("S3", 1): "S1"}
print("  state table (S3 means: the last three bits were 101; output 1 only in S3):")
for s in ("S0", "S1", "S2", "S3"): print(f"    {s}: on 0 -> {T_[(s, 0)]}, on 1 -> {T_[(s, 1)]}   output {1 if s == 'S3' else 0}")
bits = "1101011010101001"; s = "S0"; out = ""
for ch in bits: s = T_[(s, int(ch))]; out += "1" if s == "S3" else "0"
want = "".join("1" if bits[max(0, i - 2):i + 1] == "101" else "0" for i in range(len(bits)))
print(f"  input  {bits}\n  output {out}\n  reference (a window over the last three bits) {want}  -> identical: {out == want}")
print("\n== 6. timing: why a clock has a speed limit (derived arithmetic, ASSUMED delays in ns)")
tcq, tsu = 0.30, 0.10
for name, tl in (("a 4-bit ripple adder between registers", 4 * 0.5 + 0.3), ("a 32-bit ripple adder", 32 * 0.5 + 0.3), ("a 32-bit lookahead adder", 8 * 0.5)):
    T = tcq + tl + tsu; print(f"  {name:42s}: path = clk-to-q {tcq} + logic {tl:.1f} + setup {tsu} = {T:.1f} ns -> at most {1000 / T:5.0f} MHz")
