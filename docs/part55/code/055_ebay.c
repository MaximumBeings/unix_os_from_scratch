/* Chapter 47: eBay-shaped REST messages. See 055_ebay.h for which shapes are real and which are this chapter's own. */
#include "055_ebay.h"

/* ---- a bounded string writer: on overflow it latches `over` and the builder returns 0, never writing past `cap` ---- */
typedef struct { char *p; uint32_t cap, n; int over; } jw_t;
static void jw_ch(jw_t *w, char c) {
    if (w->n + 1 >= w->cap) {
        w->over = 1;
        return;
    }
    w->p[w->n++] = c;
}
static void jw_str(jw_t *w, const char *s) {
    while (*s) {
        jw_ch(w, *s++);
    }
}
static void jw_u32(jw_t *w, uint32_t v) {
    char t[11];
    int k = 0;
    do {
        t[k++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (k) {
        jw_ch(w, t[--k]);
    }
}
static void jw_pad2(jw_t *w, uint32_t v) {
    jw_ch(w, (char)('0' + (v / 10u) % 10u));
    jw_ch(w, (char)('0' + v % 10u));
}
static void jw_cents(jw_t *w, uint32_t c) { /* Amount.value is a decimal STRING: "45.00" */
    jw_u32(w, c / 100u);
    jw_ch(w, '.');
    jw_pad2(w, c % 100u);
}
static void jw_amount(jw_t *w, uint32_t cents) {
    jw_str(w, "{\"currency\":\"USD\",\"value\":\"");
    jw_cents(w, cents);
    jw_str(w, "\"}");
}
static uint32_t jw_done(jw_t *w) {
    if (w->over || w->n >= w->cap) {
        return 0;
    }
    w->p[w->n] = 0;
    return w->n;
}
#define JW(buf, capv) jw_t w; w.p = (buf); w.cap = (capv); w.n = 0; w.over = 0

/* ---- time: fictional epoch 2026-10-10T00:00:00Z + whole seconds, as ISO-8601 (Howard Hinnant's public-domain civil-date algorithms, 32-bit) ---- */
static int32_t days_from_civil(int32_t y, uint32_t m, uint32_t d) {
    y -= m <= 2;
    int32_t era = (y >= 0 ? y : y - 399) / 400;
    uint32_t yoe = (uint32_t)(y - era * 400);
    uint32_t doy = (153u * (m + (m > 2 ? (uint32_t)-3 : 9u)) + 2u) / 5u + d - 1u;
    uint32_t doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + (int32_t)doe - 719468;
}
static void civil_from_days(int32_t z, int32_t *y, uint32_t *m, uint32_t *d) {
    z += 719468;
    int32_t era = (z >= 0 ? z : z - 146096) / 146097;
    uint32_t doe = (uint32_t)(z - era * 146097);
    uint32_t yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    *y = (int32_t)yoe + era * 400;
    uint32_t doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    uint32_t mp = (5u * doy + 2u) / 153u;
    *d = doy - (153u * mp + 2u) / 5u + 1u;
    *m = mp < 10u ? mp + 3u : mp - 9u;
    *y += (*m <= 2u);
}
void ebay_iso_time(uint32_t secs, char out[28]) {
    int32_t days = days_from_civil(2026, 10, 10) + (int32_t)(secs / 86400u);
    uint32_t sod = secs % 86400u, m, d;
    int32_t y;
    civil_from_days(days, &y, &m, &d);
    jw_t w; w.p = out; w.cap = 28; w.n = 0; w.over = 0;
    jw_u32(&w, (uint32_t)y); jw_ch(&w, '-'); jw_pad2(&w, m); jw_ch(&w, '-'); jw_pad2(&w, d); jw_ch(&w, 'T');
    jw_pad2(&w, sod / 3600u); jw_ch(&w, ':'); jw_pad2(&w, (sod / 60u) % 60u); jw_ch(&w, ':'); jw_pad2(&w, sod % 60u); jw_str(&w, ".000Z");
    jw_done(&w);
}

/* ---- parsing ---- */
static int starts_with(const char *s, uint32_t n, const char *prefix) {
    uint32_t i = 0;
    while (prefix[i]) {
        if (i >= n || s[i] != prefix[i]) {
            return 0;
        }
        i++;
    }
    return 1;
}
static int parse_u32(const char *s, uint32_t n, uint32_t *out) {
    if (n == 0 || n > 9) {
        return 1;
    }
    uint32_t v = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return 1;
        }
        v = v * 10u + (uint32_t)(s[i] - '0');
    }
    *out = v;
    return 0;
}

