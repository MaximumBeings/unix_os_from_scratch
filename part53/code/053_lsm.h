/* Chapter 53: a log-structured merge-tree (LSM) key-value store: the storage design of LevelDB, RocksDB and Cassandra, small enough to read in one sitting.
 *
 *   put/delete -> write-ahead log (WAL, one CRC-protected record per operation, appended BEFORE the operation is acknowledged)
 *              -> memtable (a sorted array in memory)
 *   memtable full -> FLUSH: written as an immutable sorted table file (T00007.SST), listed in a MANIFEST, WAL emptied
 *   too many tables -> COMPACTION: every table merged into one bottom-level table; shadowed versions and tombstones (delete markers) are dropped
 *   get -> memtable, then tables newest first; a per-table Bloom filter and key range skip tables that cannot hold the key
 *   open -> pick the newest valid MANIFEST, load the tables, delete orphan files, replay the WAL's valid prefix, cut off a torn tail
 *
 * CRASH SAFETY CONTRACT (what the chapter's tests check at EVERY byte of a workload): after a crash at any instant, open() recovers a state equal to the state after the k operations that were acknowledged
 * (put/delete returned LSM_OK), or after k+1 (the operation that was in flight when the crash came may or may not have reached the log); never anything else, never a corrupt store.
 * The store talks to storage only through lsm_fs_t, so the same code runs on FAT16 files in the kernel and on a RAM file system with a crash injector on the host. The file system must make delete atomic;
 * write() and append() may be TORN by a crash (a PREFIX of the new bytes survives). Nothing is assumed about the order in which different files reach the disk beyond that: the engine only relies on write() having returned.
 *
 * LIMITS (chapter page lists them): keys 1..24 bytes, values 0..100 bytes, 64 entries in the memtable, at most 400 keys in a table, 12 tables, a 4,096-byte WAL; single-threaded; the whole table file is read at open and
 * single blocks (at most 8 entries) afterwards; the file format is this chapter's own (LevelDB-inspired, NOT byte-compatible with LevelDB). */
#ifndef LSM_H
#define LSM_H
#include <stdint.h>

enum { LSM_OK = 0, LSM_ERR_ARG = -1, LSM_ERR_FULL = -2, LSM_ERR_IO = -3, LSM_ERR_CORRUPT = -4 };
#define LSM_MAX_KEY 24
#define LSM_MAX_VAL 100
#define LSM_MEM_MAX 64       /* entries in the memtable */
#define LSM_MEM_BYTES 1024   /* memtable payload bytes that trigger a flush */
#define LSM_WAL_MAX 4096     /* WAL bytes that trigger a flush */
#define LSM_L0_MAX 4         /* level-0 tables that trigger a compaction */
#define LSM_MAX_TABLES 12
#define LSM_MAX_ENTRIES 400  /* keys in one table */
#define LSM_IDX_EVERY 8      /* one sparse-index entry per 8 table entries */
#define LSM_IO_BYTES 65536

typedef struct lsm_fs {
    void *ctx;
    int (*read)(void *ctx, const char *name, uint32_t off, uint8_t *buf, uint32_t len, uint32_t *got); /* 0 ok (got = bytes read, may be < len at end of file), nonzero if absent */
    int (*write)(void *ctx, const char *name, const uint8_t *data, uint32_t len); /* create or replace; a crash may leave a prefix */
    int (*append)(void *ctx, const char *name, const uint8_t *data, uint32_t len); /* create if absent; a crash may leave a prefix of data appended */
    int (*del)(void *ctx, const char *name); /* atomic; deleting an absent file is not an error */
    int (*size)(void *ctx, const char *name, uint32_t *size); /* 0 ok, nonzero if absent */
} lsm_fs_t;

typedef struct { uint8_t klen, tomb; uint16_t vlen; uint32_t seq; uint8_t key[LSM_MAX_KEY]; uint8_t val[LSM_MAX_VAL]; } lsm_ent_t;
typedef struct { uint32_t off; uint8_t klen; uint8_t key[LSM_MAX_KEY]; } lsm_idx_t;
typedef struct {
    uint32_t id; uint8_t level; uint32_t n, size, data_len, idx_n, min_seq, max_seq; uint16_t bloom_len; uint8_t minlen, maxlen; uint8_t minkey[LSM_MAX_KEY], maxkey[LSM_MAX_KEY];
    uint8_t bloom[512]; lsm_idx_t idx[LSM_MAX_ENTRIES / LSM_IDX_EVERY + 1];
} lsm_tab_t;
typedef struct { uint32_t puts, deletes, flushes, compactions, gets, bloom_skips, range_skips, block_reads, wal_replayed, wal_cut_bytes, orphans_deleted; } lsm_stats_t;
typedef struct {
    lsm_fs_t fs; lsm_ent_t mem[LSM_MEM_MAX]; uint32_t nmem, membytes, wal_bytes; uint32_t seq, flushed_seq, next_id, gen; uint8_t failed; /* set by any I/O error: the store then refuses every write, like a database that has gone read-only */
    lsm_tab_t tab[LSM_MAX_TABLES]; uint32_t ntab; /* newest level-0 table first, the level-1 table (if any) last */
    lsm_stats_t st; uint8_t io[LSM_IO_BYTES + 256]; uint8_t blk[8 * (8 + LSM_MAX_KEY + LSM_MAX_VAL) + 16];
} lsm_t;

int lsm_open(lsm_t *s, const lsm_fs_t *fs); /* recovers; LSM_OK, or LSM_ERR_CORRUPT / LSM_ERR_IO */
int lsm_put(lsm_t *s, const uint8_t *key, uint32_t klen, const uint8_t *val, uint32_t vlen);
int lsm_delete(lsm_t *s, const uint8_t *key, uint32_t klen);
int lsm_get(lsm_t *s, const uint8_t *key, uint32_t klen, uint8_t *out, uint32_t cap, uint32_t *vlen); /* 1 found, 0 not found, <0 error; vlen is the value's length (out receives at most cap bytes) */
int lsm_flush(lsm_t *s); /* memtable -> a new level-0 table (a no-op when empty); may then compact */
int lsm_compact(lsm_t *s); /* all tables -> one level-1 table, tombstones dropped */
int lsm_digest(lsm_t *s, uint8_t out[32], uint32_t *count); /* SHA-256 over every live pair in key order: klen, vlen, key, value; count = live keys */
int lsm_table_parse(const uint8_t *buf, uint32_t size, lsm_tab_t *out); /* validates a table file image completely; 0 ok */
uint32_t lsm_crc32(const uint8_t *p, uint32_t n);
#endif
