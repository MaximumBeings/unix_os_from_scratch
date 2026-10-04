/* Chapter 53 host tool, built from the SAME 053_lsm.c the kernel links, on the RAM file system with the crash injector. Reads a script (one command per line), prints one result line per command.
 *   put K V | del K | get K | flush | compact | open | crash N | recover | digest | stats | dump          (K, V: text, or x:HEX for binary; V "-" is the empty value)
 * `crash N` gives the file system a budget of N cost units (see ramfs.h); `recover` clones the surviving files into a fresh file system and opens a fresh store on it; `dump` prints every file (name, length, hex). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ramfs.h"
static ramfs_t fsA, fsB; static ramfs_t *cur = &fsA; static lsm_t S; static int opened;
static uint32_t tok(const char *t, uint8_t *o) { if (t[0] == 'x' && t[1] == ':') { uint32_t n = 0; for (const char *p = t + 2; p[0] && p[1]; p += 2) { unsigned v; sscanf(p, "%2x", &v); o[n++] = (uint8_t)v; } return n; } if (!strcmp(t, "-")) { return 0; } uint32_t n = (uint32_t)strlen(t); memcpy(o, t, n); return n; }
static void hexout(const uint8_t *p, uint32_t n) { for (uint32_t i = 0; i < n; i++) { printf("%02x", p[i]); } if (!n) { printf("-"); } }
static const char *rcn(int rc) { switch (rc) { case 0: return "ok"; case LSM_ERR_ARG: return "arg"; case LSM_ERR_FULL: return "full"; case LSM_ERR_IO: return "io"; case LSM_ERR_CORRUPT: return "corrupt"; } return "?"; }
static int cmpn(const void *a, const void *b) { return strcmp(((const rf_file_t *)a)->name, ((const rf_file_t *)b)->name); }
int main(int argc, char **argv) {
    if (argc < 2) { return 2; } FILE *f = fopen(argv[1], "r"); if (!f) { return 2; } char line[512]; rf_init(&fsA); lsm_fs_t fs = rf_fs(cur);
    int rc = lsm_open(&S, &fs); opened = rc == 0; printf("open %s\n", rcn(rc));
    while (fgets(line, sizeof line, f)) {
        char *w[4] = {0}; int nw = 0; for (char *t = strtok(line, " \r\n"); t && nw < 4; t = strtok(0, " \r\n")) { w[nw++] = t; } if (!nw) { continue; }
        uint8_t k[64], v[256]; uint32_t kl, vl;
        if (!strcmp(w[0], "put") && nw == 3) { kl = tok(w[1], k); vl = tok(w[2], v); rc = lsm_put(&S, k, kl, v, vl); printf("put %s %s\n", w[1], rcn(rc)); }
        else if (!strcmp(w[0], "del") && nw == 2) { kl = tok(w[1], k); rc = lsm_delete(&S, k, kl); printf("del %s %s\n", w[1], rcn(rc)); }
        else if (!strcmp(w[0], "get") && nw == 2) { kl = tok(w[1], k); uint32_t n = 0; rc = lsm_get(&S, k, kl, v, sizeof v, &n); if (rc == 1) { printf("get %s found ", w[1]); hexout(v, n); printf("\n"); } else { printf("get %s %s\n", w[1], rc == 0 ? "none" : rcn(rc)); } }
        else if (!strcmp(w[0], "flush")) { rc = lsm_flush(&S); printf("flush %s\n", rcn(rc)); }
        else if (!strcmp(w[0], "compact")) { rc = lsm_compact(&S); printf("compact %s\n", rcn(rc)); }
        else if (!strcmp(w[0], "open")) { rc = lsm_open(&S, &fs); printf("open %s\n", rcn(rc)); }
        else if (!strcmp(w[0], "crash") && nw == 2) { cur->budget = atol(w[1]); printf("crash armed\n"); }
        else if (!strcmp(w[0], "recover")) { ramfs_t *o = cur == &fsA ? &fsB : &fsA; rf_clone(o, cur); cur = o; fs = rf_fs(cur); rc = lsm_open(&S, &fs); printf("recover %s replayed %u cut %u orphans %u\n", rcn(rc), (unsigned)S.st.wal_replayed, (unsigned)S.st.wal_cut_bytes, (unsigned)S.st.orphans_deleted); }
        else if (!strcmp(w[0], "digest")) { uint8_t d[32]; uint32_t n = 0; rc = lsm_digest(&S, d, &n); printf("digest "); hexout(d, 32); printf(" %u\n", (unsigned)n); }
        else if (!strcmp(w[0], "stats")) { printf("stats puts %u deletes %u flushes %u compactions %u gets %u bloom_skips %u range_skips %u block_reads %u tables %u mem %u\n", (unsigned)S.st.puts, (unsigned)S.st.deletes, (unsigned)S.st.flushes, (unsigned)S.st.compactions, (unsigned)S.st.gets, (unsigned)S.st.bloom_skips, (unsigned)S.st.range_skips, (unsigned)S.st.block_reads, (unsigned)S.ntab, (unsigned)S.nmem); }
        else if (!strcmp(w[0], "dump")) { rf_file_t tmp[RF_FILES]; int n = 0; for (int i = 0; i < RF_FILES; i++) { if (cur->f[i].used) { tmp[n++] = cur->f[i]; } } qsort(tmp, n, sizeof tmp[0], cmpn); for (int i = 0; i < n; i++) { printf("file %s %u ", tmp[i].name, (unsigned)tmp[i].len); hexout(tmp[i].d, tmp[i].len); printf("\n"); } }
        else { printf("? %s\n", w[0]); }
    }
    (void)opened; return 0;
}