int ebay_amount_cents(const char *s, uint32_t n, uint32_t *cents) {
    uint32_t i = 0, whole = 0, frac = 0, nd = 0, fd = 0;
    while (i < n && s[i] >= '0' && s[i] <= '9') {
        whole = whole * 10u + (uint32_t)(s[i] - '0');
        i++;
        if (++nd > 7) {
            return 1;
        }
    }
    if (nd == 0) {
        return 1;
    }
    if (i < n) {
        if (s[i] != '.') {
            return 1;
        }
        i++;
        while (i < n && s[i] >= '0' && s[i] <= '9') {
            frac = frac * 10u + (uint32_t)(s[i] - '0');
            i++;
            fd++;
        }
        if (fd == 0 || fd > 2 || i != n) {
            return 1;
        }
        if (fd == 1) {
            frac *= 10u;
        }
    }
    if (whole > 1000000u || (whole == 1000000u && frac != 0)) {
        return 1; /* above $1,000,000.00: the ledger's own per-transfer limit (055_ledger.h), enforced at the door too */
    }
    *cents = whole * 100u + frac;
    return 0;
}

int json_find(const char *buf, uint32_t len, const char *dotted_path, const char **val, uint32_t *vlen) {
    uint32_t pos = 0;
    const char *p = dotted_path;
    for (;;) {
        char key[32];
        uint32_t kl = 0;
        while (*p && *p != '.' && kl < 31) {
            key[kl++] = *p++;
        }
        key[kl] = 0;
        int last = (*p == 0);
        if (!last && *p != '.') {
            return 1;
        }
        if (!last) {
            p++;
        }
        /* find "key" followed by optional spaces and ':' at or after pos */
        int found = 0;
        while (pos + kl + 2 <= len) {
            if (buf[pos] == '"') {
                uint32_t j = 0;
                while (j < kl && pos + 1 + j < len && buf[pos + 1 + j] == key[j]) {
                    j++;
                }
                if (j == kl && pos + 1 + kl < len && buf[pos + 1 + kl] == '"') {
                    uint32_t q = pos + 2 + kl;
                    while (q < len && (buf[q] == ' ' || buf[q] == '\t' || buf[q] == '\n' || buf[q] == '\r')) {
                        q++;
                    }
                    if (q < len && buf[q] == ':') {
                        q++;
                        while (q < len && (buf[q] == ' ' || buf[q] == '\t' || buf[q] == '\n' || buf[q] == '\r')) {
                            q++;
                        }
                        pos = q;
                        found = 1;
                        break;
                    }
                }
            }
            pos++;
        }
        if (!found) {
            return 1;
        }
        if (last) {
            break;
        }
    }
    if (pos >= len) {
        return 1;
    }
    if (buf[pos] == '"') {
        uint32_t s = pos + 1, e = s;
        while (e < len && buf[e] != '"') {
            if (buf[e] == '\\') {
                return 1; /* escapes are out of scope */
            }
            e++;
        }
        if (e >= len) {
            return 1;
        }
        *val = buf + s;
        *vlen = e - s;
        return 0;
    }
    uint32_t e = pos;
    while (e < len && ((buf[e] >= '0' && buf[e] <= '9') || buf[e] == '.' || buf[e] == '-' || (buf[e] >= 'a' && buf[e] <= 'z'))) {
        e++;
    }
    if (e == pos) {
        return 1;
    }
    *val = buf + pos;
    *vlen = e - pos;
    return 0;
}

