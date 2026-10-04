/* See 055_iso8583.h's own top-of-file comment for the citation of every
 * data element, width, and length-prefix rule used here. */

#include "055_iso8583.h"

/* How each supported DE is laid out on the wire. */
#define KIND_FIXED 0u
#define KIND_LLVAR 1u
#define KIND_LLLVAR 2u
#define KIND_HEX 3u   /* DE 52 (since Ch 38) -- `width` real raw bytes,
                       * 2*width real ASCII hex characters on the wire */
#define KIND_LLLVAR_HEX 4u /* new this chapter: DE 55 -- an LLLVAR field
                            * whose own LLL prefix counts real RAW bytes
                            * (this codec's own stated choice), each
                            * hex-encoded 2-for-1 on the wire like
                            * KIND_HEX */

typedef struct {
    uint8_t de;
    uint8_t kind;
    uint16_t width;   /* fixed width (or KIND_HEX's own byte count), or
                       * maximum length for LL/LLLVAR */
    uint8_t numeric;  /* 1: every byte must be an ASCII digit (unused, and
                       * ignored, for KIND_HEX -- see is_hex_digit() below) */
} de_spec_t;

static const de_spec_t g_specs[] = {
    { 2, KIND_LLVAR, ISO8583_PAN_MAX, 1},
    { 3, KIND_FIXED, 6, 1},
    { 4, KIND_FIXED, 12, 1},
    { 7, KIND_FIXED, 10, 1},
    {11, KIND_FIXED, 6, 1},
    {12, KIND_FIXED, 6, 1},
    {13, KIND_FIXED, 4, 1},
    {38, KIND_FIXED, 6, 0},
    {39, KIND_FIXED, 2, 0},
    {41, KIND_FIXED, 8, 0},
    {42, KIND_FIXED, 15, 0},
    {48, KIND_LLLVAR, ISO8583_DE48_MAX, 0},
    {49, KIND_FIXED, 3, 1},
    {52, KIND_HEX, 8, 0},
    {54, KIND_LLLVAR, 20, 0}, /* one real sub-record only -- see 055_iso8583.h */
    {55, KIND_LLLVAR_HEX, ISO8583_DE55_MAX, 0},
};
#define SPEC_COUNT (sizeof(g_specs) / sizeof(g_specs[0]))

static const de_spec_t *find_spec(uint32_t de) {
    for (uint32_t i = 0; i < SPEC_COUNT; i++) {
        if (g_specs[i].de == de) {
            return &g_specs[i];
        }
    }
    return 0;
}

void iso8583_set_field(iso8583_msg_t *msg, uint32_t de) {
    if (de >= 1u && de <= 32u) {
        msg->bitmap_hi |= 1u << (32u - de);
    } else if (de >= 33u && de <= 64u) {
        msg->bitmap_lo |= 1u << (64u - de);
    }
}

int iso8583_has_field(const iso8583_msg_t *msg, uint32_t de) {
    if (de >= 1u && de <= 32u) {
        return (msg->bitmap_hi >> (32u - de)) & 1u;
    }
    if (de >= 33u && de <= 64u) {
        return (msg->bitmap_lo >> (64u - de)) & 1u;
    }
    return 0;
}

static int is_digit(uint8_t c) {
    return c >= (uint8_t)'0' && c <= (uint8_t)'9';
}

/* Where a DE's bytes live inside iso8583_msg_t. For DE 4/DE 54, the
 * caller converts to/from amount_cents (and, for DE 54, its own other
 * sub-fields) separately, through `scratch`. DE 52's own raw bytes live
 * directly in the struct -- see build_hex()/parse_hex() for its own
 * real hex transformation, never the plain byte copy every other DE
 * uses. */
static uint8_t *field_ptr(iso8583_msg_t *m, uint32_t de, uint8_t *scratch) {
    switch (de) {
    case 2:  return m->pan;
    case 3:  return m->processing_code;
    case 4:  return scratch;
    case 7:  return m->transmission_datetime;
    case 11: return m->stan;
    case 12: return m->local_time;
    case 13: return m->local_date;
    case 38: return m->auth_id;
    case 39: return m->response_code;
    case 41: return m->terminal_id;
    case 42: return m->merchant_id;
    case 48: return m->additional_data;
    case 49: return m->currency_code;
    case 52: return m->pin_block;
    case 54: return scratch;
    case 55: return m->icc_data;
    default: return 0;
    }
}

static void put_digits(uint8_t *buf, uint32_t width, uint32_t value) {
    for (uint32_t i = 0; i < width; i++) {
        buf[width - 1u - i] = (uint8_t)('0' + (value % 10u));
        value /= 10u;
    }
}

/* DE 54's own real one-sub-record layout (055_iso8583.h): formats
 * `amt` into exactly 20 real ASCII characters. */
static void format_amt54(const iso8583_amt54_t *amt, uint8_t *out20) {
    out20[0] = amt->account_type[0];
    out20[1] = amt->account_type[1];
    out20[2] = amt->amount_type[0];
    out20[3] = amt->amount_type[1];
    out20[4] = amt->currency_code[0];
    out20[5] = amt->currency_code[1];
    out20[6] = amt->currency_code[2];
    out20[7] = amt->sign;
    put_digits(&out20[8], 12u, amt->amount_cents);
}

static const char g_hex[] = "0123456789ABCDEF";

uint32_t iso8583_build(const iso8583_msg_t *msg, uint8_t *out, uint32_t out_size) {
    if (out_size < 20u) {
        return 0;
    }
    for (uint32_t i = 0; i < 4u; i++) {
        if (!is_digit(msg->mti[i])) {
            return 0;
        }
        out[i] = msg->mti[i];
    }
    /* Primary bitmap: 16 ASCII hex characters, most significant first. */
    for (uint32_t i = 0; i < 8u; i++) {
        out[4u + i] = (uint8_t)g_hex[(msg->bitmap_hi >> (28u - 4u * i)) & 0xFu];
        out[12u + i] = (uint8_t)g_hex[(msg->bitmap_lo >> (28u - 4u * i)) & 0xFu];
    }
    uint32_t pos = 20;

    iso8583_msg_t *m = (iso8583_msg_t *)msg; /* field_ptr() only reads here */
    uint8_t scratch[20]; /* DE 4's own 12 digits, or DE 54's own 20 real characters */
    for (uint32_t de = 1; de <= 64u; de++) {
        if (!iso8583_has_field(msg, de)) {
            continue;
        }
        const de_spec_t *s = find_spec(de);
        if (s == 0) {
            return 0; /* bit 1 (secondary bitmap) or an unsupported DE */
        }
        if (de == 4u) {
            put_digits(scratch, 12u, msg->amount_cents);
        } else if (de == 54u) {
            if (msg->additional_amount.sign != (uint8_t) 'C' &&
                msg->additional_amount.sign != (uint8_t) 'D') {
                return 0; /* the real sign byte is always 'C' or 'D' */
            }
            format_amt54(&msg->additional_amount, scratch);
        }
        const uint8_t *src = field_ptr(m, de, scratch);
        uint32_t len = s->width;
        if (de == 2u) {
            len = msg->pan_len;
        } else if (de == 48u) {
            len = msg->additional_data_len;
        } else if (de == 55u) {
            len = msg->icc_data_len;
        }
        if (len > s->width) {
            return 0;
        }
        uint32_t prefix = (s->kind == KIND_LLVAR) ? 2u
                         : (s->kind == KIND_LLLVAR || s->kind == KIND_LLLVAR_HEX) ? 3u : 0u;
        uint32_t wire_len = (s->kind == KIND_HEX || s->kind == KIND_LLLVAR_HEX) ? len * 2u : len;
        if (pos + prefix + wire_len > out_size) {
            return 0;
        }
        put_digits(&out[pos], prefix, len);
        pos += prefix;
        if (s->kind == KIND_HEX || s->kind == KIND_LLLVAR_HEX) {
            for (uint32_t i = 0; i < len; i++) {
                out[pos + 2u * i] = (uint8_t) g_hex[(src[i] >> 4) & 0xFu];
                out[pos + 2u * i + 1u] = (uint8_t) g_hex[src[i] & 0xFu];
            }
        } else {
            for (uint32_t i = 0; i < len; i++) {
                if (s->numeric && !is_digit(src[i])) {
                    return 0;
                }
                out[pos + i] = src[i];
            }
        }
        pos += wire_len;
    }
    return pos;
}

static int hex_value(uint8_t c, uint32_t *out) {
    if (c >= (uint8_t)'0' && c <= (uint8_t)'9') {
        *out = (uint32_t)(c - (uint8_t)'0');
    } else if (c >= (uint8_t)'A' && c <= (uint8_t)'F') {
        *out = (uint32_t)(c - (uint8_t)'A') + 10u;
    } else if (c >= (uint8_t)'a' && c <= (uint8_t)'f') {
        *out = (uint32_t)(c - (uint8_t)'a') + 10u;
    } else {
        return 0;
    }
    return 1;
}

int iso8583_parse(const uint8_t *buf, uint32_t len, iso8583_msg_t *out) {
    uint8_t *raw = (uint8_t *)out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }
    if (len < 20u) {
        return 0;
    }
    for (uint32_t i = 0; i < 4u; i++) {
        if (!is_digit(buf[i])) {
            return 0;
        }
        out->mti[i] = buf[i];
    }
    for (uint32_t i = 0; i < 8u; i++) {
        uint32_t hi, lo;
        if (!hex_value(buf[4u + i], &hi) || !hex_value(buf[12u + i], &lo)) {
            return 0;
        }
        out->bitmap_hi = (out->bitmap_hi << 4) | hi;
        out->bitmap_lo = (out->bitmap_lo << 4) | lo;
    }
    uint32_t pos = 20;

    uint8_t scratch[20]; /* DE 4's own 12 digits, or DE 54's own 20 real characters */
    for (uint32_t de = 1; de <= 64u; de++) {
        if (!iso8583_has_field(out, de)) {
            continue;
        }
        const de_spec_t *s = find_spec(de);
        if (s == 0) {
            return 0;
        }
        uint32_t flen = s->width;
        uint32_t prefix = (s->kind == KIND_LLVAR) ? 2u
                         : (s->kind == KIND_LLLVAR || s->kind == KIND_LLLVAR_HEX) ? 3u : 0u;
        if (prefix > 0u) {
            if (pos + prefix > len) {
                return 0;
            }
            flen = 0;
            for (uint32_t i = 0; i < prefix; i++) {
                if (!is_digit(buf[pos + i])) {
                    return 0;
                }
                flen = flen * 10u + (uint32_t)(buf[pos + i] - (uint8_t)'0');
            }
            if (flen > s->width) {
                return 0;
            }
            pos += prefix;
        }
        if (de == 54u && flen != 20u) {
            return 0; /* stated limit: exactly one real 20-character sub-record */
        }
        uint32_t wire_len = (s->kind == KIND_HEX || s->kind == KIND_LLLVAR_HEX) ? flen * 2u : flen;
        if (pos + wire_len > len) {
            return 0;
        }
        uint8_t *dst = field_ptr(out, de, scratch);
        if (s->kind == KIND_HEX || s->kind == KIND_LLLVAR_HEX) {
            for (uint32_t i = 0; i < flen; i++) {
                uint32_t hi, lo;
                if (!hex_value(buf[pos + 2u * i], &hi) || !hex_value(buf[pos + 2u * i + 1u], &lo)) {
                    return 0; /* a non-hex byte in DE 52 or DE 55 */
                }
                dst[i] = (uint8_t)((hi << 4) | lo);
            }
        } else {
            for (uint32_t i = 0; i < flen; i++) {
                if (s->numeric && !is_digit(buf[pos + i])) {
                    return 0;
                }
                dst[i] = buf[pos + i];
            }
        }
        pos += wire_len;

        if (de == 2u) {
            out->pan_len = flen;
        } else if (de == 48u) {
            out->additional_data_len = flen;
        } else if (de == 55u) {
            out->icc_data_len = flen;
        } else if (de == 4u) {
            uint32_t v = 0;
            for (uint32_t i = 0; i < 12u; i++) {
                uint32_t d = (uint32_t)(scratch[i] - (uint8_t)'0');
                if (v > (0xFFFFFFFFu - d) / 10u) {
                    return 0; /* stated limit: DE 4 must fit a uint32_t */
                }
                v = v * 10u + d;
            }
            out->amount_cents = v;
        } else if (de == 54u) {
            if (scratch[7] != (uint8_t)'C' && scratch[7] != (uint8_t)'D') {
                return 0; /* the real sign byte is always 'C' or 'D' */
            }
            out->additional_amount.account_type[0] = scratch[0];
            out->additional_amount.account_type[1] = scratch[1];
            out->additional_amount.amount_type[0] = scratch[2];
            out->additional_amount.amount_type[1] = scratch[3];
            out->additional_amount.currency_code[0] = scratch[4];
            out->additional_amount.currency_code[1] = scratch[5];
            out->additional_amount.currency_code[2] = scratch[6];
            out->additional_amount.sign = scratch[7];
            uint32_t v = 0;
            for (uint32_t i = 0; i < 12u; i++) {
                if (!is_digit(scratch[8u + i])) {
                    return 0;
                }
                uint32_t d = (uint32_t)(scratch[8u + i] - (uint8_t)'0');
                if (v > (0xFFFFFFFFu - d) / 10u) {
                    return 0; /* stated limit: DE 54 amount must fit a uint32_t */
                }
                v = v * 10u + d;
            }
            out->additional_amount.amount_cents = v;
        }
    }
    return pos == len; /* trailing bytes are refused, not ignored */
}

