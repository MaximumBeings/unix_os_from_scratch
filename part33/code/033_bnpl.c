/* See 033_bnpl.h's own top-of-file comment for the full citation trail
 * (Regulation Z 1026.2(a)(17), 1026.18, 1026.22(a)(2), and Appendix J),
 * this chapter's own honest note on how those citations were obtained,
 * and every rounding convention that is this book's own choice. */

#include "033_bnpl.h"

/* ---------------------------------------------------------------- */
/* Dates                                                            */
/* ---------------------------------------------------------------- */

static int is_leap_year(uint32_t y) {
    return ((y % 4u) == 0u && (y % 100u) != 0u) || (y % 400u) == 0u;
}

static uint32_t days_in_month(uint32_t y, uint32_t m) {
    static const uint8_t dim[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2u && is_leap_year(y)) {
        return 29u;
    }
    return dim[m - 1u];
}

bnpl_date_t bnpl_add_days(bnpl_date_t d, uint32_t days) {
    uint32_t y = d.year, m = d.month, day = d.day;
    while (days > 0u) {
        uint32_t left_in_month = days_in_month(y, m) - day;
        if (days <= left_in_month) {
            day += days;
            days = 0u;
        } else {
            days -= left_in_month + 1u;
            day = 1u;
            m++;
            if (m > 12u) {
                m = 1u;
                y++;
            }
        }
    }
    bnpl_date_t out;
    out.year = (uint16_t)y;
    out.month = (uint8_t)m;
    out.day = (uint8_t)day;
    return out;
}

/* ---------------------------------------------------------------- */
/* Fixed-point arithmetic, with no libgcc                           */
/* ---------------------------------------------------------------- */

/* (a * v) >> 30 for a < 2^62 and v <= 2^30, without ever forming the
 * full up-to-92-bit product: split a into its high and low 30-bit
 * halves, a = ah * 2^30 + al, so (a * v) >> 30 = ah * v + (al * v) >> 30,
 * where ah * v < 2^62 and al * v < 2^60 both fit a uint64_t. Only
 * 64-bit multiplies, shifts, and adds -- all of which gcc emits inline
 * on i386 -- never a 64-bit division. */
static uint64_t mul_q30(uint64_t a, uint32_t v) {
    uint64_t ah = a >> 30;
    uint64_t al = a & ((1ull << 30) - 1ull);
    return ah * (uint64_t)v + ((al * (uint64_t)v) >> 30);
}

/* 64-bit by 32-bit unsigned long division, one quotient bit at a time
 * (restoring shift-and-subtract). This freestanding kernel does not
 * link libgcc, so a plain `/` on a uint64_t would fail to link with an
 * undefined __udivdi3 -- the same real failure 033_pmm.c (Chapter 7)
 * and 033_fedwire.c (Chapter 30) each document. The remainder stays
 * below the 32-bit divisor, so after one left shift it still fits a
 * uint64_t with room to spare. */
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

/* Present value, in cents scaled by 2^30, of payments[0..n-1] falling at
 * unit-periods 1..n under one-period discount factor v (Q30):
 *
 *     P_1 v + P_2 v^2 + ... + P_n v^n  =  v (P_1 + v (P_2 + ... + v P_n))
 *
 * evaluated by Horner's rule from the last payment backward. Every
 * partial sum is at most (sum of payments) * 2^30 < 12 * 2^24 * 2^30,
 * comfortably inside a uint64_t (see BNPL_MAX_CENTS in 033_bnpl.h). */
static uint64_t present_value_q30(const uint32_t *payments, uint32_t n, uint32_t v) {
    uint64_t acc = 0;
    for (uint32_t k = n; k > 0u; k--) {
        acc = mul_q30(acc + ((uint64_t)payments[k - 1u] << 30), v);
    }
    return acc;
}

