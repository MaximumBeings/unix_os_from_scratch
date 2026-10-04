/* See 049_billing.h's own top-of-file comment for the citation of the
 * proration and dunning schedule below. */

#include "049_billing.h"

uint32_t billing_prorate(uint32_t old_price_cents, uint32_t new_price_cents,
                         uint32_t days_remaining, uint32_t total_days_in_cycle) {
    if (total_days_in_cycle == 0u || days_remaining > total_days_in_cycle) {
        return 0;
    }
    uint32_t diff = (new_price_cents > old_price_cents) ? (new_price_cents - old_price_cents)
                                                         : (old_price_cents - new_price_cents);
    /* Integer-only, no floating point (this book's own established
     * limit since Chapter 30): scale first, then divide, rounding
     * down to the nearest cent -- diff and days_remaining are both
     * small enough (real cent amounts, real day counts) that this
     * never approaches a uint32_t overflow. */
    return (diff * days_remaining) / total_days_in_cycle;
}

const uint32_t g_billing_dunning_days[BILLING_MAX_RETRIES] = {1u, 3u, 5u, 7u};

int billing_retry_due(uint32_t today_day, uint32_t failed_since_day, uint32_t retry_count) {
    if (retry_count >= BILLING_MAX_RETRIES) {
        return 0;
    }
    uint32_t next_retry_day = failed_since_day + g_billing_dunning_days[retry_count];
    return today_day >= next_retry_day;
}
