#ifndef UNIX_OS_037_INVESTING_H
#define UNIX_OS_037_INVESTING_H

#include <stdint.h>

/* A real "round-up" micro-investing mechanic (spare change from a
 * purchase, rounded up to the next whole dollar, invested) plus a real
 * risk-questionnaire-driven target-allocation model (a risk score maps
 * to one of three real, standard risk bands, each with its own target
 * percentage split across a small number of funds) -- computed
 * entirely in integer cents and integer fixed-point arithmetic, with
 * no floating point and no native 64-bit division/modulo anywhere in
 * this kernel (the same __udivdi3/__umoddi3 link failure Chapters 7,
 * 30, 33, and 34 each document and avoid).
 *
 * The round-up mechanic and the three-band risk-questionnaire model
 * (conservative/moderate/aggressive, each with its own target
 * stock/bond/cash split) are real and general -- this is the well-known
 * shape every real micro-investing/robo-advisor app (Acorns' own
 * "Round-Ups", and target-date/risk-based robo-advisors generally) is
 * built around. What is this book's own invention, stated plainly, the
 * same honesty discipline Chapter 34 applied to its own rating tables:
 * the three specific fictional funds and their fictional NAVs (net
 * asset value per share) below, the exact percentage split each risk
 * band targets, and the specific risk-questionnaire scoring formula.
 * Real robo-advisors each use their own proprietary allocation models
 * and fund lineups, none of which is public in a form this book could
 * cite field-for-field.
 *
 * This chapter's own three fictional funds use the real, common US
 * mutual-fund "X"-suffix ticker convention (a real naming pattern, an
 * invented ticker): `FBNDX` (a fictional bond fund), `FSTKX` (a
 * fictional stock fund), and `FCSHX` (a fictional cash-reserve fund,
 * priced at the real, standard stable $1.00 NAV real money-market/cash
 * funds use).
 *
 * Every dollar amount below is invested as a real FRACTIONAL share --
 * exactly how real round-up micro-investing apps work, since a few
 * cents of spare change buys far less than one whole share of most
 * funds. Share quantities are carried in MILLI-SHARES (thousandths of
 * a share, so 0.083 shares is the integer 83) rather than a floating
 * share count, using the same round-half-up-via-udiv64_32 fixed-point
 * discipline Chapters 33/34 established. */

#define INV_NUM_FUNDS 3u
#define INV_MAX_PURCHASES 8u
#define INV_MAX_CENTS 0x00FFFFFFu /* stated limit, refused not truncated */

typedef struct {
    char ticker[8];
    uint32_t nav_cents; /* net asset value per share, in cents */
} inv_fund_t;

typedef enum {
    INV_RISK_CONSERVATIVE = 0,
    INV_RISK_MODERATE = 1,
    INV_RISK_AGGRESSIVE = 2
} inv_risk_band_t;

/* This chapter's own three real, standard risk bands and their target
 * allocations across [FBNDX, FSTKX, FCSHX], in basis points (parts per
 * 10000). The SHAPE (bonds dominate conservative, stocks dominate
 * aggressive, a small cash cushion throughout) is the real, general
 * pattern every risk-based allocation model uses; these exact
 * percentages are this book's own invented choice. */
typedef struct {
    uint32_t target_bp[INV_NUM_FUNDS];
} inv_allocation_t;

/* Sums a real risk-questionnaire's own answers (each 0-100, this
 * book's own invented scoring convention: higher means more risk
 * tolerance) into a single 0-100 average score, and classifies that
 * score into one of the three real risk bands: <= 33 CONSERVATIVE,
 * 34-66 MODERATE, >= 67 AGGRESSIVE. Returns 0 (refusing outright) if
 * `n` is 0, or any answer is above 100. */
int inv_score_questionnaire(const uint32_t *answers, uint32_t n, uint32_t *out_score,
                            inv_risk_band_t *out_band);

/* Rounds `purchase_cents` up to the next whole dollar and returns the
 * spare change (0-99 cents). A purchase already an exact whole number
 * of dollars rounds up by 0. */
uint32_t inv_round_up_cents(uint32_t purchase_cents);

/* Splits `pool_cents` (the sum of one or more round-ups) across
 * INV_NUM_FUNDS according to `band`'s own target allocation, in
 * round-half-up cents, with the FIRST two funds' shares computed
 * directly from the target basis points and the THIRD (cash) fund
 * absorbing whatever cents remain -- this book's own explicit
 * leftover-cents convention (the same pattern Chapters 32/33 used),
 * chosen so the three amounts always sum to exactly `pool_cents`, not
 * an approximation of it. Refuses (returns 0) if `pool_cents` is at or
 * above INV_MAX_CENTS. */
int inv_allocate(uint32_t pool_cents, inv_risk_band_t band, uint32_t out_cents[INV_NUM_FUNDS]);

/* Converts a dollar amount in cents, at a given fund's own NAV (cents
 * per share), into milli-shares (thousandths of a share), round-half-up,
 * using this file's own self-contained restoring shift-and-subtract
 * `udiv64_32()` rather than the `/` operator on a uint64_t. Refuses
 * (returns 0) if `nav_cents` is 0. */
int inv_cents_to_milli_shares(uint32_t cents, uint32_t nav_cents, uint32_t *out_milli_shares);

/* Formats `milli_shares` as a fixed-point decimal string "D.DDD" (e.g.
 * 83 -> "0.083", 24999 -> "24.999") into `out` (at least 10 bytes).
 * Returns the string length. */
uint32_t inv_format_milli_shares(uint32_t milli_shares, char *out, uint32_t out_size);

extern const inv_fund_t INV_FUNDS[INV_NUM_FUNDS]; /* FBNDX, FSTKX, FCSHX, in that order */
extern const inv_allocation_t INV_ALLOCATIONS[3]; /* indexed by inv_risk_band_t */

#endif
