/* Chapter 52: US payroll withholding for one pay period: federal income tax by the IRS percentage method (the method of Publication 15-T for automated payroll systems, Form W-4 of 2020 or later), Social Security and Medicare (FICA), the employer's share, and
 * year-to-date accumulators that make the Social Security wage base and the Additional Medicare threshold work across a whole year of pay runs. All money is an integer number of CENTS; every percentage is applied to an exact integer and rounded ONCE, half up, to the cent.
 * THE RULES (tax year 2025; see the chapter page for where each number comes from and what was and was not verified against the IRS's own publication):
 *   wages for income tax = gross - pre-tax 401(k) - Section 125 (cafeteria plan) deductions;   wages for FICA = gross - Section 125 (a 401(k) deferral is still FICA wages).
 *   FEDERAL INCOME TAX (percentage method): annual wages A = wages x periods + W-4 step 4(a) other income - step 4(b) deductions. Taxable = max(0, A - the standard deduction of the 2025 tables: single 15,000, married filing jointly 30,000, head of household 22,500; Publication 15-T builds the same
 *   number into its tables as (8,600 or 12,900) plus the table's own first zero-rate row). If the Step 2 checkbox IS checked (two jobs), the standard deduction and every bracket limit are HALVED (the author's reading of the checkbox tables). Tax = the bracket schedule
 *   (10, 12, 22, 24, 32, 35, 37 percent) on taxable; less W-4 step 3 credits (not below 0); divided by the periods per year, rounded half up; plus W-4 step 4(c) extra withholding per period.
 *   SOCIAL SECURITY: 6.2 percent of FICA wages, but only up to the annual wage base ($176,100 in 2025): the tax never exceeds 6.2 percent of the base ($10,918.20), however the pay periods round. The employer pays the same.
 *   MEDICARE: 1.45 percent of all FICA wages, employer pays the same; ADDITIONAL MEDICARE: 0.9 percent, EMPLOYEE only, on the part of the year's Medicare wages above $200,000 (withheld from the pay period in which the threshold is crossed).
 * NET PAY = gross - income tax - Social Security - Medicare - Additional Medicare - 401(k) - Section 125. The engine guarantees this identity and that nothing is negative.
 * NOT covered: state and local taxes, other pre-tax deductions, supplemental-wage (bonus) flat-rate withholding, the older pre-2020 W-4, nonresident aliens, tips, taxable fringe benefits, garnishments, quarterly deposits and Forms 941/W-2.
 * NOT TAX ADVICE. The tables are from the author's reading of the 2025 rules and were not checked against the IRS's own document (irs.gov is unreachable from the sandbox that built this book). */
#ifndef PAYROLL_H
#define PAYROLL_H
#include <stdint.h>

enum { PAY_OK = 0, PAY_ERR_STATUS = -1, PAY_ERR_PERIODS = -2, PAY_ERR_AMOUNT = -3, PAY_ERR_DEDUCT = -4, PAY_ERR_YTD = -5 };
enum { PAY_SINGLE = 0, PAY_MFJ = 1, PAY_HOH = 2 };
#define PAY_MAX_CENTS 100000000000ll /* $1,000,000,000: far beyond any pay period; keeps every product inside 64 bits */
typedef struct { uint8_t status, step2_checked; uint16_t periods; int64_t credits_a, other_income_a, deductions_a, extra_pp; } pay_emp_t; /* W-4: step 3 credits, 4(a), 4(b) are ANNUAL cents; 4(c) is per period */
typedef struct { int64_t gross, k401, s125; } pay_period_t;
typedef struct { int64_t ss_wages, ss_tax, med_wages; } pay_ytd_t;
typedef struct { int64_t fit_wages, fica_wages, fit, ss, med, addmed, net, er_ss, er_med; } pay_result_t;

int pay_compute(const pay_emp_t *e, const pay_period_t *p, pay_ytd_t *ytd, pay_result_t *r); /* PAY_OK or an error; on an error nothing is changed; on success ytd is advanced */
int pay_check(const pay_period_t *p, const pay_result_t *r); /* 0 when gross = net + every withholding and deduction, and nothing is negative */
int64_t pay_annual_tax(int status, int halved, int64_t taxable_cents); /* the bracket schedule alone, in cents, rounded half up */
const char *pay_strerror(int rc);
#endif
