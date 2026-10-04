#!/usr/bin/env python3
"""Chapter 50: random blocks for the differential test, VALID and deliberately DEFECTIVE, mined at an easy target (bits 0x207fffff, so a proof of work takes about two tries). gen(seed) -> block bytes. The defects: no coinbase,
two coinbases, a coinbase script of 1 or 101 bytes, a null input in a non-coinbase transaction, no inputs, no outputs, a negative output, an output above 21 million coins, two outputs that sum above it, a duplicate input, a duplicated
transaction (the Merkle-mutation shape), a wrong Merkle root, a wrong witness commitment, an oversized block weight (not generated: too large), a bad compact target (negative / overflow / zero), and random byte damage after mining."""
import os, random, struct, sys
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, os.path.join(here, "..")); import btc_ref as R
from btc_build import vi, push, ser_tx
BITS = 0x207fffff; NULL = b"\0" * 32
def rtx(Rn, defect=None, wit=False, n_in=None, n_out=None):
    n_in = n_in or Rn.randrange(1, 4); n_out = n_out or Rn.randrange(1, 4)
    ins = [(Rn.randbytes(32), Rn.randrange(0, 5), push(Rn.randbytes(Rn.randrange(0, 80))), Rn.choice([0xffffffff, 0xfffffffe, 0])) for _ in range(n_in)]
    outs = [(Rn.randrange(0, 5_000_000_000), Rn.randbytes(Rn.randrange(0, 40))) for _ in range(n_out)]
    if defect == "noin": ins = []
    if defect == "noout": outs = []
    if defect == "neg": outs[0] = (-Rn.randrange(1, 1000), outs[0][1])
    if defect == "big": outs[0] = (R.MAX_MONEY + Rn.randrange(1, 1000), outs[0][1])
    if defect == "total": outs = [(R.MAX_MONEY // 2 + 1, b"\x51"), (R.MAX_MONEY // 2 + 1, b"\x51")]
    if defect == "dupin" and ins: ins = ins + [ins[0]]
    if defect == "nullin": ins[0] = (NULL, 0xffffffff, ins[0][2], ins[0][3])
    wl = [[Rn.randbytes(Rn.randrange(0, 40)) for _ in range(Rn.randrange(0, 3))] for _ in ins] if wit else None
    if wl is not None and not any(wl): wl[0] = [b"\x01"]
    return ser_tx(Rn.choice([1, 2]), ins, outs, Rn.randrange(0, 2) * Rn.randrange(0, 1 << 32), wl) if ins or defect == "noin" else b""
def coinbase(Rn, height, ver, script_len=None, wit_reserved=None, commit=None, extra_out=None):
    sc = (push(height.to_bytes((height.bit_length() + 8) // 8, "little")) if ver >= 2 else push(Rn.randbytes(3))) + push(Rn.randbytes(Rn.randrange(0, 20)))
    if script_len is not None: sc = bytes(Rn.randrange(256) for _ in range(script_len))
    outs = [(Rn.randrange(0, 5_000_000_000), b"\x51")]
    if commit is not None: outs.append((0, b"\x6a\x24\xaa\x21\xa9\xed" + commit))
    return ser_tx(2, [(NULL, 0xffffffff, sc, 0xffffffff)], outs, 0, [[wit_reserved]] if wit_reserved is not None else None)
def ids(txs, wit=False):
    out = []
    for t in txs:
        try: p, _ = R.parse_tx(t, 0); out.append(p["wtxid" if wit else "txid"])
        except Exception: out.append(R.dsha(t))
    return out
def assemble(Rn, txs, ver, root=None, bits=BITS, prev=None, mine_it=True):
    ids_ = ids(txs); root = R.merkle(ids_)[0] if root is None else root
    pre = struct.pack("<I32s32sII", ver, prev if prev is not None else Rn.randbytes(32), root, 1700000000 + Rn.randrange(0, 100000), bits)
    tgt, _ = R.compact_nocheck(bits); body = vi(len(txs)) + b"".join(txs)
    nonce = 0
    while True:
        h = R.dsha(pre + struct.pack("<I", nonce))
        if not mine_it or int.from_bytes(h, "little") <= tgt: break
        nonce += 1
    return pre + struct.pack("<I", nonce) + body
KINDS = ["valid", "valid", "valid", "valid-wit", "valid-wit", "nocb", "twocb", "cb1", "cb101", "nullin", "noin", "noout", "neg", "big", "total", "dupin", "duptx", "badroot", "badwit", "badbits-neg", "badbits-over", "badbits-zero", "nopow", "damage", "ver1"]
def gen(seed):
    Rn = random.Random(seed); kind = KINDS[seed % len(KINDS)]; ver = Rn.choice([0x20000000, 0x20000000, 2, 4]) if kind != "ver1" else 1; height = Rn.randrange(1, 2_000_000)
    wit = kind.startswith("valid-wit") or kind == "badwit"; n_other = Rn.randrange(0, 5)
    others = [rtx(Rn, wit=wit and Rn.random() < .8) for _ in range(n_other)]
    commit = reserved = None
    if wit:
        reserved = Rn.randbytes(32) if Rn.random() < .9 else b"\x07" * 31
        wroot = R.merkle([NULL] + ids(others, True))[0]; commit = R.dsha(wroot + reserved)
        if kind == "badwit": commit = R.dsha(commit)
    cb = coinbase(Rn, height, ver, wit_reserved=reserved, commit=commit)
    txs = [cb] + others; root = None; bits = BITS; mine_it = True
    if kind == "nocb": txs = others if others else [rtx(Rn)]
    elif kind == "twocb": txs = [cb, coinbase(Rn, height + 1, ver)] + others
    elif kind == "cb1": txs = [coinbase(Rn, height, ver, script_len=1)] + others
    elif kind == "cb101": txs = [coinbase(Rn, height, ver, script_len=101)] + others
    elif kind in ("nullin", "noin", "noout", "neg", "big", "total", "dupin"): txs = [cb] + [rtx(Rn, kind)] + others
    elif kind == "duptx": a = rtx(Rn); b = rtx(Rn); txs = [cb, a, b, b] if Rn.random() < .5 else [cb, a, a]
    elif kind == "badroot": root = Rn.randbytes(32)
    elif kind == "badbits-neg": bits = 0x1d80ffff; mine_it = False    # an invalid target can never be met, so these are not mined: the verdict is 'high-hash' with the reason printed in the target line
    elif kind == "badbits-over": bits = 0xff00ffff; mine_it = False
    elif kind == "badbits-zero": bits = 0x1d000000; mine_it = False
    elif kind == "nopow": mine_it = False
    blk = assemble(Rn, txs, ver, root, bits, mine_it=mine_it)
    if kind == "damage":
        b = bytearray(blk)
        for _ in range(Rn.randrange(1, 4)): b[Rn.randrange(len(b))] ^= Rn.randrange(1, 256)
        if Rn.random() < .3: b = b[:Rn.randrange(1, len(b))]
        blk = bytes(b)
    return blk
if __name__ == "__main__": sys.stdout.buffer.write(gen(int(sys.argv[1])))
