#ifndef UNIX_OS_055_AGGREGATOR_H
#define UNIX_OS_055_AGGREGATOR_H

#include <stdint.h>
#include "055_ota.h"

/* This chapter's own aggregation/ranking logic -- unlike 055_ota.h's
 * own real OTA citation, neither ranking rule below is a single,
 * universally standardized algorithm any one spec defines. Both are
 * real, general, commonly-described approaches a real flight
 * aggregator runs (rank by price; rank by stops first, price as a
 * tie-breaker), not any one real aggregator's own exact proprietary
 * ranking (which typically also weighs airline preference, loyalty
 * programs, and other signals this chapter does not model), stated
 * plainly as this book's own simplified version of a real general
 * idea, the same honesty note 040_billing.h gave its own proration/
 * dunning citations. */

typedef enum {
    AGGREGATOR_RANK_CHEAPEST = 0,
    AGGREGATOR_RANK_FEWEST_STOPS_THEN_CHEAPEST = 1,
} aggregator_rank_rule_t;

/* Returns the index into `resp->itineraries` of the best offer under
 * `rule`, or -1 if `resp->count == 0` (nothing to rank). Ties are
 * broken by the lowest index (the earliest-received offer), so this
 * function's own result is deterministic given the same input. */
int aggregator_pick_best(const ota_search_response_t *resp, aggregator_rank_rule_t rule);

#endif
