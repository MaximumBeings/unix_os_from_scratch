/* Chapter 53 host test: the LSM store under AddressSanitizer + UBSan. Expected values were worked out on paper first (comments show the arithmetic). PASS/FAIL per line; exit status 1 on any FAIL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ramfs.h"
static int pass_n, fail_n;
static void check(const char *name, int ok) { printf("%s %s\n", ok ? "PASS" : "FAIL", name); if (ok) { pass_n++; } else { fail_n++; } }
static ramfs_t RF, RF2; static lsm_t S, T; static lsm_fs_t FS;
static void fresh(void) { rf_init(&RF); FS = rf_fs(&RF); lsm_open(&S, &FS); }
static int P(const char *k, const char *v) { return lsm_put(&S, (const uint8_t *)k, (uint32_t)strlen(k), (const uint8_t *)v, (uint32_t)strlen(v)); }
static int D(const char *k) { return lsm_delete(&S, (const uint8_t *)k, (uint32_t)strlen(k)); }
static int G(const char *k, char *out) { uint32_t n = 0; int rc = lsm_get(&S, (const uint8_t *)k, (uint32_t)strlen(k), (uint8_t *)out, 200, &n); if (rc == 1) { out[n] = 0; } return rc; }
static int is(const char *k, const char *v) { char b[256]; return G(k, b) == 1 && !strcmp(b, v); }
static int gone(const char *k) { char b[256]; return G(k, b) == 0; }
static void w32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static uint32_t u32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static void same_digest(lsm_t *a, lsm_t *b, int *eq) { uint8_t x[32], y[32]; lsm_digest(a, x, 0); lsm_digest(b, y, 0); *eq = !memcmp(x, y, 32); }
int main(void) {
    printf("== 1. CRC-32 and the exact bytes of the WAL ==\n");
    check("CRC-32 of \"123456789\" is the standard check value 0xCBF43926", lsm_crc32((const uint8_t *)"123456789", 9) == 0xCBF43926u);
    fresh(); P("ab", "xyz");
    /* record: crc u32 | plen u16 = 8 + 2 + 3 = 13 | seq u32 = 1 | tomb 0 | klen 2 | vlen u16 = 3 | "ab" | "xyz"  -> 6 + 13 = 19 bytes */
    rf_file_t *w = rf_find(&RF, "WAL");
    check("the first put writes one 19-byte WAL record: length 13, sequence 1, key \"ab\", value \"xyz\"", w && w->len == 19 && w->d[4] == 13 && w->d[5] == 0 && u32(w->d + 6) == 1 && w->d[10] == 0 && w->d[11] == 2 && w->d[12] == 3 && w->d[13] == 0 && !memcmp(w->d + 14, "abxyz", 5));
    check("its CRC-32 covers everything after the CRC field", w && u32(w->d) == lsm_crc32(w->d + 4, 15));
    printf("\n== 2. the exact bytes of a table ==\n");
    fresh(); P("a", "1"); P("b", "22"); D("c"); lsm_flush(&S); rf_file_t *t = rf_find(&RF, "T00001.SST");
    /* entries: a: 01 00 0100 01000000 'a' '1' (10 bytes); b: 01 00 0200 02000000 'b' '2' '2' (11); c tombstone: 01 01 0000 03000000 'c' (9) -> data_len 30.
       index: one entry (3 < 8): offset 0, klen 1, 'a' = 6 bytes. bloom: 3 keys x 10 bits = 30 bits -> 4 bytes -> minimum 8. footer 44. total 30 + 6 + 8 + 44 = 88 */
    check("the table file is 88 bytes: 30 of entries, 6 of index, 8 of Bloom filter, 44 of footer", t && t->len == 88);
    static const uint8_t want[30] = {1,0,1,0,1,0,0,0,'a','1', 1,0,2,0,2,0,0,0,'b','2','2', 1,1,0,0,3,0,0,0,'c'};
    check("the first 30 bytes are the three entries, c as a tombstone with sequence 3", t && !memcmp(t->d, want, 30));
    check("the index is [offset 0, key length 1, 'a']", t && u32(t->d + 30) == 0 && t->d[34] == 1 && t->d[35] == 'a');
    check("the footer: magic 'LSM1', 3 entries, data 30, index at 30 of length 6, Bloom at 36 of length 8, sequences 1..3", t && u32(t->d + 44) == 0x314D534Cu && u32(t->d + 48) == 3 && u32(t->d + 52) == 30 && u32(t->d + 56) == 30 && u32(t->d + 60) == 6 && u32(t->d + 64) == 36 && u32(t->d + 68) == 8 && u32(t->d + 72) == 1 && u32(t->d + 76) == 3);
    check("the footer ends with the CRC-32 of the whole file before it", t && u32(t->d + 84) == lsm_crc32(t->d, 84));
    {
        lsm_tab_t m; int ok = t && !lsm_table_parse(t->d, t->len, &m) && m.n == 3 && m.idx_n == 1 && m.minlen == 1 && m.minkey[0] == 'a' && m.maxkey[0] == 'c';
        check("lsm_table_parse accepts it and reads back 3 entries, 1 index entry, range a..c", ok);
        int det = 1; for (uint32_t i = 0; t && i < t->len; i++) { uint8_t save = t->d[i]; t->d[i] ^= 0x01; if (!lsm_table_parse(t->d, t->len, &m)) { det = 0; } t->d[i] = save; }
        check("flipping any single bit of any of the 88 bytes makes the table invalid", det);
        int cut = 1; for (uint32_t n = 0; t && n < t->len; n++) { if (!lsm_table_parse(t->d, n, &m)) { cut = 0; } } check("a table cut short at any length is invalid", cut);
    }
    printf("\n== 3. reads, overwrites, deletes ==\n");
    fresh(); P("apple", "red"); P("banana", "yellow"); P("apple", "green");
    check("the newest value wins in the memtable", is("apple", "green") && is("banana", "yellow") && gone("cherry"));
    lsm_flush(&S); P("apple", "blue"); check("a memtable value shadows a table value", is("apple", "blue"));
    lsm_flush(&S); check("a newer table shadows an older table", is("apple", "blue") && is("banana", "yellow") && S.ntab == 2);
    D("banana"); check("a delete hides the key (the tombstone lives in the memtable)", gone("banana"));
    lsm_flush(&S); check("a tombstone in a newer table hides a value in an older table", gone("banana") && is("apple", "blue") && S.ntab == 3);
    lsm_compact(&S); check("compaction drops the tombstone AND the value it hid: one table with exactly one entry", S.ntab == 1 && S.tab[0].level == 1 && S.tab[0].n == 1 && gone("banana") && is("apple", "blue"));
    check("an empty value is a real value, not a delete", P("e", "") == 0 && ({ char b[8]; uint32_t n = 99; int rc = lsm_get(&S, (const uint8_t *)"e", 1, (uint8_t *)b, 8, &n); rc == 1 && n == 0; }));
    printf("\n== 4. when the memtable and the log flush ==\n");
    fresh(); char k[16], v[101]; memset(v, 'v', 100); v[100] = 0; int f_at = 0;
    for (int i = 0; i < 12; i++) { sprintf(k, "k%02d", i); lsm_put(&S, (const uint8_t *)k, 3, (const uint8_t *)v, 100); if (!f_at && S.st.flushes) { f_at = i + 1; } }
    /* each entry is 8 + 3 + 100 = 111 bytes; nine fit (999), the tenth would make 1110 > 1024 -> flush BEFORE the tenth is logged: the flush happens during put number 10 */
    check("100-byte values: 9 entries (999 bytes) fit in the 1,024-byte memtable; the 10th put flushes the first 9", f_at == 10 && S.st.flushes == 1 && S.tab[0].n == 9 && S.nmem == 3);
    fresh(); for (int i = 0; i < 64; i++) { sprintf(k, "k%02d", i); P(k, ""); } check("64 tiny entries fit in the memtable without a flush", S.st.flushes == 0 && S.nmem == 64);
    P("kzz", ""); check("the 65th distinct key flushes the full memtable first", S.st.flushes == 1 && S.nmem == 1 && S.tab[0].n == 64);
    fresh(); int wf = 0; for (int i = 0; i < 40; i++) { lsm_put(&S, (const uint8_t *)"same", 4, (const uint8_t *)v, 100); if (!wf && S.st.flushes) { wf = i + 1; } }
    /* one key rewritten with 100 bytes: each WAL record is 6 + 8 + 4 + 100 = 118 bytes; 34 fit (4012), the 35th would reach 4130 > 4096 -> flush during put 35 although the memtable holds one entry */
    check("rewriting one key: the log (not the memtable) is full after 34 records of 118 bytes, so put 35 flushes", wf == 35 && S.tab[0].n == 1);
    fresh(); for (int i = 0; i < 4 * 64 + 1; i++) { sprintf(k, "k%03d", i); P(k, ""); }
    check("four flushes make four level-0 tables, which trigger a compaction into one level-1 table", S.st.compactions == 1 && S.ntab == 1 && S.tab[0].level == 1 && S.tab[0].n == 256);
    printf("\n== 5. Bloom filters and key ranges ==\n");
    fresh(); for (int i = 0; i < 300; i++) { sprintf(k, "key%04d", i); P(k, "x"); } lsm_flush(&S); lsm_compact(&S);
    int allfound = 1; for (int i = 0; i < 300; i++) { sprintf(k, "key%04d", i); if (!is(k, "x")) { allfound = 0; } } check("every one of 300 stored keys is found (a Bloom filter never has a false negative)", allfound);
    uint32_t b0 = S.st.block_reads, s0 = S.st.bloom_skips, fp = 0, r0 = S.st.range_skips; for (int i = 0; i < 100; i++) { sprintf(k, "zzz%04d", i); gone(k); }
    check("100 keys beyond the table's maximum key are skipped by the range check alone: no Bloom probe, no disk read", S.st.range_skips - r0 == 100 && S.st.block_reads == b0 && S.st.bloom_skips == s0);
    b0 = S.st.block_reads; s0 = S.st.bloom_skips; for (int i = 0; i < 1000; i++) { sprintf(k, "key%04d5", i % 299); gone(k); } fp = S.st.block_reads - b0; printf("     (measured: %u false positives in 1000 probes)\n", (unsigned)fp);
    /* 300 keys x 10 bits = 3,000 bits (375 bytes), 4 probes: expected false-positive rate about (1 - e^(-4*300/3000))^4 = 0.0118^... = 1.2 percent; allow up to 4 percent */
    check("1,000 absent keys inside the range: the Bloom filter saves at least 96 percent of the disk reads (the rest are false positives)", fp <= 40 && S.st.bloom_skips - s0 == 1000 - fp);
    printf("\n== 6. refusals ==\n");
    fresh(); uint32_t before = S.seq; uint8_t big[200] = {0};
    check("an empty key, a 25-byte key, a 101-byte value and a null key are refused with LSM_ERR_ARG", lsm_put(&S, big, 0, big, 1) == LSM_ERR_ARG && lsm_put(&S, big, 25, big, 1) == LSM_ERR_ARG && lsm_put(&S, big, 1, big, 101) == LSM_ERR_ARG && lsm_put(&S, 0, 1, big, 1) == LSM_ERR_ARG && lsm_delete(&S, big, 0) == LSM_ERR_ARG);
    check("the largest key (24 bytes) and value (100 bytes) are accepted", lsm_put(&S, big, 24, big, 100) == 0);
    check("a refused call logs nothing and changes no sequence number: only the one accepted put is in the log", S.seq == before + 1 && rf_find(&RF, "WAL")->len == 6 + 8 + 24 + 100);
    check("get with an empty key is refused", lsm_get(&S, big, 0, big, 1, 0) == LSM_ERR_ARG);
    printf("\n== 7. recovery ==\n");
    fresh(); P("a", "1"); P("b", "2"); P("c", "3"); uint32_t wl = rf_find(&RF, "WAL")->len; /* 3 records of 6 + 8 + 1 + 1 = 16 bytes: 48 */
    rf_clone(&RF2, &RF); FS = rf_fs(&RF2); lsm_open(&T, &FS);
    check("reopening replays the three logged operations", T.nmem == 3 && T.st.wal_replayed == 3 && T.seq == 3 && wl == 48 && ({ uint8_t o[8]; uint32_t n; lsm_get(&T, (const uint8_t *)"b", 1, o, 8, &n) == 1 && o[0] == '2'; }));
    rf_clone(&RF2, &RF); rf_find(&RF2, "WAL")->len = 40; FS = rf_fs(&RF2); lsm_open(&T, &FS);
    check("a log cut in the middle of its third record (40 of 48 bytes): two operations recovered, 8 bytes of torn tail cut", T.st.wal_replayed == 2 && T.st.wal_cut_bytes == 8 && T.ntab == 1 && T.nmem == 0);
    check("a torn log is emptied by flushing what was replayed, so it can be appended to again: the recovered two keys are now in a table and the log is empty", T.tab[0].n == 2 && rf_find(&RF2, "WAL")->len == 0);
    rf_clone(&RF2, &RF); rf_find(&RF2, "WAL")->d[16 + 15] ^= 0xFF; FS = rf_fs(&RF2); lsm_open(&T, &FS);
    check("a flipped byte in the second record: replay stops there, only the first operation survives (a record after damage is never trusted)", T.st.wal_replayed == 1 && T.st.wal_cut_bytes == 32);
    rf_clone(&RF2, &RF); { rf_file_t *f = rf_find(&RF2, "WAL"); memcpy(f->d + 16, f->d, 16); } FS = rf_fs(&RF2); lsm_open(&T, &FS);
    check("a record whose sequence number does not increase (a stale copy) ends the replay", T.st.wal_replayed == 1);
    fresh(); P("a", "1"); lsm_flush(&S); rf_clone(&RF2, &RF); FS = rf_fs(&RF2); lsm_open(&T, &FS);
    check("after a flush the manifest remembers the flushed sequence number", T.flushed_seq == 1 && T.seq == 1 && T.ntab == 1);
    fresh(); P("a", "1"); { rf_file_t *w0 = rf_find(&RF, "WAL"); uint8_t keep[64]; uint32_t kl = w0->len; memcpy(keep, w0->d, kl); lsm_flush(&S); rf_find(&RF, "WAL")->len = kl; memcpy(rf_find(&RF, "WAL")->d, keep, kl); }
    rf_clone(&RF2, &RF); FS = rf_fs(&RF2); lsm_open(&T, &FS);
    check("a crash after the manifest but before the log was emptied: the old record is skipped, not applied twice (replayed 0)", T.st.wal_replayed == 0 && T.nmem == 0 && T.ntab == 1);
    fresh(); P("a", "1"); lsm_flush(&S); P("b", "2"); lsm_flush(&S); rf_clone(&RF2, &RF);
    uint8_t junk[8] = {1}; { rf_file_t *o = rf_make(&RF2, "T00009.SST"); o->len = 8; memcpy(o->d, junk, 8); o = rf_make(&RF2, "T00003.SST"); o->len = 8; memcpy(o->d, junk, 8); } FS = rf_fs(&RF2); lsm_open(&T, &FS);
    check("table files the manifest does not list (T00003 = the next id, T00009) are orphans: T00003 is deleted, T00009 (beyond next_id) is not touched", T.st.orphans_deleted == 1 && !rf_find(&RF2, "T00003.SST") && rf_find(&RF2, "T00009.SST"));
    rf_clone(&RF2, &RF); rf_find(&RF2, "MANI0")->d[5] ^= 1; rf_find(&RF2, "MANI1")->d[5] ^= 1; FS = rf_fs(&RF2); lsm_open(&T, &FS);
    check("both manifest slots damaged: the store opens empty at generation 0 (and deletes the tables as orphans)", T.ntab == 0 && T.gen == 0);
    rf_clone(&RF2, &RF); { rf_file_t *a = rf_find(&RF2, "MANI0"), *b = rf_find(&RF2, "MANI1"); rf_file_t *newer = u32(a->d + 4) > u32(b->d + 4) ? a : b; newer->len -= 3; } FS = rf_fs(&RF2); lsm_open(&T, &FS);
    check("the newest manifest slot torn: the older slot is used (generation 1 of 2, one table)", T.gen == 1 && T.ntab == 1);
    rf_clone(&RF2, &RF); rf_find(&RF2, "T00001.SST")->d[3] ^= 1; FS = rf_fs(&RF2); int rc = lsm_open(&T, &FS);
    check("a listed table with a flipped byte is detected at open: LSM_ERR_CORRUPT", rc == LSM_ERR_CORRUPT);
    rf_clone(&RF2, &RF); rf_find(&RF2, "T00002.SST")->used = 0; FS = rf_fs(&RF2); rc = lsm_open(&T, &FS);
    check("a listed table that is missing is detected at open: LSM_ERR_CORRUPT", rc == LSM_ERR_CORRUPT);
    printf("\n== 8. the digest ==\n");
    fresh(); P("a", "1"); P("b", "2"); P("c", "3"); lsm_flush(&S); rf_init(&RF2); FS = rf_fs(&RF2); lsm_open(&T, &FS); lsm_put(&T, (const uint8_t *)"c", 1, (const uint8_t *)"3", 1); lsm_put(&T, (const uint8_t *)"b", 1, (const uint8_t *)"X", 1); lsm_put(&T, (const uint8_t *)"b", 1, (const uint8_t *)"2", 1); lsm_put(&T, (const uint8_t *)"a", 1, (const uint8_t *)"1", 1); lsm_put(&T, (const uint8_t *)"z", 1, (const uint8_t *)"9", 1); lsm_delete(&T, (const uint8_t *)"z", 1);
    int eq; same_digest(&S, &T, &eq); check("the digest depends only on the live pairs, not on the history, the tables or the memtable", eq);
    uint32_t cnt; uint8_t dg[32]; lsm_digest(&S, dg, &cnt); check("three live keys are counted", cnt == 3);
    printf("\n== 9. the table size limit ==\n");
    fresh(); int added = 0, rcf = 0; for (int i = 0; i < 700 && !rcf; i++) { sprintf(k, "q%04d", i); rcf = P(k, "v"); if (!rcf) { added++; } }
    int ok9 = 1; for (int i = 0; i < added; i++) { sprintf(k, "q%04d", i); if (!is(k, "v")) { ok9 = 0; } }
    check("when compaction would need more than 400 keys in one table the put is refused with LSM_ERR_FULL", rcf == LSM_ERR_FULL && added > 300 && added < 700);
    check("and every earlier acknowledged key is still readable", ok9);
    rf_clone(&RF2, &RF); FS = rf_fs(&RF2); rc = lsm_open(&T, &FS); int eq2; same_digest(&S, &T, &eq2);
    check("a reopen after the refusal still recovers", rc == 0);

    printf("\n== 10. memtable and log limits at the exact boundary ==\n");
    fresh(); { uint8_t kk[2] = {'a', 0}, vv[100]; memset(vv, 'v', 100); for (int i = 0; i < 9; i++) { kk[1] = (uint8_t)('0' + i); lsm_put(&S, kk, 2, vv, 100); } /* nine entries of 8 + 2 + 100 = 110: 990 bytes */
      kk[1] = '9'; lsm_put(&S, kk, 2, vv, 24); /* 8 + 2 + 24 = 34 -> exactly 1,024 */ }
    check("entries totalling exactly 1,024 bytes still fit in the memtable (the limit is 'more than 1,024')", S.st.flushes == 0 && S.nmem == 10 && S.membytes == 1024);
    { uint8_t kk[2] = {'b', '0'}; lsm_put(&S, kk, 2, (const uint8_t *)"", 0); } check("one more entry (10 bytes -> 1,034) flushes first", S.st.flushes == 1 && S.nmem == 1);
    fresh(); { uint8_t kk[2] = {'a', 0}, vv[100]; memset(vv, 'v', 100); for (int i = 0; i < 9; i++) { kk[1] = (uint8_t)('0' + i); lsm_put(&S, kk, 2, vv, 100); } kk[1] = '9'; lsm_put(&S, kk, 2, vv, 25); }
    check("entries totalling 1,025 bytes do not fit: the 10th put flushes the first nine", S.st.flushes == 1 && S.nmem == 1);
    fresh(); { uint8_t vv[100]; memset(vv, 'v', 100); for (int i = 0; i < 34; i++) { lsm_put(&S, (const uint8_t *)"same", 4, vv, 100); } /* 34 x 118 = 4,012 bytes of log */ lsm_put(&S, (const uint8_t *)"same", 4, vv, 66); }
    check("a record that brings the log to exactly 4,096 bytes (4,012 + 84) still fits: no flush", S.st.flushes == 0 && S.wal_bytes == 4096 && rf_find(&RF, "WAL")->len == 4096);
    { lsm_put(&S, (const uint8_t *)"same", 4, (const uint8_t *)"", 0); } check("the next record (4,096 + 18) flushes first", S.st.flushes == 1);
    printf("\n== 11. table and store limits at the exact boundary ==\n");
    fresh(); { char kq[16]; for (int i = 0; i < 400; i++) { sprintf(kq, "m%03d", i); P(kq, "v"); } lsm_flush(&S); int c1 = lsm_compact(&S); P("m400", "v"); lsm_flush(&S); int c2 = lsm_compact(&S);
      check("exactly 400 live keys compact into one table; a 401st key makes the compaction refuse with LSM_ERR_FULL", c1 == 0 && c2 == LSM_ERR_FULL); check("and the refusal changed nothing: all 401 keys are still readable", ({ int ok = 1; for (int i = 0; i <= 400; i++) { sprintf(kq, "m%03d", i); if (!is(kq, "v")) { ok = 0; } } ok; })); }
    fresh(); { char kq[16]; int maxt = 0, firstfull = 0; for (int i = 0; i < 3000; i++) { sprintf(kq, "n%04d", i); int rc9 = P(kq, "v"); if (rc9 == LSM_ERR_FULL && !firstfull) { firstfull = i; } if ((int)S.ntab > maxt) { maxt = (int)S.ntab; } }
      check("when compaction keeps refusing, tables pile up to exactly 12, and 2,500 more puts never produce a 13th table (the flush refuses)", maxt == LSM_MAX_TABLES && S.ntab == LSM_MAX_TABLES);
      rf_clone(&RF2, &RF); FS = rf_fs(&RF2); check("a store holding 12 tables reopens", lsm_open(&T, &FS) == 0 && T.ntab == LSM_MAX_TABLES); (void)firstfull; }
    printf("\n== 12. a compaction that leaves nothing ==\n");
    fresh(); P("a", "1"); P("b", "2"); lsm_flush(&S); D("a"); D("b"); lsm_flush(&S); lsm_compact(&S);
    check("every key deleted: the compaction leaves NO table at all, and its table files are gone", S.ntab == 0 && !rf_find(&RF, "T00001.SST") && !rf_find(&RF, "T00002.SST") && !rf_find(&RF, "T00003.SST") && gone("a") && gone("b"));
    rf_clone(&RF2, &RF); FS = rf_fs(&RF2); check("and the empty store reopens empty", lsm_open(&T, &FS) == 0 && T.ntab == 0 && T.nmem == 0);
    printf("\n== 13. crafted files whose checksums are VALID (so only the structural checks can catch them) ==\n");
    {
        static uint8_t im[8192]; lsm_tab_t m; struct { uint8_t kl, tomb; uint16_t vl, vbytes; uint32_t seq; uint8_t key[32]; } E[420];
        #define CRAFT(N, MINSEQ_ADD, SKIP_IDX_FIX) ({ uint32_t o = 0, io, il = 0, bl = ((N) * 10u + 7u) / 8u < 8u ? 8u : ((N) * 10u + 7u) / 8u; for (int i = 0; i < (N); i++) { im[o] = E[i].kl; im[o + 1] = E[i].tomb; im[o + 2] = (uint8_t)E[i].vl; im[o + 3] = (uint8_t)(E[i].vl >> 8); w32(im + o + 4, E[i].seq); memcpy(im + o + 8, E[i].key, E[i].kl); memset(im + o + 8 + E[i].kl, 'x', E[i].vbytes); o += 8u + E[i].kl + E[i].vbytes; } \
            io = o; for (int i = 0; i < (N); i += 8) { uint32_t eo = 0; for (int j = 0; j < i; j++) { eo += 8u + E[j].kl + E[j].vbytes; } w32(im + o, eo + (SKIP_IDX_FIX)); im[o + 4] = E[i].kl; memcpy(im + o + 5, E[i].key, E[i].kl); o += 5u + E[i].kl; il += 5u + E[i].kl; } \
            memset(im + o, 0, bl); o += bl; uint8_t *f = im + o; w32(f, 0x314D534Cu); w32(f + 4, (N)); w32(f + 8, io); w32(f + 12, io); w32(f + 16, il); w32(f + 20, io + il); w32(f + 24, bl); w32(f + 28, 1u + (MINSEQ_ADD)); w32(f + 32, (N)); w32(f + 36, 0); w32(f + 40, lsm_crc32(im, o + 40u)); o + 44u; })
        #define NORMAL(N) for (int i = 0; i < (N); i++) { E[i].kl = 4; E[i].tomb = 0; E[i].vl = 0; E[i].vbytes = 0; E[i].seq = (uint32_t)i + 1u; sprintf((char *)E[i].key, "k%03d", i % 1000); }
        NORMAL(20); uint32_t n1 = CRAFT(20, 0, 0); check("control: a crafted 20-entry table with a valid checksum is accepted", !lsm_table_parse(im, n1, &m) && m.n == 20 && m.idx_n == 3);
        NORMAL(20); n1 = CRAFT(20, 1, 0); check("the footer's minimum sequence number must match the entries", lsm_table_parse(im, n1, &m) != 0);
        NORMAL(20); n1 = CRAFT(20, 0, 1); check("an index entry with a wrong offset is rejected", lsm_table_parse(im, n1, &m) != 0);
        NORMAL(20); E[9].key[3] = E[8].key[3]; n1 = CRAFT(20, 0, 0); check("two entries with the same key (k008 twice) are rejected: keys must strictly ascend", lsm_table_parse(im, n1, &m) != 0);
        NORMAL(20); E[9].key[3] = '0'; E[9].key[2] = '0'; n1 = CRAFT(20, 0, 0); check("entries out of order are rejected", lsm_table_parse(im, n1, &m) != 0);
        NORMAL(20); E[0].tomb = 1; E[0].vl = 1; E[0].vbytes = 1; n1 = CRAFT(20, 0, 0); check("a tombstone that carries a value is rejected", lsm_table_parse(im, n1, &m) != 0);
        NORMAL(20); E[0].kl = 25; memset(E[0].key, 'a', 25); n1 = CRAFT(20, 0, 0); check("a 25-byte key is rejected", lsm_table_parse(im, n1, &m) != 0);
        NORMAL(20); E[0].vl = 0x0100; n1 = CRAFT(20, 0, 0); check("a value length with a non-zero high byte (256) is rejected, not read as 0", lsm_table_parse(im, n1, &m) != 0);
        NORMAL(20); E[0].vl = 101; E[0].vbytes = 101; n1 = CRAFT(20, 0, 0); check("a 101-byte value is rejected", lsm_table_parse(im, n1, &m) != 0);
        { uint32_t o = 0, bl = 8; memset(im, 0, bl); o = bl; uint8_t *f = im + o; w32(f, 0x314D534Cu); for (int q = 1; q < 10; q++) { w32(f + 4 * q, 0); } w32(f + 24, bl); w32(f + 28, 0xFFFFFFFFu); /* min_seq > max_seq: the only thing besides the entry count that could give an empty table away */ w32(f + 40, lsm_crc32(im, o + 40u)); check("a table of zero entries, with a valid checksum, is rejected", lsm_table_parse(im, o + 44u, &m) != 0); }
        for (int i = 0; i < 401; i++) { E[i].kl = 4; E[i].tomb = 0; E[i].vl = 0; E[i].vbytes = 0; E[i].seq = (uint32_t)i + 1u; sprintf((char *)E[i].key, "k%03d", i); }
        { static uint8_t big[8192]; uint32_t n2 = CRAFT(400, 0, 0); memcpy(big, im, n2); check("control: a crafted table of exactly 400 entries is accepted", !lsm_table_parse(big, n2, &m) && m.n == 400); n2 = CRAFT(401, 0, 0); check("a crafted table of 401 entries is rejected", lsm_table_parse(im, n2, &m) != 0); }
        /* manifests with valid checksums that name impossible tables */
        fresh(); P("a", "1"); lsm_flush(&S); rf_file_t *t1 = rf_find(&RF, "T00001.SST"); rf_file_t *cp = rf_make(&RF, "T00005.SST"); cp->len = t1->len; memcpy(cp->d, t1->d, t1->len); cp = rf_make(&RF, "T00000.SST"); cp->len = t1->len; memcpy(cp->d, t1->d, t1->len);
        { uint8_t mm[64]; for (int c = 0; c < 2; c++) { uint32_t id = c ? 0u : 5u; memcpy(mm, "MNFT", 4); w32(mm + 4, 9); w32(mm + 8, 3); w32(mm + 12, 1); mm[16] = 1; mm[17] = 0; w32(mm + 18, id); mm[22] = 0; w32(mm + 23, lsm_crc32(mm, 23));
          rf_clone(&RF2, &RF); rf_file_t *mf = rf_make(&RF2, "MANI1"); mf->len = 27; memcpy(mf->d, mm, 27); FS = rf_fs(&RF2); int rc3 = lsm_open(&T, &FS);
          check(c ? "a manifest (valid checksum) listing table id 0, though a valid table file T00000.SST exists, is refused" : "a manifest (valid checksum) listing table id 5 at next_id 3, though a valid table file T00005.SST exists, is refused", rc3 == LSM_ERR_CORRUPT); } }
    }
    printf("\n%d passed, %d failed\n", pass_n, fail_n); return fail_n ? 1 : 0;
}
