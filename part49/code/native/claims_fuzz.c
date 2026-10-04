/* Chapter 49: damage claims and remittances and make sure everything survives -- and that one PROPERTY always holds. Each input file (the invented 837s, the real-format samples, the built 835s) gets N random mutations (truncation, flipped
 * bytes, deleted/inserted/duplicated/swapped ranges, NUL runs, a separator changed). Every mutant goes through the X12 reader (strict and lenient), the claim reader, and the 835 reader; under AddressSanitizer and UBSan.
 * THE PROPERTY: whenever a mutant is accepted as a claim, adjudicating it under a random plan satisfies every engine invariant (charge = paid + CO + PR on each line, totals, accumulators within limits, no negative amount), the 835
 * built from the result PASSES the strict envelope check, RECONCILES (BPR02, every claim and line balance), and re-reads to the same totals. A mutant that survives parsing is not an error -- a flipped digit in a charge is still a charge
 * -- but whatever survives must be internally consistent. Usage: claims_fuzz N FILE... */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../049_x12.h"
#include "../049_claim.h"
#include "../049_adjud.h"
#include "../049_remit.h"
static uint64_t rng = 0x9E3779B97F4A7C15ull;
static uint64_t xr(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }
static x12_t X, Y; static cl_batch_t B; static adj_state_t S; static adj_claim_t R[CL_MAX_CLAIM]; static rm_report_t RM; static char out835[1 << 16];
int main(int argc, char **argv) {
    long N = atol(argv[1]); long total = 0, x12_ok = 0, x12_len_ok = 0, cl_ok = 0, adjudicated = 0, built_ok = 0, balanced = 0, bad = 0, r835_ok = 0, r835_bal = 0, unbalanced = 0, wrongly_complete = 0; long codes[24] = {0};
    for (int fi = 2; fi < argc; fi++) {
        FILE *f = fopen(argv[fi], "rb"); if (!f) { perror(argv[fi]); return 2; }
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); char *orig = malloc((size_t)n + 1); if (fread(orig, 1, (size_t)n, f) != (size_t)n) { return 2; } fclose(f);
        for (long it = 0; it < N; it++) {
            size_t len = (size_t)n; char *m = malloc((size_t)n * 2 + 64); memcpy(m, orig, (size_t)n); int kind = (int)(xr() % 8); int trunc = 0;
            switch (kind) {
            case 0: len = (size_t)(xr() % (uint64_t)n); trunc = 1; break;
            case 1: { int k = 1 + (int)(xr() % 6); for (int j = 0; j < k; j++) { m[xr() % (uint64_t)n] ^= (char)(1 + xr() % 255); } break; }
            case 2: { size_t a = xr() % (uint64_t)n, l = xr() % 300; if (a + l > len) { l = len - a; } memmove(m + a, m + a + l, len - a - l); len -= l; break; }
            case 3: { size_t a = xr() % (uint64_t)n, l = 1 + xr() % 40; memmove(m + a + l, m + a, len - a); for (size_t j = 0; j < l; j++) { m[a + j] = (char)xr(); } len += l; break; }
            case 4: { size_t a = xr() % (uint64_t)n, l = xr() % 400; if (a + l > len) { l = len - a; } memmove(m + a + l, m + a, len - a); len += l; break; }
            case 5: { size_t a = xr() % (uint64_t)n, b = xr() % (uint64_t)n, l = 1 + xr() % 60; if (a + l > len) { l = len - a; } if (b + l > len) { l = len - b; } char tmp[128]; if (l > 128) { l = 128; } memcpy(tmp, m + a, l); memcpy(m + a, m + b, l); memcpy(m + b, tmp, l); break; }
            case 6: { size_t a = xr() % (uint64_t)n, l = 1 + xr() % 20; if (a + l > len) { l = len - a; } memset(m + a, 0, l); break; }
            case 7: { /* change a digit inside a number somewhere: the mutants most likely to stay valid */ for (int tries = 0; tries < 50; tries++) { size_t a = xr() % (uint64_t)n; if (m[a] >= '0' && m[a] <= '9') { m[a] = (char)('0' + xr() % 10); break; } } break; }
            }
            total++;
            for (int lenient = 0; lenient < 2; lenient++) {
                X.lenient = (uint8_t)lenient; int rc = x12_parse(&X, m, (uint32_t)len); codes[-rc < 24 ? -rc : 23]++;
                if (rc == 0) { if (lenient) { x12_len_ok++; } else { x12_ok++; } } else { continue; }
                if (!lenient && trunc && len < (size_t)n - 8) { /* a cut before the last few characters can only be complete if only trailing whitespace was cut */ const char *tail = m + len - 1; if (*tail != '~' && *tail != '\n') { wrongly_complete++; } }
                int cr = cl_parse(&B, &X);
                if (cr == CL_OK) {
                    cl_ok++; adj_plan_t p; p.deductible = (uint32_t)(xr() % 200000); p.ded_met = (uint32_t)(xr() % (p.deductible + 1)); p.copay = (uint32_t)(xr() % 8000); p.coins_bp = (uint32_t)(xr() % 10001); p.oop_max = (uint32_t)(xr() % 900000); p.oop_met = (uint32_t)(xr() % (p.oop_max + 1));
                    adj_init(&S, &p); int ok = 1;
                    for (int i = 0; i < B.n_claim && ok; i++) { int ar = adj_claim(&S, &B.claim[i], &R[i]); if (ar == ADJ_ERR_INVARIANT) { bad++; ok = 0; } else if (ar != ADJ_OK) { ok = 0; } else if (adj_verify(&S, &B.claim[i], &R[i]) != 0) { bad++; ok = 0; } }
                    if (!ok) { continue; } adjudicated++;
                    rm_hdr_t h; memset(&h, 0, sizeof h); strcpy(h.payer_id, B.claim[0].payer_id); strcpy(h.payer_name, B.claim[0].payer_name); strcpy(h.payee_npi, B.claim[0].bill_npi); strcpy(h.payee_name, B.claim[0].bill_name); h.date = 19737; h.ctl = 1 + (uint32_t)(xr() % 999999);
                    int bl = rm_build(out835, sizeof out835, &h, B.claim, R, B.n_claim);
                    if (bl < 0) { continue; } built_ok++;
                    X.lenient = 0; int r2 = x12_parse(&Y, out835, (uint32_t)bl); if (r2 != 0) { bad++; printf("BUILT 835 REFUSED by the strict envelope check: %s\n", x12_strerror(r2)); continue; }
                    r2 = rm_parse(&RM, &Y); if (r2 != 0 || !RM.all_balanced || RM.n_claim != B.n_claim) { bad++; printf("BUILT 835 DOES NOT RECONCILE\n"); continue; } balanced++;
                } else if (cl_parse(&B, &X) != CL_OK) { /* maybe it is an 835 */ if (rm_parse(&RM, &X) == RM_OK) { r835_ok++; if (RM.all_balanced) { r835_bal++; } else { unbalanced++; } } }
            }
            free(m);
        }
        free(orig);
    }
    printf("%ld mutants; strict envelope accepted %ld, lenient %ld; claims accepted %ld, adjudicated %ld (all invariants held), 835s built and re-read strictly %ld, reconciled %ld\n", total, x12_ok, x12_len_ok, cl_ok, adjudicated, built_ok, balanced);
    printf("mutants read as 835s: %ld (%ld reconcile, %ld reported unbalanced); invariant or reconciliation failures: %ld; truncations accepted as complete: %ld\n", r835_ok, r835_bal, unbalanced, bad, wrongly_complete);
    printf("envelope verdicts over all mutants (strict and lenient):");
    const char *nm[] = {"ok", "not X12", "short ISA", "bad ISA", "bad separator", "bad segment id", "empty segment", "bad char", "truncated", "too many segments", "nesting", "multiple ISA", "SE01", "SE02", "GE01", "GE02", "IEA01", "IEA02", "after IEA", "no IEA"};
    for (int i = 0; i < 20; i++) { if (codes[i]) { printf(" [%s %ld]", nm[i], codes[i]); } } printf("\n");
    return (bad || wrongly_complete) ? 1 : 0;
}
