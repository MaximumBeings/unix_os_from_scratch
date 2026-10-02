/* See 047_hotel.h's own top-of-file comment for the citation of every
 * real element/attribute name used here. */

#include "047_hotel.h"

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

/* This chapter's own stated restriction on the real xs:decimal
 * `amount`/`commission` attributes: amounts here always use exactly 2
 * fraction digits, matching this book's own internal whole-cents
 * amounts. */
static uint32_t append_amount(uint8_t *out, uint32_t pos, uint32_t out_size, uint32_t cents) {
    pos = append_uint(out, pos, out_size, cents / 100u);
    if (pos == 0xFFFFFFFFu) return pos;
    if (pos + 3u > out_size) {
        return 0xFFFFFFFFu;
    }
    out[pos++] = '.';
    out[pos++] = (uint8_t)('0' + ((cents / 10u) % 10u));
    out[pos++] = (uint8_t)('0' + (cents % 10u));
    return pos;
}

static uint32_t cstr_bytes_len(const uint8_t *s, uint32_t max) {
    uint32_t n = 0;
    while (n < max && s[n] != 0) {
        n++;
    }
    return n;
}

static int lit_eq(const uint8_t *s, uint32_t len, const char *lit) {
    uint32_t i = 0;
    while (lit[i] != '\0') {
        if (i >= len || s[i] != (uint8_t) lit[i]) {
            return 0;
        }
        i++;
    }
    return i == len;
}

/* This chapter's own restricted subset of the real `eTipoOpcion`
 * enumeration -- 2 of its own real 13 values: plain lodging, and
 * lodging bundled with one real tourist attraction/activity. */
static int valid_option_type(const uint8_t *s, uint32_t len) {
    return lit_eq(s, len, "Hotel") || lit_eq(s, len, "HotelActivity");
}

/* This chapter's own restriction on the real `eTipoFormaPago`
 * enumeration -- always the real `MerchantPay` value, one of its own
 * real 5 values. */
static int valid_payment_type(const uint8_t *s, uint32_t len) {
    return lit_eq(s, len, "MerchantPay");
}

static int is_digit(uint8_t c) {
    return c >= (uint8_t) '0' && c <= (uint8_t) '9';
}

/* This chapter's own restriction on the real xs:string `StartDate`/
 * `EndDate` fields: always exactly `YYYY-MM-DD` (10 characters),
 * never the real schema's own unrestricted free-form string. */
static int valid_date(const uint8_t *s, uint32_t len) {
    if (len != 10u) return 0;
    for (uint32_t i = 0; i < 10u; i++) {
        if (i == 4u || i == 7u) {
            if (s[i] != (uint8_t) '-') return 0;
        } else if (!is_digit(s[i])) {
            return 0;
        }
    }
    return 1;
}

#define A(s) do { pos = append_str(out, pos, out_size, s); if (pos == 0xFFFFFFFFu) return 0; } while (0)
#define AB(b, n) do { pos = append_bytes(out, pos, out_size, b, n); if (pos == 0xFFFFFFFFu) return 0; } while (0)
#define AM(c) do { pos = append_amount(out, pos, out_size, c); if (pos == 0xFFFFFFFFu) return 0; } while (0)

static uint32_t build_stay_core(const hotel_stay_core_t *core, uint8_t *out, uint32_t pos,
                                 uint32_t out_size) {
    uint32_t slen = cstr_bytes_len(core->start_date, HOTEL_DATE_MAX - 1u);
    uint32_t elen = cstr_bytes_len(core->end_date, HOTEL_DATE_MAX - 1u);
    if (!valid_date(core->start_date, slen) || !valid_date(core->end_date, elen)) {
        return 0u;
    }
    A("<StartDate>");
    AB(core->start_date, slen);
    A("</StartDate><EndDate>");
    AB(core->end_date, elen);
    A("</EndDate>");
    return pos;
}

