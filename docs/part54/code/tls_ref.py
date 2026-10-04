#!/usr/bin/env python3
"""Chapter 54: an independent TLS 1.3 implementation in Python, built on the `cryptography` library's primitives (X25519, AES-GCM, HMAC, HKDF-Expand, RSA-PSS) and written from RFC 8446, NOT from the C code. It contains BOTH ends of the handshake:
a client (so the C client's output can be compared byte for byte) and a server (so the C client can be handed servers that are correct, and servers that have been damaged in every way). Also the line-protocol drivers for the C tools.
Usage as a module (see native/diff_tls.py and verify_054.py)."""
import hashlib, hmac, struct
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding, rsa, x25519 as _x25519
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives.kdf.hkdf import HKDFExpand
def H(b): return hashlib.sha256(b).digest()
def extract(salt, ikm): return hmac.new(salt if salt else b"\0" * 32, ikm, hashlib.sha256).digest()
def expand_label(secret, label, ctx, n):
    full = b"tls13 " + label.encode(); info = struct.pack(">HB", n, len(full)) + full + bytes([len(ctx)]) + ctx
    return HKDFExpand(hashes.SHA256(), n, info).derive(secret)
def derive_secret(secret, label, th): return expand_label(secret, label, th, 32)
def x25519(scalar, point):
    try: return _x25519.X25519PrivateKey.from_private_bytes(scalar).exchange(_x25519.X25519PublicKey.from_public_bytes(point))
    except ValueError: return b"\0" * 32
def pub_of(priv): return _x25519.X25519PrivateKey.from_private_bytes(priv).public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
def gcm_seal(key, iv, aad, pt): return AESGCM(key).encrypt(iv, pt, aad)
def gcm_open(key, iv, aad, ct):
    try: return AESGCM(key).decrypt(iv, ct, aad)
    except Exception: return None
def nonce(iv, seq): return bytes(a ^ b for a, b in zip(iv, b"\0" * 4 + struct.pack(">Q", seq)))
def u16(n): return struct.pack(">H", n)
def u24(n): return struct.pack(">I", n)[1:]
def traffic(secret): return expand_label(secret, "key", b"", 16), expand_label(secret, "iv", b"", 12)
class Keys:
    def __init__(s, secret): s.secret = secret; s.key, s.iv = traffic(secret); s.seq = 0
    def seal(s, typ, data):
        inner = data + bytes([typ]); hdr = b"\x17\x03\x03" + u16(len(inner) + 16); ct = gcm_seal(s.key, nonce(s.iv, s.seq), hdr, inner); s.seq += 1; return hdr + ct
    def open(s, rec):
        pt = gcm_open(s.key, nonce(s.iv, s.seq), rec[:5], rec[5:])
        if pt is None: return None
        s.seq += 1; pt = pt.rstrip(b"\0"); return pt[-1], pt[:-1]
def client_hello(rnd, priv, sni):
    """The ClientHello handshake message, as RFC 8448 shows it (same extensions, same order)."""
    pub = pub_of(priv); name = sni.encode()
    ext = (u16(0) + u16(len(name) + 5) + u16(len(name) + 3) + b"\0" + u16(len(name)) + name + u16(0xff01) + u16(1) + b"\0"
           + u16(10) + u16(0x14) + u16(0x12) + b"".join(u16(g) for g in (0x1d, 0x17, 0x18, 0x19, 0x100, 0x101, 0x102, 0x103, 0x104)) + u16(0x23) + u16(0)
           + u16(0x33) + u16(0x26) + u16(0x24) + u16(0x1d) + u16(32) + pub + u16(0x2b) + u16(3) + b"\2" + u16(0x304)
           + u16(0x0d) + u16(0x20) + u16(0x1e) + b"".join(u16(a) for a in (0x403, 0x503, 0x603, 0x203, 0x804, 0x805, 0x806, 0x401, 0x501, 0x601, 0x201, 0x402, 0x502, 0x602, 0x202))
           + u16(0x2d) + u16(2) + b"\1\1" + u16(0x1c) + u16(2) + u16(0x4001))
    body = u16(0x303) + rnd + b"\0" + u16(6) + u16(0x1301) + u16(0x1303) + u16(0x1302) + b"\1\0" + u16(len(ext)) + ext
    return b"\1" + u24(len(body)) + body
