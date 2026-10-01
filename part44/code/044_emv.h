#ifndef UNIX_OS_044_EMV_H
#define UNIX_OS_044_EMV_H

#include <stdint.h>
#include "044_tlv.h"

/* A real EMV contact chip transaction's own core mechanism: the
 * terminal's own GENERATE AC command, the chip's own real response
 * shape, a real Terminal Verification Results (TVR)-driven terminal
 * risk management decision, and the resulting Application Cryptogram.
 *
 * EMV's own official specifications (Books 1-4, emvco.com) are paid
 * and this book has not read them, the same honesty note as ISO 8583/
 * ISO 9564-1 since Chapters 33/38. Every real tag below -- its own
 * name and, where cited, its own real fixed width -- was instead read
 * out of TWO independent, open-source implementations that agree
 * exactly, field for field, the same cross-check discipline this book
 * has used since Chapter 33's own ISO 8583 codec:
 *
 *   - github.com/moov-io/bertlv (Go), its own emvtags.go, cloned and
 *     read directly.
 *   - github.com/misuher/EMV-tag-parser (C), its own emvTagList.c,
 *     cloned and read directly -- this one also states each tag's own
 *     real fixed width, cross-checked against the widths this chapter
 *     assigns below.
 *
 *   82    Application Interchange Profile (AIP)        fixed 2
 *   84    Dedicated File (DF) Name                      5-16 (this
 *                                                        chapter's own
 *                                                        demo AID: 8)
 *   94    Application File Locator (AFL)                variable, not
 *                                                        used by this
 *                                                        chapter's own
 *                                                        simplified
 *                                                        flow (real
 *                                                        AFL-driven
 *                                                        record reading
 *                                                        is out of
 *                                                        scope, stated
 *                                                        plainly)
 *   95    Terminal Verification Results (TVR)           fixed 5
 *   9A    Transaction Date                               fixed 3 (YYMMDD, BCD)
 *   9C    Transaction Type                               fixed 1
 *   5F2A  Transaction Currency Code                      fixed 2
 *   9F02  Amount, Authorised (Numeric)                   fixed 6 (BCD)
 *   9F1A  Terminal Country Code                          fixed 2
 *   9F37  Unpredictable Number (UN)                      fixed 4
 *   9F36  Application Transaction Counter (ATC)          fixed 2
 *   9F26  Application Cryptogram (AC)                    fixed 8
 *   9F27  Cryptogram Information Data (CID)               fixed 1
 *   9F10  Issuer Application Data (IAD)                  0-32 (this
 *                                                        chapter's own
 *                                                        demo: 8)
 *
 * The real GENERATE AC command/response shape (cited through search
 * results, the weaker honesty note this book has used since DE 39's
 * "00" in Chapter 33): a real command header CLA=0x80, INS=0xAE,
 * P1 naming the requested cryptogram type (0x80 for an Authorization
 * Request Cryptogram, ARQC), P2=0x00, carrying the terminal's own data
 * (this chapter's own DOL -- Data Object List -- is fixed and stated
 * as this chapter's own simplification, not a real card-supplied CDOL).
 * The real response is a primitive tag '80' object (or, alternately, a
 * real constructed tag '77' template -- this chapter builds only the
 * '80' shape) containing, in order, CID (1 byte), ATC (2 bytes), the
 * Application Cryptogram itself (8 bytes), and optional Issuer
 * Application Data.
 *
 * The real CID (Cryptogram Information Data) byte's own top two bits
 * name which of three real cryptogram types the chip actually returned
 * (cited through search results): 0b01xxxxxx is an ARQC (the chip is
 * asking to go online), 0b00xxxxxx is a TC (Transaction Certificate --
 * offline approval), 0b10xxxxxx is an AAC (Application Authentication
 * Cryptogram -- offline decline).
 *
 * What this chapter does NOT do, stated plainly: a real Application
 * Cryptogram is computed by the chip's own secure processor from a
 * real per-transaction session key, itself derived from an issuer
 * master key never exposed outside a real HSM, using 3DES or AES
 * (EMV Book 2, also paid and unread). This chapter has no chip, no
 * HSM, and no issuer key hierarchy to model faithfully -- its own
 * emv_generate_cryptogram() below is an explicitly invented substitute,
 * an HMAC-SHA256 (Chapter 30's own primitive) over the same real GENERATE
 * AC input data a real chip would sign, truncated to the real 8-byte
 * width. It occupies the same real place in the real flow and has the
 * same real shape on the wire; it is not, and does not claim to be,
 * a real EMV cryptogram. */