/* Luhn sum over `len` digits. When `doubling_starts_rightmost` is 1 the
 * rightmost digit is doubled (the payload of a number whose check digit
 * is about to be appended, ISO/IEC 7812-1's own "beginning with the
 * first right-hand digit"); when 0, the rightmost digit IS the check
 * digit and is left alone. Returns -1 on any non-digit byte. */
static int luhn_sum(const uint8_t *d, uint32_t len, int doubling_starts_rightmost) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < len; i++) {
        uint8_t c = d[len - 1u - i];
        if (!is_digit(c)) {
            return -1;
        }
        uint32_t v = (uint32_t)(c - (uint8_t)'0');
        int doubled = doubling_starts_rightmost ? ((i % 2u) == 0u) : ((i % 2u) == 1u);
        if (doubled) {
            v *= 2u;
            if (v > 9u) {
                v -= 9u; /* same as adding the two digits of 10..18 */
            }
        }
        sum += v;
    }
    return (int)(sum % 10u);
}

uint8_t iso8583_luhn_check_digit(const uint8_t *digits, uint32_t len) {
    int s = luhn_sum(digits, len, 1);
    if (s < 0) {
        return 0;
    }
    return (uint8_t)('0' + (uint32_t)((10 - s) % 10));
}

int iso8583_luhn_valid(const uint8_t *pan, uint32_t len) {
    return len >= 2u && luhn_sum(pan, len, 0) == 0;
}
