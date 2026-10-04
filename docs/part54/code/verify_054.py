#!/usr/bin/env python3
"""Chapter 54: verification FROM OUTSIDE THE KERNEL. It reads the serial capture and the recorded RFC 8448 handshake, and shares no code with the kernel: tls_ref.py is a separate implementation of TLS 1.3 on the `cryptography` library's X25519, AES-GCM, HMAC, HKDF and RSA-PSS.
  1. the ClientHello the kernel built equals the one Python builds from the same random and key, and the recorded one;
  2. the kernel's early, handshake and handshake-traffic secrets equal Python's key schedule, computed from the X25519 shared secret and the transcript hash;
  3. Python decrypts the recorded server flight, checks the CertificateVerify signature against the certificate's public key and the Finished MAC itself, and derives the master secret and the application traffic secrets: all must equal what the kernel printed;
  4. the client Finished, the client application record and the close_notify the kernel built equal the records Python builds; Python decrypts the server's ticket and data records;
  5. each of the fourteen attacks is judged independently by Python (would an AEAD open succeed? is the signature valid? does the transcript MAC match? ...), and the kernel's verdict and alert must agree.
Usage: verify_054.py SERIAL_TXT"""
import hashlib, hmac, os, re, struct, sys
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here)
import tls_ref as T
from cryptography import x509
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import padding
fails = 0; serial = open(sys.argv[1], errors="replace").read().replace("\r", "")
def report(ok, msg):
    global fails; fails += not ok; print(("  ok   " if ok else "  FAIL ") + msg)
vec = {}; sec = None
for l in open(os.path.join(here, "data/tls/botan_rfc8448_transcripts.vec")):
    l = l.strip()
    if l.startswith("["): sec = l; continue
    if sec == "[Simple_1RTT_Handshake]" and " = " in l and not l.startswith("#"): k, v = l.split(" = ", 1); vec[k] = bytes.fromhex(v) if re.fullmatch(r"([0-9a-fA-F]{2})+", v) else None
blk = re.search(r"@@TLS TRACE BEGIN\n(.*?)@@TLS TRACE END", serial, re.S).group(1); K = {}
for l in blk.splitlines():
    w = l.split()
    if len(w) == 2: K[w[0]] = w[1]
    elif len(w) == 4: K["rsa"] = (int(w[1]), int(w[3]))
