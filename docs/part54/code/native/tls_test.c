/* Chapter 54 host test: the TLS primitives and the client's API, worked by hand or from published vectors, under AddressSanitizer + UBSan. The values from RFC 5869, RFC 7748 and the GCM specification were written down from those documents and ALSO
 * match Python's `cryptography` library (see diff_prims.py). PASS/FAIL per line; exit status 1 on any FAIL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../054_tls.h"
static int pass_n, fail_n;
static void check(const char *name, int ok) { printf("%s %s\n", ok ? "PASS" : "FAIL", name); if (ok) { pass_n++; } else { fail_n++; } }
static uint32_t hx(const char *s, uint8_t *o) { uint32_t n = 0; for (; s[0] && s[1]; s += 2) { unsigned v; sscanf(s, "%2x", &v); o[n++] = (uint8_t)v; } return n; }
static int eqhex(const uint8_t *p, uint32_t n, const char *h) { uint8_t w[600]; return hx(h, w) == n && !memcmp(p, w, n); }
int main(void) {
    uint8_t a[300], b[300], c[300], o[600], o2[600];
    printf("== 1. X25519 (RFC 7748) ==\n");
    hx("a546e36bf0527c9d3b16154b82465edd62144c0ac1fc5a18506a2244ba449ac4", a); hx("e6db6867583030db3594c1a424b15f7c726624ec26b3353b10a903a6d0ab1c4c", b); tls_x25519(o, a, b);
    check("RFC 7748 section 5.2, first test vector", eqhex(o, 32, "c3da55379de9c6908e94ea4df28d084f32eccf03491c71f754b4075577a28552"));
    hx("4b66e9d4d1b4673c5ad22691957d6af5c11b6421e0ea01d42ca4169e7918ba0d", a); hx("e5210f12786811d3f4b7959d0538ae2c31dbe7106fc03c3efc4cd549c715a493", b); tls_x25519(o, a, b);
    check("RFC 7748 section 5.2, second test vector (the high bit of the point is ignored)", eqhex(o, 32, "95cbde9476e8907d7aade45cb4b873f88b595a68799fa152e6f8f7647aac7957"));
    memset(a, 0, 32); a[0] = 9; memcpy(b, a, 32); tls_x25519(o, a, b); check("RFC 7748 section 5.2, iterated once from k = u = 9", eqhex(o, 32, "422c8e7a6227d7bca1350b3e2bb7279f7897b87bb6854b783c60e80311ae3079"));
    { uint8_t k[32] = {9}, u[32] = {9}, t[32]; for (int i = 0; i < 1000; i++) { tls_x25519(t, k, u); memcpy(u, k, 32); memcpy(k, t, 32); } check("RFC 7748 section 5.2, iterated 1,000 times", eqhex(k, 32, "684cf59ba83309552800ef566f2f4d3c1c3887c49360e3875f2eb94d99532c51")); }
    { uint8_t ap[32], bp[32], base[32] = {9}, s1[32], s2[32]; hx("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a", a); hx("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb", b); tls_x25519(ap, a, base); tls_x25519(bp, b, base); tls_x25519(s1, a, bp); tls_x25519(s2, b, ap);
      check("RFC 7748 section 6.1: Alice's and Bob's public keys, and both compute the same shared secret", eqhex(ap, 32, "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a") && eqhex(bp, 32, "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f") && !memcmp(s1, s2, 32) && eqhex(s1, 32, "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742")); }
    printf("== 2. HKDF (RFC 5869 test case 1) and the TLS labels ==\n");
    hx("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b", a); hx("000102030405060708090a0b0c", b); tls_hkdf_extract(b, 13, a, 22, o);
    check("HKDF-Extract: PRK = 077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5", eqhex(o, 32, "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5"));
    tls_hkdf_extract(0, 0, a, 22, o); tls_hkdf_extract((const uint8_t[32]){0}, 32, a, 22, o2); check("HKDF-Extract with no salt equals a salt of 32 zero bytes (RFC 5869 section 2.2)", !memcmp(o, o2, 32));
    { uint8_t zeros[32] = {0}, early[32]; tls_hkdf_extract(0, 0, zeros, 32, early); check("TLS 1.3 early secret without a PSK = 33ad0a1c607ec03b09e6cd9893680ce210adf300aa1f2660e1b22e10f170f92a", eqhex(early, 32, "33ad0a1c607ec03b09e6cd9893680ce210adf300aa1f2660e1b22e10f170f92a"));
      uint8_t eh[32]; sha256_hash(0, 0, eh); check("SHA-256 of the empty string = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", eqhex(eh, 32, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
      uint8_t d[32]; tls_derive_secret(early, "derived", eh, d); check("Derive-Secret(early, \"derived\", \"\") = 6f2615a108c702c5678f54fc9dbab69716c076189c48250cebeac3576c3611ba (the value in the RFC 8448 trace)", eqhex(d, 32, "6f2615a108c702c5678f54fc9dbab69716c076189c48250cebeac3576c3611ba")); }
    check("an over-long label (250 bytes) is a caller bug answered with zeros, not a buffer overflow", ({ char lab[260]; memset(lab, 'a', 250); lab[250] = 0; memset(o, 0xAA, 32); tls_hkdf_expand_label(a, lab, 0, 0, o, 32); o[0] == 0 && o[31] == 0; }));
    printf("== 3. AES-128-GCM (the GCM specification's test cases 1, 2 and 4) ==\n");
    memset(a, 0, 16); memset(b, 0, 12); tls_gcm_seal(a, b, 0, 0, 0, 0, o); check("test case 1: empty plaintext, tag 58e2fccefa7e3061367f1d57a4e7455a", eqhex(o, 16, "58e2fccefa7e3061367f1d57a4e7455a"));
    memset(c, 0, 16); tls_gcm_seal(a, b, 0, 0, c, 16, o); check("test case 2: sixteen zero bytes -> 0388dace60b6a392f328c2b971b2fe78, tag ab6e47d42cec13bdf53a67b21257bddf", eqhex(o, 32, "0388dace60b6a392f328c2b971b2fe78ab6e47d42cec13bdf53a67b21257bddf"));
    { uint8_t key[16], iv[12], aad[20], pt[60], ct[76]; hx("feffe9928665731c6d6a8f9467308308", key); hx("cafebabefacedbaddecaf888", iv); hx("feedfacedeadbeeffeedfacedeadbeefabaddad2", aad);
      hx("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39", pt); tls_gcm_seal(key, iv, aad, 20, pt, 60, ct);
      check("test case 4: 60 bytes of plaintext and 20 of AAD", eqhex(ct, 76, "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091" "5bc94fbc3221a5db94fae95ae7121a47"));
      uint8_t back[60]; check("and it opens again", tls_gcm_open(key, iv, aad, 20, ct, 76, back) == 0 && !memcmp(back, pt, 60)); ct[75] ^= 1; memset(back, 0xAA, 60); check("with one tag bit flipped it is refused and NO plaintext is released (the output is zeroed)", tls_gcm_open(key, iv, aad, 20, ct, 76, back) != 0 && back[0] == 0 && back[59] == 0); }
    check("a ciphertext shorter than the 16-byte tag is refused", tls_gcm_open(a, b, 0, 0, c, 15, o) != 0);
    printf("== 4. the client's API ==\n");
    { static tls_t t; uint8_t rnd[32] = {1}, priv[32] = {2}, rec[1000], out[100], app[100]; uint32_t l = 0, ol = 0, al = 0;
      tls_init(&t); check("a record before the ClientHello is refused", tls_receive(&t, (const uint8_t *)"\x16\x03\x03\0\1\0", 6, out, sizeof out, &ol, app, sizeof app, &al) == TLS_ERR_STATE);
      check("tls_send and tls_close before the handshake are refused", tls_send(&t, app, 1, rec, sizeof rec, &l) == TLS_ERR_STATE && tls_close(&t, rec, sizeof rec, &l) == TLS_ERR_STATE);
      check("an empty server name is refused", tls_client_hello(&t, rnd, priv, "", rec, sizeof rec, &l) == TLS_ERR_ARG && t.state == TLS_ST_START);
      check("a ClientHello needs 201 bytes for the name \"server\": 200 is refused with TLS_ERR_SPACE", tls_client_hello(&t, rnd, priv, "server", rec, 200, &l) == TLS_ERR_SPACE);
      tls_init(&t); check("a ClientHello is built (a record of type 22, version 0x0301, 201 bytes)", tls_client_hello(&t, rnd, priv, "server", rec, sizeof rec, &l) == 0 && l == 201 && rec[0] == 22 && rec[1] == 3 && rec[2] == 1 && rec[3] == 0 && rec[4] == 196 && rec[5] == 1);
      check("its random is at offset 11 and its X25519 key share at offset 114, where RFC 8446 puts them (checked against the recorded RFC 8448 ClientHello in the trace test)", !memcmp(rec + 11, rnd, 32) && !memcmp(rec + 114, t.pub, 32));
      check("a second ClientHello on the same connection is refused", tls_client_hello(&t, rnd, priv, "server", rec, sizeof rec, &l) == TLS_ERR_STATE);
      tls_init(&t); t.state = TLS_ST_CONNECTED; t.wseq = 0xFFFFFFFFFFFFFFFFull; check("a send with the sequence number at 2^64 - 1 is refused, never wrapped", tls_send(&t, app, 1, rec, sizeof rec, &l) == TLS_ERR_SEQ);
      t.wseq = 0; check("a 16,384-byte send is accepted, a 16,385-byte send is not", ({ static uint8_t big[20000], r2[20000]; uint32_t l2; tls_send(&t, big, 16384, r2, sizeof r2, &l2) == 0 && l2 == 16384 + 22 && tls_send(&t, big, 16385, r2, sizeof r2, &l2) == TLS_ERR_ARG; }));
      check("a send into a too-small buffer is refused with TLS_ERR_SPACE and consumes no sequence number", ({ uint64_t before = t.wseq; uint32_t l2; int rc = tls_send(&t, app, 10, rec, 20, &l2); rc == TLS_ERR_SPACE && t.wseq == before; })); }
    printf("\n%d passed, %d failed\n", pass_n, fail_n); return fail_n ? 1 : 0;
}
