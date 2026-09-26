/* See 037_sha256.h's own top-of-file comment for the full real citation of
 * every constant and step used here (NIST FIPS 180-4). */

#include "037_sha256.h"

static const uint32_t g_h0[8] = {
    0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
};

static const uint32_t g_k[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static uint32_t rotr(uint32_t x, uint32_t n) {
    return (uint32_t)((x >> n) | (x << (32u - n)));
}

static void sha256_process_block(sha256_ctx_t *ctx, const uint8_t block[SHA256_BLOCK_SIZE]) {
    uint32_t w[64];
    for (uint32_t t = 0; t < 16u; t++) {
        w[t] = ((uint32_t)block[t * 4u + 0u] << 24) |
               ((uint32_t)block[t * 4u + 1u] << 16) |
               ((uint32_t)block[t * 4u + 2u] << 8) |
               ((uint32_t)block[t * 4u + 3u]);
    }
    for (uint32_t t = 16u; t < 64u; t++) {
        uint32_t s0 = rotr(w[t - 15u], 7u) ^ rotr(w[t - 15u], 18u) ^ (w[t - 15u] >> 3u);
        uint32_t s1 = rotr(w[t - 2u], 17u) ^ rotr(w[t - 2u], 19u) ^ (w[t - 2u] >> 10u);
        w[t] = (uint32_t)(w[t - 16u] + s0 + w[t - 7u] + s1);
    }

    uint32_t a = ctx->h[0], b = ctx->h[1], c = ctx->h[2], d = ctx->h[3];
    uint32_t e = ctx->h[4], f = ctx->h[5], g = ctx->h[6], h = ctx->h[7];

    for (uint32_t t = 0; t < 64u; t++) {
        uint32_t big_s1 = rotr(e, 6u) ^ rotr(e, 11u) ^ rotr(e, 25u);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = (uint32_t)(h + big_s1 + ch + g_k[t] + w[t]);
        uint32_t big_s0 = rotr(a, 2u) ^ rotr(a, 13u) ^ rotr(a, 22u);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = (uint32_t)(big_s0 + maj);
        h = g;
        g = f;
        f = e;
        e = (uint32_t)(d + t1);
        d = c;
        c = b;
        b = a;
        a = (uint32_t)(t1 + t2);
    }

    ctx->h[0] = (uint32_t)(ctx->h[0] + a);
    ctx->h[1] = (uint32_t)(ctx->h[1] + b);
    ctx->h[2] = (uint32_t)(ctx->h[2] + c);
    ctx->h[3] = (uint32_t)(ctx->h[3] + d);
    ctx->h[4] = (uint32_t)(ctx->h[4] + e);
    ctx->h[5] = (uint32_t)(ctx->h[5] + f);
    ctx->h[6] = (uint32_t)(ctx->h[6] + g);
    ctx->h[7] = (uint32_t)(ctx->h[7] + h);
}

void sha256_init(sha256_ctx_t *ctx) {
    for (uint32_t i = 0; i < 8u; i++) {
        ctx->h[i] = g_h0[i];
    }
    ctx->buffer_len = 0;
    ctx->total_len = 0;
}

void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, uint32_t len) {
    ctx->total_len += len;
    uint32_t off = 0;
    while (off < len) {
        uint32_t take = SHA256_BLOCK_SIZE - ctx->buffer_len;
        if (take > (len - off)) {
            take = len - off;
        }
        for (uint32_t i = 0; i < take; i++) {
            ctx->buffer[ctx->buffer_len + i] = data[off + i];
        }
        ctx->buffer_len += take;
        off += take;
        if (ctx->buffer_len == SHA256_BLOCK_SIZE) {
            sha256_process_block(ctx, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }
}

void sha256_final(sha256_ctx_t *ctx, uint8_t out[SHA256_DIGEST_SIZE]) {
    /* Real message bit length, taken before any padding is appended (the
     * padding itself is never counted -- FIPS 180-4 Section 5.1.1). */
    uint64_t bit_len = ctx->total_len * 8u;

    /* Real padding: a single 0x80 byte, then zeros, then the 64-bit
     * big-endian bit length, up to a multiple of the 512-bit block size. */
    ctx->buffer[ctx->buffer_len] = 0x80u;
    ctx->buffer_len++;

    if (ctx->buffer_len > 56u) {
        while (ctx->buffer_len < SHA256_BLOCK_SIZE) {
            ctx->buffer[ctx->buffer_len] = 0;
            ctx->buffer_len++;
        }
        sha256_process_block(ctx, ctx->buffer);
        ctx->buffer_len = 0;
    }
    while (ctx->buffer_len < 56u) {
        ctx->buffer[ctx->buffer_len] = 0;
        ctx->buffer_len++;
    }
    for (int i = 7; i >= 0; i--) {
        ctx->buffer[ctx->buffer_len] = (uint8_t)((bit_len >> (8 * i)) & 0xFFu);
        ctx->buffer_len++;
    }
    sha256_process_block(ctx, ctx->buffer);

    for (uint32_t i = 0; i < 8u; i++) {
        out[i * 4u + 0u] = (uint8_t)((ctx->h[i] >> 24) & 0xFFu);
        out[i * 4u + 1u] = (uint8_t)((ctx->h[i] >> 16) & 0xFFu);
        out[i * 4u + 2u] = (uint8_t)((ctx->h[i] >> 8) & 0xFFu);
        out[i * 4u + 3u] = (uint8_t)(ctx->h[i] & 0xFFu);
    }
}

void sha256_hash(const uint8_t *data, uint32_t len, uint8_t out[SHA256_DIGEST_SIZE]) {
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, out);
}
