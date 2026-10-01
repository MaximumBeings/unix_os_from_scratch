/* See 046_ota.h's own top-of-file comment for the citation of every
 * real element/attribute name used here. */

#include "046_ota.h"

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

/* This chapter's own stated restriction on the real `Money` type
 * (xs:decimal, up to 3 real fraction digits): amounts here always use
 * exactly 2, matching this book's own internal whole-cents amounts. */
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

#define A(s) do { pos = append_str(out, pos, out_size, s); if (pos == 0xFFFFFFFFu) return 0; } while (0)
#define AB(b, n) do { pos = append_bytes(out, pos, out_size, b, n); if (pos == 0xFFFFFFFFu) return 0; } while (0)
#define AU(v) do { pos = append_uint(out, pos, out_size, v); if (pos == 0xFFFFFFFFu) return 0; } while (0)
#define AM(c) do { pos = append_amount(out, pos, out_size, c); if (pos == 0xFFFFFFFFu) return 0; } while (0)

static uint32_t cstr_bytes_len(const uint8_t *s, uint32_t max) {
    uint32_t n = 0;
    while (n < max && s[n] != 0) {
        n++;
    }
    return n;
}

uint32_t ota_build_request(const ota_search_request_t *req, uint8_t *out, uint32_t out_size) {
    uint32_t pos = 0;
    A("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    A("<OTA_AirLowFareSearchRQ xmlns=\"http://www.opentravel.org/OTA/2003/05\">\n");
    A("<OriginDestinationInformation><DepartureDateTime>");
    AB(req->departure_date, 10u);
    A("</DepartureDateTime><OriginLocation LocationCode=\"");
    AB(req->origin, cstr_bytes_len(req->origin, sizeof(req->origin)));
    A("\"/><DestinationLocation LocationCode=\"");
    AB(req->destination, cstr_bytes_len(req->destination, sizeof(req->destination)));
    A("\"/></OriginDestinationInformation>\n");
    A("<TravelerInfoSummary><AirTravelerAvail><PassengerTypeQuantity Code=\"ADT\" "
      "Quantity=\"");
    AU(req->adt_quantity);
    A("\"/></AirTravelerAvail></TravelerInfoSummary>\n");
    A("</OTA_AirLowFareSearchRQ>\n");
    return pos;
}

