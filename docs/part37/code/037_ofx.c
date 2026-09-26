/* See 037_ofx.h's own top-of-file comment for the full citation trail
 * (every element name, and the real SGML implicit-leaf-closing rule,
 * read directly out of a real, fetched OFX 1.02 sample) and this
 * module's own fixed, known-schema approach to it. */

#include "037_ofx.h"

static uint32_t str_len(const char *s) {
    uint32_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static int is_digit(uint8_t c) {
    return c >= (uint8_t) '0' && c <= (uint8_t) '9';
}

/* ---------------------------------------------------------------- */
/* Encoding                                                          */
/* ---------------------------------------------------------------- */

static int append_str(uint8_t *buf, uint32_t *pos, uint32_t size, const char *s) {
    uint32_t n = str_len(s);
    if (*pos + n > size) {
        return 0;
    }
    for (uint32_t i = 0; i < n; i++) {
        buf[*pos + i] = (uint8_t) s[i];
    }
    *pos += n;
    return 1;
}

static int append_open(uint8_t *buf, uint32_t *pos, uint32_t size, const char *tag) {
    return append_str(buf, pos, size, "<") && append_str(buf, pos, size, tag) &&
           append_str(buf, pos, size, ">");
}

static int append_close(uint8_t *buf, uint32_t *pos, uint32_t size, const char *tag) {
    return append_str(buf, pos, size, "</") && append_str(buf, pos, size, tag) &&
           append_str(buf, pos, size, ">");
}

/* A real OFX LEAF field: "<tag>value" with no closing tag at all --
 * the real SGML rule 037_ofx.h's own top comment cites. */
static int append_leaf(uint8_t *buf, uint32_t *pos, uint32_t size, const char *tag,
                       const char *value) {
    return append_open(buf, pos, size, tag) && append_str(buf, pos, size, value);
}

/* Converts a signed cents amount to OFX's own real decimal format
 * ("-34.51", "0.01") -- sign, whole dollars (no leading zeros beyond a
 * single "0"), '.', exactly two fractional digits. */
static int append_amount(uint8_t *buf, uint32_t *pos, uint32_t size, const char *tag,
                         int32_t cents) {
    char digits[16];
    uint32_t n = 0;
    uint32_t mag = (cents < 0) ? (uint32_t) (-cents) : (uint32_t) cents;
    if (cents < 0) {
        digits[n++] = '-';
    }
    uint32_t whole = mag / 100u;
    uint32_t frac = mag % 100u;
    if (whole == 0u) {
        digits[n++] = '0';
    } else {
        char rev[10];
        uint32_t rn = 0;
        while (whole > 0u) {
            rev[rn++] = (char) ('0' + (whole % 10u));
            whole /= 10u;
        }
        while (rn > 0u) {
            digits[n++] = rev[--rn];
        }
    }
    digits[n++] = '.';
    digits[n++] = (char) ('0' + (frac / 10u) % 10u);
    digits[n++] = (char) ('0' + frac % 10u);
    digits[n] = '\0';
    return append_leaf(buf, pos, size, tag, digits);
}

/* This chapter's own fixed time-of-day suffix -- only the date matters
 * to this chapter's own demo, so every real OFX datetime it emits is a
 * real 8-digit YYYYMMDD date plus a fixed "000000" time, per the real
 * DTSERVER/DTPOSTED/DTASOF datetime format cited in 037_ofx.h. */
static int append_datetime(uint8_t *buf, uint32_t *pos, uint32_t size, const char *tag,
                          const char *yyyymmdd) {
    return append_open(buf, pos, size, tag) && append_str(buf, pos, size, yyyymmdd) &&
           append_str(buf, pos, size, "000000");
}

uint32_t ofx_build_statement(const ofx_statement_t *s, uint8_t *out, uint32_t out_size) {
    if (s->transaction_count == 0u || s->transaction_count > OFX_MAX_TRANSACTIONS) {
        return 0;
    }
    uint32_t pos = 0;
    int ok = 1;

    /* The real OFX header block -- plain "KEY:VALUE" lines, cited
     * verbatim from the fetched sample, ending in a blank line before
     * the SGML body begins. */
    ok = ok && append_str(out, &pos, out_size, "OFXHEADER:100\n");
    ok = ok && append_str(out, &pos, out_size, "DATA:OFXSGML\n");
    ok = ok && append_str(out, &pos, out_size, "VERSION:102\n");
    ok = ok && append_str(out, &pos, out_size, "SECURITY:NONE\n");
    ok = ok && append_str(out, &pos, out_size, "ENCODING:USASCII\n");
    ok = ok && append_str(out, &pos, out_size, "CHARSET:1252\n");
    ok = ok && append_str(out, &pos, out_size, "COMPRESSION:NONE\n");
    ok = ok && append_str(out, &pos, out_size, "OLDFILEUID:NONE\n");
    ok = ok && append_str(out, &pos, out_size, "NEWFILEUID:NONE\n\n");

    ok = ok && append_open(out, &pos, out_size, "OFX");
    ok = ok && append_open(out, &pos, out_size, "SIGNONMSGSRSV1");
    ok = ok && append_open(out, &pos, out_size, "SONRS");
    ok = ok && append_open(out, &pos, out_size, "STATUS");
    ok = ok && append_leaf(out, &pos, out_size, "CODE", "0");
    ok = ok && append_leaf(out, &pos, out_size, "SEVERITY", "INFO");
    ok = ok && append_close(out, &pos, out_size, "STATUS");
    ok = ok && append_datetime(out, &pos, out_size, "DTSERVER", s->dtend);
    ok = ok && append_leaf(out, &pos, out_size, "LANGUAGE", "ENG");
    ok = ok && append_close(out, &pos, out_size, "SONRS");
    ok = ok && append_close(out, &pos, out_size, "SIGNONMSGSRSV1");

    ok = ok && append_open(out, &pos, out_size, "BANKMSGSRSV1");
    ok = ok && append_open(out, &pos, out_size, "STMTTRNRS");
    ok = ok && append_leaf(out, &pos, out_size, "TRNUID", "1");
    ok = ok && append_open(out, &pos, out_size, "STATUS");
    ok = ok && append_leaf(out, &pos, out_size, "CODE", "0");
    ok = ok && append_leaf(out, &pos, out_size, "SEVERITY", "INFO");
    ok = ok && append_close(out, &pos, out_size, "STATUS");
    ok = ok && append_open(out, &pos, out_size, "STMTRS");
    ok = ok && append_leaf(out, &pos, out_size, "CURDEF", "USD");
    ok = ok && append_open(out, &pos, out_size, "BANKACCTFROM");
    ok = ok && append_leaf(out, &pos, out_size, "BANKID", s->bank_id);
    ok = ok && append_leaf(out, &pos, out_size, "ACCTID", s->acct_id);
    ok = ok && append_leaf(out, &pos, out_size, "ACCTTYPE", "CHECKING");
    ok = ok && append_close(out, &pos, out_size, "BANKACCTFROM");

    ok = ok && append_open(out, &pos, out_size, "BANKTRANLIST");
    ok = ok && append_datetime(out, &pos, out_size, "DTSTART", s->dtstart);
    ok = ok && append_datetime(out, &pos, out_size, "DTEND", s->dtend);
    for (uint32_t i = 0; i < s->transaction_count; i++) {
        const ofx_transaction_t *t = &s->transactions[i];
        ok = ok && append_open(out, &pos, out_size, "STMTTRN");
        ok = ok && append_leaf(out, &pos, out_size, "TRNTYPE", t->trn_type);
        ok = ok && append_datetime(out, &pos, out_size, "DTPOSTED", t->dtposted);
        ok = ok && append_amount(out, &pos, out_size, "TRNAMT", t->amount_cents);
        ok = ok && append_leaf(out, &pos, out_size, "FITID", t->fitid);
        ok = ok && append_leaf(out, &pos, out_size, "NAME", t->name);
        ok = ok && append_close(out, &pos, out_size, "STMTTRN");
    }
    ok = ok && append_close(out, &pos, out_size, "BANKTRANLIST");

    ok = ok && append_open(out, &pos, out_size, "LEDGERBAL");
    ok = ok && append_amount(out, &pos, out_size, "BALAMT", s->ledger_balance_cents);
    ok = ok && append_datetime(out, &pos, out_size, "DTASOF", s->dtasof);
    ok = ok && append_close(out, &pos, out_size, "LEDGERBAL");

    ok = ok && append_close(out, &pos, out_size, "STMTRS");
    ok = ok && append_close(out, &pos, out_size, "STMTTRNRS");
    ok = ok && append_close(out, &pos, out_size, "BANKMSGSRSV1");
    ok = ok && append_close(out, &pos, out_size, "OFX");
    return ok ? pos : 0u;
}

/* ---------------------------------------------------------------- */
/* Decoding                                                          */
/* ---------------------------------------------------------------- */

static int matches_at(const uint8_t *buf, uint32_t pos, uint32_t end, const char *s) {
    uint32_t n = str_len(s);
    if (pos + n > end) {
        return 0;
    }
    for (uint32_t i = 0; i < n; i++) {
        if (buf[pos + i] != (uint8_t) s[i]) {
            return 0;
        }
    }
    return 1;
}

/* Finds the literal opening tag "<tag>" at or after `start`, within
 * `end`, and returns the position right after it (where that tag's
 * own content begins). Refuses (returns 0) if not found. */
static int find_open(const uint8_t *buf, uint32_t start, uint32_t end, const char *tag,
                     uint32_t *out_content_start) {
    char open_tag[40];
    open_tag[0] = '<';
    uint32_t tn = str_len(tag);
    for (uint32_t i = 0; i < tn; i++) {
        open_tag[1 + i] = tag[i];
    }
    open_tag[1 + tn] = '>';
    open_tag[2 + tn] = '\0';
    uint32_t open_len = tn + 2u;

    for (uint32_t pos = start; pos + open_len <= end; pos++) {
        if (matches_at(buf, pos, end, open_tag)) {
            *out_content_start = pos + open_len;
            return 1;
        }
    }
    return 0;
}

/* A real OFX AGGREGATE: opens with find_open(), then its own content
 * runs until a literal "</tag>", which this function also consumes.
 * Mirrors 034_acord.c's own acord_find(), restricted the same way (no
 * same-name self-nesting in this chapter's own fixed schema). */
static int find_aggregate(const uint8_t *buf, uint32_t start, uint32_t end, const char *tag,
                          uint32_t *out_content_start, uint32_t *out_content_end,
                          uint32_t *out_after) {
    uint32_t content_start;
    if (!find_open(buf, start, end, tag, &content_start)) {
        return 0;
    }
    char close_tag[40];
    close_tag[0] = '<';
    close_tag[1] = '/';
    uint32_t tn = str_len(tag);
    for (uint32_t i = 0; i < tn; i++) {
        close_tag[2 + i] = tag[i];
    }
    close_tag[2 + tn] = '>';
    close_tag[3 + tn] = '\0';
    uint32_t close_len = tn + 3u;

    for (uint32_t pos = content_start; pos + close_len <= end; pos++) {
        if (matches_at(buf, pos, end, close_tag)) {
            *out_content_start = content_start;
            *out_content_end = pos;
            *out_after = pos + close_len;
            return 1;
        }
    }
    return 0;
}

/* A real OFX LEAF: opens with find_open(), then its own real, unclosed
 * value runs until the next literal '<' byte (the real SGML
 * implicit-close rule 037_ofx.h cites) -- never a matching close tag,
 * because a genuine OFX leaf never has one. */
static int find_leaf(const uint8_t *buf, uint32_t start, uint32_t end, const char *tag,
                     uint32_t *out_val_start, uint32_t *out_val_end, uint32_t *out_after) {
    uint32_t content_start;
    if (!find_open(buf, start, end, tag, &content_start)) {
        return 0;
    }
    uint32_t pos = content_start;
    while (pos < end && buf[pos] != (uint8_t) '<') {
        pos++;
    }
    *out_val_start = content_start;
    *out_val_end = pos;
    *out_after = pos;
    return 1;
}

static int copy_text_span(const uint8_t *buf, uint32_t start, uint32_t end, char *dst,
                          uint32_t dst_size) {
    uint32_t n = end - start;
    if (n + 1u > dst_size) {
        return 0;
    }
    for (uint32_t i = 0; i < n; i++) {
        dst[i] = (char) buf[start + i];
    }
    dst[n] = '\0';
    return 1;
}

/* Copies exactly the first 8 bytes of a real OFX datetime value (the
 * YYYYMMDD date; this chapter never inspects the time-of-day past it)
 * into `dst` (9 bytes). Refuses if those 8 bytes are not all real
 * ASCII digits, or the span is shorter than 8 bytes. */
static int copy_date8(const uint8_t *buf, uint32_t start, uint32_t end, char *dst) {
    if (end - start < 8u) {
        return 0;
    }
    for (uint32_t i = 0; i < 8u; i++) {
        if (!is_digit(buf[start + i])) {
            return 0;
        }
        dst[i] = (char) buf[start + i];
    }
    dst[8] = '\0';
    return 1;
}

/* Parses a real OFX amount span ("-34.51", "0.01") into signed cents.
 * Refuses on anything that does not match exactly that shape. */
static int parse_amount(const uint8_t *buf, uint32_t start, uint32_t end, int32_t *out_cents) {
    uint32_t pos = start;
    int negative = 0;
    if (pos < end && buf[pos] == (uint8_t) '-') {
        negative = 1;
        pos++;
    }
    uint32_t whole = 0;
    uint32_t whole_digits = 0;
    while (pos < end && is_digit(buf[pos])) {
        whole = whole * 10u + (uint32_t) (buf[pos] - (uint8_t) '0');
        pos++;
        whole_digits++;
    }
    if (whole_digits == 0u || pos >= end || buf[pos] != (uint8_t) '.') {
        return 0;
    }
    pos++;
    if (end - pos != 2u || !is_digit(buf[pos]) || !is_digit(buf[pos + 1u])) {
        return 0;
    }
    uint32_t frac = (uint32_t) (buf[pos] - (uint8_t) '0') * 10u +
                    (uint32_t) (buf[pos + 1u] - (uint8_t) '0');
    int32_t magnitude = (int32_t) (whole * 100u + frac);
    *out_cents = negative ? -magnitude : magnitude;
    return 1;
}

int ofx_parse_statement(const uint8_t *buf, uint32_t len, ofx_statement_t *out) {
    uint8_t *raw = (uint8_t *) out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }

    /* The real header block, refused outright if any stated line does
     * not match exactly -- this chapter's own encoder never varies
     * it, so a mismatch means a genuinely different or corrupted
     * message, not a real variant this decoder should tolerate. */
    static const char header[] =
        "OFXHEADER:100\nDATA:OFXSGML\nVERSION:102\nSECURITY:NONE\nENCODING:USASCII\n"
        "CHARSET:1252\nCOMPRESSION:NONE\nOLDFILEUID:NONE\nNEWFILEUID:NONE\n\n";
    uint32_t header_len = str_len(header);
    if (!matches_at(buf, 0, len, header)) {
        return 0;
    }
    uint32_t body_start = header_len;

    uint32_t ofx_cs, ofx_ce, ofx_after;
    if (!find_aggregate(buf, body_start, len, "OFX", &ofx_cs, &ofx_ce, &ofx_after) ||
        ofx_after != len) {
        return 0; /* trailing bytes after </OFX> are refused, not ignored */
    }

    uint32_t sign_cs, sign_ce, sign_after;
    if (!find_aggregate(buf, ofx_cs, ofx_ce, "SIGNONMSGSRSV1", &sign_cs, &sign_ce, &sign_after)) {
        return 0;
    }
    uint32_t sonrs_cs, sonrs_ce, sonrs_after;
    if (!find_aggregate(buf, sign_cs, sign_ce, "SONRS", &sonrs_cs, &sonrs_ce, &sonrs_after)) {
        return 0;
    }
    {
        uint32_t st_cs, st_ce, st_after;
        if (!find_aggregate(buf, sonrs_cs, sonrs_ce, "STATUS", &st_cs, &st_ce, &st_after)) {
            return 0;
        }
        uint32_t v_s, v_e, v_a;
        if (!find_leaf(buf, st_cs, st_ce, "CODE", &v_s, &v_e, &v_a) ||
            !matches_at(buf, v_s, v_e, "0") || v_e - v_s != 1u) {
            return 0; /* CODE must be exactly "0" -- a real success status */
        }
    }

    uint32_t bank_cs, bank_ce, bank_after;
    if (!find_aggregate(buf, ofx_cs, ofx_ce, "BANKMSGSRSV1", &bank_cs, &bank_ce, &bank_after)) {
        return 0;
    }
    uint32_t rs_cs, rs_ce, rs_after;
    if (!find_aggregate(buf, bank_cs, bank_ce, "STMTTRNRS", &rs_cs, &rs_ce, &rs_after)) {
        return 0;
    }
    uint32_t stmt_cs, stmt_ce, stmt_after;
    if (!find_aggregate(buf, rs_cs, rs_ce, "STMTRS", &stmt_cs, &stmt_ce, &stmt_after)) {
        return 0;
    }

    uint32_t acct_cs, acct_ce, acct_after;
    if (!find_aggregate(buf, stmt_cs, stmt_ce, "BANKACCTFROM", &acct_cs, &acct_ce, &acct_after)) {
        return 0;
    }
    {
        uint32_t v_s, v_e, v_a;
        if (!find_leaf(buf, acct_cs, acct_ce, "BANKID", &v_s, &v_e, &v_a) ||
            !copy_text_span(buf, v_s, v_e, out->bank_id, sizeof(out->bank_id))) {
            return 0;
        }
        if (!find_leaf(buf, acct_cs, acct_ce, "ACCTID", &v_s, &v_e, &v_a) ||
            !copy_text_span(buf, v_s, v_e, out->acct_id, sizeof(out->acct_id))) {
            return 0;
        }
        if (!find_leaf(buf, acct_cs, acct_ce, "ACCTTYPE", &v_s, &v_e, &v_a) ||
            !matches_at(buf, v_s, v_e, "CHECKING") || v_e - v_s != 8u) {
            return 0;
        }
    }

    uint32_t tl_cs, tl_ce, tl_after;
    if (!find_aggregate(buf, stmt_cs, stmt_ce, "BANKTRANLIST", &tl_cs, &tl_ce, &tl_after)) {
        return 0;
    }
    {
        uint32_t v_s, v_e, v_a;
        if (!find_leaf(buf, tl_cs, tl_ce, "DTSTART", &v_s, &v_e, &v_a) ||
            !copy_date8(buf, v_s, v_e, out->dtstart)) {
            return 0;
        }
        if (!find_leaf(buf, tl_cs, tl_ce, "DTEND", &v_s, &v_e, &v_a) ||
            !copy_date8(buf, v_s, v_e, out->dtend)) {
            return 0;
        }
    }

    uint32_t cursor = tl_cs;
    uint32_t count = 0;
    while (count < OFX_MAX_TRANSACTIONS) {
        uint32_t trn_cs, trn_ce, trn_after;
        if (!find_aggregate(buf, cursor, tl_ce, "STMTTRN", &trn_cs, &trn_ce, &trn_after)) {
            break;
        }
        ofx_transaction_t *t = &out->transactions[count];
        uint32_t v_s, v_e, v_a;
        if (!find_leaf(buf, trn_cs, trn_ce, "TRNTYPE", &v_s, &v_e, &v_a)) {
            return 0;
        }
        int is_debit = matches_at(buf, v_s, v_e, "DEBIT") && v_e - v_s == 5u;
        int is_credit = matches_at(buf, v_s, v_e, "CREDIT") && v_e - v_s == 6u;
        if (!is_debit && !is_credit) {
            return 0; /* only these two real TRNTYPE values, per this chapter's own scope */
        }
        if (!copy_text_span(buf, v_s, v_e, t->trn_type, sizeof(t->trn_type))) {
            return 0;
        }
        if (!find_leaf(buf, trn_cs, trn_ce, "DTPOSTED", &v_s, &v_e, &v_a) ||
            !copy_date8(buf, v_s, v_e, t->dtposted)) {
            return 0;
        }
        if (!find_leaf(buf, trn_cs, trn_ce, "TRNAMT", &v_s, &v_e, &v_a) ||
            !parse_amount(buf, v_s, v_e, &t->amount_cents)) {
            return 0;
        }
        /* A real, structural check, not merely a numeric one: a DEBIT
         * must be negative and a CREDIT positive, per this chapter's
         * own stated sign convention (037_ofx.h). */
        if ((is_debit && t->amount_cents >= 0) || (is_credit && t->amount_cents <= 0)) {
            return 0;
        }
        if (!find_leaf(buf, trn_cs, trn_ce, "FITID", &v_s, &v_e, &v_a) ||
            !copy_text_span(buf, v_s, v_e, t->fitid, sizeof(t->fitid))) {
            return 0;
        }
        if (!find_leaf(buf, trn_cs, trn_ce, "NAME", &v_s, &v_e, &v_a) ||
            !copy_text_span(buf, v_s, v_e, t->name, sizeof(t->name))) {
            return 0;
        }
        count++;
        cursor = trn_after;
    }
    if (count == 0u) {
        return 0;
    }
    out->transaction_count = count;

    uint32_t lb_cs, lb_ce, lb_after;
    if (!find_aggregate(buf, stmt_cs, stmt_ce, "LEDGERBAL", &lb_cs, &lb_ce, &lb_after)) {
        return 0;
    }
    {
        uint32_t v_s, v_e, v_a;
        if (!find_leaf(buf, lb_cs, lb_ce, "BALAMT", &v_s, &v_e, &v_a) ||
            !parse_amount(buf, v_s, v_e, &out->ledger_balance_cents)) {
            return 0;
        }
        if (!find_leaf(buf, lb_cs, lb_ce, "DTASOF", &v_s, &v_e, &v_a) ||
            !copy_date8(buf, v_s, v_e, out->dtasof)) {
            return 0;
        }
    }
    return 1;
}
