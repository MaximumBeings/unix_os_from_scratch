/* Chapter 47: the marketplace. See 047_market.h for the design rules. */
#include "047_market.h"
#include "047_sha256.h"

static void zero_bytes(void *p, uint32_t n) {
    uint8_t *b = (uint8_t *)p;
    for (uint32_t i = 0; i < n; i++) {
        b[i] = 0;
    }
}

void mkt_init(market_t *m) {
    zero_bytes(m, sizeof(*m));
    auc_init(&m->auc);
    led_init(&m->led);
}

mkt_order_t *mkt_find_order(market_t *m, uint32_t listing) {
    for (uint32_t i = 0; i < MKT_MAX_ORDERS; i++) {
        if (m->orders[i].used && m->orders[i].listing == listing) {
            return &m->orders[i];
        }
    }
    return 0;
}

/* ---- command serialisation: fixed 92 bytes, every integer little-endian, so the log does not depend on how the compiler pads a structure ---- */
static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void mkt_cmd_encode(const mkt_cmd_t *c, uint8_t out[WAL_PAYLOAD]) {
    put32(out, c->type);
    put32(out + 4, c->now);
    put32(out + 8, c->idem);
    for (uint32_t i = 0; i < 8; i++) {
        put32(out + 12 + 4 * i, c->a[i]);
    }
    for (uint32_t i = 0; i < AUC_TITLE_MAX; i++) {
        out[44 + i] = (uint8_t)c->title[i];
    }
}

int mkt_cmd_decode(const uint8_t in[WAL_PAYLOAD], mkt_cmd_t *c) {
    c->type = get32(in);
    c->now = get32(in + 4);
    c->idem = get32(in + 8);
    for (uint32_t i = 0; i < 8; i++) {
        c->a[i] = get32(in + 12 + 4 * i);
    }
    for (uint32_t i = 0; i < AUC_TITLE_MAX; i++) {
        c->title[i] = (char)in[44 + i];
    }
    c->title[AUC_TITLE_MAX - 1] = 0;
    return (c->type >= CMD_CREATE && c->type <= CMD_REFUND) ? 0 : 1;
}

/* FNV-1a over the request, so the same idempotency key with a different request is detectable. */
static uint32_t fingerprint(const mkt_cmd_t *c) {
    uint32_t h = 2166136261u;
    uint32_t words[10];
    words[0] = c->type;
    for (uint32_t i = 0; i < 8; i++) {
        words[1 + i] = c->a[i];
    }
    for (uint32_t i = 0; i < 9; i++) {
        for (int b = 0; b < 4; b++) {
            h ^= (words[i] >> (8 * b)) & 0xFFu;
            h *= 16777619u;
        }
    }
    return h;
}

static int led_code(int led_rc) {
    return led_rc == LED_ERR_FUNDS ? MKT_ERR_FUNDS : 10 + led_rc;
}

