#!/usr/bin/env python3
"""Chapter 52: the INDEPENDENT reference. It computes the same payroll with Python's Fraction (exact rational arithmetic, rounded half up ONCE per quantity) and the tax by the way Publication 15-T prints its tables: a row for each bracket with a BASE amount (the tax on everything
below the row) plus a percentage of the excess over the row's lower limit -- where the C code sums slices. It shares no code with the kernel and prints exactly what native/pay_cli.c prints. Usage: pay_ref.py FILE"""
import sys
from fractions import Fraction
LIM = {0: [11925, 48475, 103350, 197300, 250525, 626350], 1: [23850, 96950, 206700, 394600, 501050, 751600], 2: [17000, 64850, 103350, 197300, 250500, 626350]}
RATE = [Fraction(p, 100) for p in (10, 12, 22, 24, 32, 35, 37)]; STD = {0: 15000, 1: 30000, 2: 22500}; MAXC = 100000000000
def rh(x):
    x = Fraction(x); n = int(x.numerator * 2 + x.denominator) // (2 * x.denominator) if x >= 0 else -(int(-x.numerator * 2 + x.denominator) // (2 * x.denominator)); return n
def table(status, halved):
    lim = [Fraction(v * 100) / (2 if halved else 1) for v in LIM[status]]; rows = []; base = Fraction(0); low = Fraction(0)
    for i in range(7):
        rows.append((low, base, RATE[i]))
        if i < 6: base += (lim[i] - low) * RATE[i]; low = lim[i]
    return rows
def annual_tax(status, halved, taxable):
    if taxable <= 0: return 0
    row = [r for r in table(status, halved) if r[0] < taxable][-1]; return rh(row[1] + (taxable - row[0]) * row[2])
def run(path):
    emp = {}; ytd = {}; n = 0; out = []
    for line in open(path):
        t = line.split()
        if not t: continue
        if t[0] == "E" and len(t) == 9: id = int(t[1]); emp[id] = dict(st=int(t[2]), s2=int(t[3]), per=int(t[4]), cr=int(t[5]), oi=int(t[6]), de=int(t[7]), ex=int(t[8])); ytd[id] = [0, 0, 0]
        elif t[0] == "P" and len(t) == 5:
            id, g, k, s = int(t[1]), int(t[2]), int(t[3]), int(t[4]); n += 1
            if id not in emp: out.append(f"pay {n} {id} unknown"); continue
            e = emp[id]; y = ytd[id]
            if e["st"] > 2: out.append(f"pay {n} {id} status"); continue
            if e["per"] not in (52, 26, 24, 12): out.append(f"pay {n} {id} periods"); continue
            if any(v < 0 or v > MAXC for v in (e["cr"], e["oi"], e["de"], e["ex"], g, k, s)): out.append(f"pay {n} {id} amount"); continue
            if k + s > g: out.append(f"pay {n} {id} deduct"); continue
            fitw = g - k - s; ficaw = g - s; halved = bool(e["s2"]); A = fitw * e["per"] + e["oi"] - e["de"]; std = Fraction(STD[e["st"]] * 100) / (2 if halved else 1)
            taxable = A - std; annual = max(0, annual_tax(e["st"], halved, taxable) - e["cr"]); fit = rh(Fraction(annual, e["per"])) + e["ex"]
            base = 17610000; maxtax = rh(Fraction(base * 62, 1000)); room = max(0, base - y[0]); ssw = min(ficaw, room); ss = min(rh(Fraction(ssw * 62, 1000)), max(0, maxtax - y[1]))
            med = rh(Fraction(ficaw * 145, 10000)); thr = 20000000; before = max(0, y[2] - thr); after = max(0, y[2] + ficaw - thr); add = rh(Fraction((after - before) * 9, 1000))
            net = g - fit - ss - med - add - k - s
            if net < 0: out.append(f"pay {n} {id} deduct"); continue
            y[0] += ssw; y[1] += ss; y[2] += ficaw
            out.append(f"pay {n} {id} ok {fitw} {ficaw} {fit} {ss} {med} {add} {net} {ss} {med} | ytd {y[0]} {y[1]} {y[2]}")
    print("\n".join(out))
run(sys.argv[1])