int bnpl_compute_apr(uint32_t amount_financed_cents, const uint32_t *payments_cents,
                     uint32_t n, uint32_t periods_per_year,
                     uint32_t *out_apr_hundredths, uint32_t *out_rate_q30) {
    if (n == 0u || n > BNPL_MAX_PAYMENTS || periods_per_year == 0u ||
        amount_financed_cents == 0u || amount_financed_cents >= BNPL_MAX_CENTS) {
        return 0;
    }
    uint32_t total = 0;
    for (uint32_t k = 0; k < n; k++) {
        if (payments_cents[k] >= BNPL_MAX_CENTS) {
            return 0;
        }
        total += payments_cents[k];
    }
    if (total < amount_financed_cents) {
        return 0; /* a negative finance charge has no APR */
    }

    uint32_t rate_q30 = 0;
    if (total > amount_financed_cents) {
        /* PV(v) rises strictly with v, from 0 at v = 0 to the total of
         * payments at v = 1.0, and the target (amount financed) sits
         * strictly between them -- so exactly one v solves Appendix J's
         * equation, and bisection is guaranteed to converge on it.
         * Invariant: PV(lo) < target <= PV(hi). Thirty halvings of a
         * [0, 2^30] interval pin v down to one Q30 unit. */
        uint64_t target = (uint64_t)amount_financed_cents << 30;
        uint32_t lo = 0, hi = BNPL_Q30_ONE;
        while (hi - lo > 1u) {
            uint32_t mid = lo + (hi - lo) / 2u;
            if (present_value_q30(payments_cents, n, mid) < target) {
                lo = mid;
            } else {
                hi = mid;
            }
        }
        /* i = 1/v - 1, in Q30: (2^60 / v) - 2^30. v = hi >= 2^29 for any
         * unit-period rate below 100%, and for a lower v the quotient
         * still fits, since v >= 1. */
        uint64_t inv_v_q30 = udiv64_32(1ull << 60, hi);
        uint64_t r = inv_v_q30 - (1ull << 30);
        if (r > 0xFFFFFFFFull) {
            return 0; /* a unit-period rate this large is out of scope */
        }
        rate_q30 = (uint32_t)r;
    }

    /* Appendix J: APR = unit-period rate * unit-periods per year. In
     * hundredths of a percent that is rate * periods * 10000, and the
     * Q30 scale comes off with a shift, rounded to nearest by adding
     * half of 2^30 first. rate_q30 < 2^32 and periods * 10000 < 2^22
     * for any real unit-period (at most 365 a year), so the product
     * stays below 2^54. */
    uint64_t scaled = (uint64_t)rate_q30 * (uint64_t)(periods_per_year * 10000u);
    uint32_t apr = (uint32_t)((scaled + (1ull << 29)) >> 30);

    if (out_apr_hundredths) {
        *out_apr_hundredths = apr;
    }
    if (out_rate_q30) {
        *out_rate_q30 = rate_q30;
    }
    return 1;
}

/* ---------------------------------------------------------------- */
/* The Pay-in-4 plan                                                */
/* ---------------------------------------------------------------- */

int bnpl_build_pay_in_4(uint32_t cash_price_cents, uint32_t fee_cents,
                        bnpl_date_t checkout_date, bnpl_plan_t *p) {
    if (cash_price_cents < BNPL_INSTALLMENTS || cash_price_cents >= BNPL_MAX_CENTS ||
        fee_cents >= BNPL_MAX_CENTS) {
        return 0;
    }

    p->cash_price_cents = cash_price_cents;
    p->fee_cents = fee_cents;
    p->checkout_date = checkout_date;

    /* This book's own rounding convention (see 033_bnpl.h): equal
     * quarters of the cash price, leftover cents on installment 1; the
     * fee split evenly over installments 2-4, leftover cents on
     * installment 2. */
    uint32_t quarter = cash_price_cents / BNPL_INSTALLMENTS;
    uint32_t price_rem = cash_price_cents % BNPL_INSTALLMENTS;
    uint32_t fee_share = fee_cents / (BNPL_INSTALLMENTS - 1u);
    uint32_t fee_rem = fee_cents % (BNPL_INSTALLMENTS - 1u);

    p->installment_cents[0] = quarter + price_rem;
    for (uint32_t k = 1; k < BNPL_INSTALLMENTS; k++) {
        p->installment_cents[k] = quarter + fee_share + ((k == 1u) ? fee_rem : 0u);
    }
    for (uint32_t k = 0; k < BNPL_INSTALLMENTS; k++) {
        p->due_date[k] = bnpl_add_days(checkout_date, k * BNPL_INTERVAL_DAYS);
    }

    /* 1026.18: installment 1, paid at consummation, is a downpayment. */
    p->downpayment_cents = p->installment_cents[0];
    p->amount_financed_cents = cash_price_cents - p->downpayment_cents;
    p->finance_charge_cents = fee_cents;
    p->total_of_payments_cents = 0;
    for (uint32_t k = 1; k < BNPL_INSTALLMENTS; k++) {
        p->total_of_payments_cents += p->installment_cents[k];
    }

    /* 1026.2(a)(17)'s test: a finance charge, or more than four
     * installments not counting the downpayment (never true here: a
     * Pay-in-4 plan has three after it). */
    p->reg_z_covered = (fee_cents > 0u) || ((BNPL_INSTALLMENTS - 1u) > 4u);

    return bnpl_compute_apr(p->amount_financed_cents, &p->installment_cents[1],
                            BNPL_INSTALLMENTS - 1u, BNPL_UNIT_PERIODS_PER_YEAR,
                            &p->apr_hundredths, 0);
}