static void do_apply(market_t *m, const mkt_cmd_t *c, mkt_result_t *r) {
    r->code = MKT_OK;
    r->v1 = r->v2 = 0;
    switch (c->type) {
    case CMD_CREATE: {
        int rc = auc_create(&m->auc, c->a[0], c->a[1], c->title, c->a[2], c->a[3], c->now, c->a[4], c->a[5], c->a[6]);
        r->code = (uint32_t)rc;
        r->v1 = c->a[0];
        break;
    }
    case CMD_BID: {
        uint32_t price = 0;
        int high = 0;
        int rc = auc_bid(&m->auc, c->a[0], c->a[1], c->a[2], c->now, &price, &high);
        r->code = (uint32_t)rc;
        if (rc == AUC_OK) {
            r->v1 = price;
            r->v2 = (uint32_t)high;
        }
        break;
    }
    case CMD_CLOSE: {
        int rc = auc_close(&m->auc, c->a[0], c->now);
        r->code = (uint32_t)rc;
        auc_listing_t *l = auc_find(&m->auc, c->a[0]);
        if (l) {
            r->v1 = l->status;
        }
        break;
    }
    case CMD_DEPOSIT: {
        int rc = led_deposit(&m->led, c->a[0], c->a[1]);
        r->code = rc == LED_OK ? MKT_OK : (uint32_t)led_code(rc);
        if (rc == LED_OK) {
            r->v1 = (uint32_t)m->led.bal[c->a[0]];
        }
        break;
    }
    case CMD_CHECKOUT: {
        auc_listing_t *l = auc_find(&m->auc, c->a[0]);
        if (!l) {
            r->code = AUC_ERR_NO_LISTING;
        } else if (l->status != AUC_SOLD) {
            r->code = MKT_ERR_NOT_SOLD;
        } else if (c->a[1] != l->high_bidder) {
            r->code = MKT_ERR_NOT_WINNER;
        } else if (mkt_find_order(m, l->id)) {
            r->code = MKT_ERR_ORDER_EXISTS;
        } else {
            mkt_order_t *o = 0;
            for (uint32_t i = 0; i < MKT_MAX_ORDERS && !o; i++) {
                if (!m->orders[i].used) {
                    o = &m->orders[i];
                }
            }
            if (!o) {
                r->code = MKT_ERR_ORDERS_FULL;
                break;
            }
            uint32_t total = l->price_cents + c->a[2];
            int rc = led_transfer(&m->led, c->a[1], LED_ESCROW, total);
            if (rc != LED_OK) {
                r->code = (uint32_t)led_code(rc);
                break;
            }
            o->used = 1;
            o->id = l->id;
            o->listing = l->id;
            o->buyer = c->a[1];
            o->seller = l->seller;
            o->item_cents = l->price_cents;
            o->ship_cents = c->a[2];
            o->total_cents = total;
            o->fee_cents = 0;
            o->status = ORD_PAID;
            r->v1 = total;
        }
        break;
    }
    case CMD_RELEASE: {
        mkt_order_t *o = mkt_find_order(m, c->a[0]);
        if (!o) {
            r->code = MKT_ERR_NO_ORDER;
        } else if (o->status != ORD_PAID) {
            r->code = MKT_ERR_ORDER_STATE;
        } else {
            uint32_t fee = led_fee(o->total_cents);
            if (fee > o->total_cents) {
                fee = o->total_cents;
            }
            led_transfer(&m->led, LED_ESCROW, LED_FEES, fee);
            if (o->total_cents - fee > 0) {
                led_transfer(&m->led, LED_ESCROW, o->seller, o->total_cents - fee);
            }
            o->fee_cents = fee;
            o->status = ORD_RELEASED;
            r->v1 = o->total_cents - fee;
            r->v2 = fee;
        }
        break;
    }
    case CMD_REFUND: {
        mkt_order_t *o = mkt_find_order(m, c->a[0]);
        if (!o) {
            r->code = MKT_ERR_NO_ORDER;
        } else if (o->status != ORD_PAID) {
            r->code = MKT_ERR_ORDER_STATE;
        } else {
            led_transfer(&m->led, LED_ESCROW, o->buyer, o->total_cents);
            o->status = ORD_REFUNDED;
            r->v1 = o->total_cents;
        }
        break;
    }
    default:
        r->code = MKT_ERR_BAD_CMD;
    }
}

static int needs_idem(uint32_t type) {
    return type == CMD_BID || type == CMD_DEPOSIT || type == CMD_CHECKOUT;
}

void mkt_apply(market_t *m, const mkt_cmd_t *c, mkt_result_t *r) {
    r->replayed = 0;
    if (needs_idem(c->type) && c->idem != 0) {
        uint32_t fp = fingerprint(c);
        for (uint32_t i = 0; i < MKT_MAX_IDEM; i++) {
            if (m->idem[i].used && m->idem[i].key == c->idem) {
                if (m->idem[i].fingerprint != fp) {
                    r->code = MKT_ERR_IDEM_MISMATCH;
                    r->v1 = r->v2 = 0;
                } else {
                    r->code = m->idem[i].result.code;
                    r->v1 = m->idem[i].result.v1;
                    r->v2 = m->idem[i].result.v2;
                    r->replayed = 1;
                }
                m->applied++;
                return;
            }
        }
        mkt_idem_t *slot = 0;
        for (uint32_t i = 0; i < MKT_MAX_IDEM && !slot; i++) {
            if (!m->idem[i].used) {
                slot = &m->idem[i];
            }
        }
        if (!slot) {
            /* A full table must never silently stop protecting retries, and it must not apply the command either: check for room BEFORE applying, refuse if there is none. */
            r->code = MKT_ERR_IDEM_FULL;
            r->v1 = r->v2 = 0;
            m->applied++;
            return;
        }
        do_apply(m, c, r);
        slot->used = 1;
        slot->key = c->idem;
        slot->fingerprint = fp;
        slot->result.code = r->code;
        slot->result.v1 = r->v1;
        slot->result.v2 = r->v2;
        slot->result.replayed = 0;
    } else {
        do_apply(m, c, r);
    }
    m->applied++;
    if (mkt_check(m) != 0) {
        m->broken = 1;
    }
}

int mkt_submit(market_t *m, const mkt_cmd_t *c, wal_writer_t w, mkt_result_t *r) {
    uint8_t payload[WAL_PAYLOAD], rec[WAL_RECORD];
    mkt_cmd_encode(c, payload);
    wal_encode(m->wal_seq + 1, payload, rec);
    if (w(m->wal_seq + 1, rec) != 0) {
        return 1;
    }
    m->wal_seq++;
    mkt_apply(m, c, r);
    return 0;
}

