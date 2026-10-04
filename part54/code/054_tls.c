/* Chapter 54: the TLS 1.3 client (see 054_tls.h). Freestanding: no libc, no libgcc (64-bit multiplication and shifts only; no 64-bit division). */
#include "054_tls.h"
#include "054_aes.h"
#include "054_hmac.h"

static void cpy(uint8_t *d, const uint8_t *s, uint32_t n) { for (uint32_t i = 0; i < n; i++) { d[i] = s[i]; } }
static void zero(uint8_t *d, uint32_t n) { for (uint32_t i = 0; i < n; i++) { d[i] = 0; } }
static int ct_eq(const uint8_t *a, const uint8_t *b, uint32_t n) { uint8_t d = 0; for (uint32_t i = 0; i < n; i++) { d |= (uint8_t)(a[i] ^ b[i]); } return d == 0; }
static uint32_t g16(const uint8_t *p) { return ((uint32_t)p[0] << 8) | p[1]; }
static uint32_t g24(const uint8_t *p) { return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2]; }
static void p16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void p24(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 16); p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)v; }

/* ---- X25519 (RFC 7748): the Montgomery ladder over GF(2^255 - 19) in sixteen 16-bit limbs, after the public-domain TweetNaCl formulation ---- */
typedef int64_t gf[16];
static void car25519(gf o) { for (int i = 0; i < 16; i++) { o[i] += 65536; int64_t c = o[i] >> 16; o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15); o[i] -= c * 65536; } }
static void sel25519(gf p, gf q, int b) { int64_t c = ~((int64_t)b - 1); for (int i = 0; i < 16; i++) { int64_t t = c & (p[i] ^ q[i]); p[i] ^= t; q[i] ^= t; } }
static void pack25519(uint8_t *o, const gf n) {
    gf m, t; for (int i = 0; i < 16; i++) { t[i] = n[i]; } car25519(t); car25519(t); car25519(t);
    for (int j = 0; j < 2; j++) { m[0] = t[0] - 0xffed; for (int i = 1; i < 15; i++) { m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1); m[i - 1] &= 0xffff; } m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1); int b = (int)((m[15] >> 16) & 1); m[14] &= 0xffff; sel25519(t, m, 1 - b); }
    for (int i = 0; i < 16; i++) { o[2 * i] = (uint8_t)(t[i] & 0xff); o[2 * i + 1] = (uint8_t)(t[i] >> 8); }
}
static void unpack25519(gf o, const uint8_t *n) { for (int i = 0; i < 16; i++) { o[i] = n[2 * i] + ((int64_t)n[2 * i + 1] * 256); } o[15] &= 0x7fff; }
static void fa(gf o, const gf a, const gf b) { for (int i = 0; i < 16; i++) { o[i] = a[i] + b[i]; } }
static void fz(gf o, const gf a, const gf b) { for (int i = 0; i < 16; i++) { o[i] = a[i] - b[i]; } }
static void fm(gf o, const gf a, const gf b) { int64_t t[31]; for (int i = 0; i < 31; i++) { t[i] = 0; } for (int i = 0; i < 16; i++) { for (int j = 0; j < 16; j++) { t[i + j] += a[i] * b[j]; } } for (int i = 0; i < 15; i++) { t[i] += 38 * t[i + 16]; } for (int i = 0; i < 16; i++) { o[i] = t[i]; } car25519(o); car25519(o); }
static void fs(gf o, const gf a) { fm(o, a, a); }
static void finv(gf o, const gf in) { gf c; for (int a = 0; a < 16; a++) { c[a] = in[a]; } for (int a = 253; a >= 0; a--) { fs(c, c); if (a != 2 && a != 4) { fm(c, c, in); } } for (int a = 0; a < 16; a++) { o[a] = c[a]; } }
void tls_x25519(uint8_t out[32], const uint8_t scalar[32], const uint8_t point[32]) {
    static const gf k121665 = {0xDB41, 1}; uint8_t z[32]; int64_t x[80]; gf a, b, c, d, e, f;
    for (int i = 0; i < 31; i++) { z[i] = scalar[i]; } z[31] = (uint8_t)((scalar[31] & 127) | 64); z[0] &= 248; unpack25519(x, point);
    for (int i = 0; i < 16; i++) { b[i] = x[i]; d[i] = a[i] = c[i] = 0; } a[0] = d[0] = 1;
    for (int i = 254; i >= 0; --i) { int r = (z[i >> 3] >> (i & 7)) & 1; sel25519(a, b, r); sel25519(c, d, r); fa(e, a, c); fz(a, a, c); fa(c, b, d); fz(b, b, d); fs(d, e); fs(f, a); fm(a, c, a); fm(c, b, e); fa(e, a, c); fz(a, a, c); fs(b, a); fz(c, d, f); fm(a, c, k121665); fa(a, a, d); fm(c, c, a); fm(a, d, f); fm(d, b, x); fs(b, e); sel25519(a, b, r); sel25519(c, d, r); }
    for (int i = 0; i < 16; i++) { x[i + 16] = a[i]; x[i + 32] = c[i]; x[i + 48] = b[i]; x[i + 64] = d[i]; }
    finv(x + 32, x + 32); fm(x + 16, x + 16, x + 32); pack25519(out, x + 16);
}

