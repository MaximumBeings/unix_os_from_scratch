/* See 033_hmac.h's own top-of-file comment for the full real citation of
 * the HMAC construction used here (RFC 2104). */

#include "033_hmac.h"

void hmac_sha256(const uint8_t *key, uint32_t key_len,
                  const uint8_t *data, uint32_t data_len,
                  uint8_t out_tag[HMAC_SHA256_TAG_SIZE]) {
    uint8_t key_block[SHA256_BLOCK_SIZE];

    if (key_len > SHA256_BLOCK_SIZE) {
        /* RFC 2104: a key longer than the block size B is first hashed
         * down to L=32 bytes, then zero-padded to B like any short key. */
        uint8_t hashed_key[SHA256_DIGEST_SIZE];
        sha256_hash(key, key_len, hashed_key);
        for (uint32_t i = 0; i < SHA256_DIGEST_SIZE; i++) {
            key_block[i] = hashed_key[i];
        }
        for (uint32_t i = SHA256_DIGEST_SIZE; i < SHA256_BLOCK_SIZE; i++) {
            key_block[i] = 0;
        }
    } else {
        for (uint32_t i = 0; i < key_len; i++) {
            key_block[i] = key[i];
        }
        for (uint32_t i = key_len; i < SHA256_BLOCK_SIZE; i++) {
            key_block[i] = 0;
        }
    }

    /* Real ipad/opad: the byte 0x36 / 0x5C repeated B times, XORed with
     * the (possibly hashed-down, always zero-padded) key. */
    uint8_t ipad_key[SHA256_BLOCK_SIZE];
    uint8_t opad_key[SHA256_BLOCK_SIZE];
    for (uint32_t i = 0; i < SHA256_BLOCK_SIZE; i++) {
        ipad_key[i] = (uint8_t)(key_block[i] ^ 0x36u);
        opad_key[i] = (uint8_t)(key_block[i] ^ 0x5Cu);
    }

    /* Inner hash: H(K XOR ipad, text). */
    sha256_ctx_t inner_ctx;
    sha256_init(&inner_ctx);
    sha256_update(&inner_ctx, ipad_key, SHA256_BLOCK_SIZE);
    sha256_update(&inner_ctx, data, data_len);
    uint8_t inner_digest[SHA256_DIGEST_SIZE];
    sha256_final(&inner_ctx, inner_digest);

    /* Outer hash: H(K XOR opad, inner_digest). */
    sha256_ctx_t outer_ctx;
    sha256_init(&outer_ctx);
    sha256_update(&outer_ctx, opad_key, SHA256_BLOCK_SIZE);
    sha256_update(&outer_ctx, inner_digest, SHA256_DIGEST_SIZE);
    sha256_final(&outer_ctx, out_tag);
}