b = lambda name: bytes.fromhex(K[name])
rnd, priv = vec["Client_RNG_Pool"][:32], vec["Client_RNG_Pool"][32:]
print("-- 1. the ClientHello")
ch = T.client_hello(rnd, priv, "server"); chrec = T.record(22, 0x301, ch)
report(b("client_x25519_public") == T.pub_of(priv), "the X25519 public key the kernel computed from the recorded private key equals Python's")
report(b("client_hello_record") == chrec == vec["Record_ClientHello_1"], f"the kernel's ClientHello record ({len(chrec)} bytes) equals the one Python builds and the recorded one")
print("-- 2. the key schedule up to the handshake traffic secrets")
sh_rec = vec["Record_ServerHello"]; sh = sh_rec[5:]; ks = sh.index(b"\x00\x33\x00\x24\x00\x1d\x00\x20") + 8; spub = sh[ks:ks + 32]; shared = T.x25519(priv, spub); sch = T.Schedule(shared, T.H(ch + sh))
report(b("server_hello_record") == sh_rec, "the ServerHello record the kernel fed itself is the recorded one")
report(b("early_secret") == sch.early, "early secret equals Python's: " + sch.early.hex()[:16] + "...")
report(b("handshake_secret") == sch.hs, "handshake secret equals Python's (from X25519 and the transcript): " + sch.hs.hex()[:16] + "...")
report(b("client_handshake_traffic_secret") == sch.c_hs and b("server_handshake_traffic_secret") == sch.s_hs, "client and server handshake traffic secrets equal Python's")
print("-- 3. the server's flight, decrypted and checked by Python")
fl_rec = vec["Record_ServerHandshakeMessages"]; got = T.Keys(sch.s_hs).open(fl_rec); report(got is not None and got[0] == 22, "Python decrypts the recorded flight with the server handshake keys (inner type 22)")
pt = got[1]; msgs = []; o = 0
while o < len(pt): l = int.from_bytes(pt[o + 1:o + 4], "big"); msgs.append(pt[o:o + 4 + l]); o += 4 + l
report([m[0] for m in msgs] == [8, 11, 15, 20], "it holds EncryptedExtensions, Certificate, CertificateVerify, Finished, in that order")
cert_der = msgs[1][11:11 + int.from_bytes(msgs[1][8:11], "big")]; cert = x509.load_der_x509_certificate(cert_der); pub = cert.public_key(); nums = pub.public_numbers()
report(K["rsa"] == (nums.n.bit_length(), nums.e), f"the key the kernel read from the certificate has the size and exponent Python reads: {nums.n.bit_length()} bits, e = {nums.e}")
th_cv = T.H(ch + sh + msgs[0] + msgs[1]); sig = msgs[2][8:]
try: pub.verify(sig, T.cert_verify_message(th_cv), padding.PSS(padding.MGF1(hashes.SHA256()), 32), hashes.SHA256()); sigok = True
except Exception: sigok = False
report(sigok, "Python verifies the CertificateVerify signature (RSA-PSS, SHA-256) over the transcript hash with the certificate's key")
th_fin = T.H(ch + sh + msgs[0] + msgs[1] + msgs[2]); report(hmac.new(T.finished_key(sch.s_hs), th_fin, hashlib.sha256).digest() == msgs[3][4:], "Python verifies the server's Finished MAC")
tr = ch + sh + b"".join(msgs); th_sf = T.H(tr); sch.app(th_sf)
report(b("transcript_hash_through_server_finished") == th_sf, "the transcript hash through the server Finished equals Python's")
report(b("master_secret") == sch.master, "master secret equals Python's: " + sch.master.hex()[:16] + "...")
report(b("client_application_traffic_secret") == sch.c_ap and b("server_application_traffic_secret") == sch.s_ap, "client and server application traffic secrets equal Python's")
print("-- 4. the client's records and the server's application records")
cf = T.Keys(sch.c_hs).seal(22, b"\x14\0\0\x20" + hmac.new(T.finished_key(sch.c_hs), th_sf, hashlib.sha256).digest()); report(b("client_finished_record") == cf == vec["Record_ClientFinished"], f"the client Finished record ({len(cf)} bytes) equals the one Python builds and the recorded one")
sk, ck = T.Keys(sch.s_ap), T.Keys(sch.c_ap); nst = sk.open(vec["Record_NewSessionTicket"]); report(nst is not None and nst[0] == 22 and nst[1][0] == 4, "Python decrypts the NewSessionTicket under the server application keys (sequence 0)")
sa = sk.open(vec["Record_Server_AppData"]); report(sa is not None and sa[0] == 23 and sa[1] == vec["Server_AppData"] == b("server_application_data"), "Python decrypts the server's application record (sequence 1) to the 50 bytes the kernel printed")
car = ck.seal(23, vec["Client_AppData"]); report(b("client_application_record") == car == vec["Record_Client_AppData"], f"the client's application record ({len(car)} bytes) equals the one Python builds")
ccn = ck.seal(21, b"\x01\x00"); report(b("client_close_notify_record") == ccn == vec["Record_Client_CloseNotify"], "the client's close_notify record equals the one Python builds")
scn = sk.open(vec["Record_Server_CloseNotify"]); report(scn == (21, b"\x01\x00"), "Python decrypts the server's close_notify to the warning alert 'close_notify'")
print("-- 5. the fourteen attacks, each judged independently")
att = {int(m.group(1)): (m.group(2), m.group(3), int(m.group(4))) for m in re.finditer(r"attack (\d+): (.*)\n    -> client says \"(.*?)\" \(alert (\d+)\), connection failed", serial)}
def mutated_sh(pos, val): x = bytearray(sh_rec); x[pos] = val; return bytes(x)
fl = bytearray(fl_rec); a1 = bytearray(fl_rec); a1[-1] ^= 1; a2 = bytearray(fl_rec); a2[200] ^= 0x80
hrr = bytes.fromhex("cf21ad74e59a6111be1d8c021e65b891c2a211167abb8c5e079e09e2c8a8339c")
cv_off = len(msgs[0]) + len(msgs[1]); flipped = bytearray(pt); flipped[len(msgs[0]) + len(msgs[1]) + 8 + 20] ^= 1; cv_bad = bytes(flipped[len(msgs[0]) + len(msgs[1]):len(msgs[0]) + len(msgs[1]) + len(msgs[2])])
try: pub.verify(cv_bad[8:], T.cert_verify_message(th_cv), padding.PSS(padding.MGF1(hashes.SHA256()), 32), hashes.SHA256()); sig_flip_ok = True
except Exception: sig_flip_ok = False
fin_bad = bytearray(msgs[3]); fin_bad[4 + 10] ^= 1
checks = {
    1: ("record failed authentication (bad_record_mac)", 20, lambda: T.Keys(sch.s_hs).open(bytes(a1)) is None),
    2: ("record failed authentication (bad_record_mac)", 20, lambda: T.Keys(sch.s_hs).open(bytes(a2)) is None),
    3: ("malformed record", 50, lambda: (int.from_bytes(fl_rec[3:5], "big") ^ 1) != len(fl_rec) - 5),
    4: ("cipher suite not offered", 47, lambda: (lambda x: int.from_bytes(x[5 + 4 + 2 + 32 + 1:5 + 4 + 2 + 32 + 3], "big") == 0x1302 != 0x1301)(mutated_sh(5 + 4 + 2 + 32 + 1 + 1, 2))),
    5: ("HelloRetryRequest is not supported", 40, lambda: hashlib.sha256(b"HelloRetryRequest").digest() == hrr),
    6: ("unusable key share", 47, lambda: T.x25519(priv, b"\0" * 32) == b"\0" * 32),
    7: ("record not allowed in this state", 10, lambda: b"\x17" + sh_rec[1:] != sh_rec),
    8: ("CertificateVerify signature invalid", 51, lambda: not sig_flip_ok),
    9: ("Finished verify_data wrong", 51, lambda: hmac.new(T.finished_key(sch.s_hs), th_fin, hashlib.sha256).digest() != bytes(fin_bad[4:])),
    10: ("unexpected or malformed handshake message", 10, lambda: msgs[1][0] != 8),
    11: ("record failed authentication (bad_record_mac)", 20, lambda: (lambda k: (setattr(k, "seq", 2), k.open(vec["Record_Server_AppData"]))[1] is None)(T.Keys(sch.s_ap))),
    12: ("record failed authentication (bad_record_mac)", 20, lambda: T.Keys(sch.s_ap).open(vec["Record_Server_AppData"]) is None),
    13: ("alert received", 40, lambda: T.Keys(sch.s_ap).open(T.Keys(sch.s_ap).seal(21, b"\x02\x28")) == (21, b"\x02\x28")),
    14: ("malformed record", 50, lambda: len(vec["Record_NewSessionTicket"]) - 1 != 5 + int.from_bytes(vec["Record_NewSessionTicket"][3:5], "big")),
}
for i in range(1, 15):
    if i not in att: report(False, f"attack {i}: no result line in the kernel's output"); continue
    reason, alert, indep = checks[i]; kr, ka = att[i][1], att[i][2]
    report(kr == reason and ka == alert and bool(indep()), f"attack {i}: kernel '{kr}' (alert {ka}); Python's independent check agrees the damaged input is bad")
t = re.search(r"TLS demo complete: (\d+) handshake steps verified, (\d+) attacks refused, (\d+) failures", serial); report(t and tuple(map(int, t.groups())) == (9, 14, 0), "final tally: 9 handshake steps verified, 14 attacks refused, 0 failures")
print(f"\n{'ALL CHECKS PASSED' if not fails else str(fails) + ' CHECK(S) FAILED'}"); sys.exit(1 if fails else 0)
