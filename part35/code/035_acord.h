#ifndef UNIX_OS_035_ACORD_H
#define UNIX_OS_035_ACORD_H

#include <stdint.h>
#include "035_insurance.h"

/* A minimal ACORD-style XML encoder/decoder for a personal-auto
 * insurance quote request/response -- the real XML data-exchange
 * standard the US insurance industry uses between agents, carriers,
 * and comparison tools (ACORD -- the Association for Cooperative
 * Operations Research and Development -- is the real, long-standing
 * standards body for this; cited via web search results, since this
 * sandbox's network egress policy blocks acord.org and every
 * third-party ACORD-schema-documentation site this book tried
 * (schemas.liquid-technologies.com, xml.coverpages.org,
 * en.wikipedia.org) with an explicit "blocked by the network egress
 * proxy" error, the same honesty note Chapter 33 made about
 * consumerfinance.gov/ecfr.gov/govinfo.gov/law.cornell.edu).
 *
 * What this chapter COULD read in full, because GitHub itself is
 * reachable: a real, unmodified, publicly posted ACORD XML sample --
 * a clone of github.com/appulate/appulate-acordxml-svc-sample,
 * `Testing/case1/case1.xml` and `Testing/case2/case2_success.xml` (a
 * real Workers' Compensation policy-quote submission, not personal
 * auto, but built from ACORD's own shared library of elements that
 * every P&C line of business reuses). Every element name below marked
 * [OBSERVED] appears verbatim in that real file, read directly, not
 * through a search-result summary: `ACORD`, `SignonRq`, `CustLoginId`,
 * `ClientDt`, `ClientApp`/`Org`/`Name`, `InsuranceSvcRq`, `ItemIdInfo`,
 * `OtherIdentifier`/`OtherIdTypeCd`/`UID`, `TransactionRequestDt`,
 * `CurCd`, `InsuredOrPrincipal`, `GeneralPartyInfo`, `NameInfo`,
 * `PersonName`/`Surname`/`GivenName`, `CommlName`/`CommercialName`,
 * `Addr`/`StateProvCd`/`PostalCode`, `Policy`, `LOBCd`, `ContractTerm`/
 * `EffectiveDt`/`ExpirationDt`, `Coverage`/`CoverageCd`, `Limit`/
 * `FormatCurrencyAmt`/`Amt`/`LimitAppliesToCd`, `Deductible`/
 * `DeductibleAppliesToCd`, `PolicyStatusCd`, and `PolicyAmt`/`Amt`
 * (observed there labeling a PRIOR policy's own premium inside
 * `OtherOrPriorPolicy`; this chapter reuses the same real aggregate
 * shape for a freshly QUOTED premium instead -- a stated reuse, not a
 * verbatim citation of that exact usage).
 *
 * Two things below are marked [WEAK] -- real, per web-search results
 * this book could not read in full: the personal-auto request/response
 * root names `PersAutoPolicyQuoteInqRq`/`PersAutoPolicyQuoteInqRs`
 * (search results showed the XPath
 * `ACORD/InsuranceSvcRq/PersAutoPolicyQuoteInqRq/PersAutoLineBusiness/
 * PersDriver` from Oracle's own Siebel ACORD-connector documentation),
 * and the personal-auto Line-of-Business code `AUTOP` (the observed
 * sample used `WORK` for Workers' Compensation; `AUTOP` is this book's
 * own inference from ACORD's well-known LOBCd naming pattern, not a
 * value this book directly observed).
 *
 * Everything else -- `PersDriverInfo`/`Age`/`YearsLicensed`/
 * `AtFaultAccidentCnt`, and `PersVehInfo`/`VehCurrentValue`/`VehAge` --
 * is marked [INVENTED]: a compact stand-in for real ACORD driver- and
 * vehicle-history aggregates (`PersDriver`, `PersVeh`) this book could
 * not read directly. Each invented wrapper still carries its numeric
 * value through the real, OBSERVED `FormatCurrencyAmt`/`Amt` leaf
 * pattern where a dollar amount is involved, for consistency with
 * every other amount in this message.
 *
 * This module implements a restricted, stated subset of XML, matching
 * only what this chapter's own encoder produces: no attributes on any
 * element (an ACORD `id="..."` attribute, real in the observed sample,
 * is never emitted or required here), no whitespace between elements,
 * no XML declaration/prolog, no entity references, and -- the one
 * structural rule that makes `acord_find()`'s own matching logic
 * correct -- an element is never nested inside another element of the
 * SAME name. `acord_find()` therefore locates a matching close tag by
 * finding the first `</tag>` after the open tag, rather than tracking
 * full nesting depth; this chapter's own message never needs the
 * general case. Given a message that violates any of this, or is
 * missing a required element or numeric field, every decode function
 * below refuses outright (returns 0) rather than guessing, the same
 * refusal discipline `iso8583_parse()` and `ach_parse_file()`
 * established. */

#define ACORD_MAX_MESSAGE_LEN 2048u
#define ACORD_MAX_NAME_LEN 24u

typedef struct {
    char surname[ACORD_MAX_NAME_LEN];
    char given_name[ACORD_MAX_NAME_LEN];
    char state_prov_cd[3]; /* 2-letter USPS code + NUL */
    char postal_code[6];   /* 5-digit ZIP + NUL */
    ins_applicant_t applicant;
    /* Policy term: 6-digit YYMMDD, this book's own convention (same as
     * 035_bnpl.h's due dates, though this module carries YYMMDD as
     * text rather than a bnpl_date_t, since it round-trips through XML
     * text nodes rather than a fixed-width numeric field). */
    char effective_date[7];
    char expiration_date[7];
} acord_request_t;

typedef struct {
    ins_quote_t quotes[INS_MAX_CARRIERS];
    uint32_t quote_count; /* already in ascending-premium (ranked) order */
} acord_response_t;

/* Builds a real-shaped ACORD request message for `req` into `out`.
 * Returns the encoded length, or 0 if `out_size` is too small or any
 * text field is empty/too long for its own stated width. */
uint32_t acord_build_request(const acord_request_t *req, uint8_t *out, uint32_t out_size);

/* Parses exactly `len` bytes of a request message. Returns 1 on
 * success, or 0 -- refusing outright -- if any required element is
 * missing, a numeric field contains a non-digit byte, or a text field
 * would overflow its own fixed buffer. */
int acord_parse_request(const uint8_t *buf, uint32_t len, acord_request_t *out_req);

/* Builds a real-shaped ACORD response message carrying `resp`'s own
 * already-ranked quotes. Returns the encoded length, or 0 on the same
 * refusal conditions as acord_build_request(). */
uint32_t acord_build_response(const acord_response_t *resp, uint8_t *out, uint32_t out_size);

/* Parses exactly `len` bytes of a response message, the same refusal
 * discipline as acord_parse_request(). */
int acord_parse_response(const uint8_t *buf, uint32_t len, acord_response_t *out_resp);

#endif
