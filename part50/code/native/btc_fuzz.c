/* Chapter 50: damage real and synthetic blocks and make sure the validator survives -- and that one PROPERTY always holds: a block that has been changed in ANY way (a flipped bit, a deleted, inserted, duplicated or swapped range, a
 * truncation) is never VALID. Every byte of a block is covered by something: the header by the proof of work, the transactions by the Merkle root, the witness data by the witness commitment, the lengths by the parser. So a mutant that
 * is not byte-for-byte identical to the original must be refused (a parse error or a reason), never VALID. AddressSanitizer and UBSan watch for out-of-bounds access and overflow. Usage: btc_fuzz N FILE... (file names starting 'syn' use the easy limit) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../050_btc.h"
static uint64_t rng = 0x9E3779B97F4A7C15ull;
static uint64_t xr(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }
static btc_block_t B;
int main(int argc, char **argv) {
    long N = atol(argv[1]), total = 0, unchanged = 0, parse_err = 0, refused = 0, valid_changed = 0, valid_unchanged = 0, header_only = 0; long reasons[32] = {0}; const char *names[32]; int nn = 0;
    for (int fi = 2; fi < argc; fi++) {
        FILE *f = fopen(argv[fi], "rb"); if (!f) { perror(argv[fi]); return 2; }
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); uint8_t *orig = malloc((size_t)n + 1); if (fread(orig, 1, (size_t)n, f) != (size_t)n) { return 2; } fclose(f);
        const char *base = strrchr(argv[fi], '/') ? strrchr(argv[fi], '/') + 1 : argv[fi]; uint32_t limit = !strncmp(base, "syn", 3) ? 0x207fffffu : 0x1d00ffffu;
        for (long it = 0; it < N; it++) {
            size_t len = (size_t)n; uint8_t *m = malloc((size_t)n * 2 + 64); memcpy(m, orig, (size_t)n); int kind = (int)(xr() % 8);
            switch (kind) {
            case 0: len = (size_t)(xr() % (uint64_t)n); break;
            case 1: { int k = 1 + (int)(xr() % 4); for (int j = 0; j < k; j++) { m[xr() % (uint64_t)n] ^= (uint8_t)(1u << (xr() % 8)); } break; }
            case 2: { size_t a = xr() % (uint64_t)n, l = xr() % 200; if (a + l > len) { l = len - a; } memmove(m + a, m + a + l, len - a - l); len -= l; break; }
            case 3: { size_t a = xr() % (uint64_t)n, l = 1 + xr() % 40; memmove(m + a + l, m + a, len - a); for (size_t j = 0; j < l; j++) { m[a + j] = (uint8_t)xr(); } len += l; break; }
            case 4: { size_t a = xr() % (uint64_t)n, l = xr() % 300; if (a + l > len) { l = len - a; } memmove(m + a + l, m + a, len - a); len += l; break; }
            case 5: { size_t a = xr() % (uint64_t)n, b = xr() % (uint64_t)n, l = 1 + xr() % 60; if (a + l > len) { l = len - a; } if (b + l > len) { l = len - b; } uint8_t tmp[128]; if (l > 128) { l = 128; } memcpy(tmp, m + a, l); memcpy(m + a, m + b, l); memcpy(m + b, tmp, l); break; }
            case 6: { size_t a = xr() % (uint64_t)n, l = 1 + xr() % 20; if (a + l > len) { l = len - a; } memset(m + a, 0, l); break; }
            case 7: { m[xr() % (uint64_t)n] = (uint8_t)xr(); break; }
            }
            total++; int same = (len == (size_t)n && !memcmp(m, orig, (size_t)n)); if (same) { unchanged++; }
            int rc = btc_block_parse(&B, m, (uint32_t)len, limit);
            if (rc != BTC_OK) { parse_err++; }
            else if (B.reason) { refused++; int k = -1; for (int q = 0; q < nn; q++) { if (names[q] == B.reason) { k = q; } } if (k < 0 && nn < 32) { names[nn] = B.reason; k = nn++; } if (k >= 0) { reasons[k]++; } }
            else if (same) { valid_unchanged++; } else if (limit == 0x207fffffu && len == (size_t)n && !memcmp(m + 80, orig + 80, (size_t)n - 80)) { header_only++; /* at the EASY limit a different header can be a different valid block: its proof of work is a coin toss */ } else { valid_changed++; printf("A CHANGED BLOCK IS VALID: %s kind %d len %zu\n", base, kind, len); }
            free(m);
        }
        free(orig);
    }
    printf("%ld mutants of %d files: %ld byte-for-byte unchanged (still valid: %ld); of the %ld that changed: %ld refused while parsing, %ld refused by a rule, %ld VALID (not counting %ld synthetic blocks whose only change was inside the 80-byte header: at the easy limit another header can be another valid block)\n", total, argc - 2, unchanged, valid_unchanged, total - unchanged, parse_err, refused, valid_changed, header_only);
    printf("rules that caught mutants:"); for (int q = 0; q < nn; q++) { printf(" [%s %ld]", names[q], reasons[q]); } printf("\n");
    return valid_changed ? 1 : 0;
}
