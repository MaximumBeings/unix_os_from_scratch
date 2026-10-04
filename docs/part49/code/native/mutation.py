#!/usr/bin/env python3
"""READ THIS FIRST: this script deliberately BREAKS the chapter's sources, one line at a time, in a temporary copy, and runs the host-side test suite against each broken copy. It expects the suite to FAIL every time. A "caught"
line is the EXPECTED, wanted result: it shows the tests can detect that mistake. "NOT CAUGHT" would be a gap. The real sources are never modified.
For each mutant it runs: claims_test (worked by hand: numbers, envelope, claim rules, adjudication, 835), a differential test (C vs the independent Python reference on 150 random claim sets, adjudication + 835 + reconciliation), and the
fuzzer's PROPERTY check (anything accepted as a claim adjudicates consistently and builds an 835 that passes the strict check and reconciles). Output: mutation_out.txt   Usage: mutation.py   (about 15 minutes)"""
import os, shutil, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__)); code = os.path.join(here, "..")
MUT = [
 ("049_x12.c", "amounts: three decimals accepted", "if (fd == 2) { return -1; }", "if (fd == 3) { return -1; }"),
 ("049_x12.c", "amounts: '12.5' read as 12.05 (the one-decimal case is not scaled)", "if (fd == 1) { frac *= 10; }", ""),
 ("049_x12.c", "amounts: the minus sign is ignored", "*cents = neg ? -v : v;", "*cents = v;"),
 ("049_x12.c", "dates: every year divisible by 4 is a leap year", "int leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;", "int leap = (y % 4 == 0) || y % 400 == 0;"),
 ("049_x12.c", "ISA: the field-separator position of ISA06 is off by one", "{3, 6, 17, 20, 31, 34, 50, 53, 69, 76, 81, 83, 89, 99, 101, 103}", "{3, 6, 17, 20, 31, 34, 51, 53, 69, 76, 81, 83, 89, 99, 101, 103}"),
 ("049_x12.c", "ISA: the component separator may equal the element separator", "x->esep == x->csep ||", ""),
 ("049_x12.c", "envelope: SE01 is compared with one segment fewer", "cnt != (uint32_t)(i - st_i + 1)", "cnt != (uint32_t)(i - st_i)"),
 ("049_x12.c", "envelope: SE02 is never compared with ST02", "if (!same) { SOFT(X12_ERR_CTL_ST); } }", "if (0) { SOFT(X12_ERR_CTL_ST); } }"),
 ("049_x12.c", "envelope: GE01 may be larger than the number of transaction sets", "c2 != sets", "c2 < sets"),
 ("049_x12.c", "envelope: IEA01 may be larger than the number of groups", "c3 != groups", "c3 < groups"),
 ("049_x12.c", "envelope: the control-number comparison with GS06 is never made", "if (!same) { SOFT(X12_ERR_CTL_GS); } }", "if (0) { SOFT(X12_ERR_CTL_GS); } }"),
 ("049_x12.c", "envelope: IEA02 is never compared with ISA13", "if (!same) { SOFT(X12_ERR_CTL_ISA); } }", "if (0) { SOFT(X12_ERR_CTL_ISA); } }"),
 ("049_x12.c", "lenient mode forgives everything, even when not asked to", "if (x->lenient) { x->warn |=", "if (1) { x->warn |="),
 ("049_x12.c", "envelope: a GS inside a transaction set is accepted", "if (x12_is(&x->seg[i], \"ST\") || x12_is(&x->seg[i], \"GS\") || x12_is(&x->seg[i], \"GE\") || x12_is(&x->seg[i], \"IEA\"))", "if (x12_is(&x->seg[i], \"ST\") || x12_is(&x->seg[i], \"GE\") || x12_is(&x->seg[i], \"IEA\"))"),
 ("049_x12.c", "envelope: data after IEA is accepted", "i++; if (i != n) { x->err_seg = i; return X12_ERR_AFTER_IEA; }", "i++;"),
 ("049_x12.c", "segments: control characters inside a segment are accepted", "if ((unsigned char)t[p] < 0x20 && !is_ws(t[p]))", "if (0)"),
 ("049_x12.c", "segments: one-letter identifiers are accepted", "if (idl < 2 || idl > 3)", "if (idl > 3)"),
 ("049_claim.c", "NPI: the Luhn doubling starts at the wrong digit", "if (pos % 2 == 1) { d *= 2;", "if (pos % 2 == 0) { d *= 2;"),
 ("049_claim.c", "NPI: the wrong card-issuer prefix (80841 instead of 80840)", "char all[15] = {'8', '0', '8', '4', '0'};", "char all[15] = {'8', '0', '8', '4', '1'};"),
 ("049_claim.c", "HL: a number larger than the next one is accepted", "id != (uint32_t)(s.hl_n + 1)", "id < (uint32_t)(s.hl_n + 1)"),
 ("049_claim.c", "HL: a subscriber's parent need not be the billing provider", "(int)parent != s.hl20 || s.hl20 == 0", "s.hl20 == 0"),
 ("049_claim.c", "a dependent patient (HL level 23) is no longer refused", "x12_str_eq(p3, l3, \"23\")) { FAIL(CL_ERR_UNSUPPORTED", "x12_str_eq(p3, l3, \"24\")) { FAIL(CL_ERR_UNSUPPORTED"),
 ("049_claim.c", "balance: a claim total ABOVE the lines is accepted", "if (sum != c->total) {", "if (sum > c->total) {"),
 ("049_claim.c", "a zero claim total is accepted", "c->total <= 0", "c->total < 0"),
 ("049_claim.c", "units: 1000 accepted", "ln_->units < 1 || ln_->units > 999", "ln_->units < 1 || ln_->units > 1000"),
 ("049_claim.c", "a diagnosis pointer one past the last diagnosis is accepted", "ptr < 1 || ptr > c->n_dx", "ptr < 1 || ptr > c->n_dx + 1"),
 ("049_claim.c", "LX numbering: a larger number is accepted", "ln != (uint32_t)(c->n_line + 1)", "ln < (uint32_t)(c->n_line + 1)"),
 ("049_claim.c", "an impossible date of service is accepted", "if (x12_date(ds, dl, &d) != 0)", "if (0)"),
 ("049_claim.c", "the patient control number may have 39 characters", "l1 == 0 || l1 > 38 ||", "l1 == 0 || l1 > 39 ||"),
 ("049_claim.c", "SV103 need not be UN", "if (!u || !x12_str_eq(u, l2, \"UN\"))", "if (0)"),
 ("049_claim.c", "the member id buffer is declared larger than it is (overflows)", "copy_n(sub_id, 26, e3, l3)", "copy_n(sub_id, 30, e3, l3)"),
 ("049_adjud.c", "duplicates: the date of service is not compared", "h->dos == ln->dos &&", ""),
 ("049_adjud.c", "duplicates: the charge is not compared", "&& h->charge == ln->charge", ""),
 ("049_adjud.c", "an uncovered code is reported as CO-97 instead of CO-96", "add_adj(l, ADJ_CO, 96, ln->charge);", "add_adj(l, ADJ_CO, 97, ln->charge);"),
 ("049_adjud.c", "the allowed amount may exceed the charge", "int64_t allowed = ln->charge < cap ? ln->charge : cap;", "int64_t allowed = cap;"),
 ("049_adjud.c", "units do not multiply the fee", "int64_t cap = (int64_t)fee * (int64_t)ln->units;", "int64_t cap = (int64_t)fee;"),
 ("049_adjud.c", "the co-pay applies to every office-visit line, not once per date", "if (!seen) { em_dos[n_em++] = ln->dos;", "if (1) { em_dos[n_em++] = ln->dos;"),
 ("049_adjud.c", "the co-pay may exceed the allowed amount", "cp = (int64_t)s->plan.copay < allowed ? (int64_t)s->plan.copay : allowed;", "cp = (int64_t)s->plan.copay;"),
 ("049_adjud.c", "the deductible may exceed what is left of the allowed amount", "int64_t d = ded_left < after_cp ? ded_left : after_cp;", "int64_t d = ded_left;"),
 ("049_adjud.c", "coinsurance is truncated instead of rounded half up", "(uint64_t)base * (uint64_t)s->plan.coins_bp + 5000u", "(uint64_t)base * (uint64_t)s->plan.coins_bp"),
 ("049_adjud.c", "the out-of-pocket cap never cuts the co-pay", "cut = excess < cp ? excess : cp; cp -= cut; excess -= cut;", ""),
 ("049_adjud.c", "the out-of-pocket cap leaves one cent too much", "if (pr > room) {", "if (pr > room + 1) {"),
 ("049_adjud.c", "the out-of-pocket accumulator adds the deductible only", "s->plan.oop_met += (uint32_t)pr;", "s->plan.oop_met += (uint32_t)d;"),
 ("049_adjud.c", "a claim whose lines are all denied is still 'processed as primary'", "out->status = all_denied ? 4 : 1;", "out->status = 1;"),
 ("049_remit.c", "835: a second CAS adjustment is written without the empty quantity element", "ps(&w, first ? \"*\" : \"**\");", "ps(&w, \"*\");"),
 ("049_remit.c", "835: BPR02 totals the patient share instead of the plan's payment", "total += res[i].total_paid;", "total += res[i].total_pr;"),
 ("049_remit.c", "reconcile: service lines are never reported unbalanced", "l->balanced = (l->charge - l->paid == l->cas_sum);", "l->balanced = 1;"),
 ("049_remit.c", "reconcile: claim-level adjustments are ignored in favour of the service lines", "int64_t expect = cc->has_claim_cas ? cc->claim_cas : svc_cas;", "int64_t expect = svc_cas;"),
 ("049_remit.c", "reconcile: provider-level adjustments are added instead of subtracted", "r->bpr_balanced = (r->bpr_total == r->paid_sum - r->plb_sum);", "r->bpr_balanced = (r->bpr_total == r->paid_sum + r->plb_sum);"),
 ("049_remit.c", "reconcile: only every third PLB element is read as an amount", "for (int k = 4; k <= g->n_el; k += 2)", "for (int k = 4; k <= g->n_el; k += 3)"),
 ("049_remit.c", "reconcile: patient responsibility is counted from CO instead of PR adjustments", "int is_pr = x12_str_eq(grp, l, \"PR\");", "int is_pr = x12_str_eq(grp, l, \"CO\");"),
 ("049_remit.c", "reconcile: the patient-responsibility check always passes", "cc->pr_matches = (cc->patient == prsum);", "cc->pr_matches = 1;"),
 ("049_remit.c", "reconcile: a CAS before any CLP is silently ignored", "if (!c) { if (x12_is(g, \"SVC\") || x12_is(g, \"CAS\")) { RFAIL(RM_ERR_STRUCTURE, \"an SVC or CAS segment before any CLP (claim)\"); } continue; }", "if (!c) { continue; }"),
 ("claims_ref.py", "reference: coinsurance rounds down (the oracle itself broken)", "rounding=ROUND_HALF_UP", "rounding=ROUND_DOWN"),
 ("claims_ref.py", "reference: 99215 is not an office-visit code (the oracle itself broken)", "\"99214\", \"99215\"}", "\"99214\"}"),
]
def run(tmp, cmd, **kw): return subprocess.run(cmd, cwd=tmp, capture_output=True, text=True, errors="replace", **kw)
import concurrent.futures
SRC = ["../049_x12.c", "../049_claim.c", "../049_adjud.c", "../049_remit.c"]; G = ["gcc", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined"]
def suite(tmp):
    failed = []; n = os.path.join(tmp, "native")
    b = run(n, G + ["claims_test.c"] + SRC + ["-o", "ct"])
    if b.returncode != 0: failed.append("claims_test (did not build)")
    elif run(n, ["./ct"]).returncode != 0: failed.append("claims_test")
    b = run(n, G + ["claims_cli.c"] + SRC + ["-o", "cli"])
    if b.returncode != 0: failed.append("the engine (did not build)"); return failed
    if run(n, [sys.executable, "diff_claims.py", "100", "./cli"], env=dict(os.environ, STOP_AT_FIRST="1")).returncode != 0: failed.append("random claim sets vs the Python reference")
    for L in "AE":
        c = run(n, ["./cli", "adj", f"../data/claim_{L}.837"]); p = run(n, [sys.executable, "../claims_ref.py", "adj", f"../data/claim_{L}.837"])
        if c.stdout != p.stdout or c.returncode != 0 or c.stderr: failed.append("invented claims vs the Python reference"); break
    b = run(n, G + ["claims_fuzz.c"] + SRC + ["-o", "fz"])
    if b.returncode != 0: failed.append("fuzz property (did not build)")
    else:
        run(n, ["sh", "-c", "for x in A D E; do ./cli build ../data/claim_$x.837 > built_$x.835; done"])
        if run(n, ["./fz", "400", "../data/claim_A.837", "../data/claim_E.837", "../data/real/835_mult_loops.txt", "built_A.835", "built_D.835"]).returncode != 0: failed.append("fuzz property")
    return failed
print("NOTE: this script deliberately breaks copies of the chapter's sources. 'caught' lines are EXPECTED: they show the tests can detect the mistake."); sys.stdout.flush()
base = tempfile.mkdtemp(prefix="c49mut_"); shutil.copytree(code, os.path.join(base, "base"), ignore=shutil.ignore_patterns("build", "*.o", "__pycache__"))
f0 = suite(os.path.join(base, "base")); print("baseline (nothing broken):", "all tests pass (expected)" if not f0 else "UNEXPECTED FAILURES " + str(f0)); sys.stdout.flush()
def one(i):
    fn, label, old, new = MUT[i]; tmp = os.path.join(base, "m%d" % i); shutil.copytree(os.path.join(base, "base"), tmp)
    p = os.path.join(tmp, fn); s = open(p, encoding="utf-8").read(); assert s.count(old) == 1, (label, s.count(old)); open(p, "w", encoding="utf-8").write(s.replace(old, new))
    failed = suite(tmp); shutil.rmtree(tmp, ignore_errors=True); return label, failed
caught = 0
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:   # four broken copies at a time, each in its own directory; results are printed in list order
    for label, failed in pool.map(one, range(len(MUT))):
        if failed: caught += 1; print(f"{label}: caught by {', '.join(failed)}")
        else: print(f"{label}: NOT CAUGHT")
        sys.stdout.flush()
print(f"\n{caught} of {len(MUT)} broken versions caught"); shutil.rmtree(base, ignore_errors=True)
