#!/usr/bin/env python3
"""Chapter 48: tests for edgar_fetch.py without touching sec.gov. A real HTTP server on 127.0.0.1 answers with the SAME SHAPES the SEC's documented JSON endpoints use (company_tickers.json,
submissions, index.json) and serves the REAL Apple 10-K XBRL document as the instance file. What this proves: the client's logic (headers, rate limit, retries, validation, slimming).
What it cannot prove: that sec.gov still answers in exactly these shapes today -- the sandbox that built this chapter cannot reach sec.gov (see the page, 'Provenance')."""
import http.server, json, os, sys, threading, tempfile, urllib.error, io
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import edgar_fetch as ef
FULL = os.environ.get("AAPL_FULL", "/tmp/et/tests/fixtures/xbrl/aapl/10k_2023/aapl-20230930_htm.xml")
ACC = "0000320193-23-000106"; NODASH = ACC.replace("-", ""); CIK = 320193
SEEN = []; FAIL_NEXT = {"n": 0, "code": 429}
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        SEEN.append((self.path, self.headers.get("User-Agent")))
        if FAIL_NEXT["n"] > 0: FAIL_NEXT["n"] -= 1; self.send_response(FAIL_NEXT["code"]); self.end_headers(); return
        p = self.path
        if p == "/files/company_tickers.json": body = json.dumps({"0": {"cik_str": CIK, "ticker": "AAPL", "title": "Apple Inc."}, "1": {"cik_str": 789019, "ticker": "MSFT", "title": "MICROSOFT CORP"}})
        elif p == f"/submissions/CIK{CIK:010d}.json": body = json.dumps({"name": "Apple Inc.", "filings": {"recent": {"form": ["8-K", "10-Q", "10-K"], "accessionNumber": ["0000320193-23-000110", "0000320193-23-000077", ACC], "filingDate": ["2023-11-02", "2023-08-04", "2023-11-03"], "primaryDocument": ["a.htm", "b.htm", "aapl-20230930.htm"]}}})
        elif p == f"/Archives/edgar/data/{CIK}/{NODASH}/index.json": body = json.dumps({"directory": {"item": [{"name": "aapl-20230930.htm"}, {"name": "aapl-20230930_htm.xml"}, {"name": "R1.htm"}]}})
        elif p == f"/Archives/edgar/data/{CIK}/{NODASH}/aapl-20230930_htm.xml": body = open(FULL, encoding="utf-8").read()
        else: self.send_response(404); self.end_headers(); return
        b = body.encode(); self.send_response(200); self.send_header("Content-Length", str(len(b))); self.end_headers(); self.wfile.write(b)
srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), H); threading.Thread(target=srv.serve_forever, daemon=True).start(); base = f"http://127.0.0.1:{srv.server_address[1]}"
UA = "Chapter48 Test tester@example.com"; ok = 0; bad = 0
def check(name, cond):
    global ok, bad
    print(("PASS " if cond else "FAIL ") + name); ok += cond; bad += (not cond)
slept = []
def mk(**kw): return ef.Edgar(UA, www=base, data=base, sleep=lambda s: slept.append(s), limiter=ef.RateLimiter(10, sleep=lambda s: None), **kw)
# 1. the whole pipeline against the local server
tmp = tempfile.mkdtemp(); SEEN.clear()
meta, slim = ef.fetch("aapl", UA, "10-K", tmp, www=base, data=base, sleep=lambda s: None)
check("pipeline: ticker -> CIK -> latest 10-K -> instance document -> slim file", meta["accession"] == ACC and meta["cik"] == CIK and meta["filed"] == "2023-11-03" and os.path.getsize(f"{tmp}/aapl.xml") > 10000)
check("every request carried the User-Agent with the contact address", len(SEEN) == 4 and all(ua == UA for _, ua in SEEN))
check("exactly 4 requests (tickers, submissions, index, document): no wasted calls", meta["requests"] == 4)
ref = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "aapl.xml"), encoding="utf-8").read()
check("the file the fetcher wrote is byte-identical to the slim file embedded in the kernel", slim == ref)
# 2. refusals
for bad_ua in ("", "Mozilla/5.0", "justaname"):
    try: ef.Edgar(bad_ua); check(f"User-Agent {bad_ua!r} refused", False)
    except ef.FetchError: check(f"User-Agent {bad_ua!r} refused (no contact address)", True)
