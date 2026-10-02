/* See 047_betfair.h's own top-of-file comment for the citation of every
 * real field name, nesting, and string-typed-number convention used
 * here. This file builds and parses exactly the fixed literal text
 * shown there -- not a general JSON parser -- refusing outright on
 * anything that doesn't match that fixed shape byte-for-byte.
 *
 * `customerRef` (placeOrders request) has no field in
 * betfair_place_order_t to carry a caller-supplied value, so this file
 * always sends the fixed literal "UNIXOS43" -- this chapter's own
 * stated simplification, not something Betfair's own API requires.
 *
 * The placeOrders response models both the real top-level
 * `result.status` and the real per-instruction
 * `instructionReports[0].status` as the SAME value (this chapter's own
 * simplification: a real Betfair response can in principle disagree
 * between the two, e.g. one failed instruction inside an otherwise
 * successful call -- out of scope here since this chapter only ever
 * places exactly one instruction per call). `betId` is encoded as a
 * plain unquoted integer; the cited sample code never shows its own
 * real JSON type for betId (it only prints `result['status']` and
 * never prints `betId` at all), so this is this file's own stated
 * choice rather than a cited fact. */

#include "047_betfair.h"

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

static uint32_t append_lit(uint8_t *out, uint32_t pos, uint32_t out_size, const char *lit) {
    uint32_t n = 0;
    while (lit[n] != '\0') {
        n++;
    }
    return append_bytes(out, pos, out_size, (const uint8_t *) lit, n);
}

/* real decimal odds / stake sizes are tracked internally as hundredths
 * (047_odds.h's own convention) but Betfair's own real wire convention
 * sends them as a string like "2.50" -- this writes that same shape. */
static uint32_t append_decimal(uint8_t *out, uint32_t pos, uint32_t out_size, uint32_t cents) {
    uint32_t int_part = cents / 100u;
    uint32_t frac = cents % 100u;
    pos = append_uint(out, pos, out_size, int_part);
    if (pos == 0xFFFFFFFFu) {
        return pos;
    }
    pos = append_char(out, pos, out_size, (uint8_t) '.');
    if (pos == 0xFFFFFFFFu) {
        return pos;
    }
    pos = append_char(out, pos, out_size, (uint8_t)('0' + (frac / 10u)));
    if (pos == 0xFFFFFFFFu) {
        return pos;
    }
    return append_char(out, pos, out_size, (uint8_t)('0' + (frac % 10u)));
}

#define AB(b, n) do { pos = append_bytes(out, pos, out_size, b, n); if (pos == 0xFFFFFFFFu) return 0; } while (0)
#define AC(c) do { pos = append_char(out, pos, out_size, c); if (pos == 0xFFFFFFFFu) return 0; } while (0)
#define AU(v) do { pos = append_uint(out, pos, out_size, v); if (pos == 0xFFFFFFFFu) return 0; } while (0)
#define AL(s) do { pos = append_lit(out, pos, out_size, s); if (pos == 0xFFFFFFFFu) return 0; } while (0)
#define AD(v) do { pos = append_decimal(out, pos, out_size, v); if (pos == 0xFFFFFFFFu) return 0; } while (0)

uint32_t betfair_build_market_book(const betfair_market_book_t *book, uint8_t *out,
                                    uint32_t out_size) {
    if (book->runner_count > BETFAIR_MAX_RUNNERS || book->market_id_len > 16u) {
        return 0;
    }
    uint32_t pos = 0;
    AL("{\"jsonrpc\":\"2.0\",\"result\":{\"marketId\":\"");
    AB(book->market_id, book->market_id_len);
    AL("\",\"runners\":[");
    for (uint32_t i = 0; i < book->runner_count; i++) {
        if (i > 0u) {
            AL(",");
        }
        AL("{\"selectionId\":");
        AU(book->runners[i].selection_id);
        AL(",\"status\":\"ACTIVE\",\"ex\":{\"availableToBack\":[{\"price\":\"");
        AD(book->runners[i].back_price_cents);
        AL("\",\"size\":\"");
        AD(book->runners[i].back_size_cents);
        AL("\"}]}}");
    }
    AL("]},\"id\":1}");
    return pos;
}

uint32_t betfair_build_place_order(const betfair_place_order_t *order, uint8_t *out,
                                    uint32_t out_size) {
    if (order->market_id_len > 16u) {
        return 0;
    }
    uint32_t pos = 0;
    AL("{\"jsonrpc\":\"2.0\",\"method\":\"SportsAPING/v1.0/placeOrders\",\"params\":{\"marketId\":\"");
    AB(order->market_id, order->market_id_len);
    AL("\",\"instructions\":[{\"selectionId\":");
    AU(order->selection_id);
    AL(",\"handicap\":\"0\",\"side\":\"BACK\",\"orderType\":\"LIMIT\",\"limitOrder\":{\"size\":\"");
    AD(order->size_cents);
    AL("\",\"price\":\"");
    AD(order->price_cents);
    AL("\",\"persistenceType\":\"LAPSE\"}}],\"customerRef\":\"UNIXOS43\"},\"id\":1}");
    return pos;
}

