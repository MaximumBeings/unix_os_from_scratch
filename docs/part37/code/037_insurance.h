#ifndef UNIX_OS_037_INSURANCE_H
#define UNIX_OS_037_INSURANCE_H

#include <stdint.h>

/* A real personal-auto insurance rating model -- premium = base rate x
 * a chain of multiplicative rating factors -- computed for several
 * fictional carriers from one applicant profile, then ranked, all in
 * integer cents and integer fixed-point arithmetic (parts per 10000),
 * with no floating point and no native 64-bit division or modulo
 * anywhere in this kernel (the same __udivdi3/__umoddi3 link failure
 * Chapters 7, 30, and 33 each document and avoid).
 *
 * The multiplicative rating MODEL -- one base rate, adjusted by a
 * chain of independently-justified factors, is real and general: the
 * NAIC's own real ratemaking material states that "[c]lassification
 * rates may be modified to produce rates for individual risks in
 * accordance with rating plans which establish standards for measuring
 * variations in hazards ... that can be demonstrated to have a
 * probable effect upon losses or expenses" (content.naic.org, via web
 * search -- this sandbox's network egress policy blocks naic.org
 * directly, the same honesty note Chapter 33 made about
 * consumerfinance.gov/ecfr.gov/govinfo.gov/law.cornell.edu; stated here
 * rather than left implicit), and its own workers'-compensation
 * ratemaking material states plainly that "premiums are calculated as
 * a base rate multiplied by payroll" -- the same base-rate-times-factor
 * shape this module uses for personal auto. The split-limit liability
 * convention ("100/300/100": $100,000 bodily injury per person /
 * $300,000 per accident / $100,000 property damage) is likewise real
 * and standard across the US auto insurance industry, again cited only
 * through search results under this sandbox's network policy.
 *
 * What is NOT real, and is this book's own invention, stated plainly:
 * every specific rating factor value below (which age bands get which
 * multiplier, how many basis points a territory or an at-fault
 * accident costs), every fictional carrier's own base rate and factor
 * table, and the specific set of five rating factors chosen. Real
 * insurers each file their own actual rating plans with state
 * regulators, and none of those real filings is public in a form this
 * book could cite field-for-field the way Chapters 30/32/33 cited
 * NACHA, Fedwire, and ISO 8583. This chapter's own honesty precedent
 * (Chapter 29's LRU eviction, Chapter 32's "group splitting" scenario)
 * applies here at the level of the whole rating table, not just one
 * field: the SHAPE is real, the NUMBERS are invented.
 *
 * Every rating factor is expressed in basis points, "parts per 10000"
 * (10000 = 1.00x, 15000 = 1.50x, 8500 = 0.85x), and every
 * multiplication is followed immediately by a rounded division back
 * down to cents, using this chapter's own restoring shift-and-subtract
 * `udiv64_32()` (see 037_insurance.c) rather than the `/` or `%`
 * operator on a uint64_t, which this freestanding kernel cannot link
 * (no libgcc). Every intermediate value is bounded: a base rate below
 * INS_MAX_BASE_RATE_CENTS times a factor below INS_MAX_FACTOR stays
 * below 2^40, comfortably inside a uint64_t before the division. */

#define INS_MAX_CARRIERS 4u
#define INS_NUM_FACTORS 5u          /* age, territory, accidents, vehicle, deductible */
#define INS_MAX_BASE_RATE_CENTS 0x00FFFFFFu /* stated limit, refused not truncated */
#define INS_MAX_FACTOR 0x0000FFFFu          /* 655.35x, stated limit */
#define INS_BASIS_POINTS 10000u

typedef struct {
    uint32_t driver_age;             /* years */
    uint32_t years_licensed;
    uint32_t at_fault_accidents_3yr;  /* count, last 3 years */
    uint32_t territory_tier;          /* 1 (lowest risk) .. 3 (highest), this book's own invented tiering */
    uint32_t vehicle_value_cents;
    uint32_t vehicle_age_years;
    uint32_t bi_per_person_cents;     /* split-limit liability: bodily injury, per person */
    uint32_t bi_per_accident_cents;   /* bodily injury, per accident */
    uint32_t pd_cents;                /* property damage */
    uint32_t collision_deductible_cents;
} ins_applicant_t;

typedef struct {
    char name[24];
    uint32_t base_rate_cents;
    /* Rating factors, in basis points, applied in this fixed order:
     * [0] age band, [1] territory tier, [2] at-fault accident count,
     * [3] vehicle value/age band, [4] collision deductible band. */
    uint32_t factor_bp[INS_NUM_FACTORS];
} ins_carrier_t;

typedef struct {
    char carrier_name[24];
    uint32_t premium_cents;   /* annual premium, after every factor */
    uint32_t factor_bp[INS_NUM_FACTORS]; /* the factors this applicant actually drew */
} ins_quote_t;

/* Looks up this book's own invented rating factors for `a` against
 * `carrier`'s own base rate and factor table, in the fixed order
 * documented above, and returns the resulting premium in
 * *out_quote. Returns 1 on success, or 0 (refusing outright) if the
 * carrier's base rate is at or above INS_MAX_BASE_RATE_CENTS, if any
 * looked-up factor is at or above INS_MAX_FACTOR, or if any
 * intermediate premium would reach INS_MAX_BASE_RATE_CENTS (guarding
 * against a chain of factors compounding past this module's own
 * stated bound). */
int ins_quote_one_carrier(const ins_applicant_t *a, const ins_carrier_t *carrier,
                          ins_quote_t *out_quote);

/* Quotes `a` against every one of `n` carriers (n <= INS_MAX_CARRIERS),
 * writes each result into out_quotes[0..n-1] in the SAME order as
 * `carriers`, then insertion-sorts out_quotes into ascending premium
 * order (cheapest first) -- the "aggregation" and "ranking" this
 * chapter's own confirmed scope asked for. Returns the number of
 * carriers successfully quoted (a carrier this book's own rating
 * table refuses is simply omitted, not fatal to the others), or writes
 * nothing and returns 0 if n is 0 or above INS_MAX_CARRIERS. */
uint32_t ins_rank_quotes(const ins_applicant_t *a, const ins_carrier_t *carriers, uint32_t n,
                         ins_quote_t *out_quotes);

#endif
