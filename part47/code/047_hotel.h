#ifndef UNIX_OS_047_HOTEL_H
#define UNIX_OS_047_HOTEL_H

#include <stdint.h>

/* A real hotel (lodging + tourist-attraction bundle) availability and
 * reservation message pair -- `AvailRQ`/`AvailRS` and
 * `ReservationRQ`/`RS` -- this chapter's own confirmed scope names.
 *
 * OpenTravel's own official site (opentravel.org) is blocked by this
 * sandbox's network egress policy, the same pattern every blocked-
 * standards-site chapter since Chapter 33 has hit. Chapter 44's own
 * real source, github.com/XML-Travelgate/xtg-content-articles-pub --
 * XML Travelgate's own public technical documentation repository --
 * also carries its own real hotel-connectivity schema, separate from
 * the real vehicle one Chapter 44 cited: `docs/hotel/storage/hotel.xsd`
 * (2152 lines), read in full here. Every element/attribute name below
 * is copied directly out of that real schema file's own real
 * `<xs:complexType>` definitions:
 *
 *   AvailRQ (extends HotelBaseRQ)
 *     StartDate, EndDate (real xs:string date fields -- the real
 *     schema gives no fixed xs:date format, this chapter's own
 *     restriction below states one)
 *
 *   AvailRS/HotelsAvail/HotelAvail (real `@code`/`@name` attributes)
 *     MealPlans/MealPlan (real `@code` attribute)
 *       Options/Option (real `@supplierCode`/`@type`/`@paymentType`
 *       attributes; `@type` is the real `eTipoOpcion` enumeration --
 *       13 real values including plain `Hotel`, `HotelActivity`,
 *       `HotelTicket`, `HotelTicketTransfers`, and several real
 *       `HotelSkiPass*` combinations -- this schema's own real,
 *       already-existing way of modeling a hotel stay bundled with a
 *       real tourist attraction/activity, not an invented second
 *       schema; `@paymentType` is the real `eTipoFormaPago`
 *       enumeration, 5 real values)
 *         Rooms/Room (real `@code`/`@description`/`@nonRefundable`
 *         attributes)
 *           Price (real `@currency`/`@amount`/`@commission` attributes,
 *           real xs:decimal)
 *
 *   ReservationRQ (extends HotelBaseRQ)
 *     ClientLocator, StartDate, EndDate, MealPlanCode, HotelCode,
 *     Price, PaymentType (same real `eTipoFormaPago`), ResGuests/
 *     Guests/Guest (real `GivenName`/`SurName`/`@roomCandidateId`/
 *     `@paxId` fields)
 *
 *   ReservationRS (extends HotelBaseRS)
 *     ProviderLocator, PropertyReservationNumber, ResStatus (real
 *     `eEstadoReserva` enumeration: RQ/OK/CN/UN), Price
 *
 * This chapter's own restricted subset, the same "known, restricted
 * schema" approach Chapters 34/40/41/42/43/44 used: `AvailRQ` drops
 * the real schema's own `AvailDestinations`/`RoomCandidates`
 * substructures entirely (each vendor answers for its own single known
 * property set, stated explicitly); dates are a fixed
 * `YYYY-MM-DD` (10 characters), never a full real xs:dateTime; only
 * `Hotel` and `HotelActivity` of the real `eTipoOpcion`'s own 13 real
 * values are used (plain lodging, and lodging bundled with one real
 * tourist attraction/activity ticket); `PaymentType` is always the
 * real `MerchantPay` value; `ResStatus` is always the real `OK` value
 * (this chapter's own restricted subset models only a successful
 * booking, the same choice Chapter 44 made for `RESERVED`); one
 * `Option` per `MealPlan`, one `Room` per `Option`, one `MealPlan` per
 * `Hotel` (the real schema's own unbounded arrays, restricted to
 * exactly one each); and a single `Guest` per reservation (the real
 * schema's own unbounded `Guests` array, restricted to one, fixed
 * `roomCandidateId=1`/`paxId=1`). Real amounts are tracked in whole
 * cents internally (this book's own established limit since Chapter
 * 30 -- no floating point), formatted to the real `xs:decimal` shape
 * on the wire. */

#define HOTEL_MAX_OFFERS 4u
#define HOTEL_DATE_MAX 11u        /* real YYYY-MM-DD + NUL */
#define HOTEL_CODE_MAX 16u
#define HOTEL_NAME_MAX 32u
#define HOTEL_MEALPLAN_MAX 8u
#define HOTEL_OPTYPE_MAX 24u      /* "Hotel" or "HotelActivity" */
#define HOTEL_PAYTYPE_MAX 16u     /* always "MerchantPay" */
#define HOTEL_ROOM_DESC_MAX 32u
#define HOTEL_CURRENCY_MAX 4u
#define HOTEL_LOCATOR_MAX 16u
#define HOTEL_GUEST_NAME_MAX 24u
#define HOTEL_WIRE_MAX 2048u

typedef struct {
    uint8_t start_date[HOTEL_DATE_MAX];
    uint8_t end_date[HOTEL_DATE_MAX];
} hotel_stay_core_t;

