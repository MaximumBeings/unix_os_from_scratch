/* Chapter 51: the matching engine and the feed subscriber (see 052_book.h). Freestanding. */
#include "052_book.h"
#include "052_sha256.h"

const char *ob_strerror(int rc) {
    switch (rc) {
    case OB_OK: return "ok"; case OB_ERR_DUP_ID: return "dup-id"; case OB_ERR_QTY: return "qty"; case OB_ERR_PRICE: return "price"; case OB_ERR_UNKNOWN: return "unknown-id"; case OB_ERR_FULL: return "full";
    case OB_ERR_TIME: return "time"; case OB_ERR_REDUCE: return "reduce"; case OB_ERR_INVARIANT: return "invariant"; case OB_ERR_FEED: return "feed"; case OB_ERR_BUFFER: return "buffer";
    }
    return "?";
}
void ob_init(ob_t *b) {
    for (uint32_t i = 0; i < OB_MAX_ORDERS; i++) { b->o[i].live = 0; b->o[i].id = 0; b->o[i].price = b->o[i].qty = b->o[i].seq = 0; b->o[i].side = 0; }
    b->next_seq = 1; b->next_match = 1; b->last_ts = 0; b->n_trades = 0; b->volume_qty = 0; b->notional = 0; b->n_live = 0; b->n_tr = 0;
    b->feed = 0; b->feed_cap = 0; b->feed_len = 0; b->feed_msgs = 0; b->feed_err = ""; b->feed_err_msg = 0;
}
/* ---- publishing ---- */
static void p8(uint8_t *p, uint64_t v) { for (int i = 0; i < 8; i++) { p[i] = (uint8_t)(v >> (56 - 8 * i)); } }
static void p4(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) { p[i] = (uint8_t)(v >> (24 - 8 * i)); } }
static void p6(uint8_t *p, uint64_t v) { for (int i = 0; i < 6; i++) { p[i] = (uint8_t)(v >> (40 - 8 * i)); } }
static uint64_t g8(const uint8_t *p) { uint64_t v = 0; for (int i = 0; i < 8; i++) { v = v << 8 | p[i]; } return v; }
static uint32_t g4(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static uint64_t g6(const uint8_t *p) { uint64_t v = 0; for (int i = 0; i < 6; i++) { v = v << 8 | p[i]; } return v; }
static uint8_t *emit(ob_t *b, uint8_t type, uint16_t len, uint64_t ts) { /* header: 2-byte length, type, stock locate 1, tracking 0, 6-byte time; returns the body pointer after it or NULL */
    if (!b->feed) { static uint8_t scratch[64]; uint8_t *p = scratch; p[0] = 0; p[1] = (uint8_t)len; p[2] = type; p[3] = 0; p[4] = 1; p[5] = 0; p[6] = 0; p6(p + 7, ts); return p + 13; }
    if (b->feed_len + 2u + len > b->feed_cap) { return 0; }
    uint8_t *p = b->feed + b->feed_len; b->feed_len += 2u + len; b->feed_msgs++;
    p[0] = (uint8_t)(len >> 8); p[1] = (uint8_t)len; p[2] = type; p[3] = 0; p[4] = 1; p[5] = 0; p[6] = 0; p6(p + 7, ts); return p + 13;
}
static int pub_add(ob_t *b, uint64_t ts, uint64_t id, int side, uint32_t qty, uint32_t price) {
    uint8_t *p = emit(b, 'A', 36, ts); if (!p) { return OB_ERR_BUFFER; }
    p8(p, id); p[8] = side == OB_BUY ? 'B' : 'S'; p4(p + 9, qty); for (int i = 0; i < 8; i++) { p[13 + i] = "ACME    "[i]; } p4(p + 21, price); return OB_OK;
}
static int pub_exec(ob_t *b, uint64_t ts, uint64_t id, uint32_t qty, uint64_t match) { uint8_t *p = emit(b, 'E', 31, ts); if (!p) { return OB_ERR_BUFFER; } p8(p, id); p4(p + 8, qty); p8(p + 12, match); return OB_OK; }
static int pub_reduce(ob_t *b, uint64_t ts, uint64_t id, uint32_t qty) { uint8_t *p = emit(b, 'X', 23, ts); if (!p) { return OB_ERR_BUFFER; } p8(p, id); p4(p + 8, qty); return OB_OK; }
static int pub_delete(ob_t *b, uint64_t ts, uint64_t id) { uint8_t *p = emit(b, 'D', 19, ts); if (!p) { return OB_ERR_BUFFER; } p8(p, id); return OB_OK; }
static int pub_replace(ob_t *b, uint64_t ts, uint64_t old_id, uint64_t new_id, uint32_t qty, uint32_t price) { uint8_t *p = emit(b, 'U', 35, ts); if (!p) { return OB_ERR_BUFFER; } p8(p, old_id); p8(p + 8, new_id); p4(p + 16, qty); p4(p + 20, price); return OB_OK; }

static int find(const ob_t *b, uint64_t id) { for (uint32_t i = 0; i < OB_MAX_ORDERS; i++) { if (b->o[i].live && b->o[i].id == id) { return (int)i; } } return -1; }
static int free_slot(const ob_t *b) { for (uint32_t i = 0; i < OB_MAX_ORDERS; i++) { if (!b->o[i].live) { return (int)i; } } return -1; }
static int best(const ob_t *b, int side) { /* best resting order on `side`: bids highest price, asks lowest; ties earliest */
    int k = -1;
    for (uint32_t i = 0; i < OB_MAX_ORDERS; i++) {
        const ob_order_t *o = &b->o[i]; if (!o->live || o->side != side) { continue; }
        if (k < 0) { k = (int)i; continue; }
        const ob_order_t *c = &b->o[k]; int better = side == OB_BUY ? (o->price > c->price) : (o->price < c->price);
        if (better || (o->price == c->price && o->seq < c->seq)) { k = (int)i; }
    }
    return k;
}
static int crosses(const ob_t *b, int side, uint32_t price) { int k = best(b, side == OB_BUY ? OB_SELL : OB_BUY); if (k < 0) { return 0; } return price == 0 || (side == OB_BUY ? b->o[k].price <= price : b->o[k].price >= price); }
static int check_ts(ob_t *b, uint64_t ts) { if (ts > OB_MAX_TS || ts < b->last_ts) { return OB_ERR_TIME; } return OB_OK; }

static int match_and_rest(ob_t *b, uint64_t id, int side, uint32_t price, uint32_t qty, uint64_t ts) {
    uint32_t left = qty;
    while (left > 0) {
        int k = best(b, side == OB_BUY ? OB_SELL : OB_BUY); if (k < 0) { break; }
        ob_order_t *m = &b->o[k]; if (price != 0 && (side == OB_BUY ? m->price > price : m->price < price)) { break; }
        uint32_t q = left < m->qty ? left : m->qty; uint64_t mn = b->next_match++;
        ob_trade_t *t = &b->tr[b->n_tr++]; t->maker = m->id; t->taker = id; t->price = m->price; t->qty = q; t->match = mn;
        int rc = pub_exec(b, ts, m->id, q, mn); if (rc) { return rc; }
        m->qty -= q; left -= q; b->n_trades++; b->volume_qty += q; b->notional += (uint64_t)m->price * q;
        if (m->qty == 0) { m->live = 0; b->n_live--; }
    }
    if (left > 0 && price != 0) {
        int s = free_slot(b); if (s < 0) { return OB_ERR_FULL; }
        ob_order_t *o = &b->o[s]; o->id = id; o->price = price; o->qty = left; o->side = (uint8_t)side; o->live = 1; o->seq = b->next_seq++; b->n_live++;
        int rc = pub_add(b, ts, id, side, left, price); if (rc) { return rc; }
    }
    return OB_OK;
}
int ob_new(ob_t *b, uint64_t id, int side, uint32_t price, uint32_t qty, uint64_t ts) {
    b->n_tr = 0;
    if (qty == 0 || qty > OB_MAX_QTY) { return OB_ERR_QTY; }
    if (id == 0 || find(b, id) >= 0) { return OB_ERR_DUP_ID; }
    int rc = check_ts(b, ts); if (rc) { return rc; }
    if (price != 0 && free_slot(b) < 0 && !crosses(b, side, price)) { return OB_ERR_FULL; } /* a limit order that cannot trade and cannot rest */
    if (price != 0 && free_slot(b) < 0) { /* it will trade; it may then have a remainder with nowhere to rest: refuse up front unless it fills completely */
        uint32_t avail = 0; for (uint32_t i = 0; i < OB_MAX_ORDERS; i++) { const ob_order_t *o = &b->o[i]; if (o->live && o->side != side && (side == OB_BUY ? o->price <= price : o->price >= price)) { avail += o->qty > qty ? qty : o->qty; if (avail >= qty) { break; } } }
        if (avail < qty) { return OB_ERR_FULL; }
    }
    b->last_ts = ts; rc = match_and_rest(b, id, side, price, qty, ts); if (rc) { return rc; }
    return ob_check(b) ? OB_ERR_INVARIANT : OB_OK;
}
int ob_cancel(ob_t *b, uint64_t id, uint64_t ts) {
    b->n_tr = 0; int k = find(b, id); if (k < 0) { return OB_ERR_UNKNOWN; } int rc = check_ts(b, ts); if (rc) { return rc; }
    b->last_ts = ts; b->o[k].live = 0; b->n_live--; rc = pub_delete(b, ts, id); if (rc) { return rc; } return ob_check(b) ? OB_ERR_INVARIANT : OB_OK;
}
int ob_reduce(ob_t *b, uint64_t id, uint32_t cq, uint64_t ts) {
    b->n_tr = 0; int k = find(b, id); if (k < 0) { return OB_ERR_UNKNOWN; } if (cq == 0 || cq >= b->o[k].qty) { return OB_ERR_REDUCE; } int rc = check_ts(b, ts); if (rc) { return rc; }
    b->last_ts = ts; b->o[k].qty -= cq; rc = pub_reduce(b, ts, id, cq); if (rc) { return rc; } return ob_check(b) ? OB_ERR_INVARIANT : OB_OK;
}
int ob_replace(ob_t *b, uint64_t old_id, uint64_t new_id, uint32_t price, uint32_t qty, uint64_t ts) {
    b->n_tr = 0; int k = find(b, old_id); if (k < 0) { return OB_ERR_UNKNOWN; } if (qty == 0 || qty > OB_MAX_QTY) { return OB_ERR_QTY; } if (price == 0) { return OB_ERR_PRICE; }
    if (new_id == 0 || (new_id != old_id && find(b, new_id) >= 0)) { return OB_ERR_DUP_ID; } int rc = check_ts(b, ts); if (rc) { return rc; }
    int side = b->o[k].side; b->last_ts = ts;
    /* would the new order trade against the book (not counting the order being replaced, which is on the same side)? */
    if (crosses(b, side, price)) {
        b->o[k].live = 0; b->n_live--; rc = pub_delete(b, ts, old_id); if (rc) { return rc; }
        rc = match_and_rest(b, new_id, side, price, qty, ts); if (rc) { return rc; }
    } else {
        rc = pub_replace(b, ts, old_id, new_id, qty, price); if (rc) { return rc; }
        b->o[k].id = new_id; b->o[k].price = price; b->o[k].qty = qty; b->o[k].seq = b->next_seq++;
    }
    return ob_check(b) ? OB_ERR_INVARIANT : OB_OK;
}
int ob_check(const ob_t *b) {
    uint32_t live = 0; int bb = best(b, OB_BUY), ba = best(b, OB_SELL);
    for (uint32_t i = 0; i < OB_MAX_ORDERS; i++) { if (!b->o[i].live) { continue; } live++; if (b->o[i].qty == 0 || b->o[i].price == 0) { return 1; } for (uint32_t j = i + 1; j < OB_MAX_ORDERS; j++) { if (b->o[j].live && b->o[j].id == b->o[i].id) { return 2; } } }
    if (live != b->n_live) { return 3; }
    if (bb >= 0 && ba >= 0 && b->o[bb].price >= b->o[ba].price) { return 4; }
    return 0;
}
static int before(const ob_t *b, uint32_t x, uint32_t y) { const ob_order_t *a = &b->o[x], *c = &b->o[y]; if (a->side != c->side) { return a->side == OB_BUY; } if (a->price != c->price) { return a->side == OB_BUY ? a->price > c->price : a->price < c->price; } return a->seq < c->seq; }
void ob_hash(const ob_t *b, uint8_t out[32]) {
    static uint16_t idx[OB_MAX_ORDERS]; static uint8_t buf[OB_MAX_ORDERS * 17 + 4]; uint32_t n = 0;
    for (uint32_t i = 0; i < OB_MAX_ORDERS; i++) { if (!b->o[i].live) { continue; } uint32_t j = n++; while (j > 0 && before(b, i, idx[j - 1])) { idx[j] = idx[j - 1]; j--; } idx[j] = (uint16_t)i; }
    uint32_t o = 0; p4(buf, n); o = 4;
    for (uint32_t k = 0; k < n; k++) { const ob_order_t *x = &b->o[idx[k]]; p8(buf + o, x->id); p4(buf + o + 8, x->price); p4(buf + o + 12, x->qty); buf[o + 16] = x->side; o += 17; }
    sha256_hash(buf, o, out);
}
int ob_levels(const ob_t *b, int side, uint32_t *price, uint64_t *qty, int max) {
    static uint16_t idx[OB_MAX_ORDERS]; uint32_t n = 0; int lv = 0;
    for (uint32_t i = 0; i < OB_MAX_ORDERS; i++) { if (!b->o[i].live || b->o[i].side != side) { continue; } uint32_t j = n++; while (j > 0 && before(b, i, idx[j - 1])) { idx[j] = idx[j - 1]; j--; } idx[j] = (uint16_t)i; }
    for (uint32_t k = 0; k < n; k++) { const ob_order_t *x = &b->o[idx[k]]; if (lv > 0 && price[lv - 1] == x->price) { qty[lv - 1] += x->qty; } else if (lv < max) { price[lv] = x->price; qty[lv] = x->qty; lv++; } }
    return lv;
}
/* ---- the subscriber ---- */
#define FEED_FAIL(msg) do { b->feed_err = (msg); b->feed_err_msg = nmsg; return OB_ERR_FEED; } while (0)
int ob_feed_apply(ob_t *b, const uint8_t *buf, uint32_t len) {
    uint32_t pos = 0, nmsg = 0; b->feed_err = ""; b->feed_err_msg = 0;
    while (pos < len) {
        nmsg++; if (len - pos < 3) { FEED_FAIL("truncated length prefix"); }
        uint32_t ml = (uint32_t)buf[pos] << 8 | buf[pos + 1]; const uint8_t *m = buf + pos + 2; if (ml > len - pos - 2) { FEED_FAIL("message runs past the end of the feed"); }
        uint8_t t = m[0]; uint32_t want = t == 'A' ? 36 : t == 'E' ? 31 : t == 'X' ? 23 : t == 'D' ? 19 : t == 'U' ? 35 : t == 'S' ? 12 : t == 'R' ? 39 : t == 'H' ? 25 : 0;
        if (want == 0) { FEED_FAIL("unknown message type"); } if (ml != want) { FEED_FAIL("wrong length for the message type"); }
        pos += 2 + ml; if (t == 'S' || t == 'R' || t == 'H') { continue; }
        if (m[1] != 0 || m[2] != 1) { FEED_FAIL("stock locate is not 1"); } uint64_t ts = g6(m + 5); if (ts < b->last_ts) { FEED_FAIL("time goes backwards"); } b->last_ts = ts;
        if (t == 'A') {
            uint64_t id = g8(m + 11); uint8_t sd = m[19]; uint32_t q = g4(m + 20), pr = g4(m + 32); if (sd != 'B' && sd != 'S') { FEED_FAIL("side is not B or S"); } if (q == 0 || q > OB_MAX_QTY) { FEED_FAIL("add with a bad quantity"); } if (pr == 0) { FEED_FAIL("add with price 0"); }
            if (id == 0 || find(b, id) >= 0) { FEED_FAIL("add of an order id that is already live"); } for (int i = 0; i < 8; i++) { if (m[24 + i] != (uint8_t)"ACME    "[i]) { FEED_FAIL("unexpected stock symbol"); } }
            int s = free_slot(b); if (s < 0) { FEED_FAIL("order table full"); }
            ob_order_t *o = &b->o[s]; o->id = id; o->price = pr; o->qty = q; o->side = sd == 'B' ? OB_BUY : OB_SELL; o->live = 1; o->seq = b->next_seq++; b->n_live++;
        } else if (t == 'E') {
            int k = find(b, g8(m + 11)); uint32_t q = g4(m + 19); if (k < 0) { FEED_FAIL("execution of an unknown order"); } if (q == 0 || q > b->o[k].qty) { FEED_FAIL("execution larger than the remaining quantity"); }
            if (g8(m + 23) != b->next_match) { FEED_FAIL("match number out of sequence"); }
            b->next_match++; b->o[k].qty -= q; b->n_trades++; b->volume_qty += q; b->notional += (uint64_t)b->o[k].price * q; if (b->o[k].qty == 0) { b->o[k].live = 0; b->n_live--; }
        } else if (t == 'X') {
            int k = find(b, g8(m + 11)); uint32_t q = g4(m + 19); if (k < 0) { FEED_FAIL("reduce of an unknown order"); } if (q == 0 || q >= b->o[k].qty) { FEED_FAIL("reduce by zero or by the whole quantity"); } b->o[k].qty -= q;
        } else if (t == 'D') {
            int k = find(b, g8(m + 11)); if (k < 0) { FEED_FAIL("delete of an unknown order"); } b->o[k].live = 0; b->n_live--;
        } else { /* 'U' */
            int k = find(b, g8(m + 11)); uint64_t nid = g8(m + 19); uint32_t q = g4(m + 27), pr = g4(m + 31); if (k < 0) { FEED_FAIL("replace of an unknown order"); } if (q == 0 || q > OB_MAX_QTY || pr == 0) { FEED_FAIL("replace with a bad quantity or price"); }
            if (nid == 0 || (nid != b->o[k].id && find(b, nid) >= 0)) { FEED_FAIL("replace to an id that is already live"); } b->o[k].id = nid; b->o[k].qty = q; b->o[k].price = pr; b->o[k].seq = b->next_seq++;
        }
        if (ob_check(b)) { FEED_FAIL("the book is crossed or inconsistent after this message"); }
    }
    return OB_OK;
}
