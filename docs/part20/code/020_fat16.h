#ifndef UNIX_OS_020_FAT16_H
#define UNIX_OS_020_FAT16_H

#include <stdint.h>

/* This chapter's own real filesystem: a genuine FAT16 volume, cited
 * field-for-field from OSDev Wiki ("FAT": https://wiki.osdev.org/FAT),
 * built entirely on top of Chapter 19's own real ata_read_sector()/
 * ata_write_sector() -- this driver never touches an I/O port
 * directly, only ever real 512-byte sectors. Deliberately scoped to
 * one flat root directory: fat16_create_file()/fat16_read_file()/
 * fat16_delete_file()/fat16_list_root() give this kernel real files
 * addressed by an ordinary 8.3 name, but there is no fat16_mkdir()
 * and no path separator anywhere in this file -- real subdirectories
 * are a later chapter's own work, the same kind of stated scope limit
 * this book has drawn before (Chapter 19's own raw-sector-only scope,
 * one chapter earlier). */

/* Every real filename this API accepts or returns is a plain,
 * NUL-terminated "NAME.EXT" (or "NAME" with no extension) string of
 * at most 8 name characters and 3 extension characters -- an ordinary
 * 8.3 name, never a full VFAT long filename. A name that does not fit
 * is silently truncated by this chapter's own to_83_name() (020_fat16.c),
 * an honest, stated limit: this chapter's own real demo only ever
 * uses names chosen to fit. */

/* Builds a genuinely fresh FAT16 volume on the whole disk this
 * kernel's Chapter 19 driver talks to: a real boot sector/BIOS
 * Parameter Block, two mirrored File Allocation Tables, and an empty
 * root directory, all written via real ata_write_sector() calls.
 * Destroys any data on the disk already -- this chapter's own demo
 * always formats before using the volume. Returns 1 on success. */
int fat16_format(void);

/* Reads and validates the real on-disk boot sector, and computes
 * every geometry value this file's other functions need (first FAT
 * sector, first root-directory sector, first data sector, sectors per
 * FAT, total usable clusters) directly from it -- never hardcoded a
 * second time, so a genuinely differently-formatted volume would
 * still be read correctly. Must be called once, after fat16_format()
 * (or after finding an already-formatted volume), before any other
 * function below. Returns 1 if a valid FAT16 boot signature (0xAA55)
 * was found, 0 otherwise. */
int fat16_init(void);

/* Creates a new real file in the root directory: allocates however
 * many whole clusters `size` real bytes need, chains them together in
 * both real FAT copies, writes `size` real bytes across them (the
 * last, partial cluster is zero-padded), and writes a real 32-byte
 * directory entry naming the file, its size, and its first cluster.
 * Refuses -- returns 0, writes nothing -- if a file with this name
 * already exists, if the root directory has no free entry left, or if
 * the volume has no free clusters left; never overwrites or guesses.
 * If `out_first_cluster` is non-null, the file's real first cluster
 * number is written there on success, purely so this chapter's own
 * demo can later prove a deleted file's cluster gets reused. */
int fat16_create_file(const char *name, const uint8_t *data, uint32_t size,
                       uint16_t *out_first_cluster);

/* Reads an existing file's real contents into `buffer`. Refuses --
 * returns 0 -- if no file with this name exists in the root directory,
 * or if `buffer_size` is smaller than the file's own real size (this
 * function never truncates). On success, copies exactly the file's
 * real size in bytes, walking its real cluster chain one real sector
 * at a time, and writes that size to `out_size` if non-null. */
int fat16_read_file(const char *name, uint8_t *buffer, uint32_t buffer_size,
                     uint32_t *out_size);

/* Deletes an existing file: marks its real directory entry as free
 * (the cited 0xE5 "unused entry" marker) and frees every real cluster
 * in its chain back to both FAT copies (each entry set back to the
 * cited 0x0000 free value), making them available to a future
 * fat16_create_file() call again. Returns 1 if a matching file was
 * found and deleted, 0 if no file with this name exists. */
int fat16_delete_file(const char *name);

/* Prints every real, currently-occupied entry in the root directory --
 * name (converted back from the on-disk 8.3 form to an ordinary
 * "NAME.EXT" string) and real file size -- skipping every entry this
 * chapter's own format/deletion convention marks as free (0x00 end-
 * of-directory or 0xE5 deleted). Returns the real count printed. */
uint32_t fat16_list_root(void);

#endif