uint32_t ota_build_response(const ota_search_response_t *resp, uint8_t *out, uint32_t out_size) {
    if (resp->count > OTA_MAX_ITINERARIES) {
        return 0;
    }
    uint32_t pos = 0;
    A("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    A("<OTA_AirLowFareSearchRS xmlns=\"http://www.opentravel.org/OTA/2003/05\">\n");
    A("<PricedItineraries>\n");
    for (uint32_t i = 0; i < resp->count; i++) {
        const ota_priced_itinerary_t *it = &resp->itineraries[i];
        A("<PricedItinerary><AirItinerary><FlightSegment DepartureDateTime=\"");
        AB(it->departure_datetime, cstr_bytes_len(it->departure_datetime,
                                                   sizeof(it->departure_datetime)));
        A("\" ArrivalDateTime=\"");
        AB(it->arrival_datetime, cstr_bytes_len(it->arrival_datetime,
                                                 sizeof(it->arrival_datetime)));
        A("\" StopQuantity=\"");
        AU(it->stop_quantity);
        A("\" FlightNumber=\"");
        AB(it->flight_number, cstr_bytes_len(it->flight_number, sizeof(it->flight_number)));
        A("\"><DepartureAirport LocationCode=\"");
        AB(it->departure_airport, cstr_bytes_len(it->departure_airport,
                                                  sizeof(it->departure_airport)));
        A("\"/><ArrivalAirport LocationCode=\"");
        AB(it->arrival_airport, cstr_bytes_len(it->arrival_airport,
                                                sizeof(it->arrival_airport)));
        A("\"/><MarketingAirline Code=\"");
        AB(it->marketing_airline_code, cstr_bytes_len(it->marketing_airline_code,
                                                       sizeof(it->marketing_airline_code)));
        A("\"/></FlightSegment></AirItinerary>");
        A("<AirItineraryPricingInfo><ItinTotalFare><BaseFare Amount=\"");
        AM(it->base_fare_cents);
        A("\" CurrencyCode=\"");
        AB(it->currency_code, cstr_bytes_len(it->currency_code, sizeof(it->currency_code)));
        A("\"/><TotalFare Amount=\"");
        AM(it->total_fare_cents);
        A("\" CurrencyCode=\"");
        AB(it->currency_code, cstr_bytes_len(it->currency_code, sizeof(it->currency_code)));
        A("\"/></ItinTotalFare></AirItineraryPricingInfo></PricedItinerary>\n");
    }
    A("</PricedItineraries>\n</OTA_AirLowFareSearchRS>\n");
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

int ota_parse_request(const uint8_t *buf, uint32_t len, ota_search_request_t *out) {
    uint8_t *raw = (uint8_t *) out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }
    uint32_t pos = 0;
    uint8_t text[16];
    uint32_t n;

    if (!match_literal(buf, len, &pos, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")) return 0;
    if (!match_literal(buf, len, &pos,
                        "<OTA_AirLowFareSearchRQ xmlns=\"http://www.opentravel.org/OTA/2003/05"
                        "\">\n<OriginDestinationInformation><DepartureDateTime>")) return 0;
    n = read_until(buf, len, &pos, (uint8_t) '<', out->departure_date, 10u);
    if (n != 10u) return 0;
    if (!match_literal(buf, len, &pos, "</DepartureDateTime><OriginLocation LocationCode=\""))
        return 0;
    n = read_until(buf, len, &pos, (uint8_t) '"', out->origin, sizeof(out->origin) - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, &pos, "\"/><DestinationLocation LocationCode=\"")) return 0;
    n = read_until(buf, len, &pos, (uint8_t) '"', out->destination, sizeof(out->destination) - 1u);
    if (n == 0xFFFFFFFFu) return 0;
    if (!match_literal(buf, len, &pos,
                        "\"/></OriginDestinationInformation>\n<TravelerInfoSummary>"
                        "<AirTravelerAvail><PassengerTypeQuantity Code=\"ADT\" Quantity=\""))
        return 0;
    n = read_until(buf, len, &pos, (uint8_t) '"', text, sizeof(text));
    if (n == 0xFFFFFFFFu || !parse_uint(text, n, &out->adt_quantity)) return 0;
    if (!match_literal(buf, len, &pos,
                        "\"/></AirTravelerAvail></TravelerInfoSummary>\n"
                        "</OTA_AirLowFareSearchRQ>\n")) return 0;
    return pos == len;
}

int ota_parse_response(const uint8_t *buf, uint32_t len, ota_search_response_t *out) {
    uint8_t *raw = (uint8_t *) out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }
    uint32_t pos = 0;
    uint8_t text[16];
    uint32_t n;

    if (!match_literal(buf, len, &pos, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")) return 0;
    if (!match_literal(buf, len, &pos,
                        "<OTA_AirLowFareSearchRS xmlns=\"http://www.opentravel.org/OTA/2003/05"
                        "\">\n<PricedItineraries>\n")) return 0;

    uint32_t count = 0;
    while (match_literal(buf, len, &pos, "<PricedItinerary><AirItinerary><FlightSegment "
                                          "DepartureDateTime=\"")) {
        if (count >= OTA_MAX_ITINERARIES) {
            return 0;
        }
        ota_priced_itinerary_t *it = &out->itineraries[count];
        n = read_until(buf, len, &pos, (uint8_t) '"', it->departure_datetime,
                       sizeof(it->departure_datetime) - 1u);
        if (n == 0xFFFFFFFFu) return 0;
        if (!match_literal(buf, len, &pos, "\" ArrivalDateTime=\"")) return 0;
        n = read_until(buf, len, &pos, (uint8_t) '"', it->arrival_datetime,
                       sizeof(it->arrival_datetime) - 1u);
        if (n == 0xFFFFFFFFu) return 0;
        if (!match_literal(buf, len, &pos, "\" StopQuantity=\"")) return 0;
        n = read_until(buf, len, &pos, (uint8_t) '"', text, sizeof(text));
        if (n == 0xFFFFFFFFu || !parse_uint(text, n, &it->stop_quantity)) return 0;
        if (!match_literal(buf, len, &pos, "\" FlightNumber=\"")) return 0;
        n = read_until(buf, len, &pos, (uint8_t) '"', it->flight_number,
                       sizeof(it->flight_number) - 1u);
        if (n == 0xFFFFFFFFu) return 0;
        if (!match_literal(buf, len, &pos, "\"><DepartureAirport LocationCode=\"")) return 0;
        n = read_until(buf, len, &pos, (uint8_t) '"', it->departure_airport,
                       sizeof(it->departure_airport) - 1u);
        if (n == 0xFFFFFFFFu) return 0;
        if (!match_literal(buf, len, &pos, "\"/><ArrivalAirport LocationCode=\"")) return 0;
        n = read_until(buf, len, &pos, (uint8_t) '"', it->arrival_airport,
                       sizeof(it->arrival_airport) - 1u);
        if (n == 0xFFFFFFFFu) return 0;
        if (!match_literal(buf, len, &pos, "\"/><MarketingAirline Code=\"")) return 0;
        n = read_until(buf, len, &pos, (uint8_t) '"', it->marketing_airline_code,
                       sizeof(it->marketing_airline_code) - 1u);
        if (n == 0xFFFFFFFFu) return 0;
        if (!match_literal(buf, len, &pos,
                            "\"/></FlightSegment></AirItinerary><AirItineraryPricingInfo>"
                            "<ItinTotalFare><BaseFare Amount=\"")) return 0;
        n = read_until(buf, len, &pos, (uint8_t) '"', text, sizeof(text));
        if (n == 0xFFFFFFFFu || !parse_amount(text, n, &it->base_fare_cents)) return 0;
        if (!match_literal(buf, len, &pos, "\" CurrencyCode=\"")) return 0;
        n = read_until(buf, len, &pos, (uint8_t) '"', it->currency_code,
                       sizeof(it->currency_code) - 1u);
        if (n == 0xFFFFFFFFu) return 0;
        if (!match_literal(buf, len, &pos, "\"/><TotalFare Amount=\"")) return 0;
        n = read_until(buf, len, &pos, (uint8_t) '"', text, sizeof(text));
        if (n == 0xFFFFFFFFu || !parse_amount(text, n, &it->total_fare_cents)) return 0;
        if (!match_literal(buf, len, &pos, "\" CurrencyCode=\"")) return 0;
        uint8_t currency2[4];
        n = read_until(buf, len, &pos, (uint8_t) '"', currency2, sizeof(currency2) - 1u);
        if (n == 0xFFFFFFFFu) return 0;
        uint32_t first_len = cstr_bytes_len(it->currency_code, sizeof(it->currency_code));
        if (n != first_len) {
            return 0; /* both real CurrencyCode attributes must agree */
        }
        for (uint32_t i = 0; i < n; i++) {
            if (currency2[i] != it->currency_code[i]) {
                return 0; /* both real CurrencyCode attributes must agree */
            }
        }
        if (!match_literal(buf, len, &pos,
                            "\"/></ItinTotalFare></AirItineraryPricingInfo></PricedItinerary>\n"))
            return 0;
        count++;
    }
    out->count = count;
    if (!match_literal(buf, len, &pos, "</PricedItineraries>\n</OTA_AirLowFareSearchRS>\n"))
        return 0;
    return pos == len;
}