/* ---- HKDF (RFC 5869) and the TLS 1.3 label construction (RFC 8446 section 7.1) ---- */
void tls_hkdf_extract(const uint8_t *salt, uint32_t slen, const uint8_t *ikm, uint32_t ilen, uint8_t out[32]) { uint8_t z[32]; if (slen == 0) { zero(z, 32); salt = z; slen = 32; } hmac_sha256(salt, slen, ikm, ilen, out); }
static void hkdf_expand(const uint8_t prk[32], const uint8_t *info, uint32_t ilen, uint8_t *out, uint32_t olen) {
    uint8_t t[32], buf[32 + 520 + 1]; uint32_t tl = 0, o = 0; for (uint32_t c = 1; o < olen; c++) { cpy(buf, t, tl); cpy(buf + tl, info, ilen); buf[tl + ilen] = (uint8_t)c; hmac_sha256(prk, 32, buf, tl + ilen + 1, t); tl = 32; for (uint32_t i = 0; i < 32 && o < olen; i++) { out[o++] = t[i]; } }
}
void tls_hkdf_expand_label(const uint8_t secret[32], const char *label, const uint8_t *ctx, uint32_t clen, uint8_t *out, uint32_t olen) {
    uint8_t info[520]; uint32_t n = 0, ll = 0; while (label[ll]) { ll++; } if (ll > 249u || clen > 255u || olen > 255u * 32u || olen == 0) { zero(out, olen > 255u * 32u ? 0u : olen); return; } /* outside what the encoding can express: a caller bug, answered with zeros */ p16(info, olen); info[2] = (uint8_t)(6 + ll); n = 3; const char *pre = "tls13 "; for (int i = 0; i < 6; i++) { info[n++] = (uint8_t)pre[i]; } for (uint32_t i = 0; i < ll; i++) { info[n++] = (uint8_t)label[i]; }
    info[n++] = (uint8_t)clen; for (uint32_t i = 0; i < clen; i++) { info[n++] = ctx[i]; } hkdf_expand(secret, info, n, out, olen);
}
void tls_derive_secret(const uint8_t secret[32], const char *label, const uint8_t th[32], uint8_t out[32]) { tls_hkdf_expand_label(secret, label, th, 32, out, 32); }

/* ---- AES-128-GCM (NIST SP 800-38D) with a 96-bit nonce; GHASH multiplies bit by bit ---- */
static void gmul(uint8_t x[16], const uint8_t h[16]) {
    uint8_t z[16], v[16]; zero(z, 16); cpy(v, h, 16);
    for (int i = 0; i < 128; i++) { if ((x[i >> 3] >> (7 - (i & 7))) & 1) { for (int k = 0; k < 16; k++) { z[k] ^= v[k]; } } int lsb = v[15] & 1; for (int k = 15; k > 0; k--) { v[k] = (uint8_t)((v[k] >> 1) | (v[k - 1] << 7)); } v[0] >>= 1; if (lsb) { v[0] ^= 0xe1; } }
    cpy(x, z, 16);
}
static void ghash_update(uint8_t y[16], const uint8_t h[16], const uint8_t *d, uint32_t n) { while (n) { uint32_t k = n < 16 ? n : 16; for (uint32_t i = 0; i < k; i++) { y[i] ^= d[i]; } gmul(y, h); d += k; n -= k; } }
static void gcm_core(const uint8_t key[16], const uint8_t iv[12], const uint8_t *aad, uint32_t alen, const uint8_t *in, uint32_t n, uint8_t *out, const uint8_t *ghash_over, uint8_t tag[16]) {
    uint8_t rk[176], h[16], z[16], j0[16], y[16], ks[16], ctr[16]; aes128_key_expand(key, rk); zero(z, 16); aes128_encrypt_block(z, h, rk); cpy(j0, iv, 12); j0[12] = j0[13] = j0[14] = 0; j0[15] = 1; cpy(ctr, j0, 16);
    for (uint32_t off = 0; off < n; off += 16) { for (int k = 15; k >= 12; k--) { if (++ctr[k]) { break; } } aes128_encrypt_block(ctr, ks, rk); uint32_t k = n - off < 16 ? n - off : 16; for (uint32_t i = 0; i < k; i++) { out[off + i] = (uint8_t)(in[off + i] ^ ks[i]); } }
    zero(y, 16); ghash_update(y, h, aad, alen); ghash_update(y, h, ghash_over, n); uint8_t lb[16]; zero(lb, 16); uint64_t ab = (uint64_t)alen * 8u, cb = (uint64_t)n * 8u; for (int i = 0; i < 8; i++) { lb[7 - i] = (uint8_t)(ab >> (8 * i)); lb[15 - i] = (uint8_t)(cb >> (8 * i)); } for (int i = 0; i < 16; i++) { y[i] ^= lb[i]; } gmul(y, h);
    aes128_encrypt_block(j0, ks, rk); for (int i = 0; i < 16; i++) { tag[i] = (uint8_t)(y[i] ^ ks[i]); }
}
void tls_gcm_seal(const uint8_t key[16], const uint8_t iv[12], const uint8_t *aad, uint32_t alen, const uint8_t *pt, uint32_t plen, uint8_t *out) { uint8_t tag[16]; gcm_core(key, iv, aad, alen, pt, plen, out, out, tag); cpy(out + plen, tag, 16); }
int tls_gcm_open(const uint8_t key[16], const uint8_t iv[12], const uint8_t *aad, uint32_t alen, const uint8_t *ct, uint32_t clen, uint8_t *pt) {
    if (clen < 16) { return -1; } uint32_t n = clen - 16; uint8_t tag[16]; gcm_core(key, iv, aad, alen, ct, n, pt, ct, tag); if (!ct_eq(tag, ct + n, 16)) { zero(pt, n); return -1; } return 0;
}

