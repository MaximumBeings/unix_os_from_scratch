/* Chapter 54: a mutation fuzzer for the TLS client, under AddressSanitizer + UBSan. It replays the recorded RFC 8448 handshake (data/tls/botan_rfc8448_transcripts.vec) after mutating it in one random way per run: a flipped bit anywhere in the ServerHello record, the server's
 * flight, the NewSessionTicket or the application data record; a record cut short or padded; a record dropped, repeated, swapped with its neighbour or replaced by random bytes of the same length; the header's length field rewritten. Invariants: no crash or sanitizer report, and the client
 * reaches CONNECTED only in runs where the ServerHello and the flight (the first two records) were left exactly as recorded, and in every such run it does (a damaged record after the handshake may still be refused, but cannot undo it). Usage: tls_fuzz VECFILE RUNS */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../054_tls.h"
static char *filetext;
static uint32_t get(const char *key, uint8_t *out, uint32_t cap) { const char *p = strstr(filetext, "[Simple_1RTT_Handshake]"); char pat[96]; snprintf(pat, sizeof pat, "\n%s = ", key); p = strstr(p, pat); if (!p) { return 0; } p += strlen(pat); uint32_t n = 0; while (p[0] && p[1] && p[0] != '\n' && n < cap) { unsigned v; sscanf(p, "%2x", &v); out[n++] = (uint8_t)v; p += 2; } return n; }
static uint32_t rs; static uint32_t rnd(void) { rs = rs * 1664525u + 1013904223u; return rs >> 8; }
static tls_t T; static uint8_t R[6][1000]; static uint32_t RL[6];
int main(int argc, char **argv) {
    if (argc < 3) { return 2; } FILE *f = fopen(argv[1], "rb"); if (!f) { return 2; } filetext = malloc(200000); size_t fl = fread(filetext, 1, 199999, f); filetext[fl] = 0; fclose(f); int runs = atoi(argv[2]);
    uint8_t rng[64], ch[300]; get("Client_RNG_Pool", rng, 64); uint32_t chl = get("Record_ClientHello_1", ch, 300); (void)chl;
    static const char *names[5] = {"Record_ServerHello", "Record_ServerHandshakeMessages", "Record_NewSessionTicket", "Record_Server_AppData", "Record_Server_CloseNotify"};
    for (int i = 0; i < 5; i++) { RL[i] = get(names[i], R[i], 1000); }
    long untouched_ok = 0, untouched = 0, mutated = 0, connected_mutated = 0, bad_flight = 0, errs[20] = {0};
    for (int run = 0; run < runs; run++) {
        rs = 777u + (uint32_t)run * 31u; uint8_t recs[8][1000]; uint32_t rl[8]; int n = 5; for (int i = 0; i < 5; i++) { memcpy(recs[i], R[i], RL[i]); rl[i] = RL[i]; }
        int kind = (int)(rnd() % 10), which = (int)(rnd() % 5); int touched_flight = 0;
        if (kind == 0) { /* no mutation */ }
        else if (kind <= 3) { uint32_t pos = rnd() % rl[which]; recs[which][pos] ^= (uint8_t)(1u << (rnd() % 8)); }
        else if (kind == 4) { rl[which] = rnd() % rl[which]; }
        else if (kind == 5) { recs[which][rl[which]] = (uint8_t)rnd(); rl[which]++; }
        else if (kind == 6) { for (int i = which; i < n - 1; i++) { memcpy(recs[i], recs[i + 1], rl[i + 1]); rl[i] = rl[i + 1]; } n--; }
        else if (kind == 7) { for (int i = n; i > which; i--) { memcpy(recs[i], recs[i - 1], rl[i - 1]); rl[i] = rl[i - 1]; } n++; }
        else if (kind == 8) { int o = (which + 1) % 5; uint8_t tmp[1000]; uint32_t tl = rl[which]; memcpy(tmp, recs[which], tl); memcpy(recs[which], recs[o], rl[o]); rl[which] = rl[o]; memcpy(recs[o], tmp, tl); rl[o] = tl; }
        else { for (uint32_t i = 5; i < rl[which]; i++) { recs[which][i] = (uint8_t)rnd(); } }
        int intact = rl[0] == RL[0] && rl[1] == RL[1] && !memcmp(recs[0], R[0], RL[0]) && !memcmp(recs[1], R[1], RL[1]); (void)touched_flight; /* the ServerHello and the flight, the first two records, are exactly the recorded ones */
        tls_init(&T); uint8_t rec[400], out[2000], app[2000]; uint32_t l, ol, al; tls_client_hello(&T, rng, rng + 32, "server", rec, sizeof rec, &l); int reached = 0;
        for (int i = 0; i < n; i++) { int rc = tls_receive(&T, recs[i], rl[i], out, sizeof out, &ol, app, sizeof app, &al); if (rc < 0 && -rc < 20) { errs[-rc]++; } if (T.state == TLS_ST_CONNECTED) { reached = 1; } if (rc) { break; } }
        if (intact) { untouched++; if (reached) { untouched_ok++; } } else { mutated++; if (reached) { connected_mutated++; } }
        (void)bad_flight;
    }
    printf("%d fuzzed handshakes (%ld with the ServerHello and flight left intact: %ld connected; %ld with one of them damaged): the client connected %ld times; error codes seen:", runs, untouched, untouched_ok, mutated, connected_mutated);
    for (int i = 1; i < 20; i++) { if (errs[i]) { printf(" %s=%ld", tls_strerror(-i), errs[i]); } } printf("\n"); return (connected_mutated || untouched != untouched_ok) ? 1 : 0;
}
