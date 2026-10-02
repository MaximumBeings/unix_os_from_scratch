/* See 047_marketplace.h's own top-of-file comment: this file's own
 * wire format and ranking rules are this book's own invented design,
 * not a real marketplace's. Only the embedded GS1 element string
 * (047_gs1.c) is real. */

#include "047_marketplace.h"

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

static uint32_t append_char(uint8_t *out, uint32_t pos, uint32_t out_size, uint8_t c) {
    if (pos + 1u > out_size) {
        return 0xFFFFFFFFu;
    }
    out[pos] = c;
    return pos + 1u;
}

static uint32_t append_uint(uint8_t *out, uint32_t pos, uint32_t out_size, uint32_t v) {
    uint8_t digits[10];
    uint32_t n = 0;
    if (v == 0u) {
        digits[n++] = '0';
    } else {
        while (v > 0u && n < 10u) {
            digits[n++] = (uint8_t)('0' + (v % 10u));
            v /= 10u;
        }
    }
    if (pos + n > out_size) {
        return 0xFFFFFFFFu;
    }
    for (uint32_t i = 0; i < n; i++) {
        out[pos + i] = digits[n - 1u - i];
    }
    return pos + n;
}

#define AB(b, n) do { pos = append_bytes(out, pos, out_size, b, n); if (pos == 0xFFFFFFFFu) return 0; } while (0)
#define AC(c) do { pos = append_char(out, pos, out_size, c); if (pos == 0xFFFFFFFFu) return 0; } while (0)
#define AU(v) do { pos = append_uint(out, pos, out_size, v); if (pos == 0xFFFFFFFFu) return 0; } while (0)

uint32_t marketplace_build_listing(const marketplace_listing_t *listing, uint8_t *out,
                                    uint32_t out_size) {
    if (listing->marketplace_name_len > MARKETPLACE_NAME_MAX ||
        listing->section_len > MARKETPLACE_SECTION_MAX) {
        return 0;
    }
    uint32_t pos = 0;
    AB(listing->marketplace_name, listing->marketplace_name_len);
    AC((uint8_t) '|');
    AB(listing->section, listing->section_len);
    AC((uint8_t) '|');
    AU(listing->row);
    AC((uint8_t) '|');
    AU(listing->price_cents);
    AC((uint8_t) '|');
    uint32_t gs1_len = gs1_build_element_string(&listing->ticket, &out[pos], out_size - pos);
    if (gs1_len == 0) {
        return 0;
    }
    pos += gs1_len;
    AC((uint8_t) '\n');
    return pos;
}

#undef AB
#undef AC
#undef AU

static uint32_t find_delim(const uint8_t *buf, uint32_t len, uint32_t pos, uint8_t delim) {
    while (pos < len && buf[pos] != delim) {
        pos++;
    }
    return pos;
}

static int is_digit(uint8_t c) {
    return c >= (uint8_t) '0' && c <= (uint8_t) '9';
}

static int parse_uint(const uint8_t *text, uint32_t len, uint32_t *out_v) {
    uint32_t v = 0;
    if (len == 0u) {
        return 0;
    }
    for (uint32_t i = 0; i < len; i++) {
        if (!is_digit(text[i])) {
            return 0;
        }
        v = v * 10u + (uint32_t)(text[i] - (uint8_t) '0');
    }
    *out_v = v;
    return 1;
}

int marketplace_parse_listing(const uint8_t *buf, uint32_t len, marketplace_listing_t *out) {
    uint8_t *raw = (uint8_t *) out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }
    if (len == 0u || buf[len - 1u] != (uint8_t) '\n') {
        return 0;
    }
    uint32_t pos = 0;
    uint32_t next;

    next = find_delim(buf, len, pos, (uint8_t) '|');
    if (next >= len || next - pos > MARKETPLACE_NAME_MAX) return 0;
    for (uint32_t i = pos; i < next; i++) { out->marketplace_name[i - pos] = buf[i]; }
    out->marketplace_name_len = next - pos;
    pos = next + 1u;

    next = find_delim(buf, len, pos, (uint8_t) '|');
    if (next >= len || next - pos > MARKETPLACE_SECTION_MAX) return 0;
    for (uint32_t i = pos; i < next; i++) { out->section[i - pos] = buf[i]; }
    out->section_len = next - pos;
    pos = next + 1u;

    next = find_delim(buf, len, pos, (uint8_t) '|');
    if (next >= len || !parse_uint(&buf[pos], next - pos, &out->row)) return 0;
    pos = next + 1u;

    next = find_delim(buf, len, pos, (uint8_t) '|');
    if (next >= len || !parse_uint(&buf[pos], next - pos, &out->price_cents)) return 0;
    pos = next + 1u;

    /* the remaining bytes, up to but not including the trailing '\n',
     * are the real GS1 element string */
    if (!gs1_parse_element_string(&buf[pos], len - 1u - pos, &out->ticket)) {
        return 0;
    }
    out->section_rank = marketplace_section_rank(out->section, out->section_len);
    return 1;
}

uint32_t marketplace_section_rank(const uint8_t *section, uint32_t section_len) {
    if (section_len == 1u && section[0] == (uint8_t) 'A') {
        return 0u;
    }
    if (section_len == 1u && section[0] == (uint8_t) 'B') {
        return 1u;
    }
    if (section_len == 1u && section[0] == (uint8_t) 'C') {
        return 2u;
    }
    return 99u; /* any section this book's own lookup doesn't recognize */
}

int marketplace_pick_best(const marketplace_search_result_t *result,
                           marketplace_rank_rule_t rule) {
    if (result->count == 0u) {
        return -1;
    }
    uint32_t best = 0;
    for (uint32_t i = 1; i < result->count; i++) {
        const marketplace_listing_t *cand = &result->listings[i];
        const marketplace_listing_t *cur = &result->listings[best];
        int cand_better;
        if (rule == MARKETPLACE_RANK_BEST_SEAT_THEN_CHEAPEST &&
            cand->section_rank != cur->section_rank) {
            cand_better = cand->section_rank < cur->section_rank;
        } else {
            cand_better = cand->price_cents < cur->price_cents;
        }
        if (cand_better) {
            best = i;
        }
    }
    return (int) best;
}
