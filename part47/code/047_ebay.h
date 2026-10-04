/* Chapter 47: eBay-shaped REST messages -- the JSON bodies and HTTP-style request/response framing the marketplace speaks.
 *
 * WHICH SHAPES ARE REAL. eBay's own developer site (developer.ebay.com) is blocked by this sandbox's network policy. This chapter read eBay's real OpenAPI schemas as the generated
 * TypeScript types in the open-source (MIT) github.com/hendt/ebay-api, commit e20388b (2026-08-11), under src/types/restful/specs/:
 *   buy_offer_v1_beta_oas3.ts   -- POST /bidding/{item_id}/place_proxy_bid: request PlaceProxyBidRequest { maxAmount: Amount, userConsent }, response PlaceProxyBidResponse { proxyBidId };
 *                                  GET /bidding/{item_id}: Bidding { auctionEndDate, auctionStatus (ACTIVE | ENDED), bidCount, currentPrice: Amount, currentProxyBid: ProxyBid { maxAmount, proxyBidId },
 *                                  highBidder (boolean), itemId, reservePriceMet, suggestedBidAmounts: Amount[] }; Amount { currency, value } where `value` is a STRING of decimal digits.
 *   buy_browse_v1_oas3.ts       -- Item (getItem): bidCount, buyingOptions[] (AUCTION | FIXED_PRICE ...), currentBidPrice, itemEndDate, itemId, minimumPriceToBid, reservePriceMet, uniqueBidderCount.
 *   sell_inventory_v1_oas3.ts   -- Offer: sku, marketplaceId, format (AUCTION), listingDuration, pricingSummary { auctionStartPrice, auctionReservePrice (both Amount) }.
 *   sell_fulfillment_v1_oas3.ts -- Order: orderId, orderPaymentStatus, lineItems[], pricingSummary { total: Amount }, paymentSummary { totalDueSeller: Amount, payments[] { paymentStatus, amount } }.
 * Every field NAME below was read from those files. What is NOT from them and is this chapter's own, said honestly: (a) the HTTP framing around the bodies (request line, `Authorization`,
 * `Idempotency-Key` headers) -- eBay uses OAuth user tokens, which this book does not implement, so the bidder identity here is a fictional bearer token `demo-user-<n>`; (b) the
 * `{"errors":[ ... ]}` wrapper around eBay's `Error` object (the Error FIELDS errorId, domain, category, message, longMessage are in the OpenAPI files; the wrapper is eBay's documented
 * convention and was not in the files read); (c) the combined listing body below, which is the Sell Inventory offer fields plus `product.title` from the inventory-item shape;
 * (d) the item identifier `v1|<legacy id>|0`, eBay's documented RESTful item-id shape, with this chapter's own numbering; (e) the enumeration VALUES `ACTIVE`/`ENDED` (auctionStatus is named in the files but its values are only described in prose there), `PAID`/`FULLY_REFUNDED` and `AUCTION` are as eBay documents them, to the author's knowledge, not read from a values list; (f) times: eBay uses ISO-8601 dates; this chapter's clock is a
 * whole-second counter from a FICTIONAL epoch (2026-10-10T00:00:00Z), formatted as ISO-8601 so the shapes match.
 *
 * Parsing limits (stated, and fuzzed for crashes in native/ebay_fuzz.c): the JSON scanner finds `"key":` occurrences IN ORDER along a dotted path, accepts strings without backslash
 * escapes, and numbers; it is NOT a general JSON parser (it does not validate the whole document), which is enough for the fixed shapes above and is covered by the tests below. */
#ifndef EBAY_H
#define EBAY_H
#include <stdint.h>
#include "047_auction.h"
#include "047_market.h"

typedef struct { char method[8]; char path[96]; uint32_t bidder; uint32_t idem; uint32_t body_off; } http_req_t;
typedef struct { uint32_t listing, start_cents, reserve_cents, duration_secs; char title[AUC_TITLE_MAX]; char sku[24]; } offer_req_t;

int ebay_parse_request(const uint8_t *buf, uint32_t len, http_req_t *out); /* 0 ok */
int ebay_path_item_id(const char *path, const char *after, uint32_t *legacy_id); /* extracts N from "...<after>v1|N|0..." ; 0 ok */
int json_find(const char *buf, uint32_t len, const char *dotted_path, const char **val, uint32_t *vlen); /* 0 found */
int ebay_amount_cents(const char *s, uint32_t n, uint32_t *cents);          /* "45.00" -> 4500; strict; 0 ok */
int ebay_parse_proxy_bid_body(const char *body, uint32_t len, uint32_t *max_cents); /* maxAmount.currency must be USD */
int ebay_parse_offer(const char *body, uint32_t len, offer_req_t *out);     /* format must be AUCTION, listingDuration DAYS_n */

uint32_t ebay_build_bid_request(char *out, uint32_t cap, uint32_t listing, uint32_t bidder, uint32_t idem, uint32_t max_cents);
uint32_t ebay_build_offer_request(char *out, uint32_t cap, const char *sku, const char *title, uint32_t days, uint32_t start_cents, uint32_t reserve_cents);
uint32_t ebay_build_bid_response(char *out, uint32_t cap, uint32_t listing, uint32_t bidder, uint32_t bid_no);
uint32_t ebay_build_error(char *out, uint32_t cap, uint32_t http_status, uint32_t error_id, const char *domain, const char *message);
uint32_t ebay_build_bidding(char *out, uint32_t cap, const auc_listing_t *l, uint32_t viewer);
uint32_t ebay_build_item(char *out, uint32_t cap, const auc_listing_t *l);
uint32_t ebay_build_order(char *out, uint32_t cap, const mkt_order_t *o, const char *title);
void ebay_iso_time(uint32_t secs, char out[28]); /* "2026-10-10T00:00:00.000Z" form */
#endif
