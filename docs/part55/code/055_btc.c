/* Chapter 50: the Bitcoin context-free validator (see 055_btc.h). Freestanding: no string.h, no libgcc division. */
#include "055_btc.h"
#include "055_sha256.h"

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static int64_t rd64s(const uint8_t *p) { uint64_t lo = rd32(p), hi = rd32(p + 4); return (int64_t)(lo | hi << 32); }
static int same(const uint8_t *a, const uint8_t *b, uint32_t n) { for (uint32_t i = 0; i < n; i++) { if (a[i] != b[i]) { return 0; } } return 1; }
static void cpy(uint8_t *d, const uint8_t *s, uint32_t n) { for (uint32_t i = 0; i < n; i++) { d[i] = s[i]; } }
const char *btc_strerror(int rc) {
    switch (rc) {
    case BTC_OK: return "ok"; case BTC_ERR_SHORT: return "the data ends inside a structure"; case BTC_ERR_TOO_MANY_TX: return "more transactions than the table holds"; case BTC_ERR_TOO_MANY_IN: return "a transaction has more inputs than the table holds";
    case BTC_ERR_TRAILING: return "bytes after the last transaction"; case BTC_ERR_FLAG: return "unknown transaction flag (only 0x01, the witness flag, exists)"; case BTC_ERR_SUPERFLUOUS_WITNESS: return "witness flag set but every witness is empty";
    case BTC_ERR_VARINT: return "a variable-length integer is not minimal or does not fit"; case BTC_ERR_NO_TX: return "a block with no transactions";
    }
    return "unknown error";
}
void btc_dsha(const uint8_t *data, uint32_t len, uint8_t out[32]) { uint8_t t[32]; sha256_hash(data, len, t); sha256_hash(t, 32, out); }

/* a "CompactSize" integer; -1 when it runs past the end */
static int rd_varint(const uint8_t *b, uint32_t len, uint32_t *pos, uint64_t *v) {
    uint32_t p = *pos; if (p >= len) { return -1; }
    uint8_t n = b[p];
    if (n < 0xfd) { *v = n; *pos = p + 1; return 0; }
    if (n == 0xfd) { if (p + 3 > len) { return -1; } *v = (uint64_t)b[p + 1] | (uint64_t)b[p + 2] << 8; *pos = p + 3; return 0; }
    if (n == 0xfe) { if (p + 5 > len) { return -1; } *v = rd32(b + p + 1); *pos = p + 5; return 0; }
    if (p + 9 > len) { return -1; }
    *v = (uint64_t)rd32(b + p + 1) | (uint64_t)rd32(b + p + 5) << 32; *pos = p + 9; return 0;
}

