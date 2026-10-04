#!/usr/bin/env python3
"""Chapter 48: the INDEPENDENT reference for the ratio engine. It shares no code with the kernel's parser: it reads a filing with Python's own XML parser (xml.etree), so it can be run
on the FULL EDGAR document (every concept, every context) as well as on the slimmed copy the kernel embeds -- which also checks edgar_slim.py. The rules below are written down once
(the chapter page, 'The rules') and implemented twice. Arithmetic is exact: Python ints and the same round-half-away-from-zero on an exact fraction.
Usage: ratios_ref.py FILE.xml [price_cents]  ->  one canonical line per quantity, the same format the kernel prints: 'name value unit' or 'name NA reason'."""
import sys, datetime, xml.etree.ElementTree as ET
NS_XBRLI = "{http://www.xbrl.org/2003/instance}"; NS_DEI = "{http://xbrl.sec.gov/dei/"; NS_GAAP = "{http://fasb.org/us-gaap/"
REV = ["RevenueFromContractWithCustomerExcludingAssessedTax", "Revenues", "SalesRevenueNet"]
CAPEX = ["PaymentsToAcquirePropertyPlantAndEquipment", "PaymentsToAcquireProductiveAssets"]; COGS = ["CostOfRevenue", "CostOfGoodsAndServicesSold"]; INTEREST = ["InterestExpense", "InterestExpenseNonoperating"]; STI = ["MarketableSecuritiesCurrent", "ShortTermInvestments"]
USED = set(REV + COGS + INTEREST + STI + CAPEX + "Assets AssetsCurrent Liabilities LiabilitiesCurrent LiabilitiesAndStockholdersEquity StockholdersEquity StockholdersEquityIncludingPortionAttributableToNoncontrollingInterest CashAndCashEquivalentsAtCarryingValue AccountsReceivableNetCurrent GrossProfit OperatingIncomeLoss NetIncomeLoss EarningsPerShareDiluted NetCashProvidedByUsedInOperatingActivities".split())  # the duplicate rule only concerns concepts the engine reads
def rdiv(n, d):
    """round half away from zero of n/d, d > 0"""
    s = -1 if n < 0 else 1; return s * ((2 * abs(n) + d) // (2 * d))
def rdiv_g(n, scale, d):
    """the engine's guarded division: None (reported as 'overflow') when |n|*scale or d exceeds 2**62, as the 64-bit kernel arithmetic must"""
    if abs(n) * scale > 2 ** 62 or d > 2 ** 62: return None
    return rdiv(n * scale, d)
def parse_scaled(txt, frac_digits):
    """'-12.3400' -> integer scaled by 10**frac_digits; None if not a plain decimal with at most frac_digits fraction digits"""
    t = txt.strip(); neg = t.startswith("-"); t = t[1:] if neg else t
    if not t or not all(c in "0123456789." for c in t) or t.count(".") > 1 or t.startswith("."): return None
    whole, _, frac = t.partition(".")
    if "." in t and not frac: return None
    if len(frac) > frac_digits: return None
    if not whole: return None
    v = int(whole) * 10 ** frac_digits + int((frac + "0" * frac_digits)[:frac_digits] or "0"); return -v if neg else v
def load(path):
    root = ET.parse(path).getroot(); ctx = {}
    for c in root.iter(NS_XBRLI + "context"):
        seg = c.find(NS_XBRLI + "entity/" + NS_XBRLI + "segment"); per = c.find(NS_XBRLI + "period")
        sc = per.find(NS_XBRLI + "startDate"); ec = per.find(NS_XBRLI + "endDate"); ic = per.find(NS_XBRLI + "instant")
        d = lambda e: datetime.date.fromisoformat(e.text.strip())
        if c.get("id") in ctx: raise ValueError("duplicate context id")
        ctx[c.get("id")] = (seg is not None, d(sc) if sc is not None else None, d(ec) if ec is not None else None, d(ic) if ic is not None else None)
    uids = [u.get("id") for u in root.iter(NS_XBRLI + "unit")]
    if len(uids) != len(set(uids)): raise ValueError("duplicate unit id")
    units = {}   # unit id -> "usd" | "usdPerShare" | None : resolved from the <unit> element, because the id is the filer's own choice ("usd", "U_USD", "U_UnitedStatesOfAmericaDollarsShare")
    for u in root.iter(NS_XBRLI + "unit"):
        m = [x.text.strip() for x in u.findall(NS_XBRLI + "measure")]; dv = u.find(NS_XBRLI + "divide")
        if len(m) == 1 and m[0] == "iso4217:USD": units[u.get("id")] = "usd"
        elif dv is not None:
            nu = [x.text.strip() for x in dv.findall(NS_XBRLI + "unitNumerator/" + NS_XBRLI + "measure")]; de = [x.text.strip() for x in dv.findall(NS_XBRLI + "unitDenominator/" + NS_XBRLI + "measure")]
            if nu == ["iso4217:USD"] and de in (["xbrli:shares"], ["shares"]): units[u.get("id")] = "usdPerShare"
    facts = []
    for e in root:
        cr = e.get("contextRef")
        if cr is None or e.tag.startswith(NS_XBRLI): continue
        if e.get("{http://www.w3.org/2001/XMLSchema-instance}nil") == "true": continue
        ns, _, local = e.tag[1:].rpartition("}") if False else (e.tag.split("}")[0][1:], None, e.tag.split("}")[1])
        kind = "dei" if ns.startswith("http://xbrl.sec.gov/dei/") else "gaap" if ns.startswith("http://fasb.org/us-gaap/") else None
        if kind: facts.append((kind, local, cr, units.get(e.get("unitRef") or ""), "".join(e.itertext())))
    return ctx, facts
def analyse(path, price_cents=None):
    ctx, facts = load(path); out = []
    def emit(name, val, unit, why=""): out.append(f"{name} {val} {unit}" if val is not None else f"{name} NA {why}")
    dei = {}
    for k, l, cr, u, t in facts:
        if k == "dei" and not ctx[cr][0]: dei.setdefault(l, []).append(t.strip())
    end = datetime.date.fromisoformat(dei["DocumentPeriodEndDate"][0])
    # context choice
    dur = [c for c, (seg, s, e, i) in ctx.items() if not seg and e == end and s is not None and 350 <= (e - s).days <= 380]
    cur = [c for c, (seg, s, e, i) in ctx.items() if not seg and i == end]
    pri_dates = sorted({i for c, (seg, s, e, i) in ctx.items() if not seg and i is not None and 350 <= (end - i).days <= 380})
    pri = [c for c, (seg, s, e, i) in ctx.items() if not seg and pri_dates and i == pri_dates[-1]]
    vals = {}; conflicts = set()
    for k, l, cr, u, t in facts:
        if k != "gaap" or ctx[cr][0]: continue
        sc = 4 if u == "usdPerShare" else 0
        if u not in ("usd", "usdPerShare"): continue
        v = parse_scaled(t, 4)
        if v is None: continue
        if sc == 0:
            if v % 10000: continue
            v //= 10000
        key = (l, cr)
        if key in vals and vals[key] != v and l in USED: conflicts.add(l)
        vals.setdefault(key, v)
    def get(names, cset):
        if isinstance(names, str): names = [names]
        for n in names:
            if n in conflicts: return None
            got = [vals[(n, c)] for c in cset if (n, c) in vals]
            if got: return got[0]
        return None
    D = lambda n: get(n, dur); I = lambda n: get(n, cur); P = lambda n: get(n, pri)
    A, A0 = I("Assets"), P("Assets"); LSE = I("LiabilitiesAndStockholdersEquity")
    eq = I("StockholdersEquity"); eq0 = P("StockholdersEquity")
    eqtot = I("StockholdersEquityIncludingPortionAttributableToNoncontrollingInterest"); eqtot = eqtot if eqtot is not None else eq
    L = I("Liabilities"); Ld = False
    if L is None and A is not None and eqtot is not None: L = A - eqtot; Ld = True
    CA, CL = I("AssetsCurrent"), I("LiabilitiesCurrent"); cash = I("CashAndCashEquivalentsAtCarryingValue"); ar = I("AccountsReceivableNetCurrent"); sti = I(STI)
    rev = D(REV); gp = D("GrossProfit"); cg = D(COGS); gpd = False
    if gp is None and rev is not None and cg is not None: gp = rev - cg; gpd = True
    oi = D("OperatingIncomeLoss"); ni = D("NetIncomeLoss"); intr = D(INTEREST); ocf = D("NetCashProvidedByUsedInOperatingActivities"); capex = D(CAPEX); eps = D("EarningsPerShareDiluted")
    # integrity
    ok1 = None if A is None or LSE is None else A == LSE
    ok2 = None if L is None or Ld or eqtot is None or LSE is None else L + eqtot == LSE
    g_rep, g_rev, g_cg = D("GrossProfit"), rev, cg
    ok3 = None if g_rep is None or g_rev is None or g_cg is None else g_rep == g_rev - g_cg
    for name, r in (("check_assets_eq_liab_plus_equity", ok1), ("check_liabilities_plus_equity", ok2), ("check_gross_profit", ok3)):
        out.append(f"{name} {'SKIP' if r is None else 'PASS' if r else 'FAIL'}")
    out.append(f"check_conflicting_duplicates {'FAIL' if conflicts else 'PASS'}")
    if ok1 is False or conflicts:
        out.append("verdict REJECTED"); return out
    out.append("verdict ACCEPTED")
    def mult(name, n, d, why):
        if n is None or d is None: emit(name, None, "", why); return
        if d <= 0: emit(name, None, "", "denominator not positive"); return
        v = rdiv_g(n, 100, d); emit(name, v, "x", "overflow")
    def pct(name, n, d, why):
        if n is None or d is None: emit(name, None, "", why); return
        if d <= 0: emit(name, None, "", "denominator not positive"); return
        v = rdiv_g(n, 10000, d); emit(name, v, "bp", "overflow")
    def avg(name, kind, n, a1, a0, why):
        if n is None or a1 is None or a0 is None: emit(name, None, "", why); return
        if a1 + a0 <= 0: emit(name, None, "", "denominator not positive"); return
        if kind == "pct": v = rdiv_g(n, 20000, a1 + a0); emit(name, v, "bp", "overflow")
        else: v = rdiv_g(n, 200, a1 + a0); emit(name, v, "x", "overflow")
    mult("current_ratio", CA, CL, "missing input")
    q = None if cash is None or ar is None else cash + (sti or 0) + ar; mult("quick_ratio", q, CL, "missing input")
    c = None if cash is None else cash + (sti or 0); mult("cash_ratio", c, CL, "missing input")
    mult("liabilities_to_equity", L, eq, "missing input"); pct("liabilities_to_assets", L, A, "missing input"); mult("equity_multiplier", A, eq, "missing input")
    mult("interest_coverage", oi, intr, "missing input")
    pct("gross_margin", gp, rev, "missing input"); pct("operating_margin", oi, rev, "missing input"); pct("net_margin", ni, rev, "missing input")
    avg("return_on_assets", "pct", ni, A, A0, "missing input"); avg("return_on_equity", "pct", ni, eq, eq0, "missing input"); avg("asset_turnover", "x", rev, A, A0, "missing input")
    fcf = None if ocf is None or capex is None else ocf - capex
    emit("free_cash_flow", fcf, "usd", "missing input"); pct("fcf_margin", fcf, rev, "missing input")
    emit("eps_diluted_reported", eps, "usd4", "not reported as us-gaap:EarningsPerShareDiluted")
    if price_cents is None or eps is None: emit("price_to_earnings", None, "", "no price or no EPS")
    elif eps <= 0: emit("price_to_earnings", None, "", "EPS not positive")
    else: v = rdiv_g(price_cents, 10000, eps); emit("price_to_earnings", v, "x", "overflow")
    return out
if __name__ == "__main__":
    pr = int(sys.argv[2]) if len(sys.argv) > 2 else None
    print("\n".join(analyse(sys.argv[1], pr)))
