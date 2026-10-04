/* See 049_veh.h's own top-of-file comment for the citation of every
 * real element/attribute name used here. */

#include "049_veh.h"

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
 * `RateTotalAmount` attribute: amounts here always use exactly 2
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

/* This chapter's own restricted subset of the real
 * `eVehicleTransmissionType` enumeration -- never the real
 * `NOTSPECIFIED` value. */
static int valid_transmission(const uint8_t *s, uint32_t len) {
    return lit_eq(s, len, "AUTOMATIC") || lit_eq(s, len, "MANUAL");
}

/* Four of the real `eFuelType` enumeration's own nine real values. */
static int valid_fuel_type(const uint8_t *s, uint32_t len) {
    return lit_eq(s, len, "PETROL") || lit_eq(s, len, "DIESEL") ||
           lit_eq(s, len, "ELECTRIC") || lit_eq(s, len, "HYBRID");
}

#define A(s) do { pos = append_str(out, pos, out_size, s); if (pos == 0xFFFFFFFFu) return 0; } while (0)
#define AB(b, n) do { pos = append_bytes(out, pos, out_size, b, n); if (pos == 0xFFFFFFFFu) return 0; } while (0)
#define AU(v) do { pos = append_uint(out, pos, out_size, v); if (pos == 0xFFFFFFFFu) return 0; } while (0)
#define AM(c) do { pos = append_amount(out, pos, out_size, c); if (pos == 0xFFFFFFFFu) return 0; } while (0)

static uint32_t build_rental_core(const veh_rental_core_t *core, uint8_t *out, uint32_t pos,
                                   uint32_t out_size) {
    A("<VehRentalCore PickUpDateTime=\"");
    AB(core->pickup_datetime, cstr_bytes_len(core->pickup_datetime, VEH_DATETIME_MAX - 1u));
    A("\" ReturnDateTime=\"");
    AB(core->return_datetime, cstr_bytes_len(core->return_datetime, VEH_DATETIME_MAX - 1u));
    A("\"><PickUpLocation LocationCode=\"");
    AB(core->pickup_location, cstr_bytes_len(core->pickup_location, VEH_LOC_MAX - 1u));
    A("\" CodeContext=\"IATA\"/><ReturnLocation LocationCode=\"");
    AB(core->return_location, cstr_bytes_len(core->return_location, VEH_LOC_MAX - 1u));
    A("\" CodeContext=\"IATA\"/></VehRentalCore>");
    return pos;
}

uint32_t veh_build_avail_request(const veh_avail_request_t *req, uint8_t *out,
                                  uint32_t out_size) {
    uint32_t pos = 0;
    A("<OTA_VehAvailRateRQ><VehAvailRQCore>");
    pos = build_rental_core(&req->core, out, pos, out_size);
    if (pos == 0u) return 0;
    A("</VehAvailRQCore></OTA_VehAvailRateRQ>");
    return pos;
}

/* Emits the real Vehicle (TransmissionType/FuelType/VehClass) and
 * TotalCharge fields shared, in the real schema, by both VehAvailCore
 * (inside an availability offer) and VehSegmentCore (inside a booking
 * confirmation) -- never the Vendor element, whose own real position
 * differs between those two real complex types (after TotalCharge in
 * VehAvailCore, but before Vehicle in VehSegmentCore). */
static uint32_t build_vehicle_and_charge(const veh_offer_t *o, uint8_t *out, uint32_t pos,
                                          uint32_t out_size) {
    uint32_t tlen = cstr_bytes_len(o->transmission, VEH_ENUM_MAX - 1u);
    uint32_t flen = cstr_bytes_len(o->fuel_type, VEH_ENUM_MAX - 1u);
    if (!valid_transmission(o->transmission, tlen) || !valid_fuel_type(o->fuel_type, flen)) {
        return 0u;
    }
    A("<Vehicle TransmissionType=\"");
    AB(o->transmission, tlen);
    A("\" FuelType=\"");
    AB(o->fuel_type, flen);
    A("\"><VehClass Size=\"");
    AB(o->vehicle_class, cstr_bytes_len(o->vehicle_class, VEH_CLASS_MAX - 1u));
    A("\"/></Vehicle><TotalCharge RateTotalAmount=\"");
    AM(o->rate_total_cents);
    A("\" CurrencyCode=\"");
    AB(o->currency_code, cstr_bytes_len(o->currency_code, VEH_CURRENCY_MAX - 1u));
    A("\"/>");
    return pos;
}

