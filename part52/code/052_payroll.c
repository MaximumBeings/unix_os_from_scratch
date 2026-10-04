/* Chapter 52: the payroll engine (see 052_payroll.h). Freestanding: no libgcc division (rounding division is shift-and-subtract). */
#include "052_payroll.h"

static uint64_t udm(uint64_t n, uint64_t dv) { uint64_t q = 0, r = 0; for (int i = 63; i >= 0; i--) { r = (r << 1) | ((n >> i) & 1u); if (r >= dv) { r -= dv; q |= 1ull << i; } } return q; }
static int64_t rdiv(int64_t n, int64_t d) { /* round half up (away from zero for negatives), d > 0 */ uint64_t m = (uint64_t)(n < 0 ? -n : n); int64_t q = (int64_t)udm(2 * m + (uint64_t)d, 2 * (uint64_t)d); return n < 0 ? -q : q; }
const char *pay_strerror(int rc) {
    switch (rc) { case PAY_OK: return "ok"; case PAY_ERR_STATUS: return "filing status is not S, MFJ or HOH"; case PAY_ERR_PERIODS: return "pay periods per year must be 52, 26, 24 or 12"; case PAY_ERR_AMOUNT: return "an amount is negative or above $1,000,000,000";
    case PAY_ERR_DEDUCT: return "deductions exceed gross pay"; case PAY_ERR_YTD: return "year-to-date figures are negative or inconsistent"; }
    return "?";
}
/* the 2025 schedules: upper bracket limits in whole dollars for each status, rates in basis points */
static const int32_t LIM[3][6] = {{11925, 48475, 103350, 197300, 250525, 626350}, {23850, 96950, 206700, 394600, 501050, 751600}, {17000, 64850, 103350, 197300, 250500, 626350}};
static const int32_t RATE[7] = {1000, 1200, 2200, 2400, 3200, 3500, 3700};
static const int32_t STD[3] = {15000, 30000, 22500};
int64_t pay_annual_tax(int status, int halved, int64_t taxable) {
    if (taxable <= 0) { return 0; }
    int64_t prev = 0, num = 0; /* num: cents x basis points, exact */
    for (int i = 0; i < 7; i++) {
        int64_t top = i < 6 ? (int64_t)LIM[status][i] * 100 : -1; if (halved && top > 0) { top /= 2; }
        if (top < 0 || taxable <= top) { num += (taxable - prev) * RATE[i]; break; }
        num += (top - prev) * RATE[i]; prev = top;
    }
    return rdiv(num, 10000);
}
int pay_compute(const pay_emp_t *e, const pay_period_t *p, pay_ytd_t *ytd, pay_result_t *r) {
    if (e->status > PAY_HOH) { return PAY_ERR_STATUS; }
    if (e->periods != 52 && e->periods != 26 && e->periods != 24 && e->periods != 12) { return PAY_ERR_PERIODS; }
    const int64_t amts[8] = {e->credits_a, e->other_income_a, e->deductions_a, e->extra_pp, p->gross, p->k401, p->s125, 0};
    for (int i = 0; i < 7; i++) { if (amts[i] < 0 || amts[i] > PAY_MAX_CENTS) { return PAY_ERR_AMOUNT; } }
    if (p->k401 + p->s125 > p->gross) { return PAY_ERR_DEDUCT; }
    if (ytd->ss_wages < 0 || ytd->ss_tax < 0 || ytd->med_wages < 0 || ytd->ss_wages > 100 * PAY_MAX_CENTS || ytd->med_wages > 100 * PAY_MAX_CENTS) { return PAY_ERR_YTD; }
    r->fit_wages = p->gross - p->k401 - p->s125; r->fica_wages = p->gross - p->s125;
    /* federal income tax */
    int halved = e->step2_checked != 0; int64_t A = r->fit_wages * e->periods + e->other_income_a - e->deductions_a; int64_t std = (int64_t)STD[e->status] * 100; if (halved) { std /= 2; }
    int64_t taxable = A - std; int64_t annual = pay_annual_tax(e->status, halved, taxable); annual -= e->credits_a; if (annual < 0) { annual = 0; }
    r->fit = rdiv(annual, e->periods) + e->extra_pp;
    /* Social Security, capped by wage base and by the maximum tax */
    const int64_t base = 17610000, max_tax = rdiv(base * 620, 10000); /* 6.2% of $176,100.00 = $10,918.20 */
    int64_t room = base - ytd->ss_wages; if (room < 0) { room = 0; } int64_t ssw = r->fica_wages < room ? r->fica_wages : room;
    int64_t ss = rdiv(ssw * 620, 10000); int64_t tax_room = max_tax - ytd->ss_tax; if (tax_room < 0) { tax_room = 0; } if (ss > tax_room) { ss = tax_room; } r->ss = ss; r->er_ss = ss;
    /* Medicare and Additional Medicare */
    r->med = rdiv(r->fica_wages * 145, 10000); r->er_med = r->med;
    const int64_t thr = 20000000; int64_t before = ytd->med_wages > thr ? ytd->med_wages - thr : 0; int64_t after = ytd->med_wages + r->fica_wages > thr ? ytd->med_wages + r->fica_wages - thr : 0;
    r->addmed = rdiv((after - before) * 90, 10000);
    r->net = p->gross - r->fit - r->ss - r->med - r->addmed - p->k401 - p->s125;
    if (pay_check(p, r)) { return PAY_ERR_DEDUCT; } /* withholding larger than the pay: refused, nothing changed */
    ytd->ss_wages += ssw; ytd->ss_tax += r->ss; ytd->med_wages += r->fica_wages;
    return PAY_OK;
}
int pay_check(const pay_period_t *p, const pay_result_t *r) {
    if (r->fit < 0 || r->ss < 0 || r->med < 0 || r->addmed < 0 || r->net < 0 || r->er_ss < 0 || r->er_med < 0) { return 1; }
    if (p->gross != r->net + r->fit + r->ss + r->med + r->addmed + p->k401 + p->s125) { return 2; }
    return 0;
}
