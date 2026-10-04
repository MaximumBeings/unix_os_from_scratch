#ifndef UNIX_OS_048_VEH_H
#define UNIX_OS_048_VEH_H

#include <stdint.h>

/* A real OpenTravel Alliance (OTA) vehicle rental availability and
 * reservation message pair -- `OTA_VehAvailRateRQ`/`RS` and
 * `OTA_VehResRQ`/`RS` -- this chapter's own confirmed scope names.
 *
 * OpenTravel's own official site (opentravel.org) is blocked by this
 * sandbox's network egress policy, the same pattern every blocked-
 * standards-site chapter since Chapter 33 has hit, and Chapter 41's
 * own cloned mirror (github.com/mennaelnemr99/XMLtoJSONsample) only
 * carries the real AIR schemas, not the vehicle ones. This chapter
 * instead cloned github.com/XML-Travelgate/xtg-content-articles-pub --
 * XML Travelgate's own public technical documentation repository for
 * its own real car-rental connectivity product -- and read its own
 * real `docs/car/car.xsd` in full (2122 lines): a complete, real OTA
 * vehicle-rental schema, not a search summary. Every element/attribute
 * name below is copied directly out of that real schema file's own
 * real `<xs:complexType>`/`<xs:element>` definitions:
 *
 *   OTA_VehAvailRateRQ/VehAvailRQCore/VehRentalCore
 *     @PickUpDateTime, @ReturnDateTime (real xs:dateTime attributes)
 *     PickUpLocation/@LocationCode/@CodeContext,
 *     ReturnLocation/@LocationCode/@CodeContext (real `Location` type;
 *     @CodeContext is a real restricted enumeration: IATA/CITY/OFFICE/
 *     GEOCODE)
 *
 *   OTA_VehAvailRateRS/VehAvailRSCore
 *     VehRentalCore (echoed back, same real shape as the request)
 *     VehVendorAvails/VehVendorAvail/VehAvails/VehAvail/VehAvailCore
 *       @Status (real `eInventoryStatus` enumeration: Available/
 *       OnRequest/All)
 *       Vehicle/@TransmissionType (real `eVehicleTransmissionType`:
 *         NOTSPECIFIED/AUTOMATIC/MANUAL), Vehicle/@FuelType (real
 *         `eFuelType`, a real 9-value enumeration)
 *       Vehicle/VehClass/@Size (a real xs:string field -- the real
 *         schema places no enumeration on it)
 *       TotalCharge/@RateTotalAmount (real xs:decimal)/@CurrencyCode
 *       Vendor/@CompanyShortName (real `CompanyName` type)
 *
 *   OTA_VehResRQ/VehResRQCore (real `VehicleReservationRQCore`)
 *     @Status (same real `eInventoryStatus` enumeration), VehRentalCore
 *     (same real shape), VendorPref/@CompanyShortName, TotalCharge
 *     (the chosen offer's own real fields, carried back to the vendor)
 *
 *   OTA_VehResRS/VehResRSCore/VehReservation (real `VehicleReservation`)
 *     @ReservationStatus (real `eTransactionStatusType`: UNSUCCESSFUL/
 *     REQUESTED/RESERVED/CANCELLED)
 *     VehSegmentCore (real `VehicleSegmentCore`): ConfID (real
 *       `UniqueID` type: @ID/@Type), Vendor, Vehicle, TotalCharge
 *
 * This chapter's own restricted subset, the same "known, restricted
 * schema" approach Chapters 34/40/41/42/43 used: only `IATA` pickup/
 * return locations (never CITY/OFFICE/GEOCODE); only `AUTOMATIC`/
 * `MANUAL` transmissions (never the real `NOTSPECIFIED`); only
 * `PETROL`/`DIESEL`/`ELECTRIC`/`HYBRID` fuel types (four of the real
 * schema's own nine real enumerated values); one `VehAvail` per
 * `VehVendorAvail` (not the real format's own unbounded array of
 * offers per vendor); and a fixed `ConfID/@Type="1"`, since the real
 * schema's own `eUniqueIdType` enumeration is a bare list of numeric
 * strings this excerpt gives no further real meaning for. Real
 * amounts are tracked in whole cents internally (this book's own
 * established limit since Chapter 30 -- no floating point), formatted
 * to the real `xs:decimal` shape on the wire. */

