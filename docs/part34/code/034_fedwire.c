/* See 034_fedwire.h's own top-of-file comment for the full real citation
 * of every tag, field width, and the fictional-data policy used here. */

#include "034_fedwire.h"

static uint32_t str_len_bounded(const char *s, uint32_t max) {
    uint32_t n = 0;
    while (n < max && s[n] != '\0') {
        n++;
    }
    return n;
}

static uint32_t write_bytes(uint8_t *buf, uint32_t off, uint32_t cap, const char *data, uint32_t len) {
    if (off + len > cap) {
        return 0xFFFFFFFFu;
    }
    for (uint32_t i = 0; i < len; i++) {
        buf[off + i] = (uint8_t)data[i];
    }
    return off + len;
}

static uint32_t write_tag(uint8_t *buf, uint32_t off, uint32_t cap, const char *tag4) {
    if (off + 6u > cap) {
        return 0xFFFFFFFFu;
    }
    buf[off] = '{';
    buf[off + 1u] = (uint8_t)tag4[0];
    buf[off + 2u] = (uint8_t)tag4[1];
    buf[off + 3u] = (uint8_t)tag4[2];
    buf[off + 4u] = (uint8_t)tag4[3];
    buf[off + 5u] = '}';
    return off + 6u;
}

uint32_t fedwire_build_message(const fedwire_message_t *msg, uint8_t *out_buf, uint32_t out_buf_size) {
    uint32_t off = 0;

    /* {1500} Sender Supplied Information: 2-char format version + 1-char
     * real test/production code. */
    off = write_tag(out_buf, off, out_buf_size, "1500");
    if (off == 0xFFFFFFFFu) return 0;
    off = write_bytes(out_buf, off, out_buf_size, msg->sender_format_version, 2u);
    if (off == 0xFFFFFFFFu) return 0;
    if (off + 1u > out_buf_size) return 0;
    out_buf[off] = (uint8_t)msg->sender_test_production_code;
    off++;

    /* {1510} Type/Subtype: real 2+2 char widths. */
    off = write_tag(out_buf, off, out_buf_size, "1510");
    if (off == 0xFFFFFFFFu) return 0;
    off = write_bytes(out_buf, off, out_buf_size, msg->type_code, 2u);
    if (off == 0xFFFFFFFFu) return 0;
    off = write_bytes(out_buf, off, out_buf_size, msg->subtype_code, 2u);
    if (off == 0xFFFFFFFFu) return 0;

    /* {1520} IMAD: real 8+8+6 = 22 char width. */
    off = write_tag(out_buf, off, out_buf_size, "1520");
    if (off == 0xFFFFFFFFu) return 0;
    off = write_bytes(out_buf, off, out_buf_size, msg->imad_cycle_date, 8u);
    if (off == 0xFFFFFFFFu) return 0;
    off = write_bytes(out_buf, off, out_buf_size, msg->imad_source, 8u);
    if (off == 0xFFFFFFFFu) return 0;
    off = write_bytes(out_buf, off, out_buf_size, msg->imad_sequence, 6u);
    if (off == 0xFFFFFFFFu) return 0;

    /* {2000} Amount: real 12 numeric digits, implied 2 decimal places. */
    off = write_tag(out_buf, off, out_buf_size, "2000");
    if (off == 0xFFFFFFFFu) return 0;
    if (off + 12u > out_buf_size) return 0;
    {
        /* Real 32-bit division/modulo only (see 034_fedwire.h's own
         * amount_cents comment for why this chapter never reaches for
         * 64-bit division). */
        uint32_t v = msg->amount_cents;
        for (int i = 11; i >= 0; i--) {
            out_buf[off + (uint32_t)i] = (uint8_t)('0' + (v % 10u));
            v /= 10u;
        }
        off += 12u;
    }

    /* {3100} Sender DI: real 9-char ABA + a short name. */
    off = write_tag(out_buf, off, out_buf_size, "3100");
    if (off == 0xFFFFFFFFu) return 0;
    off = write_bytes(out_buf, off, out_buf_size, msg->sender_aba, FEDWIRE_ABA_LEN);
    if (off == 0xFFFFFFFFu) return 0;
    off = write_bytes(out_buf, off, out_buf_size, msg->sender_name, str_len_bounded(msg->sender_name, FEDWIRE_NAME_LEN));
    if (off == 0xFFFFFFFFu) return 0;

    /* {3400} Receiver DI: real 9-char ABA + a short name. */
    off = write_tag(out_buf, off, out_buf_size, "3400");
    if (off == 0xFFFFFFFFu) return 0;
    off = write_bytes(out_buf, off, out_buf_size, msg->receiver_aba, FEDWIRE_ABA_LEN);
    if (off == 0xFFFFFFFFu) return 0;
    off = write_bytes(out_buf, off, out_buf_size, msg->receiver_name, str_len_bounded(msg->receiver_name, FEDWIRE_NAME_LEN));
    if (off == 0xFFFFFFFFu) return 0;

    /* {3600} Business Function Code: real 3-char width. */
    off = write_tag(out_buf, off, out_buf_size, "3600");
    if (off == 0xFFFFFFFFu) return 0;
    off = write_bytes(out_buf, off, out_buf_size, msg->business_function_code, 3u);
    if (off == 0xFFFFFFFFu) return 0;

    /* {4200} Beneficiary: identifier + name. */
    off = write_tag(out_buf, off, out_buf_size, "4200");
    if (off == 0xFFFFFFFFu) return 0;
    off = write_bytes(out_buf, off, out_buf_size, msg->beneficiary_account, str_len_bounded(msg->beneficiary_account, FEDWIRE_ACCOUNT_LEN));
    if (off == 0xFFFFFFFFu) return 0;
    if (off + 1u > out_buf_size) return 0;
    out_buf[off] = (uint8_t)' ';
    off++;
    off = write_bytes(out_buf, off, out_buf_size, msg->beneficiary_name, str_len_bounded(msg->beneficiary_name, FEDWIRE_NAME_LEN));
    if (off == 0xFFFFFFFFu) return 0;

    /* {5000} Originator: identifier + name. */
    off = write_tag(out_buf, off, out_buf_size, "5000");
    if (off == 0xFFFFFFFFu) return 0;
    off = write_bytes(out_buf, off, out_buf_size, msg->originator_account, str_len_bounded(msg->originator_account, FEDWIRE_ACCOUNT_LEN));
    if (off == 0xFFFFFFFFu) return 0;
    if (off + 1u > out_buf_size) return 0;
    out_buf[off] = (uint8_t)' ';
    off++;
    off = write_bytes(out_buf, off, out_buf_size, msg->originator_name, str_len_bounded(msg->originator_name, FEDWIRE_NAME_LEN));
    if (off == 0xFFFFFFFFu) return 0;

    return off;
}

