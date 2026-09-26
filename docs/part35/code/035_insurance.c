/* See 035_insurance.h's own top-of-file comment for the full citation
 * trail (the real NAIC-cited multiplicative rating model, the real
 * split-limit liability convention) and this chapter's own honesty
 * note on which numbers below are invented. */

#include "035_insurance.h"

/* 64-bit-by-32-bit unsigned long division, one quotient bit at a time
 * (restoring shift-and-subtract) -- this freestanding kernel does not
 * link libgcc, so a plain `/` or `%` on a uint64_t would fail to link
 * with an undefined __udivdi3/__umoddi3, the same real failure
 * Chapters 7, 30, and 33 each document. Copied verbatim from
 * 035_bnpl.c's own udiv64_32() (itself carried forward from Chapter
 * 33): each chapter's own numeric module keeps this helper
 * self-contained rather than reaching across files for it, the same
 * way each chapter's own crypto file is self-contained. */
static uint64_t udiv64_32(uint64_t n, uint32_t d) {
    uint64_t q = 0, r = 0;
    for (int bit = 63; bit >= 0; bit--) {
        r = (r << 1) | ((n >> bit) & 1ull);
        if (r >= d) {
            r -= d;
            q |= (1ull << bit);
        }
    }
    return q;
}

/* Rounds premium * factor_bp / INS_BASIS_POINTS to the nearest cent
 * (round-half-up), entirely without the `/` or `%` operator on a
 * uint64_t. `premium` is bounded below INS_MAX_BASE_RATE_CENTS
 * (2^24) and `factor_bp` below INS_MAX_FACTOR (2^16), so the product
 * stays below 2^40 -- far inside a uint64_t -- both before and after
 * adding half of the 10000 divisor for rounding. */
static uint32_t apply_factor(uint32_t premium, uint32_t factor_bp) {
    uint64_t product = (uint64_t)premium * (uint64_t)factor_bp + (uint64_t)(INS_BASIS_POINTS / 2u);
    return (uint32_t)udiv64_32(product, INS_BASIS_POINTS);
}

/* This chapter's own invented rating tables, looked up against an
 * applicant's real-shaped inputs (see 035_insurance.h: the SHAPE --
 * age/territory/accidents/vehicle/deductible each moving the premium
 * by a real, general multiplicative factor -- is cited; every specific
 * number below is this book's own invention, not a real filed rate). */

static uint32_t age_factor_bp(uint32_t age, uint32_t years_licensed) {
    if (age < 20u) return 22000u;              /* youngest drivers: highest risk band */
    if (age < 25u) return 16000u;
    if (years_licensed < 2u) return 15000u;    /* a newly-licensed driver of any age */
    if (age < 65u) return 10000u;              /* baseline */
    return 11500u;                              /* senior band, this book's own modest surcharge */
}

static uint32_t territory_factor_bp(uint32_t tier) {
    static const uint32_t table[4] = {10000u, 10000u, 12500u, 16000u}; /* index 0 unused (tiers are 1-3) */
    return (tier >= 1u && tier <= 3u) ? table[tier] : 0u; /* 0 signals "unknown tier" to the caller */
}

static uint32_t accident_factor_bp(uint32_t count) {
    if (count == 0u) return 10000u;
    if (count == 1u) return 13000u;
    if (count == 2u) return 18000u;
    return 25000u; /* 3 or more */
}

static uint32_t vehicle_factor_bp(uint32_t value_cents, uint32_t age_years) {
    uint32_t f = 10000u;
    if (value_cents >= 4000000u) {       /* >= $40,000: costlier to repair/replace */
        f = 12000u;
    } else if (value_cents < 1000000u) { /* < $10,000 */
        f = 9000u;
    }
    if (age_years >= 10u) {              /* an older vehicle, this book's own modest discount */
        f = apply_factor(f, 9500u);
    }
    return f;
}

static uint32_t deductible_factor_bp(uint32_t deductible_cents) {
    if (deductible_cents >= 100000u) return 8000u; /* >= $1,000: lower premium */
    if (deductible_cents >= 50000u) return 9000u;  /* >= $500 */
    return 11000u;                                  /* below $500 */
}

int ins_quote_one_carrier(const ins_applicant_t *a, const ins_carrier_t *carrier,
                          ins_quote_t *out_quote) {
    if (carrier->base_rate_cents >= INS_MAX_BASE_RATE_CENTS) {
        return 0;
    }
    uint32_t looked_up[INS_NUM_FACTORS];
    looked_up[0] = age_factor_bp(a->driver_age, a->years_licensed);
    looked_up[1] = territory_factor_bp(a->territory_tier);
    looked_up[2] = accident_factor_bp(a->at_fault_accidents_3yr);
    looked_up[3] = vehicle_factor_bp(a->vehicle_value_cents, a->vehicle_age_years);
    looked_up[4] = deductible_factor_bp(a->collision_deductible_cents);

    uint32_t premium = carrier->base_rate_cents;
    for (uint32_t i = 0; i < INS_NUM_FACTORS; i++) {
        uint32_t f = apply_factor(looked_up[i], carrier->factor_bp[i]);
        if (f >= INS_MAX_FACTOR || looked_up[i] >= INS_MAX_FACTOR) {
            return 0;
        }
        premium = apply_factor(premium, f);
        if (premium >= INS_MAX_BASE_RATE_CENTS) {
            return 0; /* refused, not silently truncated */
        }
        out_quote->factor_bp[i] = f;
    }

    uint32_t n = 0;
    while (n < 23u && carrier->name[n] != '\0') {
        out_quote->carrier_name[n] = carrier->name[n];
        n++;
    }
    out_quote->carrier_name[n] = '\0';
    out_quote->premium_cents = premium;
    return 1;
}

uint32_t ins_rank_quotes(const ins_applicant_t *a, const ins_carrier_t *carriers, uint32_t n,
                         ins_quote_t *out_quotes) {
    if (n == 0u || n > INS_MAX_CARRIERS) {
        return 0;
    }
    uint32_t got = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (ins_quote_one_carrier(a, &carriers[i], &out_quotes[got])) {
            got++;
        }
    }
    /* Insertion sort, ascending by premium -- got is at most
     * INS_MAX_CARRIERS (4), so this book does not reach for anything
     * fancier. */
    for (uint32_t i = 1; i < got; i++) {
        ins_quote_t key = out_quotes[i];
        uint32_t j = i;
        while (j > 0u && out_quotes[j - 1u].premium_cents > key.premium_cents) {
            out_quotes[j] = out_quotes[j - 1u];
            j--;
        }
        out_quotes[j] = key;
    }
    return got;
}
