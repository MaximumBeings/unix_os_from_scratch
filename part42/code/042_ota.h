#ifndef UNIX_OS_042_OTA_H
#define UNIX_OS_042_OTA_H

#include <stdint.h>

/* A real OpenTravel Alliance (OTA) flight search request/response --
 * the classic real GDS-era XML pair (`OTA_AirLowFareSearchRQ`/`RS`)
 * this book's own confirmed scope names, the wire format most legacy
 * and many modern flight aggregators still speak underneath.
 *
 * OpenTravel's own official site (opentravel.org) is blocked by this
 * sandbox's network egress policy, the same pattern every blocked-
 * standards-site chapter since Chapter 33 has hit. This chapter
 * instead cloned github.com/mennaelnemr99/XMLtoJSONsample -- a real,
 * independent project's own copy of OpenTravel's own real XSD schema
 * files -- and read four of them in full:
 *
 *   OTA_AirLowFareSearchRQ.xsd, OTA_AirCommonTypes.xsd,
 *   OTA_CommonTypes.xsd, OTA_SimpleTypes.xsd
 *
 * and, alongside them, that same repository's own real captured
 * example request instance, `1. lowfareSearch_request.xml` -- read in
 * full and reproduced below unmodified in shape:
 *
 *   <OTA_AirLowFareSearchRQ ... TimeStamp="2019-08-22T05:44:10+05:30" ...>
 *     <OriginDestinationInformation>
 *       <DepartureDateTime>2020-12-19</DepartureDateTime>
 *       <OriginLocation LocationCode="HMB"/>
 *       <DestinationLocation LocationCode="JED"/>
 *     </OriginDestinationInformation>
 *     <TravelerInfoSummary>
 *       <AirTravelerAvail>
 *         <PassengerTypeQuantity Code="ADT" Quantity="1"/>
 *       </AirTravelerAvail>
 *     </TravelerInfoSummary>
 *   </OTA_AirLowFareSearchRQ>
 *
 * -- a real official example instance document, the strongest
 * citation tier this book's own OTA work reaches, the same tier
 * Chapter 40's own UBL work reached with OASIS's own example invoices.
 *
 * The response side, `OTA_AirLowFareSearchRS`, has no real official
 * example instance in that same repository -- only the real schema
 * TYPES it shares with the request (`OTA_AirCommonTypes.xsd`'s own
 * `PricedItineraryType`/`AirItineraryPricingInfoType`/`FareType`,
 * `OTA_CommonTypes.xsd`'s own `FlightSegmentBaseType`), read directly
 * out of those same real cloned files rather than from a search
 * summary. Every element/attribute name below is real and copied
 * directly from those real schema type definitions:
 *
 *   PricedItineraries/PricedItinerary (one per fictional airline's own
 *     offer)
 *     AirItinerary -> FlightSegment: DepartureAirport/@LocationCode,
 *       ArrivalAirport/@LocationCode, @DepartureDateTime,
 *       @ArrivalDateTime, @StopQuantity, @FlightNumber,
 *       MarketingAirline/@Code
 *     AirItineraryPricingInfo/ItinTotalFare: BaseFare/@Amount/
 *       @CurrencyCode, TotalFare/@Amount/@CurrencyCode (both real
 *       `CurrencyAmountGroup` attributes, `Money` a real xs:decimal
 *       with 3 fraction digits)
 *
 * This chapter's own stated scope limit, the same "known, restricted
 * schema" approach Chapters 34/40 used: only nonstop itineraries (one
 * real `FlightSegment` per `PricedItinerary`, not the real format's own
 * `OriginDestinationOptions` list of several connecting segments) and
 * only a single `ADT` (adult) passenger type quantity. Real amounts
 * are tracked in whole cents internally (this book's own established
 * limit since Chapter 30 -- no floating point), formatted to the real
 * `Money` type's own decimal-with-cents shape on the wire. */

#define OTA_MAX_XML_LEN 2048u
#define OTA_MAX_ITINERARIES 4u

typedef struct {
    uint8_t origin[4];      /* real 3-letter IATA airport code */
    uint8_t destination[4];
    uint8_t departure_date[10]; /* real ISO 8601 YYYY-MM-DD */
    uint32_t adt_quantity;
} ota_search_request_t;

typedef struct {
    uint8_t departure_airport[4];
    uint8_t arrival_airport[4];
    uint8_t departure_datetime[20]; /* real xs:dateTime, YYYY-MM-DDTHH:MM:SS */
    uint8_t arrival_datetime[20];
    uint32_t stop_quantity;
    uint8_t flight_number[6];
    uint8_t marketing_airline_code[4]; /* real 2-letter IATA airline code */
    uint32_t base_fare_cents;
    uint32_t total_fare_cents;
    uint8_t currency_code[4]; /* real ISO 4217 3-letter code */
} ota_priced_itinerary_t;

typedef struct {
    ota_priced_itinerary_t itineraries[OTA_MAX_ITINERARIES];
    uint32_t count;
} ota_search_response_t;

/* Builds a real OTA_AirLowFareSearchRQ into `out`. Returns the encoded
 * length, or 0 if `out_size` is too small. */
uint32_t ota_build_request(const ota_search_request_t *req, uint8_t *out, uint32_t out_size);

/* Parses exactly `len` bytes of a real OTA_AirLowFareSearchRQ back
 * into `*out`. Returns 1 on success, 0 -- refusing outright -- on
 * anything outside this chapter's own restricted subset. */
int ota_parse_request(const uint8_t *buf, uint32_t len, ota_search_request_t *out);

/* Builds a real OTA_AirLowFareSearchRS carrying `resp->count` real
 * nonstop PricedItinerary elements into `out`. Returns the encoded
 * length, or 0 if `out_size` is too small or `resp->count` exceeds
 * OTA_MAX_ITINERARIES. */
uint32_t ota_build_response(const ota_search_response_t *resp, uint8_t *out, uint32_t out_size);

/* Parses exactly `len` bytes of a real OTA_AirLowFareSearchRS back
 * into `*out`. Returns 1 on success, 0 -- refusing outright -- on
 * anything outside this chapter's own restricted subset (including
 * more than OTA_MAX_ITINERARIES real PricedItinerary elements). */
int ota_parse_response(const uint8_t *buf, uint32_t len, ota_search_response_t *out);

#endif
