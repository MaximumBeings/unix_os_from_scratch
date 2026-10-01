#ifndef UNIX_OS_046_BILLING_H
#define UNIX_OS_046_BILLING_H

#include <stdint.h>

/* This chapter's own subscription billing engine -- unlike 046_ubl.h's
 * own real OASIS-cited document format, neither the proration formula
 * nor the dunning retry schedule below is a single, universally
 * standardized algorithm any one real spec defines exactly. Both are
 * real, general, commonly-used approaches, cited through web search
 * results (the same weaker honesty note this book has used since DE
 * 39's "00"), not any one company's own exact proprietary
 * implementation:
 *
 *   PRORATION: charge the price difference for a mid-cycle plan
 *   change, scaled by the fraction of the billing cycle remaining --
 *   `price_difference * days_remaining / total_days_in_cycle`, a real,
 *   widely-described general approach (used, in shape, by essentially
 *   every real subscription billing system this chapter's own search
 *   results described), not this chapter's own invention.
 *
 *   DUNNING (retrying a failed recurring charge before giving up):
 *   this chapter's own specific schedule -- retry 1, 3, 5, and 7 days
 *   after the first failure -- is cited directly from Recurly's own
 *   published 2025 analysis (a real, named company's own real
 *   documented practice, found through search results), not this
 *   book's own invention, though a real deployment's own exact
 *   schedule varies by provider (some cite 3/7/14 days instead; this
 *   chapter states plainly it picked one real, cited schedule, not the
 *   only one that exists). */

#define BILLING_MAX_RETRIES 4u

/* Real, general proration: the magnitude of the prorated charge (or
 * credit) for changing from `old_price_cents` to `new_price_cents`
 * with `days_remaining` left in a `total_days_in_cycle`-day cycle.
 * Returns 0 if `days_remaining > total_days_in_cycle` or
 * `total_days_in_cycle == 0` (refused outright, not silently clamped).
 * The caller decides the sign: a plan upgrade (new > old) is a charge,
 * a downgrade (new < old) is a credit. */
uint32_t billing_prorate(uint32_t old_price_cents, uint32_t new_price_cents,
                         uint32_t days_remaining, uint32_t total_days_in_cycle);

/* Recurly's own cited retry schedule, in days after the first failure. */
extern const uint32_t g_billing_dunning_days[BILLING_MAX_RETRIES];

/* Returns 1 if a subscription whose most recent charge first failed on
 * day `failed_since_day`, having already attempted `retry_count` (0 to
 * BILLING_MAX_RETRIES) real dunning retries, is due for its next retry
 * on or after `today_day` -- and 0 if not yet due, or if
 * `retry_count >= BILLING_MAX_RETRIES` (the schedule is exhausted; the
 * caller is expected to suspend the subscription instead of retrying
 * again). */
int billing_retry_due(uint32_t today_day, uint32_t failed_since_day, uint32_t retry_count);

#endif
