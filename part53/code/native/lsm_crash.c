/* Chapter 53: the CRASH TEST. A seeded workload of puts, deletes, flushes and compactions is run on the RAM file system once without a crash to learn its total cost C (bytes written plus one per operation) and the digest of the correct state after each acknowledged
 * operation. Then for EVERY crash point B = 0, STRIDE, 2*STRIDE ... C the same workload is run again with a budget of B, so the file system dies in the middle of whatever write, append or delete is in flight there (torn: a prefix of the bytes survives). The surviving files are
 * cloned and a fresh store is opened on them. The recovered state must be exactly the state after the k acknowledged operations, or after k+1 (the one that was in flight); open() must succeed; the store must keep working: three more writes, a reopen, and the digest must equal the model.
 * Every STRIDE2-th crash point is also crashed AGAIN during recovery itself (a second budget over every byte recovery writes), and the second recovery must still land in the same allowed set. Usage: lsm_crash SEED OPS STRIDE STRIDE2 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ramfs.h"
#include "../053_sha256.h"
#define MAXOPS 600
typedef struct { uint8_t k[LSM_MAX_KEY], v[LSM_MAX_VAL]; uint32_t kl, vl; } kv_t;
static kv_t M[256]; static int nm; /* the model: sorted live pairs */
static uint32_t rs;
static uint32_t rnd(void) { rs = rs * 1664525u + 1013904223u; return rs >> 8; }
static int cmpkv(const kv_t *a, const kv_t *b) { uint32_t n = a->kl < b->kl ? a->kl : b->kl; int c = memcmp(a->k, b->k, n); if (c) { return c; } return a->kl == b->kl ? 0 : (a->kl < b->kl ? -1 : 1); }
static void m_put(const uint8_t *k, uint32_t kl, const uint8_t *v, uint32_t vl) { kv_t e; memset(&e, 0, sizeof e); memcpy(e.k, k, kl); e.kl = kl; memcpy(e.v, v, vl); e.vl = vl; int i = 0; while (i < nm && cmpkv(&M[i], &e) < 0) { i++; } if (i < nm && cmpkv(&M[i], &e) == 0) { M[i] = e; return; } memmove(&M[i + 1], &M[i], (nm - i) * sizeof e); M[i] = e; nm++; }
static void m_del(const uint8_t *k, uint32_t kl) { kv_t e; memset(&e, 0, sizeof e); memcpy(e.k, k, kl); e.kl = kl; for (int i = 0; i < nm; i++) { if (cmpkv(&M[i], &e) == 0) { memmove(&M[i], &M[i + 1], (nm - i - 1) * sizeof e); nm--; return; } } }
static void m_digest(uint8_t out[32]) { sha256_ctx_t h; sha256_init(&h); for (int i = 0; i < nm; i++) { uint8_t hd[2] = {(uint8_t)M[i].kl, (uint8_t)M[i].vl}; sha256_update(&h, hd, 2); sha256_update(&h, M[i].k, M[i].kl); sha256_update(&h, M[i].v, M[i].vl); } sha256_final(&h, out); }
typedef struct { int kind; /* 0 put, 1 del, 2 flush, 3 compact */ uint8_t k[LSM_MAX_KEY], v[LSM_MAX_VAL]; uint32_t kl, vl; } op_t;
static op_t OPS[MAXOPS]; static int nops;
static void make_ops(uint32_t seed, int n) { rs = seed; nops = n; for (int i = 0; i < n; i++) { op_t *o = &OPS[i]; uint32_t c = rnd() % 100; o->kind = c < 62 ? 0 : (c < 86 ? 1 : (c < 96 ? 2 : 3)); uint32_t id = rnd() % 45; o->kl = 2 + (id % 3 == 0 ? 20 : 0); memset(o->k, 'a', o->kl); o->k[0] = (uint8_t)('A' + id % 26); o->k[1] = (uint8_t)('0' + id / 26); o->vl = (rnd() % 5 == 0) ? LSM_MAX_VAL : rnd() % 30; for (uint32_t j = 0; j < o->vl; j++) { o->v[j] = (uint8_t)rnd(); } } }
static uint8_t DG[MAXOPS + 2][32]; /* DG[j] = digest of the model after j acknowledged put/delete operations */
static lsm_t S, S2; static ramfs_t RF, RF2, RF3, RF4;
static int apply(lsm_t *s, const op_t *o) { switch (o->kind) { case 0: return lsm_put(s, o->k, o->kl, o->v, o->vl); case 1: return lsm_delete(s, o->k, o->kl); case 2: return lsm_flush(s); default: return lsm_compact(s); } }
static int is_logical(const op_t *o) { return o->kind < 2; }
static void mmodel(const op_t *o) { if (o->kind == 0) { m_put(o->k, o->kl, o->v, o->vl); } else if (o->kind == 1) { m_del(o->k, o->kl); } }
int main(int argc, char **argv) {
    if (argc < 5) { return 2; } uint32_t seed = (uint32_t)atol(argv[1]); int n = atoi(argv[2]), stride = atoi(argv[3]), stride2 = atoi(argv[4]); make_ops(seed, n);
    nm = 0; m_digest(DG[0]); int logical = 0; rf_init(&RF); lsm_fs_t fs = rf_fs(&RF); if (lsm_open(&S, &fs)) { printf("FAIL: open on an empty file system\n"); return 1; }
    for (int i = 0; i < nops; i++) { if (apply(&S, &OPS[i])) { printf("FAIL: op %d failed without a crash\n", i); return 1; } if (is_logical(&OPS[i])) { mmodel(&OPS[i]); logical++; m_digest(DG[logical]); } }
    long total = RF.spent; uint32_t fl = S.st.flushes, cp = S.st.compactions; long tests = 0, fails = 0, at_k = 0, at_k1 = 0, nested = 0, nested_fail = 0, torn = 0, orph = 0;
    printf("workload: %d operations (%d puts/deletes), cost %ld units, %u flushes, %u compactions; crash points tried: every %d-th of 0..%ld\n", nops, logical, total, (unsigned)fl, (unsigned)cp, stride, total);
    for (long B = 0; B <= total; B += stride) {
        rf_init(&RF); RF.budget = B; fs = rf_fs(&RF); if (lsm_open(&S, &fs)) { printf("FAIL: open before the crash B=%ld\n", B); fails++; continue; }
        int acked = 0; for (int i = 0; i < nops; i++) { int rc = apply(&S, &OPS[i]); if (rc) { break; } if (is_logical(&OPS[i])) { acked++; } }
        rf_clone(&RF2, &RF); lsm_fs_t fs2 = rf_fs(&RF2); int rc = lsm_open(&S2, &fs2); tests++;
        uint8_t d[32]; uint32_t cnt;
        if (rc || lsm_digest(&S2, d, &cnt)) { printf("FAIL B=%ld: recovery returned %d\n", B, rc); fails++; continue; }
        int k0 = !memcmp(d, DG[acked], 32), k1 = acked + 1 <= logical && !memcmp(d, DG[acked + 1], 32);
        if (!k0 && !k1) { printf("FAIL B=%ld: after %d acknowledged operations the recovered state matches neither it nor the next (count %u)\n", B, acked, (unsigned)cnt); fails++; continue; }
        if (S2.st.wal_cut_bytes) { torn++; } if (S2.st.orphans_deleted) { orph++; }
        if (k0) { at_k++; } else { at_k1++; }
        int rec = k0 ? acked : acked + 1; /* the store keeps working: write three more, reopen, compare with the model */
        nm = 0; for (int i = 0, lg = 0; i < nops && lg < rec; i++) { if (is_logical(&OPS[i])) { mmodel(&OPS[i]); lg++; } }
        int added = 0; for (int i = 0; i < nops && added < 3; i++) { if (is_logical(&OPS[i])) { if (apply(&S2, &OPS[i])) { printf("FAIL B=%ld: write after recovery failed\n", B); fails++; break; } mmodel(&OPS[i]); added++; } }
        rf_clone(&RF3, &RF2); lsm_fs_t fs3 = rf_fs(&RF3); if (lsm_open(&S, &fs3) || lsm_digest(&S, d, &cnt)) { printf("FAIL B=%ld: reopen after continuing\n", B); fails++; continue; }
        uint8_t md[32]; m_digest(md); if (memcmp(md, d, 32)) { printf("FAIL B=%ld: state after continuing and reopening differs from the model\n", B); fails++; continue; }
        if (stride2 > 0 && (B / stride) % stride2 == 0) { /* crash AGAIN during recovery, at every byte recovery writes */
            rf_clone(&RF3, &RF); lsm_fs_t f3 = rf_fs(&RF3); RF3.budget = 1000000000L; lsm_open(&S2, &f3); long R = RF3.spent; /* cost of one recovery */
            for (long B2 = 0; B2 <= R; B2++) {
                rf_clone(&RF3, &RF); RF3.budget = B2; f3 = rf_fs(&RF3); lsm_open(&S2, &f3); rf_clone(&RF4, &RF3); lsm_fs_t f4 = rf_fs(&RF4); nested++;
                if (lsm_open(&S2, &f4) || lsm_digest(&S2, d, &cnt)) { printf("FAIL B=%ld B2=%ld: second recovery returned an error\n", B, B2); nested_fail++; continue; }
                if (memcmp(d, DG[rec], 32)) { printf("FAIL B=%ld B2=%ld: the state after a crash during recovery differs\n", B, B2); nested_fail++; }
            }
        }
    }
    printf("%ld crash points: %ld recovered to the state after the acknowledged operations, %ld to the state including the one in flight, %ld failures\n", tests, at_k, at_k1, fails);
    printf("  of them %ld had a torn log tail cut, %ld had orphan table files deleted\n%ld second crashes during recovery, %ld failures\n", torn, orph, nested, nested_fail);
    return fails || nested_fail ? 1 : 0;
}
