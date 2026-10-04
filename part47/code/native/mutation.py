#!/usr/bin/env python3
"""READ THIS FIRST: this script deliberately BREAKS the chapter's C sources, one line at a time, in a temporary copy, and runs the host-side test suite against each broken copy. It expects the
suite to FAIL every time. A "caught" line is the EXPECTED, wanted result: it shows the tests can detect that mistake. "NOT CAUGHT" would be a gap. The real sources are never modified.
For each mutant it runs: the auction differential test (C engine vs the independent Python reference, 3000 auctions), market_test (invariants, replay, torn writes, idempotency, fee split, hash
sensitivity), ledger_test (conservation, overdraft, fee table vs decimal arithmetic) and ebay_test (round trips, parsers, builders). Output: mutation_out.txt   Usage: mutation.py   (about 6 minutes)"""
import os, shutil, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__)); code = os.path.join(here, "..")
MUT = [
 ("auction", "tie-break reversed: the LATER bidder wins an equal maximum", "return x->seq < y->seq;", "return x->seq > y->seq;"),
 ("auction", "increment table wrong at $25.00-$99.99 ($0.50 instead of $1.00)", "{2500u, 100u},", "{2500u, 50u},"),
 ("auction", "the price is no longer capped at the leader's own maximum", "if (want > l->high_max) {\n            want = l->high_max;", "if (0) {\n            want = l->high_max;"),
 ("auction", "meeting the reserve no longer lifts the price to the reserve", "if (l->reserve_cents != 0 && l->high_max >= l->reserve_cents && price < l->reserve_cents) {", "if (0) {"),
 ("auction", "minimum bid forgets the increment", "return l->price_cents + auc_increment(l->price_cents);", "return l->price_cents;"),
 ("auction", "a bidder may lower or repeat their own maximum", "if (max_cents <= own) {", "if (max_cents < own) {"),
 ("auction", "the seller may bid on their own item", "if (bidder == l->seller) {", "if (0) {"),
 ("auction", "a bid exactly at the end time is accepted (off by one)", "if (l->status != AUC_ACTIVE || now >= l->end_time) {", "if (l->status != AUC_ACTIVE || now > l->end_time) {"),
 ("auction", "anti-sniping window off by one", "now + l->extend_window >= l->end_time", "now + l->extend_window > l->end_time"),
 ("auction", "an auction can be closed before its end time", "if (now < l->end_time) {\n        return AUC_ERR_NOT_ENDED;", "if (0) {\n        return AUC_ERR_NOT_ENDED;"),
 ("auction", "reserve counts as met only when STRICTLY exceeded", "l->high_max >= l->reserve_cents);\n}\n\nuint32_t auc_proxy_of", "l->high_max > l->reserve_cents);\n}\n\nuint32_t auc_proxy_of"),
 ("auction", "runner-up chosen as the WEAKEST bidder instead of the strongest of the rest", "if (!have_second || ahead(&l->proxies[i], &l->proxies[second])) {", "if (!have_second || ahead(&l->proxies[second], &l->proxies[i])) {"),
 ("ledger", "an account may be overdrawn", "if (from != LED_EXTERNAL && l->bal[from] < (int32_t)cents) {", "if (0) {"),
 ("ledger", "the fee is not rounded half up", "(total_cents * 10u + 50u) / 100u + 30u", "(total_cents * 10u) / 100u + 30u"),
 ("ledger", "a transfer debits the sender but never credits the receiver (money destroyed)", "l->bal[to] += (int32_t)cents;", "(void)to;"),
 ("market", "the idempotency fingerprint ignores the bid amount", "words[1 + i] = c->a[i];", "words[1 + i] = (i == 2) ? 0u : c->a[i];"),
 ("market", "retries are not recognised (the idempotency lookup never matches)", "if (m->idem[i].used && m->idem[i].key == c->idem) {", "if (0 && m->idem[i].used && m->idem[i].key == c->idem) {"),
 ("market", "checkout takes no money from the buyer", "int rc = led_transfer(&m->led, c->a[1], LED_ESCROW, total);", "int rc = 0;"),
 ("market", "release pays the seller the full total and the platform takes no fee, escrow left over", "uint32_t fee = led_fee(o->total_cents);", "uint32_t fee = 0;"),
 ("market", "the state hash leaves out the ledger balances", "h32(&s, (uint32_t)m->led.bal[i]);", "(void)i;"),
 ("market", "the state hash leaves out the contents of the orders", "        if (o->used) {\n            h32(&s, o->id);", "        if (0) {\n            h32(&s, o->id);"),
 ("market", "the state hash leaves out the contents of the idempotency table", "        if (e->used) {\n            h32(&s, e->key);", "        if (0) {\n            h32(&s, e->key);"),
 ("wal", "the CRC is not checked when a record is read", "if (get32(buf + 12 + WAL_PAYLOAD) != wal_crc32(buf, 12 + WAL_PAYLOAD)) {", "if (0) {"),
 ("wal", "the sequence number is not checked", "if (get32(buf + 4) != expect_seq) {", "if (0) {"),
 ("wal", "wrong CRC polynomial", "0xEDB88320u", "0xEDB88321u"),
 ("ebay", "amounts with three decimals are accepted", "if (fd == 0 || fd > 2 || i != n) {", "if (fd == 0 || fd > 3 || i != n) {"),
 ("ebay", "the amount limit ($1,000,000.00) is not enforced", "if (whole > 1000000u || (whole == 1000000u && frac != 0)) {", "if (0) {"),
 ("ebay", "the Authorization header is parsed with the wrong prefix length", "uint32_t from = ls + 32;", "uint32_t from = ls + 31;"),
]
FILES = {"auction": "047_auction.c", "ledger": "047_ledger.c", "market": "047_market.c", "wal": "047_wal.c", "ebay": "047_ebay.c"}
def run(tmp, cmd, **kw): return subprocess.run(cmd, cwd=tmp, capture_output=True, text=True, **kw)
def suite(tmp):
    """Returns the list of tests that failed in this copy."""
    failed = []; inc = ["-I.."]
    n = os.path.join(tmp, "native")
    b = run(n, ["gcc", "-O1", "-I..", "auction_fuzz.c", "../047_auction.c", "-o", "fuzz"])
    if b.returncode == 0:
        out = run(n, ["./fuzz", "3000"]).stdout; ref = run(n, [sys.executable, "auction_ref.py", "3000"]).stdout
        if out != ref: failed.append("auction differential test")
    else: failed.append("auction differential test (did not build)")
    srcs = ["../047_market.c", "../047_auction.c", "../047_ledger.c", "../047_wal.c", "../047_sha256.c"]
    b = run(n, ["gcc", "-O1", "-I..", "market_test.c", *srcs, "-o", "mt"])
    if b.returncode == 0:
        r = run(n, ["./mt", "60"])
        if r.returncode != 0: failed.append("market_test")
    else: failed.append("market_test (did not build)")
    b = run(n, ["gcc", "-O1", "-I..", "ledger_test.c", "../047_ledger.c", "-o", "lt"])
    if b.returncode == 0:
        r = run(n, ["./lt"])
        if r.returncode != 0 or run(n, [sys.executable, "ledger_ref.py", "/dev/stdin"], input=r.stdout).returncode != 0: failed.append("ledger_test")
    else: failed.append("ledger_test (did not build)")
    b = run(n, ["gcc", "-O1", "-I..", "ebay_test.c", "../047_ebay.c", *srcs, "-o", "et"])
    if b.returncode == 0:
        r = run(n, ["./et", "30000"])
        if r.returncode != 0: failed.append("ebay_test")
    else: failed.append("ebay_test (did not build)")
    return failed
