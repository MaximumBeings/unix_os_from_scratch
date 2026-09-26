#ifndef UNIX_OS_035_BNPL_H
#define UNIX_OS_035_BNPL_H

#include <stdint.h>

/* A real "Pay in 4" buy-now-pay-later installment schedule, plus the
 * real Truth in Lending (Regulation Z, 12 CFR Part 1026) closed-end
 * credit disclosures a lender would owe on it -- amount financed,
 * finance charge, total of payments, payment schedule, and the annual
 * percentage rate computed by Regulation Z's own Appendix J actuarial
 * method -- all in integer cents and integer fixed-point arithmetic,
 * with no floating point anywhere in this kernel.
 *
 * Citation trail. This chapter's own cloud sandbox runs behind a
 * network egress policy that blocks direct fetches of the CFPB's own
 * site (consumerfinance.gov), the eCFR (ecfr.gov), govinfo.gov, and
 * Cornell LII (law.cornell.edu) -- checked directly, each one returned
 * an explicit "blocked by the network egress proxy" refusal. Every
 * Regulation Z rule below was therefore located through web search
 * results pointing at those official pages, and the wording below is
 * the wording those search results surfaced from them, not text this
 * book fetched and read in full itself. That is a weaker citation than
 * Chapters 30-32 had, and it is stated here rather than hidden:
 *
 *   - 12 CFR 1026.2(a)(17) ("creditor") and its official
 *     interpretation: Regulation Z's creditor definition covers a
 *     person who regularly extends consumer credit "that is subject to
 *     a finance charge or is payable by written agreement in more than
 *     four installments (not including a down payment)". This is the
 *     real reason fee-free "Pay in 4" products have historically sat
 *     outside Regulation Z's closed-end disclosure rules: at most four
 *     installments, and no finance charge. bnpl_build_pay_in_4()
 *     records that test honestly in bnpl_plan_t.reg_z_covered below.
 *   - 12 CFR 1026.18 ("Content of disclosures"): the amount financed
 *     is the principal loan amount or cash price "subtracting any
 *     downpayment", plus other amounts financed that are not part of
 *     the finance charge, minus any prepaid finance charge; the total
 *     of payments is "the amount you will have paid when you have made
 *     all scheduled payments"; the payment schedule is "the number,
 *     amounts, and timing of payments scheduled to repay the
 *     obligation".
 *   - Appendix J to Part 1026: the unit-period is "that common period,
 *     not to exceed 1 year, that occurs most frequently in a
 *     transaction"; a standard interval is "a day, week, semimonth,
 *     month, or a multiple of a week or a month"; and the annual
 *     percentage rate is "the nominal annual percentage rate
 *     determined by multiplying the unit-period rate by the number of
 *     unit-periods in a year". A two-week unit-period is a multiple of
 *     a week, so a year holds 52 / 2 = 26 of them.
 *   - 12 CFR 1026.22(a)(2): for a regular transaction, the disclosed
 *     APR "shall be considered accurate if it is not more than 1/8 of
 *     1 percentage point above or below" the exact rate.
 *
 * Real regulatory history, stated for context and not relied on by any
 * code below: in May 2024 the CFPB issued an interpretive rule treating
 * Pay-in-4 lenders as card issuers under Regulation Z; in May 2025 the
 * CFPB withdrew it (per CFPB and law-firm coverage surfaced by the same
 * web searches). This chapter builds only the closed-end disclosure
 * arithmetic, which neither event changed.
 *
 * How "Pay in 4" maps onto those real rules, honestly:
 *
 *   - Installment 1 is paid at checkout, the moment the credit is
 *     extended (consummation). Under 1026.18 that is a DOWNPAYMENT, not
 *     a scheduled repayment of credit: it is subtracted from the cash
 *     price to get the amount financed, and it is not part of the total
 *     of payments. Only installments 2-4 repay credit.
 *   - Installments 2-4 fall exactly one, two, and three two-week
 *     unit-periods after checkout, so the transaction is "regular" in
 *     Appendix J's sense (every payment on a unit-period boundary, no
 *     odd first period), which is what makes the simple equation below
 *     the complete, exact Appendix J computation for this plan rather
 *     than an approximation of it.
 *   - A plan with a flat fee (this chapter's second demo plan) has a
 *     finance charge, so Regulation Z's closed-end disclosure rules DO
 *     apply to it even with only four installments, and its APR is the
 *     number a consumer actually needs to see. A fee-free plan has a
 *     finance charge of zero and an APR of exactly 0.00%.
 *
 * This book's own choices, not taken from any regulation:
 *
 *   - Rounding: the cash price is split into four equal installments
 *     in whole cents, and any 1-3 leftover cents are added to the FIRST
 *     installment (paid at checkout), so the three repayments of credit
 *     are always equal. A flat fee is split evenly across installments
 *     2-4, any 1-2 leftover cents added to installment 2. Real lenders
 *     choose their own rounding conventions; this one is simply stated.
 *   - The 2-week interval and "fee paid in installments, not at
 *     checkout" plan structure are this chapter's own demo design,
 *     typical of Pay-in-4 products but not copied from any one lender.
 *
 * The Appendix J equation for a regular single-advance transaction with
 * payments P_1..P_n at unit-periods 1..n, solved for the unit-period
 * rate i:
 *
 *     amount_financed = sum over k of  P_k / (1 + i)^k
 *
 * bnpl_compute_apr() solves it by bisection over v = 1 / (1 + i), the
 * one-period discount factor, in unsigned Q30 fixed point (v = 1.0 is
 * 1 << 30). Working in v rather than i keeps every intermediate value
 * bounded by the undiscounted total of payments -- v is never above
 * 1.0, so v^k never grows -- which is what lets 64-bit integers hold
 * every intermediate product without overflow. Converting the solved v
 * back to i = 1/v - 1 needs exactly one 64-bit-by-32-bit division, and
 * this freestanding kernel has never linked libgcc's __udivdi3 (the
 * same real link failure Chapters 7 and 30 hit), so 035_bnpl.c carries
 * its own small shift-and-subtract long-division helper instead.
 *
 * Every person, merchant, and amount in this chapter's own demo is
 * fictional, invented for this book. Nothing here is legal or
 * compliance advice, and nothing here is a real lender's pricing. */