uint32_t mkt_replay(market_t *m, wal_reader_t rd, int *stop_code) {
    uint8_t buf[WAL_RECORD + 16], payload[WAL_PAYLOAD];
    uint32_t n = 0;
    for (;;) {
        uint32_t len = 0;
        int rc = rd(m->wal_seq + 1, buf, sizeof(buf), &len);
        if (rc != 0) {
            *stop_code = MKT_LOG_END; /* no such record: the log simply ends here */
            return n;
        }
        rc = wal_decode(buf, len, m->wal_seq + 1, payload);
        if (rc != WAL_OK) {
            *stop_code = rc;
            return n;
        }
        mkt_cmd_t c;
        if (mkt_cmd_decode(payload, &c) != 0) {
            *stop_code = WAL_ERR_LEN;
            return n;
        }
        mkt_result_t r;
        m->wal_seq++;
        mkt_apply(m, &c, &r);
        n++;
    }
}

/* ---- canonical state hash ---- */
static void h32(sha256_ctx_t *s, uint32_t v) {
    uint8_t b[4];
    put32(b, v);
    sha256_update(s, b, 4);
}

void mkt_hash(const market_t *m, uint8_t out[32]) {
    sha256_ctx_t s;
    sha256_init(&s);
    for (uint32_t i = 0; i < AUC_MAX_LISTINGS; i++) {
        const auc_listing_t *l = &m->auc.listings[i];
        h32(&s, l->used);
        if (!l->used) {
            continue;
        }
        h32(&s, l->id); h32(&s, l->seller);
        sha256_update(&s, (const uint8_t *)l->title, AUC_TITLE_MAX);
        h32(&s, l->start_cents); h32(&s, l->reserve_cents); h32(&s, l->start_time); h32(&s, l->end_time);
        h32(&s, l->extend_window); h32(&s, l->extend_secs); h32(&s, l->status); h32(&s, l->price_cents);
        h32(&s, l->high_bidder); h32(&s, l->high_max); h32(&s, l->bid_count); h32(&s, l->bid_seq); h32(&s, l->nproxies);
        for (uint32_t k = 0; k < l->nproxies; k++) {
            h32(&s, l->proxies[k].bidder); h32(&s, l->proxies[k].max_cents); h32(&s, l->proxies[k].seq);
        }
    }
    for (uint32_t i = 0; i < LED_MAX_ACCTS; i++) {
        h32(&s, (uint32_t)m->led.bal[i]);
    }
    h32(&s, m->led.n_tx);
    for (uint32_t i = 0; i < MKT_MAX_ORDERS; i++) {
        const mkt_order_t *o = &m->orders[i];
        h32(&s, o->used);
        if (o->used) {
            h32(&s, o->id); h32(&s, o->listing); h32(&s, o->buyer); h32(&s, o->seller); h32(&s, o->item_cents);
            h32(&s, o->ship_cents); h32(&s, o->total_cents); h32(&s, o->fee_cents); h32(&s, o->status);
        }
    }
    for (uint32_t i = 0; i < MKT_MAX_IDEM; i++) {
        const mkt_idem_t *e = &m->idem[i];
        h32(&s, e->used);
        if (e->used) {
            h32(&s, e->key); h32(&s, e->fingerprint); h32(&s, e->result.code); h32(&s, e->result.v1); h32(&s, e->result.v2);
        }
    }
    h32(&s, m->applied);
    sha256_final(&s, out);
}

int mkt_check(const market_t *m) {
    if (led_total(&m->led) != 0) {
        return 1;
    }
    uint32_t escrow_due = 0;
    for (uint32_t i = 0; i < MKT_MAX_ORDERS; i++) {
        if (m->orders[i].used && m->orders[i].status == ORD_PAID) {
            escrow_due += m->orders[i].total_cents;
        }
    }
    if (m->led.bal[LED_ESCROW] != (int32_t)escrow_due) {
        return 2;
    }
    for (uint32_t i = LED_FIRST_USER; i < LED_MAX_ACCTS; i++) {
        if (m->led.bal[i] < 0) {
            return 3;
        }
    }
    if (m->led.bal[LED_ESCROW] < 0 || m->led.bal[LED_FEES] < 0) {
        return 3;
    }
    for (uint32_t i = 0; i < AUC_MAX_LISTINGS; i++) {
        const auc_listing_t *l = &m->auc.listings[i];
        if (!l->used) {
            continue;
        }
        if (l->price_cents < l->start_cents) {
            return 4;
        }
        if (l->nproxies > 0) {
            uint32_t mx = 0;
            for (uint32_t k = 0; k < l->nproxies; k++) {
                if (l->proxies[k].max_cents > mx) {
                    mx = l->proxies[k].max_cents;
                }
            }
            if (l->high_max != mx || l->price_cents > l->high_max) {
                return 5;
            }
        }
        if (l->status == AUC_SOLD && l->bid_count == 0) {
            return 6;
        }
    }
    return 0;
}