/* ---- big numbers and RSA-PSS verification (RFC 8017 sections 8.1.2 and 9.1.2) ---- */
#define BN_MAX 64u /* limbs: 2048 bits */
static uint32_t bn_load(uint32_t *l, const uint8_t *be, uint32_t n) { uint32_t nl = (n + 3u) / 4u; for (uint32_t i = 0; i < BN_MAX; i++) { l[i] = 0; } for (uint32_t i = 0; i < n; i++) { l[i / 4u] |= (uint32_t)be[n - 1u - i] << (8u * (i % 4u)); } return nl; }
static int bn_cmp(const uint32_t *a, const uint32_t *b, uint32_t nl) { for (uint32_t i = nl; i > 0; i--) { if (a[i - 1] != b[i - 1]) { return a[i - 1] < b[i - 1] ? -1 : 1; } } return 0; }
static void bn_sub(uint32_t *a, const uint32_t *b, uint32_t nl) { uint32_t br = 0; for (uint32_t i = 0; i < nl; i++) { uint32_t x = a[i], y = b[i]; uint32_t d = x - y - br; br = (x < y || (x == y && br)) ? 1u : 0u; a[i] = d; } }
static void bn_addmod(uint32_t *r, const uint32_t *a, const uint32_t *b, const uint32_t *n, uint32_t nl) { uint32_t c = 0; for (uint32_t i = 0; i < nl; i++) { uint32_t s = a[i] + b[i]; uint32_t c1 = s < a[i]; uint32_t s2 = s + c; c1 |= s2 < s; r[i] = s2; c = c1; } if (c || bn_cmp(r, n, nl) >= 0) { bn_sub(r, n, nl); } }
static void bn_mulmod(uint32_t *r, const uint32_t *a, const uint32_t *b, const uint32_t *n, uint32_t nl) { uint32_t acc[BN_MAX]; for (uint32_t i = 0; i < nl; i++) { acc[i] = 0; } for (int bit = (int)nl * 32 - 1; bit >= 0; bit--) { bn_addmod(acc, acc, acc, n, nl); if ((b[bit / 32] >> (bit % 32)) & 1u) { bn_addmod(acc, acc, a, n, nl); } } for (uint32_t i = 0; i < nl; i++) { r[i] = acc[i]; } }
static void mgf1(const uint8_t seed[32], uint8_t *out, uint32_t olen) { uint8_t buf[36], h[32]; cpy(buf, seed, 32); for (uint32_t c = 0, o = 0; o < olen; c++) { buf[32] = (uint8_t)(c >> 24); buf[33] = (uint8_t)(c >> 16); buf[34] = (uint8_t)(c >> 8); buf[35] = (uint8_t)c; sha256_hash(buf, 36, h); for (uint32_t i = 0; i < 32 && o < olen; i++) { out[o++] = h[i]; } } }
int tls_rsa_pss_verify(const uint8_t *n, uint32_t nlen, uint32_t e, const uint8_t *sig, uint32_t slen, const uint8_t mhash[32]) {
    if (nlen < 128u || nlen > 256u || n[0] == 0 || (n[nlen - 1] & 1u) == 0 || e < 3 || (e & 1u) == 0 || slen != nlen) { return -1; }
    uint32_t N[BN_MAX], S[BN_MAX], R[BN_MAX]; uint32_t nl = bn_load(N, n, nlen); bn_load(S, sig, slen); if (bn_cmp(S, N, nl) >= 0) { return -1; }
    for (uint32_t i = 0; i < nl; i++) { R[i] = S[i]; } int top = 31; while (top > 0 && !((e >> top) & 1u)) { top--; } for (int bit = top - 1; bit >= 0; bit--) { bn_mulmod(R, R, R, N, nl); if ((e >> bit) & 1u) { bn_mulmod(R, R, S, N, nl); } }
    uint8_t em[256]; uint32_t modbits = nlen * 8u; uint32_t b = n[0]; while (!(b & 0x80u)) { b <<= 1; modbits--; } uint32_t embits = modbits - 1u, emlen = (embits + 7u) / 8u; /* emlen = nlen or nlen - 1 */
    for (uint32_t i = 0; i < nlen; i++) { em[nlen - 1u - i] = (uint8_t)(R[i / 4u] >> (8u * (i % 4u))); } const uint8_t *EM = em + (nlen - emlen);
    if (nlen > emlen && em[0] != 0) { return -1; } if (emlen < 32u + 32u + 2u || EM[emlen - 1] != 0xbc) { return -1; }
    uint32_t dblen = emlen - 32u - 1u; const uint8_t *H = EM + dblen; uint8_t db[256], mask[256]; mgf1(H, mask, dblen); for (uint32_t i = 0; i < dblen; i++) { db[i] = (uint8_t)(EM[i] ^ mask[i]); }
    uint32_t unused = 8u * emlen - embits; if (unused) { db[0] &= (uint8_t)(0xFFu >> unused); } if (EM[0] & (uint8_t)(0xFFu << (8u - unused)) && unused) { return -1; }
    uint32_t ps = dblen - 32u - 1u; for (uint32_t i = 0; i < ps; i++) { if (db[i] != 0) { return -1; } } if (db[ps] != 0x01) { return -1; }
    uint8_t mp[8 + 32 + 32], hh[32]; zero(mp, 8); cpy(mp + 8, mhash, 32); cpy(mp + 40, db + ps + 1, 32); sha256_hash(mp, 72, hh); return ct_eq(hh, H, 32) ? 0 : -1;
}

