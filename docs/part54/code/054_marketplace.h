#ifndef UNIX_OS_054_MARKETPLACE_H
#define UNIX_OS_054_MARKETPLACE_H

#include <stdint.h>
#include "054_gs1.h"

/* This chapter's own marketplace listing message and ranking logic --
 * unlike 054_gs1.h's own real GS1 GDTI citation (carried inside each
 * listing below), neither the listing's own wire shape nor its own
 * seat-quality scoring is a real external format or a real published
 * algorithm. Both are this book's own invented design, stated plainly,
 * the same honesty note 038_atm.h gave its own invented account store:
 * a real ticket resale marketplace's own actual seat-ranking logic is
 * proprietary, and typically weighs many more real signals (view
 * obstruction, aisle proximity, a specific venue's own real layout)
 * than this chapter's own simplified section-then-row score. */

#define MARKETPLACE_MAX_LISTINGS 4u
#define MARKETPLACE_NAME_MAX 16u
#define MARKETPLACE_SECTION_MAX 8u
#define MARKETPLACE_LISTING_WIRE_MAX 128u

typedef struct {
    uint8_t marketplace_name[MARKETPLACE_NAME_MAX];
    uint32_t marketplace_name_len;
    uint8_t section[MARKETPLACE_SECTION_MAX];
    uint32_t section_len;
    uint32_t section_rank; /* 0 = best; this book's own invented scale --
                            * NOT part of the wire format (see
                            * marketplace_section_rank()'s own comment
                            * below on why): filled in by
                            * marketplace_parse_listing() itself from
                            * `section`, the aggregator's own scoring,
                            * never something a marketplace transmits. */
    uint32_t row;
    uint32_t price_cents;
    gs1_ticket_id_t ticket; /* the real GS1 GDTI this specific seat carries */
} marketplace_listing_t;

/* This chapter's own invented section-quality lookup -- a real
 * aggregator's own actual scoring is proprietary and would draw on a
 * specific venue's own real seating chart (view obstruction, distance
 * to the action, aisle proximity); this book's own simplified stand-in
 * maps section "A" to the best rank (0), "B" to 1, "C" to 2, and any
 * other section name to a deliberately worse rank (99) so an unknown
 * section never silently wins a best-seat comparison. This is the
 * AGGREGATOR's own judgment about a venue it already knows, never
 * something a marketplace's own listing claims about itself -- the
 * same real distinction a real ticket aggregator draws between a
 * seller's own stated price (trusted) and a seller's own claimed seat
 * quality (never trusted at face value). */
uint32_t marketplace_section_rank(const uint8_t *section, uint32_t section_len);

typedef struct {
    marketplace_listing_t listings[MARKETPLACE_MAX_LISTINGS];
    uint32_t count;
} marketplace_search_result_t;

typedef enum {
    MARKETPLACE_RANK_CHEAPEST = 0,
    MARKETPLACE_RANK_BEST_SEAT_THEN_CHEAPEST = 1,
} marketplace_rank_rule_t;

/* This chapter's own invented, simple wire format for one listing:
 * "<marketplace>|<section>|<row>|<price_cents>|<real GS1 element
 * string>\n" -- newline-terminated, '|'-delimited, this book's own
 * choice (not a real marketplace API's own wire format). Builds one
 * listing into `out`. Returns the encoded length, or 0 on refusal
 * (any field too long, or the real GS1 element string itself refused
 * by gs1_build_element_string()). */
uint32_t marketplace_build_listing(const marketplace_listing_t *listing, uint8_t *out,
                                    uint32_t out_size);

/* Parses exactly `len` bytes of one such listing back into `*out`.
 * Returns 1 on success, or 0 -- refusing outright -- on a malformed
 * field, an out-of-range numeric field, or an invalid real GS1 element
 * string (gs1_parse_element_string()). */
int marketplace_parse_listing(const uint8_t *buf, uint32_t len, marketplace_listing_t *out);

/* Returns the index into `result->listings` of the best offer under
 * `rule` (mirroring 041_aggregator.h's own established shape), or -1
 * if `result->count == 0`. Ties are broken by the lowest index. */
int marketplace_pick_best(const marketplace_search_result_t *result, marketplace_rank_rule_t rule);

#endif
