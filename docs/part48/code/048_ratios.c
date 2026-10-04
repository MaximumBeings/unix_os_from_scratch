/* Chapter 48: the ratio engine (see 048_ratios.h and the page's 'The rules'). */
#include "048_ratios.h"

static const uint8_t REV[] = {XB_C_REV_CONTRACT, XB_C_REVENUES, XB_C_SALES_NET, 0};
static const uint8_t COGS[] = {XB_C_COST_OF_REVENUE, XB_C_COGS, 0};
static const uint8_t INTEREST[] = {XB_C_INTEREST_EXPENSE, XB_C_INTEREST_NONOP, 0};
static const uint8_t STI[] = {XB_C_MKT_SEC_CURRENT, XB_C_ST_INVEST, 0};
static const uint8_t CAPEX[] = {XB_C_CAPEX_PPE, XB_C_CAPEX_PRODUCTIVE, 0};
static const uint8_t C_ASSETS[] = {XB_C_ASSETS, 0}, C_LSE[] = {XB_C_LIAB_AND_EQUITY, 0}, C_EQ[] = {XB_C_EQUITY, 0}, C_EQN[] = {XB_C_EQUITY_INCL_NCI, 0}, C_LIAB[] = {XB_C_LIABILITIES, 0};
static const uint8_t C_CA[] = {XB_C_ASSETS_CURRENT, 0}, C_CL[] = {XB_C_LIABILITIES_CURRENT, 0}, C_CASH[] = {XB_C_CASH, 0}, C_AR[] = {XB_C_RECEIVABLES, 0};
static const uint8_t C_GP[] = {XB_C_GROSS_PROFIT, 0}, C_OI[] = {XB_C_OPERATING_INCOME, 0}, C_NI[] = {XB_C_NET_INCOME, 0}, C_OCF[] = {XB_C_OCF, 0}, C_EPS[] = {XB_C_EPS_DILUTED, 0};

typedef struct { uint16_t idx[XB_MAX_CTX]; uint16_t n; } ctxset_t;
typedef struct { int64_t v; int ok; } opt_t; /* an optional integer: ok == 0 means "not available" */
typedef struct { const xb_doc_t *d; uint8_t conflict[XB_C_COUNT]; ctxset_t dur, cur, pri; } env_t;

static int64_t iabs(int64_t v) { return v < 0 ? -v : v; }

int rt_rdiv(int64_t n, int64_t scale, int64_t dv, int64_t *out) {
    uint64_t mag = (uint64_t)iabs(n), lim = (1ull << 62), rem, q;
    if (scale <= 0 || dv <= 0 || (uint64_t)dv > lim) { return -1; }
    q = xb_udivmod(lim, (uint64_t)scale, &rem);
    if (mag > q) { return -1; }
    uint64_t num = 2 * mag * (uint64_t)scale + (uint64_t)dv;
    q = xb_udivmod(num, 2 * (uint64_t)dv, &rem);
    *out = n < 0 ? -(int64_t)q : (int64_t)q;
    return 0;
}

static opt_t lookup(const env_t *e, const uint8_t *chain, const ctxset_t *set) {
    opt_t none; none.v = 0; none.ok = 0;
    for (int k = 0; chain[k]; k++) {
        uint8_t concept = chain[k];
        if (e->conflict[concept]) { return none; } /* a conflicting concept ends the chain: no silent fall through to a different concept */
        for (uint16_t s = 0; s < set->n; s++) {
            for (uint16_t i = 0; i < e->d->n_fact; i++) {
                if (e->d->fact[i].concept == concept && e->d->fact[i].ctx_idx == (int16_t)set->idx[s]) { opt_t r; r.v = e->d->fact[i].val; r.ok = 1; return r; }
            }
        }
    }
    return none;
}