static uint32_t g_in_off[BTC_MAX_IN]; /* offsets of the current transaction's inputs, for the duplicate-input check */
int btc_tx_parse(const uint8_t *b, uint32_t len, uint32_t pos, btc_tx_t *t, uint32_t *next) {
    uint32_t start = pos, p = pos; uint64_t v;
    if (len - p < 10 || p > len) { return BTC_ERR_SHORT; }
    t->version = (int32_t)rd32(b + p); p += 4; t->has_witness = 0;
    if (b[p] == 0 && p + 1 < len && b[p + 1] != 0) { if (b[p + 1] != 1) { return BTC_ERR_FLAG; } t->has_witness = 1; p += 2; }
    uint32_t vin_start = p;
    if (rd_varint(b, len, &p, &v) != 0) { return BTC_ERR_SHORT; }
    if (v > BTC_MAX_IN) { return BTC_ERR_TOO_MANY_IN; }
    t->n_in = (uint32_t)v; t->cb_script_off = 0; t->cb_script_len = 0;
    for (uint32_t i = 0; i < t->n_in; i++) {
        if (len - p < 36 || p > len) { return BTC_ERR_SHORT; }
        g_in_off[i] = p; uint32_t q = p + 36; uint64_t sl;
        if (rd_varint(b, len, &q, &sl) != 0 || sl > len - q || len - q - sl < 4) { return BTC_ERR_SHORT; }
        if (i == 0) { t->cb_script_off = q; t->cb_script_len = (uint32_t)sl; }
        p = q + (uint32_t)sl + 4;
    }
    if (rd_varint(b, len, &p, &v) != 0) { return BTC_ERR_SHORT; }
    uint64_t n_out = v; t->n_out = (uint32_t)(n_out > 0xffffffffu ? 0xffffffffu : n_out);
    uint32_t vout_first = p; t->value_out = 0; t->commit_found = 0; t->commit_off = 0; const char *bad = 0; int64_t total = 0;
    for (uint64_t i = 0; i < n_out; i++) {
        if (len - p < 8 || p > len) { return BTC_ERR_SHORT; }
        int64_t val = rd64s(b + p); uint32_t q = p + 8; uint64_t sl;
        if (rd_varint(b, len, &q, &sl) != 0 || sl > len - q) { return BTC_ERR_SHORT; }
        if (!bad) {
            if (val < 0) { bad = "bad-txns-vout-negative"; }
            else if (val > BTC_MAX_MONEY) { bad = "bad-txns-vout-toolarge"; }
            else { total += val; if (total > BTC_MAX_MONEY) { bad = "bad-txns-txouttotal-toolarge"; } }
        }
        if (sl >= 38 && b[q] == 0x6a && b[q + 1] == 0x24 && b[q + 2] == 0xaa && b[q + 3] == 0x21 && b[q + 4] == 0xa9 && b[q + 5] == 0xed) { t->commit_found = 1; t->commit_off = q + 6; }
        t->value_out = (int64_t)((uint64_t)t->value_out + (uint64_t)val); /* the plain sum of every output value (wraps only for absurd inputs) */
        p = q + (uint32_t)sl;
    }
    (void)vout_first; uint32_t vout_end = p;
    t->wit0_items = 0; t->wit0_item0_off = 0; t->wit0_item0_len = 0; int any_witness = 0;
    if (t->has_witness) {
        for (uint32_t i = 0; i < t->n_in; i++) {
            uint64_t c; if (rd_varint(b, len, &p, &c) != 0) { return BTC_ERR_SHORT; }
            if (c) { any_witness = 1; }
            if (i == 0) { t->wit0_items = (uint8_t)(c > 255 ? 255 : c); }
            for (uint64_t k = 0; k < c; k++) {
                uint64_t l; if (rd_varint(b, len, &p, &l) != 0 || l > len - p) { return BTC_ERR_SHORT; }
                if (i == 0 && k == 0) { t->wit0_item0_off = p; t->wit0_item0_len = (uint32_t)l; }
                p += (uint32_t)l;
            }
        }
        if (!any_witness) { return BTC_ERR_SUPERFLUOUS_WITNESS; }
    }
    if (len - p < 4 || p > len) { return BTC_ERR_SHORT; }
    uint32_t lt_off = p; t->lock_time = rd32(b + p); p += 4; t->size = p - start; *next = p;
    /* the txid hashes the serialisation WITHOUT witness data; the wtxid hashes everything */
    if (t->has_witness) {
        sha256_ctx_t c; uint8_t d1[32]; sha256_init(&c); sha256_update(&c, b + start, 4); sha256_update(&c, b + vin_start, vout_end - vin_start); sha256_update(&c, b + lt_off, 4); sha256_final(&c, d1); sha256_hash(d1, 32, t->txid);
        t->stripped_size = 4 + (vout_end - vin_start) + 4;
    } else { btc_dsha(b + start, t->size, t->txid); t->stripped_size = t->size; }
    btc_dsha(b + start, t->size, t->wtxid);
    t->weight = 3 * t->stripped_size + t->size;
    static const uint8_t zero32[32] = {0};
    t->is_coinbase = (t->n_in == 1 && same(b + g_in_off[0], zero32, 32) && rd32(b + g_in_off[0] + 32) == 0xffffffffu);
    /* the context-free checks, in Bitcoin Core's order */
    t->check = 0;
    if (t->n_in == 0) { t->check = "bad-txns-vin-empty"; }
    else if (n_out == 0) { t->check = "bad-txns-vout-empty"; }
    else if (t->weight > BTC_MAX_WEIGHT) { t->check = "bad-txns-oversize"; }
    else if (bad) { t->check = bad; }
    else {
        for (uint32_t i = 0; i < t->n_in && !t->check; i++) { for (uint32_t j = i + 1; j < t->n_in; j++) { if (same(b + g_in_off[i], b + g_in_off[j], 36)) { t->check = "bad-txns-inputs-duplicate"; break; } } }
        if (!t->check) {
            if (t->is_coinbase) { if (t->cb_script_len < 2 || t->cb_script_len > 100) { t->check = "bad-cb-length"; } }
            else { for (uint32_t i = 0; i < t->n_in; i++) { if (same(b + g_in_off[i], zero32, 32) && rd32(b + g_in_off[i] + 32) == 0xffffffffu) { t->check = "bad-txns-prevout-null"; break; } } }
        }
    }
    return BTC_OK;
}