typedef struct {
    uint8_t hotel_code[HOTEL_CODE_MAX];
    uint8_t hotel_name[HOTEL_NAME_MAX];
    uint8_t meal_plan_code[HOTEL_MEALPLAN_MAX];
    uint8_t supplier_code[HOTEL_NAME_MAX];
    uint8_t option_type[HOTEL_OPTYPE_MAX];   /* "Hotel" or "HotelActivity" only */
    uint8_t payment_type[HOTEL_PAYTYPE_MAX]; /* "MerchantPay" only */
    uint8_t room_code[HOTEL_CODE_MAX];
    uint8_t room_description[HOTEL_ROOM_DESC_MAX];
    int non_refundable;
    uint32_t rate_total_cents;
    uint32_t commission_cents;
    uint8_t currency_code[HOTEL_CURRENCY_MAX];
} hotel_offer_t;

typedef struct {
    hotel_stay_core_t core;
} hotel_avail_request_t;

typedef struct {
    hotel_stay_core_t core;
    hotel_offer_t offers[HOTEL_MAX_OFFERS];
    uint32_t count;
} hotel_avail_response_t;

typedef struct {
    uint8_t given_name[HOTEL_GUEST_NAME_MAX];
    uint8_t sur_name[HOTEL_GUEST_NAME_MAX];
} hotel_guest_t;

typedef struct {
    uint8_t client_locator[HOTEL_LOCATOR_MAX];
    hotel_stay_core_t core;
    hotel_offer_t chosen;
    hotel_guest_t guest;
} hotel_res_request_t;

typedef struct {
    uint8_t provider_locator[HOTEL_LOCATOR_MAX];
    uint8_t property_reservation_number[HOTEL_LOCATOR_MAX];
    hotel_offer_t confirmed;
} hotel_res_response_t;

/* Builds a real AvailRQ into `out`. Returns the encoded length, or 0 on
 * refusal (any field too long). */
uint32_t hotel_build_avail_request(const hotel_avail_request_t *req, uint8_t *out, uint32_t out_size);

/* Parses exactly `len` bytes of a real AvailRQ back into `*out`.
 * Returns 1 on success, or 0 -- refusing outright -- on anything
 * outside this chapter's own restricted subset (including a date not
 * exactly 10 real digits-and-dashes long). */
int hotel_parse_avail_request(const uint8_t *buf, uint32_t len, hotel_avail_request_t *out);

/* Builds a real AvailRS carrying `resp->count` real `Hotel` offers
 * into `out`. Returns the encoded length, or 0 on refusal (any field
 * too long, `resp->count` exceeding HOTEL_MAX_OFFERS, or an offer
 * whose `option_type`/`payment_type` falls outside this chapter's own
 * restricted subset of the real enumerations). */
uint32_t hotel_build_avail_response(const hotel_avail_response_t *resp, uint8_t *out,
                                     uint32_t out_size);

/* Parses exactly `len` bytes of a real AvailRS back into `*out`.
 * Returns 1 on success, or 0 -- refusing outright -- on anything
 * outside this chapter's own restricted subset. */
int hotel_parse_avail_response(const uint8_t *buf, uint32_t len, hotel_avail_response_t *out);

/* Builds a real ReservationRQ -- booking the one chosen offer named by
 * `req->chosen` for the one guest named by `req->guest` -- into `out`.
 * Returns the encoded length, or 0 on refusal. Mirrors the real
 * schema's own actual `ReservationRQ` shape exactly: only
 * `HotelCode`/`MealPlanCode`/`Price` identify the chosen option on the
 * wire, never the option's own real `@type`/room code/description --
 * the real schema simply has no field for those in a reservation
 * request. */
uint32_t hotel_build_res_request(const hotel_res_request_t *req, uint8_t *out, uint32_t out_size);

/* Parses that same real request shape back into `*out`. Returns 1 on
 * success, or 0 -- refusing outright -- otherwise. */
int hotel_parse_res_request(const uint8_t *buf, uint32_t len, hotel_res_request_t *out);

/* Builds a real ReservationRS confirming `provider_locator`/
 * `property_reservation_number`, with a real `ResStatus="OK"` --
 * this chapter's own restricted subset models only the real OK
 * outcome, never RQ/CN/UN. Returns the encoded length, or 0 on
 * refusal. Mirrors the real schema's own actual `ReservationRS` shape:
 * carries back only `Price`, never the option's own real `@type`/room
 * code/description either. */
uint32_t hotel_build_res_response(const uint8_t *provider_locator, uint32_t provider_locator_len,
                                   const uint8_t *property_reservation_number,
                                   uint32_t property_reservation_number_len,
                                   const hotel_offer_t *confirmed, uint8_t *out, uint32_t out_size);

/* Parses that same real response shape. Returns 1 on success (a real
 * OK status), or 0 -- refusing outright -- on anything else, including
 * a real but out-of-scope ResStatus value. Only `provider_locator`/
 * `property_reservation_number`/`Price` are genuinely recovered by
 * this parse -- the real schema's own `ReservationRS` carries nothing
 * else about the confirmed option; `out->confirmed`'s own
 * `hotel_code`/`option_type`/`room_code`/`room_description` fields are
 * left zeroed, by design, the same real gap Chapter 44 found in
 * `OTA_VehResRQ`/`RS`. */
int hotel_parse_res_response(const uint8_t *buf, uint32_t len, hotel_res_response_t *out);

#endif
