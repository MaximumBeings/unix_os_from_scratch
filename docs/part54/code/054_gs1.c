/* See 054_gs1.h's own top-of-file comment for the citation of every
 * real rule used here, read directly out of GS1 AISBL's own official
 * gs1-syntax-engine repository. */

#include "054_gs1.h"

static int is_digit(uint8_t c) {
    return c >= (uint8_t) '0' && c <= (uint8_t) '9';
}

/* GS1's own real lint_csum.c, reproduced field-for-field: the real
 * 13-byte span (12 real digits here, plus the check digit this
 * function is computing) is odd in length, so the real weight starts
 * at 1 and alternates 1,3,1,3,... via the real "4 - weight" toggle,
 * summing each digit's own real value times its own real weight. */
uint8_t gs1_checksum(const uint8_t digits[12]) {
    int weight = 1; /* GDTI's own real 13-byte span is odd: weight starts at 1 */
    int parity = 0;
    for (uint32_t i = 0; i < 12u; i++) {
        if (!is_digit(digits[i])) {
            return 0;
        }
        parity += weight * (int)(digits[i] - (uint8_t) '0');
        weight = 4 - weight;
    }
    int check = (10 - parity % 10) % 10;
    return (uint8_t)('0' + check);
}

int gs1_gdti_valid(const uint8_t gdti[GS1_GDTI_LEN]) {
    for (uint32_t i = 0; i < GS1_GDTI_LEN; i++) {
        if (!is_digit(gdti[i])) {
            return 0;
        }
    }
    uint8_t expect = gs1_checksum(gdti);
    return expect == gdti[GS1_GDTI_LEN - 1u];
}

static int is_printable(uint8_t c) {
    return c >= 0x20u && c <= 0x7Eu;
}

uint32_t gs1_build_element_string(const gs1_ticket_id_t *t, uint8_t *out, uint32_t out_size) {
    if (!gs1_gdti_valid(t->gdti) || t->serial_len > GS1_SERIAL_MAX) {
        return 0;
    }
    uint32_t needed = 5u + GS1_GDTI_LEN + t->serial_len; /* "(253)" + 13 digits + serial */
    if (needed > out_size) {
        return 0;
    }
    uint32_t pos = 0;
    out[pos++] = '(';
    out[pos++] = '2';
    out[pos++] = '5';
    out[pos++] = '3';
    out[pos++] = ')';
    for (uint32_t i = 0; i < GS1_GDTI_LEN; i++) {
        out[pos++] = t->gdti[i];
    }
    for (uint32_t i = 0; i < t->serial_len; i++) {
        if (!is_printable(t->serial[i])) {
            return 0;
        }
        out[pos++] = t->serial[i];
    }
    return pos;
}

int gs1_parse_element_string(const uint8_t *buf, uint32_t len, gs1_ticket_id_t *out) {
    uint8_t *raw = (uint8_t *) out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }
    if (len < 5u + GS1_GDTI_LEN) {
        return 0;
    }
    if (buf[0] != (uint8_t) '(' || buf[1] != (uint8_t) '2' || buf[2] != (uint8_t) '5' ||
        buf[3] != (uint8_t) '3' || buf[4] != (uint8_t) ')') {
        return 0;
    }
    for (uint32_t i = 0; i < GS1_GDTI_LEN; i++) {
        out->gdti[i] = buf[5u + i];
    }
    if (!gs1_gdti_valid(out->gdti)) {
        return 0;
    }
    uint32_t serial_len = len - (5u + GS1_GDTI_LEN);
    if (serial_len > GS1_SERIAL_MAX) {
        return 0;
    }
    for (uint32_t i = 0; i < serial_len; i++) {
        uint8_t c = buf[5u + GS1_GDTI_LEN + i];
        if (!is_printable(c)) {
            return 0;
        }
        out->serial[i] = c;
    }
    out->serial_len = serial_len;
    return 1;
}
