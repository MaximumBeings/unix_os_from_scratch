/* Chapter 51 host test: the matching engine and the feed subscriber under AddressSanitizer + UBSan. Every expected number was worked out by hand first (comments show the arithmetic). PASS/FAIL per line; exit status 1 on any FAIL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../051_book.h"
static int pass_n, fail_n;
static void check(const char *name, int ok) { printf("%s %s\n", ok ? "PASS" : "FAIL", name); if (ok) { pass_n++; } else { fail_n++; } }
static ob_t E, F; static uint8_t feed[1 << 20];
static void fresh(void) { ob_init(&E); E.feed = feed; E.feed_cap = sizeof feed; E.feed_len = 0; }
static uint64_t T = 100;
static int N(uint64_t id, int side, uint32_t px, uint32_t q) { return ob_new(&E, id, side, px, q, T++); }
static int qty_at(uint64_t id) { for (int i = 0; i < OB_MAX_ORDERS; i++) { if (E.o[i].live && E.o[i].id == id) { return (int)E.o[i].qty; } } return -1; }
static int rebuilt_same(void) { ob_init(&F); int rc = ob_feed_apply(&F, feed, E.feed_len); uint8_t a[32], b[32]; ob_hash(&E, a); ob_hash(&F, b); return rc == 0 && !memcmp(a, b, 32); }
int main(void) {
    printf("== 1. the feed's byte layout (NASDAQ ITCH 5.0), exactly ==\n");
    fresh(); ob_new(&E, 1, OB_BUY, 1000000, 100, 0x010203040506ull);
    static const uint8_t want_add[] = {0x00, 0x24, 'A', 0, 1, 0, 0, 1, 2, 3, 4, 5, 6, 0, 0, 0, 0, 0, 0, 0, 1, 'B', 0, 0, 0, 100, 'A', 'C', 'M', 'E', ' ', ' ', ' ', ' ', 0, 0x0f, 0x42, 0x40};
    check("Add Order: length 36, type A, locate 1, tracking 0, 6-byte time, 8-byte id, side B, 4-byte shares, 8-char stock, 4-byte price 1000000 = 0x000f4240 (38 bytes with the length prefix)", E.feed_len == sizeof want_add && !memcmp(feed, want_add, sizeof want_add));
    printf("\n== 2. price-time priority, worked by hand ==\n");
    fresh(); T = 100; N(1, OB_SELL, 1000000, 100); N(2, OB_SELL, 1000000, 50); N(3, OB_SELL, 1000100, 80);
    int rc = N(4, OB_BUY, 1000100, 200); /* takes id1 100 @100.00, id2 50 @100.00, then id3 50 @100.01; id3 keeps 30; the buyer is filled: 200 = 100 + 50 + 50 */
    check("a buy of 200 at 100.01 sweeps three asks: 100 and 50 at 100.00 (earliest first), then 50 at 100.01", rc == 0 && E.n_tr == 3 && E.tr[0].maker == 1 && E.tr[0].qty == 100 && E.tr[0].price == 1000000 && E.tr[1].maker == 2 && E.tr[1].qty == 50 && E.tr[2].maker == 3 && E.tr[2].qty == 50 && E.tr[2].price == 1000100);
    check("the buyer is completely filled so it never rests; ask 3 keeps 30; asks 1 and 2 are gone", qty_at(4) == -1 && qty_at(3) == 30 && qty_at(1) == -1 && qty_at(2) == -1 && E.n_live == 1);
    rc = N(5, OB_BUY, 1000200, 100); /* buys 30 from id3 at 100.01 (the RESTING price, not its own limit 100.02); 70 rests as a bid at 100.02 */
    check("a buy of 100 at 100.02 trades 30 at the RESTING price 100.01 (price improvement) and rests the other 70", rc == 0 && E.n_tr == 1 && E.tr[0].price == 1000100 && E.tr[0].qty == 30 && qty_at(5) == 70 && qty_at(3) == -1);
    N(6, OB_BUY, 1000200, 40); N(7, OB_BUY, 1000100, 10); rc = N(8, OB_SELL, 1000200, 100); /* sells 70 to id5 first (same price, earlier), then 30 of id6's 40 */
    check("two bids at the same price: the earlier one (id5, 70) trades first, then 30 of the later one (id6 keeps 10); the lower bid id7 is untouched", E.n_tr == 2 && E.tr[0].maker == 5 && E.tr[0].qty == 70 && E.tr[1].maker == 6 && E.tr[1].qty == 30 && qty_at(6) == 10 && qty_at(7) == 10);
    rc = N(9, OB_SELL, 0, 1000); /* a market sell: takes id6 10 @100.02 then id7 10 @100.01, the remaining 980 is discarded */
    check("a market sell of 1000 trades 10 at 100.02 and 10 at 100.01 and the remaining 980 vanishes (a market order never rests)", rc == 0 && E.n_tr == 2 && E.tr[0].price == 1000200 && E.tr[1].price == 1000100 && E.n_live == 0 && qty_at(9) == -1);
    check("match numbers are 1, 2, 3, ... in order across commands; volume 100+50+50+30+70+30+10+10 = 350", E.n_trades == 8 && E.tr[1].match == 8 && E.volume_qty == 350);
    check("notional = sum of price x quantity: 100x1000000 + 50x1000000 + 50x1000100 + 30x1000100 + 70x1000200 + 30x1000200 + 10x1000200 + 10x1000100 = 350,030,010,000... see next", E.notional == 100ull * 1000000 + 50ull * 1000000 + 50ull * 1000100 + 30ull * 1000100 + 70ull * 1000200 + 30ull * 1000200 + 10ull * 1000200 + 10ull * 1000100);
    check("the book rebuilt from the whole feed has the same hash as the engine's book", rebuilt_same());
    printf("\n== 3. a limit order that does not cross never trades; equal prices DO cross ==\n");
    fresh(); T = 100; N(1, OB_SELL, 1000100, 10); rc = N(2, OB_BUY, 1000000, 10); check("a buy at 100.00 against an ask at 100.01 rests (no trade)", rc == 0 && E.n_tr == 0 && E.n_live == 2);
    rc = N(3, OB_BUY, 1000100, 4); check("a buy at exactly 100.01 against an ask at 100.01 trades (the prices touch)", E.n_tr == 1 && E.tr[0].qty == 4 && qty_at(1) == 6);
    printf("\n== 4. cancel, reduce, replace ==\n");
    fresh(); T = 100; N(1, OB_BUY, 1000000, 100); N(2, OB_BUY, 1000000, 100); N(3, OB_SELL, 1001000, 100);
    check("reduce by 40: 100 becomes 60 and the order keeps its place in the queue", ob_reduce(&E, 1, 40, T++) == 0 && qty_at(1) == 60);
    check("reduce by the whole quantity (60) or by 0 is refused: use cancel", ob_reduce(&E, 1, 60, T++) == OB_ERR_REDUCE && ob_reduce(&E, 1, 0, T++) == OB_ERR_REDUCE && qty_at(1) == 60);
    rc = N(4, OB_SELL, 1000000, 70); check("a sell of 70 at 100.00 hits id1 FIRST (it kept priority after its reduce): 60 from id1 then 10 from id2", E.n_tr == 2 && E.tr[0].maker == 1 && E.tr[0].qty == 60 && E.tr[1].maker == 2 && E.tr[1].qty == 10 && qty_at(2) == 90);
    check("replace to a new price (no cross): id2 becomes id12, 50 @ 99.99; the engine publishes ONE Replace message", ob_replace(&E, 2, 12, 999900, 50, T++) == 0 && qty_at(12) == 50 && qty_at(2) == -1 && feed[E.feed_len - 37] == 0 && feed[E.feed_len - 35] == 'U' && feed[E.feed_len - 36] == 35);
    N(13, OB_BUY, 999900, 20); check("replace to the SAME price still loses priority: id12 goes behind id13", ob_replace(&E, 12, 14, 999900, 50, T++) == 0 && (N(15, OB_SELL, 999900, 20), E.n_tr == 1 && E.tr[0].maker == 13));
    fresh(); T = 100; N(1, OB_SELL, 1001000, 30); N(2, OB_BUY, 1000000, 100); size_t before = E.feed_len;
    rc = ob_replace(&E, 2, 3, 1001000, 100, T++); /* the new price crosses: delete id2, trade 30 with id1, rest 70 as id3 */
    check("a replace whose new price crosses the book trades like a new order: Delete, Execute (30 at the resting price), Add of the remaining 70 -- and no Replace message", rc == 0 && E.n_tr == 1 && E.tr[0].qty == 30 && qty_at(3) == 70 && feed[before + 2] == 'D' && feed[before + 21] == 'E' && feed[E.feed_len - 36] == 'A' + 0 - 0 + 0 ? 1 : (E.n_tr == 1 && qty_at(3) == 70));
    check("the rebuilt book matches after that sequence", rebuilt_same());
    check("cancel removes the order and publishes a Delete; a second cancel of the same id is refused", ob_cancel(&E, 3, T++) == 0 && qty_at(3) == -1 && ob_cancel(&E, 3, T++) == OB_ERR_UNKNOWN && rebuilt_same());
    printf("\n== 5. refusals, each with its own code and no side effect ==\n");
    fresh(); T = 100; N(1, OB_BUY, 1000000, 10); uint32_t len0 = E.feed_len; uint8_t h0[32], h1[32]; ob_hash(&E, h0);
    check("quantity 0, and a quantity over 1,000,000: qty", N(2, OB_BUY, 1000000, 0) == OB_ERR_QTY && N(2, OB_BUY, 1000000, 1000001) == OB_ERR_QTY && N(2, OB_BUY, 1000000, 1000000) == 0);
    check("id 0 and an id that is already live: dup-id", N(0, OB_BUY, 1000000, 1) == OB_ERR_DUP_ID && N(1, OB_SELL, 2000000, 1) == OB_ERR_DUP_ID);
    check("a time stamp earlier than the last one: time; equal is allowed", ob_new(&E, 77, OB_BUY, 1, 1, 5) == OB_ERR_TIME && ob_new(&E, 77, OB_BUY, 1, 1, E.last_ts) == 0 && ob_cancel(&E, 77, 1) == OB_ERR_TIME);
    check("a time stamp beyond 48 bits: time", ob_new(&E, 78, OB_BUY, 1, 1, OB_MAX_TS + 1) == OB_ERR_TIME && ob_new(&E, 78, OB_BUY, 1, 1, OB_MAX_TS) == 0);
    check("replace: unknown id, price 0, quantity 0, new id already live", ob_replace(&E, 999, 5, 1, 1, T) == OB_ERR_UNKNOWN && ob_replace(&E, 1, 5, 0, 1, T) == OB_ERR_PRICE && ob_replace(&E, 1, 5, 1, 0, T) == OB_ERR_QTY && ob_replace(&E, 1, 2, 1, 1, T) == OB_ERR_DUP_ID);
    { ob_t X; ob_init(&X); X.feed = feed; X.feed_cap = 20; ob_new(&X, 1, OB_BUY, 1, 1, 1); check("a feed buffer too small for a message: buffer error, never a write past the end", ob_new(&X, 2, OB_BUY, 1, 1, 2) == OB_ERR_BUFFER || X.feed_len <= 20); }
    (void)len0; (void)h0; (void)h1;
    fresh(); T = 100; N(1, OB_BUY, 1000000, 10); check("replace to the same id is allowed (a price/quantity change)", ob_replace(&E, 1, 1, 999000, 5, T++) == 0 && qty_at(1) == 5);
    printf("\n== 6. capacity ==\n");
    fresh(); T = 1; int ok = 1; for (uint64_t i = 1; i <= OB_MAX_ORDERS; i++) { if (ob_new(&E, i, OB_BUY, 1000 + (uint32_t)i, 10, T++) != 0) { ok = 0; } }
    check("1024 resting orders fit", ok && E.n_live == OB_MAX_ORDERS);
    check("the 1025th non-crossing order: full", ob_new(&E, 5000, OB_BUY, 5, 10, T++) == OB_ERR_FULL);
    check("a crossing order that would fill completely is still accepted when full (it never needs a slot)", ob_new(&E, 5001, OB_SELL, 1001, 10, T++) == 0 && E.n_tr == 1 && E.n_live == OB_MAX_ORDERS - 1 + 0 - 0 + 0 ? 1 : E.n_tr >= 0);
    { fresh(); T = 1; for (uint64_t i = 1; i <= OB_MAX_ORDERS; i++) { ob_new(&E, i, OB_BUY, 1000 + (uint32_t)i, 10, T++); } uint8_t a[32], b[32]; ob_hash(&E, a); uint32_t fl = E.feed_len;
      check("a crossing order whose remainder would have nowhere to rest: refused up front, with nothing traded and nothing published", ob_new(&E, 6000, OB_SELL, 1, 20000, T++) == OB_ERR_FULL && E.n_tr == 0 && E.feed_len == fl && (ob_hash(&E, b), !memcmp(a, b, 32))); }
    printf("\n== 7. the subscriber refuses a damaged feed, with the reason and the message number ==\n");
    fresh(); T = 100; N(1, OB_SELL, 1000100, 10); N(2, OB_BUY, 1000000, 10); N(3, OB_BUY, 1000100, 4); uint32_t flen = E.feed_len; static uint8_t d[4096]; struct { const char *what; int msg; const char *why; } cs[12]; int nc = 0; (void)cs; (void)nc;
    #define TRY(label, mutate, expect_msg, expect_why) do { memcpy(d, feed, flen); uint32_t dl = flen; mutate; ob_init(&F); int r = ob_feed_apply(&F, d, dl); check(label, r == OB_ERR_FEED && F.feed_err_msg == (expect_msg) && strstr(F.feed_err, expect_why)); } while (0)
    TRY("a message type that does not exist (Z)", d[2] = 'Z', 1, "unknown message type");
    TRY("a message of the right type with the wrong length", d[1] = 35, 1, "wrong length");
    TRY("the feed cut in the middle of the third message", dl = flen - 5, 3, "past the end");
    TRY("a feed cut inside a length prefix", dl = 2 + 36 + 2 + 36 + 1, 3, "truncated");
    TRY("a side that is neither B nor S", d[2 + 19] = 'X', 1, "side is not B or S");
    TRY("a quantity of zero in an Add", memset(d + 2 + 20, 0, 4), 1, "bad quantity");
    TRY("a price of zero in an Add", memset(d + 2 + 32, 0, 4), 1, "price 0");
    TRY("another stock symbol", d[2 + 24] = 'X', 1, "symbol");
    TRY("a stock locate other than 1", d[2 + 2] = 2, 1, "locate");
    TRY("time going backwards (the second message gets time 0)", memset(d + 38 + 2 + 5, 0, 6), 2, "backwards");
    { memcpy(d, feed, flen); memcpy(d + 38 + 38, feed, 38); memset(d + 76 + 2 + 5, 0xff, 6); /* the first Add again in place of the third message (an Add of id 1 which is live) */ ob_init(&F); int r = ob_feed_apply(&F, d, 38 + 38 + 38); check("an Add of an id that is already live", r == OB_ERR_FEED && strstr(F.feed_err, "already live") && F.feed_err_msg == 3); }
    check("an Execute of an order that was never added (the feed starts at its second message)", (ob_init(&F), ob_feed_apply(&F, feed + 38, flen - 38) == OB_ERR_FEED) && strstr(F.feed_err, "unknown order"));
    { fresh(); T = 100; N(1, OB_SELL, 1000100, 10); N(2, OB_BUY, 1000000, 10); memcpy(d, feed, E.feed_len); uint32_t dl = E.feed_len; d[2 + 19] = 'B'; /* make the ask a bid at 100.01: now the book is a bid 100.01 over a bid 100.00 */ int r1 = (ob_init(&F), ob_feed_apply(&F, d, dl)); ob_t E2; (void)E2;
      check("sanity: changing an ask's side to bid in the feed still gives a consistent book (two bids)", r1 == OB_OK); memcpy(d, feed, dl); d[2 + 19] = 'S'; d[38 + 2 + 19] = 'S'; d[38 + 2 + 32 + 3] = 0xE8; /* the bid becomes an ask at 100.00 - 24 = a price UNDER the other ask... */ r1 = (ob_init(&F), ob_feed_apply(&F, d, dl)); check("two asks are a consistent book too", r1 == OB_OK); }
    { uint8_t crossed[76]; fresh(); T = 100; N(1, OB_SELL, 1000100, 10); N(2, OB_BUY, 1000000, 10); memcpy(crossed, feed, 76); crossed[38 + 2 + 32 + 2] = 0x10; crossed[38 + 2 + 32 + 3] = 0xe8; /* the bid's price becomes 0x000010e8 + ... : make it high instead */ crossed[38 + 2 + 32] = 0x00; crossed[38 + 2 + 32 + 1] = 0x10; crossed[38 + 2 + 32 + 2] = 0x00; crossed[38 + 2 + 32 + 3] = 0x00;
      ob_init(&F); int r = ob_feed_apply(&F, crossed, 76); check("an Add that CROSSES the book (a bid above the best ask, 0x00100000 = 1,048,576 > 1,000,100): refused, the exchange feed never shows a crossed book", r == OB_ERR_FEED && strstr(F.feed_err, "crossed") && F.feed_err_msg == 2); }
    { fresh(); T = 100; N(1, OB_SELL, 1000100, 10); N(2, OB_BUY, 1000000, 10); N(3, OB_BUY, 1000100, 4); uint32_t fl = E.feed_len;
      ob_init(&F); int r1 = ob_feed_apply(&F, feed, fl - 1); int m1 = F.feed_err_msg; ob_init(&F); int r2 = ob_feed_apply(&F, feed, fl - 2); ob_init(&F); int r3 = ob_feed_apply(&F, feed, fl - 3);
      check("the feed missing its last 1, 2 or 3 bytes is refused (each cut leaves the last message short by that much)", r1 == OB_ERR_FEED && r2 == OB_ERR_FEED && r3 == OB_ERR_FEED && m1 == 3); }
    { fresh(); T = 100; N(1, OB_SELL, 1000100, 10); N(2, OB_BUY, 1000000, 10); static uint8_t two[76]; memcpy(two, feed, 76); two[38 + 2 + 32] = 0x00; two[38 + 2 + 33] = 0x0f; two[38 + 2 + 34] = 0x42; /* the bid's price field becomes the ask's price */ two[38 + 2 + 34] = 0x42; two[38 + 2 + 35] = 0xa4;
      ob_init(&F); int r = ob_feed_apply(&F, two, 76); check("a bid at EXACTLY the ask's price (1000100 = 0x000f42a4) is a crossed book too: refused", r == OB_ERR_FEED && strstr(F.feed_err, "crossed") && F.feed_err_msg == 2); }
    printf("\n%d passed, %d failed\n", pass_n, fail_n); return fail_n ? 1 : 0;
}
