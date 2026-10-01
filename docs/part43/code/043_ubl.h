#ifndef UNIX_OS_043_UBL_H
#define UNIX_OS_043_UBL_H

#include <stdint.h>

/* A real UBL (Universal Business Language) 2.1 Invoice -- OASIS's own
 * real, open e-invoicing standard (the same document format the EU's
 * PEPPOL network mandates for cross-border e-invoicing).
 *
 * OASIS's own official specification text (docs.oasis-open.org) is
 * blocked by this sandbox's network egress policy, the same pattern
 * every blocked-standards-site chapter since Chapter 33 has hit. But
 * OASIS's own GitHub organization is not: this chapter cloned
 * github.com/oasis-tcs/ubl -- the UBL Technical Committee's own real
 * repository -- and read two of its own real, official example
 * instance documents in full:
 *
 *   raw/xml/UBL-Invoice-2.1-Example-Trivial.xml (41 lines, read whole)
 *   raw/xml/UBL-Invoice-2.1-Example.xml (494 lines, read in full for
 *     its own real PaymentMeans/TaxTotal/InvoiceLine shapes)
 *
 * This is a stronger citation tier than this book's usual "an
 * independent open-source implementation agrees with another" --
 * these are OASIS's own official example documents, not a third
 * party's interpretation of the schema.
 *
 * Every element name and nesting level below is copied directly from
 * those two real files, restricted to the subset this chapter's own
 * billing engine needs (this codec's own stated scope limit, the same
 * "known, restricted schema" approach Chapter 34's own ACORD XML
 * codec used): a real UBL Invoice root element in the real
 * `urn:oasis:names:specification:ubl:schema:xsd:Invoice-2` namespace,
 * with `cbc:` (CommonBasicComponents) and `cac:`
 * (CommonAggregateComponents) child elements exactly as the real
 * examples use them:
 *
 *   cbc:ID                     the real invoice's own document number
 *   cbc:IssueDate              real ISO 8601 (YYYY-MM-DD)
 *   cac:InvoicePeriod          cbc:StartDate / cbc:EndDate -- the real
 *                              billing cycle this invoice covers
 *   cac:AccountingSupplierParty/cac:Party/cac:PartyName/cbc:Name
 *   cac:AccountingCustomerParty/cac:Party/cac:PartyName/cbc:Name
 *   cac:PaymentMeans/cbc:PaymentDueDate
 *   cac:TaxTotal/cbc:TaxAmount        (this chapter's own scope limit:
 *                                     one flat tax amount, not the
 *                                     real format's own TaxSubtotal/
 *                                     TaxCategory breakdown by rate --
 *                                     a real invoice can have several)
 *   cac:LegalMonetaryTotal/cbc:PayableAmount (with a real currencyID
 *                                     attribute, cited from both files)
 *   cac:InvoiceLine (one or more)/cbc:ID, cbc:LineExtensionAmount,
 *                                     cac:Item/cbc:Description
 *
 * Every amount carries a real `currencyID` XML attribute exactly as
 * both real example files do, and this chapter's own currency is
 * always "USD" (a real ISO 4217 code, not itself part of either
 * example file, which used CAD/EUR).
 *
 * This codec builds and parses this exact restricted subset only. It
 * does NOT implement general XML (no attributes beyond currencyID, no
 * namespaces beyond the three declared on the root element, no
 * entity escaping beyond the digits/letters this chapter's own
 * invoice text ever contains) -- refusing outright, never guessing,
 * on anything outside this subset, the same discipline Chapter 34's
 * own restricted OFX/ACORD parsers used. */

#define UBL_MAX_LINES 4u
#define UBL_MAX_NAME_LEN 32u
#define UBL_MAX_DESC_LEN 48u
#define UBL_MAX_XML_LEN 2048u

typedef struct {
    uint8_t id[8];
    uint32_t id_len;
    uint32_t line_extension_cents;
    uint8_t description[UBL_MAX_DESC_LEN];
    uint32_t description_len;
} ubl_invoice_line_t;

typedef struct {
    uint8_t id[16];
    uint32_t id_len;
    uint8_t issue_date[10]; /* real ISO 8601 YYYY-MM-DD */
    uint8_t period_start[10];
    uint8_t period_end[10];
    uint8_t supplier_name[UBL_MAX_NAME_LEN];
    uint32_t supplier_name_len;
    uint8_t customer_name[UBL_MAX_NAME_LEN];
    uint32_t customer_name_len;
    uint8_t payment_due_date[10];
    uint32_t tax_amount_cents;
    uint32_t payable_amount_cents;
    ubl_invoice_line_t lines[UBL_MAX_LINES];
    uint32_t line_count;
} ubl_invoice_t;

/* Builds this chapter's own restricted UBL Invoice XML subset into
 * `out`. Returns the encoded length, or 0 if `out_size` is too small
 * or `inv->line_count` exceeds UBL_MAX_LINES. */
uint32_t ubl_build_invoice(const ubl_invoice_t *inv, uint8_t *out, uint32_t out_size);

/* Parses exactly `len` bytes of this chapter's own restricted subset
 * back into `*out`. Returns 1 on success, or 0 -- refusing outright --
 * on anything outside that exact subset (a missing or reordered
 * element, a currencyID other than "USD", a non-digit amount, or more
 * than UBL_MAX_LINES real invoice lines). */
int ubl_parse_invoice(const uint8_t *buf, uint32_t len, ubl_invoice_t *out);

#endif
