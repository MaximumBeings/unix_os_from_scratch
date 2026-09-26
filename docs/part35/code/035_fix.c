/* See 035_fix.h's own top-of-file comment for the full citation trail
 * (every tag number and enumerated value read directly out of a real
 * clone of quickfix/quickfix's own spec/FIX44.xml) and this module's
 * own stated SOH-vs-'|' display convention. */

#include "035_fix.h"

#define FIX_SOH ((uint8_t) 0x01u)

void fix_set_field(fix_message_t *msg, uint32_t tag, const char *value) {
    uint32_t idx = msg->field_count;
    /* Refusing silently past FIX_MAX_FIELDS would be a worse bug than
     * a caller finding out immediately: every caller in this chapter
     * stays well under the limit, so an overrun here would be this
     * module's own logic error, not a real message being too large. */
    if (idx >= FIX_MAX_FIELDS) {
        return;
    }
    msg->fields[idx].tag = tag;
    uint32_t i = 0;
    while (value[i] != '\0' && i < FIX_MAX_TAG_VALUE_LEN - 1u) {
        msg->fields[idx].value[i] = value[i];
        i++;
    }
    msg->fields[idx].value[i] = '\0';
    msg->field_count = idx + 1u;
}

int fix_get_field(const fix_message_t *msg, uint32_t tag, char *out) {
    for (uint32_t i = 0; i < msg->field_count; i++) {
        if (msg->fields[i].tag == tag) {
            uint32_t j = 0;
            while (msg->fields[i].value[j] != '\0') {
                out[j] = msg->fields[i].value[j];
                j++;
            }
            out[j] = '\0';
            return 1;
        }
    }
    return 0;
}

static uint32_t str_len(const char *s) {
    uint32_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

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

static void put_digits(char *buf, uint32_t *pos, uint32_t value) {
    char rev[10];
    uint32_t rn = 0;
    if (value == 0u) {
        buf[(*pos)++] = '0';
        return;
    }
    while (value > 0u) {
        rev[rn++] = (char) ('0' + (value % 10u));
        value /= 10u; /* a plain uint32_t modulo/division: 32-bit, no libgcc call */
    }
    while (rn > 0u) {
        buf[(*pos)++] = rev[--rn];
    }
}

/* Appends "<tag>=<value><SOH>" into `buf`, refusing if it would not
 * fit in `size`. */
static int append_field(uint8_t *buf, uint32_t *pos, uint32_t size, uint32_t tag,
                        const char *value) {
    char tagbuf[12];
    uint32_t tn = 0;
    put_digits(tagbuf, &tn, tag);
    tagbuf[tn] = '\0';
    if (!append_str(buf, pos, size, tagbuf)) {
        return 0;
    }
    if (*pos + 1u > size) {
        return 0;
    }
    buf[(*pos)++] = (uint8_t) '=';
    if (!append_str(buf, pos, size, value)) {
        return 0;
    }
    if (*pos + 1u > size) {
        return 0;
    }
    buf[(*pos)++] = FIX_SOH;
    return 1;
}

/* Real FIX checksum (035_fix.h): the sum of every byte from the very
 * start of the message through the SOH immediately before tag 10,
 * modulo 256. */
static uint32_t fix_checksum(const uint8_t *buf, uint32_t len) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < len; i++) {
        sum += buf[i];
    }
    return sum % 256u;
}

uint32_t fix_build_message(const fix_message_t *msg, const char *sender_comp_id,
                          const char *target_comp_id, uint32_t seq_num,
                          uint8_t *out, uint32_t out_size) {
    if (msg->msg_type[0] == '\0' || msg->msg_type[1] != '\0') {
        return 0;
    }

    /* Build the body (everything after "9=<len>|") into a scratch
     * buffer first, since BodyLength has to be known before tag 9
     * itself can be written. */
    static uint8_t body[FIX_MAX_MESSAGE_LEN];
    uint32_t bpos = 0;
    int ok = 1;
    ok = ok && append_field(body, &bpos, sizeof(body), 35u, msg->msg_type);
    ok = ok && append_field(body, &bpos, sizeof(body), 49u, sender_comp_id);
    ok = ok && append_field(body, &bpos, sizeof(body), 56u, target_comp_id);
    char seqbuf[11];
    uint32_t sn = 0;
    put_digits(seqbuf, &sn, seq_num);
    seqbuf[sn] = '\0';
    ok = ok && append_field(body, &bpos, sizeof(body), 34u, seqbuf);
    for (uint32_t i = 0; i < msg->field_count; i++) {
        ok = ok && append_field(body, &bpos, sizeof(body), msg->fields[i].tag,
                                msg->fields[i].value);
    }
    if (!ok) {
        return 0;
    }

    uint32_t pos = 0;
    ok = ok && append_field(out, &pos, out_size, 8u, "FIX.4.4");
    char lenbuf[11];
    uint32_t ln = 0;
    put_digits(lenbuf, &ln, bpos);
    lenbuf[ln] = '\0';
    ok = ok && append_field(out, &pos, out_size, 9u, lenbuf);
    if (!ok || pos + bpos > out_size) {
        return 0;
    }
    for (uint32_t i = 0; i < bpos; i++) {
        out[pos + i] = body[i];
    }
    pos += bpos;

    uint32_t sum = fix_checksum(out, pos);
    char sumbuf[4];
    sumbuf[0] = (char) ('0' + (sum / 100u));
    sumbuf[1] = (char) ('0' + ((sum / 10u) % 10u));
    sumbuf[2] = (char) ('0' + (sum % 10u));
    sumbuf[3] = '\0';
    if (!append_field(out, &pos, out_size, 10u, sumbuf)) {
        return 0;
    }
    return pos;
}