int ebay_parse_request(const uint8_t *buf, uint32_t len, http_req_t *out) {
    const char *s = (const char *)buf;
    uint32_t i = 0, k = 0;
    out->bidder = 0;
    out->idem = 0;
    while (i < len && s[i] != ' ' && k < 7) {
        out->method[k++] = s[i++];
    }
    out->method[k] = 0;
    if (i >= len || s[i] != ' ') {
        return 1;
    }
    i++;
    k = 0;
    while (i < len && s[i] != ' ' && k < 95) {
        out->path[k++] = s[i++];
    }
    out->path[k] = 0;
    while (i < len && s[i] != '\n') {
        i++;
    }
    i++;
    /* headers until a blank line */
    for (;;) {
        if (i >= len) {
            return 1;
        }
        uint32_t ls = i;
        while (i < len && s[i] != '\n') {
            i++;
        }
        uint32_t le = i;
        i++;
        if (le > ls && s[le - 1] == '\r') {
            le--;
        }
        if (le == ls) {
            break; /* blank line */
        }
        if (starts_with(s + ls, le - ls, "Authorization: Bearer demo-user-")) {
            uint32_t from = ls + 32;
            if (parse_u32(s + from, le - from, &out->bidder) != 0) {
                return 1;
            }
        } else if (starts_with(s + ls, le - ls, "Idempotency-Key: ")) {
            uint32_t from = ls + 17;
            if (parse_u32(s + from, le - from, &out->idem) != 0) {
                return 1;
            }
        }
    }
    out->body_off = i;
    return 0;
}

int ebay_path_item_id(const char *path, const char *after, uint32_t *legacy_id) {
    uint32_t n = 0;
    while (path[n]) {
        n++;
    }
    uint32_t al = 0;
    while (after[al]) {
        al++;
    }
    for (uint32_t i = 0; i + al + 3 <= n; i++) {
        if (starts_with(path + i, n - i, after)) {
            uint32_t j = i + al;
            if (path[j] != 'v' || path[j + 1] != '1' || path[j + 2] != '|') {
                return 1;
            }
            j += 3;
            uint32_t s = j;
            while (j < n && path[j] != '|') {
                j++;
            }
            if (j >= n || parse_u32(path + s, j - s, legacy_id) != 0) {
                return 1;
            }
            return 0;
        }
    }
    return 1;
}

static int copy_value(const char *v, uint32_t n, char *dst, uint32_t cap) {
    if (n + 1 > cap) {
        return 1;
    }
    for (uint32_t i = 0; i < n; i++) {
        dst[i] = v[i];
    }
    dst[n] = 0;
    return 0;
}
static int eq(const char *v, uint32_t n, const char *lit) {
    uint32_t i = 0;
    while (lit[i]) {
        if (i >= n || v[i] != lit[i]) {
            return 0;
        }
        i++;
    }
    return i == n;
}

int ebay_parse_proxy_bid_body(const char *body, uint32_t len, uint32_t *max_cents) {
    const char *v;
    uint32_t n;
    if (json_find(body, len, "maxAmount.currency", &v, &n) != 0 || !eq(v, n, "USD")) {
        return 1;
    }
    if (json_find(body, len, "maxAmount.value", &v, &n) != 0) {
        return 1;
    }
    return ebay_amount_cents(v, n, max_cents);
}

int ebay_parse_offer(const char *body, uint32_t len, offer_req_t *out) {
    const char *v;
    uint32_t n, days;
    if (json_find(body, len, "format", &v, &n) != 0 || !eq(v, n, "AUCTION")) {
        return 1;
    }
    if (json_find(body, len, "sku", &v, &n) != 0 || copy_value(v, n, out->sku, sizeof(out->sku)) != 0) {
        return 1;
    }
    if (json_find(body, len, "product.title", &v, &n) != 0 || copy_value(v, n, out->title, sizeof(out->title)) != 0) {
        return 1;
    }
    if (json_find(body, len, "listingDuration", &v, &n) != 0 || n < 6 || !starts_with(v, n, "DAYS_") || parse_u32(v + 5, n - 5, &days) != 0 || days == 0 || days > 10) {
        return 1;
    }
    out->duration_secs = days * 86400u;
    if (json_find(body, len, "pricingSummary.auctionStartPrice.value", &v, &n) != 0 || ebay_amount_cents(v, n, &out->start_cents) != 0) {
        return 1;
    }
    out->reserve_cents = 0;
    if (json_find(body, len, "pricingSummary.auctionReservePrice.value", &v, &n) == 0 && ebay_amount_cents(v, n, &out->reserve_cents) != 0) {
        return 1;
    }
    out->listing = 0;
    return 0;
}

