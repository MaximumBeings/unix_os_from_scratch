/* Chapter 51 host tool, built from the SAME 051_book.c the kernel links.
 *   book_cli run CMDFILE    run a command stream; print, per command, the verdict, the trades and the feed bytes it produced (hex), then the final book, statistics, the book hash and the hash of the book REBUILT from the whole feed
 *   book_cli feed FILE      a raw feed file (2-byte length-prefixed messages): rebuild the book and print the verdict, or the reason and message number
 * Command lines:  N id B|S price qty ts   (price 0 = market)   C id ts   R old new price qty ts   X id qty ts */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../051_book.h"
static ob_t E, F; static uint8_t feed[1 << 20]; static uint8_t file[1 << 20];
static void hex(const uint8_t *p, uint32_t n) { for (uint32_t i = 0; i < n; i++) { printf("%02x", p[i]); } }
static void dump(const ob_t *b) {
    uint32_t pr[OB_MAX_ORDERS]; uint64_t q[OB_MAX_ORDERS]; static const char *nm[2] = {"bid", "ask"};
    for (int s = 0; s < 2; s++) { int n = ob_levels(b, s, pr, q, OB_MAX_ORDERS); for (int i = 0; i < n; i++) { printf("%s %u %llu\n", nm[s], pr[i], (unsigned long long)q[i]); } }
    uint8_t h[32]; ob_hash(b, h); printf("hash "); hex(h, 32); printf("\nstats trades %llu volume %llu notional %llu live %u\n", (unsigned long long)b->n_trades, (unsigned long long)b->volume_qty, (unsigned long long)b->notional, b->n_live);
}
int main(int argc, char **argv) {
    if (argc < 3) { return 2; }
    if (!strcmp(argv[1], "feed")) { FILE *f = fopen(argv[2], "rb"); if (!f) { return 2; } size_t n = fread(file, 1, sizeof file, f); fclose(f); ob_init(&F); int rc = ob_feed_apply(&F, file, (uint32_t)n);
        if (rc) { printf("REFUSED message %u: %s\n", F.feed_err_msg, F.feed_err); return 0; } printf("OK\n"); dump(&F); return 0; }
    FILE *f = fopen(argv[2], "r"); if (!f) { return 2; } ob_init(&E); E.feed = feed; E.feed_cap = sizeof feed; char line[256]; int n = 0;
    while (fgets(line, sizeof line, f)) {
        char c = line[0]; unsigned long long a, b2, ts; unsigned pr, q; char sd; int rc = 0; uint32_t before = E.feed_len; n++;
        if (c == 'N' && sscanf(line + 1, "%llu %c %u %u %llu", &a, &sd, &pr, &q, &ts) == 5) { rc = ob_new(&E, a, sd == 'B' ? OB_BUY : OB_SELL, pr, q, ts); }
        else if (c == 'C' && sscanf(line + 1, "%llu %llu", &a, &ts) == 2) { rc = ob_cancel(&E, a, ts); }
        else if (c == 'R' && sscanf(line + 1, "%llu %llu %u %u %llu", &a, &b2, &pr, &q, &ts) == 5) { rc = ob_replace(&E, a, b2, pr, q, ts); }
        else if (c == 'X' && sscanf(line + 1, "%llu %u %llu", &a, &q, &ts) == 3) { rc = ob_reduce(&E, a, q, ts); }
        else { continue; }
        printf("cmd %d %s\n", n, rc ? ob_strerror(rc) : "ok");
        for (uint32_t i = 0; i < E.n_tr; i++) { printf("trade %llu %llu %u %u %llu\n", (unsigned long long)E.tr[i].maker, (unsigned long long)E.tr[i].taker, E.tr[i].price, E.tr[i].qty, (unsigned long long)E.tr[i].match); }
        if (E.feed_len > before) { printf("feed "); hex(E.feed + before, E.feed_len - before); printf("\n"); }
    }
    fclose(f); dump(&E); ob_init(&F); int rc = ob_feed_apply(&F, feed, E.feed_len); uint8_t h1[32], h2[32]; ob_hash(&E, h1); ob_hash(&F, h2);
    printf("rebuilt %s %s\n", rc ? "REFUSED" : "ok", !rc && !memcmp(h1, h2, 32) ? "same-hash" : "DIFFERENT"); return 0;
}