static uint32_t build_offer(const veh_offer_t *o, uint8_t *out, uint32_t pos, uint32_t out_size) {
    A("<VehAvailCore Status=\"Available\">");
    pos = build_vehicle_and_charge(o, out, pos, out_size);
    if (pos == 0u) return 0;
    A("<Vendor CompanyShortName=\"");
    AB(o->vendor_name, cstr_bytes_len(o->vendor_name, VEH_VENDOR_MAX - 1u));
    A("\"/></VehAvailCore>");
    return pos;
}

uint32_t veh_build_avail_response(const veh_avail_response_t *resp, uint8_t *out,
                                   uint32_t out_size) {
    if (resp->count > VEH_MAX_OFFERS) {
        return 0;
    }
    uint32_t pos = 0;
    A("<OTA_VehAvailRateRS><VehAvailRSCore>");
    pos = build_rental_core(&resp->core, out, pos, out_size);
    if (pos == 0u) return 0;
    A("<VehVendorAvails>");
    for (uint32_t i = 0; i < resp->count; i++) {
        A("<VehVendorAvail><VehAvails><VehAvail>");
        pos = build_offer(&resp->offers[i], out, pos, out_size);
        if (pos == 0u) return 0;
        A("</VehAvail></VehAvails></VehVendorAvail>");
    }
    A("</VehVendorAvails></VehAvailRSCore></OTA_VehAvailRateRS>");
    return pos;
}

uint32_t veh_build_res_request(const veh_res_request_t *req, uint8_t *out, uint32_t out_size) {
    uint32_t pos = 0;
    A("<OTA_VehResRQ><VehResRQCore Status=\"Available\">");
    pos = build_rental_core(&req->core, out, pos, out_size);
    if (pos == 0u) return 0;
    A("<VendorPref CompanyShortName=\"");
    AB(req->chosen.vendor_name, cstr_bytes_len(req->chosen.vendor_name, VEH_VENDOR_MAX - 1u));
    A("\"/><TotalCharge RateTotalAmount=\"");
    AM(req->chosen.rate_total_cents);
    A("\" CurrencyCode=\"");
    AB(req->chosen.currency_code, cstr_bytes_len(req->chosen.currency_code,
                                                  VEH_CURRENCY_MAX - 1u));
    A("\"/></VehResRQCore></OTA_VehResRQ>");
    return pos;
}

uint32_t veh_build_res_response(const uint8_t *conf_id, uint32_t conf_id_len,
                                 const veh_offer_t *confirmed, uint8_t *out, uint32_t out_size) {
    if (conf_id_len >= VEH_CONF_ID_MAX) {
        return 0;
    }
    uint32_t pos = 0;
    A("<OTA_VehResRS><VehResRSCore><VehReservation ReservationStatus=\"RESERVED\">"
      "<VehSegmentCore><ConfID ID=\"");
    AB(conf_id, conf_id_len);
    A("\" Type=\"1\"/><Vendor CompanyShortName=\"");
    AB(confirmed->vendor_name, cstr_bytes_len(confirmed->vendor_name, VEH_VENDOR_MAX - 1u));
    A("\"/>");
    pos = build_vehicle_and_charge(confirmed, out, pos, out_size);
    if (pos == 0u) return 0;
    A("</VehSegmentCore></VehReservation></VehResRSCore></OTA_VehResRS>");
    return pos;
}