/* ---- builders ---- */
uint32_t ebay_build_bid_request(char *out, uint32_t cap, uint32_t listing, uint32_t bidder, uint32_t idem, uint32_t max_cents) {
    JW(out, cap);
    jw_str(&w, "POST /buy/offer/v1_beta/bidding/v1|");
    jw_u32(&w, listing);
    jw_str(&w, "|0/place_proxy_bid HTTP/1.1\r\nAuthorization: Bearer demo-user-");
    jw_u32(&w, bidder);
    jw_str(&w, "\r\nIdempotency-Key: ");
    jw_u32(&w, idem);
    jw_str(&w, "\r\nContent-Type: application/json\r\n\r\n{\"maxAmount\":");
    jw_amount(&w, max_cents);
    jw_ch(&w, '}');
    return jw_done(&w);
}

uint32_t ebay_build_offer_request(char *out, uint32_t cap, const char *sku, const char *title, uint32_t days, uint32_t start_cents, uint32_t reserve_cents) {
    JW(out, cap);
    jw_str(&w, "{\"sku\":\"");
    jw_str(&w, sku);
    jw_str(&w, "\",\"marketplaceId\":\"EBAY_US\",\"format\":\"AUCTION\",\"listingDuration\":\"DAYS_");
    jw_u32(&w, days);
    jw_str(&w, "\",\"product\":{\"title\":\"");
    jw_str(&w, title);
    jw_str(&w, "\"},\"pricingSummary\":{\"auctionStartPrice\":");
    jw_amount(&w, start_cents);
    if (reserve_cents) {
        jw_str(&w, ",\"auctionReservePrice\":");
        jw_amount(&w, reserve_cents);
    }
    jw_str(&w, "}}");
    return jw_done(&w);
}

uint32_t ebay_build_bid_response(char *out, uint32_t cap, uint32_t listing, uint32_t bidder, uint32_t bid_no) {
    JW(out, cap);
    jw_str(&w, "{\"proxyBidId\":\"PB-");
    jw_u32(&w, listing);
    jw_ch(&w, '-');
    jw_u32(&w, bidder);
    jw_ch(&w, '-');
    jw_u32(&w, bid_no);
    jw_str(&w, "\"}");
    return jw_done(&w);
}

uint32_t ebay_build_error(char *out, uint32_t cap, uint32_t http_status, uint32_t error_id, const char *domain, const char *message) {
    JW(out, cap);
    jw_str(&w, "{\"status\":");
    jw_u32(&w, http_status);
    jw_str(&w, ",\"errors\":[{\"errorId\":");
    jw_u32(&w, error_id);
    jw_str(&w, ",\"domain\":\"");
    jw_str(&w, domain);
    jw_str(&w, "\",\"category\":\"REQUEST\",\"message\":\"");
    jw_str(&w, message);
    jw_str(&w, "\"}]}");
    return jw_done(&w);
}

