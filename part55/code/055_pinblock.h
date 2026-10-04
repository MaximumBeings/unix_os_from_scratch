#ifndef UNIX_OS_055_PINBLOCK_H
#define UNIX_OS_055_PINBLOCK_H

#include <stdint.h>

/* A real ISO 9564-1 Format 0 PIN block -- the structure carried, hex-
 * encoded, in this chapter's own ISO 8583 DE 52 (055_iso8583.h/.c).
 *
 * ISO 9564-1 itself is a paid ISO standard this book has not read (the
 * same honesty note as 055_iso8583.h's own ISO 8583 citation). Every
 * nibble of this layout is instead cited through search results
 * describing the standard's own well-known "Format 0" structure:
 *
 *   nibble  0      control field, always 0 for Format 0
 *   nibble  1      PIN length N (4-12)
 *   nibbles 2..N+1 the PIN's own digits
 *   remaining      fill nibbles, each 0xF
 *
 * XORed, nibble for nibble, against a second 16-nibble field built from
 * the card's own PAN:
 *
 *   nibbles 0..3   always 0
 *   nibbles 4..15  the rightmost 12 digits of the PAN, EXCLUDING its own
 *                  Luhn check digit (iso8583_luhn_check_digit())
 *
 * Both 16-nibble fields are 8 real bytes; the XOR is this function's
 * own return value, and it is exactly the plaintext block a real PIN-
 * entry device hands to its own encryption step before transmission --
 * this book stops there (see the honesty note below on what this
 * chapter deliberately does NOT do).
 *
 * Independently cross-checked in this session, outside the kernel
 * entirely: a real, unrelated, open-source implementation --
 * github.com/luboid/pin-block-format-0 (C#), cloned locally -- was read
 * in full (PinBlockEncoder.cs, StringExtensions.cs) and its own two
 * published known-answer test vectors (PinBlockTestData.cs) were
 * reproduced by hand in Python: this exact nibble construction, followed
 * by real 3DES-ECB encryption (pycryptodome), reproduced BOTH of that
 * repository's own encrypted PIN blocks byte for byte:
 *
 *   PIN 1234, PAN 7777770000075101538, key 0123456789ABCDEFFEDCBA9876543210
 *     -> raw block 041234FFF8AEFEAC -> encrypted 81C2C3AF6CA221A5
 *   PIN 1313, PAN 0000100001899846,   key 98F849D580E001BF23B5834C16436B6B
 *     -> raw block 041312FFFFE7667B -> encrypted 60D99AF77B9A6DC7
 *
 * Both of that repository's own test vectors used a 4-digit PIN, which
 * cannot distinguish this book's single-hex-nibble length field (nibble
 * 1 holding N directly, so N=12 encodes as nibble value 0xC) from that
 * repository's own code, which instead writes N as two ASCII decimal
 * digits ("12") -- consuming an extra nibble and shifting every PIN
 * digit one place to the right for any N >= 10. The two constructions
 * agree for N <= 9 (both vectors above use N=4) but diverge for a
 * 10-12 digit PIN; this book follows the single-hex-nibble structure
 * search results describe as the real standard's own layout, stated
 * plainly since the two verified vectors cannot settle which one ISO
 * 9564-1 itself actually specifies.
 *
 * This chapter's own deliberate scope choice, confirmed before any code
 * was written: a real deployment encrypts this raw block again, under
 * its own dedicated PIN-encryption key (often inside a hardware
 * security module, and re-encrypted under a different zone key at every
 * hop toward the issuer) before it ever reaches DE 52. This book does
 * NOT do that second encryption -- the raw Format 0 block itself is
 * what travels in DE 52, and the whole ISO 8583 message carrying it is
 * instead sealed by Chapter 30's own AES-128-CBC + HMAC-SHA256
 * construction (055_aes.h/055_hmac.h), reusing this chapter's own
 * hardware loopback transport rather than reproducing a real PIN-
 * encryption-key hierarchy this book has no HSM to model. */

#define PINBLOCK_MIN_PIN_LEN 4u
#define PINBLOCK_MAX_PIN_LEN 12u
#define PINBLOCK_PAN_DIGITS 12u /* the real rightmost digits used, excluding the check digit */

/* Builds the real Format 0 block into `out8` (8 raw bytes). Returns 1
 * on success, or 0 -- refusing outright -- if `pin_len` is not in
 * [PINBLOCK_MIN_PIN_LEN, PINBLOCK_MAX_PIN_LEN], any PIN byte is not an
 * ASCII digit, `pan_len` is too short to hold 12 digits plus its own
 * check digit (pan_len < PINBLOCK_PAN_DIGITS + 1), or any byte in the
 * PAN's own rightmost 13 digits is not an ASCII digit. */
int pinblock_build_format0(const uint8_t *pin, uint32_t pin_len, const uint8_t *pan, uint32_t pan_len,
                            uint8_t out8[8]);

/* Recomputes the real Format 0 block for `pin`/`pan` and compares it,
 * byte for byte, against `block` (as received in DE 52). Returns 1 if
 * they match, 0 otherwise -- including whenever
 * pinblock_build_format0() itself would have refused. */
int pinblock_verify_format0(const uint8_t block[8], const uint8_t *pin, uint32_t pin_len, const uint8_t *pan,
                             uint32_t pan_len);

#endif
