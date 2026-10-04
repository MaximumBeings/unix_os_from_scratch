/* Chapter 47: the write-ahead log record format. See 051_wal.h. */
#include "051_wal.h"

uint32_t wal_crc32(const uint8_t *data, uint32_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void wal_encode(uint32_t seq, const uint8_t payload[WAL_PAYLOAD], uint8_t out[WAL_RECORD]) {
    put32(out, 0x314C4157u); /* 'W','A','L','1' read as little-endian bytes */
    put32(out + 4, seq);
    put32(out + 8, WAL_PAYLOAD);
    for (uint32_t i = 0; i < WAL_PAYLOAD; i++) {
        out[12 + i] = payload[i];
    }
    put32(out + 12 + WAL_PAYLOAD, wal_crc32(out, 12 + WAL_PAYLOAD));
}

int wal_decode(const uint8_t *buf, uint32_t len, uint32_t expect_seq, uint8_t payload_out[WAL_PAYLOAD]) {
    if (len < WAL_RECORD) {
        return WAL_ERR_SHORT;
    }
    if (get32(buf) != 0x314C4157u) {
        return WAL_ERR_MAGIC;
    }
    if (get32(buf + 8) != WAL_PAYLOAD) {
        return WAL_ERR_LEN;
    }
    if (get32(buf + 12 + WAL_PAYLOAD) != wal_crc32(buf, 12 + WAL_PAYLOAD)) {
        return WAL_ERR_CRC;
    }
    if (get32(buf + 4) != expect_seq) {
        return WAL_ERR_SEQ; /* checked after the CRC so a damaged record reports damage, not a misleading sequence error */
    }
    for (uint32_t i = 0; i < WAL_PAYLOAD; i++) {
        payload_out[i] = buf[12 + i];
    }
    return WAL_OK;
}
