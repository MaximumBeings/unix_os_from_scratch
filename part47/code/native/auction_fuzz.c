/* Chapter 47, host-side differential test (NOT part of the kernel): runs 047_auction.c on thousands of pseudo-random auctions and prints every result, one line per event,
 * in a format auction_ref.py reproduces with an independent implementation. Build and run:  gcc -O1 -fsanitize=address,undefined -I.. auction_fuzz.c ../047_auction.c -o fuzz && ./fuzz N */
#include <stdio.h>
#include <stdlib.h>
#include "047_auction.h"

static uint32_t rng_state;
static uint32_t rnd(void) { /* xorshift32 (Marsaglia 2003); the Python reference uses the same constants */
    uint32_t x = rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    rng_state = x;
    return x;
}

int main(int argc, char **argv) {
    uint32_t n = argc > 1 ? (uint32_t)atoi(argv[1]) : 1000;
    for (uint32_t s = 0; s < n; s++) {
        rng_state = 2463534242u + s * 2654435761u;
        if (rng_state == 0) rng_state = 1;
        auc_t a; auc_init(&a);
        uint32_t start = 1 + rnd() % 20000, reserve = (rnd() % 2) ? start + rnd() % 60000 : 0;
        uint32_t ext_on = (rnd() % 4) == 0, win = 1 + rnd() % 300, esecs = 1 + rnd() % 300, dur = 600 + rnd() % 2400;
        if (!ext_on) { win = 0; esecs = 0; }
        int rc = auc_create(&a, 100, 3, "x", start, reserve, 0, dur, win, esecs);
        printf("S%u start=%u reserve=%u win=%u ext=%u dur=%u create=%d\n", s, start, reserve, win, esecs, dur, rc);
        uint32_t t = 0, nev = (rnd() % 6 == 0) ? rnd() % 3 : 12 + rnd() % 24, maxes[16], nm = 0;
        for (uint32_t e = 0; e < nev; e++) {
            t += rnd() % (dur / (nev ? nev : 1) + 1);  /* spread the events over the auction's own duration */
            uint32_t bidder = 3 + rnd() % 6;  /* 3 is the seller: tests the seller rule */
            uint32_t m;
            uint32_t mode = rnd() % 10;
            if (mode == 0 && nm > 0) m = maxes[rnd() % nm];             /* exactly an existing maximum: ties */
            else if (mode == 1) m = auc_min_bid(auc_find(&a, 100));     /* exactly the minimum bid */
            else if (mode == 3 && reserve) m = reserve;                 /* exactly the reserve price */
            else {
                uint32_t endt = auc_find(&a, 100)->end_time;
                if (mode == 2 && win && endt >= win && endt - win >= t) t = endt - win;  /* exactly at the edge of the anti-sniping window */
                if (mode == 4 && endt >= 1 && endt - 1 >= t) t = endt - 1;                /* the last second before the end */
                m = start / 2 + rnd() % 60000;
            }
            uint32_t price = 0; int is_high = 0;
            rc = auc_bid(&a, 100, bidder, m, t, &price, &is_high);
            if (rc == AUC_OK && nm < 16) maxes[nm++] = m;
            auc_listing_t *l = auc_find(&a, 100);
            printf("b t=%u who=%u max=%u rc=%d price=%u high=%d lead=%u end=%u cnt=%u\n", t, bidder, m, rc, rc == AUC_OK ? price : 0, rc == AUC_OK ? is_high : 0, l->high_bidder, l->end_time, l->bid_count);
        }
        auc_listing_t *l = auc_find(&a, 100);
        uint32_t ct = l->end_time + (rnd() % 3);
        int early = auc_close(&a, 100, l->end_time - 1);
        int rc2 = auc_close(&a, 100, ct);
        printf("c early=%d rc=%d status=%u price=%u winner=%u cnt=%u\n", early, rc2, l->status, l->price_cents, l->status == AUC_SOLD ? l->high_bidder : 0, l->bid_count);
    }
    return 0;
}