/* ---- DER: just enough to find the RSA public key in a certificate ---- */
static int der_tlv(const uint8_t *b, uint32_t len, uint32_t *pos, uint8_t *tag, uint32_t *vlen) { /* on success *pos is the start of the value */
    if (*pos + 2u > len) { return -1; } *tag = b[*pos]; uint32_t l = b[*pos + 1], p = *pos + 2u; if (l & 0x80u) { uint32_t nb = l & 0x7Fu; if (nb < 1 || nb > 2 || p + nb > len) { return -1; } l = 0; for (uint32_t i = 0; i < nb; i++) { l = (l << 8) | b[p++]; } } if (l > len - p) { return -1; } *pos = p; *vlen = l; return 0;
}
static int der_into(const uint8_t *b, uint32_t len, uint32_t *pos, uint8_t want, uint32_t *vlen) { uint8_t tag; if (der_tlv(b, len, pos, &tag, vlen) || tag != want) { return -1; } return 0; }
static int der_skip(const uint8_t *b, uint32_t len, uint32_t *pos, uint8_t want) { uint32_t v; if (der_into(b, len, pos, want, &v)) { return -1; } *pos += v; return 0; }
static int cert_rsa_key(const uint8_t *c, uint32_t len, uint8_t *n, uint32_t *nlen, uint32_t *e) {
    uint32_t p = 0, v; if (der_into(c, len, &p, 0x30, &v)) { return -1; } if (der_into(c, len, &p, 0x30, &v)) { return -1; } /* certificate, tbsCertificate */
    if (p < len && c[p] == 0xa0) { if (der_skip(c, len, &p, 0xa0)) { return -1; } } /* version */
    if (der_skip(c, len, &p, 0x02) || der_skip(c, len, &p, 0x30) || der_skip(c, len, &p, 0x30) || der_skip(c, len, &p, 0x30) || der_skip(c, len, &p, 0x30)) { return -1; } /* serial, signature alg, issuer, validity, subject */
    if (der_into(c, len, &p, 0x30, &v)) { return -1; } /* subjectPublicKeyInfo */
    uint32_t q = p; if (der_into(c, len, &q, 0x30, &v)) { return -1; } static const uint8_t oid[11] = {0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01}; if (v < 11 || q + 11 > len) { return -1; } for (int i = 0; i < 11; i++) { if (c[q + i] != oid[i]) { return -1; } } q += v;
    if (der_into(c, len, &q, 0x03, &v) || v < 1 || c[q] != 0) { return -1; } q++; if (der_into(c, len, &q, 0x30, &v)) { return -1; }
    uint8_t tag; uint32_t nl; if (der_tlv(c, len, &q, &tag, &nl) || tag != 0x02 || nl < 2) { return -1; } uint32_t ns = q; q += nl; while (nl > 0 && c[ns] == 0) { ns++; nl--; } if (nl < 128 || nl > 256) { return -1; } cpy(n, c + ns, nl); *nlen = nl;
    uint32_t el; if (der_tlv(c, len, &q, &tag, &el) || tag != 0x02 || el < 1 || el > 4) { return -1; } uint32_t ev = 0; for (uint32_t i = 0; i < el; i++) { ev = (ev << 8) | c[q + i]; } *e = ev; return 0;
}

/* ---- records ---- */
static void make_nonce(const uint8_t iv[12], uint64_t seq, uint8_t nonce[12]) { cpy(nonce, iv, 12); for (int i = 0; i < 8; i++) { nonce[11 - i] ^= (uint8_t)(seq >> (8 * i)); } }
static int seal_record(const uint8_t key[16], const uint8_t iv[12], uint64_t *seq, uint8_t type, const uint8_t *data, uint32_t n, uint8_t *rec, uint32_t cap, uint32_t *len) {
    if (n > TLS_MAX_PLAIN) { return TLS_ERR_ARG; } if (cap < 5u + n + 1u + 16u) { return TLS_ERR_SPACE; } if (*seq == 0xFFFFFFFFFFFFFFFFull) { return TLS_ERR_SEQ; }
    uint8_t inner[TLS_MAX_PLAIN + 1], nonce[12]; cpy(inner, data, n); inner[n] = type; rec[0] = 23; rec[1] = 3; rec[2] = 3; p16(rec + 3, n + 1u + 16u); make_nonce(iv, *seq, nonce); tls_gcm_seal(key, nonce, rec, 5, inner, n + 1u, rec + 5); (*seq)++; *len = 5u + n + 1u + 16u; return TLS_OK;
}
static int fail(tls_t *t, int rc, int alert) { t->state = TLS_ST_FAILED; t->alert = alert; return rc; }
static void th_hash(const tls_t *t, uint8_t out[32]) { sha256_ctx_t c = t->th; sha256_final(&c, out); }
static void th_add(tls_t *t, const uint8_t *m, uint32_t n) { sha256_update(&t->th, m, n); }
static void set_keys(const uint8_t secret[32], uint8_t key[16], uint8_t iv[12]) { tls_hkdf_expand_label(secret, "key", 0, 0, key, 16); tls_hkdf_expand_label(secret, "iv", 0, 0, iv, 12); }

