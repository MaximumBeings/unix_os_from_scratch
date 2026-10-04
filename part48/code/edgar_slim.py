#!/usr/bin/env python3
"""Chapter 48: reduce a real SEC XBRL instance document (the *_htm.xml file EDGAR extracts from every inline-XBRL 10-K) to the facts the ratio engine needs.
It does not re-serialise anything: it copies, byte for byte, the root start tag, the <context> and <unit> blocks the facts use, and the fact elements it keeps -- so the kernel parser sees real EDGAR formatting
(attributes in any order, split over lines, elements on several lines, dimensional contexts). A fact is kept when its concept is in KEEP (every fact of that concept, dimensional or not).
Usage: edgar_slim.py FULL.xml OUT.xml"""
import re, sys
KEEP = """Assets AssetsCurrent Liabilities LiabilitiesCurrent LiabilitiesAndStockholdersEquity StockholdersEquity StockholdersEquityIncludingPortionAttributableToNoncontrollingInterest
CashAndCashEquivalentsAtCarryingValue MarketableSecuritiesCurrent ShortTermInvestments AccountsReceivableNetCurrent
Revenues RevenueFromContractWithCustomerExcludingAssessedTax SalesRevenueNet CostOfRevenue CostOfGoodsAndServicesSold GrossProfit OperatingIncomeLoss
InterestExpense InterestExpenseNonoperating NetIncomeLoss EarningsPerShareDiluted NetCashProvidedByUsedInOperatingActivities PaymentsToAcquirePropertyPlantAndEquipment PaymentsToAcquireProductiveAssets""".split()
DEI = "DocumentType DocumentPeriodEndDate EntityRegistrantName EntityCentralIndexKey".split()
def slim(text):
    root = re.search(r"<xbrl\b[^>]*>", text, re.S).group(0)
    names = "|".join(["us-gaap:" + k for k in KEEP] + ["dei:" + k for k in DEI])
    fact = re.compile(r"<(%s)\b[^>]*?>.*?</\1>|<(?:%s)\b[^>]*?/>" % (names, names), re.S)
    facts = [m.group(0) for m in fact.finditer(text)]
    used = set(re.findall(r'contextRef="([^"]+)"', "\n".join(facts)))
    ctx = [m.group(0) for m in re.finditer(r"<context\b[^>]*>.*?</context>", text, re.S) if re.search(r'id="([^"]+)"', m.group(0)).group(1) in used]
    uref = set(re.findall(r'unitRef="([^"]+)"', "\n".join(facts)))
    units = [m.group(0) for m in re.finditer(r"<unit\b[^>]*>.*?</unit>", text, re.S) if re.search(r'id="([^"]+)"', m.group(0)).group(1) in uref]
    return root + "\n" + "\n".join(ctx) + "\n" + "\n".join(units) + "\n" + "\n".join(facts) + "\n</xbrl>\n", len(facts), len(ctx)
if __name__ == "__main__":
    out, nf, nc = slim(open(sys.argv[1], encoding="utf-8").read())
    open(sys.argv[2], "w", encoding="utf-8").write(out); print(f"{sys.argv[1]}: {nf} facts, {nc} contexts, {len(out)} bytes")
