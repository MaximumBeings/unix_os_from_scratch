/* Chapter 48: the XBRL instance reader (see 052_xbrl.h). Single pass over the text with an explicit stack of open element names; no allocation. Freestanding: no string.h. */
#include "052_xbrl.h"

typedef struct { const char *p; uint32_t n; } sv_t;
static sv_t sv_make(const char *p, uint32_t n) { sv_t s; s.p = p; s.n = n; return s; }
static uint32_t cstr_len(const char *s) { uint32_t n = 0; while (s[n]) { n++; } return n; }
static int sv_is(sv_t a, const char *lit) {
    uint32_t n = cstr_len(lit);
    if (a.n != n) { return 0; }
    for (uint32_t i = 0; i < n; i++) { if (a.p[i] != lit[i]) { return 0; } }
    return 1;
}
static int sv_eq(sv_t a, sv_t b) {
    if (a.n != b.n) { return 0; }
    for (uint32_t i = 0; i < a.n; i++) { if (a.p[i] != b.p[i]) { return 0; } }
    return 1;
}
static int is_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
static sv_t sv_trim(sv_t s) {
    while (s.n && is_space(s.p[0])) { s.p++; s.n--; }
    while (s.n && is_space(s.p[s.n - 1])) { s.n--; }
    return s;
}
static sv_t sv_local(sv_t name) { /* the part after the last ':' */
    for (uint32_t i = name.n; i > 0; i--) { if (name.p[i - 1] == ':') { return sv_make(name.p + i, name.n - i); } }
    return name;
}
static sv_t sv_prefix(sv_t name, int *has) { /* the part before the first ':' */
    for (uint32_t i = 0; i < name.n; i++) { if (name.p[i] == ':') { *has = 1; return sv_make(name.p, i); } }
    *has = 0; return sv_make(name.p, 0);
}
static int sv_copy_id(char *dst, sv_t s) { /* 0 ok, -1 too long */
    if (s.n >= XB_ID_LEN) { return -1; }
    for (uint32_t i = 0; i < s.n; i++) { dst[i] = s.p[i]; }
    dst[s.n] = 0; return 0;
}
/* 64-bit unsigned division by shift-and-subtract: this kernel links without libgcc, so a plain '/' or '%' on a 64-bit value would fail with an undefined __udivdi3 / __umoddi3
 * (the same real failure Chapter 7 and the Chapter 47 modules hit). 64 iterations; exact; d must not be 0 (0 returns 0 with rem = n). */
uint64_t xb_udivmod(uint64_t n, uint64_t dv, uint64_t *rem) {
    if (dv == 0) { *rem = n; return 0; }
    uint64_t q = 0, r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1u);
        if (r >= dv) { r -= dv; q |= 1ull << i; }
    }
    *rem = r; return q;
}
static int str_eq(const char *a, const char *b) { uint32_t i = 0; while (a[i] && a[i] == b[i]) { i++; } return a[i] == b[i]; }

const char *xb_strerror(int rc) {
    switch (rc) {
    case XB_OK: return "ok";
    case XB_ERR_XML: return "malformed XML";
    case XB_ERR_TRUNCATED: return "document ends inside an element";
    case XB_ERR_TOO_MANY_CTX: return "more contexts than the table holds";
    case XB_ERR_TOO_MANY_FACT: return "more facts than the table holds";
    case XB_ERR_TOO_MANY_UNIT: return "more units than the table holds";
    case XB_ERR_BAD_DATE: return "a context has a missing or invalid date";
    case XB_ERR_ID_TOO_LONG: return "an identifier is longer than the table allows";
    case XB_ERR_DEPTH: return "elements nested too deeply";
    case XB_ERR_NO_PERIOD: return "no dei:DocumentPeriodEndDate in a dimension-free context";
    case XB_ERR_NOT_XBRL: return "root element is not <xbrl>";
    case XB_ERR_DUP_ID: return "two contexts or two units share an id";
    }
    return "unknown error";
}

