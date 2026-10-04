/* Chapter 49: the X12 reader (see 055_x12.h). Freestanding: no string.h, no libgcc division. */
#include "055_x12.h"

static int is_ws(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
static int is_alnum(char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
static uint32_t slen(const char *s) { uint32_t n = 0; while (s[n]) { n++; } return n; }
int x12_str_eq(const char *s, uint16_t n, const char *lit) {
    uint32_t m = slen(lit); if (n != m) { return 0; }
    for (uint32_t i = 0; i < m; i++) { if (s[i] != lit[i]) { return 0; } }
    return 1;
}
const char *x12_strerror(int rc) {
    switch (rc) {
    case X12_OK: return "ok";
    case X12_ERR_NOT_X12: return "the text does not start with an ISA segment";
    case X12_ERR_SHORT_ISA: return "the ISA segment is shorter than its fixed 106 characters";
    case X12_ERR_BAD_ISA: return "the ISA segment is not laid out as fixed-width fields";
    case X12_ERR_BAD_SEP: return "the separators announced in ISA are unusable";
    case X12_ERR_BAD_SEG: return "a segment identifier is not 2 or 3 upper-case letters or digits";
    case X12_ERR_EMPTY_SEG: return "an empty segment";
    case X12_ERR_BAD_CHAR: return "a control character inside a segment";
    case X12_ERR_TRUNCATED: return "the data ends inside a segment (no terminator)";
    case X12_ERR_TOO_MANY_SEG: return "more segments than the table holds";
    case X12_ERR_NESTING: return "a segment appears where the envelope structure does not allow it";
    case X12_ERR_MULTI_ISA: return "more than one interchange (ISA) in the file";
    case X12_ERR_COUNT_ST: return "SE01 does not equal the number of segments in the transaction set";
    case X12_ERR_CTL_ST: return "SE02 does not equal ST02";
    case X12_ERR_COUNT_GE: return "GE01 does not equal the number of transaction sets in the group";
    case X12_ERR_CTL_GS: return "GE02 does not equal GS06";
    case X12_ERR_COUNT_IEA: return "IEA01 does not equal the number of groups in the interchange";
    case X12_ERR_CTL_ISA: return "IEA02 does not equal ISA13";
    case X12_ERR_AFTER_IEA: return "data after the IEA segment";
    case X12_ERR_NO_IEA: return "the interchange does not end with IEA";
    }
    return "unknown error";
}
int x12_is(const x12_seg_t *s, const char *id) { return x12_str_eq(s->p, s->id_len, id); }
const char *x12_el(const x12_seg_t *s, int i, uint16_t *len) {
    if (i < 1 || i > s->n_el) { return 0; }
    /* the segment text is p[0..len); element separators are not stored in the segment, so find the i-th by scanning; the separator is whichever character follows the identifier */
    char sep = s->p[s->id_len]; uint32_t pos = (uint32_t)s->id_len + 1; int k = 1;
    while (k < i) { while (pos < s->len && s->p[pos] != sep) { pos++; } pos++; k++; }
    uint32_t start = pos; while (pos < s->len && s->p[pos] != sep) { pos++; }
    *len = (uint16_t)(pos - start); return s->p + start;
}
const char *x12_comp(const x12_t *x, const char *el, uint16_t el_len, int i, uint16_t *len) {
    uint32_t pos = 0; int k = 0;
    while (k < i) { while (pos < el_len && el[pos] != x->csep) { pos++; } if (pos >= el_len) { return 0; } pos++; k++; }
    uint32_t start = pos; while (pos < el_len && el[pos] != x->csep) { pos++; }
    *len = (uint16_t)(pos - start); return el + start;
}
int x12_uint(const char *s, uint16_t n, uint32_t *v) {
    if (n == 0 || n > 9) { return -1; }
    uint32_t a = 0; for (uint16_t i = 0; i < n; i++) { if (s[i] < '0' || s[i] > '9') { return -1; } a = a * 10 + (uint32_t)(s[i] - '0'); }
    *v = a; return 0;
}
int x12_money(const char *s, uint16_t n, int64_t *cents) {
    uint16_t i = 0; int neg = 0;
    if (n && s[0] == '-') { neg = 1; i = 1; }
    uint64_t whole = 0; int wd = 0;
    while (i < n && s[i] >= '0' && s[i] <= '9') { if (wd >= 12) { return -1; } whole = whole * 10 + (uint64_t)(s[i] - '0'); i++; wd++; }
    if (wd == 0) { return -1; }
    uint32_t frac = 0; int fd = 0;
    if (i < n && s[i] == '.') { i++; while (i < n && s[i] >= '0' && s[i] <= '9') { if (fd == 2) { return -1; } frac = frac * 10 + (uint32_t)(s[i] - '0'); i++; fd++; } if (fd == 0) { return -1; } }
    if (i != n) { return -1; }
    if (fd == 1) { frac *= 10; }
    int64_t v = (int64_t)(whole * 100u + frac); *cents = neg ? -v : v; return 0;
}
/* 64-bit unsigned division by shift-and-subtract: this kernel links without libgcc (see Chapter 48's note on __udivdi3) */
static uint64_t udm(uint64_t n, uint64_t dv, uint64_t *rem) {
    uint64_t q = 0, r = 0;
    for (int i = 63; i >= 0; i--) { r = (r << 1) | ((n >> i) & 1u); if (r >= dv) { r -= dv; q |= 1ull << i; } }
    *rem = r; return q;
}
void x12_money_str(int64_t cents, char *out) {
    uint32_t o = 0; uint64_t mag = (uint64_t)(cents < 0 ? -cents : cents);
    if (cents < 0) { out[o++] = '-'; }
    uint64_t frac, d10; uint64_t whole = udm(mag, 100u, &frac);
    char t[24]; int n = 0; if (whole == 0) { t[n++] = '0'; } while (whole) { uint64_t r; whole = udm(whole, 10u, &r); t[n++] = (char)('0' + (int)r); }
    while (n) { out[o++] = t[--n]; }
    if (frac) { uint64_t tens = udm(frac, 10u, &d10); out[o++] = '.'; out[o++] = (char)('0' + (int)tens); if (d10) { out[o++] = (char)('0' + (int)d10); } }
    out[o] = 0;
}
int x12_date(const char *s, uint16_t n, int32_t *days) {
    if (n != 8) { return -1; }
    int v[3] = {0, 0, 0}; const int from[3] = {0, 4, 6}, len[3] = {4, 2, 2};
    for (int k = 0; k < 3; k++) for (int i = 0; i < len[k]; i++) { char c = s[from[k] + i]; if (c < '0' || c > '9') { return -1; } v[k] = v[k] * 10 + (c - '0'); }
    int y = v[0], m = v[1], d = v[2]; if (y < 1 || m < 1 || m > 12 || d < 1) { return -1; }
    static const int dim[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; if (d > dim[m - 1] + (m == 2 && leap ? 1 : 0)) { return -1; }
    int yy = y - (m <= 2); int32_t era = (yy >= 0 ? yy : yy - 399) / 400; int32_t yoe = yy - era * 400; int32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    *days = era * 146097 + doe - 719468; return 0;
}

#define SOFT(code) do { if (x->lenient) { x->warn |= 1u << (-(code)); } else { x->err_seg = i; return (code); } } while (0)
static int sep_ok(char c) { return c >= 0x21 && c <= 0x7e && !is_alnum(c); }
int x12_parse(x12_t *x, const char *t, uint32_t len) {
    x->n_seg = 0; x->n_gs = 0; x->n_st = 0; x->err_seg = 0; x->warn = 0;
    uint32_t pos = 0; while (pos < len && is_ws(t[pos])) { pos++; }
    if (len - pos < 3 || t[pos] != 'I' || t[pos + 1] != 'S' || t[pos + 2] != 'A') { return X12_ERR_NOT_X12; }
    const char *isa = t + pos; uint32_t rest = len - pos;
    if (rest < 106) { return X12_ERR_SHORT_ISA; }
    char es = isa[3]; static const int SEPAT[16] = {3, 6, 17, 20, 31, 34, 50, 53, 69, 76, 81, 83, 89, 99, 101, 103};
    for (int k = 0; k < 16; k++) { if (isa[SEPAT[k]] != es) { return X12_ERR_BAD_ISA; } }
    x->esep = es; x->rsep = isa[82]; x->csep = isa[104]; x->term = isa[105];
    if (!sep_ok(x->esep) || !sep_ok(x->csep) || !sep_ok(x->term) || x->esep == x->csep || x->esep == x->term || x->csep == x->term) { return X12_ERR_BAD_SEP; }
    for (int k = 90; k <= 98; k++) { if (isa[k] < '0' || isa[k] > '9') { return X12_ERR_BAD_ISA; } } /* ISA13: nine digits */
    /* split into segments */
    pos += 0; uint32_t p = pos;
    for (;;) {
        while (p < len && is_ws(t[p])) { p++; }
        if (p >= len) { break; }
        uint32_t s = p; while (p < len && t[p] != x->term) { if ((unsigned char)t[p] < 0x20 && !is_ws(t[p])) { x->err_seg = x->n_seg; return X12_ERR_BAD_CHAR; } p++; }
        if (p >= len) { x->err_seg = x->n_seg; return X12_ERR_TRUNCATED; }
        uint32_t e = p; p++; /* e: terminator */
        if (e == s) { x->err_seg = x->n_seg; return X12_ERR_EMPTY_SEG; }
        uint32_t idl = 0; while (s + idl < e && t[s + idl] != x->esep) { idl++; }
        if (idl < 2 || idl > 3) { x->err_seg = x->n_seg; return X12_ERR_BAD_SEG; }
        for (uint32_t k = 0; k < idl; k++) { char c = t[s + k]; if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) { x->err_seg = x->n_seg; return X12_ERR_BAD_SEG; } }
        if (x->n_seg >= X12_MAX_SEG) { return X12_ERR_TOO_MANY_SEG; }
        uint8_t ne = 0; for (uint32_t k = s; k < e; k++) { if (t[k] == x->esep && ne < 255) { ne++; } }
        x12_seg_t *g = &x->seg[x->n_seg++]; g->p = t + s; g->len = (uint16_t)(e - s); g->id_len = (uint8_t)idl; g->n_el = ne;
        if (e - s > 65000) { return X12_ERR_BAD_SEG; }
    }
    /* envelope: ISA (GS (ST ... SE)+ GE)+ IEA, with control numbers and counts */
    uint16_t n = x->n_seg; uint16_t i = 0; uint16_t el; const char *a;
    if (n == 0 || !x12_is(&x->seg[0], "ISA")) { return X12_ERR_NOT_X12; }
    i = 1; uint32_t groups = 0;
    while (i < n && x12_is(&x->seg[i], "GS")) {
        const x12_seg_t *gs = &x->seg[i]; uint16_t gs_i = i; uint16_t gl; const char *g6 = x12_el(gs, 6, &gl); i++; uint32_t sets = 0;
        while (i < n && x12_is(&x->seg[i], "ST")) {
            const x12_seg_t *st = &x->seg[i]; uint16_t st_i = i; uint16_t sl; const char *s2 = x12_el(st, 2, &sl); i++;
            while (i < n && !x12_is(&x->seg[i], "SE")) {
                if (x12_is(&x->seg[i], "ST") || x12_is(&x->seg[i], "GS") || x12_is(&x->seg[i], "GE") || x12_is(&x->seg[i], "IEA")) { x->err_seg = i; return X12_ERR_NESTING; }
                if (x12_is(&x->seg[i], "ISA")) { x->err_seg = i; return X12_ERR_MULTI_ISA; }
                i++;
            }
            if (i >= n) { x->err_seg = st_i; return X12_ERR_NESTING; }
            const x12_seg_t *se = &x->seg[i]; uint32_t cnt; a = x12_el(se, 1, &el);
            if (!a || x12_uint(a, el, &cnt) != 0 || cnt != (uint32_t)(i - st_i + 1)) SOFT(X12_ERR_COUNT_ST);
            { uint16_t l2; const char *se2 = x12_el(se, 2, &l2); int same = s2 && se2 && l2 == sl; for (uint16_t k = 0; same && k < l2; k++) { if (se2[k] != s2[k]) { same = 0; } } if (!same) { SOFT(X12_ERR_CTL_ST); } }
            i++; sets++; x->n_st++;
        }
        if (i >= n) { x->err_seg = gs_i; return X12_ERR_NESTING; }
        if (x12_is(&x->seg[i], "ISA")) { x->err_seg = i; return X12_ERR_MULTI_ISA; }
        if (!x12_is(&x->seg[i], "GE")) { x->err_seg = i; return X12_ERR_NESTING; }
        const x12_seg_t *ge = &x->seg[i]; uint32_t c2; a = x12_el(ge, 1, &el);
        if (!a || x12_uint(a, el, &c2) != 0 || c2 != sets) SOFT(X12_ERR_COUNT_GE);
        { uint16_t l2; const char *ge2 = x12_el(ge, 2, &l2); int same = g6 && ge2 && l2 == gl; for (uint16_t k = 0; same && k < l2; k++) { if (ge2[k] != g6[k]) { same = 0; } } if (!same) { SOFT(X12_ERR_CTL_GS); } }
        i++; groups++; x->n_gs++;
    }
    if (i >= n) { return X12_ERR_NO_IEA; }
    if (x12_is(&x->seg[i], "ISA")) { x->err_seg = i; return X12_ERR_MULTI_ISA; }
    if (!x12_is(&x->seg[i], "IEA")) { x->err_seg = i; return x12_is(&x->seg[i], "GS") ? X12_ERR_NESTING : X12_ERR_NESTING; }
    { const x12_seg_t *iea = &x->seg[i]; uint32_t c3; a = x12_el(iea, 1, &el);
      if (!a || x12_uint(a, el, &c3) != 0 || c3 != groups) SOFT(X12_ERR_COUNT_IEA);
      uint16_t l2; const char *i2 = x12_el(iea, 2, &l2); int same = i2 && l2 == 9; for (int k = 0; same && k < 9; k++) { if (i2[k] != isa[90 + k]) { same = 0; } } if (!same) { SOFT(X12_ERR_CTL_ISA); } }
    i++; if (i != n) { x->err_seg = i; return X12_ERR_AFTER_IEA; }
    return X12_OK;
}
