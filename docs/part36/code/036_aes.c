/* See 036_aes.h's own top-of-file comment for the full real citation of
 * every constant and transformation used here (NIST FIPS 197). */

#include "036_aes.h"

static const uint8_t g_sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

/* Real inverse S-box: derived directly from g_sbox above (inv_sbox[sbox[x]]
 * = x for every x), not transcribed separately -- this book's own way of
 * proving the inverse table is genuinely consistent with the forward table
 * rather than risking two independently-transcribed tables silently
 * disagreeing. Filled once at first use rather than hand-transcribed. */
static uint8_t g_inv_sbox[256];
static int g_inv_sbox_ready = 0;

static void ensure_inv_sbox(void) {
    if (g_inv_sbox_ready) {
        return;
    }
    for (uint32_t i = 0; i < 256u; i++) {
        g_inv_sbox[g_sbox[i]] = (uint8_t)i;
    }
    g_inv_sbox_ready = 1;
}

static const uint8_t g_rcon[10] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

/* Real GF(2^8) multiply-by-2 ("xtime"), reduced by FIPS 197's own m(x) =
 * x^8+x^4+x^3+x+1 (0x11B) whenever the top bit would overflow. */
static uint8_t xtime(uint8_t a) {
    uint8_t hi = (uint8_t)(a & 0x80u);
    uint8_t shifted = (uint8_t)(a << 1);
    if (hi) {
        shifted = (uint8_t)(shifted ^ 0x1Bu);
    }
    return shifted;
}

/* Real GF(2^8) multiply by an arbitrary constant, built from xtime()/XOR --
 * the standard real construction (a real GF(2^8) element is a polynomial;
 * multiplying by a constant c is the sum, over each set bit of c, of a XOR
 * of the input doubled that many times). */
static uint8_t gmul(uint8_t a, uint8_t b) {
    uint8_t result = 0;
    uint8_t x = a;
    uint8_t bb = b;
    for (uint32_t i = 0; i < 8u; i++) {
        if (bb & 1u) {
            result = (uint8_t)(result ^ x);
        }
        x = xtime(x);
        bb = (uint8_t)(bb >> 1);
    }
    return result;
}

void aes128_key_expand(const uint8_t key[AES_KEY_SIZE], uint8_t round_keys[176]) {
    /* w[0..3] = the real 128-bit key, taken 4 bytes at a time (Section 5.2). */
    for (uint32_t i = 0; i < 16u; i++) {
        round_keys[i] = key[i];
    }
    for (uint32_t i = 4u; i < 44u; i++) {
        uint8_t temp[4];
        temp[0] = round_keys[(i - 1u) * 4u + 0u];
        temp[1] = round_keys[(i - 1u) * 4u + 1u];
        temp[2] = round_keys[(i - 1u) * 4u + 2u];
        temp[3] = round_keys[(i - 1u) * 4u + 3u];
        if (i % 4u == 0u) {
            /* RotWord: real one-byte left rotate. */
            uint8_t t0 = temp[0];
            temp[0] = temp[1];
            temp[1] = temp[2];
            temp[2] = temp[3];
            temp[3] = t0;
            /* SubWord: real S-box applied to each byte. */
            temp[0] = g_sbox[temp[0]];
            temp[1] = g_sbox[temp[1]];
            temp[2] = g_sbox[temp[2]];
            temp[3] = g_sbox[temp[3]];
            /* XOR with the real Rcon for this round. */
            temp[0] = (uint8_t)(temp[0] ^ g_rcon[i / 4u - 1u]);
        }
        round_keys[i * 4u + 0u] = (uint8_t)(round_keys[(i - 4u) * 4u + 0u] ^ temp[0]);
        round_keys[i * 4u + 1u] = (uint8_t)(round_keys[(i - 4u) * 4u + 1u] ^ temp[1]);
        round_keys[i * 4u + 2u] = (uint8_t)(round_keys[(i - 4u) * 4u + 2u] ^ temp[2]);
        round_keys[i * 4u + 3u] = (uint8_t)(round_keys[(i - 4u) * 4u + 3u] ^ temp[3]);
    }
}

static void add_round_key(uint8_t state[16], const uint8_t round_key[16]) {
    for (uint32_t i = 0; i < 16u; i++) {
        state[i] = (uint8_t)(state[i] ^ round_key[i]);
    }
}

static void sub_bytes(uint8_t state[16]) {
    for (uint32_t i = 0; i < 16u; i++) {
        state[i] = g_sbox[state[i]];
    }
}

static void inv_sub_bytes(uint8_t state[16]) {
    ensure_inv_sbox();
    for (uint32_t i = 0; i < 16u; i++) {
        state[i] = g_inv_sbox[state[i]];
    }
}

/* State is stored column-major, state[r + 4*c], matching FIPS 197's own
 * s[r,c] indexing (Section 3.4). */
static void shift_rows(uint8_t state[16]) {
    uint8_t tmp[16];
    for (uint32_t r = 0; r < 4u; r++) {
        for (uint32_t c = 0; c < 4u; c++) {
            tmp[r + 4u * c] = state[r + 4u * ((c + r) % 4u)];
        }
    }
    for (uint32_t i = 0; i < 16u; i++) {
        state[i] = tmp[i];
    }
}

static void inv_shift_rows(uint8_t state[16]) {
    uint8_t tmp[16];
    for (uint32_t r = 0; r < 4u; r++) {
        for (uint32_t c = 0; c < 4u; c++) {
            tmp[r + 4u * c] = state[r + 4u * ((c + 4u - r) % 4u)];
        }
    }
    for (uint32_t i = 0; i < 16u; i++) {
        state[i] = tmp[i];
    }
}

