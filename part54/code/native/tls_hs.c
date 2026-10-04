/* Chapter 54 host tool: the TLS client behind a line protocol, for the differential test against the independent Python client and server (tls_ref.py). Built from the SAME 054_tls.c the kernel links, with AddressSanitizer + UBSan.
 *   reset | hello RANDOM PRIV SNI | rec RECORD | send DATA | close | secrets | setseq W R
 * Every command prints one line: "rc N alert A state S out HEX app HEX" (rc: see 054_tls.h; "-" is empty), or for hello/send/close "rc N rec HEX"; secrets prints seven hex strings. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../054_tls.h"
static uint32_t hx(const char *s, uint8_t *o) { if (!strcmp(s, "-")) { return 0; } uint32_t n = 0; for (; s[0] && s[1]; s += 2) { unsigned v; sscanf(s, "%2x", &v); o[n++] = (uint8_t)v; } return n; }
static void hp(const uint8_t *p, uint32_t n) { for (uint32_t i = 0; i < n; i++) { printf("%02x", p[i]); } if (!n) { printf("-"); } }
static tls_t T;
int main(void) {
    static char line[200000]; static uint8_t a[100000], b[100000], out[100000], app[100000];
    tls_init(&T);
    while (fgets(line, sizeof line, stdin)) {
        char *w[5] = {0}; int nw = 0; for (char *t = strtok(line, " \r\n"); t && nw < 5; t = strtok(0, " \r\n")) { w[nw++] = t; } if (!nw) { continue; }
        if (!strcmp(w[0], "reset")) { tls_init(&T); printf("ok"); }
        else if (!strcmp(w[0], "hello") && nw == 4) { uint8_t r[32], p[32]; hx(w[1], r); hx(w[2], p); uint32_t l = 0; int rc = tls_client_hello(&T, r, p, w[3], out, sizeof out, &l); printf("rc %d rec ", rc); hp(out, rc ? 0 : l); }
        else if (!strcmp(w[0], "rec") && nw == 2) { uint32_t n = hx(w[1], a), ol = 0, al = 0; int rc = tls_receive(&T, a, n, out, sizeof out, &ol, app, sizeof app, &al); printf("rc %d alert %d state %d out ", rc, T.alert, T.state); hp(out, ol); printf(" app "); hp(app, al); }
        else if (!strcmp(w[0], "send") && nw == 2) { uint32_t n = hx(w[1], a), l = 0; int rc = tls_send(&T, a, n, out, sizeof out, &l); printf("rc %d rec ", rc); hp(out, rc ? 0 : l); }
        else if (!strcmp(w[0], "close")) { uint32_t l = 0; int rc = tls_close(&T, out, sizeof out, &l); printf("rc %d rec ", rc); hp(out, rc ? 0 : l); }
        else if (!strcmp(w[0], "secrets")) { hp(T.hs_secret, 32); printf(" "); hp(T.c_hs, 32); printf(" "); hp(T.s_hs, 32); printf(" "); hp(T.master, 32); printf(" "); hp(T.c_ap, 32); printf(" "); hp(T.s_ap, 32); printf(" "); hp(T.th_sf, 32); }
        else if (!strcmp(w[0], "setseq") && nw == 3) { T.wseq = strtoull(w[1], 0, 16); T.rseq = strtoull(w[2], 0, 16); printf("ok"); }
        else { printf("?"); }
        printf("\n"); fflush(stdout);
    }
    return 0;
}
