/* Chapter 47: the auction engine. See 049_auction.h for the rules and where they come from. */
#include "049_auction.h"

/* eBay's US bid-increment table: for a current price in [from, next row's from), the increment. DATA, so the one place to change it. Prices in cents. */
static const struct { uint32_t from, inc; } g_inc_table[] = {
    {0u, 5u},          /* $0.01 - $0.99    -> $0.05 */
    {100u, 25u},       /* $1.00 - $4.99    -> $0.25 */
    {500u, 50u},       /* $5.00 - $24.99   -> $0.50 */
    {2500u, 100u},     /* $25.00 - $99.99  -> $1.00 */
    {10000u, 250u},    /* $100.00 - $249.99 -> $2.50 */
    {25000u, 500u},    /* $250.00 - $499.99 -> $5.00 */
    {50000u, 1000u},   /* $500.00 - $999.99 -> $10.00 */
    {100000u, 2500u},  /* $1000.00 - $2499.99 -> $25.00 */
    {250000u, 5000u},  /* $2500.00 - $4999.99 -> $50.00 */
    {500000u, 10000u}, /* $5000.00 and up  -> $100.00 */
};

uint32_t auc_increment(uint32_t price_cents) {
    uint32_t inc = g_inc_table[0].inc;
    for (uint32_t i = 0; i < sizeof(g_inc_table) / sizeof(g_inc_table[0]); i++) {
        if (price_cents >= g_inc_table[i].from) {
            inc = g_inc_table[i].inc;
        }
    }
    return inc;
}

static void zero_bytes(void *p, uint32_t n) {
    uint8_t *b = (uint8_t *)p;
    for (uint32_t i = 0; i < n; i++) {
        b[i] = 0;
    }
}

void auc_init(auc_t *a) {
    zero_bytes(a, sizeof(*a));
}

auc_listing_t *auc_find(auc_t *a, uint32_t id) {
    for (uint32_t i = 0; i < AUC_MAX_LISTINGS; i++) {
        if (a->listings[i].used && a->listings[i].id == id) {
            return &a->listings[i];
        }
    }
    return 0;
}

int auc_create(auc_t *a, uint32_t id, uint32_t seller, const char *title, uint32_t start_cents, uint32_t reserve_cents, uint32_t now, uint32_t duration_secs,
               uint32_t extend_window, uint32_t extend_secs) {
    if (start_cents == 0 || duration_secs == 0 || id == 0 || seller == 0) {
        return AUC_ERR_BAD_ARG;
    }
    if (reserve_cents != 0 && reserve_cents < start_cents) {
        return AUC_ERR_BAD_ARG; /* a reserve below the starting price is meaningless */
    }
    if (auc_find(a, id)) {
        return AUC_ERR_EXISTS;
    }
    for (uint32_t i = 0; i < AUC_MAX_LISTINGS; i++) {
        auc_listing_t *l = &a->listings[i];
        if (l->used) {
            continue;
        }
        zero_bytes(l, sizeof(*l));
        l->used = 1;
        l->id = id;
        l->seller = seller;
        uint32_t k = 0;
        while (title[k] && k < AUC_TITLE_MAX - 1) {
            l->title[k] = title[k];
            k++;
        }
        l->start_cents = start_cents;
        l->reserve_cents = reserve_cents;
        l->start_time = now;
        l->end_time = now + duration_secs;
        l->extend_window = extend_window;
        l->extend_secs = extend_secs;
        l->status = AUC_ACTIVE;
        l->price_cents = start_cents;
        return AUC_OK;
    }
    return AUC_ERR_FULL;
}

/* Is proxy `x` ahead of proxy `y`? Higher maximum wins; equal maxima: the one that reached it earlier (smaller seq). */
static int ahead(const auc_proxy_t *x, const auc_proxy_t *y) {
    if (x->max_cents != y->max_cents) {
        return x->max_cents > y->max_cents;
    }
    return x->seq < y->seq;
}

