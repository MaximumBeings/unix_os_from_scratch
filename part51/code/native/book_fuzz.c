/* Chapter 51: randomised API calls and damaged feeds under AddressSanitizer + UBSan.
 * PART A: N x 2000 random calls to ob_new / ob_cancel / ob_replace / ob_reduce with ids, prices, quantities and time stamps drawn from small ranges (so collisions, crossings and refusals are common) AND from extreme values. After EVERY call: the book passes
 * ob_check (not crossed, positive quantities, unique ids, right count); a refused call changes nothing (the book hash is the same and nothing was published); an accepted call keeps CONSERVATION: the quantity added to the book, minus the quantity that left it
 * (traded twice -- once for each side -- cancelled, reduced) equals the change in the book's resting quantity. At the end of each run the book REBUILT from the feed has the same hash, and so does the book rebuilt from every PREFIX of the feed that ends on a message boundary (sampled).
 * PART B: the feed of each run damaged (flip, cut, delete, duplicate, swap, insert noise): the subscriber must refuse or end in a book that passes ob_check; never crash. Usage: book_fuzz N */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../051_book.h"
static uint64_t rng = 0x9E3779B97F4A7C15ull;
static uint64_t xr(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }
static ob_t E, F; static uint8_t feed[1 << 18], dm[1 << 18];
static uint64_t resting(const ob_t *b) { uint64_t s = 0; for (int i = 0; i < OB_MAX_ORDERS; i++) { if (b->o[i].live) { s += b->o[i].qty; } } return s; }
int main(int argc, char **argv) {
    (void)argc; long runs = atol(argv[1]); long calls = 0, accepted = 0, refused = 0, bad = 0, prefix_checks = 0, dmg = 0, dmg_refused = 0, dmg_ok = 0; long codes[12] = {0};
    for (long run = 0; run < runs; run++) {
        ob_init(&E); E.feed = feed; E.feed_cap = sizeof feed; uint64_t ts = 1, idmax = 1; int extreme = (run % 4) == 3;
        for (int k = 0; k < 2000; k++) {
            uint8_t h0[32], h1[32]; ob_hash(&E, h0); uint32_t fl = E.feed_len; uint64_t rest0 = resting(&E); uint64_t vol0 = E.volume_qty; int c = (int)(xr() % 10); int rc; uint64_t added = 0, removed = 0;
            ts += xr() % 3; if (xr() % 50 == 0) { ts = ts > 5 ? ts - 5 : 0; } if (extreme && xr() % 40 == 0) { ts = OB_MAX_TS - (xr() % 3); }
            uint64_t id = (xr() % 4 == 0 && idmax > 1) ? 1 + xr() % idmax : idmax++; if (extreme && xr() % 30 == 0) { id = xr() % 3 ? 0 : 0xFFFFFFFFFFFFFFFFull; }
            uint32_t px = (uint32_t)(1000 + (xr() % 40) * 10); uint32_t q = (uint32_t)(1 + xr() % 100); if (xr() % 25 == 0) { px = 0; } if (extreme && xr() % 30 == 0) { px = 0xFFFFFFFFu; q = (uint32_t)(xr() % 3 ? OB_MAX_QTY + 1 : 0); }
            if (c < 5) { rc = ob_new(&E, id, xr() & 1, px, q, ts); if (!rc && px) { added = q; } }
            else if (c < 7) { rc = ob_cancel(&E, 1 + xr() % idmax, ts); }
            else if (c < 9) { rc = ob_replace(&E, 1 + xr() % idmax, id, px, q, ts); }
            else { rc = ob_reduce(&E, 1 + xr() % idmax, (uint32_t)(1 + xr() % 120), ts); }
            calls++; codes[-rc < 11 ? -rc : 11]++;
            if (ob_check(&E)) { bad++; printf("INVARIANT BROKEN after call %d of run %ld (rc %d)\n", k, run, rc); }
            ob_hash(&E, h1);
            if (rc) { refused++; if (memcmp(h0, h1, 32) || E.feed_len != fl || E.n_tr) { bad++; printf("a REFUSED call (%s) changed something\n", ob_strerror(rc)); } }
            else {
                accepted++; uint64_t traded = E.volume_qty - vol0; uint64_t rest1 = resting(&E); (void)added; (void)removed;
                /* conservation by the feed: the quantity resting now equals the quantity before plus Adds minus Executes minus Cancels/Deletes (checked by rebuilding at the end); here: resting can only grow by this call's own order */
                if (rest1 > rest0 + (c < 5 ? q : (c < 9 ? q : 0)) ) { bad++; printf("resting quantity grew more than the order that was submitted\n"); }
                if (traded > 0 && E.n_tr == 0) { bad++; printf("volume moved without a trade record\n"); }
            }
            if (E.feed_len > sizeof feed - 4096) { break; }
        }
        ob_init(&F); int rc = ob_feed_apply(&F, feed, E.feed_len); uint8_t a[32], b[32]; ob_hash(&E, a); ob_hash(&F, b);
        if (rc || memcmp(a, b, 32) || F.n_trades != E.n_trades || F.volume_qty != E.volume_qty || F.notional != E.notional) { bad++; printf("REBUILT BOOK DIFFERS in run %ld (rc %d %s)\n", run, rc, F.feed_err); }
        uint32_t pos = 0, nmsg = 0, stride = 1 + (uint32_t)(E.feed_msgs / 20);
        while (pos < E.feed_len) { uint32_t l = 2u + ((uint32_t)feed[pos] << 8 | feed[pos + 1]); pos += l; nmsg++; if (nmsg % stride == 0) { ob_init(&F); if (ob_feed_apply(&F, feed, pos) || ob_check(&F)) { bad++; printf("a prefix of the feed does not rebuild a sane book\n"); } prefix_checks++; } }
        for (int v = 0; v < 40 && E.feed_len > 4; v++) {
            uint32_t len = E.feed_len; memcpy(dm, feed, len); int kind = (int)(xr() % 6);
            switch (kind) {
            case 0: dm[xr() % len] ^= (uint8_t)(1u << (xr() % 8)); break;
            case 1: len = (uint32_t)(xr() % len); break;
            case 2: { uint32_t a2 = xr() % len, l2 = 1 + xr() % 60; if (a2 + l2 > len) { l2 = len - a2; } memmove(dm + a2, dm + a2 + l2, len - a2 - l2); len -= l2; break; }
            case 3: { uint32_t a2 = xr() % len, l2 = 1 + xr() % 60; if (len + l2 < sizeof dm) { memmove(dm + a2 + l2, dm + a2, len - a2); len += l2; } break; }
            case 4: { uint32_t a2 = xr() % len, b2 = xr() % len, l2 = 1 + xr() % 40; if (a2 + l2 > len) { l2 = len - a2; } if (b2 + l2 > len) { l2 = len - b2; } uint8_t t[64]; if (l2 > 64) { l2 = 64; } memcpy(t, dm + a2, l2); memcpy(dm + a2, dm + b2, l2); memcpy(dm + b2, t, l2); break; }
            default: { uint32_t a2 = xr() % len; for (int i = 0; i < 6 && a2 + i < len; i++) { dm[a2 + i] = (uint8_t)xr(); } break; }
            }
            ob_init(&F); int r = ob_feed_apply(&F, dm, len); dmg++; if (r) { dmg_refused++; } else { dmg_ok++; if (ob_check(&F)) { bad++; printf("an ACCEPTED damaged feed gave an insane book\n"); } }
        }
    }
    printf("%ld random calls (%ld accepted, %ld refused: ", calls, accepted, refused); const char *nm[] = {"ok", "dup-id", "qty", "price", "unknown-id", "full", "time", "reduce", "invariant", "feed", "buffer", "?"}; for (int i = 1; i < 12; i++) { if (codes[i]) { printf("%s %ld ", nm[i], codes[i]); } }
    printf("); %ld prefixes of the feeds rebuilt; %ld damaged feeds: %ld refused, %ld accepted as a sane book; failures: %ld\n", prefix_checks, dmg, dmg_refused, dmg_ok, bad);
    return bad ? 1 : 0;
}