static void mix_columns(uint8_t state[16]) {
    for (uint32_t c = 0; c < 4u; c++) {
        uint8_t a0 = state[4u * c + 0u];
        uint8_t a1 = state[4u * c + 1u];
        uint8_t a2 = state[4u * c + 2u];
        uint8_t a3 = state[4u * c + 3u];
        state[4u * c + 0u] = (uint8_t)(gmul(a0, 2u) ^ gmul(a1, 3u) ^ a2 ^ a3);
        state[4u * c + 1u] = (uint8_t)(a0 ^ gmul(a1, 2u) ^ gmul(a2, 3u) ^ a3);
        state[4u * c + 2u] = (uint8_t)(a0 ^ a1 ^ gmul(a2, 2u) ^ gmul(a3, 3u));
        state[4u * c + 3u] = (uint8_t)(gmul(a0, 3u) ^ a1 ^ a2 ^ gmul(a3, 2u));
    }
}

static void inv_mix_columns(uint8_t state[16]) {
    for (uint32_t c = 0; c < 4u; c++) {
        uint8_t a0 = state[4u * c + 0u];
        uint8_t a1 = state[4u * c + 1u];
        uint8_t a2 = state[4u * c + 2u];
        uint8_t a3 = state[4u * c + 3u];
        state[4u * c + 0u] = (uint8_t)(gmul(a0, 0x0eu) ^ gmul(a1, 0x0bu) ^ gmul(a2, 0x0du) ^ gmul(a3, 0x09u));
        state[4u * c + 1u] = (uint8_t)(gmul(a0, 0x09u) ^ gmul(a1, 0x0eu) ^ gmul(a2, 0x0bu) ^ gmul(a3, 0x0du));
        state[4u * c + 2u] = (uint8_t)(gmul(a0, 0x0du) ^ gmul(a1, 0x09u) ^ gmul(a2, 0x0eu) ^ gmul(a3, 0x0bu));
        state[4u * c + 3u] = (uint8_t)(gmul(a0, 0x0bu) ^ gmul(a1, 0x0du) ^ gmul(a2, 0x09u) ^ gmul(a3, 0x0eu));
    }
}

void aes128_encrypt_block(const uint8_t in[AES_BLOCK_SIZE], uint8_t out[AES_BLOCK_SIZE], const uint8_t round_keys[176]) {
    uint8_t state[16];
    for (uint32_t i = 0; i < 16u; i++) {
        state[i] = in[i];
    }
    add_round_key(state, &round_keys[0]);
    for (uint32_t round = 1u; round <= 9u; round++) {
        sub_bytes(state);
        shift_rows(state);
        mix_columns(state);
        add_round_key(state, &round_keys[round * 16u]);
    }
    sub_bytes(state);
    shift_rows(state);
    add_round_key(state, &round_keys[10u * 16u]);
    for (uint32_t i = 0; i < 16u; i++) {
        out[i] = state[i];
    }
}

void aes128_decrypt_block(const uint8_t in[AES_BLOCK_SIZE], uint8_t out[AES_BLOCK_SIZE], const uint8_t round_keys[176]) {
    uint8_t state[16];
    for (uint32_t i = 0; i < 16u; i++) {
        state[i] = in[i];
    }
    add_round_key(state, &round_keys[10u * 16u]);
    for (uint32_t round = 9u; round >= 1u; round--) {
        inv_shift_rows(state);
        inv_sub_bytes(state);
        add_round_key(state, &round_keys[round * 16u]);
        inv_mix_columns(state);
    }
    inv_shift_rows(state);
    inv_sub_bytes(state);
    add_round_key(state, &round_keys[0]);
    for (uint32_t i = 0; i < 16u; i++) {
        out[i] = state[i];
    }
}

void aes128_cbc_encrypt(const uint8_t *in, uint8_t *out, uint32_t len,
                         const uint8_t key[AES_KEY_SIZE], const uint8_t iv[AES_BLOCK_SIZE]) {
    uint8_t round_keys[176];
    aes128_key_expand(key, round_keys);
    uint8_t chain[AES_BLOCK_SIZE];
    for (uint32_t i = 0; i < AES_BLOCK_SIZE; i++) {
        chain[i] = iv[i];
    }
    for (uint32_t off = 0; off < len; off += AES_BLOCK_SIZE) {
        uint8_t block[AES_BLOCK_SIZE];
        for (uint32_t i = 0; i < AES_BLOCK_SIZE; i++) {
            block[i] = (uint8_t)(in[off + i] ^ chain[i]);
        }
        aes128_encrypt_block(block, &out[off], round_keys);
        for (uint32_t i = 0; i < AES_BLOCK_SIZE; i++) {
            chain[i] = out[off + i];
        }
    }
}

void aes128_cbc_decrypt(const uint8_t *in, uint8_t *out, uint32_t len,
                         const uint8_t key[AES_KEY_SIZE], const uint8_t iv[AES_BLOCK_SIZE]) {
    uint8_t round_keys[176];
    aes128_key_expand(key, round_keys);
    uint8_t chain[AES_BLOCK_SIZE];
    for (uint32_t i = 0; i < AES_BLOCK_SIZE; i++) {
        chain[i] = iv[i];
    }
    for (uint32_t off = 0; off < len; off += AES_BLOCK_SIZE) {
        uint8_t decrypted[AES_BLOCK_SIZE];
        aes128_decrypt_block(&in[off], decrypted, round_keys);
        for (uint32_t i = 0; i < AES_BLOCK_SIZE; i++) {
            out[off + i] = (uint8_t)(decrypted[i] ^ chain[i]);
        }
        for (uint32_t i = 0; i < AES_BLOCK_SIZE; i++) {
            chain[i] = in[off + i];
        }
    }
}
