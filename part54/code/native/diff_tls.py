#!/usr/bin/env python3
"""Chapter 54: the handshake differential test and the attack catalogue. The C client (tls_hs, the SAME 054_tls.c the kernel links, with AddressSanitizer + UBSan) talks to the independent Python server (tls_ref.py, built on the `cryptography` library) and must agree with the
independent Python client on every byte it sends and on every secret it derives.
  A. N complete handshakes with random keys (1024- and 2048-bit server keys; the server's flight in one record, split in two, and split into one-byte records; with and without padding): the ClientHello record, the client Finished record, all secrets (handshake, master, the four traffic
     secrets, the transcript hash), then application records both ways (lengths 0 to 16384), a NewSessionTicket and close_notify, every record compared byte for byte.
  B. A catalogue of damaged servers, each with the alert the protocol demands: bad signature, bad Finished, messages in the wrong order, duplicated or missing messages, a CertificateRequest, a wrong or malformed certificate, a ServerHello with every kind of wrong field, a HelloRetryRequest,
     low-order key shares, downgrade sentinels, replayed, reordered and truncated records, and a fatal alert.
  C. Every byte of the ServerHello record and of the server's encrypted flight flipped, one at a time: the client must NEVER reach the connected state.
  D. Random garbage records after a valid ServerHello: no crash, never connected.   Usage: diff_tls.py N [tool]"""
