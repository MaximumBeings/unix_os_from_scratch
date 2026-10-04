/* Chapter 48: the financial-ratio engine. Input: a parsed XBRL filing (053_xbrl.h). Output: the quantities below, each either a value or "NA" with the reason, plus the accounting-identity
 * checks that decide whether the filing is trusted at all. All arithmetic is on 64-bit integers (whole dollars; per-share values in 1/10000 dollar) with ONE rounding rule -- round half away
 * from zero on the exact fraction -- so the result is reproducible to the last digit and an independent implementation (ratios_ref.py) can match it exactly. No floating point: this kernel
 * does not enable the FPU, and a ratio of money should not be a float anyway. Units of the results: "x" = hundredths (current_ratio 99 means 0.99x), "bp" = basis points (2531 = 25.31%),
 * "usd" = whole dollars, "usd4" = 1/10000 dollar. The rules (which context, which concept fallbacks, what is "n/a") are written out in the chapter page, 'The rules'. */
#ifndef RATIOS_H
#define RATIOS_H
#include <stdint.h>
#include "053_xbrl.h"

enum { RT_SKIP = 0, RT_PASS = 1, RT_FAIL = 2 };
enum { RT_CHK_BALANCE = 0, RT_CHK_L_PLUS_E, RT_CHK_GROSS, RT_CHK_DUPES, RT_N_CHECKS };
enum { RT_U_X = 0, RT_U_BP, RT_U_USD, RT_U_USD4 };
#define RT_MAX_LINES 20
typedef struct { const char *name; uint8_t na; uint8_t unit; int64_t value; const char *reason; } rt_line_t;
typedef struct {
    uint8_t check[RT_N_CHECKS]; uint8_t accepted; uint8_t n_lines; rt_line_t line[RT_MAX_LINES];
    int32_t period_end_day; uint16_t n_dur, n_cur, n_pri; /* how many dimension-free contexts matched the fiscal year, the balance-sheet date, the prior balance-sheet date */
} rt_report_t;

int rt_analyse(const xb_doc_t *d, int64_t price_cents, rt_report_t *r); /* price_cents <= 0: no price (P/E is NA). Returns XB_OK or XB_ERR_NO_PERIOD / XB_ERR_BAD_DATE. */
int rt_canonical(const rt_report_t *r, char *buf, uint32_t cap); /* the exact text ratios_ref.py prints; returns its length (or -1 if cap is too small) */
int rt_rdiv(int64_t n, int64_t scale, int64_t d, int64_t *out); /* round-half-away-from-zero of n*scale/d for d > 0; -1 on overflow (|n|*scale or d above 2^62) */
const char *rt_check_name(int i);
const char *rt_unit_name(int u);
#endif
