#ifndef UNIX_OS_052_ODDS_H
#define UNIX_OS_052_ODDS_H

#include <stdint.h>

/* Real sports-betting odds format conversion -- decimal, fractional,
 * and American (moneyline) odds all describe the exact same real
 * payout, just in three different real conventions bookmakers around
 * the world actually use (decimal: common in Europe/Australia and the
 * convention Betfair's own real API always uses, see 052_betfair.h;
 * fractional: the UK/Ireland convention; American/moneyline: the
 * convention US sportsbooks use). There is no single standards body
 * for this math the way ISO 8583 or GS1 have one -- these formulas are
 * cited through web search results, the same weaker honesty note this
 * book has used since DE 39's "00":
 *
 *   decimal  -> fractional:  fractional = decimal - 1
 *   decimal  -> American:    decimal >= 2.00: (decimal - 1) * 100
 *                             decimal <  2.00: -100 / (decimal - 1)
 *   American -> decimal:     American > 0: (American / 100) + 1
 *                             American < 0: (100 / -American) + 1
 *   fractional -> decimal:   decimal = fractional + 1
 *
 * This book's own established limit since Chapter 30 (no floating
 * point, no native 64-bit division): decimal odds are tracked as a
 * real fixed-point integer in hundredths (this file's own stated
 * choice, matching how every earlier chapter has tracked currency in
 * cents) -- so a real decimal price of 2.50 is the integer 250, and a
 * real American price of -150 is the plain signed integer -150.
 * Fractional odds are tracked as a real reduced numerator/denominator
 * pair (Euclid's GCD, a real, ancient algorithm, not this chapter's
 * own invention), since a fractional price like "6/4" is
 * conventionally shown in its own real lowest terms ("3/2"). */

typedef struct {
    uint32_t numerator;
    uint32_t denominator;
} odds_fractional_t;

/* Euclid's real GCD algorithm, used to reduce a fractional price to
 * its own real lowest terms. Returns 0 if either input is 0. */
uint32_t odds_gcd(uint32_t a, uint32_t b);

/* Converts real decimal odds (hundredths, e.g. 250 for 2.50) to a real
 * reduced fractional price. Returns 1 on success, or 0 -- refusing
 * outright -- if `decimal_cents` is below 100 (a real decimal price
 * below 1.00 does not exist: it would imply losing money on a win). */
int odds_decimal_to_fractional(uint32_t decimal_cents, odds_fractional_t *out);

/* Converts a real reduced (or unreduced) fractional price back to
 * real decimal odds (hundredths). Returns 1 on success, or 0 if
 * `f->denominator == 0`. */
int odds_fractional_to_decimal(const odds_fractional_t *f, uint32_t *out_decimal_cents);

/* Converts real decimal odds (hundredths) to a real American
 * (moneyline) price (which may be negative -- a real, ordinary case
 * for any decimal price under 2.00, called "odds-on" or a favorite).
 * Returns 1 on success, or 0 if `decimal_cents` is below 100. */
int odds_decimal_to_american(uint32_t decimal_cents, int32_t *out_american);

/* Converts a real American (moneyline) price back to real decimal
 * odds (hundredths). Returns 1 on success, or 0 -- refusing outright
 * -- if `american == 0` (a real American price is never exactly
 * zero: it is always at least +100 or at most -100). */
int odds_american_to_decimal(int32_t american, uint32_t *out_decimal_cents);

#endif