#undef A
#undef AB
#undef AU
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

static int is_digit(uint8_t c) {
    return c >= (uint8_t) '0' && c <= (uint8_t) '9';
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

static int parse_rental_core(const uint8_t *buf, uint32_t len, uint32_t *pos,
                              veh_rental_core_t *core) {
    if (!match_literal(buf, len, pos, "<VehRentalCore PickUpDateTime=\"")) return 0;
    uint32_t n = read_until(buf, len, pos, (uint8_t) '"', core->pickup_datetime,
                            VEH_DATETIME_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, pos, "\" ReturnDateTime=\"")) return 0;
    n = read_until(buf, len, pos, (uint8_t) '"', core->return_datetime, VEH_DATETIME_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, pos, "\"><PickUpLocation LocationCode=\"")) return 0;
    n = read_until(buf, len, pos, (uint8_t) '"', core->pickup_location, VEH_LOC_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, pos, "\" CodeContext=\"IATA\"/><ReturnLocation LocationCode=\""))
        return 0;
    n = read_until(buf, len, pos, (uint8_t) '"', core->return_location, VEH_LOC_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, pos, "\" CodeContext=\"IATA\"/></VehRentalCore>")) return 0;
    return 1;
}

/* Mirrors build_vehicle_and_charge() exactly -- see its own comment
 * above for why Vendor is never parsed here either. */
static int parse_vehicle_and_charge(const uint8_t *buf, uint32_t len, uint32_t *pos,
                                     veh_offer_t *o) {
    if (!match_literal(buf, len, pos, "<Vehicle TransmissionType=\"")) return 0;
    uint32_t n = read_until(buf, len, pos, (uint8_t) '"', o->transmission, VEH_ENUM_MAX - 1u);
    if (n == 0xFFFFFFFFu || !valid_transmission(o->transmission, n)) return 0;
    if (!match_literal(buf, len, pos, "\" FuelType=\"")) return 0;
    n = read_until(buf, len, pos, (uint8_t) '"', o->fuel_type, VEH_ENUM_MAX - 1u);
    if (n == 0xFFFFFFFFu || !valid_fuel_type(o->fuel_type, n)) return 0;
    if (!match_literal(buf, len, pos, "\"><VehClass Size=\"")) return 0;
    n = read_until(buf, len, pos, (uint8_t) '"', o->vehicle_class, VEH_CLASS_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, pos, "\"/></Vehicle><TotalCharge RateTotalAmount=\"")) return 0;
    uint8_t text[16];
    n = read_until(buf, len, pos, (uint8_t) '"', text, sizeof(text));
    if (n == 0xFFFFFFFFu || !parse_amount(text, n, &o->rate_total_cents)) return 0;
    if (!match_literal(buf, len, pos, "\" CurrencyCode=\"")) return 0;
    n = read_until(buf, len, pos, (uint8_t) '"', o->currency_code, VEH_CURRENCY_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, pos, "\"/>")) return 0;
    return 1;
}

static int parse_offer(const uint8_t *buf, uint32_t len, uint32_t *pos, veh_offer_t *o) {
    if (!match_literal(buf, len, pos, "<VehAvailCore Status=\"Available\">")) return 0;
    if (!parse_vehicle_and_charge(buf, len, pos, o)) return 0;
    if (!match_literal(buf, len, pos, "<Vendor CompanyShortName=\"")) return 0;
    uint32_t n = read_until(buf, len, pos, (uint8_t) '"', o->vendor_name, VEH_VENDOR_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, pos, "\"/></VehAvailCore>")) return 0;
    return 1;
}