void tls_init(tls_t *t) { uint8_t *p = (uint8_t *)t; for (uint32_t i = 0; i < sizeof *t; i++) { p[i] = 0; } t->state = TLS_ST_START; sha256_init(&t->th); }
const char *tls_strerror(int rc) {
    switch (rc) { case TLS_OK: return "ok"; case TLS_ERR_ARG: return "bad argument"; case TLS_ERR_STATE: return "record not allowed in this state"; case TLS_ERR_RECORD: return "malformed record"; case TLS_ERR_DECRYPT: return "record failed authentication (bad_record_mac)";
    case TLS_ERR_HANDSHAKE: return "unexpected or malformed handshake message"; case TLS_ERR_VERSION: return "not TLS 1.3"; case TLS_ERR_SUITE: return "cipher suite not offered"; case TLS_ERR_HRR: return "HelloRetryRequest is not supported"; case TLS_ERR_KEY: return "unusable key share";
    case TLS_ERR_CERT: return "certificate not understood"; case TLS_ERR_SIG: return "CertificateVerify signature invalid"; case TLS_ERR_FINISHED: return "Finished verify_data wrong"; case TLS_ERR_SPACE: return "output buffer too small"; case TLS_ERR_ALERT: return "alert received";
    case TLS_ERR_SEQ: return "sequence number exhausted"; case TLS_ERR_CLOSED: return "connection closed"; } return "?";
}

int tls_client_hello(tls_t *t, const uint8_t random[32], const uint8_t priv[32], const char *sni, uint8_t *rec, uint32_t cap, uint32_t *len) {
    if (t->state != TLS_ST_START) { return TLS_ERR_STATE; } uint32_t sl = 0; while (sni && sni[sl]) { sl++; } if (sl == 0 || sl > 200) { return TLS_ERR_ARG; }
    static const uint8_t base[32] = {9}; cpy(t->priv, priv, 32); cpy(t->random, random, 32); tls_x25519(t->pub, priv, base);
    uint8_t m[400]; uint32_t o = 4; p16(m + o, 0x0303); o += 2; cpy(m + o, random, 32); o += 32; m[o++] = 0; /* legacy_version, random, empty legacy_session_id */
    p16(m + o, 6); o += 2; p16(m + o, 0x1301); p16(m + o + 2, 0x1303); p16(m + o + 4, 0x1302); o += 6; m[o++] = 1; m[o++] = 0; /* cipher suites (the two we do not implement are offered, as in RFC 8448, but a server that picks one is refused), compression null */
    uint32_t extlen_at = o; o += 2; uint32_t es = o;
    p16(m + o, 0x0000); p16(m + o + 2, sl + 5u); p16(m + o + 4, sl + 3u); m[o + 6] = 0; p16(m + o + 7, sl); o += 9; cpy(m + o, (const uint8_t *)sni, sl); o += sl; /* server_name */
    p16(m + o, 0xff01); p16(m + o + 2, 1); m[o + 4] = 0; o += 5; /* renegotiation_info */
    p16(m + o, 0x000a); p16(m + o + 2, 0x0014); p16(m + o + 4, 0x0012); o += 6; static const uint16_t groups[9] = {0x001d, 0x0017, 0x0018, 0x0019, 0x0100, 0x0101, 0x0102, 0x0103, 0x0104}; for (int i = 0; i < 9; i++) { p16(m + o, groups[i]); o += 2; } /* supported_groups */
    p16(m + o, 0x0023); p16(m + o + 2, 0); o += 4; /* session_ticket */
    p16(m + o, 0x0033); p16(m + o + 2, 0x0026); p16(m + o + 4, 0x0024); p16(m + o + 6, 0x001d); p16(m + o + 8, 32); cpy(m + o + 10, t->pub, 32); o += 42; /* key_share: x25519 */
    p16(m + o, 0x002b); p16(m + o + 2, 3); m[o + 4] = 2; p16(m + o + 5, 0x0304); o += 7; /* supported_versions: TLS 1.3 only */
    p16(m + o, 0x000d); p16(m + o + 2, 0x0020); p16(m + o + 4, 0x001e); o += 6; static const uint16_t sa[15] = {0x0403, 0x0503, 0x0603, 0x0203, 0x0804, 0x0805, 0x0806, 0x0401, 0x0501, 0x0601, 0x0201, 0x0402, 0x0502, 0x0602, 0x0202}; for (int i = 0; i < 15; i++) { p16(m + o, sa[i]); o += 2; } /* signature_algorithms */
    p16(m + o, 0x002d); p16(m + o + 2, 2); m[o + 4] = 1; m[o + 5] = 1; o += 6; /* psk_key_exchange_modes: psk_dhe_ke (we never offer a PSK) */
    p16(m + o, 0x001c); p16(m + o + 2, 2); p16(m + o + 4, 0x4001); o += 6; /* record_size_limit */
    p16(m + extlen_at, o - es); m[0] = 1; p24(m + 1, o - 4u);
    if (cap < 5u + o) { return TLS_ERR_SPACE; } rec[0] = 22; rec[1] = 3; rec[2] = 1; p16(rec + 3, o); cpy(rec + 5, m, o); *len = 5u + o; th_add(t, m, o); t->state = TLS_ST_WAIT_SH; t->records_out++; return TLS_OK;
}

