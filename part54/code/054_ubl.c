/* See 054_ubl.h's own top-of-file comment for the citation of every
 * real element name used here. */

#include "054_ubl.h"

static uint32_t append_str(uint8_t *out, uint32_t pos, uint32_t out_size, const char *s) {
    uint32_t i = 0;
    while (s[i] != '\0') {
        if (pos + i >= out_size) {
            return 0xFFFFFFFFu;
        }
        out[pos + i] = (uint8_t) s[i];
        i++;
    }
    return pos + i;
}

static uint32_t append_bytes(uint8_t *out, uint32_t pos, uint32_t out_size, const uint8_t *s,
                              uint32_t len) {
    if (pos + len > out_size) {
        return 0xFFFFFFFFu;
    }
    for (uint32_t i = 0; i < len; i++) {
        out[pos + i] = s[i];
    }
    return pos + len;
}

/* Real amounts here are always non-negative cents, printed as a real
 * decimal with exactly 2 digits after the point (e.g. 292000 cents ->
 * "2920.00"), matching both real example files' own real decimal
 * amount style. */
static uint32_t append_amount(uint8_t *out, uint32_t pos, uint32_t out_size, uint32_t cents) {
    uint32_t whole = cents / 100u;
    uint32_t frac = cents % 100u;
    uint8_t digits[10];
    uint32_t n = 0;
    if (whole == 0u) {
        digits[n++] = '0';
    } else {
        while (whole > 0u && n < 10u) {
            digits[n++] = (uint8_t)('0' + (whole % 10u));
            whole /= 10u;
        }
    }
    if (pos + n + 3u > out_size) {
        return 0xFFFFFFFFu;
    }
    for (uint32_t i = 0; i < n; i++) {
        out[pos + i] = digits[n - 1u - i];
    }
    pos += n;
    out[pos++] = '.';
    out[pos++] = (uint8_t)('0' + (frac / 10u));
    out[pos++] = (uint8_t)('0' + (frac % 10u));
    return pos;
}

#define APPEND(s) do { pos = append_str(out, pos, out_size, s); if (pos == 0xFFFFFFFFu) return 0; } while (0)
#define APPEND_B(b, n) do { pos = append_bytes(out, pos, out_size, b, n); if (pos == 0xFFFFFFFFu) return 0; } while (0)
#define APPEND_AMT(c) do { pos = append_amount(out, pos, out_size, c); if (pos == 0xFFFFFFFFu) return 0; } while (0)

uint32_t ubl_build_invoice(const ubl_invoice_t *inv, uint8_t *out, uint32_t out_size) {
    if (inv->line_count > UBL_MAX_LINES) {
        return 0;
    }
    uint32_t pos = 0;
    APPEND("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    APPEND("<Invoice xmlns=\"urn:oasis:names:specification:ubl:schema:xsd:Invoice-2\" "
           "xmlns:cac=\"urn:oasis:names:specification:ubl:schema:xsd:"
           "CommonAggregateComponents-2\" xmlns:cbc=\"urn:oasis:names:specification:ubl:"
           "schema:xsd:CommonBasicComponents-2\">\n");
    APPEND("<cbc:ID>");
    APPEND_B(inv->id, inv->id_len);
    APPEND("</cbc:ID>\n<cbc:IssueDate>");
    APPEND_B(inv->issue_date, 10u);
    APPEND("</cbc:IssueDate>\n<cac:InvoicePeriod><cbc:StartDate>");
    APPEND_B(inv->period_start, 10u);
    APPEND("</cbc:StartDate><cbc:EndDate>");
    APPEND_B(inv->period_end, 10u);
    APPEND("</cbc:EndDate></cac:InvoicePeriod>\n");
    APPEND("<cac:AccountingSupplierParty><cac:Party><cac:PartyName><cbc:Name>");
    APPEND_B(inv->supplier_name, inv->supplier_name_len);
    APPEND("</cbc:Name></cac:PartyName></cac:Party></cac:AccountingSupplierParty>\n");
    APPEND("<cac:AccountingCustomerParty><cac:Party><cac:PartyName><cbc:Name>");
    APPEND_B(inv->customer_name, inv->customer_name_len);
    APPEND("</cbc:Name></cac:PartyName></cac:Party></cac:AccountingCustomerParty>\n");
    APPEND("<cac:PaymentMeans><cbc:PaymentDueDate>");
    APPEND_B(inv->payment_due_date, 10u);
    APPEND("</cbc:PaymentDueDate></cac:PaymentMeans>\n");
    APPEND("<cac:TaxTotal><cbc:TaxAmount currencyID=\"USD\">");
    APPEND_AMT(inv->tax_amount_cents);
    APPEND("</cbc:TaxAmount></cac:TaxTotal>\n");
    APPEND("<cac:LegalMonetaryTotal><cbc:PayableAmount currencyID=\"USD\">");
    APPEND_AMT(inv->payable_amount_cents);
    APPEND("</cbc:PayableAmount></cac:LegalMonetaryTotal>\n");
    for (uint32_t i = 0; i < inv->line_count; i++) {
        const ubl_invoice_line_t *l = &inv->lines[i];
        APPEND("<cac:InvoiceLine><cbc:ID>");
        APPEND_B(l->id, l->id_len);
        APPEND("</cbc:ID><cbc:LineExtensionAmount currencyID=\"USD\">");
        APPEND_AMT(l->line_extension_cents);
        APPEND("</cbc:LineExtensionAmount><cac:Item><cbc:Description>");
        APPEND_B(l->description, l->description_len);
        APPEND("</cbc:Description></cac:Item></cac:InvoiceLine>\n");
    }
    APPEND("</Invoice>\n");
    return pos;
}