#define BNPL_INSTALLMENTS 4u
#define BNPL_INTERVAL_DAYS 14u        /* two weeks -- this chapter's own plan design */
#define BNPL_UNIT_PERIODS_PER_YEAR 26u /* Appendix J: a 2-week unit-period, 52 / 2 */
#define BNPL_MAX_PAYMENTS 12u          /* bnpl_compute_apr()'s own stated limit */

/* Stated limit: every amount this module handles stays below 2^24
 * cents ($167,772.16). With v <= 1.0 in Q30 (2^30), a cents value times
 * v stays below 2^54, and summing up to BNPL_MAX_PAYMENTS of them stays
 * far below 2^64 -- so no intermediate can overflow a uint64_t. A
 * purchase at or above this limit is refused, never truncated. */
#define BNPL_MAX_CENTS 0x01000000u

#define BNPL_Q30_ONE (1u << 30)

typedef struct {
    uint16_t year;
    uint8_t month; /* 1-12 */
    uint8_t day;   /* 1-31 */
} bnpl_date_t;

typedef struct {
    uint32_t cash_price_cents;
    uint32_t fee_cents;                           /* the finance charge, if any */
    bnpl_date_t checkout_date;                    /* consummation */
    uint32_t installment_cents[BNPL_INSTALLMENTS];
    bnpl_date_t due_date[BNPL_INSTALLMENTS];

    /* The real 1026.18 disclosures. */
    uint32_t downpayment_cents;       /* installment 1, paid at checkout */
    uint32_t amount_financed_cents;   /* cash price minus downpayment */
    uint32_t finance_charge_cents;    /* the fee */
    uint32_t total_of_payments_cents; /* installments 2-4 only */
    uint32_t apr_hundredths;          /* APR in hundredths of a percent, e.g. 5167 = 51.67% */

    /* 1 when 1026.2(a)(17)'s test is met: a finance charge, or more
     * than four installments not counting the downpayment. */
    int reg_z_covered;
} bnpl_plan_t;

/* Adds `days` to `d` in the proleptic Gregorian calendar (4/100/400
 * leap-year rule). */
bnpl_date_t bnpl_add_days(bnpl_date_t d, uint32_t days);

/* Builds a real Pay-in-4 schedule plus every real disclosure above,
 * including the Appendix J APR. Returns 1 on success, or 0 (and writes
 * nothing meaningful) if the cash price is zero, is at or above
 * BNPL_MAX_CENTS, is too small to split into four nonzero installments,
 * or if the fee is at or above BNPL_MAX_CENTS. */
int bnpl_build_pay_in_4(uint32_t cash_price_cents, uint32_t fee_cents,
                        bnpl_date_t checkout_date, bnpl_plan_t *out_plan);

/* Solves Appendix J's equation for a regular single-advance
 * transaction: `n` payments `payments_cents[0..n-1]` falling at
 * unit-periods 1..n, repaying `amount_financed_cents`, with
 * `periods_per_year` unit-periods in a year. On success writes the APR
 * in hundredths of a percent (rounded to nearest) to *out_apr_hundredths
 * and the unit-period rate in Q30 to *out_rate_q30 (either may be 0),
 * and returns 1. Returns 0 if n is 0 or above BNPL_MAX_PAYMENTS, if any
 * amount is at or above BNPL_MAX_CENTS, or if the payments total LESS
 * than the amount financed (a negative finance charge has no APR). */
int bnpl_compute_apr(uint32_t amount_financed_cents, const uint32_t *payments_cents,
                     uint32_t n, uint32_t periods_per_year,
                     uint32_t *out_apr_hundredths, uint32_t *out_rate_q30);

/* DE 48 ("Additional data - private") payload carrying one plan's
 * schedule and disclosures inside this chapter's own ISO 8583 0110
 * response. The layout is this book's own, fixed-width, ASCII digits
 * only, because DE 48 is a private-use field whose contents the ISO
 * 8583 field table leaves to the parties exchanging it (see
 * 035_iso8583.h):
 *
 *   off  len  content
 *     0    2  "P4"  plan code
 *     2    1  installment count, "4"
 *     3    2  interval in days, "14"
 *     5   80  4 x (8-digit YYYYMMDD due date + 12-digit cents amount)
 *    85   12  amount financed, cents
 *    97   12  finance charge, cents
 *   109   12  total of payments, cents
 *   121    5  APR in hundredths of a percent
 *   total 126 bytes */
#define BNPL_DE48_LEN 126u

uint32_t bnpl_encode_de48(const bnpl_plan_t *plan, uint8_t *out, uint32_t out_size);

/* Parses bnpl_encode_de48()'s own layout back into a plan, refusing
 * (returning 0) on a wrong length, a wrong plan code, or any non-digit
 * byte in a numeric position. Fields DE 48 does not carry (cash price,
 * fee, checkout date, reg_z_covered) are left zeroed; downpayment is
 * set from installment 1. */
int bnpl_decode_de48(const uint8_t *buf, uint32_t len, bnpl_plan_t *out_plan);

#endif