#define VEH_MAX_OFFERS 4u
#define VEH_LOC_MAX 4u       /* real 3-letter IATA code + NUL */
#define VEH_DATETIME_MAX 20u /* real xs:dateTime YYYY-MM-DDTHH:MM:SS + NUL */
#define VEH_VENDOR_MAX 24u
#define VEH_ENUM_MAX 16u     /* transmission / fuel type */
#define VEH_CLASS_MAX 16u
#define VEH_CURRENCY_MAX 4u
#define VEH_CONF_ID_MAX 16u
#define VEH_WIRE_MAX 2048u

typedef struct {
    uint8_t pickup_location[VEH_LOC_MAX];
    uint8_t return_location[VEH_LOC_MAX];
    uint8_t pickup_datetime[VEH_DATETIME_MAX];
    uint8_t return_datetime[VEH_DATETIME_MAX];
} veh_rental_core_t;

typedef struct {
    uint8_t vendor_name[VEH_VENDOR_MAX];
    uint8_t transmission[VEH_ENUM_MAX];  /* "AUTOMATIC" or "MANUAL" only */
    uint8_t fuel_type[VEH_ENUM_MAX];     /* "PETROL"/"DIESEL"/"ELECTRIC"/"HYBRID" only */
    uint8_t vehicle_class[VEH_CLASS_MAX];
    uint32_t rate_total_cents;
    uint8_t currency_code[VEH_CURRENCY_MAX];
} veh_offer_t;

typedef struct {
    veh_rental_core_t core;
} veh_avail_request_t;

typedef struct {
    veh_rental_core_t core;
    veh_offer_t offers[VEH_MAX_OFFERS];
    uint32_t count;
} veh_avail_response_t;

typedef struct {
    veh_rental_core_t core;
    veh_offer_t chosen;
} veh_res_request_t;

typedef struct {
    uint8_t conf_id[VEH_CONF_ID_MAX];
    veh_offer_t confirmed;
} veh_res_response_t;

/* Builds a real OTA_VehAvailRateRQ into `out`. Returns the encoded
 * length, or 0 on refusal (any field too long). */
uint32_t veh_build_avail_request(const veh_avail_request_t *req, uint8_t *out, uint32_t out_size);

/* Parses exactly `len` bytes of a real OTA_VehAvailRateRQ back into
 * `*out`. Returns 1 on success, or 0 -- refusing outright -- on
 * anything outside this chapter's own restricted subset (including a
 * `CodeContext` other than the real `IATA` value). */
int veh_parse_avail_request(const uint8_t *buf, uint32_t len, veh_avail_request_t *out);

/* Builds a real OTA_VehAvailRateRS carrying `resp->count` real
 * VehVendorAvail offers into `out`. Returns the encoded length, or 0
 * on refusal (any field too long, `resp->count` exceeding
 * VEH_MAX_OFFERS, or an offer whose `transmission`/`fuel_type` falls
 * outside this chapter's own restricted subset of the real
 * enumerations). */
uint32_t veh_build_avail_response(const veh_avail_response_t *resp, uint8_t *out,
                                   uint32_t out_size);

/* Parses exactly `len` bytes of a real OTA_VehAvailRateRS back into
 * `*out`. Returns 1 on success, or 0 -- refusing outright -- on
 * anything outside this chapter's own restricted subset. */
int veh_parse_avail_response(const uint8_t *buf, uint32_t len, veh_avail_response_t *out);

/* Builds a real OTA_VehResRQ -- booking the one chosen offer named by
 * `req->chosen` -- into `out`. Returns the encoded length, or 0 on
 * refusal. */
uint32_t veh_build_res_request(const veh_res_request_t *req, uint8_t *out, uint32_t out_size);

/* Parses that same real request shape back into `*out`. Returns 1 on
 * success, or 0 -- refusing outright -- otherwise. */
int veh_parse_res_request(const uint8_t *buf, uint32_t len, veh_res_request_t *out);

/* Builds a real OTA_VehResRS confirming `conf_id` and `confirmed`,
 * with a real `ReservationStatus="RESERVED"` -- this chapter's own
 * restricted subset models only the real RESERVED outcome, never
 * UNSUCCESSFUL/REQUESTED/CANCELLED. Returns the encoded length, or 0
 * on refusal. */
uint32_t veh_build_res_response(const uint8_t *conf_id, uint32_t conf_id_len,
                                 const veh_offer_t *confirmed, uint8_t *out, uint32_t out_size);

/* Parses that same real response shape. Returns 1 on success (a real
 * RESERVED status), or 0 -- refusing outright -- on anything else,
 * including a real but out-of-scope ReservationStatus value. */
int veh_parse_res_response(const uint8_t *buf, uint32_t len, veh_res_response_t *out);

#endif
