/* Chapter 48: the filing table and formatting helpers. The accession numbers and URLs are the filings' own, exactly as the EDGAR filing metadata in the test fixtures records them. */
#include "054_filings.h"
extern const char filing_aapl_start[], filing_aapl_end[], filing_ko_start[], filing_ko_end[], filing_nvda_start[], filing_nvda_end[];
extern const char filing_msft_start[], filing_msft_end[], filing_xom_start[], filing_xom_end[], filing_jpm_start[], filing_jpm_end[];
#define U "https://www.sec.gov/Archives/edgar/data/"
const filing_t g_filings[N_FILINGS] = {
    {"AAPL", "Apple Inc.", "10-K", "2023-11-03", "0000320193-23-000106", U "320193/0000320193-23-000106-index.html", filing_aapl_start, filing_aapl_end},
    {"KO", "COCA COLA CO", "10-K", "2024-02-20", "0000021344-24-000009", U "21344/0000021344-24-000009-index.html", filing_ko_start, filing_ko_end},
    {"NVDA", "NVIDIA CORP", "10-K", "2026-02-25", "0001045810-26-000021", U "1045810/000104581026000021/0001045810-26-000021-index.html", filing_nvda_start, filing_nvda_end},
    {"MSFT", "MICROSOFT CORP", "10-K", "2024-07-30", "0000950170-24-087843", U "789019/0000950170-24-087843-index.html", filing_msft_start, filing_msft_end},
    {"XOM", "EXXON MOBIL CORP", "10-K", "2023-02-22", "0000034088-23-000020", U "34088/0000034088-23-000020-index.html", filing_xom_start, filing_xom_end},
    {"JPM", "JPMORGAN CHASE & CO", "10-K", "2024-02-16", "0000019617-24-000225", U "19617/0000019617-24-000225-index.html", filing_jpm_start, filing_jpm_end},
};

static int put_u(char *b, int o, uint64_t v, int min_digits) { /* decimal, at least min_digits digits (zero padded) */
    char t[24]; int n = 0; uint64_t r;
    while (v || n < min_digits) { v = xb_udivmod(v, 10, &r); t[n++] = (char)('0' + (int)r); }
    while (n) { b[o++] = t[--n]; }
    return o;
}
static int put_usd(char *b, int o, uint64_t v) { /* 99584000000 -> 99,584,000,000 */
    char t[40]; int n = 0, grp = 0; uint64_t r;
    if (v == 0) { t[n++] = '0'; }
    while (v) { if (grp == 3) { t[n++] = ','; grp = 0; } v = xb_udivmod(v, 10, &r); t[n++] = (char)('0' + (int)r); grp++; }
    while (n) { b[o++] = t[--n]; }
    return o;
}
int edgar_format(const rt_line_t *l, char *buf) {
    int o = 0; int64_t v = l->value; int neg = v < 0; uint64_t mag = neg ? (uint64_t)(-v) : (uint64_t)v, r, q;
    if (neg) { buf[o++] = '-'; }
    switch (l->unit) {
    case RT_U_BP: q = xb_udivmod(mag, 100, &r); o = put_u(buf, o, q, 1); buf[o++] = '.'; o = put_u(buf, o, r, 2); buf[o++] = '%'; break;
    case RT_U_X: q = xb_udivmod(mag, 100, &r); o = put_u(buf, o, q, 1); buf[o++] = '.'; o = put_u(buf, o, r, 2); buf[o++] = 'x'; break;
    case RT_U_USD: buf[o++] = '$'; o = put_usd(buf, o, mag); break;
    default: q = xb_udivmod(mag, 10000, &r); buf[o++] = '$'; o = put_u(buf, o, q, 1); buf[o++] = '.'; o = put_u(buf, o, r, 4); break;
    }
    buf[o] = 0; return o;
}
void edgar_date(int32_t days, char *buf) { /* inverse of xb_days_from_civil (Hinnant) */
    int32_t z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097, doe = z - era * 146097;
    int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365, y = yoe + era * 400, doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    int32_t d = doy - (153 * mp + 2) / 5 + 1, m = mp + (mp < 10 ? 3 : -9); y += (m <= 2);
    if (y < 1 || y > 9999) { for (int i = 0; i < 10; i++) { buf[i] = (i == 4 || i == 7) ? '-' : '?'; } buf[10] = 0; return; } /* the output buffer is 11 bytes: never write a 5-digit or negative year */
    int o = put_u(buf, 0, (uint64_t)y, 4); buf[o++] = '-'; o = put_u(buf, o, (uint64_t)m, 2); buf[o++] = '-'; o = put_u(buf, o, (uint64_t)d, 2); buf[o] = 0;
}
int32_t edgar_find(const char *hay, uint32_t n, const char *needle) {
    uint32_t m = 0; while (needle[m]) { m++; }
    for (uint32_t i = 0; i + m <= n; i++) { uint32_t k = 0; while (k < m && hay[i + k] == needle[k]) { k++; } if (k == m) { return (int32_t)i; } }
    return -1;
}
