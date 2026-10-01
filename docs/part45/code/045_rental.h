#ifndef UNIX_OS_045_RENTAL_H
#define UNIX_OS_045_RENTAL_H

#include <stdint.h>
#include "045_veh.h"

/* This chapter's own ranking logic over several real OTA
 * `VehAvailRateRS` vendor offers (045_veh.h) -- unlike that real
 * citation, neither this file's own vehicle-class scoring nor its own
 * ranking rules are real: both are this book's own invented design,
 * the same honesty note Chapter 38's own account store and Chapter
 * 42's own marketplace ranking gave. A real rental aggregator's own
 * actual class-quality scoring is proprietary and typically weighs
 * many more real signals (specific make/model, mileage policy, a
 * renter's own stated preferences) than this chapter's own simplified
 * class-name score. */

#define RENTAL_MAX_OFFERS VEH_MAX_OFFERS

/* This chapter's own invented vehicle-class quality lookup -- a real
 * aggregator's own actual scoring is proprietary; this book's own
 * simplified stand-in maps "FULLSIZE" to the best rank (0), "STANDARD"
 * to 1, "COMPACT" to 2, "ECONOMY" to 3, and any other class name to a
 * deliberately worse rank (99) so an unknown class never silently wins
 * a best-class comparison. This is the AGGREGATOR's own judgment about
 * vehicle classes, never something a vendor's own offer claims about
 * itself -- the same real distinction 042_marketplace.h's own
 * `marketplace_section_rank()` drew between a seller's own stated
 * price (trusted) and a seller's own claimed seat/class quality
 * (never trusted at face value). */
uint32_t rental_class_rank(const uint8_t *vehicle_class, uint32_t class_len);

typedef enum {
    RENTAL_RANK_CHEAPEST = 0,
    RENTAL_RANK_BEST_CLASS_THEN_CHEAPEST = 1,
} rental_rank_rule_t;

/* Returns the index into `resp->offers` of the best offer under
 * `rule` (mirroring 041_aggregator.h's own established shape), or -1
 * if `resp->count == 0`. Ties are broken by the lowest index. */
int rental_pick_best(const veh_avail_response_t *resp, rental_rank_rule_t rule);

#endif