import os, random, struct, subprocess, sys
from datetime import datetime, timedelta, timezone
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, os.path.join(here, ".."))
import tls_ref as T
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
N = int(sys.argv[1]); tool = sys.argv[2] if len(sys.argv) > 2 else "/tmp/ths"; R = random.Random(5454); fails = 0; counts = {}
OK, E_STATE, E_RECORD, E_DECRYPT, E_HS, E_VER, E_SUITE, E_HRR, E_KEY, E_CERT, E_SIG, E_FIN, E_SPACE, E_ALERT, E_SEQ, E_CLOSED = 0, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12, -13, -14, -15, -16
A_UNEXP, A_MAC, A_OVER, A_HSF, A_BADCERT, A_ILLEGAL, A_DECODE, A_DECRYPT, A_PROTO, A_INTERNAL, A_MISSING, A_UNSUPEXT = 10, 20, 22, 40, 42, 47, 50, 51, 70, 80, 109, 110
CONNECTED, CLOSED, FAILED = 3, 4, 5
p = subprocess.Popen([tool], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
def ask(line): p.stdin.write(line + "\n"); p.stdin.flush(); return p.stdout.readline().strip()
def hx(b): return b.hex() if b else "-"
def rb(n): return bytes(R.randrange(256) for _ in range(n))
def note(kind): counts[kind] = counts.get(kind, 0) + 1
def bad(kind, msg):
    global fails; fails += 1
    if fails <= 8: print(f"DIFFERENT [{kind}] {msg}")
def parse(line):
    w = line.split(); d = {"raw": line}
    for i in range(0, len(w) - 1, 2): d[w[i]] = w[i + 1]
    d["rc"] = int(d["rc"]) if "rc" in d else None; d["alert"] = int(d.get("alert", -1)); d["state"] = int(d.get("state", -1)); return d
def hexb(s): return b"" if s in ("-", None) else bytes.fromhex(s)
def make_cert(key, cn="server"):
    name = x509.Name([x509.NameAttribute(x509.oid.NameOID.COMMON_NAME, cn)]); now = datetime(2024, 1, 1, tzinfo=timezone.utc)
    return x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(key.public_key()).serial_number(1000 + R.randrange(1000)).not_valid_before(now).not_valid_after(now + timedelta(days=3650)).sign(key, hashes.SHA256()).public_bytes(serialization.Encoding.DER)
def rfc_cert():  # the certificate RFC 8448's server sends: taken from the recorded Certificate message
    for l in open(os.path.join(here, "..", "data/tls/botan_rfc8448_transcripts.vec")):
        if l.startswith("Message_Server_Certificate = "): m = bytes.fromhex(l.split(" = ")[1].strip()); n = int.from_bytes(m[8:11], "big"); return m[11:11 + n]
K1024 = serialization.load_pem_private_key(open(os.path.join(here, "..", "data/tls/botan_server_key.pem"), "rb").read(), None); C1024 = rfc_cert()
K2048 = serialization.load_pem_private_key(open(os.path.join(here, "..", "data/tls/test_rsa2048.pem"), "rb").read(), None); C2048 = make_cert(K2048); KOTHER = rsa.generate_private_key(65537, 1024)
SNI = "server"
def start(seed=None):
    rnd, priv = rb(32), rb(32); ask("reset"); r = parse(ask(f"hello {rnd.hex()} {priv.hex()} {SNI}")); ch = T.client_hello(rnd, priv, SNI); crec = T.record(22, 0x301, ch)
    if r["rc"] != 0 or hexb(r["rec"]) != crec: bad("ClientHello", f"the C ClientHello differs from the Python one ({r['raw'][:60]})")
    note("ClientHello records compared"); return rnd, priv, ch
def server(key=K1024, cert=C1024): return T.Server(key, cert, rb(32), rb(32))
def feed(rec): return parse(ask(f"rec {rec.hex()}"))
def expect(kind, r, rc, alert=None, state=FAILED):
    ok = r["rc"] == rc and (alert is None or r["alert"] == alert) and r["state"] == state
    note(kind + (" ... correct result" if ok else " ... WRONG"))
    if not ok: bad(kind, f"expected rc {rc} alert {alert} state {state}, got {r['raw'][:90]}")
    return ok
def flight_records(f, mode="one", pad=0):
    pt = b"".join(f["msgs"]); sk = f["sk"]
    if mode == "one": parts = [pt]
    elif mode == "two": k = R.randrange(1, len(pt)); parts = [pt[:k], pt[k:]]
    elif mode == "bytes": parts = [pt[i:i + 1] for i in range(len(pt))]
    else: parts = [pt]
    out = []
    for part in parts:
        inner = part + b"\x16" + b"\0" * pad; hdr = b"\x17\x03\x03" + T.u16(len(inner) + 16); out.append(hdr + T.gcm_seal(sk.key, T.nonce(sk.iv, sk.seq), hdr, inner)); sk.seq += 1
    return out
def cli_secrets(): return ask("secrets").split()
# ---------------------------------------------------------------- A. complete handshakes
for i in range(N):
    key, cert = (K1024, C1024) if i % 2 == 0 else (K2048, C2048); rnd, priv, ch = start(); srv = server(key, cert); f = srv.respond(ch); sch = f["sch"]
    r = feed(f["sh"]); 
    if r["rc"] != 0: bad("ServerHello", f"valid ServerHello refused: {r['raw'][:80]}"); continue
    sec = cli_secrets(); want = [sch.hs, sch.c_hs, sch.s_hs]
    if [bytes.fromhex(x) for x in sec[:3]] != want: bad("key schedule", "handshake secret or handshake traffic secrets differ")
    note("handshake-secret triples compared")
    mode = ["one", "two", "bytes"][i % 3] if i % 7 else "one"; pad = [0, 0, 1, 17][i % 4]; recs = flight_records(f, mode, pad); last = None
    for rec in recs:
        last = feed(rec)
        if last["rc"] != 0: break
    if last["rc"] != 0 or last["state"] != CONNECTED: bad("flight", f"valid flight (mode {mode}, pad {pad}, {len(key.public_key().public_numbers().n.to_bytes(256, 'big').lstrip(bytes(1))) * 8}-bit key) refused: {last['raw'][:90]}"); continue
    note(f"server flights accepted (all in one record / split in two / one byte per record / padded)")
    cfin = T.Keys(sch.c_hs).seal(22, b"\x14\0\0\x20" + f["cf"])
    if hexb(last["out"]) != cfin: bad("client Finished", "the client Finished record differs from the Python client's")
    note("client Finished records compared")
    sec = cli_secrets(); want = [sch.hs, sch.c_hs, sch.s_hs, sch.master, sch.c_ap, sch.s_ap, T.H(f["tr"])]
    if [bytes.fromhex(x) for x in sec] != want: bad("key schedule", "master secret, application traffic secrets or the transcript hash differ")
    note("full secret sets (7 values each) compared")
    sk, ck = T.Keys(sch.s_ap), T.Keys(sch.c_ap)
    nst = b"\x04" + T.u24(4 + 4 + 8 + 4 + 2 + 20 + 2) + struct.pack(">II", 7200, 1234) + b"\x08" + rb(8) + T.u16(20) + rb(20) + T.u16(0)
    r = feed(sk.seal(22, nst)); 
    if r["rc"] != 0: bad("NewSessionTicket", r["raw"][:80])
    for ln in [0, 1, 50, 255, 1000, 16384, R.randrange(2, 5000)]:
        d = rb(ln); r = feed(sk.seal(23, d)); 
        if r["rc"] != 0 or hexb(r["app"]) != d: bad("application data", f"server record of {ln} bytes not delivered intact: {r['raw'][:80]}")
        note("server application records delivered")
        d = rb(ln); r = parse(ask(f"send {hx(d)}"))
        if r["rc"] != 0 or hexb(r["rec"]) != ck.seal(23, d): bad("application data", f"client record of {ln} bytes differs from the Python client's")
        note("client application records compared (byte for byte)")
    r = parse(ask(f"send {hx(rb(16385))}")); note("a 16,385-byte send is refused" + (" ... correct result" if r["rc"] == -1 else " ... WRONG"))
    if r["rc"] != -1: bad("send limit", r["raw"][:60])
    r = parse(ask("close")); 
    if r["rc"] != 0 or hexb(r["rec"]) != ck.seal(21, b"\x01\x00"): bad("close_notify", "the client's close_notify differs")
    note("client close_notify records compared")
    r = parse(ask("send 00")); note("sending after close is refused" + (" ... correct result" if r["rc"] == -2 else " ... WRONG"))
    r = feed(sk.seal(21, b"\x01\x00")); note("server close_notify after the client's own close is ignored without error" if r["rc"] in (0, -16) else "WRONG")
# ---------------------------------------------------------------- B. the attack catalogue
def attack(kind, tamper=None, rc=None, alert=None, key=K1024, cert=C1024, mutate_sh=None, seal_kw=None, modify_records=None, state=FAILED):
    rnd, priv, ch = start(); srv = server(key, cert); f = srv.respond(ch, tamper=tamper); sh_rec = f["sh"]
    if mutate_sh: sh_rec = mutate_sh(f)
    r = feed(sh_rec)
    if r["rc"] != 0 or modify_records == "sh_only":
        return expect(kind, r, rc, alert, state) if r["rc"] != 0 or modify_records == "sh_only" else None
    recs = flight_records(f)
    if modify_records: recs = modify_records(f, recs)
    for rec in recs:
        r = feed(rec)
        if r["rc"] != 0: break
    return expect(kind, r, rc, alert, state)
def tamper_msgs(fn):
    def t(flight, sch): fn(flight, sch)
    return t
def swap(a, b): 
    def t(fl, sch): fl[a], fl[b] = fl[b], fl[a]
    return t
def flipbit(msg_key, pos_from_end):
    def t(fl, sch): m = bytearray(fl[msg_key]); m[len(m) - 1 - pos_from_end] ^= 1; fl[msg_key] = bytes(m)
    return t
attack("flight: CertificateVerify signature with one bit flipped", flipbit("cv", 5), E_SIG, A_DECRYPT)
attack("flight: Finished verify_data with one bit flipped", flipbit("fin", 3), E_FIN, A_DECRYPT)
attack("flight: Certificate before EncryptedExtensions", swap("ee", "cert"), E_HS, A_UNEXP)
attack("flight: CertificateVerify before Certificate", swap("cert", "cv"), E_HS, A_UNEXP)
attack("flight: Finished before CertificateVerify", swap("cv", "fin"), E_HS, A_UNEXP)
attack("flight: EncryptedExtensions sent twice", lambda fl, s: fl.__setitem__("ee", fl["ee"] * 2), E_HS, A_UNEXP)
attack("flight: no Certificate and no CertificateVerify (EncryptedExtensions then Finished)", lambda fl, s: (fl.__setitem__("cert", b""), fl.__setitem__("cv", b"")), E_HS, A_UNEXP)
attack("flight: a CertificateRequest (client authentication) before the Certificate", lambda fl, s: fl.__setitem__("ee", fl["ee"] + b"\x0d\0\0\x04\0\0\0\0"), E_HS, A_HSF)
attack("flight: an unknown handshake type (99) after EncryptedExtensions", lambda fl, s: fl.__setitem__("ee", fl["ee"] + b"\x63\0\0\0"), E_HS, A_UNEXP)
attack("flight: a handshake message follows the server Finished in the same record", lambda fl, s: fl.__setitem__("fin", fl["fin"] + b"\x04\0\0\0"), E_HS, A_UNEXP)
attack("flight: EncryptedExtensions whose extension list overruns its message", lambda fl, s: fl.__setitem__("ee", b"\x08\0\0\x06\0\x08\0\x0a\0\x40\0\0"), E_HS, A_DECODE)
attack("flight: a Certificate with a non-zero request context", lambda fl, s: fl.__setitem__("cert", b"\x0b" + T.u24(len(fl["cert"]) - 3) + b"\x01" + fl["cert"][5:]), E_HS, A_DECODE)
attack("flight: a Certificate with an empty certificate list", lambda fl, s: fl.__setitem__("cert", b"\x0b\0\0\4\0\0\0\0"), E_CERT, A_BADCERT)
attack("flight: a Certificate whose first entry is not a certificate (garbage DER)", lambda fl, s: fl.__setitem__("cert", b"\x0b" + T.u24(4 + 3 + 40 + 2) + b"\0" + T.u24(3 + 40 + 2) + T.u24(40) + rb(40) + T.u16(0)), E_CERT, A_BADCERT)
attack("flight: a Certificate whose key is an EC key (not RSA)", lambda fl, s: fl.__setitem__("cert", b"\x0b" + T.u24(4 + 3 + 3 + 2) + b"\0" + T.u24(3 + 3 + 2) + T.u24(3) + b"\x30\x01\x00" + T.u16(0)), E_CERT, A_BADCERT)
def oid_changed(fl, sch):
    c = fl["cert"]; i = c.index(bytes.fromhex("2a864886f70d010101")); fl["cert"] = c[:i + 8] + b"\x0a" + c[i + 9:]
attack("flight: a Certificate whose public key algorithm OID is rsassa-pss (1.2.840.113549.1.1.10), not rsaEncryption", oid_changed, E_CERT, A_BADCERT)
def bitstring_unused(fl, sch):
    c = fl["cert"]; i = c.index(bytes.fromhex("2a864886f70d010101")); j = c.index(b"\x03\x81\x8d\x00", i); fl["cert"] = c[:j + 3] + b"\x01" + c[j + 4:]
attack("flight: a Certificate whose subjectPublicKey BIT STRING declares 1 unused bit", bitstring_unused, E_CERT, A_BADCERT)
def rebuild_cert(fl, der):  # a Certificate message carrying `der` as its only entry
    entry = T.u24(len(der)) + der + T.u16(0); body = b"\0" + T.u24(len(entry)) + entry; fl["cert"] = b"\x0b" + T.u24(len(body)) + body
def der_three_byte_len(fl, sch): rebuild_cert(fl, b"\x30\x83\x00" + C1024[2:4] + C1024[4:])
attack("flight: a Certificate whose outer DER length uses the three-byte form (unsupported)", der_three_byte_len, E_CERT, A_BADCERT)
def der_len_too_big(fl, sch): c = bytearray(C1024); c[6:8] = b"\xff\xff"; rebuild_cert(fl, bytes(c))
attack("flight: a Certificate whose tbsCertificate length claims more bytes than exist", der_len_too_big, E_CERT, A_BADCERT)
def exponent_5_bytes(fl, sch): c = C1024; i = c.index(b"\x02\x03\x01\x00\x01"); rebuild_cert(fl, c[:i] + b"\x02\x05\x01\x00\x00\x00\x01" + c[i + 5:])
attack("flight: a Certificate whose public exponent is five bytes long (2^32 + 1)", exponent_5_bytes, E_CERT, A_BADCERT)
KBIG = rsa.generate_private_key(65537, 2056); CBIG = make_cert(KBIG)
attack("flight: a Certificate holding a 2056-bit RSA key (larger than the 2048-bit limit)", None, E_CERT, A_BADCERT, key=KBIG, cert=CBIG)
KSMALL = serialization.load_pem_private_key(open(os.path.join(here, "..", "data/tls/test_rsa1008.pem"), "rb").read(), None); CSMALL = make_cert(KSMALL)
attack("flight: a Certificate holding a 1008-bit RSA key (smaller than the 1024-bit limit)", None, E_CERT, A_BADCERT, key=KSMALL, cert=CSMALL)
def huge_hs_len(fl, sch): fl["ee"] = b"\x08\xff\xff\xff" + fl["ee"][4:]
attack("flight: a handshake message header claiming 16,777,215 bytes", huge_hs_len, E_HS, A_ILLEGAL)
attack("flight: a CertificateVerify with signature scheme 0x0403 (ecdsa) instead of 0x0804", lambda fl, s: fl.__setitem__("cv", fl["cv"][:4] + b"\x04\x03" + fl["cv"][6:]), E_SIG, A_ILLEGAL)
attack("flight: a CertificateVerify whose signature length field is wrong", lambda fl, s: fl.__setitem__("cv", fl["cv"][:6] + b"\x00\x7f" + fl["cv"][8:]), E_HS, A_DECODE)
def wrongkey(fl, sch):
    th = T.H(fl["tr_before_cv"]) if "tr_before_cv" in fl else None
    sig = T.pss_sign(KOTHER, b"x"); fl["cv"] = b"\x0f" + T.u24(4 + len(sig)) + T.u16(0x0804) + T.u16(len(sig)) + sig
attack("flight: a CertificateVerify signed by a different RSA key than the certificate's", wrongkey, E_SIG, A_DECRYPT)
attack("flight: a CertificateVerify that signs the wrong transcript hash", lambda fl, s: fl.__setitem__("cv", b"\x0f" + T.u24(4 + 128) + T.u16(0x0804) + T.u16(128) + T.pss_sign(K1024, T.cert_verify_message(b"\0" * 32))), E_SIG, A_DECRYPT)
attack("flight: a Finished of the wrong length (31 bytes)", lambda fl, s: fl.__setitem__("fin", b"\x14\0\0\x1f" + fl["fin"][4:35]), E_HS, A_DECODE)
attack("flight: a Finished that is the client's key schedule value, not the server's", lambda fl, s: fl.__setitem__("fin", b"\x14\0\0\x20" + T.hmac.new(T.finished_key(s.c_hs), T.H(b""), T.hashlib.sha256).digest()), E_FIN, A_DECRYPT)
# ServerHello damage
def sh_with(f, **kw):
    spub = f["spub"]; version = kw.get("version", 0x303); random_ = kw.get("random_", kw.get("random", f["sh_msg"][6:38])); sid = kw.get("sid", b""); suite = kw.get("suite", 0x1301); comp = kw.get("comp", 0)
    ks = kw.get("ks", T.u16(0x33) + T.u16(36) + T.u16(0x1d) + T.u16(32) + spub); sv = kw.get("sv", T.u16(0x2b) + T.u16(2) + T.u16(0x304)); extra = kw.get("extra", b"")
    ext = ks + sv + extra if not kw.get("order_sv_first") else sv + ks + extra; body = T.u16(version) + random_ + bytes([len(sid)]) + sid + T.u16(suite) + bytes([comp]) + T.u16(len(ext)) + ext
    msg = b"\2" + T.u24(len(body)) + body; return T.record(22, 0x303, msg)
S = lambda **kw: (lambda f: sh_with(f, **kw))
attack("ServerHello: a HelloRetryRequest (the special random)", None, E_HRR, A_HSF, mutate_sh=S(random=bytes.fromhex("cf21ad74e59a6111be1d8c021e65b891c2a211167abb8c5e079e09e2c8a8339c")))
attack("ServerHello: cipher suite TLS_AES_256_GCM_SHA384 (not offered as usable)", None, E_SUITE, A_ILLEGAL, mutate_sh=S(suite=0x1302))
attack("ServerHello: cipher suite TLS_CHACHA20_POLY1305_SHA256 (offered, not implemented)", None, E_SUITE, A_ILLEGAL, mutate_sh=S(suite=0x1303))
attack("ServerHello: cipher suite 0x0000", None, E_SUITE, A_ILLEGAL, mutate_sh=S(suite=0))
attack("ServerHello: legacy_version 0x0302", None, E_VER, A_PROTO, mutate_sh=S(version=0x302))
attack("ServerHello: supported_versions says TLS 1.2 (0x0303)", None, E_VER, A_ILLEGAL, mutate_sh=S(sv=T.u16(0x2b) + T.u16(2) + T.u16(0x303)))
attack("ServerHello: no supported_versions extension", None, E_VER, A_MISSING, mutate_sh=S(sv=b""))
attack("ServerHello: no key_share extension", None, E_KEY, A_MISSING, mutate_sh=S(ks=b""))
attack("ServerHello: key_share for group secp256r1 (0x0017)", None, E_KEY, A_ILLEGAL, mutate_sh=lambda f: sh_with(f, ks=T.u16(0x33) + T.u16(4 + 65) + T.u16(0x17) + T.u16(65) + b"\4" + rb(64)))
attack("ServerHello: key_share for group secp256r1 whose data is 32 bytes long (right length, wrong group)", None, E_KEY, A_ILLEGAL, mutate_sh=lambda f: sh_with(f, ks=T.u16(0x33) + T.u16(36) + T.u16(0x17) + T.u16(32) + f["spub"]))
attack("ServerHello: key_share twice", None, E_KEY, A_ILLEGAL, mutate_sh=lambda f: sh_with(f, extra=T.u16(0x33) + T.u16(36) + T.u16(0x1d) + T.u16(32) + f["spub"]))
attack("ServerHello: key_share with a 31-byte X25519 key", None, E_KEY, A_ILLEGAL, mutate_sh=lambda f: sh_with(f, ks=T.u16(0x33) + T.u16(35) + T.u16(0x1d) + T.u16(31) + f["spub"][:31]))
for i, pt in enumerate(["00" * 32, "01" + "00" * 31, "e0eb7a7c3b41b8ae1656e3faf19fc46ada098deb9c32b1fd866205165f49b800", "5f9c95bca3508c24b1d0b1559c83ef5b04445cc4581c8e86d8224eddd09f1157"]):
    attack(f"ServerHello: low-order X25519 key share #{i + 1} (the shared secret would be all zero)", None, E_KEY, A_ILLEGAL, mutate_sh=lambda f, pt=pt: sh_with(f, ks=T.u16(0x33) + T.u16(36) + T.u16(0x1d) + T.u16(32) + bytes.fromhex(pt)))
attack("ServerHello: a non-empty legacy_session_id_echo", None, E_HS, A_ILLEGAL, mutate_sh=S(sid=b"\1" * 32))
attack("ServerHello: compression method 1", None, E_HS, A_ILLEGAL, mutate_sh=S(comp=1))
attack("ServerHello: an unknown extension (0x1234)", None, E_HS, A_UNSUPEXT, mutate_sh=S(extra=T.u16(0x1234) + T.u16(0)))
attack("ServerHello: a pre_shared_key extension although none was offered", None, E_HS, A_UNSUPEXT, mutate_sh=S(extra=T.u16(0x29) + T.u16(2) + T.u16(0)))
attack("ServerHello: supported_versions twice", None, E_VER, A_ILLEGAL, mutate_sh=lambda f: sh_with(f, extra=T.u16(0x2b) + T.u16(2) + T.u16(0x304)))
attack("ServerHello: the downgrade sentinel 'DOWNGRD\\x01' in the random", None, E_VER, A_ILLEGAL, mutate_sh=lambda f: sh_with(f, random_=rb(24) + b"DOWNGRD\x01"))
attack("ServerHello: the downgrade sentinel 'DOWNGRD\\x00' in the random", None, E_VER, A_ILLEGAL, mutate_sh=lambda f: sh_with(f, random_=rb(24) + b"DOWNGRD\x00"))
attack("ServerHello: handshake length field one too large", None, E_HS, A_DECODE, mutate_sh=lambda f: f["sh"][:6] + bytes([f["sh"][6] + 1]) + f["sh"][7:])
attack("ServerHello: a record claiming a plaintext length over 16,384", None, E_RECORD, A_OVER, mutate_sh=lambda f: b"\x16\x03\x03" + T.u16(16385) + b"\0" * 16385)
attack("ServerHello: sent as an application_data record (before any keys exist)", None, E_STATE, A_UNEXP, mutate_sh=lambda f: b"\x17" + f["sh"][1:])
attack("ServerHello: a record whose legacy_version is 0x0301", None, E_RECORD, A_PROTO, mutate_sh=lambda f: f["sh"][:1] + b"\x03\x01" + f["sh"][3:])
attack("ServerHello: a record with trailing bytes beyond its length", None, E_RECORD, A_DECODE, mutate_sh=lambda f: f["sh"] + b"\0")
attack("ServerHello: a record cut short", None, E_RECORD, A_DECODE, mutate_sh=lambda f: f["sh"][:-1])
attack("ServerHello: a change_cipher_spec record carrying 02 instead of 01", None, E_RECORD, A_UNEXP, mutate_sh=lambda f: b"\x14\x03\x03\0\1\2")
# record layer after the handshake
def tail_tests():
    for kind in ["replay", "reorder", "truncated", "extra byte", "app data before the handshake finished", "fatal alert", "unknown inner type", "all-zero inner plaintext", "oversized plaintext", "alert of wrong length", "plaintext record after ServerHello", "handshake message in application phase", "sequence number exhausted", "application data under handshake keys", "warning alert"]:
        rnd, priv, ch = start(); f = server().respond(ch); sch = f["sch"]; feed(f["sh"])
        if kind == "app data before the handshake finished":
            r = feed(T.Keys(sch.s_ap).seal(23, b"early")); expect("record layer: " + kind + " (sealed with application keys during the handshake)", r, E_DECRYPT, A_MAC); continue
        if kind == "application data under handshake keys":
            r = feed(T.Keys(sch.s_hs).seal(23, b"early data")); expect("record layer: application data (inner type 23) sealed with the handshake keys during the handshake", r, E_STATE, A_UNEXP); continue
        if kind == "plaintext record after ServerHello": r = feed(T.record(22, 0x303, b"\x14\0\0\x20" + rb(32))); expect("record layer: " + kind, r, E_STATE, A_UNEXP); continue
        for rec in flight_records(f): last = feed(rec)
        if last["rc"] != 0: bad("setup", last["raw"]); continue
        sk = T.Keys(sch.s_ap)
        if kind == "replay": r1 = feed(sk.seal(23, b"one")); r2 = feed(T.Keys(sch.s_ap).seal(23, b"one")); expect("record layer: a server record delivered twice (the second has a stale sequence number)", r2, E_DECRYPT, A_MAC)
        elif kind == "reorder": a, b = sk.seal(23, b"first"), sk.seal(23, b"second"); r = feed(b); expect("record layer: records delivered out of order", r, E_DECRYPT, A_MAC)
        elif kind == "truncated": rec = sk.seal(23, b"hello world"); r = feed(rec[:-1]); expect("record layer: a record cut short", r, E_RECORD, A_DECODE)
        elif kind == "extra byte": rec = sk.seal(23, b"hello world"); r = feed(rec + b"\0"); expect("record layer: a record with a trailing byte", r, E_RECORD, A_DECODE)
        elif kind == "fatal alert": r = feed(sk.seal(21, b"\x02\x28")); expect("record layer: a fatal handshake_failure alert from the server", r, E_ALERT, 40)
        elif kind == "unknown inner type": r = feed(sk.seal(99, b"x")); expect("record layer: an unknown inner content type (99)", r, E_RECORD, A_UNEXP)
        elif kind == "all-zero inner plaintext":
            hdr = b"\x17\x03\x03" + T.u16(20 + 16 - 16 + 16); inner = b"\0" * 20; hdr = b"\x17\x03\x03" + T.u16(len(inner) + 16); rec = hdr + T.gcm_seal(sk.key, T.nonce(sk.iv, sk.seq), hdr, inner); r = feed(rec); expect("record layer: an inner plaintext of only zero bytes (no content type)", r, E_RECORD, A_UNEXP)
        elif kind == "oversized plaintext": inner = b"x" * 16385 + b"\x17"; hdr = b"\x17\x03\x03" + T.u16(len(inner) + 16); rec = hdr + T.gcm_seal(sk.key, T.nonce(sk.iv, sk.seq), hdr, inner); r = feed(rec); expect("record layer: a protected record with 16,385 bytes of plaintext", r, E_RECORD, A_OVER)
        elif kind == "warning alert": r = feed(sk.seal(21, b"\x01\x5a")); expect("record layer: a warning alert other than close_notify (user_canceled, 90) is treated as fatal", r, E_ALERT, 90)
        elif kind == "alert of wrong length": r = feed(sk.seal(21, b"\x01")); expect("record layer: an alert one byte long", r, E_RECORD, A_DECODE)
        elif kind == "handshake message in application phase": r = feed(sk.seal(22, b"\x14\0\0\x20" + rb(32))); expect("record layer: a Finished message after the handshake", r, E_HS, A_UNEXP)
        elif kind == "sequence number exhausted":
            ask("setseq 0 ffffffffffffffff"); r = feed(sk.seal(23, b"x")); expect("record layer: the receive sequence number at 2^64 - 1 (the next record is refused, never wrapped)", r, E_SEQ, A_INTERNAL)
    # after FAILED everything is refused; after CLOSED likewise
    rnd, priv, ch = start(); f = server().respond(ch); feed(f["sh"]); recs = flight_records(f); bad_rec = bytearray(recs[0]); bad_rec[-1] ^= 1; r = feed(bytes(bad_rec)); expect("record layer: a flipped bit in the flight's tag", r, E_DECRYPT, A_MAC); r2 = feed(recs[0]); expect("a client that has failed refuses even the genuine record afterwards", r2, E_STATE, state=FAILED)
    rnd, priv, ch = start(); f = server().respond(ch); feed(f["sh"]); [feed(x) for x in flight_records(f)]; sk = T.Keys(f["sch"].s_ap); r = feed(sk.seal(21, b"\x01\x00")); expect("record layer: close_notify from the server closes the connection", r, OK, A_UNEXP - 10, CLOSED); r2 = feed(sk.seal(23, b"late")); expect("data after close_notify is refused (connection closed)", r2, E_CLOSED, state=CLOSED)
    r = parse(ask("send 00")); note("sending on a closed connection is refused" + (" ... correct result" if r["rc"] == -2 else " ... WRONG"))
    ask("reset"); r = feed(b"\x16\x03\x03\0\1\0"); expect("a record before the ClientHello is refused", r, E_STATE, state=0)
tail_tests()
# ---------------------------------------------------------------- C. every byte of the ServerHello record and of the flight, flipped
def never_connected(kind, which):
    rnd, priv, ch = start(); f = server().respond(ch); recs = [f["sh"]] + flight_records(f); target = recs[which]; total = len(target); outcomes = {}
    positions = range(total)
    for pos in positions:
        rnd2, priv2 = rnd, priv; ask("reset"); ask(f"hello {rnd.hex()} {priv.hex()} {SNI}"); damaged = bytearray(target); damaged[pos] ^= 1 << R.randrange(8); lst = list(recs); lst[which] = bytes(damaged); state = None; rc = None
        for rec in lst:
            r = feed(rec); rc, state = r["rc"], r["state"]
            if r["rc"] != 0: break
        outcomes[rc] = outcomes.get(rc, 0) + 1; note(kind + " ... byte flips tried")
        if state == CONNECTED: bad(kind, f"flipping byte {pos} left the client CONNECTED")
    return outcomes
o1 = never_connected("ServerHello record, every byte flipped: the client never connects", 0); o2 = never_connected("encrypted flight record, every byte flipped: the client never connects", 1)
print("   outcomes of the ServerHello flips (rc: count):", dict(sorted(o1.items())), "; of the flight flips:", dict(sorted(o2.items())))
# ---------------------------------------------------------------- D. garbage
for i in range(max(30, N)):
    rnd, priv, ch = start(); f = server().respond(ch); feed(f["sh"]); state = None
    for j in range(R.randrange(1, 6)):
        typ = R.choice([22, 23, 23, 20, 21, 24, 0, 255]); body = rb(R.choice([0, 1, 5, 16, 17, 40, 300])); rec = bytes([typ]) + (b"\x03\x03" if R.random() < .9 else rb(2)) + T.u16(len(body) if R.random() < .9 else R.randrange(0, 600)) + body
        r = feed(rec); state = r["state"]
        if state == CONNECTED: bad("garbage", "random records connected the client")
    note("random garbage records fed after a valid ServerHello (never connected)")
p.stdin.close(); p.wait()
for k in sorted(counts): print(f"  {k}: {counts[k]}")
print(f"{sum(counts.values())} checks, {fails} differences"); sys.exit(1 if fails else 0)
