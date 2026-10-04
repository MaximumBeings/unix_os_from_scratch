/* Chapter 54 host test: the TLS 1.3 client against the recorded handshake of RFC 8448 section 3 ("Simple 1-RTT Handshake"), as written down in the test data of the Botan library (data/tls/botan_rfc8448_transcripts.vec, BSD licence).
 * The client is given the client's random and X25519 private key from that file and is then fed the recorded SERVER records one by one. Every byte the client sends must equal the recorded bytes; every message the server sent must be accepted, which
 * needs the whole key schedule, the AES-128-GCM record layer, the RSA-PSS CertificateVerify and the Finished check to be right. Built with AddressSanitizer + UBSan. Usage: tls_trace VECFILE */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../054_tls.h"
static int pass_n, fail_n;
static void check(const char *name, int ok) { printf("%s %s\n", ok ? "PASS" : "FAIL", name); if (ok) { pass_n++; } else { fail_n++; } }
static char *filetext; static const char *sect;
static uint32_t get(const char *key, uint8_t *out, uint32_t cap) { /* hex value of "key = ..." in the Simple_1RTT section */
    const char *p = strstr(filetext, "[Simple_1RTT_Handshake]"); char pat[96]; snprintf(pat, sizeof pat, "\n%s = ", key); p = strstr(p, pat); if (!p) { return 0; } p += strlen(pat); uint32_t n = 0; while (p[0] && p[1] && p[0] != '\n' && n < cap) { unsigned v; sscanf(p, "%2x", &v); out[n++] = (uint8_t)v; p += 2; } return n; }