int32_t xb_days_from_civil(int y, int m, int dd) { /* Howard Hinnant's algorithm */
    y -= m <= 2;
    int32_t era = (y >= 0 ? y : y - 399) / 400;
    int32_t yoe = y - era * 400;
    int32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + dd - 1;
    int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
int xb_parse_date(const char *s, uint32_t n, int32_t *days_out) {
    if (n != 10 || s[4] != '-' || s[7] != '-') { return -1; }
    int v[3] = {0, 0, 0}; const int from[3] = {0, 5, 8}, len[3] = {4, 2, 2};
    for (int k = 0; k < 3; k++) {
        for (int i = 0; i < len[k]; i++) {
            char c = s[from[k] + i];
            if (c < '0' || c > '9') { return -1; }
            v[k] = v[k] * 10 + (c - '0');
        }
    }
    int y = v[0], m = v[1], dd = v[2];
    if (y < 1 || m < 1 || m > 12 || dd < 1) { return -1; }
    int leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    static const int dim[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int maxd = dim[m - 1] + (m == 2 && leap ? 1 : 0);
    if (dd > maxd) { return -1; }
    *days_out = xb_days_from_civil(y, m, dd);
    return 0;
}
int xb_parse_scaled(const char *s, uint32_t n, int64_t *out) {
    sv_t t = sv_trim(sv_make(s, n)); uint32_t i = 0; int neg = 0;
    if (t.n && t.p[0] == '-') { neg = 1; i = 1; }
    uint64_t whole = 0; uint32_t wd = 0;
    while (i < t.n && t.p[i] >= '0' && t.p[i] <= '9') {
        if (whole > 900000000000000ull) { return -1; } /* far beyond any filing: reject rather than overflow */
        whole = whole * 10 + (uint64_t)(t.p[i] - '0'); i++; wd++;
    }
    if (wd == 0) { return -1; }
    uint32_t frac = 0, fd = 0;
    if (i < t.n && t.p[i] == '.') {
        i++;
        while (i < t.n && t.p[i] >= '0' && t.p[i] <= '9') {
            if (fd == 4) { return -1; }
            frac = frac * 10 + (uint32_t)(t.p[i] - '0'); i++; fd++;
        }
        if (fd == 0) { return -1; }
    }
    if (i != t.n) { return -1; }
    while (fd < 4) { frac *= 10; fd++; }
    int64_t v = (int64_t)(whole * 10000ull + frac);
    *out = neg ? -v : v;
    return 0;
}

static const struct { const char *name; uint8_t id; } GAAP[] = {
    {"Assets", XB_C_ASSETS}, {"AssetsCurrent", XB_C_ASSETS_CURRENT}, {"Liabilities", XB_C_LIABILITIES}, {"LiabilitiesCurrent", XB_C_LIABILITIES_CURRENT},
    {"LiabilitiesAndStockholdersEquity", XB_C_LIAB_AND_EQUITY}, {"StockholdersEquity", XB_C_EQUITY},
    {"StockholdersEquityIncludingPortionAttributableToNoncontrollingInterest", XB_C_EQUITY_INCL_NCI}, {"CashAndCashEquivalentsAtCarryingValue", XB_C_CASH},
    {"MarketableSecuritiesCurrent", XB_C_MKT_SEC_CURRENT}, {"ShortTermInvestments", XB_C_ST_INVEST}, {"AccountsReceivableNetCurrent", XB_C_RECEIVABLES},
    {"RevenueFromContractWithCustomerExcludingAssessedTax", XB_C_REV_CONTRACT}, {"Revenues", XB_C_REVENUES}, {"SalesRevenueNet", XB_C_SALES_NET},
    {"CostOfRevenue", XB_C_COST_OF_REVENUE}, {"CostOfGoodsAndServicesSold", XB_C_COGS}, {"GrossProfit", XB_C_GROSS_PROFIT}, {"OperatingIncomeLoss", XB_C_OPERATING_INCOME},
    {"InterestExpense", XB_C_INTEREST_EXPENSE}, {"InterestExpenseNonoperating", XB_C_INTEREST_NONOP}, {"NetIncomeLoss", XB_C_NET_INCOME}, {"EarningsPerShareDiluted", XB_C_EPS_DILUTED},
    {"NetCashProvidedByUsedInOperatingActivities", XB_C_OCF}, {"PaymentsToAcquirePropertyPlantAndEquipment", XB_C_CAPEX_PPE}, {"PaymentsToAcquireProductiveAssets", XB_C_CAPEX_PRODUCTIVE},
};
static const struct { const char *name; uint8_t id; } DEI[] = {
    {"DocumentType", XB_DEI_TYPE}, {"DocumentPeriodEndDate", XB_DEI_PERIOD_END}, {"EntityRegistrantName", XB_DEI_NAME}, {"EntityCentralIndexKey", XB_DEI_CIK},
};
#define N_GAAP ((int)(sizeof(GAAP) / sizeof(GAAP[0])))
#define N_DEI ((int)(sizeof(DEI) / sizeof(DEI[0])))

/* element text -> a plain C string, trimmed, with the five predefined XML entities decoded (cut at XB_TEXT_LEN - 1) */
static void decode_text(char *dst, sv_t s) {
    s = sv_trim(s); uint32_t o = 0;
    for (uint32_t i = 0; i < s.n && o < XB_TEXT_LEN - 1;) {
        if (s.p[i] == '&') {
            static const struct { const char *e; char c; } ENT[] = {{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}};
            int hit = 0;
            for (int k = 0; k < 5; k++) {
                uint32_t el = cstr_len(ENT[k].e);
                if (i + el <= s.n && sv_is(sv_make(s.p + i, el), ENT[k].e)) { dst[o++] = ENT[k].c; i += el; hit = 1; break; }
            }
            if (hit) { continue; }
        }
        dst[o++] = s.p[i++];
    }
    dst[o] = 0;
}

typedef struct { sv_t name, value; } attr_t;
#define MAX_ATTR 24
#define MAX_DEPTH 64
enum { CAP_NONE = 0, CAP_START, CAP_END, CAP_INSTANT, CAP_MEASURE, CAP_FACT, CAP_DEI };

typedef struct {
    sv_t gaap_prefix[4], dei_prefix[4]; int n_gaap, n_dei;
    int cur_ctx, cur_unit; uint8_t ctx_has_start, ctx_has_end, ctx_has_inst;
    int in_divide, in_num, in_den, direct_n, direct_usd, num_n, num_usd, den_n, den_ok;
} pstate_t;

static int attr_find(const attr_t *a, int n, const char *name, sv_t *out) {
    for (int i = 0; i < n; i++) { if (sv_is(a[i].name, name)) { *out = a[i].value; return 1; } }
    return 0;
}

int xb_parse(xb_doc_t *d, const char *text, uint32_t len) {
    d->n_ctx = 0; d->n_fact = 0; d->n_unit = 0; d->n_dei = 0; d->n_elements = 0; d->n_skipped_facts = 0; d->n_orphans = 0;
    pstate_t ps; ps.n_gaap = 0; ps.n_dei = 0; ps.cur_ctx = -1; ps.cur_unit = -1; ps.ctx_has_start = ps.ctx_has_end = ps.ctx_has_inst = 0;
    ps.in_divide = ps.in_num = ps.in_den = ps.direct_n = ps.direct_usd = ps.num_n = ps.num_usd = ps.den_n = ps.den_ok = 0;
    sv_t stack[MAX_DEPTH]; int depth = 0; int seen_root = 0, root_closed = 0;
    int cap = CAP_NONE, cap_depth = 0; uint32_t cap_start = 0; int cap_target = 0;  /* cap_target: the fact/dei index being filled */
    uint32_t pos = 0;
    while (pos < len) {
        if (text[pos] != '<') { pos++; continue; }
        if (pos + 1 >= len) { return XB_ERR_TRUNCATED; }
        char c1 = text[pos + 1];
        if (c1 == '?') { /* processing instruction / xml declaration */
            uint32_t q = pos + 2; while (q + 1 < len && !(text[q] == '?' && text[q + 1] == '>')) { q++; }
            if (q + 1 >= len) { return XB_ERR_TRUNCATED; }
            pos = q + 2; continue;
        }
        if (c1 == '!') {
            if (pos + 3 < len && text[pos + 2] == '-' && text[pos + 3] == '-') { /* comment */
                uint32_t q = pos + 4; while (q + 2 < len && !(text[q] == '-' && text[q + 1] == '-' && text[q + 2] == '>')) { q++; }
                if (q + 2 >= len) { return XB_ERR_TRUNCATED; }
                pos = q + 3; continue;
            }
            if (pos + 8 < len && text[pos + 2] == '[' && text[pos + 3] == 'C') { /* CDATA: skipped (no fact we read lives in one) */
                uint32_t q = pos + 9; while (q + 2 < len && !(text[q] == ']' && text[q + 1] == ']' && text[q + 2] == '>')) { q++; }
                if (q + 2 >= len) { return XB_ERR_TRUNCATED; }
                pos = q + 3; continue;
            }
            uint32_t q = pos + 2; while (q < len && text[q] != '>') { q++; } /* <!DOCTYPE ...> */
            if (q >= len) { return XB_ERR_TRUNCATED; }
            pos = q + 1; continue;
        }
        if (c1 == '/') { /* closing tag */
            uint32_t q = pos + 2; while (q < len && text[q] != '>') { q++; }
            if (q >= len) { return XB_ERR_TRUNCATED; }
            sv_t name = sv_trim(sv_make(text + pos + 2, q - (pos + 2)));
            if (depth == 0 || !sv_eq(name, stack[depth - 1])) { return XB_ERR_XML; }
            if (cap != CAP_NONE && cap_depth == depth) { /* text of the element being closed: apply it */
                sv_t t = sv_make(text + cap_start, pos - cap_start);
                if (cap == CAP_START || cap == CAP_END || cap == CAP_INSTANT) {
                    sv_t tt = sv_trim(t); int32_t day;
                    if (xb_parse_date(tt.p, tt.n, &day) != 0) { return XB_ERR_BAD_DATE; }
                    xb_ctx_t *cx = &d->ctx[ps.cur_ctx];
                    if (cap == CAP_START) { cx->start = day; ps.ctx_has_start = 1; }
                    else if (cap == CAP_END) { cx->end = day; ps.ctx_has_end = 1; }
                    else { cx->start = cx->end = day; ps.ctx_has_inst = 1; }
                } else if (cap == CAP_MEASURE) {
                    sv_t tt = sv_trim(t);
                    if (!ps.in_divide) { ps.direct_n++; ps.direct_usd = sv_is(tt, "iso4217:USD"); }
                    else if (ps.in_num) { ps.num_n++; ps.num_usd = sv_is(tt, "iso4217:USD"); }
                    else if (ps.in_den) { ps.den_n++; ps.den_ok = sv_is(tt, "xbrli:shares") || sv_is(tt, "shares"); }
                } else if (cap == CAP_FACT) {
                    sv_t tt = sv_trim(t); int64_t v;
                    if (xb_parse_scaled(tt.p, tt.n, &v) == 0) { d->fact[cap_target].val = v; }
                    else { d->fact[cap_target].concept = XB_C_NONE; d->n_skipped_facts++; } /* not a plain number: dropped at the end */
                } else if (cap == CAP_DEI) {
                    decode_text(d->dei[cap_target].text, t);
                }
                cap = CAP_NONE;
            } else if (cap != CAP_NONE && cap_depth > depth) { cap = CAP_NONE; }
            sv_t loc = sv_local(name);
            if (ps.cur_ctx >= 0 && depth == 2 && sv_is(loc, "context")) {
                xb_ctx_t *cx = &d->ctx[ps.cur_ctx];
                if (ps.ctx_has_inst && !ps.ctx_has_start && !ps.ctx_has_end) { cx->kind = XB_P_INSTANT; }
                else if (ps.ctx_has_start && ps.ctx_has_end && !ps.ctx_has_inst) { cx->kind = XB_P_DURATION; }
                else { return XB_ERR_BAD_DATE; }
                ps.cur_ctx = -1;
            } else if (ps.cur_unit >= 0 && depth == 2 && sv_is(loc, "unit")) {
                xb_unit_t *u = &d->unit[ps.cur_unit]; u->kind = XB_UNIT_NONE;
                if (ps.direct_n == 1 && ps.direct_usd && !ps.in_divide && ps.num_n == 0 && ps.den_n == 0) { u->kind = XB_UNIT_USD; }
                else if (ps.direct_n == 0 && ps.num_n == 1 && ps.num_usd && ps.den_n == 1 && ps.den_ok) { u->kind = XB_UNIT_USD_PER_SHARE; }
                ps.cur_unit = -1;
            } else if (ps.cur_unit >= 0 && sv_is(loc, "divide")) { ps.in_divide = 0; }
            else if (ps.cur_unit >= 0 && sv_is(loc, "unitNumerator")) { ps.in_num = 0; }
            else if (ps.cur_unit >= 0 && sv_is(loc, "unitDenominator")) { ps.in_den = 0; }
            depth--; if (depth == 0) { root_closed = 1; }
            pos = q + 1; continue;
        }
        /* opening tag */
        if (root_closed) { pos++; continue; } /* trailing junk after the root element is ignored */
        uint32_t q = pos + 1; while (q < len && !is_space(text[q]) && text[q] != '>' && text[q] != '/') { q++; }
        if (q >= len) { return XB_ERR_TRUNCATED; }
        sv_t name = sv_make(text + pos + 1, q - (pos + 1));
        if (name.n == 0) { return XB_ERR_XML; }
        attr_t at[MAX_ATTR]; int na = 0; int self_close = 0;
        for (;;) {
            while (q < len && is_space(text[q])) { q++; }
            if (q >= len) { return XB_ERR_TRUNCATED; }
            if (text[q] == '>') { q++; break; }
            if (text[q] == '/') { if (q + 1 >= len) { return XB_ERR_TRUNCATED; } if (text[q + 1] != '>') { return XB_ERR_XML; } self_close = 1; q += 2; break; }
            uint32_t ns = q; while (q < len && !is_space(text[q]) && text[q] != '=' && text[q] != '>' && text[q] != '/') { q++; }
            if (q >= len) { return XB_ERR_TRUNCATED; }
            sv_t an = sv_make(text + ns, q - ns);
            while (q < len && is_space(text[q])) { q++; }
            if (q >= len) { return XB_ERR_TRUNCATED; }
            if (an.n == 0 || text[q] != '=') { return XB_ERR_XML; }
            q++;
            while (q < len && is_space(text[q])) { q++; }
            if (q >= len) { return XB_ERR_TRUNCATED; }
            char quote = text[q]; if (quote != '"' && quote != '\'') { return XB_ERR_XML; }
            q++; uint32_t vs = q; while (q < len && text[q] != quote) { q++; }
            if (q >= len) { return XB_ERR_TRUNCATED; }
            if (na < MAX_ATTR) { at[na].name = an; at[na].value = sv_make(text + vs, q - vs); na++; }
            q++;
        }
        d->n_elements++;
        if (depth >= MAX_DEPTH) { return XB_ERR_DEPTH; }
        stack[depth++] = name;
        if (cap != CAP_NONE) { cap = CAP_NONE; } /* a child element inside a captured element: the text is not a plain value */
        sv_t loc = sv_local(name);
        if (depth == 1) {
            seen_root = 1;
            if (!sv_is(loc, "xbrl")) { return XB_ERR_NOT_XBRL; }
            for (int i = 0; i < na; i++) {
                if (at[i].name.n > 6 && at[i].name.p[0] == 'x' && at[i].name.p[1] == 'm' && at[i].name.p[2] == 'l' && at[i].name.p[3] == 'n' && at[i].name.p[4] == 's' && at[i].name.p[5] == ':') {
                    sv_t pre = sv_make(at[i].name.p + 6, at[i].name.n - 6);
                    if (at[i].value.n >= 24 && sv_is(sv_make(at[i].value.p, 24), "http://fasb.org/us-gaap/") && ps.n_gaap < 4) { ps.gaap_prefix[ps.n_gaap++] = pre; }
                    if (at[i].value.n >= 24 && sv_is(sv_make(at[i].value.p, 24), "http://xbrl.sec.gov/dei/") && ps.n_dei < 4) { ps.dei_prefix[ps.n_dei++] = pre; }
                }
            }
        } else if (depth == 2 && sv_is(loc, "context")) {
            sv_t id; if (!attr_find(at, na, "id", &id)) { return XB_ERR_XML; }
            if (d->n_ctx >= XB_MAX_CTX) { return XB_ERR_TOO_MANY_CTX; }
            xb_ctx_t *cx = &d->ctx[d->n_ctx]; if (sv_copy_id(cx->id, id) != 0) { return XB_ERR_ID_TOO_LONG; }
            for (uint16_t k = 0; k < d->n_ctx; k++) { if (str_eq(d->ctx[k].id, cx->id)) { return XB_ERR_DUP_ID; } } /* ids are unique in XBRL; a second one would silently redirect facts */
            cx->dim = 0; cx->kind = XB_P_NONE; cx->start = cx->end = 0; ps.cur_ctx = d->n_ctx++;
            ps.ctx_has_start = ps.ctx_has_end = ps.ctx_has_inst = 0;
        } else if (depth == 2 && sv_is(loc, "unit")) {
            sv_t id; if (!attr_find(at, na, "id", &id)) { return XB_ERR_XML; }
            if (d->n_unit >= XB_MAX_UNIT) { return XB_ERR_TOO_MANY_UNIT; }
            xb_unit_t *u = &d->unit[d->n_unit]; if (sv_copy_id(u->id, id) != 0) { return XB_ERR_ID_TOO_LONG; }
            for (uint16_t k = 0; k < d->n_unit; k++) { if (str_eq(d->unit[k].id, u->id)) { return XB_ERR_DUP_ID; } }
            u->kind = XB_UNIT_NONE; ps.cur_unit = d->n_unit++;
            ps.in_divide = ps.in_num = ps.in_den = ps.direct_n = ps.direct_usd = ps.num_n = ps.num_usd = ps.den_n = ps.den_ok = 0;
        } else if (ps.cur_ctx >= 0 && sv_is(loc, "segment")) { d->ctx[ps.cur_ctx].dim = 1; }
        else if (ps.cur_ctx >= 0 && !self_close && (sv_is(loc, "startDate") || sv_is(loc, "endDate") || sv_is(loc, "instant"))) {
            cap = sv_is(loc, "startDate") ? CAP_START : sv_is(loc, "endDate") ? CAP_END : CAP_INSTANT; cap_depth = depth; cap_start = q;
        } else if (ps.cur_ctx >= 0 && self_close && (sv_is(loc, "startDate") || sv_is(loc, "endDate") || sv_is(loc, "instant"))) { return XB_ERR_BAD_DATE; }
        else if (ps.cur_unit >= 0 && sv_is(loc, "divide")) { ps.in_divide = 1; }
        else if (ps.cur_unit >= 0 && sv_is(loc, "unitNumerator")) { ps.in_num = 1; ps.in_den = 0; }
        else if (ps.cur_unit >= 0 && sv_is(loc, "unitDenominator")) { ps.in_den = 1; ps.in_num = 0; }
        else if (ps.cur_unit >= 0 && sv_is(loc, "measure") && !self_close) { cap = CAP_MEASURE; cap_depth = depth; cap_start = q; }
        else if (depth == 2) {
            sv_t cref, uref, nil; int has_ctx = attr_find(at, na, "contextRef", &cref);
            if (has_ctx) {
                int has_pre; sv_t pre = sv_prefix(name, &has_pre); int is_gaap = 0, is_dei = 0;
                if (has_pre) {
                    for (int i = 0; i < ps.n_gaap; i++) { if (sv_eq(pre, ps.gaap_prefix[i])) { is_gaap = 1; } }
                    for (int i = 0; i < ps.n_dei; i++) { if (sv_eq(pre, ps.dei_prefix[i])) { is_dei = 1; } }
                }
                int is_nil = attr_find(at, na, "xsi:nil", &nil) && sv_is(nil, "true");
                if (is_gaap) {
                    int cid = XB_C_NONE;
                    for (int i = 0; i < N_GAAP; i++) { if (sv_is(loc, GAAP[i].name)) { cid = GAAP[i].id; break; } }
                    if (cid != XB_C_NONE && !is_nil) {
                        if (d->n_fact >= XB_MAX_FACT) { return XB_ERR_TOO_MANY_FACT; }
                        xb_fact_t *f = &d->fact[d->n_fact];
                        if (sv_copy_id(f->ctx, cref) != 0) { return XB_ERR_ID_TOO_LONG; }
                        f->unit_id[0] = 0;
                        if (attr_find(at, na, "unitRef", &uref)) { if (sv_copy_id(f->unit_id, uref) != 0) { return XB_ERR_ID_TOO_LONG; } }
                        f->concept = (uint8_t)cid; f->unit = XB_UNIT_NONE; f->val = 0; f->ctx_idx = -1;
                        if (!self_close) { cap = CAP_FACT; cap_depth = depth; cap_start = q; cap_target = d->n_fact; }
                        else { f->concept = XB_C_NONE; d->n_skipped_facts++; }
                        d->n_fact++;
                    }
                } else if (is_dei && !is_nil && !self_close && d->n_dei < XB_MAX_DEI) {
                    int did = -1; for (int i = 0; i < N_DEI; i++) { if (sv_is(loc, DEI[i].name)) { did = DEI[i].id; break; } }
                    if (did >= 0) {
                        xb_dei_t *e = &d->dei[d->n_dei]; if (sv_copy_id(e->ctx, cref) != 0) { return XB_ERR_ID_TOO_LONG; }
                        e->which = (uint8_t)did; e->text[0] = 0; e->ctx_idx = -1;
                        cap = CAP_DEI; cap_depth = depth; cap_start = q; cap_target = d->n_dei; d->n_dei++;
                    }
                }
            }
        }
        if (self_close) {
            if (depth == 2 && ps.cur_ctx >= 0 && sv_is(loc, "context")) { return XB_ERR_BAD_DATE; } /* an empty <context/> has no period */
            if (depth == 2 && ps.cur_unit >= 0 && sv_is(loc, "unit")) { d->unit[ps.cur_unit].kind = XB_UNIT_NONE; ps.cur_unit = -1; }
            depth--; if (depth == 0) { root_closed = 1; } if (cap != CAP_NONE && cap_depth > depth) { cap = CAP_NONE; } }
        pos = q;
    }
    if (!seen_root) { return XB_ERR_NOT_XBRL; }
    if (depth != 0) { return XB_ERR_TRUNCATED; }
    /* resolve references (contexts and units may legally come after the facts that use them) and drop facts we cannot use */
    uint16_t out = 0;
    for (uint16_t i = 0; i < d->n_fact; i++) {
        xb_fact_t *f = &d->fact[i];
        if (f->concept == XB_C_NONE) { continue; }
        int ci = -1; for (uint16_t k = 0; k < d->n_ctx; k++) { if (str_eq(d->ctx[k].id, f->ctx)) { ci = k; break; } }
        if (ci < 0) { d->n_orphans++; continue; }
        int uk = XB_UNIT_NONE; for (uint16_t k = 0; k < d->n_unit; k++) { if (str_eq(d->unit[k].id, f->unit_id)) { uk = d->unit[k].kind; break; } }
        int64_t v = f->val;
        if (uk == XB_UNIT_USD) {
            uint64_t mag = v < 0 ? (uint64_t)(-v) : (uint64_t)v, rem; uint64_t q = xb_udivmod(mag, 10000u, &rem);
            if (rem != 0) { d->n_skipped_facts++; continue; }
            v = v < 0 ? -(int64_t)q : (int64_t)q;
        }
        else if (uk != XB_UNIT_USD_PER_SHARE) { d->n_skipped_facts++; continue; }
        xb_fact_t *o = &d->fact[out++];
        if (o != f) { /* compact in place, field by field: a struct assignment may compile to a memcpy call, and this kernel has no libc */
            for (int b = 0; b < XB_ID_LEN; b++) { o->ctx[b] = f->ctx[b]; o->unit_id[b] = f->unit_id[b]; }
            o->concept = f->concept;
        }
        o->ctx_idx = (int16_t)ci; o->unit = (uint8_t)uk; o->val = v;
    }
    d->n_fact = out;
    for (uint16_t i = 0; i < d->n_dei; i++) {
        d->dei[i].ctx_idx = -1;
        for (uint16_t k = 0; k < d->n_ctx; k++) { if (str_eq(d->ctx[k].id, d->dei[i].ctx)) { d->dei[i].ctx_idx = (int16_t)k; break; } }
    }
    return XB_OK;
}
const char *xb_dei_text(const xb_doc_t *d, int which) {
    for (uint16_t i = 0; i < d->n_dei; i++) {
        if (d->dei[i].which == which && d->dei[i].ctx_idx >= 0 && !d->ctx[d->dei[i].ctx_idx].dim) { return d->dei[i].text; }
    }
    return 0;
}