uint32_t betfair_build_place_order_response(const char *status, uint32_t bet_id, uint8_t *out,
                                             uint32_t out_size) {
    int success;
    uint32_t i = 0;
    const char *success_lit = "SUCCESS";
    const char *failure_lit = "FAILURE";
    while (status[i] != '\0' && success_lit[i] != '\0' && status[i] == success_lit[i]) {
        i++;
    }
    if (status[i] == '\0' && success_lit[i] == '\0') {
        success = 1;
    } else {
        i = 0;
        while (status[i] != '\0' && failure_lit[i] != '\0' && status[i] == failure_lit[i]) {
            i++;
        }
        if (status[i] == '\0' && failure_lit[i] == '\0') {
            success = 0;
        } else {
            return 0;
        }
    }
    uint32_t pos = 0;
    AL("{\"jsonrpc\":\"2.0\",\"result\":{\"status\":\"");
    AL(success ? "SUCCESS" : "FAILURE");
    AL("\",\"instructionReports\":[{\"status\":\"");
    AL(success ? "SUCCESS" : "FAILURE");
    AL("\",\"betId\":");
    AU(bet_id);
    AL("}]},\"id\":1}");
    return pos;
}

#undef AB
#undef AC
#undef AU
#undef AL
#undef AD

static int expect_lit(const uint8_t *buf, uint32_t len, uint32_t *pos, const char *lit) {
    uint32_t n = 0;
    while (lit[n] != '\0') {
        n++;
    }
    if (*pos + n > len) {
        return 0;
    }
    for (uint32_t i = 0; i < n; i++) {
        if (buf[*pos + i] != (uint8_t) lit[i]) {
            return 0;
        }
    }
    *pos += n;
    return 1;
}

static uint32_t find_char(const uint8_t *buf, uint32_t len, uint32_t pos, uint8_t c) {
    while (pos < len && buf[pos] != c) {
        pos++;
    }
    return pos;
}

static int is_digit(uint8_t c) {
    return c >= (uint8_t) '0' && c <= (uint8_t) '9';
}

static int parse_uint_field(const uint8_t *buf, uint32_t len, uint32_t *pos, uint32_t *out_v) {
    uint32_t v = 0;
    uint32_t n = 0;
    while (*pos < len && is_digit(buf[*pos])) {
        v = v * 10u + (uint32_t)(buf[*pos] - (uint8_t) '0');
        (*pos)++;
        n++;
    }
    if (n == 0u) {
        return 0;
    }
    *out_v = v;
    return 1;
}

static int parse_decimal_field(const uint8_t *buf, uint32_t len, uint32_t *pos,
                                uint32_t *out_cents) {
    uint32_t int_part;
    if (!parse_uint_field(buf, len, pos, &int_part)) {
        return 0;
    }
    if (*pos >= len || buf[*pos] != (uint8_t) '.') {
        return 0;
    }
    (*pos)++;
    if (*pos + 2u > len || !is_digit(buf[*pos]) || !is_digit(buf[*pos + 1u])) {
        return 0;
    }
    uint32_t frac = (uint32_t)(buf[*pos] - (uint8_t) '0') * 10u +
                     (uint32_t)(buf[*pos + 1u] - (uint8_t) '0');
    *pos += 2u;
    *out_cents = int_part * 100u + frac;
    return 1;
}

static int parse_market_id(const uint8_t *buf, uint32_t len, uint32_t *pos, uint8_t *out_id,
                            uint32_t *out_len) {
    uint32_t start = *pos;
    uint32_t end = find_char(buf, len, start, (uint8_t) '"');
    if (end >= len || end - start > 16u) {
        return 0;
    }
    for (uint32_t i = start; i < end; i++) {
        out_id[i - start] = buf[i];
    }
    *out_len = end - start;
    *pos = end;
    return 1;
}

