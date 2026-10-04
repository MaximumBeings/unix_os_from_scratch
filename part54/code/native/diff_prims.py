#!/usr/bin/env python3
"""Chapter 54: differential test of the primitives. The C functions (tls_prim, the SAME 054_tls.c the kernel links, with AddressSanitizer + UBSan) and Python's `cryptography` library (an independent implementation) are given the same inputs and must give the same answers:
X25519 (random inputs, the RFC 7748 vector, the eight low-order points, non-canonical encodings), HKDF-Extract and HKDF-Expand-Label (random labels, contexts and lengths), AES-128-GCM seal and open (every plaintext and AAD length from 0 to 70 plus random longer ones, then every damaged variant: a flipped ciphertext bit, a flipped tag bit, wrong
AAD, wrong nonce, truncation), and RSA-PSS verification (keys of 1024, 1536 and 2048 bits, random messages and salts, then damaged signatures, wrong hashes and wrong keys). Usage: diff_prims.py N [tool]"""
import hashlib, os, random, struct, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, os.path.join(here, ".."))
import tls_ref
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import rsa
N = int(sys.argv[1]); tool = sys.argv[2] if len(sys.argv) > 2 else "/tmp/tprim"; R = random.Random(54); fails = 0; counts = {}
p = subprocess.Popen([tool], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
def ask(line): p.stdin.write(line + "\n"); p.stdin.flush(); return p.stdout.readline().strip()
def hx(b): return b.hex() if b else "-"
def rb(n): return bytes(R.randrange(256) for _ in range(n))
def check(kind, got, want, info):
    global fails; counts[kind] = counts.get(kind, 0) + 1
    if got != want: fails += 1; print(f"DIFFERENT {kind}: C={got[:80]} Py={want[:80]} ({info})") if fails <= 5 else None
# X25519
check("X25519: the RFC 7748 vector", ask("x25519 a546e36bf0527c9d3b16154b82465edd62144c0ac1fc5a18506a2244ba449ac4 e6db6867583030db3594c1a424b15f7c726624ec26b3353b10a903a6d0ab1c4c"), "c3da55379de9c6908e94ea4df28d084f32eccf03491c71f754b4075577a28552", "")
low = ["00" * 32, "01" + "00" * 31, "e0eb7a7c3b41b8ae1656e3faf19fc46ada098deb9c32b1fd866205165f49b800", "5f9c95bca3508c24b1d0b1559c83ef5b04445cc4581c8e86d8224eddd09f1157", "ecff" + "ff" * 29 + "7f", "edff" + "ff" * 29 + "7f", "eeff" + "ff" * 29 + "7f", "00" * 31 + "80", "01" + "00" * 30 + "80"]
for pt in low:
    for _ in range(3):
        sc = rb(32); check("X25519: low-order / non-canonical points (all-zero output)", ask(f"x25519 {sc.hex()} {pt}"), tls_ref.x25519(sc, bytes.fromhex(pt)).hex(), pt[:16])
for i in range(N * 4): sc, pt = rb(32), rb(32); check("X25519: random scalar and point", ask(f"x25519 {sc.hex()} {pt.hex()}"), tls_ref.x25519(sc, pt).hex(), "")
# HKDF
for i in range(N * 2):
    salt, ikm = rb(R.choice([0, 1, 32, 32, 40, 64, 100])), rb(R.choice([0, 1, 32, 32, 48]))
    check("HKDF-Extract (empty, short, block-length and long salts)", ask(f"extract {hx(salt)} {hx(ikm)}"), tls_ref.extract(salt, ikm).hex(), "")
    sec = rb(32); label = "".join(R.choice("abcdefghijklmnopqrstuvwxyz ") for _ in range(R.choice([1, 3, 12, 20, 100, 249]))); ctx = rb(R.choice([0, 0, 1, 32, 32, 100, 255])); ln = R.choice([1, 12, 16, 32, 33, 64, 100, 255])
    check("HKDF-Expand-Label (labels up to 249 bytes, contexts up to 255, outputs up to 255 bytes)", ask(f"hkdfl {sec.hex()} {label.replace(' ', '_')} {hx(ctx)} {ln}"), tls_ref.expand_label(sec, label.replace(" ", "_"), ctx, ln).hex(), label[:20])
# AES-GCM
def gseal(): return tls_ref.gcm_seal
for plen in list(range(0, 71)) + [R.randrange(71, 600) for _ in range(N)]:
    key, iv, aad, pt = rb(16), rb(12), rb(R.choice([0, 5, 13, 16, 33, 70])), rb(plen); want = tls_ref.gcm_seal(key, iv, aad, pt)
    got = ask(f"gcm_seal {key.hex()} {iv.hex()} {hx(aad)} {hx(pt)}"); check("AES-128-GCM seal (plaintext lengths 0-70 and random longer; AAD 0-70)", got, want.hex(), plen)
    check("AES-128-GCM open of a genuine ciphertext", ask(f"gcm_open {key.hex()} {iv.hex()} {hx(aad)} {want.hex()}"), "ok " + hx(pt), plen)
    bad = bytearray(want); k = R.randrange(len(bad)); bad[k] ^= 1 << R.randrange(8); check("AES-128-GCM open: a flipped bit anywhere in ciphertext or tag is refused", ask(f"gcm_open {key.hex()} {iv.hex()} {hx(aad)} {bytes(bad).hex()}"), "bad", k)
    check("AES-128-GCM open: wrong AAD refused", ask(f"gcm_open {key.hex()} {iv.hex()} {hx(aad + b'x')} {want.hex()}"), "bad", plen)
    iv2 = bytearray(iv); iv2[R.randrange(12)] ^= 1 << R.randrange(8); check("AES-128-GCM open: wrong nonce refused", ask(f"gcm_open {key.hex()} {bytes(iv2).hex()} {hx(aad)} {want.hex()}"), "bad", plen)
    check("AES-128-GCM open: a truncated message (a missing tag byte) is refused", ask(f"gcm_open {key.hex()} {iv.hex()} {hx(aad)} {want[:-1].hex()}"), "bad", plen)
# RSA-PSS
keys = [serialization.load_pem_private_key(open(os.path.join(here, "..", "data/tls/botan_server_key.pem"), "rb").read(), None), serialization.load_pem_private_key(open(os.path.join(here, "..", "data/tls/test_rsa2048.pem"), "rb").read(), None), rsa.generate_private_key(65537, 1536), rsa.generate_private_key(65537, 3072), serialization.load_pem_private_key(open(os.path.join(here, "..", "data/tls/test_rsa1008.pem"), "rb").read(), None), rsa.generate_private_key(65537, 1025), rsa.generate_private_key(65537, 1030), rsa.generate_private_key(65537, 2041)]
def nb(k): n = k.public_key().public_numbers().n; return n.to_bytes((n.bit_length() + 7) // 8, "big")
for ki, key in enumerate(keys):
    n = nb(key); e = key.public_key().public_numbers().e; eb = e.to_bytes(4, "big").lstrip(b"\0")
    for i in range(max(8, N // 4)):
        msg = rb(R.randrange(0, 200)); sig = tls_ref.pss_sign(key, msg); mh = hashlib.sha256(msg).digest(); supported = 128 <= len(n) <= 256
        check(f"RSA-PSS verify, {len(n) * 8}-bit key, genuine signature" + ("" if supported else " (outside the supported 1024 to 2048 bits: refused)"), ask(f"pss {n.hex()} {eb.hex()} {sig.hex()} {mh.hex()}"), "ok" if supported else "bad", i)
        if not supported: continue
        bad = bytearray(sig); bad[R.randrange(len(bad))] ^= 1 << R.randrange(8); check("RSA-PSS verify: a flipped signature bit is refused", ask(f"pss {n.hex()} {eb.hex()} {bytes(bad).hex()} {mh.hex()}"), "bad", i)
        mh2 = bytearray(mh); mh2[R.randrange(32)] ^= 1; check("RSA-PSS verify: a different message hash is refused", ask(f"pss {n.hex()} {eb.hex()} {sig.hex()} {bytes(mh2).hex()}"), "bad", i)
        check("RSA-PSS verify: a signature as large as the modulus (s = n) is refused", ask(f"pss {n.hex()} {eb.hex()} {n.hex()} {mh.hex()}"), "bad", i)
        check("RSA-PSS verify: a short signature is refused", ask(f"pss {n.hex()} {eb.hex()} {sig[1:].hex()} {mh.hex()}"), "bad", i)
        other = nb(keys[(ki + 1) % 2]); check("RSA-PSS verify: another key's modulus refuses it", ask(f"pss {other.hex()} {eb.hex()} {sig.hex()} {mh.hex()}") if len(other) == len(n) or True else "bad", "bad", i)
        check("RSA-PSS verify: a wrong public exponent (3) is refused", ask(f"pss {n.hex()} 03 {sig.hex()} {mh.hex()}"), "bad", i)
        check("RSA-PSS verify: an even exponent is refused", ask(f"pss {n.hex()} 010000 {sig.hex()} {mh.hex()}"), "bad", i)
# RSA-PSS: hand-built encodings signed with the raw private operation, so that each rule of RFC 8017 section 9.1.2 can be broken on its own
def mgf1(seed, n):
    out = b""; c = 0
    while len(out) < n: out += hashlib.sha256(seed + struct.pack(">I", c)).digest(); c += 1
    return out[:n]
def encode(mh, salt, embits, emlen, tweak_db=None, tweak_h=None, tweak_top=False, trailer=0xbc):
    h = hashlib.sha256(b"\0" * 8 + mh + salt).digest()
    if tweak_h: h = bytes(h[:-1]) + bytes([h[-1] ^ 1])  # the embedded H differs from the true hash in its last byte only; the mask is made from the embedded H, so the rest decodes cleanly
    ps = emlen - 32 - 32 - 2; db = bytearray(b"\0" * ps + b"\x01" + salt)
    if tweak_db: tweak_db(db, ps)
    mask = mgf1(h, emlen - 33); masked = bytearray(a ^ b for a, b in zip(db, mask)); unused = 8 * emlen - embits
    if unused: masked[0] &= 0xff >> unused
    if tweak_top and unused: masked[0] |= 0x80
    return bytes(masked) + h + bytes([trailer])
for ki, key in enumerate(keys):
    n_int = key.public_key().public_numbers().n; nbytes = (n_int.bit_length() + 7) // 8; e = key.public_key().public_numbers().e; eb = e.to_bytes(4, "big").lstrip(b"\0")
    if not 128 <= nbytes <= 256: continue
    embits = n_int.bit_length() - 1; emlen = (embits + 7) // 8; d = key.private_numbers().d
    def raw(em_int): return pow(em_int, d, n_int).to_bytes(nbytes, "big")
    for i in range(6):
        msg = rb(R.randrange(0, 80)); mh = hashlib.sha256(msg).digest(); salt = rb(32); nb_ = f"{n_int.bit_length()}-bit"
        cases = [("a hand-built valid encoding", encode(mh, salt, embits, emlen), "ok"),
                 ("the last padding byte nonzero", encode(mh, salt, embits, emlen, tweak_db=lambda db, ps: db.__setitem__(ps - 1, 0x55)), "bad"),
                 ("the first padding byte nonzero", encode(mh, salt, embits, emlen, tweak_db=lambda db, ps: db.__setitem__(0, 0x01)), "bad"),
                 ("a middle padding byte nonzero", encode(mh, salt, embits, emlen, tweak_db=lambda db, ps: db.__setitem__(ps // 2, 0x33)), "bad"),
                 ("the 0x01 separator replaced by 0x02", encode(mh, salt, embits, emlen, tweak_db=lambda db, ps: db.__setitem__(ps, 0x02)), "bad"),
                 ("the trailer 0xbd instead of 0xbc", encode(mh, salt, embits, emlen, trailer=0xbd), "bad"),
                 ("the embedded H differing from the true hash in its LAST byte only", encode(mh, salt, embits, emlen, tweak_h=True), "bad"),
                 ("the salt altered after hashing", encode(mh, salt, embits, emlen, tweak_db=lambda db, ps: db.__setitem__(len(db) - 1, db[-1] ^ 1)), "bad")]
        if 8 * emlen != embits: cases.append(("a top bit of the masked DB set (beyond emBits)", encode(mh, salt, embits, emlen, tweak_top=True), "bad"))
        for what, em, want in cases:
            sig = raw(int.from_bytes(em, "big")); check(f"RSA-PSS verify, {nb_} key, {what}", ask(f"pss {n_int.to_bytes(nbytes, 'big').hex()} {eb.hex()} {sig.hex()} {mh.hex()}"), want, i)
        while True:  # a genuine signature whose first byte is 0: its short form (one byte stripped) and a long form (one 00 byte added) must both be refused
            msg2 = rb(20); s2 = tls_ref.pss_sign(key, msg2)
            if s2[0] == 0: break
        mh2 = hashlib.sha256(msg2).digest(); nh = n_int.to_bytes(nbytes, "big").hex()
        check(f"RSA-PSS verify, {n_int.bit_length()}-bit key, a genuine signature with its leading zero byte stripped (wrong length)", ask(f"pss {nh} {eb.hex()} {s2[1:].hex()} {mh2.hex()}"), "bad", i)
        check(f"RSA-PSS verify, {n_int.bit_length()}-bit key, a genuine signature with an extra leading zero byte (wrong length)", ask(f"pss {nh} {eb.hex()} {('00' + s2.hex())} {mh2.hex()}"), "bad", i)
        if nbytes > emlen:  # a leading non-zero byte in EM (the modulus is 8k+1 bits long)
            em = encode(mh, salt, embits, emlen); big = (1 << (8 * emlen)) + int.from_bytes(em, "big")
            if big < n_int: check(f"RSA-PSS verify, {nb_} key, a non-zero leading byte before EM", ask(f"pss {n_int.to_bytes(nbytes, 'big').hex()} {eb.hex()} {raw(big).hex()} {mh.hex()}"), "bad", i)
p.stdin.close(); p.wait()
for k in sorted(counts): print(f"  {k}: {counts[k]}")
print(f"{sum(counts.values())} comparisons, {fails} differences"); sys.exit(1 if fails else 0)
