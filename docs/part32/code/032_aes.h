#ifndef UNIX_OS_032_AES_H
#define UNIX_OS_032_AES_H

/* Real AES-128 (Rijndael restricted to a 128-bit key/block), implemented
 * field-for-field from a real primary source: NIST FIPS 197,
 * "Advanced Encryption Standard (AES)"
 * (https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.197-upd1.pdf), fetched
 * and quoted directly for this chapter's own research:
 *   - Nk=4 (128-bit key), Nb=4 (128-bit block), Nr=10 rounds (Section 6.3).
 *   - GF(2^8) field arithmetic uses the reduction polynomial
 *     m(x) = x^8 + x^4 + x^3 + x + 1, i.e. 0x11B (Section 4.2).
 *   - SubBytes()/InvSubBytes() apply the real S-box / inverse S-box
 *     (Section 5.1.1 / 5.3.2, Table 4). The real S-box constant below was
 *     re-derived and cross-checked against the real published test vector
 *     in 032_aes.c's own top-of-file comment (a WebFetch extraction of the
 *     table came back column-major/transposed -- a PDF-table-extraction
 *     artifact, not a citation of a different table -- so the S-box here
 *     is this book's own carefully re-verified transcription, proven
 *     correct by matching FIPS 197's own published Appendix B example
 *     end-to-end rather than trusted from either source alone).
 *   - ShiftRows()/InvShiftRows() (Section 5.1.2 / 5.3.1): row r shifts
 *     left by r bytes to encrypt, right by r bytes (i.e. s'[r,c] =
 *     s[r,(c-r) mod 4]) to decrypt.
 *   - MixColumns()/InvMixColumns() (Section 5.1.3 / 5.3.3): each column
 *     multiplied in GF(2^8) by the fixed matrix with first word
 *     {02,03,01,01} to encrypt, {0e,0b,0d,09} to decrypt.
 *   - Key expansion (Section 5.2) uses SubWord/RotWord and the real Rcon
 *     values 01,02,04,08,10,20,40,80,1b,36 (powers of x in GF(2^8)).
 *   - CBC chaining (Cipher Block Chaining, cited separately in
 *     032_fedwire.h) is from NIST SP 800-38A, not FIPS 197 itself --
 *     FIPS 197 only defines the single-block cipher primitive.
 *
 * This is a real, from-scratch, freestanding (no libc, no hardware AES-NI)
 * implementation -- correctness proven against FIPS 197's own published
 * Appendix B test vector (key 000102030405060708090a0b0c0d0e0f, plaintext
 * 00112233445566778899aabbccddeeff, ciphertext
 * 69c4e0d86a7b0430d8cdb78070b4c55a) before this code ever ran in the kernel,
 * the same "test the primitive against a known-answer vector before trusting
 * it in the real system" discipline this book has used since Chapter 11's
 * own independent switch-count cross-check.
 */

#include <stdint.h>

#define AES_BLOCK_SIZE 16u
#define AES_KEY_SIZE 16u

void aes128_key_expand(const uint8_t key[AES_KEY_SIZE], uint8_t round_keys[176]);
void aes128_encrypt_block(const uint8_t in[AES_BLOCK_SIZE], uint8_t out[AES_BLOCK_SIZE], const uint8_t round_keys[176]);
void aes128_decrypt_block(const uint8_t in[AES_BLOCK_SIZE], uint8_t out[AES_BLOCK_SIZE], const uint8_t round_keys[176]);

/* CBC mode (NIST SP 800-38A): C1 = CIPH(P1 XOR IV); Cj = CIPH(Pj XOR Cj-1).
 * len must be a multiple of AES_BLOCK_SIZE (this chapter's own messages are
 * padded to a block multiple by 032_fedwire.c before encryption). */
void aes128_cbc_encrypt(const uint8_t *in, uint8_t *out, uint32_t len,
                         const uint8_t key[AES_KEY_SIZE], const uint8_t iv[AES_BLOCK_SIZE]);
void aes128_cbc_decrypt(const uint8_t *in, uint8_t *out, uint32_t len,
                         const uint8_t key[AES_KEY_SIZE], const uint8_t iv[AES_BLOCK_SIZE]);

#endif
