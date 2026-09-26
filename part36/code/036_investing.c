/* See 036_investing.h's own top-of-file comment for the full citation
 * trail (the real round-up/risk-band shape, this book's own invented
 * fund lineup, NAVs, and exact allocation percentages). */

#include "036_investing.h"

const inv_fund_t INV_FUNDS[INV_NUM_FUNDS] = {
    {"FBNDX", 1015u}, /* a fictional bond fund, $10.15 NAV */
    {"FSTKX", 4287u}, /* a fictional stock fund, $42.87 NAV */
    {"FCSHX",  100u}, /* a fictional cash-reserve fund, the real, standard stable $1.00 NAV */
};

const inv_allocation_t INV_ALLOCATIONS[3] = {
    {{7000u, 2000u, 1000u}}, /* CONSERVATIVE: 70% bond / 20% stock / 10% cash */
    {{4000u, 5000u, 1000u}}, /* MODERATE:     40% bond / 50% stock / 10% cash */
    {{1000u, 8000u, 1000u}}, /* AGGRESSIVE:   10% bond / 80% stock / 10% cash */
};

/* 64-bit-by-32-bit unsigned long division (restoring shift-and-subtract)
 * -- this freestanding kernel does not link libgcc, so a plain `/` on
 * a uint64_t would fail to link with an undefined __udivdi3, the same
 * real failure Chapters 7, 30, 33, and 34 each document. Copied
 * verbatim from 036_bnpl.c's own udiv64_32() -- each chapter's own
 * numeric module keeps this helper self-contained rather than reaching
 * across files for it. */
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

int inv_score_questionnaire(const uint32_t *answers, uint32_t n, uint32_t *out_score,
                            inv_risk_band_t *out_band) {
    if (n == 0u) {
        return 0;
    }
    uint32_t sum = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (answers[i] > 100u) {
            return 0;
        }
        sum += answers[i];
    }
    /* round-half-up average: n is at most a handful of real answers
     * (this chapter's own demo uses 3), so a plain uint32_t divide is
     * safe -- no 64-bit value is ever involved here. */
    uint32_t score = (sum + n / 2u) / n;
    *out_score = score;
    if (score <= 33u) {
        *out_band = INV_RISK_CONSERVATIVE;
    } else if (score <= 66u) {
        *out_band = INV_RISK_MODERATE;
    } else {
        *out_band = INV_RISK_AGGRESSIVE;
    }
    return 1;
}

uint32_t inv_round_up_cents(uint32_t purchase_cents) {
    uint32_t remainder = purchase_cents % 100u;
    return (remainder == 0u) ? 0u : (100u - remainder);
}

int inv_allocate(uint32_t pool_cents, inv_risk_band_t band, uint32_t out_cents[INV_NUM_FUNDS]) {
    if (pool_cents >= INV_MAX_CENTS) {
        return 0;
    }
    const inv_allocation_t *a = &INV_ALLOCATIONS[band];
    uint32_t assigned = 0;
    for (uint32_t i = 0; i + 1u < INV_NUM_FUNDS; i++) {
        uint64_t product = (uint64_t) pool_cents * (uint64_t) a->target_bp[i] + 5000u;
        out_cents[i] = (uint32_t) udiv64_32(product, 10000u);
        assigned += out_cents[i];
    }
    /* The last fund (this chapter's own fixed convention: FCSHX, the
     * cash fund) absorbs whatever is left, guaranteeing an exact sum
     * rather than a rounding-error approximation of pool_cents. */
    out_cents[INV_NUM_FUNDS - 1u] = pool_cents - assigned;
    return 1;
}

int inv_cents_to_milli_shares(uint32_t cents, uint32_t nav_cents, uint32_t *out_milli_shares) {
    if (nav_cents == 0u) {
        return 0;
    }
    uint64_t product = (uint64_t) cents * 1000ull + (uint64_t) (nav_cents / 2u);
    *out_milli_shares = (uint32_t) udiv64_32(product, nav_cents);
    return 1;
}

static void put_digits(char *buf, uint32_t *pos, uint32_t value, uint32_t min_width) {
    char rev[10];
    uint32_t rn = 0;
    if (value == 0u) {
        rev[rn++] = '0';
    }
    while (value > 0u) {
        rev[rn++] = (char) ('0' + (value % 10u));
        value /= 10u;
    }
    while (rn < min_width) {
        rev[rn++] = '0';
    }
    while (rn > 0u) {
        buf[(*pos)++] = rev[--rn];
    }
}

uint32_t inv_format_milli_shares(uint32_t milli_shares, char *out, uint32_t out_size) {
    if (out_size < 6u) {
        return 0;
    }
    uint32_t whole = milli_shares / 1000u;
    uint32_t frac = milli_shares % 1000u;
    uint32_t pos = 0;
    put_digits(out, &pos, whole, 1u);
    out[pos++] = '.';
    put_digits(out, &pos, frac, 3u);
    out[pos] = '\0';
    return pos;
}
