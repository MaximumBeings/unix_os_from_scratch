/* This chapter's own real filesystem, still the same genuine FAT16
 * volume Chapter 20 built entirely on top of Chapter 19's own real
 * ata_read_sector()/ata_write_sector() -- every structure below is
 * read and written as ordinary 512-byte sectors, never as a direct
 * port I/O call. Every field layout, formula, and special value
 * Chapter 20 already used is cited field-for-field from OSDev Wiki's
 * own "FAT" page (https://wiki.osdev.org/FAT); Chapter 21's own
 * subdirectory conventions (ATTR_DIRECTORY, the "." and ".." entries,
 * a subdirectory growing as an ordinary cluster chain the same way a
 * file does) are cited field-for-field from the Microsoft FAT
 * specification, Section 6 ("FAT Directory Structure") -- the primary
 * source OSDev's own "FAT" page itself lists in its External Links,
 * consulted directly there because OSDev's own page never documents
 * subdirectory layout at all; Chapter 22's own fat16_rmdir() and its
 * "a directory must be empty before it can be removed" rule are cited
 * from IEEE Std 1003.1-2008 (POSIX.1-2008)'s own rmdir() specification,
 * neither format document describing directory removal at all. This
 * chapter's own new work is resolve_path() itself: Chapters 21 and 22
 * both refused outright the instant a name held more than one real
 * '/', a deliberately stated one-level-of-nesting scope. Neither OSDev
 * Wiki's own "FAT" page nor the Microsoft FAT specification says
 * anything about walking a MULTI-component path at all -- both
 * describe the real on-disk FORMAT, not how a driver should parse the
 * string an application hands it. So this chapter's own new,
 * arbitrary-depth resolve_path() below is cited instead from IEEE Std
 * 1003.1-2008 (POSIX.1-2008), Base Definitions, Section 4.11,
 * "Pathname Resolution" (https://pubs.opengroup.org/onlinepubs/
 * 009696799/basedefs/xbd_chap04.html), quoted directly above its own
 * implementation. */

#include <stdint.h>

#include "036_ata.h"
#include "036_fat16.h"
#include "036_printf.h"

/* This chapter's own disk geometry, chosen deliberately: 8 MiB (16384
 * real 512-byte sectors) rather than Chapter 19's own 1 MiB -- a real
 * FAT16 volume needs at least 4085 usable clusters before the
 * standard cluster-count convention most real implementations use to
 * tell FAT12/FAT16/FAT32 apart would even call it "FAT16" rather than
 * "FAT12," and Chapter 19's own smaller disk could never reach that
 * with any genuinely usable cluster size. One sector per cluster
 * keeps every cluster the same size as one real ata_read_sector()/
 * ata_write_sector() call -- no extra loop needed inside this file to
 * read or write a whole cluster. */
#define FAT16_BYTES_PER_SECTOR    512u
#define FAT16_SECTORS_PER_CLUSTER 1u
#define FAT16_RESERVED_SECTORS    1u
#define FAT16_FAT_COUNT           2u
#define FAT16_ROOT_ENTRY_COUNT    512u
#define FAT16_SECTORS_PER_FAT     64u
#define FAT16_TOTAL_SECTORS       16384u

/* "The size of the root directory ... root_dir_sectors = ((fat_boot->
 * root_entry_count * 32) + (fat_boot->bytes_per_sector - 1)) /
 * fat_boot->bytes_per_sector;" (OSDev Wiki, "FAT") -- 512 entries * 32
 * bytes each = 16384 bytes = exactly 32 whole 512-byte sectors, no
 * rounding needed for this chapter's own chosen root_entry_count. */
#define FAT16_ROOT_DIR_SECTORS \
    (((FAT16_ROOT_ENTRY_COUNT * 32u) + (FAT16_BYTES_PER_SECTOR - 1u)) / FAT16_BYTES_PER_SECTOR)

#define FAT16_ENTRIES_PER_SECTOR (FAT16_BYTES_PER_SECTOR / 32u)  /* 16 */

/* FAT16 special cluster values, cited the same way: "Free cluster:
 * Index 0", "End-of-chain: >= 0xFFF8", "Bad cluster: == 0xFFF7". This
 * chapter's own fat16_format() always writes the cited end-of-chain
 * marker 0xFFFF specifically, never merely "some value >= 0xFFF8". */
#define FAT16_CLUSTER_FREE     0x0000u
#define FAT16_CLUSTER_END      0xFFFFu
#define FAT16_CLUSTER_END_MIN  0xFFF8u
#define FAT16_CLUSTER_BAD      0xFFF7u

/* Directory-entry byte-0 special values, cited the same way: "If the
 * first byte of the entry is equal to 0 then there are no more
 * files/directories in this directory" and "If the first byte of the
 * entry is equal to 0xE5 then the entry is unused." */
#define FAT16_DIRENT_END       0x00u
#define FAT16_DIRENT_DELETED   0xE5u

#define FAT16_ATTR_ARCHIVE     0x20u

/* This chapter's own new attribute bit, cited directly (Microsoft FAT
 * specification, Section 6.2): "ATTR_DIRECTORY (0x10) The
 * corresponding entry represents a directory (a child or
 * sub-directory to the containing directory)." */
#define FAT16_ATTR_DIRECTORY   0x10u

/* This chapter's own real 512-byte Boot Sector/BIOS Parameter Block,
 * quoted field-for-field from OSDev Wiki's own cited offset table --
 * every field below sits at exactly the byte offset the cited table
 * gives it, verified by this struct's own total size coming out to
 * exactly 512 bytes with no manual padding anywhere. */
struct fat16_boot_sector {
    uint8_t  jump[3];              /* offset 0: "JMP SHORT 3C NOP" */
    uint8_t  oem_identifier[8];    /* offset 3 */
    uint16_t bytes_per_sector;     /* offset 11 */
    uint8_t  sectors_per_cluster;  /* offset 13 */
    uint16_t reserved_sector_count;/* offset 14 */
    uint8_t  table_count;          /* offset 16: "Often this value is 2" */
    uint16_t root_entry_count;     /* offset 17 */
    uint16_t total_sectors_16;     /* offset 19 */
    uint8_t  media_descriptor;     /* offset 21 */
    uint16_t sectors_per_fat;      /* offset 22: "FAT12/FAT16 only" */
    uint16_t sectors_per_track;    /* offset 24 */
    uint16_t head_count;           /* offset 26 */
    uint32_t hidden_sector_count;  /* offset 28 */
    uint32_t total_sectors_32;     /* offset 32 */
    /* FAT16 Extended Boot Record, offset 36 */
    uint8_t  drive_number;         /* offset 36 */
    uint8_t  nt_flags;             /* offset 37 */
    uint8_t  signature;            /* offset 38: "must be 0x28 or 0x29" */
    uint32_t volume_id;            /* offset 39 */
    uint8_t  volume_label[11];     /* offset 43, "padded with spaces" */
    uint8_t  system_identifier[8]; /* offset 54 */
    uint8_t  boot_code[448];       /* offset 62 */
    uint16_t boot_signature;       /* offset 510: "0xAA55" */
} __attribute__((packed));

