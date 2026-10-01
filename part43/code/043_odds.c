/* See 043_odds.h's own top-of-file comment for the citation of every
 * real conversion formula used here.
 *
 * Stated rounding behavior: every division below truncates toward
 * zero (this book's own established integer-only limit since Chapter
 * 30 -- no floating point). Every demo value this chapter actually
 * uses was deliberately chosen to divide evenly, so this never loses
 * precision in practice; an arbitrary real-world decimal price that
 * does not divide evenly will round down rather than refuse, stated
 * as this chapter's own choice rather than an oversight. */

#include "043_odds.h"

uint32_t odds_gcd(uint32_t a, uint32_t b) {
    if (a == 0u || b == 0u) {
        return 0u;
    }
    while (b != 0u) {
        uint32_t t = b;
        b = a % b;
        a = t;
    }
    return a;
}

int odds_decimal_to_fractional(uint32_t decimal_cents, odds_fractional_t *out) {
    if (decimal_cents < 100u) {
        return 0;
    }
    uint32_t num = decimal_cents - 100u;
    uint32_t den = 100u;
    if (num == 0u) {
        out->numerator = 0u;
        out->denominator = 1u;
        return 1;
    }
    uint32_t g = odds_gcd(num, den);
    out->numerator = num / g;
    out->denominator = den / g;
    return 1;
}

int odds_fractional_to_decimal(const odds_fractional_t *f, uint32_t *out_decimal_cents) {
    if (f->denominator == 0u) {
        return 0;
    }
    *out_decimal_cents = (f->numerator * 100u) / f->denominator + 100u;
    return 1;
}

int odds_decimal_to_american(uint32_t decimal_cents, int32_t *out_american) {
    if (decimal_cents <= 100u) {
        return 0;
    }
    if (decimal_cents >= 200u) {
        *out_american = (int32_t)(decimal_cents - 100u);
    } else {
        *out_american = -(int32_t)(10000u / (decimal_cents - 100u));
    }
    return 1;
}

int odds_american_to_decimal(int32_t american, uint32_t *out_decimal_cents) {
    if (american == 0) {
        return 0;
    }
    if (american > 0) {
        *out_decimal_cents = (uint32_t) american + 100u;
    } else {
        *out_decimal_cents = 10000u / (uint32_t)(-american) + 100u;
    }
    return 1;
}