uint32_t hotel_build_avail_request(const hotel_avail_request_t *req, uint8_t *out,
                                    uint32_t out_size) {
    uint32_t pos = 0;
    A("<AvailRQ>");
    pos = build_stay_core(&req->core, out, pos, out_size);
    if (pos == 0u) return 0;
    A("</AvailRQ>");
    return pos;
}

static uint32_t build_offer(const hotel_offer_t *o, uint8_t *out, uint32_t pos, uint32_t out_size) {
    uint32_t otlen = cstr_bytes_len(o->option_type, HOTEL_OPTYPE_MAX - 1u);
    uint32_t ptlen = cstr_bytes_len(o->payment_type, HOTEL_PAYTYPE_MAX - 1u);
    if (!valid_option_type(o->option_type, otlen) || !valid_payment_type(o->payment_type, ptlen)) {
        return 0u;
    }
    A("<Hotel code=\"");
    AB(o->hotel_code, cstr_bytes_len(o->hotel_code, HOTEL_CODE_MAX - 1u));
    A("\" name=\"");
    AB(o->hotel_name, cstr_bytes_len(o->hotel_name, HOTEL_NAME_MAX - 1u));
    A("\"><MealPlan code=\"");
    AB(o->meal_plan_code, cstr_bytes_len(o->meal_plan_code, HOTEL_MEALPLAN_MAX - 1u));
    A("\"><Option supplierCode=\"");
    AB(o->supplier_code, cstr_bytes_len(o->supplier_code, HOTEL_NAME_MAX - 1u));
    A("\" type=\"");
    AB(o->option_type, otlen);
    A("\" paymentType=\"");
    AB(o->payment_type, ptlen);
    A("\"><Room code=\"");
    AB(o->room_code, cstr_bytes_len(o->room_code, HOTEL_CODE_MAX - 1u));
    A("\" description=\"");
    AB(o->room_description, cstr_bytes_len(o->room_description, HOTEL_ROOM_DESC_MAX - 1u));
    A("\" nonRefundable=\"");
    A(o->non_refundable ? "true" : "false");
    A("\"><Price currency=\"");
    AB(o->currency_code, cstr_bytes_len(o->currency_code, HOTEL_CURRENCY_MAX - 1u));
    A("\" amount=\"");
    AM(o->rate_total_cents);
    A("\" commission=\"");
    AM(o->commission_cents);
    A("\"/></Room></Option></MealPlan></Hotel>");
    return pos;
}

uint32_t hotel_build_avail_response(const hotel_avail_response_t *resp, uint8_t *out,
                                     uint32_t out_size) {
    if (resp->count > HOTEL_MAX_OFFERS) {
        return 0;
    }
    uint32_t pos = 0;
    A("<AvailRS>");
    pos = build_stay_core(&resp->core, out, pos, out_size);
    if (pos == 0u) return 0;
    A("<Hotels>");
    for (uint32_t i = 0; i < resp->count; i++) {
        pos = build_offer(&resp->offers[i], out, pos, out_size);
        if (pos == 0u) return 0;
    }
    A("</Hotels></AvailRS>");
    return pos;
}

/* Mirrors the real schema's own actual `ReservationRQ` shape exactly:
 * only `HotelCode`/`MealPlanCode`/`Price` identify the chosen option
 * on the wire -- the real schema has no field in `ReservationRQ` for
 * the option's own `@type`/room code/description at all. */
