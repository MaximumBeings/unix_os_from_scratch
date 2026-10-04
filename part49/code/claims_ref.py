#!/usr/bin/env python3
"""Chapter 49: the INDEPENDENT reference. It shares no code with the kernel: its own X12 splitter, its own claim extraction (it assumes a VALID claim -- validation is what the C reader does), its own adjudicator (the coinsurance
rounding done with Python's decimal module, ROUND_HALF_UP), its own 835 builder and its own 835 reconciler. The fee schedule is DATA, not logic, and is the one thing both sides share (typed in again here, not imported).
Usage:  claims_ref.py adj  FILE [ded ded_met copay bp oop oop_met]   -> the canonical text 049_adjud.c's adj_canonical() prints
        claims_ref.py build FILE [...]                               -> the 835 text
        claims_ref.py recon FILE                                     -> the canonical text of claims_cli's `recon`"""
import sys, datetime
from decimal import Decimal, ROUND_HALF_UP
FEES = {"99202": 9000, "99203": 13500, "99204": 20500, "99205": 26500, "99211": 2500, "99212": 5500, "99213": 8800, "99214": 12500, "99215": 17500, "85025": 1500, "80053": 2000, "36415": 300, "71046": 4500, "93000": 3500,
        "45378": 38000, "29881": 120000, "27447": 1500000}
EM = {"99202", "99203", "99204", "99205", "99211", "99212", "99213", "99214", "99215"}
def split(text):
    t = text.lstrip(); es, cs, term = t[3], t[104], t[105]
    segs = [s.strip("\r\n \t") for s in t.split(term)]; return es, cs, [s for s in segs if s]
def money(s):
    neg = s.startswith("-"); s = s.lstrip("-"); w, _, f = s.partition("."); v = int(w) * 100 + int((f + "00")[:2]); return -v if neg else v
def parse_837(text):
    es, cs, segs = split(text); claims = []; cur = None; billing = None; sub = None
    for s in segs:
        e = s.split(es)
        if e[0] == "NM1" and e[1] == "85": billing = (e[3], e[9])
        elif e[0] == "NM1" and e[1] == "IL": sub = (e[3], e[4], e[9])
        elif e[0] == "NM1" and e[1] == "PR": payer = (e[3], e[9])
        elif e[0] == "CLM": cur = dict(pcn=e[1], total=money(e[2]), lines=[], billing=billing, sub=sub, payer=payer); claims.append(cur)
        elif e[0] == "SV1": code = e[1].split(cs)[1]; cur["lines"].append(dict(code=code, charge=money(e[2]), units=int(e[4]), dos=None))
        elif e[0] == "DTP" and e[1] == "472" and cur and cur["lines"]: cur["lines"][-1]["dos"] = e[3][:8]
    return claims
def adjudicate(claims, plan):
    ded, ded_met, copay, bp, oop, oop_met = plan; hist = set(); out = []
    for c in claims:
        em_dos = set(); lines = []
        for ln in c["lines"]:
            key = (c["pcn"], ln["code"], ln["dos"], ln["charge"]); adj = []; allowed = 0; paid = 0
            if key in hist: adj.append(("CO", 18, ln["charge"]))
            elif ln["code"] not in FEES: adj.append(("CO", 96, ln["charge"]))
            else:
                allowed = min(ln["charge"], FEES[ln["code"]] * ln["units"]); co45 = ln["charge"] - allowed; cp = 0
                if ln["code"] in EM and ln["dos"] not in em_dos: em_dos.add(ln["dos"]); cp = min(copay, allowed)
                d = min(max(ded - ded_met, 0), allowed - cp); base = allowed - cp - d
                coins = int((Decimal(base) * Decimal(bp) / Decimal(10000)).quantize(Decimal(1), rounding=ROUND_HALF_UP))
                room = max(oop - oop_met, 0); pr = cp + d + coins
                if pr > room:
                    ex = pr - room; cut = min(ex, coins); coins -= cut; ex -= cut; cut = min(ex, d); d -= cut; ex -= cut; cut = min(ex, cp); cp -= cut; ex -= cut
                pr = cp + d + coins; paid = allowed - pr
                if co45: adj.append(("CO", 45, co45))
                for g, r, a in (("PR", 3, cp), ("PR", 1, d), ("PR", 2, coins)):
                    if a: adj.append((g, r, a))
                ded_met += d; oop_met += pr
            hist.add(key); lines.append(dict(code=ln["code"], units=ln["units"], dos=ln["dos"], charge=ln["charge"], allowed=allowed, paid=paid, adj=adj))
        tp = sum(l["paid"] for l in lines); tpr = sum(a for l in lines for g, r, a in l["adj"] if g == "PR"); tco = sum(a for l in lines for g, r, a in l["adj"] if g == "CO")
        status = 4 if all(l["allowed"] == 0 and l["paid"] == 0 and any(r in (18, 96) for g, r, a in l["adj"]) for l in lines) else 1
        out.append(dict(c=c, lines=lines, status=status, charge=sum(l["charge"] for l in lines), paid=tp, pr=tpr, co=tco, ded_met=ded_met, oop_met=oop_met))
    return out
def canonical(res):
    o = []
    for r in res:
        o.append(f"claim {r['c']['pcn']} status {r['status']} charge {r['charge']} paid {r['paid']} pr {r['pr']} co {r['co']}")
        for i, l in enumerate(r["lines"], 1): o.append(f"  line {i} {l['code']} charge {l['charge']} allowed {l['allowed']} paid {l['paid']}" + "".join(f" {g}-{rc}={a}" for g, rc, a in l["adj"]))
        o.append(f"  accumulators deductible_met {r['ded_met']} oop_met {r['oop_met']}")
    return "\n".join(o) + "\n"
