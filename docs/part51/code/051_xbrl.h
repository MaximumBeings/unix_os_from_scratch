/* Chapter 48: a small XBRL instance-document reader. SEC filings carry their numbers as XBRL: every figure is a "fact" element (<us-gaap:Assets contextRef="c-23" unitRef="usd" decimals="-6">
 * 352755000000</us-gaap:Assets>) that points at a <context> (WHICH period: a duration such as a fiscal year, or an instant such as a balance-sheet date; and whether it is the whole company
 * or a slice -- a <segment> with dimensions) and at a <unit> (which currency, or currency per share). This reader keeps only what the ratio engine reads, in fixed tables: no malloc, no libc,
 * no recursion. It is strict about structure (mismatched or unclosed tags are errors, tables that overflow are errors -- never a silent truncation) and tolerant about formatting (attributes in
 * any order, split over lines, single or double quotes, comments, processing instructions, CDATA). A document that is not XBRL, or is cut short, yields an error code and no facts. */
#ifndef XBRL_H
#define XBRL_H
#include <stdint.h>

#define XB_MAX_CTX 256
#define XB_MAX_FACT 512
#define XB_MAX_UNIT 16
#define XB_MAX_DEI 16
#define XB_ID_LEN 64
#define XB_TEXT_LEN 64

enum { XB_OK = 0, XB_ERR_XML = -1, XB_ERR_TRUNCATED = -2, XB_ERR_TOO_MANY_CTX = -3, XB_ERR_TOO_MANY_FACT = -4, XB_ERR_TOO_MANY_UNIT = -5, XB_ERR_BAD_DATE = -6, XB_ERR_ID_TOO_LONG = -7, XB_ERR_DEPTH = -8,
       XB_ERR_NO_PERIOD = -9, XB_ERR_NOT_XBRL = -10, XB_ERR_DUP_ID = -11 };

/* The us-gaap concepts the ratio engine reads. XB_C_NONE means "not one we read". */
enum {
    XB_C_NONE = 0, XB_C_ASSETS, XB_C_ASSETS_CURRENT, XB_C_LIABILITIES, XB_C_LIABILITIES_CURRENT, XB_C_LIAB_AND_EQUITY, XB_C_EQUITY, XB_C_EQUITY_INCL_NCI, XB_C_CASH, XB_C_MKT_SEC_CURRENT,
    XB_C_ST_INVEST, XB_C_RECEIVABLES, XB_C_REV_CONTRACT, XB_C_REVENUES, XB_C_SALES_NET, XB_C_COST_OF_REVENUE, XB_C_COGS, XB_C_GROSS_PROFIT, XB_C_OPERATING_INCOME, XB_C_INTEREST_EXPENSE,
    XB_C_INTEREST_NONOP, XB_C_NET_INCOME, XB_C_EPS_DILUTED, XB_C_OCF, XB_C_CAPEX_PPE, XB_C_CAPEX_PRODUCTIVE, XB_C_COUNT
};
enum { XB_UNIT_NONE = 0, XB_UNIT_USD = 1, XB_UNIT_USD_PER_SHARE = 2 };
enum { XB_P_NONE = 0, XB_P_DURATION = 1, XB_P_INSTANT = 2 };
enum { XB_DEI_TYPE = 0, XB_DEI_PERIOD_END, XB_DEI_NAME, XB_DEI_CIK };

typedef struct { char id[XB_ID_LEN]; uint8_t dim; /* has a <segment> */ uint8_t kind; int32_t start, end; /* days since 1970-01-01; instant: start == end */ } xb_ctx_t;
typedef struct { char ctx[XB_ID_LEN]; char unit_id[XB_ID_LEN]; int16_t ctx_idx; uint8_t concept, unit; int64_t val; /* dollars, or 1/10000 dollar for per-share */ } xb_fact_t;
typedef struct { char ctx[XB_ID_LEN]; int16_t ctx_idx; uint8_t which; char text[XB_TEXT_LEN]; } xb_dei_t;
typedef struct { char id[XB_ID_LEN]; uint8_t kind; } xb_unit_t;
typedef struct {
    uint16_t n_ctx, n_fact, n_unit, n_dei; xb_ctx_t ctx[XB_MAX_CTX]; xb_fact_t fact[XB_MAX_FACT]; xb_unit_t unit[XB_MAX_UNIT]; xb_dei_t dei[XB_MAX_DEI];
    uint32_t n_elements, n_skipped_facts, n_orphans; /* diagnostics: elements seen; facts dropped (bad unit, not a plain number, dimension-free requirement not met is NOT counted here); facts naming an unknown context */
} xb_doc_t;

uint64_t xb_udivmod(uint64_t n, uint64_t d, uint64_t *rem); /* freestanding 64-bit division (no libgcc) */
int xb_parse(xb_doc_t *d, const char *text, uint32_t len); /* XB_OK or a negative XB_ERR_* */
const char *xb_strerror(int rc);
int32_t xb_days_from_civil(int y, int m, int dd); /* proleptic Gregorian; days since 1970-01-01 */
int xb_parse_date(const char *s, uint32_t n, int32_t *days_out); /* strict YYYY-MM-DD, valid calendar date; 0 on success */
int xb_parse_scaled(const char *s, uint32_t n, int64_t *out); /* '-12.3400' -> -123400 (scaled by 10^4); at most 4 fraction digits, digits required on both sides of '.'; 0 on success */
const char *xb_dei_text(const xb_doc_t *d, int which); /* first dimension-free value, or 0 */
#endif