static const char *CHECK_NAMES[RT_N_CHECKS] = {"check_assets_eq_liab_plus_equity", "check_liabilities_plus_equity", "check_gross_profit", "check_conflicting_duplicates"};
const char *rt_check_name(int i) { return CHECK_NAMES[i]; }
const char *rt_unit_name(int u) { static const char *U[] = {"x", "bp", "usd", "usd4"}; return U[u]; }

static void emit_val(rt_report_t *r, const char *name, int64_t v, int unit) {
    rt_line_t *l = &r->line[r->n_lines++]; l->name = name; l->na = 0; l->unit = (uint8_t)unit; l->value = v; l->reason = 0;
}
static void emit_na(rt_report_t *r, const char *name, const char *why) {
    rt_line_t *l = &r->line[r->n_lines++]; l->name = name; l->na = 1; l->unit = 0; l->value = 0; l->reason = why;
}
/* n/d scaled: unit RT_U_X uses scale 100, RT_U_BP scale 10000. */
static void ratio(rt_report_t *r, const char *name, opt_t n, opt_t dn, int64_t scale, int unit) {
    int64_t q;
    if (!n.ok || !dn.ok) { emit_na(r, name, "missing input"); return; }
    if (dn.v <= 0) { emit_na(r, name, "denominator not positive"); return; }
    if (rt_rdiv(n.v, scale, dn.v, &q) != 0) { emit_na(r, name, "overflow"); return; }
    emit_val(r, name, q, unit);
}
/* n over the AVERAGE of two balances: n / ((a1 + a0) / 2) = 2n / (a1 + a0) */
static void ratio_avg(rt_report_t *r, const char *name, opt_t n, opt_t a1, opt_t a0, int64_t scale, int unit) {
    int64_t q;
    if (!n.ok || !a1.ok || !a0.ok) { emit_na(r, name, "missing input"); return; }
    if (a1.v + a0.v <= 0) { emit_na(r, name, "denominator not positive"); return; }
    if (rt_rdiv(n.v, 2 * scale, a1.v + a0.v, &q) != 0) { emit_na(r, name, "overflow"); return; }
    emit_val(r, name, q, unit);
}
static opt_t mk(int64_t v) { opt_t o; o.v = v; o.ok = 1; return o; }
static opt_t no(void) { opt_t o; o.v = 0; o.ok = 0; return o; }
static opt_t sub2(opt_t a, opt_t b) { return (a.ok && b.ok) ? mk(a.v - b.v) : no(); }