uint32_t hotel_build_res_request(const hotel_res_request_t *req, uint8_t *out, uint32_t out_size) {
    uint32_t ptlen = cstr_bytes_len(req->chosen.payment_type, HOTEL_PAYTYPE_MAX - 1u);
    if (!valid_payment_type(req->chosen.payment_type, ptlen)) {
        return 0u;
    }
    uint32_t pos = 0;
    A("<ReservationRQ><ClientLocator>");
    AB(req->client_locator, cstr_bytes_len(req->client_locator, HOTEL_LOCATOR_MAX - 1u));
    A("</ClientLocator>");
    pos = build_stay_core(&req->core, out, pos, out_size);
    if (pos == 0u) return 0;
    A("<HotelCode>");
    AB(req->chosen.hotel_code, cstr_bytes_len(req->chosen.hotel_code, HOTEL_CODE_MAX - 1u));
    A("</HotelCode><MealPlanCode>");
    AB(req->chosen.meal_plan_code, cstr_bytes_len(req->chosen.meal_plan_code,
                                                   HOTEL_MEALPLAN_MAX - 1u));
    A("</MealPlanCode><Price currency=\"");
    AB(req->chosen.currency_code, cstr_bytes_len(req->chosen.currency_code,
                                                  HOTEL_CURRENCY_MAX - 1u));
    A("\" amount=\"");
    AM(req->chosen.rate_total_cents);
    A("\"/><PaymentType>");
    AB(req->chosen.payment_type, ptlen);
    A("</PaymentType><ResGuests><Guests><Guest roomCandidateId=\"1\" paxId=\"1\">"
      "<GivenName>");
    AB(req->guest.given_name, cstr_bytes_len(req->guest.given_name, HOTEL_GUEST_NAME_MAX - 1u));
    A("</GivenName><SurName>");
    AB(req->guest.sur_name, cstr_bytes_len(req->guest.sur_name, HOTEL_GUEST_NAME_MAX - 1u));
    A("</SurName></Guest></Guests></ResGuests></ReservationRQ>");
    return pos;
}

/* Mirrors the real schema's own actual `ReservationRS` shape: carries
 * back only `ProviderLocator`/`PropertyReservationNumber`/`ResStatus`/
 * `Price` -- nothing else about the confirmed option. */
uint32_t hotel_build_res_response(const uint8_t *provider_locator, uint32_t provider_locator_len,
                                   const uint8_t *property_reservation_number,
                                   uint32_t property_reservation_number_len,
                                   const hotel_offer_t *confirmed, uint8_t *out,
                                   uint32_t out_size) {
    if (provider_locator_len >= HOTEL_LOCATOR_MAX ||
        property_reservation_number_len >= HOTEL_LOCATOR_MAX) {
        return 0;
    }
    uint32_t pos = 0;
    A("<ReservationRS><ProviderLocator>");
    AB(provider_locator, provider_locator_len);
    A("</ProviderLocator><PropertyReservationNumber>");
    AB(property_reservation_number, property_reservation_number_len);
    A("</PropertyReservationNumber><ResStatus>OK</ResStatus><Price currency=\"");
    AB(confirmed->currency_code, cstr_bytes_len(confirmed->currency_code,
                                                 HOTEL_CURRENCY_MAX - 1u));
    A("\" amount=\"");
    AM(confirmed->rate_total_cents);
    A("\"/></ReservationRS>");
    return pos;
}

#undef A
#undef AB
#undef AM

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

static uint32_t read_until(const uint8_t *buf, uint32_t len, uint32_t *pos, uint8_t delim,
                            uint8_t *out, uint32_t max) {
    uint32_t n = 0;
    while (*pos < len && buf[*pos] != delim) {
        if (n >= max) {
            return 0xFFFFFFFFu;
        }
        out[n++] = buf[*pos];
        (*pos)++;
    }
    if (*pos >= len) {
        return 0xFFFFFFFFu;
    }
    return n;
}

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

static int parse_stay_core(const uint8_t *buf, uint32_t len, uint32_t *pos,
                            hotel_stay_core_t *core) {
    if (!match_literal(buf, len, pos, "<StartDate>")) return 0;
    uint32_t n = read_until(buf, len, pos, (uint8_t) '<', core->start_date, HOTEL_DATE_MAX - 1u);
    if (n == 0xFFFFFFFFu || !valid_date(core->start_date, n)) return 0;
    if (!match_literal(buf, len, pos, "</StartDate><EndDate>")) return 0;
    n = read_until(buf, len, pos, (uint8_t) '<', core->end_date, HOTEL_DATE_MAX - 1u);
    if (n == 0xFFFFFFFFu || !valid_date(core->end_date, n)) return 0;
    if (!match_literal(buf, len, pos, "</EndDate>")) return 0;
    return 1;
}

