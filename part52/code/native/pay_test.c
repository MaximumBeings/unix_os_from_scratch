/* Chapter 52 host test: the payroll engine under AddressSanitizer + UBSan. Every expected number was worked out on paper first (comments show the arithmetic). PASS/FAIL per line; exit status 1 on any FAIL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../052_payroll.h"
static int pass_n, fail_n;
static void check(const char *name, int ok) { printf("%s %s\n", ok ? "PASS" : "FAIL", name); if (ok) { pass_n++; } else { fail_n++; } }
static pay_result_t R; static pay_ytd_t Y;
static int run(int st, int s2, int per, int64_t cr, int64_t oi, int64_t de, int64_t ex, int64_t g, int64_t k, int64_t s) { pay_emp_t e = {(uint8_t)st, (uint8_t)s2, (uint16_t)per, cr, oi, de, ex}; pay_period_t p = {g, k, s}; return pay_compute(&e, &p, &Y, &R); }
int main(void) {
    printf("== 1. the bracket schedule alone ==\n");
    /* single: 10% of 11,925 = 1,192.50; to 48,475: + 12% of 36,550 = 4,386.00 -> 5,578.50; to 103,350: + 22% of 54,875 = 12,072.50 -> 17,651.00 */
    check("single, taxable $11,925 (top of the 10% bracket): $1,192.50", pay_annual_tax(PAY_SINGLE, 0, 1192500) == 119250);
    check("single, taxable $48,475: $5,578.50", pay_annual_tax(PAY_SINGLE, 0, 4847500) == 557850);
    check("single, taxable $103,350: $17,651.00", pay_annual_tax(PAY_SINGLE, 0, 10335000) == 1765100);
    check("one DOLLAR past a bracket limit is taxed at the next rate: $11,926.00 -> 1,192.50 + 12% of $1.00 = $1,192.62; a single cent more adds a twelfth of a cent, which rounds away: $11,925.01 -> $1,192.50", pay_annual_tax(PAY_SINGLE, 0, 1192600) == 119262 && pay_annual_tax(PAY_SINGLE, 0, 1192501) == 119250);
    /* the top bracket: single, $700,000: 626,350 limit; tax to there: 17,651.00 + 24% x 93,950 (22,548.00) = 40,199.00 + 32% x 53,225 (17,032.00) = 57,231.00 + 35% x 375,825 (131,538.75) = 188,769.75; + 37% x 73,650 (27,250.50) = 216,020.25 */
    check("single, taxable $700,000 (through the 37% bracket): $216,020.25", pay_annual_tax(PAY_SINGLE, 0, 70000000) == 21602025);
    check("married jointly, taxable $66,000: 10% of 23,850 (2,385.00) + 12% of 42,150 (5,058.00) = $7,443.00", pay_annual_tax(PAY_MFJ, 0, 6600000) == 744300);
    check("head of household, taxable $29,500: 10% of 17,000 + 12% of 12,500 = $3,200.00", pay_annual_tax(PAY_HOH, 0, 2950000) == 320000);
    check("halved schedule (Step 2 box), single, taxable $44,500: 10% of 5,962.50 + 12% of 18,275 + 22% of 20,262.50 = 596.25 + 2,193.00 + 4,457.75 = $7,247.00", pay_annual_tax(PAY_SINGLE, 1, 4450000) == 724700);
    check("zero and negative taxable income: $0", pay_annual_tax(PAY_SINGLE, 0, 0) == 0 && pay_annual_tax(PAY_SINGLE, 0, -500) == 0);
    printf("\n== 2. one pay period, worked by hand ==\n");
    memset(&Y, 0, sizeof Y);
    /* single, biweekly $2,000: annual 52,000; taxable 52,000 - 15,000 = 37,000; tax 1,192.50 + 12% x 25,075 (3,009.00) = 4,201.50; / 26 = 161.5962 -> 161.60; SS 124.00; Medicare 29.00; net 2,000 - 161.60 - 124 - 29 = 1,685.40 */
    int rc = run(PAY_SINGLE, 0, 26, 0, 0, 0, 0, 200000, 0, 0);
    check("single, biweekly $2,000: income tax $161.60, Social Security $124.00, Medicare $29.00, net $1,685.40", rc == 0 && R.fit == 16160 && R.ss == 12400 && R.med == 2900 && R.addmed == 0 && R.net == 168540 && R.er_ss == 12400 && R.er_med == 2900);
    memset(&Y, 0, sizeof Y); rc = run(PAY_MFJ, 0, 12, 0, 0, 0, 0, 800000, 0, 0); /* 96,000 - 30,000 = 66,000 -> 7,443.00 / 12 = 620.25 */
    check("married jointly, monthly $8,000: income tax $620.25, Social Security $496.00, Medicare $116.00", rc == 0 && R.fit == 62025 && R.ss == 49600 && R.med == 11600);
    memset(&Y, 0, sizeof Y); rc = run(PAY_SINGLE, 1, 26, 0, 0, 0, 0, 200000, 0, 0); /* halved: 7,247.00 / 26 = 278.7308 -> 278.73 */
    check("single, Step 2 box checked, biweekly $2,000: income tax $278.73 (more than the $161.60 without the box)", rc == 0 && R.fit == 27873);
    memset(&Y, 0, sizeof Y); rc = run(PAY_HOH, 0, 52, 200000, 0, 0, 500, 100000, 0, 0); /* 52,000 - 22,500 = 29,500 -> 3,200.00 - credits 2,000.00 = 1,200.00 / 52 = 23.0769 -> 23.08, + extra 5.00 = 28.08 */
    check("head of household, weekly $1,000, $2,000 of credits (Step 3) and $5.00 extra (Step 4c): income tax $28.08", rc == 0 && R.fit == 2808);
    memset(&Y, 0, sizeof Y); rc = run(PAY_SINGLE, 0, 26, 0, 0, 0, 0, 50000, 0, 0); check("single, biweekly $500 (annual $13,000, under the $15,000 standard deduction): income tax $0, FICA still due ($31.00 and $7.25)", rc == 0 && R.fit == 0 && R.ss == 3100 && R.med == 725);
    memset(&Y, 0, sizeof Y); rc = run(PAY_SINGLE, 0, 26, 99999999, 0, 0, 0, 200000, 0, 0); check("credits larger than the annual tax: tax floors at $0", rc == 0 && R.fit == 0);
    memset(&Y, 0, sizeof Y); rc = run(PAY_SINGLE, 0, 26, 0, 1000000, 0, 0, 200000, 0, 0); /* +10,000 other income: 62,000 - 15,000 = 47,000: 1,192.50 + 12% x 35,075 (4,209.00) = 5,401.50 / 26 = 207.7500 -> 207.75 */
    check("Step 4(a) other income of $10,000 a year raises the tax to $207.75 per period", rc == 0 && R.fit == 20775);
    memset(&Y, 0, sizeof Y); rc = run(PAY_SINGLE, 0, 26, 0, 0, 1000000, 0, 200000, 0, 0); /* -10,000 deductions: 42,000 - 15,000 = 27,000: 1,192.50 + 12% x 15,075 (1,809.00) = 3,001.50 / 26 = 115.4423 -> 115.44 */
    check("Step 4(b) deductions of $10,000 a year lower it to $115.44", rc == 0 && R.fit == 11544);
    printf("\n== 3. pre-tax deductions: 401(k) leaves FICA wages alone, Section 125 reduces both ==\n");
    memset(&Y, 0, sizeof Y); rc = run(PAY_SINGLE, 0, 12, 0, 0, 0, 0, 400000, 40000, 0);
    check("monthly $4,000 with a $400 401(k): income-tax wages $3,600, FICA wages $4,000 (Social Security $248.00)", rc == 0 && R.fit_wages == 360000 && R.fica_wages == 400000 && R.ss == 24800 && R.net == 400000 - R.fit - 24800 - 5800 - 40000);
    memset(&Y, 0, sizeof Y); rc = run(PAY_SINGLE, 0, 12, 0, 0, 0, 0, 400000, 0, 20000);
    check("monthly $4,000 with $200 of Section 125 (health premiums): both wage bases $3,800 (Social Security $235.60, Medicare $55.10)", rc == 0 && R.fit_wages == 380000 && R.fica_wages == 380000 && R.ss == 23560 && R.med == 5510);
    printf("\n== 4. rounding: one rule, half up, once per quantity ==\n");
    memset(&Y, 0, sizeof Y); rc = run(PAY_SINGLE, 0, 26, 0, 0, 0, 0, 250, 0, 0); check("Social Security on $2.50 is 15.5 cents: rounds UP to 16 cents", rc == 0 && R.ss == 16);
    memset(&Y, 0, sizeof Y); rc = run(PAY_SINGLE, 0, 26, 0, 0, 0, 0, 1000, 0, 0); check("Medicare on $10.00 is 14.5 cents: rounds UP to 15 cents", rc == 0 && R.med == 15);
    printf("\n== 5. the Social Security wage base, across pay periods ==\n");
    memset(&Y, 0, sizeof Y); Y.ss_wages = 17500000; Y.ss_tax = 1085000; Y.med_wages = 17500000; rc = run(PAY_SINGLE, 0, 26, 0, 0, 0, 0, 500000, 0, 0); /* $175,000 so far: $1,100 of room: 6.2% of 1,100.00 = 68.20 */
    check("$1,100 of wage base left, pay $5,000: Social Security only on the $1,100 = $68.20; Medicare on all $5,000 = $72.50", rc == 0 && R.ss == 6820 && R.med == 7250 && Y.ss_wages == 17610000);
    rc = run(PAY_SINGLE, 0, 26, 0, 0, 0, 0, 500000, 0, 0); check("the next period: the wage base is used up, Social Security $0.00, Medicare still $72.50", rc == 0 && R.ss == 0 && R.med == 7250 && Y.ss_wages == 17610000);
    memset(&Y, 0, sizeof Y); Y.ss_wages = 17000000; Y.ss_tax = 1091820 - 1; rc = run(PAY_SINGLE, 0, 26, 0, 0, 0, 0, 500000, 0, 0);
    check("the tax itself is capped at 6.2% of the base, $10,918.20: with one cent of cap left, Social Security is one cent whatever the wages", rc == 0 && R.ss == 1);
    memset(&Y, 0, sizeof Y); Y.ss_wages = 17610000; Y.ss_tax = 1091820; rc = run(PAY_SINGLE, 0, 26, 0, 0, 0, 0, 500000, 0, 0); check("with the tax cap reached, Social Security is $0.00", rc == 0 && R.ss == 0);
    printf("\n== 6. Additional Medicare: 0.9%% of Medicare wages above $200,000, employee only ==\n");
    memset(&Y, 0, sizeof Y); Y.med_wages = 19900000; Y.ss_wages = 17610000; Y.ss_tax = 1091820; rc = run(PAY_SINGLE, 0, 26, 0, 0, 0, 0, 500000, 0, 0); /* $199,000 so far + 5,000: $4,000 over: 0.9% = 36.00 */
    check("year-to-date Medicare wages $199,000, pay $5,000: Additional Medicare 0.9% of the $4,000 above $200,000 = $36.00; Medicare $72.50; the employer pays no additional tax", rc == 0 && R.addmed == 3600 && R.med == 7250 && R.er_med == 7250);
    rc = run(PAY_SINGLE, 0, 26, 0, 0, 0, 0, 500000, 0, 0); check("the next period is entirely above the threshold: 0.9% of $5,000 = $45.00", rc == 0 && R.addmed == 4500);
    memset(&Y, 0, sizeof Y); Y.med_wages = 19999900; rc = run(PAY_SINGLE, 0, 26, 0, 0, 0, 0, 100, 0, 0); check("exactly at the threshold: wages of $1.00 on top of $199,999.00 reach $200,000.00 and owe nothing extra", rc == 0 && R.addmed == 0);
    rc = run(PAY_SINGLE, 0, 26, 0, 0, 0, 0, 100, 0, 0); check("one more dollar: 0.9% of $1.00 = 0.9 cent rounds to 1 cent", rc == 0 && R.addmed == 1);
    printf("\n== 7. refusals change nothing ==\n");
    memset(&Y, 0, sizeof Y); Y.ss_wages = 5; pay_ytd_t y0 = Y;
    check("filing status 3: status", run(3, 0, 26, 0, 0, 0, 0, 100, 0, 0) == PAY_ERR_STATUS);
    check("7 pay periods a year: periods", run(0, 0, 7, 0, 0, 0, 0, 100, 0, 0) == PAY_ERR_PERIODS && run(0, 0, 0, 0, 0, 0, 0, 100, 0, 0) == PAY_ERR_PERIODS && run(0, 0, 53, 0, 0, 0, 0, 100, 0, 0) == PAY_ERR_PERIODS);
    check("a negative gross, credit, deduction, extra withholding; and $1,000,000,001: amount", run(0, 0, 26, 0, 0, 0, 0, -1, 0, 0) == PAY_ERR_AMOUNT && run(0, 0, 26, -1, 0, 0, 0, 100, 0, 0) == PAY_ERR_AMOUNT && run(0, 0, 26, 0, -1, 0, 0, 100, 0, 0) == PAY_ERR_AMOUNT && run(0, 0, 26, 0, 0, -1, 0, 100, 0, 0) == PAY_ERR_AMOUNT && run(0, 0, 26, 0, 0, 0, -1, 100, 0, 0) == PAY_ERR_AMOUNT && run(0, 0, 26, 0, 0, 0, 0, PAY_MAX_CENTS + 1, 0, 0) == PAY_ERR_AMOUNT);
    check("a gross of exactly $1,000,000,000 is accepted (the limit is inclusive)", run(0, 0, 26, 0, 0, 0, 0, PAY_MAX_CENTS, 0, 0) == PAY_OK); memset(&Y, 0, sizeof Y); Y.ss_wages = 5; y0 = Y;
    check("401(k) plus Section 125 above gross: deduct", run(0, 0, 26, 0, 0, 0, 0, 1000, 600, 500) == PAY_ERR_DEDUCT && run(0, 0, 26, 0, 0, 0, 0, 1000, 500, 400) == PAY_OK); memset(&Y, 0, sizeof Y); Y.ss_wages = 5; y0 = Y;
    check("extra withholding larger than the pay (net would be negative): deduct", run(0, 0, 26, 0, 0, 0, 100000, 1000, 0, 0) == PAY_ERR_DEDUCT);
    check("negative year-to-date figures: ytd", (Y.ss_wages = -1, run(0, 0, 26, 0, 0, 0, 0, 100, 0, 0) == PAY_ERR_YTD)); Y = y0;
    check("a refused call leaves the year-to-date untouched", (run(3, 0, 26, 0, 0, 0, 0, 100, 0, 0), !memcmp(&Y, &y0, sizeof Y)));
    check("a Section 125 amount of $1,000,000,000.01 on a gross of $1,000,000,000 is an amount error, not a deduction error", run(0, 0, 26, 0, 0, 0, 0, PAY_MAX_CENTS, 0, PAY_MAX_CENTS + 1) == PAY_ERR_AMOUNT);
    check("a negative year-to-date Social Security tax: ytd", (memset(&Y, 0, sizeof Y), Y.ss_tax = -1, run(0, 0, 26, 0, 0, 0, 0, 100, 0, 0) == PAY_ERR_YTD)); memset(&Y, 0, sizeof Y);
    check("zero gross pay is a valid pay period: everything $0, net $0", (memset(&Y, 0, sizeof Y), run(0, 0, 26, 0, 0, 0, 0, 0, 0, 0) == 0 && R.net == 0 && R.fit == 0 && R.ss == 0));
    printf("\n== 8. a whole year, single, biweekly $8,000: the cap and the threshold arrive in the right pay periods ==\n");
    memset(&Y, 0, sizeof Y); int cap_period = 0, add_period = 0, ok = 1; int64_t ss_total = 0, add_total = 0;
    for (int n = 1; n <= 26; n++) { rc = run(PAY_SINGLE, 0, 26, 0, 0, 0, 0, 800000, 0, 0); if (rc) { ok = 0; } ss_total += R.ss; add_total += R.addmed; if (!cap_period && R.ss < 49600) { cap_period = n; } if (!add_period && R.addmed > 0) { add_period = n; } }
    /* wages 8,000 x 22 = 176,000 after 22 periods: 100 left in period 23: 6.2% x 100 = 6.20 */
    check("Social Security: full $496.00 for 22 periods, $6.20 in the 23rd, nothing after; the year totals exactly $10,918.20", ok && cap_period == 23 && ss_total == 1091820 && Y.ss_tax == 1091820);
    /* Medicare wages: 8,000 x 25 = 200,000 after 25 periods; the 26th is all above: 0.9% x 8,000 = 72.00 */
    check("Additional Medicare starts in period 26 ($72.00 = 0.9% of $8,000) and the year's total is $72.00 (Medicare wages $208,000 - $200,000 = $8,000 over)", add_period == 26 && add_total == 7200 && Y.med_wages == 20800000);
    printf("\n%d passed, %d failed\n", pass_n, fail_n); return fail_n ? 1 : 0;
}
