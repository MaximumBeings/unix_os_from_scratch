#!/usr/bin/env python3
"""READ THIS FIRST: this script deliberately BREAKS the chapter's sources, one line at a time, in a temporary copy, and runs the host-side test suite against each broken copy. It expects the suite to FAIL every time. A "caught" line is the EXPECTED, wanted
result: it shows the tests can detect that mistake. "NOT CAUGHT" would be a gap. The real sources are never modified. For each mutant it runs: book_test (worked by hand), the differential test against the independent Python engine (40 random order flows, each with
damaged feeds) and the fuzzer (invariants after every call). Four broken copies are tested at a time. Output: mutation_out.txt   Usage: mutation.py"""
import concurrent.futures, os, shutil, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__)); code = os.path.join(here, "..")
MUT = [
 ("051_book.c", "best price: the best BID is the lowest price", "int better = side == OB_BUY ? (o->price > c->price) : (o->price < c->price);", "int better = side == OB_BUY ? (o->price < c->price) : (o->price < c->price);"),
 ("051_book.c", "best price: the best ASK is the highest price", "int better = side == OB_BUY ? (o->price > c->price) : (o->price < c->price);", "int better = side == OB_BUY ? (o->price > c->price) : (o->price > c->price);"),
 ("051_book.c", "time priority: among equal prices the LATEST order is first", "if (better || (o->price == c->price && o->seq < c->seq)) { k = (int)i; }", "if (better || (o->price == c->price && o->seq > c->seq)) { k = (int)i; }"),
 ("051_book.c", "crossing: a limit equal to the best opposite price does not trade", "return price == 0 || (side == OB_BUY ? b->o[k].price <= price : b->o[k].price >= price); }", "return price == 0 || (side == OB_BUY ? b->o[k].price < price : b->o[k].price > price); }"),
 ("051_book.c", "matching: stops at a resting price equal to the limit", "if (price != 0 && (side == OB_BUY ? m->price > price : m->price < price)) { break; }", "if (price != 0 && (side == OB_BUY ? m->price >= price : m->price <= price)) { break; }"),
 ("051_book.c", "matching: trades at the taker's limit price instead of the resting price", "t->price = m->price; t->qty = q; t->match = mn;", "t->price = price ? price : m->price; t->qty = q; t->match = mn;"),
 ("051_book.c", "matching: the quantity traded is the larger of the two", "uint32_t q = left < m->qty ? left : m->qty;", "uint32_t q = left < m->qty ? m->qty : left;"),
 ("051_book.c", "a market order's remainder rests in the book", "if (left > 0 && price != 0) {", "if (left > 0) {"),
 ("051_book.c", "replace keeps its place in the queue", "b->o[k].id = new_id; b->o[k].price = price; b->o[k].qty = qty; b->o[k].seq = b->next_seq++;", "b->o[k].id = new_id; b->o[k].price = price; b->o[k].qty = qty;"),
 ("051_book.c", "a replace that crosses the book is never matched", "if (crosses(b, side, price)) {\n        b->o[k].live = 0;", "if (0) {\n        b->o[k].live = 0;"),
 ("051_book.c", "reduce by exactly the remaining quantity is accepted", "if (cq == 0 || cq >= b->o[k].qty) { return OB_ERR_REDUCE; }", "if (cq == 0 || cq > b->o[k].qty) { return OB_ERR_REDUCE; }"),
 ("051_book.c", "a time stamp equal to the last one is refused", "if (ts > OB_MAX_TS || ts < b->last_ts) { return OB_ERR_TIME; }", "if (ts > OB_MAX_TS || ts <= b->last_ts) { return OB_ERR_TIME; }"),
 ("051_book.c", "a time stamp beyond 48 bits is accepted", "if (ts > OB_MAX_TS || ts < b->last_ts) { return OB_ERR_TIME; }", "if (ts < b->last_ts) { return OB_ERR_TIME; }"),
 ("051_book.c", "order id 0 is accepted", "if (id == 0 || find(b, id) >= 0) { return OB_ERR_DUP_ID; }", "if (find(b, id) >= 0) { return OB_ERR_DUP_ID; }"),
 ("051_book.c", "a quantity of 1,000,001 is accepted for a new order", "if (qty == 0 || qty > OB_MAX_QTY) { return OB_ERR_QTY; }\n    if (id == 0", "if (qty == 0 || qty > OB_MAX_QTY + 1) { return OB_ERR_QTY; }\n    if (id == 0"),
 ("051_book.c", "full book: a crossing order whose remainder cannot rest is not refused up front", "if (avail < qty) { return OB_ERR_FULL; }", ""),
 ("051_book.c", "a replace to price 0 is accepted", "if (qty == 0 || qty > OB_MAX_QTY) { return OB_ERR_QTY; } if (price == 0) { return OB_ERR_PRICE; }", "if (qty == 0 || qty > OB_MAX_QTY) { return OB_ERR_QTY; }"),
 ("051_book.c", "statistics: the trade count is not kept", "left -= q; b->n_trades++;", "left -= q;"),
 ("051_book.c", "statistics: the notional uses the taker's quantity twice", "b->notional += (uint64_t)m->price * q;\n        if (m->qty == 0)", "b->notional += (uint64_t)m->price * left;\n        if (m->qty == 0)"),
 ("051_book.c", "hash: bids sorted lowest price first", "return a->side == OB_BUY ? a->price > c->price : a->price < c->price; } return a->seq < c->seq; }", "return a->side == OB_BUY ? a->price < c->price : a->price < c->price; } return a->seq < c->seq; }"),
 ("051_book.c", "hash: the quantity is left out", "p4(buf + o + 12, x->qty);", "p4(buf + o + 12, 0);"),
 ("051_book.c", "hash: the side is left out", "buf[o + 16] = x->side; o += 17;", "buf[o + 16] = 0; o += 17;"),
 ("051_book.c", "price levels are not aggregated", "if (lv > 0 && price[lv - 1] == x->price) { qty[lv - 1] += x->qty; } else if (lv < max)", "if (lv < max)"),
 ("051_book.c", "feed: the Execute message is 30 bytes long", "uint8_t *p = emit(b, 'E', 31, ts);", "uint8_t *p = emit(b, 'E', 30, ts);"),
 ("051_book.c", "feed: the time stamp is written in 5 bytes' worth (the top byte lost)", "p6(p + 7, ts); return p + 13;\n}\nstatic int pub_add", "p6(p + 7, ts & 0xFFFFFFFFFFull); return p + 13;\n}\nstatic int pub_add"),
 ("051_book.c", "feed: the side letter of a sell is B", "p[8] = side == OB_BUY ? 'B' : 'S';", "p[8] = 'B';"),
 ("051_book.c", "subscriber: a message longer than what is left of the feed is accepted by two bytes", "if (ml > len - pos - 2) { FEED_FAIL(\"message runs past the end of the feed\"); }", "if (ml > len - pos) { FEED_FAIL(\"message runs past the end of the feed\"); }"),
 ("051_book.c", "subscriber: an Execute larger than the remaining quantity is accepted", "if (q == 0 || q > b->o[k].qty) { FEED_FAIL(\"execution larger than the remaining quantity\"); }", "if (q == 0) { FEED_FAIL(\"execution larger than the remaining quantity\"); }"),
 ("051_book.c", "subscriber: match numbers are not checked", "if (g8(m + 23) != b->next_match) { FEED_FAIL(\"match number out of sequence\"); }", ""),
 ("051_book.c", "subscriber: time may go backwards", "if (ts < b->last_ts) { FEED_FAIL(\"time goes backwards\"); }", ""),
 ("051_book.c", "subscriber: a crossed book is accepted", "if (ob_check(b)) { FEED_FAIL(\"the book is crossed or inconsistent after this message\"); }", ""),
 ("051_book.c", "subscriber: Replace keeps the queue position", "b->o[k].price = pr; b->o[k].seq = b->next_seq++;", "b->o[k].price = pr;"),
 ("051_book.c", "subscriber: an Add of an id that is already live is accepted", "if (id == 0 || find(b, id) >= 0) { FEED_FAIL(\"add of an order id that is already live\"); }", "if (id == 0) { FEED_FAIL(\"add of an order id that is already live\"); }"),
 ("051_book.c", "subscriber: an unknown message type is skipped as if harmless", "if (want == 0) { FEED_FAIL(\"unknown message type\"); }", "if (want == 0) { pos += 2 + ml; continue; }"),
 ("051_book.c", "invariant: a bid EQUAL to the best ask is not crossed", "if (bb >= 0 && ba >= 0 && b->o[bb].price >= b->o[ba].price) { return 4; }", "if (bb >= 0 && ba >= 0 && b->o[bb].price > b->o[ba].price) { return 4; }"),
 ("book_ref.py", "reference: ties go to the NEWEST order (the oracle itself broken)", "s.lv[side].setdefault(price, deque()).append(id)", "s.lv[side].setdefault(price, deque()).appendleft(id)"),
 ("book_ref.py", "reference: notional uses the wrong price (the oracle itself broken)", "s.notional += bp * q", "s.notional += (price or bp) * q"),
]
def run(tmp, cmd, **kw): return subprocess.run(cmd, cwd=tmp, capture_output=True, text=True, errors="replace", **kw)
SRC = ["../051_book.c", "../051_sha256.c"]; G = ["gcc", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined"]
def suite(tmp):
    failed = []; n = os.path.join(tmp, "native")
    b = run(n, G + ["book_test.c"] + SRC + ["-o", "bt"])
    if b.returncode != 0: failed.append("book_test (did not build)")
    elif run(n, ["./bt"]).returncode != 0: failed.append("book_test")
    b = run(n, G + ["book_cli.c"] + SRC + ["-o", "cli"])
    if b.returncode != 0: failed.append("the engine (did not build)"); return failed
    if run(n, [sys.executable, "diff_book.py", "40", "./cli"], env=dict(os.environ, STOP_AT_FIRST="1")).returncode != 0: failed.append("random flows vs the Python engine")
    b = run(n, G + ["book_fuzz.c"] + SRC + ["-o", "fz"])
    if b.returncode != 0: failed.append("fuzz (did not build)")
    elif run(n, ["./fz", "4"]).returncode != 0: failed.append("fuzz")
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
