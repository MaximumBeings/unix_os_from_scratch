#!/usr/bin/env python3
"""Chapter 47: independent check of the platform fee. The kernel computes (total*10 + 50) / 100 + 30 in 32-bit integers. This reference never uses that formula: it takes 10% of the total as an exact
DECIMAL number, rounds it half UP to a whole cent with Python's decimal module, and adds 30 cents. Usage: ledger_ref.py ledger_out.txt   (the file ledger_test printed)"""
import sys
from decimal import Decimal, ROUND_HALF_UP
n = bad = 0
for line in open(sys.argv[1]):
    if line.startswith("FEE "):
        _, t, f = line.split(); t = int(t); f = int(f)
        want = int((Decimal(t) / Decimal(10)).quantize(Decimal(1), rounding=ROUND_HALF_UP)) + 30
        n += 1
        if f != want: bad += 1; print("DIFF total", t, "kernel", f, "decimal", want) if bad < 5 else None
print(f"{n} sale totals (0 to {n - 1} cents) checked against decimal half-up rounding: {'all identical' if not bad else str(bad) + ' differ'}")
sys.exit(1 if bad else 0)