def record(typ, ver, payload): return bytes([typ]) + u16(ver) + u16(len(payload)) + payload
class Schedule:
    """The key schedule, from the ECDHE secret and the transcript, exactly as RFC 8446 section 7.1 lays it out."""
    def __init__(s, shared, th_sh):
        z = b"\0" * 32; s.early = extract(b"", z); d = derive_secret(s.early, "derived", H(b"")); s.hs = extract(d, shared)
        s.c_hs = derive_secret(s.hs, "c hs traffic", th_sh); s.s_hs = derive_secret(s.hs, "s hs traffic", th_sh)
        d2 = derive_secret(s.hs, "derived", H(b"")); s.master = extract(d2, z)
    def app(s, th_sf): s.c_ap = derive_secret(s.master, "c ap traffic", th_sf); s.s_ap = derive_secret(s.master, "s ap traffic", th_sf)
def finished_key(secret): return expand_label(secret, "finished", b"", 32)
def pss_sign(key, msg): return key.sign(msg, padding.PSS(padding.MGF1(hashes.SHA256()), 32), hashes.SHA256())
def cert_verify_message(th): return b"\x20" * 64 + b"TLS 1.3, server CertificateVerify\0" + th
class Server:
    """A TLS 1.3 server (TLS_AES_128_GCM_SHA256, X25519, rsa_pss_rsae_sha256) that answers a ClientHello message. Hooks let the tests damage any part of its flight."""
    def __init__(s, rsa_key, cert_der, srandom, spriv): s.key = rsa_key; s.cert = cert_der; s.srandom = srandom; s.spriv = spriv
    def respond(s, ch_msg, ee=None, tamper=None):
        cpub = None; o = 4 + 2 + 32; o += 1 + ch_msg[o]; cs = struct.unpack(">H", ch_msg[o:o + 2])[0]; o += 2 + cs; o += 1 + ch_msg[o]; el = struct.unpack(">H", ch_msg[o:o + 2])[0]; o += 2; end = o + el
        while o < end:
            ty, l = struct.unpack(">HH", ch_msg[o:o + 4]); o += 4
            if ty == 0x33: cpub = ch_msg[o + 6:o + 6 + 32]
            o += l
        shared = x25519(s.spriv, cpub); spub = pub_of(s.spriv)
        sh_body = u16(0x303) + s.srandom + b"\0" + u16(0x1301) + b"\0" + u16(0x2e) + u16(0x33) + u16(0x24) + u16(0x1d) + u16(32) + spub + u16(0x2b) + u16(2) + u16(0x304)
        sh = b"\2" + u24(len(sh_body)) + sh_body; tr = ch_msg + sh; sch = Schedule(shared, H(tr)); sk = Keys(sch.s_hs)
        ee_msg = b"\x08" + u24(len(ee if ee is not None else b"\0\0")) + (ee if ee is not None else b"\0\0")
        cert_entry = u24(len(s.cert)) + s.cert + u16(0); cert_body = b"\0" + u24(len(cert_entry)) + cert_entry; cert_msg = b"\x0b" + u24(len(cert_body)) + cert_body
        tr += ee_msg + cert_msg; sig = pss_sign(s.key, cert_verify_message(H(tr))); cv_body = u16(0x0804) + u16(len(sig)) + sig; cv_msg = b"\x0f" + u24(len(cv_body)) + cv_body; tr += cv_msg
        fin = hmac.new(finished_key(sch.s_hs), H(tr), hashlib.sha256).digest(); fin_msg = b"\x14" + u24(32) + fin; tr += fin_msg
        sch.app(H(tr)); flight = {"sh": record(22, 0x303, sh), "ee": ee_msg, "cert": cert_msg, "cv": cv_msg, "fin": fin_msg}
        if tamper: tamper(flight, sch)
        cf_expected = hmac.new(finished_key(sch.c_hs), H(tr), hashlib.sha256).digest()
        return {"sh": flight["sh"], "msgs": [flight["ee"], flight["cert"], flight["cv"], flight["fin"]], "sch": sch, "sk": sk, "tr": tr, "cf": cf_expected, "sh_msg": sh, "spub": spub}
