#ifndef UNIX_OS_045_GS1_H
#define UNIX_OS_045_GS1_H

#include <stdint.h>

/* A real GS1 GDTI (Global Document Type Identifier, Application
 * Identifier 253) -- the real GS1 barcode element string this
 * chapter's own fictional event ticket carries, exactly the kind of
 * "document type, with an optional serial number" GS1's own published
 * material describes AI 253 for (cited through search results, the
 * weaker honesty note this book has used since DE 39's "00", since
 * this specific real-world application to event tickets was not found
 * as an official GS1 example).
 *
 * GS1's own official specification text (gs1.org) is blocked by this
 * sandbox's network egress policy, the same pattern every blocked-
 * standards-site chapter since Chapter 33 has hit. But GS1 AISBL's own
 * real GitHub organization is not: this chapter cloned
 * github.com/gs1/gs1-syntax-engine -- GS1's own official open-source
 * repository -- and read two of its own real files in full:
 *
 *   src/c-lib/gs1-syntax-dictionary.txt (GS1's own real "Barcode
 *     Syntax Dictionary", release 2026-01-27) -- its own real entry
 *     for AI 253:
 *       "253  ?  N13,csum,gcppos1 [X..17]  dlpkey  # GDTI"
 *     read literally: a mandatory 13-digit numeric component (with a
 *     real GS1 check digit, "csum", and the GS1 Company Prefix
 *     starting at position 1, "gcppos1"), optionally followed by 1-17
 *     more alphanumeric characters (GS1's own "CSET 82") -- a real
 *     serial component.
 *   src/c-lib/syntax/lint_csum.c -- GS1's own real reference
 *     implementation of that same check digit algorithm, read in
 *     full and reproduced field-for-field below: sum the numeric
 *     value of every digit except the last, weighted alternately by 3
 *     then 1 starting from the RIGHTMOST digit of that span (which
 *     this file's own real comment states as "weighted by alternating
 *     ...3:1:3 values, from right to left"); the real check digit is
 *     whatever value makes that sum plus itself a multiple of 10.
 *     GS1's own real unit tests in that same file supplied this
 *     chapter's own independently-reproduced known-answer vectors:
 *     "1234567890128" and "12345678901231" both pass; changing either
 *     one's own last digit by one fails.
 *
 * This chapter's own real, cited convention for representing an AI
 * element string outside an actual barcode symbol -- a parenthesised
 * "(AI)data" form, e.g. "(253)1234567890128SECA-R5-S12" -- is GS1's
 * own real "bracketed" human-readable representation (cited through
 * search results), the form printed under a real barcode symbol
 * rather than the real FNC1-separated bytes the symbol itself
 * actually encodes; this chapter builds and parses that same
 * bracketed text form, not a real barcode image.
 *
 * Stated limits of this implementation: only AI 253 is supported (this
 * chapter's own scope limit -- GS1's own real dictionary defines
 * hundreds of AIs this chapter has no use for); the optional serial
 * component is capped at GS1_SERIAL_MAX bytes, matching the real
 * dictionary's own "X..17" (up to 17); this codec accepts any
 * printable ASCII in the serial component rather than implementing
 * GS1's own real, narrower CSET 82 character set validation, stated
 * as such. */

#define GS1_GDTI_LEN 13u
#define GS1_SERIAL_MAX 17u

typedef struct {
    uint8_t gdti[GS1_GDTI_LEN]; /* real 13 digits, including the real check digit */
    uint8_t serial[GS1_SERIAL_MAX];
    uint32_t serial_len;
} gs1_ticket_id_t;

/* GS1's own real check digit algorithm (lint_csum.c, read in full and
 * cited above), computed over the 12 real digits in `digits` (which do
 * NOT yet include a check digit). Returns the real check digit
 * ('0'-'9'), or 0 if any byte in `digits` is not an ASCII digit. */
uint8_t gs1_checksum(const uint8_t digits[12]);

/* The same real algorithm, applied to a full real 13-digit GDTI that
 * DOES already include its own check digit as the 13th byte. Returns
 * 1 if valid, 0 otherwise (including any non-digit byte). */
int gs1_gdti_valid(const uint8_t gdti[GS1_GDTI_LEN]);

/* Builds the real bracketed "(253)<13 real GDTI digits><serial>" form
 * into `out`. Returns the encoded length, or 0 if `out_size` is too
 * small, `t->gdti` is not a valid real GDTI (gs1_gdti_valid()), or
 * `t->serial_len` exceeds GS1_SERIAL_MAX. */
uint32_t gs1_build_element_string(const gs1_ticket_id_t *t, uint8_t *out, uint32_t out_size);

/* Parses exactly `len` bytes of that same real bracketed form back
 * into `*out`. Returns 1 on success, or 0 -- refusing outright -- if
 * the AI is not "253", the 13-digit GDTI's own real check digit is
 * invalid, the serial component exceeds GS1_SERIAL_MAX bytes, or any
 * serial byte is not printable ASCII. */
int gs1_parse_element_string(const uint8_t *buf, uint32_t len, gs1_ticket_id_t *out);

#endif
