#!/usr/bin/env python3
"""READ THIS FIRST: this script deliberately BREAKS the chapter's sources, one line at a time, in a temporary copy, and runs the host-side test suite against each broken copy. It expects the
suite to FAIL every time. A "caught" line is the EXPECTED, wanted result: it shows the tests can detect that mistake. "NOT CAUGHT" would be a gap. The real sources are never modified.
For each mutant it runs: xbrl_test (arithmetic, parsers, hand-computed answers, malformed documents), the six real filings (C vs Python), 150 random filings (C vs Python), the fetcher tests, and
a check that edgar_slim.py still reproduces the embedded filings byte for byte from the full EDGAR documents. Output: mutation_out.txt   Usage: mutation.py   (about 12 minutes)"""
import os, shutil, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__)); code = os.path.join(here, "..")
FX = os.environ.get("FIXTURES", "/tmp/et/tests/fixtures/xbrl")
MUT = [
 ("048_xbrl.c", "udivmod: the remainder test is off by one (r > d instead of r >= d)", "if (r >= dv) { r -= dv; q |= 1ull << i; }", "if (r > dv) { r -= dv; q |= 1ull << i; }"),
 ("048_xbrl.c", "dates: month 13 accepted", "if (y < 1 || m < 1 || m > 12 || dd < 1) { return -1; }", "if (y < 1 || m < 1 || m > 13 || dd < 1) { return -1; }"),
 ("048_xbrl.c", "dates: every year divisible by 4 is a leap year (1900 too)", "int leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;", "int leap = (y % 4 == 0) || y % 400 == 0;"),
 ("048_xbrl.c", "numbers: five decimal places accepted", "if (fd == 4) { return -1; }", "if (fd == 5) { return -1; }"),
 ("048_xbrl.c", "numbers: the minus sign is ignored", "*out = neg ? -v : v;", "*out = v;"),
 ("048_xbrl.c", "contexts: a <segment> no longer marks a context as dimensional", "d->ctx[ps.cur_ctx].dim = 1;", "d->ctx[ps.cur_ctx].dim = 0;"),
 ("048_xbrl.c", "units: any currency counts as US dollars", "ps.direct_usd = sv_is(tt, \"iso4217:USD\");", "ps.direct_usd = 1;"),
 ("048_xbrl.c", "units: the unprefixed measure 'shares' is not recognised", "ps.den_ok = sv_is(tt, \"xbrli:shares\") || sv_is(tt, \"shares\");", "ps.den_ok = sv_is(tt, \"xbrli:shares\");"),
 ("048_xbrl.c", "facts: dollars with cents are kept (and truncated)", "if (rem != 0) { d->n_skipped_facts++; continue; }", "if (0) { d->n_skipped_facts++; continue; }"),
 ("048_xbrl.c", "contexts: a duplicated context id is accepted", "if (str_eq(d->ctx[k].id, cx->id)) { return XB_ERR_DUP_ID; }", "if (0) { return XB_ERR_DUP_ID; }"),
 ("048_xbrl.c", "facts: xsi:nil is ignored", "int is_nil = attr_find(at, na, \"xsi:nil\", &nil) && sv_is(nil, \"true\");", "int is_nil = 0; (void)nil;"),
 ("048_xbrl.c", "structure: a closing tag need not match the open one", "if (depth == 0 || !sv_eq(name, stack[depth - 1])) { return XB_ERR_XML; }", "if (depth == 0) { return XB_ERR_XML; }"),
 ("048_xbrl.c", "structure: an unclosed root element is accepted", "if (depth != 0) { return XB_ERR_TRUNCATED; }", ""),
 ("048_xbrl.c", "limits: the nesting limit is off by one (writes past the stack array)", "if (depth >= MAX_DEPTH) { return XB_ERR_DEPTH; }", "if (depth > MAX_DEPTH) { return XB_ERR_DEPTH; }"),
 ("048_xbrl.c", "limits: the fact table limit is off by one (writes past the table)", "if (d->n_fact >= XB_MAX_FACT) { return XB_ERR_TOO_MANY_FACT; }", "if (d->n_fact > XB_MAX_FACT) { return XB_ERR_TOO_MANY_FACT; }"),
 ("048_xbrl.c", "references: a fact naming an unknown context is attached to the first context", "if (ci < 0) { d->n_orphans++; continue; }", "if (ci < 0) { d->n_orphans++; ci = 0; }"),
 ("048_xbrl.c", "dei: a dei fact inside a dimensional context may supply the period end", "d->ctx[d->dei[i].ctx_idx].dim) { return d->dei[i].text; }", "d->ctx[d->dei[i].ctx_idx].dim || 1) { return d->dei[i].text; }"),
 ("048_xbrl.c", "namespaces: the us-gaap prefix is never registered", "ps.n_gaap < 4) { ps.gaap_prefix[ps.n_gaap++] = pre; }", "ps.n_gaap < 0) { ps.gaap_prefix[ps.n_gaap++] = pre; }"),
 ("048_ratios.c", "period: fiscal-year window starts at 351 days instead of 350 (balance-sheet date)", "end - c->start >= 350 && end - c->start <= 380 && (!have_best", "end - c->start >= 351 && end - c->start <= 380 && (!have_best"),
 ("048_ratios.c", "period: fiscal-year window ends at 381 days", "c->end - c->start >= 350 && c->end - c->start <= 380", "c->end - c->start >= 350 && c->end - c->start <= 381"),
 ("048_ratios.c", "period: the EARLIEST candidate prior date is used, not the latest", "(!have_best || c->start > best)", "(!have_best || c->start < best)"),
 ("048_ratios.c", "period: a balance-sheet date on or before the period end counts as current", "if (c->kind == XB_P_INSTANT && c->start == end) {", "if (c->kind == XB_P_INSTANT && c->start <= end) {"),
 ("048_ratios.c", "duplicates: equal values are flagged as conflicts instead of different ones", "b->val != a->val) { e.conflict[a->concept] = 1; }", "b->val == a->val) { e.conflict[a->concept] = 1; }"),
 ("048_ratios.c", "duplicates: dimensional facts take part in the conflict test", "const xb_fact_t *a = &d->fact[i]; if (d->ctx[a->ctx_idx].dim) { continue; }", "const xb_fact_t *a = &d->fact[i];"),
 ("048_ratios.c", "derived liabilities subtract the parent's equity, not equity including non-controlling interest", "L = mk(A.v - eqtot.v); L_derived = 1;", "L = mk(A.v - eq.v); L_derived = 1;"),
 ("048_ratios.c", "rounding: truncates instead of rounding half away from zero", "uint64_t num = 2 * mag * (uint64_t)scale + (uint64_t)dv;", "uint64_t num = 2 * mag * (uint64_t)scale;"),
 ("048_ratios.c", "rounding: the sign of a negative ratio is lost", "*out = n < 0 ? -(int64_t)q : (int64_t)q;", "*out = (int64_t)q;"),
 ("048_ratios.c", "return on equity uses the current equity twice instead of the average", "ratio_avg(r, \"return_on_equity\", ni, eq, eq0,", "ratio_avg(r, \"return_on_equity\", ni, eq, eq,"),
 ("048_ratios.c", "free cash flow is capex minus operating cash flow", "opt_t fcf = sub2(ocf, capex);", "opt_t fcf = sub2(capex, ocf);"),
 ("048_ratios.c", "a conflicting duplicate no longer rejects the filing", "if (r->check[RT_CHK_BALANCE] == RT_FAIL || any_conflict) {", "if (r->check[RT_CHK_BALANCE] == RT_FAIL) {"),
 ("048_ratios.c", "a balance sheet that does not balance no longer rejects the filing", "if (r->check[RT_CHK_BALANCE] == RT_FAIL || any_conflict) {", "if (any_conflict) {"),
 ("048_ratios.c", "quick ratio leaves out the short-term investments", "opt_t quick = (cash.ok && ar.ok) ? mk(cash.v + (sti.ok ? sti.v : 0) + ar.v) : no();", "opt_t quick = (cash.ok && ar.ok) ? mk(cash.v + ar.v) : no();"),
 ("048_ratios.c", "revenue fallback order swapped (Revenues before the contract-revenue concept)", "{XB_C_REV_CONTRACT, XB_C_REVENUES, XB_C_SALES_NET, 0}", "{XB_C_REVENUES, XB_C_REV_CONTRACT, XB_C_SALES_NET, 0}"),
 ("048_ratios.c", "a zero denominator is no longer 'not positive'", "if (dn.v <= 0) { emit_na(r, name, \"denominator not positive\"); return; }", "if (dn.v < 0) { emit_na(r, name, \"denominator not positive\"); return; }"),
 ("048_ratios.c", "gross profit is always derived, even when the filing reports it", "if (!gp.ok && rev.ok && cg.ok) { gp = mk(rev.v - cg.v); }", "if (rev.ok && cg.ok) { gp = mk(rev.v - cg.v); }"),
 ("048_ratios.c", "the overflow guard compares with 2^63 instead of 2^62", "uint64_t mag = (uint64_t)iabs(n), lim = (1ull << 62), rem, q;", "uint64_t mag = (uint64_t)iabs(n), lim = (1ull << 63), rem, q;"),
 ("048_filings.c", "format: percentages lose the zero padding of the cents (0.05% shown as 0.5%)", "o = put_u(buf, o, r, 2); buf[o++] = '%'; break;", "o = put_u(buf, o, r, 1); buf[o++] = '%'; break;"),
 ("048_filings.c", "format: thousands separators every four digits", "if (grp == 3) { t[n++] = ','; grp = 0; }", "if (grp == 4) { t[n++] = ','; grp = 0; }"),
 ("048_filings.c", "date: the year bound check is removed (writes past the 11-byte buffer)", "if (y < 1 || y > 9999) {", "if (0) {"),
 ("edgar_slim.py", "slimmer: the dei facts (period end, name) are not kept", "+ [\"dei:\" + k for k in DEI])", "+ [])"),
 ("edgar_slim.py", "slimmer: contexts the facts do not use are kept too", "if re.search(r'id=\"([^\"]+)\"', m.group(0)).group(1) in used]\n    uref", "if True]\n    uref"),
 ("edgar_slim.py", "slimmer: GrossProfit is no longer kept", "CostOfRevenue CostOfGoodsAndServicesSold GrossProfit OperatingIncomeLoss", "CostOfRevenue CostOfGoodsAndServicesSold OperatingIncomeLoss"),
 ("edgar_fetch.py", "fetcher: any User-Agent is accepted (no contact address required)", "if not re.search(r\"\\S+@\\S+\\.\\S+\", user_agent or \"\"):", "if False:"),
 ("edgar_fetch.py", "fetcher: 429 (too many requests) is not retried", "if e.code in (429, 500, 502, 503, 504) and attempt < 4:", "if e.code in (500, 502, 503, 504) and attempt < 4:"),
 ("edgar_fetch.py", "fetcher: a 404 is retried like a server error", "if e.code in (429, 500, 502, 503, 504) and attempt < 4:", "if e.code in (404, 429, 500, 502, 503, 504) and attempt < 4:"),
 ("edgar_fetch.py", "fetcher: five attempts instead of four", "if e.code in (429, 500, 502, 503, 504) and attempt < 4:", "if e.code in (429, 500, 502, 503, 504) and attempt < 5:"),
 ("edgar_fetch.py", "fetcher: the rate limiter allows 11 requests per second", "if len(self.stamps) >= self.per_second:", "if len(self.stamps) > self.per_second:"),
 ("edgar_fetch.py", "fetcher: it takes the first instance document when a filing has two", "if len(inst) != 1:", "if len(inst) < 1:"),
 ("edgar_fetch.py", "fetcher: a document with no financial facts is accepted", "if nf < 20 or \"DocumentPeriodEndDate\" not in slim:", "if False:"),
 ("ratios_ref.py", "reference: rounds down (the oracle itself broken, the differential tests must notice)", "return s * ((2 * abs(n) + d) // (2 * d))", "return s * ((2 * abs(n)) // (2 * d))"),
 ("ratios_ref.py", "reference: fiscal-year window too short (the oracle itself broken)", "if not seg and e == end and s is not None and 350 <= (e - s).days <= 380]", "if not seg and e == end and s is not None and 350 <= (e - s).days <= 379]"),
]
def run(tmp, cmd, **kw): return subprocess.run(cmd, cwd=tmp, capture_output=True, text=True, errors="replace", **kw)
def suite(tmp):
    failed = []; n = os.path.join(tmp, "native")
    b = run(n, ["gcc", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined", "xbrl_test.c", "../048_xbrl.c", "../048_ratios.c", "../048_filings.c", "-o", "xt"])
    if b.returncode != 0: failed.append("xbrl_test (did not build)")
    elif run(n, ["./xt"]).returncode != 0: failed.append("xbrl_test")
    b = run(n, ["gcc", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined", "ratios_cli.c", "../048_xbrl.c", "../048_ratios.c", "-o", "cli"])
    if b.returncode != 0: failed.append("the engine (did not build)"); return failed
    bad = []
    for t in ("aapl", "ko", "nvda", "msft", "xom", "jpm"):
        c = run(n, ["./cli", f"../data/{t}.xml", "10000"]); p = run(n, [sys.executable, "../ratios_ref.py", f"../data/{t}.xml", "10000"])
        if c.stdout != p.stdout or c.returncode != 0 or p.returncode != 0 or c.stderr: bad.append(t)
    if bad: failed.append("six real filings (" + ",".join(bad) + ")")
    if run(n, [sys.executable, "diff_test.py", "150", "./cli"]).returncode != 0: failed.append("random-filing differential test")
    if run(n, [sys.executable, "test_fetch.py"]).returncode != 0: failed.append("fetcher tests")
    slim_bad = []
    for t, rel in (("aapl", "aapl/10k_2023/aapl-20230930_htm.xml"), ("ko", "ko/10k_2024/ko-20240220_htm.xml"), ("jpm", "jpm/10k_2024/jpm-20240216_htm.xml")):
        r = run(tmp, [sys.executable, "-c", f"import edgar_slim,sys; sys.stdout.write(edgar_slim.slim(open('{FX}/{rel}',encoding='utf-8').read())[0])"])
        if r.stdout != open(os.path.join(tmp, "data", t + ".xml"), encoding="utf-8").read(): slim_bad.append(t)
    if slim_bad: failed.append("slimmer reproduces embedded filings (" + ",".join(slim_bad) + ")")
    return failed
print("NOTE: this script deliberately breaks copies of the chapter's sources. 'caught' lines are EXPECTED: they show the tests can detect the mistake."); sys.stdout.flush()
base = tempfile.mkdtemp(prefix="c48mut_"); shutil.copytree(code, os.path.join(base, "base"), ignore=shutil.ignore_patterns("build", "*.o", "__pycache__"))
f0 = suite(os.path.join(base, "base")); print("baseline (nothing broken):", "all tests pass (expected)" if not f0 else "UNEXPECTED FAILURES " + str(f0)); sys.stdout.flush()
caught = 0
for fn, label, old, new in MUT:
    tmp = os.path.join(base, "m"); shutil.rmtree(tmp, ignore_errors=True); shutil.copytree(os.path.join(base, "base"), tmp)
    p = os.path.join(tmp, fn); s = open(p, encoding="utf-8").read(); assert s.count(old) == 1, (label, s.count(old)); open(p, "w", encoding="utf-8").write(s.replace(old, new))
    failed = suite(tmp)
    if failed: caught += 1; print(f"{label}: caught by {', '.join(failed)}")
    else: print(f"{label}: NOT CAUGHT")
    sys.stdout.flush()
print(f"\n{caught} of {len(MUT)} broken versions caught"); shutil.rmtree(base, ignore_errors=True)
