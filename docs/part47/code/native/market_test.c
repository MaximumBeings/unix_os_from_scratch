/* Chapter 47, host-side tests of the marketplace layer (NOT part of the kernel). Build and run:
 *   gcc -O1 -fsanitize=address,undefined -I.. market_test.c ../047_market.c ../047_auction.c ../047_ledger.c ../047_wal.c ../047_sha256.c -o market_test && ./market_test N
 * It checks, over N pseudo-random marketplaces:
 *   1. after EVERY command, all marketplace invariants hold (mkt_check() == 0, `broken` never latched);
 *   2. REPLAY: rebuilding a market from the log gives the same state hash as the live market;
 *   3. EVERY PREFIX of the log replays to the state the live market had after that many commands (what a crash after any command would recover);
 *   4. TORN TAIL: damaging the last record in six different ways (truncate, flip a payload bit, flip a CRC bit, zero the magic, wrong sequence number, drop it) stops replay at the last
 *      good record, with the right reason, and the recovered state equals the state before the damaged command;
 *   5. IDEMPOTENCY: a retried value-moving request is answered from the stored result and changes nothing; the same key with a different request is refused.
 * Prints counters; exits non-zero on the first failure. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "047_market.h"

#define MAXLOG 400
static uint8_t g_log[MAXLOG][WAL_RECORD];
static uint32_t g_log_len[MAXLOG];
static uint32_t g_nlog;
static int g_fail_writes;

static int writer(uint32_t seq, const uint8_t rec[WAL_RECORD]) {
    if (seq > MAXLOG) return 1;
    memcpy(g_log[seq - 1], rec, WAL_RECORD);
    g_log_len[seq - 1] = WAL_RECORD;
    if (seq > g_nlog) g_nlog = seq;
    return g_fail_writes ? 1 : 0;
}
static int reader(uint32_t seq, uint8_t *buf, uint32_t size, uint32_t *out_len) {
    if (seq == 0 || seq > g_nlog) return 1;
    uint32_t n = g_log_len[seq - 1]; if (n > size) n = size;
    memcpy(buf, g_log[seq - 1], n); *out_len = n;
    return 0;
}

static uint32_t rs;
static uint32_t rnd(void) { uint32_t x = rs; x ^= x << 13; x ^= x >> 17; x ^= x << 5; rs = x; return x; }
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); exit(1); } } while (0)

static void hash_hex(const market_t *m, char out[65]) { uint8_t h[32]; mkt_hash(m, h); for (int i = 0; i < 32; i++) sprintf(out + 2 * i, "%02x", h[i]); }
static int same(const market_t *a, const market_t *b) { uint8_t x[32], y[32]; mkt_hash(a, x); mkt_hash(b, y); return memcmp(x, y, 32) == 0; }

int main(int argc, char **argv) {
    uint32_t N = argc > 1 ? (uint32_t)atoi(argv[1]) : 200;
    CHECK(wal_crc32((const uint8_t *)"123456789", 9) == 0xCBF43926u, "CRC-32 check value");
    uint64_t releases_checked = 0, hash_probes = 0, total_cmds = 0, prefixes = 0, torn = 0, replays = 0, retries = 0, mismatches = 0, sold = 0, released = 0, refunded = 0;
    static market_t m, f;
    static uint32_t step_hash_words[MAXLOG][8];
    for (uint32_t s = 0; s < N; s++) {
        rs = 88172645u + s * 2654435761u; if (!rs) rs = 1;
        mkt_init(&m); g_nlog = 0; g_fail_writes = 0;
        uint32_t now = 0, idem = 1, last_bid_idem[4] = {0}; mkt_cmd_t last_bid[4]; uint32_t nlast = 0;
        for (uint32_t u = 4; u < 10; u++) { mkt_cmd_t c; memset(&c, 0, sizeof c); c.type = CMD_DEPOSIT; c.idem = idem++; c.a[0] = u; c.a[1] = 100000 + rnd() % 400000; mkt_result_t r; CHECK(mkt_submit(&m, &c, writer, &r) == 0, "write"); uint8_t h0[32]; mkt_hash(&m, h0); memcpy(step_hash_words[g_nlog - 1], h0, 32); }
        uint32_t ncmd = 30 + rnd() % 60;
        for (uint32_t k = 0; k < ncmd && g_nlog < MAXLOG - 2; k++) {
            mkt_cmd_t c; memset(&c, 0, sizeof c); mkt_result_t r; now += rnd() % 400;
            uint32_t pick = rnd() % 20, lid = 1 + rnd() % 3;
            if (pick < 2) { c.type = CMD_CREATE; c.now = now; c.a[0] = lid; c.a[1] = 3 + rnd() % 2; c.a[2] = 100 + rnd() % 5000; c.a[3] = (rnd() % 2) ? c.a[2] + rnd() % 20000 : 0; c.a[4] = 300 + rnd() % 2000; c.a[5] = (rnd() % 3 == 0) ? 60 : 0; c.a[6] = c.a[5] ? 120 : 0; strcpy(c.title, "item"); }
            else if (pick < 9) { c.type = CMD_BID; c.now = now; c.idem = idem++; c.a[0] = lid; c.a[1] = 4 + rnd() % 6; c.a[2] = 100 + rnd() % 40000; if (nlast < 4) { last_bid[nlast] = c; nlast++; } }
            else if (pick < 13) { c.type = CMD_CLOSE; c.now = now + 2000; c.a[0] = lid; }
            else if (pick < 16) { c.type = CMD_CHECKOUT; c.idem = idem++; c.now = now; c.a[0] = lid; auc_listing_t *l = auc_find(&m.auc, lid); c.a[1] = (l && rnd() % 5) ? l->high_bidder : 4 + rnd() % 6; c.a[2] = rnd() % 2000; }
            else if (pick < 18) { c.type = CMD_RELEASE; c.now = now; c.a[0] = lid; }
            else if (pick < 19) { c.type = CMD_REFUND; c.now = now; c.a[0] = lid; }
            else { c.type = CMD_DEPOSIT; c.idem = idem++; c.a[0] = 4 + rnd() % 6; c.a[1] = 1 + rnd() % 50000; }
            int32_t fees_before = m.led.bal[LED_FEES], esc_before = m.led.bal[LED_ESCROW], seller_before = 0; uint32_t total_before = 0, seller_id = 0;
            if (c.type == CMD_RELEASE) { mkt_order_t *o = mkt_find_order(&m, c.a[0]); if (o && o->status == ORD_PAID) { seller_id = o->seller; seller_before = m.led.bal[o->seller]; total_before = o->total_cents; } }
            CHECK(mkt_submit(&m, &c, writer, &r) == 0, "write");
            CHECK(mkt_check(&m) == 0 && !m.broken, "invariant %d broken after command %u of scenario %u (type %u)", mkt_check(&m), k, s, c.type);
            if (c.type == CMD_RELEASE && r.code == 0) {   /* a release moves EXACTLY total = seller share + fee out of escrow, and the fee is at least 30 cents */
                uint32_t fee = r.v2, share = r.v1;
                CHECK(share + fee == total_before && fee >= (total_before < 30 ? total_before : 30), "release split %u + %u of %u", share, fee, total_before);
                CHECK(m.led.bal[LED_FEES] - fees_before == (int32_t)fee && m.led.bal[seller_id] - seller_before == (int32_t)share && esc_before - m.led.bal[LED_ESCROW] == (int32_t)total_before, "release moved the wrong amounts");
                releases_checked++;
            }
            total_cmds++;
            if (c.type == CMD_CHECKOUT && r.code == 0) { /* ok */ }
            /* record the state hash after this many log records, for the prefix test */
            { uint8_t h[32]; mkt_hash(&m, h); memcpy(step_hash_words[g_nlog - 1], h, 32); }
            /* idempotency: retry an earlier value-moving command with the SAME key and request, on a random subset: must be a no-op returning the stored result */
            if (nlast > 0 && rnd() % 4 == 0) {
                mkt_cmd_t rc_ = last_bid[rnd() % nlast]; uint8_t before[32], after[32]; mkt_result_t r2;
                mkt_hash(&m, before); uint32_t led_before = m.led.n_tx; uint32_t applied_before = m.applied;
                CHECK(mkt_submit(&m, &rc_, writer, &r2) == 0, "write");
                { uint8_t h[32]; mkt_hash(&m, h); memcpy(step_hash_words[g_nlog - 1], h, 32); }
                CHECK(r2.replayed == 1, "retry must be answered from the stored result");
                CHECK(m.led.n_tx == led_before && m.applied == applied_before + 1, "a retry changed money");
                retries++; (void)after; (void)before;
                mkt_cmd_t bad = rc_; bad.a[2] += 1; mkt_result_t r3; CHECK(mkt_submit(&m, &bad, writer, &r3) == 0, "write");
                { uint8_t h[32]; mkt_hash(&m, h); memcpy(step_hash_words[g_nlog - 1], h, 32); }
                CHECK(r3.code == MKT_ERR_IDEM_MISMATCH, "same key, different request must be refused (got %u)", r3.code);
                mismatches++;
            }
        }
        for (uint32_t i = 0; i < MKT_MAX_ORDERS; i++) if (m.orders[i].used) { if (m.orders[i].status == ORD_RELEASED) released++; if (m.orders[i].status == ORD_REFUNDED) refunded++; }
        for (uint32_t i = 0; i < AUC_MAX_LISTINGS; i++) if (m.auc.listings[i].used && m.auc.listings[i].status == AUC_SOLD) sold++;
        /* hash sensitivity: changing ANY kind of field of a copy of the state must change the hash (a hash that ignored a whole class of state could not prove recovery) */
        {
            static market_t x; uint8_t h0[32], h1[32]; mkt_hash(&m, h0); int probes = 0;
            #define PROBE(stmt) do { memcpy(&x, &m, sizeof x); stmt; mkt_hash(&x, h1); CHECK(memcmp(h0, h1, 32) != 0, "hash did not change for: %s", #stmt); probes++; hash_probes++; } while (0)
            PROBE(x.led.bal[4] += 1);
            PROBE(x.led.n_tx += 1);
            PROBE(x.applied += 1);
            for (uint32_t i = 0; i < AUC_MAX_LISTINGS; i++) if (m.auc.listings[i].used) {
                PROBE(x.auc.listings[i].price_cents += 1); PROBE(x.auc.listings[i].status ^= 1); PROBE(x.auc.listings[i].end_time += 1); PROBE(x.auc.listings[i].title[0] ^= 1);
                if (m.auc.listings[i].nproxies) { PROBE(x.auc.listings[i].proxies[0].max_cents += 1); PROBE(x.auc.listings[i].proxies[0].seq += 1); }
                break;
            }
            for (uint32_t i = 0; i < MKT_MAX_ORDERS; i++) if (m.orders[i].used) { PROBE(x.orders[i].status ^= 1); PROBE(x.orders[i].fee_cents += 1); break; }
            for (uint32_t i = 0; i < MKT_MAX_IDEM; i++) if (m.idem[i].used) { PROBE(x.idem[i].result.v1 += 1); PROBE(x.idem[i].fingerprint ^= 1); break; }
            (void)probes;
        }
        /* 2. full replay */
        int stop; mkt_init(&f); uint32_t n = mkt_replay(&f, reader, &stop);
        CHECK(n == g_nlog && stop == MKT_LOG_END && same(&m, &f), "full replay differs (replayed %u of %u, stop %d)", n, g_nlog, stop);
        replays++;
        /* 3. every prefix */
        for (uint32_t k = 1; k <= g_nlog; k++) {
            uint32_t saved = g_nlog; g_nlog = k; mkt_init(&f); n = mkt_replay(&f, reader, &stop); g_nlog = saved;
            uint8_t h[32]; mkt_hash(&f, h);
            CHECK(n == k && memcmp(h, step_hash_words[k - 1], 32) == 0, "prefix %u of scenario %u replays to a different state", k, s);
            prefixes++;
        }
        /* 4. torn tail: damage the LAST record six ways; the state before it must be recovered and the stop reason must be right */
        if (g_nlog >= 2) {
            uint32_t last = g_nlog; uint8_t saved[WAL_RECORD]; memcpy(saved, g_log[last - 1], WAL_RECORD); uint32_t savedlen = g_log_len[last - 1];
            for (int kind = 0; kind < 6; kind++) {
                memcpy(g_log[last - 1], saved, WAL_RECORD); g_log_len[last - 1] = savedlen; int want = WAL_ERR_CRC; uint32_t savedn = g_nlog;
                if (kind == 0) { g_log_len[last - 1] = WAL_RECORD - 7; want = WAL_ERR_SHORT; }
                if (kind == 1) { g_log[last - 1][20] ^= 0x10; want = WAL_ERR_CRC; }
                if (kind == 2) { g_log[last - 1][WAL_RECORD - 2] ^= 0x01; want = WAL_ERR_CRC; }
                if (kind == 3) { g_log[last - 1][0] = 0; want = WAL_ERR_MAGIC; }
                if (kind == 4) { uint8_t p[WAL_PAYLOAD]; memcpy(p, saved + 12, WAL_PAYLOAD); wal_encode(last + 5, p, g_log[last - 1]); want = WAL_ERR_SEQ; }
                if (kind == 5) { g_nlog = last - 1; want = MKT_LOG_END; }
                mkt_init(&f); n = mkt_replay(&f, reader, &stop); g_nlog = savedn;
                uint8_t h[32]; mkt_hash(&f, h);
                CHECK(n == last - 1 && stop == want && memcmp(h, step_hash_words[last - 2], 32) == 0, "torn kind %d of scenario %u: replayed %u (want %u), stop %d (want %d)", kind, s, n, last - 1, stop, want);
                torn++;
            }
            memcpy(g_log[last - 1], saved, WAL_RECORD); g_log_len[last - 1] = savedlen;
        }
    }
    /* 6. idempotency table FULL: the 25th distinct key must be refused WITHOUT applying the command */
    {
        mkt_init(&m); g_nlog = 0; mkt_result_t r; mkt_cmd_t c;
        for (uint32_t k = 0; k < MKT_MAX_IDEM; k++) { memset(&c, 0, sizeof c); c.type = CMD_DEPOSIT; c.idem = 5000 + k; c.a[0] = 4; c.a[1] = 100; CHECK(mkt_submit(&m, &c, writer, &r) == 0 && r.code == 0, "deposit %u", k); }
        int32_t before = m.led.bal[4]; uint32_t tx = m.led.n_tx;
        memset(&c, 0, sizeof c); c.type = CMD_DEPOSIT; c.idem = 9999; c.a[0] = 4; c.a[1] = 100; CHECK(mkt_submit(&m, &c, writer, &r) == 0, "write");
        CHECK(r.code == MKT_ERR_IDEM_FULL && m.led.bal[4] == before && m.led.n_tx == tx, "a full idempotency table must refuse WITHOUT applying (code %u, balance %d vs %d)", r.code, m.led.bal[4], before);
        mkt_init(&f); int stop; uint32_t n = mkt_replay(&f, reader, &stop); CHECK(n == g_nlog && same(&m, &f), "replay after a refused command");
        memset(&c, 0, sizeof c); c.type = CMD_DEPOSIT; c.idem = 5000; c.a[0] = 4; c.a[1] = 100; CHECK(mkt_submit(&m, &c, writer, &r) == 0 && r.replayed == 1, "a known key is still answered when the table is full");
        printf("Directed test 1: idempotency table full (%u keys): the next new key is refused with nothing applied, a known key is still answered from the table, and replay agrees\n", MKT_MAX_IDEM);
    }
    /* 7. directed: a winner without enough money cannot check out; nothing moves, no order exists, invariants hold */
    {
        mkt_init(&m); g_nlog = 0; mkt_result_t r; mkt_cmd_t c;
        memset(&c, 0, sizeof c); c.type = CMD_DEPOSIT; c.idem = 1; c.a[0] = 4; c.a[1] = 500; mkt_submit(&m, &c, writer, &r);
        memset(&c, 0, sizeof c); c.type = CMD_CREATE; c.a[0] = 1; c.a[1] = 3; c.a[2] = 1000; c.a[4] = 100; strcpy(c.title, "lamp"); mkt_submit(&m, &c, writer, &r);
        memset(&c, 0, sizeof c); c.type = CMD_BID; c.now = 5; c.idem = 2; c.a[0] = 1; c.a[1] = 4; c.a[2] = 1000; mkt_submit(&m, &c, writer, &r); CHECK(r.code == 0, "bid");
        memset(&c, 0, sizeof c); c.type = CMD_CLOSE; c.now = 200; c.a[0] = 1; mkt_submit(&m, &c, writer, &r);
        memset(&c, 0, sizeof c); c.type = CMD_CHECKOUT; c.now = 201; c.idem = 3; c.a[0] = 1; c.a[1] = 4; c.a[2] = 0; mkt_submit(&m, &c, writer, &r);
        CHECK(r.code == MKT_ERR_FUNDS && m.led.bal[4] == 500 && m.led.bal[LED_ESCROW] == 0 && mkt_find_order(&m, 1) == 0 && mkt_check(&m) == 0, "insufficient funds: code %u", r.code);
        printf("Directed test 2: a winner with $5.00 against a $10.00 price: checkout refused (code %u), balance still $5.00, escrow empty, no order, invariants hold\n", r.code);
    }
    printf("marketplaces %u, commands %llu: invariants held after every one; %llu full replays identical; %llu log prefixes replayed identically; %llu torn-tail cases recovered correctly;\n"
           "%llu retries answered from the stored result without changing anything; %llu same-key-different-request refusals; listings sold %llu, orders released %llu, refunded %llu; %llu releases checked to the cent; %llu hash-sensitivity probes\n",
           N, (unsigned long long)total_cmds, (unsigned long long)replays, (unsigned long long)prefixes, (unsigned long long)torn, (unsigned long long)retries, (unsigned long long)mismatches,
           (unsigned long long)sold, (unsigned long long)released, (unsigned long long)refunded, (unsigned long long)releases_checked, (unsigned long long)hash_probes);
    return 0;
}
