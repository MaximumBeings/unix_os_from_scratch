#ifndef UNIX_OS_031_HMAC_H
#define UNIX_OS_031_HMAC_H

/* Real HMAC (Keyed-Hashing for Message Authentication), implemented
 * field-for-field from a real primary source: RFC 2104
 * (https://www.rfc-editor.org/rfc/rfc2104.html), fetched and quoted
 * directly for this chapter's own research. The real construction:
 *
 *   HMAC(K, text) = H(K XOR opad, H(K XOR ipad, text))
 *
 * where H is the underlying hash function (SHA-256, this chapter's own
 * 031_sha256.h/.c, cited separately from FIPS 180-4), B is H's real block
 * size (64 bytes for SHA-256), ipad is the byte 0x36 repeated B times, and
 * opad is the byte 0x5C repeated B times. Per RFC 2104: "Applications that
 * use keys longer than B bytes will first hash the key using H and then
 * use the resultant L byte string as the actual key to HMAC" (L = the
 * hash's own output length, 32 bytes for SHA-256); a key shorter than B
 * bytes is zero-padded on the right to B bytes. RFC 2104 also states "the
 * minimal recommended length for K is L bytes" -- this chapter's own
 * fictional demo key is exactly L=32 bytes for that reason.
 *
 * This chapter uses HMAC for real message INTEGRITY (proving a received
 * message was not tampered with), paired with real AES-128-CBC
 * (031_aes.h/.c) for real message CONFIDENTIALITY, in the encrypt-then-MAC
 * order: encrypt first, then compute the real HMAC over the ciphertext --
 * the order this chapter's own 031_fedwire.c documents choosing (and why)
 * in its own top-of-file comment.
 */

#include <stdint.h>
#include "031_sha256.h"

#define HMAC_SHA256_KEY_SIZE 32u
#define HMAC_SHA256_TAG_SIZE 32u

void hmac_sha256(const uint8_t *key, uint32_t key_len,
                  const uint8_t *data, uint32_t data_len,
                  uint8_t out_tag[HMAC_SHA256_TAG_SIZE]);

#endif