#undef APPEND
#undef APPEND_B
#undef APPEND_AMT

static int match_literal(const uint8_t *buf, uint32_t len, uint32_t *pos, const char *lit) {
    uint32_t i = 0;
    while (lit[i] != '\0') {
        if (*pos + i >= len || buf[*pos + i] != (uint8_t) lit[i]) {
            return 0;
        }
        i++;
    }
    *pos += i;
    return 1;
}

/* Reads real text up to (not including) the next '<', refusing if
 * that text would overflow `max` or if no '<' appears before `len`. */
static uint32_t read_text(const uint8_t *buf, uint32_t len, uint32_t *pos, uint8_t *out,
                          uint32_t max) {
    uint32_t n = 0;
    while (*pos < len && buf[*pos] != (uint8_t) '<') {
        if (n >= max) {
            return 0xFFFFFFFFu;
        }
        out[n++] = buf[*pos];
        (*pos)++;
    }
    return n;
}

static int is_digit(uint8_t c) {
    return c >= (uint8_t) '0' && c <= (uint8_t) '9';
}

/* Parses a real decimal amount of the exact shape append_amount()
 * writes (one or more digits, '.', exactly 2 more digits) into whole
 * cents. Refuses anything else outright. */
static int parse_amount(const uint8_t *text, uint32_t len, uint32_t *out_cents) {
    uint32_t i = 0;
    uint32_t whole = 0;
    while (i < len && is_digit(text[i])) {
        whole = whole * 10u + (uint32_t)(text[i] - (uint8_t) '0');
        i++;
    }
    if (i == 0u || i >= len || text[i] != (uint8_t) '.' || len - i != 3u ||
        !is_digit(text[i + 1u]) || !is_digit(text[i + 2u])) {
        return 0;
    }
    uint32_t frac = (uint32_t)(text[i + 1u] - (uint8_t) '0') * 10u +
                    (uint32_t)(text[i + 2u] - (uint8_t) '0');
    *out_cents = whole * 100u + frac;
    return 1;
}