static int process_server_hello(tls_t *t, const uint8_t *m, uint32_t n) { /* m: the whole handshake message including its 4-byte header */
    if (n < 4u + 2u + 32u + 1u + 2u + 1u + 2u) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_DECODE_ERROR); } if (m[0] != 2) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_UNEXPECTED_MESSAGE); } if (g24(m + 1) != n - 4u) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_DECODE_ERROR); }
    uint32_t o = 4; if (g16(m + o) != 0x0303) { return fail(t, TLS_ERR_VERSION, TLS_ALERT_PROTOCOL_VERSION); } o += 2; const uint8_t *srandom = m + o; o += 32;
    static const uint8_t hrr[32] = {0xcf, 0x21, 0xad, 0x74, 0xe5, 0x9a, 0x61, 0x11, 0xbe, 0x1d, 0x8c, 0x02, 0x1e, 0x65, 0xb8, 0x91, 0xc2, 0xa2, 0x11, 0x16, 0x7a, 0xbb, 0x8c, 0x5e, 0x07, 0x9e, 0x09, 0xe2, 0xc8, 0xa8, 0x33, 0x9c};
    if (ct_eq(srandom, hrr, 32)) { return fail(t, TLS_ERR_HRR, TLS_ALERT_HANDSHAKE_FAILURE); }
    if (m[o] != 0) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_ILLEGAL_PARAMETER); } o++; /* we sent an empty session id, so the echo must be empty */
    if (g16(m + o) != 0x1301) { return fail(t, TLS_ERR_SUITE, TLS_ALERT_ILLEGAL_PARAMETER); } o += 2; if (m[o] != 0) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_ILLEGAL_PARAMETER); } o++;
    uint32_t el = g16(m + o); o += 2; if (el != n - o) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_DECODE_ERROR); }
    int saw_ver = 0, saw_ks = 0; const uint8_t *share = 0;
    while (o < n) {
        if (n - o < 4u) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_DECODE_ERROR); } uint32_t ty = g16(m + o), l = g16(m + o + 2); o += 4; if (l > n - o) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_DECODE_ERROR); }
        if (ty == 0x002b) { if (saw_ver || l != 2 || g16(m + o) != 0x0304) { return fail(t, TLS_ERR_VERSION, TLS_ALERT_ILLEGAL_PARAMETER); } saw_ver = 1; }
        else if (ty == 0x0033) { if (saw_ks || l != 36 || g16(m + o) != 0x001d || g16(m + o + 2) != 32) { return fail(t, TLS_ERR_KEY, TLS_ALERT_ILLEGAL_PARAMETER); } saw_ks = 1; share = m + o + 4; }
        else { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_UNSUPPORTED_EXTENSION); }
        o += l;
    }
    if (!saw_ver) { return fail(t, TLS_ERR_VERSION, TLS_ALERT_MISSING_EXTENSION); } if (!saw_ks) { return fail(t, TLS_ERR_KEY, TLS_ALERT_MISSING_EXTENSION); }
    static const uint8_t dg[8] = {'D', 'O', 'W', 'N', 'G', 'R', 'D', 1}; int sentinel = 1; for (int i = 0; i < 7; i++) { if (srandom[24 + i] != dg[i]) { sentinel = 0; } } if (sentinel && srandom[31] <= 1) { return fail(t, TLS_ERR_VERSION, TLS_ALERT_ILLEGAL_PARAMETER); }
    uint8_t shared[32], any = 0; tls_x25519(shared, t->priv, share); for (int i = 0; i < 32; i++) { any |= shared[i]; } if (!any) { return fail(t, TLS_ERR_KEY, TLS_ALERT_ILLEGAL_PARAMETER); } /* a low-order point gives the all-zero secret */
    th_add(t, m, n); uint8_t early[32], derived[32], zeros[32], eh[32], th[32]; zero(zeros, 32); tls_hkdf_extract(0, 0, zeros, 32, early); sha256_hash(0, 0, eh); tls_derive_secret(early, "derived", eh, derived);
    tls_hkdf_extract(derived, 32, shared, 32, t->hs_secret); th_hash(t, th); tls_derive_secret(t->hs_secret, "c hs traffic", th, t->c_hs); tls_derive_secret(t->hs_secret, "s hs traffic", th, t->s_hs);
    tls_derive_secret(t->hs_secret, "derived", eh, derived); tls_hkdf_extract(derived, 32, zeros, 32, t->master);
    set_keys(t->s_hs, t->rkey, t->riv); t->rseq = 0; set_keys(t->c_hs, t->wkey, t->wiv); t->wseq = 0; t->state = TLS_ST_WAIT_FLIGHT; return TLS_OK;
}