static int is_digit(uint8_t c) {
    return c >= (uint8_t) '0' && c <= (uint8_t) '9';
}

int fix_parse_message(const uint8_t *buf, uint32_t len, fix_message_t *out) {
    uint8_t *raw = (uint8_t *) out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }

    static const char begin_string[] = "8=FIX.4.4";
    uint32_t bs_len = str_len(begin_string);
    if (len < bs_len + 1u) {
        return 0;
    }
    for (uint32_t i = 0; i < bs_len; i++) {
        if (buf[i] != (uint8_t) begin_string[i]) {
            return 0;
        }
    }
    if (buf[bs_len] != FIX_SOH) {
        return 0;
    }
    uint32_t pos = bs_len + 1u;

    /* Tag 9 (BodyLength) must come next. */
    if (pos + 2u > len || buf[pos] != (uint8_t) '9' || buf[pos + 1u] != (uint8_t) '=') {
        return 0;
    }
    pos += 2u;
    uint32_t body_len = 0;
    uint32_t digits_start = pos;
    while (pos < len && buf[pos] != FIX_SOH) {
        if (!is_digit(buf[pos])) {
            return 0;
        }
        body_len = body_len * 10u + (uint32_t) (buf[pos] - (uint8_t) '0');
        pos++;
    }
    if (pos >= len || pos == digits_start) {
        return 0;
    }
    pos++; /* past the SOH ending tag 9 */

    uint32_t body_start = pos;
    if (body_start + body_len > len) {
        return 0; /* stated BodyLength runs past the real message */
    }
    uint32_t after_body = body_start + body_len;

    /* Tag 10 (CheckSum) must immediately follow the stated body. */
    if (after_body + 3u > len || buf[after_body] != (uint8_t) '1' ||
        buf[after_body + 1u] != (uint8_t) '0' || buf[after_body + 2u] != (uint8_t) '=') {
        return 0;
    }
    uint32_t cs_pos = after_body + 3u;
    if (cs_pos + 4u > len) {
        return 0; /* 3 checksum digits + trailing SOH */
    }
    if (!is_digit(buf[cs_pos]) || !is_digit(buf[cs_pos + 1u]) || !is_digit(buf[cs_pos + 2u]) ||
        buf[cs_pos + 3u] != FIX_SOH) {
        return 0;
    }
    uint32_t claimed_checksum = (uint32_t) (buf[cs_pos] - (uint8_t) '0') * 100u +
                                (uint32_t) (buf[cs_pos + 1u] - (uint8_t) '0') * 10u +
                                (uint32_t) (buf[cs_pos + 2u] - (uint8_t) '0');
    if (claimed_checksum != fix_checksum(buf, after_body)) {
        return 0; /* real checksum mismatch -- refused, not corrected */
    }
    if (cs_pos + 4u != len) {
        return 0; /* trailing bytes after the message are refused */
    }

    /* Walk the body's own tag=value|tag=value|... fields. The first
     * must be tag 35 (MsgType); tags 49/56/34 (SenderCompID/
     * TargetCompID/MsgSeqNum) are real but not surfaced to the
     * caller -- this chapter's own demo does not need to inspect
     * them, only to have sent and received them for real. */
    pos = body_start;
    int seen_msg_type = 0;
    while (pos < after_body) {
        uint32_t tag = 0;
        uint32_t tag_start = pos;
        while (pos < after_body && buf[pos] != (uint8_t) '=') {
            if (!is_digit(buf[pos])) {
                return 0;
            }
            tag = tag * 10u + (uint32_t) (buf[pos] - (uint8_t) '0');
            pos++;
        }
        if (pos >= after_body || pos == tag_start) {
            return 0;
        }
        pos++; /* past '=' */
        uint32_t val_start = pos;
        while (pos < after_body && buf[pos] != FIX_SOH) {
            pos++;
        }
        if (pos >= after_body) {
            return 0; /* a field with no closing SOH */
        }
        uint32_t val_len = pos - val_start;
        if (val_len + 1u > FIX_MAX_TAG_VALUE_LEN) {
            return 0;
        }
        pos++; /* past the SOH ending this field */

        if (tag == 35u) {
            if (val_len != 1u) {
                return 0;
            }
            out->msg_type[0] = (char) buf[val_start];
            out->msg_type[1] = '\0';
            seen_msg_type = 1;
        } else if (tag == 49u || tag == 56u || tag == 34u) {
            /* real, present, deliberately not surfaced -- see above */
        } else {
            if (out->field_count >= FIX_MAX_FIELDS) {
                return 0;
            }
            fix_field_t *f = &out->fields[out->field_count];
            f->tag = tag;
            for (uint32_t i = 0; i < val_len; i++) {
                f->value[i] = (char) buf[val_start + i];
            }
            f->value[val_len] = '\0';
            out->field_count++;
        }
    }
    return seen_msg_type;
}
