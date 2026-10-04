#!/usr/bin/env python3
"""READ THIS FIRST: this script deliberately BREAKS the chapter's sources, one line at a time, in a temporary copy, and runs the host-side test suite against each broken copy. It expects the suite to FAIL every time. A "caught" line is the EXPECTED, wanted
result: it shows the tests can detect that mistake. "NOT CAUGHT" would be a gap. The real sources are never modified. For each mutant it runs: tls_test (primitives and API), tls_trace (the recorded RFC 8448 handshake), the differential test of the primitives against Python's cryptography library, the handshake differential test with its attack catalogue and byte flips, and the mutation fuzzer. Four broken copies are tested at a time. Output: mutation_out.txt   Usage: mutation.py"""
import concurrent.futures, os, shutil, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__)); code = os.path.join(here, "..")
MUT = [
 ('054_tls.c', "X25519: the scalar's bit 254 is not forced", 'z[31] = (uint8_t)((scalar[31] & 127) | 64);', 'z[31] = (uint8_t)((scalar[31] & 127) | 0);'),
 ('054_tls.c', "X25519: the scalar's low bits are not cleared", 'z[0] &= 248;', 'z[0] &= 255;'),
 ('054_tls.c', 'X25519: wrong a24 constant (0xDB41 -> 0xDB40)', '{0xDB41, 1}', '{0xDB40, 1}'),
 ('054_tls.c', 'X25519: the carry wraps with 37 instead of 38', 'o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);', 'o[(i + 1) * (i < 15)] += c - 1 + 36 * (c - 1) * (i == 15);'),
 ('054_tls.c', 'X25519: multiplication folds with 37 instead of 38', 't[i] += 38 * t[i + 16];', 't[i] += 37 * t[i + 16];'),
 ('054_tls.c', 'X25519: the inversion exponent skips only bit 2', 'if (a != 2 && a != 4) { fm(c, c, in); }', 'if (a != 2) { fm(c, c, in); }'),
 ('054_tls.c', "X25519: the high bit of the peer's point is not masked", 'o[15] &= 0x7fff; }', '}'),
 ('054_tls.c', 'X25519: the ladder starts at bit 253', 'for (int i = 254; i >= 0; --i) { int r =', 'for (int i = 253; i >= 0; --i) { int r ='),
 ('054_tls.c', 'HKDF-Expand: the block counter starts at 2', 'for (uint32_t c = 1; o < olen; c++)', 'for (uint32_t c = 2; o < olen; c++)'),
 ('054_tls.c', 'HKDF-Expand: the previous block is not chained', 'cpy(buf, t, tl); cpy(buf + tl, info, ilen);', 'cpy(buf, t, 0); cpy(buf, info, ilen); tl = 0;'),
 ('054_tls.c', "HKDF label: prefix 'tls12 ' instead of 'tls13 '", 'const char *pre = "tls13 ";', 'const char *pre = "tls12 ";'),
 ('054_tls.c', 'HKDF label: the output length is encoded one too large', 'p16(info, olen);', 'p16(info, olen + 1u);'),
 ('054_tls.c', 'HKDF label: the label length byte omits the prefix', 'info[2] = (uint8_t)(6 + ll);', 'info[2] = (uint8_t)(ll);'),
 ('054_tls.c', 'HKDF label: the context length byte is always 0', 'info[n++] = (uint8_t)clen; for (uint32_t i = 0; i < clen; i++)', 'info[n++] = 0; for (uint32_t i = 0; i < clen; i++)'),
 ('054_tls.c', 'HKDF label: 250-byte labels are accepted', 'if (ll > 249u ||', 'if (ll > 250u ||'),
 ('054_tls.c', 'GCM: GHASH reduction constant 0xe1 -> 0xe0', 'if (lsb) { v[0] ^= 0xe1; }', 'if (lsb) { v[0] ^= 0xe0; }'),
 ('054_tls.c', 'GCM: GHASH shifts by 2', 'v[0] >>= 1; if (lsb)', 'v[0] >>= 2; if (lsb)'),
 ('054_tls.c', 'GCM: the AAD and ciphertext bit lengths are swapped', 'lb[7 - i] = (uint8_t)(ab >> (8 * i)); lb[15 - i] = (uint8_t)(cb >> (8 * i));', 'lb[7 - i] = (uint8_t)(cb >> (8 * i)); lb[15 - i] = (uint8_t)(ab >> (8 * i));'),
 ('054_tls.c', 'GCM: the 32-bit counter increments only its low byte', 'for (int k = 15; k >= 12; k--) { if (++ctr[k]) { break; } }', 'for (int k = 15; k >= 15; k--) { if (++ctr[k]) { break; } }'),
 ('054_tls.c', 'GCM: J0 ends in 0 instead of 1', 'j0[14] = 0; j0[15] = 1;', 'j0[14] = 0; j0[15] = 0;'),
 ('054_tls.c', 'GCM: only 15 bytes of the tag are compared', 'if (!ct_eq(tag, ct + n, 16)) { zero(pt, n); return -1; }', 'if (!ct_eq(tag, ct + n, 15)) { zero(pt, n); return -1; }'),
 ('054_tls.c', 'GCM: a failed open leaves the plaintext in the output', 'if (!ct_eq(tag, ct + n, 16)) { zero(pt, n); return -1; }', 'if (!ct_eq(tag, ct + n, 16)) { return -1; }'),
 ('054_tls.c', 'GCM: an empty message (tag only) is refused', 'if (clen < 16) { return -1; } uint32_t n = clen - 16;', 'if (clen <= 16) { return -1; } uint32_t n = clen - 16;'),
 ('054_tls.c', 'GCM: the AAD is not authenticated', 'ghash_update(y, h, aad, alen); ghash_update(y, h, ghash_over, n);', 'ghash_update(y, h, aad, 0); ghash_update(y, h, ghash_over, n);'),
 ('054_tls.c', 'RSA: the modulus length in limbs is truncated', 'uint32_t nl = (n + 3u) / 4u;', 'uint32_t nl = n / 4u;'),
 ('054_tls.c', 'RSA: the public exponent is applied once (no squarings)', 'bn_mulmod(R, R, R, N, nl); if ((e >> bit) & 1u) { bn_mulmod(R, R, S, N, nl); }', 'if ((e >> bit) & 1u) { bn_mulmod(R, R, S, N, nl); }'),
 ('054_tls.c', 'RSA-PSS: emBits = modBits instead of modBits - 1', 'uint32_t embits = modbits - 1u,', 'uint32_t embits = modbits,'),
 ('054_tls.c', 'RSA-PSS: the leading zero byte of EM is not required', 'if (nlen > emlen && em[0] != 0) { return -1; }', ''),
 ('054_tls.c', 'RSA-PSS: the trailer byte is 0xbd', 'EM[emlen - 1] != 0xbc', 'EM[emlen - 1] != 0xbd'),
 ('054_tls.c', 'RSA-PSS: MGF1 counter off by one', 'buf[35] = (uint8_t)c;', 'buf[35] = (uint8_t)(c + 1u);'),
 ('054_tls.c', 'RSA-PSS: the top bits of DB are not cleared', 'if (unused) { db[0] &= (uint8_t)(0xFFu >> unused); }', 'if (0) { db[0] &= (uint8_t)(0xFFu >> unused); }'),
 ('054_tls.c', 'RSA-PSS: the last padding byte is not checked', 'for (uint32_t i = 0; i < ps; i++) { if (db[i] != 0) { return -1; } }', 'for (uint32_t i = 0; i + 1u < ps; i++) { if (db[i] != 0) { return -1; } }'),
 ('054_tls.c', 'RSA-PSS: the 0x01 separator is 0x02', 'if (db[ps] != 0x01)', 'if (db[ps] != 0x02)'),
 ('054_tls.c', 'RSA-PSS: the salt length is 31', 'uint32_t ps = dblen - 32u - 1u;', 'uint32_t ps = dblen - 31u - 1u;'),
 ('054_tls.c', 'RSA-PSS: only 31 bytes of the hash are compared', 'return ct_eq(hh, H, 32) ? 0 : -1;', 'return ct_eq(hh, H, 31) ? 0 : -1;'),
 ('054_tls.c', 'RSA-PSS: moduli below 1024 bits are accepted', 'if (nlen < 128u || nlen > 256u ||', 'if (nlen < 120u || nlen > 256u ||'),
 ('054_tls.c', 'RSA-PSS: moduli above 2048 bits are accepted (buffer overrun)', 'if (nlen < 128u || nlen > 256u ||', 'if (nlen < 128u || nlen > 384u ||'),
 ('054_tls.c', 'RSA-PSS: a signature of the wrong length is accepted', '|| slen != nlen) { return -1; }', ') { return -1; }'),
 ('054_tls.c', 'DER: three-byte lengths are accepted', 'if (nb < 1 || nb > 2 || p + nb > len)', 'if (nb < 1 || nb > 3 || p + nb > len)'),
 ('054_tls.c', 'DER: a length beyond the buffer is accepted', 'if (l > len - p) { return -1; } *pos = p;', '*pos = p;'),
 ('054_tls.c', 'DER: the version field tag is wrong (0xa1)', 'if (p < len && c[p] == 0xa0) { if (der_skip(c, len, &p, 0xa0)) { return -1; } }', 'if (p < len && c[p] == 0xa1) { if (der_skip(c, len, &p, 0xa1)) { return -1; } }'),
 ('054_tls.c', 'DER: only the first 10 bytes of the algorithm OID are compared', 'for (int i = 0; i < 11; i++) { if (c[q + i] != oid[i]) { return -1; } }', 'for (int i = 0; i < 10; i++) { if (c[q + i] != oid[i]) { return -1; } }'),
 ('054_tls.c', "DER: the BIT STRING's unused-bits byte is not checked", 'if (der_into(c, len, &q, 0x03, &v) || v < 1 || c[q] != 0) { return -1; }', 'if (der_into(c, len, &q, 0x03, &v) || v < 1) { return -1; }'),
 ('054_tls.c', 'DER: a modulus of 2049 to 2056 bits is accepted', 'if (nl < 128 || nl > 256) { return -1; } cpy(n, c + ns, nl);', 'if (nl < 128 || nl > 300) { return -1; } cpy(n, c + ns, nl);'),
 ('054_tls.c', 'DER: a 5-byte exponent is accepted', 'el < 1 || el > 4) { return -1; }', 'el < 1 || el > 5) { return -1; }'),
 ('054_tls.c', 'records: the nonce XOR starts at the wrong end', 'nonce[11 - i] ^= (uint8_t)(seq >> (8 * i));', 'nonce[i] ^= (uint8_t)(seq >> (8 * i));'),
 ('054_tls.c', 'records: the sequence number is not advanced on send', 'tls_gcm_seal(key, nonce, rec, 5, inner, n + 1u, rec + 5); (*seq)++;', 'tls_gcm_seal(key, nonce, rec, 5, inner, n + 1u, rec + 5);'),
 ('054_tls.c', 'records: the outer record version is 0x0301', 'rec[0] = 23; rec[1] = 3; rec[2] = 3; p16(rec + 3, n + 1u + 16u);', 'rec[0] = 23; rec[1] = 3; rec[2] = 1; p16(rec + 3, n + 1u + 16u);'),
 ('054_tls.c', 'records: the inner content type is always application_data', 'inner[n] = type; rec[0] = 23;', 'inner[n] = 23; rec[0] = 23;'),
 ('054_tls.c', 'records: the length field omits the tag', 'p16(rec + 3, n + 1u + 16u);', 'p16(rec + 3, n + 1u + 15u);'),
 ('054_tls.c', 'records: trailing bytes after a record are tolerated', 'if (len != 5u + rl) { return fail(t, TLS_ERR_RECORD, TLS_ALERT_DECODE_ERROR); }', 'if (len < 5u + rl) { return fail(t, TLS_ERR_RECORD, TLS_ALERT_DECODE_ERROR); }'),
 ('054_tls.c', 'records: the record version is not checked', 'if (ver != 0x0303) { return fail(t, TLS_ERR_RECORD, TLS_ALERT_PROTOCOL_VERSION); }', ''),
 ('054_tls.c', 'records: zero padding is not stripped', 'uint32_t pl = rl - 16u; while (pl > 0 && inner[pl - 1] == 0) { pl--; }', 'uint32_t pl = rl - 16u;'),
 ('054_tls.c', 'records: the received sequence number is not advanced', 'if (tls_gcm_open(t->rkey, nonce, rec, 5, rec + 5, rl, inner)) { return fail(t, TLS_ERR_DECRYPT, TLS_ALERT_BAD_RECORD_MAC); } t->rseq++;', 'if (tls_gcm_open(t->rkey, nonce, rec, 5, rec + 5, rl, inner)) { return fail(t, TLS_ERR_DECRYPT, TLS_ALERT_BAD_RECORD_MAC); }'),
 ('054_tls.c', 'records: a warning alert other than close_notify closes cleanly', 'if (inner[1] == 0) { t->state = TLS_ST_CLOSED;', 'if (inner[1] == 0 || inner[0] == 1) { t->state = TLS_ST_CLOSED;'),
 ('054_tls.c', 'records: application data before the handshake is delivered', 'if (t->state != TLS_ST_CONNECTED) { return fail(t, TLS_ERR_STATE, TLS_ALERT_UNEXPECTED_MESSAGE); } if (pl > appcap)', 'if (pl > appcap)'),
 ('054_tls.c', 'records: a handshake message after the handshake need not be a NewSessionTicket', 'if (t->hs[0] != 4) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_UNEXPECTED_MESSAGE); } t->tickets++;', 't->tickets++;'),
 ('054_tls.c', 'records: a handshake message waits for one byte more than it needs', 'if (t->hs_len < 4u + ml) { break; }', 'if (t->hs_len <= 4u + ml) { break; }'),
 ('054_tls.c', 'records: an oversize handshake message length is not rejected', 'if (ml > TLS_HS_BUF - 4u) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_ILLEGAL_PARAMETER); }', ''),
 ('054_tls.c', 'ServerHello: the HelloRetryRequest random is not recognised', '0xcf, 0x21, 0xad, 0x74,', '0xce, 0x21, 0xad, 0x74,'),
 ('054_tls.c', 'ServerHello: a non-empty session id echo is accepted', 'if (m[o] != 0) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_ILLEGAL_PARAMETER); } o++; /* we sent an empty session id', 'o++; /* we sent an empty session id'),
 ('054_tls.c', 'ServerHello: any cipher suite is accepted', 'if (g16(m + o) != 0x1301) { return fail(t, TLS_ERR_SUITE, TLS_ALERT_ILLEGAL_PARAMETER); }', 'if (g16(m + o) == 0x0000) { return fail(t, TLS_ERR_SUITE, TLS_ALERT_ILLEGAL_PARAMETER); }'),
 ('054_tls.c', 'ServerHello: compression method 1 is accepted', 'o += 2; if (m[o] != 0) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_ILLEGAL_PARAMETER); } o++;', 'o += 2; o++;'),
 ('054_tls.c', 'ServerHello: the selected version value is not checked', 'if (saw_ver || l != 2 || g16(m + o) != 0x0304)', 'if (saw_ver || l != 2)'),
 ('054_tls.c', 'ServerHello: a key share for another group is accepted', 'l != 36 || g16(m + o) != 0x001d || g16(m + o + 2) != 32)', 'l != 36 || g16(m + o + 2) != 32)'),
 ('054_tls.c', 'ServerHello: unknown extensions are ignored', 'else { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_UNSUPPORTED_EXTENSION); }\n        o += l;', 'else { }\n        o += l;'),
 ('054_tls.c', 'ServerHello: a repeated supported_versions extension is accepted', 'if (saw_ver || l != 2 || g16(m + o) != 0x0304)', 'if (l != 2 || g16(m + o) != 0x0304)'),
 ('054_tls.c', 'ServerHello: a repeated key_share extension is accepted', 'if (saw_ks || l != 36 ||', 'if (l != 36 ||'),
 ('054_tls.c', 'ServerHello: the all-zero shared secret is accepted', 'if (!any) { return fail(t, TLS_ERR_KEY, TLS_ALERT_ILLEGAL_PARAMETER); }', ''),
 ('054_tls.c', 'ServerHello: the downgrade sentinel is not checked', 'if (sentinel && srandom[31] <= 1)', 'if (0 && sentinel && srandom[31] <= 1)'),
 ('054_tls.c', 'ServerHello: the ServerHello is left out of the transcript', 'th_add(t, m, n); uint8_t early[32],', 'uint8_t early[32],'),
 ('054_tls.c', 'key schedule: the server handshake secret uses the client label', 'tls_derive_secret(t->hs_secret, "s hs traffic", th, t->s_hs);', 'tls_derive_secret(t->hs_secret, "c hs traffic", th, t->s_hs);'),
 ('054_tls.c', 'key schedule: the master secret is derived from the early secret', 'tls_derive_secret(t->hs_secret, "derived", eh, derived); tls_hkdf_extract(derived, 32, zeros, 32, t->master);', 'tls_derive_secret(early, "derived", eh, derived); tls_hkdf_extract(derived, 32, zeros, 32, t->master);'),
 ('054_tls.c', "key schedule: the client Finished key comes from the server's secret", 'tls_hkdf_expand_label(t->c_hs, "finished", 0, 0, fk, 32); hmac_sha256(fk, 32, t->th_sf, 32, cf + 4);', 'tls_hkdf_expand_label(t->s_hs, "finished", 0, 0, fk, 32); hmac_sha256(fk, 32, t->th_sf, 32, cf + 4);'),
 ('054_tls.c', 'key schedule: the client application secret uses the server label', 'tls_derive_secret(t->master, "c ap traffic", t->th_sf, t->c_ap);', 'tls_derive_secret(t->master, "s ap traffic", t->th_sf, t->c_ap);'),
 ('054_tls.c', 'key schedule: the receive sequence number is not reset after the handshake', 'set_keys(t->s_ap, t->rkey, t->riv); t->rseq = 0;', 'set_keys(t->s_ap, t->rkey, t->riv);'),
 ('054_tls.c', 'key schedule: the send sequence number is not reset after the handshake', 'set_keys(t->c_ap, t->wkey, t->wiv); t->wseq = 0; *done = 1;', 'set_keys(t->c_ap, t->wkey, t->wiv); *done = 1;'),
 ('054_tls.c', 'flight: the Certificate may come before EncryptedExtensions', 'if (!t->saw_ee || t->saw_cert) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_UNEXPECTED_MESSAGE); }', 'if (t->saw_cert) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_UNEXPECTED_MESSAGE); }'),
 ('054_tls.c', 'flight: a second EncryptedExtensions is accepted', 'if (t->saw_ee) { return fail(t, TLS_ERR_HANDSHAKE, TLS_ALERT_UNEXPECTED_MESSAGE); } if (body < 2u', 'if (body < 2u'),
 ('054_tls.c', 'flight: the CertificateVerify signature scheme is not checked', 'if (g16(b) != 0x0804) { return fail(t, TLS_ERR_SIG, TLS_ALERT_ILLEGAL_PARAMETER); }', ''),
 ('054_tls.c', "flight: the CertificateVerify context string says 'client'", 'static const char ctx[] = "TLS 1.3, server CertificateVerify";', 'static const char ctx[] = "TLS 1.3, client CertificateVerify";'),
 ('054_tls.c', 'flight: the server Finished is not compared', 'if (!ct_eq(vd, b, 32)) { return fail(t, TLS_ERR_FINISHED, TLS_ALERT_DECRYPT_ERROR); }', ''),
 ('054_tls.c', 'ClientHello: the legacy version is 0x0302', 'p16(m + o, 0x0303); o += 2; cpy(m + o, random, 32);', 'p16(m + o, 0x0302); o += 2; cpy(m + o, random, 32);'),
 ('054_tls.c', 'ClientHello: the server name length is off by one', 'p16(m + o + 2, sl + 5u); p16(m + o + 4, sl + 3u);', 'p16(m + o + 2, sl + 5u); p16(m + o + 4, sl + 2u);'),
 ('054_tls.c', 'ClientHello: an empty server name is accepted', 'if (sl == 0 || sl > 200) { return TLS_ERR_ARG; }', 'if (sl > 200) { return TLS_ERR_ARG; }'),
 ('054_tls.c', 'ClientHello: the output size check is too lenient', 'if (cap < 5u + o) { return TLS_ERR_SPACE; } rec[0] = 22;', 'if (cap < o) { return TLS_ERR_SPACE; } rec[0] = 22;'),
 ('tls_ref.py', 'reference: the transcript hash of the CertificateVerify omits the Certificate (the oracle itself broken)', 'tr += ee_msg + cert_msg; sig = pss_sign(s.key, cert_verify_message(H(tr)));', 'tr += ee_msg + cert_msg; sig = pss_sign(s.key, cert_verify_message(H(tr[:-len(cert_msg)])));'),
]
def run(tmp, cmd, **kw): return subprocess.run(cmd, cwd=tmp, capture_output=True, text=True, errors="replace", **kw)
SRC = ["../054_tls.c", "../054_sha256.c", "../054_hmac.c", "../054_aes.c"]; G = ["gcc", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined"]
VEC = "../data/tls/botan_rfc8448_transcripts.vec"
def suite(tmp):
    failed = []; n = os.path.join(tmp, "native")
    b = run(n, G + ["tls_test.c"] + SRC + ["-o", "tt"])
    if b.returncode != 0: failed.append("tls_test (did not build)")
    elif run(n, ["./tt"]).returncode != 0: failed.append("tls_test")
    b = run(n, G + ["tls_trace.c"] + SRC + ["-o", "tr"])
    if b.returncode != 0: failed.append("RFC 8448 trace (did not build)")
    elif run(n, ["./tr", VEC]).returncode != 0: failed.append("RFC 8448 trace")
    b = run(n, G + ["tls_prim.c"] + SRC + ["-o", "prim"])
    if b.returncode != 0: failed.append("primitives (did not build)"); return failed
    if run(n, [sys.executable, "diff_prims.py", "12", "./prim"]).returncode != 0: failed.append("primitives vs Python cryptography")
    b = run(n, G + ["tls_hs.c"] + SRC + ["-o", "hs"])
    if b.returncode != 0: failed.append("handshake (did not build)"); return failed
    if run(n, [sys.executable, "diff_tls.py", "4", "./hs"]).returncode != 0: failed.append("handshakes and attacks vs Python")
    b = run(n, G + ["tls_fuzz.c"] + SRC + ["-o", "fz"])
    if b.returncode != 0: failed.append("fuzz (did not build)")
    elif run(n, ["./fz", VEC, "250"]).returncode != 0: failed.append("fuzz")
    return failed
print("NOTE: this script deliberately breaks copies of the chapter's sources. 'caught' lines are EXPECTED: they show the tests can detect the mistake."); sys.stdout.flush()
base = tempfile.mkdtemp(prefix="c51mut_"); shutil.copytree(code, os.path.join(base, "base"), ignore=shutil.ignore_patterns("build", "*.o", "__pycache__"))
f0 = suite(os.path.join(base, "base")); print("baseline (nothing broken):", "all tests pass (expected)" if not f0 else "UNEXPECTED FAILURES " + str(f0)); sys.stdout.flush()
def one(i):
    fn, label, old, new = MUT[i]; tmp = os.path.join(base, "m%d" % i); shutil.copytree(os.path.join(base, "base"), tmp)
    p = os.path.join(tmp, fn); s = open(p, encoding="utf-8").read(); assert s.count(old) == 1, (label, s.count(old)); open(p, "w", encoding="utf-8").write(s.replace(old, new))
    failed = suite(tmp); shutil.rmtree(tmp, ignore_errors=True); return label, failed
caught = 0
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    for label, failed in pool.map(one, range(len(MUT))):
        if failed: caught += 1; print(f"{label}: caught by {', '.join(failed)}")
        else: print(f"{label}: NOT CAUGHT")
        sys.stdout.flush()
print(f"\n{caught} of {len(MUT)} broken versions caught"); shutil.rmtree(base, ignore_errors=True)