static int process_flight_message(tls_t *t, const uint8_t *m, uint32_t n, uint8_t *out, uint32_t outcap, uint32_t *outlen, int *done) {
    uint32_t body = n - 4u; const uint8_t *b = m + 4; uint8_t ty = m[0];
    if (ty == 8) { /* EncryptedExtensions */
        if (t->saw_ee) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_UNEXPECTED_MESSAGE); } if (body < 2u || g16(b) != body - 2u) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_DECODE_ERROR); }
        for (uint32_t o = 2; o < body;) { if (body - o < 4u) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_DECODE_ERROR); } uint32_t l = g16(b + o + 2); o += 4; if (l > body - o) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_DECODE_ERROR); } o += l; }
        t->saw_ee = 1; th_add(t, m, n); return TLS_OK;
    }
    if (ty == 11) { /* Certificate */
        if (!t->saw_ee || t->saw_cert) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_UNEXPECTED_MESSAGE); } if (body < 4u || b[0] != 0 || g24(b + 1) != body - 4u) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_DECODE_ERROR); }
        uint32_t o = 4; int first = 1; while (o < body) { if (body - o < 3u) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_DECODE_ERROR); } uint32_t cl = g24(b + o); o += 3; if (cl == 0 || cl > body - o) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_DECODE_ERROR); }
            if (first) { if (cert_rsa_key(b + o, cl, t->rsa_n, &t->rsa_nlen, &t->rsa_e)) { return fail(t, TLS_ERR_CERT, TLS_ALERT_BAD_CERTIFICATE); } first = 0; } o += cl; if (body - o < 2u) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_DECODE_ERROR); } uint32_t xl = g16(b + o); o += 2; if (xl > body - o) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_DECODE_ERROR); } o += xl; }
        if (first) { return fail(t, TLS_ERR_CERT, TLS_ALERT_BAD_CERTIFICATE); } t->saw_cert = 1; th_add(t, m, n); return TLS_OK;
    }
    if (ty == 15) { /* CertificateVerify */
        if (!t->saw_cert || t->saw_cv) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_UNEXPECTED_MESSAGE); } if (body < 4u || g16(b + 2) != body - 4u) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_DECODE_ERROR); } if (g16(b) != 0x0804) { return fail(t, TLS_ERR_SIG, TLS_ALERT_ILLEGAL_PARAMETER); }
        uint8_t th[32], msg[64 + 33 + 1 + 32], mh[32]; th_hash(t, th); for (int i = 0; i < 64; i++) { msg[i] = 0x20; } static const char ctx[] = "TLS 1.3, server CertificateVerify"; for (int i = 0; i < 33; i++) { msg[64 + i] = (uint8_t)ctx[i]; } msg[97] = 0; cpy(msg + 98, th, 32); sha256_hash(msg, 130, mh);
        if (tls_rsa_pss_verify(t->rsa_n, t->rsa_nlen, t->rsa_e, b + 4, body - 4u, mh)) { return fail(t, TLS_ERR_SIG, TLS_ALERT_DECRYPT_ERROR); } t->saw_cv = 1; th_add(t, m, n); return TLS_OK;
    }
    if (ty == 20) { /* Finished */
        if (!t->saw_cv) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_UNEXPECTED_MESSAGE); } if (body != 32u) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_DECODE_ERROR); }
        uint8_t th[32], fk[32], vd[32]; th_hash(t, th); tls_hkdf_expand_label(t->s_hs, "finished", 0, 0, fk, 32); hmac_sha256(fk, 32, th, 32, vd); if (!ct_eq(vd, b, 32)) { return fail(t, TLS_ERR_FINISHED, TLS_ALERT_DECRYPT_ERROR); }
        if (t->hs_len != n) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_UNEXPECTED_MESSAGE); } /* nothing may follow the server Finished in the same key epoch */
        th_add(t, m, n); th_hash(t, t->th_sf); tls_derive_secret(t->master, "c ap traffic", t->th_sf, t->c_ap); tls_derive_secret(t->master, "s ap traffic", t->th_sf, t->s_ap);
        uint8_t cf[36]; cf[0] = 20; p24(cf + 1, 32); tls_hkdf_expand_label(t->c_hs, "finished", 0, 0, fk, 32); hmac_sha256(fk, 32, t->th_sf, 32, cf + 4); th_add(t, cf, 36);
        int rc = seal_record(t->wkey, t->wiv, &t->wseq, 22, cf, 36, out, outcap, outlen); if (rc) { return fail(t, rc, TLS_ALERT_INTERNAL_ERROR); } t->records_out++;
        set_keys(t->s_ap, t->rkey, t->riv); t->rseq = 0; set_keys(t->c_ap, t->wkey, t->wiv); t->wseq = 0; *done = 1; return TLS_OK;
    }
    return fail(t, TLS_ERR_HANDSHAKE, ty == 13 ? TLS_ALERT_HANDSHAKE_FAILURE : TLS_ALERT_UNEXPECTED_MESSAGE); /* CertificateRequest (client auth) and anything else is not supported */
}

