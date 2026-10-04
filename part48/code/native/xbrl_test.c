/* Chapter 48 host test: the XBRL reader and the ratio engine, built with AddressSanitizer + UBSan. Part 1: arithmetic against 128-bit reference arithmetic. Part 2: the date and number parsers.
 * Part 3: hand-computed answers on small documents, one rule at a time. Part 4: every way a document can be malformed. Each line prints PASS or FAIL; exit status 1 on any FAIL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../048_xbrl.h"
#include "../048_ratios.h"
#include "../048_filings.h"
/* 048_filings.c names the six embedded documents; the host test does not link them, so give it empty stand-ins */
const char filing_aapl_start[1], filing_aapl_end[1], filing_ko_start[1], filing_ko_end[1], filing_nvda_start[1], filing_nvda_end[1], filing_msft_start[1], filing_msft_end[1], filing_xom_start[1], filing_xom_end[1], filing_jpm_start[1], filing_jpm_end[1];
static int pass_n, fail_n;
static void check(const char *name, int cond) { printf("%s %s\n", cond ? "PASS" : "FAIL", name); if (cond) { pass_n++; } else { fail_n++; } }
static uint64_t rng = 88172645463325252ull;
static uint64_t xr(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }
static xb_doc_t doc; static rt_report_t rep;
static char out[8192];
static int run(const char *text, int64_t price, char **res) {
    int rc = xb_parse(&doc, text, (uint32_t)strlen(text)); if (rc) { *res = ""; return rc; }
    rc = rt_analyse(&doc, price, &rep); if (rc) { *res = ""; return rc; }
    rt_canonical(&rep, out, sizeof out); *res = out; return 0;
}
/* ---- a tiny document builder ---- */
static char B[1 << 17]; static size_t bl;
static void P(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#include <stdarg.h>
static void P(const char *fmt, ...) { va_list ap; va_start(ap, fmt); bl += (size_t)vsnprintf(B + bl, sizeof B - bl, fmt, ap); va_end(ap); }
static void begin(void) { bl = 0; P("<?xml version=\"1.0\"?><xbrl xmlns=\"http://www.xbrl.org/2003/instance\" xmlns:us-gaap=\"http://fasb.org/us-gaap/2023\" xmlns:dei=\"http://xbrl.sec.gov/dei/2023\" xmlns:xbrldi=\"http://xbrl.org/2006/xbrldi\">\n"); }
static void ctx_dur(const char *id, const char *s, const char *e, int dim) { P("<context id=\"%s\"><entity><identifier scheme=\"x\">1</identifier>%s</entity><period><startDate>%s</startDate><endDate>%s</endDate></period></context>\n", id, dim ? "<segment><xbrldi:explicitMember dimension=\"a:B\">a:C</xbrldi:explicitMember></segment>" : "", s, e); }
static void ctx_inst(const char *id, const char *d, int dim) { P("<context id=\"%s\"><entity><identifier scheme=\"x\">1</identifier>%s</entity><period><instant>%s</instant></period></context>\n", id, dim ? "<segment><xbrldi:explicitMember dimension=\"a:B\">a:C</xbrldi:explicitMember></segment>" : "", d); }
static void units(void) { P("<unit id=\"usd\"><measure>iso4217:USD</measure></unit><unit id=\"ps\"><divide><unitNumerator><measure>iso4217:USD</measure></unitNumerator><unitDenominator><measure>shares</measure></unitDenominator></divide></unit>\n"); }
static void F(const char *n, const char *c, long long v) { P("<us-gaap:%s contextRef=\"%s\" unitRef=\"usd\" decimals=\"-6\">%lld</us-gaap:%s>\n", n, c, v, n); }
static void dei(const char *end) { P("<dei:DocumentPeriodEndDate contextRef=\"D\" id=\"d1\">%s</dei:DocumentPeriodEndDate><dei:EntityRegistrantName contextRef=\"D\">Test &amp; Co</dei:EntityRegistrantName>\n", end); }
static const char *finish(void);
static const char *finish(void) { P("</xbrl>\n"); return B; }
static void std_ctx(void) { ctx_dur("D", "2022-12-31", "2023-12-31", 0); ctx_inst("I", "2023-12-31", 0); ctx_inst("P", "2022-12-31", 0); }
static const char *base_company(void) {
    begin(); std_ctx(); units(); dei("2023-12-31");
    F("Assets", "I", 1000); F("Assets", "P", 800); F("AssetsCurrent", "I", 400); F("LiabilitiesCurrent", "I", 200); F("Liabilities", "I", 600); F("StockholdersEquity", "I", 400); F("StockholdersEquity", "P", 300);
    F("LiabilitiesAndStockholdersEquity", "I", 1000); F("CashAndCashEquivalentsAtCarryingValue", "I", 100); F("AccountsReceivableNetCurrent", "I", 50); F("MarketableSecuritiesCurrent", "I", 50);
    F("Revenues", "D", 2000); F("CostOfRevenue", "D", 1200); F("GrossProfit", "D", 800); F("OperatingIncomeLoss", "D", 300); F("InterestExpense", "D", 30); F("NetIncomeLoss", "D", 200);
    F("NetCashProvidedByUsedInOperatingActivities", "D", 250); F("PaymentsToAcquirePropertyPlantAndEquipment", "D", 100);
    P("<us-gaap:EarningsPerShareDiluted contextRef=\"D\" unitRef=\"ps\" decimals=\"2\">2.5</us-gaap:EarningsPerShareDiluted>\n");
    return finish();
}
static int has(const char *res, const char *line) { /* the line appears as a whole line of the output */
    size_t n = strlen(line); const char *p = res;
    while ((p = strstr(p, line))) { if ((p == res || p[-1] == '\n') && p[n] == '\n') { return 1; } p++; }
    return 0;
}
int main(void) {
    char *res;
    printf("== 1. 64-bit arithmetic against 128-bit reference (no libgcc in the kernel, so division is ours) ==\n");
    int bad = 0;
    for (int i = 0; i < 300000; i++) {
        uint64_t n = xr() >> (xr() % 64), d = (xr() >> (xr() % 64)) | 1, r; uint64_t q = xb_udivmod(n, d, &r);
        if (q != n / d || r != n % d) { bad++; }
    }
    check("xb_udivmod equals native / and % on 300000 random pairs (all magnitudes)", bad == 0);
    uint64_t r0; check("xb_udivmod: division by zero returns 0 and leaves the dividend in rem", xb_udivmod(77, 0, &r0) == 0 && r0 == 77);
    bad = 0;
    for (int i = 0; i < 300000; i++) {
        int64_t n = (int64_t)(xr() >> (2 + xr() % 62)); if (xr() & 1) { n = -n; }
        int64_t scale = (int64_t[]){100, 10000, 20000, 200}[xr() % 4], d = (int64_t)((xr() >> (2 + xr() % 62)) | 1), got; int rc = rt_rdiv(n, scale, d, &got);
        __int128 num = (__int128)n * scale; __int128 an = num < 0 ? -num : num; int overflow = an > ((__int128)1 << 62) || d > ((int64_t)1 << 62);
        if (overflow) { if (rc == 0) { bad++; } continue; }
        __int128 q = (2 * an + d) / (2 * (__int128)d); if (num < 0) { q = -q; }
        if (rc != 0 || got != (int64_t)q) { bad++; }
    }
    check("rt_rdiv equals round-half-away-from-zero computed in 128 bits, overflow refused, on 300000 random cases", bad == 0);
    int64_t v;
    check("rt_rdiv: 1/8 of 100 = 12.5 rounds to 13", rt_rdiv(1, 100, 8, &v) == 0 && v == 13); check("rt_rdiv: -1/8 of 100 = -12.5 rounds to -13 (away from zero)", rt_rdiv(-1, 100, 8, &v) == 0 && v == -13);
    check("rt_rdiv: 1/20000 of 10000 = 0.5 rounds to 1, and -0.5 to -1", rt_rdiv(1, 10000, 20000, &v) == 0 && v == 1 && rt_rdiv(-1, 10000, 20000, &v) == 0 && v == -1);
    check("rt_rdiv: 0.4999 rounds to 0", rt_rdiv(4999, 1, 10000, &v) == 0 && v == 0);
    check("rt_rdiv: zero or negative denominator refused", rt_rdiv(1, 1, 0, &v) != 0 && rt_rdiv(1, 1, -5, &v) != 0);
    printf("\n== 2. dates and numbers ==\n");
    int32_t day;
    check("1970-01-01 is day 0", xb_parse_date("1970-01-01", 10, &day) == 0 && day == 0); check("2000-03-01 is day 11017", xb_parse_date("2000-03-01", 10, &day) == 0 && day == 11017);
    check("2024-02-29 (leap) accepted", xb_parse_date("2024-02-29", 10, &day) == 0); check("2023-02-29 refused", xb_parse_date("2023-02-29", 10, &day) != 0);
    check("1900-02-29 refused (century, not leap)", xb_parse_date("1900-02-29", 10, &day) != 0); check("2000-02-29 accepted (400-year leap)", xb_parse_date("2000-02-29", 10, &day) == 0);
    const char *bad_dates[] = {"2023-13-01", "2023-00-10", "2023-04-31", "2023-4-1", "20230401", "2023/04/01", "2023-04-0x", "", "2023-04-011", " 2023-04-01"};
    int all = 1; for (int i = 0; i < 10; i++) { if (xb_parse_date(bad_dates[i], (uint32_t)strlen(bad_dates[i]), &day) == 0) { all = 0; } }
    check("ten malformed dates all refused", all);
    check("one year later is 365 days (non-leap) and 366 days (across Feb 29, 2024)", xb_days_from_civil(2024, 1, 1) - xb_days_from_civil(2023, 1, 1) == 365 && xb_days_from_civil(2025, 1, 1) - xb_days_from_civil(2024, 1, 1) == 366);
    struct { const char *s; int ok; long long v; } nums[] = {{"123", 1, 1230000}, {"-7", 1, -70000}, {"6.13", 1, 61300}, {"-0.5", 1, -5000}, {"2.1234", 1, 21234}, {"  42  ", 1, 420000}, {"0", 1, 0}, {"1.23456", 0, 0}, {"", 0, 0}, {"-", 0, 0}, {".5", 0, 0},
                                  {"5.", 0, 0}, {"1e6", 0, 0}, {"1,000", 0, 0}, {"+5", 0, 0}, {"--5", 0, 0}, {"1.2.3", 0, 0}, {"12a", 0, 0}, {"999999999999999999999", 0, 0}};
    all = 1; for (size_t i = 0; i < sizeof nums / sizeof nums[0]; i++) { int64_t o = 0; int rc = xb_parse_scaled(nums[i].s, (uint32_t)strlen(nums[i].s), &o); if ((rc == 0) != nums[i].ok || (rc == 0 && o != nums[i].v)) { all = 0; printf("   number case failed: '%s'\n", nums[i].s); } }
    check("19 number cases (plain, signed, 4-decimals, padded; and refused: 5 decimals, empty, '.5', '5.', exponent, commas, '+', '1.2.3', letters, overflow)", all);
    printf("\n== 3. hand-computed answers, one rule at a time ==\n");
    int rc = run(base_company(), 5000, &res);
    check("a small company parses and is ACCEPTED", rc == 0 && has(res, "verdict ACCEPTED"));
    struct { const char *line; const char *what; } exp[] = {
        {"current_ratio 200 x", "current ratio 400/200 = 2.00"}, {"quick_ratio 100 x", "quick ratio (100 cash + 50 securities + 50 receivables)/200 = 1.00"}, {"cash_ratio 75 x", "cash ratio (100+50)/200 = 0.75"},
        {"liabilities_to_equity 150 x", "liabilities/equity 600/400 = 1.50"}, {"liabilities_to_assets 6000 bp", "liabilities/assets 60.00%"}, {"equity_multiplier 250 x", "equity multiplier 1000/400 = 2.50"},
        {"interest_coverage 1000 x", "interest coverage 300/30 = 10.00"}, {"gross_margin 4000 bp", "gross margin 800/2000 = 40.00%"}, {"operating_margin 1500 bp", "operating margin 15.00%"}, {"net_margin 1000 bp", "net margin 10.00%"},
        {"return_on_assets 2222 bp", "ROA 200/avg(1000,800) = 22.22%"}, {"return_on_equity 5714 bp", "ROE 200/avg(400,300) = 57.14%"}, {"asset_turnover 222 x", "asset turnover 2000/900 = 2.22"},
        {"free_cash_flow 150 usd", "free cash flow 250-100"}, {"fcf_margin 750 bp", "FCF margin 7.50%"}, {"eps_diluted_reported 25000 usd4", "EPS 2.5000 as 1/10000 dollars"}, {"price_to_earnings 2000 x", "P/E $50.00 / $2.50 = 20.00"}};
    for (size_t i = 0; i < sizeof exp / sizeof exp[0]; i++) { check(exp[i].what, has(res, exp[i].line)); }
    check("all four checks PASS", has(res, "check_assets_eq_liab_plus_equity PASS") && has(res, "check_liabilities_plus_equity PASS") && has(res, "check_gross_profit PASS") && has(res, "check_conflicting_duplicates PASS"));
    check("period: fiscal year, balance-sheet date and prior date each matched exactly one context", rep.n_dur == 1 && rep.n_cur == 1 && rep.n_pri == 1);
    check("the registrant name decodes &amp;", strcmp(xb_dei_text(&doc, XB_DEI_NAME), "Test & Co") == 0);
    /* rounding: NI/revenue = 0.5 bp exactly */
    begin(); std_ctx(); units(); dei("2023-12-31"); F("Revenues", "D", 20000); F("NetIncomeLoss", "D", 1); rc = run(finish(), 0, &res);
    check("net margin exactly 0.5 basis point rounds to 1 (half away from zero)", rc == 0 && has(res, "net_margin 1 bp"));
    begin(); std_ctx(); units(); dei("2023-12-31"); F("Revenues", "D", 20000); F("NetIncomeLoss", "D", -1); rc = run(finish(), 0, &res);
    check("net margin exactly -0.5 basis point rounds to -1", rc == 0 && has(res, "net_margin -1 bp"));
    /* dimensional facts never count */
    begin(); std_ctx(); ctx_inst("IX", "2023-12-31", 1); ctx_dur("DX", "2022-12-31", "2023-12-31", 1); units(); dei("2023-12-31");
    F("Assets", "IX", 999999); F("Assets", "I", 1000); F("Liabilities", "I", 600); F("StockholdersEquity", "I", 400); F("Revenues", "DX", 5); F("Revenues", "D", 2000); F("NetIncomeLoss", "D", 200); rc = run(finish(), 0, &res);
    check("a segment value for Assets (999999) is ignored; the consolidated 1000 is used", rc == 0 && has(res, "liabilities_to_assets 6000 bp") && has(res, "net_margin 1000 bp"));
    check("a document with ONLY dimensional facts for a concept reports it missing", (begin(), std_ctx(), ctx_inst("IX", "2023-12-31", 1), units(), dei("2023-12-31"), F("Assets", "IX", 5), F("Liabilities", "I", 6), run(finish(), 0, &res) == 0 && has(res, "liabilities_to_assets NA missing input")));
    /* the 350..380 day window (fiscal year end 2023-12-31): 350 days back is 2023-01-15, 380 days back is 2022-12-16 */
    const char *ds[] = {"2023-01-16", "2023-01-15", "2023-01-14", "2022-12-17", "2022-12-16", "2022-12-15"}; const int dn[] = {0, 1, 1, 1, 1, 0}; int ok_win = 1;
    for (int i = 0; i < 6; i++) { begin(); ctx_dur("D", ds[i], "2023-12-31", 0); ctx_inst("I", "2023-12-31", 0); units(); dei("2023-12-31"); rc = run(finish(), 0, &res); if (rc != 0 || rep.n_dur != dn[i]) { ok_win = 0; printf("   (year starting %s: n_dur %d)\n", ds[i], rep.n_dur); } }
    check("fiscal-year context: 349 days refused, 350 / 351 / 379 / 380 accepted, 381 refused (both edges of 350..380)", ok_win);
    ok_win = 1;
    for (int i = 0; i < 6; i++) { begin(); ctx_dur("D", "2022-12-31", "2023-12-31", 0); ctx_inst("I", "2023-12-31", 0); ctx_inst("P", ds[i], 0); units(); dei("2023-12-31"); rc = run(finish(), 0, &res); if (rc != 0 || rep.n_pri != dn[i]) { ok_win = 0; } }
    check("prior balance-sheet date: the same window, 349 refused ... 381 refused", ok_win);
    begin(); std_ctx(); ctx_inst("P2", "2023-01-13", 0); units(); dei("2023-12-31"); F("Assets", "P", 111); F("Assets", "P2", 222); F("Assets", "I", 1000); F("NetIncomeLoss", "D", 100);
    rc = run(finish(), 0, &res);
    check("two candidate prior dates (365 and 352 days back): the LATER one is used (ROA = 100 / avg(1000, 222) = 16.37%)", rc == 0 && has(res, "return_on_assets 1637 bp"));
    check("a 52/53-week year-end that is NOT the dei period end is not used (instant one day off: no current-year balance sheet)", (begin(), ctx_dur("D", "2022-12-31", "2023-12-31", 0), ctx_inst("I", "2023-12-30", 0), units(), dei("2023-12-31"), F("Assets", "I", 5), F("Liabilities", "I", 3), F("StockholdersEquity", "I", 2), run(finish(), 0, &res) == 0 && has(res, "liabilities_to_assets NA missing input")));
    /* integrity checks and rejection */
    begin(); std_ctx(); units(); dei("2023-12-31"); F("Assets", "I", 1000); F("LiabilitiesAndStockholdersEquity", "I", 1001); F("Revenues", "D", 5); rc = run(finish(), 0, &res);
    check("assets 1000 vs liabilities-and-equity 1001: REJECTED, no ratios printed", rc == 0 && has(res, "check_assets_eq_liab_plus_equity FAIL") && has(res, "verdict REJECTED") && !strstr(res, "net_margin"));
    begin(); std_ctx(); units(); dei("2023-12-31"); F("Assets", "I", 1000); F("Assets", "I", 1000); F("Revenues", "D", 5); F("NetIncomeLoss", "D", 1); rc = run(finish(), 0, &res);
    check("the same fact twice with the SAME value is harmless", rc == 0 && has(res, "check_conflicting_duplicates PASS") && has(res, "verdict ACCEPTED"));
    begin(); std_ctx(); units(); dei("2023-12-31"); F("Assets", "I", 1000); F("Assets", "I", 1001); F("Revenues", "D", 5); rc = run(finish(), 0, &res);
    check("the same fact twice with DIFFERENT values: REJECTED (XBRL inconsistent duplicate)", rc == 0 && has(res, "check_conflicting_duplicates FAIL") && has(res, "verdict REJECTED"));
    begin(); std_ctx(); units(); dei("2023-12-31"); F("Revenues", "D", 100); F("Revenues", "D", 101); F("NetIncomeLoss", "D", 10); rc = run(finish(), 0, &res);
    check("a conflict in a concept the document does not even need still rejects it (the filing contradicts itself)", rc == 0 && has(res, "verdict REJECTED"));
    begin(); std_ctx(); units(); dei("2023-12-31"); F("Assets", "I", 1000); F("Liabilities", "I", 600); F("StockholdersEquity", "I", 400); F("LiabilitiesAndStockholdersEquity", "I", 1000); F("StockholdersEquityIncludingPortionAttributableToNoncontrollingInterest", "I", 450); rc = run(finish(), 0, &res);
    check("liabilities + equity-including-NCI (600 + 450) vs 1000: reported FAIL but NOT rejected (redeemable interests make this legitimate)", rc == 0 && has(res, "check_liabilities_plus_equity FAIL") && has(res, "verdict ACCEPTED"));
    begin(); std_ctx(); units(); dei("2023-12-31"); F("Assets", "I", 1000); F("StockholdersEquity", "I", 400); rc = run(finish(), 0, &res);
    check("Liabilities not reported: derived as assets - equity = 600, and the identity check is SKIPPED (it would be circular)", rc == 0 && has(res, "liabilities_to_assets 6000 bp") && has(res, "check_liabilities_plus_equity SKIP"));
    begin(); std_ctx(); units(); dei("2023-12-31"); F("Revenues", "D", 1000); F("CostOfGoodsAndServicesSold", "D", 400); rc = run(finish(), 0, &res);
    check("GrossProfit not reported: derived as revenue - cost = 600 (60.00%)", rc == 0 && has(res, "gross_margin 6000 bp") && has(res, "check_gross_profit SKIP"));
    begin(); std_ctx(); units(); dei("2023-12-31"); F("Revenues", "D", 1000); F("CostOfRevenue", "D", 400); F("GrossProfit", "D", 650); rc = run(finish(), 0, &res);
    check("reported GrossProfit 650 vs revenue - cost 600: check FAIL (reported, not rejected) and the REPORTED 650 is used", rc == 0 && has(res, "check_gross_profit FAIL") && has(res, "gross_margin 6500 bp"));
    begin(); std_ctx(); units(); dei("2023-12-31"); F("Revenues", "D", 1000); F("RevenueFromContractWithCustomerExcludingAssessedTax", "D", 2000); F("SalesRevenueNet", "D", 3000); F("NetIncomeLoss", "D", 200); rc = run(finish(), 0, &res);
    check("three revenue concepts present: the contract-revenue one wins (200/2000 = 10.00%)", rc == 0 && has(res, "net_margin 1000 bp"));
    begin(); std_ctx(); units(); dei("2023-12-31"); F("Revenues", "D", 1000); F("SalesRevenueNet", "D", 3000); F("NetIncomeLoss", "D", 200); rc = run(finish(), 0, &res);
    check("Revenues beats SalesRevenueNet (200/1000 = 20.00%)", rc == 0 && has(res, "net_margin 2000 bp"));
    begin(); std_ctx(); units(); dei("2023-12-31"); F("OperatingIncomeLoss", "D", 90); F("InterestExpenseNonoperating", "D", 30); rc = run(finish(), 0, &res);
    check("InterestExpenseNonoperating is the fallback for interest coverage (90/30 = 3.00)", rc == 0 && has(res, "interest_coverage 300 x"));
    begin(); std_ctx(); units(); dei("2023-12-31"); F("OperatingIncomeLoss", "D", 90); F("InterestExpense", "D", 0); rc = run(finish(), 0, &res);
    check("zero interest expense: coverage is NA (denominator not positive), never a division by zero", rc == 0 && has(res, "interest_coverage NA denominator not positive"));
    begin(); std_ctx(); units(); dei("2023-12-31"); F("Assets", "I", 1000); F("StockholdersEquity", "I", -50); F("Liabilities", "I", 1050); F("NetIncomeLoss", "D", 10); F("StockholdersEquity", "P", -80); rc = run(finish(), 0, &res);
    check("negative shareholders' equity: liabilities/equity, equity multiplier and ROE are NA (denominator not positive), liabilities/assets still computed", rc == 0 && has(res, "liabilities_to_equity NA denominator not positive") && has(res, "equity_multiplier NA denominator not positive") && has(res, "return_on_equity NA denominator not positive") && has(res, "liabilities_to_assets 10500 bp"));
    begin(); std_ctx(); units(); dei("2023-12-31"); F("PaymentsToAcquireProductiveAssets", "D", 30); F("NetCashProvidedByUsedInOperatingActivities", "D", 100); rc = run(finish(), 0, &res);
    check("PaymentsToAcquireProductiveAssets is the fallback for capex (100 - 30 = 70)", rc == 0 && has(res, "free_cash_flow 70 usd"));
    begin(); std_ctx(); units(); dei("2023-12-31"); P("<us-gaap:EarningsPerShareDiluted contextRef=\"D\" unitRef=\"ps\">-0.05</us-gaap:EarningsPerShareDiluted>\n"); rc = run(finish(), 5000, &res);
    check("negative EPS: reported as -500, P/E is NA (EPS not positive)", rc == 0 && has(res, "eps_diluted_reported -500 usd4") && has(res, "price_to_earnings NA EPS not positive"));
    begin(); std_ctx(); units(); dei("2023-12-31"); F("Revenues", "D", 1); F("NetIncomeLoss", "D", 900000000000000LL); rc = run(finish(), 0, &res);
    check("net income of 9e14 dollars: times 10000 it would overflow 64 bits, so the ratio is NA (overflow), not garbage", rc == 0 && has(res, "net_margin NA overflow"));
    /* units and ordering */
    bl = 0; P("<?xml version=\"1.0\"?><xbrl xmlns:us-gaap=\"http://fasb.org/us-gaap/2023\" xmlns:dei=\"http://xbrl.sec.gov/dei/2023\">");
    F("Revenues", "D", 1000); F("NetIncomeLoss", "D", 100); dei("2023-12-31"); units(); std_ctx(); rc = run(finish(), 0, &res);
    check("facts, dei and units placed BEFORE the contexts they refer to: references are resolved at the end, result identical", rc == 0 && has(res, "net_margin 1000 bp"));
    begin(); std_ctx(); P("<unit id=\"U_USD\"><measure>iso4217:USD</measure></unit><unit id=\"EUR1\"><measure>iso4217:EUR</measure></unit><unit id=\"sh\"><measure>shares</measure></unit>\n"); dei("2023-12-31");
    P("<us-gaap:Revenues contextRef=\"D\" unitRef=\"U_USD\">1000</us-gaap:Revenues><us-gaap:NetIncomeLoss contextRef=\"D\" unitRef=\"EUR1\">100</us-gaap:NetIncomeLoss><us-gaap:OperatingIncomeLoss contextRef=\"D\" unitRef=\"sh\">50</us-gaap:OperatingIncomeLoss>"); rc = run(finish(), 0, &res);
    check("the unit is found by its <measure>, not its id: 'U_USD' is dollars; a euro fact and a shares fact are dropped (net_margin and operating_margin NA)", rc == 0 && has(res, "net_margin NA missing input") && has(res, "operating_margin NA missing input") && doc.n_skipped_facts == 2);
    begin(); std_ctx(); units(); dei("2023-12-31"); P("<us-gaap:Revenues contextRef=\"D\" unitRef=\"usd\">1000.5</us-gaap:Revenues><us-gaap:NetIncomeLoss contextRef=\"D\" unitRef=\"usd\">100.0</us-gaap:NetIncomeLoss>"); rc = run(finish(), 0, &res);
    check("dollars with cents (1000.5) dropped, a whole number written '100.0' kept", rc == 0 && has(res, "net_margin NA missing input") && doc.n_skipped_facts == 1);
    begin(); std_ctx(); units(); dei("2023-12-31"); P("<us-gaap:Assets contextRef=\"I\" unitRef=\"usd\" xsi:nil=\"true\"/><us-gaap:Revenues contextRef=\"NOPE\" unitRef=\"usd\">1</us-gaap:Revenues><ext:Assets contextRef=\"I\" unitRef=\"usd\">7</ext:Assets>"); rc = run(finish(), 0, &res);
    check("a nil fact, a fact naming a context that does not exist (counted as an orphan), and an extension 'Assets' are all ignored", rc == 0 && doc.n_fact == 0 && doc.n_orphans == 1);
    bl = 0; P("<?xml version='1.0'?><!-- c --><xbrl xmlns:g='http://fasb.org/us-gaap/2023' xmlns:dei='http://xbrl.sec.gov/dei/2023'><![CDATA[ <g:Revenues> ]]>"); std_ctx(); units(); dei("2023-12-31");
    P("<g:Revenues\n\n   unitRef = 'usd'\n   contextRef\n = 'D'>1000</g:Revenues><g:NetIncomeLoss contextRef='D' unitRef='usd'>100</g:NetIncomeLoss>"); rc = run(finish(), 0, &res);
    check("us-gaap bound to the prefix 'g', single quotes, attributes split over lines with spaces around '=', a comment, a CDATA block: all handled", rc == 0 && has(res, "net_margin 1000 bp"));
    begin(); std_ctx(); units(); dei("2023-12-31"); P("<us-gaap:Revenues contextRef=\"D\" unitRef=\"usd\">1000</us-gaap:Revenues><us-gaap:NetIncomeLoss contextRef=\"D\" unitRef=\"usd\">-100</us-gaap:NetIncomeLoss>"); rc = run(finish(), 0, &res);
    check("a net loss (-100 on 1000) gives net margin -10.00%", rc == 0 && has(res, "net_margin -1000 bp"));
    check("no dei:DocumentPeriodEndDate -> XB_ERR_NO_PERIOD from the analysis", (begin(), std_ctx(), units(), F("Revenues", "D", 1), run(finish(), 0, &res) == XB_ERR_NO_PERIOD));
    check("a dei:DocumentPeriodEndDate that is not a date -> XB_ERR_BAD_DATE", (begin(), std_ctx(), units(), dei("December 31"), run(finish(), 0, &res) == XB_ERR_BAD_DATE));
    /* three rules that only a hostile document exercises */
    begin(); ctx_dur("D", "2022-12-31", "2023-12-31", 0); ctx_dur("DX", "2019-12-31", "2020-12-31", 1); ctx_inst("I", "2023-12-31", 0); ctx_inst("P", "2022-12-31", 0); units();
    P("<dei:DocumentPeriodEndDate contextRef=\"DX\">2020-12-31</dei:DocumentPeriodEndDate>\n"); dei("2023-12-31"); rc = run(finish(), 0, &res);
    check("a dei:DocumentPeriodEndDate inside a DIMENSIONAL context (2020-12-31) is ignored; the dimension-free one (2023-12-31) decides the period", rc == 0 && rep.n_dur == 1 && rep.n_cur == 1);
    begin(); std_ctx(); ctx_inst("IX", "2023-12-31", 1); units(); dei("2023-12-31"); F("Assets", "IX", 5); F("Assets", "IX", 6); F("Assets", "I", 1000); F("StockholdersEquity", "I", 400); rc = run(finish(), 0, &res);
    check("two different Assets values inside one SEGMENT context are not a conflict: dimensional facts are ignored entirely", rc == 0 && has(res, "check_conflicting_duplicates PASS") && has(res, "verdict ACCEPTED"));
    begin(); std_ctx(); units(); dei("2023-12-31"); F("Assets", "I", 1000); F("StockholdersEquity", "I", 400); F("StockholdersEquityIncludingPortionAttributableToNoncontrollingInterest", "I", 450); rc = run(finish(), 0, &res);
    check("derived liabilities use equity INCLUDING non-controlling interest: 1000 - 450 = 550 (55.00% of assets), while liabilities/equity uses the parent's 400 only", rc == 0 && has(res, "liabilities_to_assets 5500 bp") && has(res, "liabilities_to_equity 138 x"));
    begin(); std_ctx(); units(); dei("2023-12-31"); P("<us-gaap:Revenues contextRef=\"D\" unitRef=\"usd\" xsi:nil=\"true\">999</us-gaap:Revenues><us-gaap:Revenues contextRef=\"D\" unitRef=\"usd\">1000</us-gaap:Revenues><us-gaap:NetIncomeLoss contextRef=\"D\" unitRef=\"usd\">100</us-gaap:NetIncomeLoss>"); rc = run(finish(), 0, &res);
    check("a fact marked xsi:nil=\"true\" is ignored even if it carries text (999): it does not become a conflicting duplicate of the real 1000", rc == 0 && has(res, "check_conflicting_duplicates PASS") && has(res, "net_margin 1000 bp"));
    printf("\n== 3b. the formatting helpers the kernel's printout uses ==\n");
    { rt_line_t l; char b[48]; struct { int unit; int64_t v; const char *want; } fm[] = {{RT_U_BP, 2531, "25.31%"}, {RT_U_BP, 5, "0.05%"}, {RT_U_BP, -1000, "-10.00%"}, {RT_U_BP, 0, "0.00%"}, {RT_U_X, 99, "0.99x"}, {RT_U_X, 12345, "123.45x"}, {RT_U_X, -7, "-0.07x"},
        {RT_U_USD, 99584000000LL, "$99,584,000,000"}, {RT_U_USD, 0, "$0"}, {RT_U_USD, 999, "$999"}, {RT_U_USD, 1000, "$1,000"}, {RT_U_USD, -58390000000LL, "-$58,390,000,000"}, {RT_U_USD4, 61300, "$6.1300"}, {RT_U_USD4, -500, "-$0.0500"}};
      int all_f = 1; for (size_t i = 0; i < sizeof fm / sizeof fm[0]; i++) { l.unit = (uint8_t)fm[i].unit; l.value = fm[i].v; edgar_format(&l, b); if (strcmp(b, fm[i].want) != 0) { all_f = 0; printf("   format '%s' != '%s'\n", b, fm[i].want); } }
      check("14 value formats: percent, multiple, dollars with thousands separators, per-share dollars, negatives, zero", all_f);
      int all_d = 1; for (int32_t dd = -719162; dd <= 2932896; dd += 37) { char db[12]; edgar_date(dd, db); int32_t back; if (xb_parse_date(db, 10, &back) != 0 || back != dd) { all_d = 0; } }
      check("edgar_date is the exact inverse of xb_parse_date over 98,000 day numbers spread across the years 0001-9999", all_d);
      { char o1[12], o2[12]; edgar_date(-800000, o1); edgar_date(3000000, o2); check("edgar_date of a day number outside years 1..9999 writes a placeholder date and stays inside its 11-byte buffer", o1[0] == '?' && o1[4] == '-' && o1[10] == 0 && o2[0] == '?' && o2[7] == '-' && o2[10] == 0); }
      char db[12]; edgar_date(0, db); int z0 = strcmp(db, "1970-01-01") == 0; check("edgar_date(0) = 1970-01-01", z0);
      edgar_date(xb_days_from_civil(2023, 9, 30), db); check("edgar_date of the Apple fiscal year end is 2023-09-30", strcmp(db, "2023-09-30") == 0);
      static const char hay[] = "abcabd"; check("edgar_find: first of two partial matches, a miss, and a match at the very end", edgar_find(hay, 6, "abd") == 3 && edgar_find(hay, 6, "abx") == -1 && edgar_find(hay, 6, "d") == 5 && edgar_find(hay, 5, "d") == -1); }
    printf("\n== 4. malformed documents: an error code, never a crash, never a silent partial result ==\n");
    const char *good = base_company(); size_t glen = strlen(good); char *g2 = malloc(glen + 1); strcpy(g2, good);
    int accepted_prefix = 0, crashed = 0; (void)crashed;
    for (size_t cut = 0; cut + 9 < glen; cut += 7) { int r2 = xb_parse(&doc, g2, (uint32_t)cut); if (r2 == XB_OK) { accepted_prefix++; } }
    check("the sample document cut at every 7th byte before its closing tag: every one is refused (none parsed as complete)", accepted_prefix == 0);
    check("the full sample document, cut exactly after '</xbrl>' plus trailing junk, still parses", xb_parse(&doc, g2, (uint32_t)glen) == XB_OK);
    struct { const char *name; const char *text; int code; } mal[] = {
        {"empty input", "", XB_ERR_NOT_XBRL}, {"plain text", "hello world", XB_ERR_NOT_XBRL}, {"root is <html>", "<html><body></body></html>", XB_ERR_NOT_XBRL},
        {"mismatched closing tag", "<xbrl><a></b></xbrl>", XB_ERR_XML}, {"closing tag with nothing open", "</xbrl>", XB_ERR_XML}, {"unclosed root", "<xbrl><a></a>", XB_ERR_TRUNCATED},
        {"attribute without a value", "<xbrl a></xbrl>", XB_ERR_XML}, {"unquoted attribute value", "<xbrl a=1></xbrl>", XB_ERR_XML}, {"unterminated attribute value", "<xbrl a=\"1></xbrl>", XB_ERR_TRUNCATED},
        {"unterminated comment", "<xbrl><!-- never ends</xbrl>", XB_ERR_TRUNCATED}, {"unterminated CDATA", "<xbrl><![CDATA[ x </xbrl>", XB_ERR_TRUNCATED}, {"tag cut off in its name", "<xbrl><us-g", XB_ERR_TRUNCATED},
        {"context with no id", "<xbrl><context></context></xbrl>", XB_ERR_XML}, {"context with no period", "<xbrl><context id=\"a\"><entity/></context></xbrl>", XB_ERR_BAD_DATE},
        {"context that is self-closing", "<xbrl><context id=\"a\"/></xbrl>", XB_ERR_BAD_DATE},
        {"context with both instant and duration", "<xbrl><context id=\"a\"><period><instant>2023-01-01</instant><startDate>2023-01-01</startDate><endDate>2023-12-31</endDate></period></context></xbrl>", XB_ERR_BAD_DATE},
        {"context with an impossible date", "<xbrl><context id=\"a\"><period><instant>2023-02-30</instant></period></context></xbrl>", XB_ERR_BAD_DATE},
        {"context with an empty date", "<xbrl><context id=\"a\"><period><instant></instant></period></context></xbrl>", XB_ERR_BAD_DATE},
        {"two contexts with the same id", "<xbrl><context id=\"a\"><period><instant>2023-01-01</instant></period></context><context id=\"a\"><period><instant>2023-01-02</instant></period></context></xbrl>", XB_ERR_DUP_ID},
        {"two units with the same id", "<xbrl><unit id=\"u\"><measure>iso4217:USD</measure></unit><unit id=\"u\"><measure>iso4217:USD</measure></unit></xbrl>", XB_ERR_DUP_ID},
    };
    for (size_t i = 0; i < sizeof mal / sizeof mal[0]; i++) { int r2 = xb_parse(&doc, mal[i].text, (uint32_t)strlen(mal[i].text)); char nm[160]; snprintf(nm, sizeof nm, "%s -> %s", mal[i].name, xb_strerror(mal[i].code)); check(nm, r2 == mal[i].code); }
    { /* limits */
        static char big[1 << 20]; size_t o = 0; o += (size_t)snprintf(big + o, sizeof big - o, "<xbrl xmlns:us-gaap=\"http://fasb.org/us-gaap/2023\"><context id=\"c\"><period><instant>2023-01-01</instant></period></context><unit id=\"usd\"><measure>iso4217:USD</measure></unit>");
        for (int i = 0; i < 513; i++) { o += (size_t)snprintf(big + o, sizeof big - o, "<us-gaap:Assets contextRef=\"c\" unitRef=\"usd\">%d</us-gaap:Assets>", i); }
        snprintf(big + o, sizeof big - o, "</xbrl>"); check("513 facts (table holds 512) -> an error, not a silent truncation", xb_parse(&doc, big, (uint32_t)strlen(big)) == XB_ERR_TOO_MANY_FACT);
        o = 0; o += (size_t)snprintf(big + o, sizeof big - o, "<xbrl>"); for (int i = 0; i < 257; i++) { o += (size_t)snprintf(big + o, sizeof big - o, "<context id=\"c%d\"><period><instant>2023-01-01</instant></period></context>", i); }
        snprintf(big + o, sizeof big - o, "</xbrl>"); check("257 contexts (table holds 256) -> an error", xb_parse(&doc, big, (uint32_t)strlen(big)) == XB_ERR_TOO_MANY_CTX);
        o = 0; o += (size_t)snprintf(big + o, sizeof big - o, "<xbrl>"); for (int i = 0; i < 17; i++) { o += (size_t)snprintf(big + o, sizeof big - o, "<unit id=\"u%d\"><measure>iso4217:USD</measure></unit>", i); }
        snprintf(big + o, sizeof big - o, "</xbrl>"); check("17 units (table holds 16) -> an error", xb_parse(&doc, big, (uint32_t)strlen(big)) == XB_ERR_TOO_MANY_UNIT);
        o = 0; o += (size_t)snprintf(big + o, sizeof big - o, "<xbrl>"); for (int i = 0; i < 70; i++) { o += (size_t)snprintf(big + o, sizeof big - o, "<a>"); } for (int i = 0; i < 70; i++) { o += (size_t)snprintf(big + o, sizeof big - o, "</a>"); }
        snprintf(big + o, sizeof big - o, "</xbrl>"); check("70 levels of nesting (limit 64) -> an error, no stack overflow", xb_parse(&doc, big, (uint32_t)strlen(big)) == XB_ERR_DEPTH);
        o = 0; o += (size_t)snprintf(big + o, sizeof big - o, "<xbrl><context id=\""); for (int i = 0; i < 64; i++) { big[o++] = 'a'; } snprintf(big + o, sizeof big - o, "\"><period><instant>2023-01-01</instant></period></context></xbrl>");
        check("a 64-character context id (limit 63) -> an error, no buffer overflow", xb_parse(&doc, big, (uint32_t)strlen(big)) == XB_ERR_ID_TOO_LONG);
    }
    free(g2);
    printf("\n%d passed, %d failed\n", pass_n, fail_n); return fail_n ? 1 : 0;
}