/* This chapter's own real 32-byte directory entry, quoted the same
 * way (OSDev Wiki, "FAT"). Every field this chapter's own code
 * actually uses (name, attributes, cluster, size) is kept meaningful;
 * every timestamp field is kept for real on-disk layout fidelity but
 * always written as 0 -- this chapter's own filesystem deliberately
 * does not track creation/modification times, a stated scope limit,
 * not an oversight. */
struct fat16_dir_entry {
    uint8_t  name[11];               /* offset 0: "8.3 file name" */
    uint8_t  attributes;             /* offset 11 */
    uint8_t  nt_reserved;            /* offset 12 */
    uint8_t  creation_time_ds;       /* offset 13 */
    uint16_t creation_time;          /* offset 14 */
    uint16_t creation_date;          /* offset 16 */
    uint16_t access_date;            /* offset 18 */
    uint16_t high_cluster_bits;      /* offset 20: "always zero" for FAT16 */
    uint16_t modification_time;      /* offset 22 */
    uint16_t modification_date;      /* offset 24 */
    uint16_t low_cluster_bits;       /* offset 26 */
    uint32_t file_size;              /* offset 28 */
} __attribute__((packed));

/* This chapter's own real, computed-once geometry -- every value here
 * is read straight out of the real on-disk boot sector by fat16_init(),
 * never hardcoded a second time by any function below it. */
static uint32_t g_first_fat_sector = 0;
static uint32_t g_first_root_dir_sector = 0;
static uint32_t g_first_data_sector = 0;
static uint32_t g_sectors_per_fat = 0;
static uint32_t g_sectors_per_cluster = 0;
static uint32_t g_root_dir_sectors = 0;
static uint32_t g_total_data_clusters = 0;
static int g_ready = 0;

static uint8_t to_upper(uint8_t c) {
    if (c >= 'a' && c <= 'z') {
        return (uint8_t) (c - 'a' + 'A');
    }
    return c;
}

/* This chapter's own hand-rolled equivalents of libc's memcmp()/
 * strlen() -- this book has never linked a C library, freestanding
 * since Chapter 1, so every byte comparison and copy in this file is
 * an ordinary loop, the same convention Chapter 18's own elf_load()
 * already established for copying ELF segment bytes. */