#define EMV_TAG_AIP        0x82u
#define EMV_TAG_TVR        0x95u
#define EMV_CID_ARQC 0x40u /* real top 2 bits: 01 */
#define EMV_CID_TC   0x00u /* real top 2 bits: 00 */
#define EMV_CID_AAC  0x80u /* real top 2 bits: 10 */
#define EMV_CID_TYPE_MASK 0xC0u

/* This chapter's own fixed GENERATE AC input fields -- the real data a
 * terminal's own CDOL1 names, simplified to a fixed list rather than a
 * real card-supplied Data Object List (this chapter's own stated
 * scope limit). */
typedef struct {
    uint8_t amount_authorized[6]; /* 9F02, BCD */
    uint8_t transaction_currency_code[2]; /* 5F2A */
    uint8_t transaction_date[3]; /* 9A, BCD YYMMDD */
    uint8_t transaction_type; /* 9C */
    uint8_t unpredictable_number[4]; /* 9F37 */
    uint8_t terminal_country_code[2]; /* 9F1A */
    uint8_t tvr[5]; /* 95 */
    uint16_t atc; /* 9F36 */
} emv_gac_input_t;

/* This chapter's own real-shaped GENERATE AC response: CID + ATC + AC
 * (+ IAD), matching the tag '80' primitive shape cited above. */
#define EMV_IAD_LEN 8u
typedef struct {
    uint8_t cid;
    uint16_t atc;
    uint8_t ac[8];
    uint8_t iad[EMV_IAD_LEN];
} emv_gac_response_t;

/* This chapter's own explicitly invented substitute for a real EMV
 * cryptogram -- see this file's own top comment. HMAC-SHA256, keyed by
 * `key`/`key_len`, over the real GENERATE AC input fields in `in` plus
 * `cid` itself (a real cryptogram's own session key is likewise bound
 * to the specific cryptogram type requested), truncated to 8 bytes. */
void emv_generate_cryptogram(const emv_gac_input_t *in, uint8_t cid, const uint8_t *key,
                              uint32_t key_len, uint8_t out_ac[8]);

/* This chapter's own terminal risk management check: real in shape
 * (inspecting real TVR bits to decide whether a transaction may be
 * approved offline), but this chapter's own specific rule, stated as
 * such -- not any one real terminal's own configured Terminal Action
 * Codes (a real, per-issuer configurable table this book does not
 * model). This chapter's own rule: if TVR byte 1 bit 8 (0x80, "offline
 * data authentication was not performed" -- cited through search
 * results) or bit 5 (0x08, "card appears on hotlist") is set, or the
 * authorized amount exceeds `floor_limit_cents`, the terminal requests
 * an ARQC (go online); otherwise it requests a TC (approve offline).
 * Returns the real CID top-bits value (EMV_CID_ARQC or EMV_CID_TC) the
 * terminal should ask for. */
uint8_t emv_terminal_risk_management(const uint8_t tvr[5], uint32_t amount_cents,
                                      uint32_t floor_limit_cents);

/* Builds the real tag '80' GENERATE AC response TLV object into `out`.
 * Returns the encoded length, or 0 if `out_size` is too small. */
uint32_t emv_build_gac_response(const emv_gac_response_t *resp, uint8_t *out, uint32_t out_size);

/* Parses a real tag '80' GENERATE AC response object out of `list`
 * (as already decoded by 044_tlv.c). Returns 1 on success, or 0 if tag
 * '80' is absent or its own value is not exactly CID(1)+ATC(2)+AC(8)
 * with, optionally, an IAD immediately after. */
int emv_parse_gac_response(const tlv_list_t *list, emv_gac_response_t *out);

#endif
