#ifndef UNIX_OS_047_LODGING_H
#define UNIX_OS_047_LODGING_H

#include <stdint.h>
#include "047_hotel.h"

/* This chapter's own ranking logic over several real hotel `AvailRS`
 * offers (047_hotel.h) -- unlike that real citation, neither this
 * file's own bundle-preference scoring nor its own ranking rules are
 * real: both are this book's own invented design, the same honesty
 * note Chapter 44's own 047_rental.h gave. A real OTA/hotel
 * aggregator's own actual ranking weighs many more real signals (star
 * rating, cancellation flexibility, guest reviews, loyalty program
 * membership) than this chapter's own simplified bundle-type score. */

#define LODGING_MAX_OFFERS HOTEL_MAX_OFFERS

/* This chapter's own invented option-type preference -- maps the real
 * `HotelActivity` option type (lodging bundled with a real tourist
 * attraction/activity) to the best rank (0), and plain `Hotel` to a
 * deliberately worse rank (1). This is the AGGREGATOR's own judgment
 * about which option type travelers prefer, never something a
 * vendor's own offer claims about itself -- the same real distinction
 * 047_rental.h's own `rental_class_rank()` drew. */
uint32_t lodging_bundle_rank(const uint8_t *option_type, uint32_t option_type_len);

typedef enum {
    LODGING_RANK_CHEAPEST = 0,
    LODGING_RANK_BUNDLE_THEN_CHEAPEST = 1,
} lodging_rank_rule_t;

/* Returns the index into `resp->offers` of the best offer under
 * `rule` (mirroring 047_rental.h's own established shape), or -1 if
 * `resp->count == 0`. Ties are broken by the lowest index. */
int lodging_pick_best(const hotel_avail_response_t *resp, lodging_rank_rule_t rule);

#endif
