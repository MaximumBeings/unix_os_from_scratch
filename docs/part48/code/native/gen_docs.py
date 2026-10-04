#!/usr/bin/env python3
"""Chapter 48: synthetic XBRL filings for differential testing. Each document is random but VALID XBRL: random company size, fiscal-year length (including 349- and 381-day years the rules must
NOT accept), balance sheets that balance (or deliberately do not), concept presence and absence, concept fallbacks, dimensional 'noise' facts that must be ignored (a segment value for Assets that
differs from the consolidated one), inconsistent duplicates, equal duplicates, filer-chosen unit ids, non-USD facts, fractional dollars, nil facts, a custom prefix for us-gaap, extension concepts
with the same local name, facts BEFORE the contexts they use, attributes split across lines, single quotes, comments. gen(seed) -> (xml text, price_cents)."""
import random, datetime
NS = 'xmlns="http://www.xbrl.org/2003/instance" xmlns:{g}="http://fasb.org/us-gaap/2023" xmlns:dei="http://xbrl.sec.gov/dei/2023" xmlns:iso4217="http://www.xbrl.org/2003/iso4217" xmlns:xbrldi="http://xbrl.org/2006/xbrldi" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:ext="http://example.com/ext"'
def gen(seed):
    R = random.Random(seed); g = R.choice(["us-gaap", "us-gaap", "us-gaap", "gaap", "ug"]); q = R.choice(['"', '"', '"', "'"])
    end = datetime.date(2000, 1, 1) + datetime.timedelta(days=R.randrange(0, 9000))
    dlen = R.choice([364, 364, 365, 365, 371, 350, 380, 349, 381, 366]); start = end - datetime.timedelta(days=dlen)
    pdays = R.choice([364, 365, 371, 350, 380, 349, 381, 365])
    prior = end - datetime.timedelta(days=pdays); prior2 = end - datetime.timedelta(days=R.choice([352, 360, 378])) if R.random() < 0.15 else None
    uid = R.choice(["usd", "U_USD", "u1", "iso4217_USD_x"]); eid = R.choice(["usdPerShare", "U_UnitedStatesOfAmericaDollarsShare", "e1"])
    used_ids = set()
    def idn(prefix):
        while True:
            v = prefix + "-" + "".join(R.choice("abcdef0123456789-_") for _ in range(R.randrange(3, 30)))
            if v not in used_ids: used_ids.add(v); return v
    ctxs = []   # (id, kind, a, b, dim)
    def newctx(kind, a, b, dim=False):
        i = idn("c"); ctxs.append((i, kind, a, b, dim)); return i
    cdei = newctx("instant", None, None) if False else None
    c_dur = newctx("dur", start, end); c_cur = newctx("inst", end, end); c_pri = newctx("inst", prior, prior)
    c_pri2 = newctx("inst", prior2, prior2) if prior2 else None
    c_dim_dur = newctx("dur", start, end, True); c_dim_cur = newctx("inst", end, end, True)
    if R.random() < 0.02: ctxs.append((ctxs[-1][0],) + ctxs[-1][1:])   # a deliberately duplicated context id: both implementations must refuse the document
    c_dei = newctx("dur", end - datetime.timedelta(days=R.randrange(1, 30)), end + datetime.timedelta(days=R.randrange(0, 60))) if False else c_dur
    if R.random() < 0.1: c_dup_cur = newctx("inst", end, end)  # a second identical dimension-free context
    else: c_dup_cur = None
    scale = 10 ** R.randrange(3, 12)
    def amt(lo=1, hi=1000): return R.randrange(lo, hi) * scale + (R.randrange(0, scale) if R.random() < 0.5 else 0)
    facts = []   # (prefix, concept, ctx, unit, text)
    def add(name, ctx, val, unit=None, pre=None, text=None):
        facts.append(((pre or g), name, ctx, unit if unit else uid, text if text is not None else str(val)))
    balanced = R.random() < 0.85
    A = amt(50, 5000); eq = R.randrange(-A // 3, A) if R.random() < 0.15 else amt(1, 50) % A or 1; L = A - eq
    if R.random() < 0.8:
        add("Assets", c_cur, A); add("LiabilitiesAndStockholdersEquity", c_cur, A if balanced else A + R.randrange(1, 1000))
    if R.random() < 0.9: add("Assets", c_pri, int(A * R.uniform(0.5, 1.5)))
    if c_pri2 and R.random() < 0.7: add("Assets", c_pri2, int(A * R.uniform(0.5, 1.5)))
    if R.random() < 0.7: add("Liabilities", c_cur, L)
    if R.random() < 0.9: add("StockholdersEquity", c_cur, eq)
    if R.random() < 0.9: add("StockholdersEquity", c_pri, int(eq * R.uniform(0.5, 1.5)))
    if R.random() < 0.3: add("StockholdersEquityIncludingPortionAttributableToNoncontrollingInterest", c_cur, eq + R.randrange(0, 1000))
    for n in ("AssetsCurrent", "LiabilitiesCurrent", "CashAndCashEquivalentsAtCarryingValue", "AccountsReceivableNetCurrent"):
        if R.random() < 0.75: add(n, c_cur, amt())
    for n in ("MarketableSecuritiesCurrent", "ShortTermInvestments"):
        if R.random() < 0.3: add(n, c_cur, amt())
    rev_names = ["RevenueFromContractWithCustomerExcludingAssessedTax", "Revenues", "SalesRevenueNet"]
    for n in R.sample(rev_names, R.choice([0, 1, 1, 1, 2, 3])): add(n, c_dur, amt(10, 5000))
    revv = None
    for f in facts:
        if f[1] in rev_names and f[2] == c_dur: revv = int(f[4]); break
    cg_names = ["CostOfRevenue", "CostOfGoodsAndServicesSold"]
    cgv = amt(1, 2000) if R.random() < 0.7 else None
    if cgv is not None: add(R.choice(cg_names), c_dur, cgv)
    if R.random() < 0.5 and revv is not None and cgv is not None: add("GrossProfit", c_dur, (revv - cgv) if R.random() < 0.85 else revv - cgv + 7)
    elif R.random() < 0.3: add("GrossProfit", c_dur, amt())
    for n in ("OperatingIncomeLoss", "NetIncomeLoss", "NetCashProvidedByUsedInOperatingActivities"):
        if R.random() < 0.8: add(n, c_dur, amt(1, 800) * (-1 if R.random() < 0.15 else 1))
    for n in R.sample(["InterestExpense", "InterestExpenseNonoperating"], R.choice([0, 1, 1, 2])): add(n, c_dur, amt(0, 100) if R.random() < 0.9 else 0)
    for n in R.sample(["PaymentsToAcquirePropertyPlantAndEquipment", "PaymentsToAcquireProductiveAssets"], R.choice([0, 1, 1, 2])): add(n, c_dur, amt(1, 900))
    if R.random() < 0.7:
        e = R.randrange(-50000, 900000); add("EarningsPerShareDiluted", c_dur, e, eid, text=("-" if e < 0 else "") + f"{abs(e)//10000}.{abs(e)%10000:04d}"[: R.choice([None, None]) or None])
    # noise that MUST be ignored or must behave as the rules say
    if R.random() < 0.8: add("Assets", c_dim_cur, amt()); add("Revenues", c_dim_dur, amt()); add("NetIncomeLoss", c_dim_dur, amt())
    if R.random() < 0.3: add("Assets", c_cur, amt(), pre="ext")
    if R.random() < 0.25: add("Revenues", c_dur, 5, text="1.5")                       # fractional dollars: dropped
    if R.random() < 0.2: add("NetIncomeLoss", c_dur, 1, unit="eur")                    # non-USD: dropped
    if R.random() < 0.15: add("NetIncomeLoss", c_dur, 1, unit="nosuchunit")            # unit never defined: dropped
    if R.random() < 0.15: add("OperatingIncomeLoss", c_dur, 1, text="n/a")             # not a number: dropped
    if R.random() < 0.15: facts.append((g, "GrossProfit", c_dur, uid, ""))
    if R.random() < 0.1 and facts: f = R.choice(facts); facts.append(f)                  # equal duplicate: harmless
    if R.random() < 0.08 and facts: f = R.choice([x for x in facts if x[3] == uid and x[4].lstrip("-").isdigit()] or facts); facts.append((f[0], f[1], f[2], f[3], str(int(f[4]) + 1) if f[4].lstrip("-").isdigit() else f[4]))  # conflicting duplicate
    if c_dup_cur and R.random() < 0.7 and facts: f = R.choice(facts); facts.append((f[0], f[1], c_dup_cur, f[3], f[4]))
    nil = R.random() < 0.15
    def fact_xml(f):
        pre, name, ctx, unit, text = f; sp = R.choice([" ", " ", "\n      ", "  "])
        a = [f"contextRef={q}{ctx}{q}", f"unitRef={q}{unit}{q}", f'decimals={q}-6{q}', f'id={q}{idn("f")}{q}']; R.shuffle(a)
        return f"<{pre}:{name}{sp}" + sp.join(a) + f">{text}</{pre}:{name}>"
    def ctx_xml(c):
        i, kind, a, b, dim = c; seg = f'<segment><xbrldi:explicitMember dimension={q}us-gaap:Ax{q}>us-gaap:M{q}</xbrldi:explicitMember></segment>' if dim else ""
        per = f"<instant>{a}</instant>" if kind == "inst" else f"<startDate>{a}</startDate><endDate>{b}</endDate>"
        return f"<context id={q}{i}{q}><entity><identifier scheme={q}http://www.sec.gov/CIK{q}>0000000001</identifier>{seg}</entity><period>{per}</period></context>"
    units = [f'<unit id={q}{uid}{q}><measure>iso4217:USD</measure></unit>', f'<unit id={q}eur{q}><measure>iso4217:EUR</measure></unit>',
             f'<unit id={q}{eid}{q}><divide><unitNumerator><measure>iso4217:USD</measure></unitNumerator><unitDenominator><measure>{R.choice(["shares", "xbrli:shares"])}</measure></unitDenominator></divide></unit>']
    dei = [("dei", "DocumentType", c_dur, "10-K"), ("dei", "EntityRegistrantName", c_dur, R.choice(["ACME &amp; SONS", "Plain Co", "X &lt;Y&gt;"]))]
    if R.random() < 0.97: dei.insert(1, ("dei", "DocumentPeriodEndDate", c_dur, end.isoformat()))
    body = [ctx_xml(c) for c in ctxs] + units
    items = [fact_xml(f) for f in facts] + [f"<dei:{n} contextRef={q}{c}{q} id={q}{idn('d')}{q}>{t}</dei:{n}>" for _, n, c, t in dei]
    if nil: items.append(f"<{g}:Assets contextRef={q}{c_cur}{q} xsi:nil={q}true{q}/>")
    if R.random() < 0.3: body = body[: len(body)]; items = items; R.shuffle(items)
    out = [f'<?xml version="1.0" encoding="utf-8"?>', "<!-- synthetic -->", f"<xbrl\n  {NS.format(g=g)}>"]
    if R.random() < 0.5: out += body + items          # contexts first (the usual EDGAR order)
    else: out += items + body                          # facts first: legal, and a reader that assumes otherwise is wrong
    out.append("</xbrl>"); price = R.choice([0, 0, 1, 10000, 2550, 99999999, 12345678901])
    return "\n".join(out) + "\n", price
if __name__ == "__main__":
    import sys; t, p = gen(int(sys.argv[1])); sys.stdout.write(t); sys.stderr.write(f"price {p}\n")