int ubl_parse_invoice(const uint8_t *buf, uint32_t len, ubl_invoice_t *out) {
    uint8_t *raw = (uint8_t *) out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }
    uint32_t pos = 0;
    uint8_t text[UBL_MAX_NAME_LEN];
    uint32_t n;

    if (!match_literal(buf, len, &pos, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")) return 0;
    if (!match_literal(buf, len, &pos,
                        "<Invoice xmlns=\"urn:oasis:names:specification:ubl:schema:xsd:"
                        "Invoice-2\" xmlns:cac=\"urn:oasis:names:specification:ubl:schema:"
                        "xsd:CommonAggregateComponents-2\" xmlns:cbc=\"urn:oasis:names:"
                        "specification:ubl:schema:xsd:CommonBasicComponents-2\">\n")) return 0;

    if (!match_literal(buf, len, &pos, "<cbc:ID>")) return 0;
    n = read_text(buf, len, &pos, out->id, sizeof(out->id));
    if (n == 0xFFFFFFFFu) return 0;
    out->id_len = n;
    if (!match_literal(buf, len, &pos, "</cbc:ID>\n<cbc:IssueDate>")) return 0;
    n = read_text(buf, len, &pos, out->issue_date, 10u);
    if (n != 10u) return 0;
    if (!match_literal(buf, len, &pos,
                        "</cbc:IssueDate>\n<cac:InvoicePeriod><cbc:StartDate>")) return 0;
    n = read_text(buf, len, &pos, out->period_start, 10u);
    if (n != 10u) return 0;
    if (!match_literal(buf, len, &pos, "</cbc:StartDate><cbc:EndDate>")) return 0;
    n = read_text(buf, len, &pos, out->period_end, 10u);
    if (n != 10u) return 0;
    if (!match_literal(buf, len, &pos,
                        "</cbc:EndDate></cac:InvoicePeriod>\n"
                        "<cac:AccountingSupplierParty><cac:Party><cac:PartyName><cbc:Name>"))
        return 0;
    n = read_text(buf, len, &pos, out->supplier_name, UBL_MAX_NAME_LEN);
    if (n == 0xFFFFFFFFu) return 0;
    out->supplier_name_len = n;
    if (!match_literal(buf, len, &pos,
                        "</cbc:Name></cac:PartyName></cac:Party></cac:AccountingSupplierParty>\n"
                        "<cac:AccountingCustomerParty><cac:Party><cac:PartyName><cbc:Name>"))
        return 0;
    n = read_text(buf, len, &pos, out->customer_name, UBL_MAX_NAME_LEN);
    if (n == 0xFFFFFFFFu) return 0;
    out->customer_name_len = n;
    if (!match_literal(buf, len, &pos,
                        "</cbc:Name></cac:PartyName></cac:Party></cac:AccountingCustomerParty>\n"
                        "<cac:PaymentMeans><cbc:PaymentDueDate>")) return 0;
    n = read_text(buf, len, &pos, out->payment_due_date, 10u);
    if (n != 10u) return 0;
    if (!match_literal(buf, len, &pos,
                        "</cbc:PaymentDueDate></cac:PaymentMeans>\n"
                        "<cac:TaxTotal><cbc:TaxAmount currencyID=\"USD\">")) return 0;
    n = read_text(buf, len, &pos, text, sizeof(text));
    if (n == 0xFFFFFFFFu || !parse_amount(text, n, &out->tax_amount_cents)) return 0;
    if (!match_literal(buf, len, &pos,
                        "</cbc:TaxAmount></cac:TaxTotal>\n"
                        "<cac:LegalMonetaryTotal><cbc:PayableAmount currencyID=\"USD\">"))
        return 0;
    n = read_text(buf, len, &pos, text, sizeof(text));
    if (n == 0xFFFFFFFFu || !parse_amount(text, n, &out->payable_amount_cents)) return 0;
    if (!match_literal(buf, len, &pos,
                        "</cbc:PayableAmount></cac:LegalMonetaryTotal>\n")) return 0;

    uint32_t line_count = 0;
    while (match_literal(buf, len, &pos, "<cac:InvoiceLine><cbc:ID>")) {
        if (line_count >= UBL_MAX_LINES) {
            return 0;
        }
        ubl_invoice_line_t *l = &out->lines[line_count];
        n = read_text(buf, len, &pos, l->id, sizeof(l->id));
        if (n == 0xFFFFFFFFu) return 0;
        l->id_len = n;
        if (!match_literal(buf, len, &pos,
                            "</cbc:ID><cbc:LineExtensionAmount currencyID=\"USD\">")) return 0;
        n = read_text(buf, len, &pos, text, sizeof(text));
        if (n == 0xFFFFFFFFu || !parse_amount(text, n, &l->line_extension_cents)) return 0;
        if (!match_literal(buf, len, &pos,
                            "</cbc:LineExtensionAmount><cac:Item><cbc:Description>")) return 0;
        n = read_text(buf, len, &pos, l->description, UBL_MAX_DESC_LEN);
        if (n == 0xFFFFFFFFu) return 0;
        l->description_len = n;
        if (!match_literal(buf, len, &pos,
                            "</cbc:Description></cac:Item></cac:InvoiceLine>\n")) return 0;
        line_count++;
    }
    out->line_count = line_count;
    if (!match_literal(buf, len, &pos, "</Invoice>\n")) return 0;
    return pos == len;
}