void btc_merkle(const uint8_t (*leaves)[32], uint32_t n, uint8_t root[32], uint8_t *mutated) {
    static uint8_t lv[BTC_MAX_TX + 1][32]; uint8_t pair[64]; *mutated = 0;
    if (n == 0) { for (int i = 0; i < 32; i++) { root[i] = 0; } return; }
    for (uint32_t i = 0; i < n; i++) { cpy(lv[i], leaves[i], 32); }
    while (n > 1) {
        for (uint32_t i = 0; i + 1 < n; i += 2) { if (same(lv[i], lv[i + 1], 32)) { *mutated = 1; } }
        if (n & 1u) { cpy(lv[n], lv[n - 1], 32); n++; }
        for (uint32_t i = 0; i < n; i += 2) { cpy(pair, lv[i], 32); cpy(pair + 32, lv[i + 1], 32); btc_dsha(pair, 64, lv[i / 2]); }
        n /= 2;
    }
    cpy(root, lv[0], 32);
}

/* the compact "bits" -> a 256-bit target, big-endian; validity as Bitcoin Core's SetCompact + range checks */
static void expand(uint32_t bits, uint8_t t[32], int *neg, int *over) {
    uint32_t size = bits >> 24, word = bits & 0x007fffffu; for (int i = 0; i < 32; i++) { t[i] = 0; }
    *neg = (word != 0) && (bits & 0x00800000u) != 0; *over = (word != 0) && (size > 34 || (word > 0xff && size > 33) || (word > 0xffff && size > 32));
    if (size <= 3) { word >>= 8 * (3 - size); t[29] = (uint8_t)(word >> 16); t[30] = (uint8_t)(word >> 8); t[31] = (uint8_t)word; return; }
    if (*over) { return; }
    /* word * 256^(size-3): its three bytes sit at 32-size .. 34-size */
    int top = 32 - (int)size; uint8_t m[3] = {(uint8_t)(word >> 16), (uint8_t)(word >> 8), (uint8_t)word};
    for (int k = 0; k < 3; k++) { int idx = top + k; if (idx >= 0 && idx < 32) { t[idx] = m[k]; } }
}
static int cmp_be(const uint8_t *a, const uint8_t *b) { for (int i = 0; i < 32; i++) { if (a[i] != b[i]) { return a[i] < b[i] ? -1 : 1; } } return 0; }
int btc_compact(uint32_t bits, uint32_t limit_bits, uint8_t target[32], const char **why) {
    int neg, over; expand(bits, target, &neg, &over);
    if (neg) { *why = "negative"; return 0; }
    if (over) { *why = "overflow"; return 0; }
    int zero = 1; for (int i = 0; i < 32; i++) { if (target[i]) { zero = 0; } }
    if (zero) { *why = "zero"; return 0; }
    uint8_t lim[32]; int n2, o2; expand(limit_bits, lim, &n2, &o2);
    if (cmp_be(target, lim) > 0) { *why = "above the limit"; return 0; }
    *why = ""; return 1;
}
static int bip34_height(const uint8_t *s, uint32_t n, int64_t *out) { /* 1 and the height, or 0 when the script does not start with a height push */
    if (n == 0) { return 0; }
    uint8_t k = s[0];
    if (k == 0) { *out = 0; return 1; }
    if (k >= 0x51 && k <= 0x60) { *out = (int64_t)k - 0x50; return 1; }
    if (k < 1 || k > 8 || n < 1u + k) { return 0; }
    uint64_t v = 0; for (int i = k - 1; i >= 0; i--) { v = (v << 8) | s[1 + i]; }
    if (s[k] & 0x80) { v &= ~((uint64_t)0x80 << (8 * (k - 1))); *out = -(int64_t)v; return 1; }
    *out = (int64_t)v; return 1;
}
int btc_block_parse(btc_block_t *B, const uint8_t *b, uint32_t len, uint32_t pow_limit_bits) {
    static uint8_t ids[BTC_MAX_TX][32], wids[BTC_MAX_TX][32]; uint8_t mut2;
    if (len < 81) { return BTC_ERR_SHORT; }
    B->version = rd32(b); cpy(B->prev, b + 4, 32); cpy(B->merkle, b + 36, 32); B->time = rd32(b + 68); B->bits = rd32(b + 72); B->nonce = rd32(b + 76); B->size = len; B->reason = 0;
    btc_dsha(b, 80, B->hash);
    uint32_t p = 80; uint64_t n; if (rd_varint(b, len, &p, &n) != 0) { return BTC_ERR_SHORT; }
    uint32_t cnt_len = p - 80;
    if (n > BTC_MAX_TX) { return BTC_ERR_TOO_MANY_TX; }
    B->n_tx = (uint32_t)n; if (n == 0) { return BTC_ERR_NO_TX; }
    uint32_t stripped = 80 + cnt_len, any_wit = 0;
    for (uint32_t i = 0; i < B->n_tx; i++) {
        uint32_t nx; int rc = btc_tx_parse(b, len, p, &B->tx[i], &nx); if (rc != BTC_OK) { return rc; }
        cpy(ids[i], B->tx[i].txid, 32); cpy(wids[i], B->tx[i].wtxid, 32); stripped += B->tx[i].stripped_size; if (B->tx[i].has_witness) { any_wit = 1; } p = nx;
    }
    if (p != len) { return BTC_ERR_TRAILING; }
    B->weight = 3 * stripped + len;
    B->target_ok = (uint8_t)btc_compact(B->bits, pow_limit_bits, B->target, &B->target_why);
    B->pow_ok = 0;
    if (B->target_ok) { int ok = 1; for (int i = 31; i >= 0; i--) { uint8_t h = B->hash[i], t = B->target[31 - i]; if (h != t) { ok = h < t; break; } } B->pow_ok = (uint8_t)ok; }
    B->zero_bits = 0; { for (int i = 31; i >= 0; i--) { uint8_t h = B->hash[i]; if (h == 0) { B->zero_bits += 8; continue; } uint8_t m = 0x80; while (m && !(h & m)) { B->zero_bits++; m >>= 1; } break; } }
    btc_merkle((const uint8_t (*)[32])ids, B->n_tx, B->computed_merkle, &B->merkle_mutated);
    B->first_is_cb = B->tx[0].is_coinbase; B->other_cb = 0; for (uint32_t i = 1; i < B->n_tx; i++) { if (B->tx[i].is_coinbase) { B->other_cb = 1; } }
    B->height = 0; B->height_ok = 0; if (B->first_is_cb && B->version >= 2) { B->height_ok = (uint8_t)bip34_height(b + B->tx[0].cb_script_off, B->tx[0].cb_script_len, &B->height); }
    B->witness = 0;
    if (any_wit && B->first_is_cb && B->tx[0].commit_found) {
        for (int i = 0; i < 32; i++) { wids[0][i] = 0; }
        uint8_t wroot[32], buf[64], calc[32]; btc_merkle((const uint8_t (*)[32])wids, B->n_tx, wroot, &mut2);
        int ok = B->tx[0].has_witness && B->tx[0].wit0_items == 1 && B->tx[0].wit0_item0_len == 32;
        if (ok) { cpy(buf, wroot, 32); cpy(buf + 32, b + B->tx[0].wit0_item0_off, 32); btc_dsha(buf, 64, calc); ok = same(calc, b + B->tx[0].commit_off, 32); }
        B->witness = ok ? 1 : 2;
    }
    int mr_ok = same(B->computed_merkle, B->merkle, 32);
    if (!B->pow_ok) { B->reason = "high-hash"; }
    else if (!mr_ok) { B->reason = "bad-txnmrklroot"; }
    else if (B->merkle_mutated) { B->reason = "bad-txns-duplicate"; }
    else if (B->weight > BTC_MAX_WEIGHT) { B->reason = "bad-blk-length"; }
    else if (!B->first_is_cb) { B->reason = "bad-cb-missing"; }
    else if (B->other_cb) { B->reason = "bad-cb-multiple"; }
    else { for (uint32_t i = 0; i < B->n_tx; i++) { if (B->tx[i].check) { B->reason = B->tx[i].check; break; } } }
    if (!B->reason && B->witness == 2) { B->reason = "bad-witness-merkle-match"; }
    return BTC_OK;
}