int rt_analyse(const xb_doc_t *d, int64_t price_cents, rt_report_t *r) {
    static env_t e; /* ~4 KB of index sets: static, not on the kernel's small stack */
    e.d = d; e.dur.n = e.cur.n = e.pri.n = 0;
    for (int c = 0; c < XB_C_COUNT; c++) { e.conflict[c] = 0; }
    r->n_lines = 0; r->accepted = 0; for (int i = 0; i < RT_N_CHECKS; i++) { r->check[i] = RT_SKIP; }
    /* the period end: the first dei:DocumentPeriodEndDate in a dimension-free context */
    const char *pe = xb_dei_text(d, XB_DEI_PERIOD_END); int32_t end;
    if (!pe) { return XB_ERR_NO_PERIOD; }
    uint32_t pl = 0; while (pe[pl]) { pl++; }
    if (xb_parse_date(pe, pl, &end) != 0) { return XB_ERR_BAD_DATE; }
    r->period_end_day = end;
    int32_t best = 0; int have_best = 0;
    for (uint16_t i = 0; i < d->n_ctx; i++) {
        const xb_ctx_t *c = &d->ctx[i]; if (c->dim) { continue; }
        if (c->kind == XB_P_INSTANT && end - c->start >= 350 && end - c->start <= 380 && (!have_best || c->start > best)) { best = c->start; have_best = 1; }
    }
    for (uint16_t i = 0; i < d->n_ctx; i++) {
        const xb_ctx_t *c = &d->ctx[i]; if (c->dim) { continue; }
        if (c->kind == XB_P_DURATION && c->end == end && c->end - c->start >= 350 && c->end - c->start <= 380) { e.dur.idx[e.dur.n++] = i; }
        if (c->kind == XB_P_INSTANT && c->start == end) { e.cur.idx[e.cur.n++] = i; }
        if (c->kind == XB_P_INSTANT && have_best && c->start == best) { e.pri.idx[e.pri.n++] = i; }
    }
    r->n_dur = e.dur.n; r->n_cur = e.cur.n; r->n_pri = e.pri.n;
    /* conflicting duplicates: the same concept in the same dimension-free context with two different values (the XBRL "inconsistent duplicate") */
    for (uint16_t i = 0; i < d->n_fact; i++) {
        const xb_fact_t *a = &d->fact[i]; if (d->ctx[a->ctx_idx].dim) { continue; }
        for (uint16_t j = (uint16_t)(i + 1); j < d->n_fact; j++) {
            const xb_fact_t *b = &d->fact[j];
            if (b->concept == a->concept && b->ctx_idx == a->ctx_idx && b->val != a->val) { e.conflict[a->concept] = 1; }
        }
    }
    int any_conflict = 0; for (int c = 0; c < XB_C_COUNT; c++) { if (e.conflict[c]) { any_conflict = 1; } }
    #define D(chain) lookup(&e, chain, &e.dur)
    #define I(chain) lookup(&e, chain, &e.cur)
    #define P(chain) lookup(&e, chain, &e.pri)
    opt_t A = I(C_ASSETS), A0 = P(C_ASSETS), LSE = I(C_LSE), eq = I(C_EQ), eq0 = P(C_EQ), eqtot = I(C_EQN);
    if (!eqtot.ok) { eqtot = eq; }
    opt_t L = I(C_LIAB); int L_derived = 0;
    if (!L.ok && A.ok && eqtot.ok) { L = mk(A.v - eqtot.v); L_derived = 1; }
    opt_t CA = I(C_CA), CL = I(C_CL), cash = I(C_CASH), ar = I(C_AR), sti = I(STI);
    opt_t rev = D(REV), gp = D(C_GP), cg = D(COGS);
    if (!gp.ok && rev.ok && cg.ok) { gp = mk(rev.v - cg.v); }
    opt_t oi = D(C_OI), ni = D(C_NI), intr = D(INTEREST), ocf = D(C_OCF), capex = D(CAPEX), eps = D(C_EPS);
    /* integrity checks */
    r->check[RT_CHK_BALANCE] = (A.ok && LSE.ok) ? (A.v == LSE.v ? RT_PASS : RT_FAIL) : RT_SKIP;
    r->check[RT_CHK_L_PLUS_E] = (L.ok && !L_derived && eqtot.ok && LSE.ok) ? (L.v + eqtot.v == LSE.v ? RT_PASS : RT_FAIL) : RT_SKIP;
    opt_t g_rep = D(C_GP);
    r->check[RT_CHK_GROSS] = (g_rep.ok && rev.ok && cg.ok) ? (g_rep.v == rev.v - cg.v ? RT_PASS : RT_FAIL) : RT_SKIP;
    r->check[RT_CHK_DUPES] = any_conflict ? RT_FAIL : RT_PASS;
    if (r->check[RT_CHK_BALANCE] == RT_FAIL || any_conflict) { return XB_OK; } /* REJECTED: no ratios from a filing that does not balance or contradicts itself */
    r->accepted = 1;
    ratio(r, "current_ratio", CA, CL, 100, RT_U_X);
    opt_t quick = (cash.ok && ar.ok) ? mk(cash.v + (sti.ok ? sti.v : 0) + ar.v) : no(); ratio(r, "quick_ratio", quick, CL, 100, RT_U_X);
    opt_t cashr = cash.ok ? mk(cash.v + (sti.ok ? sti.v : 0)) : no(); ratio(r, "cash_ratio", cashr, CL, 100, RT_U_X);
    ratio(r, "liabilities_to_equity", L, eq, 100, RT_U_X); ratio(r, "liabilities_to_assets", L, A, 10000, RT_U_BP); ratio(r, "equity_multiplier", A, eq, 100, RT_U_X);
    ratio(r, "interest_coverage", oi, intr, 100, RT_U_X);
    ratio(r, "gross_margin", gp, rev, 10000, RT_U_BP); ratio(r, "operating_margin", oi, rev, 10000, RT_U_BP); ratio(r, "net_margin", ni, rev, 10000, RT_U_BP);
    ratio_avg(r, "return_on_assets", ni, A, A0, 10000, RT_U_BP); ratio_avg(r, "return_on_equity", ni, eq, eq0, 10000, RT_U_BP); ratio_avg(r, "asset_turnover", rev, A, A0, 100, RT_U_X);
    opt_t fcf = sub2(ocf, capex);
    if (fcf.ok) { emit_val(r, "free_cash_flow", fcf.v, RT_U_USD); } else { emit_na(r, "free_cash_flow", "missing input"); }
    ratio(r, "fcf_margin", fcf, rev, 10000, RT_U_BP);
    if (eps.ok) { emit_val(r, "eps_diluted_reported", eps.v, RT_U_USD4); } else { emit_na(r, "eps_diluted_reported", "not reported as us-gaap:EarningsPerShareDiluted"); }
    if (price_cents <= 0 || !eps.ok) { emit_na(r, "price_to_earnings", "no price or no EPS"); }
    else if (eps.v <= 0) { emit_na(r, "price_to_earnings", "EPS not positive"); }
    else { int64_t q; if (rt_rdiv(price_cents, 10000, eps.v, &q) != 0) { emit_na(r, "price_to_earnings", "overflow"); } else { emit_val(r, "price_to_earnings", q, RT_U_X); } }
    #undef D
    #undef I
    #undef P
    return XB_OK;
}

