/* Chapter 52 host tool, built from the SAME 052_payroll.c the kernel links. Input lines: "E id status(0 S,1 MFJ,2 HOH) step2 periods credits_a other_a deductions_a extra_pp"  and  "P id gross k401 s125" (amounts in cents; pay lines advance the year-to-date of that employee).
 * Output, per pay line: "pay n id ok fit_wages fica_wages fit ss med addmed net er_ss er_med | ytd ss_wages ss_tax med_wages" or "pay n id <error name>". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../052_payroll.h"
#define MAXE 64
static pay_emp_t emp[MAXE]; static pay_ytd_t ytd[MAXE]; static long ids[MAXE]; static int ne;
static const char *name(int rc) { switch (rc) { case PAY_ERR_STATUS: return "status"; case PAY_ERR_PERIODS: return "periods"; case PAY_ERR_AMOUNT: return "amount"; case PAY_ERR_DEDUCT: return "deduct"; case PAY_ERR_YTD: return "ytd"; } return "?"; }
int main(int argc, char **argv) {
    if (argc < 2) { return 2; } FILE *f = fopen(argv[1], "r"); if (!f) { return 2; } char line[256]; int n = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == 'E') { long id; int st, s2, per; long long cr, oi, de, ex; if (sscanf(line + 1, "%ld %d %d %d %lld %lld %lld %lld", &id, &st, &s2, &per, &cr, &oi, &de, &ex) == 8 && ne < MAXE) { ids[ne] = id; emp[ne].status = (uint8_t)st; emp[ne].step2_checked = (uint8_t)s2; emp[ne].periods = (uint16_t)per; emp[ne].credits_a = cr; emp[ne].other_income_a = oi; emp[ne].deductions_a = de; emp[ne].extra_pp = ex; memset(&ytd[ne], 0, sizeof ytd[ne]); ne++; } }
        else if (line[0] == 'P') { long id; long long g, k, s; if (sscanf(line + 1, "%ld %lld %lld %lld", &id, &g, &k, &s) != 4) { continue; } int i; for (i = 0; i < ne; i++) { if (ids[i] == id) { break; } } n++; if (i == ne) { printf("pay %d %ld unknown\n", n, id); continue; }
            pay_period_t p = {g, k, s}; pay_result_t r; int rc = pay_compute(&emp[i], &p, &ytd[i], &r);
            if (rc) { printf("pay %d %ld %s\n", n, id, name(rc)); continue; }
            printf("pay %d %ld ok %lld %lld %lld %lld %lld %lld %lld %lld %lld | ytd %lld %lld %lld\n", n, id, (long long)r.fit_wages, (long long)r.fica_wages, (long long)r.fit, (long long)r.ss, (long long)r.med, (long long)r.addmed, (long long)r.net, (long long)r.er_ss, (long long)r.er_med, (long long)ytd[i].ss_wages, (long long)ytd[i].ss_tax, (long long)ytd[i].med_wages); }
    }
    return 0;
}