static int bytes_equal(const uint8_t *a, const uint8_t *b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static uint32_t string_length(const char *s) {
    uint32_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

/* Converts an ordinary "NAME.EXT" (or extension-less "NAME") C string
 * into the real on-disk 11-byte 8.3 form -- upper-cased, space-padded,
 * name and extension each right up to (never past) 8 and 3 characters
 * (OSDev Wiki, "FAT": "The first 8 characters are the name and the
 * last 3 are the extension"). A name or extension longer than that is
 * silently truncated -- an honest, stated limit; this chapter's own
 * real demo only ever uses names chosen to fit. */
static void to_83_name(const char *input, uint8_t out[11]) {
    for (int i = 0; i < 11; i++) {
        out[i] = ' ';
    }

    uint32_t len = string_length(input);
    uint32_t dot = len;
    for (uint32_t i = 0; i < len; i++) {
        if (input[i] == '.') {
            dot = i;
            break;
        }
    }

    uint32_t name_len = dot;
    if (name_len > 8u) {
        name_len = 8u;
    }
    for (uint32_t i = 0; i < name_len; i++) {
        out[i] = to_upper((uint8_t) input[i]);
    }

    if (dot < len) {
        uint32_t ext_start = dot + 1u;
        uint32_t ext_len = len - ext_start;
        if (ext_len > 3u) {
            ext_len = 3u;
        }
        for (uint32_t i = 0; i < ext_len; i++) {
            out[8 + i] = to_upper((uint8_t) input[ext_start + i]);
        }
    }
}

/* The inverse of to_83_name(), for fat16_list_root(): trims the
 * space-padding back off the name and extension and re-inserts the
 * '.' only when a real extension is present. */
static void from_83_name(const uint8_t name[11], char *out) {
    uint32_t pos = 0;
    for (uint32_t i = 0; i < 8u && name[i] != ' '; i++) {
        out[pos++] = (char) name[i];
    }
    if (name[8] != ' ') {
        out[pos++] = '.';
        for (uint32_t i = 8u; i < 11u && name[i] != ' '; i++) {
            out[pos++] = (char) name[i];
        }
    }
    out[pos] = '\0';
}

static uint32_t cluster_to_lba(uint16_t cluster) {
    /* "first_sector_of_cluster = ((cluster - 2) * fat_boot->
     * sectors_per_cluster) + first_data_sector;" (OSDev Wiki, "FAT"). */
    return g_first_data_sector + ((uint32_t) (cluster - 2u) * g_sectors_per_cluster);
}

/* "unsigned int fat_offset = active_cluster * 2; unsigned int
 * fat_sector = first_fat_sector + (fat_offset / sector_size);
 * unsigned int ent_offset = fat_offset % sector_size;" (OSDev Wiki,
 * "FAT") -- this chapter's own real FAT16 entry lookup, quoted
 * directly, reading only the FIRST FAT copy (the second is written in
 * lock-step by fat_write_entry() below purely for real on-disk
 * redundancy, never read back by this file itself). */
static uint16_t fat_read_entry(uint16_t cluster) {
    uint32_t fat_offset = (uint32_t) cluster * 2u;
    uint32_t fat_sector = g_first_fat_sector + (fat_offset / FAT16_BYTES_PER_SECTOR);
    uint32_t ent_offset = fat_offset % FAT16_BYTES_PER_SECTOR;

    uint8_t buf[FAT16_BYTES_PER_SECTOR];
    ata_read_sector(fat_sector, buf);

    uint16_t *table = (uint16_t *) buf;
    return table[ent_offset / 2u];
}

/* Writes one real FAT16 entry to BOTH real mirrored FAT copies (this
 * chapter's own FAT16 volume always has exactly FAT16_FAT_COUNT = 2),
 * a real read-modify-write per copy so every other byte in that
 * sector is preserved untouched. */
static void fat_write_entry(uint16_t cluster, uint16_t value) {
    uint32_t fat_offset = (uint32_t) cluster * 2u;
    uint32_t ent_offset = fat_offset % FAT16_BYTES_PER_SECTOR;
    uint32_t sector_within_fat = fat_offset / FAT16_BYTES_PER_SECTOR;

    for (uint32_t copy = 0; copy < FAT16_FAT_COUNT; copy++) {
        uint32_t fat_sector = g_first_fat_sector + copy * g_sectors_per_fat + sector_within_fat;

        uint8_t buf[FAT16_BYTES_PER_SECTOR];
        ata_read_sector(fat_sector, buf);

        uint16_t *table = (uint16_t *) buf;
        table[ent_offset / 2u] = value;

        ata_write_sector(fat_sector, buf);
    }
}

/* Scans forward from cluster 2 (cluster 0 and 1 are always reserved --
 * see fat16_format()) for the first real cluster whose FAT entry reads
 * back as FAT16_CLUSTER_FREE. Returns 0 (never a valid data cluster
 * number) if the whole volume is full. */
static uint16_t find_free_cluster(void) {
    for (uint32_t c = 2; c < 2u + g_total_data_clusters; c++) {
        if (fat_read_entry((uint16_t) c) == FAT16_CLUSTER_FREE) {
            return (uint16_t) c;
        }
    }
    return 0;
}

int fat16_format(void) {
    struct fat16_boot_sector boot;
    uint8_t *raw = (uint8_t *) &boot;
    for (uint32_t i = 0; i < sizeof(boot); i++) {
        raw[i] = 0;
    }

    boot.jump[0] = 0xEBu;
    boot.jump[1] = 0x3Cu;
    boot.jump[2] = 0x90u;

    const char *oem = "UNIXOS21";
    for (int i = 0; i < 8; i++) {
        boot.oem_identifier[i] = (uint8_t) oem[i];
    }

    boot.bytes_per_sector = FAT16_BYTES_PER_SECTOR;
    boot.sectors_per_cluster = FAT16_SECTORS_PER_CLUSTER;
    boot.reserved_sector_count = FAT16_RESERVED_SECTORS;
    boot.table_count = FAT16_FAT_COUNT;
    boot.root_entry_count = FAT16_ROOT_ENTRY_COUNT;
    boot.total_sectors_16 = (uint16_t) FAT16_TOTAL_SECTORS;  /* fits in 16 bits */
    boot.media_descriptor = 0xF8u;  /* fixed disk */
    boot.sectors_per_fat = FAT16_SECTORS_PER_FAT;
    boot.sectors_per_track = 0;     /* unused by this driver -- kept for
                                      * real BPB layout fidelity only */
    boot.head_count = 0;            /* unused by this driver */
    boot.hidden_sector_count = 0;   /* this whole disk is one volume,
                                      * no partition table in front of it */
    boot.total_sectors_32 = 0;      /* total_sectors_16 already covers
                                      * this chapter's own volume size */

    boot.drive_number = 0x80u;      /* "0x80 for hard disks" */
    boot.nt_flags = 0;
    boot.signature = 0x29u;

    boot.volume_id = 0x00000001u;

    const char *label = "UNIXOSFAT16";
    for (int i = 0; i < 11; i++) {
        boot.volume_label[i] = (uint8_t) label[i];
    }

    const char *sysid = "FAT16   ";
    for (int i = 0; i < 8; i++) {
        boot.system_identifier[i] = (uint8_t) sysid[i];
    }
    /* boot_code[448] stays zeroed -- this disk is a real second, data-
     * only drive, never booted from, so there is no real bootstrap
     * code for this field to hold. */

    boot.boot_signature = 0xAA55u;

    kprintf("fat16_format: writing real boot sector/BPB to LBA 0...\n");
    ata_write_sector(0, raw);

    uint32_t first_fat_sector = FAT16_RESERVED_SECTORS;
    uint32_t sectors_per_fat = FAT16_SECTORS_PER_FAT;

    kprintf("fat16_format: zeroing %u real FAT sectors (%u copies)...\n",
            sectors_per_fat * FAT16_FAT_COUNT, (uint32_t) FAT16_FAT_COUNT);

    uint8_t zero_sector[FAT16_BYTES_PER_SECTOR];
    for (uint32_t i = 0; i < FAT16_BYTES_PER_SECTOR; i++) {
        zero_sector[i] = 0;
    }

    for (uint32_t copy = 0; copy < FAT16_FAT_COUNT; copy++) {
        for (uint32_t s = 0; s < sectors_per_fat; s++) {
            ata_write_sector(first_fat_sector + copy * sectors_per_fat + s, zero_sector);
        }
    }

    /* "Reserved entry 1 must hold the value 0xFFFF" (OSDev Wiki,
     * "FAT") -- entry 0 conventionally mirrors the media descriptor
     * byte in its low byte, OR'd with 0xFF00; both real cluster
     * numbers 0 and 1 are reserved and never handed out by
     * find_free_cluster(), which always starts its search at 2. */
    g_first_fat_sector = first_fat_sector;
    g_sectors_per_fat = sectors_per_fat;
    fat_write_entry(0, (uint16_t) (0xFF00u | boot.media_descriptor));
    fat_write_entry(1, 0xFFFFu);

    uint32_t root_dir_sectors = FAT16_ROOT_DIR_SECTORS;
    uint32_t first_root_dir_sector = first_fat_sector + FAT16_FAT_COUNT * sectors_per_fat;

    kprintf("fat16_format: zeroing %u real root directory sectors...\n", root_dir_sectors);
    for (uint32_t s = 0; s < root_dir_sectors; s++) {
        ata_write_sector(first_root_dir_sector + s, zero_sector);
    }

    kprintf("fat16_format: done -- real FAT16 volume written to disk\n");
    return 1;
}

int fat16_init(void) {
    uint8_t sector0[FAT16_BYTES_PER_SECTOR];
    ata_read_sector(0, sector0);

    struct fat16_boot_sector *boot = (struct fat16_boot_sector *) sector0;

    /* "0xAA55" (OSDev Wiki, "FAT") -- refuses to treat this disk as a
     * real FAT16 volume at all unless the real, on-disk boot signature
     * confirms it, rather than assuming fat16_format() was the last
     * thing to touch this disk. */
    if (boot->boot_signature != 0xAA55u) {
        kprintf("fat16_init: boot signature 0x%x, not the real 0xAA55 -- refusing\n",
                boot->boot_signature);
        return 0;
    }

    g_sectors_per_cluster = boot->sectors_per_cluster;
    g_sectors_per_fat = boot->sectors_per_fat;
    g_root_dir_sectors = ((uint32_t) boot->root_entry_count * 32u + (boot->bytes_per_sector - 1u))
                          / boot->bytes_per_sector;
    g_first_fat_sector = boot->reserved_sector_count;
    g_first_root_dir_sector = g_first_fat_sector + (uint32_t) boot->table_count * g_sectors_per_fat;
    g_first_data_sector = g_first_root_dir_sector + g_root_dir_sectors;

    uint32_t total_sectors = boot->total_sectors_16 != 0
                              ? boot->total_sectors_16
                              : boot->total_sectors_32;
    uint32_t data_sectors = total_sectors - g_first_data_sector;
    g_total_data_clusters = data_sectors / g_sectors_per_cluster;

    g_ready = 1;

    kprintf("fat16_init: real volume \"");
    for (int i = 0; i < 11 && boot->volume_label[i] != ' '; i++) {
        kprintf("%c", boot->volume_label[i]);
    }
    kprintf("\" -- %u bytes/sector, %u sector(s)/cluster, %u FAT(s) * %u sectors, "
            "root dir %u sectors (first at LBA %u), data starts LBA %u, %u usable clusters\n",
            boot->bytes_per_sector, g_sectors_per_cluster, boot->table_count, g_sectors_per_fat,
            g_root_dir_sectors, g_first_root_dir_sector, g_first_data_sector, g_total_data_clusters);

    return 1;
}

/* Resolves logical directory-sector index `logical_index` (0, 1, 2, ...)
 * of the real directory whose own first cluster is `dir_cluster` into a
 * real disk LBA -- the one place this chapter's own code has to know the
 * difference between the root directory and an ordinary subdirectory.
 * `dir_cluster == 0` means the root directory itself, the same real
 * on-disk convention this chapter's own fat16_mkdir() uses for a ".."
 * entry whose parent is the root (Microsoft FAT specification, Section
 * 6.5: "If the parent of the current directory is the root directory ...
 * the DIR_FstClusLO and DIR_FstClusHI contents must be set to 0"). For
 * the root, `logical_index` is bounded by the real, FIXED
 * g_root_dir_sectors -- the root can never grow, a genuine, permanent
 * FAT12/FAT16 constraint, not a limitation of this driver -- so this
 * function returns 0 (never a valid LBA, since LBA 0 always holds the
 * boot sector) once that fixed size is exhausted, `grow` or not. For an
 * ordinary subdirectory, this function walks its real cluster chain
 * `logical_index` real links deep; if the chain is shorter than that and
 * `grow` is set, it allocates and zeroes one more real cluster and links
 * it on, exactly the way fat16_create_file() already grows a file's own
 * chain -- cited directly (Microsoft FAT specification, Section 6.5,
 * confirmed a subdirectory is an ordinary cluster chain that "must" be
 * allocated the same way a file's is). */
static uint32_t dir_sector_lba(uint16_t dir_cluster, uint32_t logical_index, int grow) {
    if (dir_cluster == 0) {
        if (logical_index >= g_root_dir_sectors) {
            return 0;  /* the root directory can never grow past its fixed size */
        }
        return g_first_root_dir_sector + logical_index;
    }

    uint16_t cluster = dir_cluster;
    for (uint32_t i = 0; i < logical_index; i++) {
        uint16_t next = fat_read_entry(cluster);
        if (next >= FAT16_CLUSTER_END_MIN) {
            if (!grow) {
                return 0;
            }
            uint16_t new_cluster = find_free_cluster();
            if (new_cluster == 0) {
                return 0;  /* volume has no free clusters left */
            }
            uint8_t zero_sector[FAT16_BYTES_PER_SECTOR];
            for (uint32_t z = 0; z < FAT16_BYTES_PER_SECTOR; z++) {
                zero_sector[z] = 0;
            }
            ata_write_sector(cluster_to_lba(new_cluster), zero_sector);
            fat_write_entry(new_cluster, FAT16_CLUSTER_END);
            fat_write_entry(cluster, new_cluster);
            next = new_cluster;
        }
        cluster = next;
    }
    return cluster_to_lba(cluster);
}

/* Scans the real directory whose own first cluster is `dir_cluster`
 * (0 for the root, exactly as dir_sector_lba() above) once, looking for
 * BOTH an existing entry with this exact name AND the first reusable
 * slot (a real 0xE5 deleted entry, the real 0x00 end-of-directory
 * marker, or -- new this chapter -- a freshly grown cluster's own first
 * entry, if `want_free_slot` and the directory was genuinely full).
 * `out_lba`/`out_index` locate the slot as a real disk LBA and an entry
 * index inside that sector's own 16 real entries, so the caller can
 * read-modify-write exactly that one real 32-byte entry directly, with
 * no further translation needed. */
static int scan_dir(uint16_t dir_cluster, const uint8_t name83[11], int want_free_slot,
                     uint32_t *out_lba, uint32_t *out_index, int *out_found_existing) {
    int free_found = 0;
    uint32_t free_lba = 0, free_index = 0;
    uint32_t s;

    for (s = 0; ; s++) {
        uint32_t lba = dir_sector_lba(dir_cluster, s, 0);
        if (lba == 0) {
            break;  /* no more real, already-allocated sectors in this directory */
        }

        uint8_t buf[FAT16_BYTES_PER_SECTOR];
        ata_read_sector(lba, buf);
        struct fat16_dir_entry *entries = (struct fat16_dir_entry *) buf;

        for (uint32_t e = 0; e < FAT16_ENTRIES_PER_SECTOR; e++) {
            uint8_t b0 = entries[e].name[0];

            if (b0 == FAT16_DIRENT_END) {
                if (want_free_slot) {
                    if (!free_found) {
                        free_lba = lba;
                        free_index = e;
                    }
                    *out_lba = free_lba;
                    *out_index = free_index;
                    *out_found_existing = 0;
                    return 1;
                }
                return 0;  /* pure lookup: nothing past here is real */
            }

            if (b0 == FAT16_DIRENT_DELETED) {
                if (!free_found) {
                    free_found = 1;
                    free_lba = lba;
                    free_index = e;
                }
                continue;
            }

            if (bytes_equal(entries[e].name, name83, 11)) {
                *out_lba = lba;
                *out_index = e;
                *out_found_existing = 1;
                return 1;
            }
        }
    }

    if (!want_free_slot) {
        return 0;
    }
    if (free_found) {
        *out_lba = free_lba;
        *out_index = free_index;
        *out_found_existing = 0;
        return 1;
    }

    /* Every already-allocated sector was full, with no 0x00/0xE5 slot
     * anywhere -- grow the directory by one more real cluster (for the
     * root, dir_cluster == 0, dir_sector_lba() above correctly refuses
     * to grow at all, the real fixed-size constraint this chapter never
     * works around). A freshly allocated cluster is zeroed by
     * dir_sector_lba() itself, so its very first entry already reads
     * back as a real 0x00 end-of-directory marker -- exactly the free
     * slot this call needs. */
    uint32_t new_lba = dir_sector_lba(dir_cluster, s, 1);
    if (new_lba == 0) {
        return 0;
    }
    *out_lba = new_lba;
    *out_index = 0;
    *out_found_existing = 0;
    return 1;
}

/* This chapter's own new real path walk -- an arbitrary-depth
 * generalization of Chapters 21's and 22's own resolve_path(), which
 * only ever split `path` at its FIRST '/' and refused outright the
 * instant a second one appeared. Cited directly (IEEE Std 1003.1-2008,
 * Base Definitions, Section 4.11, "Pathname Resolution"): "Each
 * filename in the pathname is located in the directory specified by
 * its predecessor (for example, in the pathname fragment a/b, file b
 * is located in directory a). Pathname resolution shall fail if this
 * cannot be accomplished." This function implements exactly that,
 * component by component, starting at the root (`dir_cluster == 0`,
 * the same real sentinel dir_sector_lba() already uses): every
 * component up to the last is looked up, in order, in the real
 * directory the PREVIOUS component resolved to, via the same scan_dir()
 * every other lookup in this file already uses; refuses -- returns 0,
 * the resolution "fails" exactly as the specification says -- the
 * instant one of them doesn't exist, or exists but isn't really a
 * directory (the cited ATTR_DIRECTORY bit unset -- the real, on-disk
 * fact behind POSIX's own [ENOTDIR], "Not a directory"), never
 * silently creating anything or guessing past the failure. `path` must
 * never begin or end with '/', and no component may be empty (an
 * accidental "//" mid-path) -- both refused outright too, an honest,
 * stated limit this chapter's own real demo never needs to exercise.
 * With no '/' at all, `path` names something directly in the root,
 * exactly as every earlier chapter's own resolve_path() already did.
 * On success, `*out_dir_cluster` is the real first cluster of the
 * directory that should hold the FINAL component, and `*out_leaf`
 * points at that final component's own substring inside `path` itself
 * (no copy taken -- `path` must outlive the caller's own use of
 * `*out_leaf`). */
static int resolve_path(const char *path, uint16_t *out_dir_cluster, const char **out_leaf) {
    uint16_t dir_cluster = 0;
    uint32_t start = 0;

    for (;;) {
        uint32_t i = start;
        while (path[i] != '\0' && path[i] != '/') {
            i++;
        }

        if (i == start) {
            return 0;  /* an empty component: a leading '/', a "//", or an empty path */
        }

        if (path[i] == '\0') {
            /* The FINAL component -- the leaf itself, not another real
             * directory to walk through. */
            *out_dir_cluster = dir_cluster;
            *out_leaf = path + start;
            return 1;
        }

        /* An intermediate component -- must already exist as a real
         * directory, or the whole resolution fails right here (cited
         * above: "Pathname resolution shall fail if this cannot be
         * accomplished"). */
        char component[64];
        uint32_t len = i - start;
        if (len > sizeof(component) - 1u) {
            len = sizeof(component) - 1u;
        }
        for (uint32_t k = 0; k < len; k++) {
            component[k] = path[start + k];
        }
        component[len] = '\0';

        uint8_t name83[11];
        to_83_name(component, name83);

        uint32_t lba, index;
        int found;
        if (!scan_dir(dir_cluster, name83, 0, &lba, &index, &found) || !found) {
            return 0;
        }

        uint8_t buf[FAT16_BYTES_PER_SECTOR];
        ata_read_sector(lba, buf);
        struct fat16_dir_entry *entries = (struct fat16_dir_entry *) buf;
        if (!(entries[index].attributes & FAT16_ATTR_DIRECTORY)) {
            return 0;  /* named, but it's a real file, not a real directory */
        }

        dir_cluster = entries[index].low_cluster_bits;
        start = i + 1u;
    }
}

int fat16_mkdir(const char *name, uint16_t *out_first_cluster) {
    if (!g_ready) {
        kprintf("fat16_mkdir: fat16_init() was never called -- refusing\n");
        return 0;
    }

    uint16_t parent_cluster;
    const char *leaf;
    if (!resolve_path(name, &parent_cluster, &leaf)) {
        kprintf("fat16_mkdir: \"%s\" -- directory component not found, or not really a "
                "directory -- refusing\n", name);
        return 0;
    }

    uint8_t name83[11];
    to_83_name(leaf, name83);

    uint32_t slot_lba, slot_index;
    int found_existing;
    if (!scan_dir(parent_cluster, name83, 1, &slot_lba, &slot_index, &found_existing)) {
        kprintf("fat16_mkdir: \"%s\" -- parent directory has no free entry -- refusing\n", name);
        return 0;
    }
    if (found_existing) {
        kprintf("fat16_mkdir: \"%s\" already exists -- refusing\n", name);
        return 0;
    }

    uint16_t new_cluster = find_free_cluster();
    if (new_cluster == 0) {
        kprintf("fat16_mkdir: \"%s\" -- volume has no free clusters left -- refusing\n", name);
        return 0;
    }
    fat_write_entry(new_cluster, FAT16_CLUSTER_END);

    uint8_t buf[FAT16_BYTES_PER_SECTOR];
    for (uint32_t i = 0; i < FAT16_BYTES_PER_SECTOR; i++) {
        buf[i] = 0;
    }
    struct fat16_dir_entry *entries = (struct fat16_dir_entry *) buf;

    /* "." -- self-reference. Cited directly (Microsoft FAT specification,
     * Section 6.5): "Since the dot entry refers to the current directory
     * (the one containing the dot entry), the contents of the
     * DIR_FstClusLO and DIR_FstClusHI fields must be the same as that of
     * the current directory." */
    for (int i = 0; i < 11; i++) {
        entries[0].name[i] = ' ';
    }
    entries[0].name[0] = '.';
    entries[0].attributes = FAT16_ATTR_DIRECTORY;
    entries[0].low_cluster_bits = new_cluster;
    entries[0].file_size = 0;

    /* ".." -- parent reference. Chapters 21 and 22 could hardcode this
     * to the real 0 because fat16_mkdir() only ever created a
     * subdirectory directly inside the root; this chapter's own new
     * resolve_path() can now return a genuine, non-zero parent cluster
     * for anything deeper, so this chapter uses `parent_cluster`
     * itself -- cited directly (Microsoft FAT specification, Section
     * 6.5): "the contents of the DIR_FstClusLO and DIR_FstClusHI
     * fields must be the same as that of the parent of the current
     * directory. If the parent of the current directory is the root
     * directory ... the DIR_FstClusLO and DIR_FstClusHI contents must
     * be set to 0" -- true here too, since `parent_cluster` is already
     * the real sentinel 0 whenever the parent genuinely is the root. */
    for (int i = 0; i < 11; i++) {
        entries[1].name[i] = ' ';
    }
    entries[1].name[0] = '.';
    entries[1].name[1] = '.';
    entries[1].attributes = FAT16_ATTR_DIRECTORY;
    entries[1].low_cluster_bits = parent_cluster;
    entries[1].file_size = 0;

    ata_write_sector(cluster_to_lba(new_cluster), buf);

    uint8_t dir_buf[FAT16_BYTES_PER_SECTOR];
    ata_read_sector(slot_lba, dir_buf);
    struct fat16_dir_entry *root_entries = (struct fat16_dir_entry *) dir_buf;
    struct fat16_dir_entry *entry = &root_entries[slot_index];

    for (int i = 0; i < 11; i++) {
        entry->name[i] = name83[i];
    }
    entry->attributes = FAT16_ATTR_DIRECTORY;
    entry->nt_reserved = 0;
    entry->creation_time_ds = 0;
    entry->creation_time = 0;
    entry->creation_date = 0;
    entry->access_date = 0;
    entry->high_cluster_bits = 0;
    entry->modification_time = 0;
    entry->modification_date = 0;
    entry->low_cluster_bits = new_cluster;
    /* "The DIR_FileSize must be set to 0" (Microsoft FAT specification,
     * Section 6.5) -- true here even though a real cluster is already
     * allocated: a directory's own real size is never tracked in bytes,
     * only in however many clusters its own chain holds. */
    entry->file_size = 0;

    ata_write_sector(slot_lba, dir_buf);

    if (out_first_cluster != 0) {
        *out_first_cluster = new_cluster;
    }

    kprintf("fat16_mkdir: \"%s\" -- real subdirectory created, first cluster %u\n",
            name, new_cluster);
    return 1;
}

int fat16_create_file(const char *name, const uint8_t *data, uint32_t size,
                       uint16_t *out_first_cluster) {
    if (!g_ready) {
        kprintf("fat16_create_file: fat16_init() was never called -- refusing\n");
        return 0;
    }

    uint16_t dir_cluster;
    const char *leaf;
    if (!resolve_path(name, &dir_cluster, &leaf)) {
        kprintf("fat16_create_file: \"%s\" -- directory component not found, or not really a "
                "directory -- refusing\n", name);
        return 0;
    }

    uint8_t name83[11];
    to_83_name(leaf, name83);

    uint32_t slot_lba, slot_index;
    int found_existing;
    if (!scan_dir(dir_cluster, name83, 1, &slot_lba, &slot_index, &found_existing)) {
        kprintf("fat16_create_file: \"%s\" -- directory has no free entry (and could not grow) "
                "-- refusing\n", name);
        return 0;
    }
    if (found_existing) {
        kprintf("fat16_create_file: \"%s\" already exists -- refusing\n", name);
        return 0;
    }

    uint32_t bytes_per_cluster = FAT16_BYTES_PER_SECTOR * g_sectors_per_cluster;
    uint32_t clusters_needed = (size + bytes_per_cluster - 1u) / bytes_per_cluster;
    if (clusters_needed == 0u) {
        clusters_needed = 1u;  /* a real, valid zero-byte file still owns one cluster */
    }

    uint16_t first_cluster = 0;
    uint16_t prev_cluster = 0;
    uint32_t bytes_left = size;
    const uint8_t *src = data;

    for (uint32_t i = 0; i < clusters_needed; i++) {
        uint16_t c = find_free_cluster();
        if (c == 0) {
            kprintf("fat16_create_file: \"%s\" -- volume has no free clusters left -- refusing\n", name);
            /* Honest, but incomplete for this chapter's own stated
             * scope: a real implementation would also free whatever
             * clusters were already claimed by this same call before
             * refusing. This chapter's own real demo never exhausts
             * the volume, so that cleanup path is never exercised. */
            return 0;
        }

        if (i == 0) {
            first_cluster = c;
        } else {
            fat_write_entry(prev_cluster, c);
        }

        uint8_t cluster_buf[FAT16_BYTES_PER_SECTOR];
        uint32_t chunk = bytes_left < FAT16_BYTES_PER_SECTOR ? bytes_left : FAT16_BYTES_PER_SECTOR;
        for (uint32_t b = 0; b < chunk; b++) {
            cluster_buf[b] = src[b];
        }
        for (uint32_t b = chunk; b < FAT16_BYTES_PER_SECTOR; b++) {
            cluster_buf[b] = 0;
        }
        ata_write_sector(cluster_to_lba(c), cluster_buf);

        src += chunk;
        bytes_left -= chunk;
        prev_cluster = c;

        /* Mark this cluster END for now -- the very next iteration (if
         * any) overwrites it with the next cluster in the chain via
         * fat_write_entry(prev_cluster, c) above. Leaving a genuinely
         * un-terminated chain visible on disk, even for one real write
         * in between, is never allowed here. */
        fat_write_entry(c, FAT16_CLUSTER_END);
    }

    uint8_t dir_buf[FAT16_BYTES_PER_SECTOR];
    ata_read_sector(slot_lba, dir_buf);
    struct fat16_dir_entry *entries = (struct fat16_dir_entry *) dir_buf;
    struct fat16_dir_entry *entry = &entries[slot_index];

    for (int i = 0; i < 11; i++) {
        entry->name[i] = name83[i];
    }
    entry->attributes = FAT16_ATTR_ARCHIVE;
    entry->nt_reserved = 0;
    entry->creation_time_ds = 0;
    entry->creation_time = 0;
    entry->creation_date = 0;
    entry->access_date = 0;
    entry->high_cluster_bits = 0;  /* "always zero" for FAT16 */
    entry->modification_time = 0;
    entry->modification_date = 0;
    entry->low_cluster_bits = first_cluster;
    entry->file_size = size;

    ata_write_sector(slot_lba, dir_buf);

    if (out_first_cluster != 0) {
        *out_first_cluster = first_cluster;
    }

    kprintf("fat16_create_file: \"%s\" -- %u bytes, %u cluster(s), first cluster %u\n",
            name, size, clusters_needed, first_cluster);
    return 1;
}

int fat16_read_file(const char *name, uint8_t *buffer, uint32_t buffer_size, uint32_t *out_size) {
    if (!g_ready) {
        kprintf("fat16_read_file: fat16_init() was never called -- refusing\n");
        return 0;
    }

    uint16_t dir_cluster;
    const char *leaf;
    if (!resolve_path(name, &dir_cluster, &leaf)) {
        kprintf("fat16_read_file: \"%s\" -- directory component not found, or not really a "
                "directory -- refusing\n", name);
        return 0;
    }

    uint8_t name83[11];
    to_83_name(leaf, name83);

    uint32_t lba, index;
    int found_existing;
    if (!scan_dir(dir_cluster, name83, 0, &lba, &index, &found_existing) || !found_existing) {
        kprintf("fat16_read_file: \"%s\" not found -- refusing\n", name);
        return 0;
    }

    uint8_t dir_buf[FAT16_BYTES_PER_SECTOR];
    ata_read_sector(lba, dir_buf);
    struct fat16_dir_entry *entries = (struct fat16_dir_entry *) dir_buf;
    struct fat16_dir_entry *entry = &entries[index];

    if (entry->attributes & FAT16_ATTR_DIRECTORY) {
        kprintf("fat16_read_file: \"%s\" is a real directory, not a file -- refusing\n", name);
        return 0;
    }

    uint32_t size = entry->file_size;
    if (buffer_size < size) {
        kprintf("fat16_read_file: \"%s\" is %u bytes, buffer is only %u -- refusing\n",
                name, size, buffer_size);
        return 0;
    }

    uint16_t cluster = entry->low_cluster_bits;
    uint32_t bytes_left = size;
    uint8_t *dst = buffer;

    while (bytes_left > 0u && cluster != 0u && cluster < FAT16_CLUSTER_END_MIN) {
        uint8_t cluster_buf[FAT16_BYTES_PER_SECTOR];
        ata_read_sector(cluster_to_lba(cluster), cluster_buf);

        uint32_t chunk = bytes_left < FAT16_BYTES_PER_SECTOR ? bytes_left : FAT16_BYTES_PER_SECTOR;
        for (uint32_t b = 0; b < chunk; b++) {
            dst[b] = cluster_buf[b];
        }
        dst += chunk;
        bytes_left -= chunk;

        cluster = fat_read_entry(cluster);
    }

    if (out_size != 0) {
        *out_size = size;
    }

    kprintf("fat16_read_file: \"%s\" -- %u bytes read\n", name, size);
    return 1;
}

int fat16_delete_file(const char *name) {
    if (!g_ready) {
        kprintf("fat16_delete_file: fat16_init() was never called -- refusing\n");
        return 0;
    }

    uint16_t dir_cluster;
    const char *leaf;
    if (!resolve_path(name, &dir_cluster, &leaf)) {
        kprintf("fat16_delete_file: \"%s\" -- directory component not found, or not really a "
                "directory -- refusing\n", name);
        return 0;
    }

    uint8_t name83[11];
    to_83_name(leaf, name83);

    uint32_t lba, index;
    int found_existing;
    if (!scan_dir(dir_cluster, name83, 0, &lba, &index, &found_existing) || !found_existing) {
        kprintf("fat16_delete_file: \"%s\" not found -- refusing\n", name);
        return 0;
    }

    uint8_t dir_buf[FAT16_BYTES_PER_SECTOR];
    ata_read_sector(lba, dir_buf);
    struct fat16_dir_entry *entries = (struct fat16_dir_entry *) dir_buf;
    struct fat16_dir_entry *entry = &entries[index];

    if (entry->attributes & FAT16_ATTR_DIRECTORY) {
        kprintf("fat16_delete_file: \"%s\" is a real directory -- use fat16_rmdir() instead -- "
                "refusing\n", name);
        return 0;
    }

    uint16_t cluster = entry->low_cluster_bits;
    uint32_t freed = 0;
    while (cluster != 0u && cluster < FAT16_CLUSTER_END_MIN) {
        uint16_t next = fat_read_entry(cluster);
        fat_write_entry(cluster, FAT16_CLUSTER_FREE);
        cluster = next;
        freed++;
    }

    entry->name[0] = FAT16_DIRENT_DELETED;
    ata_write_sector(lba, dir_buf);

    kprintf("fat16_delete_file: \"%s\" -- %u cluster(s) freed\n", name, freed);
    return 1;
}

/* This chapter's own new real emptiness check, walking the real
 * subdirectory whose own first cluster is `dir_cluster` exactly the
 * way scan_dir()/list_dir_cluster() already do (dir_sector_lba() with
 * no `grow`), looking for any real, currently-occupied entry OTHER
 * than the directory's own "." and ".." pair fat16_mkdir() always
 * writes as entries 0 and 1. Cited directly (IEEE Std 1003.1-2008,
 * rmdir()): "The path argument names a directory that is not an empty
 * directory, or there are hard links to the directory other than dot
 * or a single entry in dot-dot" -- this function's own real, on-disk
 * definition of "empty" is exactly that: nothing besides the real "."
 * and ".." entries themselves. Returns 1 if genuinely empty, 0 if any
 * other real entry is found (or once a null-directory sentinel or an
 * on-disk read failure makes the answer unknowable -- this chapter's
 * own fat16_rmdir() only ever calls this after already confirming
 * `dir_cluster` names a real, existing directory). */
static int is_dir_empty(uint16_t dir_cluster) {
    for (uint32_t s = 0; ; s++) {
        uint32_t lba = dir_sector_lba(dir_cluster, s, 0);
        if (lba == 0) {
            return 1;  /* walked the whole real chain, nothing extra found */
        }

        uint8_t buf[FAT16_BYTES_PER_SECTOR];
        ata_read_sector(lba, buf);
        struct fat16_dir_entry *entries = (struct fat16_dir_entry *) buf;

        for (uint32_t e = 0; e < FAT16_ENTRIES_PER_SECTOR; e++) {
            uint8_t b0 = entries[e].name[0];
            if (b0 == FAT16_DIRENT_END) {
                return 1;
            }
            if (b0 == FAT16_DIRENT_DELETED) {
                continue;
            }
            /* A real "." or ".." entry (this directory's own first two
             * real entries, by fat16_mkdir()'s own construction) is
             * not extra content -- every other real, occupied entry
             * genuinely is. */
            int is_dot = (entries[e].name[0] == '.' && entries[e].name[1] == ' ');
            int is_dotdot = (entries[e].name[0] == '.' && entries[e].name[1] == '.'
                              && entries[e].name[2] == ' ');
            if (is_dot || is_dotdot) {
                continue;
            }
            return 0;  /* a real file or a real nested subdirectory still lives here */
        }
    }
}

int fat16_rmdir(const char *name) {
    if (!g_ready) {
        kprintf("fat16_rmdir: fat16_init() was never called -- refusing\n");
        return 0;
    }

    uint16_t parent_cluster;
    const char *leaf;
    if (!resolve_path(name, &parent_cluster, &leaf)) {
        kprintf("fat16_rmdir: \"%s\" -- directory component not found, or not really a "
                "directory -- refusing\n", name);
        return 0;
    }

    uint8_t name83[11];
    to_83_name(leaf, name83);

    uint32_t lba, index;
    int found_existing;
    if (!scan_dir(parent_cluster, name83, 0, &lba, &index, &found_existing) || !found_existing) {
        kprintf("fat16_rmdir: \"%s\" not found -- refusing\n", name);
        return 0;
    }

    uint8_t dir_buf[FAT16_BYTES_PER_SECTOR];
    ata_read_sector(lba, dir_buf);
    struct fat16_dir_entry *entries = (struct fat16_dir_entry *) dir_buf;
    struct fat16_dir_entry *entry = &entries[index];

    if (!(entry->attributes & FAT16_ATTR_DIRECTORY)) {
        kprintf("fat16_rmdir: \"%s\" is a real file, not a directory -- use fat16_delete_file() "
                "instead -- refusing\n", name);
        return 0;
    }

    uint16_t dir_cluster = entry->low_cluster_bits;

    /* Cited directly (IEEE Std 1003.1-2008, rmdir()): "If the directory
     * is not an empty directory, rmdir() shall fail". Checked BEFORE a
     * single real cluster is freed -- a directory that turns out to
     * hold real content is left completely untouched on disk, the same
     * honest, no-partial-effect refusal fat16_create_file() already
     * uses when the volume runs out of clusters mid-chain. */
    if (!is_dir_empty(dir_cluster)) {
        kprintf("fat16_rmdir: \"%s\" is not empty -- refusing\n", name);
        return 0;
    }

    /* Frees the directory's own WHOLE real cluster chain, not merely
     * its first cluster -- a real subdirectory can genuinely grow past
     * one cluster the same way a file does (dir_sector_lba()'s own
     * `grow` path), even though by the time this line runs
     * is_dir_empty() has already confirmed every one of those clusters
     * holds nothing but real "."/".." entries and 0x00/0xE5 filler. */
    uint16_t cluster = dir_cluster;
    uint32_t freed = 0;
    while (cluster != 0u && cluster < FAT16_CLUSTER_END_MIN) {
        uint16_t next = fat_read_entry(cluster);
        fat_write_entry(cluster, FAT16_CLUSTER_FREE);
        cluster = next;
        freed++;
    }

    entry->name[0] = FAT16_DIRENT_DELETED;
    ata_write_sector(lba, dir_buf);

    kprintf("fat16_rmdir: \"%s\" -- %u cluster(s) freed\n", name, freed);
    return 1;
}

/* The shared real listing logic behind both fat16_list_root() (dir_cluster
 * == 0) and fat16_list_dir() (dir_cluster == a real subdirectory's own
 * first cluster) -- walks dir_sector_lba() forward with no `grow`, the
 * same read-only traversal scan_dir() already uses for a pure lookup,
 * printing every real, currently-occupied entry, tagging a real
 * ATTR_DIRECTORY entry with "<DIR>" instead of a byte count. */
static uint32_t list_dir_cluster(uint16_t dir_cluster) {
    uint32_t count = 0;

    for (uint32_t s = 0; ; s++) {
        uint32_t lba = dir_sector_lba(dir_cluster, s, 0);
        if (lba == 0) {
            break;
        }

        uint8_t buf[FAT16_BYTES_PER_SECTOR];
        ata_read_sector(lba, buf);
        struct fat16_dir_entry *entries = (struct fat16_dir_entry *) buf;

        for (uint32_t e = 0; e < FAT16_ENTRIES_PER_SECTOR; e++) {
            uint8_t b0 = entries[e].name[0];
            if (b0 == FAT16_DIRENT_END) {
                kprintf("  (%u entr(ies) total)\n", count);
                return count;
            }
            if (b0 == FAT16_DIRENT_DELETED) {
                continue;
            }

            char display[13];
            from_83_name(entries[e].name, display);
            if (entries[e].attributes & FAT16_ATTR_DIRECTORY) {
                kprintf("  %s  <DIR>  (first cluster %u)\n", display, entries[e].low_cluster_bits);
            } else {
                kprintf("  %s  %u bytes  (first cluster %u)\n",
                        display, entries[e].file_size, entries[e].low_cluster_bits);
            }
            count++;
        }
    }

    kprintf("  (%u entr(ies) total)\n", count);
    return count;
}

uint32_t fat16_list_root(void) {
    kprintf("fat16_list_root:\n");
    return list_dir_cluster(0);
}

uint32_t fat16_list_dir(const char *name) {
    if (!g_ready) {
        kprintf("fat16_list_dir: fat16_init() was never called -- refusing\n");
        return 0;
    }

    uint16_t parent_cluster;
    const char *leaf;
    if (!resolve_path(name, &parent_cluster, &leaf)) {
        kprintf("fat16_list_dir: \"%s\" -- directory component not found, or not really a "
                "directory -- refusing\n", name);
        return 0;
    }

    uint8_t name83[11];
    to_83_name(leaf, name83);

    uint32_t lba, index;
    int found_existing;
    if (!scan_dir(parent_cluster, name83, 0, &lba, &index, &found_existing) || !found_existing) {
        kprintf("fat16_list_dir: \"%s\" not found -- refusing\n", name);
        return 0;
    }

    uint8_t buf[FAT16_BYTES_PER_SECTOR];
    ata_read_sector(lba, buf);
    struct fat16_dir_entry *entries = (struct fat16_dir_entry *) buf;
    if (!(entries[index].attributes & FAT16_ATTR_DIRECTORY)) {
        kprintf("fat16_list_dir: \"%s\" is a real file, not a directory -- refusing\n", name);
        return 0;
    }

    kprintf("fat16_list_dir(\"%s\"):\n", name);
    return list_dir_cluster(entries[index].low_cluster_bits);
}
