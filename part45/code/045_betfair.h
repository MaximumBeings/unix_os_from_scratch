#ifndef UNIX_OS_045_BETFAIR_H
#define UNIX_OS_045_BETFAIR_H

#include <stdint.h>

/* A real Betfair Exchange API message pair -- `listMarketBook` (odds
 * lookup) and `placeOrders` (bet placement) -- the real betting
 * exchange whose own API this chapter's own confirmed scope names.
 *
 * Betfair's own official developer documentation
 * (developer.betfair.com) is blocked by this sandbox's network egress
 * policy, the same pattern every blocked-standards-site chapter since
 * Chapter 33 has hit. But Betfair's own official GitHub organization
 * is not: this chapter cloned github.com/betfair/API-NG-sample-code --
 * Betfair's own real sample code repository -- and read
 * `python/ApiNgDemoJsonRpc-python3.py` in full. Every real field name
 * below is copied directly out of that real file's own real,
 * hand-built JSON-RPC request/response strings, not a search summary:
 *
 *   listMarketBook request:
 *     {"jsonrpc": "2.0", "method": "SportsAPING/v1.0/listMarketBook",
 *      "params": {"marketIds": [...],
 *                 "priceProjection": {"priceData": ["EX_BEST_OFFERS"]}},
 *      "id": 1}
 *   listMarketBook response (per runner):
 *     runner.selectionId, runner.status ("ACTIVE"),
 *     runner.ex.availableToBack / .availableToLay, each a real list of
 *     {"price": ..., "size": ...} (Betfair's own real PriceSize pair --
 *     "price" is always real DECIMAL odds, Betfair's own one and only
 *     real internal convention, cited directly from this same file's
 *     own `"price":"1.50"` literal in its own placeOrders example)
 *
 *   placeOrders request:
 *     {"jsonrpc": "2.0", "method": "SportsAPING/v1.0/placeOrders",
 *      "params": {"marketId": "...",
 *                 "instructions": [{"selectionId": "...",
 *                                   "handicap": "0", "side": "BACK",
 *                                   "orderType": "LIMIT",
 *                                   "limitOrder": {"size": "...",
 *                                                  "price": "...",
 *                                                  "persistenceType": "LAPSE"}}],
 *                 "customerRef": "..."}, "id": 1}
 *   placeOrders response:
 *     result.status, result.instructionReports[0].betId
 *
 * This chapter's own stated scope limit, the same "known, restricted
 * schema" approach Chapters 34/40/41 used: this is real Betfair-
 * SHAPED JSON (the same real field names, nesting, and string-typed
 * numeric values Betfair's own API actually sends), built and parsed
 * by this chapter's own fixed, restricted-subset codec -- not a
 * general JSON parser. Real Betfair prices and sizes are sent as JSON
 * STRINGS, not numbers (e.g. `"price":"1.50"`), exactly as the cited
 * sample file's own literal text shows; this codec follows that same
 * real convention, encoding this book's own internal decimal-cents
 * integers as that same real string shape. Only one runner's own real
 * best-offer price (not Betfair's own real full depth-of-book ladder)
 * is modeled, this chapter's own stated simplification. */

#define BETFAIR_MAX_RUNNERS 4u
#define BETFAIR_WIRE_MAX 1024u

typedef struct {
    uint32_t selection_id;
    uint32_t back_price_cents; /* real decimal odds, hundredths */
    uint32_t back_size_cents;  /* real stake size available, cents */
} betfair_runner_t;

typedef struct {
    uint8_t market_id[16];
    uint32_t market_id_len;
    betfair_runner_t runners[BETFAIR_MAX_RUNNERS];
    uint32_t runner_count;
} betfair_market_book_t;

/* Builds a real Betfair-shaped `listMarketBook` RESPONSE (this
 * chapter's own simplified subset: one best back price per runner)
 * into `out`. Returns the encoded length, or 0 if `out_size` is too
 * small or `book->runner_count` exceeds BETFAIR_MAX_RUNNERS. */
uint32_t betfair_build_market_book(const betfair_market_book_t *book, uint8_t *out,
                                    uint32_t out_size);

/* Parses exactly `len` bytes of that same real shape back into
 * `*out`. Returns 1 on success, or 0 -- refusing outright -- on
 * anything outside this chapter's own restricted subset. */
int betfair_parse_market_book(const uint8_t *buf, uint32_t len, betfair_market_book_t *out);

typedef struct {
    uint8_t market_id[16];
    uint32_t market_id_len;
    uint32_t selection_id;
    uint32_t price_cents; /* real decimal odds, hundredths */
    uint32_t size_cents;  /* real stake size, cents */
} betfair_place_order_t;

/* Builds a real Betfair-shaped `placeOrders` REQUEST (one real BACK/
 * LIMIT/LAPSE instruction, this chapter's own stated simplification --
 * real Betfair supports LAY orders and other order/persistence types
 * this chapter never needs) into `out`. Returns the encoded length,
 * or 0 on refusal. */
uint32_t betfair_build_place_order(const betfair_place_order_t *order, uint8_t *out,
                                    uint32_t out_size);

/* Parses that same real request shape back into `*out`. Returns 1 on
 * success, 0 -- refusing outright -- otherwise. */
int betfair_parse_place_order(const uint8_t *buf, uint32_t len, betfair_place_order_t *out);

/* Builds a real Betfair-shaped `placeOrders` RESPONSE carrying one
 * real instruction report's own status and bet ID. `status` must be
 * "SUCCESS" or "FAILURE" (Betfair's own real values, cited the weaker
 * way through search results, since this file's own cloned sample
 * code only prints `result['status']` without quoting its own real
 * value). Returns the encoded length, or 0 on refusal. */
uint32_t betfair_build_place_order_response(const char *status, uint32_t bet_id, uint8_t *out,
                                             uint32_t out_size);

/* Parses that same real response shape. `out_status` receives 1 for a
 * real "SUCCESS" and 0 for a real "FAILURE". Returns 1 on success
 * (either real status), or 0 -- refusing outright -- on anything
 * else. */
int betfair_parse_place_order_response(const uint8_t *buf, uint32_t len, int *out_status,
                                       uint32_t *out_bet_id);

#endif
