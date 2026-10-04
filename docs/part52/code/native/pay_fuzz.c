/* Chapter 52: random pay periods under AddressSanitizer + UBSan, ordinary and extreme values. After EVERY call: a refused call changed nothing (the year-to-date is identical); an accepted call satisfies pay_check (gross = net + every withholding and deduction, nothing negative);
 * the year-to-date only grows; Social Security wages never exceed the wage base and Social Security tax never exceeds 6.2% of it ($10,918.20); the employer's shares equal the employee's; Additional Medicare is zero until the year's Medicare wages pass $200,000 and
 * the total of Additional Medicare over a year equals 0.9% (rounded per period) of the wages above the threshold within a cent per period. Usage: pay_fuzz N */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../052_payroll.h"
static uint64_t rng = 0x9E3779B97F4A7C15ull; static uint64_t xr(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }
int main(int argc, char **argv) {
    (void)argc; long years = atol(argv[1]), calls = 0, ok = 0, refused = 0, bad = 0; static const uint16_t PER[4] = {52, 26, 24, 12};
    for (long y = 0; y < years; y++) {
        pay_emp_t e; e.status = (uint8_t)(xr() % 3); e.step2_checked = (uint8_t)(xr() & 1); e.periods = PER[xr() % 4]; e.credits_a = (int64_t)(xr() % 3 ? 0 : xr() % 2000000); e.other_income_a = (int64_t)(xr() % 3 ? 0 : xr() % 9000000); e.deductions_a = (int64_t)(xr() % 3 ? 0 : xr() % 4000000); e.extra_pp = (int64_t)(xr() % 3 ? 0 : xr() % 30000);
        pay_ytd_t Y; memset(&Y, 0, sizeof Y); int64_t base_pay = (int64_t)(5000 + xr() % 3000000) * 52 / e.periods; if (xr() % 4 == 0) { base_pay *= 20; }
        int64_t addmed_total = 0; for (int n = 0; n < e.periods; n++) {
            pay_period_t p; int extreme = xr() % 40 == 0; p.gross = extreme ? (int64_t)(xr() % 3 ? PAY_MAX_CENTS + (int64_t)(xr() % 3) : -(int64_t)(xr() % 5)) : base_pay / 2 + (int64_t)(xr() % (uint64_t)(base_pay + 1)); p.k401 = xr() % 3 ? 0 : (int64_t)(xr() % (uint64_t)(p.gross > 0 ? p.gross : 1)); p.s125 = xr() % 4 ? 0 : (int64_t)(xr() % (uint64_t)(p.gross > 0 ? p.gross / 4 + 1 : 1));
            if (xr() % 50 == 0) { p.k401 = p.gross + 1; }
            pay_ytd_t before = Y; pay_result_t r; int rc = pay_compute(&e, &p, &Y, &r); calls++;
            if (rc) { refused++; if (memcmp(&before, &Y, sizeof Y)) { bad++; printf("a REFUSED call (%s) changed the year-to-date\n", pay_strerror(rc)); } continue; }
            ok++;
            if (pay_check(&p, &r)) { bad++; printf("an ACCEPTED call fails pay_check\n"); }
            if (Y.ss_wages < before.ss_wages || Y.ss_tax < before.ss_tax || Y.med_wages < before.med_wages) { bad++; printf("year-to-date went down\n"); }
            if (Y.ss_wages > 17610000 || Y.ss_tax > 1091820) { bad++; printf("Social Security passed its wage base or its maximum tax\n"); }
            if (r.er_ss != r.ss || r.er_med != r.med) { bad++; printf("employer share differs from the employee's\n"); }
            if (before.med_wages + r.fica_wages <= 20000000 && r.addmed != 0) { bad++; printf("Additional Medicare below the threshold\n"); }
            if (r.fit < e.extra_pp) { bad++; printf("income tax below the extra withholding\n"); }
            addmed_total += r.addmed;
        }
        int64_t over = Y.med_wages > 20000000 ? Y.med_wages - 20000000 : 0; int64_t ideal = (over * 9 + 500) / 1000, diff = addmed_total > ideal ? addmed_total - ideal : ideal - addmed_total;
        if (diff > e.periods) { bad++; printf("a year's Additional Medicare (%lld) is far from 0.9%% of the wages over $200,000 (%lld)\n", (long long)addmed_total, (long long)ideal); }
    }
    printf("%ld random pay periods in %ld years (%ld accepted, %ld refused); failures: %ld\n", calls, years, ok, refused, bad); return bad ? 1 : 0;
}