int tls_receive(tls_t *t, const uint8_t *rec, uint32_t len, uint8_t *out, uint32_t outcap, uint32_t *outlen, uint8_t *app, uint32_t appcap, uint32_t *applen) {
    *outlen = 0; if (applen) { *applen = 0; } if (t->state == TLS_ST_FAILED) { return TLS_ERR_STATE; } if (t->state == TLS_ST_CLOSED) { return TLS_ERR_CLOSED; } if (t->state == TLS_ST_START) { return TLS_ERR_STATE; }
    if (len < 5u) { return fail(t, TLS_ERR_RECORD, TLS_ALERT_DECODE_ERROR); } uint8_t type = rec[0]; uint32_t ver = g16(rec + 1), rl = g16(rec + 3); if (len != 5u + rl) { return fail(t, TLS_ERR_RECORD, TLS_ALERT_DECODE_ERROR); }
    if (ver != 0x0303) { return fail(t, TLS_ERR_RECORD, TLS_ALERT_PROTOCOL_VERSION); } t->records_in++;
    if (t->state == TLS_ST_WAIT_SH) {
        if (type == 20) { if (rl != 1 || rec[5] != 1 || t->saw_ccs) { return fail(t, TLS_ERR_RECORD, TLS_ALERT_UNEXPECTED_MESSAGE); } t->saw_ccs = 1; return TLS_OK; } /* a compatibility-mode change_cipher_spec is ignored */
        if (type != 22) { return fail(t, TLS_ERR_STATE, TLS_ALERT_UNEXPECTED_MESSAGE); } if (rl == 0 || rl > TLS_MAX_PLAIN) { return fail(t, TLS_ERR_RECORD, TLS_ALERT_RECORD_OVERFLOW); } return process_server_hello(t, rec + 5, rl);
    }
    if (type == 20) { if (rl != 1 || rec[5] != 1 || t->saw_ccs || t->state != TLS_ST_WAIT_FLIGHT) { return fail(t, TLS_ERR_RECORD, TLS_ALERT_UNEXPECTED_MESSAGE); } t->saw_ccs = 1; return TLS_OK; }
    if (type != 23) { return fail(t, TLS_ERR_STATE, TLS_ALERT_UNEXPECTED_MESSAGE); } /* after the ServerHello everything must be protected */
    if (rl < 17u || rl > TLS_MAX_PLAIN + 256u) { return fail(t, TLS_ERR_RECORD, TLS_ALERT_RECORD_OVERFLOW); } if (t->rseq == 0xFFFFFFFFFFFFFFFFull) { return fail(t, TLS_ERR_SEQ, TLS_ALERT_INTERNAL_ERROR); }
    static uint8_t inner[TLS_MAX_PLAIN + 256]; uint8_t nonce[12]; make_nonce(t->riv, t->rseq, nonce); if (tls_gcm_open(t->rkey, nonce, rec, 5, rec + 5, rl, inner)) { return fail(t, TLS_ERR_DECRYPT, TLS_ALERT_BAD_RECORD_MAC); } t->rseq++;
    uint32_t pl = rl - 16u; while (pl > 0 && inner[pl - 1] == 0) { pl--; } if (pl == 0) { return fail(t, TLS_ERR_RECORD, TLS_ALERT_UNEXPECTED_MESSAGE); } uint8_t itype = inner[pl - 1]; pl--; if (pl > TLS_MAX_PLAIN) { return fail(t, TLS_ERR_RECORD, TLS_ALERT_RECORD_OVERFLOW); }
    if (itype == 21) { if (pl != 2) { return fail(t, TLS_ERR_RECORD, TLS_ALERT_DECODE_ERROR); } if (inner[1] == 0) { t->state = TLS_ST_CLOSED; t->alert = TLS_ALERT_CLOSE_NOTIFY; return TLS_OK; } t->alert = inner[1]; t->state = TLS_ST_FAILED; return TLS_ERR_ALERT; }
    if (itype == 22) {
        if (pl == 0) { return fail(t, TLS_ERR_RECORD, TLS_ALERT_UNEXPECTED_MESSAGE); } if (t->hs_len + pl > TLS_HS_BUF) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_INTERNAL_ERROR); } cpy(t->hs + t->hs_len, inner, pl); t->hs_len += pl;
        for (;;) {
            if (t->hs_len < 4u) { break; } uint32_t ml = g24(t->hs + 1); if (ml > TLS_HS_BUF - 4u) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_ILLEGAL_PARAMETER); } if (t->hs_len < 4u + ml) { break; }
            uint32_t mn = 4u + ml; int done = 0, rc;
            if (t->state == TLS_ST_WAIT_FLIGHT) { rc = process_flight_message(t, t->hs, mn, out, outcap, outlen, &done); if (rc) { return rc; } }
            else if (t->state == TLS_ST_CONNECTED) { if (t->hs[0] != 4) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_UNEXPECTED_MESSAGE); } t->tickets++; /* NewSessionTicket: parsed no further, not stored */ }
            else { return fail(t, TLS_ERR_STATE, TLS_ALERT_UNEXPECTED_MESSAGE); }
            uint32_t rest = t->hs_len - mn; for (uint32_t i = 0; i < rest; i++) { t->hs[i] = t->hs[mn + i]; } t->hs_len = rest;
            if (done) { if (t->hs_len != 0) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_UNEXPECTED_MESSAGE); } t->state = TLS_ST_CONNECTED; break; }
        }
        return TLS_OK;
    }
    if (itype == 23) { if (t->state != TLS_ST_CONNECTED) { return fail(t, TLS_ERR_STATE, TLS_ALERT_UNEXPECTED_MESSAGE); } if (pl > appcap) { return fail(t, TLS_ERR_SPACE, TLS_ALERT_INTERNAL_ERROR); } cpy(app, inner, pl); if (applen) { *applen = pl; } return TLS_OK; }
    return fail(t, TLS_ERR_RECORD, TLS_ALERT_UNEXPECTED_MESSAGE);
}
int tls_send(tls_t *t, const uint8_t *data, uint32_t n, uint8_t *rec, uint32_t cap, uint32_t *len) { if (t->state != TLS_ST_CONNECTED) { return TLS_ERR_STATE; } int rc = seal_record(t->wkey, t->wiv, &t->wseq, 23, data, n, rec, cap, len); if (!rc) { t->records_out++; } return rc; }
int tls_close(tls_t *t, uint8_t *rec, uint32_t cap, uint32_t *len) { if (t->state != TLS_ST_CONNECTED) { return TLS_ERR_STATE; } static const uint8_t al[2] = {1, 0}; int rc = seal_record(t->wkey, t->wiv, &t->wseq, 21, al, 2, rec, cap, len); if (!rc) { t->state = TLS_ST_CLOSED; t->records_out++; } return rc; }