/* Recompute leader and price from the standing maxima. The ONLY place the price is decided. */
static void auc_recompute(auc_listing_t *l) {
    if (l->nproxies == 0) {
        l->high_bidder = 0;
        l->high_max = 0;
        l->price_cents = l->start_cents;
        return;
    }
    uint32_t hi = 0;
    for (uint32_t i = 1; i < l->nproxies; i++) {
        if (ahead(&l->proxies[i], &l->proxies[hi])) {
            hi = i;
        }
    }
    int have_second = 0;
    uint32_t second = 0;
    for (uint32_t i = 0; i < l->nproxies; i++) {
        if (i == hi) {
            continue;
        }
        if (!have_second || ahead(&l->proxies[i], &l->proxies[second])) {
            second = i;
            have_second = 1;
        }
    }
    l->high_bidder = l->proxies[hi].bidder;
    l->high_max = l->proxies[hi].max_cents;
    uint32_t price = l->start_cents;
    if (have_second) {
        uint32_t sm = l->proxies[second].max_cents;
        uint32_t want = sm + auc_increment(sm);
        if (want > l->high_max) {
            want = l->high_max; /* never more than the leader's own maximum (this is also the equal-maxima case) */
        }
        if (want > price) {
            price = want;
        }
    }
    if (l->reserve_cents != 0 && l->high_max >= l->reserve_cents && price < l->reserve_cents) {
        price = l->reserve_cents; /* meeting the reserve lifts the price to the reserve */
    }
    l->price_cents = price;
}

uint32_t auc_min_bid(const auc_listing_t *l) {
    if (l->bid_count == 0) {
        return l->start_cents;
    }
    return l->price_cents + auc_increment(l->price_cents);
}

int auc_reserve_met(const auc_listing_t *l) {
    return l->reserve_cents == 0 || (l->bid_count > 0 && l->high_max >= l->reserve_cents);
}

uint32_t auc_proxy_of(const auc_listing_t *l, uint32_t bidder) {
    for (uint32_t i = 0; i < l->nproxies; i++) {
        if (l->proxies[i].bidder == bidder) {
            return l->proxies[i].max_cents;
        }
    }
    return 0;
}

int auc_bid(auc_t *a, uint32_t id, uint32_t bidder, uint32_t max_cents, uint32_t now, uint32_t *out_price, int *out_is_high) {
    auc_listing_t *l = auc_find(a, id);
    if (!l) {
        return AUC_ERR_NO_LISTING;
    }
    if (bidder == 0 || max_cents == 0) {
        return AUC_ERR_BAD_ARG;
    }
    if (l->status != AUC_ACTIVE || now >= l->end_time) {
        return AUC_ERR_ENDED;
    }
    if (bidder == l->seller) {
        return AUC_ERR_SELLER;
    }
    uint32_t own = auc_proxy_of(l, bidder);
    if (own != 0) {
        if (max_cents <= own) {
            return AUC_ERR_LOWER_OWN; /* a standing maximum can only be raised */
        }
        if (bidder != l->high_bidder && max_cents < auc_min_bid(l)) {
            return AUC_ERR_TOO_LOW;
        }
    } else {
        if (max_cents < auc_min_bid(l)) {
            return AUC_ERR_TOO_LOW;
        }
        if (l->nproxies >= AUC_MAX_BIDDERS) {
            return AUC_ERR_FULL;
        }
    }
    l->bid_seq++;
    if (own != 0) {
        for (uint32_t i = 0; i < l->nproxies; i++) {
            if (l->proxies[i].bidder == bidder) {
                l->proxies[i].max_cents = max_cents;
                l->proxies[i].seq = l->bid_seq;
            }
        }
    } else {
        l->proxies[l->nproxies].bidder = bidder;
        l->proxies[l->nproxies].max_cents = max_cents;
        l->proxies[l->nproxies].seq = l->bid_seq;
        l->nproxies++;
    }
    l->bid_count++;
    auc_recompute(l);
    if (l->extend_secs != 0 && now + l->extend_window >= l->end_time && now + l->extend_secs > l->end_time) {
        l->end_time = now + l->extend_secs; /* optional anti-sniping extension; off (0) reproduces eBay's hard close */
    }
    if (out_price) {
        *out_price = l->price_cents;
    }
    if (out_is_high) {
        *out_is_high = (l->high_bidder == bidder);
    }
    return AUC_OK;
}

int auc_close(auc_t *a, uint32_t id, uint32_t now) {
    auc_listing_t *l = auc_find(a, id);
    if (!l) {
        return AUC_ERR_NO_LISTING;
    }
    if (l->status != AUC_ACTIVE) {
        return AUC_OK; /* closing twice is harmless: the second close changes nothing */
    }
    if (now < l->end_time) {
        return AUC_ERR_NOT_ENDED;
    }
    if (l->bid_count == 0) {
        l->status = AUC_NO_BIDS;
    } else if (!auc_reserve_met(l)) {
        l->status = AUC_RESERVE_NOT_MET;
    } else {
        l->status = AUC_SOLD;
    }
    return AUC_OK;
}
