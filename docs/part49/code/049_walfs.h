/* Chapter 47: where the write-ahead log lives in the KERNEL: one FAT16 file per record, named by its sequence number -- W0000001.LOG, W0000002.LOG, ... (8.3 names, in the root
 * directory of Chapters 19-23's own FAT16 volume on the real ATA disk). Why one file per record: this book's FAT16 driver can create a file with its whole contents in one call
 * (fat16_create_file) and read one back, but has no append -- so a record is the unit of atomicity: it either exists completely or (if power is lost while it is being written)
 * does not, or is short, which 049_wal.h's CRC and length checks reject. A production system would append to one file and fsync; the record format and the replay logic above this
 * layer would not change. The host-side tests (native/market_test.c) use an in-memory array behind the same two callbacks instead. */
#ifndef WALFS_H
#define WALFS_H
#include <stdint.h>
#include "049_wal.h"
int walfs_write(uint32_t seq, const uint8_t rec[WAL_RECORD]); /* 0 on success (matches wal_writer_t) */
int walfs_read(uint32_t seq, uint8_t *buf, uint32_t size, uint32_t *out_len); /* 0 on success (matches wal_reader_t) */
/* Fault injection for the demo, to simulate what a crash or bad sector leaves behind: replace record `seq` by its first keep_bytes bytes (a torn write), or flip bits in it. 0 on success. */
int walfs_tear(uint32_t seq, uint32_t keep_bytes);
int walfs_flip(uint32_t seq, uint32_t offset, uint8_t xor_mask);
int walfs_drop(uint32_t seq); /* delete record `seq` (truncating a damaged log tail before appending again) */
int walfs_restore(uint32_t seq, const uint8_t rec[WAL_RECORD]); /* delete and rewrite the full record */
#endif
