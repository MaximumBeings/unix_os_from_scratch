/* Chapter 47, host-side tests of the eBay-shaped message layer (NOT part of the kernel). Build and run:
 *   gcc -O1 -fsanitize=address,undefined -I.. ebay_test.c ../047_ebay.c ../047_market.c ../047_auction.c ../047_ledger.c ../047_wal.c ../047_sha256.c -o ebay_test && ./ebay_test > ebay_out.txt
 * 1. round trips: build a request -> parse it -> same values (200,000 random cases);  2. strict amount parsing against a table of good and bad strings;
 * 3. the offer body round trip;  4. every builder with every too-small buffer size: never writes out of bounds, returns 0 or a complete string (ASan checks the writes);
 * 5. PARSER ROBUSTNESS: 2,000,000 randomly damaged requests and bodies (flip, delete, insert, truncate, splice) through every parser: no crash, no out-of-bounds read (ASan/UBSan),
 *    and any value the parser accepts is in range;  6. ISO-8601 times for 20 sample instants, printed for ebay_iso_check.py to compare with Python's datetime. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "047_ebay.h"
static uint32_t rs = 123456789u;
static uint32_t rnd(void) { uint32_t x = rs; x ^= x << 13; x ^= x >> 17; x ^= x << 5; rs = x; return x; }
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); exit(1); } } while (0)

static void feed_all_parsers(const uint8_t *buf, uint32_t len) {
    http_req_t rq; uint32_t id, cents; offer_req_t of; const char *v; uint32_t n;
    if (ebay_parse_request(buf, len, &rq) == 0) {
        CHECK(rq.body_off <= len + 1, "body offset out of range");
        if (ebay_path_item_id(rq.path, "bidding/", &id) == 0) CHECK(id < 1000000000u, "id range");
        if (rq.body_off < len && ebay_parse_proxy_bid_body((const char *)buf + rq.body_off, len - rq.body_off, &cents) == 0) CHECK(cents < 1000000000u, "cents range");
    }
    if (ebay_parse_offer((const char *)buf, len, &of) == 0) CHECK(of.duration_secs >= 86400u && of.duration_secs <= 864000u && strlen(of.title) < AUC_TITLE_MAX, "offer range");
    (void)json_find((const char *)buf, len, "a.b.c", &v, &n);
    (void)ebay_amount_cents((const char *)buf, len, &cents);
}

int main(int argc, char **argv) {
    uint32_t fuzz_cases = argc > 1 ? (uint32_t)atoi(argv[1]) : 2000000u; /* the chapter's run uses the default; mutation.py uses fewer */
    char buf[1024];
    /* 1 round trips */
    for (uint32_t i = 0; i < 200000; i++) {
        uint32_t listing = 1 + rnd() % 999999, bidder = 1 + rnd() % 99999, idem = 1 + rnd() % 999999, cents = 1 + rnd() % 9999999;
        uint32_t n = ebay_build_bid_request(buf, sizeof buf, listing, bidder, idem, cents);
        CHECK(n > 0, "build failed");
        http_req_t rq; uint32_t id = 0, mx = 0;
        CHECK(ebay_parse_request((uint8_t *)buf, n, &rq) == 0, "parse request");
        CHECK(strcmp(rq.method, "POST") == 0 && rq.bidder == bidder && rq.idem == idem, "headers");
        CHECK(ebay_path_item_id(rq.path, "bidding/", &id) == 0 && id == listing, "item id %u vs %u", id, listing);
        CHECK(ebay_parse_proxy_bid_body(buf + rq.body_off, n - rq.body_off, &mx) == 0 && mx == cents, "amount %u vs %u", mx, cents);
    }
    printf("1. 200000 bid requests built, parsed back, identical (method, bidder, idempotency key, item id, amount)\n");
    /* 2 strict amounts */
    static const struct { const char *s; int ok; uint32_t c; } T[] = {
        {"45.00", 1, 4500}, {"45", 1, 4500}, {"45.5", 1, 4550}, {"0.99", 1, 99}, {"0", 1, 0}, {"9999999.99", 0, 0}, {"999999.99", 1, 99999999}, {"12.345", 0, 0}, {"12.", 0, 0}, {".50", 0, 0},
        {"-5.00", 0, 0}, {"1e3", 0, 0}, {"", 0, 0}, {"4 5", 0, 0}, {"4,5", 0, 0}, {"007.10", 1, 710}, {"12345678", 0, 0} };
    int ok_ct = 0;
    for (uint32_t i = 0; i < sizeof T / sizeof T[0]; i++) {
        uint32_t c = 0; int rc = ebay_amount_cents(T[i].s, (uint32_t)strlen(T[i].s), &c);
        CHECK((rc == 0) == T[i].ok && (rc != 0 || c == T[i].c), "amount \"%s\": rc %d c %u", T[i].s, rc, c); ok_ct++;
    }
    printf("2. %d amount strings (good and bad) parsed exactly as specified\n", ok_ct);
    /* 3 offer round trip */
    for (uint32_t i = 0; i < 20000; i++) {
        uint32_t days = 1 + rnd() % 10, st = 1 + rnd() % 500000, rv = (rnd() % 2) ? st + rnd() % 500000 : 0;
        uint32_t n = ebay_build_offer_request(buf, sizeof buf, "SKU-LAMP-1", "Brass lamp", days, st, rv); CHECK(n > 0, "build offer");
        offer_req_t o; CHECK(ebay_parse_offer(buf, n, &o) == 0, "parse offer");
        CHECK(o.start_cents == st && o.reserve_cents == rv && o.duration_secs == days * 86400u && strcmp(o.title, "Brass lamp") == 0 && strcmp(o.sku, "SKU-LAMP-1") == 0, "offer fields");
    }
    printf("3. 20000 AUCTION offers built and parsed back, identical\n");
    /* 4 small buffers */
    auc_t a; auc_init(&a); auc_create(&a, 110001, 3, "Brass lamp", 999, 4000, 0, 86400, 0, 0); uint32_t p = 0; int h = 0; auc_bid(&a, 110001, 4, 5000, 10, &p, &h);
    mkt_order_t o; memset(&o, 0, sizeof o); o.used = 1; o.id = 110001; o.listing = 110001; o.buyer = 4; o.seller = 3; o.item_cents = 4100; o.ship_cents = 500; o.total_cents = 4600; o.fee_cents = 760; o.status = ORD_RELEASED;
    uint32_t full[6] = {0}; char *scratch;
    for (uint32_t cap = 1; cap < 700; cap++) {
        scratch = malloc(cap);
        uint32_t r[6];
        r[0] = ebay_build_bid_request(scratch, cap, 110001, 4, 7, 5000); r[1] = ebay_build_bidding(scratch, cap, &a.listings[0], 4); r[2] = ebay_build_item(scratch, cap, &a.listings[0]);
        r[3] = ebay_build_order(scratch, cap, &o, "Brass lamp"); r[4] = ebay_build_error(scratch, cap, 400, 9001, "API_BIDDING", "Bid too low"); r[5] = ebay_build_bid_response(scratch, cap, 110001, 4, 1);
        for (int k = 0; k < 6; k++) { CHECK(r[k] == 0 || r[k] < cap, "builder %d returned %u for cap %u", k, r[k], cap); if (r[k]) full[k] = r[k]; }
        free(scratch);
    }
    printf("4. six builders x 699 buffer sizes: never wrote out of bounds; complete outputs are %u, %u, %u, %u, %u, %u bytes\n", full[0], full[1], full[2], full[3], full[4], full[5]);
    /* 5 robustness */
    char seeds[4][700]; uint32_t sl[4];
    sl[0] = ebay_build_bid_request(seeds[0], 700, 110001, 4, 7, 5000); sl[1] = ebay_build_offer_request(seeds[1], 700, "SKU-1", "Lamp", 3, 999, 4000);
    sl[2] = ebay_build_bidding(seeds[2], 700, &a.listings[0], 4); sl[3] = ebay_build_order(seeds[3], 700, &o, "Lamp");
    uint64_t cases = 0;
    for (uint32_t i = 0; i < fuzz_cases; i++) {
        uint32_t k = rnd() % 4, n = sl[k]; char *m = malloc(n + 16); memcpy(m, seeds[k], n);
        uint32_t edits = 1 + rnd() % 4;
        for (uint32_t e = 0; e < edits && n > 0; e++) {
            uint32_t pos = rnd() % n, op = rnd() % 5;
            if (op == 0) m[pos] ^= (char)(1u << (rnd() % 8));
            else if (op == 1) { memmove(m + pos, m + pos + 1, n - pos - 1); n--; }
            else if (op == 2 && n < sl[k] + 8) { memmove(m + pos + 1, m + pos, n - pos); m[pos] = (char)rnd(); n++; }
            else if (op == 3) n = pos;
            else m[pos] = "{}[]\":,. \\\r\n0123456789"[rnd() % 23];
        }
        char *exact = malloc(n ? n : 1); memcpy(exact, m, n); free(m);     /* exact-size heap block: ASan flags any read past the end */
        feed_all_parsers((const uint8_t *)exact, n); free(exact); cases++;
    }
    printf("5. %llu randomly damaged requests and bodies through every parser: no crash, no out-of-bounds access, every accepted value in range\n", (unsigned long long)cases);
    /* 6 times */
    uint32_t samples[] = {0, 1, 59, 3600, 86399, 86400, 22 * 86400, 3 * 86400 + 3661, 100 * 86400, 365 * 86400, 1000000, 31536000u + 59, 5000000, 40000000, 123456789};
    for (uint32_t i = 0; i < sizeof samples / sizeof samples[0]; i++) { char iso[28]; ebay_iso_time(samples[i], iso); printf("ISO %u %s\n", samples[i], iso); }
    return 0;
}
