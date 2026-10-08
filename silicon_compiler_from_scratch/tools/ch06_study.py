#!/usr/bin/env python3
"""Chapter 6: how close is the fixed-point softmax to the real one? Compares the integer model (the circuit's behaviour, bit-exact to the tests) with floating-point softmax of x/16 on random inputs. Fixed seeds."""
import os, random, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "model")); import softmax_gold as g
R = random.Random(66)
print(f"{'N':>3s} {'inputs':<22s} {'cases':>6s} {'max abs err':>12s} {'in 2^-16':>9s} {'worst |sum-1|':>14s} {'argmax kept':>12s}")
for N in (4, 8, 64):
    for name, gen in (("uniform -128..127", lambda: [R.randrange(-128, 128) for _ in range(N)]), ("narrow -16..15", lambda: [R.randrange(-16, 16) for _ in range(N)]),
                      ("one big, rest small", lambda: [R.randrange(100, 128)] + [R.randrange(-128, -64) for _ in range(N - 1)])):
        worst = 0.0; wsum = 0.0; kept = 0; cases = 3000
        for _ in range(cases):
            xs = gen(); p = g.softmax_fixed(xs); f = g.softmax_float(xs)
            worst = max(worst, max(abs(a / 65536 - b) for a, b in zip(p, f))); wsum = max(wsum, abs(sum(p) / 65536 - 1))
            top = max(xs); kept += all(p[i] == max(p) for i in range(N) if xs[i] == top)          # every maximal input has the maximal probability
        print(f"{N:3d} {name:<22s} {cases:6d} {worst:12.2e} {worst*65536:9.2f} {wsum:14.2e} {kept/cases:12.3f}")
print("\nentries of the exp table that are zero (the probability of anything that far below the maximum is rounded to 0):")
z = [d for d in range(256) if g.T[d] == 0][0]; print(f"first zero entry d = {z}: exp(-{z}/16) = {__import__('math').exp(-z/16):.3e} (half a unit of 65535 = {0.5/65535:.3e})")
print("the divider's range: s = sum of e_i lies between", 65535, "and", 64 * 65535, "so r = 2^38 / s needs", (2**38 // 65535).bit_length(), "bits at most and the product e_i * r needs", (65535 * (2**38 // 65535)).bit_length(), "bits")
