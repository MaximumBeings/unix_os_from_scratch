/* See 051_aggregator.h's own top-of-file comment on why neither
 * ranking rule below is a real, universally standardized algorithm. */

#include "051_aggregator.h"

int aggregator_pick_best(const ota_search_response_t *resp, aggregator_rank_rule_t rule) {
    if (resp->count == 0u) {
        return -1;
    }
    uint32_t best = 0;
    for (uint32_t i = 1; i < resp->count; i++) {
        const ota_priced_itinerary_t *cand = &resp->itineraries[i];
        const ota_priced_itinerary_t *cur = &resp->itineraries[best];
        int cand_better;
        if (rule == AGGREGATOR_RANK_FEWEST_STOPS_THEN_CHEAPEST &&
            cand->stop_quantity != cur->stop_quantity) {
            cand_better = cand->stop_quantity < cur->stop_quantity;
        } else {
            cand_better = cand->total_fare_cents < cur->total_fare_cents;
        }
        if (cand_better) {
            best = i;
        }
    }
    return (int) best;
}