int betfair_parse_market_book(const uint8_t *buf, uint32_t len, betfair_market_book_t *out) {
    uint8_t *raw = (uint8_t *) out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }
    uint32_t pos = 0;
    if (!expect_lit(buf, len, &pos, "{\"jsonrpc\":\"2.0\",\"result\":{\"marketId\":\"")) {
        return 0;
    }
    if (!parse_market_id(buf, len, &pos, out->market_id, &out->market_id_len)) {
        return 0;
    }
    if (!expect_lit(buf, len, &pos, "\",\"runners\":[")) {
        return 0;
    }
    uint32_t count = 0;
    if (pos < len && buf[pos] == (uint8_t) ']') {
        /* real Betfair markets always have runners; this chapter's own
         * codec still accepts an empty array rather than refusing it */
    } else {
        while (1) {
            if (count >= BETFAIR_MAX_RUNNERS) {
                return 0;
            }
            if (!expect_lit(buf, len, &pos, "{\"selectionId\":")) {
                return 0;
            }
            if (!parse_uint_field(buf, len, &pos, &out->runners[count].selection_id)) {
                return 0;
            }
            if (!expect_lit(buf, len, &pos,
                             ",\"status\":\"ACTIVE\",\"ex\":{\"availableToBack\":[{\"price\":\"")) {
                return 0;
            }
            if (!parse_decimal_field(buf, len, &pos, &out->runners[count].back_price_cents)) {
                return 0;
            }
            if (!expect_lit(buf, len, &pos, "\",\"size\":\"")) {
                return 0;
            }
            if (!parse_decimal_field(buf, len, &pos, &out->runners[count].back_size_cents)) {
                return 0;
            }
            if (!expect_lit(buf, len, &pos, "\"}]}}")) {
                return 0;
            }
            count++;
            if (pos < len && buf[pos] == (uint8_t) ',') {
                pos++;
                continue;
            }
            break;
        }
    }
    if (!expect_lit(buf, len, &pos, "]},\"id\":1}")) {
        return 0;
    }
    out->runner_count = count;
    return 1;
}

int betfair_parse_place_order(const uint8_t *buf, uint32_t len, betfair_place_order_t *out) {
    uint8_t *raw = (uint8_t *) out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }
    uint32_t pos = 0;
    if (!expect_lit(buf, len, &pos,
                     "{\"jsonrpc\":\"2.0\",\"method\":\"SportsAPING/v1.0/placeOrders\","
                     "\"params\":{\"marketId\":\"")) {
        return 0;
    }
    if (!parse_market_id(buf, len, &pos, out->market_id, &out->market_id_len)) {
        return 0;
    }
    if (!expect_lit(buf, len, &pos, "\",\"instructions\":[{\"selectionId\":")) {
        return 0;
    }
    if (!parse_uint_field(buf, len, &pos, &out->selection_id)) {
        return 0;
    }
    if (!expect_lit(buf, len, &pos,
                     ",\"handicap\":\"0\",\"side\":\"BACK\",\"orderType\":\"LIMIT\","
                     "\"limitOrder\":{\"size\":\"")) {
        return 0;
    }
    if (!parse_decimal_field(buf, len, &pos, &out->size_cents)) {
        return 0;
    }
    if (!expect_lit(buf, len, &pos, "\",\"price\":\"")) {
        return 0;
    }
    if (!parse_decimal_field(buf, len, &pos, &out->price_cents)) {
        return 0;
    }
    if (!expect_lit(buf, len, &pos,
                     "\",\"persistenceType\":\"LAPSE\"}}],\"customerRef\":\"UNIXOS43\"},\"id\":1}")) {
        return 0;
    }
    return 1;
}

int betfair_parse_place_order_response(const uint8_t *buf, uint32_t len, int *out_status,
                                       uint32_t *out_bet_id) {
    uint32_t pos = 0;
    if (!expect_lit(buf, len, &pos, "{\"jsonrpc\":\"2.0\",\"result\":{\"status\":\"")) {
        return 0;
    }
    int status1;
    if (expect_lit(buf, len, &pos, "SUCCESS")) {
        status1 = 1;
    } else if (expect_lit(buf, len, &pos, "FAILURE")) {
        status1 = 0;
    } else {
        return 0;
    }
    if (!expect_lit(buf, len, &pos, "\",\"instructionReports\":[{\"status\":\"")) {
        return 0;
    }
    int status2;
    if (expect_lit(buf, len, &pos, "SUCCESS")) {
        status2 = 1;
    } else if (expect_lit(buf, len, &pos, "FAILURE")) {
        status2 = 0;
    } else {
        return 0;
    }
    if (status1 != status2) {
        return 0;
    }
    if (!expect_lit(buf, len, &pos, "\",\"betId\":")) {
        return 0;
    }
    if (!parse_uint_field(buf, len, &pos, out_bet_id)) {
        return 0;
    }
    if (!expect_lit(buf, len, &pos, "}]},\"id\":1}")) {
        return 0;
    }
    *out_status = status1;
    return 1;
}
