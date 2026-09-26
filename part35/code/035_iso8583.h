#ifndef UNIX_OS_035_ISO8583_H
#define UNIX_OS_035_ISO8583_H

#include <stdint.h>

/* A real ISO 8583:1987 card-authorization message encoder/decoder -- the
 * message format a point-of-sale terminal's authorization request
 * actually travels in on its way to a card issuer, and the one a
 * virtual-card BNPL checkout rides on at the merchant's till.
 *
 * ISO 8583 itself is a paid ISO standard this book has not read.
 * Instead, every field number, name, width, and length-prefix rule below
 * is cited from two independent, open-source implementations of its 1987
 * ASCII variant that agree, field for field, on every data element this
 * chapter uses:
 *
 *   - moov-io/iso8583 (Go), specs/spec87ascii.go, read directly from a
 *     clone of github.com/moov-io/iso8583 at commit 5219813 (Sep 2026).
 *   - pyiso8583 4.0.1 (Python, PyPI), its built-in `default_ascii` spec.
 *
 * The message layout: a 4-character ASCII Message Type Indicator (MTI),
 * then the primary bitmap as 16 ASCII hex characters (64 bits, most
 * significant first; bit 1 is the leftmost), then each present data
 * element (DE) in ascending order. Bit n set means DE n is present; bit
 * 1 set means a secondary bitmap follows (DEs 65-128) -- this chapter's
 * own decoder refuses any message with bit 1 set, since it uses no DE
 * above 64. That bitmap layout is pyiso8583's `default_ascii`, and it is
 * the one moov-io's own docs/bitmap.md describes ("The primary bitmap is
 * 8 bytes long ... There may also be a secondary bitmap"). moov-io's
 * spec87ascii.go itself differs on this one point, found only when this
 * chapter's own cross-check first failed: its field 1 is declared with
 * Length 16, and moov-io counts that length in DECODED bytes, so that
 * spec always writes a 128-bit bitmap (32 hex characters) -- primary and
 * secondary -- even when bit 1 is clear. Both are self-consistent; this
 * chapter follows the 8-byte primary layout. The DEs this chapter uses:
 *
 *   DE  2  Primary Account Number     LLVAR, up to 19 (2-digit ASCII length)
 *   DE  3  Processing Code            fixed 6
 *   DE  4  Transaction Amount         fixed 12, zero-padded on the left
 *   DE  7  Transmission Date & Time   fixed 10 (MMDDhhmmss)
 *   DE 11  Systems Trace Audit Number fixed 6
 *   DE 12  Local Transaction Time     fixed 6 (hhmmss)
 *   DE 13  Local Transaction Date     fixed 4 (MMDD)
 *   DE 38  Authorization ID Response  fixed 6
 *   DE 39  Response Code              fixed 2
 *   DE 41  Card Acceptor Terminal ID  fixed 8
 *   DE 42  Card Acceptor ID Code      fixed 15
 *   DE 48  Additional Data - Private  LLLVAR, up to 999 (3-digit ASCII length)
 *   DE 49  Transaction Currency Code  fixed 3
 *
 * Code values, cited through web search results rather than the ISO
 * text itself (same honesty note as 035_bnpl.h): MTI 0100 is an
 * authorization request and 0110 its response (moov-io/iso8583's own
 * constant.go names both, "AuthorizationRequest"/"AuthorizationResponse");
 * DE 39 "00" means "Approved or completed successfully" (Elavon's
 * published Field 39 description); DE 3 "000000" is the conventional
 * processing code for a purchase with no account type specified, though
 * sources stress that each network defines its own operational mapping;
 * DE 49 "840" is the ISO 4217 numeric code for the US dollar.
 *
 * DE 48 is "private": both implementations define it only as a
 * variable-length string. What goes inside it is agreed between the
 * parties exchanging it, never by ISO 8583 itself. This chapter's own
 * BNPL plan payload inside DE 48 (035_bnpl.h's bnpl_encode_de48()) is
 * therefore this book's own layout, stated as such -- not a real
 * network's installment field.
 *
 * Real PANs end in a Luhn check digit (ISO/IEC 7812-1, "double the
 * value of alternate digits beginning with the first right-hand digit
 * (low order)" when computing it; cited via search results quoting the
 * 2015 edition). iso8583_luhn_check_digit()/iso8583_luhn_valid() below
 * implement it. This chapter's own demo PAN is fictional, invented for
 * this book, and only Luhn-valid so that the issuer side's own check
 * has something real to verify.
 *
 * Stated limits of this implementation: DE 4 amounts must fit a
 * uint32_t (the same no-64-bit-division limit Chapters 30 and 32 state);
 * DE 48 is capped at ISO8583_DE48_MAX bytes, well below the format's
 * own 999, because this chapter never needs more; any set bitmap bit
 * for a DE not listed above is refused outright, because a decoder that
 * does not know a field's width cannot skip it safely. */

#define ISO8583_PAN_MAX 19u
#define ISO8583_DE48_MAX 256u
#define ISO8583_MAX_MESSAGE_LEN 512u

typedef struct {
    uint8_t mti[4];
    uint32_t bitmap_hi;  /* bits 1-32: bit n is (1u << (32 - n)) */
    uint32_t bitmap_lo;  /* bits 33-64: bit n is (1u << (64 - n)) */

    uint8_t pan[ISO8583_PAN_MAX];      /* DE 2 */
    uint32_t pan_len;
    uint8_t processing_code[6];        /* DE 3 */
    uint32_t amount_cents;             /* DE 4 */
    uint8_t transmission_datetime[10]; /* DE 7 */
    uint8_t stan[6];                   /* DE 11 */
    uint8_t local_time[6];             /* DE 12 */
    uint8_t local_date[4];             /* DE 13 */
    uint8_t auth_id[6];                /* DE 38 */
    uint8_t response_code[2];          /* DE 39 */
    uint8_t terminal_id[8];            /* DE 41 */
    uint8_t merchant_id[15];           /* DE 42 */
    uint8_t additional_data[ISO8583_DE48_MAX]; /* DE 48 */
    uint32_t additional_data_len;
    uint8_t currency_code[3];          /* DE 49 */
} iso8583_msg_t;

void iso8583_set_field(iso8583_msg_t *msg, uint32_t de);
int iso8583_has_field(const iso8583_msg_t *msg, uint32_t de);

/* Encodes `msg` into `out`. Returns the encoded length, or 0 if the
 * buffer is too small, the MTI is not four ASCII digits, a bitmap bit
 * names an unsupported DE, or a variable-length field is too long. */
uint32_t iso8583_build(const iso8583_msg_t *msg, uint8_t *out, uint32_t out_size);

/* Decodes exactly `len` bytes into `*out`. Returns 1 on success, or 0 --
 * refusing outright, never guessing -- on a non-digit MTI, a non-hex
 * bitmap, bit 1 (secondary bitmap) set, any unsupported DE, a non-digit
 * byte in a numeric field or length prefix, an out-of-range length, a
 * DE 4 amount too large for a uint32_t, a truncated field, or any
 * trailing bytes after the last field. */
int iso8583_parse(const uint8_t *buf, uint32_t len, iso8583_msg_t *out);

/* Luhn (ISO/IEC 7812-1): the check digit ('0'-'9') for `len` ASCII
 * digits that do NOT yet include one, and a validity test for a full
 * PAN that does. Both return 0 / '\0' on any non-digit byte. */
uint8_t iso8583_luhn_check_digit(const uint8_t *digits, uint32_t len);
int iso8583_luhn_valid(const uint8_t *pan, uint32_t len);

#endif
