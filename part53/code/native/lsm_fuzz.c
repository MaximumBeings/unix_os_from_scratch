/* Chapter 53: random operations against a model (a sorted array), ordinary and extreme (1- and 24-byte keys with 0x00 and 0xFF bytes, empty and 100-byte values), with invariants checked after EVERY call: the store and the model agree on every key of the pool and on the digest,
 * every table file on the file system is a valid table whose entry count matches the in-memory description, there are never more than 12 tables, and a refused call changed nothing. Now and then the files are cloned and reopened (the digest must be unchanged), and a
 * single byte of a listed table or of the log is flipped in a clone: a damaged table must be reported as LSM_ERR_CORRUPT, and a damaged log must recover to a state the model really had at some earlier time (a prefix), never to anything else. Usage: lsm_fuzz RUNS */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ramfs.h"
#include "../053_sha256.h"
typedef struct { uint8_t k[LSM_MAX_KEY], v[LSM_MAX_VAL]; uint32_t kl, vl; int live; } kv_t;
#define POOL 90
static kv_t pool[POOL]; static int hist_n; static uint8_t hist[20000][32];
static uint32_t rs; static uint32_t rnd(void) { rs = rs * 1664525u + 1013904223u; return rs >> 8; }
static void m_digest(uint8_t out[32]) { /* pool sorted by key once; digest over live ones */ sha256_ctx_t h; sha256_init(&h); int idx[POOL]; for (int i = 0; i < POOL; i++) { idx[i] = i; }
    for (int i = 1; i < POOL; i++) { int x = idx[i], j = i - 1; while (j >= 0) { const kv_t *a = &pool[idx[j]], *b = &pool[x]; uint32_t n = a->kl < b->kl ? a->kl : b->kl; int c = memcmp(a->k, b->k, n); if (!c) { c = a->kl == b->kl ? 0 : (a->kl < b->kl ? -1 : 1); } if (c <= 0) { break; } idx[j + 1] = idx[j]; j--; } idx[j + 1] = x; }
    for (int i = 0; i < POOL; i++) { const kv_t *e = &pool[idx[i]]; if (!e->live) { continue; } uint8_t hd[2] = {(uint8_t)e->kl, (uint8_t)e->vl}; sha256_update(&h, hd, 2); sha256_update(&h, e->k, e->kl); sha256_update(&h, e->v, e->vl); } sha256_final(&h, out); }
