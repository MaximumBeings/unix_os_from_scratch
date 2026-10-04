#!/usr/bin/env python3
"""Chapter 52: random employees and pay runs for the differential test. gen(seed) -> text. Employees: every filing status and period count, the Step 2 checkbox, credits, other income, deductions, extra withholding, sometimes invalid (bad status or periods, negative or enormous amounts). Pay lines: ordinary
pay, pay that crosses the Social Security wage base and the Additional Medicare threshold during the year, deductions larger than gross, zero pay, and pay for unknown employees."""
import random, sys
def gen(seed, runs=30):
    R = random.Random(seed); out = []; emps = []
    for i in range(R.randrange(1, 6)):
        per = R.choice([52, 26, 24, 12, 12, 26, 26, 7 if R.random() < .05 else 26]); st = R.choice([0, 1, 2, 0, 1, 3 if R.random() < .05 else 0]); s2 = R.choice([0, 0, 1])
        cr = R.choice([0, 0, 200000, R.randrange(0, 900000)]); oi = R.choice([0, 0, R.randrange(0, 5000000)]); de = R.choice([0, 0, R.randrange(0, 3000000)]); ex = R.choice([0, 0, 5000, R.randrange(0, 20000)])
        if R.random() < .03: cr = -5
        base = R.choice([R.randrange(20000, 400000), R.randrange(100000, 2500000), R.randrange(400000, 9000000)]) * 52 // per; emps.append((i + 1, per, base)); out.append(f"E {i + 1} {st} {s2} {per} {cr} {oi} {de} {ex}")
    for r in range(runs):
        for id, per, base in emps:
            g = max(0, base + R.randrange(-base // 10, base // 10 + 1)) if R.random() < .95 else R.choice([0, 1, 99999999999, 100000000001]); k = R.choice([0, 0, g // 20, R.randrange(0, g + 1) if g else 0]); s = R.choice([0, 0, g // 30, R.randrange(0, g + 1) if g else 0])
            if R.random() < .04: k = g + 1
            out.append(f"P {id} {g} {k} {s}")
        if R.random() < .05: out.append(f"P 99 1000 0 0")
    return "\n".join(out) + "\n"
if __name__ == "__main__": sys.stdout.write(gen(int(sys.argv[1])))