static int put_str(char *buf, uint32_t cap, uint32_t *o, const char *s) {
    while (*s) { if (*o + 1 >= cap) { return -1; } buf[(*o)++] = *s++; }
    return 0;
}
static int put_i64(char *buf, uint32_t cap, uint32_t *o, int64_t v) {
    char t[24]; int n = 0; uint64_t mag = (uint64_t)iabs(v), rem;
    if (mag == 0) { t[n++] = '0'; }
    while (mag) { mag = xb_udivmod(mag, 10, &rem); t[n++] = (char)('0' + (int)rem); }
    if (v < 0 && put_str(buf, cap, o, "-") != 0) { return -1; }
    while (n) { char one[2]; one[0] = t[--n]; one[1] = 0; if (put_str(buf, cap, o, one) != 0) { return -1; } }
    return 0;
}
int rt_canonical(const rt_report_t *r, char *buf, uint32_t cap) {
    uint32_t o = 0; static const char *RES[] = {"SKIP", "PASS", "FAIL"};
    for (int i = 0; i < RT_N_CHECKS; i++) {
        if (put_str(buf, cap, &o, CHECK_NAMES[i]) || put_str(buf, cap, &o, " ") || put_str(buf, cap, &o, RES[r->check[i]]) || put_str(buf, cap, &o, "\n")) { return -1; }
    }
    if (put_str(buf, cap, &o, r->accepted ? "verdict ACCEPTED\n" : "verdict REJECTED\n")) { return -1; }
    for (int i = 0; i < r->n_lines; i++) {
        const rt_line_t *l = &r->line[i];
        if (put_str(buf, cap, &o, l->name) || put_str(buf, cap, &o, " ")) { return -1; }
        if (l->na) { if (put_str(buf, cap, &o, "NA ") || put_str(buf, cap, &o, l->reason)) { return -1; } }
        else { if (put_i64(buf, cap, &o, l->value) || put_str(buf, cap, &o, " ") || put_str(buf, cap, &o, rt_unit_name(l->unit))) { return -1; } }
        if (put_str(buf, cap, &o, "\n")) { return -1; }
    }
    buf[o] = 0; return (int)o;
}