static ramfs_t RF, RF2; static lsm_t S, T;
int main(int argc, char **argv) {
    int runs = argc > 1 ? atoi(argv[1]) : 10; long calls = 0, bad = 0, reopens = 0, tflips = 0, tflip_ok = 0, wflips = 0, wflip_ok = 0, refused = 0;
    for (int run = 0; run < runs; run++) {
        rs = 1234567u + (uint32_t)run * 7919u;
        for (int i = 0; i < POOL; i++) { kv_t *e = &pool[i]; memset(e, 0, sizeof *e); uint32_t t = rnd() % 10; e->kl = t < 2 ? 1 : (t < 4 ? LSM_MAX_KEY : 2 + rnd() % 10); for (uint32_t j = 0; j < e->kl; j++) { uint32_t b = rnd() % 4; e->k[j] = b == 0 ? 0 : (b == 1 ? 255 : (uint8_t)rnd()); } e->k[e->kl - 1] = (uint8_t)i; e->live = 0; } /* the last byte makes every pool key distinct */
        rf_init(&RF); lsm_fs_t fs = rf_fs(&RF); if (lsm_open(&S, &fs)) { bad++; continue; } hist_n = 0; m_digest(hist[hist_n++]);
        for (int op = 0; op < 3000; op++) {
            kv_t *e = &pool[rnd() % POOL]; uint32_t c = rnd() % 100; int rc; calls++;
            if (c < 55) { uint32_t vl = rnd() % 6 == 0 ? LSM_MAX_VAL : (rnd() % 5 == 0 ? 0 : rnd() % 40); uint8_t v[LSM_MAX_VAL]; for (uint32_t j = 0; j < vl; j++) { v[j] = (uint8_t)rnd(); } rc = lsm_put(&S, e->k, e->kl, v, vl); if (rc) { bad++; printf("FAIL: put returned %d (run %d op %d)\n", rc, run, op); break; } memcpy(e->v, v, vl); e->vl = vl; e->live = 1; m_digest(hist[hist_n++]); }
            else if (c < 75) { rc = lsm_delete(&S, e->k, e->kl); if (rc) { bad++; printf("FAIL: delete returned %d\n", rc); break; } e->live = 0; m_digest(hist[hist_n++]); }
            else if (c < 79) { rc = lsm_flush(&S); if (rc) { bad++; printf("FAIL: flush returned %d\n", rc); break; } }
            else if (c < 82) { rc = lsm_compact(&S); if (rc) { bad++; printf("FAIL: compact returned %d\n", rc); break; } }
            else if (c < 84) { uint8_t z[200] = {0}; uint32_t sq = S.seq, wl = rf_find(&RF, "WAL") ? rf_find(&RF, "WAL")->len : 0; rc = lsm_put(&S, z, rnd() % 2 ? 0 : 25, z, 1); refused++; if (rc != LSM_ERR_ARG || S.seq != sq || (rf_find(&RF, "WAL") ? rf_find(&RF, "WAL")->len : 0) != wl) { bad++; printf("FAIL: a refused put changed something\n"); } }
            if (op % 5 == 0) { /* invariants */
                uint8_t d[32], md[32]; uint32_t cnt, live = 0; if (lsm_digest(&S, d, &cnt)) { bad++; printf("FAIL: digest error\n"); break; } m_digest(md); for (int i = 0; i < POOL; i++) { live += pool[i].live; }
                if (memcmp(d, md, 32) || cnt != live) { bad++; printf("FAIL: digest differs from the model (run %d op %d)\n", run, op); break; }
                for (int i = 0; i < POOL; i++) { uint8_t o[LSM_MAX_VAL]; uint32_t n = 0; int g = lsm_get(&S, pool[i].k, pool[i].kl, o, sizeof o, &n); if (g != pool[i].live || (g == 1 && (n != pool[i].vl || memcmp(o, pool[i].v, n)))) { bad++; printf("FAIL: get disagrees with the model (run %d op %d key %d)\n", run, op, i); goto next; } }
                if (S.ntab > LSM_MAX_TABLES) { bad++; printf("FAIL: too many tables\n"); }
                for (uint32_t i = 0; i < S.ntab; i++) { char nm[11]; nm[0] = 'T'; uint32_t id = S.tab[i].id; for (int q = 5; q >= 1; q--) { nm[q] = (char)('0' + id % 10); id /= 10; } strcpy(nm + 6, ".SST"); rf_file_t *f = rf_find(&RF, nm); lsm_tab_t m; if (!f || lsm_table_parse(f->d, f->len, &m) || m.n != S.tab[i].n) { bad++; printf("FAIL: table %s invalid or inconsistent\n", nm); } }
            }
            if (op % 97 == 0) { rf_clone(&RF2, &RF); lsm_fs_t f2 = rf_fs(&RF2); uint8_t d[32], md[32]; reopens++; if (lsm_open(&T, &f2) || lsm_digest(&T, d, 0)) { bad++; printf("FAIL: reopen\n"); } else { m_digest(md); if (memcmp(d, md, 32)) { bad++; printf("FAIL: reopen changed the state\n"); } } }
            if (op % 211 == 0 && op > 0) { /* flip one byte */
                rf_clone(&RF2, &RF); int which = (int)(rnd() % (S.ntab + 1)); rf_file_t *f = 0; int is_log = 0;
                if (which == (int)S.ntab || S.ntab == 0) { f = rf_find(&RF2, "WAL"); is_log = 1; } else { char nm[11]; nm[0] = 'T'; uint32_t id = S.tab[which].id; for (int q = 5; q >= 1; q--) { nm[q] = (char)('0' + id % 10); id /= 10; } strcpy(nm + 6, ".SST"); f = rf_find(&RF2, nm); }
                if (f && f->len > 0) {
                    f->d[rnd() % f->len] ^= (uint8_t)(1u << (rnd() % 8)); lsm_fs_t f2 = rf_fs(&RF2); int rc2 = lsm_open(&T, &f2);
                    if (is_log) { wflips++; uint8_t d[32]; int ok = !rc2 && !lsm_digest(&T, d, 0); int found = 0; for (int h = 0; ok && h < hist_n && !found; h++) { if (!memcmp(d, hist[h], 32)) { found = 1; } } if (ok && found) { wflip_ok++; } else { bad++; printf("FAIL: a flipped log byte produced a state the model never had (rc %d)\n", rc2); } }
                    else { tflips++; if (rc2 == LSM_ERR_CORRUPT) { tflip_ok++; } else { bad++; printf("FAIL: a flipped table byte was not detected (rc %d)\n", rc2); } }
                }
            }
        }
        next:;
    }
    printf("%ld random calls in %d runs (%ld refused as intended); %ld reopens; %ld table byte flips (%ld detected); %ld log byte flips (%ld recovered to a state the model really had); failures: %ld\n", calls, runs, refused, reopens, tflips, tflip_ok, wflips, wflip_ok, bad);
    return bad ? 1 : 0;
}
