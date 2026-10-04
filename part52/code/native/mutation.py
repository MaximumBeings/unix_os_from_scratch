#!/usr/bin/env python3
"""READ THIS FIRST: this script deliberately BREAKS the chapter's sources, one line at a time, in a temporary copy, and runs the host-side test suite against each broken copy. It expects the suite to FAIL every time. A "caught" line is the EXPECTED, wanted
result: it shows the tests can detect that mistake. "NOT CAUGHT" would be a gap. The real sources are never modified. For each mutant it runs: pay_test (worked by hand), the differential test against the independent Python reference (40 random companies) and the fuzzer (invariants after every call). Four broken copies are tested at a time. Output: mutation_out.txt   Usage: mutation.py"""
import concurrent.futures, os, shutil, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__)); code = os.path.join(here, "..")
MUT = [
 ('052_payroll.c', 'rounding: half rounds down instead of up', 'udm(2 * m + (uint64_t)d, 2 * (uint64_t)d)', 'udm(2 * m + (uint64_t)d - 1, 2 * (uint64_t)d)'),
 ('052_payroll.c', 'rounding: truncation instead of rounding', 'udm(2 * m + (uint64_t)d, 2 * (uint64_t)d)', 'udm(m, (uint64_t)d)'),
 ('052_payroll.c', 'an unknown filing status 3 is accepted', 'if (e->status > PAY_HOH) { return PAY_ERR_STATUS; }', 'if (e->status > 3) { return PAY_ERR_STATUS; }'),
 ('052_payroll.c', '7 pay periods a year is accepted', 'e->periods != 24 && e->periods != 12)', 'e->periods != 24 && e->periods != 12 && e->periods != 7)'),
 ('052_payroll.c', '12 pay periods is refused', 'e->periods != 24 && e->periods != 12)', 'e->periods != 24)'),
 ('052_payroll.c', 'single: first bracket limit is $11,925 -> $11,926', '{{11925, 48475,', '{{11926, 48475,'),
 ('052_payroll.c', 'single: 37% bracket starts at the wrong figure', '197300, 250525, 626350}, {23850', '197300, 250525, 626351}, {23850'),
 ('052_payroll.c', 'MFJ: second limit wrong', '{23850, 96950,', '{23850, 96951,'),
 ('052_payroll.c', 'MFJ: 35% limit wrong', '394600, 501050, 751600}', '394600, 501051, 751600}'),
 ('052_payroll.c', 'HOH: first limit wrong', '{17000, 64850,', '{17001, 64850,'),
 ('052_payroll.c', 'HOH: 24% limit wrong', '103350, 197300, 250500, 626350}}', '103350, 197301, 250500, 626350}}'),
 ('052_payroll.c', 'rate: the 22% bracket taxed at 23%', '1000, 1200, 2200, 2400', '1000, 1200, 2300, 2400'),
 ('052_payroll.c', 'rate: top bracket 37% taxed at 35%', '3500, 3700}', '3500, 3500}'),
 ('052_payroll.c', 'standard deduction: single $15,000 -> $14,600', 'STD[3] = {15000,', 'STD[3] = {14600,'),
 ('052_payroll.c', 'standard deduction: MFJ halved wrongly', '30000, 22500}', '15000, 22500}'),
 ('052_payroll.c', 'standard deduction: HOH wrong', '30000, 22500}', '30000, 21900}'),
 ('052_payroll.c', 'bracket arithmetic: the tax on a bracket uses the next rate', 'num += (top - prev) * RATE[i]; prev = top;', 'num += (top - prev) * RATE[i + 1 < 7 ? i + 1 : i]; prev = top;'),
 ('052_payroll.c', 'Step 2 box: standard deduction not halved', 'if (halved) { std /= 2; }', 'if (0) { std /= 2; }'),
 ('052_payroll.c', 'Step 2 box: bracket limits not halved', 'if (halved && top > 0) { top /= 2; }', 'if (0 && top > 0) { top /= 2; }'),
 ('052_payroll.c', 'annualization uses 52 periods always', 'int64_t A = r->fit_wages * e->periods', 'int64_t A = r->fit_wages * 52'),
 ('052_payroll.c', 'annual tax divided by 52 not the real periods', 'r->fit = rdiv(annual, e->periods) + e->extra_pp;', 'r->fit = rdiv(annual, 52) + e->extra_pp;'),
 ('052_payroll.c', 'other income is subtracted', '+ e->other_income_a - e->deductions_a', '- e->other_income_a - e->deductions_a'),
 ('052_payroll.c', 'deductions (Step 4b) are added', 'e->other_income_a - e->deductions_a;', 'e->other_income_a + e->deductions_a;'),
 ('052_payroll.c', 'credits are ignored', 'annual -= e->credits_a;', 'annual -= 0;'),
 ('052_payroll.c', 'a credit can drive the tax negative', 'if (annual < 0) { annual = 0; }', ''),
 ('052_payroll.c', 'extra withholding is not added', 'r->fit = rdiv(annual, e->periods) + e->extra_pp;', 'r->fit = rdiv(annual, e->periods);'),
 ('052_payroll.c', '401(k) is not taken out of income-tax wages', 'r->fit_wages = p->gross - p->k401 - p->s125;', 'r->fit_wages = p->gross - p->s125;'),
 ('052_payroll.c', 'Section 125 is not taken out of income-tax wages', 'r->fit_wages = p->gross - p->k401 - p->s125;', 'r->fit_wages = p->gross - p->k401;'),
 ('052_payroll.c', '401(k) wrongly reduces FICA wages', 'r->fica_wages = p->gross - p->s125;', 'r->fica_wages = p->gross - p->s125 - p->k401;'),
 ('052_payroll.c', 'Section 125 does not reduce FICA wages', 'r->fica_wages = p->gross - p->s125;', 'r->fica_wages = p->gross;'),
 ('052_payroll.c', 'Social Security rate 6.2% -> 6.0%', 'rdiv(ssw * 620, 10000); int64_t tax_room', 'rdiv(ssw * 600, 10000); int64_t tax_room'),
 ('052_payroll.c', 'wage base $176,100 -> $168,600', 'base = 17610000,', 'base = 16860000,'),
 ('052_payroll.c', 'wage base cap ignored (room not limited)', 'int64_t ssw = r->fica_wages < room ? r->fica_wages : room;', 'int64_t ssw = r->fica_wages;'),
 ('052_payroll.c', 'the maximum-tax cap is not applied', 'if (ss > tax_room) { ss = tax_room; }', ''),
 ('052_payroll.c', 'Social Security wages in the year-to-date count uncapped wages', 'ytd->ss_wages += ssw;', 'ytd->ss_wages += r->fica_wages;'),
 ('052_payroll.c', 'Social Security rounding is done on the year not the period (tax_room off by one)', 'int64_t tax_room = max_tax - ytd->ss_tax;', 'int64_t tax_room = max_tax - ytd->ss_tax - 1;'),
 ('052_payroll.c', 'Medicare rate 1.45% -> 1.5%', 'r->med = rdiv(r->fica_wages * 145, 10000)', 'r->med = rdiv(r->fica_wages * 150, 10000)'),
 ('052_payroll.c', 'Additional Medicare threshold $200,000 -> $250,000', 'thr = 20000000;', 'thr = 25000000;'),
 ('052_payroll.c', 'Additional Medicare rate 0.9% -> 1.45%', 'rdiv((after - before) * 90, 10000)', 'rdiv((after - before) * 145, 10000)'),
 ('052_payroll.c', 'Additional Medicare charged on the whole period once over', 'int64_t after = ytd->med_wages + r->fica_wages > thr ? ytd->med_wages + r->fica_wages - thr : 0;', 'int64_t after = ytd->med_wages + r->fica_wages > thr ? r->fica_wages : 0;'),
 ('052_payroll.c', 'Additional Medicare ignores earlier wages (before=0)', 'int64_t before = ytd->med_wages > thr ? ytd->med_wages - thr : 0;', 'int64_t before = 0;'),
 ('052_payroll.c', 'the employer also pays Additional Medicare (er_med includes it)', 'r->er_med = r->med;', 'r->er_med = r->med + 1;'),
 ('052_payroll.c', "employer's Social Security share differs", 'r->ss = ss; r->er_ss = ss;', 'r->ss = ss; r->er_ss = ss + 0 * 1 + (ss > 0 ? 1 : 0);'),
 ('052_payroll.c', 'net pay forgets the 401(k)', '- r->addmed - p->k401 - p->s125;', '- r->addmed - p->s125;'),
 ('052_payroll.c', 'net pay forgets Additional Medicare', 'r->net = p->gross - r->fit - r->ss - r->med - r->addmed', 'r->net = p->gross - r->fit - r->ss - r->med'),
 ('052_payroll.c', 'deductions equal to gross are refused', 'if (p->k401 + p->s125 > p->gross)', 'if (p->k401 + p->s125 >= p->gross)'),
 ('052_payroll.c', 'a negative amount is accepted', 'if (amts[i] < 0 || amts[i] > PAY_MAX_CENTS) { return PAY_ERR_AMOUNT; }', 'if (amts[i] > PAY_MAX_CENTS) { return PAY_ERR_AMOUNT; }'),
 ('052_payroll.c', 'an amount of exactly the maximum is refused', 'amts[i] > PAY_MAX_CENTS)', 'amts[i] >= PAY_MAX_CENTS)'),
 ('052_payroll.c', 'the loop over amounts skips extra withholding (index 3)', 'for (int i = 0; i < 7; i++) { if (amts[i] < 0', 'for (int i = 0; i < 7; i++) { if (i != 3 && amts[i] < 0'),
 ('052_payroll.c', 'the loop over amounts skips the section 125 amount (index 6)', 'for (int i = 0; i < 7; i++) { if (amts[i] < 0', 'for (int i = 0; i < 6; i++) { if (amts[i] < 0'),
 ('052_payroll.c', 'a negative year-to-date is accepted', 'if (ytd->ss_wages < 0 || ytd->ss_tax < 0 || ytd->med_wages < 0 ||', 'if (ytd->ss_wages < 0 || ytd->med_wages < 0 ||'),
 ('052_payroll.c', 'a refused withholding still updates the year-to-date', 'if (pay_check(p, r)) { return PAY_ERR_DEDUCT; }', 'if (pay_check(p, r)) { ytd->med_wages += r->fica_wages; return PAY_ERR_DEDUCT; }'),
 ('052_payroll.c', 'the year-to-date Medicare wages are not advanced', 'ytd->med_wages += r->fica_wages;\n', '\n'),
 ('052_payroll.c', 'pay_check: a negative net is allowed', '|| r->net < 0 ||', '||'),
 ('pay_ref.py', 'reference: Additional Medicare threshold wrong (the oracle itself broken)', '200000', '250000'),
]
def run(tmp, cmd, **kw): return subprocess.run(cmd, cwd=tmp, capture_output=True, text=True, errors="replace", **kw)
SRC = ["../052_payroll.c"]; G = ["gcc", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined"]
def suite(tmp):
    failed = []; n = os.path.join(tmp, "native")
    b = run(n, G + ["pay_test.c"] + SRC + ["-o", "pt"])
    if b.returncode != 0: failed.append("pay_test (did not build)")
    elif run(n, ["./pt"]).returncode != 0: failed.append("pay_test")
    b = run(n, G + ["pay_cli.c"] + SRC + ["-o", "cli"])
    if b.returncode != 0: failed.append("the engine (did not build)"); return failed
    if run(n, [sys.executable, "diff_pay.py", "40", "./cli"], env=dict(os.environ, STOP_AT_FIRST="1")).returncode != 0: failed.append("random companies vs the Python reference")
    b = run(n, G + ["pay_fuzz.c"] + SRC + ["-o", "fz"])
    if b.returncode != 0: failed.append("fuzz (did not build)")
    elif run(n, ["./fz", "300"]).returncode != 0: failed.append("fuzz")
    return failed
print("NOTE: this script deliberately breaks copies of the chapter's sources. 'caught' lines are EXPECTED: they show the tests can detect the mistake."); sys.stdout.flush()
base = tempfile.mkdtemp(prefix="c51mut_"); shutil.copytree(code, os.path.join(base, "base"), ignore=shutil.ignore_patterns("build", "*.o", "__pycache__"))
f0 = suite(os.path.join(base, "base")); print("baseline (nothing broken):", "all tests pass (expected)" if not f0 else "UNEXPECTED FAILURES " + str(f0)); sys.stdout.flush()
def one(i):
    fn, label, old, new = MUT[i]; tmp = os.path.join(base, "m%d" % i); shutil.copytree(os.path.join(base, "base"), tmp)
    p = os.path.join(tmp, fn); s = open(p, encoding="utf-8").read(); assert s.count(old) == 1, (label, s.count(old)); open(p, "w", encoding="utf-8").write(s.replace(old, new))
    failed = suite(tmp); shutil.rmtree(tmp, ignore_errors=True); return label, failed
caught = 0
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    for label, failed in pool.map(one, range(len(MUT))):
        if failed: caught += 1; print(f"{label}: caught by {', '.join(failed)}")
        else: print(f"{label}: NOT CAUGHT")
        sys.stdout.flush()
print(f"\n{caught} of {len(MUT)} broken versions caught"); shutil.rmtree(base, ignore_errors=True)
