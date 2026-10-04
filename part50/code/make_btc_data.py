#!/usr/bin/env python3
"""Chapter 50: writes the data files the kernel embeds (data/btc/*) and the manifest the verifier reads.
  1. ten REAL testnet3 blocks, copied from Bitcoin Core's own test data (src/test/data/blockfilters.json: block hex and published block hash), MIT licence;
  2. the REAL mainnet genesis block, rebuilt byte by byte from its published fields and PROVED by its published hash (000000000019d668...ce26f);
  3. Bitcoin Core's transaction test vectors (tx_valid.json, tx_invalid.json): 121 + 93 raw transactions, with Core's own expectation for the nine 'Tests for CheckTransaction()' entries;
  4. a SYNTHETIC chain of 12 blocks mined here at easy difficulty (labelled synthetic everywhere): linkage, BIP 34 heights, a witness block with a commitment, and a block with a duplicated transaction (the CVE-2012-2459 shape).
Usage: make_btc_data.py [BITCOIN_CORE_DATA_DIR]   (default /tmp/btc/src/test/data, a sparse checkout of github.com/bitcoin/bitcoin)"""
import hashlib, json, os, struct, sys
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here)
import btc_ref as R
SRC = sys.argv[1] if len(sys.argv) > 1 else "/tmp/btc/src/test/data"; OUT = os.path.join(here, "data", "btc"); os.makedirs(OUT, exist_ok=True)
from btc_build import vi, push, ser_tx, mine
# 1. real testnet blocks
manifest = []
for r in json.load(open(os.path.join(SRC, "blockfilters.json")))[1:]:
    raw = bytes.fromhex(r[2]); assert R.dsha(raw[:80])[::-1].hex() == r[1]; open(os.path.join(OUT, f"tn_{r[0]}.blk"), "wb").write(raw); manifest.append(f"testnet {r[0]} {r[1]} tn_{r[0]}.blk")
# 2. mainnet genesis, rebuilt from its published fields
msg = b"The Times 03/Jan/2009 Chancellor on brink of second bailout for banks"
cb_script = push(bytes.fromhex("ffff001d")) + push(b"\x04") + push(msg)
pub = bytes.fromhex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f")
tx = ser_tx(1, [(b"\0" * 32, 0xffffffff, cb_script, 0xffffffff)], [(5000000000, push(pub) + b"\xac")])
root = R.dsha(tx); hdr = struct.pack("<I32s32sIII", 1, b"\0" * 32, root, 1231006505, 0x1d00ffff, 2083236893); gen = hdr + vi(1) + tx
assert R.dsha(hdr)[::-1].hex() == "000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f", "genesis hash"
open(os.path.join(OUT, "mn_0.blk"), "wb").write(gen); manifest.append(f"mainnet 0 {R.dsha(hdr)[::-1].hex()} mn_0.blk")
# 3. Core's transaction vectors: record = kind(1) expect(1) len(2 LE) bytes ; expect: 0 = passes CheckTransaction, 1..8 = the nine CheckTransaction tests in Core's order
CATS = {6: 1, 7: 2, 8: 3, 9: 4, 10: 5, 11: 6, 12: 6, 13: 7, 14: 7}   # entry number in tx_invalid.json -> category (vout-empty, negative, toolarge, total, duplicate, cb-length x2, prevout-null x2)
recs = b""; n_valid = n_inv = 0
for e in json.load(open(os.path.join(SRC, "tx_valid.json"))):
    if len(e) == 1 and isinstance(e[0], str): continue
    raw = bytes.fromhex(e[1]); recs += bytes([0, 0]) + struct.pack("<H", len(raw)) + raw; n_valid += 1
k = 0
for e in json.load(open(os.path.join(SRC, "tx_invalid.json"))):
    if len(e) == 1 and isinstance(e[0], str): continue
    k += 1; raw = bytes.fromhex(e[1]); recs += bytes([1, CATS.get(k, 0)]) + struct.pack("<H", len(raw)) + raw; n_inv += 1
open(os.path.join(OUT, "txvec.bin"), "wb").write(recs); manifest.append(f"txvec {n_valid} valid {n_inv} invalid txvec.bin")
# 4. the synthetic chain
import random
Rn = random.Random(50); BITS = 0x1f00ffff; prev = b"\0" * 32; tm = 1700000000; chain = []
def rand_tx(n_in, n_out, wit=False):
    ins = [(Rn.randbytes(32), Rn.randrange(0, 4), push(Rn.randbytes(71)) + push(b"\x02" + Rn.randbytes(32)), 0xfffffffe) for _ in range(n_in)]
    outs = [(Rn.randrange(1000, 5_000_000), b"\x76\xa9\x14" + Rn.randbytes(20) + b"\x88\xac") for _ in range(n_out)]
    return ser_tx(2, ins, outs, 0, [[Rn.randbytes(72), b"\x02" + Rn.randbytes(32)] for _ in ins] if wit else None)
def height_script(h, extra): return push(h.to_bytes((h.bit_length() + 8) // 8, "little")) + push(extra)
for height in range(1, 13):
    extra = b"ch50 synthetic block %d" % height; seg = height == 6
    cb_in = [(b"\0" * 32, 0xffffffff, height_script(height, extra), 0xffffffff)]; other = [rand_tx(Rn.randrange(1, 3), Rn.randrange(1, 3), seg) for _ in range(Rn.randrange(0, 4) if not seg else 2)]
    if seg:
        wroot, _ = R.merkle([b"\0" * 32] + [R.parse_tx(t, 0)[0]["wtxid"] for t in other]); reserved = b"\0" * 32; commit = R.dsha(wroot + reserved)
        cb = ser_tx(2, cb_in, [(2500000000, b"\x51"), (0, b"\x6a\x24\xaa\x21\xa9\xed" + commit)], 0, [[reserved]])
    else: cb = ser_tx(2, cb_in, [(2500000000, b"\x51")])
    txs = [cb] + other; root, _ = R.merkle([R.parse_tx(t, 0)[0]["txid"] for t in txs]); blk, h = mine(0x20000000, prev, root, tm + 600 * height, BITS, txs)
    open(os.path.join(OUT, f"syn_{height}.blk"), "wb").write(blk); manifest.append(f"synthetic {height} {h[::-1].hex()} syn_{height}.blk"); prev = h
# the duplicated-transaction block: [cb, a, b, b] has the same Merkle root as [cb, a, b]
cb_in = [(b"\0" * 32, 0xffffffff, height_script(13, b"ch50 mutated merkle"), 0xffffffff)]; cb = ser_tx(2, cb_in, [(2500000000, b"\x51")]); a = rand_tx(1, 1); b = rand_tx(1, 2)
ids = [R.parse_tx(t, 0)[0]["txid"] for t in (cb, a, b)]; root3, _ = R.merkle(ids); root4, mut = R.merkle(ids + [ids[-1]]); assert root3 == root4 and mut
blk, h = mine(0x20000000, prev, root3, tm + 600 * 13, BITS, [cb, a, b, b]); open(os.path.join(OUT, "syn_dup.blk"), "wb").write(blk); manifest.append(f"synthetic-dup 13 {h[::-1].hex()} syn_dup.blk")
open(os.path.join(OUT, "MANIFEST.txt"), "w").write("\n".join(manifest) + "\n"); print("\n".join(manifest))
