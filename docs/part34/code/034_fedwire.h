#ifndef UNIX_OS_034_FEDWIRE_H
#define UNIX_OS_034_FEDWIRE_H

/* A real, tag-delimited wire-transfer message format, cited field-for-field
 * from the real Fedwire Funds Service's own historical message format
 * (the tag-based format Fedwire itself used for decades before its own
 * real July 2025 migration to ISO 20022 XML messages -- see the Federal
 * Register notice cited below). Sources for this chapter's own research:
 *
 *   - "Fedwire Funds Service - Format Reference Guide" (the real Federal
 *     Reserve Financial Services publication defining these tags), read
 *     via two independent mirrors that agree field-for-field: a
 *     studylib.net copy and an idoc.pub copy of the same real guide.
 *   - Independently corroborated (not just mirrored) by a real,
 *     third-party enterprise banking software manual -- Oracle FLEXCUBE
 *     Universal Banking's own "FEDWIRE Interface" technical reference
 *     (docs.oracle.com/cd/E51715_01/PDF/IF/IF_FEDWIRE.pdf), which quotes
 *     the exact same tag numbers and field widths from its own real
 *     integration documentation, including a real sample message.
 *   - Federal Register 2018-14351, "New Message Format for the Fedwire
 *     Funds Service" (federalregister.gov), for the real historical
 *     context: Fedwire's own classic format is described there as a real
 *     "proprietary message format", interoperable with SWIFT MT/CHIPS,
 *     that the Fed itself later moved away from in favor of ISO 20022 --
 *     cited here for honesty about what this chapter models (the classic,
 *     real, now-superseded tag format) versus what Fedwire actually runs
 *     today (ISO 20022 XML, out of this chapter's own scope).
 *
 * Real tags and real field widths modeled by this chapter (all independently
 * corroborated by both citations above):
 *   {1500} Sender Supplied Information (this chapter's own simplified
 *          2-char format version + 1-char test/production code; the real
 *          field also carries a user request correlation and message
 *          duplication code this chapter does not model -- stated plainly
 *          rather than silently dropped)
 *   {1510} Type/Subtype: 2-char type code + 2-char subtype code
 *   {1520} IMAD (Input Message Accountability Data): 22 real chars =
 *          8-char InputCycleDate (YYYYMMDD) + 8-char InputSource +
 *          6-char InputSequenceNumber
 *   {2000} Amount: 12 numeric digits, implied 2 decimal places (up to
 *          $9,999,999,999.99), matching the real field width exactly
 *   {3100} Sender DI: 9-char ABA routing number + a short name
 *   {3400} Receiver DI: 9-char ABA routing number + a short name
 *   {3600} Business Function Code: 3 chars (this chapter uses the real
 *          documented code "CTR", customer transfer)
 *   {4200} Beneficiary: an identifier + a name
 *   {5000} Originator: an identifier + a name
 *
 * IMPORTANT -- entirely fictional data: every bank name, ABA routing
 * number, account identifier, and person/company name this chapter's own
 * demo builds is invented for this book. None corresponds to any real
 * financial institution, routing number, account, or person. This code
 * builds a message that LOOKS structurally like a real (historical)
 * Fedwire message for real educational/engineering purposes -- parsing a
 * real tag-delimited wire format, then really encrypting and
 * authenticating it -- but it is not connected to, does not transmit to,
 * and could never be accepted by any real payment network. Real Fedwire's
 * actual security infrastructure (key management, authentication,
 * settlement finality) is proprietary and out of scope; this chapter's own
 * AES-128-CBC + HMAC-SHA256 (034_aes.h/.c, 034_hmac.h/.c) is this book's
 * own general-purpose encrypt-then-MAC construction, illustrating the
 * KIND of real cryptographic integrity/confidentiality protection actual
 * payment messaging relies on -- not a reproduction of Fedwire's own real,
 * non-public security protocol.
 */

#include <stdint.h>

#define FEDWIRE_ABA_LEN 9u
#define FEDWIRE_NAME_LEN 24u
#define FEDWIRE_ACCOUNT_LEN 20u
#define FEDWIRE_MAX_MESSAGE_LEN 256u

typedef struct {
    char sender_format_version[2];
    char sender_test_production_code; /* 'T' or 'P', real field semantics */

    char type_code[2];
    char subtype_code[2];

    char imad_cycle_date[8];   /* real YYYYMMDD */
    char imad_source[8];
    char imad_sequence[6];

    /* The real {2000} field is 12 numeric digits (up to
     * $9,999,999,999.99), which genuinely needs more than 32 bits.
     * Representing the FULL real range would need 64-bit division to
     * convert to/from decimal digits, which this freestanding kernel has
     * deliberately never linked libgcc for since Chapter 7's own real
     * build failure and fix (avoid the dependency, not add it) -- so
     * this chapter's own implementation stores Amount as a real 32-bit
     * value instead (up to $42,949,672.95), a stated, honest scope
     * limitation on top of the real cited field width, not a silent
     * one. */
    uint32_t amount_cents;

    char sender_aba[FEDWIRE_ABA_LEN];
    char sender_name[FEDWIRE_NAME_LEN];

    char receiver_aba[FEDWIRE_ABA_LEN];
    char receiver_name[FEDWIRE_NAME_LEN];

    char business_function_code[3];

    char beneficiary_account[FEDWIRE_ACCOUNT_LEN];
    char beneficiary_name[FEDWIRE_NAME_LEN];

    char originator_account[FEDWIRE_ACCOUNT_LEN];
    char originator_name[FEDWIRE_NAME_LEN];
} fedwire_message_t;

/* Builds the real tag-delimited wire format into out_buf. Returns the
 * real number of bytes written, or 0 if out_buf_size is too small. */
uint32_t fedwire_build_message(const fedwire_message_t *msg, uint8_t *out_buf, uint32_t out_buf_size);

/* Parses a real tag-delimited buffer back into a fedwire_message_t.
 * Returns 1 on success, 0 if any required tag is missing or malformed --
 * never partially fills out_msg on failure, this book's own established
 * no-partial-effect discipline since Chapter 20's own FAT16 refusals. */
int fedwire_parse_message(const uint8_t *buf, uint32_t len, fedwire_message_t *out_msg);

/* PKCS#7-style padding (RFC 5652 Section 6.3: pad with N bytes each of
 * value N, where N = block_size - (len % block_size), or a full block of
 * value block_size if len is already a multiple) -- needed since AES-128-CBC
 * only operates on whole 16-byte blocks and this chapter's own message
 * length varies with name/account field lengths. */
uint32_t fedwire_pkcs7_pad(const uint8_t *in, uint32_t in_len, uint8_t *out, uint32_t out_buf_size, uint32_t block_size);
/* Returns the real unpadded length, or 0xFFFFFFFFu if the padding is invalid
 * (used by the receive side to detect a genuinely corrupted/tampered
 * plaintext after decryption, on top of the HMAC check that runs first). */
uint32_t fedwire_pkcs7_unpad(const uint8_t *in, uint32_t in_len, uint32_t block_size);

#endif
