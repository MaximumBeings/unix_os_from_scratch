/* See 051_rental.h's own top-of-file comment: this file's own ranking
 * rules are this book's own invented design, not any real
 * aggregator's. */

#include "051_rental.h"

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

uint32_t rental_class_rank(const uint8_t *vehicle_class, uint32_t class_len) {
    if (lit_eq(vehicle_class, class_len, "FULLSIZE")) {
        return 0u;
    }
    if (lit_eq(vehicle_class, class_len, "STANDARD")) {
        return 1u;
    }
    if (lit_eq(vehicle_class, class_len, "COMPACT")) {
        return 2u;
    }
    if (lit_eq(vehicle_class, class_len, "ECONOMY")) {
        return 3u;
    }
    return 99u; /* any class this book's own lookup doesn't recognize */
}

static uint32_t cstr_bytes_len(const uint8_t *s, uint32_t max) {
    uint32_t n = 0;
    while (n < max && s[n] != 0) {
        n++;
    }
    return n;
}

int rental_pick_best(const veh_avail_response_t *resp, rental_rank_rule_t rule) {
    if (resp->count == 0u) {
        return -1;
    }
    uint32_t best = 0;
    for (uint32_t i = 1; i < resp->count; i++) {
        const veh_offer_t *cand = &resp->offers[i];
        const veh_offer_t *cur = &resp->offers[best];
        int cand_better;
        if (rule == RENTAL_RANK_BEST_CLASS_THEN_CHEAPEST) {
            uint32_t cand_rank = rental_class_rank(cand->vehicle_class,
                                                    cstr_bytes_len(cand->vehicle_class,
                                                                   VEH_CLASS_MAX - 1u));
            uint32_t cur_rank = rental_class_rank(cur->vehicle_class,
                                                   cstr_bytes_len(cur->vehicle_class,
                                                                  VEH_CLASS_MAX - 1u));
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
