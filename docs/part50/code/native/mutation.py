#!/usr/bin/env python3
"""READ THIS FIRST: this script deliberately BREAKS the chapter's sources, one line at a time, in a temporary copy, and runs the host-side test suite against each broken copy. It expects the suite to FAIL every time. A "caught"
line is the EXPECTED, wanted result: it shows the tests can detect that mistake. "NOT CAUGHT" would be a gap. The real sources are never modified.
For each mutant it runs: btc_test (hashing, the compact target, real blocks vs published hashes, Core's vectors, every rule, limits, damage), the differential test against the independent Python reference (100 random blocks + the real
block files + the transaction vectors), and the fuzzer's property check (a changed block is never valid). Four broken copies are tested at a time. Output: mutation_out.txt   Usage: mutation.py"""
import concurrent.futures, os, shutil, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__)); code = os.path.join(here, "..")
MUT = [
 ("050_btc.c", "varint: a 0xfd-prefixed number is read from too few bytes", "if (n == 0xfd) { if (p + 3 > len) { return -1; }", "if (n == 0xfd) { if (p + 2 > len) { return -1; }"),
 ("050_btc.c", "compact: the 'negative' sign bit is ignored", "*neg = (word != 0) && (bits & 0x00800000u) != 0;", "*neg = 0;"),
 ("050_btc.c", "compact: the overflow exponent limit is 35 instead of 34", "(size > 34 ||", "(size > 35 ||"),
 ("050_btc.c", "compact: a mantissa above 0xff is allowed one exponent too high", "(word > 0xff && size > 33)", "(word > 0xff && size > 34)"),
 ("050_btc.c", "compact: a mantissa above 0xffff is allowed one exponent too high", "(word > 0xffff && size > 32)", "(word > 0xffff && size > 33)"),
 ("050_btc.c", "compact: a zero target is accepted", "if (zero) { *why = \"zero\"; return 0; }", ""),
 ("050_btc.c", "compact: a target EQUAL to the limit is refused", "if (cmp_be(target, lim) > 0) {", "if (cmp_be(target, lim) >= 0) {"),
 ("050_btc.c", "proof of work: the comparison is reversed (a hash ABOVE the target passes)", "if (h != t) { ok = h < t; break; }", "if (h != t) { ok = h > t; break; }"),
 ("050_btc.c", "proof of work: leading zero bits counted four to a zero byte", "B->zero_bits += 8; continue;", "B->zero_bits += 4; continue;"),
 ("050_btc.c", "Merkle: an odd level is not padded with its last hash", "if (n & 1u) { cpy(lv[n], lv[n - 1], 32); n++; }", ""),
 ("050_btc.c", "Merkle: side-by-side equal hashes are no longer flagged as mutation", "if (same(lv[i], lv[i + 1], 32)) { *mutated = 1; }", ""),
 ("050_btc.c", "Merkle: a pair is hashed once, not twice", "btc_dsha(pair, 64, lv[i / 2]);", "sha256_hash(pair, 64, lv[i / 2]);"),
 ("050_btc.c", "transactions: the SegWit marker is looked for as 00 00", "if (b[p] == 0 && p + 1 < len && b[p + 1] != 0) {", "if (b[p] == 0 && p + 1 < len && b[p + 1] == 0) {"),
 ("050_btc.c", "transactions: any flag byte is accepted, not only 01", "if (b[p + 1] != 1) { return BTC_ERR_FLAG; }", ""),
 ("050_btc.c", "transactions: a witness flag with every witness empty is accepted", "if (!any_witness) { return BTC_ERR_SUPERFLUOUS_WITNESS; }", ""),
 ("050_btc.c", "txid: the stripped serialisation omits the lock time", "sha256_update(&c, b + lt_off, 4); sha256_final(&c, d1);", "sha256_final(&c, d1);"),
 ("050_btc.c", "weight: the stripped size is counted twice, not three times", "t->weight = 3 * t->stripped_size + t->size;", "t->weight = 2 * t->stripped_size + t->size;"),
 ("050_btc.c", "coinbase: the null-index test is dropped (any single input with a null hash is a coinbase)", "t->is_coinbase = (t->n_in == 1 && same(b + g_in_off[0], zero32, 32) && rd32(b + g_in_off[0] + 32) == 0xffffffffu);", "t->is_coinbase = (t->n_in == 1 && same(b + g_in_off[0], zero32, 32));"),
 ("050_btc.c", "checks: an oversized transaction is accepted", "else if (t->weight > BTC_MAX_WEIGHT) { t->check = \"bad-txns-oversize\"; }", ""),
 ("050_btc.c", "checks: an output of exactly 21,000,000 coins is refused", "else if (val > BTC_MAX_MONEY) {", "else if (val >= BTC_MAX_MONEY) {"),
 ("050_btc.c", "checks: outputs summing to exactly 21,000,000 coins are refused", "if (total > BTC_MAX_MONEY) { bad = \"bad-txns-txouttotal-toolarge\"; }", "if (total >= BTC_MAX_MONEY) { bad = \"bad-txns-txouttotal-toolarge\"; }"),
 ("050_btc.c", "checks: duplicate inputs that are next to each other are missed", "for (uint32_t j = i + 1; j < t->n_in; j++)", "for (uint32_t j = i + 2; j < t->n_in; j++)"),
 ("050_btc.c", "checks: a coinbase script of 1 byte is accepted", "if (t->cb_script_len < 2 || t->cb_script_len > 100)", "if (t->cb_script_len < 1 || t->cb_script_len > 100)"),
 ("050_btc.c", "checks: a coinbase script of 101 bytes is accepted", "if (t->cb_script_len < 2 || t->cb_script_len > 100)", "if (t->cb_script_len < 2 || t->cb_script_len > 101)"),
 ("050_btc.c", "checks: a null input in an ordinary transaction is accepted", "if (same(b + g_in_off[i], zero32, 32) && rd32(b + g_in_off[i] + 32) == 0xffffffffu) { t->check = \"bad-txns-prevout-null\"; break; }", ""),
 ("050_btc.c", "witness: the commitment marker's last byte is wrong (aa21a9ee instead of aa21a9ed)", "b[q + 5] == 0xed", "b[q + 5] == 0xee"),
 ("050_btc.c", "witness: the coinbase's own wtxid is not replaced by zeros in the witness Merkle tree", "for (int i = 0; i < 32; i++) { wids[0][i] = 0; }", ""),
 ("050_btc.c", "BIP 34: the sign bit of the height is read from the wrong bit", "if (s[k] & 0x80) {", "if (s[k] & 0x40) {"),
 ("050_btc.c", "BIP 34: applied to version 1 blocks too", "if (B->first_is_cb && B->version >= 2) { B->height_ok", "if (B->first_is_cb && B->version >= 1) { B->height_ok"),
 ("050_btc.c", "block: 513 transactions are accepted (writes past the table)", "if (n > BTC_MAX_TX) { return BTC_ERR_TOO_MANY_TX; }", "if (n > BTC_MAX_TX + 1) { return BTC_ERR_TOO_MANY_TX; }"),
 ("050_btc.c", "block: bytes after the last transaction are accepted", "if (p != len) { return BTC_ERR_TRAILING; }", ""),
 ("050_btc.c", "block: a block with zero transactions is parsed (reads an uninitialised coinbase)", "B->n_tx = (uint32_t)n; if (n == 0) { return BTC_ERR_NO_TX; }", "B->n_tx = (uint32_t)n;"),
 ("050_btc.c", "transactions: 4097 inputs accepted (writes past the table)", "if (v > BTC_MAX_IN) { return BTC_ERR_TOO_MANY_IN; }", "if (v > BTC_MAX_IN + 1) { return BTC_ERR_TOO_MANY_IN; }"),
 ("050_btc.c", "verdict: a block with a duplicated transaction and a correct Merkle root is accepted", "else if (B->merkle_mutated) { B->reason = \"bad-txns-duplicate\"; }", ""),
 ("050_btc.c", "verdict: a block weight over 4,000,000 is accepted", "else if (B->weight > BTC_MAX_WEIGHT) { B->reason = \"bad-blk-length\"; }", ""),
 ("050_btc.c", "verdict: a second coinbase is accepted", "else if (B->other_cb) { B->reason = \"bad-cb-multiple\"; }", ""),
 ("050_btc.c", "verdict: a block whose first transaction is not a coinbase is accepted", "else if (!B->first_is_cb) { B->reason = \"bad-cb-missing\"; }", ""),
 ("050_btc.c", "verdict: a failing witness commitment is not an error", "if (!B->reason && B->witness == 2) { B->reason = \"bad-witness-merkle-match\"; }", ""),
 ("050_btc.c", "verdict: transaction checks are not applied to the block", "else { for (uint32_t i = 0; i < B->n_tx; i++) { if (B->tx[i].check) { B->reason = B->tx[i].check; break; } } }", ""),
 ("btc_ref.py", "reference: a duplicate hash pair is not flagged as mutated (the oracle itself broken)", "if hs[i] == hs[i + 1]: mutated = True", "pass"),
 ("btc_ref.py", "reference: an output of exactly 21,000,000 coins is refused (the oracle itself broken)", "if v > MAX_MONEY: return \"bad-txns-vout-toolarge\"", "if v >= MAX_MONEY: return \"bad-txns-vout-toolarge\""),
]
def run(tmp, cmd, **kw): return subprocess.run(cmd, cwd=tmp, capture_output=True, text=True, errors="replace", **kw)
SRC = ["../050_btc.c", "../050_sha256.c"]; G = ["gcc", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined"]
def suite(tmp):
    failed = []; n = os.path.join(tmp, "native")
    b = run(n, G + ["btc_test.c"] + SRC + ["-o", "bt"])
    if b.returncode != 0: failed.append("btc_test (did not build)")
    elif run(n, ["./bt"]).returncode != 0: failed.append("btc_test")
    b = run(n, G + ["btc_cli.c"] + SRC + ["-o", "cli"])
    if b.returncode != 0: failed.append("the engine (did not build)"); return failed
    if run(n, [sys.executable, "diff_btc.py", "100", "./cli"], env=dict(os.environ, STOP_AT_FIRST="1")).returncode != 0: failed.append("random blocks, block files and Core vectors vs the Python reference")
    b = run(n, G + ["btc_fuzz.c"] + SRC + ["-o", "fz"])
    if b.returncode != 0: failed.append("fuzz property (did not build)")
    elif run(n, ["./fz", "300", "../data/btc/tn_49291.blk", "../data/btc/tn_180480.blk", "../data/btc/tn_926485.blk", "../data/btc/syn_6.blk", "../data/btc/syn_dup.blk"]).returncode != 0: failed.append("fuzz property")
    return failed
print("NOTE: this script deliberately breaks copies of the chapter's sources. 'caught' lines are EXPECTED: they show the tests can detect the mistake."); sys.stdout.flush()
base = tempfile.mkdtemp(prefix="c50mut_"); shutil.copytree(code, os.path.join(base, "base"), ignore=shutil.ignore_patterns("build", "*.o", "__pycache__"))
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