int hotel_parse_avail_request(const uint8_t *buf, uint32_t len, hotel_avail_request_t *out) {
    uint8_t *raw = (uint8_t *) out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }
    uint32_t pos = 0;
    if (!match_literal(buf, len, &pos, "<AvailRQ>")) return 0;
    if (!parse_stay_core(buf, len, &pos, &out->core)) return 0;
    if (!match_literal(buf, len, &pos, "</AvailRQ>")) return 0;
    return pos == len;
}

static int parse_offer(const uint8_t *buf, uint32_t len, uint32_t *pos, hotel_offer_t *o) {
    if (!match_literal(buf, len, pos, "<Hotel code=\"")) return 0;
    uint32_t n = read_until(buf, len, pos, (uint8_t) '"', o->hotel_code, HOTEL_CODE_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, pos, "\" name=\"")) return 0;
    n = read_until(buf, len, pos, (uint8_t) '"', o->hotel_name, HOTEL_NAME_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, pos, "\"><MealPlan code=\"")) return 0;
    n = read_until(buf, len, pos, (uint8_t) '"', o->meal_plan_code, HOTEL_MEALPLAN_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, pos, "\"><Option supplierCode=\"")) return 0;
    n = read_until(buf, len, pos, (uint8_t) '"', o->supplier_code, HOTEL_NAME_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, pos, "\" type=\"")) return 0;
    n = read_until(buf, len, pos, (uint8_t) '"', o->option_type, HOTEL_OPTYPE_MAX - 1u);
    if (n == 0xFFFFFFFFu || !valid_option_type(o->option_type, n)) return 0;
    if (!match_literal(buf, len, pos, "\" paymentType=\"")) return 0;
    n = read_until(buf, len, pos, (uint8_t) '"', o->payment_type, HOTEL_PAYTYPE_MAX - 1u);
    if (n == 0xFFFFFFFFu || !valid_payment_type(o->payment_type, n)) return 0;
    if (!match_literal(buf, len, pos, "\"><Room code=\"")) return 0;
    n = read_until(buf, len, pos, (uint8_t) '"', o->room_code, HOTEL_CODE_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, pos, "\" description=\"")) return 0;
    n = read_until(buf, len, pos, (uint8_t) '"', o->room_description, HOTEL_ROOM_DESC_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, pos, "\" nonRefundable=\"")) return 0;
    if (match_literal(buf, len, pos, "true")) {
        o->non_refundable = 1;
    } else if (match_literal(buf, len, pos, "false")) {
        o->non_refundable = 0;
    } else {
        return 0;
    }
    if (!match_literal(buf, len, pos, "\"><Price currency=\"")) return 0;
    n = read_until(buf, len, pos, (uint8_t) '"', o->currency_code, HOTEL_CURRENCY_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, pos, "\" amount=\"")) return 0;
    uint8_t text[16];
    n = read_until(buf, len, pos, (uint8_t) '"', text, sizeof(text));
    if (n == 0xFFFFFFFFu || !parse_amount(text, n, &o->rate_total_cents)) return 0;
    if (!match_literal(buf, len, pos, "\" commission=\"")) return 0;
    n = read_until(buf, len, pos, (uint8_t) '"', text, sizeof(text));
    if (n == 0xFFFFFFFFu || !parse_amount(text, n, &o->commission_cents)) return 0;
    if (!match_literal(buf, len, pos, "\"/></Room></Option></MealPlan></Hotel>")) return 0;
    return 1;
}

int hotel_parse_avail_response(const uint8_t *buf, uint32_t len, hotel_avail_response_t *out) {
    uint8_t *raw = (uint8_t *) out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }
    uint32_t pos = 0;
    if (!match_literal(buf, len, &pos, "<AvailRS>")) return 0;
    if (!parse_stay_core(buf, len, &pos, &out->core)) return 0;
    if (!match_literal(buf, len, &pos, "<Hotels>")) return 0;
    uint32_t count = 0;
    while (!match_literal(buf, len, &pos, "</Hotels></AvailRS>")) {
        if (count >= HOTEL_MAX_OFFERS) return 0;
        if (!parse_offer(buf, len, &pos, &out->offers[count])) return 0;
        count++;
    }
    out->count = count;
    return pos == len;
}

