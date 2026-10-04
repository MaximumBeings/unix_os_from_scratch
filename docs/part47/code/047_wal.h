/* Chapter 47: the write-ahead log record format. A write-ahead log (WAL) is the oldest durability trick in databases: before a change is applied to the in-memory state, a
 * record describing the change is written to disk; after a crash, replaying the records in order rebuilds exactly the state the memory had. This file is only the RECORD
 * FORMAT and the checksum; where the records live (FAT16 files in the kernel, an array in the host-side tests) is the caller's business -- see mkt_replay()'s reader callback.
 *
 * One record = magic 'WAL1' (4 bytes), sequence number (4), payload length (4), payload (WAL_PAYLOAD bytes), CRC-32 of everything before it (4); all integers little-endian.
 * The CRC is the standard CRC-32 (IEEE 802.3 polynomial 0xEDB88320, reflected, init and final XOR 0xFFFFFFFF); its check value for the ASCII string "123456789" is 0xCBF43926,
 * the published check value for CRC-32, and the host test verifies it. A record whose magic, length, sequence number or CRC is wrong -- the shape a torn write (power lost mid-write)
 * leaves behind -- is REJECTED, and replay stops at the last good record: everything before the damage is kept, nothing after it is guessed at. */
#ifndef WAL_H
#define WAL_H
#include <stdint.h>
#define WAL_PAYLOAD 92u
#define WAL_RECORD (4u + 4u + 4u + WAL_PAYLOAD + 4u)
enum { WAL_OK = 0, WAL_ERR_SHORT = 1, WAL_ERR_MAGIC = 2, WAL_ERR_LEN = 3, WAL_ERR_SEQ = 4, WAL_ERR_CRC = 5 };
uint32_t wal_crc32(const uint8_t *data, uint32_t len);
void wal_encode(uint32_t seq, const uint8_t payload[WAL_PAYLOAD], uint8_t out[WAL_RECORD]);
int wal_decode(const uint8_t *buf, uint32_t len, uint32_t expect_seq, uint8_t payload_out[WAL_PAYLOAD]);
#endif
