/* Chapter 47: the auction engine. A real eBay-style PROXY-BIDDING auction, in this book's own integer-only arithmetic.
 *
 * What "proxy bidding" means, from eBay's own Buy Offer API documentation (read in full this chapter, in the generated OpenAPI types of the open-source
 * github.com/hendt/ebay-api, commit e20388b, file src/types/restful/specs/buy_offer_v1_beta_oas3.ts -- eBay's own developer site, developer.ebay.com, is blocked by
 * this sandbox's network policy): "By placing a proxy bid, the buyer is agreeing to purchase the item if they win the auction. After this bid is placed, if someone
 * else outbids the buyer a bid, eBay automatically bids again for the buyer up to the amount of their maximum bid. When the bid exceeds the buyer's maximum bid, eBay
 * will notify them that they have been outbid." And on the reserve price: "A reserve price is set by the seller and is the minimum amount the seller is willing to
 * sell the item for. If the highest bid is not equal to or higher than the reserve price when the auction ends, the listing ends and the item is not sold."
 *
 * How this file turns those sentences into rules (every one is a line in auc_recompute() below and is cross-checked against an INDEPENDENT step-by-step Python
 * simulation of "eBay bids again for the buyer" -- see the chapter page):
 *   - Every bidder has one standing maximum. The leader is the bidder with the highest maximum; if two maxima are EQUAL, the bidder who reached that maximum EARLIER
 *     leads (the `seq` field).
 *   - The current price is the least the leader must pay to stay ahead: one bid increment above the runner-up's maximum, but never more than the leader's own maximum.
 *     With a single bidder the price is the starting price. If the leader's maximum reaches the reserve, the price is at least the reserve.
 *   - The next bid must be at least the current price plus the increment (or the starting price when there are no bids). A leader may only RAISE their own maximum.
 *   - The bid increment depends on the price (auc_increment() below). THE TABLE IS DATA, not logic: eBay's own published US increment table is not in the OpenAPI
 *     files this chapter could read, so it is reproduced from eBay's public help pages as the author knows them and could NOT be re-verified in this sandbox; it is
 *     one array to correct if eBay's differs.
 *   - eBay auctions close HARD at the end time (a well-known property: no automatic extension). This engine matches that by default; an OPTIONAL anti-sniping
 *     extension (extend the end time when a bid arrives inside a final window) exists for marketplaces that want it, and is tested both ways.
 *
 * Industrial-grade rules this file follows: money is unsigned 32-bit CENTS everywhere (no floating point, and no 64-bit division, which this freestanding kernel
 * has never linked libgcc for); the engine never prints and never reads a clock -- the caller passes `now` in whole seconds, so the same inputs always give the
 * same outputs (which is what makes write-ahead-log replay possible); and it never copies a structure with `=` or calls memset/memcpy (this kernel has no libc, and
 * the compiler may emit such calls for structure copies). */
#ifndef AUCTION_H
#define AUCTION_H
#include <stdint.h>

#define AUC_MAX_LISTINGS 8
#define AUC_MAX_BIDDERS 8
#define AUC_TITLE_MAX 48

enum { AUC_ACTIVE = 0, AUC_SOLD = 1, AUC_RESERVE_NOT_MET = 2, AUC_NO_BIDS = 3 };
enum { AUC_OK = 0, AUC_ERR_NO_LISTING = 1, AUC_ERR_ENDED = 2, AUC_ERR_SELLER = 3, AUC_ERR_TOO_LOW = 4, AUC_ERR_LOWER_OWN = 5, AUC_ERR_FULL = 6,
       AUC_ERR_NOT_ENDED = 7, AUC_ERR_EXISTS = 8, AUC_ERR_BAD_ARG = 9 };

typedef struct { uint32_t bidder, max_cents, seq; } auc_proxy_t;

typedef struct {
    uint32_t used, id, seller;
    char title[AUC_TITLE_MAX];
    uint32_t start_cents, reserve_cents; /* reserve 0 = no reserve */
    uint32_t start_time, end_time;       /* whole seconds on the caller's clock */
    uint32_t extend_window, extend_secs; /* both 0 = eBay-style hard close */
    uint32_t status;
    uint32_t price_cents, high_bidder, high_max;
    uint32_t bid_count, bid_seq;
    uint32_t nproxies;
    auc_proxy_t proxies[AUC_MAX_BIDDERS];
} auc_listing_t;

typedef struct { auc_listing_t listings[AUC_MAX_LISTINGS]; } auc_t;

uint32_t auc_increment(uint32_t price_cents);
void auc_init(auc_t *a);
auc_listing_t *auc_find(auc_t *a, uint32_t id);
int auc_create(auc_t *a, uint32_t id, uint32_t seller, const char *title, uint32_t start_cents, uint32_t reserve_cents, uint32_t now, uint32_t duration_secs,
               uint32_t extend_window, uint32_t extend_secs);
/* Place or raise a proxy bid. On AUC_OK: *out_price is the new current price and *out_is_high says whether `bidder` now leads. */
int auc_bid(auc_t *a, uint32_t id, uint32_t bidder, uint32_t max_cents, uint32_t now, uint32_t *out_price, int *out_is_high);
int auc_close(auc_t *a, uint32_t id, uint32_t now);
uint32_t auc_min_bid(const auc_listing_t *l);
int auc_reserve_met(const auc_listing_t *l);
uint32_t auc_proxy_of(const auc_listing_t *l, uint32_t bidder); /* 0 if none */
#endif
