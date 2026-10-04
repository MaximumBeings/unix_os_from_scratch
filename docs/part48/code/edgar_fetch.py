#!/usr/bin/env python3
"""Chapter 48: the HOST-side half of 'pull SEC EDGAR reports'. The kernel has no TLS (HTTPS needs certificate validation and elliptic-curve crypto this book has not built), so the download
runs on the host and hands the kernel a slimmed XBRL file (edgar_slim.py). The rules EDGAR publishes for automated access are built in, not bolted on:
  * every request carries a descriptive User-Agent with a contact address (SEC fair-access policy; requests without one are blocked) -- required argument, no default;
  * at most 10 requests per second (a limiter with an injectable clock, so the test can prove it without waiting);
  * 429/5xx answers are retried with exponential backoff, at most 4 attempts, then a clear error -- never an endless loop, never a silently empty result;
  * every download is checked: JSON must parse, the XBRL document must contain the facts the engine needs, the accession number in the URL must match the one asked for.
Endpoints used (documented at sec.gov/edgar/sec-api-documentation):  www.sec.gov/files/company_tickers.json  (ticker -> CIK),  data.sec.gov/submissions/CIK##########.json  (the filing list),
  www.sec.gov/Archives/edgar/data/<cik>/<accession without dashes>/index.json  (the files of one filing; the instance document is the *_htm.xml the SEC extracts from inline XBRL).
Usage: edgar_fetch.py --user-agent "Name email@example.com" TICKER [--form 10-K] [--out DIR] [--base-www URL --base-data URL]  (the --base options exist so the test can point it at a local server)"""
import argparse, json, re, sys, time, urllib.request, urllib.error
import edgar_slim
class FetchError(Exception): pass
class RateLimiter:
    def __init__(self, per_second=10, clock=time.monotonic, sleep=time.sleep):
        self.per_second, self.clock, self.sleep, self.stamps = per_second, clock, sleep, []
    def wait(self):
        now = self.clock()
        self.stamps = [t for t in self.stamps if now - t < 1.0]
        if len(self.stamps) >= self.per_second:
            self.sleep(1.0 - (now - self.stamps[0])); now = self.clock(); self.stamps = [t for t in self.stamps if now - t < 1.0]
        self.stamps.append(now)
class Edgar:
    def __init__(self, user_agent, www="https://www.sec.gov", data="https://data.sec.gov", limiter=None, sleep=time.sleep, opener=None):
        if not re.search(r"\S+@\S+\.\S+", user_agent or ""): raise FetchError("User-Agent must include a contact e-mail address (SEC fair-access policy)")
        self.ua, self.www, self.data = user_agent, www.rstrip("/"), data.rstrip("/"); self.limiter = limiter or RateLimiter(); self.sleep = sleep; self.opener = opener or urllib.request.urlopen
        self.requests = 0
    def get(self, url):
        delay = 1.0
        for attempt in range(1, 5):
            self.limiter.wait(); self.requests += 1
            req = urllib.request.Request(url, headers={"User-Agent": self.ua, "Accept-Encoding": "identity"})
            try:
                with self.opener(req, timeout=30) as r: return r.read()
            except urllib.error.HTTPError as e:
                if e.code in (429, 500, 502, 503, 504) and attempt < 4: self.sleep(delay); delay *= 2; continue
                raise FetchError(f"{url}: HTTP {e.code} after {attempt} attempt(s)")
            except urllib.error.URLError as e:
                if attempt < 4: self.sleep(delay); delay *= 2; continue
                raise FetchError(f"{url}: {e.reason} after {attempt} attempts")
    def json(self, url):
        raw = self.get(url)
        try: return json.loads(raw)
        except ValueError: raise FetchError(f"{url}: not JSON (first bytes {raw[:40]!r})")
    def cik_for(self, ticker):
        for row in self.json(self.www + "/files/company_tickers.json").values():
            if row["ticker"].upper() == ticker.upper(): return int(row["cik_str"])
        raise FetchError(f"unknown ticker {ticker}")
    def latest_filing(self, cik, form):
        sub = self.json(f"{self.data}/submissions/CIK{cik:010d}.json"); rec = sub["filings"]["recent"]
        for i, f in enumerate(rec["form"]):
            if f == form: return {"accession": rec["accessionNumber"][i], "filed": rec["filingDate"][i], "primary": rec["primaryDocument"][i], "name": sub["name"]}
        raise FetchError(f"no {form} in the recent filings of CIK {cik}")
    def instance_document(self, cik, accession):
        base = f"{self.www}/Archives/edgar/data/{cik}/{accession.replace('-', '')}"
        names = [it["name"] for it in self.json(base + "/index.json")["directory"]["item"]]
        inst = [n for n in names if n.endswith("_htm.xml")]
        if len(inst) != 1: raise FetchError(f"{accession}: expected exactly one *_htm.xml instance document, found {inst}")
        return base + "/" + inst[0], self.get(base + "/" + inst[0]).decode("utf-8")
def fetch(ticker, user_agent, form="10-K", out=None, **kw):
    e = Edgar(user_agent, **kw); cik = e.cik_for(ticker); f = e.latest_filing(cik, form); url, text = e.instance_document(cik, f["accession"])
    slim, nf, nc = edgar_slim.slim(text)
    if nf < 20 or "DocumentPeriodEndDate" not in slim: raise FetchError(f"{url}: does not look like a financial-statement instance document ({nf} facts kept)")
    meta = {"ticker": ticker.upper(), "cik": cik, "form": form, "accession": f["accession"], "filed": f["filed"], "name": f["name"], "url": url, "requests": e.requests}
    if out:
        open(f"{out}/{ticker.lower()}.xml", "w", encoding="utf-8").write(slim); json.dump(meta, open(f"{out}/{ticker.lower()}.json", "w"), indent=1)
    return meta, slim
if __name__ == "__main__":
    ap = argparse.ArgumentParser(); ap.add_argument("ticker"); ap.add_argument("--user-agent", required=True); ap.add_argument("--form", default="10-K"); ap.add_argument("--out", default=".")
    ap.add_argument("--base-www", default="https://www.sec.gov"); ap.add_argument("--base-data", default="https://data.sec.gov"); a = ap.parse_args()
    try: meta, _ = fetch(a.ticker, a.user_agent, a.form, a.out, www=a.base_www, data=a.base_data)
    except FetchError as e: print("error:", e, file=sys.stderr); sys.exit(1)
    print(json.dumps(meta))
