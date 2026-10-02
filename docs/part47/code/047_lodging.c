/* See 047_lodging.h's own top-of-file comment: this file's own
 * ranking rules are this book's own invented design, not any real
 * aggregator's. */

#include "047_lodging.h"

static int lit_eq(const uint8_t *s, uint32_t len, const char *lit) {
    uint32_t i = 0;
    while (lit[i] != '\0') {
        if (i >= len || s[i] != (uint8_t) lit[i]) {
            return 0;
        }
        i++;
    }
    return i == len;
}

uint32_t lodging_bundle_rank(const uint8_t *option_type, uint32_t option_type_len) {
    if (lit_eq(option_type, option_type_len, "HotelActivity")) {
        return 0u;
    }
    if (lit_eq(option_type, option_type_len, "Hotel")) {
        return 1u;
    }
    return 99u; /* any option type this book's own lookup doesn't recognize */
}

static uint32_t cstr_bytes_len(const uint8_t *s, uint32_t max) {
    uint32_t n = 0;
    while (n < max && s[n] != 0) {
        n++;
    }
    return n;
}

int lodging_pick_best(const hotel_avail_response_t *resp, lodging_rank_rule_t rule) {
    if (resp->count == 0u) {
        return -1;
    }
    uint32_t best = 0;
    for (uint32_t i = 1; i < resp->count; i++) {
        const hotel_offer_t *cand = &resp->offers[i];
        const hotel_offer_t *cur = &resp->offers[best];
        int cand_better;
        if (rule == LODGING_RANK_BUNDLE_THEN_CHEAPEST) {
            uint32_t cand_rank = lodging_bundle_rank(cand->option_type,
                                                      cstr_bytes_len(cand->option_type,
                                                                     HOTEL_OPTYPE_MAX - 1u));
            uint32_t cur_rank = lodging_bundle_rank(cur->option_type,
                                                     cstr_bytes_len(cur->option_type,
                                                                    HOTEL_OPTYPE_MAX - 1u));
            if (cand_rank != cur_rank) {
                cand_better = cand_rank < cur_rank;
            } else {
                cand_better = cand->rate_total_cents < cur->rate_total_cents;
            }
        } else {
            cand_better = cand->rate_total_cents < cur->rate_total_cents;
        }
        if (cand_better) {
            best = i;
        }
    }
    return (int) best;
}