int veh_parse_avail_request(const uint8_t *buf, uint32_t len, veh_avail_request_t *out) {
    uint8_t *raw = (uint8_t *) out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }
    uint32_t pos = 0;
    if (!match_literal(buf, len, &pos, "<OTA_VehAvailRateRQ><VehAvailRQCore>")) return 0;
    if (!parse_rental_core(buf, len, &pos, &out->core)) return 0;
    if (!match_literal(buf, len, &pos, "</VehAvailRQCore></OTA_VehAvailRateRQ>")) return 0;
    return pos == len;
}

int veh_parse_avail_response(const uint8_t *buf, uint32_t len, veh_avail_response_t *out) {
    uint8_t *raw = (uint8_t *) out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }
    uint32_t pos = 0;
    if (!match_literal(buf, len, &pos, "<OTA_VehAvailRateRS><VehAvailRSCore>")) return 0;
    if (!parse_rental_core(buf, len, &pos, &out->core)) return 0;
    if (!match_literal(buf, len, &pos, "<VehVendorAvails>")) return 0;
    uint32_t count = 0;
    while (match_literal(buf, len, &pos, "<VehVendorAvail><VehAvails><VehAvail>")) {
        if (count >= VEH_MAX_OFFERS) return 0;
        if (!parse_offer(buf, len, &pos, &out->offers[count])) return 0;
        if (!match_literal(buf, len, &pos, "</VehAvail></VehAvails></VehVendorAvail>")) return 0;
        count++;
    }
    out->count = count;
    if (!match_literal(buf, len, &pos, "</VehVendorAvails></VehAvailRSCore></OTA_VehAvailRateRS>"))
        return 0;
    return pos == len;
}

int veh_parse_res_request(const uint8_t *buf, uint32_t len, veh_res_request_t *out) {
    uint8_t *raw = (uint8_t *) out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }
    uint32_t pos = 0;
    if (!match_literal(buf, len, &pos, "<OTA_VehResRQ><VehResRQCore Status=\"Available\">"))
        return 0;
    if (!parse_rental_core(buf, len, &pos, &out->core)) return 0;
    if (!match_literal(buf, len, &pos, "<VendorPref CompanyShortName=\"")) return 0;
    uint32_t n = read_until(buf, len, &pos, (uint8_t) '"', out->chosen.vendor_name,
                            VEH_VENDOR_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, &pos, "\"/><TotalCharge RateTotalAmount=\"")) return 0;
    uint8_t text[16];
    n = read_until(buf, len, &pos, (uint8_t) '"', text, sizeof(text));
    if (n == 0xFFFFFFFFu || !parse_amount(text, n, &out->chosen.rate_total_cents)) return 0;
    if (!match_literal(buf, len, &pos, "\" CurrencyCode=\"")) return 0;
    n = read_until(buf, len, &pos, (uint8_t) '"', out->chosen.currency_code,
                   VEH_CURRENCY_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, &pos, "\"/></VehResRQCore></OTA_VehResRQ>")) return 0;
    return pos == len;
}

int veh_parse_res_response(const uint8_t *buf, uint32_t len, veh_res_response_t *out) {
    uint8_t *raw = (uint8_t *) out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }
    uint32_t pos = 0;
    if (!match_literal(buf, len, &pos,
                        "<OTA_VehResRS><VehResRSCore><VehReservation "
                        "ReservationStatus=\"RESERVED\"><VehSegmentCore><ConfID ID=\""))
        return 0;
    uint32_t n = read_until(buf, len, &pos, (uint8_t) '"', out->conf_id, VEH_CONF_ID_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, &pos, "\" Type=\"1\"/><Vendor CompanyShortName=\"")) return 0;
    n = read_until(buf, len, &pos, (uint8_t) '"', out->confirmed.vendor_name,
                   VEH_VENDOR_MAX - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, &pos, "\"/>")) return 0;
    if (!parse_vehicle_and_charge(buf, len, &pos, &out->confirmed)) return 0;
    if (!match_literal(buf, len, &pos, "</VehSegmentCore></VehReservation></VehResRSCore>"
                                        "</OTA_VehResRS>"))
        return 0;
    return pos == len;
}
