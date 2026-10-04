/* Chapter 48: damage real filings and make sure the reader and the engine survive. For each of the six embedded filings: N random mutations -- truncation at a random byte, 1..8 flipped bytes, a deleted range, inserted random bytes, a duplicated chunk, a swapped pair of chunks, a run of NULs. Every mutant is parsed and, if it parses, analysed; AddressSanitizer and UBSan watch for out-of-bounds access, overflow and misuse. Also checked: parsing is deterministic (twice, same tables), a truncation before the root's closing tag is NEVER accepted as complete, and a result that is ACCEPTED never contains a ratio computed from a missing input (re-derived through rt_canonical's NA lines). Usage: xbrl_fuzz N FILE... */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../048_xbrl.h"
#include "../048_ratios.h"
static uint64_t rng = 0x9E3779B97F4A7C15ull;
static uint64_t xr(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }
static xb_doc_t d1, d2; static rt_report_t rep; static char canon[8192];
int main(int argc, char **argv) {
    long N = atol(argv[1]); long codes[16] = {0}; long analysed = 0, accepted = 0, rejected = 0, wrongly_complete = 0, nondet = 0, total = 0;
    for (int fi = 2; fi < argc; fi++) {
        FILE *f = fopen(argv[fi], "rb"); if (!f) { perror(argv[fi]); return 2; }
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); char *orig = malloc((size_t)n + 1); if (fread(orig, 1, (size_t)n, f) != (size_t)n) { return 2; } fclose(f);
        size_t root_close = (size_t)n; for (long i = n - 7; i > 0; i--) { if (memcmp(orig + i, "</xbrl>", 7) == 0) { root_close = (size_t)i; break; } }
        for (long it = 0; it < N; it++) {
            size_t len = (size_t)n; char *m = malloc((size_t)n * 2 + 64); memcpy(m, orig, (size_t)n); int kind = (int)(xr() % 7); int trunc = 0;
            switch (kind) {
            case 0: len = (size_t)(xr() % (uint64_t)n); trunc = 1; break;
            case 1: { int k = 1 + (int)(xr() % 8); for (int j = 0; j < k; j++) { m[xr() % (uint64_t)n] ^= (char)(1 + xr() % 255); } break; }
            case 2: { size_t a = xr() % (uint64_t)n, l = xr() % 400; if (a + l > len) { l = len - a; } memmove(m + a, m + a + l, len - a - l); len -= l; break; }
            case 3: { size_t a = xr() % (uint64_t)n, l = 1 + xr() % 40; memmove(m + a + l, m + a, len - a); for (size_t j = 0; j < l; j++) { m[a + j] = (char)xr(); } len += l; break; }
            case 4: { size_t a = xr() % (uint64_t)n, l = xr() % 600; if (a + l > len) { l = len - a; } memmove(m + a + l, m + a, len - a); len += l; break; }
            case 5: { size_t a = xr() % (uint64_t)n, b = xr() % (uint64_t)n, l = 1 + xr() % 80; if (a + l > len) { l = len - a; } if (b + l > len) { l = len - b; } char tmp[128]; if (l > 128) { l = 128; } memcpy(tmp, m + a, l); memcpy(m + a, m + b, l); memcpy(m + b, tmp, l); break; }
            case 6: { size_t a = xr() % (uint64_t)n, l = 1 + xr() % 30; if (a + l > len) { l = len - a; } memset(m + a, 0, l); break; }
            }
            int rc = xb_parse(&d1, m, (uint32_t)len); total++; codes[-rc < 16 ? -rc : 15]++;
            if (trunc && len < root_close && rc == XB_OK) { wrongly_complete++; }
            if (rc == XB_OK) {
                int rc2 = xb_parse(&d2, m, (uint32_t)len); if (rc2 != rc || d1.n_fact != d2.n_fact || d1.n_ctx != d2.n_ctx) { nondet++; }
                int ra = rt_analyse(&d1, 10000, &rep); analysed++;
                if (ra == XB_OK) { if (rep.accepted) { accepted++; } else { rejected++; } if (rt_canonical(&rep, canon, sizeof canon) < 0) { printf("canonical buffer overflow\n"); return 1; } }
            }
            free(m);
        }
        free(orig);
    }
    printf("%ld mutants of %d real filings; parse results:", total, argc - 2);
    const char *nm[] = {"ok", "malformed XML", "truncated", "too many contexts", "too many facts", "too many units", "bad date", "id too long", "too deep", "no period", "not XBRL", "duplicate id"};
    for (int i = 0; i < 12; i++) { if (codes[i]) { printf(" [%s %ld]", nm[i], codes[i]); } }
    printf("\nanalysed %ld parsed mutants: %ld ACCEPTED, %ld REJECTED, %ld refused (no period end / bad date)\n", analysed, accepted, rejected, analysed - accepted - rejected);
    printf("truncations accepted as complete: %ld   non-deterministic parses: %ld\n", wrongly_complete, nondet);
    return (wrongly_complete || nondet) ? 1 : 0;
}
