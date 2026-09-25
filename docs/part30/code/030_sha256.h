#ifndef UNIX_OS_030_SHA256_H
#define UNIX_OS_030_SHA256_H

/* Real SHA-256, implemented field-for-field from a real primary source:
 * NIST FIPS 180-4, "Secure Hash Standard (SHS)"
 * (https://nvlpubs.nist.gov/nistpubs/fips/nist.fips.180-4.pdf), fetched and
 * quoted directly for this chapter's own research:
 *   - Initial hash values H0..H7 (Section 5.3.3): the first 32 bits of the
 *     fractional parts of the square roots of the first 8 primes.
 *   - Round constants K0..K63 (Section 4.2.2): the first 32 bits of the
 *     fractional parts of the cube roots of the first 64 primes.
 *   - Message schedule extension (Section 6.2.2, step 1):
 *     W[t] = sigma1(W[t-2]) + W[t-7] + sigma0(W[t-15]) + W[t-16] for t=16..63,
 *     where sigma0(x) = ROTR7(x) ^ ROTR18(x) ^ SHR3(x) and
 *     sigma1(x) = ROTR17(x) ^ ROTR19(x) ^ SHR10(x) (Section 4.1.2).
 *   - Compression function (Section 6.2.2, step 3): the real eight-variable
 *     (a..h) round using Ch(x,y,z) = (x&y) ^ (~x&z),
 *     Maj(x,y,z) = (x&y) ^ (x&z) ^ (y&z), BigSigma0(x) = ROTR2(x) ^ ROTR13(x)
 *     ^ ROTR22(x), BigSigma1(x) = ROTR6(x) ^ ROTR11(x) ^ ROTR25(x), all
 *     arithmetic modulo 2^32.
 *   - Padding (Section 5.1.1): message || 0x80 || zeros || 64-bit real
 *     big-endian bit length, padded to a multiple of 512 bits.
 *
 * A real, from-scratch, freestanding implementation -- correctness proven
 * against FIPS 180-4's own well-known published test vector
 * (SHA-256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff
 * 61f20015ad) before this code ever ran in the kernel.
 */

#include <stdint.h>

#define SHA256_DIGEST_SIZE 32u
#define SHA256_BLOCK_SIZE 64u

typedef struct {
    uint32_t h[8];
    uint8_t buffer[SHA256_BLOCK_SIZE];
    uint32_t buffer_len;
    uint64_t total_len;
} sha256_ctx_t;

void sha256_init(sha256_ctx_t *ctx);
void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, uint32_t len);
void sha256_final(sha256_ctx_t *ctx, uint8_t out[SHA256_DIGEST_SIZE]);

/* One-shot convenience wrapper over init/update/final. */
void sha256_hash(const uint8_t *data, uint32_t len, uint8_t out[SHA256_DIGEST_SIZE]);

#endif