typedef struct { char *b; uint32_t cap, o; int bad; } out_t;
static void pc(out_t *w, char c) { if (w->o + 1 >= w->cap) { w->bad = 1; return; } w->b[w->o++] = c; }
static void ps(out_t *w, const char *s) { while (*s) { pc(w, *s++); } }
static uint64_t udm(uint64_t n, uint32_t d, uint32_t *r) { uint64_t q = 0, rem = 0; for (int i = 63; i >= 0; i--) { rem = (rem << 1) | ((n >> i) & 1u); if (rem >= d) { rem -= d; q |= 1ull << i; } } *r = (uint32_t)rem; return q; }
static void pu(out_t *w, uint64_t v) { char t[24]; int n = 0; uint32_t r; if (v == 0) { t[n++] = '0'; } while (v) { v = udm(v, 10, &r); t[n++] = (char)('0' + r); } while (n) { pc(w, t[--n]); } }
static void pi(out_t *w, int64_t v) { if (v < 0) { pc(w, '-'); pu(w, (uint64_t)(-v)); } else { pu(w, (uint64_t)v); } }
static void ph(out_t *w, const uint8_t *d, int reverse) { static const char H[] = "0123456789abcdef"; for (int i = 0; i < 32; i++) { uint8_t x = reverse ? d[31 - i] : d[i]; pc(w, H[x >> 4]); pc(w, H[x & 15]); } }
static void ph8(out_t *w, uint32_t v) { static const char H[] = "0123456789abcdef"; for (int i = 7; i >= 0; i--) { pc(w, H[(v >> (4 * i)) & 15]); } }
int btc_report(char *buf, uint32_t cap, const char *name, const btc_block_t *B) {
    out_t w; w.b = buf; w.cap = cap; w.o = 0; w.bad = 0;
    ps(&w, "block "); ps(&w, name); ps(&w, " size "); pu(&w, B->size); ps(&w, " weight "); pu(&w, B->weight); ps(&w, " ntx "); pu(&w, B->n_tx); ps(&w, "\n");
    ps(&w, "  header hash "); ph(&w, B->hash, 1); ps(&w, " prev "); ph(&w, B->prev, 1); ps(&w, " merkle "); ph(&w, B->merkle, 1); ps(&w, " version "); ph8(&w, B->version); ps(&w, " time "); pu(&w, B->time); ps(&w, " bits "); ph8(&w, B->bits); ps(&w, " nonce "); pu(&w, B->nonce); ps(&w, "\n");
    ps(&w, "  target "); if (B->target_ok) { ph(&w, B->target, 0); } else { ps(&w, "invalid ("); ps(&w, B->target_why); ps(&w, ")"); } ps(&w, "\n");
    ps(&w, "  pow "); ps(&w, B->pow_ok ? "PASS" : "FAIL"); ps(&w, " zero_bits "); pu(&w, B->zero_bits); ps(&w, "\n");
    ps(&w, "  merkle "); ps(&w, same(B->computed_merkle, B->merkle, 32) ? "PASS" : "FAIL"); ps(&w, " computed "); ph(&w, B->computed_merkle, 1); ps(&w, " mutated "); ps(&w, B->merkle_mutated ? "yes" : "no"); ps(&w, "\n");
    ps(&w, "  coinbase first "); ps(&w, B->first_is_cb ? "yes" : "no"); ps(&w, " others "); ps(&w, B->other_cb ? "YES" : "no"); ps(&w, " height "); if (B->height_ok) { pi(&w, B->height); } else { ps(&w, "none"); } ps(&w, "\n");
    ps(&w, "  witness "); ps(&w, B->witness == 0 ? "none" : B->witness == 1 ? "PASS" : "FAIL"); ps(&w, "\n");
    for (uint32_t i = 0; i < B->n_tx; i++) {
        const btc_tx_t *t = &B->tx[i];
        ps(&w, "  tx "); pu(&w, i); ps(&w, " txid "); ph(&w, t->txid, 1); ps(&w, " wtxid "); ph(&w, t->wtxid, 1); ps(&w, " in "); pu(&w, t->n_in); ps(&w, " out "); pu(&w, t->n_out); ps(&w, " out_sat "); pi(&w, t->value_out);
        ps(&w, " size "); pu(&w, t->size); ps(&w, " weight "); pu(&w, t->weight); ps(&w, " check "); ps(&w, t->check ? t->check : "ok"); ps(&w, "\n");
    }
    ps(&w, "  verdict "); if (B->reason) { ps(&w, "INVALID "); ps(&w, B->reason); } else { ps(&w, "VALID"); } ps(&w, "\n");
    if (w.bad) { return -1; } w.b[w.o] = 0; return (int)w.o;
}
