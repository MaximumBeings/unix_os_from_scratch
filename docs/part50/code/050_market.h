/* Chapter 47: the marketplace -- the layer that turns the auction engine (050_auction.h), the money ledger (050_ledger.h) and the write-ahead log (050_wal.h) into one
 * deterministic state machine.
 *
 * THE central design rule of the chapter: EVERY change to the marketplace is a `mkt_cmd_t`, and EVERY command is applied by exactly one function, mkt_apply(), which reads no clock,
 * no random source and no outside state. The live path is therefore "encode the command, write it to the log, THEN apply it" (mkt_submit()), and the recovery path is "read the
 * commands back, apply each one" (mkt_replay()) -- the same mkt_apply() both times, so a replayed marketplace is the same marketplace, bit for bit, which the chapter proves by hashing.
 *
 * Idempotency (a retried request never applies twice): a command that moves value (PLACE_BID, DEPOSIT, CHECKOUT) carries a client-chosen idempotency key. The first time a key is
 * seen, its result is stored with a fingerprint of the request; a retry with the same key and the same request returns the STORED result without applying anything; the same key with a
 * DIFFERENT request is refused (MKT_ERR_IDEM_MISMATCH). (This is the semantics of the "Idempotency-Key" header in widely used payment APIs; eBay's Buy Offer API has no such header, so
 * the header name below is this chapter's addition.) The table is part of the replayed state, so a retry that arrives after a crash and a replay is still recognised.
 *
 * Invariants checked after EVERY command (mkt_check()): the ledger sums to zero; escrow holds exactly the totals of the PAID orders; no user account is negative; every listing's price
 * is at least its starting price and (once bids exist) at most the leader's maximum; the leader's maximum is the largest standing maximum; a SOLD listing has bids. A violation latches
 * `broken` and the demo and tests assert it never happens. */
#ifndef MARKET_H
#define MARKET_H
#include <stdint.h>
#include "050_auction.h"
#include "050_ledger.h"
#include "050_wal.h"

#define MKT_MAX_ORDERS 8
#define MKT_MAX_IDEM 24
enum { CMD_CREATE = 1, CMD_BID = 2, CMD_CLOSE = 3, CMD_DEPOSIT = 4, CMD_CHECKOUT = 5, CMD_RELEASE = 6, CMD_REFUND = 7 };
/* Result codes: 0..9 are AUC_*, 10..13 are LED_*+10, then the marketplace's own. */
enum { MKT_OK = 0, MKT_ERR_NOT_SOLD = 20, MKT_ERR_NOT_WINNER = 21, MKT_ERR_NO_ORDER = 22, MKT_ERR_ORDER_STATE = 23, MKT_ERR_ORDER_EXISTS = 24, MKT_ERR_IDEM_MISMATCH = 25,
       MKT_ERR_IDEM_FULL = 26, MKT_ERR_BAD_CMD = 27, MKT_ERR_ORDERS_FULL = 28, MKT_ERR_FUNDS = 29 };
enum { ORD_PAID = 1, ORD_RELEASED = 2, ORD_REFUNDED = 3 };

typedef struct {
    uint32_t type, now, idem;
    uint32_t a[8]; /* CREATE: id seller start reserve duration ext_window ext_secs | BID: listing bidder max | CLOSE: listing | DEPOSIT: acct cents | CHECKOUT: listing buyer ship | RELEASE/REFUND: listing */
    char title[AUC_TITLE_MAX];
} mkt_cmd_t;

typedef struct { uint32_t code, v1, v2, replayed; } mkt_result_t; /* v1/v2: BID price,is_high | CHECKOUT total,fee | CLOSE status | DEPOSIT balance */

typedef struct { uint32_t used, id, listing, buyer, seller, item_cents, ship_cents, total_cents, fee_cents, status; } mkt_order_t;
typedef struct { uint32_t used, key, fingerprint; mkt_result_t result; } mkt_idem_t;

typedef struct {
    auc_t auc;
    ledger_t led;
    mkt_order_t orders[MKT_MAX_ORDERS];
    mkt_idem_t idem[MKT_MAX_IDEM];
    uint32_t applied; /* number of commands applied */
    uint32_t wal_seq; /* last sequence number written to / replayed from the log */
    uint32_t broken;  /* latched if an invariant ever failed */
} market_t;

#define MKT_LOG_END (-1)
typedef int (*wal_writer_t)(uint32_t seq, const uint8_t rec[WAL_RECORD]);
/* Returns 0 and fills buf and *out_len, or non-zero when the record does not exist. */
typedef int (*wal_reader_t)(uint32_t seq, uint8_t *buf, uint32_t buf_size, uint32_t *out_len);

void mkt_init(market_t *m);
void mkt_apply(market_t *m, const mkt_cmd_t *c, mkt_result_t *r);
/* Write-ahead: log the command with `w`, and only if the write succeeded apply it. Returns 0 on success, 1 if the log write failed (nothing applied). */
int mkt_submit(market_t *m, const mkt_cmd_t *c, wal_writer_t w, mkt_result_t *r);
/* Rebuild a market from the log. *stop_code is MKT_LOG_END when the log simply ends (a record with the next sequence number does not exist: normal), or the WAL_ERR_* code that
 * stopped replay at a damaged record (a record that EXISTS but is short, has a bad magic, length or checksum, or the wrong sequence number). Returns commands applied. */
uint32_t mkt_replay(market_t *m, wal_reader_t rd, int *stop_code);
void mkt_hash(const market_t *m, uint8_t out[32]); /* SHA-256 of the canonical state */
int mkt_check(const market_t *m);                  /* 0 if every invariant holds, else the number of the first violated one */
void mkt_cmd_encode(const mkt_cmd_t *c, uint8_t out[WAL_PAYLOAD]);
int mkt_cmd_decode(const uint8_t in[WAL_PAYLOAD], mkt_cmd_t *c); /* 0 ok, 1 unknown command type */
mkt_order_t *mkt_find_order(market_t *m, uint32_t listing);
#endif