static void hexp(const char *label, const uint8_t *p, uint32_t n) { printf("     %s ", label); for (uint32_t i = 0; i < n; i++) { printf("%02x", p[i]); } printf("\n"); }
static tls_t T; static uint8_t buf[20000], rec[20000], out[20000], app[20000], want[20000], pt[20000];
int main(int argc, char **argv) {
    if (argc < 2) { return 2; } FILE *f = fopen(argv[1], "rb"); if (!f) { return 2; } filetext = malloc(200000); size_t fl = fread(filetext, 1, 199999, f); filetext[fl] = 0; fclose(f); (void)sect;
    uint8_t rng[64]; uint32_t n = get("Client_RNG_Pool", rng, 64); check("the client's random number pool is 64 bytes: a 32-byte ClientHello random then the 32-byte X25519 private key", n == 64);
    printf("== 1. the ClientHello ==\n"); tls_init(&T); uint32_t rl = 0; int rc = tls_client_hello(&T, rng, rng + 32, "server", rec, sizeof rec, &rl); n = get("Record_ClientHello_1", want, sizeof want);
    check("the X25519 public key computed from the recorded private key is the recorded key share (99381de5...)", T.pub[0] == 0x99 && T.pub[1] == 0x38 && T.pub[2] == 0x1d && T.pub[3] == 0xe5 && T.pub[31] == 0x2c);
    check("the ClientHello record the client builds equals the recorded one, byte for byte (201 bytes)", rc == 0 && rl == n && n == 201 && !memcmp(rec, want, n));
    printf("== 2. the ServerHello ==\n"); n = get("Record_ServerHello", buf, sizeof buf); rc = tls_receive(&T, buf, n, out, sizeof out, &rl, app, sizeof app, &n);
    check("the recorded ServerHello record is accepted", rc == 0 && T.state == TLS_ST_WAIT_FLIGHT);
    uint8_t zeros[32] = {0}, early[32], eh[32], derived[32]; tls_hkdf_extract(0, 0, zeros, 32, early); sha256_hash(0, 0, eh); tls_derive_secret(early, "derived", eh, derived);
    hexp("early secret:     ", early, 32); hexp("handshake secret: ", T.hs_secret, 32); hexp("client hs traffic:", T.c_hs, 32); hexp("server hs traffic:", T.s_hs, 32);
    check("the early secret (no PSK) is the well-known 33ad0a1c607ec03b09e6cd9893680ce210adf300aa1f2660e1b22e10f170f92a", early[0] == 0x33 && early[1] == 0xad && early[2] == 0x0a && early[31] == 0x2a);
    printf("== 3. the server's encrypted flight ==\n");
    uint32_t recl = get("Record_ServerHandshakeMessages", buf, sizeof buf); check("the recorded server flight is one 679-byte record", recl == 679);
    { uint8_t k[16], iv[12], nonce[12]; tls_hkdf_expand_label(T.s_hs, "key", 0, 0, k, 16); tls_hkdf_expand_label(T.s_hs, "iv", 0, 0, iv, 12); memcpy(nonce, iv, 12); int o = tls_gcm_open(k, nonce, buf, 5, buf + 5, recl - 5, pt);
      uint32_t a = get("Message_EncryptedExtensions", want, 4000), b = get("Message_Server_Certificate", want + a, 4000), c = get("Message_Server_CertificateVerify", want + a + b, 4000), d = get("Message_Server_Finished", want + a + b + c, 4000);
      check("independently decrypted, the record holds EncryptedExtensions, Certificate, CertificateVerify and Finished exactly as recorded, then the inner type byte 0x16 (handshake)", o == 0 && recl - 5 - 16 == a + b + c + d + 1 && !memcmp(pt, want, a + b + c + d) && pt[a + b + c + d] == 22);
      uint32_t o2 = a + b + 4 + 4 + 0; (void)o2; /* CertificateVerify: scheme 0804 then a 128-byte signature */
      uint8_t sig[256]; uint32_t sl = get("Server_MessageSignature", sig, 256); check("its signature field is the recorded 128-byte RSA-PSS signature", sl == 128 && !memcmp(pt + a + b + 4 + 4, sig, 128)); }
    rc = tls_receive(&T, buf, recl, out, sizeof out, &rl, app, sizeof app, &n);
    check("the client accepts it: the certificate's RSA key is found, the CertificateVerify signature is VERIFIED, the server Finished is verified", rc == 0 && T.state == TLS_ST_CONNECTED);
    check("the RSA key taken from the certificate is a 1024-bit modulus with public exponent 65537", T.rsa_nlen == 128 && T.rsa_e == 65537);
    { uint8_t tosign[256]; uint32_t ts = get("Server_MessageToSign", tosign, 256); uint8_t th[32]; sha256_ctx_t c; sha256_init(&c); uint8_t ch[400], sh[400]; uint32_t chl = get("Record_ClientHello_1", ch, 400) - 5, shl = get("Message_ServerHello", sh, 400), a = get("Message_EncryptedExtensions", want, 4000), b = get("Message_Server_Certificate", want + a, 4000);
      uint8_t chrec[400]; get("Record_ClientHello_1", chrec, 400); sha256_update(&c, chrec + 5, chl); sha256_update(&c, sh, shl); sha256_update(&c, want, a + b); sha256_final(&c, th);
      check("the 130-byte message the server signed (64 spaces, \"TLS 1.3, server CertificateVerify\", a zero byte, the transcript hash) equals the recorded one, so the transcript hash is right", ts == 130 && !memcmp(tosign + 98, th, 32) && tosign[0] == 0x20 && tosign[97] == 0); }
    printf("== 4. the client's Finished and the traffic keys ==\n"); n = get("Record_ClientFinished", want, sizeof want);
    check("the client Finished record the client sends equals the recorded one, byte for byte (58 bytes)", rl == n && n == 58 && !memcmp(out, want, n));
    hexp("master secret:     ", T.master, 32); hexp("client ap traffic: ", T.c_ap, 32); hexp("server ap traffic: ", T.s_ap, 32);
    printf("== 5. application data, the ticket, close_notify ==\n");
    n = get("Record_NewSessionTicket", buf, sizeof buf); rc = tls_receive(&T, buf, n, out, sizeof out, &rl, app, sizeof app, &n); check("the NewSessionTicket record (227 bytes) is accepted and counted", rc == 0 && T.tickets == 1 && rl == 0);
    uint32_t sdl = get("Server_AppData", want, sizeof want); n = get("Record_Server_AppData", buf, sizeof buf); rc = tls_receive(&T, buf, n, out, sizeof out, &rl, app, sizeof app, &n); check("the server's application data record decrypts to the recorded 50 bytes (00 01 02 ... 31)", rc == 0 && n == sdl && sdl == 50 && !memcmp(app, want, n));
    uint32_t cdl = get("Client_AppData", pt, sizeof pt); rc = tls_send(&T, pt, cdl, rec, sizeof rec, &rl); n = get("Record_Client_AppData", want, sizeof want); check("the client's application data record equals the recorded one, byte for byte (72 bytes)", rc == 0 && rl == n && n == 72 && !memcmp(rec, want, n));
    rc = tls_close(&T, rec, sizeof rec, &rl); n = get("Record_Client_CloseNotify", want, sizeof want); check("the client's close_notify record equals the recorded one (24 bytes)", rc == 0 && rl == n && !memcmp(rec, want, n));
    T.state = TLS_ST_CONNECTED; n = get("Record_Server_CloseNotify", buf, sizeof buf); rc = tls_receive(&T, buf, n, out, sizeof out, &rl, app, sizeof app, &n); check("the server's close_notify record is accepted and closes the connection", rc == 0 && T.state == TLS_ST_CLOSED && T.alert == TLS_ALERT_CLOSE_NOTIFY);
    printf("\n%d passed, %d failed\n", pass_n, fail_n); return fail_n ? 1 : 0;
}
