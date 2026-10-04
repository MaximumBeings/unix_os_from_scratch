"""Chapter 50: builders shared by make_btc_data.py and native/gen_blocks.py: serialise a transaction (with or without witness data), mine a block at an easy target."""
import struct, btc_ref as R
def vi(n): return bytes([n]) if n < 0xfd else b"\xfd" + struct.pack("<H", n) if n <= 0xffff else b"\xfe" + struct.pack("<I", n)
def push(b): return bytes([len(b)]) + b
def ser_tx(ver, ins, outs, lt=0, wit=None):
    s = struct.pack("<i", ver)
    if wit is not None: s += b"\x00\x01"
    s += vi(len(ins)) + b"".join(h + struct.pack("<I", i) + vi(len(sc)) + sc + struct.pack("<I", q) for h, i, sc, q in ins)
    s += vi(len(outs)) + b"".join(struct.pack("<q", v) + vi(len(sc)) + sc for v, sc in outs)
    if wit is not None: s += b"".join(vi(len(st)) + b"".join(vi(len(x)) + x for x in st) for st in wit)
    return s + struct.pack("<I", lt)
def mine(ver, prev, root, tm, bits, txs_raw):
    tgt, _ = R.compact_nocheck(bits); pre = struct.pack("<I32s32sII", ver, prev, root, tm, bits)
    for nonce in range(1 << 32):
        h = R.dsha(pre + struct.pack("<I", nonce))
        if int.from_bytes(h, "little") <= tgt: return pre + struct.pack("<I", nonce) + vi(len(txs_raw)) + b"".join(txs_raw), h
    raise SystemExit("no nonce")
