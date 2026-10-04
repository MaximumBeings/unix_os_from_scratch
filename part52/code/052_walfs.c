/* Chapter 47: the write-ahead log on the FAT16 volume. See 052_walfs.h. */
#include "052_walfs.h"
#include "052_fat16.h"

static void name_of(uint32_t seq, char out[16]) {
    out[0] = 'W';
    for (int i = 7; i >= 1; i--) {
        out[i] = (char)('0' + seq % 10u);
        seq /= 10u;
    }
    out[8] = '.'; out[9] = 'L'; out[10] = 'O'; out[11] = 'G'; out[12] = 0;
}

int walfs_write(uint32_t seq, const uint8_t rec[WAL_RECORD]) {
    char n[16];
    name_of(seq, n);
    return fat16_create_file(n, rec, WAL_RECORD, 0) ? 0 : 1;
}

int walfs_read(uint32_t seq, uint8_t *buf, uint32_t size, uint32_t *out_len) {
    char n[16];
    name_of(seq, n);
    uint32_t got = 0;
    if (!fat16_read_file(n, buf, size, &got)) {
        return 1;
    }
    *out_len = got;
    return 0;
}

int walfs_tear(uint32_t seq, uint32_t keep_bytes) {
    char n[16];
    uint8_t buf[WAL_RECORD + 16];
    uint32_t got = 0;
    name_of(seq, n);
    if (!fat16_read_file(n, buf, sizeof(buf), &got) || keep_bytes > got || !fat16_delete_file(n)) {
        return 1;
    }
    return fat16_create_file(n, buf, keep_bytes, 0) ? 0 : 1;
}

int walfs_flip(uint32_t seq, uint32_t offset, uint8_t xor_mask) {
    char n[16];
    uint8_t buf[WAL_RECORD + 16];
    uint32_t got = 0;
    name_of(seq, n);
    if (!fat16_read_file(n, buf, sizeof(buf), &got) || offset >= got || !fat16_delete_file(n)) {
        return 1;
    }
    buf[offset] ^= xor_mask;
    return fat16_create_file(n, buf, got, 0) ? 0 : 1;
}

int walfs_restore(uint32_t seq, const uint8_t rec[WAL_RECORD]) {
    char n[16];
    name_of(seq, n);
    (void)fat16_delete_file(n);
    return fat16_create_file(n, rec, WAL_RECORD, 0) ? 0 : 1;
}

int walfs_drop(uint32_t seq) {
    char n[16];
    name_of(seq, n);
    return fat16_delete_file(n) ? 0 : 1;
}