/* ---------------------------------------------------------------- */
/* DE 48 payload                                                    */
/* ---------------------------------------------------------------- */

static void put_digits(uint8_t *buf, uint32_t off, uint32_t width, uint32_t value) {
    for (uint32_t i = 0; i < width; i++) {
        buf[off + width - 1u - i] = (uint8_t)('0' + (value % 10u));
        value /= 10u;
    }
}

/* Reads `width` ASCII digits; returns 0 on any non-digit byte, or if the
 * value would not fit a uint32_t. */
static int get_digits(const uint8_t *buf, uint32_t off, uint32_t width, uint32_t *out) {
    uint32_t v = 0;
    for (uint32_t i = 0; i < width; i++) {
        uint8_t c = buf[off + i];
        if (c < (uint8_t)'0' || c > (uint8_t)'9') {
            return 0;
        }
        uint32_t d = (uint32_t)(c - (uint8_t)'0');
        if (v > (0xFFFFFFFFu - d) / 10u) {
            return 0;
        }
        v = v * 10u + d;
    }
    *out = v;
    return 1;
}

uint32_t bnpl_encode_de48(const bnpl_plan_t *p, uint8_t *out, uint32_t out_size) {
    if (out_size < BNPL_DE48_LEN) {
        return 0;
    }
    out[0] = (uint8_t)'P';
    out[1] = (uint8_t)'4';
    put_digits(out, 2, 1, BNPL_INSTALLMENTS);
    put_digits(out, 3, 2, BNPL_INTERVAL_DAYS);
    for (uint32_t k = 0; k < BNPL_INSTALLMENTS; k++) {
        uint32_t off = 5u + k * 20u;
        put_digits(out, off, 4, p->due_date[k].year);
        put_digits(out, off + 4u, 2, p->due_date[k].month);
        put_digits(out, off + 6u, 2, p->due_date[k].day);
        put_digits(out, off + 8u, 12, p->installment_cents[k]);
    }
    put_digits(out, 85, 12, p->amount_financed_cents);
    put_digits(out, 97, 12, p->finance_charge_cents);
    put_digits(out, 109, 12, p->total_of_payments_cents);
    put_digits(out, 121, 5, p->apr_hundredths);
    return BNPL_DE48_LEN;
}

int bnpl_decode_de48(const uint8_t *buf, uint32_t len, bnpl_plan_t *p) {
    if (len != BNPL_DE48_LEN || buf[0] != (uint8_t)'P' || buf[1] != (uint8_t)'4') {
        return 0;
    }
    uint32_t count, interval;
    if (!get_digits(buf, 2, 1, &count) || count != BNPL_INSTALLMENTS ||
        !get_digits(buf, 3, 2, &interval) || interval != BNPL_INTERVAL_DAYS) {
        return 0;
    }
    uint8_t *raw = (uint8_t *)p;
    for (uint32_t i = 0; i < sizeof(*p); i++) {
        raw[i] = 0;
    }
    for (uint32_t k = 0; k < BNPL_INSTALLMENTS; k++) {
        uint32_t off = 5u + k * 20u, y, m, d;
        if (!get_digits(buf, off, 4, &y) || !get_digits(buf, off + 4u, 2, &m) ||
            !get_digits(buf, off + 6u, 2, &d) ||
            !get_digits(buf, off + 8u, 12, &p->installment_cents[k])) {
            return 0;
        }
        if (m < 1u || m > 12u || d < 1u || d > 31u) {
            return 0;
        }
        p->due_date[k].year = (uint16_t)y;
        p->due_date[k].month = (uint8_t)m;
        p->due_date[k].day = (uint8_t)d;
    }
    if (!get_digits(buf, 85, 12, &p->amount_financed_cents) ||
        !get_digits(buf, 97, 12, &p->finance_charge_cents) ||
        !get_digits(buf, 109, 12, &p->total_of_payments_cents) ||
        !get_digits(buf, 121, 5, &p->apr_hundredths)) {
        return 0;
    }
    p->downpayment_cents = p->installment_cents[0];
    return 1;
}