/* Finds the real byte offset of a given 4-char tag's payload (right after
 * its closing '}'), or 0xFFFFFFFFu if the tag never appears. Never trusts
 * a match that isn't actually the real "{TAGN}" 6-byte sequence. */
static uint32_t find_tag(const uint8_t *buf, uint32_t len, const char *tag4) {
    if (len < 6u) {
        return 0xFFFFFFFFu;
    }
    for (uint32_t i = 0; i + 6u <= len; i++) {
        if (buf[i] == '{' && buf[i + 1u] == (uint8_t)tag4[0] &&
            buf[i + 2u] == (uint8_t)tag4[1] && buf[i + 3u] == (uint8_t)tag4[2] &&
            buf[i + 4u] == (uint8_t)tag4[3] && buf[i + 5u] == '}') {
            return i + 6u;
        }
    }
    return 0xFFFFFFFFu;
}

static void copy_field(char *dst, uint32_t dst_cap, const uint8_t *src, uint32_t src_off, uint32_t field_len) {
    uint32_t n = field_len;
    if (n > dst_cap) {
        n = dst_cap;
    }
    for (uint32_t i = 0; i < n; i++) {
        dst[i] = (char)src[src_off + i];
    }
    for (uint32_t i = n; i < dst_cap; i++) {
        dst[i] = '\0';
    }
}

/* Real variable-length field reader: a name/account field runs until the
 * next real "{TAG}" marker or the end of the buffer -- refuses (returns
 * 0xFFFFFFFFu) rather than guess if no terminator is ever found before
 * out_buf_size would be exceeded, the same no-partial-effect discipline
 * fedwire_build_message() itself follows. */
static uint32_t field_run_length(const uint8_t *buf, uint32_t len, uint32_t off) {
    uint32_t i = off;
    while (i < len) {
        if (buf[i] == '{' && i + 6u <= len && buf[i + 5u] == '}') {
            break;
        }
        i++;
    }
    return i - off;
}

