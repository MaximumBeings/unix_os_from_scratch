#!/usr/bin/env python3
"""Chapter 52: writes data/pay/year.txt, the INVENTED company's payroll for one year: six employees with different filing statuses, pay frequencies and W-4 choices (one earns enough to cross the Social Security wage base and the Additional Medicare threshold).
Lines: E id status step2 periods credits_a other_income_a deductions_a extra_pp   and   P id gross k401 s125   (cents). Every name, salary and election is made up."""
import os
EMP = [  # id, status(0 S,1 MFJ,2 HOH), step2, periods, credits, other, deductions, extra, gross per period, k401, s125
    (1, 0, 0, 26, 0, 0, 0, 0, 200000, 0, 0),            # single, biweekly $2,000
    (2, 1, 0, 12, 400000, 0, 0, 0, 800000, 40000, 25000),  # married jointly, monthly $8,000, $400 401k, $250 health
    (3, 2, 0, 52, 200000, 0, 0, 500, 100000, 0, 0),      # head of household, weekly $1,000, credits $2,000, $5 extra
    (4, 0, 1, 24, 0, 0, 0, 0, 350000, 17500, 0),         # single, two jobs (step 2), semimonthly $3,500
    (5, 0, 0, 26, 0, 0, 0, 0, 950000, 57000, 15000),     # single, biweekly $9,500: crosses the wage base and the $200,000 threshold
    (6, 0, 0, 26, 0, 0, 0, 0, 50000, 0, 0),              # single, biweekly $500: no income tax
]
out = [f"E {i} {st} {s2} {per} {cr} {oi} {de} {ex}" for i, st, s2, per, cr, oi, de, ex, g, k, s in EMP]
for i, st, s2, per, cr, oi, de, ex, g, k, s in EMP:
    for n in range(per): out.append(f"P {i} {g} {k} {s}")
d = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data", "pay"); os.makedirs(d, exist_ok=True); open(os.path.join(d, "year.txt"), "w").write("\n".join(out) + "\n"); print(len(out), "lines")