print("NOTE: this script deliberately breaks copies of the chapter's sources. 'caught' lines are EXPECTED: they show the tests can detect the mistake."); sys.stdout.flush()
base = tempfile.mkdtemp(prefix="c47mut_"); shutil.copytree(code, os.path.join(base, "base"), ignore=shutil.ignore_patterns("build", "*.o", "__pycache__"))
f0 = suite(os.path.join(base, "base")); print("baseline (nothing broken):", "all tests pass (expected)" if not f0 else "UNEXPECTED FAILURES " + str(f0)); sys.stdout.flush()
caught = 0
for kind, label, old, new in MUT:
    tmp = os.path.join(base, "m"); shutil.rmtree(tmp, ignore_errors=True); shutil.copytree(os.path.join(base, "base"), tmp)
    p = os.path.join(tmp, FILES[kind]); s = open(p).read(); assert s.count(old) == 1, (label, s.count(old)); open(p, "w").write(s.replace(old, new))
    failed = suite(tmp)
    if failed: caught += 1; print(f"{label}: caught by {', '.join(failed)}")
    else: print(f"{label}: NOT CAUGHT")
    sys.stdout.flush()
print(f"\n{caught} of {len(MUT)} broken versions caught"); shutil.rmtree(base, ignore_errors=True)