uint32_t ebay_build_bidding(char *out, uint32_t cap, const auc_listing_t *l, uint32_t viewer) {
    JW(out, cap);
    char iso[28];
    ebay_iso_time(l->end_time, iso);
    jw_str(&w, "{\"itemId\":\"v1|");
    jw_u32(&w, l->id);
    jw_str(&w, "|0\",\"auctionStatus\":\"");
    jw_str(&w, l->status == AUC_ACTIVE ? "ACTIVE" : "ENDED");
    jw_str(&w, "\",\"auctionEndDate\":\"");
    jw_str(&w, iso);
    jw_str(&w, "\",\"bidCount\":");
    jw_u32(&w, l->bid_count);
    jw_str(&w, ",\"currentPrice\":");
    jw_amount(&w, l->price_cents);
    uint32_t mine = auc_proxy_of(l, viewer);
    if (mine) {
        jw_str(&w, ",\"currentProxyBid\":{\"maxAmount\":");
        jw_amount(&w, mine);
        jw_str(&w, ",\"proxyBidId\":\"PB-");
        jw_u32(&w, l->id);
        jw_ch(&w, '-');
        jw_u32(&w, viewer);
        jw_str(&w, "\"}");
    }
    jw_str(&w, ",\"highBidder\":");
    jw_str(&w, (mine && l->high_bidder == viewer) ? "true" : "false");
    if (l->reserve_cents) {
        jw_str(&w, ",\"reservePriceMet\":");
        jw_str(&w, auc_reserve_met(l) ? "true" : "false");
    }
    if (l->status == AUC_ACTIVE) {
        uint32_t s1 = auc_min_bid(l);
        jw_str(&w, ",\"suggestedBidAmounts\":[");
        jw_amount(&w, s1);
        jw_ch(&w, ',');
        jw_amount(&w, s1 + auc_increment(s1));
        jw_ch(&w, ']');
    }
    jw_ch(&w, '}');
    return jw_done(&w);
}

uint32_t ebay_build_item(char *out, uint32_t cap, const auc_listing_t *l) {
    JW(out, cap);
    char iso[28];
    uint32_t uniq = l->nproxies;
    ebay_iso_time(l->end_time, iso);
    jw_str(&w, "{\"itemId\":\"v1|");
    jw_u32(&w, l->id);
    jw_str(&w, "|0\",\"title\":\"");
    jw_str(&w, l->title);
    jw_str(&w, "\",\"buyingOptions\":[\"AUCTION\"],\"currentBidPrice\":");
    jw_amount(&w, l->price_cents);
    jw_str(&w, ",\"minimumPriceToBid\":");
    jw_amount(&w, auc_min_bid(l));
    jw_str(&w, ",\"bidCount\":");
    jw_u32(&w, l->bid_count);
    jw_str(&w, ",\"uniqueBidderCount\":");
    jw_u32(&w, uniq);
    jw_str(&w, ",\"itemEndDate\":\"");
    jw_str(&w, iso);
    jw_ch(&w, '"');
    if (l->reserve_cents) {
        jw_str(&w, ",\"reservePriceMet\":");
        jw_str(&w, auc_reserve_met(l) ? "true" : "false");
    }
    jw_ch(&w, '}');
    return jw_done(&w);
}

uint32_t ebay_build_order(char *out, uint32_t cap, const mkt_order_t *o, const char *title) {
    JW(out, cap);
    const char *st = o->status == ORD_REFUNDED ? "FULLY_REFUNDED" : "PAID"; /* the enum values are eBay's OrderPaymentStatusEnum as the author knows it; the OpenAPI files name the enum but do not list its values */
    jw_str(&w, "{\"orderId\":\"ORD-");
    jw_u32(&w, o->id);
    jw_str(&w, "\",\"orderPaymentStatus\":\"");
    jw_str(&w, st);
    jw_str(&w, "\",\"lineItems\":[{\"lineItemId\":\"LI-");
    jw_u32(&w, o->id);
    jw_str(&w, "\",\"title\":\"");
    jw_str(&w, title);
    jw_str(&w, "\",\"total\":");
    jw_amount(&w, o->item_cents);
    jw_str(&w, "}],\"pricingSummary\":{\"total\":");
    jw_amount(&w, o->total_cents);
    jw_str(&w, "},\"paymentSummary\":{\"totalDueSeller\":");
    jw_amount(&w, o->status == ORD_RELEASED ? o->total_cents - o->fee_cents : 0);
    jw_str(&w, ",\"payments\":[{\"paymentStatus\":\"PAID");
    jw_str(&w, "\",\"amount\":");
    jw_amount(&w, o->total_cents);
    jw_str(&w, "}]}}");
    return jw_done(&w);
}