int hotel_parse_res_request(const uint8_t *buf, uint32_t len, hotel_res_request_t *out) {
    uint8_t *raw = (uint8_t *) out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }
    uint32_t pos = 0;
    if (!match_literal(buf, len, &pos, "<ReservationRQ><ClientLocator>")) return 0;
    uint32_t n = read_until(buf, len, &pos, (uint8_t) '<', out->client_locator,
                            HOTEL_LOCATOR_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, &pos, "</ClientLocator>")) return 0;
    if (!parse_stay_core(buf, len, &pos, &out->core)) return 0;
    if (!match_literal(buf, len, &pos, "<HotelCode>")) return 0;
    n = read_until(buf, len, &pos, (uint8_t) '<', out->chosen.hotel_code, HOTEL_CODE_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, &pos, "</HotelCode><MealPlanCode>")) return 0;
    n = read_until(buf, len, &pos, (uint8_t) '<', out->chosen.meal_plan_code,
                   HOTEL_MEALPLAN_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, &pos, "</MealPlanCode><Price currency=\"")) return 0;
    n = read_until(buf, len, &pos, (uint8_t) '"', out->chosen.currency_code,
                   HOTEL_CURRENCY_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, &pos, "\" amount=\"")) return 0;
    uint8_t text[16];
    n = read_until(buf, len, &pos, (uint8_t) '"', text, sizeof(text));
    if (n == 0xFFFFFFFFu || !parse_amount(text, n, &out->chosen.rate_total_cents)) return 0;
    if (!match_literal(buf, len, &pos, "\"/><PaymentType>")) return 0;
    n = read_until(buf, len, &pos, (uint8_t) '<', out->chosen.payment_type,
                   HOTEL_PAYTYPE_MAX - 1u);
    if (n == 0xFFFFFFFFu || !valid_payment_type(out->chosen.payment_type, n)) return 0;
    if (!match_literal(buf, len, &pos,
                        "</PaymentType><ResGuests><Guests><Guest roomCandidateId=\"1\" "
                        "paxId=\"1\"><GivenName>"))
        return 0;
    n = read_until(buf, len, &pos, (uint8_t) '<', out->guest.given_name,
                   HOTEL_GUEST_NAME_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, &pos, "</GivenName><SurName>")) return 0;
    n = read_until(buf, len, &pos, (uint8_t) '<', out->guest.sur_name,
                   HOTEL_GUEST_NAME_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, &pos, "</SurName></Guest></Guests></ResGuests></ReservationRQ>"))
        return 0;
    return pos == len;
}

int hotel_parse_res_response(const uint8_t *buf, uint32_t len, hotel_res_response_t *out) {
    uint8_t *raw = (uint8_t *) out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }
    uint32_t pos = 0;
    if (!match_literal(buf, len, &pos, "<ReservationRS><ProviderLocator>")) return 0;
    uint32_t n = read_until(buf, len, &pos, (uint8_t) '<', out->provider_locator,
                            HOTEL_LOCATOR_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, &pos, "</ProviderLocator><PropertyReservationNumber>")) return 0;
    n = read_until(buf, len, &pos, (uint8_t) '<', out->property_reservation_number,
                   HOTEL_LOCATOR_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, &pos,
                        "</PropertyReservationNumber><ResStatus>OK</ResStatus><Price currency=\""))
        return 0;
    n = read_until(buf, len, &pos, (uint8_t) '"', out->confirmed.currency_code,
                   HOTEL_CURRENCY_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, &pos, "\" amount=\"")) return 0;
    uint8_t text[16];
    n = read_until(buf, len, &pos, (uint8_t) '"', text, sizeof(text));
    if (n == 0xFFFFFFFFu || !parse_amount(text, n, &out->confirmed.rate_total_cents)) return 0;
    if (!match_literal(buf, len, &pos, "\"/></ReservationRS>")) return 0;
    return pos == len;
}