def ms(c):
    if c < 0: return "-" + ms(-c)
    w, f = divmod(c, 100); return str(w) + ("" if f == 0 else "." + (f"{f:02d}".rstrip("0") if f % 10 == 0 else f"{f:02d}"))
def ymd(days): return (datetime.date(1970, 1, 1) + datetime.timedelta(days=days)).strftime("%Y%m%d")
def build_835(res, payer_id, payer_name, npi, payee_name, date_days, ctl):
    d = ymd(date_days); S = []
    total = sum(r["paid"] for r in res)
    S += ["ST*835*0001", f"BPR*I*{ms(total)}*C*CHK************{d}", f"TRN*1*{ctl}*{payer_id}", f"DTM*405*{d}", f"N1*PR*{payer_name}", f"N1*PE*{payee_name}*XX*{npi}"]
    for i, r in enumerate(res, 1):
        c = r["c"]; S += [f"LX*{i}", f"CLP*{c['pcn']}*{r['status']}*{ms(r['charge'])}*{ms(r['paid'])}*{ms(r['pr'])}*MC*ICN{ctl}-{i}", f"NM1*QC*1*{c['sub'][0]}*{c['sub'][1]}****MI*{c['sub'][2]}"]
        for l in r["lines"]:
            S += [f"SVC*HC:{l['code']}*{ms(l['charge'])}*{ms(l['paid'])}**{l['units']}", "DTM*472*" + l["dos"]]
            for g in ("CO", "PR"):
                items = [(rc, a) for gg, rc, a in l["adj"] if gg == g]
                if items: S.append(f"CAS*{g}*" + "**".join(f"{rc}*{ms(a)}" for rc, a in items))
    S.append(f"SE*{len(S) + 1}*0001")
    c9 = "%09d" % ctl
    head = f"ISA*00*          *00*          *ZZ*{payer_id.ljust(15)}*ZZ*{npi.ljust(15)}*{d[2:]}*1200*^*00501*{c9}*0*T*:"
    return "~".join([head, f"GS*HP*{payer_id}*{npi}*{d}*1200*{ctl}*X*005010X221A1"] + S + [f"GE*1*{ctl}", f"IEA*1*{c9}"]) + "~"
def recon(text):
    es, cs, segs = split(text); claims = []; cur = None; ln = None; bpr = None; plb = 0; o = []
    for s in segs:
        e = s.split(es)
        if e[0] == "BPR": bpr = money(e[2])
        elif e[0] == "PLB": plb += sum(money(x) for x in e[4::2] if x)
        elif e[0] == "CLP": cur = dict(pcn=e[1], status=int(e[2]), charge=money(e[3]), paid=money(e[4]), patient=money(e[5]) if len(e) > 5 and e[5] else 0, lines=[], ccas=None, cpr=0); claims.append(cur); ln = None
        elif e[0] == "SVC": ln = dict(charge=money(e[2]), paid=money(e[3]), cas=0, pr=0); cur["lines"].append(ln)
        elif e[0] == "CAS":
            amts = [money(x) for x in e[3::3]]; pr = sum(amts) if e[1] == "PR" else 0
            if ln is not None: ln["cas"] += sum(amts); ln["pr"] += pr
            else: cur["ccas"] = (cur["ccas"] or 0) + sum(amts); cur["cpr"] += pr
    paid_sum = sum(c["paid"] for c in claims); bpr_ok = bpr == paid_sum - plb; o.append(f"bpr {ms(bpr)} clp_paid_sum {ms(paid_sum)} plb {'yes' if plb or any(x.startswith('PLB') for x in segs) else 'no'} balanced {int(bpr_ok)}"); allb = bpr_ok
    for c in claims:
        lines_ok = all(l["charge"] - l["paid"] == l["cas"] for l in c["lines"]); expect = c["ccas"] if c["ccas"] is not None else sum(l["cas"] for l in c["lines"])
        bal = (c["charge"] - c["paid"] == expect) and lines_ok; prm = c["patient"] == c["cpr"] + sum(l["pr"] for l in c["lines"]); allb = allb and bal
        o.append(f"claim {c['pcn']} status {c['status']} charge {ms(c['charge'])} paid {ms(c['paid'])} patient {ms(c['patient'])} lines {len(c['lines'])} balanced {int(bal)} pr_matches {int(prm)}")
        for k, l in enumerate(c["lines"], 1): o.append(f"  line {k} charge {ms(l['charge'])} paid {ms(l['paid'])} cas {ms(l['cas'])} balanced {int(l['charge'] - l['paid'] == l['cas'])}")
    o.append(f"all_balanced {int(allb)}"); return "\n".join(o) + "\n"
if __name__ == "__main__":
    mode, path = sys.argv[1], sys.argv[2]; text = open(path).read()
    if mode == "recon": sys.stdout.write(recon(text)); sys.exit(0)
    plan = tuple(int(x) for x in sys.argv[3:9]) if len(sys.argv) >= 9 else (50000, 0, 2500, 2000, 200000, 0)
    res = adjudicate(parse_837(text), plan)
    if mode == "adj": sys.stdout.write(canonical(res))
    else:
        cl = res[0]["c"]; sys.stdout.write(build_835(res, cl["payer"][1], cl["payer"][0], cl["billing"][1], cl["billing"][0], 19000, 1))
