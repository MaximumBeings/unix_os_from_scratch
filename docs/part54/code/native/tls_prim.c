/* Chapter 54 host tool: the TLS client's primitives behind a line protocol, for the differential test against an independent library (Python's `cryptography`). Built from the SAME 054_tls.c the kernel links, with AddressSanitizer + UBSan.
 * One command per line, hex arguments ("-" is empty), one result line each:
 *   x25519 SCALAR POINT | extract SALT IKM | hkdfl SECRET LABEL CONTEXT LEN | gcm_seal KEY IV AAD PLAINTEXT | gcm_open KEY IV AAD CIPHERTEXT+TAG | pss N E SIG MHASH */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../054_tls.h"
static uint32_t hx(const char *s, uint8_t *o) { if (!strcmp(s, "-")) { return 0; } uint32_t n = 0; for (; s[0] && s[1]; s += 2) { unsigned v; sscanf(s, "%2x", &v); o[n++] = (uint8_t)v; } return n; }
static void hp(const uint8_t *p, uint32_t n) { for (uint32_t i = 0; i < n; i++) { printf("%02x", p[i]); } if (!n) { printf("-"); } }
int main(void) {
    static char line[100000]; static uint8_t a[40000], b[40000], c[40000], d[40000], o[40000];
    while (fgets(line, sizeof line, stdin)) {
        char *w[6] = {0}; int nw = 0; for (char *t = strtok(line, " \r\n"); t && nw < 6; t = strtok(0, " \r\n")) { w[nw++] = t; } if (!nw) { continue; }
        if (!strcmp(w[0], "x25519") && nw == 3) { hx(w[1], a); hx(w[2], b); tls_x25519(o, a, b); hp(o, 32); }
        else if (!strcmp(w[0], "extract") && nw == 3) { uint32_t sl = hx(w[1], a), il = hx(w[2], b); tls_hkdf_extract(a, sl, b, il, o); hp(o, 32); }
        else if (!strcmp(w[0], "hkdfl") && nw == 5) { hx(w[1], a); uint32_t cl = hx(w[3], b); uint32_t len = (uint32_t)atoi(w[4]); tls_hkdf_expand_label(a, w[2], b, cl, o, len); hp(o, len); }
        else if (!strcmp(w[0], "gcm_seal") && nw == 5) { hx(w[1], a); hx(w[2], b); uint32_t al = hx(w[3], c), pl = hx(w[4], d); tls_gcm_seal(a, b, c, al, d, pl, o); hp(o, pl + 16); }
        else if (!strcmp(w[0], "gcm_open") && nw == 5) { hx(w[1], a); hx(w[2], b); uint32_t al = hx(w[3], c), cl = hx(w[4], d); int rc = tls_gcm_open(a, b, c, al, d, cl, o); if (rc) { printf("bad"); } else { printf("ok "); hp(o, cl - 16); } }
        else if (!strcmp(w[0], "pss") && nw == 5) { uint32_t nl = hx(w[1], a); uint32_t el = hx(w[2], b); uint32_t e = 0; for (uint32_t i = 0; i < el; i++) { e = (e << 8) | b[i]; } uint32_t sl = hx(w[3], c); hx(w[4], d); printf("%s", tls_rsa_pss_verify(a, nl, e, c, sl, d) == 0 ? "ok" : "bad"); }
        else { printf("?"); }
        printf("\n"); fflush(stdout);
    }
    return 0;
}