try: ef.fetch("nosuch", UA, www=base, data=base, sleep=lambda s: None); check("unknown ticker -> error", False)
except ef.FetchError as e: check("unknown ticker -> clear error", "unknown ticker" in str(e))
try: ef.fetch("aapl", UA, "20-F", www=base, data=base, sleep=lambda s: None); check("no filing of that form -> error", False)
except ef.FetchError as e: check("no filing of that form -> clear error", "no 20-F" in str(e))
# 3. retries
e = mk(); FAIL_NEXT.update(n=2, code=429); slept.clear(); e.json(base + "/files/company_tickers.json")
check("two 429 answers then success: retried with backoff 1s, 2s", slept == [1.0, 2.0])
e = mk(); FAIL_NEXT.update(n=99, code=503); slept.clear()
try: e.json(base + "/files/company_tickers.json"); check("endless 503 -> error", False)
except ef.FetchError as x: check("endless 503 -> gives up after exactly 4 attempts (3 sleeps 1,2,4)", "4 attempt" in str(x) and slept == [1.0, 2.0, 4.0])
FAIL_NEXT["n"] = 0
e = mk(); slept.clear()
try: e.get(base + "/nothing"); check("404 -> error", False)
except ef.FetchError as x: check("404 is NOT retried (one attempt, no sleep)", "1 attempt" in str(x) and slept == [])
# 4. validation of what came back
class Fake:
    def __init__(self, bodies): self.bodies = bodies
    def __call__(self, req, timeout=0):
        for k, v in self.bodies.items():
            if req.full_url.endswith(k): return io.BytesIO(v if isinstance(v, bytes) else v.encode())
        raise urllib.error.HTTPError(req.full_url, 404, "x", {}, None)
e = ef.Edgar(UA, opener=Fake({"company_tickers.json": "<html>blocked</html>"}), limiter=ef.RateLimiter(10, sleep=lambda s: None))
try: e.cik_for("AAPL"); check("HTML instead of JSON -> error", False)
except ef.FetchError as x: check("an HTML error page where JSON was expected is reported, not parsed", "not JSON" in str(x))
two = json.dumps({"directory": {"item": [{"name": "a_htm.xml"}, {"name": "b_htm.xml"}]}})
e = ef.Edgar(UA, opener=Fake({"index.json": two}), limiter=ef.RateLimiter(10, sleep=lambda s: None))
try: e.instance_document(1, "0000000001-23-000001"); check("two instance documents -> error", False)
except ef.FetchError as x: check("two *_htm.xml files in one filing -> refuses to guess", "exactly one" in str(x))
tiny = json.dumps({"directory": {"item": [{"name": "x_htm.xml"}]}})
e = ef.Edgar(UA, opener=Fake({"company_tickers.json": json.dumps({"0": {"cik_str": 5, "ticker": "TINY", "title": "t"}}), "CIK0000000005.json": json.dumps({"name": "Tiny", "filings": {"recent": {"form": ["10-K"], "accessionNumber": ["0000000005-23-000001"], "filingDate": ["2023-01-01"], "primaryDocument": ["x.htm"]}}}),
                              "index.json": tiny, "x_htm.xml": "<xbrl><note>not a financial statement</note></xbrl>"}), limiter=ef.RateLimiter(10, sleep=lambda s: None))
try: ef.fetch("tiny", UA, "10-K", None, www="http://x", data="http://x", opener=e.opener) ; check("a document with no financial facts -> error", False)
except (ef.FetchError, TypeError) as x: check("an instance document with no financial-statement facts is refused (never slimmed into an empty file)", isinstance(x, ef.FetchError) and "does not look like" in str(x))
# 5. the rate limiter, on a fake clock: 35 requests in a burst must never put more than 10 inside any one second
t = [0.0]; rl = ef.RateLimiter(10, clock=lambda: t[0], sleep=lambda s: t.__setitem__(0, t[0] + s)); stamps = []
for _ in range(35): rl.wait(); stamps.append(t[0])
worst = max(sum(1 for s in stamps if a <= s < a + 1.0) for a in stamps)
check(f"rate limiter: 35 back-to-back requests, at most {worst} in any one second (limit 10), finished at t={t[0]:.2f}s", worst <= 10 and t[0] >= 3.0)
print(f"\n{ok} checks passed, {bad} failed"); srv.shutdown(); sys.exit(1 if bad else 0)