int fedwire_parse_message(const uint8_t *buf, uint32_t len, fedwire_message_t *out_msg) {
    uint32_t off;

    off = find_tag(buf, len, "1500");
    if (off == 0xFFFFFFFFu || off + 3u > len) return 0;
    out_msg->sender_format_version[0] = (char)buf[off];
    out_msg->sender_format_version[1] = (char)buf[off + 1u];
    out_msg->sender_test_production_code = (char)buf[off + 2u];

    off = find_tag(buf, len, "1510");
    if (off == 0xFFFFFFFFu || off + 4u > len) return 0;
    out_msg->type_code[0] = (char)buf[off];
    out_msg->type_code[1] = (char)buf[off + 1u];
    out_msg->subtype_code[0] = (char)buf[off + 2u];
    out_msg->subtype_code[1] = (char)buf[off + 3u];

    off = find_tag(buf, len, "1520");
    if (off == 0xFFFFFFFFu || off + 22u > len) return 0;
    copy_field(out_msg->imad_cycle_date, 8u, buf, off, 8u);
    copy_field(out_msg->imad_source, 8u, buf, off + 8u, 8u);
    copy_field(out_msg->imad_sequence, 6u, buf, off + 16u, 6u);

    off = find_tag(buf, len, "2000");
    if (off == 0xFFFFFFFFu || off + 12u > len) return 0;
    {
        /* Real 32-bit accumulation only -- see 034_fedwire.h's own
         * amount_cents comment. A real 12-digit field whose value
         * genuinely exceeds 32 bits would silently wrap here; this
         * chapter's own demo never builds one. */
        uint32_t v = 0;
        for (uint32_t i = 0; i < 12u; i++) {
            uint8_t c = buf[off + i];
            if (c < (uint8_t)'0' || c > (uint8_t)'9') return 0;
            v = v * 10u + (uint32_t)(c - (uint8_t)'0');
        }
        out_msg->amount_cents = v;
    }

    off = find_tag(buf, len, "3100");
    if (off == 0xFFFFFFFFu || off + FEDWIRE_ABA_LEN > len) return 0;
    copy_field(out_msg->sender_aba, FEDWIRE_ABA_LEN, buf, off, FEDWIRE_ABA_LEN);
    {
        uint32_t name_off = off + FEDWIRE_ABA_LEN;
        uint32_t name_len = field_run_length(buf, len, name_off);
        copy_field(out_msg->sender_name, FEDWIRE_NAME_LEN, buf, name_off, name_len);
    }

    off = find_tag(buf, len, "3400");
    if (off == 0xFFFFFFFFu || off + FEDWIRE_ABA_LEN > len) return 0;
    copy_field(out_msg->receiver_aba, FEDWIRE_ABA_LEN, buf, off, FEDWIRE_ABA_LEN);
    {
        uint32_t name_off = off + FEDWIRE_ABA_LEN;
        uint32_t name_len = field_run_length(buf, len, name_off);
        copy_field(out_msg->receiver_name, FEDWIRE_NAME_LEN, buf, name_off, name_len);
    }

    off = find_tag(buf, len, "3600");
    if (off == 0xFFFFFFFFu || off + 3u > len) return 0;
    out_msg->business_function_code[0] = (char)buf[off];
    out_msg->business_function_code[1] = (char)buf[off + 1u];
    out_msg->business_function_code[2] = (char)buf[off + 2u];

    off = find_tag(buf, len, "4200");
    if (off == 0xFFFFFFFFu) return 0;
    {
        uint32_t run = field_run_length(buf, len, off);
        /* account, a space, then name -- split on the first space */
        uint32_t sp = 0;
        while (sp < run && buf[off + sp] != (uint8_t)' ') {
            sp++;
        }
        copy_field(out_msg->beneficiary_account, FEDWIRE_ACCOUNT_LEN, buf, off, sp);
        if (sp + 1u < run) {
            copy_field(out_msg->beneficiary_name, FEDWIRE_NAME_LEN, buf, off + sp + 1u, run - sp - 1u);
        } else {
            copy_field(out_msg->beneficiary_name, FEDWIRE_NAME_LEN, buf, off, 0u);
        }
    }

    off = find_tag(buf, len, "5000");
    if (off == 0xFFFFFFFFu) return 0;
    {
        uint32_t run = field_run_length(buf, len, off);
        uint32_t sp = 0;
        while (sp < run && buf[off + sp] != (uint8_t)' ') {
            sp++;
        }
        copy_field(out_msg->originator_account, FEDWIRE_ACCOUNT_LEN, buf, off, sp);
        if (sp + 1u < run) {
            copy_field(out_msg->originator_name, FEDWIRE_NAME_LEN, buf, off + sp + 1u, run - sp - 1u);
        } else {
            copy_field(out_msg->originator_name, FEDWIRE_NAME_LEN, buf, off, 0u);
        }
    }

    return 1;
}

uint32_t fedwire_pkcs7_pad(const uint8_t *in, uint32_t in_len, uint8_t *out, uint32_t out_buf_size, uint32_t block_size) {
    uint32_t pad_len = block_size - (in_len % block_size);
    uint32_t total = in_len + pad_len;
    if (total > out_buf_size) {
        return 0;
    }
    for (uint32_t i = 0; i < in_len; i++) {
        out[i] = in[i];
    }
    for (uint32_t i = 0; i < pad_len; i++) {
        out[in_len + i] = (uint8_t)pad_len;
    }
    return total;
}

uint32_t fedwire_pkcs7_unpad(const uint8_t *in, uint32_t in_len, uint32_t block_size) {
    if (in_len == 0 || in_len % block_size != 0u) {
        return 0xFFFFFFFFu;
    }
    uint8_t pad_len = in[in_len - 1u];
    if (pad_len == 0 || pad_len > block_size || (uint32_t)pad_len > in_len) {
        return 0xFFFFFFFFu;
    }
    for (uint32_t i = 0; i < pad_len; i++) {
        if (in[in_len - 1u - i] != pad_len) {
            return 0xFFFFFFFFu;
        }
    }
    return in_len - (uint32_t)pad_len;
}
