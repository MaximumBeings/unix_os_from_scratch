/* Chapter 51: a limit-order-book matching engine and the exchange feed it publishes. One symbol, integer prices (1/10000 dollar) and quantities, PRICE-TIME PRIORITY: an incoming order trades against the best-priced resting orders on the other
 * side, the earliest first, at the RESTING order's price; what is left of a limit order rests in the book, what is left of a market order is discarded. Commands are NEW (limit or market), CANCEL, REPLACE and REDUCE.
 * Every change the engine makes is published as a binary message in the layout of NASDAQ TotalView-ITCH 5.0 (the exchange-to-subscribers feed): Add Order 'A', Order Executed 'E', Order Cancel (reduce) 'X', Order Delete 'D', Order Replace 'U',
 * each preceded by a 2-byte big-endian length as in NASDAQ's binary file format. ITCH is an OUTPUT feed -- it says what happened to resting orders; it has no aggressive orders and no prices on executions -- so a subscriber can REBUILD the book
 * from it alone. ob_feed_apply() is that subscriber. The central test of the chapter: the book rebuilt from the feed has the same SHA-256 hash as the engine's own book, after every command.
 * Priority is the order of arrival into the book; REPLACE is cancel-and-new (priority lost); a replace whose new price would cross trades like a new order. Time stamps are 48-bit nanoseconds supplied by the caller and must not go backwards.
 * Not covered: several symbols, order types beyond limit and market (IOC/FOK/stop/iceberg), self-trade prevention, auctions, order-entry protocols, fees. */
#ifndef BOOK_H
#define BOOK_H
#include <stdint.h>

#define OB_MAX_ORDERS 1024
#define OB_MAX_QTY 1000000u
#define OB_MAX_TS 0xFFFFFFFFFFFFull
#define OB_MAX_TRADES 1100
enum { OB_OK = 0, OB_ERR_DUP_ID = -1, OB_ERR_QTY = -2, OB_ERR_PRICE = -3, OB_ERR_UNKNOWN = -4, OB_ERR_FULL = -5, OB_ERR_TIME = -6, OB_ERR_REDUCE = -7, OB_ERR_INVARIANT = -8, OB_ERR_FEED = -9, OB_ERR_BUFFER = -10 };
enum { OB_BUY = 0, OB_SELL = 1 };
typedef struct { uint64_t id; uint32_t price, qty, seq; uint8_t side, live; } ob_order_t;
typedef struct { uint64_t maker, taker, match; uint32_t price, qty; } ob_trade_t;
typedef struct {
    ob_order_t o[OB_MAX_ORDERS]; uint32_t next_seq; uint64_t next_match, last_ts, n_trades, volume_qty, notional; uint32_t n_live;
    ob_trade_t tr[OB_MAX_TRADES]; uint32_t n_tr; /* trades of the LAST command */
    uint8_t *feed; uint32_t feed_cap, feed_len; /* where messages are appended (NULL: not published) */
    uint32_t feed_msgs; const char *feed_err; uint32_t feed_err_msg; /* subscriber side */
} ob_t;

void ob_init(ob_t *b);
int ob_new(ob_t *b, uint64_t id, int side, uint32_t price /* 0 = market */, uint32_t qty, uint64_t ts);
int ob_cancel(ob_t *b, uint64_t id, uint64_t ts);
int ob_replace(ob_t *b, uint64_t old_id, uint64_t new_id, uint32_t price, uint32_t qty, uint64_t ts);
int ob_reduce(ob_t *b, uint64_t id, uint32_t cancel_qty, uint64_t ts);
int ob_check(const ob_t *b); /* 0 when the book is sane: not crossed, quantities positive, ids unique, counts right; else a positive code */
void ob_hash(const ob_t *b, uint8_t out[32]); /* SHA-256 over the live orders in priority order (bids best first, then asks best first): id, price, qty, side */
int ob_feed_apply(ob_t *b, const uint8_t *buf, uint32_t len); /* the subscriber: apply every message of a feed to b; OB_OK or OB_ERR_FEED (b->feed_err, b->feed_err_msg say why and where) */
int ob_levels(const ob_t *b, int side, uint32_t *price, uint64_t *qty, int max); /* aggregated price levels, best first; returns how many */
const char *ob_strerror(int rc);
#endif
